#include "render/graphics_renderer.hpp"

#ifdef _WIN32
#include "render/d3d9_renderer.hpp"
#include "render/directdraw_renderer.hpp"
#include "render/opengl_renderer.hpp"
#endif

#include <algorithm>
#include <stdexcept>
#include <limits>
#include <optional>
#include <unordered_map>

namespace tibia::render {

struct GraphicsRenderer::Impl {
    struct DrawTransform {
        int source_width{};
        int source_height{};
        int destination_x{};
        int destination_y{};
        int destination_width{};
        int destination_height{};
        int clip_x{};
        int clip_y{};
        int clip_width{};
        int clip_height{};
    };

    GraphicsBackend selected{GraphicsBackend::direct3d9};
    std::optional<DrawTransform> draw_transform;
#ifdef _WIN32
    void* window_handle{};
    std::uint32_t width{};
    std::uint32_t height{};
    std::unordered_map<std::uint32_t, resources::TibiaSprImage> uploaded_images;
    std::unique_ptr<DirectDrawRenderer> direct_draw;
    std::unique_ptr<OpenGLRenderer> open_gl;
    std::unique_ptr<D3D9Renderer> direct3d9;
#endif
};

namespace {

int transform_edge(int value, int source_extent, int destination_origin,
    int destination_extent) noexcept {
    const auto scaled = static_cast<std::int64_t>(value) * destination_extent;
    auto quotient = scaled / source_extent;
    if (scaled < 0 && scaled % source_extent != 0) --quotient;
    const auto result = static_cast<std::int64_t>(destination_origin) + quotient;
    if (result < std::numeric_limits<int>::min()) return std::numeric_limits<int>::min();
    if (result > std::numeric_limits<int>::max()) return std::numeric_limits<int>::max();
    return static_cast<int>(result);
}

}

GraphicsRenderer::GraphicsRenderer() : impl_(std::make_unique<Impl>()) {}
GraphicsRenderer::~GraphicsRenderer() = default;

void GraphicsRenderer::initialize(GraphicsBackend backend, void* window_handle,
    std::uint32_t width, std::uint32_t height) {
#ifndef _WIN32
    (void)backend; (void)window_handle; (void)width; (void)height;
    throw std::runtime_error("the recovered graphics backends require Windows");
#else
    std::unique_ptr<DirectDrawRenderer> direct_draw;
    std::unique_ptr<OpenGLRenderer> open_gl;
    std::unique_ptr<D3D9Renderer> direct3d9;
    switch (backend) {
    case GraphicsBackend::direct_draw:
        direct_draw = std::make_unique<DirectDrawRenderer>();
        direct_draw->initialize(window_handle, width, height);
        for (const auto& [id, image] : impl_->uploaded_images) direct_draw->upload(id, image);
        break;
    case GraphicsBackend::open_gl:
        open_gl = std::make_unique<OpenGLRenderer>();
        open_gl->initialize(window_handle, width, height);
        for (const auto& [id, image] : impl_->uploaded_images) open_gl->upload(id, image);
        break;
    case GraphicsBackend::direct3d9:
        direct3d9 = std::make_unique<D3D9Renderer>();
        direct3d9->initialize(window_handle, width, height);
        for (const auto& [id, image] : impl_->uploaded_images) direct3d9->upload(id, image);
        break;
    default:
        throw std::invalid_argument("unknown Tibia graphics backend selector");
    }
    impl_->direct_draw = std::move(direct_draw);
    impl_->open_gl = std::move(open_gl);
    impl_->direct3d9 = std::move(direct3d9);
    impl_->selected = backend;
    impl_->window_handle = window_handle;
    impl_->width = width;
    impl_->height = height;
#endif
}

void GraphicsRenderer::resize(std::uint32_t width, std::uint32_t height) {
#ifdef _WIN32
    if (width == 0 || height == 0) return;
    if (impl_->direct_draw) impl_->direct_draw->resize(width, height);
    else if (impl_->open_gl) impl_->open_gl->resize(width, height);
    else if (impl_->direct3d9) impl_->direct3d9->reset(width, height);
    else throw std::logic_error("graphics renderer is not initialized");
#else
    (void)width; (void)height;
    throw std::runtime_error("the recovered graphics backends require Windows");
#endif
}

void GraphicsRenderer::upload(std::uint32_t texture_id, const resources::TibiaSprImage& image) {
#ifdef _WIN32
    impl_->uploaded_images.insert_or_assign(texture_id, image);
    if (impl_->direct_draw) impl_->direct_draw->upload(texture_id, image);
    else if (impl_->open_gl) impl_->open_gl->upload(texture_id, image);
    else if (impl_->direct3d9) impl_->direct3d9->upload(texture_id, image);
    else throw std::logic_error("graphics renderer is not initialized");
#else
    (void)texture_id; (void)image;
    throw std::runtime_error("the recovered graphics backends require Windows");
#endif
}

void GraphicsRenderer::begin_frame(RgbaColor clear) {
    impl_->draw_transform.reset();
#ifdef _WIN32
    if (impl_->direct_draw) impl_->direct_draw->begin_frame(clear);
    else if (impl_->open_gl) {
        const auto argb = (static_cast<std::uint32_t>(clear.alpha) << 24U) |
            (static_cast<std::uint32_t>(clear.red) << 16U) |
            (static_cast<std::uint32_t>(clear.green) << 8U) | clear.blue;
        impl_->open_gl->begin_frame(argb);
    } else if (impl_->direct3d9) {
        const auto argb = (static_cast<std::uint32_t>(clear.alpha) << 24U) |
            (static_cast<std::uint32_t>(clear.red) << 16U) |
            (static_cast<std::uint32_t>(clear.green) << 8U) | clear.blue;
        impl_->direct3d9->begin_frame(argb);
    } else throw std::logic_error("graphics renderer is not initialized");
#else
    (void)clear;
    throw std::runtime_error("the recovered graphics backends require Windows");
#endif
}

void GraphicsRenderer::set_draw_transform(int source_width, int source_height,
    int destination_x, int destination_y, int destination_width,
    int destination_height, int clip_x, int clip_y, int clip_width,
    int clip_height) {
    if (source_width <= 0 || source_height <= 0 ||
        destination_width <= 0 || destination_height <= 0 ||
        clip_width <= 0 || clip_height <= 0)
        throw std::invalid_argument("draw transform dimensions must be positive");
    impl_->draw_transform = Impl::DrawTransform{
        source_width, source_height, destination_x, destination_y,
        destination_width, destination_height, clip_x, clip_y,
        clip_width, clip_height};
}

void GraphicsRenderer::clear_draw_transform() noexcept {
    impl_->draw_transform.reset();
}

void GraphicsRenderer::draw(std::uint32_t texture_id, int x, int y, int width, int height,
    std::uint8_t opacity) {
    if (impl_->draw_transform) {
        const auto& transform = *impl_->draw_transform;
        const auto left = transform_edge(x, transform.source_width,
            transform.destination_x, transform.destination_width);
        const auto top = transform_edge(y, transform.source_height,
            transform.destination_y, transform.destination_height);
        const auto right = transform_edge(x + width, transform.source_width,
            transform.destination_x, transform.destination_width);
        const auto bottom = transform_edge(y + height, transform.source_height,
            transform.destination_y, transform.destination_height);
        x = left;
        y = top;
        width = right - left;
        height = bottom - top;
        if (width <= 0 || height <= 0) return;
#ifdef _WIN32
        if (impl_->direct_draw)
            impl_->direct_draw->draw_clipped(texture_id, x, y, width, height,
                transform.clip_x, transform.clip_y, transform.clip_width,
                transform.clip_height, opacity);
        else if (impl_->open_gl)
            impl_->open_gl->draw_clipped(texture_id, x, y, width, height,
                transform.clip_x, transform.clip_y, transform.clip_width,
                transform.clip_height, opacity);
        else if (impl_->direct3d9)
            impl_->direct3d9->draw_clipped(texture_id, x, y, width, height,
                transform.clip_x, transform.clip_y, transform.clip_width,
                transform.clip_height, opacity);
        else throw std::logic_error("graphics renderer is not initialized");
        return;
#else
        throw std::runtime_error("the recovered graphics backends require Windows");
#endif
    }
#ifdef _WIN32
    if (impl_->direct_draw) {
        impl_->direct_draw->draw(texture_id, x, y, width, height, opacity);
    } else if (impl_->open_gl) impl_->open_gl->draw(texture_id, x, y, width, height, opacity);
    else if (impl_->direct3d9) impl_->direct3d9->draw(texture_id, x, y, width, height, opacity);
    else throw std::logic_error("graphics renderer is not initialized");
#else
    (void)texture_id; (void)x; (void)y; (void)width; (void)height; (void)opacity;
    throw std::runtime_error("the recovered graphics backends require Windows");
#endif
}

void GraphicsRenderer::draw_clipped(std::uint32_t texture_id, int destination_x,
    int destination_y, int destination_width, int destination_height,
    int clip_x, int clip_y, int clip_width, int clip_height,
    std::uint8_t opacity) {
    if (destination_width <= 0 || destination_height <= 0 ||
        clip_width <= 0 || clip_height <= 0 || opacity == 0) return;
    if (impl_->draw_transform) {
        const auto& transform = *impl_->draw_transform;
        const auto map_x = [&](int value) {
            return transform_edge(value, transform.source_width,
                transform.destination_x, transform.destination_width);
        };
        const auto map_y = [&](int value) {
            return transform_edge(value, transform.source_height,
                transform.destination_y, transform.destination_height);
        };
        const auto left = map_x(destination_x);
        const auto top = map_y(destination_y);
        const auto right = map_x(destination_x + destination_width);
        const auto bottom = map_y(destination_y + destination_height);
        const auto clip_left = map_x(clip_x);
        const auto clip_top = map_y(clip_y);
        const auto clip_right = map_x(clip_x + clip_width);
        const auto clip_bottom = map_y(clip_y + clip_height);
        destination_x = left;
        destination_y = top;
        destination_width = right - left;
        destination_height = bottom - top;
        clip_x = clip_left;
        clip_y = clip_top;
        clip_width = clip_right - clip_left;
        clip_height = clip_bottom - clip_top;
        const auto physical_right = transform.clip_x + transform.clip_width;
        const auto physical_bottom = transform.clip_y + transform.clip_height;
        const auto right_edge = std::min(clip_x + clip_width, physical_right);
        const auto bottom_edge = std::min(clip_y + clip_height, physical_bottom);
        clip_x = std::max(clip_x, transform.clip_x);
        clip_y = std::max(clip_y, transform.clip_y);
        clip_width = right_edge - clip_x;
        clip_height = bottom_edge - clip_y;
        if (destination_width <= 0 || destination_height <= 0 ||
            clip_width <= 0 || clip_height <= 0) return;
    }
#ifdef _WIN32
    if (impl_->direct_draw)
        impl_->direct_draw->draw_clipped(texture_id, destination_x, destination_y,
            destination_width, destination_height, clip_x, clip_y,
            clip_width, clip_height, opacity);
    else if (impl_->open_gl)
        impl_->open_gl->draw_clipped(texture_id, destination_x, destination_y,
            destination_width, destination_height, clip_x, clip_y,
            clip_width, clip_height, opacity);
    else if (impl_->direct3d9)
        impl_->direct3d9->draw_clipped(texture_id, destination_x, destination_y,
            destination_width, destination_height, clip_x, clip_y,
            clip_width, clip_height, opacity);
    else throw std::logic_error("graphics renderer is not initialized");
#else
    (void)texture_id; (void)destination_x; (void)destination_y;
    (void)destination_width; (void)destination_height; (void)clip_x;
    (void)clip_y; (void)clip_width; (void)clip_height; (void)opacity;
    throw std::runtime_error("the recovered graphics backends require Windows");
#endif
}

void GraphicsRenderer::draw_light_quads(std::span<const LightQuad> quads,
    int clip_x, int clip_y, int clip_width, int clip_height) {
    if (quads.empty() || clip_width <= 0 || clip_height <= 0) return;
    if (impl_->draw_transform) {
        const auto& transform = *impl_->draw_transform;
        const auto map_x = [&](int value) {
            return transform_edge(value, transform.source_width,
                transform.destination_x, transform.destination_width);
        };
        const auto map_y = [&](int value) {
            return transform_edge(value, transform.source_height,
                transform.destination_y, transform.destination_height);
        };
        std::vector<LightQuad> transformed;
        transformed.reserve(quads.size());
        for (const auto& quad : quads) {
            const auto left = map_x(quad.x);
            const auto top = map_y(quad.y);
            const auto right = map_x(quad.x + quad.width);
            const auto bottom = map_y(quad.y + quad.height);
            if (right <= left || bottom <= top) continue;
            transformed.push_back({left, top, right - left, bottom - top, quad.corners});
        }
        const auto clip_left = map_x(clip_x);
        const auto clip_top = map_y(clip_y);
        const auto clip_right = map_x(clip_x + clip_width);
        const auto clip_bottom = map_y(clip_y + clip_height);
        const auto physical_right = transform.clip_x + transform.clip_width;
        const auto physical_bottom = transform.clip_y + transform.clip_height;
        const auto right_edge = std::min(clip_right, physical_right);
        const auto bottom_edge = std::min(clip_bottom, physical_bottom);
        clip_x = std::max(clip_left, transform.clip_x);
        clip_y = std::max(clip_top, transform.clip_y);
        clip_width = right_edge - clip_x;
        clip_height = bottom_edge - clip_y;
        quads = transformed;
        if (clip_width <= 0 || clip_height <= 0 || quads.empty()) return;
    }
#ifdef _WIN32
    if (impl_->direct_draw)
        impl_->direct_draw->draw_light_quads(quads, clip_x, clip_y, clip_width, clip_height);
    else if (impl_->open_gl)
        impl_->open_gl->draw_light_quads(quads, clip_x, clip_y, clip_width, clip_height);
    else if (impl_->direct3d9)
        impl_->direct3d9->draw_light_quads(quads, clip_x, clip_y, clip_width, clip_height);
    else throw std::logic_error("graphics renderer is not initialized");
#else
    throw std::runtime_error("the recovered graphics backends require Windows");
#endif
}

bool GraphicsRenderer::end_frame() {
#ifdef _WIN32
    if (impl_->direct_draw) return impl_->direct_draw->end_frame();
    if (impl_->open_gl) { impl_->open_gl->end_frame(); return true; }
    if (impl_->direct3d9) return impl_->direct3d9->end_frame();
    throw std::logic_error("graphics renderer is not initialized");
#else
    throw std::runtime_error("the recovered graphics backends require Windows");
#endif
}

GraphicsBackend GraphicsRenderer::backend() const { return impl_->selected; }

bool GraphicsRenderer::supports_light_quads() const noexcept {
#ifdef _WIN32
    return impl_->open_gl != nullptr || impl_->direct3d9 != nullptr;
#else
    return false;
#endif
}

} // namespace tibia::render
