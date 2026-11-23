#include "ImpactExplosion.h"

#include <osg/Billboard>
#include <osg/BlendFunc>
#include <osg/Depth>
#include <osg/Geometry>
#include <osg/Program>

#include <algorithm>
#include <cmath>

namespace {
osg::Program* explosionProgram() {
    static osg::ref_ptr<osg::Program> program = [] {
        auto* shader = new osg::Program;
        shader->addShader(new osg::Shader(osg::Shader::VERTEX, R"(#version 330
in vec4 osg_Vertex;
in vec4 osg_MultiTexCoord0;
uniform mat4 osg_ModelViewProjectionMatrix;
out vec2 uv;
void main() {
    gl_Position = osg_ModelViewProjectionMatrix * osg_Vertex;
    uv = osg_MultiTexCoord0.xy;
})"));
        shader->addShader(new osg::Shader(osg::Shader::FRAGMENT, R"(#version 330
in vec2 uv;
uniform vec4 tint;
out vec4 fragmentColor;

float hash(vec2 cell) {
    return fract(sin(dot(cell, vec2(127.1, 311.7))) * 43758.5453);
}
float noise(vec2 p) {
    vec2 cell = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash(cell), hash(cell + vec2(1,0)), f.x),
               mix(hash(cell + vec2(0,1)), hash(cell + vec2(1,1)), f.x), f.y);
}
void main() {
    vec2 p = uv * 2.0 - 1.0;
    float radius = length(p);
    float cloud = 0.7 * noise(4.0 * p + 7.0) + 0.3 * noise(12.0 * p);
    float shape = 1.0 - smoothstep(0.24, 0.95, radius + 0.24 * (cloud - 0.5));
    float opacity = tint.a * shape * (0.65 + 0.35 * cloud);
    if (opacity < 0.012) discard;
    fragmentColor = vec4(tint.rgb * (0.65 + 0.5 * cloud), opacity);
})"));
        return shader;
    }();
    return program.get();
}
}

ImpactExplosion::ImpactExplosion(osg::Group& ship, const osg::Vec3d& localImpact, double start)
    : ship_(&ship), start_(start) {
    anchor_->setMatrix(osg::Matrix::translate(localImpact));
    auto* state = anchor_->getOrCreateStateSet();
    state->setAttributeAndModes(explosionProgram());
    state->setMode(GL_BLEND, osg::StateAttribute::ON);
    state->setMode(GL_CULL_FACE, osg::StateAttribute::OFF);
    state->setAttributeAndModes(new osg::BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
    state->setAttributeAndModes(new osg::Depth(osg::Depth::LEQUAL, 0., 1., false));
    state->setRenderingHint(osg::StateSet::TRANSPARENT_BIN);
    ship.addChild(anchor_);

    addPuff({0, 0, 18}, 0, .24, 15, 38, {1, .96f, .74f, 1});
    for (int i = 0; i < 6; ++i) {
        const double angle = i * osg::PI / 3;
        addPuff({6 * std::cos(angle), 6 * std::sin(angle), 16 + i * 1.4},
                .015 * i, .65, 11, 18, {1, .38f + .06f * (i % 3), .035f, .93f});
        addPuff({3 * std::cos(angle), 3 * std::sin(angle), 17 + i * 1.1},
                .19 + .035 * i, 2.25, 12, 15, {.24f, .23f, .22f, .72f});
    }
}

void ImpactExplosion::addPuff(const osg::Vec3d& drift, double delay, double lifetime,
                              double size, double growth, const osg::Vec4& color) {
    auto node = new osg::MatrixTransform;
    auto billboard = new osg::Billboard;
    billboard->setMode(osg::Billboard::POINT_ROT_EYE);
    auto* quad = osg::createTexturedQuadGeometry({-.5f, 0, -.5f}, {1, 0, 0}, {0, 0, 1});
    quad->setUseDisplayList(false);
    quad->setUseVertexBufferObjects(true);
    billboard->addDrawable(quad);
    node->addChild(billboard);
    auto tint = new osg::Uniform("tint", color);
    node->getOrCreateStateSet()->addUniform(tint);
    anchor_->addChild(node);
    puffs_.push_back({drift, delay, lifetime, size, growth, node, tint, color});
}

bool ImpactExplosion::advance(double now) {
    const double time = now - start_;
    bool active = false;
    for (auto& puff : puffs_) {
        const double age = time - puff.delay;
        const bool visible = age >= 0 && age < puff.lifetime;
        active |= age < puff.lifetime;
        puff.node->setNodeMask(visible ? ~0u : 0u);
        if (!visible) continue;
        const double size = puff.size + puff.growth * age;
        puff.node->setMatrix(osg::Matrix::scale(size, size, size) *
                             osg::Matrix::translate(puff.drift * time));
        const double remaining = 1 - age / puff.lifetime;
        const double fadeIn = puff.delay >= .19 ? std::min(age / .16, 1.) : 1.;
        auto color = puff.color;
        color.a() *= static_cast<float>(remaining * remaining * fadeIn);
        puff.tint->set(color);
    }
    if (!active && ship_.valid()) ship_->removeChild(anchor_);
    return active;
}
