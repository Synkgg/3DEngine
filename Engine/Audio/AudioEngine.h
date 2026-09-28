#pragma once
#include <SDL3/SDL_audio.h>
#include <SDL3_mixer/SDL_mixer.h>
#include <string>
#include <vector>
#include <filesystem>
class AudioEngine {
public:
 bool Initialize(); void Shutdown(); void Update();
 bool PlaySound(const std::string& path,float volume=1.0f);
 void SetProjectRoot(const std::filesystem::path& root){m_ProjectRoot=root;}
 void PlayInteractSound(){PlaySound("Assets/Audio/Interaction/interact.wav",0.85f*m_SFXVolume);}
 void SetMasterVolume(float v); float GetMasterVolume() const { return m_MasterVolume; }
 void SetSFXVolume(float v); float GetSFXVolume() const { return m_SFXVolume; }
 void SetUIVolume(float v); float GetUIVolume() const { return m_UIVolume; }
private: std::filesystem::path m_ProjectRoot; SDL_AudioDeviceID m_Device=0; MIX_Mixer* m_Mixer=nullptr; std::vector<MIX_Audio*> m_PlayingAudio; std::vector<SDL_AudioStream*> m_Streams; float m_MasterVolume=1.0f,m_SFXVolume=1.0f,m_UIVolume=1.0f;
};
