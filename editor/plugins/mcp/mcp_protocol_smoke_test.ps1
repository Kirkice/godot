param(
    [string]$Endpoint = "http://127.0.0.1:30100/mcp",
    [Parameter(Mandatory = $true)][string]$Token
)

$ErrorActionPreference = "Stop"
function Invoke-Mcp([string]$Body) {
    try {
        return (Invoke-WebRequest -UseBasicParsing -Uri $Endpoint -Method Post -Headers @{ Authorization = "Bearer $Token" } -ContentType "application/json" -Body $Body -TimeoutSec 5).Content | ConvertFrom-Json
    } catch {
        $reader = New-Object System.IO.StreamReader($_.Exception.Response.GetResponseStream())
        return $reader.ReadToEnd() | ConvertFrom-Json
    }
}

$status = Invoke-Mcp '{"jsonrpc":"2.0","id":1,"method":"tools/call","params":{"name":"godot.editor.status","arguments":{}}}'
if ($status.result.structuredContent.state -ne "running") { throw "status did not report running" }
$main_scene = Invoke-Mcp '{"jsonrpc":"2.0","id":37,"method":"tools/call","params":{"name":"godot.project.set_main_scene","arguments":{"scene_path":"res://TestMCP.tscn"}}}'
if ($main_scene.error.code -ne $null -and $main_scene.error.code -ne -32020) { throw "main scene setting returned unexpected error" }
$camera_capture = Invoke-Mcp '{"jsonrpc":"2.0","id":38,"method":"tools/call","params":{"name":"godot.run.capture_camera_view","arguments":{"scene_path":"res://TestMCP.tscn","camera_path":"Camera","output_path":"user://mcp_camera_smoke.png"}}}'
if ($camera_capture.error.code -ne $null -and $camera_capture.error.code -ne -32020 -and $camera_capture.error.code -ne -32040) { throw "camera buffer capture returned unexpected error" }
$capture = Invoke-Mcp '{"jsonrpc":"2.0","id":33,"method":"tools/call","params":{"name":"godot.capture.editor_view","arguments":{"output_path":"user://mcp_smoke_capture.png"}}}'
if ($capture.error.code -ne $null -and $capture.error.code -ne -32040) { throw "editor capture returned unexpected error" }
if ($capture.result.captured -eq $true -and $capture.result.mime_type -ne "image/png") { throw "editor capture mime type is invalid" }
$add_3d = Invoke-Mcp '{"jsonrpc":"2.0","id":34,"method":"tools/call","params":{"name":"godot.scene.add_3d_node","arguments":{"scene_path":"res://mcp_real_scene.tscn","parent_path":".","node_type":"MeshInstance3D","node_name":"SmokeMesh"}}}'
if ($add_3d.error.code -ne $null -and $add_3d.error.code -ne -32020) { throw "3D node add returned unexpected error" }
$set_mesh = Invoke-Mcp '{"jsonrpc":"2.0","id":35,"method":"tools/call","params":{"name":"godot.scene.set_mesh","arguments":{"scene_path":"res://mcp_real_scene.tscn","node_path":"SmokeMesh","mesh_type":"BoxMesh"}}}'
if ($set_mesh.error.code -ne $null -and $set_mesh.error.code -ne -32020 -and $set_mesh.error.code -ne -32034) { throw "mesh assignment returned unexpected error" }
$set_transform = Invoke-Mcp '{"jsonrpc":"2.0","id":36,"method":"tools/call","params":{"name":"godot.scene.set_transform","arguments":{"scene_path":"res://mcp_real_scene.tscn","node_path":"SmokeMesh","position":[0,1,0],"rotation_degrees":[0,0,0],"scale":[1,1,1]}}}'
if ($set_transform.error.code -ne $null -and $set_transform.error.code -ne -32020 -and $set_transform.error.code -ne -32034) { throw "3D transform returned unexpected error" }

$invalid = Invoke-Mcp '{"jsonrpc":"2.0","id":2,"method":"tools/call","params":{"name":"godot.scene.create","arguments":{"scene_path":"C:/bad.tscn","root_type":"Node"}}}'
if ($invalid.error.code -ne -32602) { throw "invalid scene path was not rejected" }
$overwrite = Invoke-Mcp '{"jsonrpc":"2.0","id":25,"method":"tools/call","params":{"name":"godot.scene.create","arguments":{"scene_path":"res://mcp_real_scene.tscn","root_type":"Node"}}}'
if ($overwrite.error.code -ne -32036) { throw "existing scene overwrite was not rejected" }

