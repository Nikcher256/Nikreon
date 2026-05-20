#pragma once

#include <chrono>

namespace Engine {

class Time {
public:
    void reset();
    void tick();

    [[nodiscard]] float deltaSeconds() const;
    [[nodiscard]] double elapsedSeconds() const;

private:
    using Clock = std::chrono::steady_clock;

    Clock::time_point m_startTime{Clock::now()};
    Clock::time_point m_lastFrameTime{m_startTime};
    float m_deltaSeconds{0.0f};
};

} // namespace Engine
