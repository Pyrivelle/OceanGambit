# Use the validated DMU runtime and the matching Qt release deployment.
file(COPY "${OSG_RUNTIME}/osgPlugins-3.6.5" DESTINATION "${BIN}")
file(GLOB osg_runtime_dlls "${OSG_RUNTIME}/*.dll")
list(FILTER osg_runtime_dlls EXCLUDE REGEX "/Qt[^/]*\\.dll$")
file(COPY ${osg_runtime_dlls} DESTINATION "${BIN}")

foreach(module Core Gui Widgets)
    file(COPY "${QT_RUNTIME}/Qt5${module}.dll" DESTINATION "${BIN}")
endforeach()
file(COPY "${QT_RUNTIME}/platforms/qwindows.dll" DESTINATION "${BIN}/platforms")

set(resources "${TRITON_SDK}/Resources")
file(COPY "${resources}/" DESTINATION "${BIN}/resource/Triton/Resources"
    PATTERN "dll" EXCLUDE
    PATTERN "dll64" EXCLUDE
    PATTERN "vc14" EXCLUDE)
file(GLOB backends "${resources}/vc14/x64/Triton*-MT-DLL.dll")
file(GLOB dependencies "${resources}/dll64/*.dll")
list(FILTER dependencies EXCLUDE REGEX "/vcomp90d\\.dll$")
file(COPY ${backends} DESTINATION "${BIN}/Triton/vc14/x64")
file(COPY ${backends} ${dependencies} DESTINATION "${BIN}")
if(CRT_DLLS)
    file(COPY ${CRT_DLLS} DESTINATION "${BIN}")
endif()
