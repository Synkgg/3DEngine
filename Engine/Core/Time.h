#pragma once

class Time
{
public:
    Time();

    void Update();

    float GetDeltaTime() const;

private:
    double m_LastTime;
    float m_DeltaTime;
};