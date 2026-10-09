#include "AudioEngine.h"
#include "../Core/Logger.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <cstdint>

bool AudioEngine::Initialize(){if(m_Device)return true;
m_Device=SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,nullptr);
if(!m_Device){Logger::Warning(std::string("Audio unavailable: ")+SDL_GetError());
return false;
}SDL_ResumeAudioDevice(m_Device);
return true;
}
void AudioEngine::Shutdown(){for(auto*s:m_Streams){SDL_UnbindAudioStream(s);
SDL_DestroyAudioStream(s);
}m_Streams.clear();
if(m_Device){SDL_CloseAudioDevice(m_Device);
m_Device=0;
}}
void AudioEngine::Update(){for(auto i=m_Streams.begin();i!=m_Streams.end();){auto*s=*i;
if(SDL_GetAudioStreamQueued(s)==0&&SDL_GetAudioStreamAvailable(s)==0){SDL_UnbindAudioStream(s);
SDL_DestroyAudioStream(s);
i=m_Streams.erase(i);
}else ++i;
}}
bool AudioEngine::PlaySound(const std::string&p,float v){if(!m_Device)return false;
std::filesystem::path resolved(p);
if(!resolved.is_absolute()&&!m_ProjectRoot.empty())resolved=(m_ProjectRoot/resolved).lexically_normal();
SDL_AudioSpec src{},dst{};
Uint8*d=nullptr;
Uint32 n=0;
if(!SDL_LoadWAV(resolved.string().c_str(),&src,&d,&n)){Logger::Warning("Sound not found: "+resolved.string());
return false;
}SDL_GetAudioDeviceFormat(m_Device,&dst,nullptr);
auto*s=SDL_CreateAudioStream(&src,&dst);
if(!s){SDL_free(d);
return false;
}if(!SDL_BindAudioStream(m_Device,s)){SDL_DestroyAudioStream(s);
SDL_free(d);
return false;
}SDL_SetAudioStreamGain(s,std::clamp(v*m_MasterVolume,0.0f,1.0f));
bool ok=SDL_PutAudioStreamData(s,d,(int)n);
SDL_free(d);
if(!ok){SDL_UnbindAudioStream(s);
SDL_DestroyAudioStream(s);
return false;
}SDL_FlushAudioStream(s);
m_Streams.push_back(s);
return true;
}

void AudioEngine::SetMasterVolume(float v){m_MasterVolume=std::clamp(v,0.0f,1.0f);}
void AudioEngine::SetSFXVolume(float v){m_SFXVolume=std::clamp(v,0.0f,1.0f);}
void AudioEngine::SetUIVolume(float v){m_UIVolume=std::clamp(v,0.0f,1.0f);}

bool AudioEngine::PlayFootstep(const std::string& surface, bool sprint)
{
    if (!m_Device) return false;
    // Two staggered contact transients (heel + toe) with low-frequency body,
    // surface-dependent grit and, on metal, a short resonant plate ring.
    // Each step gets a different pitch, timing and noise seed; no WAV files or
    // filesystem access are required in packaged games.
    constexpr int sampleRate = 48000;
    constexpr float pi = 3.14159265358979323846f;
    const bool metal = surface == "metal";
    const unsigned int sequence = ++m_FootstepSequence;
    std::uint32_t rng = 0x9E3779B9u ^ (sequence * 1664525u);
    auto noise = [&rng]() -> float
    {
        rng = rng * 1664525u + 1013904223u;
        return (static_cast<float>((rng >> 8) & 0xFFFFu) / 32767.5f) - 1.0f;
    };
    const float variation = static_cast<float>(sequence % 7u) / 6.0f;
    const float pitch = 0.91f + variation * 0.17f + (sprint ? 0.10f : 0.0f);
    const float duration = metal ? 0.24f : 0.19f;
    const int count = static_cast<int>(sampleRate * duration);
    std::vector<float> samples(static_cast<std::size_t>(count), 0.0f);
    float low = 0.0f;
    float previous = 0.0f;
    for (int i = 0; i < count; ++i)
    {
        const float t = static_cast<float>(i) / static_cast<float>(sampleRate);
        const float raw = noise();
        const float cutoff = metal ? 0.22f : 0.11f;
        low += cutoff * (raw - low);
        const float body = low * std::exp(-t * (metal ? 26.0f : 34.0f));
        const float heel = std::exp(-t * 67.0f);
        const float toeTime = t - (sprint ? 0.062f : 0.078f);
        const float toe = toeTime > 0.0f ? std::exp(-toeTime * 76.0f) : 0.0f;
        const float contact = (heel + 0.62f * toe);
        const float bass = std::sin(2.0f * pi * (78.0f * pitch * t - 38.0f * t * t)) *
                           std::exp(-t * 29.0f);
        const float grit = (raw - previous * 0.35f) * contact * (metal ? 0.11f : 0.17f);
        const float ring = metal ?
            (std::sin(2.0f * pi * 840.0f * pitch * t) * 0.13f +
             std::sin(2.0f * pi * 1220.0f * pitch * t) * 0.06f) *
            std::exp(-t * 23.0f) : 0.0f;
        previous = raw;
        float sample = (body * 0.8f + bass * 0.26f + grit + ring) *
                       (sprint ? 0.85f : 0.70f);
        // A short attack ramp avoids a digital click at the start.
        sample *= std::min(1.0f, t * 900.0f);
        samples[static_cast<std::size_t>(i)] = std::clamp(sample, -1.0f, 1.0f);
    }

    SDL_AudioSpec src{};
    src.format = SDL_AUDIO_F32;
    src.channels = 1;
    src.freq = sampleRate;
    SDL_AudioSpec dst{};
    if (!SDL_GetAudioDeviceFormat(m_Device, &dst, nullptr)) return false;
    SDL_AudioStream* stream = SDL_CreateAudioStream(&src, &dst);
    if (!stream) return false;
    if (!SDL_BindAudioStream(m_Device, stream))
    {
        SDL_DestroyAudioStream(stream);
        return false;
    }
    SDL_SetAudioStreamGain(stream, std::clamp(m_MasterVolume * m_SFXVolume, 0.0f, 1.0f));
    const bool queued = SDL_PutAudioStreamData(stream, samples.data(),
        static_cast<int>(samples.size() * sizeof(float))) && SDL_FlushAudioStream(stream);
    if (!queued)
    {
        SDL_UnbindAudioStream(stream);
        SDL_DestroyAudioStream(stream);
        return false;
    }
    m_Streams.push_back(stream);
    return true;
}
