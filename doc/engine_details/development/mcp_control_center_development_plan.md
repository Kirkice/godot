# MCP Control Center 开发需求与实施方案

## 1. 文档目的

本文定义定制 Godot 编辑器内置 **MCP Control Center**（MCP 控制中心）的产品需求、C++ 源码设计、UI 方案、开发阶段、代码规范与验收标准。

该功能是源码内置 MCP Server 的可视化控制台，不是项目 `addons/` 插件。它用于配置本机 MCP 服务、查看工具能力、管理权限与确认策略、观察连接和审计请求，并为后续“自然语言创建场景、截图分析、自动调整场景”的工作流提供操作面板。

关联总体规划：

```text
doc/engine_details/development/mcp_natural_language_creation_plan.md
```

## 2. 目标与非目标

### 2.1 目标

1. 在 Godot 编辑器内提供一个高级、清晰、符合 Godot 原生主题的 MCP 控制面板。
2. 用户可启用或关闭源码内置 MCP Server。
3. 用户可配置监听 IP、端口、自动端口、随编辑器启动、认证令牌和连接信息。
4. 用户可按类别浏览 MCP Tools，查看工具描述、输入 Schema、状态、权限、风险等级与确认策略。
5. 用户可查看 MCP 客户端连接、最近请求、成功/警告/失败/拒绝记录及审计详情。
6. 面板和网络服务共用同一 `MCPService`、`MCPToolRegistry`、权限策略和审计数据源；UI 的开关必须立即影响实际工具调用权限。
7. 默认安全：MCP Server 仅监听 `127.0.0.1`，未显式开启时不启动，且不影响普通 Godot 编辑器工作流。

### 2.2 非目标

第一阶段不实现：

- 项目级 `addons/` MCP 插件；
- 外部 Node.js / Python MCP Server；
- 非回环网络地址的实际监听；
- 任意 Shell、任意代码执行或任意文件系统访问；
- 完整的场景创建、资源创建、截图或游戏生成工具本身；
- 自定义 Web UI、WebView、HTML/CSS 或第三方 UI 框架。

Control Center 在 Phase 0/1 中可以展示“规划中”的 Tool，但不能将未实现工具显示成可调用状态。

## 3. 用户故事与需求

### 3.1 服务配置

作为编辑器使用者，我需要：

- 通过 `Enable MCP` 启用/禁用 MCP Server；
- 可点击 `Start`、`Stop`，并看到 `Disabled`、`Starting`、`Running`、`Stopping`、`Error` 状态；
- 设置 Bind Address、Port、Auto-select available port、Start MCP server with editor；
- 查看最终 endpoint，例如：

  ```text
  http://127.0.0.1:30100/mcp
  ```

- 复制 endpoint；
- 查看、复制、重新生成认证 Token；Token 默认遮蔽；
- 看到启动失败原因（端口占用、非法地址、认证初始化失败等）。

### 3.2 IP 地址策略

面板必须存在 Bind Address 输入项，满足“可配置 IP 地址”的需求；但第一阶段只允许：

```text
127.0.0.1
::1
```

- 默认值为 `127.0.0.1`；
- 输入其他地址时，显示明确风险说明和禁用原因；
- `0.0.0.0`、局域网 IP 与公网 IP 不允许实际启动；
- 为未来高级网络模式预留设置与实现位置，但不在本阶段开放。

这样 UI 完整，同时维持 MCP 的本机安全边界。

### 3.3 工具浏览与分类

用户需要可搜索、按类浏览 Tools。

预定义一级分类：

```text
Project & Diagnostics
Scene Authoring
Resources & Assets
Run & Debug
Capture & Validation
Transactions & Security
```

每个 Tool 在列表中显示：

- 名称，例如 `godot.scene.add_node`；
- 状态：Implemented / Disabled / Planned / Unavailable；
- 风险：Low / Medium / High / Destructive；
- 是否需要确认；
- 是否已对 MCP Client 发布。

选中 Tool 后，详情页显示：

