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

启动后调用 `tools/list` 和 `godot.editor.status`，以返回值确认 MCP 服务的实际 Endpoint、绑定地址和端口，不要从 Skill 中推测这些值。

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

确认响应是 JSON-RPC 成功响应，并且工具列表包含至少：

```text
godot.editor.status
godot.scene.set_transform
godot.run.current_scene
godot.run.capture_camera_view
```

如果需要 3D 场景能力，还应包含：

```text
godot.project.set_main_scene
godot.editor.open_scene
godot.scene.add_3d_node
godot.scene.set_mesh
godot.scene.set_material
godot.scene.set_property
godot.run.stop
```

### 2. 服务状态

调用：

```text
tools/call
name = godot.editor.status
arguments = {}
```

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
    "name": "godot.editor.status",
    "arguments": {}
  }
}
```

每次请求使用唯一的数值或字符串 `id`，并检查 HTTP 状态码、JSON-RPC `error` 和 `result`，不能仅根据请求已发送判断成功。

## Agent 工作状态判断

连接成功不等于场景任务可以立即执行。开始场景任务前还要确认：

```text
godot.editor.status.state = running
tools/list 包含目标工具
Godot 编辑器项目已打开
```

对于 3D 场景任务，还应按以下顺序使用：

```text
godot.editor.open_scene
godot.project.set_main_scene
godot.run.current_scene
godot.run.capture_camera_view
```

`godot.run.capture_camera_view` 是实际游戏 Camera3D Buffer 截图工具。它返回 `capture_requested: true` 时，表示截图请求已提交，不代表 PNG 已经生成；Agent 必须等待目标 `user://*.png` 文件生成后再读取并进行视觉判断。

不要使用 `godot.run.capture_view` 作为 3D 视觉验收的主要工具，因为它可能捕获编辑器或嵌入式运行容器外层画面。

## 常见错误判断

### 找不到工具

```text
-32601 Method not found
Tool is not available
```

处理：重新调用 `tools/list`，确认 MCP Server 二进制版本和工具注册是否为最新。如果工具是在修改 C++ 后新增的，需要重新编译引擎并重启 Godot；仅修改场景不需要编译。

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
godot.run.current_scene 已成功
已等待几帧
使用的是 godot.run.capture_camera_view
目标 Camera3D 路径正确
```

`capture_camera_view` 仍返回 `capture_requested` 时，等待 PNG 生成后读取；不要立即读取旧截图文件。

### 服务重启或连接中断

若 Godot 重启或 MCPService 断开：

1. 重新建立 HTTP MCP 连接；
3. 再次调用 `tools/list`；
4. 再次调用 `godot.editor.status`；
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

