#include "Core/Foundation/Logger.h"
#include <cassert>
#include <thread>
#include <vector>
#include <iostream>
int main()
{
    std::filesystem::current_path("Obj");
    Logger::Initialize();
    const auto path = Logger::GetPath();
    Logger::Initialize();
    assert(path == Logger::GetPath());
    int evaluated = 0;
    const int line = __LINE__ + 1;
    Log("logger-test %d", ++evaluated);
#if defined(_DEBUG) || defined(VVV_DEVELOPMENT)
    assert(evaluated == 1);
    assert(path.parent_path().filename() == "Log");
    assert(std::filesystem::exists(path.parent_path().parent_path() / "VEER.sln"));
    std::vector<std::thread> threads;
    for (int i = 0; i < 4; ++i) threads.emplace_back([] { for (int n = 0; n < 25; ++n) Log("worker %d", n); });
    for (auto& thread : threads) thread.join();
    std::ifstream input(path);
    const std::string content((std::istreambuf_iterator<char>(input)), {});
    assert(content.find("LoggerTests.cpp:" + std::to_string(line)) != std::string::npos);
    assert(content.find("logger-test 1") != std::string::npos);
    int workers = 0; size_t offset = 0;
    while ((offset = content.find("worker ", offset)) != std::string::npos) { ++workers; offset += 7; }
    assert(workers == 100);
#else
    assert(!evaluated && path.empty());
#endif
    std::cout << "Logger tests passed.\n";
}