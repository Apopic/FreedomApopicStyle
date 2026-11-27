#pragma once
// Explicit dependencies of the Game interface; do not depend on a local Include.h.
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <charconv>
#include <chrono>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <deque>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <future>
#include <iomanip>
#include <iterator>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <numeric>
#include <optional>
#include <ranges>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

// Socket.h must precede the Windows API headers (Winsock include order).
#include "Library/Net.hpp"
#include "Platform/Gfx.hpp"
#include "Platform/Text.hpp"
#include "Platform/Audio.hpp"
#include "Platform/Sys.hpp"
#include "Platform/Movie.hpp"
#ifndef __ANDROID__
#include <windows.h>
#include <windowsx.h>
#include <shobjidl.h>
#include <shellapi.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <execution>

#endif
#include "libarrier/timer.hpp"
#include "magic_enum/magic_enum.hpp"
#include "Library/AddStruct.hpp"
#include "Library/Easing.hpp"
#include "Library/ExString.hpp"
#include "Library/ExMath.hpp"
#include "Library/Input.hpp"
#include "Library/DiscordRpc.hpp"
#include "Library/TjaDocument.hpp"
#include "Library/SoundDevice.hpp"
#include "Library/ChartCatalog.hpp"
#include "Library/SkinCatalog.hpp"
#include "Library/EditorForms.hpp"
#include "Library/TempoEstimate.hpp"
#include "Library/AudioAlignment.hpp"
#include "Library/TimeStretch.hpp"
#include "Library/RecordingAudio.hpp"
#ifndef __ANDROID__
#include "Library/PlayRecorder.hpp"
#include "Library/ToolInstaller.hpp"
#else
#include "Library/AndroidBridge.hpp"
#endif

namespace fs = std::filesystem;
