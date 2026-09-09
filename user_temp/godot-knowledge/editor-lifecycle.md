# 编辑器生命周期与退出清理

本文件面向引擎编辑器插件开发和 Debug。

## 关键文件

| 文件 | 负责内容 |
|---|---|
| `editor/editor_node.cpp/h` | 编辑器主对象和插件卸载时机 |
| `editor/plugins/editor_plugin.cpp/h` | 插件基类生命周期 |
| `editor/docks/editor_dock_manager.cpp/h` | Dock 注册、删除和重父级 |
| `scene/main/node.cpp/h` | Node 删除、自动分离和父子关系断言 |
| `scene/main/scene_tree.cpp/h` | 节点进入/退出 SceneTree |

## 插件生命周期模型

```text
EditorNode 创建插件
  → EditorPlugin 构造
  → add_child / add_dock
  → 正常编辑器运行
  → remove_editor_plugin
  → 移除 Dock
  → 停止服务/线程
  → 删除子节点
```

## 安全清理

删除子节点前不要假定它仍由原父节点持有：

```cpp
if (child != nullptr && child->get_parent() == this) {
    remove_child(child);
}
memdelete(child);
```

## 常见错误

```text
Condition "child->data.parent != this" is true.
```

通常表示：

- `remove_child()` 被重复调用；
- Godot 的 PREDELETE 已经自动分离节点；
- DockManager 已经重父级；
- 析构顺序与代码预期不同。

## 修改生命周期代码时检查

- 停止网络、线程、Timer 和异步回调；
- 取消 deferred call 或在回调中确认对象仍有效；
- 先从 DockManager 移除 Dock，再释放 Dock；
- `remove_child()` 前检查 `get_parent()`；
- `memdelete()` 前清理引用；
- 用正常编辑器退出和无残留进程验证。
