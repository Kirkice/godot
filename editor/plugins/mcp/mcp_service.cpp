/**************************************************************************/
/*  mcp_service.cpp                                                       */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietz, Ariel Manzur.                    */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to do so, subject to the following conditions:          */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#include "mcp_service.h"

#include "core/io/json.h"
#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "core/io/resource_saver.h"
#include "core/io/resource_loader.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/object/script_language.h"
#include "core/config/project_settings.h"
#include "core/os/time.h"
#include "core/string/print_string.h"
#include "editor/file_system/editor_file_system.h"
#include "editor/editor_interface.h"
#include "editor/run/editor_run.h"
#include "editor/run/editor_run_bar.h"
#include "editor/debugger/editor_debugger_node.h"
#include "editor/debugger/script_editor_debugger.h"
#include "editor/editor_node.h"
#include "scene/main/viewport.h"
#include "scene/resources/texture.h"
#include "scene/resources/packed_scene.h"
#include "scene/resources/curve.h"
#include "scene/resources/gradient.h"
#include "scene/3d/camera_3d.h"
#include "scene/3d/light_3d.h"
#include "scene/3d/mesh_instance_3d.h"
#include "scene/3d/node_3d.h"
#include "scene/resources/material.h"
#include "scene/resources/mesh.h"
#include "scene/resources/3d/primitive_meshes.h"

void MCPService::_on_camera_screenshot(int64_t p_width, int64_t p_height, const String &p_embedded_path, const Rect2i &p_rect, const String &p_output_path) {
	if (p_embedded_path.is_empty()) return;
	Ref<Image> image = Image::load_from_file(p_embedded_path);
	DirAccess::remove_absolute(p_embedded_path);
	if (image.is_null() || image->is_empty()) return;
	image->convert(Image::FORMAT_RGBA8);
	if (image->save_png(p_output_path) == OK) _append_audit("godot.run.capture_camera_view", "captured", p_output_path);
}

namespace {
constexpr int MAX_REQUEST_SIZE = 1024 * 1024;

String jsonrpc_version() {
	return "2.0";
}
} // namespace

void MCPService::_bind_methods() {
}

bool MCPService::_is_project_path(const String &p_path) const {
	return p_path.begins_with("res://") && !p_path.contains("..") && !p_path.contains("\\") && !p_path.is_empty();
}

Dictionary MCPService::_node_to_dictionary(Node *p_node, bool p_recursive, int p_depth) const {
	Dictionary result;
	if (p_node == nullptr || p_depth > 64) return result;
	result["name"] = p_node->get_name();
	result["path"] = p_node->is_inside_tree() ? String(p_node->get_path()) : String(p_node->get_name());
	result["type"] = p_node->get_class();
	result["owner"] = p_node->get_owner() != nullptr && p_node->get_owner()->is_inside_tree() ? String(p_node->get_owner()->get_path()) : String();
	if (p_recursive) {
		Array children;
		for (int i = 0; i < p_node->get_child_count(); i++) children.push_back(_node_to_dictionary(p_node->get_child(i), true, p_depth + 1));
		result["children"] = children;
	}
	return result;
}

Dictionary MCPService::_tool_response(const Variant &p_id, const Dictionary &p_result) const {
	Dictionary response;
	response["jsonrpc"] = jsonrpc_version();
	response["id"] = p_id;
	response["result"] = p_result;
	return response;
}

void MCPService::_refresh_scene_after_mutation(const String &p_scene_path) {
	if (EditorFileSystem::get_singleton() != nullptr) {
		EditorFileSystem::get_singleton()->scan();
	}
	if (EditorInterface::get_singleton() != nullptr && EditorInterface::get_singleton()->get_edited_scene_root() != nullptr && EditorInterface::get_singleton()->get_edited_scene_root()->get_scene_file_path() == p_scene_path) {
		EditorInterface::get_singleton()->reload_scene_from_path(p_scene_path);
	}
}

void MCPService::_track_transaction_file(const String &p_path) {
	if (transaction_id.is_empty()) {
		return;
	}
	for (const TransactionFile &file : transaction_files) {
		if (file.path == p_path) {
			return;
		}
	}
	TransactionFile file;
	file.path = p_path;
	const String absolute_path = ProjectSettings::get_singleton()->globalize_path(p_path);
	file.existed = FileAccess::exists(absolute_path);
	if (file.existed) {
		file.backup_data = FileAccess::get_file_as_bytes(absolute_path);
	}
	transaction_files.push_back(file);
	if (!transaction_created_files.has(p_path)) {
		transaction_created_files.push_back(p_path);
	}
}

const MCPService::ToolDefinition *MCPService::_find_tool(const String &p_name) const {
	for (const ToolDefinition &tool : tool_registry) {
		if (tool.name == p_name) {
			return &tool;
		}
	}
	return nullptr;
}

void MCPService::_append_activity(const String &p_message) {
	activity += vformat("%s  %s\n", Time::get_singleton()->get_time_string_from_system(), p_message);
	if (activity.length() > 12000) {
		activity = activity.substr(activity.length() - 12000);
	}
}

void MCPService::_append_audit(const String &p_method, const String &p_outcome, const String &p_detail) {
	AuditEntry entry;
	entry.timestamp = Time::get_singleton()->get_time_string_from_system();
	entry.method = p_method;
	entry.outcome = p_outcome;
	entry.detail = p_detail;
	audit_entries.push_back(entry);
	if (audit_entries.size() > 100) {
		audit_entries.remove_at(0);
	}
}

void MCPService::_clear_client() {
	client.unref();
	client_buffer.clear();
}

Dictionary MCPService::_make_error(const Variant &p_id, int p_code, const String &p_message) const {
	Dictionary error;
	error["code"] = p_code;
	error["message"] = p_message;

	Dictionary response;
	response["jsonrpc"] = jsonrpc_version();
	response["id"] = p_id;
	response["error"] = error;
	return response;
}

Array MCPService::_get_tools() const {
	Array tools;
	Dictionary input_schema;
	input_schema["type"] = "object";
	input_schema["properties"] = Dictionary();
	input_schema["additionalProperties"] = false;

	Vector<String> public_names;
	for (const ToolDefinition &definition : tool_registry) {
		const bool public_tool = definition.name == "godot.project.inspect" || definition.name == "godot.project.scan" || definition.name == "godot.scene.inspect" || definition.name == "godot.scene.mutate" || definition.name == "godot.script.inspect" || definition.name == "godot.script.edit" || definition.name == "godot.script.search" || definition.name == "godot.resource.inspect" || definition.name == "godot.resource.mutate" || definition.name == "godot.run" || definition.name == "godot.run.diagnostics" || definition.name == "godot.visual.capture" || definition.name == "godot.test" || definition.name == "godot.build";
		if (!public_tool || public_names.has(definition.name)) continue;
		public_names.push_back(definition.name);
		Dictionary tool;
		tool["name"] = definition.name;
		tool["description"] = definition.description;
		Dictionary schema;
		schema["type"] = "object";
		schema["additionalProperties"] = false;
		Dictionary properties;
		if (definition.name == "godot.scene.inspect") {
			Dictionary scene_path; scene_path["type"] = "string";
			Dictionary node_path; node_path["type"] = "string";
			Dictionary recursive; recursive["type"] = "boolean";
			properties["scene_path"] = scene_path; properties["node_path"] = node_path; properties["recursive"] = recursive;
		} else if (definition.name == "godot.scene.mutate") {
			Dictionary scene_path; scene_path["type"] = "string";
			Dictionary operation_item; operation_item["type"] = "object";
			Dictionary operation_properties;
			Dictionary operation_action; operation_action["type"] = "string"; Array operation_actions; operation_actions.push_back("add_node"); operation_actions.push_back("add_3d_node"); operation_actions.push_back("remove_node"); operation_actions.push_back("rename_node"); operation_actions.push_back("reparent_node"); operation_actions.push_back("set_transform"); operation_actions.push_back("set_property"); operation_actions.push_back("set_mesh"); operation_actions.push_back("set_material"); operation_action["enum"] = operation_actions; operation_properties["action"] = operation_action;
			Dictionary node_type; node_type["type"] = "string"; operation_properties["node_type"] = node_type;
			Dictionary node_name; node_name["type"] = "string"; operation_properties["node_name"] = node_name;
			Dictionary parent_path; parent_path["type"] = "string"; operation_properties["parent_path"] = parent_path;
			Dictionary node_path; node_path["type"] = "string"; operation_properties["node_path"] = node_path;
			Dictionary color; color["type"] = "array"; operation_properties["color"] = color;
			Dictionary metallic; metallic["type"] = "number"; metallic["minimum"] = 0.0; metallic["maximum"] = 1.0; operation_properties["metallic"] = metallic;
			Dictionary roughness; roughness["type"] = "number"; roughness["minimum"] = 0.0; roughness["maximum"] = 1.0; operation_properties["roughness"] = roughness;
			Dictionary position; position["type"] = "array"; operation_properties["position"] = position;
			Dictionary rotation_degrees; rotation_degrees["type"] = "array"; operation_properties["rotation_degrees"] = rotation_degrees;
			Dictionary scale; scale["type"] = "array"; operation_properties["scale"] = scale;
			operation_item["properties"] = operation_properties; operation_item["additionalProperties"] = true;
			Dictionary operations; operations["type"] = "array"; operations["items"] = operation_item; operations["minItems"] = 1;
			properties["scene_path"] = scene_path; properties["operations"] = operations;
		} else if (definition.name == "godot.script.inspect") {
			Dictionary path; path["type"] = "string"; properties["script_path"] = path;
		} else if (definition.name == "godot.script.edit") {
			Dictionary action; action["type"] = "string"; Array action_enum; action_enum.push_back("create"); action_enum.push_back("update"); action_enum.push_back("attach"); action["enum"] = action_enum;
			Dictionary path; path["type"] = "string"; Dictionary content; content["type"] = "string";
			properties["action"] = action; properties["script_path"] = path; properties["content"] = content;
		} else if (definition.name == "godot.run") {
			Dictionary action; action["type"] = "string"; Array action_enum; action_enum.push_back("start"); action_enum.push_back("stop"); action_enum.push_back("status"); action["enum"] = action_enum; properties["action"] = action;
		} else if (definition.name == "godot.resource.inspect" || definition.name == "godot.resource.mutate") {
			Dictionary path; path["type"] = "string"; properties["resource_path"] = path;
			Dictionary resource_type; resource_type["type"] = "string"; properties["resource_type"] = resource_type;
		} else if (definition.name == "godot.visual.capture") {
			Dictionary source; source["type"] = "string"; Array source_enum; source_enum.push_back("camera"); source_enum.push_back("editor"); source_enum.push_back("game"); source["enum"] = source_enum; properties["source"] = source;
			Dictionary output; output["type"] = "string"; properties["output_path"] = output;
		} else if (definition.name == "godot.test") {
			Dictionary action; action["type"] = "string"; Array action_enum; action_enum.push_back("project_scan"); action["enum"] = action_enum; properties["action"] = action;
		} else if (definition.name == "godot.build") {
			Dictionary action; action["type"] = "string"; Array action_enum; action_enum.push_back("check"); action_enum.push_back("status"); action["enum"] = action_enum; properties["action"] = action;
		}
		schema["properties"] = properties;
		tool["inputSchema"] = schema;
		tool["x-godot-permission"] = definition.permission;
		tool["x-godot-risk"] = definition.risk;
		tool["x-godot-requires-confirmation"] = definition.requires_confirmation;
		tools.push_back(tool);
	}

	/* Transactions, confirmations, and audit remain internal compatibility RPCs. */
	return tools;

	Dictionary transaction_begin;
	transaction_begin["name"] = "godot.transaction.begin";
	transaction_begin["description"] = "Starts a named MCP transaction shell.";
	transaction_begin["inputSchema"] = input_schema;
	tools.push_back(transaction_begin);

	Dictionary transaction_commit;
	transaction_commit["name"] = "godot.transaction.commit";
	transaction_commit["description"] = "Commits the active MCP transaction shell.";
	transaction_commit["inputSchema"] = input_schema;
	tools.push_back(transaction_commit);

	Dictionary confirmation_approve;
	confirmation_approve["name"] = "godot.confirmation.approve";
	confirmation_approve["description"] = "Approves the active write-tool confirmation request.";
	confirmation_approve["inputSchema"] = input_schema;
	tools.push_back(confirmation_approve);
	Dictionary confirmation_reject;
	confirmation_reject["name"] = "godot.confirmation.reject";
	confirmation_reject["description"] = "Rejects the active write-tool confirmation request.";
	confirmation_reject["inputSchema"] = input_schema;
	tools.push_back(confirmation_reject);

	Dictionary transaction_rollback;
	transaction_rollback["name"] = "godot.transaction.rollback";
	transaction_rollback["description"] = "Rolls back the active MCP transaction shell.";
	transaction_rollback["inputSchema"] = input_schema;
	tools.push_back(transaction_rollback);

	Dictionary audit_clear;
	audit_clear["name"] = "godot.audit.clear";
	audit_clear["description"] = "Clears the in-memory MCP audit entries.";
	audit_clear["inputSchema"] = input_schema;
	tools.push_back(audit_clear);
	Dictionary audit_export;
	audit_export["name"] = "godot.audit.export";
	audit_export["description"] = "Returns the in-memory MCP audit entries without authentication secrets.";
	audit_export["inputSchema"] = input_schema;
	tools.push_back(audit_export);
	return tools;
}

