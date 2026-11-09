#include "RHISmokeTest.h"
#include "RHI.h"
#include "../Renderer.h"
#include "../../Platform/SDL/Window.h"
#include "../../Core/Logger.h"
#include <SDL3/SDL.h>
#include <imgui.h>
#include "../Texture2D.h"
#include <fstream>
#include <map>
#include <cmath>
#include <algorithm>
#include <cstring>
#include "../../UI/UIText.h"
#include "../../UI/UITextInput.h"
#include "../../UI/UISlider.h"
#include "../../Audio/AudioEngine.h"
#include "../../UI/UISerializer.h"
#include "../../Scene/Runtime/Runtime.h"
#include "../../Core/ProjectManager.h"
#include "../../Platform/SDL/Input.h"
#include "../../Scene/Components/TransformComponent.h"
#include "../../Scene/Components/ColliderComponent.h"
#include "../../UI/UIButton.h"
#include "../../UI/UIProgressBar.h"

#include "../../Scene/Scene.h"
#include "../../Scene/SceneSerializer.h"
#include "../../Scene/Systems/CollisionSystem.h"
#include "../../Editor/HierarchyFolder.h"
#include "../../Scene/Components/MeshComponent.h"
#include "../../Scene/Components/MaterialComponent.h"
#include "../../Scene/Components/ColorComponent.h"
#include "../../Scene/Components/LightComponent.h"
#include "../../Scene/Components/TextureComponent.h"

bool RunUIInteractionTests(Renderer& renderer);

namespace
{
    float Channel(const std::vector<uint8_t>& pixels, int x, int y, int channel)
    {
        if (pixels.size() != 640 * 480 * 8) return -1;
        uint16_t h; std::memcpy(&h, pixels.data() + (y * 640 + x) * 8 + channel * 2, 2);
        return std::ldexp(float((h & 1023) + ((h & 0x7c00) ? 1024 : 0)), int((h >> 10) & 31) - 25);
    }

    bool CheckEmberwildRuntime(Renderer& renderer)
    {
        ProjectManager project;
        if(!project.Load("Projects/Emberwild/Emberwild.project"))return false;
        renderer.SetProjectRoot(std::filesystem::absolute("Projects/Emberwild"));
        Scene hs,cs;std::vector<HierarchyFolder> folders;
        SceneSerializer hl(hs),cl(cs);
        if(!hl.Load(project.ResolveAssetPath("Assets/Scenes/MainMenu.scene"),folders)||
           !cl.Load(project.ResolveAssetPath("Assets/Scenes/MainMenu.scene"),folders))return false;
        Runtime h,c;Input hi,ci;UICanvas hu,cu;h.SetProjectManager(&project);c.SetProjectManager(&project);
        h.Start(hs,renderer,hi,hu);c.Start(cs,renderer,ci,cu);
        auto capture=[&](Scene& scene,UICanvas& canvas,const char* path,bool world){
            ImGui::NewFrame();ImGui::Render();renderer.BeginFrame();
            if(world){
                renderer.SetDirectionalLight(Vec3(-.48f,-.78f,-.38f),Vec3(1,.88f,.7f),2.f);
                renderer.DrawSky();
                for(auto e:scene.GetEntities())if(auto* mesh=scene.GetComponent<MeshComponent>(e)) {
                    auto transform=scene.GetWorldTransform(e);
                    renderer.DrawModel(transform,mesh->modelPath,1,1,1,1);
                }
            }
            renderer.EndScene();renderer.BeginOverlay();
            auto& ui=renderer.GetUIRenderer();ui.SetLogicalSize(1920,1080);ui.Begin();ui.RenderCanvas(canvas,&renderer);ui.End();
            renderer.EndOverlay();renderer.EndFrame();std::vector<uint8_t> pixels;
            if(!Velcryn::RHI::GetDevice()->ReadTexture(renderer.GetSceneColorTexture(),pixels))return false;
            std::ofstream image(path,std::ios::binary);image<<"P6\n640 480\n255\n";
            for(int y=0;y<480;++y)for(int x=0;x<640;++x)for(int ch=0;ch<3;++ch)
                image.put(char(std::clamp(Channel(pixels,x,y,ch),0.f,1.f)*255));
            return true;
        };
        bool ok=capture(hs,hu,"out/build/emberwild-menu.ppm",false);
        auto* host=dynamic_cast<UIButton*>(hu.GetRoot()->Find("HostButton"));
        if(!host)ok=false;else host->SetClicked(true);
        for(int i=0;i<10;++i)h.Update(hs,renderer,hi,1.f/60);
        auto* join=dynamic_cast<UIButton*>(cu.GetRoot()->Find("JoinButton"));
        if(!join)ok=false;else join->SetClicked(true);
        for(int i=0;i<180;++i){h.Update(hs,renderer,hi,1.f/60);c.Update(cs,renderer,ci,1.f/60);SDL_Delay(1);}
        auto* objective=dynamic_cast<UIText*>(cu.GetRoot()->Find("Objective"));
        ok=ok&&hs.FindEntityByName("Survivor").IsValid()&&cs.FindEntityByName("Survivor").IsValid()&&
            hs.FindEntityByName("Partner").IsValid()&&cs.FindEntityByName("Partner").IsValid()&&
            objective&&objective->GetText().find("DAY 1")!=std::string::npos;
        auto* survivor=cs.GetComponent<TransformComponent>(cs.FindEntityByName("Survivor"));
        if(survivor)survivor->transform.position=Vec3(-7,1,4);else ok=false;
        for(int i=0;i<30;++i){h.Update(hs,renderer,hi,1.f/60);c.Update(cs,renderer,ci,1.f/60);}
        c.GetNetwork().SendNetworkMessage(41,"1:use");
        c.GetNetwork().SendNetworkMessage(41,"1:use");
        for(int i=0;i<30;++i){h.Update(hs,renderer,hi,1.f/60);c.Update(cs,renderer,ci,1.f/60);}
        auto* inventory=dynamic_cast<UIText*>(cu.GetRoot()->Find("Inventory"));
        if(inventory)Logger::Info("Emberwild client inventory: "+inventory->GetText());
        ok=ok&&inventory&&inventory->GetText().find("WOOD 2")!=std::string::npos;
        renderer.SetCameraPosition(Vec3(0,2,8));renderer.SetCameraRotation(-1.57f,-.1f);
        ok=capture(hs,hu,"out/build/emberwild-forest.ppm",true)&&ok;
        c.Stop(cs);h.Stop(hs);renderer.SetProjectRoot({});renderer.ResetCamera();
        Logger::Info(ok?"PASS: Emberwild real menu, host/join, two live runtimes, partner replication and rendered HUD":"FAIL: Emberwild live runtime");
        return ok;
    }

