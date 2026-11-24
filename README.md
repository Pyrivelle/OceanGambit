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

默认模式中两艘船的中心相距 1,000 米，分别从原来正对的方向偏转 45°，舰艏方向相差 90°。`--moving` 让第二艘船缓慢横移。自动验收检查 Triton 是否初始化、同一机位两帧的海面区域是否发生变化、炮弹飞行和落水阶段是否触发、两艘船是否各获得烧蚀贴花，以及贴花是否随第二艘船移动。设置 `OCEAN_CAPTURE_DIR` 后会保存两张海面帧，以及飞行、舰体爆炸、残烟和水柱帧。日志包含仿真时间、发射方、弹丸编号和场景坐标。已有的单船桥视角模式由 `OpenPrototype.cmd` 调用外部 InfraredPreview 原型启动。

## 跟随相机与望远镜

默认跟随一号船，按 `1` 或 `2` 可切换跟随对象。按住 **Alt** 移动鼠标调整观察方向，松开保留新角度。按住鼠标右键临时环顾，松开弹回之前的角度。按一次 **Shift** 进入望远镜并用鼠标调整视线，再按一次退出。望远镜隐藏所跟随舰船的 `ChuanLou` 渲染节点。两种视角共享局部偏航和俯仰，切换观察点时近处物体可能发生视差。准星绿色表示射线命中船模，白色表示找到 Z=0 海平面，灰色表示没有有效目标。

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

## 场景骨架

`main.cpp` 负责启动应用。`BattleView` 提供 Qt/OpenGL 窗口并驱动帧循环。`BattleScene` 创建场景根节点，然后调用 `loadSky()`、`loadOcean()` 和 `loadShipManager()`。

- `render/SkyLoader.cpp` 设置演示的天空背景。
- `render/OceanLoader.cpp` 加载 Triton 海面并配置水位。
- `ShipManager` 通过 `addShip(code)` 缓存舰型定义并创建独立的 DMU 实例。
- `ShipController` 管理模型世界姿态、舰桥和炮塔。旧场景使用第一座炮塔的第一根炮管，各炮管的局部炮口和方向独立存储。共享俯仰路径只建立一个控制器。
- `NodeController` 验证从加载根节点开始的 `/n0/...` 索引和末端名称。普通节点使用模型空间的 `pivot`、`axis`、`sign`，原生 DOF 则由其 HPR 变换控制。
- `battle/DemoScenario.cpp` 提供船位和射击事件。`render/NavalEffects.cpp` 调用现有原型播放特效及投射贴花。

舰型回归使用 `NodeControllerCheck` 验证普通节点、DOF 和索引路径。构建 Release 后，可用本地模型检查零位炮口、偏航、俯仰、两艘船实例隔离及共享俯仰节点。

```powershell
$env:PATH = "$PWD/bin;$env:PATH"
.\build\Release\NodeControllerCheck.exe --020528 "$PWD/bin"
.\OpenOceanGambit.cmd --check
.\OpenOceanGambit.cmd --check-camera --moving
```

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

## 可切换弹药阻力

默认保留原始指数阻尼演示参数：初速 670 m/s、下坠 16 m/s²、指数衰减 0.06 s⁻¹。
使用 `--drag-quadratic` 切换平方空气阻力，重力采用 9.80665 m/s²、等效阻力系数 `0.06/670 m⁻¹`，初速仍为 670 m/s。平方阻力的系数只是与原模型在初速处匹配阻力减速度的过渡值，并非实测炮弹参数。

```powershell
.\OpenOceanGambit.cmd --drag-quadratic
.\OpenOceanGambit.cmd --check --moving --drag-quadratic
ctest --test-dir build -C Release --output-on-failure
```

`AmmoConfig` 可按弹药类型配置初速、下坠、阻力模型、散布与寿命；旧 `ProjectileTuning` 名称保留为兼容别名。炮塔仰角现在依据当前目标位置和相同弹道积分器求近似低弧解，暂未加入移动目标提前量。散布仍使用带种子的随机发射角，保持回放可复现。

## wows_shell 参考计算器（实验性）

