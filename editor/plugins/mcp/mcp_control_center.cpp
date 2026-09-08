/**************************************************************************/
/*  mcp_control_center.cpp                                                */
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

#include "mcp_control_center.h"

#include "editor/plugins/mcp/mcp_service.h"

#include "core/math/random_number_generator.h"
#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "core/os/os.h"
#include "core/os/time.h"
#include "editor/editor_string_names.h"
#include "editor/settings/editor_command_palette.h"
#include "editor/settings/editor_settings.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/button.h"
#include "scene/gui/check_box.h"
#include "scene/gui/color_rect.h"
#include "scene/gui/label.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/panel_container.h"
#include "scene/gui/separator.h"
#include "scene/gui/spin_box.h"
#include "scene/gui/split_container.h"
#include "scene/gui/text_edit.h"
#include "scene/gui/tree.h"
#include "scene/gui/box_container.h"
#include "scene/resources/style_box_flat.h"
#include "servers/display/display_server.h"

namespace {
constexpr const char *SETTING_ENABLED = "mcp/bridge/enabled";
constexpr const char *SETTING_BIND_ADDRESS = "mcp/bridge/bind_address";
constexpr const char *SETTING_PORT = "mcp/bridge/port";
constexpr const char *SETTING_AUTO_PORT = "mcp/bridge/auto_select_port";
constexpr const char *SETTING_START_WITH_EDITOR = "mcp/bridge/start_with_editor";
constexpr const char *SETTING_TOKEN = "mcp/bridge/token";
constexpr const char *SETTING_CONFIRMATION = "mcp/bridge/require_confirmation";
constexpr const char *SETTING_CONFIRMATION_INITIALIZED = "mcp/bridge/confirmation_initialized";

Label *create_section_title(const String &p_text) {
	Label *label = memnew(Label);
	label->set_text(p_text);
	label->add_theme_font_size_override(SceneStringName(font_size), 16);
	return label;
}

Label *create_field_label(const String &p_text) {
	Label *label = memnew(Label);
	label->set_text(p_text);
	label->add_theme_color_override(SceneStringName(font_color), Color(0.62, 0.68, 0.76));
	return label;
}

void apply_card_style(Control *p_control, const Color &p_color, int p_radius = 6) {
	Ref<StyleBoxFlat> style;
	style.instantiate();
	style->set_bg_color(p_color);
	style->set_corner_radius_all(p_radius);
	style->set_border_width_all(1);
	style->set_border_color(Color(0.20, 0.22, 0.25, 0.9));
	style->set_content_margin(SIDE_LEFT, 8 * EDSCALE);
	style->set_content_margin(SIDE_RIGHT, 8 * EDSCALE);
	style->set_content_margin(SIDE_TOP, 7 * EDSCALE);
	style->set_content_margin(SIDE_BOTTOM, 7 * EDSCALE);
	p_control->add_theme_style_override(SceneStringName(panel), style);
}

void add_section_accent(Control *p_parent, const Color &p_color) {
	ColorRect *accent = memnew(ColorRect);
	accent->set_custom_minimum_size(Size2(3, 0) * EDSCALE);
	accent->set_color(p_color);
	p_parent->add_child(accent);
}

String create_token() {
	RandomNumberGenerator random;
	String token;
	for (int i = 0; i < 6; i++) {
		token += vformat("%08x", random.randi());
	}
	return token;
}
} // namespace

void MCPControlCenter::_notification(int p_what) {
	if (p_what == NOTIFICATION_PROCESS) {
		_refresh_from_service();
	} else if (p_what == NOTIFICATION_POSTINITIALIZE || p_what == NOTIFICATION_THEME_CHANGED) {
		_update_service_view();
		_update_confirmation_view();
	}
}
void MCPControlCenter::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_start_mcp_service"), &MCPControlCenter::_start_mcp_service);
}


