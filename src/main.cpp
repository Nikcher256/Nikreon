#include "Engine/Core/Application.hpp"
#include "Engine/Core/Log.hpp"

#include <cstring>
#include <exception>

#include <spdlog/spdlog.h>

int main(int argc, char** argv)
{
    Engine::Log::init();

    try {
        Engine::RunOptions runOptions;
        for (int index = 1; index < argc; ++index) {
            if (std::strcmp(argv[index], "--smoke-test") == 0) {
                runOptions.maxFrames = 1;
            }
        }

        Engine::Application app;
        app.run(runOptions);
    } catch (const std::exception& error) {
        spdlog::critical("Fatal error: {}", error.what());
        Engine::Log::shutdown();
        return 1;
    }

    Engine::Log::shutdown();
    return 0;
}
