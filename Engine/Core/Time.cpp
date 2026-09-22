#include "Time.h"

#include <chrono>

Time::Time()
    : m_LastTime(0.0),
    m_DeltaTime(0.0f)
{
    m_LastTime = std::chrono::duration<double>(
        std::chrono::steady_clock::now().time_since_epoch()
    ).count();
}

void Time::Update()
{
    double currentTime = std::chrono::duration<double>(
        std::chrono::steady_clock::now().time_since_epoch()
    ).count();

    m_DeltaTime = static_cast<float>(
        currentTime - m_LastTime
        );

    m_LastTime = currentTime;
}

float Time::GetDeltaTime() const
{
    return m_DeltaTime;
}