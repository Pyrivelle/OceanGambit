#pragma once

#include "ShipDecals.h"
#include <functional>
#include <osg/Geode>
#include <osg/Geometry>
#include <osgGA/GUIEventHandler>
#include <osgViewer/Viewer>

class NavalEffectsPreview final : public osgGA::GUIEventHandler {
public:
  enum Stage : unsigned { Flash = 1, Smoke = 2, Flight = 4, Splash = 8 };
  static constexpr double ContactTime = 1.4;
  static constexpr double SequenceTime = 4.1;
  NavalEffectsPreview(osgViewer::Viewer &viewer, const osg::Vec3d &muzzle,
                      const osg::Vec3d &waterContact);
  struct Aim {
    osg::Vec3d muzzle;
    osg::Vec3d forward;
  };
  void enableGunnery(osg::Node &target, ShipDecals &decals,
                     std::function<Aim(double, double)> aim);
  void setPresentationFlight(double previewReach, double duration);
  void beginExternalFlight(double now, const Aim& aim);
  void sampleExternalFlight(double now, const osg::Vec3d& position);
  void finishExternalFlight(double now, const osg::Vec3d& position, bool splash);
  bool externalFlightVisible() const { return active_; }
  double yaw() const { return yaw_; }
  double pitch() const { return pitch_; }
  osg::Node *node() const { return node_.get(); }
  unsigned visibleStages() const { return stages_; }
  bool handle(const osgGA::GUIEventAdapter &,
              osgGA::GUIActionAdapter &) override;

private:
  enum class Shape { Flash, Smoke, Glow, Trail, Plume, Drop, Ring };
  void update(double time);
  void fire(double now);
  void updateControls(const char *result);
  void quad(const osg::Vec3d &, const osg::Vec3d &, const osg::Vec3d &,
            const osg::Vec3d &, Shape, const osg::Vec4 &);
  void sprite(const osg::Vec3d &center, double width, double height, Shape,
              const osg::Vec4 &color);
  osg::Vec3d head(double phase) const;

  osgViewer::Viewer &viewer_;
  osg::Vec3d muzzle_, waterContact_, forward_{0., 1., 0.}, hitNormal_;
  osg::observer_ptr<osg::Node> target_;
  osg::ref_ptr<ShipDecals> decals_;
  std::function<Aim(double, double)> aim_;
  double yaw_ = 0., pitch_ = 0., contactTime_ = ContactTime;
  double previewReach_ = 155., presentationDuration_ = 0.;
  bool hit_ = false, splash_ = true, impactPending_ = false;
  osg::Vec3d right_, up_, eye_;
  osg::ref_ptr<osg::Geode> node_ = new osg::Geode;
  osg::ref_ptr<osg::Geometry> mesh_ = new osg::Geometry;
  osg::ref_ptr<osg::Vec3Array> vertices_ = new osg::Vec3Array;
  osg::ref_ptr<osg::Vec3Array> uvs_ = new osg::Vec3Array;
  osg::ref_ptr<osg::Vec4Array> colors_ = new osg::Vec4Array;
  osg::ref_ptr<osg::DrawArrays> triangles_ =
      new osg::DrawArrays(GL_TRIANGLES, 0, 0);
  bool externalFlight_ = false, externalFlying_ = false;
  osg::Vec3d externalHead_, externalTail_;
  double start_ = 0;
  bool active_ = false;
  unsigned stages_ = 0;
};
