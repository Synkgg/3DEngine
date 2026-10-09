#include "NetworkManager.h"
#include "../Core/Logger.h"
#include <algorithm>
#include <cstring>
#include <cmath>
#include <iterator>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
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
using SocketHandle=SOCKET; constexpr SocketHandle InvalidSocket=INVALID_SOCKET;
#else
using SocketHandle=int; constexpr SocketHandle InvalidSocket=-1;
#endif
constexpr std::uint32_t Magic=0x454E474E; // ENGN
constexpr std::size_t MaxMessagePayload=192;
enum : std::uint8_t { Hello=1, Welcome=2, Transform=3, Goodbye=4, Message=5, HostShutdown=6, Ping=7, Pong=8, Full=9, Discover=10, DiscoverReply=11 };
constexpr std::uint32_t DiscoveryVersion=1;
constexpr std::uint64_t SearchDurationMs=4000;
constexpr std::uint64_t SearchBroadcastIntervalMs=900;
#pragma pack(push,1)
struct PacketHeader { std::uint32_t magic; std::uint8_t type; };
struct WelcomePacket { PacketHeader header; std::uint32_t playerID; };
struct DiscoverPacket { PacketHeader header; std::uint32_t nonce; std::uint32_t version; };
struct DiscoverReplyPacket { PacketHeader header; std::uint32_t nonce; std::uint32_t version; std::uint16_t port; std::uint16_t players; std::uint16_t maxPlayers; char name[48]; };
struct TransformPacket { PacketHeader header; std::uint32_t playerID; std::uint32_t sequence; float x,y,z,rx,ry,rz; };
struct MessagePacket { PacketHeader header; std::uint32_t senderID; std::uint16_t channel; std::uint16_t size; char payload[MaxMessagePayload]; };
#pragma pack(pop)
bool InitializeSockets(){
#ifdef _WIN32
 static bool ready=false;
 if(!ready){WSADATA data{};if(WSAStartup(MAKEWORD(2,2),&data)!=0)return false;ready=true;}
#endif
 return true;
}
void CloseSocket(SocketHandle s){
#ifdef _WIN32
 if(s!=InvalidSocket) closesocket(s);
#else
 if(s!=InvalidSocket) close(s);
#endif
}
}

NetworkManager::~NetworkManager(){Disconnect();}
void NetworkManager::SetError(const std::string& m){m_LastError=m;Logger::Error("Network: "+m);}
bool NetworkManager::OpenSocket(std::uint16_t port){
 if(!InitializeSockets()){SetError("Could not initialize network sockets.");return false;}
 SocketHandle s=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP); if(s==InvalidSocket){SetError("Could not create UDP socket.");return false;}
 sockaddr_in local{};local.sin_family=AF_INET;local.sin_addr.s_addr=htonl(INADDR_ANY);local.sin_port=htons(port);
 if(bind(s,reinterpret_cast<sockaddr*>(&local),sizeof(local))!=0){CloseSocket(s);SetError("Could not bind UDP port "+std::to_string(port)+".");return false;}
#ifdef _WIN32
 u_long nb=1;ioctlsocket(s,FIONBIO,&nb);m_Socket=static_cast<std::uintptr_t>(s);
#else
 fcntl(s,F_SETFL,fcntl(s,F_GETFL,0)|O_NONBLOCK);m_Socket=s;
#endif
 sockaddr_in bound{};
#ifdef _WIN32
 int boundSize=sizeof(bound);
#else
 socklen_t boundSize=sizeof(bound);
