# 文件系统与资源 API

## 文件读写

- `core/io/file_access.cpp/h`：文本/二进制文件，使用 `FileAccess::open()`、`get_as_text()`、`store_string()`。
- `core/io/dir_access.cpp/h`：目录遍历、创建目录和列举文件。
- `core/config/project_settings.cpp/h`：项目设置和 `globalize_path()`。

推荐路径链路：

```cpp
String absolute = ProjectSettings::get_singleton()->globalize_path(path);
Ref<FileAccess> file = FileAccess::open(absolute, FileAccess::READ);
```

## 资源 API

- `core/io/resource_loader.cpp/h`：`ResourceLoader::load(path)`。
- `core/io/resource_saver.cpp/h`：`ResourceSaver::save(resource, path)`。
- `scene/resources/curve.h/cpp`：Curve 控制点。
- `scene/resources/gradient.h/cpp`：Gradient offsets/colors。
- `scene/resources/packed_scene.h/cpp`：场景资源。

## 编辑器扫描

`editor/file_system/editor_file_system.cpp/h` 负责编辑器资源数据库。文件写入或 ResourceSaver 后通常需要：

```cpp
if (EditorFileSystem::get_singleton() != nullptr) {
    EditorFileSystem::get_singleton()->scan();
}
```

## 安全检查

- 用户路径必须是 `res://` 或受控 `user://`。
- 通过 `ProjectSettings::globalize_path()` 后再次确认位于项目目录/用户目录。
- 拒绝 `..`、绝对路径和非预期扩展名。
- 覆盖现有资源前需要显式 allow_overwrite 和确认。
- 事务系统应在首次写入前记录原始字节和 existed 状态。