void MCPControlCenter::_register_settings() {
	// 保留已加载的配置值；set_initial_value 仅在首次登记时提供默认值。
	EditorSettings *editor_settings = EditorSettings::get_singleton();
	if (!editor_settings->has_default_value(SETTING_ENABLED)) {
		editor_settings->set_initial_value(SETTING_ENABLED, false);
		editor_settings->set_initial_value(SETTING_BIND_ADDRESS, "127.0.0.1");
		editor_settings->set_initial_value(SETTING_PORT, 0);
		editor_settings->set_initial_value(SETTING_AUTO_PORT, true);
		editor_settings->set_initial_value(SETTING_START_WITH_EDITOR, false);
		editor_settings->set_initial_value(SETTING_TOKEN, "");
		editor_settings->set_initial_value(SETTING_CONFIRMATION, true);
		editor_settings->set_initial_value(SETTING_CONFIRMATION_INITIALIZED, false);
	}
}

void MCPControlCenter::_load_settings() {
	EditorSettings *editor_settings = EditorSettings::get_singleton();
	configured_enabled = editor_settings->get(SETTING_ENABLED);
	bind_address->set_text(editor_settings->get(SETTING_BIND_ADDRESS));
	port->set_value(editor_settings->get(SETTING_PORT));
	auto_select_port->set_pressed(editor_settings->get(SETTING_AUTO_PORT));
	start_with_editor->set_pressed(editor_settings->get(SETTING_START_WITH_EDITOR));
	bool confirmation_value = editor_settings->get(SETTING_CONFIRMATION);
	const bool confirmation_initialized = editor_settings->get(SETTING_CONFIRMATION_INITIALIZED);
	if (!confirmation_initialized) {
		confirmation_value = true;
		editor_settings->set(SETTING_CONFIRMATION, true);
		editor_settings->set(SETTING_CONFIRMATION_INITIALIZED, true);
		editor_settings->mark_setting_changed(SETTING_CONFIRMATION);
		editor_settings->mark_setting_changed(SETTING_CONFIRMATION_INITIALIZED);
		EditorSettings::save();
	}
	confirmation_required->set_pressed(confirmation_value);

	String saved_token = editor_settings->get(SETTING_TOKEN);
	if (saved_token.is_empty()) {
		// Token 仅用于本地 MCP 会话识别，活动日志和界面摘要都不能记录其明文。
		if (generated_token.is_empty()) {
			generated_token = create_token();
		}
		saved_token = generated_token;
		editor_settings->set_manually(SETTING_TOKEN, saved_token);
		editor_settings->mark_setting_changed(SETTING_TOKEN);
		EditorSettings::save();
	}
	token->set_text(saved_token);
	_update_endpoint();
	_update_service_view();
}

void MCPControlCenter::_save_settings() {
	EditorSettings *editor_settings = EditorSettings::get_singleton();
	editor_settings->set(SETTING_ENABLED, configured_enabled);
	editor_settings->set(SETTING_BIND_ADDRESS, bind_address->get_text());
	editor_settings->set(SETTING_PORT, (int)port->get_value());
	editor_settings->set(SETTING_AUTO_PORT, auto_select_port->is_pressed());
	editor_settings->set(SETTING_START_WITH_EDITOR, start_with_editor->is_pressed());
	editor_settings->set(SETTING_CONFIRMATION, confirmation_required->is_pressed());
	editor_settings->set(SETTING_TOKEN, token->get_text());
}

bool MCPControlCenter::_is_loopback_address(const String &p_address) const {
	return p_address == "127.0.0.1" || p_address == "::1";
}

void MCPControlCenter::_refresh_from_service() {
	if (!is_inside_tree()) {
		return;
	}
	if (service != nullptr) {
		const String service_activity = service->get_activity();
		if (activity_log->get_text() != service_activity) {
			activity_log->set_text(service_activity);
		}
	}
	_update_endpoint();
	_update_service_view();
	_update_confirmation_view();
}

void MCPControlCenter::_update_confirmation_view() {
	if (pending_confirmation == nullptr || service == nullptr) {
		return;
	}
	const bool has_pending = service->has_pending_confirmation();
	pending_confirmation->set_text(has_pending ? vformat(TTR("A write-tool confirmation is pending for %s. Review the MCP client request before approving."), service->get_pending_confirmation_tool()) : TTR("No pending tool confirmation."));
	if (is_inside_tree()) {
		pending_confirmation->add_theme_color_override(SceneStringName(font_color), get_theme_color(has_pending ? SNAME("warning_color") : SNAME("font_disabled_color"), EditorStringName(Editor)));
	}
}