#endif
 if(getsockname(s,reinterpret_cast<sockaddr*>(&bound),&boundSize)==0)m_BoundPort=ntohs(bound.sin_port);
 m_LastSend=m_LastReceive=SDL_GetTicks();
 return true;
}
bool NetworkManager::Host(std::uint16_t port,std::uint32_t maxPlayers,const std::string& serverName){
 Disconnect();
 m_MaxPlayers=std::clamp(maxPlayers,2u,32u);
 m_HostName=serverName.substr(0,47);
 for(char& ch:m_HostName)if(static_cast<unsigned char>(ch)<32)ch=' ';
 if(m_HostName.empty())m_HostName="Velcryn Server";
 if(!OpenSocket(port))return false;
 m_Mode=Mode::Host;m_LocalPlayerID=1;m_LastError.clear();
 Logger::Info("Network: hosting "+m_HostName+" on UDP port "+std::to_string(m_BoundPort)+".");
 return true;
}
bool NetworkManager::Join(const std::string& address,std::uint16_t port){Disconnect();if(!OpenSocket(0))return false;in_addr a{};if(inet_pton(AF_INET,address.c_str(),&a)!=1){Disconnect();SetError("Join currently requires an IPv4 address.");return false;}m_Server={a.s_addr,port,1};m_Mode=Mode::Client;m_LastError.clear();SendHello();Logger::Info("Network: joining "+address+":"+std::to_string(port)+".");return true;}
bool NetworkManager::SearchServers(std::uint16_t port)
{
 StopServerSearch();
 m_Servers.clear();
 m_LastError.clear();
 if(port==0){SetError("Choose a valid LAN server port.");return false;}
 if(!InitializeSockets()){SetError("Could not initialize LAN discovery sockets.");return false;}
 SocketHandle s=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
 if(s==InvalidSocket){SetError("Could not create LAN discovery socket.");return false;}
 sockaddr_in local{};local.sin_family=AF_INET;local.sin_addr.s_addr=htonl(INADDR_ANY);local.sin_port=0;
 if(bind(s,reinterpret_cast<sockaddr*>(&local),sizeof(local))!=0){
  CloseSocket(s);SetError("Could not bind LAN discovery socket.");return false;
 }
 int broadcast=1;
 if(setsockopt(s,SOL_SOCKET,SO_BROADCAST,reinterpret_cast<const char*>(&broadcast),sizeof(broadcast))!=0){
  CloseSocket(s);SetError("Could not enable UDP LAN broadcast.");return false;
 }
#ifdef _WIN32
 u_long nb=1;
 if(ioctlsocket(s,FIONBIO,&nb)!=0){CloseSocket(s);SetError("Could not make LAN discovery nonblocking.");return false;}
 m_SearchSocket=static_cast<std::uintptr_t>(s);
#else
 if(fcntl(s,F_SETFL,fcntl(s,F_GETFL,0)|O_NONBLOCK)!=0){
  CloseSocket(s);SetError("Could not make LAN discovery nonblocking.");return false;
 }
 m_SearchSocket=s;
#endif
 m_SearchPort=port;
 m_SearchStarted=SDL_GetTicks();
 m_SearchLastBroadcast=0;
 m_SearchNonce=static_cast<std::uint32_t>(SDL_GetTicksNS() ^ (static_cast<std::uint64_t>(port)<<16));
 m_SearchingServers=true;
 PollServerSearch();
 return true;
}

void NetworkManager::StopServerSearch()
{
 const SocketHandle s=static_cast<SocketHandle>(m_SearchSocket);
 if(s!=InvalidSocket)CloseSocket(s);
#ifdef _WIN32
 m_SearchSocket=~(std::uintptr_t)0;
#else
 m_SearchSocket=-1;
#endif
 m_SearchingServers=false;
}