`third_party/wows_shell/` 保留 jcw780/wows_shell 的原始 MIT 授权头文件和许可证（来源：`master` 分支）；`battle/WowsShellAdapter.h/.cpp` 用独立接口演示对其弹道计算的调用。对应的 `WowsShellCheck` 测试通过 CTest 注册。

此 PR **不改变** `ProjectileSystem`、场景弹丸、随机散布或默认物理模型；适配器不进入实时战斗循环。可生成不同仰角下的射程、时间、末速、基础穿深样本，为之后的数值比较、火控表和穿甲规则替换提供入口。

`wows_shell` 的穿深/Krupp、Sigma/散布、引信/跳弹规则仍保留在原始库中，但**没有启用装甲判定**；当前战斗碰撞结算照旧。其游戏时间倍率不用于适配器输出（直接读取真实秒数 `timeToTarget`）。原库的空气动力学/弹道尺度是 WoWS 模型，并不自动等于真实海战弹道模型。详情见 `third_party/wows_shell/INTEGRATION.md`。

```powershell
cmake --build build --config Release --target WowsShellCheck
ctest --test-dir build -C Release --output-on-failure -R WowsShellCheck
```

## 实时 WoWS 风格弹道（实验开关）

`--ballistics-wows` **实际切换实时弹丸运动** 到从 `wows_shell` 源码移植的三维逐步大气阻力计算。海面与舰体的线段碰撞仍每 0.01 秒进行一次。注意：第三方 `wows_shell` 原始 API 是批量射角模拟器，无法对每颗运动弹丸增量推进；因此 `battle/WowsMotion.h` 移植了它的 ISA 气压/密度和固定 Cd 二次阻力微分方程，同时将数值积分器用于 3D 运动。它是独立端口，不是直接调用 `shellCalc::calculateImpact` 来推进弹丸；该库依然用于批量基准计算。

```powershell
.\OpenOceanGambit.cmd --ballistics-wows
.\OpenOceanGambit.cmd --ballistics-wows --integrator-euler
.\OpenOceanGambit.cmd --ballistics-wows --integrator-rk2
.\OpenOceanGambit.cmd --ballistics-wows --integrator-ab5
.\OpenOceanGambit.cmd --check --moving --ballistics-wows
cmake --build build --config Release --target WowsMotionCheck
ctest --test-dir build -C Release --output-on-failure
```

默认仍是旧指数阻尼演示模型，以防破坏历史演示和回归验收；`--drag-quadratic` 仍保持原效果。WoWS 模式默认 RK4，其他积分器可切换。该模式使用海平面标准大气、固定阻力系数、真实秒数与米单位。演示弹药参数（152.4 mm、45 kg、670 m/s、Cd=0.3）仅供测试，并非真实舰炮数据。弹道射角反解复用实时推进器，范围搜索失败时跳过发射，不假设任何距离都可达；射角仍是给静止目标的低弧解。

WoWS 风格散布使用截断正态采样（`sigma=2.0`，可重放种子），目前仍是在发射方向上的角度扰动，**不等于完整的 WoWS 散布椭圆/距离分布**。第三方穿深和引信算法继续保留，尚未接入实际结算；没有装备装甲带模型时沿用现有碰撞特效。暂未实现交互式鼠标指定落点、AI 目标提前量、滚转补偿或预计算射角表。

## 鼠标火控反解（feature/vendor-wows-shell-adapter）

玩家左键发射时，先从**已锁定**观察方向（右键临时环顾不会改变它）及相机位置投射射线，取得舰体或 Z=0 海面世界坐标。若未命中有效目标，直接取消发射，不再把观察方向当作炮弹的直接初速度方向。所选舰船的实际炮口位置用于反求低弧仰角，反复校正炮口姿态造成的炮口位置偏移，最终从实际炮口发射；随舰船切换（数字键 1、2）更新控制舰船及脚本射击排除对象。

目前这是**静止目标点的弹道反解**。目标移动提前量、船体速度导致的细微弹道偏差、炮塔转速、射击散布以及舰桥到炮口的遮挡差异，仍需后续完善。可查看终端的 `fire_control ship=... elevation_deg=... target=... muzzle=...` 日志。既有 `ProjectileCheck` 新增了关闭散布时反解与实弹同积分器的落点一致性测试。

