/**************************************************************************/
/*  mcp_editor_plugin.cpp                                                 */
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

#include "mcp_editor_plugin.h"

#include "core/object/class_db.h"
#include "editor/docks/editor_dock_manager.h"
#include "editor/plugins/mcp/mcp_control_center.h"
#include "editor/plugins/mcp/mcp_service.h"

void MCPBridgeEditorPlugin::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_start_mcp_service"), &MCPBridgeEditorPlugin::_start_mcp_service);
}

void MCPBridgeEditorPlugin::_start_mcp_service() {
	// 若用户在 Control Center 中勾选随编辑器启动，则由同一 UI 状态启动服务。
	control_center->start_with_editor_if_configured();
}

MCPBridgeEditorPlugin::MCPBridgeEditorPlugin() {
	service = memnew(MCPService);
	add_child(service);

	control_center = memnew(MCPControlCenter);
	control_center->set_service(service);
	EditorDockManager::get_singleton()->add_dock(control_center);
	// 编辑器插件初始化尚未结束时不要直接绑定端口，延后到下一轮主循环再启动服务。
	call_deferred(SNAME("_start_mcp_service"));
}

MCPBridgeEditorPlugin::~MCPBridgeEditorPlugin() {
	if (control_center != nullptr) {
		EditorDockManager::get_singleton()->remove_dock(control_center);
		memdelete(control_center);
		control_center = nullptr;
	}
	if (service != nullptr) {
		service->stop();
		if (service->get_parent() == this) {
			remove_child(service);
		}
		memdelete(service);
		service = nullptr;
	}
}
