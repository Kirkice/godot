/**************************************************************************/
/*  mcp_service.h                                                         */
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

#pragma once

#include "core/io/tcp_server.h"
#include "scene/main/node.h"

class MCPService : public Node {
	GDCLASS(MCPService, Node);

public:
	enum State {
		STATE_DISABLED,
		STATE_RUNNING,
		STATE_ERROR,
	};

	struct AuditEntry {
		String timestamp;
		String method;
		String outcome;
		String detail;
	};

	Ref<TCPServer> server;
	Ref<StreamPeerTCP> client;
	String client_buffer;
	String token;
	String authorization_value;
	String bind_address;
	String last_error;
	String activity;
	Vector<AuditEntry> audit_entries;
	int port = 0;
	State state = STATE_DISABLED;
	int request_count = 0;
	String transaction_id;
	String transaction_label;

	void _append_activity(const String &p_message);
	void _append_audit(const String &p_method, const String &p_outcome, const String &p_detail);
	void _send_response(int p_status, const Dictionary &p_body);
	void _handle_request(const String &p_request);
	Dictionary _make_error(const Variant &p_id, int p_code, const String &p_message) const;
	Dictionary _handle_rpc(const Dictionary &p_request);
	Dictionary _make_status_result() const;
	Array _get_tools() const;
	void _clear_client();

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	Error start(const String &p_bind_address, int p_port, bool p_auto_port, const String &p_token);
	void stop();
	bool is_running() const { return state == STATE_RUNNING; }
	State get_state() const { return state; }
	String get_last_error() const { return last_error; }
	String get_endpoint() const;
	String get_activity() const { return activity; }
	int get_request_count() const { return request_count; }
	Vector<AuditEntry> get_audit_entries() const { return audit_entries; }

	MCPService();
	~MCPService();
};