    bool CheckTeamMatch(Renderer& renderer, ProjectManager& project, int size, int matchCount=1)
    {
        struct TestClient {
            Scene scene;
            Runtime runtime;
            Input input;
            UICanvas ui;
        };
        std::vector<std::unique_ptr<TestClient>> clients;
        const int rosterSize=2*size;
        for(int i=0;i<rosterSize*matchCount;++i){
            auto client=std::make_unique<TestClient>();
            std::vector<HierarchyFolder> folders;
            SceneSerializer loader(client->scene);
            if(!loader.Load(project.ResolveAssetPath("Assets/Scenes/MainMenu.scene"),folders)) return false;
            client->runtime.SetProjectManager(&project);
            client->runtime.SetStateBool("duel_test_loopback",true);
            client->runtime.Start(client->scene,renderer,client->input,client->ui);
            clients.push_back(std::move(client));
        }
        auto tick=[&](int frames){
            for(int i=0;i<frames;++i){
                for(auto& c:clients) c->runtime.Update(c->scene,renderer,c->input,1.f/60);
                SDL_Delay(16);
            }
        };
        auto clickAll=[&](const char* name){
            for(auto& c:clients){
                auto* button=dynamic_cast<UIButton*>(c->ui.GetRoot()->Find(name));
                if(!button) return false;
                button->SetClicked(true);
            }
            tick(3);
            return true;
        };
        bool ok=true;
        for(int i=1;i<size;++i) ok=ok && clickAll("ModeButton");
        ok=ok && clickAll("PlayButton");
        tick(260);
        std::map<int,std::vector<TestClient*>> rosters;
        for(auto& c:clients){
            const int code=static_cast<int>(c->runtime.GetStateNumber("duel_cloud_code"));
            auto& slots=rosters.try_emplace(code,rosterSize+1,nullptr).first->second;
            const auto slot=c->runtime.GetNetwork().GetLocalPlayerID();
            if(slot==0 || slot>=slots.size() || slots[slot] ||
               c->runtime.GetStateNumber("duel_team_size")!=size ||
               c->runtime.GetNetwork().GetRemoteTransforms().size()!=static_cast<size_t>(rosterSize-1) ||
               !c->scene.FindEntityByName("Remote Operator").IsValid()){
                ok=false;
                break;
            }
            slots[slot]=c.get();
        }
        auto position=[](TestClient& c,Vec3 p){
            auto* transform=c.scene.GetComponent<TransformComponent>(c.scene.FindEntityByName("Operator"));
            if(transform) transform->transform.position=p;
            return transform!=nullptr;
        };
        // Slot one faces down the empty west lane. Move one opponent at a
        // time into view and check that only the final elimination scores.
        ok=ok && rosters.size()==static_cast<size_t>(matchCount);
        for(auto& [code,slots]:rosters){
            for(int slot=1;ok && slot<=rosterSize;++slot)
                ok=slots[slot] && slots[slot]->runtime.GetStateNumber("duel_score")==0 &&
                   slots[slot]->runtime.GetStateNumber("duel_opponent_score")==0 &&
                   position(*slots[slot],slot==1 ? Vec3(-12.5f,1,5) : Vec3(2.f*slot,1,12));
            for(int enemy=2;ok && enemy<=rosterSize;enemy+=2){
                ok=position(*slots[enemy],Vec3(-12.5f,1,-5));
                tick(30);
                slots[1]->input.UpdateMouseState(0,0,SDL_BUTTON_LMASK);
                tick(56);
                slots[1]->input.UpdateMouseState(0,0,0);
                tick(16);
                ok=ok && slots[enemy]->runtime.GetStateNumber("duel_health")==0;
                const int expected=enemy==rosterSize ? 1 : 0;
                for(int slot=1;slot<=rosterSize;++slot){
                    const char* key=slot%2 ? "duel_score" : "duel_opponent_score";
                    ok=ok && slots[slot]->runtime.GetStateNumber(key)==expected;
                }
            }
            // Release this worker before scoring in the next match.
            for(auto* player:slots) if(player) player->runtime.Stop(player->scene);
        }
        for(auto& c:clients) c->runtime.Stop(c->scene);
        renderer.SetProjectRoot({});renderer.ResetCamera();
        Logger::Info(std::string(ok?"PASS: ":"FAIL: ")+std::to_string(size)+"v"+std::to_string(size)+" matchmaking, replication and team elimination");
        return ok;
    }

