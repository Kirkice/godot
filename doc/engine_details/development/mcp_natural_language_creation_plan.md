# Godot 源码内置 MCP 与自然语言场景创作开发计划

本文定义本维护分支的开发方向：**直接修改 Godot 编辑器 C++ 源码**，在定制 Godot 编辑器中内置 MCP Bridge、编辑器自动化、运行诊断与截图能力。目标是让外部 AI 通过 MCP 的结构化工具，安全地创建、修改、运行和视觉验证 Godot 项目。

首个验收用例：

> 创建一个场景，场景内有一个球和一个 Cube，打一盏红色平行光；AI 截图检查画面并在必要时调整相机和光照。

最终目标：

```text
自然语言
  → AI 使用 MCP 工具读取工程状态
  → 定制 Godot 编辑器执行源码内置的场景/资源操作
  → 保存、运行、收集错误、截图
  → AI 根据结果继续有限次数调整
  → 交付可打开、可运行、可审计和可回滚的简单游戏项目
```

## 1. 架构决策

### 1.1 采用源码内置，而不是项目 EditorPlugin

本项目后续由维护者维护 Godot 源码分支，因此 MCP Bridge **不作为 `addons/` 项目插件交付**。实现将编译进定制编辑器，从而直接复用编辑器内部 API、运行调试器、主视口与渲染管线能力。

`EditorPlugin` 仍可作为内部组织形式：Godot 编辑器中许多功能位于 `editor/plugins/` 并继承 `EditorPlugin`。这里的区别是：本功能的 C++ 实现在引擎源码中编译，而不是用户项目中的 GDScript 插件。

推荐结构：

```text
editor/plugins/mcp/
  mcp_editor_plugin.h/.cpp          # 编辑器生命周期、菜单、Dock、状态显示
  mcp_service.h/.cpp                # 服务协调、会话、命令分派
  mcp_command_router.h/.cpp         # tool → C++ handler 映射与统一结果
  mcp_scene_commands.h/.cpp         # 场景树创建、编辑、保存、Undo/Redo
  mcp_resource_commands.h/.cpp      # 资源、材质、Mesh、脚本等
  mcp_run_commands.h/.cpp           # 运行/停止、日志和调试诊断
  mcp_capture_commands.h/.cpp       # 编辑器/游戏/Camera 输出截图
  mcp_transaction.h/.cpp            # 事务、确认、审计与回滚
  mcp_protocol.h/.cpp               # 内部请求、响应、错误码、Variant/JSON 转换
  mcp_settings.h/.cpp               # EditorSettings、令牌与授权策略
  register_types.h/.cpp             # 可选：仅在拆为独立模块时使用
```

初期可先放入 `editor/plugins/mcp/`，并通过 `editor/register_editor_types.cpp` 注册。若后续功能膨胀、希望有独立 SCons 开关，再迁为 `modules/mcp_bridge/`。

### 1.2 MCP 协议边界：完全内置

不保留 Node.js、Python 或其他外部 MCP 适配进程。定制 Godot 编辑器本身就是 MCP Server：在 C++ 中实现标准 MCP transport、`initialize`、`tools/list`、`tools/call`、资源返回与会话管理，同时执行全部真实编辑器操作。

为适应已经运行的 GUI 编辑器，并避免标准输出与 Godot 日志混用，首选实现 **仅绑定 `127.0.0.1` 的 Streamable HTTP MCP transport**；不将 stdio 作为默认 transport。若后续某个目标 MCP Client 仅支持 stdio，应由 Godot 提供受控的 `--mcp-stdio` 启动模式，并将协议 JSON 与普通日志严格分离到不同输出通道。

```text
AI / MCP Client
  ↕ 标准 MCP over Streamable HTTP（仅 localhost）
定制 Godot 编辑器（源码内置 MCP Server）
  ├─ MCP transport、tools/list、tools/call、JSON Schema
  ├─ 会话、认证、确认、审计与事务
  └─ EditorNode / UndoRedo / EditorDebugger / Viewport / ProjectSettings
```

所有协议兼容、工具 Schema 和业务实现都由本维护分支的 Godot C++ 源码负责。这样删除了进程间转发和外部运行时依赖，但代价是 MCP 规范升级、客户端兼容性和 transport 行为也需要随源码分支维护。

### 1.3 编译与开关策略

该能力仅存在于 `target=editor`，默认不影响导出模板和运行时游戏。

推荐 SCons 选项：

```text
mcp_bridge=yes|no       # 默认 no，定制编辑器构建时显式开启
mcp_bridge_port=0       # 0 表示启动时自动选择空闲 loopback 端口
```

