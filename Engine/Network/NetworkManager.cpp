#include "NetworkManager.h"
#include "../Core/Logger.h"
#include <algorithm>
#include <cstring>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace {
#ifdef _WIN32
using SocketHandle = SOCKET;
constexpr SocketHandle InvalidSocket = INVALID_SOCKET;
#else
using SocketHandle = int;
constexpr SocketHandle InvalidSocket = -1;
#endif
constexpr char Hello[] = "VORTEK_HELLO_1";
constexpr char Welcome[] = "VORTEK_WELCOME_1";
void CloseSocket(SocketHandle s) {
#ifdef _WIN32
    if (s != InvalidSocket) closesocket(s);
#else
    if (s != InvalidSocket) close(s);
#endif
}
}

NetworkManager::~NetworkManager() { Disconnect(); }

void NetworkManager::SetError(const std::string& message) { m_LastError = message; Logger::Error("Network: " + message); }

bool NetworkManager::OpenSocket(std::uint16_t port)
{
#ifdef _WIN32
    static bool winsockReady = false;
    if (!winsockReady) { WSADATA data{}; if (WSAStartup(MAKEWORD(2,2), &data) != 0) { SetError("WSAStartup failed."); return false; } winsockReady = true; }
#endif
    SocketHandle s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == InvalidSocket) { SetError("Could not create UDP socket."); return false; }
    sockaddr_in local{}; local.sin_family = AF_INET; local.sin_addr.s_addr = htonl(INADDR_ANY); local.sin_port = htons(port);
    if (bind(s, reinterpret_cast<sockaddr*>(&local), sizeof(local)) != 0) { CloseSocket(s); SetError("Could not bind UDP port " + std::to_string(port) + "."); return false; }
#ifdef _WIN32
    u_long nonBlocking = 1; ioctlsocket(s, FIONBIO, &nonBlocking);
    m_Socket = static_cast<std::uintptr_t>(s);
#else
    fcntl(s, F_SETFL, fcntl(s, F_GETFL, 0) | O_NONBLOCK);
    m_Socket = s;
#endif
    return true;
}

bool NetworkManager::Host(std::uint16_t port)
{
    Disconnect();
    if (!OpenSocket(port)) return false;
    m_Mode = Mode::Host; m_LastError.clear();
    Logger::Info("Network: hosting on UDP port " + std::to_string(port) + ".");
    return true;
}

bool NetworkManager::Join(const std::string& address, std::uint16_t port)
{
    Disconnect();
    if (!OpenSocket(0)) return false;
    in_addr parsed{};
    if (inet_pton(AF_INET, address.c_str(), &parsed) != 1) { Disconnect(); SetError("Join currently requires an IPv4 address."); return false; }
    m_Server.address = parsed.s_addr; m_Server.port = port; m_Mode = Mode::Client; m_LastError.clear(); SendHello();
    Logger::Info("Network: joining " + address + ":" + std::to_string(port) + ".");
    return true;
}

void NetworkManager::SendHello()
{
    if (m_Mode != Mode::Client) return;
    SocketHandle s = static_cast<SocketHandle>(m_Socket);
    sockaddr_in to{}; to.sin_family=AF_INET; to.sin_addr.s_addr=m_Server.address; to.sin_port=htons(m_Server.port);
    sendto(s, Hello, sizeof(Hello), 0, reinterpret_cast<sockaddr*>(&to), sizeof(to));
}

void NetworkManager::Update()
{
    if (m_Mode == Mode::Offline) return;
    SocketHandle s = static_cast<SocketHandle>(m_Socket);
    char buffer[256]; sockaddr_in from{};
#ifdef _WIN32
    int fromLen=sizeof(from);
#else
    socklen_t fromLen=sizeof(from);
#endif
    for (;;) {
        int received=(int)recvfrom(s, buffer, sizeof(buffer)-1, 0, reinterpret_cast<sockaddr*>(&from), &fromLen);
        if (received <= 0) break;
        buffer[received]=0;
        if (m_Mode==Mode::Host && std::strcmp(buffer, Hello)==0) {
            Endpoint ep{from.sin_addr.s_addr, ntohs(from.sin_port)};
            auto it=std::find_if(m_Clients.begin(),m_Clients.end(),[&](const Endpoint& e){return e.address==ep.address&&e.port==ep.port;});
            if(it==m_Clients.end()){m_Clients.push_back(ep);Logger::Info("Network: client connected.");}
            sendto(s, Welcome, sizeof(Welcome), 0, reinterpret_cast<sockaddr*>(&from), fromLen);
        } else if (m_Mode==Mode::Client && std::strcmp(buffer, Welcome)==0) {
            Logger::Info("Network: host acknowledged connection.");
        }
    }
}

void NetworkManager::Disconnect()
{
    if (m_Mode != Mode::Offline) CloseSocket(static_cast<SocketHandle>(m_Socket));
#ifdef _WIN32
    m_Socket = ~(std::uintptr_t)0;
#else
    m_Socket = -1;
#endif
    m_Mode=Mode::Offline; m_Clients.clear(); m_Server={}; m_HelloTimer=0.0f;
}
