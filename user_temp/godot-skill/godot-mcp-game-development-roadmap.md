# Godot MCP 完整游戏开发能力路线图

> 目标：将当前面向受控 3D 场景编辑和视觉验证的 Godot MCP，逐步扩展为覆盖游戏开发、调试、测试和发布的完整本地开发工具。
>
> 工具设计原则：对 LLM 暴露少量高层语义工具；旧的细粒度工具仅作为隐藏兼容 RPC 保留。

## 当前对外工具面

```text
godot.project.inspect
godot.project.scan
godot.scene.inspect
godot.scene.mutate
godot.script.inspect
godot.script.edit
godot.script.search
godot.resource.inspect
godot.resource.mutate
godot.run
godot.run.diagnostics
godot.visual.capture
godot.test
godot.build
```

旧工具、事务、确认和审计接口不出现在 `tools/list`，仅用于内部兼容和服务控制。

## 1. 当前已有能力

| 能力分类 | MCP 工具 | 状态 |
|---|---|---|
| 编辑器状态 | `godot.editor.status` | 已有 |
| 场景打开 | `godot.editor.open_scene` | 已有 |
| 场景创建 | `godot.scene.create` | 已有；Control Center 显示状态需要与服务注册保持同步 |
| 节点操作 | `godot.scene.add_node`、`remove_node`、`rename_node`、`reparent_node` | 已有 |
| 3D 节点 | `godot.scene.add_3d_node` | 已有 |
| 变换修改 | `godot.scene.set_transform` | 已有 |
| 受控属性修改 | `godot.scene.set_property` | 已有；使用白名单 |
| 基础网格 | `godot.scene.set_mesh` | 已有 |
| 基础材质 | `godot.scene.set_material` | 已有；目前主要面向 StandardMaterial3D 基础颜色 |
| 资源创建 | `godot.resource.create` | 已有；受控资源类型 |
| 主场景设置 | `godot.project.set_main_scene` | 已有 |
| 运行控制 | `godot.run.current_scene`、`godot.run.stop` | 已有 |
| 编辑器截图 | `godot.capture.editor_view` | 已有 |
| 运行视图截图 | `godot.run.capture_view` | 已有 |
| Camera3D Buffer 截图 | `godot.run.capture_camera_view` | 已有；用于实际游戏画面验证 |
| 事务 | `godot.transaction.begin`、`commit`、`rollback` | 已有 |
| 写入确认 | `godot.confirmation.approve`、`reject` | 已有 |
| 审计 | `godot.audit.clear`、`export` | 已有 |
| 安全边界 | Loopback-only：`127.0.0.1` / `::1` | 已有 |

## 2. 第一阶段：编程和调试闭环（最高优先级）

目标：让 Agent 能够读取项目、编写游戏逻辑、运行项目、读取错误并修复问题。

| 优先级 | 能力 | 建议工具 | 目的 |
|---|---|---|---|
| P0 | 场景树查询 | `godot.scene.list_nodes`、`godot.scene.inspect` | 修改前理解场景结构、节点类型和层级 |
| P0 | 属性读取 | `godot.scene.get_property`、`godot.resource.inspect` | 修改前获取真实状态，避免盲目覆盖 |
| P0 | 脚本创建 | `godot.script.create` | 创建 GDScript/C# 脚本文件 |
| P0 | 脚本读取 | `godot.script.read` | 理解现有代码 |
| P0 | 脚本写入 | `godot.script.write` | 创建或修改游戏逻辑 |
| P0 | 脚本挂载 | `godot.script.attach`、`godot.script.detach` | 建立场景节点和脚本关系 |
| P0 | 代码搜索 | `godot.script.search` | 搜索类、函数、信号和依赖 |
| P0 | 脚本验证 | `godot.script.validate` | 写入后检查语法和编译错误 |
| P0 | 运行日志 | `godot.run.get_output` | 获取 Godot 输出和游戏调试日志 |
| P0 | 运行错误 | `godot.run.get_errors` | 获取错误、警告和异常 |
| P0 | 堆栈信息 | `godot.run.get_stack_trace` | 定位脚本错误和具体代码行 |
| P0 | 项目扫描 | `godot.project.scan` | 检查资源、脚本和项目状态 |

### 第一阶段完成标准

```text
读取场景
→ 查询节点和属性
→ 写入脚本
→ 挂载脚本
→ 运行场景
→ 获取日志和错误
→ 定位堆栈
→ 修复代码或场景
→ 再次运行验证
```

## 3. 第二阶段：基础游戏系统

