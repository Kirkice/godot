/**************************************************************************/
/*  mcp_control_center.h                                                  */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
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

#include "editor/docks/editor_dock.h"

class Button;
class CheckBox;
class Label;
class LineEdit;
class SpinBox;
class TextEdit;
class Tree;
class TreeItem;

class MCPService;

class MCPControlCenter : public EditorDock {
	GDCLASS(MCPControlCenter, EditorDock);

	enum ToolStatus {
		TOOL_STATUS_PLANNED,
		TOOL_STATUS_IMPLEMENTED,
	};

	struct ToolInfo {
		StringName category;
		StringName name;
		String description;
		String permission;
		String risk;
		String schema;
		ToolStatus status = TOOL_STATUS_PLANNED;
	};

	Label *service_status = nullptr;
	Label *service_hint = nullptr;
	LineEdit *bind_address = nullptr;
	SpinBox *port = nullptr;
	CheckBox *auto_select_port = nullptr;
	CheckBox *start_with_editor = nullptr;
	LineEdit *endpoint = nullptr;
	LineEdit *token = nullptr;
	Button *enable_button = nullptr;
	Button *start_button = nullptr;
	Button *stop_button = nullptr;
	LineEdit *tool_search = nullptr;
	Tree *tool_tree = nullptr;
	Label *tool_name = nullptr;
	Label *tool_status = nullptr;
	Label *tool_description = nullptr;
	Label *tool_permission = nullptr;
	Label *tool_risk = nullptr;
	TextEdit *tool_schema = nullptr;
	TextEdit *activity_log = nullptr;

	Vector<ToolInfo> tools;
	MCPService *service = nullptr;
	bool configured_enabled = false;
	String generated_token;

	void _register_settings();
	void _load_settings();
	void _save_settings();
	void _update_service_view();
	void _update_endpoint();
	void _rebuild_tool_tree();
	void _show_tool(const ToolInfo &p_tool);
	void _append_activity(const String &p_message);
	void _refresh_from_service();
	bool _is_loopback_address(const String &p_address) const;

	void _enable_pressed();
	void _enable_toggled(bool p_pressed);
	void _start_pressed();
	void _start_mcp_service();
	void _stop_pressed();
	void _address_changed(const String &p_text);
	void _port_changed(double p_value);
	void _auto_port_toggled(bool p_pressed);
	void _start_with_editor_toggled(bool p_pressed);
	void _tool_search_changed(const String &p_text);
	void _tool_selected();
	void _copy_endpoint();
	void _copy_token();
	void _regenerate_token();

protected:
	void _notification(int p_what);
	static void _bind_methods();

public:
	void set_service(MCPService *p_service);
	void start_with_editor_if_configured();

	MCPControlCenter();
};
