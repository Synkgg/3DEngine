#include "RHISmokeTest.h"
#include "RHI.h"
#include "../Renderer.h"
#include "../../Platform/SDL/Window.h"
#include "../../Core/Logger.h"
#include <SDL3/SDL.h>
#include <imgui.h>
#include "../Texture2D.h"
#include <fstream>
#include <cmath>
#include <algorithm>
#include <cstring>
#include "../../UI/UIText.h"
#include "../../UI/UISerializer.h"
#include "../../Scene/Runtime/Runtime.h"
#include "../../Core/ProjectManager.h"
#include "../../Platform/SDL/Input.h"
#include "../../Scene/Components/TransformComponent.h"
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

    bool CheckBreakbulkRuntime(Renderer& renderer)
    {
        ProjectManager project;
        if(!project.Load("Projects/DuelFPS/DuelFPS.project"))return false;
        renderer.SetProjectRoot(std::filesystem::absolute("Projects/DuelFPS"));
        // All four authored meshes must import as actual renderable geometry.
        for(const char* weapon:{"MakoP12","KestrelAR4","BreachS8","VectorK9"}) {
            if(!renderer.GetModelAsset(std::string("Assets/Models/Arsenal/")+weapon+".obj")) {
                Logger::Error(std::string("BREAKBULK: cannot import weapon mesh: ")+weapon);
                return false;
            }
        }
        Scene menu;std::vector<HierarchyFolder> folders;SceneSerializer menuLoader(menu);
        if(!menuLoader.Load(project.ResolveAssetPath("Assets/Scenes/MainMenu.scene"),folders))return false;
        Input menuInput;UICanvas menuUI;Runtime menuRuntime;menuRuntime.SetProjectManager(&project);
        menuRuntime.Start(menu,renderer,menuInput,menuUI);
        auto* armory=dynamic_cast<UIButton*>(menuUI.GetRoot()->Find("LoadoutButton"));
        if(!armory){menuRuntime.Stop(menu);return false;}
        armory->SetClicked(true);
        for(int i=0;i<10;++i)menuRuntime.Update(menu,renderer,menuInput,1.f/60);
        if(!menuUI.GetRoot()->Find("PrimaryTab") || !menuUI.GetRoot()->Find("shotgunButton") ||
           !menuUI.GetRoot()->Find("smgButton")) {
            Logger::Error("BREAKBULK: four-weapon loadout menu failed to open");
            menuRuntime.Stop(menu);return false;
        }
        auto* back=dynamic_cast<UIButton*>(menuUI.GetRoot()->Find("BackButton"));
        if(!back){menuRuntime.Stop(menu);return false;}
        back->SetClicked(true);
        for(int i=0;i<10;++i)menuRuntime.Update(menu,renderer,menuInput,1.f/60);
        auto* practice=dynamic_cast<UIButton*>(menuUI.GetRoot()->Find("PracticeButton"));
        if(!practice){menuRuntime.Stop(menu);return false;}
        practice->SetClicked(true);
        for(int i=0;i<10;++i)menuRuntime.Update(menu,renderer,menuInput,1.f/60);
        const bool hasWeapon=menu.FindEntityByName("KESTREL AR4 Viewmodel").IsValid() ||
            menu.FindEntityByName("MAKO P12 Viewmodel").IsValid() ||
            menu.FindEntityByName("BREACH S8 Viewmodel").IsValid() ||
            menu.FindEntityByName("VECTOR K9 Viewmodel").IsValid();
        const bool drill=menu.FindEntityByName("PracticeMode").IsValid()&&menuUI.GetRoot()->Find("AmmoText")&&hasWeapon;
        menuRuntime.Stop(menu);
        if(!drill){Logger::Error("BREAKBULK: actual menu-to-drill Lua flow failed");return false;}
        Scene hostScene,clientScene;SceneSerializer hl(hostScene),cl(clientScene);
        if(!hl.Load(project.ResolveAssetPath("Assets/Scenes/Arena.scene"),folders)||!cl.Load(project.ResolveAssetPath("Assets/Scenes/Arena.scene"),folders))return false;
        Runtime host,client;Input hi,ci;UICanvas hu,cu;host.SetProjectManager(&project);client.SetProjectManager(&project);
        if(!host.GetNetwork().Host(0,2))return false;
        host.Start(hostScene,renderer,hi,hu);
        if(!client.GetNetwork().Join("127.0.0.1",host.GetNetwork().GetBoundPort())){host.Stop(hostScene);return false;}
        for(int i=0;i<5;++i){host.GetNetwork().Update();client.GetNetwork().Update();SDL_Delay(1);}
        client.Start(clientScene,renderer,ci,cu);
        auto tick=[&](){host.Update(hostScene,renderer,hi,1.f/60);client.Update(clientScene,renderer,ci,1.f/60);};
        for(int i=0;i<240;++i)tick();
        auto* hp=hostScene.GetComponent<TransformComponent>(hostScene.FindEntityByName("Operator"));
        auto* cp=clientScene.GetComponent<TransformComponent>(clientScene.FindEntityByName("Operator"));
        bool ok=hp&&cp&&hostScene.FindEntityByName("Remote Operator").IsValid()&&clientScene.FindEntityByName("Remote Operator").IsValid();
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
                foundLight=std::abs(light->intensity-(terminal?2.3f:1.4f))<.001f;
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
            Velcryn::RHI::GetDevice()->WaitIdle();
        }
        renderer.Shutdown();
    }
    ImGui::DestroyContext();
    SDL_Quit();
    Logger::Info(result ? "RHI smoke test FAILED" : "RHI smoke test completed");
    return result;
}
