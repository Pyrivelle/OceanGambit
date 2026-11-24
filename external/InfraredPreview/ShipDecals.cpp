#include "ShipDecals.h"

#include <algorithm>
#include <cmath>
#include <osg/BlendFunc>
#include <osg/Depth>
#include <osg/Geode>
#include <osg/Geometry>
#include <osg/Uniform>
#include <osgUtil/IntersectionVisitor>
#include <osgUtil/LineSegmentIntersector>

namespace {
constexpr unsigned TextureSize = 256;
constexpr unsigned Grid = 18;

float falloff(float lo, float hi, float value) {
  const float t = std::clamp((value - lo) / (hi - lo), 0.f, 1.f);
  return 1.f - t * t * (3.f - 2.f * t);
}

float speckle(unsigned x, unsigned y) {
  unsigned n = x * 374761393u + y * 668265263u;
  n = (n ^ (n >> 13u)) * 1274126177u;
  return float((n ^ (n >> 16u)) & 255u) / 255.f;
}

osg::ref_ptr<osg::Texture2D> makeTexture(unsigned type) {
  osg::ref_ptr<osg::Image> image = new osg::Image;
  image->allocateImage(TextureSize, TextureSize, 1, GL_RGBA, GL_UNSIGNED_BYTE);
  for (unsigned y = 0; y < TextureSize; ++y) {
    for (unsigned x = 0; x < TextureSize; ++x) {
      const float u = (float(x) + .5f) / (TextureSize / 2.f) - 1.f;
      const float v = (float(y) + .5f) / (TextureSize / 2.f) - 1.f;
      const float angle = std::atan2(v, u);
      const float radius = std::hypot(u, v);
      const float grain = speckle(x, y);
      const float jagged = radius + .045f * std::sin(11.f * angle) +
                           .035f * std::cos(19.f * angle);
      osg::Vec3 color;
      float alpha;
      if (type == 0) {
        const float plume = std::hypot(u * 1.2f, (v - .16f) * .86f);
        const float uneven =
            plume + .11f * std::sin(8.f * u + 6.f * v) + .06f * grain;
        alpha = .90f * falloff(.03f, .96f, uneven) * (.70f + .30f * grain);
        color.set(.075f, .066f, .061f);
      } else if (type == 1) {
        const float hole = falloff(.20f, .26f, jagged);
        const float rim = falloff(.29f, .39f, jagged);
        alpha = std::max(rim, .48f * falloff(.34f, .94f, jagged) *
                                  (.65f + .35f * grain));
        const float glint =
            .35f + .50f * std::max(0.f, std::sin(angle + .6f)) + .10f * grain;
        color = osg::Vec3(.08f, .069f, .060f) * (1.f - rim) +
                osg::Vec3(glint, glint * .91f, glint * .73f) * rim;
        color = color * (1.f - hole) + osg::Vec3(.012f, .012f, .013f) * hole;
      } else {
        const float crater = falloff(.18f, .47f, jagged);
        const float oxidized = falloff(.52f, .77f, jagged);
        alpha = .96f * falloff(.45f, .96f, jagged) * (.78f + .22f * grain);
        color = osg::Vec3(.13f, .085f, .049f) * (1.f - oxidized) +
                osg::Vec3(.52f, .235f, .095f) * (oxidized * (1.f - crater)) +
                osg::Vec3(.12f, .078f, .054f) * crater;
        if (jagged < .52f && grain > .94f)
          color += osg::Vec3(.19f, .12f, .07f);
      }
      auto *pixel = image->data(x, y);
      for (unsigned i = 0; i < 3; ++i)
        pixel[i] =
            static_cast<unsigned char>(255.f * std::clamp(color[i], 0.f, 1.f));
      pixel[3] = static_cast<unsigned char>(255.f * alpha *
                                            falloff(.87f, 1.f, radius));
    }
  }
  osg::ref_ptr<osg::Texture2D> texture = new osg::Texture2D(image);
  texture->setFilter(osg::Texture::MIN_FILTER,
                     osg::Texture::LINEAR_MIPMAP_LINEAR);
  texture->setFilter(osg::Texture::MAG_FILTER, osg::Texture::LINEAR);
  texture->setWrap(osg::Texture::WRAP_S, osg::Texture::CLAMP_TO_EDGE);
  texture->setWrap(osg::Texture::WRAP_T, osg::Texture::CLAMP_TO_EDGE);
  return texture;
}

struct Sample {
  osg::Vec3d point;
  bool present = false;
};

Sample project(osg::Node &ship, const osg::Vec3d &planePoint,
               const osg::Vec3d &normal, double depth) {
  osg::ref_ptr<osgUtil::LineSegmentIntersector> ray =
      new osgUtil::LineSegmentIntersector(planePoint + normal * depth,
                                          planePoint - normal * depth);
  osgUtil::IntersectionVisitor visitor(ray);
  ship.accept(visitor);
  double best = depth + 1;
  Sample sample;
  for (const auto &hit : ray->getIntersections()) {
    auto point = hit.getWorldIntersectPoint();
    auto surfaceNormal = hit.getWorldIntersectNormal();
    surfaceNormal.normalize();
    const double distance = (point - planePoint).length();
    if (surfaceNormal * normal > .48 && distance < best) {
      best = distance;
      sample = {point + osg::Vec3d(surfaceNormal) * .014, true};
    }
  }
  return sample;
}

osg::ref_ptr<osg::Geometry> buildPatch(osg::Node &ship, const osg::Vec3d &point,
                                       const osg::Vec3d &normal,
                                       double radius) {
  osg::Vec3d horizontal = osg::Vec3d(0, 0, 1) ^ normal;
  if (horizontal.normalize() < .1)
    horizontal = osg::Vec3d(0, 1, 0) ^ normal;
  horizontal.normalize();
  const osg::Vec3d vertical = normal ^ horizontal;
  osg::ref_ptr<osg::Vec3Array> vertices = new osg::Vec3Array;
  osg::ref_ptr<osg::Vec2Array> uvs = new osg::Vec2Array;
  std::array<Sample, (Grid + 1) * (Grid + 1)> samples;
  for (unsigned y = 0; y <= Grid; ++y) {
    for (unsigned x = 0; x <= Grid; ++x) {
      const float u = float(x) / Grid, v = float(y) / Grid;
      auto &sample = samples[y * (Grid + 1) + x];
      sample = project(
          ship,
          point + (horizontal * (2 * u - 1) + vertical * (2 * v - 1)) * radius,
          normal, radius * .52);
      vertices->push_back(osg::Vec3(sample.point));
      uvs->push_back({u, v});
    }
  }
  osg::ref_ptr<osg::DrawElementsUInt> indices =
      new osg::DrawElementsUInt(GL_TRIANGLES);
  const double maxEdge = radius * 5.0 / Grid;
  auto add = [&](unsigned a, unsigned b, unsigned c) {
    const auto &p = samples[a], &q = samples[b], &r = samples[c];
    if (!p.present || !q.present || !r.present ||
        (p.point - q.point).length() > maxEdge ||
        (p.point - r.point).length() > maxEdge ||
        (q.point - r.point).length() > maxEdge)
      return;
    indices->push_back(a);
    indices->push_back(b);
    indices->push_back(c);
  };
  for (unsigned y = 0; y < Grid; ++y)
    for (unsigned x = 0; x < Grid; ++x) {
      const unsigned a = y * (Grid + 1) + x;
      add(a, a + 1, a + Grid + 1);
      add(a + 1, a + Grid + 2, a + Grid + 1);
    }
  if (indices->empty())
    return {};
  osg::ref_ptr<osg::Geometry> geometry = new osg::Geometry;
  geometry->setVertexArray(vertices);
  geometry->setTexCoordArray(0, uvs);
  geometry->addPrimitiveSet(indices);
  geometry->setUseDisplayList(false);
  geometry->setUseVertexBufferObjects(true);
  return geometry;
}
} // namespace