## 按舰型配置的高抛／低抛火控

舰船使用 `ShipWeapons.json` 的 `ships[].gun.arc` 决定默认弹道分支，接受 `low`（默认）和 `high`；**没有运行时按键切换**。可用环境变量 `OCEAN_WEAPON_CONFIG` 指定配置文件路径，或者编辑运行目录 `bin/ShipWeapons.json`（构建时由 `config/ShipWeapons.json` 部署）。可设置 `min_elevation_deg` 和 `max_elevation_deg` 限制射角。

反解器现在在允许的炮口仰角范围内搜索高度误差与零的全部交点：优先选择最小角度的低抛解或最大角度的高抛解；当舰船指定高抛模式时必须存在独立的第二个解，不会悄悄回退到低抛。没有对应的有效解则拒绝发射，并输出日志。玩家和脚本射击均读取同一舰船配置。

高抛可能飞行很久，因此 WoWS 实验弹药寿命由 30 秒提高到 120 秒（仅该模式）；这只是演示上限，不代表真实炮弹寿命。某些距离、弹药阻力、飞行时限或最大炮口仰角约束下，根本不存在有效的高抛射击解。

目前船只型号按 `model_file` 匹配，日后多炮塔或多弹种应将偏好下沉到每一门武器配置。注意目前火控仍忽略舰船初始速度的反解修正，不保证移动舰船的精确落点；炮口与目标坐标校正保留。

## 火控诊断与高抛近距离回退

对于配置 `gun.arc=high` 的舰船，优先求两个独立弹道根中的高抛解；当目标距离下只找到低抛解时，**自动使用低抛并正常发射**，输出 `high_arc_unavailable_fallback_low`。默认低抛设置不受影响。旧 `solveElevation(..., FireArc::High, ...)` 严格接口仍可用于查询是否真正存在高抛根；新 `fireControlSolution(...)` 返回实际选用角度与诊断状态。

无有效解时，火控输出结构化原因：`out_of_range`（按弹药运动模型和寿命超出可达水平射程）、`elevation_limit`（物理解存在但落在舰炮仰角范围外）、`no_ballistic_solution`（其他无法求解状态）、`invalid_target`（输入/参数无效）。`out_of_range` 的极限射程由失败时的数值采样近似判断，精度不保证在最大射程切点附近绝对准确。敌方舰体或地形的**飞行途中遮挡/撞击**仍由实际碰撞模块判定，不与弹道射程诊断混为一谈。

新增回归测试覆盖 1000 m 的高抛回退、角度受限、超射程、非法目标，以及已有双解计算。

## 火控行为整合（自由射击／舰速补偿）

- `pickTarget` 命中舰体或海面时仍进行目标点火控反解；抬头对空、没有有效交点时，按相机锁定方向从实体炮口**自由射击**，不再取消开火。右键临时观察仍不改变锁定射向。
- 自由射击将俯仰角限制在 `ShipWeapons.json` 的最小/最大仰角内；它不会把超过限制的自由射向误认为射程不足。
- 武器配置新增 `compensate_ship_motion`，默认 `true`。开启时玩家/脚本目标点火控对舰船平移速度使用弹道反解器返回的**预计飞行时间迭代修正**；关闭后玩家可自行估计修正量。物理发射始终继承舰船速度，此开关不改变实际弹丸运动。该算法考虑舰船当前平移速度，不包含目标运动提前量、船体转动与姿态变化。
- `high` 是优先分支，无高抛解仍允许低抛回退；角度与距离失败保持各自诊断日志。

### 验收清单
- [x] 无有效目标交点时改为自由发射
- [x] 自由射击按武器俯仰配置限制
- [x] 舰速补偿支持配置开关（初阶估计）
- [x] 玩家与脚本射击使用同一舰船配置
- [ ] 在 Windows 上编译并运行自动测试
- [ ] 在实际舰船模型上手动确认炮管俯仰方向与发射方向一致
- [ ] 检查移动目标的自动提前量（未来功能）