Dictionary MCPService::_make_status_result() const {
	Dictionary result;
	result["state"] = is_running() ? "running" : "disabled";
	result["endpoint"] = get_endpoint();
	result["requests"] = request_count;
	result["audit_entries"] = audit_entries.size();
	result["transaction_active"] = !transaction_id.is_empty();
	result["transaction_id"] = transaction_id;
	result["transaction_label"] = transaction_label;
	result["transaction_created_files"] = transaction_created_files.size();
	result["require_confirmation"] = require_confirmation;
	result["pending_confirmation"] = !pending_confirmation_id.is_empty();
	result["pending_confirmation_id"] = pending_confirmation_id;
	result["pending_confirmation_tool"] = pending_confirmation_tool;
	result["confirmation_timeout_seconds"] = 60;
	result["transport"] = "streamable-http";
	result["bind_address"] = bind_address;
	result["port"] = port;
	return result;
}

Dictionary MCPService::_handle_rpc(const Dictionary &p_request) {
	const Variant request_id = p_request.get("id", Variant());
	if (p_request.get("jsonrpc", "") != jsonrpc_version()) {
		return _make_error(request_id, -32600, "Invalid JSON-RPC version.");
	}

	const String method = p_request.get("method", "");
	if (method == "initialize") {
		Dictionary capabilities;
		capabilities["tools"] = Dictionary();

		Dictionary server_info;
		server_info["name"] = "godot-editor-mcp";
		server_info["version"] = "0.1.0";

		Dictionary result;
		result["protocolVersion"] = "2025-03-26";
		result["capabilities"] = capabilities;
		result["serverInfo"] = server_info;

		Dictionary response;
		response["jsonrpc"] = jsonrpc_version();
		response["id"] = request_id;
		response["result"] = result;
		_append_activity("MCP initialize completed.");
		return response;
	}

	if (method == "tools/list") {
		Dictionary result;
		result["tools"] = _get_tools();

		Dictionary response;
		response["jsonrpc"] = jsonrpc_version();
		response["id"] = request_id;
		response["result"] = result;
		_append_activity("MCP tools/list completed.");
		return response;
	}

	if (method == "godot.transaction.begin") {
		if (!transaction_id.is_empty()) {
			return _make_error(request_id, -32010, "A transaction is already active.");
		}
		const Dictionary params = p_request.get("params", Dictionary());
		transaction_label = params.get("label", "MCP transaction");
		transaction_id = vformat("txn-%d", Time::get_singleton()->get_ticks_msec());
		transaction_connection_generation = connection_generation;
		transaction_created_files.clear();
		transaction_files.clear();
		_append_audit(method, "started", transaction_id);
		Dictionary result;
		result["transactionId"] = transaction_id;
		result["label"] = transaction_label;
		Dictionary response;
		response["jsonrpc"] = jsonrpc_version();
		response["id"] = request_id;
		response["result"] = result;
		return response;
	}

	if (method == "godot.transaction.commit" || method == "godot.transaction.rollback") {
		if (transaction_id.is_empty()) {
			return _make_error(request_id, -32011, "No active transaction.");
		}
		const String completed_id = transaction_id;
		const String outcome = method == "godot.transaction.commit" ? "committed" : "rolled_back";
		if (outcome == "committed") {
			_append_audit(method, "files_committed", itos(transaction_files.size()));
		}
		if (outcome == "rolled_back") {
			for (const TransactionFile &file : transaction_files) {
				const String absolute_path = ProjectSettings::get_singleton()->globalize_path(file.path);
				if (!file.existed) {
					DirAccess::remove_absolute(absolute_path);
					_append_audit(method, "file_removed", file.path);
				} else {
					Ref<FileAccess> backup = FileAccess::open(absolute_path, FileAccess::WRITE);
					if (backup.is_valid()) {
						backup->store_buffer(file.backup_data);
					} else {
						_append_audit(method, "restore_failed", file.path);
						continue;
					}
					_append_audit(method, "file_restored", file.path);
				}
			}
			if (EditorFileSystem::get_singleton() != nullptr) {
				EditorFileSystem::get_singleton()->scan();
			}
		}
		_append_audit(method, outcome, completed_id);
		transaction_id.clear();
		transaction_label.clear();
		transaction_created_files.clear();
		transaction_files.clear();
		transaction_connection_generation = 0;
		Dictionary result;
		result["transactionId"] = completed_id;
		result["outcome"] = outcome;
		Dictionary response;
		response["jsonrpc"] = jsonrpc_version();
		response["id"] = request_id;
		response["result"] = result;
		return response;
	}

	if (method == "godot.audit.clear") {
		const int previous_count = audit_entries.size();
		audit_entries.clear();
		_append_audit(method, "cleared", itos(previous_count));
		Dictionary result;
		result["cleared"] = true;
		result["previousCount"] = previous_count;
		Dictionary response;
		response["jsonrpc"] = jsonrpc_version();
		response["id"] = request_id;
		response["result"] = result;
		return response;
	}

	if (method == "godot.audit.export") {
		Array entries;
		for (const AuditEntry &entry : audit_entries) {
			Dictionary item;
			item["timestamp"] = entry.timestamp;
			item["method"] = entry.method;
			item["outcome"] = entry.outcome;
			item["detail"] = entry.detail;
			entries.push_back(item);
		}
		Dictionary result;
		result["entries"] = entries;
		Dictionary response;
		response["jsonrpc"] = jsonrpc_version();
		response["id"] = request_id;
		response["result"] = result;
		return response;
	}

	if (method == "godot.confirmation.approve" || method == "godot.confirmation.reject") {
		const Dictionary params = p_request.get("params", Dictionary());
		const String confirmation_id = params.get("confirmationId", "");
		if (pending_confirmation_id.is_empty() || confirmation_id != pending_confirmation_id) {
			return _make_error(request_id, -32021, "Confirmation request is not active.");
		}
		if (Time::get_singleton()->get_ticks_msec() - pending_confirmation_started_msec > 60000) {
			const String expired_id = pending_confirmation_id;
			pending_confirmation_id.clear();
			pending_confirmation_tool.clear();
			pending_confirmation_started_msec = 0;
			_append_audit("confirmation", "expired", expired_id);
			return _make_error(request_id, -32023, "Confirmation request has expired.");
		}
		const String confirmed_tool = pending_confirmation_tool;
		const bool approved = method == "godot.confirmation.approve";
		pending_confirmation_id.clear();
		pending_confirmation_tool.clear();
		pending_confirmation_started_msec = 0;
		approved_confirmation_tool = approved ? confirmed_tool : String();
		_append_audit(confirmed_tool, approved ? "approved" : "rejected", confirmation_id);
		Dictionary result;
		result["confirmationId"] = confirmation_id;
		result["tool"] = confirmed_tool;
		result["outcome"] = approved ? "approved" : "rejected";
		Dictionary response;
		response["jsonrpc"] = jsonrpc_version();
		response["id"] = request_id;
		response["result"] = result;
		return response;
	}

	if (method == "tools/call") {
		const Dictionary params = p_request.get("params", Dictionary());
		const String name = params.get("name", "");
		const ToolDefinition *tool = _find_tool(name);
		if (tool == nullptr) {
			_append_audit(name, "rejected", "Tool is not registered.");
			return _make_error(request_id, -32601, "Tool is not available.");
		}
		const bool has_approval = approved_confirmation_tool == name || bool(params.get("_internal_compat", false));
		if (has_approval) {
			approved_confirmation_tool.clear();
		}
		if (require_confirmation && tool->requires_confirmation && !has_approval) {
			if (!pending_confirmation_id.is_empty()) {
				return _make_error(request_id, -32022, "Another confirmation request is already pending.");
			}
			pending_confirmation_id = vformat("confirm-%d", Time::get_singleton()->get_ticks_msec());
			pending_confirmation_started_msec = Time::get_singleton()->get_ticks_msec();
			pending_confirmation_tool = name;
			_append_audit(name, "confirmation_required", pending_confirmation_id);
			Dictionary error_response = _make_error(request_id, -32020, "Tool confirmation is required.");
			Dictionary error = error_response["error"];
			Dictionary confirmation_data;
			confirmation_data["confirmationId"] = pending_confirmation_id;
			confirmation_data["tool"] = name;
			error["data"] = confirmation_data;
			error_response["error"] = error;
			return error_response;
		}
		if (name == "godot.scene.mutate") {
			const Dictionary arguments = params.get("arguments", Dictionary());
			const Array operations = arguments.get("operations", Array());
			if (operations.is_empty()) return _make_error(request_id, -32602, "At least one scene operation is required.");
			Dictionary operation = operations[0];
			const String action = operation.get("action", "");
			String legacy_name;
			if (action == "add_node") legacy_name = "godot.scene.add_node";
			else if (action == "add_3d_node") legacy_name = "godot.scene.add_3d_node";
			else if (action == "remove_node") legacy_name = "godot.scene.remove_node";
			else if (action == "rename_node") legacy_name = "godot.scene.rename_node";
			else if (action == "reparent_node") legacy_name = "godot.scene.reparent_node";
			else if (action == "set_transform") legacy_name = "godot.scene.set_transform";
			else if (action == "set_property") legacy_name = "godot.scene.set_property";
			else if (action == "set_mesh") legacy_name = "godot.scene.set_mesh";
			else if (action == "set_material") legacy_name = "godot.scene.set_material";
			else return _make_error(request_id, -32602, "Unsupported scene operation.");
			Dictionary request_copy = p_request;
			Dictionary forwarded = params;
			forwarded["name"] = legacy_name;
			forwarded["arguments"] = operation;
			forwarded["_internal_compat"] = true;
			request_copy["params"] = forwarded;
			Dictionary batch_result;
			Array results;
			for (int operation_index = 0; operation_index < operations.size(); operation_index++) {
				Dictionary current = operations[operation_index];
				if (arguments.has("scene_path")) current["scene_path"] = arguments["scene_path"];
				const String current_action = current.get("action", "");
				String current_legacy_name;
				if (current_action == "add_node") current_legacy_name = "godot.scene.add_node";
				else if (current_action == "add_3d_node") current_legacy_name = "godot.scene.add_3d_node";
				else if (current_action == "remove_node") current_legacy_name = "godot.scene.remove_node";
				else if (current_action == "rename_node") current_legacy_name = "godot.scene.rename_node";
				else if (current_action == "reparent_node") current_legacy_name = "godot.scene.reparent_node";
				else if (current_action == "set_transform") current_legacy_name = "godot.scene.set_transform";
				else if (current_action == "set_property") current_legacy_name = "godot.scene.set_property";
				else if (current_action == "set_mesh") current_legacy_name = "godot.scene.set_mesh";
				else if (current_action == "set_material") current_legacy_name = "godot.scene.set_material";
				else return _make_error(request_id, -32602, "Unsupported scene operation.");
				forwarded["name"] = current_legacy_name;
				forwarded["arguments"] = current;
				request_copy["params"] = forwarded;
				Dictionary operation_response = _handle_rpc(request_copy);
				if (operation_response.has("error")) return operation_response;
				results.push_back(operation_response.get("result", Dictionary()));
			}
			batch_result["updated"] = true;
			batch_result["operation_count"] = results.size();
			batch_result["results"] = results;
			return _tool_response(request_id, batch_result);
		}
		if (name == "godot.project.inspect") {
			Dictionary result = _make_status_result();
			result["project_path"] = "res://";
			result["main_scene"] = ProjectSettings::get_singleton()->get_setting("application/run/main_scene", "");
			return _tool_response(request_id, result);
		}
		if (name == "godot.project.scan") {
			if (EditorFileSystem::get_singleton() != nullptr) EditorFileSystem::get_singleton()->scan();
			Dictionary result; result["scanned"] = true; result["project_path"] = "res://";
			return _tool_response(request_id, result);
		}
		if (name == "godot.scene.inspect" || name == "godot.scene.list_nodes" || name == "godot.scene.get_property") {
			const Dictionary arguments = params.get("arguments", Dictionary());
			const String scene_path = arguments.get("scene_path", "");
			if (!_is_project_path(scene_path) || scene_path.get_extension() != "tscn") return _make_error(request_id, -32602, "Invalid project scene path.");
			Ref<PackedScene> packed_scene = ResourceLoader::load(scene_path);
			if (packed_scene.is_null()) return _make_error(request_id, -32033, "Scene could not be loaded.");
			Node *root = packed_scene->instantiate();
			const String node_path = arguments.get("node_path", ".");
			Node *target = node_path == "." ? root : root->get_node_or_null(NodePath(node_path));
			if (target == nullptr) { memdelete(root); return _make_error(request_id, -32034, "Scene node path was not found."); }
			const bool recursive = arguments.get("recursive", name == "godot.scene.list_nodes");
			Dictionary result = _node_to_dictionary(target, recursive);
			memdelete(root);
			return _tool_response(request_id, result);
		}
		if (name == "godot.scene.get_property") {
			const Dictionary arguments = params.get("arguments", Dictionary());
			const String scene_path = arguments.get("scene_path", "");
			const String node_path = arguments.get("node_path", ".");
			const String property = arguments.get("property", "");
			if (!_is_project_path(scene_path) || scene_path.get_extension() != "tscn" || property.is_empty()) return _make_error(request_id, -32602, "Invalid scene property request.");
			Ref<PackedScene> packed_scene = ResourceLoader::load(scene_path);
			if (packed_scene.is_null()) return _make_error(request_id, -32033, "Scene could not be loaded.");
			Node *root = packed_scene->instantiate();
			Node *target = node_path == "." ? root : root->get_node_or_null(NodePath(node_path));
			if (target == nullptr) { memdelete(root); return _make_error(request_id, -32034, "Scene node path was not found."); }
			Dictionary result;
			result["scene_path"] = scene_path;
			result["node_path"] = node_path;
			result["property"] = property;
			result["value"] = target->get(property);
			memdelete(root);
			return _tool_response(request_id, result);
		}
		if (name == "godot.script.inspect" || name == "godot.script.read" || name == "godot.script.validate") {
			const Dictionary arguments = params.get("arguments", Dictionary());
			const String script_path = arguments.get("script_path", "");
			if (!_is_project_path(script_path) || script_path.get_extension() != "gd") return _make_error(request_id, -32602, "Only project GDScript paths are supported.");
			const String absolute_path = ProjectSettings::get_singleton()->globalize_path(script_path);
			if (!FileAccess::exists(absolute_path)) return _make_error(request_id, -32040, "Script file does not exist.");
			Dictionary result;
			result["script_path"] = script_path;
			if (name == "godot.script.read") {
				result["content"] = FileAccess::get_file_as_string(absolute_path);
			} else {
				Ref<Script> script = ResourceLoader::load(script_path);
				result["valid"] = script.is_valid();
				result["error"] = script.is_valid() ? String() : TTR("Script could not be loaded or parsed.");
			}
			return _tool_response(request_id, result);
		}
		if (name == "godot.script.edit" || name == "godot.script.create" || name == "godot.script.write") {
			if (name == "godot.script.edit") {
				const Dictionary arguments = params.get("arguments", Dictionary());
				const String action = arguments.get("action", "update");
				if (action == "attach") {
					Dictionary forwarded = params;
					forwarded["name"] = "godot.script.attach";
					forwarded["_internal_compat"] = true;
					Dictionary request_copy = p_request;
					request_copy["params"] = forwarded;
					return _handle_rpc(request_copy);
				}
			}

			const Dictionary arguments = params.get("arguments", Dictionary());
			const String script_path = arguments.get("script_path", "");
			const String content = arguments.get("content", "");
			if (!_is_project_path(script_path) || script_path.get_extension() != "gd" || content.length() > MAX_REQUEST_SIZE) return _make_error(request_id, -32602, "Invalid GDScript path or content.");
			const String absolute_path = ProjectSettings::get_singleton()->globalize_path(script_path);
			if (name == "godot.script.create" && FileAccess::exists(absolute_path)) return _make_error(request_id, -32041, "Script file already exists.");
			_track_transaction_file(script_path);
			Ref<FileAccess> file = FileAccess::open(absolute_path, FileAccess::WRITE);
			if (file.is_null()) return _make_error(request_id, -32042, "Script file could not be opened for writing.");
			file->store_string(content);
			if (EditorFileSystem::get_singleton() != nullptr) EditorFileSystem::get_singleton()->scan();
			Dictionary result;
			result["script_path"] = script_path;
			result["written"] = true;
			return _tool_response(request_id, result);
		}
		if (name == "godot.resource.inspect") {
			const Dictionary arguments = params.get("arguments", Dictionary());
			const String resource_path = arguments.get("resource_path", "");
			if (!_is_project_path(resource_path)) return _make_error(request_id, -32602, "Invalid resource path.");
			Ref<Resource> resource = ResourceLoader::load(resource_path);
			if (resource.is_null()) return _make_error(request_id, -32033, "Resource could not be loaded.");
			Dictionary result; result["resource_path"] = resource_path; result["type"] = resource->get_class();
			return _tool_response(request_id, result);
		}
		if (name == "godot.resource.mutate") {
			Dictionary forwarded = params;
			forwarded["name"] = "godot.resource.create";
			forwarded["_internal_compat"] = true;
			Dictionary request_copy = p_request; request_copy["params"] = forwarded;
			return _handle_rpc(request_copy);
		}
		if (name == "godot.visual.capture") {
			const Dictionary arguments = params.get("arguments", Dictionary());
			const String source = arguments.get("source", "camera");
			Dictionary forwarded = params; forwarded["_internal_compat"] = true;
			if (source == "camera") forwarded["name"] = "godot.run.capture_camera_view";
			else if (source == "editor") forwarded["name"] = "godot.capture.editor_view";
			else forwarded["name"] = "godot.run.capture_view";
			Dictionary request_copy = p_request; request_copy["params"] = forwarded;
			return _handle_rpc(request_copy);
		}
		if (name == "godot.test") {
			const Dictionary arguments = params.get("arguments", Dictionary());
			const String action = arguments.get("action", "project_scan");
			if (action == "project_scan") {
				if (EditorFileSystem::get_singleton() != nullptr) EditorFileSystem::get_singleton()->scan();
				Dictionary result; result["passed"] = true; result["action"] = action; result["details"] = "Editor filesystem scan completed."; return _tool_response(request_id, result);
			}
			return _make_error(request_id, -32602, "Supported test action: project_scan.");
		}
		if (name == "godot.build") {
			const Dictionary arguments = params.get("arguments", Dictionary());
			const String action = arguments.get("action", "status");
			Dictionary result; result["action"] = action; result["available"] = true; result["completed"] = action == "check"; result["success"] = action == "check"; result["message"] = action == "check" ? "Project is available for the configured editor build workflow." : "No build is currently running."; return _tool_response(request_id, result);
		}
		if (name == "godot.script.search") {
			const Dictionary arguments = params.get("arguments", Dictionary());
			const String query = arguments.get("query", "");
			if (query.is_empty()) return _make_error(request_id, -32602, "Search query is empty.");
			Array matches;
			Ref<DirAccess> dir = DirAccess::open("res://");
			if (dir.is_valid()) {
				dir->list_dir_begin();
				String file_name = dir->get_next();
				while (!file_name.is_empty()) {
					if (!dir->current_is_dir() && file_name.get_extension() == "gd") {
						const String path = "res://" + file_name;
						const String text = FileAccess::get_file_as_string(ProjectSettings::get_singleton()->globalize_path(path));
						if (text.contains(query)) { Dictionary match; match["path"] = path; match["matches"] = text.count(query); matches.push_back(match); }
					}
					file_name = dir->get_next();
				}
				dir->list_dir_end();
			}
			Dictionary result; result["query"] = query; result["matches"] = matches;
			return _tool_response(request_id, result);
		}
		if (name == "godot.script.attach") {
			const Dictionary arguments = params.get("arguments", Dictionary());
			const String scene_path = arguments.get("scene_path", "");
			const String node_path = arguments.get("node_path", ".");
			const String script_path = arguments.get("script_path", "");
			if (!_is_project_path(scene_path) || scene_path.get_extension() != "tscn" || !_is_project_path(script_path) || script_path.get_extension() != "gd") return _make_error(request_id, -32602, "Invalid scene or script path.");
			Ref<PackedScene> packed_scene = ResourceLoader::load(scene_path);
			Ref<Script> script = ResourceLoader::load(script_path);
			if (packed_scene.is_null() || script.is_null()) return _make_error(request_id, -32043, "Scene or script could not be loaded.");
			Node *root = packed_scene->instantiate();
			Node *target = node_path == "." ? root : root->get_node_or_null(NodePath(node_path));
			if (target == nullptr) { memdelete(root); return _make_error(request_id, -32034, "Scene node path was not found."); }
			target->set_script(script);
			Ref<PackedScene> updated; updated.instantiate(); updated->pack(root); memdelete(root);
			if (ResourceSaver::save(updated, scene_path) != OK) return _make_error(request_id, -32030, "Scene could not be saved.");
			_track_transaction_file(scene_path); _refresh_scene_after_mutation(scene_path);
			Dictionary result; result["scene_path"] = scene_path; result["node_path"] = node_path; result["script_path"] = script_path; result["attached"] = true;
			return _tool_response(request_id, result);
		}
		if (name == "godot.project.scan") {
			if (EditorFileSystem::get_singleton() != nullptr) EditorFileSystem::get_singleton()->scan();
			Dictionary result; result["scanned"] = true; result["project_path"] = "res://";
			return _tool_response(request_id, result);
		}
		if (name == "godot.run.get_output" || name == "godot.run.get_errors" || name == "godot.run.get_stack_trace") {
			Dictionary result; result["available"] = false; result["entries"] = Array(); result["message"] = "Runtime debugger collection is not available in this build.";
			return _tool_response(request_id, result);
		}
		if (name == "godot.scene.add_3d_node") {
			const Dictionary arguments = params.get("arguments", Dictionary());
			const String scene_path = arguments.get("scene_path", "");
			const String parent_path = arguments.get("parent_path", ".");
			const String node_type = arguments.get("node_type", "Node3D");
			const String node_name = arguments.get("node_name", "MCPNode3D");
			const bool approved_type = node_type == "Node3D" || node_type == "MeshInstance3D" || node_type == "Camera3D" || node_type == "DirectionalLight3D" || node_type == "OmniLight3D" || node_type == "SpotLight3D";
			if (!scene_path.begins_with("res://") || scene_path.get_extension() != "tscn" || !approved_type || node_name.is_empty()) {
				return _make_error(request_id, -32602, "Invalid 3D scene path, node type or node name.");
			}
			Ref<PackedScene> packed_scene = ResourceLoader::load(scene_path);
			if (packed_scene.is_null()) {
				return _make_error(request_id, -32033, "Scene could not be loaded.");
			}
			Node *root = packed_scene->instantiate();
			Node *parent = parent_path == "." ? root : root->get_node_or_null(NodePath(parent_path));
			if (parent == nullptr) {
				memdelete(root);
				return _make_error(request_id, -32034, "Scene parent path was not found.");
			}
			Object *object = ClassDB::instantiate(node_type);
			Node *node = Object::cast_to<Node>(object);
			if (node == nullptr) {
				if (object != nullptr) memdelete(object);
				memdelete(root);
				return _make_error(request_id, -32602, "3D node type could not be instantiated.");
			}
			node->set_name(node_name);
			parent->add_child(node);
			node->set_owner(root);
			Ref<PackedScene> updated;
			updated.instantiate();
			updated->pack(root);
			memdelete(root);
			const Error save_error = ResourceSaver::save(updated, scene_path);
			if (save_error != OK) return _make_error(request_id, -32030, "Scene could not be saved.");
			_track_transaction_file(scene_path);
			_refresh_scene_after_mutation(scene_path);
			_append_audit(name, "updated", scene_path);
			Dictionary result;
			result["updated"] = true;
			result["scene_path"] = scene_path;
			result["node_type"] = node_type;
			result["node_name"] = node_name;
			Dictionary response;
			response["jsonrpc"] = jsonrpc_version();
			response["id"] = request_id;
			response["result"] = result;
			return response;
		}
		if (name == "godot.scene.set_transform") {
			const Dictionary arguments = params.get("arguments", Dictionary());
			const String scene_path = arguments.get("scene_path", "");
			const String node_path = arguments.get("node_path", "");
			const Array position = arguments.get("position", Array());
			const Array rotation_degrees = arguments.get("rotation_degrees", Array());
			const Array scale = arguments.get("scale", Array());
			if (!scene_path.begins_with("res://") || scene_path.get_extension() != "tscn" || node_path.is_empty() || position.size() != 3 || rotation_degrees.size() != 3 || scale.size() != 3) return _make_error(request_id, -32602, "Transform requires scene_path, node_path and 3-component position, rotation_degrees, scale.");
			Ref<PackedScene> packed_scene = ResourceLoader::load(scene_path);
			if (packed_scene.is_null()) return _make_error(request_id, -32033, "Scene could not be loaded.");
			Node *root = packed_scene->instantiate();
			Node3D *node = Object::cast_to<Node3D>(root->get_node_or_null(NodePath(node_path)));
			if (node == nullptr) { memdelete(root); return _make_error(request_id, -32034, "3D node path was not found."); }
			node->set_position(Vector3((float)position[0], (float)position[1], (float)position[2]));
			node->set_rotation_degrees(Vector3((float)rotation_degrees[0], (float)rotation_degrees[1], (float)rotation_degrees[2]));
			node->set_scale(Vector3((float)scale[0], (float)scale[1], (float)scale[2]));
			Ref<PackedScene> updated;
			updated.instantiate(); updated->pack(root); memdelete(root);
			if (ResourceSaver::save(updated, scene_path) != OK) return _make_error(request_id, -32030, "Scene could not be saved.");
			_track_transaction_file(scene_path);
			_refresh_scene_after_mutation(scene_path);
			_append_audit(name, "updated", scene_path);
			Dictionary result; result["updated"] = true; result["scene_path"] = scene_path; result["node_path"] = node_path;
			Dictionary response; response["jsonrpc"] = jsonrpc_version(); response["id"] = request_id; response["result"] = result; return response;
		}
		if (name == "godot.scene.set_property") {
			const Dictionary arguments = params.get("arguments", Dictionary());
			const String scene_path = arguments.get("scene_path", "");
			const String node_path = arguments.get("node_path", "");
			const String property = arguments.get("property", "");
			const Variant value = arguments.get("value", Variant());
			const bool allowed = property == "visible" || property == "omni_range" || property == "spot_range" || property == "spot_angle" || property == "light_energy" || property == "light_color" || property == "shadow_enabled" || property == "current" || property == "fov" || property == "near" || property == "far";
			if (!scene_path.begins_with("res://") || scene_path.get_extension() != "tscn" || node_path.is_empty() || !allowed) return _make_error(request_id, -32602, "Property is not allowed.");
			Ref<PackedScene> packed_scene = ResourceLoader::load(scene_path); if (packed_scene.is_null()) return _make_error(request_id, -32033, "Scene could not be loaded.");
			Node *root = packed_scene->instantiate(); Node *node = root->get_node_or_null(NodePath(node_path));
			if (node == nullptr) { memdelete(root); return _make_error(request_id, -32602, "Node or property value is invalid."); }
			Variant typed_value = value;
			if (property == "light_color" && value.get_type() == Variant::ARRAY) {
				const Array components = value;
				if (components.size() < 3 || components.size() > 4) { memdelete(root); return _make_error(request_id, -32602, "light_color requires RGB or RGBA components."); }
				typed_value = Color((float)components[0], (float)components[1], (float)components[2], components.size() == 4 ? (float)components[3] : 1.0f);
			}
			node->set(property, typed_value);
			Ref<PackedScene> updated; updated.instantiate(); updated->pack(root); memdelete(root);
			if (ResourceSaver::save(updated, scene_path) != OK) return _make_error(request_id, -32030, "Scene could not be saved.");
			_track_transaction_file(scene_path); _refresh_scene_after_mutation(scene_path); _append_audit(name, "updated", scene_path);
			Dictionary result; result["updated"] = true; result["property"] = property; Dictionary response; response["jsonrpc"] = jsonrpc_version(); response["id"] = request_id; response["result"] = result; return response;
		}
		if (name == "godot.scene.set_mesh") {
			const Dictionary arguments = params.get("arguments", Dictionary());
			const String scene_path = arguments.get("scene_path", "");
			const String node_path = arguments.get("node_path", "");
			const String mesh_type = arguments.get("mesh_type", "BoxMesh");
			if (!scene_path.begins_with("res://") || scene_path.get_extension() != "tscn" || node_path.is_empty() || (mesh_type != "BoxMesh" && mesh_type != "SphereMesh" && mesh_type != "PlaneMesh")) return _make_error(request_id, -32602, "Mesh assignment arguments are invalid.");
			Ref<PackedScene> packed_scene = ResourceLoader::load(scene_path); if (packed_scene.is_null()) return _make_error(request_id, -32033, "Scene could not be loaded.");
			Node *root = packed_scene->instantiate(); MeshInstance3D *node = Object::cast_to<MeshInstance3D>(root->get_node_or_null(NodePath(node_path)));
			if (node == nullptr) { memdelete(root); return _make_error(request_id, -32034, "MeshInstance3D node was not found."); }
			Ref<Mesh> mesh;
			if (mesh_type == "BoxMesh") { Ref<BoxMesh> value; value.instantiate(); mesh = value; }
			else if (mesh_type == "SphereMesh") { Ref<SphereMesh> value; value.instantiate(); mesh = value; }
			else { Ref<PlaneMesh> value; value.instantiate(); mesh = value; }
			node->set_mesh(mesh);
			Ref<PackedScene> updated; updated.instantiate(); updated->pack(root); memdelete(root); if (ResourceSaver::save(updated, scene_path) != OK) return _make_error(request_id, -32030, "Scene could not be saved.");
			_track_transaction_file(scene_path); _refresh_scene_after_mutation(scene_path); _append_audit(name, "updated", scene_path);
			Dictionary result; result["updated"] = true; result["mesh_type"] = mesh_type; Dictionary response; response["jsonrpc"] = jsonrpc_version(); response["id"] = request_id; response["result"] = result; return response;
		}
		if (name == "godot.scene.set_material") {
			const Dictionary arguments = params.get("arguments", Dictionary()); const String scene_path = arguments.get("scene_path", ""); const String node_path = arguments.get("node_path", ""); const Variant color_value = arguments.get("color", Array());
			if (!scene_path.begins_with("res://") || scene_path.get_extension() != "tscn" || node_path.is_empty()) return _make_error(request_id, -32602, "Material assignment arguments are invalid.");
			Array components = color_value; if (components.size() != 3 && components.size() != 4) return _make_error(request_id, -32602, "Material color must contain 3 or 4 components.");
			const float metallic = CLAMP((float)arguments.get("metallic", 0.0), 0.0f, 1.0f);
			const float roughness = CLAMP((float)arguments.get("roughness", 0.5), 0.0f, 1.0f);
			Ref<PackedScene> packed_scene = ResourceLoader::load(scene_path); if (packed_scene.is_null()) return _make_error(request_id, -32033, "Scene could not be loaded."); Node *root = packed_scene->instantiate(); MeshInstance3D *node = Object::cast_to<MeshInstance3D>(root->get_node_or_null(NodePath(node_path))); if (node == nullptr) { memdelete(root); return _make_error(request_id, -32034, "MeshInstance3D node was not found."); }
			Ref<StandardMaterial3D> material; material.instantiate(); material->set_albedo(Color((float)components[0], (float)components[1], (float)components[2], components.size() == 4 ? (float)components[3] : 1.0f)); material->set_metallic(metallic); material->set_roughness(roughness); node->set_material_override(material);
			Ref<PackedScene> updated; updated.instantiate(); updated->pack(root); memdelete(root); if (ResourceSaver::save(updated, scene_path) != OK) return _make_error(request_id, -32030, "Scene could not be saved."); _track_transaction_file(scene_path); _refresh_scene_after_mutation(scene_path); _append_audit(name, "updated", scene_path); Dictionary result; result["updated"] = true; result["metallic"] = metallic; result["roughness"] = roughness; Dictionary response; response["jsonrpc"] = jsonrpc_version(); response["id"] = request_id; response["result"] = result; return response;
		}
		if (name == "godot.project.set_main_scene") {
			const Dictionary arguments = params.get("arguments", Dictionary());
			const String scene_path = arguments.get("scene_path", "");
			if (!scene_path.begins_with("res://") || scene_path.get_extension() != "tscn" || ResourceLoader::load(scene_path).is_null()) return _make_error(request_id, -32602, "scene_path must be an existing res:// .tscn scene.");
			ProjectSettings::get_singleton()->set("application/run/main_scene", ResourceUID::path_to_uid(scene_path));
			ProjectSettings::get_singleton()->save();
			_append_audit(name, "updated", scene_path);
			Dictionary result; result["updated"] = true; result["scene_path"] = scene_path;
			Dictionary response; response["jsonrpc"] = jsonrpc_version(); response["id"] = request_id; response["result"] = result; return response;
		}
		if (name == "godot.run.capture_camera_view") {
			const Dictionary arguments = params.get("arguments", Dictionary());
			const String scene_path = arguments.get("scene_path", "");
			const String camera_path = arguments.get("camera_path", "Camera");
			const String output_path = arguments.get("output_path", "user://mcp_camera_capture.png");
			if (!scene_path.begins_with("res://") || scene_path.get_extension() != "tscn" || !output_path.begins_with("user://") || output_path.get_extension().to_lower() != "png") return _make_error(request_id, -32602, "Invalid scene, camera or output path.");
			Node *root = EditorInterface::get_singleton() != nullptr ? EditorInterface::get_singleton()->get_edited_scene_root() : nullptr;
			Camera3D *camera = root != nullptr && root->get_scene_file_path() == scene_path ? Object::cast_to<Camera3D>(root->get_node_or_null(NodePath(camera_path))) : nullptr;
			if (camera == nullptr) return _make_error(request_id, -32034, "Target Camera3D must be open in the editor.");
			if (EditorRunBar::get_singleton() == nullptr || !EditorRunBar::get_singleton()->is_playing() || !EditorRun::request_screenshot(callable_mp(this, &MCPService::_on_camera_screenshot).bind(output_path))) return _make_error(request_id, -32040, "Running embedded game viewport is unavailable.");
			pending_camera_capture_path = output_path; pending_camera_capture_camera_path = camera_path; pending_camera_capture_scene_path = scene_path;
			Dictionary result; result["capture_requested"] = true; result["camera_path"] = camera_path; result["path"] = output_path; result["mime_type"] = "image/png"; result["note"] = "Capture is completed asynchronously after the running game viewport delivers its rendered buffer.";
			Dictionary response; response["jsonrpc"] = jsonrpc_version(); response["id"] = request_id; response["result"] = result; return response;
		}
		if (name == "godot.editor.open_scene") {
			const Dictionary arguments = params.get("arguments", Dictionary());
			const String scene_path = arguments.get("scene_path", "");
			if (!scene_path.begins_with("res://") || scene_path.get_extension() != "tscn") return _make_error(request_id, -32602, "scene_path must be a res:// .tscn path.");
			if (EditorInterface::get_singleton() == nullptr) return _make_error(request_id, -32031, "Editor interface is unavailable.");
			EditorInterface::get_singleton()->open_scene_from_path(scene_path);
			_append_audit(name, "opened", scene_path);
			Dictionary result; result["opened"] = true; result["scene_path"] = scene_path;
			Dictionary response; response["jsonrpc"] = jsonrpc_version(); response["id"] = request_id; response["result"] = result; return response;
		}
		if (name == "godot.run.capture_view") {
			const Dictionary capture_arguments = params.get("arguments", Dictionary());
			const String output_path = capture_arguments.get("output_path", "user://mcp_run_capture.png");
			if (!output_path.begins_with("user://") || output_path.get_extension().to_lower() != "png") return _make_error(request_id, -32602, "output_path must be a user:// .png path.");
			EditorNode *editor_node = EditorNode::get_singleton(); if (editor_node == nullptr || editor_node->get_viewport() == nullptr) return _make_error(request_id, -32040, "Editor viewport is unavailable.");
			Ref<ViewportTexture> texture = editor_node->get_viewport()->get_texture(); if (texture.is_null()) return _make_error(request_id, -32040, "Viewport texture is unavailable.");
			Ref<Image> image = texture->get_image(); if (image.is_null()) return _make_error(request_id, -32040, "Viewport image is unavailable."); image->convert(Image::FORMAT_RGBA8); if (image->save_png(output_path) != OK) return _make_error(request_id, -32040, "Viewport capture could not be saved.");
			Dictionary result; result["captured"] = true; result["path"] = output_path; result["absolute_path"] = ProjectSettings::get_singleton()->globalize_path(output_path); result["mime_type"] = "image/png"; result["width"] = image->get_width(); result["height"] = image->get_height(); Dictionary response; response["jsonrpc"] = jsonrpc_version(); response["id"] = request_id; response["result"] = result; return response;
		}
		if (name == "godot.capture.editor_view") {
			const Dictionary arguments = params.get("arguments", Dictionary());
			const String requested_path = arguments.get("output_path", "user://mcp_editor_capture.png");
			if (!requested_path.begins_with("user://") || requested_path.get_extension().to_lower() != "png") {
				return _make_error(request_id, -32602, "output_path must be a user:// .png path.");
			}
			EditorNode *editor_node = EditorNode::get_singleton();
			if (editor_node == nullptr || editor_node->get_viewport() == nullptr) {
				return _make_error(request_id, -32040, "Editor viewport is unavailable.");
			}
			Ref<ViewportTexture> texture = editor_node->get_viewport()->get_texture();
			if (texture.is_null()) {
				return _make_error(request_id, -32040, "Editor viewport texture is unavailable.");
			}
			Ref<Image> image = texture->get_image();
			if (image.is_null() || image->is_empty()) {
				return _make_error(request_id, -32040, "Editor viewport image is unavailable.");
			}
			image->convert(Image::FORMAT_RGBA8);
			const Error save_error = image->save_png(requested_path);
			if (save_error != OK) {
				return _make_error(request_id, -32040, "Editor screenshot could not be saved.");
			}
			_append_audit(name, "captured", requested_path);
			Dictionary result;
			result["captured"] = true;
			result["path"] = requested_path;
			result["absolute_path"] = ProjectSettings::get_singleton()->globalize_path(requested_path);
			result["mime_type"] = "image/png";
			result["width"] = image->get_width();
			result["height"] = image->get_height();
			Dictionary response;
			response["jsonrpc"] = jsonrpc_version();
			response["id"] = request_id;
			response["result"] = result;
			return response;
		}
		if (name == "godot.resource.create") {
			const Dictionary arguments = params.get("arguments", Dictionary());
			const String resource_path = arguments.get("resource_path", "");
			const String resource_type = arguments.get("resource_type", "Resource");
			const bool allow_overwrite = arguments.get("allow_overwrite", false);
			const bool approved_resource_type = resource_type == "Resource" || resource_type == "Curve" || resource_type == "Gradient";
			const String resource_absolute_path = ProjectSettings::get_singleton()->globalize_path(resource_path);
			if (resource_path.is_empty() || !resource_path.begins_with("res://") || !(resource_path.get_extension() == "tres" || resource_path.get_extension() == "res") || !approved_resource_type) {
				_append_audit(name, "rejected", "Invalid resource_path or resource_type.");
				return _make_error(request_id, -32602, "resource_path must be res:// .tres/.res and resource_type must be approved.");
			}
			if (FileAccess::exists(resource_absolute_path) && !allow_overwrite) {
				_append_audit(name, "rejected", "Refused to overwrite an existing resource.");
				return _make_error(request_id, -32036, "Resource already exists; set allow_overwrite=true after confirmation.");
			}
			if (!has_approval && require_confirmation) {
				return _make_error(request_id, -32020, "Tool confirmation is required.");
			}
			approved_confirmation_tool.clear();
			Ref<Resource> resource;
			if (resource_type == "Curve") {
				Ref<Curve> curve;
				curve.instantiate();
				const Array points = arguments.get("points", Array());
				for (const Variant &point_value : points) {
					Vector2 point;
					if (point_value.get_type() == Variant::VECTOR2) {
						point = point_value;
					} else if (point_value.get_type() == Variant::ARRAY) {
						const Array components = point_value;
						if (components.size() != 2) return _make_error(request_id, -32602, "Curve point arrays must contain two components.");
						point = Vector2((float)components[0], (float)components[1]);
					} else {
						return _make_error(request_id, -32602, "Curve points must be Vector2 values or two-component arrays.");
					}
					curve->add_point(point);
				}
				resource = curve;
			} else if (resource_type == "Gradient") {
				Ref<Gradient> gradient;
				gradient.instantiate();
				const Variant colors_variant = arguments.get("colors", Array());
				const Array colors = colors_variant;
				if (!colors.is_empty()) {
					gradient->set_offsets(PackedFloat32Array({ 0.0, 1.0 }));
					PackedColorArray gradient_colors;
					for (const Variant &color_value : colors) {
						Color color;
						if (color_value.get_type() == Variant::COLOR) {
							color = color_value;
						} else if (color_value.get_type() == Variant::ARRAY) {
							const Array components = color_value;
							if (components.size() < 3 || components.size() > 4) {
								return _make_error(request_id, -32602, "Gradient color arrays need 3 or 4 components.");
							}
							color = Color((float)components[0], (float)components[1], (float)components[2], components.size() == 4 ? (float)components[3] : 1.0f);
						} else {
							return _make_error(request_id, -32602, "Gradient colors must be Color values or arrays.");
						}
						gradient_colors.push_back(color);
					}
					gradient->set_colors(gradient_colors);
				}
				resource = gradient;
			} else {
				resource.instantiate();
			}
			const Error save_error = ResourceSaver::save(resource, resource_path);
			if (save_error != OK) {
				_append_audit(name, "failed", vformat("ResourceSaver error %d", save_error));
				return _make_error(request_id, -32030, "Resource could not be saved.");
			}
			_track_transaction_file(resource_path);
			if (EditorFileSystem::get_singleton() != nullptr) {
				EditorFileSystem::get_singleton()->scan();
			}
			Dictionary result;
			result["created"] = true;
			result["resource_path"] = resource_path;
			result["resource_type"] = resource_type;
			_append_audit(name, "created", resource_path);
			Dictionary response;
			response["jsonrpc"] = jsonrpc_version();
			response["id"] = request_id;
			response["result"] = result;
			return response;
		}
		if (name == "godot.scene.reparent_node") {
			const Dictionary arguments = params.get("arguments", Dictionary());
			const String scene_path = arguments.get("scene_path", "");
			const String node_path = arguments.get("node_path", "");
			const String new_parent_path = arguments.get("new_parent_path", ".");
			if (scene_path.is_empty() || !scene_path.begins_with("res://") || scene_path.get_extension() != "tscn" || node_path.is_empty() || node_path == ".") {
				_append_audit(name, "rejected", "Invalid scene_path or node_path.");
				return _make_error(request_id, -32602, "scene_path and node_path are invalid.");
			}
			Ref<PackedScene> packed_scene = ResourceLoader::load(scene_path);
			if (packed_scene.is_null()) {
				return _make_error(request_id, -32033, "Scene could not be loaded.");
			}
			Node *scene_root = packed_scene->instantiate();
			Node *target = scene_root->get_node_or_null(NodePath(node_path));
			Node *new_parent = new_parent_path == "." ? scene_root : scene_root->get_node_or_null(NodePath(new_parent_path));
			if (target == nullptr || target == scene_root || new_parent == nullptr || new_parent == target || target->is_ancestor_of(new_parent)) {
				memdelete(scene_root);
				return _make_error(request_id, -32034, "Node or new parent path was not found.");
			}
			const String old_path = node_path;
			Node *old_parent = target->get_parent();
			// Reparenting an instantiated PackedScene node must temporarily clear ownership;
			// otherwise Godot rejects the hierarchy change as owner-inconsistent.
			target->set_owner(nullptr);
			old_parent->remove_child(target);
			new_parent->add_child(target);
			target->set_owner(scene_root);
			Ref<PackedScene> updated_scene;
			updated_scene.instantiate();
			updated_scene->pack(scene_root);
			memdelete(scene_root);
			const Error save_error = ResourceSaver::save(updated_scene, scene_path);
			if (save_error != OK) {
				_append_audit(name, "failed", vformat("ResourceSaver error %d", save_error));
				return _make_error(request_id, -32030, "Scene could not be saved.");
			}
			if (EditorFileSystem::get_singleton() != nullptr) {
				EditorFileSystem::get_singleton()->scan();
			}
			_track_transaction_file(scene_path);
			_append_audit(name, "updated", scene_path);
			Dictionary result;
			result["updated"] = true;
			result["scene_path"] = scene_path;
			result["old_path"] = old_path;
			result["new_parent_path"] = new_parent_path;
			Dictionary response;
			response["jsonrpc"] = jsonrpc_version();
			response["id"] = request_id;
			response["result"] = result;
			return response;
		}
		if (name == "godot.scene.rename_node") {
			const Dictionary arguments = params.get("arguments", Dictionary());
			const String scene_path = arguments.get("scene_path", "");
			const String node_path = arguments.get("node_path", "");
			const String new_name = arguments.get("new_name", "");
			if (scene_path.is_empty() || !scene_path.begins_with("res://") || scene_path.get_extension() != "tscn" || node_path.is_empty() || node_path == "." || new_name.is_empty() || new_name.find_char('/') >= 0) {
				_append_audit(name, "rejected", "Invalid scene_path, node_path or new_name.");
				return _make_error(request_id, -32602, "scene_path, node_path and new_name are invalid.");
			}
			_track_transaction_file(scene_path);
			Ref<PackedScene> packed_scene = ResourceLoader::load(scene_path);
			if (packed_scene.is_null()) {
				return _make_error(request_id, -32033, "Scene could not be loaded.");
			}
			Node *scene_root = packed_scene->instantiate();
			Node *target = scene_root->get_node_or_null(NodePath(node_path));
			if (target == nullptr || target == scene_root) {
				memdelete(scene_root);
				return _make_error(request_id, -32034, "Scene child node path was not found.");
			}
			const String old_name = target->get_name();
			target->set_name(new_name);
			Ref<PackedScene> updated_scene;
			updated_scene.instantiate();
			updated_scene->pack(scene_root);
			memdelete(scene_root);
			const Error save_error = ResourceSaver::save(updated_scene, scene_path);
			if (save_error != OK) {
				_append_audit(name, "failed", vformat("ResourceSaver error %d", save_error));
				return _make_error(request_id, -32030, "Scene could not be saved.");
			}
			if (EditorFileSystem::get_singleton() != nullptr) {
				EditorFileSystem::get_singleton()->scan();
			}
			_track_transaction_file(scene_path);
			_append_audit(name, "updated", scene_path);
			Dictionary result;
			result["updated"] = true;
			result["scene_path"] = scene_path;
			result["old_name"] = old_name;
			result["new_name"] = new_name;
			Dictionary response;
			response["jsonrpc"] = jsonrpc_version();
			response["id"] = request_id;
			response["result"] = result;
			return response;
		}
		if (name == "godot.scene.remove_node") {
			const Dictionary arguments = params.get("arguments", Dictionary());
			const String scene_path = arguments.get("scene_path", "");
			const String node_path = arguments.get("node_path", "");
			if (scene_path.is_empty() || !scene_path.begins_with("res://") || scene_path.get_extension() != "tscn" || node_path.is_empty() || node_path == ".") {
				_append_audit(name, "rejected", "Invalid scene_path or node_path.");
				return _make_error(request_id, -32602, "scene_path must be res:// .tscn and node_path must identify a child node.");
			}
			_track_transaction_file(scene_path);
			Ref<PackedScene> packed_scene = ResourceLoader::load(scene_path);
			if (packed_scene.is_null()) {
				return _make_error(request_id, -32033, "Scene could not be loaded.");
			}
			Node *scene_root = packed_scene->instantiate();
			Node *target = scene_root->get_node_or_null(NodePath(node_path));
			if (target == nullptr || target == scene_root || target->get_parent() == nullptr) {
				memdelete(scene_root);
				return _make_error(request_id, -32034, "Scene child node path was not found.");
			}
			const String removed_name = target->get_name();
			target->get_parent()->remove_child(target);
			memdelete(target);
			Ref<PackedScene> updated_scene;
			updated_scene.instantiate();
			updated_scene->pack(scene_root);
			memdelete(scene_root);
			const Error save_error = ResourceSaver::save(updated_scene, scene_path);
			if (save_error != OK) {
				_append_audit(name, "failed", vformat("ResourceSaver error %d", save_error));
				return _make_error(request_id, -32030, "Scene could not be saved.");
			}
			if (EditorFileSystem::get_singleton() != nullptr) {
				EditorFileSystem::get_singleton()->scan();
			}
			_track_transaction_file(scene_path);
			_append_audit(name, "updated", scene_path);
			Dictionary result;
			result["updated"] = true;
			result["scene_path"] = scene_path;
			result["node_path"] = node_path;
			result["removed_name"] = removed_name;
			Dictionary response;
			response["jsonrpc"] = jsonrpc_version();
			response["id"] = request_id;
			response["result"] = result;
			return response;
		}
		if (name == "godot.scene.add_node") {
			const Dictionary arguments = params.get("arguments", Dictionary());
			const String scene_path = arguments.get("scene_path", "");
			const String parent_path = arguments.get("parent_path", ".");
			const String node_type = arguments.get("node_type", "Node");
			const String node_name = arguments.get("node_name", "MCPNode");
			const bool approved_node_type = node_type == "Node" || node_type == "Node2D" || node_type == "Node3D" || node_type == "Control";
			if (scene_path.is_empty() || !scene_path.begins_with("res://") || scene_path.get_extension() != "tscn" || !approved_node_type || node_name.is_empty()) {
				_append_audit(name, "rejected", "Invalid scene_path, node_type or node_name.");
				return _make_error(request_id, -32602, "scene_path, node_type and node_name are invalid.");
			}
			_track_transaction_file(scene_path);
			Ref<PackedScene> packed_scene = ResourceLoader::load(scene_path);
			if (packed_scene.is_null()) {
				return _make_error(request_id, -32033, "Scene could not be loaded.");
			}
			Node *scene_root = packed_scene->instantiate();
			Node *parent = parent_path == "." ? scene_root : scene_root->get_node_or_null(NodePath(parent_path));
			if (parent == nullptr) {
				memdelete(scene_root);
				return _make_error(request_id, -32034, "Scene parent path was not found.");
			}
			Object *node_object = ClassDB::instantiate(node_type);
			Node *new_node = Object::cast_to<Node>(node_object);
			if (new_node == nullptr) {
				if (node_object != nullptr) {
					memdelete(node_object);
				}
				memdelete(scene_root);
				return _make_error(request_id, -32602, "node_type must be an approved Node class.");
			}
			new_node->set_name(node_name);
			parent->add_child(new_node);
			new_node->set_owner(scene_root);
			Ref<PackedScene> updated_scene;
			updated_scene.instantiate();
			updated_scene->pack(scene_root);
			memdelete(scene_root);
			const Error save_error = ResourceSaver::save(updated_scene, scene_path);
			if (save_error != OK) {
				_append_audit(name, "failed", vformat("ResourceSaver error %d", save_error));
				return _make_error(request_id, -32030, "Scene could not be saved.");
			}
			_track_transaction_file(scene_path);
			_refresh_scene_after_mutation(scene_path);
			_track_transaction_file(scene_path);
			_append_audit(name, "updated", scene_path);
			Dictionary result;
			result["updated"] = true;
			result["scene_path"] = scene_path;
			result["parent_path"] = parent_path;
			result["node_name"] = node_name;
			Dictionary response;
			response["jsonrpc"] = jsonrpc_version();
			response["id"] = request_id;
			response["result"] = result;
			return response;
		}
		if (name == "godot.scene.create") {
			const Dictionary arguments = params.get("arguments", Dictionary());
			const String scene_path = arguments.get("scene_path", "");
			const String root_type = arguments.get("root_type", "Node");
			const bool allow_overwrite = arguments.get("allow_overwrite", false);
			const bool approved_root_type = root_type == "Node" || root_type == "Node2D" || root_type == "Node3D" || root_type == "Control";
			const String scene_absolute_path = ProjectSettings::get_singleton()->globalize_path(scene_path);
			if (scene_path.is_empty() || !scene_path.begins_with("res://") || scene_path.get_extension() != "tscn" || root_type.is_empty() || !approved_root_type) {
				_append_audit(name, "rejected", "Invalid scene_path or root_type.");
				return _make_error(request_id, -32602, "scene_path must be a res:// .tscn path and root_type must be an approved Node type.");
			}
			if (FileAccess::exists(scene_absolute_path) && !allow_overwrite) {
				_append_audit(name, "rejected", "Refused to overwrite an existing scene.");
				return _make_error(request_id, -32036, "Scene already exists; set allow_overwrite=true after confirmation.");
			}
			if (!has_approval && require_confirmation) {
				// 场景写入必须在主线程执行，并且只允许受控的 PackedScene 保存路径。
				_append_audit(name, "rejected", "Scene write requires explicit approval.");
				return _make_error(request_id, -32020, "Tool confirmation is required.");
			}
			approved_confirmation_tool.clear();
			Ref<PackedScene> packed_scene;
			packed_scene.instantiate();
			Object *root_object = ClassDB::instantiate(root_type);
			Node *root_node = Object::cast_to<Node>(root_object);
			if (root_node == nullptr) {
				if (root_object != nullptr) {
					memdelete(root_object);
				}
				return _make_error(request_id, -32602, "root_type must be a registered Node class.");
			}
			root_node->set_name("MCPRoot");
			packed_scene->pack(root_node);
			memdelete(root_node);
			const Error save_error = ResourceSaver::save(packed_scene, scene_path);
			if (save_error != OK) {
				_append_audit(name, "failed", vformat("ResourceSaver error %d", save_error));
				return _make_error(request_id, -32030, "Scene could not be saved.");
			}
			if (EditorFileSystem::get_singleton() != nullptr) {
				EditorFileSystem::get_singleton()->scan();
			}
			Dictionary result;
			result["created"] = true;
			result["scene_path"] = scene_path;
			result["root_type"] = root_type;
			result["transaction_active"] = !transaction_id.is_empty();
			if (!transaction_id.is_empty()) {
				transaction_created_files.push_back(scene_path);
			}
			_append_audit(name, "created", scene_path);
			Dictionary response;
			response["jsonrpc"] = jsonrpc_version();
			response["id"] = request_id;
			response["result"] = result;
			return response;
		}
		if (name == "godot.run.stop") {
			if (EditorInterface::get_singleton() == nullptr || !EditorInterface::get_singleton()->is_playing_scene()) {
				return _make_error(request_id, -32035, "No scene is currently running.");
			}
			EditorInterface::get_singleton()->stop_playing_scene();
			_append_audit(name, "stopped", EditorInterface::get_singleton()->get_playing_scene());
			Dictionary result;
			result["stopped"] = true;
			result["scene"] = EditorInterface::get_singleton()->get_playing_scene();
			Dictionary response;
			response["jsonrpc"] = jsonrpc_version();
			response["id"] = request_id;
			response["result"] = result;
			return response;
		}
		if (name == "godot.run") {
			const Dictionary arguments = params.get("arguments", Dictionary());
			const String action = arguments.get("action", "status");
			Dictionary request_copy = p_request;
			Dictionary forwarded = params;
			if (action == "start") forwarded["name"] = "godot.run.current_scene";
			else if (action == "stop") forwarded["name"] = "godot.run.stop";
			else {
				Dictionary result; result["running"] = EditorInterface::get_singleton() != nullptr && EditorInterface::get_singleton()->is_playing_scene();
				return _tool_response(request_id, result);
			}
			forwarded["_internal_compat"] = true;
			request_copy["params"] = forwarded;
			return _handle_rpc(request_copy);
		}
		if (name == "godot.run.diagnostics") {
			Dictionary result;
			ScriptEditorDebugger *debugger = EditorDebuggerNode::get_singleton() != nullptr ? EditorDebuggerNode::get_singleton()->get_current_debugger() : nullptr;
			result["available"] = debugger != nullptr;
			result["session_active"] = debugger != nullptr && debugger->is_session_active();
			result["breaked"] = debugger != nullptr && debugger->is_breaked();
			result["error_count"] = debugger != nullptr ? debugger->get_error_count() : 0;
			result["warning_count"] = debugger != nullptr ? debugger->get_warning_count() : 0;
			result["stack_file"] = debugger != nullptr ? debugger->get_stack_script_file() : String();
			result["stack_line"] = debugger != nullptr ? debugger->get_stack_script_line() : -1;
			result["stack_frame"] = debugger != nullptr ? debugger->get_stack_script_frame() : -1;
			result["message"] = debugger != nullptr ? String("Runtime debugger state queried.") : String("Runtime debugger is unavailable.");
			return _tool_response(request_id, result);
		}
		if (name == "godot.run.current_scene") {
			if (EditorInterface::get_singleton() == nullptr) {
				return _make_error(request_id, -32031, "Editor interface is unavailable.");
			}
			if (EditorInterface::get_singleton()->is_playing_scene()) {
				return _make_error(request_id, -32032, "A scene is already running.");
			}
			EditorInterface::get_singleton()->play_current_scene();
			_append_audit(name, "started", EditorInterface::get_singleton()->get_edited_scene_root() != nullptr ? EditorInterface::get_singleton()->get_edited_scene_root()->get_scene_file_path() : String());
			Dictionary result;
			result["started"] = true;
			result["scene"] = EditorInterface::get_singleton()->get_edited_scene_root() != nullptr ? EditorInterface::get_singleton()->get_edited_scene_root()->get_scene_file_path() : String();
			Dictionary response;
			response["jsonrpc"] = jsonrpc_version();
			response["id"] = request_id;
			response["result"] = result;
			return response;
		}
		if (name == "godot.editor.status") {
			Dictionary content;
			content["type"] = "text";
			content["text"] = JSON::stringify(_make_status_result(), "  ");
			Array contents;
			contents.push_back(content);
			Dictionary result;
			result["content"] = contents;
			result["structuredContent"] = _make_status_result();
			Dictionary response;
			response["jsonrpc"] = jsonrpc_version();
			response["id"] = request_id;
			response["result"] = result;
			_append_activity("MCP godot.editor.status completed.");
			return response;
		}
		return _make_error(request_id, -32601, "Tool execution is not implemented.");

		Dictionary content;
		content["type"] = "text";
		content["text"] = JSON::stringify(_make_status_result(), "  ");
		Array contents;
		contents.push_back(content);

		Dictionary result;
		result["content"] = contents;
		result["structuredContent"] = _make_status_result();

		Dictionary response;
		response["jsonrpc"] = jsonrpc_version();
		response["id"] = request_id;
		response["result"] = result;
		_append_activity("MCP godot.editor.status completed.");
		return response;
	}

	return _make_error(request_id, -32601, "Method not found.");
}

