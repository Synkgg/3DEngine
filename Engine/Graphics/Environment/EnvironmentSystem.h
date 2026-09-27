#pragma once
class EnvironmentSystem
{
public:
    bool Initialize();
    void Shutdown();
    void Bind(unsigned int slot) const;
    unsigned int GetEnvironmentMap() const { return m_EnvironmentMap; }
private:
    unsigned int m_EnvironmentMap = 0;
};