- 名称、分类与简短描述；
- 实现状态；
- 权限范围，例如 Read-only / Scene write / Project write / Run control；
- 风险等级和确认策略；
- 输入 JSON Schema（只读格式化文本）；
- 输出摘要；
- 当前不可用原因（如 MCP 未启用、工具尚未实现、没有打开场景）；
- 后续阶段可增加 `Test Tool`，但 Phase 0 默认不允许从 UI 执行会修改项目的 Tool。

### 3.4 权限与确认策略

用户可以针对已实现 Tool 设置：

```text
Enabled: On / Off
Confirmation: Never / Once per session / Every call
```

权限范围和风险等级由 C++ Tool Registry 定义，UI 只显示，不能把高风险 Tool 降级为低风险。

以下操作默认必须经过确认：

```text
godot.scene.remove_node
godot.scene.reparent_node
godot.script.write（覆盖已有文件）
godot.project.set_main_scene
godot.project.configure_input_action
godot.run.project
godot.test.send_input
```

### 3.5 活动、连接和审计

面板需要：

- 当前连接数和活动会话；
- 客户端标识、连接时间、最后活动时间、会话状态；
- 最近 MCP 请求列表；
- 按 All / Success / Warning / Error / Denied 筛选；
- 按 Tool 名称或 `request_id` 搜索；
- 点击请求查看脱敏参数、响应摘要、受影响文件、耗时、错误；
- 导出审计日志；
- 清除仅当前 UI 的临时显示记录，不删除持久审计日志。

## 4. UI 与交互设计

### 4.1 入口和布局

推荐新增底部面板：

```text
MCP
```

理由：工具树、详情、服务配置和活动记录均需要较大横向空间；底部面板的工作方式与 Godot Output、Debugger、Profiler 一致。

建议默认布局：

```text
┌──────────────────────────────────────────────────────────────────────────────┐
│ MCP Control Center                 ● Running                 [Start] [Stop]  │
├───────────────────────┬─────────────────────────┬────────────────────────────┤
│ Server & Connection   │ Tools                   │ Tool Details               │
│ Bind Address          │ Search                  │ Name / Status / Risk       │
│ [127.0.0.1        ]   │ ▾ Project & Diagnostics │ Description                │
│ Port [30100] [Auto]   │ ▾ Scene Authoring       │ Permission / Confirmation  │
│ Start with editor     │ ▾ Resources & Assets    │ Input JSON Schema          │
│ Endpoint [Copy]       │ ▾ Run & Debug           │ [Enable] [Policy]          │
│ Token [Reveal][Copy]  │ ▾ Capture & Validation  │                            │
│ Clients               │ ▾ Transactions & Sec.   │                            │
├───────────────────────┴─────────────────────────┴────────────────────────────┤
│ Activity & Audit                         [Filter] [Search] [Export] [Clear]  │
│ 12:44:12 ✓ godot.scene.add_node — Created World/Sphere                       │
│ 12:44:16 ! godot.capture.editor_view — No active 3D viewport                 │
└──────────────────────────────────────────────────────────────────────────────┘
```

### 4.2 Godot 原生 UI 要求

必须复用 Godot 编辑器控件与主题：

```text
VBoxContainer / HBoxContainer / SplitContainer
MarginContainer / PanelContainer
Tree / ItemList / TabContainer
LineEdit / SpinBox / CheckBox / OptionButton / Button
RichTextLabel / TextEdit 或 CodeEdit
PopupMenu / ConfirmationDialog
EditorTheme / EditorIcons / EditorColorMap
```

禁止：

- WebView；
- HTML/CSS；
- Qt、ImGui、Electron 或第三方 UI 框架；
- 硬编码亮色/暗色主题颜色；
- 用 emoji 代替 Godot 编辑器图标。

### 4.3 视觉标准

“高级美观”通过信息层级、密度、状态可读性和主题一致性实现：