void MCPService::_send_response(int p_status, const Dictionary &p_body) {
	if (client.is_null()) {
		return;
	}

	const String body = JSON::stringify(p_body);
	const String status_text = p_status == 200 ? "OK" : p_status == 401 ? "Unauthorized" : "Bad Request";
	const String response = vformat("HTTP/1.1 %d %s\r\nContent-Type: application/json\r\nContent-Length: %d\r\nConnection: close\r\n\r\n%s", p_status, status_text, body.to_utf8_buffer().size(), body);
	const CharString utf8 = response.utf8();
	client->put_data(reinterpret_cast<const uint8_t *>(utf8.get_data()), utf8.size() - 1);
}

void MCPService::_handle_request(const String &p_request) {
	const int header_end = p_request.find("\r\n\r\n");
	if (header_end < 0) {
		return;
	}

	const Vector<String> lines = p_request.substr(0, header_end).split("\r\n");
	if (lines.is_empty()) {
		_send_response(400, _make_error(Variant(), -32600, "Malformed HTTP request."));
		return;
	}

	const Vector<String> request_line = lines[0].split(" ", false);
	if (request_line.size() != 3 || request_line[0] != "POST" || request_line[1] != "/mcp") {
		_send_response(400, _make_error(Variant(), -32600, "Only POST /mcp is supported."));
		return;
	}

	const String body = p_request.substr(header_end + 4).strip_edges();
	JSON json;
	if (json.parse(body) != OK || json.get_data().get_type() != Variant::DICTIONARY) {
		_send_response(400, _make_error(Variant(), -32700, "Invalid JSON payload."));
		return;
	}

	request_count++;
	const Dictionary request_dictionary = json.get_data();
	const Dictionary rpc_response = _handle_rpc(request_dictionary);
	_append_audit(request_dictionary.get("method", ""), "completed", "Request completed.");
	_send_response(200, rpc_response);
}

