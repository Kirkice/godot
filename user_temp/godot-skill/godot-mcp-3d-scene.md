---
name: godot-mcp-3d-scene
summary: 通过 Godot 内置 MCP 受控创建、配置、运行、截图并迭代验收 3D 场景。
---

# Godot MCP 3D Scene Builder

## 适用范围

当用户要求通过自然语言创建 3D 场景、配置物体/灯光/相机，并根据截图反复调整时，使用 Godot 内置 MCP 服务完成受控编辑。不要执行 shell、任意脚本或任意文件操作。

## MCP 连接

连接参数必须由 `godot-mcp-common.md` 完成确认。此 Skill 不硬编码 Endpoint、端口、项目路径或输出目录。

如果连接参数、项目路径或 Godot 编辑器状态未知，先询问用户，不要猜测或自动启动/重启编辑器。

先调用：

```text
tools/list
godot.project.inspect
```

确认服务可用，并以 `tools/list` 返回的 Schema 为准。当前新版服务对外公开的是高层聚合工具，旧版细粒度工具可能仍可内部兼容调用，但不会出现在 `tools/list`。

## 受控 3D 工具

新版场景编辑统一使用：

```text
godot.scene.inspect
godot.scene.mutate
godot.resource.mutate
godot.run
godot.visual.capture
godot.test
```

不要因为旧工具不在 `tools/list` 就认为服务损坏，也不要优先尝试旧版工具名。写入工具受确认策略约束；收到确认错误时，按 MCP 客户端显示的确认流程批准后，只重试原始请求一次。

发生错误或视觉验收失败时，优先调整后再次截图；如果客户端支持内部事务 RPC，再按服务实际列出的事务接口执行回滚；不要猜测不存在的 RPC 名称。

## 受支持的 3D 节点

`godot.scene.mutate` 的 `add_3d_node` operation 的受控类型包括：

```text
Node3D
MeshInstance3D
Camera3D
DirectionalLight3D
OmniLight3D
SpotLight3D
```

`godot.scene.mutate` 的 `set_mesh` operation 的 primitive 类型包括：

```text
BoxMesh
SphereMesh
PlaneMesh
```

## `godot.scene.mutate` 调用格式

### 外层参数

```json
{
  "scene_path": "res://<scene>.tscn",
  "operations": []
}
```

`scene_path` 放在外层，服务会自动传递给每个 operation。每一项必须包含 `action`。

### 支持的 operation

```text
add_node
add_3d_node
remove_node
rename_node
reparent_node
set_transform
set_property
set_mesh
set_material
```

### 添加 3D 节点

```json
{
  "scene_path": "res://<scene>.tscn",
  "operations": [
    {
      "action": "add_3d_node",
      "parent_path": ".",
      "node_type": "MeshInstance3D",
      "node_name": "PBR_Sphere_01"
    }
  ]
}
```

允许的 `node_type`：

```text
Node3D
MeshInstance3D
Camera3D
DirectionalLight3D
OmniLight3D
SpotLight3D
```

### 设置变换

```json
{
  "scene_path": "res://<scene>.tscn",
  "operations": [
    {
      "action": "set_transform",
      "node_path": "PBR_Sphere_01",
      "position": [0, 1, 0],
      "rotation_degrees": [0, 0, 0],
      "scale": [1, 1, 1]
    }
  ]
}
```

三个数组都必须是 3 个数值。

### 设置 Mesh

```json
{
  "scene_path": "res://<scene>.tscn",
  "operations": [
    {
      "action": "set_mesh",
      "node_path": "PBR_Sphere_01",
      "mesh_type": "SphereMesh",
      "size": [2, 2, 2]
    }
  ]
}
```

### 设置材质

```json
{
  "scene_path": "res://<scene>.tscn",
  "operations": [
    {
      "action": "set_material",
      "node_path": "PBR_Sphere_01",
      "color": [0.8, 0.2, 0.1, 1.0],
      "metallic": 0.75,
      "roughness": 0.25
    }
  ]
}
```