- 标题栏使用清晰的服务状态徽标；
- 绿色仅表示 Running / Success，红色仅表示 Error / Destructive，黄色表示 Warning / Confirmation；
- 通过 `EditorTheme` 和 `EditorColorMap` 获取颜色与 StyleBox，不写固定 RGB；
- 关键区域使用适度 `PanelContainer` 与留白，不增加大面积渐变、玻璃拟态或发光效果；
- Tool Tree 使用分类图标、状态图标和计数；
- Schema 使用只读、等宽字体的 `CodeEdit`，提供复制按钮；
- 窄窗口下允许 SplitContainer 拖动，最小宽度合理，面板不得遮挡或截断核心信息；
- 所有文案第一期可为英文标识符与英文工具名，面向用户的解释文本需走 Godot 翻译宏，便于后续本地化。

### 4.4 状态与错误交互

- 启动时配置无效：保留用户输入、显示内联错误，不静默替换；
- 端口冲突：显示端口和“Use Auto Port”快捷操作；
- 服务运行时修改 Bind Address/Port：提示需停止并重新启动，或提供 `Restart`；
- 禁用 MCP 时：停止服务、断开客户端，Tool 列表仍可浏览，但调用状态显示 Unavailable；
- 高风险策略修改：使用 `ConfirmationDialog`，显示影响范围；
- Token reveal 需要短时显示并提供再次遮蔽，审计日志不得保存 Token。

## 5. 源码设计

### 5.1 文件结构

建议在 `editor/plugins/mcp/` 新增：

```text
mcp_editor_plugin.h/.cpp             # EditorPlugin 生命周期、底部面板注册
mcp_control_center.h/.cpp            # 根控件，组合子视图与刷新调度
mcp_server_settings_view.h/.cpp      # 地址/端口/开关/endpoint/token/客户端
mcp_tool_registry_view.h/.cpp        # 分类 Tree、搜索、Tool 选择
mcp_tool_detail_view.h/.cpp          # 详情、Schema、状态、策略
mcp_activity_view.h/.cpp             # 活动列表、筛选、审计详情
mcp_service.h/.cpp                   # MCP 生命周期、HTTP transport、会话
mcp_tool_registry.h/.cpp             # 工具元数据、状态、Schema 和权限
mcp_command_router.h/.cpp            # tools/call 分发（后续 Phase）
mcp_audit_log.h/.cpp                 # 内存活动记录与持久审计
mcp_settings.h/.cpp                  # EditorSettings 键、配置校验
mcp_types.h                           # 枚举、ToolDescriptor、请求/响应 DTO
```

Phase 0 可只实现：

```text
mcp_editor_plugin
mcp_control_center
mcp_server_settings_view
mcp_tool_registry_view
mcp_tool_detail_view
mcp_activity_view
mcp_service
mcp_tool_registry
mcp_settings
mcp_types
```

### 5.2 类职责

| 类 | 职责 |
|---|---|
| `MCPBridgeEditorPlugin` | 创建/销毁 `MCPService` 与 `MCPControlCenter`；注册/移除底部面板；与 EditorSettings 生命周期对接 |
| `MCPControlCenter` | 组合 UI、连接信号、定时/事件驱动刷新；不包含网络协议或编辑场景业务 |
| `MCPServerSettingsView` | 显示和编辑配置；只调用 Service 的验证/启动/停止接口 |
| `MCPToolRegistryView` | 展示分类、过滤、状态和计数；发出 Tool 选中信号 |
| `MCPToolDetailView` | 显示 ToolDescriptor；编辑启用/确认策略；不执行 Tool 业务 |
| `MCPActivityView` | 展示 Service 发出的活动事件、筛选、详情与导出 |
| `MCPService` | 单一真实状态源；管理 HTTP、会话、认证、事件、启动/停止和 Tool Registry |
| `MCPToolRegistry` | 登记 Tool 元数据与 Handler；为 UI 与 `tools/list` 提供同一份描述 |
| `MCPAuditLog` | 保存脱敏活动和持久审计，禁止记录 Token |
| `MCPSettings` | 定义设置键、默认值、合法性校验、保存与迁移 |

依赖方向必须单向：

```text
UI Views → MCPControlCenter → MCPService → MCPToolRegistry / MCPAuditLog
                                            → 后续 Command Router / Editor APIs
```

`MCPService` 不得依赖具体 UI View；服务必须可以在无 Dock 可见的情况下正常运行。

### 5.3 ToolDescriptor

