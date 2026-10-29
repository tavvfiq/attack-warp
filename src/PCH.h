// Precompiled header: CommonLib, SKSE, spdlog and Win32 pieces shared by every Attack Warp source file.
#pragma once

#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>

#include <spdlog/sinks/basic_file_sink.h>

#include <Windows.h>

#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <memory>
#include <mutex>
#include <numbers>
#include <shared_mutex>
#include <unordered_map>
#include <format>
#include <string>

using namespace std::literals;

namespace logger = SKSE::log;
