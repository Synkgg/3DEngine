#pragma once
#include <SDL3/SDL_audio.h>
#include <string>
#include <vector>
class AudioEngine {
public:
 bool Initialize(); void Shutdown(); void Update();
 bool PlaySound(const std::string& path,float volume=1.0f);
 void PlayUISound(){PlaySound("Assets/Audio/UI/click.wav",0.65f);}
 void PlayInteractSound(){PlaySound("Assets/Audio/Interaction/interact.wav",0.85f);}
private: SDL_AudioDeviceID m_Device=0; std::vector<SDL_AudioStream*> m_Streams;
};
