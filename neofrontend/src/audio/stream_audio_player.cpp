#include "stream_audio_player.h"
#include <godot_cpp/classes/audio_server.hpp>
#include <godot_cpp/classes/marshalls.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/core/class_db.hpp>


#include <algorithm>
#include <cstring>


using namespace godot;

void StreamAudioPlayer::_bind_methods() {
	// 绑定方法
	ClassDB::bind_method(D_METHOD("init"), &StreamAudioPlayer::init);
	ClassDB::bind_method(D_METHOD("start_stream"), &StreamAudioPlayer::start_stream);
	ClassDB::bind_method(D_METHOD("pause"), &StreamAudioPlayer::pause);
	ClassDB::bind_method(D_METHOD("resume"), &StreamAudioPlayer::resume);
	ClassDB::bind_method(D_METHOD("stop"), &StreamAudioPlayer::stop);
	ClassDB::bind_method(D_METHOD("add_wav_data", "base64_wav_data"), &StreamAudioPlayer::add_wav_data);
	ClassDB::bind_method(D_METHOD("add_wav_data_raw", "wav_data"), &StreamAudioPlayer::add_wav_data_raw);
	ClassDB::bind_method(D_METHOD("wait_until_finish", "media_id"), &StreamAudioPlayer::wait_until_finish);
	ClassDB::bind_method(D_METHOD("get_total_duration"), &StreamAudioPlayer::get_total_duration);
	ClassDB::bind_method(D_METHOD("get_remaining_duration"), &StreamAudioPlayer::get_remaining_duration);
	ClassDB::bind_method(D_METHOD("get_current_time"), &StreamAudioPlayer::get_current_time);
	ClassDB::bind_method(D_METHOD("get_volume"), &StreamAudioPlayer::get_volume);
}

StreamAudioPlayer::StreamAudioPlayer() {
	audio_player = nullptr;
	buffer_position = 0;
	expected_sample_rate = 24000;
	num_channels = 1;
	is_playing = false;
	is_streaming = false;
	media_id_counter = 0;
	volume = 0.0f;
	last_volume_update_time = 0;
}

StreamAudioPlayer::~StreamAudioPlayer() {
	stop();
}

void StreamAudioPlayer::_ready() {
	// 创建音频播放器
	audio_player = memnew(AudioStreamPlayer);
	add_child(audio_player);
}

void StreamAudioPlayer::_process(double delta) {
	if (!is_playing || !is_streaming) {
		return;
	}

	// 填充音频缓冲区
	fill_audio_buffer();
}

bool StreamAudioPlayer::init() {
	// 创建音频流生成器
	stream.instantiate();

	// 获取音频服务器的采样率
	int mix_rate = AudioServer::get_singleton()->get_mix_rate();

	stream->set_mix_rate(mix_rate);
	stream->set_buffer_length(0.1f); // 100ms 缓冲

	audio_player->set_stream(stream);

	UtilityFunctions::print("StreamAudioPlayer initialized, mix rate: ", mix_rate);
	return true;
}

void StreamAudioPlayer::start_stream() {
	if (is_streaming) {
		UtilityFunctions::print("Stream is already playing");
		return;
	}

	is_streaming = true;
	is_playing = true;

	// 开始播放
	audio_player->play();

	// 获取播放器
	playback = audio_player->get_stream_playback();

	UtilityFunctions::print("Stream started");
}

void StreamAudioPlayer::fill_audio_buffer() {
	if (!playback.is_valid()) {
		return;
	}

	// 获取可用的帧数
	int frames_available = playback->get_frames_available();

	if (frames_available <= 0) {
		return;
	}

	// 准备音频数据
	PackedVector2Array frames;
	frames.resize(frames_available);

	for (int i = 0; i < frames_available; i++) {
		float sample = 0.0f;

		// 从缓冲区读取样本
		if (buffer_position < (int64_t)audio_buffer.size()) {
			sample = audio_buffer[buffer_position];
			buffer_position++;
		}

		// 单声道转立体声
		frames[i] = Vector2(sample, sample);
	}

	// 推送帧到播放器
	playback->push_buffer(frames);

	// 更新音量
	std::vector<float> samples(frames_available);
	for (int i = 0; i < frames_available; i++) {
		samples[i] = frames[i].x;
	}
	update_volume_from_data(samples);
}

int StreamAudioPlayer::add_wav_data(const PackedByteArray &base64_wav_data) {
	if (!is_streaming) {
		UtilityFunctions::print("Stream not started. Call start_stream() first.");
		return -1;
	}

	// Base64 解码
	String base64_str;
	for (int i = 0; i < base64_wav_data.size(); i++) {
		base64_str += String::chr(base64_wav_data[i]);
	}

	// 移除 data URL 前缀（如果存在）
	if (base64_str.begins_with("data:audio/wav;base64,")) {
		base64_str = base64_str.substr(22);
	}

	// 解码 Base64
	PackedByteArray wav_data = Marshalls::get_singleton()->base64_to_raw(base64_str);

	return add_wav_data_raw(wav_data);
}

