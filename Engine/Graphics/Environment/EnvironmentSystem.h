#pragma once
#include "../RHI/RHITypes.h"
class EnvironmentSystem{public:bool Initialize();void Shutdown();void Bind(unsigned int)const{}void BindIrradiance(unsigned int)const{}Velcryn::RHI::TextureHandle GetEnvironmentMap()const{return m_EnvironmentMap;}Velcryn::RHI::TextureHandle GetIrradianceMap()const{return m_IrradianceMap;}private:Velcryn::RHI::TextureHandle m_EnvironmentMap{},m_IrradianceMap{};};