    bool CheckBreakbulkRuntime(Renderer& renderer)
    {
        // UI tests apply preferences through the real game APIs. Preserve the
        // player's files even on an early test failure and use default settings.
        struct PreferencesGuard {
            std::filesystem::path folder="Projects/DuelFPS/Saved";
            std::map<std::filesystem::path,std::string> files;
            PreferencesGuard() {
                if(std::filesystem::exists(folder))for(const auto& entry:std::filesystem::directory_iterator(folder))
                    if(entry.is_regular_file() && entry.path().extension()==".txt") {
                        std::ifstream input(entry.path(),std::ios::binary);
                        files[entry.path()]={std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>()};
                    }
                std::error_code error;
                std::filesystem::remove(folder/"duel_settings_v1.txt",error);
            }
            ~PreferencesGuard() {
                std::error_code error;
                if(std::filesystem::exists(folder,error))for(const auto& entry:std::filesystem::directory_iterator(folder,error))
                    if(entry.path().extension()==".txt" && !files.contains(entry.path()))std::filesystem::remove(entry.path(),error);
                for(const auto& [path,data]:files){std::ofstream output(path,std::ios::binary);output<<data;}
            }
        } preferencesGuard;
        ProjectManager project;
        if(!project.Load("Projects/DuelFPS/DuelFPS.project"))return false;
        renderer.SetProjectRoot(std::filesystem::absolute("Projects/DuelFPS"));
        if(const char* mode=SDL_getenv("DUELFPS_TEST_CLOUD")){
            if(std::strcmp(mode,"2v2")==0) return CheckTeamMatch(renderer,project,2);
            if(std::strcmp(mode,"3v3")==0) return CheckTeamMatch(renderer,project,3);
            if(std::strcmp(mode,"multi")==0) return CheckTeamMatch(renderer,project,3,2);
        }
        // Import the actual six meshes referenced by the shipping viewmodels.
        for(const char* weapon:{"pistol","rifle","shotgun","smg","sniper","marksman","pistol2","rifle2","shotgun2","smg2","sniper2","marksman2"}) {
            if(!renderer.GetModelAsset(std::string("Assets/Models/Tactical/")+weapon+".obj")) {
                Logger::Error(std::string("BREAKBULK: cannot import weapon mesh: ")+weapon);
                return false;
            }
        }
        Scene menu;std::vector<HierarchyFolder> folders;SceneSerializer menuLoader(menu);
        if(!menuLoader.Load(project.ResolveAssetPath("Assets/Scenes/MainMenu.scene"),folders))return false;
        Input menuInput;UICanvas menuUI;Runtime menuRuntime;menuRuntime.SetProjectManager(&project);
        menuRuntime.Start(menu,renderer,menuInput,menuUI);
        auto captureUI=[&](const char* path){
            ImGui::NewFrame();ImGui::Render();renderer.BeginFrame();
            if(menu.FindEntityByName("PracticeMode").IsValid()){
                renderer.SetDirectionalLight(Vec3(-.48f,-.78f,-.38f),Vec3(1,.82f,.64f),1.35f);
                for(int i=0;i<Renderer::ShadowCascadeCount;++i)if(renderer.BeginShadowPass(i)){
                    for(auto e:menu.GetEntities())if(auto* m=menu.GetComponent<MeshComponent>(e)){
                        auto t=menu.GetWorldTransform(e);if(m->modelPath.empty())renderer.DrawShadowMesh(t,m->primitive);else renderer.DrawShadowModel(t,m->modelPath);}
                    renderer.EndShadowPass();}
                renderer.DrawSky();
                for(auto e:menu.GetEntities())if(auto* mesh=menu.GetComponent<MeshComponent>(e)){
                    auto t=menu.GetWorldTransform(e);auto* c=menu.GetComponent<ColorComponent>(e);ColorComponent white;if(!c)c=&white;
                    auto* texture=menu.GetComponent<TextureComponent>(e);
                    if(mesh->modelPath.empty())renderer.DrawMesh(t,mesh->primitive,c->r,c->g,c->b,c->a,texture?renderer.LoadTexture(texture->path):nullptr);
                    else renderer.DrawModel(t,mesh->modelPath,c->r,c->g,c->b,c->a);
                }
            }
            renderer.EndScene();renderer.BeginOverlay();
            auto& ui=renderer.GetUIRenderer();ui.SetLogicalSize(1920,1080);ui.Begin();ui.RenderCanvas(menuUI,&renderer);ui.End();renderer.EndOverlay();renderer.EndFrame();
            std::vector<uint8_t> pixels;if(!Velcryn::RHI::GetDevice()->ReadTexture(renderer.GetSceneColorTexture(),pixels))return;
            std::ofstream image(path,std::ios::binary);image<<"P6\n640 480\n255\n";
            for(int y=0;y<480;++y)for(int x=0;x<640;++x)for(int ch=0;ch<3;++ch)image.put(char(std::clamp(Channel(pixels,x,y,ch),0.f,1.f)*255));
        };
        auto stepMenu=[&](int count=3){for(int i=0;i<count;++i)menuRuntime.Update(menu,renderer,menuInput,1.f/60);};
        auto clickMenu=[&](const char* name){auto* b=dynamic_cast<UIButton*>(menuUI.GetRoot()->Find(name));if(!b)return false;b->SetClicked(true);stepMenu();return true;};
        AudioEngine settingsAudio;menuRuntime.SetAudioEngine(&settingsAudio);
        if(!clickMenu("SettingsButton")){menuRuntime.Stop(menu);return false;}
        auto* sensitivity=dynamic_cast<UISlider*>(menuUI.GetRoot()->Find("Setting_sensitivity"));
        if(!sensitivity){Logger::Error("Settings canvas did not open");menuRuntime.Stop(menu);return false;}
        sensitivity->SetValue(.25f);stepMenu();
        if(std::abs(menuRuntime.GetStateNumber("mouse_sensitivity")-.007)>.0001){menuRuntime.Stop(menu);return false;}
        if(!clickMenu("Bind_fireButton")){menuRuntime.Stop(menu);return false;}
        stepMenu(15);menuInput.UpdateMouseState(0,0,SDL_BUTTON_X1MASK);stepMenu(1);menuInput.UpdateMouseState(0,0,0);stepMenu();
        auto* binding=dynamic_cast<UIText*>(menuUI.GetRoot()->Find("Bind_fireLabel"));
        if(!binding||binding->GetText()!="Mouse side 1"){Logger::Error("Settings mouse rebind failed");menuRuntime.Stop(menu);return false;}
        captureUI("out/build/duel-settings-controls.ppm");
        clickMenu("TabAudioButton");
        auto* master=dynamic_cast<UISlider*>(menuUI.GetRoot()->Find("Setting_master"));
        if(!master){menuRuntime.Stop(menu);return false;}
        master->SetValue(.35f);stepMenu();
        if(std::abs(settingsAudio.GetMasterVolume()-.35f)>.001f){menuRuntime.Stop(menu);return false;}
        captureUI("out/build/duel-settings-audio.ppm");
        clickMenu("TabGraphicsButton");clickMenu("Toggle_qualityButton");
        if(renderer.GetRenderSettings().shadowQuality!=0){Logger::Error("Low quality preset did not apply");menuRuntime.Stop(menu);return false;}
        captureUI("out/build/duel-settings-graphics.ppm");
        clickMenu("SettingsApplyButton");clickMenu("SettingsButton");clickMenu("TabAudioButton");
        master=dynamic_cast<UISlider*>(menuUI.GetRoot()->Find("Setting_master"));
        if(!master||std::abs(master->GetValue()-.35f)>.001f){menuRuntime.Stop(menu);return false;}
        master->SetValue(.9f);stepMenu();clickMenu("SettingsCancelButton");
        if(std::abs(settingsAudio.GetMasterVolume()-.35f)>.001f){menuRuntime.Stop(menu);return false;}
        // Restore defaults so later shooting regression uses the normal trigger.
        clickMenu("SettingsButton");clickMenu("SettingsResetButton");clickMenu("SettingsApplyButton");
        Logger::Info("PASS: real settings controls, mouse capture, audio preview, quality, apply, reopen and cancel");
        auto* armory=dynamic_cast<UIButton*>(menuUI.GetRoot()->Find("LoadoutButton"));
        if(!armory){menuRuntime.Stop(menu);return false;}
        armory->SetClicked(true);
        for(int i=0;i<10;++i)menuRuntime.Update(menu,renderer,menuInput,1.f/60);
        auto shown=[&](const char* id){auto* w=menuUI.GetRoot()->Find(id);return w&&w->IsVisible();};
        if(!shown("Overview")||shown("Picker")){menuRuntime.Stop(menu);return false;}
        captureUI("out/build/duel-loadout.ppm");
        if(!clickMenu("SecondaryTab")||!clickMenu("Category_sniper_Button")||shown("Overview")||!shown("Picker")||!shown("Choices_sniper")){menuRuntime.Stop(menu);return false;}
        captureUI("out/build/duel-picker.ppm");
        if(!clickMenu("sniper2Button")||!shown("Overview")||shown("Picker")||!shown("SecondaryIcon_sniper2")){menuRuntime.Stop(menu);return false;}
        if(!clickMenu("PrimaryTab")||!clickMenu("PickerBackButton")||!shown("Overview")){menuRuntime.Stop(menu);return false;}
        auto* back=dynamic_cast<UIButton*>(menuUI.GetRoot()->Find("BackButton"));
        if(!back){menuRuntime.Stop(menu);return false;}
        back->SetClicked(true);
        for(int i=0;i<10;++i)menuRuntime.Update(menu,renderer,menuInput,1.f/60);
        auto* play=dynamic_cast<UIButton*>(menuUI.GetRoot()->Find("CustomButton"));
        if(!play){menuRuntime.Stop(menu);return false;}
        play->SetClicked(true);
        for(int i=0;i<3;++i)menuRuntime.Update(menu,renderer,menuInput,1.f/60);
        if(!clickMenu("BackButton") || !clickMenu("DrillButton") || !clickMenu("MapButton")){menuRuntime.Stop(menu);return false;}
        auto* practice=dynamic_cast<UIButton*>(menuUI.GetRoot()->Find("PracticeButton"));
        if(!practice){menuRuntime.Stop(menu);return false;}
        practice->SetClicked(true);
        for(int i=0;i<10;++i)menuRuntime.Update(menu,renderer,menuInput,1.f/60);
        const bool hasWeapon=menu.FindEntityByName("KESTREL AR4 Viewmodel").IsValid() ||
            menu.FindEntityByName("MAKO P12 Viewmodel").IsValid() ||
            menu.FindEntityByName("BREACH S8 Viewmodel").IsValid() ||
            menu.FindEntityByName("VECTOR K9 Viewmodel").IsValid();
        const bool drill=menu.FindEntityByName("PracticeMode").IsValid()&&menu.FindEntityByName("Depot retaining wall").IsValid()&&menuUI.GetRoot()->Find("AmmoText")&&(hasWeapon||menu.FindEntityByName("MuzzleSocket").IsValid());
        captureUI("out/build/duel-depot-hud.ppm");
        if(!clickMenu("SettingsButton") || !menuUI.GetRoot()->Find("Setting_fov")) {
            Logger::Error("In-game settings did not open");menuRuntime.Stop(menu);return false;
        }
        clickMenu("SettingsCancelButton");
        auto* pause=menuUI.GetRoot()->Find("PauseMenu");
        if(!pause || !pause->IsVisible()){Logger::Error("Settings did not return to pause");menuRuntime.Stop(menu);return false;}
        clickMenu("ResumeButton");
        auto actor=menu.FindEntityByName("Operator");
        bool stance=CollisionSystem::ResizeStandingCollider(menu,actor,1.16f);
        auto p=menu.GetWorldTransform(actor).position;
        auto roof=menu.CreateEntity();
        menu.GetComponent<TransformComponent>(roof)->transform.position=p+Vec3(0,.84f,0);
        ColliderComponent obstacle;obstacle.width=2;obstacle.height=.1f;obstacle.depth=2;menu.AddComponent(roof,obstacle);
        stance=stance && !CollisionSystem::ResizeStandingCollider(menu,actor,1.8f);
        menu.DestroyEntity(roof);stance=stance&&CollisionSystem::ResizeStandingCollider(menu,actor,1.8f);
        menuRuntime.Stop(menu);
        if(!drill || !stance){Logger::Error("BREAKBULK: depot drill or crouch clearance failed");return false;}
        Scene hostScene,clientScene;SceneSerializer hl(hostScene),cl(clientScene);
        if(!hl.Load(project.ResolveAssetPath("Assets/Scenes/MainMenu.scene"),folders)||!cl.Load(project.ResolveAssetPath("Assets/Scenes/MainMenu.scene"),folders))return false;
        Runtime host,client;Input hi,ci;UICanvas hu,cu;host.SetProjectManager(&project);client.SetProjectManager(&project);
        const char* cloudMode = SDL_getenv("DUELFPS_TEST_CLOUD");
        const bool cloud = cloudMode != nullptr;
        const bool quick = cloud && std::strcmp(cloudMode,"quick")==0;
        host.SetStateBool("duel_test_loopback",cloud);
        client.SetStateBool("duel_test_loopback",cloud);
        host.Start(hostScene,renderer,hi,hu);
        client.Start(clientScene,renderer,ci,cu);
        auto tick=[&](){
            host.Update(hostScene,renderer,hi,1.f/60);
            client.Update(clientScene,renderer,ci,1.f/60);
            if(cloud) SDL_Delay(16);
        };
        auto hostMenu=[&](const char* name){auto* b=dynamic_cast<UIButton*>(hu.GetRoot()->Find(name));if(!b)return false;b->SetClicked(true);for(int i=0;i<4;++i)tick();return true;};
        if(quick ? !hostMenu("PlayButton") :
           !hostMenu("CustomButton")||hu.GetRoot()->Find("LimitButton")||
           (!cloud && !hostMenu("TransportButton")) ||
           !hostMenu("HostButton")||host.GetNetwork().IsConnected()||!hostMenu("MapButton")||!hostMenu("LimitButton")||!hostMenu("TimeButton")||
           !hostMenu("StartButton")){client.Stop(clientScene);host.Stop(hostScene);return false;}
        auto joinMenu=[&](const char* name){auto* b=dynamic_cast<UIButton*>(cu.GetRoot()->Find(name));if(!b)return false;b->SetClicked(true);for(int i=0;i<4;++i)tick();return true;};
        if(!joinMenu(quick ? "PlayButton" : "CustomButton")){client.Stop(clientScene);host.Stop(hostScene);return false;}
        if(cloud && !quick){
            for(int i=0;i<480 && !hostScene.FindEntityByName("Operator").IsValid();++i) tick();
            const int code = static_cast<int>(host.GetStateNumber("duel_cloud_code"));
            auto* address = dynamic_cast<UITextInput*>(cu.GetRoot()->Find("AddressField"));
            if(!address || code<10000000 || host.GetNetwork().IsHost()){
                Logger::Error("Cloud creator did not enter its dedicated match");
                client.Stop(clientScene);host.Stop(hostScene);return false;
            }
            address->SetText(std::to_string(code));
        }
        if(!quick && cloud && !joinMenu("JoinButton")){client.Stop(clientScene);host.Stop(hostScene);return false;}
        if(!cloud){
            bool found=joinMenu("TransportButton") && joinMenu("BrowseButton");
            for(int i=0;found && i<180;++i){
                auto* row=cu.GetRoot()->Find("ServerRow1");
                if(row && row->IsVisible()) break;
                tick(); SDL_Delay(2);
            }
            auto* row=cu.GetRoot()->Find("ServerRow1");
            if(!found || !row || !row->IsVisible() || !joinMenu("Server1Button")){
                client.Stop(clientScene);host.Stop(hostScene);return false;
            }
        }
        for(int i=0;i<240;++i)tick();
        auto* hp=hostScene.GetComponent<TransformComponent>(hostScene.FindEntityByName("Operator"));
        auto* cp=clientScene.GetComponent<TransformComponent>(clientScene.FindEntityByName("Operator"));
        bool ok=hp&&cp&&client.GetStateNumber("duel_limit")==(quick?5:7)&&client.GetStateNumber("duel_seconds")==(quick?90:120)&&clientScene.FindEntityByName("Depot retaining wall").IsValid()&&hostScene.FindEntityByName("Remote Operator").IsValid()&&clientScene.FindEntityByName("Remote Operator").IsValid();
        if(quick) ok=ok && !host.GetNetwork().IsHost() && !client.GetNetwork().IsHost() &&
            host.GetStateBool("duel_public_match",false) && client.GetStateBool("duel_public_match",false);
        if(ok){
            hp->transform.position=Vec3(-12.5f,1,5);cp->transform.position=Vec3(-12.5f,1,-5);
            for(int i=0;i<45;++i)tick();
            hi.UpdateMouseState(0,0,SDL_BUTTON_LMASK);
            for(int i=0;i<60;++i)tick();
            hi.UpdateMouseState(0,0,0);
            for(int i=0;i<20;++i)tick();
            ok=host.GetStateNumber("duel_score")==1&&client.GetStateNumber("duel_opponent_score")==1;
        }
        client.Stop(clientScene);host.Stop(hostScene);renderer.SetProjectRoot({});renderer.ResetCamera();
        if(ok)Logger::Info("BREAKBULK: actual Lua menu, drill, two live runtimes, physics hit, round win and replicated score passed");
        else Logger::Error("BREAKBULK: live runtime duel regression");
        return ok;
    }

