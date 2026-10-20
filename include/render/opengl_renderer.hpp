#pragma once

#include "resources/tibia_spr.hpp"
#include "render/light_quad.hpp"

#include <cstdint>
#include <memory>
#include <span>

namespace tibia::render {

class OpenGLRenderer {
public:
    OpenGLRenderer();
    ~OpenGLRenderer();
    OpenGLRenderer(const OpenGLRenderer&) = delete;
    OpenGLRenderer& operator=(const OpenGLRenderer&) = delete;

    void initialize(void* window_handle, std::uint32_t width, std::uint32_t height);
    void resize(std::uint32_t width, std::uint32_t height);
    void upload(std::uint32_t texture_id, const resources::TibiaSprImage& image);
    void begin_frame(std::uint32_t argb_clear_color = 0xff000000U);
    void draw(std::uint32_t texture_id, int x, int y, int width, int height,
        std::uint8_t opacity = 255);
    void draw_clipped(std::uint32_t texture_id, int destination_x, int destination_y,
        int destination_width, int destination_height, int clip_x, int clip_y,
        int clip_width, int clip_height, std::uint8_t opacity = 255);
    void draw_light_quads(std::span<const LightQuad> quads,
        int clip_x, int clip_y, int clip_width, int clip_height);
    void end_frame();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace tibia::render
