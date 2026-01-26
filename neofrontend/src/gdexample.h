#pragma once

#include <godot_cpp/classes/audio_stream_player.hpp>
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/camera2d.hpp>
#include <godot_cpp/classes/canvas_item.hpp>
#include <godot_cpp/classes/check_box.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/global_constants.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/json.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/line_edit.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/texture_rect.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/classes/web_socket_peer.hpp>
#include <godot_cpp/core/error_macros.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/typed_array.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <CubismFramework.hpp>
#include <ICubismModelSetting.hpp>
#include <Live2DCubismCore.hpp>
#include <Model/CubismUserModel.hpp>

#include "audio/stream_audio_player.h"

#include <map>
#include <memory>
#include <queue>
#include <string>

namespace godot {

class Live2DController : public Node {
	GDCLASS(Live2DController, Node)

private:
	// Live2D相关
	Csm::CubismFramework *cubism_framework;
	Csm::CubismUserModel *model;
	String model_url;
	Viewport *viewport;
	Camera2D *camera;
	TextureRect *model_display;

	// UI元素
	LineEdit *input_area;
	Button *audio_button;
	Label *subtitle_label;
	Control *subtitle_container;
	Control *config_ui;
	CheckBox *dictation_checkbox;
	CheckBox *fullscreen_checkbox;
	CheckBox *pause_dictation_checkbox;

	// 音频相关
	AudioStreamPlayer *audio_player;
	StreamAudioPlayer *stream_audio_player;
	bool audio_enabled;
	float volume;

	// WebSocket相关
	Ref<WebSocketPeer> ws_client;
	String server_url;
	String agent_name;
	bool connected;

	// 状态管理
	String input_text;
	bool show_config_ui;
	bool enable_dictation;
	bool enable_full_screen;
	bool allow_pause_dictation;
	bool subtitle_hidden;

	// 事件队列
	std::queue<Dictionary> event_queue;
	bool in_response;

	// 口型同步
	Callable lip_sync_func;

	// 初始化Live2D框架
	void initialize_cubism();

	// 加载Live2D模型
	void load_model(const String &model_path);

	// 处理WebSocket消息
	void _on_websocket_connected_to_server();
	void _on_websocket_connection_closed(int code, String reason, bool was_clean);
	void _on_websocket_connection_error();
	void _on_websocket_data_received(PackedByteArray data);
	void _on_websocket_server_close_request(int code, String reason);
	void _process_websocket_events();

	// UI事件处理
	void _on_audio_button_pressed();
	void _on_input_area_text_submitted(const String &new_text);
	void _on_dictation_checkbox_toggled(bool button_pressed);
	void _on_fullscreen_checkbox_toggled(bool button_pressed);
	void _on_pause_dictation_checkbox_toggled(bool button_pressed);

	// 音频流播放
	void start_audio_stream();
	int add_wav_data(const PackedByteArray &data);
	void wait_audio_finish(int media_id);

	// 字幕处理
	void show_subtitle();
	void hide_subtitle();

	// 表情和动作处理
	void set_expression(const String &expression_name);
	void launch_motion(const String &motion_name);

	// 事件队列处理
	void process_event_queue();

protected:
	static void _bind_methods();

public:
	Live2DController();
	~Live2DController();

	void _ready() override;
	void _process(double delta) override;
	void _input(const Ref<InputEvent> &event) override;

	// 公共方法
	void enable_audio_activities();
	void send_message(const String &message);
	void connect_to_server(const String &url, const String &name);
};

} // namespace godot