    bool CheckPracticeRange(Renderer& renderer, bool terminal=false)
    {
        Scene scene;SceneSerializer serializer(scene);std::vector<HierarchyFolder> folders;
        if(!serializer.Load(terminal?"Projects/DuelFPS/Assets/Scenes/Arena.scene":"Projects/DuelFPS/Assets/Scenes/PracticeRange.scene",folders))return false;
        renderer.SetProjectRoot(std::filesystem::absolute("Projects/DuelFPS"));
        renderer.SetRenderSettings(RenderSettings{});renderer.ClearLocalLights();
        bool foundLight=false;
        for(auto entity:scene.GetEntities())if(auto* light=scene.GetComponent<LightComponent>(entity)) {
            if(light->type==LightType::Directional) {
                renderer.SetDirectionalLight(light->direction,light->color,light->intensity);
                foundLight=std::abs(light->intensity-(terminal?1.35f:1.4f))<.001f;
            }
        }
        renderer.SetCameraPosition(terminal?Vec3(22,21,27):Vec3(4,3,14));renderer.SetCameraRotation(-2.28f,terminal?-.55f:-.41f);
        ImGui::NewFrame();ImGui::Render();renderer.BeginFrame();
        auto draw=[&](bool shadow) {
            for(auto entity:scene.GetEntities()) {
                auto* mesh=scene.GetComponent<MeshComponent>(entity);if(!mesh)continue;
                auto transform=scene.GetWorldTransform(entity);transform.position=transform.position+mesh->offset;transform.rotation=transform.rotation+mesh->rotation;
                if(shadow) {
                    if(mesh->modelPath.empty())renderer.DrawShadowMesh(transform,mesh->primitive);
                    else renderer.DrawShadowModel(transform,mesh->modelPath);
                    continue;
                }
                const auto* color=scene.GetComponent<ColorComponent>(entity);ColorComponent white;
                if(!color)color=&white;
                const auto* material=scene.GetComponent<MaterialComponent>(entity);MaterialComponent fallback;
                const bool overrideMaterial=material!=nullptr;if(!material)material=&fallback;
                auto map=[&](const std::string& path)->Texture2D* {return path.empty()?nullptr:renderer.LoadTexture(path);};
                auto* texture=scene.GetComponent<TextureComponent>(entity);auto* base=texture?map(texture->path):nullptr;
                if(mesh->modelPath.empty())renderer.DrawMesh(transform,mesh->primitive,color->r,color->g,color->b,color->a,base,
                    material->metallic,material->roughness,material->ambientOcclusion,material->emissive,
                    map(material->normalMap),map(material->metallicMap),map(material->roughnessMap),map(material->aoMap),map(material->emissiveMap));
                else renderer.DrawModel(transform,mesh->modelPath,color->r,color->g,color->b,color->a,base,
                    material->metallic,material->roughness,material->ambientOcclusion,material->emissive,
                    map(material->normalMap),map(material->metallicMap),map(material->roughnessMap),map(material->aoMap),map(material->emissiveMap),overrideMaterial);
            }
        };
        for(int cascade=0;cascade<Renderer::ShadowCascadeCount;++cascade)if(renderer.BeginShadowPass(cascade)){draw(true);renderer.EndShadowPass();}
        renderer.DrawSky();draw(false);if(!terminal)renderer.DrawGrid();
        if(!terminal){renderer.DrawCollider(Transform{},1,1,1);renderer.AddDebugLine(Vec3(0,0,0),Vec3(0,2,0));renderer.DrawDebugLines(1.f/60);}
        renderer.EndScene();renderer.EndFrame();std::vector<uint8_t> pixels;
        const bool read=Velcryn::RHI::GetDevice()->ReadTexture(renderer.GetSceneColorTexture(),pixels);
        std::ofstream image(terminal?"out/build/breakbulk-terminal.ppm":"out/build/rhi-practice-range.ppm",std::ios::binary);image<<"P6\n640 480\n255\n";
        for(int y=0;y<480;++y)for(int x=0;x<640;++x)for(int c=0;c<3;++c)
            image.put(static_cast<char>(std::clamp(Channel(pixels,x,y,c),0.f,1.f)*255));
        if(terminal) {
            auto ground=CollisionSystem::Raycast(scene,Vec3(-4,4,14),Vec3(0,-1,0),10,Entity(1));
            auto blocked=CollisionSystem::Raycast(scene,Vec3(-4,1.85f,14),Vec3(8,0,-28).Normalized(),29,Entity(1));
            auto tunnel=CollisionSystem::Raycast(scene,Vec3(-8,1.4f,9),Vec3(0,0,-1),7,Entity(1));
            if(!ground.hit||std::abs(ground.point.y)>.05f||!blocked.hit||tunnel.hit){Logger::Error("BREAKBULK collision: floor, spawn cover, or open tunnel mismatch");return false;}
            renderer.SetCameraPosition(Vec3(-4,1.85f,14));renderer.SetCameraRotation(-1.38f,-.06f);
            ImGui::NewFrame();ImGui::Render();renderer.BeginFrame();
            for(int c=0;c<Renderer::ShadowCascadeCount;++c)if(renderer.BeginShadowPass(c)){draw(true);renderer.EndShadowPass();}
            renderer.DrawSky();draw(false);
            Transform weapon;const auto forward=renderer.GetCameraForward(),right=renderer.GetCameraRight();
            weapon.position=renderer.GetCameraPosition()+forward*.62f+right*.27f+Vec3(0,-.24f,0);
            weapon.rotation.y=std::atan2(-forward.x,-forward.z);
            renderer.DrawModel(weapon,"Assets/Models/Arsenal/KestrelAR4.obj",1,1,1,1);
            renderer.EndScene();
            UICanvas hud;
            if(!UISerializer::Load(hud,"Projects/DuelFPS/Assets/UI/Duel.ui")){Logger::Error("BREAKBULK HUD failed to load");return false;}
            for(const char* name:{"Hitmarker","ReloadText","CenterMessage"})if(auto* widget=hud.GetRoot()->Find(name))widget->SetVisible(false);
            auto& ui=renderer.GetUIRenderer();ui.SetLogicalSize(1920,1080);renderer.BeginOverlay();ui.Begin();ui.RenderCanvas(hud,&renderer);ui.End();renderer.EndOverlay();renderer.EndFrame();
            std::vector<uint8_t> frame;
            if(!Velcryn::RHI::GetDevice()->ReadTexture(renderer.GetSceneColorTexture(),frame))return false;
            std::ofstream capture("out/build/breakbulk-first-person.ppm",std::ios::binary);capture<<"P6\n640 480\n255\n";
            for(int y=0;y<480;++y)for(int x=0;x<640;++x)for(int c=0;c<3;++c)capture.put(char(std::clamp(Channel(frame,x,y,c),0.f,1.f)*255));
        }
        renderer.SetProjectRoot({});renderer.ResetCamera();
        if(!foundLight||!read)Logger::Error("RHI parity regression: real PracticeRange scene and directional light");
        else Logger::Info("RHI parity passed: real PracticeRange with shadows, MSAA, grid, debug lines and authored weapon materials");
        return foundLight&&read;
    }