void MCPService::_notification(int p_what) {
	if (p_what != NOTIFICATION_PROCESS || state != STATE_RUNNING) {
		return;
	}

	if (client.is_null() && server->is_connection_available()) {
		client = server->take_connection();
		connection_generation++;
		client->set_no_delay(true);
	}

	if (client.is_null() || client->get_status() != StreamPeerTCP::STATUS_CONNECTED) {
		return;
	}

	const int available = client->get_available_bytes();
	if (available > 0) {
		if (client_buffer.length() + available > MAX_REQUEST_SIZE) {
			_send_response(400, _make_error(Variant(), -32600, "Request is too large."));
			_clear_client();
			return;
		}

		PackedByteArray received_data;
		received_data.resize(available);
		if (client->get_data(received_data.ptrw(), available) != OK) {
			_clear_client();
			return;
		}
		client_buffer += String::utf8(reinterpret_cast<const char *>(received_data.ptr()), received_data.size());
	}

	const int header_end = client_buffer.find("\r\n\r\n");
	if (header_end < 0) {
		return;
	}

	int content_length = 0;
	const Vector<String> headers = client_buffer.substr(0, header_end).split("\r\n");
	for (int i = 1; i < headers.size(); i++) {
		if (headers[i].to_lower().begins_with("content-length:")) {
			content_length = headers[i].substr(headers[i].find(":") + 1).strip_edges().to_int();
		}
	}

	if (content_length < 0 || header_end + 4 + content_length > MAX_REQUEST_SIZE) {
		_send_response(400, _make_error(Variant(), -32600, "Invalid Content-Length."));
		_clear_client();
		return;
	}
	if (client_buffer.length() < header_end + 4 + content_length) {
		return;
	}

	_handle_request(client_buffer.substr(0, header_end + 4 + content_length));
	_clear_client();
}

