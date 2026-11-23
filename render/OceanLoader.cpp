#include "OceanLoader.h"

#include <QCoreApplication>
#include <QDir>
#include <stdexcept>

osg::ref_ptr<TritonOceanSurface> loadOcean(osg::Group& root) {
    const auto resources = QDir(qEnvironmentVariable("VEYLAXIS_RUNTIME", QCoreApplication::applicationDirPath()))
        .filePath("resource/Triton/Resources");
    if (!QDir(resources).exists())
        throw std::runtime_error("Triton resources unavailable");
    TritonOceanSurfaceOptions options;
    options.resourcePath = resources;
    options.windSpeedMetersPerSecond = 8.0;
    osg::ref_ptr<TritonOceanSurface> ocean = new TritonOceanSurface(options);
    ocean->setSeaLevel(0);
    root.addChild(ocean);
    return ocean;
}