    bool CheckRendererParity(Renderer& renderer)
    {
        auto* device=Velcryn::RHI::GetDevice();
        bool ok=true;
        auto check=[&](bool pass,const char* label) {
            if(pass) Logger::Info(std::string("RHI parity passed: ")+label);
            else {Logger::Error(std::string("RHI parity regression: ")+label);ok=false;}
        };
        auto difference=[](const std::vector<uint8_t>& a,const std::vector<uint8_t>& b) {
            if(a.size()!=640*480*8 || b.size()!=a.size()) return 0;
            int changed=0;
            for(int y=0;y<480;++y)for(int x=0;x<640;++x)
                if(std::abs(Channel(a,x,y,0)-Channel(b,x,y,0))+std::abs(Channel(a,x,y,1)-Channel(b,x,y,1))+std::abs(Channel(a,x,y,2)-Channel(b,x,y,2))>.035f)++changed;
            return changed;
        };
        auto save=[](const char* path,const std::vector<uint8_t>& pixels) {
            std::ofstream image(path,std::ios::binary); image<<"P6\n640 480\n255\n";
            for(int y=0;y<480;++y)for(int x=0;x<640;++x)for(int c=0;c<3;++c)
                image.put(static_cast<char>(std::clamp(Channel(pixels,x,y,c),0.f,1.f)*255));
        };
        renderer.SetCameraPosition(Vec3(0,2.5f,6));renderer.SetCameraRotation(-1.5707963f,-.3f);
        renderer.ClearLocalLights();renderer.SetDirectionalLight(Vec3(-.6f,-1,-.4f),Vec3(1,.95f,.85f),3);
        RenderSettings settings;settings.antiAliasing=false;settings.bloom=false;settings.atmosphereStrength=0;
        settings.screenSpaceReflections=false;settings.giStrength=0;settings.shadowQuality=0;
        Texture2D surfaceMap;check(surfaceMap.Load("out/build/rhi-colors.tga"),"surface map fixture");
        Transform floor;floor.position.y=-1;floor.scale=Vec3(12,1,12);
        Transform object;object.rotation.y=.4f;
        auto render=[&](int mode=0,float time=0.f) {
            renderer.SetRenderSettings(settings);renderer.InvalidateTemporalHistory();
            ImGui::NewFrame();ImGui::Render();renderer.BeginFrame();
            for(int cascade=0;cascade<Renderer::ShadowCascadeCount;++cascade) if(renderer.BeginShadowPass(cascade)) {
                renderer.DrawShadowMesh(floor,PrimitiveType::Plane);renderer.DrawShadowMesh(object,PrimitiveType::Cube);renderer.EndShadowPass();
            }
            renderer.DrawSky();
            renderer.DrawMesh(floor,PrimitiveType::Plane,.4f,.4f,.4f,1,nullptr,0,.8f);
            if(mode==1) renderer.DrawAnimatedModel(object,"Projects/DuelFPS/Assets/Models/Test/RiggedSimple/RiggedSimple.gltf",0,time,true);
            else if(mode==2 || mode==4) renderer.DrawModel(object,"Projects/DuelFPS/Assets/Models/Weapons/AssaultRifle_1.obj",1,1,1,1,
                nullptr,.7f,.12f,1,0,nullptr,nullptr,nullptr,nullptr,nullptr,mode==4);
            else renderer.DrawMesh(object,PrimitiveType::Sphere,.55f,.32f,.12f,1,nullptr,.95f,.17f,1,mode==3?5.f:0.f,
                mode==5?&surfaceMap:nullptr,mode==9?&surfaceMap:nullptr,mode==6?&surfaceMap:nullptr,
                mode==7?&surfaceMap:nullptr,mode==8?&surfaceMap:nullptr);
            renderer.EndScene();renderer.EndFrame();std::vector<uint8_t> pixels;
            check(device->ReadTexture(renderer.GetSceneColorTexture(),pixels),"advanced scene readback");return pixels;
        };
        settings.shadows=false;auto unshadowed=render();
        settings.shadows=true;auto shadowed=render();check(difference(unshadowed,shadowed)>50,"cascaded shadow occlusion changes receiver");
        save("out/build/rhi-parity-lit.ppm",shadowed);
        const char* mapLabels[]={"normal texture shading","roughness texture shading","AO texture shading","emissive texture shading","metallic texture shading"};
        for(int mode=5;mode<=9;++mode)check(difference(shadowed,render(mode))>50,mapLabels[mode-5]);
        settings.environmentReflectionStrength=0;settings.indirectLightStrength=0;auto noEnvironment=render();
        check(difference(shadowed,noEnvironment)>500,"environment lights metallic surfaces");
        settings.environmentReflectionStrength=1;settings.indirectLightStrength=1;
        renderer.SetDebugView(RenderDebugView::Normals);auto normals=render();check(difference(normals,shadowed)>500,"normal MRT and debug view");
        renderer.SetDebugView(RenderDebugView::Roughness);auto roughness=render();check(difference(normals,roughness)>500,"roughness MRT and debug view");
        renderer.SetDebugView(RenderDebugView::Lit);
        settings.viewDistance=50;renderer.SetCameraPosition(Vec3(0,12,35));
        auto distant=render();settings.fog=true;settings.fogDensity=.1f;auto fog=render();
        check(difference(fog,distant)>100,"distance fog");settings.fog=false;settings.viewDistance=1000;
        renderer.SetCameraPosition(Vec3(0,2.5f,6));
        auto emissive=render(3);settings.bloom=true;auto bloom=render(3);check(difference(emissive,bloom)>50,"HDR bloom responds to emissive");settings.bloom=false;
        auto pose0=render(1,0);auto pose1=render(1,.5f);check(difference(pose0,pose1)>50,"GPU bone palette animates geometry");
        check(renderer.RenderModelPreview("Projects/DuelFPS/Assets/Models/Weapons/AssaultRifle_1.obj",144,144)!=0,"64-bit model preview descriptor");
        check(renderer.RenderAnimatedModelPreview("Projects/DuelFPS/Assets/Models/Test/RiggedSimple/RiggedSimple.gltf",0,.5f,144,144)!=0,"animated preview descriptor");
        auto weapon=render(2);save("out/build/rhi-parity-weapon.ppm",weapon);
        renderer.SetDebugView(RenderDebugView::Roughness);
        auto importedMaterial=render(2);auto authoredMaterial=render(4);
        check(difference(importedMaterial,authoredMaterial)>100,"authored weapon roughness overrides imported material");
        renderer.SetDebugView(RenderDebugView::Lit);
        settings.antiAliasing=true;
        for(int samples:{2,4,8}) {settings.antiAliasingSamples=samples;auto temporal=render();check(!temporal.empty(),"MSAA and temporal resolve after advanced passes");}
        settings.antiAliasingSamples=4;
        renderer.SetRenderSettings(RenderSettings{});renderer.ResetCamera();
        return ok;
    }

