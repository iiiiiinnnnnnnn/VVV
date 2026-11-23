#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cwctype>
#include <cstdlib>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

#include "Core/Foundation/Json.h"

#include <Windows.h>
#include <assert.h>
#include <tchar.h>
#include <d3d11.h>
#include <wrl.h>
#include <PxPhysicsAPI.h>
#include <imgui.h>
#include <imgui_stdlib.h>

#include <Dx11Kit/Dx11Kit.h>
#include <magic_enum/magic_enum_all.hpp>
#include "Core/Foundation/Random.h"
#include "Core/Foundation/StrUtils.h"
#include "Core/Foundation/BinaryFile.h"
#include "Core/Foundation/Logger.h"
