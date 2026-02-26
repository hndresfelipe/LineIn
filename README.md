# LineIn

Real-time audio passthrough app for Android. Connect your guitar (or any audio source) via USB audio interface and hear it through your phone's output with minimal latency.

## Features

- **Low-latency audio passthrough** using [Oboe](https://github.com/google/oboe) (C++/NDK)
- **MMAP support** for the lowest possible latency on compatible devices
- **Volume control** with adjustable gain (0.5x - 10x)
- **Latency tuning** with configurable buffer targets and drain speed
- **Presets** for quick latency configuration (Off / Low / Safe)
- **Real-time audio status** showing MMAP mode, latency estimates, and buffer levels
- **Foreground service** to keep audio running in the background
- **Soft clipping** to prevent audio distortion at high gain levels

## Requirements

- Android 16 (API 36) or higher
- USB audio interface (recommended) or built-in microphone
- `RECORD_AUDIO` permission

## Tech Stack

- **Language:** Kotlin
- **UI:** Jetpack Compose + Material 3
- **Audio Engine:** Oboe 1.9.3 (C++/NDK)
- **Architecture:** MVVM
- **Build:** Gradle with version catalogs

## Building

1. Clone the repository:
   ```bash
   git clone https://github.com/hndresfelipe/LineIn.git
   ```

2. Open the project in Android Studio (Ladybug or newer recommended).

3. Make sure you have the NDK and CMake installed via SDK Manager.

4. Build and run on a device with API 36+.

## Project Structure

```
app/src/main/
├── cpp/                          # Native audio engine (Oboe)
│   ├── CMakeLists.txt
│   ├── PassthroughEngine.cpp/h   # Full-duplex audio processing
│   ├── FullDuplexPass.h          # Audio passthrough callback
│   └── jni_bridge.cpp            # JNI bindings
├── java/.../linein/
│   ├── MainActivity.kt           # UI (Compose)
│   ├── AudioPassthroughService.kt# Foreground service
│   └── PassthroughEngine.kt      # JNI wrapper
└── res/
```

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
