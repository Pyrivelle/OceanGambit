# 场景与舰船架构

## 场景骨架

`main.cpp` 负责启动应用。`BattleView` 提供 Qt/OpenGL 窗口并驱动帧循环。`BattleScene` 创建场景根节点，然后调用 `loadSky()`、`loadOcean()` 和 `loadShipManager()`。

- `render/SkyLoader.cpp` 设置演示的天空背景。
- `render/OceanLoader.cpp` 加载 Triton 海面并配置水位。
- `EntityManager` 管理单调递增的 `EntityID`、阵营、分类、变换、选择能力和场景节点生命周期。类型工厂创建独立表现对象，成功后再注册进场景。
- `ShipManager` 负责舰型定义和表现工厂，按 code 缓存不可变的舰型定义，供多个独立 `ShipController` 共享。
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

## 战场实体 V1

`BattleScene` 持有 `EntityManager`。外部系统可按 ID 查找实体，按类型、阵营或选择规则获取按 ID 排序的快照。销毁会从场景卸下节点并注销 ID，后续新实体使用新 ID。舰船世界位置以米为单位，航向是绕世界 +Z 的弧度。

双舰脚本、碰撞和特效通过两个旧演示槽位访问实体。槽位只在兼容层中作数组下标，映射到独立的 `EntityID`。现有 RTS 框选保存实体 ID，查询 `Multiple` 选择规则。舰船定义文件仍采用 `config/ships/{code}.json`。

V1 注册了舰船工厂。陆地单位、飞机、建筑只有分类定义，生命值、编队与通用命令系统留待后续接入。

### RTS 航行

`ShipManager::build` 为 020528 附加独立的 `MovementTrait` 及测试调校参数，舰型模型 JSON 沿用原格式。Trait 保存目的地、航速和状态，更新时读写由 Entity 持有的位姿。舰船节点矩阵由 Entity 写入，供 OSG 绘制。

`BattleScene` 在 0.01 秒的固定时间步消费移动命令、核验所属阵营和实体有效性、推进舰船，然后更新现有弹丸及场景效果。独立的舰桥和双舰脚本测试继续使用原调度，RTS 航行模式不执行旧射击脚本。
