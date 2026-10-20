#include "render/directdraw_renderer.hpp"

#include <windows.h>
#include <ddraw.h>

#include <cstring>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace tibia::render {
namespace {

void check_dd(HRESULT result, const char* operation) {
    if (FAILED(result)) throw std::runtime_error(std::string(operation) + " failed (HRESULT " +
        std::to_string(static_cast<unsigned long>(result)) + ")");
}

} // namespace

struct DirectDrawRenderer::Impl {
    using DirectDrawCreateFn = HRESULT (WINAPI*)(GUID*, LPDIRECTDRAW*, IUnknown*);
    HMODULE module{};
    LPDIRECTDRAW direct_draw{};
    LPDIRECTDRAWSURFACE primary{};
    LPDIRECTDRAWSURFACE backbuffer{};
    HWND window{};
    std::uint32_t width{};
    std::uint32_t height{};
    std::unique_ptr<SoftwareSurface> canvas;
    std::unordered_map<std::uint32_t, resources::TibiaSprImage> textures;
    bool in_frame{};

    void release_surfaces() noexcept {
        if (backbuffer) { backbuffer->Release(); backbuffer = nullptr; }
        if (primary) { primary->Release(); primary = nullptr; }
    }
    void clear() noexcept {
        release_surfaces();
        if (direct_draw) { direct_draw->Release(); direct_draw = nullptr; }
        if (module) { FreeLibrary(module); module = nullptr; }
        canvas.reset(); textures.clear(); window = nullptr; in_frame = false;
    }
    void create_backbuffer() {
        DDSURFACEDESC desc{};
        desc.dwSize = sizeof(desc);
        desc.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT | DDSD_PIXELFORMAT;
        desc.dwWidth = width;
        desc.dwHeight = height;
        desc.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_SYSTEMMEMORY;
        desc.ddpfPixelFormat.dwSize = sizeof(DDPIXELFORMAT);
        desc.ddpfPixelFormat.dwFlags = DDPF_RGB | DDPF_ALPHAPIXELS;
        desc.ddpfPixelFormat.dwRGBBitCount = 32;
        desc.ddpfPixelFormat.dwRBitMask = 0x00ff0000;
        desc.ddpfPixelFormat.dwGBitMask = 0x0000ff00;
        desc.ddpfPixelFormat.dwBBitMask = 0x000000ff;
        desc.ddpfPixelFormat.dwRGBAlphaBitMask = 0xff000000;
        check_dd(direct_draw->CreateSurface(&desc, &backbuffer, nullptr), "IDirectDraw::CreateSurface(backbuffer)");
    }
};

DirectDrawRenderer::DirectDrawRenderer() : impl_(std::make_unique<Impl>()) {}
DirectDrawRenderer::~DirectDrawRenderer() = default;

void DirectDrawRenderer::initialize(void* window_handle, std::uint32_t width, std::uint32_t height) {
    if (!window_handle) throw std::invalid_argument("DirectDraw renderer requires a window handle");
    if (width == 0 || height == 0) throw std::invalid_argument("DirectDraw surface dimensions must be nonzero");
    impl_->clear();
    impl_->window = static_cast<HWND>(window_handle);
    impl_->width = width;
    impl_->height = height;
    impl_->module = LoadLibraryA("ddraw.dll");
    if (!impl_->module) throw std::runtime_error("could not load ddraw.dll");
    const auto create = reinterpret_cast<Impl::DirectDrawCreateFn>(
        GetProcAddress(impl_->module, "DirectDrawCreate"));
    if (!create) { impl_->clear(); throw std::runtime_error("ddraw.dll does not export DirectDrawCreate"); }
    check_dd(create(nullptr, &impl_->direct_draw, nullptr), "DirectDrawCreate");
    check_dd(impl_->direct_draw->SetCooperativeLevel(impl_->window, DDSCL_NORMAL),
        "IDirectDraw::SetCooperativeLevel");

    DDSURFACEDESC primary_desc{};
    primary_desc.dwSize = sizeof(primary_desc);
    primary_desc.dwFlags = DDSD_CAPS;
    primary_desc.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;
    check_dd(impl_->direct_draw->CreateSurface(&primary_desc, &impl_->primary, nullptr),
        "IDirectDraw::CreateSurface(primary)");
    impl_->canvas = std::make_unique<SoftwareSurface>(width, height, RgbaColor{});
    impl_->create_backbuffer();
}

void DirectDrawRenderer::resize(std::uint32_t width, std::uint32_t height) {
    if (!impl_->direct_draw) throw std::logic_error("DirectDraw renderer is not initialized");
    if (width == 0 || height == 0) throw std::invalid_argument("DirectDraw surface dimensions must be nonzero");
    if (impl_->in_frame) throw std::logic_error("cannot resize DirectDraw surfaces during a frame");
    impl_->width = width;
    impl_->height = height;
    impl_->canvas = std::make_unique<SoftwareSurface>(width, height, RgbaColor{});
    if (impl_->backbuffer) { impl_->backbuffer->Release(); impl_->backbuffer = nullptr; }
    impl_->create_backbuffer();
}

