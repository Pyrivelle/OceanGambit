#pragma once

#include "ShipDefinition.h"

#include <osg/MatrixTransform>
#include <osgSim/DOFTransform>

class NodeController {
public:
    enum class Motion { Yaw, Pitch };

    static osg::NodePath resolve(osg::Node& root, const NodeBinding& binding, const std::string& code);
    NodeController(const osg::NodePath& path, const RotationBinding& binding, Motion motion,
                   const std::string& code);

    void setAngle(double radians);
    osg::Group& motionNode() const { return *motionNode_; }

private:
    osg::ref_ptr<osg::Group> motionNode_;
    osg::ref_ptr<osg::MatrixTransform> wrapper_;
    osg::ref_ptr<osgSim::DOFTransform> dof_;
    osg::Vec3d pivot_, axis_;
    osg::Vec3 restHpr_;
    unsigned component_;
    double sign_;
};
