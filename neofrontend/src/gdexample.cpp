#include "gdexample.h"
#include <godot_cpp/classes/check_box.hpp>
#include <godot_cpp/classes/display_server.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/core/memory.hpp>

#include <ws/client.h>

using namespace godot;
using namespace Csm;

void Live2DController::_bind_methods() {
	// 绑定方法
	ClassDB::bind_method(D_METHOD("_process", "delta"), &Live2DController::_process);
	ClassDB::bind_method(D_METHOD("_input", "event"), &Live2DController::_input);
	ClassDB::bind_method(D_METHOD("_ready"), &Live2DController::_ready);

	// WebSocket信号
	ClassDB::bind_method(D_METHOD("_on_websocket_connected_to_server"), &Live2DController::_on_websocket_connected_to_server);
	ClassDB::bind_method(D_METHOD("_on_websocket_connection_closed", "code", "reason", "was_clean"), &Live2DController::_on_websocket_connection_closed);
	ClassDB::bind_method(D_METHOD("_on_websocket_connection_error"), &Live2DController::_on_websocket_connection_error);
	ClassDB::bind_method(D_METHOD("_on_websocket_data_received", "data"), &Live2DController::_on_websocket_data_received);
	ClassDB::bind_method(D_METHOD("_on_websocket_server_close_request", "code", "reason"), &Live2DController::_on_websocket_server_close_request);
	ClassDB::bind_method(D_METHOD("_process_websocket_events"), &Live2DController::_process_websocket_events);

	// UI信号
	ClassDB::bind_method(D_METHOD("_on_audio_button_pressed"), &Live2DController::_on_audio_button_pressed);
	ClassDB::bind_method(D_METHOD("_on_input_area_text_submitted", "new_text"), &Live2DController::_on_input_area_text_submitted);
	ClassDB::bind_method(D_METHOD("_on_dictation_checkbox_toggled", "button_pressed"), &Live2DController::_on_dictation_checkbox_toggled);
	ClassDB::bind_method(D_METHOD("_on_fullscreen_checkbox_toggled", "button_pressed"), &Live2DController::_on_fullscreen_checkbox_toggled);
	ClassDB::bind_method(D_METHOD("_on_pause_dictation_checkbox_toggled", "button_pressed"), &Live2DController::_on_pause_dictation_checkbox_toggled);

	// 公共方法
	ClassDB::bind_method(D_METHOD("enable_audio_activities"), &Live2DController::enable_audio_activities);
	ClassDB::bind_method(D_METHOD("send_message", "message"), &Live2DController::send_message);
	ClassDB::bind_method(D_METHOD("connect_to_server", "url", "name"), &Live2DController::connect_to_server);

	// 属性
	ClassDB::add_property(get_class_static(), PropertyInfo(Variant::STRING, "model_url"), "set_model_url", "get_model_url");
	ClassDB::add_property(get_class_static(), PropertyInfo(Variant::STRING, "server_url"), "set_server_url", "get_server_url");
	ClassDB::add_property(get_class_static(), PropertyInfo(Variant::STRING, "agent_name"), "set_agent_name", "get_agent_name");
}

Live2DController::Live2DController() {
	// 初始化变量
	cubism_framework = nullptr;
	model = nullptr;
	model_url = ""; // 默认模型路径
	server_url = "ws://localhost:8000";
	agent_name = "shumeiniang";
	connected = false;

	// UI状态
	audio_enabled = false;
	volume = 0.0f;
	stream_audio_player = nullptr;
	input_text = "";
	show_config_ui = false;
	enable_dictation = false;
	enable_full_screen = false;
	allow_pause_dictation = true;
	subtitle_hidden = true;
	in_response = false;
}

Live2DController::~Live2DController() {
	// 清理资源
	if (model) {
		delete model;
	}

	if (cubism_framework) {
		CubismFramework::Dispose();
	}
}

