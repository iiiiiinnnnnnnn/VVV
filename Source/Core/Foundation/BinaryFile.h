#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <utility>
#include <vector>

class BinaryFile
{
  public:
	static BinaryFile Read(const std::filesystem::path& path,
		size_t maxSize = std::numeric_limits<size_t>::max())
	{
		BinaryFile result;
		std::ifstream file(path, std::ios::binary | std::ios::ate);
		if (!file) return result;
		const std::streamoff size = file.tellg();
		if (size < 0 || static_cast<std::uintmax_t>(size) > maxSize) return result;
		result.data.resize(static_cast<size_t>(size));
		file.seekg(0, std::ios::beg);
		if (size && !file.read(reinterpret_cast<char*>(result.data.data()), static_cast<std::streamsize>(result.data.size())))
		{
			result.data.clear();
			return result;
		}
		result.valid = true;
		return result;
	}

	bool IsValid() const { return valid; }
	const std::vector<uint8_t>& GetData() const { return data; }
	size_t GetSize() const { return data.size(); }
	std::vector<uint8_t> TakeData()
	{
		valid = false;
		return std::exchange(data, {});
	}

  private:
	std::vector<uint8_t> data;
	bool valid = false;
};