void NetworkManager::PollServerSearch()
{
 if(!m_SearchingServers)return;
 const auto now=SDL_GetTicks();
 SocketHandle s=static_cast<SocketHandle>(m_SearchSocket);
 if(now-m_SearchStarted>=SearchDurationMs){StopServerSearch();return;}
 if(m_SearchLastBroadcast==0 || now-m_SearchLastBroadcast>=SearchBroadcastIntervalMs){
  m_SearchLastBroadcast=now;
  DiscoverPacket query{{Magic,Discover},m_SearchNonce,DiscoveryVersion};
  sockaddr_in to{};to.sin_family=AF_INET;to.sin_port=htons(m_SearchPort);
  to.sin_addr.s_addr=htonl(INADDR_BROADCAST);
  sendto(s,reinterpret_cast<const char*>(&query),sizeof(query),0,reinterpret_cast<sockaddr*>(&to),sizeof(to));
  // Also discover a server running on this computer (useful for testing).
  to.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
  sendto(s,reinterpret_cast<const char*>(&query),sizeof(query),0,reinterpret_cast<sockaddr*>(&to),sizeof(to));
 }
 for(int i=0;i<64;++i){
  DiscoverReplyPacket reply{};sockaddr_in from{};
#ifdef _WIN32
  int len=sizeof(from);
#else
  socklen_t len=sizeof(from);
#endif
  const int n=(int)recvfrom(s,reinterpret_cast<char*>(&reply),sizeof(reply),0,reinterpret_cast<sockaddr*>(&from),&len);
  if(n<=0)break;
  if(n!=sizeof(reply)||reply.header.magic!=Magic||reply.header.type!=DiscoverReply||
     reply.version!=DiscoveryVersion||reply.nonce!=m_SearchNonce||
     reply.port==0||reply.maxPlayers<2||reply.maxPlayers>32||reply.players<1||
     reply.players>reply.maxPlayers)continue;
  char ip[INET_ADDRSTRLEN]{};
  if(!inet_ntop(AF_INET,&from.sin_addr,ip,sizeof(ip)))continue;
  const auto nameLength=std::find(reply.name,reply.name+sizeof(reply.name),'\0')-reply.name;
  std::string name(reply.name,static_cast<std::size_t>(nameLength));
  for(char& ch:name)if(static_cast<unsigned char>(ch)<32)ch=' ';
  if(name.empty())name="LAN DUEL";
  auto existing=std::find_if(m_Servers.begin(),m_Servers.end(),[&](const NetworkServerInfo& info){
   return info.address==ip&&info.port==reply.port;
  });
  NetworkServerInfo info{name,ip,reply.port,reply.players,reply.maxPlayers,now};
  if(existing!=m_Servers.end())*existing=std::move(info);
  else if(m_Servers.size()<64)m_Servers.push_back(std::move(info));
 }
 std::sort(m_Servers.begin(),m_Servers.end(),[](const NetworkServerInfo& a,const NetworkServerInfo& b){
  if(a.players==a.maxPlayers && b.players!=b.maxPlayers)return false;
  if(a.players!=a.maxPlayers && b.players==b.maxPlayers)return true;
  return a.address<b.address || (a.address==b.address&&a.port<b.port);
 });
}

