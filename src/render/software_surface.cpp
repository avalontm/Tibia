#include "render/software_surface.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace tibia::render {
namespace {

std::uint8_t blend_channel(std::uint8_t source, std::uint8_t destination,
    unsigned source_alpha, unsigned destination_alpha, unsigned output_alpha) {
    if (output_alpha == 0) return 0;
    const auto numerator = source * source_alpha * 255U +
        destination * destination_alpha * (255U - source_alpha);
    const auto denominator = output_alpha * 255U;
    return static_cast<std::uint8_t>((numerator + denominator / 2U) / denominator);
}

} // namespace

SoftwareSurface::SoftwareSurface(std::uint32_t width, std::uint32_t height, RgbaColor color)
    : width_(width), height_(height) {
    if (height != 0 && static_cast<std::size_t>(width) >
        std::numeric_limits<std::size_t>::max() / height / 4)
        throw std::length_error("surface dimensions overflow addressable memory");
    pixels_.resize(static_cast<std::size_t>(width) * height * 4);
    clear(color);
}

void SoftwareSurface::clear(RgbaColor color) {
    for (std::size_t i = 0; i < pixels_.size(); i += 4) {
        pixels_[i] = color.red;
        pixels_[i + 1] = color.green;
        pixels_[i + 2] = color.blue;
        pixels_[i + 3] = color.alpha;
    }
}

void SoftwareSurface::draw(const resources::TibiaSprImage& image, int x, int y, std::uint8_t opacity) {
    draw_scaled(image, x, y, static_cast<std::uint32_t>(resources::TibiaSprImage::width),
        static_cast<std::uint32_t>(resources::TibiaSprImage::height), opacity);
}

void SoftwareSurface::draw(const SoftwareSurface& surface, int x, int y, std::uint8_t opacity) {
    if (&surface == this) {
        const auto copy = surface;
        draw(copy, x, y, opacity);
        return;
    }
    if (opacity == 0 || width_ == 0 || height_ == 0 || surface.width_ == 0 || surface.height_ == 0)
        return;
    const auto left = std::max<std::int64_t>(0, x);
    const auto top = std::max<std::int64_t>(0, y);
    const auto right = std::min<std::int64_t>(width_, static_cast<std::int64_t>(x) + surface.width_);
    const auto bottom = std::min<std::int64_t>(height_, static_cast<std::int64_t>(y) + surface.height_);
    if (left >= right || top >= bottom) return;
    for (auto dy = top; dy < bottom; ++dy) {
        for (auto dx = left; dx < right; ++dx) {
            const auto si = (static_cast<std::size_t>(dy - y) * surface.width_ +
                static_cast<std::size_t>(dx - x)) * 4;
            const auto alpha = static_cast<unsigned>(surface.pixels_[si + 3]) * opacity / 255U;
            if (alpha == 0) continue;
            const auto di = (static_cast<std::size_t>(dy) * width_ + static_cast<std::size_t>(dx)) * 4;
            const auto destination_alpha = static_cast<unsigned>(pixels_[di + 3]);
            const auto output_alpha = alpha + (destination_alpha * (255U - alpha) + 127U) / 255U;
            pixels_[di] = blend_channel(surface.pixels_[si], pixels_[di], alpha, destination_alpha, output_alpha);
            pixels_[di + 1] = blend_channel(surface.pixels_[si + 1], pixels_[di + 1], alpha, destination_alpha, output_alpha);
            pixels_[di + 2] = blend_channel(surface.pixels_[si + 2], pixels_[di + 2], alpha, destination_alpha, output_alpha);
            pixels_[di + 3] = static_cast<std::uint8_t>(output_alpha);
        }
    }
}

void SoftwareSurface::draw_scaled(const resources::TibiaSprImage& image, int x, int y,
    std::uint32_t destination_width, std::uint32_t destination_height, std::uint8_t opacity) {
    if (destination_width == 0 || destination_height == 0 || opacity == 0 || width_ == 0 || height_ == 0)
        return;
    const auto left = std::max<std::int64_t>(0, x);
    const auto top = std::max<std::int64_t>(0, y);
    const auto right = std::min<std::int64_t>(width_, static_cast<std::int64_t>(x) + destination_width);
    const auto bottom = std::min<std::int64_t>(height_, static_cast<std::int64_t>(y) + destination_height);
    if (left >= right || top >= bottom) return;

    for (auto dy = top; dy < bottom; ++dy) {
        const auto source_y = static_cast<std::size_t>((dy - y) * image.content_height / destination_height);
        for (auto dx = left; dx < right; ++dx) {
            const auto source_x = static_cast<std::size_t>((dx - x) * image.content_width / destination_width);
            const auto si = (source_y * resources::TibiaSprImage::width + source_x) * 4;
            const auto alpha = static_cast<unsigned>(image.rgba[si + 3]) * opacity / 255U;
            if (alpha == 0) continue;
            const auto di = (static_cast<std::size_t>(dy) * width_ + static_cast<std::size_t>(dx)) * 4;
            const auto destination_alpha = static_cast<unsigned>(pixels_[di + 3]);
            const auto output_alpha = alpha + (destination_alpha * (255U - alpha) + 127U) / 255U;
            pixels_[di] = blend_channel(image.rgba[si], pixels_[di], alpha, destination_alpha, output_alpha);
            pixels_[di + 1] = blend_channel(image.rgba[si + 1], pixels_[di + 1], alpha, destination_alpha, output_alpha);
            pixels_[di + 2] = blend_channel(image.rgba[si + 2], pixels_[di + 2], alpha, destination_alpha, output_alpha);
            pixels_[di + 3] = static_cast<std::uint8_t>(output_alpha);
        }
    }
}