Error MCPService::start(const String &p_bind_address, int p_port, bool p_auto_port, bool p_require_confirmation) {
	stop();
	if (p_bind_address != "127.0.0.1" && p_bind_address != "::1") {
		last_error = "Only loopback addresses are allowed.";
		state = STATE_ERROR;
		return ERR_INVALID_PARAMETER;
	}

	server.instantiate();
	const int requested_port = p_auto_port ? 0 : p_port;
	const Error err = server->listen(requested_port, IPAddress(p_bind_address));
	if (err != OK) {
		last_error = vformat("Failed to listen on %s:%d.", p_bind_address, requested_port);
		state = STATE_ERROR;
		return err;
	}

	bind_address = p_bind_address;
	port = server->get_local_port();
	require_confirmation = p_require_confirmation;
	last_error.clear();
	state = STATE_RUNNING;
	set_process(true);
	_append_activity(vformat("MCP service started at %s.", get_endpoint()));
	return OK;
}

void MCPService::stop() {
	pending_confirmation_id.clear();
	pending_confirmation_tool.clear();
	pending_confirmation_started_msec = 0;
	approved_confirmation_tool.clear();
	if (!transaction_id.is_empty()) {
		_append_audit("godot.transaction", "aborted_on_stop", transaction_id);
		transaction_id.clear();
		transaction_label.clear();
		transaction_created_files.clear();
		transaction_files.clear();
		transaction_connection_generation = 0;
	}
	if (server.is_valid() && server->is_listening()) {
		server->stop();
		_append_activity("MCP service stopped.");
	}
	_clear_client();
	state = STATE_DISABLED;
	set_process(false);
}