Windows 定制编辑器构建示例：

```bat
build_windows_editor.bat
```

待 `mcp_bridge` SCons 选项实现后：

```bat
call "<VS_INSTALL_PATH>\VC\Auxiliary\Build\vcvars64.bat" >nul && py -m SCons platform=windows target=editor dev_build=yes opengl3=no mcp_bridge=yes -j%NUMBER_OF_PROCESSORS%
```

一键脚本后续增加透传参数，例如：

```bat
build_windows_editor.bat --mcp-bridge
build_windows_editor.bat --mcp-bridge --with-vsproj
```

> 当前 `build_windows_editor.bat` 尚未支持 `--mcp-bridge`；这是 Phase 0 的明确实现项，不能在实现前假设其已可用。

## 2. 源码接入点

| 领域 | 优先接入位置 | 用途 |
|---|---|---|
| 编辑器生命周期 | `editor/editor_node.*`、`editor/register_editor_types.cpp` | 创建/销毁服务，注册菜单、Dock 和快捷状态 |
| 编辑器 UI | `editor/plugins/mcp/mcp_editor_plugin.*` | 显示连接状态、端口、会话、审计和确认请求 |
| 场景编辑 | `editor/scene_tree_dock.*`、`EditorUndoRedoManager` | 创建节点、修改属性、删除/重挂节点并支持 Undo |
| 资源与文件系统 | `editor/editor_file_system.*`、`ResourceLoader`、`ResourceSaver` | 创建/保存资源、刷新文件系统、报告导入状态 |
| 场景文件 | `PackedScene`、`EditorData`、`EditorNode` | 打开、保存、当前场景与选中节点 |
| 项目设置 | `core/config/project_settings.*`、InputMap 编辑相关 API | 主场景、输入映射、Autoload 等受控修改 |
| 运行控制 | `editor/run/*`、`editor/debugger/*` | 运行/停止场景或项目，读取 PID、输出与错误 |
| 截图 | `editor/plugins/*`、`Viewport`/`SubViewport`、嵌入式游戏视图 | 获取编辑器视口、运行画面和相机预览 |
| 渲染读回 | `RenderingServer` / `RenderingDevice`（仅必要时） | 处理无法从 UI Viewport 获取的稳定图像读回 |

设计原则：**命令只调用已有编辑器能力或新增的、窄范围的 C++ 服务接口；不要让网络请求直接跨越到任意内部对象。**

## 3. MCP 能力清单

Godot 源码内置 MCP Server 直接向模型暴露以下工具。每个工具使用固定 JSON Schema；Godot 返回固定字段：

```json
{
  "ok": true,
  "request_id": "...",
  "scene_revision": 12,
  "changed_files": ["res://scenes/demo.tscn"],
  "warnings": [],
  "errors": []
}
```

### 3.1 项目与诊断：Phase 1

```text
godot.project.info
godot.project.list_files
godot.project.read_text
godot.scene.current
godot.scene.inspect
godot.diagnostics.get_errors
godot.diagnostics.get_logs
godot.editor.status
```

用途：在任何写操作前建立上下文。`scene.inspect` 必须返回节点类型、稳定 NodePath、关键属性、脚本与资源引用；避免模型猜测场景结构。

### 3.2 场景图编辑：Phase 1

```text
godot.scene.create
godot.scene.open
godot.scene.save
godot.scene.add_node
godot.scene.remove_node
godot.scene.rename_node
godot.scene.reparent_node
godot.scene.set_property
godot.scene.set_transform
godot.scene.connect_signal
godot.scene.select_node
godot.scene.duplicate_node
```

实现要求：

- 操作通过 `EditorUndoRedoManager` 登记；
- 所有节点定位使用 `scene_path + node_path`；
- `set_property` 采用 Variant 的白名单结构化编码，不接收任意表达式；
- 删除、重父级和覆盖现有内容需要事务或明确确认；
- 每个修改后递增 `scene_revision`。

### 3.3 资源、材质和脚本：Phase 2

```text
godot.resource.create
godot.resource.set_property
godot.resource.save
godot.node.assign_resource
godot.script.write
godot.script.attach
godot.project.set_main_scene
godot.project.configure_input_action
```

MVP 资源白名单：

```text
SphereMesh, BoxMesh, PlaneMesh, StandardMaterial3D,
BoxShape3D, SphereShape3D, Environment,
DirectionalLight3D, OmniLight3D, Camera3D
```

脚本覆写、项目设置、InputMap 和主场景修改必须触发确认或处于已确认事务内。

