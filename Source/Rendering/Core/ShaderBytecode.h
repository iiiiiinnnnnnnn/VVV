#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace Gpu
{
class ShaderBytecode
{
  public:
	void Load(const char* filename);
	
	const uint8_t* GetData() const
	{
		return bytes.data();
	}
	size_t GetSize() const
	{
		return bytes.size();
	}

  private:
	std::vector<uint8_t> bytes;
};
} // namespace Gpu
