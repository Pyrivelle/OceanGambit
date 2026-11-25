#pragma once

#include <osg/Matrixd>
#include <osg/Node>
#include <osg/Vec2d>

struct ShipSelectionBox {
    osg::Vec2d first, last;
    bool contains(const osg::Vec3d& world, const osg::Matrixd& viewProjection) const;
};

osg::ref_ptr<osg::Node> createShipSelectionRing(const osg::BoundingSphere& bounds);
osg::ref_ptr<osg::Node> createMoveMarker();
