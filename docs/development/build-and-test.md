# 构建、运行与验收

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

默认模式中两艘船的中心相距 1,000 米，分别从原来正对的方向偏转 45°，舰艏方向相差 90°。`--moving` 让第二艘船缓慢横移。自动验收检查 Triton 是否初始化、同一机位两帧的海面区域是否发生变化、炮弹飞行和落水阶段是否触发、两艘船是否各获得烧蚀贴花，以及贴花是否随第二艘船移动。设置 `OCEAN_CAPTURE_DIR` 后会保存两张海面帧，以及飞行、舰体爆炸、残烟和水柱帧。日志包含仿真时间、发射方、弹丸编号和场景坐标。已有的单船桥视角模式由 `OpenPrototype.cmd` 调用外部 InfraredPreview 原型启动。

## RTS 战场相机

运行 `.\OpenOceanGambit.cmd` 默认进入 RTS 战场视角。初始观察中心是双舰场景原点，朝向正北，从水平海面向下看 60°。

| 输入 | 操作 |
| --- | --- |
| 左键单击 | 选中己方舰船，点空处清空 |
| 左键拖动 | 框选投影中心落在框内的己方舰船 |
| 右键点击海面 | 让选中的己方舰船驶往该点，绿色圆环短暂标记目的地 |
| 中键拖动 | 以鼠标下的海面点拖动战场 |
| 鼠标移到窗口边缘 | 边缘滚屏，角落支持对角移动 |
| 鼠标滚轮 | 向前拉远、向后靠近，保持鼠标下的海面点 |
| Alt + 左键拖动 | 水平旋转及调整俯角 |
| Home | 恢复正北和默认俯角，保留位置与缩放 |
| F6 | 在 RTS 和舰船调试视角之间切换，返回时恢复 RTS 状态 |

`--no-edge-scroll` 关闭边缘滚屏，`--follow` 直接进入舰船调试视角，`--overview` 使用原固定全景。默认 RTS 场景有六艘 020528，双方各三艘。选中舰船显示青色圆环。己方舰船独立航行，新的右键命令覆盖目的地。对方保持静止。RTS 模式停用旧自动射击脚本，`--overview` 和 `--check` 保留独立双舰演示。

运行 `.\OpenOceanGambit.cmd --check-rts-camera` 验证海面锚点、俯角和距离边界、边缘滚屏、Home、模式往返、己方选择与右键航行。可设置 `OCEAN_CAPTURE_DIR` 保存 `camera-rts.png` 和 `camera-rts-selected.png`。

## 跟随相机与望远镜

舰船调试视角默认跟随一号船，按 `1` 或 `2` 可切换跟随对象。按住 **Alt** 移动鼠标调整观察方向，松开保留新角度。按住鼠标右键临时环顾，松开弹回之前的角度。按一次 **Shift** 进入望远镜并用鼠标调整视线，再按一次退出。望远镜隐藏所跟随舰船的 `ChuanLou` 渲染节点。两种视角共享局部偏航和俯仰，切换观察点时近处物体可能发生视差。准星绿色表示射线命中船模，白色表示找到 Z=0 海平面，灰色表示没有有效目标。

`config/ShipCameras.json` 按 `model_file`（模型文件名）配置并部署到 `bin/ShipCameras.json`。可通过 `OCEAN_CAMERA_CONFIG` 指定自用 JSON。舰桥观察点来自舰型定义 `views` 中 id 为 `bridge` 的条目。坐标相对于模型原点，+Y 指向舰艏，+Z 向上。

| JSON 字段 | 单位或含义 |
| --- | --- |
| 舰型定义中的 `views[bridge].eye` | 舰桥观察点的局部 X/Y/Z，米 |
| `follow.distance` / `follow.height` | 跟随相机后拉距离和高度，米 |
| `follow.pitch_deg` / `follow.fov_deg` | 初始俯仰角和垂直视场角，度 |
| `follow.smoothing_seconds` | 位移平滑时间，秒，0 为关闭 |
| `binocular.fov_deg` | 望远镜垂直视场角，度 |

新增船型需要在舰型定义中填入舰桥观察点，并在相机配置中登记模型文件名及望远镜 FOV。缺少有效配置时仍可跟随，Shift 望远镜不可用。

```powershell
.\OpenOceanGambit.cmd --moving
.\OpenOceanGambit.cmd --overview
.\OpenOceanGambit.cmd --check-camera --moving
```

`--overview` 恢复原双船全景，原有 `--check` 也沿用全景验收。相机验收覆盖舰艏方向、Shift 切换、鼠标旋转、平移和转向跟随、舰体和海面取点以及无效射线。玩家视角下一号船只在左键点击时发射，炮塔按已锁定的观察方向直接转动，弹丸从炮口出发。右键临时环顾期间，左键使用环顾前的方向。二号船仍由脚本控制。`--overview` 和原有 `--check` 使用双船脚本。

## 构建

本机使用 Visual Studio 18 2026 x64 和 CMake 4.2。

```powershell
& 'C:/Program Files/CMake/bin/cmake.exe' -S . -B build -G 'Visual Studio 18 2026' -A x64
& 'C:/Program Files/CMake/bin/cmake.exe' --build build --config Release
```

双船窗口编译项目内的 `external/InfraredPreview/` 和 `external/Veylaxis/` 源码副本，分别来自 perevia-osgkit 的 InfraredPreview 原型和 Veylaxis 的 Triton 适配层。两艘船分别通过原型使用的 DMU 读取器加载。Qt 提供 OpenGL Compatibility 上下文和 FBO，Veylaxis 的状态恢复代码在 Triton 绘制后恢复 OSG 使用的状态。

Release 构建将可执行文件放入 `bin/`，并部署 Qt 平台插件、OSG/DMU 的 DLL、Triton 后端和资源。可直接运行 `bin/OceanGambit.exe`，也可使用根目录的启动脚本。

CMake 缓存项 `OSG_DIR`、`OPENGL_HEADERS`、`OSG_RUNTIME`、`QT_RUNTIME`、`VEYLAXIS` 和 `TRITON_SDK` 可覆盖构建与部署来源。双舰演示使用本地舰型定义。首次检出仓库时，请将已转换的 `020528.json` 放入 `config/ships/`，构建部署会将它复制到 `bin/ships/`。可在没有这份配置时编译，运行双舰场景前需提供该文件。可通过 `OCEAN_SHIP_CONFIG_DIR` 指定其他舰型目录。模型默认从 `OCEAN_SHIP_MODEL_ROOT`（缺省为 `D:/Dragon/_Data`）下的分类/舰型目录加载，也可通过 `OCEAN_MODEL` 指定同名模型文件。读取设置及许可证优先使用进程环境，其次读取 `OCEAN_CONFIG` 指定的本机配置。

演示脚本只提供射击时机和瞄准偏角。弹丸按 0.01 秒步进，默认使用游戏调校的下坠和指数速度阻尼（可切换平方阻力），散布在出膛时由同一随机种子生成。判定使用当步舰船网格和 Z=0 海面，跳过发射舰并选取线段上最先接触的位置。命中后沿用贴花、爆炸和水柱特效。参数是游戏调校值。

构建完成后，可用 `ctest --test-dir build -C Release --output-on-failure` 检查下坠、阻尼、散布复现和单次接触。窗口验收还会检查静止或移动场景中的实际舰体接触与落水。