| 优先级 | 能力 | 建议工具 | 目的 |
|---|---|---|---|
| P1 | UI 节点 | `godot.ui.add_node`、`godot.ui.set_property` | 创建菜单、HUD、按钮和控件 |
| P1 | UI 布局 | `godot.ui.set_layout` | 设置 Anchor、Offset 和 Container 布局 |
| P1 | UI 主题 | `godot.ui.set_theme` | 设置字体、颜色和控件样式 |
| P1 | UI 信号 | `godot.ui.connect_signal` | 连接按钮和控件交互 |
| P1 | 输入动作 | `godot.input.create_action`、`remove_action` | 管理 InputMap 动作 |
| P1 | 键盘绑定 | `godot.input.bind_key` | WASD、跳跃、攻击等操作 |
| P1 | 鼠标绑定 | `godot.input.bind_mouse` | 点击、瞄准和视角控制 |
| P1 | 手柄绑定 | `godot.input.bind_gamepad` | 支持手柄操作 |
| P1 | 物理节点 | `godot.physics.add_body`、`add_area` | 创建刚体、角色体和区域 |
| P1 | 碰撞形状 | `godot.physics.set_shape` | 配置 CollisionShape 和 Shape3D |
| P1 | 碰撞层 | `godot.physics.set_layers` | 管理 Layer 和 Mask |
| P1 | 角色控制 | `godot.physics.configure_character` | 配置 CharacterBody 行为 |
| P1 | 动画创建 | `godot.animation.create` | 创建 AnimationPlayer 动画 |
| P1 | 动画轨道 | `godot.animation.add_track`、`set_key` | 添加轨道和关键帧 |
| P1 | 动画播放 | `godot.animation.play` | 播放、停止和切换动画 |
| P1 | 音频播放器 | `godot.audio.add_player` | 添加音效和音乐播放器 |
| P1 | 音频控制 | `godot.audio.play`、`stop`、`set_volume` | 控制播放和音量 |
| P1 | 音频总线 | `godot.audio.set_bus` | 管理混音和音量分组 |

## 4. 第三阶段：资产和视觉生产

| 优先级 | 能力 | 建议工具 | 目的 |
|---|---|---|---|
| P1 | 资产列表 | `godot.asset.list` | 查询项目资源 |
| P1 | 资产导入 | `godot.asset.import` | 导入 PNG、GLB、WAV 等资产 |
| P1 | 重新导入 | `godot.asset.reimport` | 更新外部资源 |
| P1 | 资产检查 | `godot.asset.inspect` | 查询资源类型、依赖和导入状态 |
| P1 | 引用查询 | `godot.asset.find_references` | 安全删除或替换资源 |
| P1 | 资产移动 | `godot.asset.move` | 整理资源目录 |
| P1 | PBR 材质 | `godot.material.set_pbr` | 设置 Albedo、Metallic、Roughness |
| P1 | 纹理设置 | `godot.material.set_texture` | 设置 Albedo、Normal、Roughness 等纹理 |
| P1 | Shader | `godot.shader.create`、`godot.shader.write` | 创建和编辑 Shader |
| P1 | 粒子 | `godot.particles.create`、`configure` | 创建火焰、烟雾和特效 |
| P1 | 环境 | `godot.environment.configure` | 设置天空、雾、曝光和后处理 |
| P1 | 灯光 | `godot.light.configure` | 配置颜色、范围、衰减和阴影 |
| P1 | 摄像机 | `godot.camera.configure` | 配置镜头、跟随和视角 |

## 5. 第四阶段：项目配置

| 优先级 | 能力 | 建议工具 | 目的 |
|---|---|---|---|
| P1 | 设置读取 | `godot.project.get_setting` | 读取 ProjectSettings |
| P1 | 设置修改 | `godot.project.set_setting` | 修改 ProjectSettings |
| P1 | 窗口配置 | `godot.project.set_display` | 分辨率、拉伸和全屏设置 |
| P1 | 渲染配置 | `godot.project.set_rendering` | 渲染器、阴影和质量设置 |
| P1 | 物理配置 | `godot.project.set_physics` | 物理帧率和重力设置 |
| P1 | 输入配置 | `godot.project.get_input_map` | 读取和管理 InputMap |
| P1 | 自动加载 | `godot.project.set_autoload` | 配置全局管理器 |
| P1 | 插件管理 | `godot.project.set_plugin_state` | 启用和禁用项目插件 |
| P1 | 导出预设 | `godot.project.export_presets` | 准备发布配置 |

## 6. 第五阶段：测试、性能和发布