void SoftwareSurface::draw_clipped(const resources::TibiaSprImage& image,
    int destination_x, int destination_y, int destination_width,
    int destination_height, int clip_x, int clip_y,
    int clip_width, int clip_height, std::uint8_t opacity) {
    if (destination_width <= 0 || destination_height <= 0 ||
        clip_width <= 0 || clip_height <= 0 || opacity == 0)
        return;
    const auto left = std::max<std::int64_t>(
        std::max<std::int64_t>(0, destination_x), clip_x);
    const auto top = std::max<std::int64_t>(
        std::max<std::int64_t>(0, destination_y), clip_y);
    const auto right = std::min<std::int64_t>({
        static_cast<std::int64_t>(width_),
        static_cast<std::int64_t>(destination_x) + destination_width,
        static_cast<std::int64_t>(clip_x) + clip_width});
    const auto bottom = std::min<std::int64_t>({
        static_cast<std::int64_t>(height_),
        static_cast<std::int64_t>(destination_y) + destination_height,
        static_cast<std::int64_t>(clip_y) + clip_height});
    if (left >= right || top >= bottom) return;
    for (auto dy = top; dy < bottom; ++dy) {
        const auto sy = static_cast<std::size_t>(
            (dy - destination_y) * image.content_height / destination_height);
        for (auto dx = left; dx < right; ++dx) {
            const auto sx = static_cast<std::size_t>(
            (dx - destination_x) * image.content_width / destination_width);
            const auto source_index = (sy * resources::TibiaSprImage::width + sx) * 4;
            const auto alpha = static_cast<unsigned>(image.rgba[source_index + 3]) * opacity / 255U;
            if (alpha == 0) continue;
            const auto destination_index = (static_cast<std::size_t>(dy) * width_ +
                static_cast<std::size_t>(dx)) * 4;
            const auto destination_alpha = static_cast<unsigned>(pixels_[destination_index + 3]);
            const auto output_alpha = alpha +
                (destination_alpha * (255U - alpha) + 127U) / 255U;
            pixels_[destination_index] = blend_channel(image.rgba[source_index],
                pixels_[destination_index], alpha, destination_alpha, output_alpha);
            pixels_[destination_index + 1] = blend_channel(image.rgba[source_index + 1],
                pixels_[destination_index + 1], alpha, destination_alpha, output_alpha);
            pixels_[destination_index + 2] = blend_channel(image.rgba[source_index + 2],
                pixels_[destination_index + 2], alpha, destination_alpha, output_alpha);
            pixels_[destination_index + 3] = static_cast<std::uint8_t>(output_alpha);
        }
    }
}

void SoftwareSurface::draw_light_quads(std::span<const LightQuad> quads,
    int clip_x, int clip_y, int clip_width, int clip_height) {
    if (clip_width <= 0 || clip_height <= 0) return;
    const auto clip_left = std::max<std::int64_t>(0, clip_x);
    const auto clip_top = std::max<std::int64_t>(0, clip_y);
    const auto clip_right = std::min<std::int64_t>(width_,
        static_cast<std::int64_t>(clip_x) + clip_width);
    const auto clip_bottom = std::min<std::int64_t>(height_,
        static_cast<std::int64_t>(clip_y) + clip_height);
    if (clip_left >= clip_right || clip_top >= clip_bottom) return;
    for (const auto& quad : quads) {
        if (quad.width <= 0 || quad.height <= 0) continue;
        const auto left = std::max<std::int64_t>(clip_left, quad.x);
        const auto top = std::max<std::int64_t>(clip_top, quad.y);
        const auto right = std::min<std::int64_t>(clip_right,
            static_cast<std::int64_t>(quad.x) + quad.width);
        const auto bottom = std::min<std::int64_t>(clip_bottom,
            static_cast<std::int64_t>(quad.y) + quad.height);
        if (left >= right || top >= bottom) continue;
        for (auto y = top; y < bottom; ++y) {
            const auto v = (static_cast<double>(y - quad.y) + 0.5) / quad.height;
            for (auto x = left; x < right; ++x) {
                const auto u = (static_cast<double>(x - quad.x) + 0.5) / quad.width;
                std::array<double, 4> weights{};
                if (u + v <= 1.0) {
                    weights = {1.0 - u - v, u, v, 0.0};
                } else {
                    weights = {0.0, 1.0 - v, 1.0 - u, u + v - 1.0};
                }
                const auto index = (static_cast<std::size_t>(y) * width_ +
                    static_cast<std::size_t>(x)) * 4;
                for (std::size_t channel = 0; channel < 3; ++channel) {
                    const std::array<std::uint8_t, 4> values{
                        channel == 0 ? quad.corners[0].red : channel == 1 ? quad.corners[0].green : quad.corners[0].blue,
                        channel == 0 ? quad.corners[1].red : channel == 1 ? quad.corners[1].green : quad.corners[1].blue,
                        channel == 0 ? quad.corners[2].red : channel == 1 ? quad.corners[2].green : quad.corners[2].blue,
                        channel == 0 ? quad.corners[3].red : channel == 1 ? quad.corners[3].green : quad.corners[3].blue};
                    auto shade = 0.0;
                    for (std::size_t corner = 0; corner < 4; ++corner)
                        shade += values[corner] * weights[corner];
                    pixels_[index + channel] = static_cast<std::uint8_t>(
                        (pixels_[index + channel] * static_cast<unsigned>(std::clamp(shade, 0.0, 255.0))) / 255U);
                }
            }
        }
    }
}

} // namespace tibia::render
