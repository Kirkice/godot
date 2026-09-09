# GDScript 与脚本编辑源码定位

本文件面向引擎源码开发和脚本系统 Debug。

## 主要文件

| 路径 | 负责内容 |
|---|---|
| `core/object/script_language.h/cpp` | Script、ScriptLanguage 抽象接口 |
| `modules/gdscript/` | GDScript 解析、编译、运行时和语言支持 |
| `editor/plugins/script_editor_plugin.cpp/h` | 脚本工作区、打开脚本和脚本编辑器管理 |
| `editor/plugins/script_text_editor.cpp/h` | 文本编辑、语法高亮、错误显示 |
| `editor/plugins/script_create_dialog.cpp/h` | 脚本创建对话框 |
| `editor/debugger/script_editor_debugger.cpp/h` | 脚本运行时 Debugger |

## 脚本加载链

```text
脚本路径
  → ResourceLoader::load()
  → Script / GDScript
  → ScriptLanguage 编译或校验
  → ScriptInstance
  → Node::set_script()
```

## 调试关键词

```text
ScriptLanguage
GDScriptParser
GDScriptCompiler
reload_scripts
get_error_count
set_breakpoint
request_stack_dump
```

## 修改脚本系统时检查

- ResourceLoader 缓存与脚本重载；
- 解析器错误的文件、行号和列号；
- Node 挂载脚本后的 ScriptInstance 生命周期；
- 编辑器文本模型和磁盘文件的一致性；
- 脚本错误是否只在运行时出现，而不是编辑器加载阶段。
