#include "NavalEffectsPreview.h"
#include <osgUtil/IntersectionVisitor>
#include <osgUtil/LineSegmentIntersector>

#include <algorithm>
#include <cmath>
#include <osg/BlendFunc>
#include <osg/Depth>
#include <osg/Program>

namespace {
const char *vertex = R"GLSL(#version 330
in vec4 osg_Vertex;
in vec4 osg_Color;
in vec4 osg_MultiTexCoord0;
uniform mat4 osg_ModelViewProjectionMatrix;
out vec2 uv;
out vec4 tint;
flat out int kind;
void main() {
    gl_Position = osg_ModelViewProjectionMatrix * osg_Vertex;
    uv = osg_MultiTexCoord0.xy;
    kind = int(osg_MultiTexCoord0.z + 0.5);
    tint = osg_Color;
})GLSL";

const char *fragment = R"GLSL(#version 330
in vec2 uv;
in vec4 tint;
flat in int kind;
out vec4 fragmentColor;
float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
float noise(vec2 p) {
    vec2 cell = floor(p), f = fract(p);
    f = f*f*(3.0-2.0*f);
    return mix(mix(hash(cell), hash(cell+vec2(1,0)), f.x),
               mix(hash(cell+vec2(0,1)), hash(cell+vec2(1,1)), f.x), f.y);
}
void main() {
    vec2 p = 2.0*uv-1.0;
    float r = length(p), a = 0.0;
    vec3 color = tint.rgb;
    switch (kind) {
    case 0: {
        float theta = atan(p.y, p.x);
        float rays = pow(0.5+0.5*cos(7.0*theta + 0.48*sin(3.0*theta)), 4.0);
        float edge = 0.43 + 0.50*rays;
        a = 1.0-smoothstep(0.12, edge, r);
        color = mix(color, vec3(1.0, 0.99, 0.9), 1.0-smoothstep(0.05,0.43,r));
        break;
    }
    case 1: {
        float cloud = 0.72*noise(5.2*p) + 0.28*noise(13.0*p);
        a = (1.0-smoothstep(0.13,0.97,r+0.30*(cloud-0.35)))*(0.52+0.48*cloud);
        color *= 0.77 + cloud*0.36;
        break;
    }
    case 2:
        a = pow(1.0-smoothstep(0.0,1.0,r), 1.6);
        color = mix(color, vec3(1,0.98,0.78), 1.0-smoothstep(0.08,0.54,r));
        break;
    case 3:
        a = pow(max(1.0-abs(p.y),0.0), 1.7);
        color = mix(color, vec3(1,0.96,0.76), pow(max(1.0-abs(p.y),0.0), 5.0));
        break;
    case 4: {
        float strands = noise(vec2(22.0*uv.x, 5.0*uv.y));
        float mist = 0.6*noise(8.0*uv)+0.4*noise(31.0*uv);
        float width = mix(0.84, 0.28, uv.y) + .11*mist;
        a = (1.0-smoothstep(0.15,width,abs(p.x)+.16*strands)) *
            (0.42+0.58*strands) * smoothstep(0.0,0.075,uv.y) * (1.0-smoothstep(0.55,1.0,uv.y));
        color = mix(color,vec3(.98,1,1),strands);
        break;
    }
    case 5:
        a = 1.0-smoothstep(.12, .98, r);
        break;
    case 6: {
        float ring = r+0.055*(noise(22.0*p)-0.5);
        a = 0.8*(1.0-smoothstep(.02,.12,abs(ring-.73))) +
            0.35*(1.0-smoothstep(.015,.065,abs(ring-.46)));
        break;
    }
    }
    a *= tint.a;
    if(a < 0.012) discard;
    fragmentColor = vec4(color,a);
})GLSL";

double smooth(double t) {
  t = std::clamp(t, 0., 1.);
  return t * t * (3 - 2 * t);
}
} // namespace