void MCPControlCenter::_update_service_view() {
	if (!is_inside_tree()) {
		return;
	}
	const bool valid_address = _is_loopback_address(bind_address->get_text());
	const bool running = service != nullptr && service->is_running();
	const bool failed = service != nullptr && service->get_state() == MCPService::STATE_ERROR;

	service_status->set_text(running ? TTR("● Running") : failed ? TTR("● Error") : TTR("● Disabled"));
	if (is_inside_tree()) {
		service_status->add_theme_color_override(SceneStringName(font_color), get_theme_color(running ? SNAME("success_color") : failed ? SNAME("error_color") : SNAME("font_disabled_color"), EditorStringName(Editor)));
	}

	if (!valid_address) {
		service_hint->set_text(TTR("MCP currently accepts only loopback addresses (127.0.0.1 or ::1)."));
	} else if (failed) {
		service_hint->set_text(service->get_last_error());
	} else if (running) {
		service_hint->set_text(TTR("The built-in MCP service is listening on a local Streamable HTTP endpoint."));
	} else {
		service_hint->set_text(TTR("Enable MCP and start the local service."));
	}

	enable_button->set_text(configured_enabled ? TTR("Disable MCP") : TTR("Enable MCP"));
	start_button->set_disabled(!configured_enabled || !valid_address || running);
	stop_button->set_disabled(!running);
}

void MCPControlCenter::_update_endpoint() {
	if (!is_inside_tree()) {
		return;
	}
	const String address = bind_address->get_text();
	const int selected_port = (int)port->get_value();
	const bool use_auto_port = auto_select_port->is_pressed();
	endpoint->set_text(service != nullptr && service->is_running() ? service->get_endpoint() : vformat("http://%s:%s/mcp", address, use_auto_port ? TTR("auto") : itos(selected_port)));
	port->set_editable(!use_auto_port);
}

void MCPControlCenter::_append_activity(const String &p_message) {
	activity_log->set_text(activity_log->get_text() + p_message + "\n");
}

void MCPControlCenter::_rebuild_tool_tree() {
	tool_tree->clear();
	TreeItem *root = tool_tree->create_item();
	HashMap<StringName, TreeItem *> categories;
	const String filter = tool_search->get_text().to_lower();

	for (int i = 0; i < tools.size(); i++) {
		const ToolInfo &tool = tools[i];
		if (!filter.is_empty() && !String(tool.name).to_lower().contains(filter) && !tool.description.to_lower().contains(filter)) {
			continue;
		}

		TreeItem **category = categories.getptr(tool.category);
		if (category == nullptr) {
			TreeItem *item = tool_tree->create_item(root);
			item->set_text(0, tool.category);
			item->set_selectable(0, false);
			categories[tool.category] = item;
			category = categories.getptr(tool.category);
		}

		TreeItem *tool_item = tool_tree->create_item(*category);
		tool_item->set_text(0, tool.name);
		tool_item->set_text(1, tool.status == TOOL_STATUS_PLANNED ? TTR("Planned") : TTR("Ready"));
		tool_item->set_metadata(0, i);
		tool_item->set_tooltip_text(0, tool.description);
		tool_item->set_custom_bg_color(0, tool.status == TOOL_STATUS_PLANNED ? Color(0.18, 0.16, 0.14, 0.45) : Color(0.12, 0.20, 0.17, 0.45));
	}

	for (const KeyValue<StringName, TreeItem *> &entry : categories) {
		entry.value->set_collapsed(false);
	}
}

void MCPControlCenter::_show_tool(const ToolInfo &p_tool) {
	tool_name->set_text(p_tool.name);
	tool_status->set_text(p_tool.status == TOOL_STATUS_PLANNED ? TTR("Planned — not published to MCP clients") : TTR("Implemented — available when the MCP service is running"));
	tool_description->set_text(p_tool.description);
	tool_permission->set_text(vformat("%s: %s", TTR("Permission"), p_tool.permission));
	tool_risk->set_text(vformat("%s: %s", TTR("Risk"), p_tool.risk));
	tool_schema->set_text(p_tool.schema);
}

void MCPControlCenter::_enable_pressed() {
	_enable_toggled(!configured_enabled);
}