| 优先级 | 能力 | 建议工具 | 目的 |
|---|---|---|---|
| P0 | 场景测试 | `godot.test.run_scene` | 单独验证指定场景 |
| P0 | 脚本测试 | `godot.test.run_script` | 运行脚本测试 |
| P0 | 项目检查 | `godot.project.check` | 检查资源、脚本和场景错误 |
| P1 | 测试套件 | `godot.test.run` | 运行自动化测试 |
| P1 | 远程场景树 | `godot.debugger.remote_tree` | 查看运行时真实节点 |
| P1 | 远程属性 | `godot.debugger.inspect` | 查看运行时属性 |
| P1 | 性能统计 | `godot.profiler.get_stats` | 获取 FPS、内存和 Draw Call |
| P1 | 性能采样 | `godot.profiler.start`、`stop` | 定位性能瓶颈 |
| P1 | 项目构建 | `godot.build.project` | 验证项目可构建 |
| P1 | 项目导出 | `godot.export.project` | 导出 Windows、Web 等平台 |
| P1 | 构建日志 | `godot.build.get_output` | 诊断构建和导出失败 |
| P2 | 资源热加载 | `godot.run.reload_resources` | 加快迭代 |
| P2 | 截图回归 | `godot.test.capture_reference` | 自动捕获和对比截图 |
| P2 | 性能基准 | `godot.test.benchmark` | 持续性能验证 |

## 7. 完整流程能力矩阵

| 开发阶段 | 必需能力 | 当前支持情况 |
|---|---|---|
| 项目结构 | 项目扫描、资源创建、目录管理 | 部分支持 |
| 场景搭建 | 场景创建、节点添加、节点查询 | 创建和添加已有；查询不足 |
| 3D 制作 | Mesh、材质、灯光、相机、环境 | 基础能力已有 |
| UI 制作 | Control、布局、主题、信号 | 基本缺失 |
| 游戏逻辑 | 脚本创建、读取、写入、挂载 | 缺失 |
| 输入系统 | InputMap、键盘、鼠标、手柄 | 缺失 |
| 角色系统 | CharacterBody、碰撞、动画 | 缺失 |
| 交互系统 | Area、信号、脚本 | 缺失 |
| 动画系统 | AnimationPlayer、轨道、关键帧 | 缺失 |
| 音频系统 | AudioStream、AudioBus、播放控制 | 缺失 |
| 运行控制 | 启动、停止、当前场景 | 已有 |
| 错误调试 | 日志、错误、堆栈 | 缺失 |
| 视觉验证 | Camera3D 截图、编辑器截图 | 已有 |
| 自动测试 | 场景测试、脚本测试、项目检查 | 缺失 |
| 性能优化 | Profiler、远程树、运行时属性 | 缺失 |
| 发布构建 | Export Preset、导出、构建日志 | 缺失 |

## 8. 建议开发顺序

### 第一批：最小可用游戏开发闭环

```text
godot.scene.list_nodes
godot.scene.inspect
godot.scene.get_property
godot.scene.set_property

godot.script.create
godot.script.read
godot.script.write
godot.script.attach
godot.script.validate

godot.run.current_scene
godot.run.stop
godot.run.get_output
godot.run.get_errors
godot.run.get_stack_trace

godot.project.get_setting
godot.project.set_setting
```

完成后，Agent 可以：

```text
读取场景
→ 修改场景
→ 编写脚本
→ 挂载脚本
→ 运行游戏
→ 读取错误
→ 修复问题
→ 再次验证
```

### 第二批：游戏基础系统

```text
godot.input.*
godot.ui.*
godot.physics.*
godot.animation.*
godot.audio.*
```

目标：支持玩家移动、碰撞、交互、UI、动画、音效和基础游戏循环。

### 第三批：资产和视觉生产

```text
godot.asset.*
godot.material.*
godot.shader.*
godot.particles.*
godot.environment.*
godot.light.*
godot.camera.*
```

目标：支持真实资产导入、材质、Shader、粒子、环境和完整镜头制作。

### 第四批：测试、性能和发布

```text
godot.debugger.*
godot.profiler.*
godot.test.*
godot.build.*
godot.export.*
```

目标：覆盖测试、调试、性能分析、构建和最终导出。

## 9. 实现原则

1. 所有文件写入限制在当前 Godot 项目目录内。
2. 写入操作继续使用事务、确认和审计机制。
3. 优先提供读取工具，再提供对应写入工具。
4. 工具输入使用明确的 JSON Schema，并拒绝未知字段。
5. 脚本、资源和项目设置修改后，应触发必要的扫描、重载或编译检查。
6. 运行工具应返回进程状态、退出状态、日志和错误摘要。
7. 截图工具继续支持异步请求，并明确返回截图生成状态。
8. 保持 MCP 服务 loopback-only，不提供公网或局域网监听。
9. 工具注册表、Control Center 展示和实际实现必须保持同步。
10. 每增加一组 MCP 工具，都应补充成功、失败、权限和回滚验证。
