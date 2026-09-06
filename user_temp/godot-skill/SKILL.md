---
name: godot-windows-build-run
summary: 在受支持的 Windows 桌面系统上配置、编译、验证并启动 Godot 源码编辑器。
---

# Godot Windows Build and Run

## 适用范围

适用于受支持 Windows 桌面系统上的 Godot 引擎源码树，使用 Visual Studio 2022、MSVC x64 与 SCons。默认配置面向 Vulkan/RD 图形开发，关闭 OpenGL Compatibility 渲染器。

Windows 10 与 Windows 11 均可使用。除非构建目标、依赖安装或用户问题涉及特定系统 API、SDK 版本、驱动能力或兼容性差异，否则无需询问或固定 Windows 的具体版本。

## 工作原则

1. 在每个构建命令中显式调用 `vcvars64.bat`；不要假定普通终端已经加载 MSVC 环境。
2. 始终使用 `py -m SCons`，避免用户级 Python Scripts 目录未加入 PATH 时找不到 `scons`。
3. 每步检查退出码和输出；不要将命令已执行当作构建成功。
4. 缺少 AccessKit 或 D3D12 依赖时，安装依赖而非直接禁用相关功能。
5. **先确认源码根目录。** 若用户的对话或当前工作区没有明确给出 Godot 源码路径，必须先询问用户；不得推测、硬编码或使用任何绝对路径。
6. 获取源码根目录后，所有命令均以该目录作为工作目录运行；下文命令只使用相对路径。

## 1. 检查并安装 SCons

从源码根目录运行：

```bat
where py
py --version
py -m SCons --version
```

如当前 Python 缺少 SCons：

```bat
py -m pip install --user scons
```

要求安装 Visual Studio 2022 Community 或 Build Tools，并在安装器中包含：

- Desktop development with C++；
- MSVC x64/x86 C++ build tools；
- Windows 10 或 Windows 11 SDK。

## 2. 加载并验证 MSVC x64 环境

先使用 Visual Studio Installer 自带的 `vswhere` 发现安装目录：

```bat
"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
```

将输出目录记为 `VS_INSTALL_PATH`，再从**源码根目录**运行：

```bat
call "<VS_INSTALL_PATH>\VC\Auxiliary\Build\vcvars64.bat" >nul && where cl && cl
```

该命令必须找到 `cl.exe` 并显示 Microsoft C/C++ Compiler 版本。若 `vswhere` 或 `cl.exe` 不可用，使用 Visual Studio Installer 安装或修复 C++ 桌面开发、MSVC 与 Windows SDK。`<VS_INSTALL_PATH>` 是发现后的用户本机路径，不能写入或固化到 Skill。

## 3. 安装 Godot 的 Windows 依赖

如 SCons 报告 AccessKit 或 Direct3D 12 渲染驱动依赖缺失，运行：

```bat
py misc\scripts\install_accesskit.py
py misc\scripts\install_d3d12_sdk_windows.py
```

这些脚本会下载并安装 AccessKit、Mesa NIR、PIX Runtime 与 DirectX 12 Agility SDK 到 `%LOCALAPPDATA%\Godot\build_deps`。安装脚本的非 x86_64 辅助工具提示不一定代表目标编辑器构建失败；以脚本的最终退出码和成功信息为准。

## 4. 构建 Vulkan/RD 开发编辑器

在已确认的源码根目录运行，并将 `<VS_INSTALL_PATH>` 替换为第 2 步发现的路径：

```bat
call "<VS_INSTALL_PATH>\VC\Auxiliary\Build\vcvars64.bat" >nul && py -m SCons platform=windows target=editor dev_build=yes opengl3=no -j%NUMBER_OF_PROCESSORS%
```

参数说明：

- `platform=windows`：构建 Windows 平台；
- `target=editor`：构建编辑器；
- `dev_build=yes`：采用适合引擎和渲染调试的开发构建，保留调试符号；
- `opengl3=no`：关闭 Compatibility/OpenGL 渲染器，保留 Vulkan/RD/Forward+ 路径；若需测试 Compatibility，移除此参数；
- `-j%NUMBER_OF_PROCESSORS%`：按逻辑 CPU 核数并行构建。内存紧张时改为较小的数值，例如 `-j8`。

若输出 `'.' is up to date`，表示增量构建已成功确认目标为最新状态。

## 5. 验证构建产物

预期二进制：

- `bin\godot.windows.editor.dev.x86_64.exe`；
- `bin\godot.windows.editor.dev.x86_64.console.exe`。

运行无窗口版本验证：

```bat
bin\godot.windows.editor.dev.x86_64.console.exe --version
```

命令应输出 Godot 版本字符串并以退出码 0 返回。

## 6. 启动引擎编辑器

在已确认的源码根目录异步启动：

```bat
start "Godot Engine Dev Build" /D "%CD%" "bin\godot.windows.editor.dev.x86_64.exe"
```

`/D` 显式指定工作目录，确保编辑器可正确解析源码树中的相对路径。启动 GUI 后不必等待其退出。

## 日常增量构建

修改 C++、RD shader 或生成代码后，重复第 4 步。普通实现文件改动通常只重编译受影响对象；修改 RenderingDevice API、渲染公共头文件或 shader 生成基础设施时，可能产生较大规模重编译。

## 常见问题

### `No module named SCons`

运行：

```bat
py -m pip install --user scons
```

### 找不到 `cl.exe`

检查是否在构建命令中调用了 `vcvars64.bat`。若已调用仍失败，在 Visual Studio Installer 中安装 C++ 工具链与 Windows SDK。

### 缺少 AccessKit 或 D3D12 依赖

重复第 3 步的安装脚本，然后重新执行第 4 步。

### `msgfmt not found`

该警告通常不阻断构建；Godot 会回退使用 `.po` 翻译文件。只有需要预编译 `.mo` 翻译资源时才需单独安装 GNU gettext。
