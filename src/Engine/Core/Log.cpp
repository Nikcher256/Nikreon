#include "Engine/Core/Log.hpp"

#include <spdlog/spdlog.h>

namespace Engine {

void Log::init()
{
    spdlog::set_pattern("[%T] [%^%l%$] %v");
    spdlog::info("Logger initialized.");
}

void Log::shutdown()
{
    spdlog::info("Logger shutdown.");
    spdlog::shutdown();
}

} // namespace Engine
