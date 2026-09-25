#pragma once
#include <SDL3/SDL.h>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

struct NetworkGameState
{
    std::uint32_t revision = 0;
    int redScore = 0;
    int blueScore = 0;
    int roundSeconds = 180;
    float orbX = 0, orbY = 1, orbZ = 0;
};

struct NetworkTransformState
{
    std::uint32_t playerID = 0;
    float x=0, y=0, z=0;
    float rx=0, ry=0, rz=0;
};

class NetworkManager
{
public:
    ~NetworkManager();
    bool Host(std::uint16_t port = 7777);
    bool Join(const std::string& address, std::uint16_t port = 7777);
    void Update();
    void Disconnect();
    void SendLocalTransform(const NetworkTransformState& state);
    void SetGameState(const NetworkGameState& state);
    const NetworkGameState& GetGameState() const { return m_GameState; }
    const std::unordered_map<std::uint32_t, NetworkTransformState>& GetRemoteTransforms() const { return m_RemoteTransforms; }

    bool IsHost() const { return m_Mode == Mode::Host; }
    bool IsConnected() const { return m_Mode != Mode::Offline; }
    bool IsHandshakeComplete() const { return m_Mode == Mode::Host || m_LocalPlayerID != 0; }
    std::uint32_t GetLocalPlayerID() const { return m_Mode == Mode::Host ? 1u : m_LocalPlayerID; }
    int GetPlayerCount() const { return m_Mode == Mode::Offline ? 1 : (m_Mode == Mode::Host ? 1 + (int)m_Clients.size() : (m_LocalPlayerID ? 2 : 1)); }
    const std::string& GetLastError() const { return m_LastError; }

private:
    enum class Mode { Offline, Host, Client };
    struct Endpoint { std::uint32_t address=0; std::uint16_t port=0; std::uint32_t playerID=0; };
    bool OpenSocket(std::uint16_t port);
    void SendHello();
    void SetError(const std::string& message);
    void SendTransformTo(const Endpoint& endpoint, const NetworkTransformState& state);
    void SendGameStateTo(const Endpoint& endpoint);

    Mode m_Mode=Mode::Offline;
#ifdef _WIN32
    std::uintptr_t m_Socket=~(std::uintptr_t)0;
#else
    int m_Socket=-1;
#endif
    Endpoint m_Server;
    std::vector<Endpoint> m_Clients;
    std::unordered_map<std::uint32_t, NetworkTransformState> m_RemoteTransforms;
    std::string m_LastError;
    std::uint32_t m_LocalPlayerID=0;
    std::uint32_t m_NextPlayerID=2;
    NetworkGameState m_GameState{};
};
