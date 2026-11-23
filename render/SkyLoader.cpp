#include "SkyLoader.h"

#include <osg/Camera>

void loadSky(osg::Camera& camera) {
    // The current demo uses the camera background as its sky.
    camera.setClearColor({.55f, .71f, .81f, 1});
}