材质颜色是 3 或 4 个分量的 RGB/RGBA 数组。PBR 参数通过同一个 `set_material` operation 提供：`metallic` 和 `roughness` 都是 0.0 到 1.0 的数值；不需要再猜测 `set_property` 的材质字段。

### 混合批处理

```json
{
  "scene_path": "res://<scene>.tscn",
  "operations": [
    {
      "action": "add_3d_node",
      "parent_path": ".",
      "node_type": "MeshInstance3D",
      "node_name": "PBR_Sphere_01"
    },
    {
      "action": "set_transform",
      "node_path": "PBR_Sphere_01",
      "position": [-3, 1, 0],
      "rotation_degrees": [0, 0, 0],
      "scale": [1, 1, 1]
    },
    {
      "action": "set_mesh",
      "node_path": "PBR_Sphere_01",
      "mesh_type": "SphereMesh",
      "size": [2, 2, 2]
    },
    {
      "action": "set_material",
      "node_path": "PBR_Sphere_01",
      "color": [0.8, 0.2, 0.1, 1]
    }
  ]
}
```

服务会逐项执行并返回：

```json
{
  "updated": true,
  "operation_count": 4,
  "results": []
}
```

每个 operation 都必须带正确的 `action`；不能把旧工具名写进 `action`，例如不要写 `godot.scene.add_3d_node`，只写 `add_3d_node`。

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

不要通过该 operation 写入任意属性、脚本、资源路径或代码。物体颜色使用 `godot.scene.mutate` 的 `set_material` operation 和 RGB/RGBA 数组。

## 截图视觉迭代

### 首选：目标 Camera3D 实际游戏 Buffer

必须先确保：

```text
godot.scene.inspect
→ godot.run (action=start)
→ 等待游戏完成渲染
→ godot.visual.capture (source=camera)
```

调用示例：

```json
{
  "scene_path": "res://<scene>.tscn",
  "camera_path": "Camera",
  "output_path": "user://<capture>.png"
}
```

`godot.visual.capture` 使用 `source=camera` 获取实际游戏渲染 Buffer，而不是编辑器主窗口 Buffer。截图可能是异步的；如果返回请求已提交状态，应等待 PNG 文件生成后再读取图片。

成功生成的截图通常是游戏视口尺寸，例如 `1152x648`，应当包含实际 3D 场景，而不是 Godot 编辑器界面或纯灰色容器区域。

MCP 客户端必须读取返回的 PNG，并将图片内容作为 image content 传给支持视觉的 LLM；仅把 JSON 路径显示给模型不会产生视觉理解。

### 其他截图来源的用途

```text
source=camera → 主要的 3D 场景验收
source=game   → 运行容器或游戏视图调试
source=editor → 编辑器主窗口 UI 检查
```

如果 camera 截图不可用，依次确认：

1. 通过 `godot.scene.inspect` 确认场景和 Camera3D 节点路径；
2. 使用 `godot.run` 的 `action=start` 启动场景；
3. 等待几帧渲染完成；
4. 使用 `godot.visual.capture`，参数 `source=camera`；
5. 当前运行模式支持嵌入式游戏截图。

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

- 工具不存在：先调用 `tools/list`，确认使用的是新版聚合工具；不要立即切换到旧工具名；
- `Unsupported scene operation`：检查 `godot.scene.mutate` Schema 和本 Skill 的 `action` 枚举，只传 `add_3d_node` 等 operation action，不要传完整工具名；
- `-32020`：执行客户端显示的确认流程，不要绕过确认；
- 场景节点路径错误：先调用 `godot.scene.inspect` 检查当前已知路径，不要猜测任意路径；
- 截图不可用：检查运行状态、Camera3D 路径和嵌入式游戏 Buffer，再重试一次；
- camera 截图返回请求已提交状态：等待目标 PNG 文件生成后再读取，不要立即将请求结果当作图片；
- 截图仍为编辑器或灰色容器：改用 `source=camera`，不要用 `source=editor` 或 `source=game` 判断最终 3D 视觉效果；
- 多轮调整仍不满足：rollback 并报告最后一张截图路径；
- 成功后调用 `godot.audit.export`，报告场景路径、节点列表、截图路径、迭代轮数和最终验收结论。
