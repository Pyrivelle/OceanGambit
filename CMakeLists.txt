cmake_minimum_required(VERSION 3.21)
project(OceanGambit LANGUAGES CXX)
set(OSG_DIR "C:/CProj/third_party/osg-3.6.5-glcore/install" CACHE PATH "OSG SDK")
set(VEYLAXIS "C:/CProj/Veylaxis" CACHE PATH "Veylaxis sources and SDKs")
set(OPENGL_HEADERS "C:/CProj/vcpkg/installed/x64-windows/include" CACHE PATH "Directory containing GL/glcorearb.h")
set(TRITON_SDK "${VEYLAXIS}/third_party/Triton" CACHE PATH "Triton SDK")
find_package(OpenSceneGraph 3.6.5 REQUIRED COMPONENTS osg osgDB osgUtil osgViewer osgGA)
find_package(Qt5 REQUIRED COMPONENTS Widgets PATHS "D:/Qt/Qt5.14.2/5.14.2/msvc2017_64/lib/cmake/Qt5" NO_DEFAULT_PATH)
find_package(OpenGL REQUIRED)

add_executable(OceanGambit
    main.cpp
    app/BattleView.cpp
    app/LocalSettings.cpp
    app/ShipCameraSettings.cpp
    battle/DemoScenario.cpp
    battle/ProjectileSystem.cpp
    render/BattleScene.cpp
    render/ImpactExplosion.cpp
    render/NavalEffects.cpp
    render/OceanLoader.cpp
    render/ShipManager.cpp
    render/SkyLoader.cpp
    render/ShipCameraController.cpp
    "external/InfraredPreview/ShipDecals.cpp"
    "external/InfraredPreview/NavalEffectsPreview.cpp"
    "external/InfraredPreview/InfraredPreview.cpp"
    "external/Veylaxis/src/osg/TritonOceanSurface.cpp")
target_compile_features(OceanGambit PRIVATE cxx_std_17)
target_compile_definitions(OceanGambit PRIVATE VEYLAXIS_GLCORE GLAPI=WINGDIAPI NOMINMAX WIN32_LEAN_AND_MEAN)
target_include_directories(OceanGambit PRIVATE ${OPENSCENEGRAPH_INCLUDE_DIRS} "${OPENGL_HEADERS}"
    "${PROJECT_SOURCE_DIR}/external/InfraredPreview" "${PROJECT_SOURCE_DIR}/external/Veylaxis/include" "${TRITON_SDK}/Public Headers")
target_link_libraries(OceanGambit PRIVATE ${OPENSCENEGRAPH_LIBRARIES} Qt5::Widgets OpenGL::GL
    "${TRITON_SDK}/lib/vc14/x64/Triton-MT-DLL.lib")
target_compile_options(OceanGambit PRIVATE /utf-8)

set(OSG_RUNTIME "C:/CProj/perevia-osgkit/build/infrared-preview-gl3/runtime" CACHE PATH "Validated OSG/DMU runtime")
set(QT_RUNTIME "${VEYLAXIS}/bin/GL3/Release" CACHE PATH "Matching Qt release runtime")
set(BIN "${PROJECT_SOURCE_DIR}/bin")
set_target_properties(OceanGambit PROPERTIES RUNTIME_OUTPUT_DIRECTORY_RELEASE "${BIN}")

set(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_SKIP TRUE)
include(InstallRequiredSystemLibraries)
add_custom_target(OceanGambitRuntime ALL
    COMMAND "${CMAKE_COMMAND}"
        "-DBIN=$<TARGET_FILE_DIR:OceanGambit>"
        "-DOSG_RUNTIME=${OSG_RUNTIME}"
        "-DQT_RUNTIME=${QT_RUNTIME}"
        "-DTRITON_SDK=${TRITON_SDK}"
        "-DCRT_DLLS=${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS}"
        -P "${PROJECT_SOURCE_DIR}/cmake/DeployRuntime.cmake"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${PROJECT_SOURCE_DIR}/config/ShipCameras.json" "$<TARGET_FILE_DIR:OceanGambit>/ShipCameras.json"
    DEPENDS OceanGambit
    VERBATIM)

include(CTest)
if(BUILD_TESTING)
    add_executable(ProjectileCheck tests/ProjectileCheck.cpp battle/ProjectileSystem.cpp)
    target_compile_features(ProjectileCheck PRIVATE cxx_std_17)
    target_include_directories(ProjectileCheck PRIVATE ${OPENSCENEGRAPH_INCLUDE_DIRS} "${OPENGL_HEADERS}")
    add_test(NAME ProjectileCheck COMMAND ProjectileCheck)

    # Standalone, OSG-free third-party comparison test; not used by the battle loop.
    add_executable(WowsShellCheck tests/WowsShellCheck.cpp battle/WowsShellAdapter.cpp)
    target_compile_features(WowsShellCheck PRIVATE cxx_std_17)
    target_include_directories(WowsShellCheck PRIVATE "${PROJECT_SOURCE_DIR}")
    if(MSVC)
        target_compile_options(WowsShellCheck PRIVATE /utf-8)
    endif()
    add_test(NAME WowsShellCheck COMMAND WowsShellCheck)

    add_executable(WowsMotionCheck tests/WowsMotionCheck.cpp)
    target_compile_features(WowsMotionCheck PRIVATE cxx_std_17)
    add_test(NAME WowsMotionCheck COMMAND WowsMotionCheck)
endif()
