#include "Core/Foundation/BinaryFile.h"
#include <array>
#include <cassert>
#include <iostream>

int main(int argc, char** argv)
{
	assert(argc == 2);
	const std::filesystem::path directory(argv[1]);
	std::filesystem::create_directories(directory);
	const auto path = directory / "binary-file-test.bin";
	const auto emptyPath = directory / "binary-file-empty.bin";
	const std::array<uint8_t, 4> expected{0, 1, 127, 255};
	{
		std::ofstream output(path, std::ios::binary);
		output.write(reinterpret_cast<const char*>(expected.data()), expected.size());
		assert(output);
	}
	{
		std::ofstream output(emptyPath, std::ios::binary);
		assert(output);
	}
	auto file = BinaryFile::Read(path);
	assert(file.IsValid());
	assert(file.GetSize() == expected.size());
	assert(file.GetData() == std::vector<uint8_t>(expected.begin(), expected.end()));
	assert(!BinaryFile::Read(path, expected.size() - 1).IsValid());
	assert(!BinaryFile::Read(directory / "does-not-exist.bin").IsValid());
	const auto empty = BinaryFile::Read(emptyPath);
	assert(empty.IsValid() && !empty.GetSize());
	const auto transferred = file.TakeData();
	assert(transferred.size() == expected.size());
	assert(!file.IsValid() && !file.GetSize());
	std::filesystem::remove(path);
	std::filesystem::remove(emptyPath);
	std::cout << "BinaryFile tests passed.\n";
}
