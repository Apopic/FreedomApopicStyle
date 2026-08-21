#pragma once
#include "Library/Include.h"
#include <filesystem>
#include <fstream>
#include <complex>
#include <thread>
#include <mutex>
#ifndef __ANDROID__
#include <shobjidl.h>
#else
#include "jni.h"
#endif

namespace fs = std::filesystem;
using namespace libarrier;
using namespace cppunzip;