    bool CheckMaterialsAndUI(Renderer& renderer)
    {
        auto* device = Velcryn::RHI::GetDevice();
        renderer.SetClearColor(0.1f, 0.1f, 0.15f, 1.0f);
        bool ok = true;
        auto check = [&](bool passed, const char* label) {
            if (!passed) { Logger::Error(std::string("RHI regression: ") + label); ok = false; }
            else Logger::Info(std::string("RHI regression passed: ") + label);
        };
        // A 2x2, top-origin TGA with known encoded colors. Gray catches accidental sRGB decode in UI.
        const uint8_t header[18] = {0,0,2,0,0,0,0,0,0,0,0,0,2,0,2,0,24,32};
        const uint8_t texels[12] = {0,0,255, 0,255,0, 255,0,0, 128,128,128};
        { std::ofstream file("out/build/rhi-colors.tga", std::ios::binary); file.write(reinterpret_cast<const char*>(header),18); file.write(reinterpret_cast<const char*>(texels),12); }
        Texture2D texture;
        check(texture.Load("out/build/rhi-colors.tga"), "diagnostic texture load");
        auto render = [&](bool textured, bool overlay, bool empty = false) {
            ImGui::NewFrame(); ImGui::Render();
            renderer.BeginFrame();
            if (!overlay && !empty) renderer.DrawMesh(Transform{}, PrimitiveType::Cube, 1,1,1,1, textured ? &texture : nullptr);
            renderer.EndScene();
            if (overlay)
            {
                auto& ui = renderer.GetUIRenderer(); ui.Clear(); ui.SetLogicalSize(640,480);
                renderer.BeginOverlay(); ui.Begin();
                ui.SetRect("alpha",20,20,80,80,1,0,0,0.5f);
                ui.SetImage("colors", &texture,120,20,80,80,1,1,1,1);
                ui.SetText("script", "Script UI: 123!",20,180,3,1,1,1,1);
                UICanvas canvas; canvas.SetSize(Vec2(640,480));
                auto text = std::make_unique<UIText>(); text->SetText("Canvas text!"); text->SetPosition(Vec2(20,130));
                text->SetSize(Vec2(260,40)); text->SetFontSize(24); text->SetColor(Vec4(1,1,1,1));
                canvas.GetRoot()->AddChild(std::move(text));
                auto button = std::make_unique<UIButton>(); button->SetPosition(Vec2(240,20)); button->SetSize(Vec2(180,80));
                button->SetNormalColor(Vec4(0,0,1,1)); button->SetCornerRadius(20); canvas.GetRoot()->AddChild(std::move(button));
                auto progress = std::make_unique<UIProgressBar>(); progress->SetPosition(Vec2(240,120)); progress->SetSize(Vec2(180,30));
                progress->SetColor(Vec4(0,0,0,1)); progress->SetFillColor(Vec4(0,1,0,1)); progress->SetPercent(0.5f);
                canvas.GetRoot()->AddChild(std::move(progress));
                ui.RenderCanvas(canvas, &renderer); ui.End(); renderer.EndOverlay();
            }
            renderer.EndFrame();
            std::vector<uint8_t> pixels;
            check(device->ReadTexture(renderer.GetSceneColorTexture(), pixels), "framebuffer readback");
            return pixels;
        };
        RenderSettings settings; settings.indirectLightStrength = 0;
        renderer.SetRenderSettings(settings); renderer.ClearLocalLights();
        renderer.SetDirectionalLight(Vec3(0,0,-1), Vec3(1,1,1),0);
        auto dark = render(false,false);
        renderer.SetDirectionalLight(Vec3(0,0,-1), Vec3(1,1,1),4);
        auto lit = render(false,false);
        check(Channel(lit,320,240,0) > Channel(dark,320,240,0) + 0.2f, "directional intensity changes pixels");
        renderer.SetDirectionalLight(Vec3(0,0,-1), Vec3(1,0,0),4);
        auto red = render(false,false);
        check(Channel(red,320,240,0) > Channel(red,320,240,1) + 0.2f, "directional color changes pixels");
        renderer.SetDirectionalLight(Vec3(0,0,-1), Vec3(1,1,1),0);
        renderer.AddPointLight({Vec3(0,0,2),Vec3(0,1,0),10,10});
        auto point = render(false,false);
        check(Channel(point,320,240,1) > Channel(point,320,240,0) + 0.2f, "point light changes pixels");
        renderer.ClearLocalLights();
        renderer.AddSpotLight({Vec3(0,0,2),Vec3(0,0,-1),Vec3(0,0,1),10,10,0.95f,0.8f});
        auto spot = render(false,false);
        check(Channel(spot,320,240,2) > Channel(spot,320,240,0) + 0.2f, "spot light changes pixels");
        renderer.ClearLocalLights(); renderer.SetDirectionalLight(Vec3(0,0,-1),Vec3(1,1,1),4);
        auto textured = render(true,false);
        check(std::abs(Channel(textured,290,210,0) - Channel(textured,290,210,1)) > 0.1f, "mesh samples colored texture");
        auto background = render(false,false,true);
        auto ui = render(false,true);
        check(std::abs(Channel(ui,60,60,0) - (0.5f+0.5f*Channel(background,60,60,0))) < 0.03f && std::abs(Channel(ui,60,60,1) - 0.5f*Channel(background,60,60,1)) < 0.02f, "UI alpha blends over preserved scene");
        check(Channel(ui,130,30,0) > 0.95f && Channel(ui,130,30,2) < 0.05f, "UI image top-left orientation");
        check(std::abs(Channel(ui,190,90,0) - 128.0f/255) < 0.02f, "UI gray keeps display encoding");
        check(Channel(ui,250,135,1) > 0.95f && Channel(ui,400,135,1) < 0.05f, "progress bar fill extent");
        check(Channel(ui,330,95,2) > 0.95f && std::abs(Channel(ui,241,21,2)-Channel(background,241,21,2)) < 0.02f, "non-square rounded button bounds");
        for (int row : {130,180}) {
            int ink = 0;
            for (int y=row;y<row+40;++y) for(int x=20;x<240;++x) if(Channel(ui,x,y,0)>0.7f) ++ink;
            check(ink>80, row==130 ? "canvas glyph atlas draws text" : "scripted glyph atlas draws text");
        }
        std::ofstream image("out/build/rhi-ui.ppm", std::ios::binary); image << "P6\n640 480\n255\n";
        for(int y=0;y<480;++y) for(int x=0;x<640;++x) for(int c=0;c<3;++c)
            image.put(static_cast<char>(std::clamp(Channel(ui,x,y,c),0.0f,1.0f)*255));
        renderer.GetUIRenderer().Clear();
        return ok;
    }
}

