#pragma once

#include "Resource/SerialArchive.h"
#include "Core/Foundation/Json.h"

void WriteSerialJson(SerialNode& node, const json& value);
json ReadSerialJson(SerialNode& node);