void NetworkManager::SendHello(){if(m_Mode!=Mode::Client)return;PacketHeader p{Magic,Hello};sockaddr_in to{};to.sin_family=AF_INET;to.sin_addr.s_addr=m_Server.address;to.sin_port=htons(m_Server.port);sendto(static_cast<SocketHandle>(m_Socket),reinterpret_cast<const char*>(&p),sizeof(p),0,reinterpret_cast<sockaddr*>(&to),sizeof(to));}
void NetworkManager::SendTransformTo(const Endpoint& e,const NetworkTransformState& s){TransformPacket p{{Magic,Transform},s.playerID,m_LocalTransformSequence,s.x,s.y,s.z,s.rx,s.ry,s.rz};sockaddr_in to{};to.sin_family=AF_INET;to.sin_addr.s_addr=e.address;to.sin_port=htons(e.port);sendto(static_cast<SocketHandle>(m_Socket),reinterpret_cast<const char*>(&p),sizeof(p),0,reinterpret_cast<sockaddr*>(&to),sizeof(to));}
void NetworkManager::SendMessageTo(const Endpoint& e,std::uint32_t senderID,std::uint16_t channel,const std::string& payload){
 MessagePacket p{};p.header={Magic,Message};p.senderID=senderID;p.channel=channel;p.size=static_cast<std::uint16_t>(std::min(payload.size(),MaxMessagePayload));std::memcpy(p.payload,payload.data(),p.size);
 sockaddr_in to{};to.sin_family=AF_INET;to.sin_addr.s_addr=e.address;to.sin_port=htons(e.port);const int bytes=static_cast<int>(sizeof(PacketHeader)+sizeof(p.senderID)+sizeof(p.channel)+sizeof(p.size)+p.size);sendto(static_cast<SocketHandle>(m_Socket),reinterpret_cast<const char*>(&p),bytes,0,reinterpret_cast<sockaddr*>(&to),sizeof(to));
}
void NetworkManager::SendNetworkMessage(std::uint16_t channel,const std::string& payload){
 if(m_Mode==Mode::Offline||!IsHandshakeComplete())return;const std::string clipped=payload.substr(0,MaxMessagePayload);const auto sender=GetLocalPlayerID();
 if(m_Mode==Mode::Host){m_Messages.push_back({sender,channel,clipped});for(const auto& c:m_Clients)SendMessageTo(c,sender,channel,clipped);}
 else SendMessageTo(m_Server,sender,channel,clipped);
}
std::vector<NetworkMessage> NetworkManager::ConsumeMessages(){auto messages=std::move(m_Messages);m_Messages.clear();return messages;}
bool NetworkManager::WasKickedByHost(){const bool value=m_KickedByHost;m_KickedByHost=false;return value;}
void NetworkManager::SendLocalTransform(const NetworkTransformState& state){
 if(m_Mode==Mode::Offline||!IsHandshakeComplete())return;NetworkTransformState s=state;s.playerID=GetLocalPlayerID();
 ++m_LocalTransformSequence;
 if(m_Mode==Mode::Host){for(const auto& c:m_Clients)SendTransformTo(c,s);}else SendTransformTo(m_Server,s);
}
void NetworkManager::Update(){
 PollServerSearch();
 if(m_Mode==Mode::Offline)return;
 const auto now=SDL_GetTicks();
 if(m_Mode==Mode::Client && now-m_LastReceive>10000){Disconnect();SetError("Connection timed out. Check the host address and UDP port.");return;}
 if(m_Mode==Mode::Client && now-m_LastSend>=500){
  m_LastSend=now;
  if(!IsHandshakeComplete())SendHello();
  else {PacketHeader p{Magic,Ping};sockaddr_in to{};to.sin_family=AF_INET;to.sin_addr.s_addr=m_Server.address;to.sin_port=htons(m_Server.port);sendto(static_cast<SocketHandle>(m_Socket),reinterpret_cast<const char*>(&p),sizeof(p),0,reinterpret_cast<sockaddr*>(&to),sizeof(to));}
 }
 if(m_Mode==Mode::Host)for(auto it=m_Clients.begin();it!=m_Clients.end();) {
  if(now-it->lastSeen>10000){m_RemoteTransforms.erase(it->playerID);m_LastRemoteTransformSequence.erase(it->playerID);it=m_Clients.erase(it);}else ++it;
 }
 SocketHandle s=static_cast<SocketHandle>(m_Socket);char b[512];sockaddr_in from{};constexpr int MaxPacketsPerUpdate=128;
 for(int packetIndex=0;packetIndex<MaxPacketsPerUpdate;++packetIndex){
#ifdef _WIN32
  int len=sizeof(from);
#else
  socklen_t len=sizeof(from);
#endif
  int n=(int)recvfrom(s,b,sizeof(b),0,reinterpret_cast<sockaddr*>(&from),&len);if(n<=0)break;if(n<(int)sizeof(PacketHeader))continue;
  PacketHeader h{};std::memcpy(&h,b,sizeof(h));if(h.magic!=Magic)continue;
  if(m_Mode==Mode::Host && h.type==Discover){
   if(n!=(int)sizeof(DiscoverPacket))continue;
   DiscoverPacket query{};std::memcpy(&query,b,sizeof(query));
   if(query.version!=DiscoveryVersion)continue;
   DiscoverReplyPacket reply{};
   reply.header={Magic,DiscoverReply};reply.nonce=query.nonce;reply.version=DiscoveryVersion;
   reply.port=m_BoundPort;
   reply.players=static_cast<std::uint16_t>(GetPlayerCount());
   reply.maxPlayers=static_cast<std::uint16_t>(m_MaxPlayers);
   std::memcpy(reply.name,m_HostName.data(),m_HostName.size());
   sendto(s,reinterpret_cast<const char*>(&reply),sizeof(reply),0,reinterpret_cast<sockaddr*>(&from),len);
   continue;
  }
  if(m_Mode==Mode::Client){if(from.sin_addr.s_addr!=m_Server.address||ntohs(from.sin_port)!=m_Server.port)continue;m_LastReceive=now;}
  auto known=std::find_if(m_Clients.begin(),m_Clients.end(),[&](const Endpoint&e){return e.address==from.sin_addr.s_addr&&e.port==ntohs(from.sin_port);});
  if(known!=m_Clients.end())known->lastSeen=now;
  if(m_Mode==Mode::Host&&h.type==Ping&&known!=m_Clients.end()){PacketHeader pong{Magic,Pong};sendto(s,reinterpret_cast<const char*>(&pong),sizeof(pong),0,reinterpret_cast<sockaddr*>(&from),len);continue;}
  if(m_Mode==Mode::Client&&h.type==Full){Disconnect();SetError("This duel already has two operators.");return;}

  if(m_Mode==Mode::Host&&h.type==Hello){
   auto it=std::find_if(m_Clients.begin(),m_Clients.end(),[&](const Endpoint&e){return e.address==from.sin_addr.s_addr&&e.port==ntohs(from.sin_port);});
   if(it==m_Clients.end()){
    if(m_Clients.size()+1>=m_MaxPlayers){PacketHeader full{Magic,Full};sendto(s,reinterpret_cast<const char*>(&full),sizeof(full),0,reinterpret_cast<sockaddr*>(&from),len);continue;}
    std::uint32_t id=2;while(std::any_of(m_Clients.begin(),m_Clients.end(),[&](const Endpoint& e){return e.playerID==id;}))++id;
    m_Clients.push_back({from.sin_addr.s_addr,ntohs(from.sin_port),id,now});it=std::prev(m_Clients.end());Logger::Info("Network: client connected as player "+std::to_string(it->playerID)+".");}
   WelcomePacket w{{Magic,Welcome},it->playerID};sendto(s,reinterpret_cast<const char*>(&w),sizeof(w),0,reinterpret_cast<sockaddr*>(&from),len);
  }else if(m_Mode==Mode::Client&&h.type==Welcome&&n>=(int)sizeof(WelcomePacket)){
   WelcomePacket w{};std::memcpy(&w,b,sizeof(w));if(m_LocalPlayerID==0)Logger::Info("Network: joined as player "+std::to_string(w.playerID)+".");m_LocalPlayerID=w.playerID;
  }else if(m_Mode==Mode::Host&&h.type==Goodbye){
   auto it=std::find_if(m_Clients.begin(),m_Clients.end(),[&](const Endpoint&e){return e.address==from.sin_addr.s_addr&&e.port==ntohs(from.sin_port);});if(it!=m_Clients.end()){m_RemoteTransforms.erase(it->playerID);m_LastRemoteTransformSequence.erase(it->playerID);m_Clients.erase(it);}
  }else if(m_Mode==Mode::Client&&h.type==HostShutdown){
   if(from.sin_addr.s_addr==m_Server.address&&ntohs(from.sin_port)==m_Server.port){m_KickedByHost=true;CloseSocket(s);m_Socket=InvalidSocket;m_Mode=Mode::Offline;m_RemoteTransforms.clear();m_Messages.clear();m_Server={};m_LocalPlayerID=0;break;}
  }else if(h.type==Message&&n>=(int)(sizeof(PacketHeader)+8)){
   MessagePacket p{};std::memcpy(&p,b,std::min<int>(n,sizeof(p)));if(p.size>MaxMessagePayload||n<int(sizeof(PacketHeader)+8+p.size))continue;
   if(m_Mode==Mode::Host){auto sender=std::find_if(m_Clients.begin(),m_Clients.end(),[&](const Endpoint&e){return e.address==from.sin_addr.s_addr&&e.port==ntohs(from.sin_port)&&e.playerID==p.senderID;});if(sender==m_Clients.end())continue;}
   const std::string payload(p.payload,p.payload+p.size);m_Messages.push_back({p.senderID,p.channel,payload});
   if(m_Mode==Mode::Host)for(const auto& c:m_Clients)if(c.playerID!=p.senderID)SendMessageTo(c,p.senderID,p.channel,payload);
  }else if(h.type==Transform&&n>=(int)sizeof(TransformPacket)){
   TransformPacket p{};std::memcpy(&p,b,sizeof(p));if(p.playerID==GetLocalPlayerID())continue;if(!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(p.z)||!std::isfinite(p.rx)||!std::isfinite(p.ry)||!std::isfinite(p.rz))continue;
   if(m_Mode==Mode::Host){auto sender=std::find_if(m_Clients.begin(),m_Clients.end(),[&](const Endpoint&e){return e.address==from.sin_addr.s_addr&&e.port==ntohs(from.sin_port)&&e.playerID==p.playerID;});if(sender==m_Clients.end())continue;}
   const auto lastSequence=m_LastRemoteTransformSequence.find(p.playerID);
   if(lastSequence!=m_LastRemoteTransformSequence.end()&&p.sequence<=lastSequence->second)continue;
   m_LastRemoteTransformSequence[p.playerID]=p.sequence;
   NetworkTransformState st{p.playerID,p.x,p.y,p.z,p.rx,p.ry,p.rz};m_RemoteTransforms[p.playerID]=st;
   if(m_Mode==Mode::Host){
    const std::uint32_t hostSequence=m_LocalTransformSequence;
    m_LocalTransformSequence=p.sequence;
    for(const auto& c:m_Clients)if(c.playerID!=p.playerID)SendTransformTo(c,st);
    m_LocalTransformSequence=hostSequence;
   }
  }
 }
}
void NetworkManager::Disconnect(){
 StopServerSearch();
 m_Servers.clear();
 if(m_Mode==Mode::Host&&m_Socket!=InvalidSocket){PacketHeader p{Magic,HostShutdown};for(const auto& c:m_Clients){sockaddr_in to{};to.sin_family=AF_INET;to.sin_addr.s_addr=c.address;to.sin_port=htons(c.port);for(int i=0;i<3;++i)sendto(static_cast<SocketHandle>(m_Socket),reinterpret_cast<const char*>(&p),sizeof(p),0,reinterpret_cast<sockaddr*>(&to),sizeof(to));}}
 if(m_Mode==Mode::Client&&m_Socket!=InvalidSocket){PacketHeader p{Magic,Goodbye};sockaddr_in to{};to.sin_family=AF_INET;to.sin_addr.s_addr=m_Server.address;to.sin_port=htons(m_Server.port);sendto(static_cast<SocketHandle>(m_Socket),reinterpret_cast<const char*>(&p),sizeof(p),0,reinterpret_cast<sockaddr*>(&to),sizeof(to));}
 if(static_cast<SocketHandle>(m_Socket)!=InvalidSocket)CloseSocket(static_cast<SocketHandle>(m_Socket));
#ifdef _WIN32
 m_Socket=~(std::uintptr_t)0;
#else
 m_Socket=-1;
#endif
 m_Mode=Mode::Offline;m_Clients.clear();m_RemoteTransforms.clear();m_LastRemoteTransformSequence.clear();m_Messages.clear();m_Server={};m_LocalPlayerID=0;m_NextPlayerID=2;m_LocalTransformSequence=0;
}
