# 场景编辑源码定位

## 核心对象关系

```text
PackedScene (资源文件)
  → instantiate()
  → Node 根节点
  → get_node_or_null(NodePath)
  → 修改节点
  → PackedScene::pack(root)
  → ResourceSaver::save()
```

## 关键源码

| 文件 | 负责内容 |
|---|---|
| `scene/main/node.cpp/h` | Node 父子树、名称、Owner、路径和生命周期 |
| `scene/resources/packed_scene.cpp/h` | `.tscn` 实例化与打包保存 |
| `scene/3d/node_3d.cpp/h` | `position`、`rotation`、`scale`、`transform` |
| `scene/3d/mesh_instance_3d.cpp/h` | Mesh、material_override |
| `scene/resources/3d/primitive_meshes.cpp/h` | SphereMesh、BoxMesh、PlaneMesh |
| `scene/resources/material.cpp/h` | StandardMaterial3D |
| `editor/editor_interface.cpp/h` | 当前编辑场景、打开/刷新、运行控制 |
| `editor/file_system/editor_file_system.cpp/h` | 扫描和资源变更通知 |

## 修改安全规则

1. 不允许修改根节点本身的名称/删除。
2. `node_path` 必须在场景树中存在。
3. Reparent 前检查新父级不是目标节点的后代。
4. 临时实例化树中的 Owner 要正确设置，否则 `PackedScene::pack()` 可能丢失节点。
5. 修改完成后先 pack，再 save；保存失败时释放实例并返回错误。
6. 场景保存后调用 `EditorFileSystem::scan()`，必要时刷新编辑器当前场景。
7. 重新加载外部写入的当前场景后，同步编辑器的磁盘修改时间基线，否则外部变更检测可能再次弹出“场景已修改，重新加载”对话框。

## 外部修改检测

`editor/editor_node.cpp` 的 `_scan_external_changes()` 比较：

```text
FileAccess::get_modified_time(scene_path)
vs.
EditorData::get_scene_modified_time(index)
```

磁盘时间更晚时会显示外部修改对话框。`EditorNode::reload_scene()` 成功加载并恢复场景 tab 索引后应调用 `EditorData::set_scene_modified_time()`，把当前磁盘时间记录为新的基线。

## 常见 API

```cpp
Ref<PackedScene> packed = ResourceLoader::load(scene_path);
Node *root = packed->instantiate();
Node *node = root->get_node_or_null(NodePath(node_path));
node->set_name(new_name);
node->set("property", value);
Ref<PackedScene> updated;
updated.instantiate();
updated->pack(root);
ResourceSaver::save(updated, scene_path);
memdelete(root);
```

## 常见错误

- `Cannot get path of node as it is not in a scene tree`：实例化节点未进入 SceneTree，读取路径前检查 `is_inside_tree()`。
- `child->data.parent != this`：重复 `remove_child()` 或父节点已被 Godot 自动清理。
- pack 后节点消失：检查 Owner 是否指向场景根节点。
