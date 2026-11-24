#include "InfraredPreview.h"
#include <cmath>
#include <osg/AlphaFunc>
#include <osg/Geode>
#include <osg/Geometry>
#include <osg/Material>
#include <osg/Texture>
#include <osg/Transform>
#include <set>
#include <stdexcept>

namespace Pyrive {
namespace OsgKit {
namespace {
const char *vertex = R"(#version 330
in vec4 osg_Vertex; in vec3 osg_Normal; in vec4 osg_Color; in vec4 osg_MultiTexCoord0;
uniform mat4 osg_ModelViewProjectionMatrix; uniform mat3 osg_NormalMatrix;
uniform mat4 ir_localToModel;
out vec3 modelPosition; out vec3 modelNormal; out vec3 normal; out vec2 uv; out vec4 vertexColor;
void main(){ gl_Position=osg_ModelViewProjectionMatrix*osg_Vertex;
modelPosition=(ir_localToModel*osg_Vertex).xyz; normal=osg_NormalMatrix*osg_Normal;
modelNormal=transpose(inverse(mat3(ir_localToModel)))*osg_Normal;
uv=osg_MultiTexCoord0.xy;vertexColor=osg_Color; }
)";
const char *fragment = R"(#version 330
in vec3 modelPosition; in vec3 modelNormal; in vec3 normal; in vec2 uv; in vec4 vertexColor;
uniform int ir_mode; uniform vec4 ir_color; uniform bool ir_texture,ir_vertexColor;
uniform sampler2D ir_texture0; uniform float ir_alphaCutoff;
uniform float ir_baseK,ir_minK,ir_maxK,ir_heatDeltaK;
uniform bool ir_isMarker; uniform vec3 ir_heatCenter,ir_heatRadius;
uniform float ir_surfaceVariationK;
out vec4 fragmentColor;
vec3 palette(float t){
 if(t<.25)return mix(vec3(.015,.015,.10),vec3(.28,.04,.53),t*4.);
 if(t<.5)return mix(vec3(.28,.04,.53),vec3(.88,.12,.22),(t-.25)*4.);
 if(t<.75)return mix(vec3(.88,.12,.22),vec3(1.,.63,.08),(t-.5)*4.);
 return mix(vec3(1.,.63,.08),vec3(1.,1.,.85),(t-.75)*4.);
}
void main(){
 vec4 color=ir_vertexColor?vertexColor:ir_color;
 if(ir_texture)color*=texture(ir_texture0,uv);
 if(color.a<=ir_alphaCutoff)discard;
 if(ir_mode==0){float light=length(normal)>.001?.55+.45*abs(dot(normalize(normal),normalize(vec3(.3,-.4,1.)))):1.;fragmentColor=vec4(color.rgb*light,color.a);return;}
 // Original RGB does NOT define temperature; texture is used only for cutout alpha.
 vec3 q=(modelPosition-ir_heatCenter)/ir_heatRadius;
 if(ir_isMarker)discard;
 float temp=ir_baseK+ir_heatDeltaK*exp(-.5*dot(q,q));
 float top=length(modelNormal)>.001?abs(normalize(modelNormal).z):0.;
 temp+=ir_surfaceVariationK*(top+.2*sin(modelPosition.x*.18)*cos(modelPosition.y*.11));
 float t=pow(clamp((temp-ir_minK)/(ir_maxK-ir_minK),0.,1.),.65);
 vec3 result=ir_mode==3?palette(t):vec3(ir_mode==2?1.-t:t);
 fragmentColor=vec4(result,1.);
}
)";
bool legacy(GLenum m) {
  return m == 0x0B50 || (m >= 0x4000 && m <= 0x4007) || m == 0x0B57 ||
         m == 0x0BA1 || m == 0x803A || m == 0x0BC0 || m == 0x0B60 ||
         m == 0x0DE0 || m == 0x0DE1 || m == 0x806F || m == 0x84F5 ||
         m == 0x8513 || (m >= 0x0C60 && m <= 0x0C63);
}
void clean(osg::StateSet *s) {
  if (!s)
    return;
  auto modes = s->getModeList();
  for (auto p : modes)
    if (legacy(p.first))
      s->removeMode(p.first);
  auto tm = s->getTextureModeList();
  for (unsigned i = 0; i < tm.size(); ++i)
    for (auto p : tm[i])
      if (legacy(p.first))
        s->removeTextureMode(i, p.first);
  s->removeAttribute(osg::StateAttribute::MATERIAL);
  for (unsigned i = 0; i < s->getTextureAttributeList().size(); ++i) {
    s->removeTextureAttribute(i, osg::StateAttribute::TEXENV);
    s->removeTextureAttribute(i, osg::StateAttribute::TEXGEN);
  }
}
struct Prepare : osg::NodeVisitor {
  using States = std::vector<
      std::pair<osg::ref_ptr<osg::StateSet>, std::vector<std::string>>>;
  osg::Program *program;
  States &states;
  std::vector<osg::ref_ptr<osg::StateSet>> inherited;
  Prepare(osg::Program *p, States &s)
      : NodeVisitor(TRAVERSE_ALL_CHILDREN), program(p), states(s) {
    setNodeMaskOverride(~0u);
  }
  void apply(osg::Node &n) override {
    osg::ref_ptr<osg::StateSet> e =
        inherited.empty()
            ? new osg::StateSet
            : new osg::StateSet(*inherited.back(), osg::CopyOp::SHALLOW_COPY);
    if (n.getStateSet())
      e->merge(*n.getStateSet());
    inherited.push_back(e);
    traverse(n);
    inherited.pop_back();
    clean(n.getStateSet());
  }
  void apply(osg::Geode &n) override {
    osg::ref_ptr<osg::StateSet> e =
        inherited.empty()
            ? new osg::StateSet
            : new osg::StateSet(*inherited.back(), osg::CopyOp::SHALLOW_COPY);
    if (n.getStateSet())
      e->merge(*n.getStateSet());
    std::vector<std::string> names;
    bool marker = false;
    for (auto *p : getNodePath()) {
      names.push_back(p->getName());
      marker = marker || p->getName() == "GPS";
    }
    for (unsigned i = 0; i < n.getNumDrawables(); ++i) {
      auto *g = n.getDrawable(i)->asGeometry();
      if (!g)
        continue;
      osg::ref_ptr<osg::StateSet> s =
          new osg::StateSet(*e, osg::CopyOp::SHALLOW_COPY);
      if (g->getStateSet())
        s->merge(*g->getStateSet());
      auto *m = dynamic_cast<osg::Material *>(
          s->getAttribute(osg::StateAttribute::MATERIAL));
      const auto c = m ? m->getDiffuse(osg::Material::FRONT)
                       : osg::Vec4(.7f, .72f, .74f, 1);
      auto *a = dynamic_cast<osg::AlphaFunc *>(
          s->getAttribute(osg::StateAttribute::ALPHAFUNC));
      const float cutoff = a && (s->getMode(0x0BC0) & osg::StateAttribute::ON)
                               ? a->getReferenceValue()
                               : 0;
      bool tex = s->getTextureAttribute(0, osg::StateAttribute::TEXTURE) &&
                 g->getTexCoordArray(0);
      clean(s);
      s->setAttributeAndModes(program, osg::StateAttribute::ON |
                                           osg::StateAttribute::PROTECTED);
      s->addUniform(new osg::Uniform(
          "ir_localToModel",
          osg::Matrixf(osg::computeLocalToWorld(getNodePath()))));
      s->addUniform(new osg::Uniform("ir_color", c));
      s->addUniform(new osg::Uniform("ir_texture", tex));
      s->addUniform(new osg::Uniform("ir_texture0", 0));
      s->addUniform(
          new osg::Uniform("ir_vertexColor", g->getColorArray() != nullptr));
      s->addUniform(new osg::Uniform("ir_alphaCutoff", cutoff));
      s->addUniform(new osg::Uniform("ir_isMarker", marker));
      g->setStateSet(s);
      g->setUseDisplayList(false);
      g->setUseVertexBufferObjects(true);
      g->setUseVertexArrayObject(true);
      states.emplace_back(s, names);
    }
    clean(n.getStateSet());
  }
};
} // namespace
InfraredPreview::InfraredPreview(osg::Node *source) {
  if (!source)
    throw std::invalid_argument("InfraredPreview source is null");
  node_ = osg::clone(source, osg::CopyOp(osg::CopyOp::DEEP_COPY_NODES |
                                         osg::CopyOp::DEEP_COPY_DRAWABLES |
                                         osg::CopyOp::DEEP_COPY_STATESETS));
  program_ = new osg::Program;
  program_->addShader(new osg::Shader(osg::Shader::VERTEX, vertex));
  program_->addShader(new osg::Shader(osg::Shader::FRAGMENT, fragment));
  Prepare visitor(program_, states_);
  node_->accept(visitor);
  setParameters(Parameters{});
  setMode(Mode::Visible);
}
void InfraredPreview::setMode(Mode mode) {
  for (auto &[s, names] : states_)
    s->addUniform(new osg::Uniform("ir_mode", int(mode)));
}
std::map<std::string, unsigned>
InfraredPreview::setParameters(const Parameters &p) {
  if (!std::isfinite(p.minimumKelvin) || !std::isfinite(p.maximumKelvin) ||
      p.minimumKelvin >= p.maximumKelvin || !std::isfinite(p.hullKelvin) ||
      !std::isfinite(p.cabinKelvin) || !std::isfinite(p.heatDeltaKelvin) ||
      !std::isfinite(p.surfaceVariationKelvin))
    throw std::invalid_argument("Invalid preview temperature range");
  for (unsigned i = 0; i < 3; ++i)
    if (!std::isfinite(p.heatCenter[i]) || !std::isfinite(p.heatRadius[i]) ||
        p.heatRadius[i] <= 0)
      throw std::invalid_argument("Invalid preview heat mask");
  auto temperatures = p.nodeKelvin;
  temperatures.try_emplace("ChuanLou", p.cabinKelvin);
  for (const auto &[name, kelvin] : temperatures)
    if (!std::isfinite(kelvin))
      throw std::invalid_argument("Invalid node temperature: " + name);
  std::map<std::string, unsigned> matches;
  for (auto &[s, names] : states_) {
    float base = p.hullKelvin;
    for (auto name = names.rbegin(); name != names.rend(); ++name) {
      const auto match = temperatures.find(*name);
      if (match == temperatures.end())
        continue;
      base = match->second;
      ++matches[match->first];
      break;
    }
    s->addUniform(new osg::Uniform("ir_baseK", base));
    s->addUniform(new osg::Uniform("ir_minK", p.minimumKelvin));
    s->addUniform(new osg::Uniform("ir_maxK", p.maximumKelvin));
    s->addUniform(new osg::Uniform("ir_heatCenter", p.heatCenter));
    s->addUniform(new osg::Uniform("ir_heatRadius", p.heatRadius));
    s->addUniform(new osg::Uniform("ir_heatDeltaK", p.heatDeltaKelvin));
    s->addUniform(
        new osg::Uniform("ir_surfaceVariationK", p.surfaceVariationKelvin));
  }
  return matches;
}
} // namespace OsgKit
} // namespace Pyrive
