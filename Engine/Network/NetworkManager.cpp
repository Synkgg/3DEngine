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
using SocketHandle=SOCKET; constexpr SocketHandle InvalidSocket=INVALID_SOCKET;
#else
using SocketHandle=int; constexpr SocketHandle InvalidSocket=-1;
#endif
constexpr std::uint32_t Magic=0x4B545256; // VRTK
enum : std::uint8_t { Hello=1, Welcome=2, Transform=3, Goodbye=4, GameState=5, GameAction=6, HostShutdown=7 };
#pragma pack(push,1)
struct PacketHeader { std::uint32_t magic; std::uint8_t type; };
struct WelcomePacket { PacketHeader header; std::uint32_t playerID; };
struct TransformPacket { PacketHeader header; std::uint32_t playerID; float x,y,z,rx,ry,rz; };
struct GameStatePacket { PacketHeader header; std::uint32_t revision; std::int32_t redScore,blueScore,roundSeconds; float orbX,orbY,orbZ; std::uint32_t carrierID; std::int32_t winner,matchStarted; };
struct GameActionPacket { PacketHeader header; std::uint32_t playerID; std::uint8_t action; };
#pragma pack(pop)
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
#ifdef _WIN32
 static bool ready=false; if(!ready){WSADATA d{};if(WSAStartup(MAKEWORD(2,2),&d)!=0){SetError("WSAStartup failed.");return false;}ready=true;}
#endif
 SocketHandle s=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP); if(s==InvalidSocket){SetError("Could not create UDP socket.");return false;}
 sockaddr_in local{};local.sin_family=AF_INET;local.sin_addr.s_addr=htonl(INADDR_ANY);local.sin_port=htons(port);
 if(bind(s,reinterpret_cast<sockaddr*>(&local),sizeof(local))!=0){CloseSocket(s);SetError("Could not bind UDP port "+std::to_string(port)+".");return false;}
#ifdef _WIN32
 u_long nb=1;ioctlsocket(s,FIONBIO,&nb);m_Socket=static_cast<std::uintptr_t>(s);
#else
 fcntl(s,F_SETFL,fcntl(s,F_GETFL,0)|O_NONBLOCK);m_Socket=s;
