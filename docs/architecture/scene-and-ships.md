# 场景与舰船架构

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

## RTS 相机边界

`RTSCameraController` 持有海面观察中心、距离、航向和俯角，以及集中配置的缩放、阻尼、旋转和边缘滚屏参数。世界距离以米为单位，角度以度为单位，滚屏边缘和旋转输入使用 Qt 逻辑像素。中键拖动和滚轮定位都基于透视反投影与 Z=0 的交点。

`BattleView` 按当前模式转发输入，并在已有 Qt/OSG 帧循环里驱动同一台相机。RTS 控制器独立于舰船和战斗逻辑。模式切换只切换输入路由和相机控制权，保留 RTS 状态。