void MCPControlCenter::_enable_toggled(bool p_pressed) {
	configured_enabled = p_pressed;
	if (!configured_enabled && service != nullptr) {
		service->stop();
	}
	_save_settings();
	_update_endpoint();
	_update_service_view();
	_append_activity(vformat("%s  %s", Time::get_singleton()->get_time_string_from_system(), configured_enabled ? TTR("MCP configuration enabled.") : TTR("MCP configuration disabled.")));
}

void MCPControlCenter::_start_pressed() {
	if (service == nullptr) {
		return;
	}
	const Error err = service->start(bind_address->get_text(), (int)port->get_value(), auto_select_port->is_pressed(), token->get_text(), confirmation_required->is_pressed());
	if (err == OK) {
		_append_activity(vformat("%s  %s", Time::get_singleton()->get_time_string_from_system(), TTR("MCP service started.")));
	} else {
		_append_activity(vformat("%s  %s", Time::get_singleton()->get_time_string_from_system(), service->get_last_error()));
	}
	_update_endpoint();
	_update_service_view();
}

void MCPControlCenter::_start_mcp_service() {
	_start_pressed();
}

void MCPControlCenter::_stop_pressed() {
	if (service != nullptr) {
		service->stop();
	}
	_update_endpoint();
	_update_service_view();
}

void MCPControlCenter::_address_changed(const String &p_text) {
	_save_settings();
	_update_endpoint();
	_update_service_view();
}

void MCPControlCenter::_port_changed(double p_value) {
	_save_settings();
	_update_endpoint();
}

void MCPControlCenter::_auto_port_toggled(bool p_pressed) {
	_save_settings();
	_update_endpoint();
}

void MCPControlCenter::_start_with_editor_toggled(bool p_pressed) {
	_save_settings();
}

void MCPControlCenter::_confirmation_toggled(bool p_pressed) {
	_save_settings();
	_append_activity(vformat("%s  %s", Time::get_singleton()->get_time_string_from_system(), p_pressed ? TTR("Confirmation required for write tools.") : TTR("Confirmation requirement disabled.")));
}

void MCPControlCenter::_tool_search_changed(const String &p_text) {
	_rebuild_tool_tree();
}

void MCPControlCenter::_tool_selected() {
	TreeItem *selected = tool_tree->get_selected();
	if (selected == nullptr || selected->get_metadata(0).get_type() == Variant::NIL) {
		return;
	}

	const int tool_index = selected->get_metadata(0);
	ERR_FAIL_INDEX(tool_index, tools.size());
	_show_tool(tools[tool_index]);
}

void MCPControlCenter::_copy_endpoint() {
	DisplayServer::get_singleton()->clipboard_set(endpoint->get_text());
	_append_activity(vformat("%s  %s", Time::get_singleton()->get_time_string_from_system(), TTR("Endpoint copied to the clipboard.")));
}

void MCPControlCenter::_copy_token() {
	DisplayServer::get_singleton()->clipboard_set(token->get_text());
	_append_activity(vformat("%s  %s", Time::get_singleton()->get_time_string_from_system(), TTR("Authentication token copied to the clipboard.")));
}

void MCPControlCenter::_regenerate_token() {
	// 重新生成令牌后，后续 MCP 服务必须让旧会话失效，防止旧令牌继续被使用。
	generated_token = create_token();
	token->set_text(generated_token);
	_save_settings();
	_append_activity(vformat("%s  %s", Time::get_singleton()->get_time_string_from_system(), TTR("Authentication token regenerated.")));
}

void MCPControlCenter::set_service(MCPService *p_service) {
	service = p_service;
	_update_endpoint();
	_update_service_view();
}

void MCPControlCenter::start_with_editor_if_configured() {
	if (configured_enabled && start_with_editor->is_pressed()) {
		call_deferred(SNAME("_start_mcp_service"));
	}
}