Tool 元数据建议在 C++ 使用结构体集中定义，示意：

```cpp
struct MCPToolDescriptor {
	StringName name;
	StringName category;
	String description;
	MCPToolStatus status;
	MCPPermission permission;
	MCPRiskLevel risk_level;
	MCPConfirmationPolicy confirmation_policy;
	Dictionary input_schema;
};
```

要求：

- UI、`tools/list`、权限校验和审计全部引用同一描述；
- 不在 UI 中复制 Tool 名称、Schema 或风险规则；
- `Planned` Tool 只作为路线图展示，不能从 `tools/list` 发布为可调用 Tool；
- 工具 Handler 是否存在、当前编辑器状态是否满足要求，共同决定 `Implemented` / `Unavailable`。

### 5.4 设置键建议

使用 `EditorSettings`，键前缀统一：

```text
mcp/bridge/enabled
mcp/bridge/bind_address
mcp/bridge/port
mcp/bridge/auto_select_port
mcp/bridge/start_with_editor
mcp/bridge/token
mcp/bridge/tool_policy/<tool_name>
mcp/bridge/audit_log_enabled
```

Token 不写入项目 `project.godot`、场景或资源。需检查 Godot 现有 EditorSettings 的敏感设置存储能力；若不存在安全存储机制，第一阶段可在每次启动生成临时 Token，而不是持久化明文 Token。

### 5.5 网络与线程模型

- 仅 `target=editor` 且 `mcp_bridge=yes` 时编译/启用；
- HTTP Server 只允许绑定 `127.0.0.1` 或 `::1`；
- 网络接收、JSON 解析和连接处理不能阻塞主线程；
- 所有 Editor API、场景修改和 UI 更新必须回到 Godot 主线程执行；
- 将请求转为有界队列，由主线程在安全时机分派；
- 每个请求有 `request_id`、超时、最大 payload 和错误码；
- 停止服务时拒绝新请求、取消/完成队列、关闭会话；
- 不在网络线程直接访问 `EditorNode`、`SceneTree`、Resource 或 Viewport。

## 6. C++ 代码规范与中文注释

### 6.1 Godot 代码风格：强制要求

所有新 C++ 代码必须遵循当前 Godot 源码的格式和惯例：

- 使用 tab 缩进；不使用空格缩进；
- 大括号、命名、`ClassDB` 绑定、`GDCLASS`、宏、include 顺序与同目录 Godot C++ 文件保持一致；
- 类名使用 `PascalCase`，成员函数使用 `snake_case`，成员变量使用 Godot 现有前缀习惯；
- 使用 Godot 核心容器和类型：`String`、`StringName`、`Vector`、`HashMap`、`Dictionary`、`Variant`、`Ref<T>`；
- 资源和对象生命周期使用 `RefCounted`、`Ref<T>`、`ObjectID`、`memnew` / `memdelete`、`Callable` 等 Godot 约定；
- 错误处理使用 `ERR_FAIL_*`、`ERR_PRINT`、`WARN_PRINT` 或现有编辑器惯用模式；
- 用户可见文本使用 `TTR()` / `RTR()` 等 Godot 编辑器翻译宏；
- 不引入 STL 容器、异常、RTTI、第三方网络/JSON/UI 库或不符合现有依赖策略的新库；
- 提交前运行仓库已有格式化/静态检查流程（如 `.clang-format`、`.clang-tidy` 和项目约定的 pre-commit）。

### 6.2 中文注释要求

新代码需要中文注释，但注释必须解释**意图、约束、线程边界、安全原因或非显然行为**，不能逐行翻译代码。

必须添加中文注释的位置：

- MCP 仅允许 loopback 监听的安全边界；
- 网络线程与 Godot 主线程之间的任务投递；
- Token 的生命周期、脱敏和禁止审计规则；
- Tool 权限、风险和确认策略的判定；
- 事务/UndoRedo 的提交与回滚边界；
- 截图读回或异步资源等待的生命周期；
- 任何因 Godot 编辑器内部时序而必须存在的延迟/队列。

示例：