#endif
 return true;
}
bool NetworkManager::Host(std::uint16_t port){Disconnect();if(!OpenSocket(port))return false;m_Mode=Mode::Host;m_LocalPlayerID=1;m_LastError.clear();Logger::Info("Network: hosting on UDP port "+std::to_string(port)+".");return true;}
bool NetworkManager::Join(const std::string& address,std::uint16_t port){Disconnect();if(!OpenSocket(0))return false;in_addr a{};if(inet_pton(AF_INET,address.c_str(),&a)!=1){Disconnect();SetError("Join currently requires an IPv4 address.");return false;}m_Server={a.s_addr,port,1};m_Mode=Mode::Client;m_LastError.clear();SendHello();Logger::Info("Network: joining "+address+":"+std::to_string(port)+".");return true;}
void NetworkManager::SendHello(){if(m_Mode!=Mode::Client)return;PacketHeader p{Magic,Hello};Endpoint e=m_Server;sockaddr_in to{};to.sin_family=AF_INET;to.sin_addr.s_addr=e.address;to.sin_port=htons(e.port);sendto(static_cast<SocketHandle>(m_Socket),reinterpret_cast<const char*>(&p),sizeof(p),0,reinterpret_cast<sockaddr*>(&to),sizeof(to));}
void NetworkManager::SendTransformTo(const Endpoint& e,const NetworkTransformState& s){TransformPacket p{{Magic,Transform},s.playerID,s.x,s.y,s.z,s.rx,s.ry,s.rz};sockaddr_in to{};to.sin_family=AF_INET;to.sin_addr.s_addr=e.address;to.sin_port=htons(e.port);sendto(static_cast<SocketHandle>(m_Socket),reinterpret_cast<const char*>(&p),sizeof(p),0,reinterpret_cast<sockaddr*>(&to),sizeof(to));}
void NetworkManager::SendGameStateTo(const Endpoint& e){
 GameStatePacket p{{Magic,GameState},m_GameState.revision,m_GameState.redScore,m_GameState.blueScore,m_GameState.roundSeconds,m_GameState.orbX,m_GameState.orbY,m_GameState.orbZ,m_GameState.carrierID,m_GameState.winner,m_GameState.matchStarted};
 sockaddr_in to{};to.sin_family=AF_INET;to.sin_addr.s_addr=e.address;to.sin_port=htons(e.port);
 sendto(static_cast<SocketHandle>(m_Socket),reinterpret_cast<const char*>(&p),sizeof(p),0,reinterpret_cast<sockaddr*>(&to),sizeof(to));
}
void NetworkManager::SetGameState(const NetworkGameState& state){
 if(m_Mode!=Mode::Host)return;m_GameState=state;for(const auto& client:m_Clients)SendGameStateTo(client);
}
void NetworkManager::SendGameAction(std::uint8_t action){
 if(m_Mode==Mode::Offline||!IsHandshakeComplete())return;
 if(m_Mode==Mode::Host){m_GameActions.emplace_back(GetLocalPlayerID(),action);return;}
 GameActionPacket p{{Magic,GameAction},GetLocalPlayerID(),action};sockaddr_in to{};to.sin_family=AF_INET;to.sin_addr.s_addr=m_Server.address;to.sin_port=htons(m_Server.port);sendto(static_cast<SocketHandle>(m_Socket),reinterpret_cast<const char*>(&p),sizeof(p),0,reinterpret_cast<sockaddr*>(&to),sizeof(to));
}
std::vector<std::pair<std::uint32_t,std::uint8_t>> NetworkManager::ConsumeGameActions(){auto actions=std::move(m_GameActions);m_GameActions.clear();return actions;}
bool NetworkManager::WasKickedByHost(){const bool value=m_KickedByHost;m_KickedByHost=false;return value;}
void NetworkManager::SendLocalTransform(const NetworkTransformState& state){
 if(m_Mode==Mode::Offline||!IsHandshakeComplete())return;NetworkTransformState s=state;s.playerID=GetLocalPlayerID();
 if(m_Mode==Mode::Host){for(const auto& c:m_Clients)SendTransformTo(c,s);}else SendTransformTo(m_Server,s);
}
void NetworkManager::Update(){
 if(m_Mode==Mode::Offline)return;SocketHandle s=static_cast<SocketHandle>(m_Socket);char b[256];sockaddr_in from{};
 // Keep networking from monopolizing an entire render frame if packets
 // arrive faster than they can be consumed. Remaining UDP packets stay queued
 // for the next update.
 constexpr int MaxPacketsPerUpdate=128;
 for(int packetIndex=0;packetIndex<MaxPacketsPerUpdate;++packetIndex){
#ifdef _WIN32
  int len=sizeof(from);
#else
  socklen_t len=sizeof(from);
#endif
  int n=(int)recvfrom(s,b,sizeof(b),0,reinterpret_cast<sockaddr*>(&from),&len);if(n<=0)break;if(n<(int)sizeof(PacketHeader))continue;
  PacketHeader h{};std::memcpy(&h,b,sizeof(h));if(h.magic!=Magic)continue;
  if(m_Mode==Mode::Host&&h.type==Hello){
   auto it=std::find_if(m_Clients.begin(),m_Clients.end(),[&](const Endpoint&e){return e.address==from.sin_addr.s_addr&&e.port==ntohs(from.sin_port);});
   if(it==m_Clients.end()){m_Clients.push_back({from.sin_addr.s_addr,ntohs(from.sin_port),m_NextPlayerID++});it=std::prev(m_Clients.end());Logger::Info("Network: client connected as player "+std::to_string(it->playerID)+".");}
   WelcomePacket w{{Magic,Welcome},it->playerID};sendto(s,reinterpret_cast<const char*>(&w),sizeof(w),0,reinterpret_cast<sockaddr*>(&from),len);SendGameStateTo(*it);
  }else if(m_Mode==Mode::Client&&h.type==Welcome&&n>=(int)sizeof(WelcomePacket)){
   WelcomePacket w{};std::memcpy(&w,b,sizeof(w));if(m_LocalPlayerID==0)Logger::Info("Network: joined as player "+std::to_string(w.playerID)+".");m_LocalPlayerID=w.playerID;
  }else if(m_Mode==Mode::Host&&h.type==Goodbye){
   auto it=std::find_if(m_Clients.begin(),m_Clients.end(),[&](const Endpoint&e){return e.address==from.sin_addr.s_addr&&e.port==ntohs(from.sin_port);});
   if(it!=m_Clients.end()){const std::uint32_t playerID=it->playerID;m_RemoteTransforms.erase(playerID);m_Clients.erase(it);Logger::Info("Network: player "+std::to_string(playerID)+" disconnected.");}
  }else if(m_Mode==Mode::Client&&h.type==HostShutdown){
   if(from.sin_addr.s_addr==m_Server.address&&ntohs(from.sin_port)==m_Server.port){Logger::Info("Network: host ended the match.");m_KickedByHost=true;CloseSocket(s);m_Socket=InvalidSocket;m_Mode=Mode::Offline;m_RemoteTransforms.clear();m_Server={};m_LocalPlayerID=0;m_GameState={};m_GameActions.clear();break;}
  }else if(m_Mode==Mode::Client&&h.type==GameState&&n>=(int)sizeof(GameStatePacket)){
   GameStatePacket p{};std::memcpy(&p,b,sizeof(p));if(p.revision>=m_GameState.revision)m_GameState={p.revision,p.redScore,p.blueScore,p.roundSeconds,p.orbX,p.orbY,p.orbZ,p.carrierID,p.winner,p.matchStarted};
  }else if(m_Mode==Mode::Host&&h.type==GameAction&&n>=(int)sizeof(GameActionPacket)){
   GameActionPacket p{};std::memcpy(&p,b,sizeof(p));auto sender=std::find_if(m_Clients.begin(),m_Clients.end(),[&](const Endpoint&e){return e.address==from.sin_addr.s_addr&&e.port==ntohs(from.sin_port)&&e.playerID==p.playerID;});if(sender!=m_Clients.end())m_GameActions.emplace_back(p.playerID,p.action);
  }else if(h.type==Transform&&n>=(int)sizeof(TransformPacket)){
   TransformPacket p{};std::memcpy(&p,b,sizeof(p));if(p.playerID==GetLocalPlayerID())continue;NetworkTransformState st{p.playerID,p.x,p.y,p.z,p.rx,p.ry,p.rz};m_RemoteTransforms[p.playerID]=st;
   if(m_Mode==Mode::Host){for(const auto& c:m_Clients)if(c.playerID!=p.playerID)SendTransformTo(c,st);}
  }
 }
}
void NetworkManager::Disconnect(){
 if(m_Mode==Mode::Host&&m_Socket!=InvalidSocket){
  PacketHeader p{Magic,HostShutdown};
  for(const auto& client:m_Clients){sockaddr_in to{};to.sin_family=AF_INET;to.sin_addr.s_addr=client.address;to.sin_port=htons(client.port);for(int i=0;i<3;++i)sendto(static_cast<SocketHandle>(m_Socket),reinterpret_cast<const char*>(&p),sizeof(p),0,reinterpret_cast<sockaddr*>(&to),sizeof(to));}
 }
 if(m_Mode==Mode::Client&&m_Socket!=InvalidSocket){
  PacketHeader p{Magic,Goodbye};sockaddr_in to{};to.sin_family=AF_INET;to.sin_addr.s_addr=m_Server.address;to.sin_port=htons(m_Server.port);
  sendto(static_cast<SocketHandle>(m_Socket),reinterpret_cast<const char*>(&p),sizeof(p),0,reinterpret_cast<sockaddr*>(&to),sizeof(to));
 }
 if(m_Mode!=Mode::Offline)CloseSocket(static_cast<SocketHandle>(m_Socket));
#ifdef _WIN32
 m_Socket=~(std::uintptr_t)0;
#else
 m_Socket=-1;
#endif
 m_Mode=Mode::Offline;m_Clients.clear();m_RemoteTransforms.clear();m_Server={};m_LocalPlayerID=0;m_NextPlayerID=2;m_GameState={};m_GameActions.clear();
}