ShipDecals::ShipDecals(osgViewer::Viewer &viewer, osg::Node &ship,
                       osg::Group &layer, bool showControls)
    : viewer_(viewer), ship_(ship), layer_(layer), program_(new osg::Program) {
  for (unsigned i = 0; i < textures_.size(); ++i)
    textures_[i] = makeTexture(i);
  program_->addShader(new osg::Shader(osg::Shader::VERTEX, R"(#version 330
in vec4 osg_Vertex;
in vec4 osg_MultiTexCoord0;
uniform mat4 osg_ModelViewProjectionMatrix;
out vec2 uv;
void main() {
    uv = osg_MultiTexCoord0.xy;
    gl_Position = osg_ModelViewProjectionMatrix * osg_Vertex;
})"));
  program_->addShader(new osg::Shader(osg::Shader::FRAGMENT, R"(#version 330
in vec2 uv;
uniform sampler2D stamp;
out vec4 fragmentColor;
void main() {
    fragmentColor = texture(stamp, uv);
    if (fragmentColor.a < 0.015) discard;
})"));
  if (showControls)
    updateTitle();
}

void ShipDecals::clear() {
  layer_.removeChildren(0, layer_.getNumChildren());
  counts_.fill(0);
}

void ShipDecals::updateTitle() {
  const char *names[] = {"Soot", "Bullet hole", "Scorch"};
  osgViewer::ViewerBase::Windows windows;
  viewer_.getWindows(windows);
  for (auto *window : windows)
    window->setWindowName(
        std::string("020528 Ship Decals | Selected: ") + names[selected_] +
        " | 1 Soot  2 Hole  3 Scorch  C Clear | Click hull, drag to orbit");
}

bool ShipDecals::handle(const osgGA::GUIEventAdapter &event,
                        osgGA::GUIActionAdapter &) {
  if (event.getEventType() == osgGA::GUIEventAdapter::KEYDOWN) {
    if (event.getKey() >= '1' && event.getKey() <= '3') {
      selected_ = unsigned(event.getKey() - '1');
      updateTitle();
      return true;
    }
    if (event.getKey() == 'c' || event.getKey() == 'C') {
      clear();
      return true;
    }
  }
  if (event.getEventType() == osgGA::GUIEventAdapter::PUSH &&
      event.getButton() == osgGA::GUIEventAdapter::LEFT_MOUSE_BUTTON) {
    down_.set(event.getX(), event.getY());
    trackingClick_ = true;
  }
  if (event.getEventType() == osgGA::GUIEventAdapter::DRAG &&
      (down_ - osg::Vec2(event.getX(), event.getY())).length2() > 25.f)
    trackingClick_ = false;
  if (event.getEventType() == osgGA::GUIEventAdapter::RELEASE &&
      event.getButton() == osgGA::GUIEventAdapter::LEFT_MOUSE_BUTTON) {
    if (trackingClick_ &&
        (down_ - osg::Vec2(event.getX(), event.getY())).length2() <= 25.f)
      stamp(event);
    trackingClick_ = false;
  }
  return false;
}

void ShipDecals::stamp(const osgGA::GUIEventAdapter &event) {
  osgUtil::LineSegmentIntersector::Intersections intersections;
  if (!viewer_.computeIntersections(event, intersections, ShipMask))
    return;
  const auto &hit = *intersections.begin();
  stampAt(hit.getWorldIntersectPoint(),
          osg::Vec3d(hit.getWorldIntersectNormal()), selected_);
}

bool ShipDecals::stampAt(const osg::Vec3d &point, osg::Vec3d normal,
                         unsigned style) {
  if (style >= textures_.size())
    return false;
  if (normal.normalize() < .001)
    return false;
  constexpr double radii[] = {1.8, .84, 1.33};
  auto patch = buildPatch(ship_, point, normal, radii[style]);
  if (!patch)
    return false;
  osg::ref_ptr<osg::Geode> geode = new osg::Geode;
  geode->addDrawable(patch);
  auto *state = geode->getOrCreateStateSet();
  state->setAttributeAndModes(program_);
  state->setTextureAttribute(0, textures_[style]);
  state->addUniform(new osg::Uniform("stamp", 0));
  state->setMode(GL_BLEND, osg::StateAttribute::ON);
  state->setAttributeAndModes(
      new osg::BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
  state->setAttributeAndModes(
      new osg::Depth(osg::Depth::LEQUAL, 0., 1., false));
  state->setRenderingHint(osg::StateSet::TRANSPARENT_BIN);
  layer_.addChild(geode);
  ++counts_[style];
  return true;
}
