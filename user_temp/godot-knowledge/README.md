# Godot Engine 源码知识库

本目录只记录 Godot 引擎源码的模块结构、文件职责、关键 API、调用关系、调试方法和构建验证方法。

它服务于**引擎源码开发和 Debug 环境**，不记录编译后游戏制作环境的自动化工具协议。

## 知识文件

| 文件 | 内容 |
|---|---|
| `module-map.md` | 编辑器、场景、资源、脚本、运行、调试和构建模块总览 |
| `scene-authoring.md` | 场景、PackedScene、Node、变换、材质和保存流程 |
| `scripting.md` | GDScript、脚本编辑器、脚本加载和验证 |
| `run-debug-capture.md` | 编辑器运行控制、调试器和画面捕获源码 |
| `filesystem-resources.md` | 文件系统、项目设置、资源加载和资源保存 |
| `editor-lifecycle.md` | EditorPlugin、EditorDock、Node 生命周期和退出清理 |
| `build-validation.md` | Godot 源码编译、启动、Debug 和常见构建问题 |

## 源码根目录

```text
D:\Program Files\godot
```

## 定位原则

1. 先按模块阅读 `module-map.md`。
2. 先读头文件，再读对应实现文件。
3. 沿着入口、对象所有权、线程要求、文件副作用和错误处理追踪调用链。
4. 修改后先做增量构建，再运行最小验证场景或测试。
5. 不要只依据文件名猜测职责，至少检查类声明和调用点。
