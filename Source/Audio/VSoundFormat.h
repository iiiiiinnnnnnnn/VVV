#pragma once

#include <cstdint>

namespace VSound
{
constexpr uint32_t Magic = 0x444E5356; // VSND
constexpr uint32_t Version = 1;
constexpr const char* Extension = ".vsnd";

struct Entry
{
	int32_t variant = 0;
	uint32_t uncompressedSize = 0;
	uint32_t compressedSize = 0;
	uint64_t offset = 0;
};
}
