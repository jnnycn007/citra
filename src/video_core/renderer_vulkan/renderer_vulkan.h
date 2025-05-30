// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <condition_variable>
#include <mutex>
#include "common/common_types.h"
#include "common/math_util.h"
#include "core/hw/gpu.h"
#include "video_core/renderer_base.h"
#include "video_core/renderer_vulkan/vk_descriptor_pool.h"
#include "video_core/renderer_vulkan/vk_instance.h"
#include "video_core/renderer_vulkan/vk_present_window.h"
#include "video_core/renderer_vulkan/vk_rasterizer.h"
#include "video_core/renderer_vulkan/vk_renderpass_cache.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"
#include "video_core/renderer_vulkan/vk_swapchain.h"

namespace Core {
class System;
}

namespace Memory {
class MemorySystem;
}

namespace Layout {
struct FramebufferLayout;
}

namespace Vulkan {

struct TextureInfo {
    u32 width;
    u32 height;
    GPU::Regs::PixelFormat format;
    vk::Image image;
    vk::ImageView image_view;
    VmaAllocation allocation;
};

struct ScreenInfo {
    TextureInfo texture;
    Common::Rectangle<f32> texcoords;
    vk::ImageView image_view;
};

struct PresentUniformData {
    std::array<f32, 4 * 4> modelview;
    Common::Vec4f i_resolution;
    Common::Vec4f o_resolution;
    int screen_id_l = 0;
    int screen_id_r = 0;
    int layer = 0;
    int reverse_interlaced = 0;
};
static_assert(sizeof(PresentUniformData) == 112,
              "PresentUniformData does not structure in shader!");

class RendererVulkan : public VideoCore::RendererBase {
    static constexpr std::size_t PRESENT_PIPELINES = 3;

public:
    explicit RendererVulkan(Core::System& system, Frontend::EmuWindow& window,
                            VkInstance instance,
                            VkPhysicalDevice gpu);
    explicit RendererVulkan(Core::System& system, Frontend::EmuWindow& window,
                            Frontend::EmuWindow* secondary_window);
    ~RendererVulkan() override;

    [[nodiscard]] VideoCore::RasterizerInterface* Rasterizer() override {
        return rasterizer.get();
    }

    void NotifySurfaceChanged() override {
        main_window->NotifySurfaceChanged();
    }

    void SwapBuffers() override;
    void TryPresent(int timeout_ms, bool is_secondary) override {}
    void Sync() override;

    Instance* GetInstance() {
        return &instance;
    }
    void CreateMainWindow();

private:
    void ReloadPipeline();
    void CompileShaders();
    void BuildLayouts();
    void BuildPipelines();
    void ConfigureFramebufferTexture(TextureInfo& texture,
                                     const GPU::Regs::FramebufferConfig& framebuffer);
    void ConfigureRenderPipeline();
    void PrepareRendertarget();
    void RenderScreenshot();
    void RenderScreenshotWithStagingCopy();
    bool TryRenderScreenshotWithHostMemory();
    void PrepareDraw(Frame* frame, const Layout::FramebufferLayout& layout);
    void RenderToWindow(PresentWindow& window, const Layout::FramebufferLayout& layout,
                        bool flipped);

    void DrawScreens(Frame* frame, const Layout::FramebufferLayout& layout, bool flipped);
    void DrawBottomScreen(const Layout::FramebufferLayout& layout,
                          const Common::Rectangle<u32>& bottom_screen);
    void DrawTopScreen(const Layout::FramebufferLayout& layout,
                       const Common::Rectangle<u32>& top_screen);
    void DrawSingleScreen(u32 screen_id, float x, float y, float w, float h,
                          Layout::DisplayOrientation orientation);
    void DrawSingleScreenStereo(u32 screen_id_l, u32 screen_id_r, float x, float y, float w,
                                float h, Layout::DisplayOrientation orientation);
    void LoadFBToScreenInfo(const GPU::Regs::FramebufferConfig& framebuffer,
                            ScreenInfo& screen_info, bool right_eye);
    void LoadColorToActiveVkTexture(u8 color_r, u8 color_g, u8 color_b, const TextureInfo& texture);

private:
    Memory::MemorySystem& memory;

    Instance instance;
    Scheduler scheduler;
    RenderpassCache renderpass_cache;
    DescriptorPool pool;
    std::unique_ptr<PresentWindow> main_window;
    StreamBuffer vertex_buffer;
    std::unique_ptr<RasterizerVulkan> rasterizer;
    std::unique_ptr<PresentWindow> second_window;

    vk::UniquePipelineLayout present_pipeline_layout;
    DescriptorSetProvider present_set_provider;
    std::array<vk::Pipeline, PRESENT_PIPELINES> present_pipelines;
    std::array<vk::ShaderModule, PRESENT_PIPELINES> present_shaders;
    std::array<vk::Sampler, 2> present_samplers;
    vk::ShaderModule present_vertex_shader;
    u32 current_pipeline = 0;

    std::array<ScreenInfo, 3> screen_infos{};
    std::array<DescriptorData, 3> present_textures{};
    PresentUniformData draw_info{};
    vk::ClearColorValue clear_color{};
};

} // namespace Vulkan