MCPControlCenter::MCPControlCenter() {
	set_name(TTRC("MCP"));
	set_icon_name(SNAME("Network"));
	set_dock_shortcut(ED_SHORTCUT_AND_COMMAND("bottom_panels/toggle_mcp_bottom_panel", TTRC("Toggle MCP Dock")));
	set_default_slot(EditorDock::DOCK_SLOT_BOTTOM);
	set_available_layouts(EditorDock::DOCK_LAYOUT_HORIZONTAL | EditorDock::DOCK_LAYOUT_FLOATING);
	set_global(false);
	set_transient(true);
	set_custom_minimum_size(Size2(0, 300) * EDSCALE);

	_register_settings();

	VBoxContainer *root = memnew(VBoxContainer);
	root->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	root->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	add_child(root);

	HBoxContainer *header = memnew(HBoxContainer);
	root->add_child(header);
	apply_card_style(header, Color(31.0 / 255.0, 31.0 / 255.0, 31.0 / 255.0, 1.0));

	Label *header_title = create_section_title(TTR("MCP Control Center"));
	header_title->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	header->add_child(header_title);

	service_status = memnew(Label);
	service_status->add_theme_font_size_override(SceneStringName(font_size), 14);
	header->add_child(service_status);

	enable_button = memnew(Button);
	enable_button->connect(SceneStringName(pressed), callable_mp(this, &MCPControlCenter::_enable_pressed));
	header->add_child(enable_button);

	start_button = memnew(Button);
	start_button->set_text(TTR("Start"));
	start_button->connect(SceneStringName(pressed), callable_mp(this, &MCPControlCenter::_start_pressed));
	header->add_child(start_button);

	stop_button = memnew(Button);
	stop_button->set_text(TTR("Stop"));
	stop_button->connect(SceneStringName(pressed), callable_mp(this, &MCPControlCenter::_stop_pressed));
	header->add_child(stop_button);

	service_hint = memnew(Label);
	service_hint->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	root->add_child(service_hint);

	root->add_child(memnew(HSeparator));

	// 采用调试器式的可拖拽纵向栏：四个工作区并列，分别拥有独立分隔条。
	HSplitContainer *main_split = memnew(HSplitContainer);
	main_split->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	main_split->set_split_offset(250 * EDSCALE);
	root->add_child(main_split);

	PanelContainer *server_panel = memnew(PanelContainer);
	server_panel->set_custom_minimum_size(Size2(250, 0) * EDSCALE);
	apply_card_style(server_panel, Color(31.0 / 255.0, 31.0 / 255.0, 31.0 / 255.0, 1.0));
	main_split->add_child(server_panel);

	VBoxContainer *server_box = memnew(VBoxContainer);
	server_panel->add_child(server_box);
	Label *server_title = create_section_title(TTR("Server & Connection"));
	server_title->add_theme_color_override(SceneStringName(font_color), Color(0.58, 0.82, 0.76));
	server_box->add_child(server_title);

	HBoxContainer *connection_row = memnew(HBoxContainer);
	connection_row->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	server_box->add_child(connection_row);
	VBoxContainer *address_box = memnew(VBoxContainer);
	address_box->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	connection_row->add_child(address_box);
	address_box->add_child(create_field_label(TTR("Bind Address")));
	bind_address = memnew(LineEdit);
	bind_address->connect(SceneStringName(text_changed), callable_mp(this, &MCPControlCenter::_address_changed));
	address_box->add_child(bind_address);
	VBoxContainer *port_box = memnew(VBoxContainer);
	port_box->set_custom_minimum_size(Size2(150, 0) * EDSCALE);
	connection_row->add_child(port_box);
	port_box->add_child(create_field_label(TTR("Port")));
	port = memnew(SpinBox);
	port->set_min(0);
	port->set_max(65535);
	port->set_step(1);
	port->connect(SceneStringName(value_changed), callable_mp(this, &MCPControlCenter::_port_changed));
	port_box->add_child(port);

	HBoxContainer *options_row = memnew(HBoxContainer);
	options_row->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	server_box->add_child(options_row);
	auto_select_port = memnew(CheckBox);
	auto_select_port->set_text(TTR("Auto-select available port"));
	auto_select_port->connect(SceneStringName(toggled), callable_mp(this, &MCPControlCenter::_auto_port_toggled));
	options_row->add_child(auto_select_port);
	start_with_editor = memnew(CheckBox);
	start_with_editor->set_text(TTR("Start with editor"));
	start_with_editor->connect(SceneStringName(toggled), callable_mp(this, &MCPControlCenter::_start_with_editor_toggled));
	options_row->add_child(start_with_editor);
	confirmation_required = memnew(CheckBox);
	confirmation_required->set_text(TTR("Confirm write tools"));
	confirmation_required->connect(SceneStringName(toggled), callable_mp(this, &MCPControlCenter::_confirmation_toggled));
	options_row->add_child(confirmation_required);

	HBoxContainer *endpoint_row = memnew(HBoxContainer);
	endpoint_row->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	server_box->add_child(create_field_label(TTR("Endpoint")));
	server_box->add_child(endpoint_row);
	endpoint = memnew(LineEdit);
	endpoint->set_editable(false);
	endpoint->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	endpoint_row->add_child(endpoint);
	Button *copy_endpoint = memnew(Button);
	copy_endpoint->set_text(TTR("Copy"));
	copy_endpoint->connect(SceneStringName(pressed), callable_mp(this, &MCPControlCenter::_copy_endpoint));
	endpoint_row->add_child(copy_endpoint);

	HBoxContainer *token_row = memnew(HBoxContainer);
	token_row->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	server_box->add_child(create_field_label(TTR("Authentication Token")));
	server_box->add_child(token_row);
	token = memnew(LineEdit);
	token->set_secret(true);
	token->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	token_row->add_child(token);
	Button *copy_token = memnew(Button);
	copy_token->set_text(TTR("Copy"));
	copy_token->connect(SceneStringName(pressed), callable_mp(this, &MCPControlCenter::_copy_token));
	token_row->add_child(copy_token);
	Button *regenerate_token = memnew(Button);
	regenerate_token->set_text(TTR("Regenerate"));
	regenerate_token->connect(SceneStringName(pressed), callable_mp(this, &MCPControlCenter::_regenerate_token));
	token_row->add_child(regenerate_token);

	HSplitContainer *workspace_split = memnew(HSplitContainer);
	workspace_split->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	workspace_split->set_split_offset(300 * EDSCALE);
	main_split->add_child(workspace_split);

	PanelContainer *tools_panel = memnew(PanelContainer);
	tools_panel->set_custom_minimum_size(Size2(260, 0) * EDSCALE);
	apply_card_style(tools_panel, Color(31.0 / 255.0, 31.0 / 255.0, 31.0 / 255.0, 1.0));
	workspace_split->add_child(tools_panel);
	VBoxContainer *tools_box = memnew(VBoxContainer);
	tools_panel->add_child(tools_box);
	Label *tools_title = create_section_title(TTR("Tools"));
	tools_title->add_theme_color_override(SceneStringName(font_color), Color(0.62, 0.78, 0.96));
	tools_box->add_child(tools_title);
	tool_search = memnew(LineEdit);
	tool_search->set_placeholder(TTR("Search tools"));
	tool_search->connect(SceneStringName(text_changed), callable_mp(this, &MCPControlCenter::_tool_search_changed));
	tools_box->add_child(tool_search);
	tool_tree = memnew(Tree);
	tool_tree->set_hide_root(true);
	tool_tree->set_columns(2);
	tool_tree->set_column_title(0, TTR("Tool"));
	tool_tree->set_column_title(1, TTR("State"));
	tool_tree->set_column_titles_visible(true);
	tool_tree->set_column_expand(0, true);
	tool_tree->set_column_expand(1, false);
	tool_tree->set_column_custom_minimum_width(1, 74 * EDSCALE);
	tool_tree->set_theme_type_variation("TreeSecondary");
	tool_tree->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	tool_tree->connect(SceneStringName(item_selected), callable_mp(this, &MCPControlCenter::_tool_selected));
	tools_box->add_child(tool_tree);

	VSplitContainer *details_activity_split = memnew(VSplitContainer);
	details_activity_split->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	details_activity_split->set_split_offset(300 * EDSCALE);
	workspace_split->add_child(details_activity_split);

	PanelContainer *detail_panel = memnew(PanelContainer);
	detail_panel->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	apply_card_style(detail_panel, Color(31.0 / 255.0, 31.0 / 255.0, 31.0 / 255.0, 1.0));
	details_activity_split->add_child(detail_panel);
	VBoxContainer *detail_box = memnew(VBoxContainer);
	detail_panel->add_child(detail_box);
	Label *details_title = create_section_title(TTR("Tool Details"));
	details_title->add_theme_color_override(SceneStringName(font_color), Color(0.76, 0.68, 0.94));
	detail_box->add_child(details_title);
	tool_name = create_section_title(TTR("Select a tool"));
	detail_box->add_child(tool_name);
	tool_name->add_theme_color_override(SceneStringName(font_color), Color(0.82, 0.86, 0.94));
	tool_status = memnew(Label);
	detail_box->add_child(tool_status);
	tool_description = memnew(Label);
	tool_description->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	detail_box->add_child(tool_description);
	tool_permission = memnew(Label);
	tool_permission->add_theme_color_override(SceneStringName(font_color), Color(0.62, 0.76, 0.90));
	detail_box->add_child(tool_permission);
	tool_risk = memnew(Label);
	tool_risk->add_theme_color_override(SceneStringName(font_color), Color(0.90, 0.72, 0.48));
	detail_box->add_child(tool_risk);
	detail_box->add_child(create_field_label(TTR("Input JSON Schema")));
	tool_schema = memnew(TextEdit);
	tool_schema->set_editable(false);
	tool_schema->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	detail_box->add_child(tool_schema);

	PanelContainer *activity_panel = memnew(PanelContainer);
	activity_panel->set_custom_minimum_size(Size2(0, 130) * EDSCALE);
	apply_card_style(activity_panel, Color(31.0 / 255.0, 31.0 / 255.0, 31.0 / 255.0, 1.0));
	details_activity_split->add_child(activity_panel);
	VBoxContainer *activity_box = memnew(VBoxContainer);
	activity_panel->add_child(activity_box);
	Label *activity_title = create_section_title(TTR("Activity & Audit"));
	activity_title->add_theme_color_override(SceneStringName(font_color), Color(0.66, 0.75, 0.88));
	activity_box->add_child(activity_title);
	activity_log = memnew(TextEdit);
	activity_log->set_editable(false);
	activity_log->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	activity_box->add_child(activity_log);
	pending_confirmation = memnew(Label);
	pending_confirmation->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	activity_box->add_child(pending_confirmation);

	set_process(true);
	tools.push_back({ SNAME("Project & Diagnostics"), SNAME("godot.editor.status"), TTR("Returns the MCP editor status and local endpoint configuration."), TTR("Read-only"), TTR("Low"), "{\n  \"type\": \"object\",\n  \"properties\": {}\n}", TOOL_STATUS_IMPLEMENTED });
	tools.push_back({ SNAME("Scene Authoring"), SNAME("godot.scene.create"), TTR("Creates a new scene with a typed root node."), TTR("Scene write"), TTR("Medium"), "{\n  \"type\": \"object\",\n  \"required\": [\"scene_path\", \"root_type\"]\n}", TOOL_STATUS_PLANNED });
	tools.push_back({ SNAME("Scene Authoring"), SNAME("godot.scene.add_node"), TTR("Adds a typed child node to an existing scene."), TTR("Scene write"), TTR("Medium"), "{\n  \"type\": \"object\",\n  \"required\": [\"scene_path\", \"parent_path\", \"node_type\"]\n}", TOOL_STATUS_IMPLEMENTED });
	tools.push_back({ SNAME("Resources & Assets"), SNAME("godot.resource.create"), TTR("Creates an approved Godot resource type."), TTR("Project write"), TTR("Medium"), "{\n  \"type\": \"object\",\n  \"required\": [\"resource_type\"]\n}", TOOL_STATUS_IMPLEMENTED });
	tools.push_back({ SNAME("Run & Debug"), SNAME("godot.run.current_scene"), TTR("Runs the current scene through the editor debugger."), TTR("Run control"), TTR("High"), "{\n  \"type\": \"object\",\n  \"properties\": {}\n}", TOOL_STATUS_IMPLEMENTED });
	tools.push_back({ SNAME("Capture & Validation"), SNAME("godot.capture.editor_view"), TTR("Captures the active editor viewport for visual validation."), TTR("Read-only"), TTR("Low"), "{\n  \"type\": \"object\",\n  \"properties\": {}\n}", TOOL_STATUS_PLANNED });
	tools.push_back({ SNAME("Transactions & Security"), SNAME("godot.transaction.begin"), TTR("Starts a named MCP transaction for grouped editor changes."), TTR("Scene write"), TTR("Medium"), "{\n  \"type\": \"object\",\n  \"required\": [\"label\"]\n}", TOOL_STATUS_PLANNED });

	_load_settings();
	_rebuild_tool_tree();
	_append_activity(vformat("%s  %s", Time::get_singleton()->get_time_string_from_system(), TTR("MCP Control Center initialized.")));
}
