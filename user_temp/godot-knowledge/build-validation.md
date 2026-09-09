# Godot 引擎编译与 Debug 验证

本文件只描述引擎源码构建和运行验证，不描述游戏制作环境的自动化工具。

## 源码根目录

```text
D:\Program Files\godot
```

## Windows 编辑器构建

如果源码树包含项目包装脚本，运行：

```bat
build_windows_editor.bat
```

或按项目约定指定开发构建参数。构建成功需要同时检查退出码、版本输出和目标二进制，不要只根据命令启动过判断成功。

产物通常位于：

```text
bin\godot.windows.editor.dev.x86_64.exe
bin\godot.windows.editor.dev.x86_64.console.exe
```

## 启动 Debug 编辑器

```bat
bin\godot.windows.editor.dev.x86_64.console.exe --editor --path <project-path>
```

无界面或启动验证可使用：

```bat
bin\godot.windows.editor.dev.x86_64.console.exe --editor --path <project-path> --quit-after 120
```

## 验证顺序

1. 检查源码改动涉及的 `SCsub` 和头文件依赖。
2. 增量编译修改过的模块。
3. 启动编辑器和最小测试项目。
4. 打开受影响的场景/脚本/资源。
5. 触发目标功能并检查 Console、Debugger 和资源状态。
6. 进行正常关闭，检查退出日志和残留进程。
7. 必要时使用 Debug 构建、断点或堆栈定位。

## 常见构建问题

- `.exe access denied`：旧编辑器进程仍锁定目标文件。
- 编译错误：先读完整错误位置和包含链，再检查 API 版本。
- 链接错误：检查新增源文件是否加入 `SCsub`。
- 运行时崩溃：区分初始化顺序、主线程、对象所有权和析构顺序。
- 资源不更新：检查 ResourceLoader 缓存、EditorFileSystem 扫描和导入状态。

## 调试原则

- 不要把“进程启动”当作“功能验证成功”。
- 每个修改都应有最小可复现项目或测试场景。
- 保存源码修改前记录调用链和对象生命周期。
