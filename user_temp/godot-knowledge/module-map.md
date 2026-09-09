# Godot 引擎源码模块地图

本文件只描述引擎源码，不描述编译后游戏制作环境的自动化接口。

## 编辑器基础

| 路径 | 负责模块 | 关键对象 |
|---|---|---|
| `editor/editor_node.cpp/h` | 编辑器主窗口、编辑器生命周期、面板和主循环 | `EditorNode` |
| `editor/editor_interface.cpp/h` | 编辑器对插件暴露的场景、资源和运行入口 | `EditorInterface` |
| `editor/plugins/editor_plugin.cpp/h` | 编辑器插件基类和插件生命周期 | `EditorPlugin` |
| `editor/docks/editor_dock.h` | 编辑器 Dock 基类 | `EditorDock` |
| `editor/docks/editor_dock_manager.cpp/h` | Dock 注册、移除、布局和重父级 | `EditorDockManager` |
| `editor/plugins/` | 各类编辑器插件，包括脚本、动画、场景和资源编辑器 | 各插件类 |

## 场景和节点

| 路径 | 负责模块 |
|---|---|
| `scene/main/node.cpp/h` | Node 父子树、名称、Owner、路径和生命周期 |
| `scene/main/scene_tree.cpp/h` | SceneTree、节点进入/退出树、组和消息 |
| `scene/resources/packed_scene.cpp/h` | 场景实例化和保存打包 |
| `scene/3d/node_3d.cpp/h` | 3D 变换和空间层级 |
| `scene/3d/camera_3d.cpp/h` | 3D 相机和视锥 |
| `scene/3d/light_3d.cpp/h` | 3D 灯光 |
| `scene/3d/mesh_instance_3d.cpp/h` | Mesh 实例和材质覆盖 |
| `scene/resources/3d/primitive_meshes.cpp/h` | 基础 Mesh 资源 |
| `scene/resources/material.cpp/h` | 材质资源 |

## 脚本系统

| 路径 | 负责模块 |
|---|---|
| `core/object/script_language.h/cpp` | Script 和 ScriptLanguage 抽象接口 |
| `modules/gdscript/` | GDScript 解析、编译、运行时和语言服务器相关实现 |
| `editor/plugins/script_editor_plugin.cpp/h` | 脚本工作区和脚本文件管理 |
| `editor/plugins/script_text_editor.cpp/h` | 文本编辑、语法高亮和错误显示 |

## 文件和资源

| 路径 | 负责模块 |
|---|---|
| `core/io/file_access.cpp/h` | 文件读写 |
| `core/io/dir_access.cpp/h` | 目录访问和遍历 |
| `core/io/resource_loader.cpp/h` | Resource 加载 |
| `core/io/resource_saver.cpp/h` | Resource 保存 |
| `core/config/project_settings.cpp/h` | 项目配置和路径转换 |
| `editor/file_system/editor_file_system.cpp/h` | 编辑器资源扫描和导入数据库 |
| `editor/file_system/editor_paths.cpp/h` | 编辑器路径 |

## 运行和调试

| 路径 | 负责模块 |
|---|---|
| `editor/run/editor_run.cpp/h` | 编辑器运行控制 |
| `editor/run/editor_run_bar.cpp/h` | 运行按钮和运行目标 |
| `editor/run/embedded_process.cpp/h` | 嵌入式游戏进程 |
| `editor/run/game_view_plugin.cpp/h` | 嵌入式游戏视图 |
| `editor/debugger/editor_debugger_node.cpp/h` | 调试器 Dock 和当前调试会话 |
| `editor/debugger/script_editor_debugger.cpp/h` | 运行时错误、警告、断点和堆栈 |
| `editor/debugger/editor_debugger_server.cpp/h` | 编辑器调试连接服务 |
| `editor/debugger/editor_profiler.cpp/h` | 性能分析 |
| `editor/debugger/editor_visual_profiler.cpp/h` | 可视化性能分析 |

## 构建系统

| 路径 | 负责模块 |
|---|---|
| `SConstruct` | Godot 主构建入口 |
| `methods.py` | SCons 辅助逻辑 |
| `SCsub` | 子目录构建规则 |
| `build_windows_editor.bat` | 本环境的 Windows 编辑器构建包装脚本 |
