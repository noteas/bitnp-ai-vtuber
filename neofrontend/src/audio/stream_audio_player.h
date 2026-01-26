#pragma once

#include <godot_cpp/classes/audio_stream_generator.hpp>
#include <godot_cpp/classes/audio_stream_generator_playback.hpp>
#include <godot_cpp/classes/audio_stream_player.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/utility_functions.hpp>


#include <cmath>
#include <map>
#include <queue>
#include <vector>


namespace godot {

// 媒体信息结构
struct MediaInfo {
	std::vector<float> data;
	int64_t timestamp;
	double end_time;
};

class StreamAudioPlayer : public Node {
	GDCLASS(StreamAudioPlayer, Node)

private:
	// 音频播放器
	AudioStreamPlayer *audio_player;
	Ref<AudioStreamGenerator> stream;
	Ref<AudioStreamGeneratorPlayback> playback;

	// 音频缓冲区
	std::vector<float> audio_buffer;
	int64_t buffer_position;

	// 配置参数
	int expected_sample_rate;
	int num_channels;
	bool is_playing;
	bool is_streaming;

	// 媒体管理
	int media_id_counter;
	std::map<int, MediaInfo> media_map;

	// 音量计算
	float volume;
	int64_t last_volume_update_time;

	// WAV 解码辅助方法
	std::vector<float> decode_wav_data(const PackedByteArray &wav_data);
	std::vector<float> extract_audio_data(const uint8_t *data, int offset, int size, int num_channels, int bits_per_sample);
	std::vector<float> resample_audio_data(const std::vector<float> &audio_data, int original_sample_rate, int target_sample_rate);
	void append_audio_data(const std::vector<float> &float32_array);

	// 音量计算
	void update_volume_from_data(const std::vector<float> &audio_data);

	// 音频处理循环
	void fill_audio_buffer();

	// 辅助函数
	uint16_t read_uint16_le(const uint8_t *data, int offset);
	uint32_t read_uint32_le(const uint8_t *data, int offset);
	int16_t read_int16_le(const uint8_t *data, int offset);
	float read_float32_le(const uint8_t *data, int offset);

protected:
	static void _bind_methods();

public:
	StreamAudioPlayer();
	~StreamAudioPlayer();

	void _ready() override;
	void _process(double delta) override;

	// 初始化音频上下文
	bool init();

	// 控制方法
	void start_stream();
	void pause();
	void resume();
	void stop();

	// 添加音频数据
	int add_wav_data(const PackedByteArray &base64_wav_data);
	int add_wav_data_raw(const PackedByteArray &wav_data);

	// 等待播放完成
	void wait_until_finish(int media_id);

	// 获取播放信息
	double get_total_duration() const;
	double get_remaining_duration() const;
	double get_current_time() const;
	float get_volume() const { return volume; }
};

} // namespace godot