$missing = Invoke-Mcp '{"jsonrpc":"2.0","id":3,"method":"tools/call","params":{"name":"godot.resource.create","arguments":{"resource_path":"res://bad.txt","resource_type":"Gradient"}}}'
if ($missing.error.code -ne -32602) { throw "invalid resource path was not rejected" }
$property_gradient = Invoke-Mcp '{"jsonrpc":"2.0","id":27,"method":"tools/call","params":{"name":"godot.resource.create","arguments":{"resource_path":"res://mcp_smoke_gradient.tres","resource_type":"Gradient","colors":[[1.0,0.0,0.0,1.0],[0.0,0.0,1.0,1.0]]}}}'
if ($property_gradient.error.code -ne $null -and $property_gradient.error.code -ne -32020 -and $property_gradient.error.code -ne -32036) { throw "gradient properties returned unexpected error" }

$unknown = Invoke-Mcp '{"jsonrpc":"2.0","id":4,"method":"tools/call","params":{"name":"godot.unknown","arguments":{}}}'
if ($unknown.error.code -ne -32601) { throw "unknown tool was not rejected" }

$tools = Invoke-Mcp '{"jsonrpc":"2.0","id":5,"method":"tools/list","params":{}}'
if ($tools.result.tools.Count -lt 5) { throw "tool registry is incomplete" }

$unauthorized = $null
try {
    $unauthorized = (Invoke-WebRequest -UseBasicParsing -Uri $Endpoint -Method Post -Headers @{ Authorization = "Bearer invalid-token" } -ContentType "application/json" -Body '{"jsonrpc":"2.0","id":6,"method":"tools/list","params":{}}' -TimeoutSec 5).StatusCode
} catch {
    $unauthorized = [int]$_.Exception.Response.StatusCode
}
if ($unauthorized -ne 401) { throw "invalid token was not rejected with HTTP 401" }
$bad_confirmation = Invoke-Mcp '{"jsonrpc":"2.0","id":28,"method":"godot.confirmation.approve","params":{"confirmationId":"invalid-confirmation"}}'
if ($bad_confirmation.error.code -ne -32021) { throw "invalid confirmation id was not rejected" }

$begin = Invoke-Mcp '{"jsonrpc":"2.0","id":7,"method":"godot.transaction.begin","params":{"label":"smoke"}}'
if ([string]::IsNullOrEmpty($begin.result.transactionId)) { throw "transaction did not begin" }
$commit = Invoke-Mcp '{"jsonrpc":"2.0","id":8,"method":"godot.transaction.commit","params":{}}'
if ($commit.result.outcome -ne "committed") { throw "transaction did not commit" }

$audit = Invoke-Mcp '{"jsonrpc":"2.0","id":9,"method":"godot.audit.export","params":{}}'
if ($audit.result.entries.Count -lt 1) { throw "audit export is empty" }
if (($audit.result.entries | ConvertTo-Json -Depth 8) -match "invalid-token") { throw "audit contains token material" }
$audit_clear = Invoke-Mcp '{"jsonrpc":"2.0","id":24,"method":"godot.audit.clear","params":{}}'
if ($audit_clear.result.cleared -ne $true) { throw "audit clear failed" }

$run = Invoke-Mcp '{"jsonrpc":"2.0","id":15,"method":"tools/call","params":{"name":"godot.run.current_scene","arguments":{}}}'
if ($run.error.code -ne $null -and $run.error.code -ne -32032) { throw "current scene run returned unexpected error" }
$stop = Invoke-Mcp '{"jsonrpc":"2.0","id":16,"method":"tools/call","params":{"name":"godot.run.stop","arguments":{}}}'
if ($stop.error.code -ne $null -and $stop.error.code -ne -32035) { throw "scene stop returned unexpected error" }

