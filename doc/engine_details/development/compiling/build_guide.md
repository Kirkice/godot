# Godot 源码构建指南

本文说明如何在常见桌面平台上从 Godot 源码构建编辑器，以及如何通过本仓库的 `godot-windows-build-run` Skill 让 AI 协助完成 Windows 构建。

> 本仓库的主构建系统为 **SCons**，根目录入口是 `SConstruct`。Visual Studio、Xcode 和 CLion 可以用于编辑、索引或调试，但它们不是替代 SCons 的主构建系统。

## 通用准备

1. 获取完整 Godot 源码，并在**源码根目录**执行命令（该目录应包含 `SConstruct`、`core`、`editor`、`platform` 等内容）。
2. 确保 Python 3 可用。
3. 安装 SCons：

   ```sh
   python3 -m pip install --user scons
   ```

   Windows 推荐使用：

   ```bat
   py -m pip install --user scons
   ```

4. 验证 SCons：

   ```sh
   python3 -m SCons --version
   ```

   Windows：

   ```bat
   py -m SCons --version
   ```

## Windows（Visual Studio / MSVC）

### 前置条件

安装 Visual Studio 2022 Community 或 Build Tools，并在 Visual Studio Installer 中选中：

- **Desktop development with C++**；
- MSVC x64/x86 C++ build tools；
- Windows 10 或 Windows 11 SDK。

首次构建前，如 SCons 报 AccessKit 或 Direct3D 12 依赖缺失，在源码根目录执行：

```bat
py misc\scripts\install_accesskit.py
py misc\scripts\install_d3d12_sdk_windows.py
```

### 一键构建脚本（推荐）

源码根目录包含 `build_windows_editor.bat`。它会根据脚本自身所在目录定位源码树，通过 Visual Studio Installer 附带的 `vswhere.exe` 自动发现 MSVC，随后构建、验证编辑器；脚本中**没有硬编码的绝对安装路径**。

可从源码根目录或任意工作目录的 `cmd.exe` 调用：

```bat
build_windows_editor.bat
```

同时构建编辑器并生成/更新 Visual Studio 解决方案：

```bat
build_windows_editor.bat --with-vsproj
```

查看参数说明：

```bat
build_windows_editor.bat --help
```

脚本要求系统可通过 PATH 找到 `py`，且已安装 SCons、Visual Studio 2022（或 Build Tools）及 C++ 桌面开发组件。若 SCons 提示 AccessKit 或 D3D12 依赖缺失，先执行本节前置条件中的两个依赖安装命令。

### 手动构建开发版编辑器（Vulkan/RD）

在源码根目录的 `cmd.exe` 中执行。以下路径是本机已验证的 VS 2022 Community 路径；如果你的安装位置不同，请替换它：

```bat
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul && py -m SCons platform=windows target=editor dev_build=yes opengl3=no -j%NUMBER_OF_PROCESSORS%
```

参数说明：

- `platform=windows`：构建 Windows 平台；
- `target=editor`：构建 Godot 编辑器；
- `dev_build=yes`：开发构建，带调试符号；
- `opengl3=no`：关闭 Compatibility/OpenGL 渲染器，保留 Vulkan/RD；
- `-j%NUMBER_OF_PROCESSORS%`：按逻辑 CPU 核数并行编译。内存不足时可改为 `-j8` 等较小值。

构建产物：

```text
bin\godot.windows.editor.dev.x86_64.exe
bin\godot.windows.editor.dev.x86_64.console.exe
```

验证版本：

```bat
bin\godot.windows.editor.dev.x86_64.console.exe --version
```

启动编辑器：

```bat
start "Godot Engine Dev Build" /D "%CD%" "bin\godot.windows.editor.dev.x86_64.exe"
```

### 生成 Visual Studio 解决方案

`.sln` 不是 Godot 的主构建入口，而是由 SCons 生成，适合在 Visual Studio 中浏览、编辑、索引和调试：

```bat
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul && py -m SCons platform=windows target=editor dev_build=yes opengl3=no vsproj=yes
```

