---
name: godot-engine-guide
summary: 快速定位 Godot 引擎源码模块、文件职责和关键 API，并配置、编译、启动和 Debug Godot 源码编辑器。
---

# Godot Engine Guide

## 定位

本 Skill 只用于**Godot 引擎源码开发和 Debug 环境**。

它不包含编译后游戏制作环境的自动化接口、工具协议或工作流。游戏制作环境与引擎源码环境是两个独立边界：

- 引擎源码环境：修改引擎、编辑器、运行时、渲染、资源、脚本和调试器；
- 游戏制作环境：使用已编译引擎开发具体游戏，自动化创建场景、资源、脚本和验证内容。

## 知识库入口

```text
<engine-source-root>/user_temp/godot-knowledge/README.md
```

根据任务读取：

| 任务 | 阅读文件 |
|---|---|
| 不知道源码在哪 | `module-map.md` |
| 场景/节点/资源保存 | `scene-authoring.md`、`filesystem-resources.md` |
| GDScript/脚本错误 | `scripting.md` |
| 运行、调试、截图 | `run-debug-capture.md` |
| 插件、Dock、退出崩溃 | `editor-lifecycle.md` |
| 编译、启动、Debug 验证 | `build-validation.md` |

## 源码根目录

源码根目录必须由用户或当前开发环境明确提供。

```text
<engine-source-root>
```

不要在 Skill 中假定本机绝对路径；确认源码根目录后，再使用相对路径。

## 快速定位流程

1. 依据问题选择模块：`scene/`、`core/`、`editor/`、`modules/`。
2. 先读取 `.h`，确认类、成员、生命周期和公开 API。
3. 再读取 `.cpp`，跟踪实际调用链、错误处理和副作用。
4. 搜索类名、`get_singleton()`、`_notification()`、信号、构造/析构函数。
5. 修改前确认主线程要求、对象所有权、缓存和资源生命周期。
6. 修改后编译受影响目标，启动最小项目并检查 Debugger/Console。

## 模块地图

### 编辑器

```text
editor/editor_node.cpp/h
editor/editor_interface.cpp/h
editor/plugins/editor_plugin.cpp/h
editor/docks/editor_dock_manager.cpp/h
editor/docks/editor_dock.h
```

### 场景和节点

```text
scene/main/node.cpp/h
scene/main/scene_tree.cpp/h
scene/resources/packed_scene.cpp/h
scene/3d/node_3d.cpp/h
scene/3d/camera_3d.cpp/h
scene/3d/mesh_instance_3d.cpp/h
scene/resources/material.cpp/h
```

### 文件和资源

```text
core/io/file_access.cpp/h
core/io/dir_access.cpp/h
core/io/resource_loader.cpp/h
core/io/resource_saver.cpp/h
core/config/project_settings.cpp/h
editor/file_system/editor_file_system.cpp/h
```

### 脚本和 Debugger

```text
core/object/script_language.h/cpp
modules/gdscript/
editor/plugins/script_editor_plugin.cpp/h
editor/plugins/script_text_editor.cpp/h
editor/debugger/editor_debugger_node.cpp/h
editor/debugger/script_editor_debugger.cpp/h
editor/debugger/editor_debugger_server.cpp/h
```

### 运行和嵌入式游戏视图

```text
editor/run/editor_run.cpp/h
editor/run/editor_run_bar.cpp/h
editor/run/embedded_process.cpp/h
editor/run/game_view_plugin.cpp/h
```

## 重要开发规则

### 场景

```text
PackedScene::instantiate()
→ 修改 Node 树
→ PackedScene::pack()
→ ResourceSaver::save()
```

检查 Node 路径、Owner、根节点保护、Reparent 祖先关系和 SceneTree 生命周期。

### 资源

使用 `ResourceLoader` 和 `ResourceSaver`，注意缓存、导入状态、路径限制和保存错误。

### 文件

使用 `FileAccess`/`DirAccess`，通过 `ProjectSettings` 处理项目路径，不要把任意绝对路径直接写入编辑器功能。

### 编辑器 UI

EditorInterface、DockManager、Control 和 SceneTree 操作通常要求主线程。析构时不要假设 child 仍然属于当前 parent，调用 `remove_child()` 前检查 `get_parent()`。

### Debug

区分：

- 编辑器自身 Console；
- 运行项目的远程调试会话；
- 脚本解析/编译错误；
- 渲染线程或资源导入错误。