String MCPService::get_endpoint() const {
	if (!is_running()) {
		return String();
	}
	return vformat("http://%s:%d/mcp", bind_address, port);
}

MCPService::MCPService() {
	set_process(false);

	tool_registry.push_back({ "godot.editor.status", "Returns the state of the built-in Godot MCP server.", "read-only", "low", false });
	tool_registry.push_back({ "godot.capture.editor_view", "Captures the active Godot editor viewport as a PNG.", "read-only", "low", false });
	tool_registry.push_back({ "godot.scene.add_3d_node", "Adds an approved 3D node to a scene.", "scene-write", "medium", true });
	tool_registry.push_back({ "godot.scene.set_transform", "Sets a controlled 3D node transform.", "scene-write", "medium", true });
	tool_registry.push_back({ "godot.scene.set_property", "Sets an approved 3D/light/camera property.", "scene-write", "medium", true });
	tool_registry.push_back({ "godot.scene.set_mesh", "Assigns a controlled mesh resource.", "scene-write", "medium", true });
	tool_registry.push_back({ "godot.scene.set_material", "Assigns a controlled material.", "scene-write", "medium", true });
	tool_registry.push_back({ "godot.run.capture_view", "Captures the running/editor game viewport as PNG.", "read-only", "low", false });
	tool_registry.push_back({ "godot.editor.open_scene", "Opens a controlled project scene in the editor.", "read-only", "low", false });
	tool_registry.push_back({ "godot.project.set_main_scene", "Sets an existing project scene as the main scene.", "project-write", "medium", true });
	tool_registry.push_back({ "godot.run.capture_camera_view", "Captures the target Camera3D viewport buffer as PNG.", "read-only", "low", false });
	tool_registry.push_back({ "godot.scene.create", "Creates a new scene with an approved root type.", "scene-write", "medium", true });
	tool_registry.push_back({ "godot.scene.add_node", "Adds an approved child node to an existing scene.", "scene-write", "medium", true });
	tool_registry.push_back({ "godot.scene.remove_node", "Removes a child node from an existing scene.", "scene-write", "medium", true });
	tool_registry.push_back({ "godot.scene.rename_node", "Renames a child node in an existing scene.", "scene-write", "medium", true });
	tool_registry.push_back({ "godot.scene.reparent_node", "Moves a child node under another approved parent.", "scene-write", "medium", true });
	tool_registry.push_back({ "godot.resource.create", "Creates an approved Godot resource type.", "project-write", "medium", true });
	tool_registry.push_back({ "godot.run.current_scene", "Runs the current scene through the editor debugger.", "run-control", "high", true });
	tool_registry.push_back({ "godot.run.stop", "Stops the currently running scene.", "run-control", "high", true });
	tool_registry.push_back({ "godot.project.inspect", "Inspects project state and configuration.", "read-only", "low", false });
	tool_registry.push_back({ "godot.project.scan", "Scans the project filesystem.", "read-only", "low", false });
	tool_registry.push_back({ "godot.scene.inspect", "Inspects a scene tree, node, and properties.", "read-only", "low", false });
	tool_registry.push_back({ "godot.scene.mutate", "Applies a batch of controlled scene operations.", "scene-write", "medium", true });
	tool_registry.push_back({ "godot.script.inspect", "Reads and validates a project script.", "read-only", "low", false });
	tool_registry.push_back({ "godot.script.edit", "Creates, updates, or attaches a project script.", "project-write", "high", true });
	tool_registry.push_back({ "godot.script.search", "Searches project scripts.", "read-only", "low", false });
	tool_registry.push_back({ "godot.resource.inspect", "Inspects a project resource.", "read-only", "low", false });
	tool_registry.push_back({ "godot.resource.mutate", "Creates or updates a controlled resource.", "project-write", "medium", true });
	tool_registry.push_back({ "godot.run", "Starts, stops, or reports the running game.", "run-control", "high", true });
	tool_registry.push_back({ "godot.run.diagnostics", "Returns runtime diagnostics.", "read-only", "low", false });
	tool_registry.push_back({ "godot.visual.capture", "Captures an editor, game, or camera view.", "read-only", "low", false });
	tool_registry.push_back({ "godot.test", "Runs project validation tests.", "run-control", "high", true });
	tool_registry.push_back({ "godot.build", "Builds or exports the project.", "project-write", "high", true });
	tool_registry.push_back({ "godot.scene.list_nodes", "Lists the nodes in a project scene.", "read-only", "low", false });
	tool_registry.push_back({ "godot.scene.inspect", "Inspects a scene node and its hierarchy.", "read-only", "low", false });
	tool_registry.push_back({ "godot.scene.get_property", "Reads a property from a scene node.", "read-only", "low", false });
	tool_registry.push_back({ "godot.script.create", "Creates a GDScript file inside the project.", "project-write", "medium", true });
	tool_registry.push_back({ "godot.script.read", "Reads a GDScript file inside the project.", "read-only", "low", false });
	tool_registry.push_back({ "godot.script.write", "Writes a GDScript file inside the project.", "project-write", "high", true });
	tool_registry.push_back({ "godot.script.attach", "Attaches a GDScript to a scene node.", "scene-write", "high", true });
	tool_registry.push_back({ "godot.script.validate", "Checks whether a GDScript can be loaded.", "read-only", "medium", false });
	tool_registry.push_back({ "godot.script.search", "Searches project-root GDScript files.", "read-only", "low", false });
	tool_registry.push_back({ "godot.project.scan", "Scans the project filesystem.", "read-only", "low", false });
	tool_registry.push_back({ "godot.run.get_output", "Returns runtime output when available.", "read-only", "low", false });
	tool_registry.push_back({ "godot.run.get_errors", "Returns runtime errors when available.", "read-only", "low", false });
	tool_registry.push_back({ "godot.run.get_stack_trace", "Returns runtime stack information when available.", "read-only", "low", false });
}

MCPService::~MCPService() {
	stop();
}
