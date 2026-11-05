#pragma once
#include "Resource/SerialArchive.h"

class SerialViewer
{
public:
	bool Draw(SerialDocument& document, bool* open);
private:
	bool DrawNode(SerialNode& node, size_t depth, std::vector<uint64_t>& path, bool arrayElement);
	std::vector<uint64_t> selected;
	size_t bytePage = 0;
};
