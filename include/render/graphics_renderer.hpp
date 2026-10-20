#pragma once

#include "render/software_surface.hpp"
#include "render/light_quad.hpp"

#include <cstdint>
#include <memory>
#include <span>

namespace tibia::render {

// Values match the original client's observed graphics-engine selector.
enum class GraphicsBackend : std::uint8_t { direct_draw = 0, open_gl = 1, direct3d9 = 2 };

class GraphicsRenderer {
public:
    GraphicsRenderer();
    ~GraphicsRenderer();
    GraphicsRenderer(const GraphicsRenderer&) = delete;
    GraphicsRenderer& operator=(const GraphicsRenderer&) = delete;

    void initialize(GraphicsBackend backend, void* window_handle, std::uint32_t width, std::uint32_t height);
    void resize(std::uint32_t width, std::uint32_t height);
    void upload(std::uint32_t texture_id, const resources::TibiaSprImage& image);
    void begin_frame(RgbaColor clear = {});
    void set_draw_transform(int source_width, int source_height,
        int destination_x, int destination_y, int destination_width,
        int destination_height, int clip_x, int clip_y, int clip_width,
        int clip_height);
    void clear_draw_transform() noexcept;
    void draw(std::uint32_t texture_id, int x, int y, int width, int height,
        std::uint8_t opacity = 255);
    void draw_clipped(std::uint32_t texture_id, int destination_x, int destination_y,
        int destination_width, int destination_height, int clip_x, int clip_y,
        int clip_width, int clip_height, std::uint8_t opacity = 255);
    void draw_light_quads(std::span<const LightQuad> quads,
        int clip_x, int clip_y, int clip_width, int clip_height);
    [[nodiscard]] bool supports_light_quads() const noexcept;
    [[nodiscard]] bool end_frame();
    [[nodiscard]] GraphicsBackend backend() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace tibia::render