void DirectDrawRenderer::upload(std::uint32_t texture_id, const resources::TibiaSprImage& image) {
    if (!impl_->direct_draw) throw std::logic_error("DirectDraw renderer is not initialized");
    impl_->textures.insert_or_assign(texture_id, image);
}

void DirectDrawRenderer::begin_frame(RgbaColor clear) {
    if (!impl_->canvas) throw std::logic_error("DirectDraw renderer is not initialized");
    if (impl_->in_frame) throw std::logic_error("DirectDraw frame is already active");
    impl_->canvas->clear(clear);
    impl_->in_frame = true;
}

void DirectDrawRenderer::draw(std::uint32_t texture_id, int x, int y, int width, int height,
    std::uint8_t opacity) {
    if (!impl_->in_frame) throw std::logic_error("begin_frame must be called before draw");
    const auto found = impl_->textures.find(texture_id);
    if (found == impl_->textures.end()) throw std::out_of_range("DirectDraw texture ID is not uploaded");
    if (width == 32 && height == 32) impl_->canvas->draw(found->second, x, y, opacity);
    else if (width > 0 && height > 0)
        impl_->canvas->draw_scaled(found->second, x, y,
            static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height), opacity);
}

void DirectDrawRenderer::draw_clipped(std::uint32_t texture_id, int destination_x,
    int destination_y, int destination_width, int destination_height,
    int clip_x, int clip_y, int clip_width, int clip_height,
    std::uint8_t opacity) {
    if (!impl_->in_frame) throw std::logic_error("begin_frame must be called before draw");
    const auto found = impl_->textures.find(texture_id);
    if (found == impl_->textures.end()) throw std::out_of_range("DirectDraw texture ID is not uploaded");
    impl_->canvas->draw_clipped(found->second, destination_x, destination_y,
        destination_width, destination_height, clip_x, clip_y,
        clip_width, clip_height, opacity);
}

void DirectDrawRenderer::draw_light_quads(std::span<const LightQuad> quads,
    int clip_x, int clip_y, int clip_width, int clip_height) {
    if (!impl_->in_frame) throw std::logic_error("begin_frame must be called before draw");
    impl_->canvas->draw_light_quads(quads, clip_x, clip_y, clip_width, clip_height);
}

bool DirectDrawRenderer::end_frame() {
    if (!impl_->in_frame || !impl_->backbuffer || !impl_->primary)
        throw std::logic_error("no DirectDraw frame is active");
    impl_->in_frame = false;
    DDSURFACEDESC locked{};
    locked.dwSize = sizeof(locked);
    auto result = impl_->backbuffer->Lock(nullptr, &locked, DDLOCK_WAIT | DDLOCK_SURFACEMEMORYPTR, nullptr);
    if (result == DDERR_SURFACELOST) {
        impl_->backbuffer->Restore();
        return false;
    }
    check_dd(result, "IDirectDrawSurface::Lock");
    const auto& rgba = impl_->canvas->pixels();
    for (std::uint32_t y = 0; y < impl_->height; ++y) {
        auto* destination = static_cast<std::uint8_t*>(locked.lpSurface) +
            static_cast<std::ptrdiff_t>(y) * locked.lPitch;
        for (std::uint32_t x = 0; x < impl_->width; ++x) {
            const auto source = (static_cast<std::size_t>(y) * impl_->width + x) * 4;
            const auto target = static_cast<std::size_t>(x) * 4;
            destination[target] = rgba[source + 2];
            destination[target + 1] = rgba[source + 1];
            destination[target + 2] = rgba[source];
            destination[target + 3] = rgba[source + 3];
        }
    }
    check_dd(impl_->backbuffer->Unlock(nullptr), "IDirectDrawSurface::Unlock");

    RECT destination{0, 0, static_cast<LONG>(impl_->width), static_cast<LONG>(impl_->height)};
    POINT origin{0, 0};
    if (!ClientToScreen(impl_->window, &origin)) throw std::runtime_error("ClientToScreen failed");
    OffsetRect(&destination, origin.x, origin.y);
    RECT source{0, 0, static_cast<LONG>(impl_->width), static_cast<LONG>(impl_->height)};
    result = impl_->primary->Blt(&destination, impl_->backbuffer, &source, DDBLT_WAIT, nullptr);
    if (result == DDERR_SURFACELOST) {
        impl_->primary->Restore();
        impl_->backbuffer->Restore();
        return false;
    }
    check_dd(result, "IDirectDrawSurface::Blt");
    return true;
}

} // namespace tibia::render