NavalEffectsPreview::NavalEffectsPreview(osgViewer::Viewer &viewer,
                                         const osg::Vec3d &muzzle,
                                         const osg::Vec3d &waterContact)
    : viewer_(viewer), muzzle_(muzzle), waterContact_(waterContact) {
  auto *state = node_->getOrCreateStateSet();
  osg::ref_ptr<osg::Program> program = new osg::Program;
  program->addShader(new osg::Shader(osg::Shader::VERTEX, vertex));
  program->addShader(new osg::Shader(osg::Shader::FRAGMENT, fragment));
  state->setAttributeAndModes(program);
  state->setMode(GL_BLEND, osg::StateAttribute::ON);
  state->setAttributeAndModes(
      new osg::BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
  state->setAttributeAndModes(
      new osg::Depth(osg::Depth::LEQUAL, 0., 1., false));
  state->setRenderingHint(osg::StateSet::TRANSPARENT_BIN);
  mesh_->setDataVariance(osg::Object::DYNAMIC);
  mesh_->setVertexArray(vertices_);
  mesh_->setTexCoordArray(0, uvs_);
  mesh_->setColorArray(colors_, osg::Array::BIND_PER_VERTEX);
  mesh_->addPrimitiveSet(triangles_);
  mesh_->setUseDisplayList(false);
  mesh_->setUseVertexBufferObjects(true);
  node_->addDrawable(mesh_);
  node_->setNodeMask(0);
  osgViewer::ViewerBase::Windows windows;
  viewer_.getWindows(windows);
  for (auto *window : windows)
    window->setWindowName("020528 Naval Effects | Space / F: play effect | C: "
                          "clear | Drag: orbit");
}

bool NavalEffectsPreview::handle(const osgGA::GUIEventAdapter &event,
                                 osgGA::GUIActionAdapter &) {
  const auto *stamp = viewer_.getFrameStamp();
  if (!stamp)
    return false;
  const double now = stamp->getSimulationTime();
  if (event.getEventType() == osgGA::GUIEventAdapter::KEYDOWN) {
    const int rawKey = event.getKey();
    const int key =
        rawKey >= 'A' && rawKey <= 'Z' ? rawKey - 'A' + 'a' : rawKey;
    if (aim_ && (key == 'a' || key == 'd' || key == 'w' || key == 's')) {
      yaw_ = std::clamp(yaw_ + (key == 'a'   ? -2.
                                : key == 'd' ? 2.
                                             : 0.),
                        -70., 70.);
      pitch_ = std::clamp(pitch_ + (key == 's'   ? -1.
                                    : key == 'w' ? 1.
                                                 : 0.),
                          -15., 20.);
      aim_(yaw_, pitch_);
      updateControls(nullptr);
      return true;
    }
    if (key == ' ' || (!aim_ && key == 'f')) {
      if (!aim_ || !impactPending_)
        fire(now);
      return true;
    }
    if (key == 'c') {
      active_ = false;
      impactPending_ = false;
      stages_ = 0;
      node_->setNodeMask(0);
      if (decals_) {
        decals_->clear();
        updateControls("Ready");
      }
      return true;
    }
  }
  if (event.getEventType() == osgGA::GUIEventAdapter::FRAME && active_)
    update(std::max(0., now - start_));
  return false;
}

void NavalEffectsPreview::enableGunnery(
    osg::Node &target, ShipDecals &decals,
    std::function<Aim(double, double)> aim) {
  target_ = &target;
  decals_ = &decals;
  aim_ = std::move(aim);
  aim_(yaw_, pitch_);
  updateControls("Ready");
}

void NavalEffectsPreview::updateControls(const char *result) {
  osgViewer::ViewerBase::Windows windows;
  viewer_.getWindows(windows);
  for (auto *window : windows) {
    std::string title = "020528 | WASD Aim | Space Fire | C Clear | Yaw " +
                        std::to_string(int(yaw_)) + " / Pitch " +
                        std::to_string(int(pitch_));
    if (result)
      title += std::string(" | ") + result;
    window->setWindowName(title);
  }
}

void NavalEffectsPreview::setPresentationFlight(double previewReach, double duration) {
  previewReach_ = previewReach;
  presentationDuration_ = duration;
}

void NavalEffectsPreview::fire(double now) {
  if (aim_) {
    const Aim aim = aim_(yaw_, pitch_);
    muzzle_ = aim.muzzle;
    forward_ = aim.forward;
    const double range = previewReach_;
    constexpr double seaZ = -.15;
    double distance = range;
    splash_ = false;
    if (forward_.z() < 0.) {
      const double waterDistance = (seaZ - muzzle_.z()) / forward_.z();
      if (waterDistance >= 0. && waterDistance < range) {
        distance = waterDistance;
        splash_ = true;
      }
    }
    waterContact_ = muzzle_ + forward_ * distance;
    osg::ref_ptr<osgUtil::LineSegmentIntersector> ray =
        new osgUtil::LineSegmentIntersector(muzzle_, waterContact_);
    osgUtil::IntersectionVisitor visitor(ray);
    visitor.setLODSelectionMode(
        osgUtil::IntersectionVisitor::USE_HIGHEST_LEVEL_OF_DETAIL);
    if (target_.valid())
      target_->accept(visitor);
    hit_ = ray->containsIntersections();
    if (hit_) {
      const auto hit = ray->getFirstIntersection();
      waterContact_ = hit.getWorldIntersectPoint();
      hitNormal_ = osg::Vec3d(hit.getWorldIntersectNormal());
      splash_ = false;
    }
    contactTime_ = presentationDuration_ > 0.
        ? presentationDuration_
        : std::max(.1, (waterContact_ - muzzle_).length() / 62.);
    impactPending_ = true;
    updateControls("In flight");
  }
  start_ = now;
  active_ = true;
  update(0.);
}

void NavalEffectsPreview::beginExternalFlight(double now, const Aim& aim) {
  externalFlight_ = externalFlying_ = active_ = true;
  splash_ = impactPending_ = false;
  start_ = now;
  muzzle_ = externalHead_ = externalTail_ = aim.muzzle;
  forward_ = aim.forward;
  update(0.);
}

void NavalEffectsPreview::sampleExternalFlight(double now, const osg::Vec3d& position) {
  if (externalFlying_ && position != externalHead_) {
    externalHead_ = position;
    externalTail_ = externalTail_ * .82 + externalHead_ * .18;
  }
  if (active_) update(now - start_);
}

void NavalEffectsPreview::finishExternalFlight(double now, const osg::Vec3d& position,
                                                bool splash) {
  externalFlying_ = false;
  waterContact_ = position;
  splash_ = splash;
  contactTime_ = now - start_;
  update(contactTime_);
}

void NavalEffectsPreview::quad(const osg::Vec3d &a, const osg::Vec3d &b,
                               const osg::Vec3d &c, const osg::Vec3d &d,
                               Shape shape, const osg::Vec4 &color) {
  const float id = static_cast<float>(shape);
  const osg::Vec3d corners[] = {a, b, c, a, c, d};
  const osg::Vec2 uv[] = {{0, 0}, {1, 0}, {1, 1}, {0, 0}, {1, 1}, {0, 1}};
  for (unsigned i = 0; i < 6; ++i) {
    vertices_->push_back(osg::Vec3(corners[i]));
    uvs_->push_back({uv[i].x(), uv[i].y(), id});
    colors_->push_back(color);
  }
}

void NavalEffectsPreview::sprite(const osg::Vec3d &center, double width,
                                 double height, Shape shape,
                                 const osg::Vec4 &color) {
  const osg::Vec3d w = right_ * (width * .5), h = up_ * (height * .5);
  quad(center - w - h, center + w - h, center + w + h, center - w + h, shape,
       color);
}

osg::Vec3d NavalEffectsPreview::head(double phase) const {
  const double t = std::clamp(phase, 0., 1.);
  if (externalFlight_) return externalTail_ + (externalHead_ - externalTail_) * t;
  auto point = muzzle_ + (waterContact_ - muzzle_) * t;
  if (!aim_)
    point.z() += 7.0 * std::sin(osg::PI * t);
  return point;
}

void NavalEffectsPreview::update(double time) {
  stages_ = 0;
  if (impactPending_ && time >= contactTime_) {
    impactPending_ = false;
    if (hit_) {
      const bool stamped = decals_->stampAt(waterContact_, hitNormal_, 2);
      updateControls(stamped ? "Hit" : "Hit (no decal)");
    } else {
      updateControls(splash_ ? "Water" : "Miss");
    }
  }
  if (!externalFlying_ && time >= contactTime_ + (SequenceTime - ContactTime)) {
    active_ = false;
    node_->setNodeMask(0);
    return;
  }
  osg::Vec3d center;
  viewer_.getCamera()->getViewMatrixAsLookAt(eye_, center, up_);
  right_ = (center - eye_) ^ up_;
  right_.normalize();
  up_.normalize();
  vertices_->clear();
  uvs_->clear();
  colors_->clear();

  if (time < 2.6) {
    stages_ |= Smoke;
    for (int i = 0; i < 8; ++i) {
      const double age = time - i * .024;
      if (age < 0 || age > 2.3)
        continue;
      const auto drift = osg::Vec3d(1.8 * age + .35 * std::cos(i * 2.4),
                                    (.5 + i * .28) * age + i * .36,
                                    .7 + age * (1.1 + i * .15));
      const float alpha =
          float(.69 * smooth(age / .09) * (1 - smooth(age / 2.3)));
      const float shade = .27f + .037f * float(i);
      sprite(muzzle_ + drift, 2.35 + 4.1 * age, 1.9 + 3.25 * age, Shape::Smoke,
             {shade, shade * .94f, shade * .87f, alpha});
    }
  }
  if (time < .22) {
    stages_ |= Flash;
    const float alpha = float(1 - smooth(time / .22));
    sprite(muzzle_ + forward_ * 1.36, 12., 9.5, Shape::Glow,
           {1, .34f, .045f, alpha * .57f});
    sprite(muzzle_ + forward_ * 1.36, 9.0, 7.0, Shape::Flash,
           {1, .56f, .12f, alpha});
    sprite(muzzle_ + forward_ * .31, 2.8, 2.25, Shape::Glow,
           {1, .86f, .44f, alpha});
  }
  if (time > .025 && (externalFlight_ ? externalFlying_ : time < contactTime_)) {
    stages_ |= Flight;
    const double phase = externalFlight_ ? 1. : time / contactTime_;
    const double back = std::max(0., phase - .22);
    auto emitTrail = [&](double width, const osg::Vec4 &tint) {
      for (unsigned i = 0; i < 16; ++i) {
        const double u0 = i / 16., u1 = (i + 1) / 16.;
        const auto a = head(back + (phase - back) * u0),
                   b = head(back + (phase - back) * u1);
        auto edge = (b - a) ^ (eye_ - a);
        edge.normalize();
        edge *= width;
        osg::Vec4 color = tint;
        color.a() *= float(smooth((u0 + u1) * .5));
        quad(a - edge * (.28 + .72 * u0), b - edge * (.28 + .72 * u1),
             b + edge * (.28 + .72 * u1), a + edge * (.28 + .72 * u0),
             Shape::Trail, color);
      }
    };
    emitTrail(.46, {1, .34f, .055f, .55f});
    emitTrail(.18, {1, .76f, .28f, .96f});
    sprite(head(phase), 2.9, 2.9, Shape::Glow, {1, .61f, .13f, .79f});
    sprite(head(phase), .96, .96, Shape::Glow, {1, .93f, .52f, 1});
  }
  if (splash_ && time >= contactTime_) {
    stages_ |= Splash;
    const double age = time - contactTime_;
    const float fade = float(1 - smooth(age / 2.65));
    const osg::Vec3d ringCenter = waterContact_ + osg::Vec3d(0, 0, .045);
    const osg::Vec3d w(2.8 + age * 5.9, 0, 0), h(0, 2.8 + age * 5.9, 0);
    quad(ringCenter - w - h, ringCenter + w - h, ringCenter + w + h,
         ringCenter - w + h, Shape::Ring, {.82f, .94f, .99f, fade * .90f});
    const double height = (1.5 + 14.0 * smooth(age / .49)) *
                          (1 - .79 * smooth((age - .44) / 1.62));
    osg::Vec3d r = right_;
    r.z() = 0.;
    r.normalize();
    for (int i = -1; i <= 1; ++i) {
      const auto base = waterContact_ + r * (i * 1.12);
      const auto top = base + osg::Vec3d(i * .91 * age, .54 * age,
                                         height * (i == 0 ? 1. : .76));
      const auto span = r * (2.75 + age * 1.05);
      quad(base - span, base + span, top + span, top - span, Shape::Plume,
           {.77f, .91f, .98f, fade * (i == 0 ? .94f : .63f)});
    }
    for (int i = 0; i < 56; ++i) {
      const double t = age - (i % 7) * .020;
      if (t < 0)
        continue;
      const double z = t * (8.4 + (i % 13) * .98) - 8.8 * t * t;
      if (z < .05)
        continue;
      const double angle = i * 2.399963;
      const double distance = t * (2.1 + (i % 11) * .67);
      const auto p = waterContact_ + osg::Vec3d(std::cos(angle) * distance,
                                                std::sin(angle) * distance, z);
      const double size = .22 + (i % 5) * .093;
      sprite(p, size, size * 1.65, Shape::Drop, {.82f, .94f, 1, fade * .78f});
    }
    for (int i = 0; i < 5; ++i) {
      const double x = (i - 2) * (.94 + age * .55);
      sprite(waterContact_ + r * x + osg::Vec3d(0, 0, .74 + (i % 2) * .36),
             2.5 + age * 2.6, .98 + age * .96, Shape::Smoke,
             {.8f, .92f, .99f, fade * .64f});
    }
  }
  vertices_->dirty();
  uvs_->dirty();
  colors_->dirty();
  triangles_->setCount(GLsizei(vertices_->size()));
  mesh_->dirtyBound();
  node_->setNodeMask(~osg::Node::NodeMask(0));
}
