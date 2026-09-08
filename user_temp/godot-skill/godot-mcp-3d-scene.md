---
name: godot-mcp-3d-scene
summary: 通过 Godot 内置 MCP 受控创建、配置、运行、截图并迭代验收 3D 场景。
---

# Godot MCP 3D Scene Builder

## 适用范围

当用户要求通过自然语言创建 3D 场景、配置物体/灯光/相机，并根据截图反复调整时，使用 Godot 内置 MCP 服务完成受控编辑。不要执行 shell、任意脚本或任意文件操作。

## MCP 连接

连接参数必须由 `godot-mcp-common.md` 完成确认。此 Skill 不硬编码 Endpoint、端口、Token、项目路径或输出目录。

如果连接参数、项目路径或 Godot 编辑器状态未知，先询问用户，不要猜测或自动启动/重启编辑器。

每次请求使用用户通过 Secret 或安全输入提供的 Bearer Token。先调用：

```text
godot.editor.status
tools/list
```

确认服务处于 `running`，并检查目标工具存在。写入工具受确认策略约束；收到 `-32020` 时，读取返回的 `confirmationId`，调用 `godot.confirmation.approve` 后只重试同一个工具一次。

## 受控 3D 工具

按以下顺序调用：

```text
godot.transaction.begin
godot.scene.create
godot.project.set_main_scene
godot.editor.open_scene
godot.scene.add_3d_node
godot.scene.set_mesh
godot.scene.set_transform
godot.scene.set_property
godot.scene.set_material
godot.run.current_scene
godot.run.capture_camera_view
godot.run.stop
godot.transaction.commit
```

其中：

- `godot.project.set_main_scene` 将目标 `.tscn` 写入 `application/run/main_scene`，确保运行主场景正确；
- `godot.editor.open_scene` 确保目标场景已经加载到编辑器，便于获取目标 Camera3D；
- `godot.run.capture_camera_view` 必须作为主要视觉验证工具，读取运行中嵌入式游戏进程返回的实际渲染 Buffer；
- `godot.run.capture_view` 只能作为编辑器/运行容器调试截图，不得作为 3D 场景视觉验收的主要依据。

发生错误或视觉验收失败时，优先调整后再次截图；无法恢复时调用：

```text
godot.transaction.rollback
```

## 受支持的 3D 节点

`godot.scene.add_3d_node` 的受控类型包括：

```text
Node3D
MeshInstance3D
Camera3D
DirectionalLight3D
OmniLight3D
SpotLight3D
```

`godot.scene.set_mesh` 的 primitive 类型包括：

```text
BoxMesh
SphereMesh
PlaneMesh
```

## 3D 场景生成规范

将自然语言目标先转换为结构化计划，至少包含：

- 场景路径和根节点；
- 每个物体的名称、类型、mesh、位置、旋转和缩放；
- 每盏灯的类型、颜色、能量、阴影状态和位置/朝向；
- 相机位置、朝向、FOV、near/far；
- 材质颜色；
- 视觉验收标准。

推荐节点命名：

```text
Sphere
Cube
Plane
DirectionalLight
PointLight
SpotLight
Camera
```

推荐先创建物体和灯光，再设置 mesh、material、transform、light/camera properties，最后配置相机并运行场景。

## 属性白名单

`godot.scene.set_property` 只能使用 MCP 服务端允许的属性。当前已支持：

```text
visible
omni_range
spot_range
spot_angle
light_energy
shadow_enabled
light_color
current
fov
near
far
```

不要通过该工具写入任意属性、脚本、资源路径或代码。物体颜色使用 `godot.scene.set_material` 的 RGB/RGBA 数组。

## 截图视觉迭代

### 首选：目标 Camera3D 实际游戏 Buffer

必须先确保：

```text
godot.editor.open_scene
→ godot.project.set_main_scene
→ godot.run.current_scene
→ 等待游戏完成渲染
→ godot.run.capture_camera_view
```

调用示例：

```json
{
  "scene_path": "res://TestMCP.tscn",
  "camera_path": "Camera",
  "output_path": "user://TestMCP_camera.png"
}
```

`godot.run.capture_camera_view` 使用嵌入式游戏进程的截图回调获取实际渲染 Buffer，而不是编辑器主窗口 Buffer。该工具是异步的，成功返回 `capture_requested: true` 后，等待 PNG 文件生成，再读取图片。

成功生成的截图通常是游戏视口尺寸，例如 `1152x648`，应当包含实际 3D 场景，而不是 Godot 编辑器界面或纯灰色容器区域。

MCP 客户端必须读取返回的 PNG，并将图片内容作为 image content 传给支持视觉的 LLM；仅把 JSON 路径显示给模型不会产生视觉理解。

### 其他截图工具的用途

```text
godot.run.capture_view       → 编辑器/运行容器调试截图，不用于主要视觉验收
godot.capture.editor_view    → 编辑器主窗口截图，用于检查编辑器 UI
```

如果 `capture_camera_view` 返回 `-32040`，依次确认：

1. 目标场景已经通过 `godot.editor.open_scene` 打开；
2. `camera_path` 指向真实的 Camera3D；
3. 场景已经通过 `godot.run.current_scene` 运行；
4. 已等待几帧渲染完成；
5. 当前运行模式支持嵌入式游戏截图回调。

每轮截图后检查：

1. Sphere、Cube、Plane 是否全部可见；
2. Plane 是否位于物体下方且没有被裁切；
3. 相机视角是否自然，物体是否居中且留有边距；
4. DirectionalLight 是否整体照明并启用阴影；
5. 红色 OmniLight 是否主要照射 Sphere；
6. 黄色 SpotLight 是否主要照射 Cube；
7. 是否存在过曝、全黑、灯光方向错误或阴影缺失；
8. 必要时只调整相关节点的 transform、light property、camera property 或 material。

最大迭代轮数建议为 5。每轮只做一组可解释调整，并重新运行、截图和检查。达到验收标准后停止迭代并提交事务。

## 失败处理与交付报告

- 工具不存在：停止并报告 MCP 服务版本或工具注册不完整；
- `-32020`：执行确认流程，不要绕过确认；
- 场景节点路径错误：先检查当前已知路径，不要猜测任意路径；
- 截图不可用：检查运行状态、Camera3D 路径和嵌入式游戏 Buffer，再重试一次；
- `capture_camera_view` 返回 `capture_requested`：等待目标 PNG 文件生成后再读取，不要立即将请求结果当作图片；
- 截图仍为编辑器或灰色容器：不要继续使用 `capture_view` 判断场景，改用 `capture_camera_view`；
- 多轮调整仍不满足：rollback 并报告最后一张截图路径；
- 成功后调用 `godot.audit.export`，报告场景路径、节点列表、截图路径、迭代轮数和最终验收结论。