int StreamAudioPlayer::add_wav_data_raw(const PackedByteArray &wav_data) {
	if (!is_streaming) {
		UtilityFunctions::print("Stream not started. Call start_stream() first.");
		return -1;
	}

	try {
		int media_id = ++media_id_counter;

		// 解析 WAV 数据
		std::vector<float> audio_data = decode_wav_data(wav_data);

		// 添加到缓冲区
		append_audio_data(audio_data);

		// 保存媒体信息
		MediaInfo info;
		info.data = audio_data;
		info.timestamp = Time::get_singleton()->get_ticks_msec();
		info.end_time = get_total_duration();
		media_map[media_id] = info;

		return media_id;
	} catch (...) {
		UtilityFunctions::print("Failed to add WAV data");
		return -1;
	}
}

void StreamAudioPlayer::wait_until_finish(int media_id) {
	auto it = media_map.find(media_id);
	if (it == media_map.end()) {
		UtilityFunctions::print("Media ID not found: ", media_id);
		return;
	}

	double end_time = it->second.end_time;

	UtilityFunctions::print("[DEBUG] waiting media until finish: ", media_id,
		" curr_time ", get_current_time(),
		" end_time ", end_time);

	// 注意：这是一个阻塞式等待，在实际应用中可能需要改为异步实现
	while (get_current_time() < end_time) {
		// 在 Godot 中，通常使用 yield/await 或信号机制
		// 这里简化处理
	}
}

std::vector<float> StreamAudioPlayer::decode_wav_data(const PackedByteArray &wav_data) {
	if (wav_data.size() < 44) {
		UtilityFunctions::print("WAV data too small");
		return std::vector<float>();
	}

	const uint8_t *data = wav_data.ptr();

	// 检查 RIFF 头
	if (data[0] != 'R' || data[1] != 'I' || data[2] != 'F' || data[3] != 'F') {
		UtilityFunctions::print("Not a valid WAV file");
		return std::vector<float>();
	}

	// 解析 WAV 格式
	int offset = 12;
	std::vector<float> audio_data;
	int sample_rate = 24000;
	int wav_num_channels = 1;
	int bits_per_sample = 16;

	while (offset < wav_data.size()) {
		if (offset + 8 > wav_data.size()) {
			break;
		}

		// 读取 chunk ID
		char chunk_id[5] = { 0 };
		chunk_id[0] = data[offset];
		chunk_id[1] = data[offset + 1];
		chunk_id[2] = data[offset + 2];
		chunk_id[3] = data[offset + 3];

		uint32_t chunk_size = read_uint32_le(data, offset + 4);

		if (strcmp(chunk_id, "fmt ") == 0) {
			// 解析 fmt 区块
			uint16_t audio_format = read_uint16_le(data, offset + 8);
			wav_num_channels = read_uint16_le(data, offset + 10);
			sample_rate = read_uint32_le(data, offset + 12);
			bits_per_sample = read_uint16_le(data, offset + 22);

			UtilityFunctions::print("WAV Info: format=", audio_format,
				", channels=", wav_num_channels,
				", sampleRate=", sample_rate,
				", bits=", bits_per_sample);

			if (audio_format != 1) {
				UtilityFunctions::print("Only PCM WAV format is supported");
				return std::vector<float>();
			}
		} else if (strcmp(chunk_id, "data") == 0) {
			// 找到 data 区块
			int data_offset = offset + 8;
			int data_size = chunk_size;

			// 提取音频数据
			audio_data = extract_audio_data(data, data_offset, data_size, wav_num_channels, bits_per_sample);

			// 重采样
			int mix_rate = AudioServer::get_singleton()->get_mix_rate();
			if (sample_rate != mix_rate) {
				UtilityFunctions::print("Resampling from ", sample_rate, "Hz to ", mix_rate, "Hz");
				audio_data = resample_audio_data(audio_data, sample_rate, mix_rate);
			}

			break;
		}

		offset += 8 + chunk_size;
	}

	return audio_data;
}

std::vector<float> StreamAudioPlayer::extract_audio_data(const uint8_t *data, int offset, int size,
	int num_channels, int bits_per_sample) {
	int bytes_per_sample = bits_per_sample / 8;
	int total_samples = size / bytes_per_sample;

	std::vector<float> float_data(total_samples);

	if (bits_per_sample == 16) {
		// 16-bit PCM
		for (int i = 0; i < total_samples; i++) {
			int16_t sample = read_int16_le(data, offset + i * 2);
			float_data[i] = sample / 32768.0f;
		}
	} else if (bits_per_sample == 8) {
		// 8-bit PCM
		for (int i = 0; i < total_samples; i++) {
			uint8_t sample = data[offset + i];
			float_data[i] = (sample - 128) / 128.0f;
		}
	} else if (bits_per_sample == 24) {
		// 24-bit PCM
		for (int i = 0; i < total_samples; i++) {
			int32_t sample = (int8_t)data[offset + i * 3 + 2] << 16 |
				data[offset + i * 3 + 1] << 8 |
				data[offset + i * 3];
			float_data[i] = sample / 8388608.0f;
		}
	} else if (bits_per_sample == 32) {
		// 32-bit float
		for (int i = 0; i < total_samples; i++) {
			float_data[i] = read_float32_le(data, offset + i * 4);
		}
	}

	return float_data;
}