```cpp
// MCP 仅允许回环地址，避免将高权限编辑器自动化接口暴露到局域网或公网。
if (!MCPSettings::is_loopback_address(p_address)) {
	return ERR_INVALID_PARAMETER;
}

// 网络线程不能直接访问 EditorNode；将请求投递到主线程后再执行编辑器操作。
_pending_requests.push_back(p_request);
```

不需要的注释示例：

```cpp
// 设置端口。
port = p_port;
```

所有中文注释使用 UTF-8，术语统一：

```text
工具（Tool）
服务（Service）
事务（Transaction）
审计（Audit）
回环地址（Loopback Address）
主线程（Main Thread）
请求（Request）
```

## 7. 开发阶段

### Phase 0A：编辑器面板骨架

实现：

- 新增 `editor/plugins/mcp/`；
- 注册 `MCPBridgeEditorPlugin` 和底部 `MCP` 面板；
- Service 状态模型：Disabled / Starting / Running / Stopping / Error；
- 左侧 Server 配置、中央 Tool 分类、右侧详情、底部 Activity 的静态/模拟数据布局；
- Tool Registry 至少登记 `godot.editor.status` 为 Implemented，其余可标为 Planned；
- EditorSettings 保存开关、loopback 地址、端口和自动端口；
- 所有关键 UI 文案通过翻译宏；
- 完成深色和浅色编辑器主题下的人工视觉检查。

验收：编辑器可构建、启动，MCP 面板可打开/关闭、调整分栏、搜索和选择 Tool；无服务启动时 UI 不报错。

### Phase 0B：服务生命周期和连接可视化

实现：

- `mcp_bridge=yes|no` SCons 选项；
- 仅 loopback 的 Streamable HTTP MCP transport；
- `initialize`、`tools/list` 和 `godot.editor.status`；
- 临时 Token、endpoint 复制、服务 Start/Stop/Restart；
- 客户端会话与活动记录；
- 非法地址、端口冲突和认证失败的 UI 错误状态；
- 更新 `build_windows_editor.bat`，增加 `--mcp-bridge` 参数。

验收：MCP Client 能通过 endpoint 连接定制编辑器，获取 `tools/list`，调用 `godot.editor.status`；面板实时显示连接和请求，停服后连接被安全关闭。

### Phase 1：Tool 策略、审计和事务外壳

实现：

- Tool 启停、确认策略、风险显示；
- 审计记录持久化与导出；
- `MCPTransactionManager` 的 begin/commit/rollback 外壳；
- 删除/覆盖类 Tool 的确认弹窗基础设施；
- 在 UI 中展示权限拒绝和确认等待状态。

验收：关闭某 Tool 后，它不再出现在 `tools/list` 或被调用时返回明确拒绝；审计记录不包含 Token；高风险调用能正确等待/拒绝确认。

### Phase 2：场景、资源、运行和截图工具

实现：

- 按总体 MCP 规划逐步接入 scene/resource/run/capture Tool；
- 控制中心展示实际 Tool 可用性和运行前置条件；
- Tool Detail 支持只读 JSON Schema 与受限测试；
- 活动区关联场景版本、变更文件、截图和错误详情。

验收：AI 完成“球 + Cube + 红色平行光”示例，Control Center 能看到完整请求链、截图、调整和最终成功结果。

## 8. 测试与验收

### 8.1 自动化测试

至少覆盖：

- `MCPSettings`：合法 loopback、非法地址、端口范围、默认值、迁移；
- `MCPToolRegistry`：分类、状态、禁用 Tool 不发布、Schema 输出；
- `MCPService`：启动/停止状态迁移、认证失败、端口占用、请求超时；
- `MCPAuditLog`：Token 脱敏、筛选、导出；
- 事务和确认策略：拒绝、取消、提交、回滚；
- JSON-RPC / MCP 初始化与 `tools/list` / `tools/call` 的协议一致性。

### 8.2 手工 UI 验收

- 深色、浅色主题下均无固定色导致的可读性问题；
- 100%、125%、150% UI 缩放下布局可用；
- 分栏可拖动，窄宽度不崩溃；
- Server 状态、错误、Tool 风险和活动记录颜色有清晰区别；
- Token 默认隐藏，复制和重新生成反馈明确；
- 面板关闭/重开、编辑器重启后设置行为符合配置；
- 未启用 `mcp_bridge` 构建时，编辑器不包含监听服务，也不影响现有功能。