### 3.4 运行、截图和视觉验证：Phase 2

```text
godot.run.current_scene
godot.run.project
godot.run.stop
godot.run.status
godot.diagnostics.wait_idle
godot.capture.editor_view
godot.capture.game_view
godot.capture.camera_preview
godot.test.send_input
```

截图结果返回 MCP 资源附件或受控的临时本地 URI，并包含：

```text
width, height, capture_source, scene_revision, timestamp
```

截图顺序：优先编辑器主视口；支持嵌入式游戏视图后再支持外部游戏窗口；Camera3D 专用渲染输出作为后续扩展。禁止把大段图片二进制塞入普通工具日志。

### 3.5 高层、幂等模板：Phase 3

```text
godot.template.create_3d_scene
godot.template.add_primitive
godot.template.frame_camera
godot.template.ensure_light
godot.template.create_player_2d
```

它们提高自然语言任务成功率，但必须基于并暴露底层节点/资源结果；不能成为不可检查的“黑盒生成器”。

## 4. 示例：红色平行光场景的完整闭环

用户：

> 创建一个场景，场景内有一个球和一个 Cube，打一盏平行光，要求是红色。

AI 的约束式执行步骤：

1. `godot.project.info` 与 `godot.scene.current`：确认项目和目标位置；用户未指定时，询问创建新场景还是修改当前场景。
2. `godot.scene.create(root_type="Node3D", scene_path="res://scenes/red_light_demo.tscn")`。
3. 添加 `MeshInstance3D`：`Sphere`、`Cube`；添加 `DirectionalLight3D`：`RedDirectionalLight`；添加 `Camera3D`。为可视化稳定性，可加地面和 `WorldEnvironment`。
4. 创建 `SphereMesh`、`BoxMesh`，分别绑定给球和 Cube。
5. 设置 Transform，保证不重叠，例如 Sphere `(-1, 0, 0)`、Cube `(1, 0, 0)`；设置相机能同时看见两个物体。
6. 设置：

   ```json
   {
     "node_path": "RedDirectionalLight",
     "property": "light_color",
     "value": {"type": "Color", "r": 1.0, "g": 0.0, "b": 0.0, "a": 1.0}
   }
   ```

   同时设置合理灯光能量、方向与环境，避免画面过暗。
7. 保存，`godot.diagnostics.wait_idle`，再执行 `godot.capture.editor_view`。
8. AI 检查截图：物体不在画面内则调整相机；物体太暗则调灯光方向/能量或环境；物体重叠则调整位置。
9. 最多进行预设次数的视觉调整，保存并再次截图。
10. 返回场景路径、节点树、资源路径、截图、日志摘要与变更说明。

## 5. 事务、安全和审计

### 5.1 事务

```text
godot.transaction.begin(label)
  → 多个受控工具调用
godot.project.validate
godot.transaction.commit
```

失败或用户取消时：

```text
godot.transaction.rollback
```

实现上将场景操作接入 `EditorUndoRedoManager`；对文本和项目设置建立修改前快照。事务没有提交前，MCP 响应应明确标注 `pending_commit`。

### 5.2 默认禁止

- 任意 Shell 命令；
- 任意网络访问；
- `res://` 之外的任意读写；
- 模型提供的任意 GDScript/C# 字符串直接执行；
- 无确认的覆盖、删除、项目全局设置变更；
- 监听非 loopback 网络地址。

### 5.3 认证与审计

- 服务只监听 `127.0.0.1`；
- 每次编辑器启动生成一次性随机令牌；在 MCP Dock 中显示连接信息，并仅通过首次本地受控握手向 MCP Client 提供令牌；
- 所有命令要求 `request_id`、令牌和显式项目根；
- 记录时间、工具、脱敏参数、结果、变更文件、场景版本和错误；
- 审计写入 `.godot/mcp/` 或外部用户数据目录，不提交令牌。

## 6. Skill 强化规划

MCP 定义原子工具；Skill 负责让 AI 按正确方法使用工具、验证结果并保持项目整洁。

| Skill | 作用 | 依赖 MCP |
|---|---|---|
| `godot-scene-authoring` | 自然语言创建/修改 2D、3D 节点、基础布局、相机与灯光 | scene、resource、transaction |
| `godot-asset-authoring` | 创建材质、Mesh、碰撞、Shader 与资源目录规范 | resource、scene |
| `godot-gameplay-authoring` | 创建 GDScript、输入、信号、状态和基础玩法 | script、settings、run |
| `godot-visual-validation` | 截图、检查、有限自动调整和验收报告 | capture、diagnostics、scene |
| `godot-windows-build-run` | 构建定制 Godot 编辑器与可选 `.sln` | 现有构建脚本与 SCons |