$node_add = Invoke-Mcp '{"jsonrpc":"2.0","id":17,"method":"tools/call","params":{"name":"godot.scene.add_node","arguments":{"scene_path":"res://mcp_real_scene.tscn","parent_path":".","node_type":"Node2D","node_name":"SmokeNode"}}}'
if ($node_add.error.code -ne $null -and $node_add.error.code -ne -32020) { throw "scene add node returned unexpected error" }
$node_rename = Invoke-Mcp '{"jsonrpc":"2.0","id":18,"method":"tools/call","params":{"name":"godot.scene.rename_node","arguments":{"scene_path":"res://mcp_real_scene.tscn","node_path":"SmokeNode","new_name":"SmokeNodeRenamed"}}}'
if ($node_rename.error.code -ne $null -and $node_rename.error.code -ne -32020 -and $node_rename.error.code -ne -32034) { throw "scene rename node returned unexpected error" }
$node_remove = Invoke-Mcp '{"jsonrpc":"2.0","id":19,"method":"tools/call","params":{"name":"godot.scene.remove_node","arguments":{"scene_path":"res://mcp_real_scene.tscn","node_path":"SmokeNodeRenamed"}}}'
if ($node_remove.error.code -ne $null -and $node_remove.error.code -ne -32020 -and $node_remove.error.code -ne -32034) { throw "scene remove node returned unexpected error" }
$node_reparent = Invoke-Mcp '{"jsonrpc":"2.0","id":26,"method":"tools/call","params":{"name":"godot.scene.reparent_node","arguments":{"scene_path":"res://mcp_real_scene.tscn","node_path":"SmokeNode","new_parent_path":"."}}}'
if ($node_reparent.error.code -ne $null -and $node_reparent.error.code -ne -32020 -and $node_reparent.error.code -ne -32034) { throw "scene reparent node returned unexpected error" }
$reparent_txn = Invoke-Mcp '{"jsonrpc":"2.0","id":29,"method":"godot.transaction.begin","params":{"label":"reparent-transaction-smoke"}}'
$reparent_edit = Invoke-Mcp '{"jsonrpc":"2.0","id":30,"method":"tools/call","params":{"name":"godot.scene.reparent_node","arguments":{"scene_path":"res://mcp_real_scene.tscn","node_path":"ParentA/ChildA","new_parent_path":"."}}}'
$reparent_status = Invoke-Mcp '{"jsonrpc":"2.0","id":31,"method":"tools/call","params":{"name":"godot.editor.status","arguments":{}}}'
if ($reparent_status.result.structuredContent.transaction_created_files -ne 1) { throw "reparent did not register transaction file" }
$reparent_rollback = Invoke-Mcp '{"jsonrpc":"2.0","id":32,"method":"godot.transaction.rollback","params":{}}'
if ($reparent_rollback.result.outcome -ne "rolled_back") { throw "reparent transaction did not roll back" }

$multi_begin = Invoke-Mcp '{"jsonrpc":"2.0","id":10,"method":"godot.transaction.begin","params":{"label":"multi-file-smoke"}}'
$multi_scene = Invoke-Mcp '{"jsonrpc":"2.0","id":11,"method":"tools/call","params":{"name":"godot.scene.create","arguments":{"scene_path":"res://mcp_smoke_scene.tscn","root_type":"Node","allow_overwrite":true}}}'
$multi_resource = Invoke-Mcp '{"jsonrpc":"2.0","id":12,"method":"tools/call","params":{"name":"godot.resource.create","arguments":{"resource_path":"res://mcp_smoke_resource.tres","resource_type":"Gradient","allow_overwrite":true}}}'
$status_after_create = Invoke-Mcp '{"jsonrpc":"2.0","id":13,"method":"tools/call","params":{"name":"godot.editor.status","arguments":{}}}'
if ($status_after_create.result.structuredContent.transaction_created_files -ne 2) { throw "transaction did not track two created files" }
$multi_rollback = Invoke-Mcp '{"jsonrpc":"2.0","id":14,"method":"godot.transaction.rollback","params":{}}'
if ($multi_rollback.result.outcome -ne "rolled_back") { throw "multi-file transaction did not roll back" }

$commit_begin = Invoke-Mcp '{"jsonrpc":"2.0","id":20,"method":"godot.transaction.begin","params":{"label":"commit-smoke"}}'
$commit_scene = Invoke-Mcp '{"jsonrpc":"2.0","id":21,"method":"tools/call","params":{"name":"godot.scene.create","arguments":{"scene_path":"res://mcp_commit_scene.tscn","root_type":"Node"}}}'
$commit_resource = Invoke-Mcp '{"jsonrpc":"2.0","id":22,"method":"tools/call","params":{"name":"godot.resource.create","arguments":{"resource_path":"res://mcp_commit_resource.tres","resource_type":"Gradient"}}}'
$commit_result = Invoke-Mcp '{"jsonrpc":"2.0","id":23,"method":"godot.transaction.commit","params":{}}'
if ($commit_result.result.outcome -ne "committed") { throw "transaction did not commit" }
if (!(Test-Path (Join-Path $PSScriptRoot "../../../user_temp/godot_learn/mcp_commit_scene.tscn"))) { throw "committed scene is missing" }
if (!(Test-Path (Join-Path $PSScriptRoot "../../../user_temp/godot_learn/mcp_commit_resource.tres"))) { throw "committed resource is missing" }

Write-Output "MCP protocol smoke test passed: status, registry, validation, auth, transaction, audit, multi-file rollback."