void Live2DController::_ready() {
	// 获取场景中的节点
	viewport = get_node<Viewport>(NodePath("Viewport"));
	camera = get_node<Camera2D>(NodePath("Viewport/Camera2D"));
	model_display = get_node<TextureRect>(NodePath("ModelDisplay"));

	input_area = get_node<LineEdit>(NodePath("UserInterface/InputArea"));
	audio_button = get_node<Button>(NodePath("UserInterface/AudioButton"));
	subtitle_label = get_node<Label>(NodePath("SubtitleContainer/SubtitleText"));
	subtitle_container = get_node<Control>(NodePath("SubtitleContainer"));
	config_ui = get_node<Control>(NodePath("ConfigUI"));
	dictation_checkbox = get_node<CheckBox>(NodePath("ConfigUI/DictationCheckbox"));
	fullscreen_checkbox = get_node<CheckBox>(NodePath("ConfigUI/FullscreenCheckbox"));
	pause_dictation_checkbox = get_node<CheckBox>(NodePath("ConfigUI/PauseDictationCheckbox"));
	audio_player = get_node<AudioStreamPlayer>(NodePath("AudioPlayer"));

	// 创建 StreamAudioPlayer
	stream_audio_player = memnew(StreamAudioPlayer);
	add_child(stream_audio_player);
	stream_audio_player->init();

	// 初始化WebSocket
	ws_client.instantiate();

	// 连接到服务器
	connect_to_server(server_url, agent_name);

	audio_button->connect("pressed", Callable(this, "_on_audio_button_pressed"));
	input_area->connect("text_submitted", Callable(this, "_on_input_area_text_submitted"));
	dictation_checkbox->connect("toggled", Callable(this, "_on_dictation_checkbox_toggled"));
	fullscreen_checkbox->connect("toggled", Callable(this, "_on_fullscreen_checkbox_toggled"));
	pause_dictation_checkbox->connect("toggled", Callable(this, "_on_pause_dictation_checkbox_toggled"));

	// 初始化Live2D
	initialize_cubism();

	// 加载模型
	if (!model_url.is_empty()) {
		load_model(model_url);
	}
}

void Live2DController::_process(double delta) {
	// 处理WebSocket事件
	_process_websocket_events();

	// 处理事件队列
	process_event_queue();

	// 更新Live2D模型
	if (model) {
		// 更新模型参数
		// 这里需要实现Live2D模型的更新逻辑
	}

	// 口型同步
	if (stream_audio_player && stream_audio_player->get_volume() > 0) {
		// 计算音量
		volume = stream_audio_player->get_volume();

		// 设置模型口型参数
		if (model) {
			// 这里需要实现口型同步逻辑
		}
	}
}

void Live2DController::_input(const Ref<InputEvent> &event) {
	// 处理键盘事件
	if (event->is_class("InputEventKey")) {
		Ref<InputEventKey> key_event = event;
		if (key_event.is_valid() && key_event->is_pressed() && key_event->get_keycode() == KEY_EQUAL) {
			// 显示/隐藏配置界面
			show_config_ui = !show_config_ui;
			config_ui->set_visible(show_config_ui);
		}
	}
}

void Live2DController::initialize_cubism() {
	// 初始化Live2D框架
	if (!CubismFramework::IsInitialized()) {
		CubismFramework::Initialize();
		// 注意：CubismFramework 5.x 不再使用 GetInstance()
	}
}

void Live2DController::load_model(const String &model_path) {
	// 加载Live2D模型
	// 这里需要实现Live2D模型的加载逻辑
	// 参考Cubism SDK的示例代码
}

void Live2DController::_on_websocket_connected_to_server() {
	// WebSocket连接成功
	connected = true;
	UtilityFunctions::print("WebSocket connected to server");
}

void Live2DController::_on_websocket_connection_closed(int code, String reason, bool was_clean) {
	// WebSocket连接关闭
	connected = false;
	UtilityFunctions::print("WebSocket connection closed: " + reason);
}

void Live2DController::_on_websocket_connection_error() {
	// WebSocket连接错误
	UtilityFunctions::print("WebSocket connection error");
}

void Live2DController::_on_websocket_data_received(PackedByteArray data) {
	// 处理接收到的数据
	String json_str;
	for (uint8_t byte : data) {
		json_str += String::chr(byte);
	}

	// 解析JSON数据
	JSON json_parser;
	Error err = json_parser.parse(json_str);
	if (err == OK) {
		Dictionary message = json_parser.get_data();
		if (!message.is_empty()) {
			event_queue.push(message);
		}
	}
}

void Live2DController::_on_websocket_server_close_request(int code, String reason) {
	// 服务器请求关闭连接
	UtilityFunctions::print("WebSocket server close request: " + reason);
}

void Live2DController::_process_websocket_events() {
	// 处理WebSocket事件
	if (ws_client.is_valid()) {
		ws_client->poll();

		// 检查连接状态
		if (ws_client->get_ready_state() == WebSocketPeer::STATE_CONNECTING) {
			// 正在连接
		} else if (ws_client->get_ready_state() == WebSocketPeer::STATE_OPEN && !connected) {
			// 连接成功
			_on_websocket_connected_to_server();
		} else if (ws_client->get_ready_state() == WebSocketPeer::STATE_CLOSING) {
			// 正在关闭
		} else if (ws_client->get_ready_state() == WebSocketPeer::STATE_CLOSED && connected) {
			// 连接已关闭
			_on_websocket_connection_closed(0, "Connection closed", false);
		}

		// 处理接收到的数据
		while (ws_client->get_available_packet_count() > 0) {
			PackedByteArray data = ws_client->get_packet();
			_on_websocket_data_received(data);
		}
	}
}

