#pragma once
#include <SDL3/SDL.h>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

struct NetworkTransformState
{
    std::uint32_t playerID = 0;
    float x=0, y=0, z=0;
    float rx=0, ry=0, rz=0;
};

struct NetworkMessage
{
    std::uint32_t senderID = 0;
    std::uint16_t channel = 0;
    std::string payload;
};

class NetworkManager
{
public:
    ~NetworkManager();
    bool Host(std::uint16_t port = 7777);
    bool Join(const std::string& address, std::uint16_t port = 7777);
    void Update();
    void Disconnect();

    // Generic replication primitives. Gameplay decides what these values mean.
    void SendLocalTransform(const NetworkTransformState& state);
    void SendNetworkMessage(std::uint16_t channel, const std::string& payload);
    std::vector<NetworkMessage> ConsumeMessages();

    const std::unordered_map<std::uint32_t, NetworkTransformState>& GetRemoteTransforms() const { return m_RemoteTransforms; }
    bool IsHost() const { return m_Mode == Mode::Host; }
    bool IsConnected() const { return m_Mode != Mode::Offline; }
    bool IsHandshakeComplete() const { return m_Mode == Mode::Host || m_LocalPlayerID != 0; }
    bool WasKickedByHost();
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
    void SendMessageTo(const Endpoint& endpoint, std::uint32_t senderID, std::uint16_t channel, const std::string& payload);

    Mode m_Mode=Mode::Offline;
#ifdef _WIN32
    std::uintptr_t m_Socket=~(std::uintptr_t)0;
#else
    int m_Socket=-1;
#endif
    Endpoint m_Server;
    std::vector<Endpoint> m_Clients;
    std::unordered_map<std::uint32_t, NetworkTransformState> m_RemoteTransforms;
    std::vector<NetworkMessage> m_Messages;
    std::string m_LastError;
    std::uint32_t m_LocalPlayerID=0;
    std::uint32_t m_NextPlayerID=2;
    bool m_KickedByHost=false;
};
