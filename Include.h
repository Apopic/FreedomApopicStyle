#pragma once
#include "Library/Include.h"
#include <filesystem>
#include <fstream>
#include <complex>
#include <thread>
#include <atomic>
#include <optional>
#include <functional>
#include <charconv>
#include <execution>
#include <unordered_set>
#include <limits>
#include <future>
#include "Library/TempoEstimate.hpp"
#include "Library/TimeStretch.hpp"
#ifndef __ANDROID__
#include <shobjidl.h>
// Library/Include.h の Input / SoundDevice マクロが Windows のヘッダーのメンバー名とぶつかるため、読み込む間だけ外す
#pragma push_macro("Input")
#pragma push_macro("SoundDevice")
#undef Input
#undef SoundDevice
#include <mmdeviceapi.h>
#include <audioclient.h>
#include "Library/PlayRecorder.hpp"
#include "Library/ToolInstaller.hpp"
#pragma pop_macro("SoundDevice")
#pragma pop_macro("Input")
#pragma comment(lib, "ole32.lib")      // フォルダ選択ダイアログ・音声デバイスの問い合わせ (COM)
#pragma comment(lib, "shell32.lib")
#else
#include "jni.h"
#endif

namespace fs = std::filesystem;
using namespace libarrier;
using namespace cppunzip;