std::vector<float> StreamAudioPlayer::resample_audio_data(const std::vector<float> &audio_data,
	int original_sample_rate,
	int target_sample_rate) {
	if (original_sample_rate == target_sample_rate) {
		return audio_data;
	}

	float ratio = (float)target_sample_rate / (float)original_sample_rate;
	int original_length = audio_data.size();
	int target_length = (int)(original_length * ratio);

	std::vector<float> resampled_data(target_length);

	// 简单线性插值重采样
	for (int i = 0; i < target_length; i++) {
		float original_index = i / ratio;
		int index1 = (int)original_index;
		int index2 = std::min(index1 + 1, original_length - 1);
		float weight = original_index - index1;

		if (index1 == index2) {
			resampled_data[i] = audio_data[index1];
		} else {
			resampled_data[i] = audio_data[index1] * (1 - weight) + audio_data[index2] * weight;
		}
	}

	UtilityFunctions::print("Resampled: ", original_length, " samples -> ", target_length, " samples");
	return resampled_data;
}

void StreamAudioPlayer::append_audio_data(const std::vector<float> &float32_array) {
	audio_buffer.insert(audio_buffer.end(), float32_array.begin(), float32_array.end());

	UtilityFunctions::print("Audio buffer size: ", audio_buffer.size(),
		" samples, duration: ", get_total_duration(), "s");
}

void StreamAudioPlayer::update_volume_from_data(const std::vector<float> &audio_data) {
	if (audio_data.empty()) {
		return;
	}

	// 计算 RMS 音量
	float sum = 0.0f;
	for (float sample : audio_data) {
		sum += sample * sample;
	}
	float rms = std::sqrt(sum / audio_data.size());

	// 计算峰值
	float peak = 0.0f;
	for (float sample : audio_data) {
		float abs_value = std::abs(sample);
		if (abs_value > peak) {
			peak = abs_value;
		}
	}

	// 使用 RMS 和峰值的综合值
	float raw_volume = std::max(rms, peak * 0.7f) * 10.0f;

	// 平滑处理
	float smoothing_factor = 0.5f;
	volume = smoothing_factor * raw_volume + (1.0f - smoothing_factor) * volume;
}

double StreamAudioPlayer::get_total_duration() const {
	if (audio_buffer.empty()) {
		return 0.0;
	}
	int mix_rate = AudioServer::get_singleton()->get_mix_rate();
	return (double)audio_buffer.size() / mix_rate;
}

double StreamAudioPlayer::get_remaining_duration() const {
	if (audio_buffer.empty()) {
		return 0.0;
	}
	int64_t remaining_samples = audio_buffer.size() - buffer_position;
	int mix_rate = AudioServer::get_singleton()->get_mix_rate();
	return (double)remaining_samples / mix_rate;
}

double StreamAudioPlayer::get_current_time() const {
	if (audio_buffer.empty() || buffer_position == 0) {
		return 0.0;
	}
	int mix_rate = AudioServer::get_singleton()->get_mix_rate();
	return (double)buffer_position / mix_rate;
}

void StreamAudioPlayer::pause() {
	is_playing = false;
	if (audio_player) {
		audio_player->set_stream_paused(true);
	}
}

void StreamAudioPlayer::resume() {
	is_playing = true;
	if (audio_player) {
		audio_player->set_stream_paused(false);
	}
}

void StreamAudioPlayer::stop() {
	is_playing = false;
	is_streaming = false;
	buffer_position = 0;

	if (audio_player && audio_player->is_playing()) {
		audio_player->stop();
	}

	audio_buffer.clear();
	media_map.clear();
}

// 辅助函数
uint16_t StreamAudioPlayer::read_uint16_le(const uint8_t *data, int offset) {
	return (uint16_t)data[offset] | ((uint16_t)data[offset + 1] << 8);
}

uint32_t StreamAudioPlayer::read_uint32_le(const uint8_t *data, int offset) {
	return (uint32_t)data[offset] |
		((uint32_t)data[offset + 1] << 8) |
		((uint32_t)data[offset + 2] << 16) |
		((uint32_t)data[offset + 3] << 24);
}

int16_t StreamAudioPlayer::read_int16_le(const uint8_t *data, int offset) {
	return (int16_t)read_uint16_le(data, offset);
}

float StreamAudioPlayer::read_float32_le(const uint8_t *data, int offset) {
	uint32_t bits = read_uint32_le(data, offset);
	float value;
	std::memcpy(&value, &bits, sizeof(float));
	return value;
}