## Windows 编译与验证

本 Skill 同时负责 Godot 源码的配置、编译、启动和 Debug 验证。适用于 Windows 10/11、Visual Studio 2022、MSVC x64 与 SCons。

### 工作原则

1. 先确认源码根目录；在其它机器不得假定绝对路径。
2. 每步检查退出码和输出，不把命令执行过当作构建成功。
3. 优先使用项目提供的 `build_windows_editor.bat`。
4. 手动构建时显式加载 `vcvars64.bat`，并使用 `py -m SCons`。
5. 如果用户要求生成 Visual Studio 解决方案，构建成功后再使用一致参数运行 `vsproj=yes`。

### 检查 Python/SCons

```bat
where py
py --version
py -m SCons --version
```

缺少 SCons 时：

```bat
py -m pip install --user scons
```

### 检查 MSVC

使用 `vswhere.exe` 找到 Visual Studio 安装目录：

```bat
"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
```

再加载 x64 编译环境：

```bat
call "<VS_INSTALL_PATH>\VC\Auxiliary\Build\vcvars64.bat" >nul && where cl && cl
```

需要安装：

- Desktop development with C++；
- MSVC x64/x86 C++ build tools；
- Windows 10/11 SDK。

### 安装构建依赖

如果 SCons 报告 AccessKit 或 D3D12 依赖缺失：

```bat
py misc\scripts\install_accesskit.py
py misc\scripts\install_d3d12_sdk_windows.py
```

### 构建编辑器

源码根目录下优先运行：

```bat
build_windows_editor.bat
```

需要同步生成 Visual Studio 方案时：

```bat
build_windows_editor.bat --with-vsproj
```

如果没有包装脚本，使用已加载 MSVC 环境的 SCons：

```bat
py -m SCons platform=windows target=editor dev_build=yes opengl3=no -j%NUMBER_OF_PROCESSORS%
```

参数含义：

- `platform=windows`：Windows 平台；
- `target=editor`：编辑器目标；
- `dev_build=yes`：开发构建和调试符号；
- `opengl3=no`：关闭 Compatibility/OpenGL，使用 Vulkan/RD；
- `-j%NUMBER_OF_PROCESSORS%`：并行编译。

生成方案时保持相同目标参数，并添加：

```bat
py -m SCons platform=windows target=editor dev_build=yes opengl3=no vsproj=yes
```

不要用 Visual Studio“生成解决方案”替代日常 SCons 引擎构建。

### 验证产物

预期产物：

```text
bin\godot.windows.editor.dev.x86_64.exe
bin\godot.windows.editor.dev.x86_64.console.exe
```

检查版本：

```bat
bin\godot.windows.editor.dev.x86_64.console.exe --version
```

启动编辑器：

```bat
bin\godot.windows.editor.dev.x86_64.console.exe --editor --path <project-path>
```

无界面生命周期验证：

```bat
bin\godot.windows.editor.dev.x86_64.console.exe --editor --path <project-path> --quit-after 120
```

### 构建问题

- `.exe access denied`：旧编辑器进程锁定目标文件。
- 找不到 `cl.exe`：重新调用 `vcvars64.bat` 或安装 MSVC/SDK。
- `No module named SCons`：运行 `py -m pip install --user scons`。
- 缺少 AccessKit/D3D12：运行对应安装脚本后重新构建。
- 链接错误：检查新增源文件是否加入对应 `SCsub`。
- 运行时崩溃：优先检查初始化顺序、主线程、对象所有权和析构顺序。
- 资源不更新：检查 ResourceLoader 缓存、EditorFileSystem 扫描和导入状态。

更多源码验证注意事项见：

```text
user_temp/godot-knowledge/build-validation.md
```

## 输出定位结果格式

```text
模块：<模块>
文件：<相对路径>
类/函数：<符号>
职责：<功能>
调用链：<入口 → 核心 API → 结果>
注意事项：<线程/所有权/缓存/生命周期>
验证：<构建或 Debug 方法>
```

## 禁止事项

- 不要只凭文件名猜测模块职责。
- 不要把编辑器 API 当作可跨线程调用的普通函数。
- 不要忽略 ResourceLoader 缓存和 EditorFileSystem 导入状态。
- 不要在没有最小复现和构建验证的情况下声称修复完成。
- 不要把游戏制作环境的自动化能力混入本引擎源码知识库。
