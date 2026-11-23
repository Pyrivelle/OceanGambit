#pragma once

#include <osg/MatrixTransform>
#include <osg/Uniform>
#include <osg/observer_ptr>

#include <vector>

class ImpactExplosion {
public:
    ImpactExplosion(osg::Group& ship, const osg::Vec3d& localImpact, double start);
    bool advance(double now);

private:
    struct Puff {
        osg::Vec3d drift;
        double delay, lifetime, size, growth;
        osg::ref_ptr<osg::MatrixTransform> node;
        osg::ref_ptr<osg::Uniform> tint;
        osg::Vec4 color;
    };

    void addPuff(const osg::Vec3d& drift, double delay, double lifetime,
                 double size, double growth, const osg::Vec4& color);

    osg::observer_ptr<osg::Group> ship_;
    osg::ref_ptr<osg::MatrixTransform> anchor_ = new osg::MatrixTransform;
    double start_;
    std::vector<Puff> puffs_;
};
