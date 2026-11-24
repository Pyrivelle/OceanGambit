#include "NodeController.h"

#include <charconv>
#include <stdexcept>
#include <string_view>

namespace {
std::runtime_error invalid(const std::string& code, const std::string& path, const char* reason) {
    return std::runtime_error("Ship " + code + " node " + path + ": " + reason);
}
}

osg::NodePath NodeController::resolve(osg::Node& root, const NodeBinding& binding,
                                      const std::string& code) {
    const std::string_view path(binding.path);
    if (path.substr(0, 3) != "/n0" || (path.size() > 3 && path[3] != '/'))
        throw invalid(code, binding.path, "expected root /n0");
    osg::NodePath nodes{&root};
    for (size_t start = 3; start < path.size();) {
        const auto end = path.find('/', start + 1);
        const auto part = path.substr(start + 1, end - start - 1);
        if (part.size() < 2 || part[0] != 'n' || (part.size() > 2 && part[1] == '0'))
            throw invalid(code, binding.path, "malformed child index");
        unsigned index = 0;
        const auto parsed = std::from_chars(part.data() + 1, part.data() + part.size(), index);
        auto* group = nodes.back()->asGroup();
        if (parsed.ec != std::errc{} || parsed.ptr != part.data() + part.size() ||
            !group || index >= group->getNumChildren())
            throw invalid(code, binding.path, "child index outside loaded model");
        nodes.push_back(group->getChild(index));
        start = end == std::string_view::npos ? path.size() : end;
    }
    if (nodes.back()->getName() != binding.name)
        throw std::runtime_error("Ship " + code + " node " + binding.path +
            ": expected '" + binding.name + "', got '" + nodes.back()->getName() + "'");
    return nodes;
}

NodeController::NodeController(const osg::NodePath& path, const RotationBinding& binding,
                               Motion motion, const std::string& code)
    : component_(motion == Motion::Yaw ? 0u : 1u), sign_(binding.sign.value_or(1)) {
    if (path.size() < 2)
        throw invalid(code, binding.node.path, "cannot animate inspection root");
    auto* node = path.back();
    if (node->getNumParents() != 1)
        throw invalid(code, binding.node.path, "motion node must have one parent");
    dof_ = dynamic_cast<osgSim::DOFTransform*>(node);
    if (dof_) {
        motionNode_ = dof_.get();
        restHpr_ = dof_->getCurrentHPR();
        dof_->setAnimationOn(false);
        return;
    }
    if (!binding.pivot || !binding.axis || !binding.sign)
        throw invalid(code, binding.node.path, "ordinary node needs pivot, axis and sign");

    const osg::NodePath parentPath(path.begin(), path.end() - 1);
    osg::Matrixd inverse;
    if (!inverse.invert(osg::computeLocalToWorld(parentPath)))
        throw invalid(code, binding.node.path, "singular parent transform");
    pivot_ = *binding.pivot * inverse;
    axis_ = osg::Matrixd::transform3x3(*binding.axis, inverse);
    axis_.normalize();
    wrapper_ = new osg::MatrixTransform;
    wrapper_->addChild(node);
    if (!parentPath.back()->asGroup()->replaceChild(node, wrapper_))
        throw invalid(code, binding.node.path, "could not attach rotation transform");
    motionNode_ = wrapper_.get();
}

void NodeController::setAngle(double radians) {
    if (dof_) {
        auto next = restHpr_;
        next[component_] += static_cast<float>(radians);
        dof_->updateCurrentHPR(next);
    } else {
        wrapper_->setMatrix(osg::Matrixd::translate(-pivot_) *
            osg::Matrixd::rotate(sign_ * radians, axis_) * osg::Matrixd::translate(pivot_));
    }
}
