---
name: godot-mcp-common
summary: 配置、连接并验证 Godot 内置 MCP Server 的通用 Agent Skill。
---

# Godot MCP Common

## 适用范围

当 Agent 需要连接本机 Godot 编辑器内置 MCP Server、检查服务状态、发现工具或判断连接问题时，使用本 Skill。

本 Skill 只负责 MCP 连接配置和通用诊断；3D 场景创建、灯光配置和 Camera Buffer 视觉迭代使用：

```text
godot-mcp-3d-scene.md
```

## 架构

Godot MCP Server 直接集成在 Godot 编辑器中，不需要 Node.js、Python adapter、独立 proxy 或外部文件服务：

```text
Agent Runtime / MCP Client
        │
        │ Streamable HTTP (loopback-only, no authentication)
        ▼
<用户确认的 MCP Endpoint>
        │
        ▼
Godot Editor MCPService
```

## 连接参数

本 Skill 不预设项目路径、端口或地址。开始连接前，Agent 必须先确认以下信息；如果用户没有提供，应主动询问：

```text
1. Godot 编辑器当前使用的 MCP Endpoint 是什么？
2. MCP Server 是否只监听本机地址？
3. Agent 使用哪个 MCP Client 配置入口？
5. 目标 Godot 项目路径是什么？
```

用户确认后，将实际值填入：

```text
名称：<用户指定的 MCP Server 名称>
Transport：Streamable HTTP
URL：<用户确认的 MCP Endpoint>
Header：无需认证 Header
```

默认安全要求是只允许 loopback 地址；如果用户提供非 loopback 地址，Agent 必须提示安全风险并要求用户明确确认，不得自行放宽限制。

不要改成局域网地址，不要要求 MCP Server 监听 `0.0.0.0`。当前 MCP 仅允许 loopback 绑定，并提示本机其他进程可以调用 MCP。

## 配置 Agent MCP Client

在 Agent 宿主支持自定义 MCP Server 的配置界面中，添加。名称和 Endpoint 使用用户确认的值，不添加认证 Header：

```text
Server name: <用户指定名称>
Transport: Streamable HTTP
Endpoint: <用户确认 Endpoint>
Headers: none
```

如果 Agent 使用 JSON 配置，常见格式如下：

```json
{
  "mcpServers": {
    "godot": {
      "url": "<USER_CONFIRMED_MCP_ENDPOINT>",
      "headers": {}
    }
  }
}
```

部分 Agent 使用以下等价结构：

```json
{
  "servers": {
    "godot": {
      "transport": "streamable-http",
      "url": "<USER_CONFIRMED_MCP_ENDPOINT>",
      "headers": {}
    }
  }
}
```

字段名由 Agent 宿主决定，但以下值必须使用用户确认的信息：

```text
Transport：Streamable HTTP
URL：<用户确认的 MCP Endpoint>
Headers：none
```

## 启动 Godot

连接前先确认 Godot 编辑器是否已经运行。如果没有运行，Agent 必须询问用户：

```text
请提供 Godot 编辑器可执行文件路径、Godot 项目路径，以及是否允许我启动编辑器。
```

不得假设源码目录、项目目录、可执行文件名或构建产物位置。启动命令必须使用用户确认的实际路径；如果用户只要求连接而没有授权启动，不得自动启动进程。

如果引擎源码或编辑器二进制刚刚修改过，才需要先重新构建；仅调整场景时不需要重新编译引擎或重启编辑器。

启动后调用 `tools/list`，并使用实际公开的项目状态工具（如果当前服务提供）确认 MCP 服务状态、Endpoint、绑定地址和端口；不要从 Skill 中推测工具名或连接值。

## 认证

本地 MCP 已移除 Token、Bearer Authentication、Token 配置、Token 生成、复制和重置功能。Agent 连接时不需要提供 Token 或 Authorization Header。

安全边界只保留 loopback 绑定：

```text
127.0.0.1 或 ::1
```

关闭认证后，本机其他进程也可能调用 MCP，因此不要将服务绑定到局域网或公网地址。

如果实际运行的 Godot 版本仍要求 Token，说明编辑器没有使用最新构建产物，应先重新编译并重启编辑器；不要向用户反复索要 Token。

## 连接验证顺序

Agent 连接后必须按以下顺序判断：

### 0. 连接前信息确认

在建立连接前，若下列任何信息未知，先询问用户，不要猜测：

```text
- MCP Endpoint；
- Godot 项目路径；
- Godot 编辑器是否已启动；
- 是否允许启动或重启编辑器；
- Agent 宿主的 MCP 配置格式。
```

### 1. 工具发现

先调用标准 MCP 方法：

```text
tools/list
```

确认响应是 JSON-RPC 成功响应，并以实际列表和 Schema 为准。新版公开工具至少应包含：

```text
godot.project.inspect
godot.scene.inspect
godot.scene.mutate
godot.resource.mutate
godot.run
godot.visual.capture
```

旧版的 `godot.scene.add_3d_node`、`godot.scene.set_mesh`、`godot.scene.set_material`、`godot.scene.set_transform` 不要求出现在列表中；它们是隐藏兼容 RPC。

### 2. 服务状态

调用公开项目状态工具：

```text
tools/call
name = godot.project.inspect
arguments = {}
```

如果当前服务仍提供旧的状态兼容 RPC，也可以用于诊断，但不要求它出现在公开工具列表中。

期望结果：

```json
{
  "state": "running",
  "bind_address": "<实际绑定地址>",
  "port": <实际端口>,
  "endpoint": "<实际 Endpoint>"
}
```

同时确认：

