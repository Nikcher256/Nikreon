#include "Engine/Core/Time.hpp"

#include <chrono>

namespace Engine {

void Time::reset()
{
    m_startTime = Clock::now();
    m_lastFrameTime = m_startTime;
    m_deltaSeconds = 0.0f;
}

void Time::tick()
{
    const auto now = Clock::now();
    m_deltaSeconds = std::chrono::duration<float>(now - m_lastFrameTime).count();
    m_lastFrameTime = now;
}

float Time::deltaSeconds() const
{
    return m_deltaSeconds;
}

double Time::elapsedSeconds() const
{
    return std::chrono::duration<double>(Clock::now() - m_startTime).count();
}

} // namespace Engine