生成文件包括：

```text
godot.sln
godot.vcxproj
godot.vcxproj.filters
```

请在 VS 中打开 `godot.sln`。日常引擎编译仍建议使用上面的 SCons 构建命令。调试时，可将 `bin\godot.windows.editor.dev.x86_64.exe` 设置为启动程序，工作目录设为源码根目录。

## Linux

### 前置条件

以 Debian/Ubuntu 为例，安装 C++ 工具链、Python、SCons 和常见桌面依赖：

```sh
sudo apt update
sudo apt install build-essential scons pkg-config python3 python3-dev \
  libx11-dev libxcursor-dev libxinerama-dev libgl1-mesa-dev \
  libglu1-mesa-dev libpulse-dev libasound2-dev libudev-dev \
  libwayland-dev libdecor-0-dev
```

不同发行版的软件包名称可能不同。请以 Godot 对应版本的官方编译文档为准。

### 构建开发版编辑器

```sh
python3 -m SCons platform=linuxbsd target=editor dev_build=yes -j"$(nproc)"
```

常见产物：

```text
bin/godot.linuxbsd.editor.dev.x86_64
```

验证并启动：

```sh
bin/godot.linuxbsd.editor.dev.x86_64 --version
./bin/godot.linuxbsd.editor.dev.x86_64
```

## macOS

### 前置条件

1. 安装 Xcode 和 Command Line Tools：

   ```sh
   xcode-select --install
   ```

2. 安装 Python 与 SCons。例如使用 Homebrew：

   ```sh
   brew install python scons
   ```

   或：

   ```sh
   python3 -m pip install --user scons
   ```

### 构建开发版编辑器

在源码根目录执行：

```sh
python3 -m SCons platform=macos target=editor dev_build=yes -j"$(sysctl -n hw.ncpu)"
```

常见产物位于 `bin/` 下，名称会包含 `macos`、`editor`、`dev` 和目标架构。可用下列命令确认：

```sh
ls -lh bin/
```

构建完成后，通过实际生成的可执行文件运行 `--version`，再启动编辑器。若要构建 Universal Binary 或指定 `arch=arm64` / `arch=x86_64`，请依据所使用 Godot 版本的官方 macOS 编译文档选择参数。

## 构建配置提示

- 需要 Compatibility/OpenGL 渲染器时，移除 Windows 命令中的 `opengl3=no`。
- 修改 C++、渲染 shader 或生成代码后，重复同一条 SCons 命令即可进行增量构建。
- 若 Windows 构建输出 `'.' is up to date`，表示目标已经是最新状态。
- `msgfmt not found` 通常只是翻译资源预编译的非阻断警告；只有需要生成 `.mo` 翻译文件时才需要安装 GNU gettext。

## 通过 Skill 让 AI 协助构建（Windows）

本仓库提供的 Skill 文件为：

```text
user_temp/godot-skill/SKILL.md
```

其名称是：

```text
godot-windows-build-run
```

向 AI 发出类似以下请求即可：

```text
请使用 godot-windows-build-run Skill 构建 Godot。源码根目录是 D:\Program Files\godot。
```

也可以明确提出需要生成 Visual Studio 解决方案：

```text
请使用 godot-windows-build-run Skill 构建 Godot，并同时生成 Visual Studio 解决方案。源码根目录是 D:\Program Files\godot。
```

Skill 的工作流程会：

1. 确认源码根目录；
2. 检查 Python、SCons、MSVC 和 Windows SDK；
3. 在必要时安装 AccessKit 与 D3D12 依赖；
4. 通过 SCons 构建编辑器；
5. 验证生成的可执行文件和版本；
6. 启动编辑器；
7. 每次用户请求构建时询问是否还需要生成/更新 Visual Studio `.sln`（如果请求中已明确说明需要，则不会重复询问）。

> Skill 当前面向 Windows + Visual Studio/MSVC。Linux 或 macOS 构建请按本指南的命令执行，并根据所用 Godot 分支的官方文档补齐平台特定依赖。