### 8.3 首个端到端验收

在 `mcp_bridge=yes` 的编辑器中：

1. 从 MCP 面板启用服务，确认 endpoint、端口和 Token；
2. MCP Client 完成初始化并获得 `tools/list`；
3. 调用 `godot.editor.status`，在 Activity 中显示成功记录；
4. 后续接入场景 Tool 后，执行红色平行光示例；
5. 用编辑器/游戏截图确认球和 Cube 可见、平行光为红色；
6. 审计可导出，事务可撤销或回滚，且日志不泄露 Token。

## 9. 交付物

第一期交付必须包括：

```text
editor/plugins/mcp/ 下的源码与中文意图注释
SCons mcp_bridge 构建选项
build_windows_editor.bat 的 --mcp-bridge 支持
MCP Control Center 底部面板
Streamable HTTP MCP 的最小实现
initialize / tools/list / godot.editor.status
自动化测试与手工验收记录
本开发方案和总体 MCP 规划的同步更新
```

在 Phase 0A 未完成并验证前，不进入复杂场景生成、脚本执行、截图自动调整或简单游戏生成开发。

## 10. TODO List

> 约定：完成的事项必须从 `TODO` 移至 `Done`，并在条目中记录验证证据；未完成事项不得标记为 Done。

### TODO

- [ ] **P0-07**：构建 `mcp_bridge=yes` 编辑器，启动后人工验证面板、主题和基础交互。
- [ ] **P1-01**：实现 Tool 启停、确认策略、审计导出和事务外壳。
- [ ] **P2-01**：实现场景/资源/运行/截图工具，完成红色平行光示例闭环。

### Done

- [x] **P0-01**：创建 `editor/plugins/mcp/` 目录与最小 C++ 骨架。交付 `mcp_editor_plugin.*` 和 `mcp_control_center.*`，并以 Godot `EditorDock` 组织底部面板。
- [x] **P0-02**：在 `editor/register_editor_types.cpp` 注册 `MCPBridgeEditorPlugin`；启用 `mcp_bridge=yes` 的编辑器会添加可停靠/可浮动的 `MCP` Dock。
- [x] **P0-05**：实现第一版界面骨架：服务配置区、工具分类与搜索、工具详情、只读 JSON Schema、Token 复制/重建和临时 Activity 列表。规划中 Tool 明确标为 Planned，未发布为可调用 MCP Tool。
- [x] **P0-06**：在 `SConstruct` 添加 `mcp_bridge=yes|no`，并在 `editor/plugins/SCsub` 条件编译 MCP 源码；`build_windows_editor.bat` 已支持 `--mcp-bridge` 与 `--with-vsproj` 联用。
- [x] **P0-03**：实现真实 `MCPService` 状态、仅回环地址校验、固定/自动端口和 EditorSettings 配置；服务运行时面板显示实际 endpoint 与状态。
- [x] **P0-04**：发布最小后端 Tool 集合：`godot.editor.status` 显示为 Implemented，其余已规划 Tool 保持 Planned 且不会发布给 MCP Client。
- [x] **P0-08**：实现本机 Streamable HTTP 风格 JSON-RPC 端点 `POST /mcp`、Bearer Token 认证、`initialize`、`tools/list` 和 `tools/call(godot.editor.status)`。验证：在 `http://127.0.0.1:30100/mcp` 获得三项 HTTP 200 JSON-RPC 响应；无效 Token 返回 HTTP 401。
- [x] **Build-01**：执行 `build_windows_editor.bat --mcp-bridge`，SCons 构建与 `--version` 验证均以退出码 0 完成；随后启动 MCP-enabled 编辑器，进程 PID 为 `35860`。
- [x] **Build-02**：Phase 0B 最终构建 `build_windows_editor.bat --mcp-bridge` 成功；打开 `user_temp/godot_learn` 后监听 `127.0.0.1:30100` 并完成 MCP 协议验证。
