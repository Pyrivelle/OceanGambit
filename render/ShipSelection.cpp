#include "ShipSelection.h"

#include <osg/BlendFunc>
#include <osg/Depth>
#include <osg/Geode>
#include <osg/Geometry>
#include <osg/Math>
#include <osg/Program>
#include <osg/Uniform>

#include <algorithm>
#include <cmath>

bool ShipSelectionBox::contains(const osg::Vec3d& world, const osg::Matrixd& viewProjection) const {
    const osg::Vec4d clip = osg::Vec4d(world.x(), world.y(), world.z(), 1) * viewProjection;
    const double w = clip.w();
    if (w <= 0 || std::abs(clip.z()) > w) return false;
    return clip.x() >= std::max(-1., std::min(first.x(), last.x())) * w &&
           clip.x() <= std::min(1., std::max(first.x(), last.x())) * w &&
           clip.y() >= std::max(-1., std::min(first.y(), last.y())) * w &&
           clip.y() <= std::min(1., std::max(first.y(), last.y())) * w;
}

namespace {
osg::Program* selectionProgram() {
    static osg::ref_ptr<osg::Program> program = [] {
        auto shader = new osg::Program;
        shader->addShader(new osg::Shader(osg::Shader::VERTEX, R"(#version 330
in vec4 osg_Vertex;
uniform mat4 osg_ModelViewProjectionMatrix;
void main() { gl_Position = osg_ModelViewProjectionMatrix * osg_Vertex; }
)"));
        shader->addShader(new osg::Shader(osg::Shader::FRAGMENT, R"(#version 330
uniform vec4 selectionTint;
out vec4 fragmentColor;
void main() { fragmentColor = selectionTint; }
)"));
        return shader;
    }();
    return program.get();
}

void appendArc(osg::Vec3Array& vertices, const osg::Vec3& center, double radius, double width,
               double start, double end, unsigned steps) {
    const auto point = [&](double section, double r) {
        const double angle = start + (end - start) * section;
        return osg::Vec3(center.x() + r * std::cos(angle), center.y() + r * std::sin(angle), 2.5);
    };
    const double inner = radius - width * .5, outer = radius + width * .5;
    for (unsigned i = 0; i < steps; ++i) {
        const double a = double(i) / steps, b = double(i + 1) / steps;
        const auto ai = point(a, inner), ao = point(a, outer);
        const auto bi = point(b, inner), bo = point(b, outer);
        for (const auto& vertex : {ai, ao, bo, ai, bo, bi}) vertices.push_back(vertex);
    }
}

void addRing(osg::Geode& root, const osg::Vec3& center, double radius, double width,
             const osg::Vec4& color, bool dashed) {
    osg::ref_ptr<osg::Vec3Array> vertices = new osg::Vec3Array;
    if (dashed) {
        for (unsigned i = 0; i < 12; ++i) {
            const double start = i * osg::PI / 6;
            appendArc(*vertices, center, radius, width, start, start + osg::PI / 9, 8);
        }
    } else {
        appendArc(*vertices, center, radius, width, 0, 2 * osg::PI, 96);
    }
    auto geometry = new osg::Geometry;
    geometry->setVertexArray(vertices);
    geometry->addPrimitiveSet(new osg::DrawArrays(GL_TRIANGLES, 0, vertices->size()));
    geometry->setUseDisplayList(false);
    geometry->setUseVertexBufferObjects(true);
    geometry->getOrCreateStateSet()->addUniform(new osg::Uniform("selectionTint", color));
    root.addDrawable(geometry);
}
}

namespace {
osg::ref_ptr<osg::Geode> makeOverlay() {
    osg::ref_ptr<osg::Geode> ring = new osg::Geode;
    auto* state = ring->getOrCreateStateSet();
    state->setAttributeAndModes(selectionProgram());
    state->setMode(GL_BLEND, osg::StateAttribute::ON);
    state->setMode(GL_CULL_FACE, osg::StateAttribute::OFF);
    state->setAttributeAndModes(new osg::BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
    state->setAttributeAndModes(new osg::Depth(osg::Depth::LEQUAL, 0, 1, false));
    state->setRenderBinDetails(25, "DepthSortedBin");
    return ring;
}
}

osg::ref_ptr<osg::Node> createShipSelectionRing(const osg::BoundingSphere& bounds) {
    auto ring = makeOverlay();
    const double radius = std::max(30., bounds.radius() * 1.08);
    addRing(*ring, bounds.center(), radius, 8., {.15f, .8f, 1.f, .22f}, false);
    addRing(*ring, bounds.center(), radius, 2.8, {.65f, .98f, 1.f, .95f}, false);
    addRing(*ring, bounds.center(), radius * 1.17, 2.6, {.38f, .9f, 1.f, .9f}, true);
    return ring;
}

osg::ref_ptr<osg::Node> createMoveMarker() {
    auto marker = makeOverlay();
    addRing(*marker, {}, 15, 3.6, {.35f, 1.f, .52f, .95f}, false);
    addRing(*marker, {}, 21, 2.5, {.35f, 1.f, .52f, .65f}, true);
    return marker;
}
