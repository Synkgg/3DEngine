#include "NetworkManager.h"
#include "../Core/Logger.h"
#include <SDL3/SDL.h>
extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

int RunNetworkSmokeTest()
{
    bool ok=true;
    auto check=[&](bool pass,const char* name){Logger::Info(std::string(pass?"PASS: ":"FAIL: ")+name);ok&=pass;};
    NetworkManager host,client,third;
    check(host.Host(0,2),"bind ephemeral host socket");
    check(client.Join("127.0.0.1",host.GetBoundPort()),"start client handshake");
    auto pump=[&](){for(int i=0;i<20;++i){host.Update();client.Update();third.Update();SDL_Delay(2);}};
    pump();
    check(client.IsHandshakeComplete()&&client.GetLocalPlayerID()==2&&host.GetPlayerCount()==2,"two-player handshake");
    client.SendNetworkMessage(20,"SHOT:1:1:rifle:0:1:0:0:0:-1");pump();
    auto messages=host.ConsumeMessages();
    check(messages.size()==1&&messages[0].senderID==2,"authenticated client gameplay message");
    host.SendNetworkMessage(20,"STATE:1:2:900:1:100:100:0:0:0:0");pump();
    messages=client.ConsumeMessages();check(messages.size()==1&&messages[0].senderID==1,"host state replication");
    client.SendLocalTransform({2,4,1,-14,0,90,0});pump();
    check(host.GetRemoteTransforms().contains(2)&&host.GetRemoteTransforms().at(2).z==-14,"remote transform replication");
    third.Join("127.0.0.1",host.GetBoundPort());pump();
    check(!third.IsConnected()&&host.GetPlayerCount()==2,"full duel rejects third player");
    client.Disconnect();pump();check(host.GetPlayerCount()==1,"disconnect frees slot");
    client.Join("127.0.0.1",host.GetBoundPort());pump();
    check(client.GetLocalPlayerID()==2&&host.GetPlayerCount()==2,"rejoin reuses duel slot two");
    host.Disconnect();pump();check(!client.IsConnected(),"host shutdown reaches client");
    lua_State* lua=luaL_newstate();luaL_openlibs(lua);
    if(luaL_dofile(lua,"Projects/DuelFPS/Tests/Gameplay.lua")!=LUA_OK){Logger::Error(lua_tostring(lua,-1));ok=false;}
    lua_close(lua);
    return ok?0:1;
}
