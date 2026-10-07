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
            Velcryn::RHI::GetDevice()->WaitIdle();
        }
        renderer.Shutdown();
    }
    ImGui::DestroyContext();
    SDL_Quit();
    Logger::Info(result ? "RHI smoke test FAILED" : "RHI smoke test completed");
    return result;
}
