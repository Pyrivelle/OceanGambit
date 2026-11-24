#pragma once

#include <array>
#include <osg/Group>
#include <osg/Program>
#include <osg/Texture2D>
#include <osgGA/GUIEventHandler>
#include <osgViewer/Viewer>

// Scene-space decal layer for the stationary test ship.
class ShipDecals : public osgGA::GUIEventHandler {
public:
  static constexpr osg::Node::NodeMask ShipMask = 0x1;
  static constexpr osg::Node::NodeMask OtherMask = 0x2;
  ShipDecals(osgViewer::Viewer &viewer, osg::Node &ship, osg::Group &layer,
             bool showControls = true);
  bool stampAt(const osg::Vec3d &point, osg::Vec3d normal, unsigned style);
  void clear();
  bool handle(const osgGA::GUIEventAdapter &event,
              osgGA::GUIActionAdapter &) override;
  const std::array<unsigned, 3> &counts() const { return counts_; }

private:
  void stamp(const osgGA::GUIEventAdapter &event);
  void updateTitle();
  osgViewer::Viewer &viewer_;
  osg::Node &ship_;
  osg::Group &layer_;
  osg::ref_ptr<osg::Program> program_;
  std::array<osg::ref_ptr<osg::Texture2D>, 3> textures_;
  std::array<unsigned, 3> counts_{};
  unsigned selected_ = 0;
  bool trackingClick_ = false;
  osg::Vec2 down_;
};
