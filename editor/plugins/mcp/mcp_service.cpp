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
#include "core/os/time.h"
#include "core/string/print_string.h"

namespace {
constexpr int MAX_REQUEST_SIZE = 1024 * 1024;

String jsonrpc_version() {
	return "2.0";
}
} // namespace

void MCPService::_bind_methods() {
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

	Dictionary tool;
	tool["name"] = "godot.editor.status";
	tool["title"] = "Godot Editor Status";
	tool["description"] = "Returns the state of the built-in Godot MCP server.";
	tool["inputSchema"] = input_schema;
	tools.push_back(tool);
	tools.push_back(Dictionary());
	Dictionary transaction_begin;
	transaction_begin["name"] = "godot.transaction.begin";
	transaction_begin["description"] = "Starts a named MCP transaction shell.";
	transaction_begin["inputSchema"] = input_schema;
	tools[1] = transaction_begin;
	Dictionary transaction_commit;
	transaction_commit["name"] = "godot.transaction.commit";
	transaction_commit["description"] = "Commits the active MCP transaction shell.";
	transaction_commit["inputSchema"] = input_schema;
	tools.push_back(transaction_commit);
	Dictionary transaction_rollback;
	transaction_rollback["name"] = "godot.transaction.rollback";
	transaction_rollback["description"] = "Rolls back the active MCP transaction shell.";
	transaction_rollback["inputSchema"] = input_schema;
	tools.push_back(transaction_rollback);
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
		_append_audit(method, outcome, completed_id);
		transaction_id.clear();
		transaction_label.clear();
		Dictionary result;
		result["transactionId"] = completed_id;
		result["outcome"] = outcome;
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

	if (method == "tools/call") {
		const Dictionary params = p_request.get("params", Dictionary());
		const String name = params.get("name", "");
		if (name != "godot.editor.status") {
			return _make_error(request_id, -32601, "Tool is not available.");
		}

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

	String authorization;
	for (int i = 1; i < lines.size(); i++) {
		const int separator = lines[i].find(":");
		if (separator < 0) {
			continue;
		}
		const String key = lines[i].substr(0, separator).strip_edges().to_lower();
		if (key == "authorization") {
			authorization = lines[i].substr(separator + 1).strip_edges();
		}
	}

	// 本地服务仍要求 Bearer Token，避免同机其他进程无授权调用编辑器自动化接口。
	if (authorization.strip_edges() != authorization_value) {
		_send_response(401, _make_error(Variant(), -32001, "Unauthorized."));
		_append_audit("authentication", "rejected", "Invalid Bearer token.");
		_append_activity("Rejected an MCP request with an invalid token.");
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

Error MCPService::start(const String &p_bind_address, int p_port, bool p_auto_port, const String &p_token) {
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
	token = p_token.strip_edges();
	authorization_value = String("Bearer ") + token;
	last_error.clear();
	state = STATE_RUNNING;
	set_process(true);
	_append_activity(vformat("MCP service started at %s.", get_endpoint()));
	return OK;
}

void MCPService::stop() {
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
}

MCPService::~MCPService() {
	stop();
}