### 6.1 `godot-scene-authoring` 必须遵循

1. 先 `inspect`，后修改；
2. 未指定目标场景时先询问；
3. 创建可见 3D 物体时，确保相机、灯光和可读背景存在；
4. 统一节点命名和资源目录；
5. 所有写操作纳入事务；
6. 完成后必须保存、检查诊断和截图；
7. 输出路径、节点树、资源、截图和未解决问题。

### 6.2 `godot-visual-validation` 最低验收

以红光示例为例：

- 场景无资源/脚本解析阻断错误；
- `Sphere`、`Cube` 和 `RedDirectionalLight` 存在且类型正确；
- 光源颜色为红色；
- 相机同时看见球和 Cube；
- 截图可辨识两个独立物体；
- 最终结果可撤销或回滚；
- 结果报告列出文件路径和验证证据。

## 7. 分阶段开发计划

### Phase 0：源码骨架与定制构建（1 个里程碑）

实现：

- 增加 `editor/plugins/mcp/` 目录与 `MCPBridgeEditorPlugin`；
- 在编辑器注册路径注册；
- 增加 `mcp_bridge=yes|no` SCons 选项，仅允许 editor target；
- MCP Dock 显示：启用状态、loopback 地址、端口、令牌摘要、最近命令；
- 实现 loopback 服务生命周期和 `godot.editor.status`；
- 更新 `build_windows_editor.bat`，增加 `--mcp-bridge` 和 `--mcp-bridge --with-vsproj`；
- 添加编译验证与最小手动连接测试。

验收：带 `mcp_bridge=yes` 的定制编辑器可构建、启动，Dock 正常显示，未开启时不启动端口。

### Phase 1：只读与可撤销场景编辑 MVP

实现：

- 项目/场景检查、创建、打开、保存；
- 添加、选择、重命名节点；
- `set_transform` 和受限 `set_property`；
- Undo/Redo、场景版本、审计日志、事务 begin/commit/rollback；
- 源码内置 MCP Server 直接实现对应工具的 `tools/list`、`tools/call` 与 JSON Schema。

验收：AI 可创建 `Node3D + Sphere + Cube + DirectionalLight3D + Camera3D` 场景，保存后编辑器可重新打开，Undo 可撤回一次完整事务。

### Phase 2：资源、运行和截图闭环

实现：

- 常用 Mesh/Material/Light/Collision 资源创建与绑定；
- 当前场景运行、停止、日志与错误采集；
- 编辑器视口截图；
- 嵌入式 Game 视图截图；
- `wait_idle`、错误码和失败降级逻辑。

验收：完整完成红光示例，AI 能根据至少一次截图检查调节相机或光照，并保存最终场景。

### Phase 3：Skill 与简单游戏生成

实现：

- 编写并启用 `godot-scene-authoring` 与 `godot-visual-validation` Skill；
- 加入高层模板工具；
- 脚本写入/挂接、InputMap、运行时错误定位；
- 用 2D 小游戏作为稳定的端到端基准。

验收：自然语言生成简单 2D 游戏，能运行、截图、读取日志并完成基本玩法验证。

### Phase 4：高级 3D 与编辑器深度集成

实现：

- Camera3D 专用渲染截图、外部游戏窗口截图或稳定的渲染读回；
- 动画、导航、粒子、Shader、导入资产；
- 复杂多场景、依赖分析、Git 变更摘要；
- 评估在 `modules/mcp_bridge/` 独立模块化的必要性。

验收：能通过自然语言建立可维护的 3D 小型演示，并通过多轮视觉验证保持可控修改范围。

## 8. 首次开发任务拆分

下一次实际编码建议只执行 Phase 0 的以下最小切片：

1. 创建 `editor/plugins/mcp/` C++ 文件和最小 `MCPBridgeEditorPlugin`；
2. 注册插件，显示一个只读 MCP Dock；
3. 增加 `mcp_bridge` SCons 选项，默认关闭；
4. `mcp_bridge=yes` 时创建仅 loopback 的服务骨架，但先只响应 `godot.editor.status`；
5. 更新 Windows 一键构建脚本以透传该选项；
6. 构建、启动、通过本地客户端确认状态 JSON；
7. 再进入 Phase 1，绝不在第一步引入场景编辑、截图、脚本执行和外部进程控制。

这种切分能先验证：**源码注册 → 定制构建 → 编辑器启动 → 安全本地通信**，为后续工具建立可维护基础。
