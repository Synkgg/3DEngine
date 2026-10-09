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

struct NetworkServerInfo
{
    std::string name;
    std::string address;
    std::uint16_t port = 0;
    std::uint32_t players = 0;
    std::uint32_t maxPlayers = 0;
    std::uint64_t lastSeen = 0;
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
    bool Host(std::uint16_t port = 7777, std::uint32_t maxPlayers = 8);
    std::uint16_t GetBoundPort() const { return m_BoundPort; }
    bool Join(const std::string& address, std::uint16_t port = 7777);
    void Update();
    void Disconnect();

    // LAN discovery broadcasts a query to hosts on the selected UDP port.
    // Results are validated and refreshed during Update(), without blocking
    // the game loop. Internet-wide discovery requires a separate directory.
    bool SearchServers(std::uint16_t port = 7777);
    void StopServerSearch();
    bool IsSearchingServers() const { return m_SearchingServers; }
    const std::vector<NetworkServerInfo>& GetServers() const { return m_Servers; }

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
    struct Endpoint { std::uint32_t address=0; std::uint16_t port=0; std::uint32_t playerID=0; std::uint64_t lastSeen=0; };
    bool OpenSocket(std::uint16_t port);
    void SendHello();
    void PollServerSearch();
    void SetError(const std::string& message);
    void SendTransformTo(const Endpoint& endpoint, const NetworkTransformState& state);
    void SendMessageTo(const Endpoint& endpoint, std::uint32_t senderID, std::uint16_t channel, const std::string& payload);

    Mode m_Mode=Mode::Offline;
    std::uint32_t m_MaxPlayers=8;
    std::uint16_t m_BoundPort=0;
    std::uint64_t m_LastSend=0,m_LastReceive=0;
#ifdef _WIN32
    std::uintptr_t m_Socket=~(std::uintptr_t)0;
#else
    int m_Socket=-1;
#endif
    Endpoint m_Server;
    std::vector<Endpoint> m_Clients;
    std::unordered_map<std::uint32_t, NetworkTransformState> m_RemoteTransforms;
    std::unordered_map<std::uint32_t, std::uint32_t> m_LastRemoteTransformSequence;
    std::vector<NetworkMessage> m_Messages;
    std::string m_LastError;
    std::uint32_t m_LocalPlayerID=0;
    std::uint32_t m_NextPlayerID=2;
    std::uint32_t m_LocalTransformSequence=0;
    bool m_KickedByHost=false;
#ifdef _WIN32
    std::uintptr_t m_SearchSocket=~(std::uintptr_t)0;
#else
    int m_SearchSocket=-1;
#endif
    std::uint16_t m_SearchPort=7777;
    std::uint32_t m_SearchNonce=0;
    std::uint64_t m_SearchStarted=0;
    std::uint64_t m_SearchLastBroadcast=0;
    bool m_SearchingServers=false;
    std::vector<NetworkServerInfo> m_Servers;
};
