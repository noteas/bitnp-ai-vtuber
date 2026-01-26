# StreamAudioPlayer - C++ 音频流播放器

这是一个为 Godot 4.x 实现的音频流播放器，功能与 JavaScript 版本的 `StreamAudioPlayer.js` 保持一致。

## 功能特性

- **流式音频播放**: 支持连续添加音频数据并实时播放
- **WAV 格式支持**: 自动解析和播放 WAV 格式音频
- **多种采样率**: 支持 8-bit、16-bit、24-bit、32-bit PCM 和 32-bit float
- **自动重采样**: 自动将音频重采样到 Godot 的混音采样率
- **音量检测**: 实时计算和返回播放音量（RMS + 峰值）
- **媒体追踪**: 支持通过 media ID 追踪每个音频片段

## 类接口

### 初始化

```gdscript
var audio_player = StreamAudioPlayer.new()
add_child(audio_player)
audio_player.init()
```

### 控制方法

#### `start_stream()`

开始音频流播放。必须在添加音频数据前调用。

```gdscript
audio_player.start_stream()
```

#### `pause()`

暂停播放。

```gdscript
audio_player.pause()
```

#### `resume()`

恢复播放。

```gdscript
audio_player.resume()
```

#### `stop()`

停止播放并清空缓冲区。

```gdscript
audio_player.stop()
```

### 添加音频数据

#### `add_wav_data(base64_wav_data: PackedByteArray) -> int`

添加 Base64 编码的 WAV 音频数据。返回 media ID。

```gdscript
var wav_base64 = "data:audio/wav;base64,UklGR..."  # Base64 编码的 WAV
var media_id = audio_player.add_wav_data(wav_base64.to_utf8_buffer())
```

#### `add_wav_data_raw(wav_data: PackedByteArray) -> int`

直接添加 WAV 音频数据（原始字节）。返回 media ID。

```gdscript
var wav_bytes = FileAccess.get_file_as_bytes("audio.wav")
var media_id = audio_player.add_wav_data_raw(wav_bytes)
```

### 获取播放信息

#### `get_total_duration() -> float`

获取总音频时长（秒）。

```gdscript
var duration = audio_player.get_total_duration()
print("Total duration: ", duration, " seconds")
```

#### `get_remaining_duration() -> float`

获取剩余播放时长（秒）。

```gdscript
var remaining = audio_player.get_remaining_duration()
```

#### `get_current_time() -> float`

获取当前播放位置（秒）。

```gdscript
var current = audio_player.get_current_time()
```

#### `get_volume() -> float`

获取当前播放音量（0.0 - 1.0+）。

```gdscript
var volume = audio_player.get_volume()
# 可用于口型同步等功能
```

### 等待播放完成

#### `wait_until_finish(media_id: int)`

等待特定音频片段播放完成。

```gdscript
var media_id = audio_player.add_wav_data_raw(wav_data)
audio_player.wait_until_finish(media_id)
print("Audio finished playing")
```

## 在 Live2DController 中使用

`StreamAudioPlayer` 已集成到 `Live2DController` 中：

```cpp
// 在 Live2DController 内部
void Live2DController::_ready() {
    // ...
    stream_audio_player = memnew(StreamAudioPlayer);
    add_child(stream_audio_player);
    stream_audio_player->init();
}

// 开始音频流
void Live2DController::start_audio_stream() {
    if (stream_audio_player) {
        stream_audio_player->start_stream();
    }
}

// 添加音频数据
int Live2DController::add_wav_data(const PackedByteArray& data) {
    if (stream_audio_player) {
        return stream_audio_player->add_wav_data_raw(data);
    }
    return -1;
}

// 在 _process 中使用音量进行口型同步
void Live2DController::_process(double delta) {
    if (stream_audio_player && stream_audio_player->get_volume() > 0) {
        volume = stream_audio_player->get_volume();
        // 使用 volume 控制 Live2D 模型的口型参数
    }
}
```

## 实现细节

### WAV 解析

- 支持标准 RIFF WAV 格式
- 解析 fmt 和 data 区块
- 支持多种位深度和采样率

### 音频处理

- 使用 Godot 的 `AudioStreamGenerator` 进行流式播放
- 使用 `AudioStreamGeneratorPlayback` 推送音频帧
- 自动进行线性插值重采样

### 音量计算

- 结合 RMS（均方根）和峰值计算
- 应用平滑因子避免音量跳变
- 实时更新，可用于口型同步

## 与 JavaScript 版本的对应关系

| JavaScript 方法 | C++ 方法 | 说明 |
| ---------------- | ---------- | ------ |
| `init()` | `init()` | 初始化音频上下文 |
| `startStream()` | `start_stream()` | 开始流播放 |
| `addWavData()` | `add_wav_data()` | 添加 Base64 WAV 数据 |
| - | `add_wav_data_raw()` | 添加原始 WAV 数据 |
| `waitUntilFinish()` | `wait_until_finish()` | 等待播放完成 |
| `getTotalDuration()` | `get_total_duration()` | 获取总时长 |
| `getRemainingDuration()` | `get_remaining_duration()` | 获取剩余时长 |
| `getCurrentTime()` | `get_current_time()` | 获取当前时间 |
| `volume` 属性 | `get_volume()` | 获取音量 |
| `pause()` | `pause()` | 暂停播放 |
| `resume()` | `resume()` | 恢复播放 |
| `stop()` | `stop()` | 停止播放 |

## 注意事项

1. **必须先调用 `init()`**: 在使用任何其他方法前必须先初始化
2. **必须先调用 `start_stream()`**: 在添加音频数据前必须先开始流
3. **线程安全**: 当前实现不是线程安全的，所有操作应在主线程中进行
4. **阻塞式等待**: `wait_until_finish()` 是阻塞式的，在生产环境中应考虑使用信号机制

## 编译

该类已集成到项目构建系统中。运行以下命令编译：

```bash
scons platform=windows custom_api_file=extension_api.json
```

编译成功后，DLL 文件会生成在 `bin/` 目录下。
