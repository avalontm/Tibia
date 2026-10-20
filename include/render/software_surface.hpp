#pragma once

#include "resources/tibia_spr.hpp"
#include "render/light_quad.hpp"

#include <span>
#include <cstdint>
#include <vector>

namespace tibia::render {

struct RgbaColor {
    std::uint8_t red{};
    std::uint8_t green{};
    std::uint8_t blue{};
    std::uint8_t alpha{255};
};

// Deterministic CPU surface used for resource composition and as a renderer fallback.
class SoftwareSurface {
public:
    SoftwareSurface(std::uint32_t width, std::uint32_t height, RgbaColor clear = {});

    [[nodiscard]] std::uint32_t width() const noexcept { return width_; }
    [[nodiscard]] std::uint32_t height() const noexcept { return height_; }
    [[nodiscard]] const std::vector<std::uint8_t>& pixels() const noexcept { return pixels_; }
    void clear(RgbaColor color);
    void draw(const resources::TibiaSprImage& image, int x, int y, std::uint8_t opacity = 255);
    void draw(const SoftwareSurface& surface, int x, int y, std::uint8_t opacity = 255);
    void draw_scaled(const resources::TibiaSprImage& image, int x, int y,
        std::uint32_t destination_width, std::uint32_t destination_height,
        std::uint8_t opacity = 255);
    void draw_clipped(const resources::TibiaSprImage& image,
        int destination_x, int destination_y, int destination_width,
        int destination_height, int clip_x, int clip_y,
        int clip_width, int clip_height, std::uint8_t opacity = 255);
    void draw_light_quads(std::span<const LightQuad> quads,
        int clip_x, int clip_y, int clip_width, int clip_height);

private:
    std::uint32_t width_;
    std::uint32_t height_;
    std::vector<std::uint8_t> pixels_;
};

} // namespace tibia::render
