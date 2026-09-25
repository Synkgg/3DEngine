#include "AudioEngine.h"
#include "../Core/Logger.h"
#include <SDL3/SDL.h>
#include <algorithm>
bool AudioEngine::Initialize(){if(m_Device)return true;m_Device=SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,nullptr);if(!m_Device){Logger::Warning(std::string("Audio unavailable: ")+SDL_GetError());return false;}SDL_ResumeAudioDevice(m_Device);return true;}
void AudioEngine::Shutdown(){for(auto*s:m_Streams){SDL_UnbindAudioStream(s);SDL_DestroyAudioStream(s);}m_Streams.clear();if(m_Device){SDL_CloseAudioDevice(m_Device);m_Device=0;}}
void AudioEngine::Update(){for(auto i=m_Streams.begin();i!=m_Streams.end();){auto*s=*i;if(SDL_GetAudioStreamQueued(s)==0&&SDL_GetAudioStreamAvailable(s)==0){SDL_UnbindAudioStream(s);SDL_DestroyAudioStream(s);i=m_Streams.erase(i);}else ++i;}}
bool AudioEngine::PlaySound(const std::string&p,float v){if(!m_Device)return false;SDL_AudioSpec src{},dst{};Uint8*d=nullptr;Uint32 n=0;if(!SDL_LoadWAV(p.c_str(),&src,&d,&n)){Logger::Warning("Sound not found: "+p);return false;}SDL_GetAudioDeviceFormat(m_Device,&dst,nullptr);auto*s=SDL_CreateAudioStream(&src,&dst);if(!s){SDL_free(d);return false;}if(!SDL_BindAudioStream(m_Device,s)){SDL_DestroyAudioStream(s);SDL_free(d);return false;}SDL_SetAudioStreamGain(s,std::clamp(v*m_MasterVolume,0.0f,1.0f));bool ok=SDL_PutAudioStreamData(s,d,(int)n);SDL_free(d);if(!ok){SDL_UnbindAudioStream(s);SDL_DestroyAudioStream(s);return false;}SDL_FlushAudioStream(s);m_Streams.push_back(s);return true;}

void AudioEngine::SetMasterVolume(float v){m_MasterVolume=std::clamp(v,0.0f,1.0f);}
void AudioEngine::SetSFXVolume(float v){m_SFXVolume=std::clamp(v,0.0f,1.0f);}
void AudioEngine::SetUIVolume(float v){m_UIVolume=std::clamp(v,0.0f,1.0f);}
