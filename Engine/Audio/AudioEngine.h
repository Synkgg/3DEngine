#pragma once
#include <SDL3/SDL_audio.h>
#include <string>
#include <vector>
#include <filesystem>
class AudioEngine {
public:
 bool Initialize();
 void Shutdown();
 void Update();
 enum class Bus { Direct, Effects, UI };
 bool PlaySound(const std::string& path,float volume=1.0f,Bus bus=Bus::Direct);
 // Layered, pitch-varied procedural contact audio, generated in memory.
 bool PlayFootstep(const std::string& surface, bool sprint=false);
 void SetProjectRoot(const std::filesystem::path& root){m_ProjectRoot=root;}
 void PlayInteractSound(){PlaySound("Assets/Audio/Interaction/interact.wav",0.85f,Bus::Effects);}
 void SetMasterVolume(float v);
 float GetMasterVolume() const { return m_MasterVolume;
 }
 void SetSFXVolume(float v);
 float GetSFXVolume() const { return m_SFXVolume;
 }
 void SetUIVolume(float v);
 float GetUIVolume() const { return m_UIVolume;
 }
private: std::filesystem::path m_ProjectRoot;
SDL_AudioDeviceID m_Device=0;
struct PlayingSound { SDL_AudioStream* stream; float volume; Bus bus; };
std::vector<PlayingSound> m_Streams;
void ApplyVolumes();
float m_MasterVolume=1.0f,m_SFXVolume=1.0f,m_UIVolume=1.0f;
unsigned int m_FootstepSequence=0;
};