void Live2DController::_on_audio_button_pressed() {
	// 启用音频
	enable_audio_activities();
}

void Live2DController::_on_input_area_text_submitted(const String &new_text) {
	// 发送用户输入
	send_message(new_text);
	input_area->set_text("");
}

void Live2DController::_on_dictation_checkbox_toggled(bool button_pressed) {
	// 启用/禁用听写
	enable_dictation = button_pressed;
}

void Live2DController::_on_fullscreen_checkbox_toggled(bool button_pressed) {
	// 启用/禁用全屏
	enable_full_screen = button_pressed;
	if (enable_full_screen) {
		// 使用默认窗口ID 0
		DisplayServer::get_singleton()->window_set_mode(DisplayServer::WINDOW_MODE_FULLSCREEN, 0);
	} else {
		// 使用默认窗口ID 0
		DisplayServer::get_singleton()->window_set_mode(DisplayServer::WINDOW_MODE_WINDOWED, 0);
	}
}

void Live2DController::_on_pause_dictation_checkbox_toggled(bool button_pressed) {
	// 启用/禁用在AI说话时暂停听写
	allow_pause_dictation = button_pressed;
}

void Live2DController::enable_audio_activities() {
	// 启用音频
	audio_enabled = true;
	audio_button->set_visible(false);
	UtilityFunctions::print("Audio enabled");
}

void Live2DController::send_message(const String &message) {
	// 发送消息到服务器
	if (connected && ws_client.is_valid()) {
		Dictionary data;
		data["type"] = "event";
		Dictionary event_data;
		event_data["type"] = "user_input";
		event_data["content"] = message;
		data["data"] = event_data;

		String json_str = JSON::stringify(data);
		ws_client->send_text(json_str);

		UtilityFunctions::print("Sent message: " + message);
	}
}

void Live2DController::connect_to_server(const String &url, const String &name) {
	// 连接到WebSocket服务器
	if (ws_client.is_valid()) {
		Error err = ws_client->connect_to_url(url);
		if (err == OK) {
			UtilityFunctions::print("Connecting to server: " + url);
		} else {
			UtilityFunctions::print("Failed to connect to server: " + String::num(err));
		}
	}
}

void Live2DController::start_audio_stream() {
	// 开始音频流
	if (stream_audio_player) {
		stream_audio_player->start_stream();
		UtilityFunctions::print("Audio stream started");
	}
}

int Live2DController::add_wav_data(const PackedByteArray &data) {
	// 添加WAV数据
	if (stream_audio_player) {
		return stream_audio_player->add_wav_data_raw(data);
	}
	return -1;
}

void Live2DController::wait_audio_finish(int media_id) {
	// 等待音频播放完成
	if (stream_audio_player) {
		stream_audio_player->wait_until_finish(media_id);
	}
}

void Live2DController::show_subtitle() {
	// 显示字幕
	subtitle_hidden = false;
	subtitle_container->set_visible(true);
}

void Live2DController::hide_subtitle() {
	// 隐藏字幕
	subtitle_hidden = true;
	subtitle_container->set_visible(false);
}

void Live2DController::set_expression(const String &expression_name) {
	// 设置模型表情
	// 这里需要实现表情设置逻辑
}

void Live2DController::launch_motion(const String &motion_name) {
	// 播放模型动作
	// 这里需要实现动作播放逻辑
}

void Live2DController::process_event_queue() {
	// 处理事件队列
	while (!event_queue.empty()) {
		Dictionary message = event_queue.front();
		event_queue.pop();

		// 处理消息
		if (message.has("type")) {
			String type = message["type"];

			if (type == "say_aloud") {
				// 处理说话事件
				if (message.has("content")) {
					String content = message["content"];
					subtitle_label->set_text(content);
					show_subtitle();
				}

				// 播放音频
				if (message.has("media_data") && audio_enabled) {
					PackedByteArray media_data = message["media_data"];
					if (stream_audio_player) {
						// 开始流（如果还没有开始）
						if (!stream_audio_player->get_volume()) {
							start_audio_stream();
						}
						add_wav_data(media_data);
					}
				}
			} else if (type == "bracket_tag") {
				// 处理表情标签
				if (message.has("content")) {
					String content = message["content"];
					set_expression(content);
				}
			} else if (type == "start_of_response") {
				// 响应开始
				show_subtitle();
				in_response = true;
			} else if (type == "end_of_response") {
				// 响应结束
				in_response = false;
				// 使用定时器替代set_timeout
				subtitle_label->set_text("");
				hide_subtitle();
			}
		}
	}
}