```text
state = running
bind_address = 用户确认的地址
port = 用户确认或 status 返回的端口
endpoint = status 返回的实际 Endpoint
```

如果返回的地址或端口与用户配置不一致，暂停任务并询问用户，不要自行修改 MCP Server 配置。

`transaction_active` 是否为 true 取决于当前任务，不是连接成功的必要条件。

### 3. 连接判断

- `200` 且返回 JSON-RPC 结果：连接正常；
- `403`：请求来源或绑定地址不符合本机限制；
- 连接拒绝：Godot 未启动、MCP 未启用、用户确认的 Endpoint 不可达，或服务端口已被其他进程占用。

MCP 不再提供 Token 认证，不要添加 Authorization Header，也不要向用户索要 Token。仍然必须保持 loopback-only，不得暴露端口。

## JSON-RPC 调用约定

HTTP 请求必须使用：

```text
POST <用户确认的 MCP Endpoint>
Content-Type: application/json
Headers: none
```

请求结构：

```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "method": "tools/call",
  "params": {
    "name": "godot.project.inspect",
    "arguments": {}
  }
}
```

每次请求使用唯一的数值或字符串 `id`，并检查 HTTP 状态码、JSON-RPC `error` 和 `result`，不能仅根据请求已发送判断成功。

## Agent 工作状态判断

连接成功不等于场景任务可以立即执行。开始场景任务前还要确认：

```text
godot.project.inspect 返回成功，且 tools/list 包含目标工具
tools/list 包含目标工具
Godot 编辑器项目已打开
```

对于新版 3D 场景任务，还应按以下顺序使用：

```text
godot.scene.inspect
godot.scene.mutate
godot.run (action=start)
godot.visual.capture (source=camera)
godot.run (action=stop)
```

`godot.visual.capture` 使用 `source=camera` 时是实际游戏 Camera3D Buffer 截图工具。若返回截图请求已提交状态，表示请求已提交，不代表 PNG 已经生成；Agent 必须等待目标 `user://*.png` 文件生成后再读取并进行视觉判断。

不要使用 `source=editor` 作为 3D 视觉验收的主要工具，因为它捕获的是编辑器界面。

## 常见错误判断

### 找不到工具

```text
-32601 Method not found
Tool is not available
```

处理：重新调用 `tools/list`，确认 MCP Server 二进制版本和工具注册是否为最新。如果旧版细粒度工具不在列表中，先使用新版聚合工具，不要误判为服务损坏。只有在修改 C++ 后新增了新能力时，才需要重新编译引擎并重启 Godot；仅修改场景不需要编译。

### `godot.scene.mutate` 返回 `Unsupported scene operation`

检查：

1. `action` 只写 operation 名，不写完整工具名；
2. 例如正确写 `add_3d_node`，错误写 `godot.scene.add_3d_node`；
3. 重新调用 `tools/list` 读取当前 Schema；
4. 外层提供 `scene_path`，每个 operation 提供自己的节点参数；
5. 当前新版支持：

```text
add_node
add_3d_node
remove_node
rename_node
reparent_node
set_transform
set_property
set_mesh
set_material
```

`set_material` 支持 `color`、`metallic` 和 `roughness`，其中 `metallic`/`roughness` 范围为 0.0 到 1.0。

示例：

```json
{
  "scene_path": "res://<scene>.tscn",
  "operations": [
    {
      "action": "add_3d_node",
      "parent_path": ".",
      "node_type": "MeshInstance3D",
      "node_name": "PBR_Sphere_01"
    }
  ]
}
```

`set_transform` 的 `position`、`rotation_degrees`、`scale` 都必须是三个数值的数组。多个 operation 可以放在同一个 `operations` 数组中，服务按顺序执行并返回 `operation_count` 和 `results`。

### 需要确认

```text
-32020 Tool confirmation is required
```

处理：读取错误中的 `confirmationId`，调用：

```text
godot.confirmation.approve
```

然后只重试原来的同一个写入工具一次。不要跳过确认，也不要重复提交多个写操作。

### 连接状态正常但场景工具失败

依次检查：

1. `scene_path` 是否为 `res://` 下的 `.tscn`；
2. 目标场景是否存在；
3. `node_path` 是否与当前 Scene Tree 一致；
4. 当前是否有未结束的事务；
5. 写入工具是否已保存场景；
6. 运行前是否设置了正确的主场景；
7. 截图前是否等待了渲染完成。

### 截图为灰色或编辑器界面

不要把截图当作场景失败。先确认：

```text
godot.run (action=start) 已成功
已等待几帧
使用的是 godot.visual.capture，source=camera
目标 Camera3D 路径正确
```

如果 camera 截图返回请求已提交状态，等待 PNG 生成后读取；不要立即读取旧截图文件。

### 服务重启或连接中断

若 Godot 重启或 MCPService 断开：

1. 重新建立 HTTP MCP 连接；
3. 再次调用 `tools/list`；
4. 再次调用 `godot.project.inspect`，或调用当前服务实际提供的状态兼容 RPC；
5. 不要假设旧事务、旧确认或旧连接仍然有效。

## 安全规则

- 只连接 `127.0.0.1` 或 `::1`；
- 不执行 shell、任意代码或任意文件操作；
- 不绕过确认策略；
- 不把 MCP 端口暴露到公网或局域网；
- 不将 MCP 当作通用远程控制通道；
- 只调用 Agent 任务所需的已注册工具。

## 连接成功报告

连接和验证完成后，向用户简短报告：

```text
Godot MCP 已连接
Endpoint: <status 返回的实际 Endpoint>
State: running
Tools discovered: <数量>
3D tools: available/unavailable
Camera Buffer capture: available/unavailable
Main scene: verified/not verified
```