int RunRHISmokeTest()
{
    if (!SDL_Init(SDL_INIT_VIDEO)) return 1;
    ImGui::CreateContext();
    ImGui::GetIO().BackendFlags |= ImGuiBackendFlags_RendererHasTextures;
    ImGui::GetIO().IniFilename = nullptr;
    int result = 0;
    {
        Window window("Velcryn RHI smoke test", 640, 480);
        Renderer renderer;
        if (!window.Initialize() || !renderer.Initialize(window)) result = 1;
        else
        {
            auto* device = Velcryn::RHI::GetDevice();
            const uint32_t initial[] = {0, 1, 2};
            Velcryn::RHI::BufferDesc bufferDesc{};
            bufferDesc.size = sizeof(initial); bufferDesc.usage = Velcryn::RHI::BufferUsage::Index;
            const auto buffer = device->CreateBuffer(bufferDesc, initial);
            if (!buffer || !device->UpdateBuffer(buffer, initial, sizeof(initial))) result = 1;
            device->DestroyBuffer(buffer);
            if (device->UpdateBuffer(buffer, initial, sizeof(initial))) result = 1;
            const auto reused = device->CreateBuffer(bufferDesc, initial);
            if (!reused || reused.index != buffer.index || reused.generation == buffer.generation) result = 1;
            device->DestroyBuffer(buffer); // A stale destroy must not destroy the replacement.
            if (!device->UpdateBuffer(reused, initial, sizeof(initial))) result = 1;
            device->DestroyBuffer(reused);
            Texture2D logo;
            if (!logo.Load("Engine/Branding/VelcrynLogo.png") || !logo.GetID()) result = 1;
            for (int frame = 0; frame < 24; ++frame)
            {
                SDL_PumpEvents();
                if (frame == 8) { renderer.ResizeViewport(320, 240); SDL_SetWindowSize(window.GetNativeWindow(), 800, 600); }
                if (frame == 16) { renderer.ResizeViewport(640, 480); SDL_SetWindowSize(window.GetNativeWindow(), 640, 480); }
                ImGui::GetIO().DisplaySize = ImVec2(640, 480);
                ImGui::GetIO().DeltaTime = 1.0f / 60.0f;
                ImGui::NewFrame();
                renderer.BeginFrame();
                Transform transform;
                transform.rotation.y = frame * 0.05f;
                transform.rotation.x = 0.65f;
                renderer.DrawMesh(transform, static_cast<PrimitiveType>(1 + frame % 4), 0.7f, 0.4f, 0.2f, 1);
                renderer.EndScene();
                if (!renderer.GetViewportTexture()) result = 1;
                ImGui::Begin("RHI smoke test");
                ImGui::Text("Font atlas, image descriptors, scene output");
                ImGui::Image(static_cast<ImTextureID>(renderer.GetViewportTexture()), ImVec2(320, 240));
                if (logo.GetID()) ImGui::Image(static_cast<ImTextureID>(logo.GetID()), ImVec2(64, 64));
                ImGui::End();
                ImGui::Render();
                renderer.EndFrame();
                if (frame >= 20)
                {
                    std::vector<uint8_t> pixels;
                    if (!Velcryn::RHI::GetDevice()->ReadTexture(renderer.GetSceneColorTexture(), pixels) || pixels.size() != 640 * 480 * 8)
                        result = 1;
                    else
                    {
                        size_t changed = 0;
                        for (size_t i = 8; i < pixels.size(); i += 8)
                            if (std::memcmp(pixels.data(), pixels.data() + i, 6)) ++changed;
                        Logger::Info("RHI smoke: primitive " + std::to_string(1 + frame % 4) + " shaded pixels=" + std::to_string(changed));
                        if (changed < 100) result = 1;
                        if (frame == 20)
                        {
                            std::ofstream image("out/build/rhi-cube.ppm", std::ios::binary);
                            image << "P6\n640 480\n255\n";
                            for (size_t i = 0; i < pixels.size(); i += 8)
                                for (size_t c = 0; c < 3; ++c)
                                {
                                    uint16_t h; std::memcpy(&h, pixels.data() + i + c * 2, 2);
                                    float value = std::ldexp(float((h & 1023) + ((h & 0x7c00) ? 1024 : 0)), int((h >> 10) & 31) - 25);
                                    image.put(static_cast<char>(std::clamp(value, 0.0f, 1.0f) * 255));
                                }
                        }
                    }
                }
            }
            if (!CheckMaterialsAndUI(renderer)) result = 1;
            if (!RunUIInteractionTests(renderer)) result = 1;
            if (!CheckRendererParity(renderer)) result = 1;
            if (!CheckPracticeRange(renderer)) result = 1;
            if (!CheckPracticeRange(renderer,true)) result = 1;
            if (!CheckBreakbulkRuntime(renderer)) result = 1;
            if (!CheckEmberwildRuntime(renderer)) result = 1;
            Velcryn::RHI::GetDevice()->WaitIdle();
        }
        renderer.Shutdown();
    }
    ImGui::DestroyContext();
    SDL_Quit();
    Logger::Info(result ? "RHI smoke test FAILED" : "RHI smoke test completed");
    return result;
}
