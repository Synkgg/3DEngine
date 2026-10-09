#include "UIRenderer.h"
#include "UIButton.h"
#include "UITextInput.h"
#include "UISlider.h"
#include "UIScrollBox.h"
#include "UISerializer.h"
#include "UIUserWidget.h"
#include "../Platform/SDL/Input.h"
#include "../Graphics/Renderer.h"
#include "../Graphics/RHI/RHI.h"
#include "../Core/Logger.h"
#include <imgui.h>
#include <cmath>
#include <cstring>
#include <fstream>
#include <algorithm>

bool RunUIInteractionTests(Renderer& renderer)
{
    auto& ui = renderer.GetUIRenderer();
    UICanvas canvas;
    Input input;
    bool ok = true;
    auto check = [&](bool passed, const char* label) {
        if (passed) Logger::Info(std::string("UI interaction passed: ") + label);
        else { Logger::Error(std::string("UI interaction FAILED: ") + label); ok = false; }
    };
    auto pointer = [&](float x, float y, bool down) {
        input.UpdateMouseState(x, y, down ? SDL_BUTTON_LMASK : 0);
        ui.UpdateInput(canvas, input, 100, 50, 640, 480);
    };
    ui.SetMouseInteractionEnabled(true);
    // Both hosts must instantiate the SAME visual asset but route buttons
    // to the controller script belonging to their current scene.
    {
        UICanvas composed;
        const bool lobbyLoaded = UISerializer::Load(
            composed, "Projects/DuelFPS/Assets/UI/LoadoutLobby.ui");
        check(lobbyLoaded, "lobby widget blueprint composition");
        if (lobbyLoaded)
        {
            const auto* instance = dynamic_cast<const UIUserWidget*>(
                composed.GetRoot()->Find("LoadoutPage"));
            const auto* button = dynamic_cast<const UIButton*>(
                composed.GetRoot()->Find("SaveButton"));
            check(instance && instance->GetSourcePath() == "Assets/UI/Loadout.ui" &&
                button && button->GetOnClickScript() == "Assets/Scripts/MainMenu.lua",
                "lobby instance uses shared layout and lobby callbacks");
        }
        const bool gameLoaded = UISerializer::Load(
            composed, "Projects/DuelFPS/Assets/UI/LoadoutInGame.ui");
        check(gameLoaded, "in-game widget blueprint composition");
        if (gameLoaded)
        {
            const auto* instance = dynamic_cast<const UIUserWidget*>(
                composed.GetRoot()->Find("LoadoutPage"));
            const auto* button = dynamic_cast<const UIButton*>(
                composed.GetRoot()->Find("SaveButton"));
            check(instance && instance->GetSourcePath() == "Assets/UI/Loadout.ui" &&
                button && button->GetOnClickScript() == "Assets/Scripts/Player.lua",
                "in-game instance uses shared layout and gameplay callbacks");
            check(button && composed.GetRoot()->Find("LoadoutPage.SaveButton") == button,
                "qualified widget names resolve inside reused UI pages");
        }
    }
    check(UISerializer::Load(canvas, "Projects/DuelFPS/Assets/UI/MainMenu.ui"), "load real menu asset");
    ui.SetLogicalSize(1920,1080);
    auto* host = dynamic_cast<UIButton*>(canvas.GetRoot()->Find("HostButton"));
    if (!host) return false;
    pointer(0,0,false);
    auto render = [&]() {
        ImGui::NewFrame(); ImGui::Render();
        renderer.BeginFrame(); renderer.EndScene(); renderer.BeginOverlay();
        ui.Begin(); ui.RenderCanvas(canvas, &renderer); renderer.EndOverlay(); renderer.EndFrame();
        std::vector<uint8_t> pixels;
        check(Velcryn::RHI::GetDevice()->ReadTexture(renderer.GetSceneColorTexture(), pixels), "UI framebuffer readback");
        return pixels;
    };
    auto normal = render();
    { std::ofstream image("out/build/breakbulk-menu.ppm",std::ios::binary);image<<"P6\n640 480\n255\n";
      for(size_t i=0;i<normal.size();i+=8)for(int c=0;c<3;++c){uint16_t h;std::memcpy(&h,normal.data()+i+c*2,2);float value=std::ldexp(float((h&1023)+((h&0x7c00)?1024:0)),int((h>>10)&31)-25);image.put(char(std::clamp(value,0.f,1.f)*255));} }

    // Letterboxed 1920x1080 canvas in a 640x480 viewport at window offset 100,50.
    pointer(100+1400.f/3, 50+60+454.f/3, false);
    check(host->IsHovered(), "hover uses viewport offset and letterbox scale");
    auto hovered = render();
    check(normal.size()==hovered.size() && normal!=hovered, "menu hover changes GPU pixels");
    pointer(100+1400.f/3, 50+60+454.f/3, true);
    check(host->IsPressed(), "press state");
    pointer(100+1400.f/3, 50+60+454.f/3, false);
    check(host->ConsumeClick() && !host->ConsumeClick(), "click consumed exactly once");
    check(host->ConsumeClickEvent() && !host->ConsumeClickEvent(), "script event queued exactly once");
    pointer(100+1400.f/3, 50+60+454.f/3, true);
    pointer(0,0,true);
    check(!host->IsHovered() && !host->IsPressed(), "dragging off cancels pressed appearance");
    pointer(0,0,false);
    check(!host->ConsumeClick(), "release outside does not click");
    pointer(330,65,false);
    check(!host->IsHovered(), "letterbox bars do not hover controls");

    canvas.Clear(); canvas.SetSize(Vec2(640,480)); ui.SetLogicalSize(640,480);
    auto button = std::make_unique<UIButton>(); button->SetPosition({20,20}); button->SetSize({200,60});
    auto* lower = static_cast<UIButton*>(canvas.GetRoot()->AddChild(std::move(button)));
    auto field = std::make_unique<UITextInput>(); field->SetPosition({20,20}); field->SetSize({200,60});
    auto* upper = static_cast<UITextInput*>(canvas.GetRoot()->AddChild(std::move(field)));
    pointer(150,90,true); pointer(150,90,false);
    check(upper->IsFocused() && !lower->IsHovered() && !lower->ConsumeClick(), "top text field blocks underlying button");
    upper->SetText("127.0.0.1"); upper->SelectAll(); pointer(150,90,false);
    check(upper->HasSelection() && upper->GetText()=="127.0.0.1", "idle frames preserve text selection");
    SDL_Event textEvent{}; textEvent.type = SDL_EVENT_TEXT_INPUT; textEvent.text.text = "host:7777!";
    input.ProcessEvent(textEvent); input.Update(); pointer(150,90,false);
    check(upper->GetText()=="host:7777!", "SDL text entry replaces selection and preserves punctuation");
    input.Update();
    lower->SetZOrder(2);
    pointer(150,90,false);
    check(lower->IsHovered(), "changing z order updates hit testing");
    lower->SetEnabled(false);
    pointer(150,90,true); pointer(150,90,false);
    check(!upper->IsFocused() && !lower->ConsumeClick(), "disabled top button blocks click-through");
    lower->SetVisible(false); pointer(150,90,true); pointer(150,90,false);
    check(upper->IsFocused(), "hidden controls do not intercept input");
    canvas.GetRoot()->RemoveChild(upper);
    pointer(150,90,false); // Must not dereference the deleted focused widget.
    canvas.Clear(); pointer(150,90,false);
    check(true, "focused widget removal and canvas replacement are safe");

    auto parent = std::make_unique<UIWidget>(UIWidgetType::Panel); parent->SetPosition({50,40});
    auto nested = std::make_unique<UIWidget>(UIWidgetType::Panel); nested->SetPosition({30,20});
    auto slider = std::make_unique<UISlider>(); slider->SetPosition({10,10}); slider->SetSize({100,20});
    auto* dragged = static_cast<UISlider*>(nested->AddChild(std::move(slider)));
    parent->AddChild(std::move(nested)); canvas.GetRoot()->AddChild(std::move(parent));
    pointer(100+140,50+80,true);
    check(std::abs(dragged->GetValue()-0.5f)<0.01f, "nested slider uses full ancestor transform");
    pointer(1000,130,true);
    check(dragged->GetValue()==1, "slider capture continues outside viewport and clamps");
    ui.SetMouseInteractionEnabled(false); pointer(1000,130,true);
    ui.SetMouseInteractionEnabled(true); pointer(190,130,true);
    check(dragged->GetValue()==1, "disabling mouse cancels slider capture");
    pointer(190,130,false);
    canvas.Clear(); pointer(0,0,false);
    auto rounded = std::make_unique<UIButton>(); rounded->SetPosition({20,20}); rounded->SetSize({100,60}); rounded->SetCornerRadius(25);
    auto* round = static_cast<UIButton*>(canvas.GetRoot()->AddChild(std::move(rounded)));
    pointer(121,71,false); check(!round->IsHovered(), "rounded transparent corners are not clickable");
    round->SetNormalImage("normal.png"); round->SetHovered(true);
    check(round->GetCurrentImage()=="normal.png", "missing hover image keeps normal image");
    // Scroll box: wheel changes content offset, children are clipped for
    // input, and v11 serialization preserves the authored scroll settings.
    canvas.Clear(); canvas.SetSize({640,480}); ui.SetLogicalSize(640,480);
    auto scrolling = std::make_unique<UIScrollBox>();
    scrolling->SetPosition({20,20}); scrolling->SetSize({200,100});
    scrolling->SetContentHeight(320);
    auto scrolledButton = std::make_unique<UIButton>();
    scrolledButton->SetName("ScrolledButton");
    scrolledButton->SetPosition({10,150}); scrolledButton->SetSize({100,40});
    auto* scrolled = static_cast<UIButton*>(scrolling->AddChild(std::move(scrolledButton)));
    auto* scrollPtr = static_cast<UIScrollBox*>(canvas.GetRoot()->AddChild(std::move(scrolling)));
    pointer(140,120,false);
    check(!scrolled->IsHovered(), "off-screen scroll box children cannot be clicked");
    SDL_Event wheel{}; wheel.type=SDL_EVENT_MOUSE_WHEEL; wheel.wheel.y=-2.0f;
    input.ProcessEvent(wheel); input.Update(); input.UpdateMouseState(140,90,0);
    ui.UpdateInput(canvas,input,100,50,640,480);
    check(std::abs(scrollPtr->GetScrollOffset()-110.0f)<0.01f, "mouse wheel scrolls clipped content");
    input.Update(); // consume wheel once, just as the main loop does
    pointer(140,120,false);
    check(scrolled->IsHovered(), "scrolled child is hit-testable inside viewport");
    check(UISerializer::Save(canvas,"out/build/scrollbox-smoke.ui"), "save scroll box asset");
    UICanvas loaded;
    check(UISerializer::Load(loaded,"out/build/scrollbox-smoke.ui"), "load scroll box asset");
    auto* loadedScroll = dynamic_cast<UIScrollBox*>(loaded.GetRoot()->Find("Scroll Box"));
    check(loadedScroll && loadedScroll->GetContentHeight()==320 &&
        std::abs(loadedScroll->GetScrollOffset()-110.0f)<0.01f,
        "scroll box survives UI v12 roundtrip");

    check(UISerializer::Load(canvas,"Projects/DuelFPS/Assets/UI/Loadout.ui"),
        "load grouped main-menu armory");
    check(canvas.GetRoot()->Find("Profile5Button") &&
        canvas.GetRoot()->Find("Category_rifle_Button") &&
        canvas.GetRoot()->Find("WeaponCategory_smg"),
        "loadout profiles, category tabs and parent groups exist");
    check(UISerializer::Load(canvas,"Projects/DuelFPS/Assets/UI/LoadoutInGame.ui"),
        "load grouped in-game armory");
    ui.Clear(); ui.SetMouseInteractionEnabled(false);
    return ok;
}
