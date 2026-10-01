#pragma once
class EnvironmentSystem
{
public:
    bool Initialize();
    void Shutdown();
    void Bind(unsigned int slot) const;
    void BindIrradiance(unsigned int slot) const;
    unsigned int GetEnvironmentMap() const { return m_EnvironmentMap; }
    unsigned int GetIrradianceMap() const { return m_IrradianceMap; }
private:
    unsigned int m_EnvironmentMap = 0;
    unsigned int m_IrradianceMap = 0;
};
