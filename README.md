# OceanGambit

两艘 020528 在同一个 Triton 海面上交替播放游戏射击。OceanGambit 使用 OSG 3.6.5 和 Qt 5 的 OpenGL 窗口。

## 启动与验收

从项目根目录运行。

```powershell
.\OpenOceanGambit.cmd
.\OpenOceanGambit.cmd --moving
$env:OCEAN_CAPTURE_DIR = "$PWD/build/captures"
.\OpenOceanGambit.cmd --check
$env:OCEAN_CAPTURE_DIR = "$PWD/build/captures-moving"
.\OpenOceanGambit.cmd --check --moving
.\OpenPrototype.cmd
```

默认模式中两艘船的中心相距 1,000 米，分别从原来正对的方向偏转 45°，舰艏方向相差 90°。`--moving` 让第二艘船缓慢横移。自动验收检查 Triton 是否初始化、同一机位两帧的海面区域是否发生变化、炮弹飞行和落水阶段是否触发、两艘船是否各获得烧蚀贴花，以及贴花是否随第二艘船移动。设置 `OCEAN_CAPTURE_DIR` 后会保存两张海面帧，以及飞行、舰体爆炸、残烟和水柱帧。日志包含仿真时间、发射方、弹丸编号和场景坐标。已有的单船桥视角模式继续由 `OpenPrototype.cmd` 启动。

## 场景骨架

`main.cpp` 负责启动应用。`BattleView` 提供 Qt/OpenGL 窗口并驱动帧循环。`BattleScene` 创建场景根节点，然后调用 `loadSky()`、`loadOcean()` 和 `loadShipManager()`。

- `render/SkyLoader.cpp` 设置演示的天空背景。
- `render/OceanLoader.cpp` 加载 Triton 海面并配置水位。
- `render/ShipManager.cpp` 提供 `addShip()`，从 DMU 加载独立船体实例，并管理炮塔和位置。
- `battle/DemoScenario.cpp` 提供船位和射击事件。`render/NavalEffects.cpp` 调用现有原型播放特效及投射贴花。

## 构建

本机使用 Visual Studio 18 2026 x64 和 CMake 4.2。

```powershell
& 'C:/Program Files/CMake/bin/cmake.exe' -S . -B build -G 'Visual Studio 18 2026' -A x64
& 'C:/Program Files/CMake/bin/cmake.exe' --build build --config Release
```

双船窗口需要本机的 InfraredPreview 源码，其中 `NavalEffectsPreview` 提供 `beginExternalFlight()`、`sampleExternalFlight()` 和 `finishExternalFlight()` 接口。双船窗口直接编译已有的 `InfraredPreview.cpp`、`NavalEffectsPreview.cpp`、`ShipDecals.cpp` 和 Veylaxis 的 `TritonOceanSurface.cpp`。两艘船分别通过原型使用的 DMU 读取器加载。Qt 提供 OpenGL Compatibility 上下文和 FBO，Veylaxis 的状态恢复代码在 Triton 绘制后恢复 OSG 使用的状态。

Release 构建将可执行文件放入 `bin/`，并部署 Qt 平台插件、OSG/DMU 的 DLL、Triton 后端和资源。可直接运行 `bin/OceanGambit.exe`，也可使用根目录的启动脚本。

CMake 缓存项 `OSG_DIR`、`OPENGL_HEADERS`、`NAVAL_PREVIEW`、`OSG_RUNTIME`、`QT_RUNTIME`、`VEYLAXIS` 和 `TRITON_SDK` 可覆盖构建与部署来源。舰船模型沿用本机 020528，读取设置及许可证优先使用进程环境，其次读取本机配置。可通过 `OCEAN_MODEL` 或 `OCEAN_CONFIG` 指定相应文件。

演示脚本只提供射击时机和瞄准偏角。弹丸按 0.01 秒步进，使用游戏调校的下坠和指数速度阻尼，散布在出膛时由同一随机种子生成。判定使用当步舰船网格和 Z=0 海面，跳过发射舰并选取线段上最先接触的位置。命中后沿用贴花、爆炸和水柱特效。参数是游戏调校值。

构建完成后，可用 `ctest --test-dir build -C Release --output-on-failure` 检查下坠、阻尼、散布复现和单次接触。窗口验收还会检查静止或移动场景中的实际舰体接触与落水。
