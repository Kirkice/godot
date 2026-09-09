# 运行、调试与画面捕获源码

本文件描述引擎编辑器运行与调试模块，不描述外部自动化协议。

## 运行控制

| 文件 | 负责内容 |
|---|---|
| `editor/run/editor_run.cpp/h` | 启动、停止和管理编辑器运行实例 |
| `editor/run/editor_run_bar.cpp/h` | 编辑器运行栏和运行目标 |
| `editor/run/embedded_process.cpp/h` | 嵌入式游戏进程 |
| `editor/run/game_view_plugin.cpp/h` | 嵌入式游戏视图、Viewport 和显示 |
| `editor/editor_interface.cpp/h` | 编辑器级运行、场景打开和资源操作入口 |

## 调试器

| 文件 | 负责内容 |
|---|---|
| `editor/debugger/editor_debugger_node.cpp/h` | 调试器面板和当前 debugger |
| `editor/debugger/script_editor_debugger.cpp/h` | 调试会话、错误/警告计数、断点和脚本堆栈 |
| `editor/debugger/editor_debugger_server.cpp/h` | 编辑器调试服务和连接 |
| `editor/debugger/editor_profiler.cpp/h` | CPU/性能分析 |
| `editor/debugger/editor_visual_profiler.cpp/h` | 可视化性能分析 |

## 典型调用链

```text
编辑器 Run 按钮
  → EditorInterface / EditorRun
  → 项目进程或 EmbeddedProcess
  → SceneTree / MainLoop
  → ScriptDebugger / EditorDebuggerServer
```

## 调试定位关键词

```text
get_current_debugger
get_error_count
get_warning_count
is_session_active
is_breaked
get_stack_script_file
get_stack_script_line
request_stack_dump
```

## 画面捕获定位

```text
editor/run/game_view_plugin.cpp
editor/run/embedded_process.cpp
scene/main/viewport.cpp
scene/main/viewport_texture.cpp
core/image/image.cpp
```

常见链路：

```text
Viewport
  → ViewportTexture
  → Image
  → Image::save_png()
```

## 调试注意事项

- `available`/debugger 对象存在不代表游戏进程正在运行。
- 没有调试会话时堆栈文件为空、行号为负值属于正常状态。
- 编辑器 UI、SceneTree 和调试器对象通常要求主线程。
- 区分编辑器自身日志、运行项目日志和远程脚本调试器状态。
