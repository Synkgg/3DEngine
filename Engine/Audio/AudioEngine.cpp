#include "AudioEngine.h"
#include "../Core/Logger.h"
#include <SDL3/SDL.h>
#include <algorithm>

bool AudioEngine::Initialize()
{
    if (m_Mixer) return true;
    m_Mixer = MIX_CreateMixerDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr);
    if (!m_Mixer)
    {
        Logger::Warning(std::string("Audio unavailable: ") + SDL_GetError());
        return false;
    }
    return true;
}

void AudioEngine::Shutdown()
{
    for (auto* audio : m_PlayingAudio)
        MIX_DestroyAudio(audio);
    m_PlayingAudio.clear();

    if (m_Mixer)
    {
        MIX_DestroyMixer(m_Mixer);
        m_Mixer = nullptr;
    }
}

void AudioEngine::Update()
{
    for (auto it = m_PlayingAudio.begin(); it != m_PlayingAudio.end();)
    {
        if (!MIX_AudioPlaying(*it))
        {
            MIX_DestroyAudio(*it);
            it = m_PlayingAudio.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

bool AudioEngine::PlaySound(const std::string& path, float volume)
{
    if (!m_Mixer) return false;

    std::filesystem::path resolved(path);
    if (!resolved.is_absolute() && !m_ProjectRoot.empty())
        resolved = (m_ProjectRoot / resolved).lexically_normal();

    MIX_Audio* audio = MIX_LoadAudio(m_Mixer, resolved.string().c_str(), false);
    if (!audio)
    {
        Logger::Warning("Sound not found or unsupported: " + resolved.string());
        return false;
    }

    MIX_SetAudioGain(audio, std::clamp(volume * m_MasterVolume, 0.0f, 1.0f));
    if (!MIX_PlayAudio(m_Mixer, audio))
    {
        MIX_DestroyAudio(audio);
        return false;
    }

    m_PlayingAudio.push_back(audio);
    return true;
}

void AudioEngine::SetMasterVolume(float v){m_MasterVolume=std::clamp(v,0.0f,1.0f);}
void AudioEngine::SetSFXVolume(float v){m_SFXVolume=std::clamp(v,0.0f,1.0f);}
void AudioEngine::SetUIVolume(float v){m_UIVolume=std::clamp(v,0.0f,1.0f);}
