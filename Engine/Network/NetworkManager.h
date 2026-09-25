#pragma once
#include <SDL3/SDL.h>
#include <cstdint>
#include <string>
#include <vector>

class NetworkManager
{
public:
    ~NetworkManager();
    bool Host(std::uint16_t port = 7777);
    bool Join(const std::string& address, std::uint16_t port = 7777);
    void Update();
    void Disconnect();

    bool IsHost() const { return m_Mode == Mode::Host; }
    bool IsConnected() const { return m_Mode != Mode::Offline; }
    int GetPlayerCount() const { return m_Mode == Mode::Offline ? 1 : (m_Mode == Mode::Host ? 1 + (int)m_Clients.size() : 2); }
    const std::string& GetLastError() const { return m_LastError; }

private:
    enum class Mode { Offline, Host, Client };
    struct Endpoint { std::uint32_t address = 0; std::uint16_t port = 0; };
    bool OpenSocket(std::uint16_t port);
    void SendHello();
    void SetError(const std::string& message);

    Mode m_Mode = Mode::Offline;
    SDL_IOStream* m_Unused = nullptr; // keeps this header SDL-only; socket is native below
#ifdef _WIN32
    std::uintptr_t m_Socket = ~(std::uintptr_t)0;
#else
    int m_Socket = -1;
#endif
    Endpoint m_Server;
    std::vector<Endpoint> m_Clients;
    std::string m_LastError;
    float m_HelloTimer = 0.0f;
};
