#include "render/d3d9_renderer.hpp"

#include <windows.h>
#include <d3d9.h>

#include <array>
#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace tibia::render {
namespace {

void check_hr(HRESULT result, const char* operation) {
    if (FAILED(result)) throw std::runtime_error(std::string(operation) + " failed (HRESULT " +
        std::to_string(static_cast<unsigned long>(result)) + ")");
}

struct Vertex {
    float x, y, z, rhw;
    D3DCOLOR diffuse;
    float u, v;
};

void configure_2d_device(IDirect3DDevice9* device) {
    check_hr(device->SetVertexShader(nullptr), "SetVertexShader");
    check_hr(device->SetPixelShader(nullptr), "SetPixelShader");
    check_hr(device->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1), "SetFVF");
    check_hr(device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE), "SetRenderState(ALPHABLENDENABLE)");
    check_hr(device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA), "SetRenderState(SRCBLEND)");
    check_hr(device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA), "SetRenderState(DESTBLEND)");
    check_hr(device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE), "SetRenderState(CULLMODE)");
    check_hr(device->SetRenderState(D3DRS_ZENABLE, FALSE), "SetRenderState(ZENABLE)");
    check_hr(device->SetRenderState(D3DRS_LIGHTING, FALSE), "SetRenderState(LIGHTING)");
    check_hr(device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE), "SetTextureStageState(COLOROP)");
    check_hr(device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE), "SetTextureStageState(COLORARG1)");
    check_hr(device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE), "SetTextureStageState(COLORARG2)");
    check_hr(device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE), "SetTextureStageState(ALPHAOP)");
    check_hr(device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE), "SetTextureStageState(ALPHAARG1)");
    check_hr(device->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE), "SetTextureStageState(ALPHAARG2)");
    check_hr(device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT), "SetSamplerState(MINFILTER)");
    check_hr(device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT), "SetSamplerState(MAGFILTER)");
    check_hr(device->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP), "SetSamplerState(ADDRESSU)");
    check_hr(device->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP), "SetSamplerState(ADDRESSV)");
}

} // namespace

struct D3D9Renderer::Impl {
    struct Texture {
        IDirect3DTexture9* handle{};
        std::uint8_t content_width{32};
        std::uint8_t content_height{32};
    };

    HMODULE module{};
    IDirect3D9* d3d{};
    IDirect3DDevice9* device{};
    D3DPRESENT_PARAMETERS presentation{};
    std::unordered_map<std::uint32_t, Texture> textures;
    bool in_scene{};

    ~Impl() { clear(); }
    void clear() noexcept {
        if (in_scene && device) { device->EndScene(); in_scene = false; }
        for (auto& [id, texture] : textures) {
            (void)id;
            if (texture.handle) texture.handle->Release();
        }
        textures.clear();
        if (device) { device->Release(); device = nullptr; }
        if (d3d) { d3d->Release(); d3d = nullptr; }
        if (module) { FreeLibrary(module); module = nullptr; }
    }
};

D3D9Renderer::D3D9Renderer() : impl_(std::make_unique<Impl>()) {}
D3D9Renderer::~D3D9Renderer() = default;

void D3D9Renderer::initialize(void* window_handle, std::uint32_t width, std::uint32_t height) {
    if (!window_handle) throw std::invalid_argument("D3D9 renderer requires a valid window handle");
    if (width == 0 || height == 0) throw std::invalid_argument("D3D9 backbuffer dimensions must be nonzero");
    impl_->clear();
    impl_->module = LoadLibraryA("d3d9.dll");
    if (!impl_->module) throw std::runtime_error("could not load d3d9.dll");
    const auto create9 = reinterpret_cast<IDirect3D9* (WINAPI*)(UINT)>(
        GetProcAddress(impl_->module, "Direct3DCreate9"));
    if (!create9) { impl_->clear(); throw std::runtime_error("d3d9.dll does not export Direct3DCreate9"); }
    impl_->d3d = create9(D3D_SDK_VERSION);
    if (!impl_->d3d) throw std::runtime_error("Direct3DCreate9 could not initialize Direct3D 9");

    auto& pp = impl_->presentation;
    pp = {};
    pp.BackBufferWidth = width;
    pp.BackBufferHeight = height;
    pp.BackBufferFormat = D3DFMT_UNKNOWN;
    pp.BackBufferCount = 1;
    pp.MultiSampleType = D3DMULTISAMPLE_NONE;
    pp.SwapEffect = D3DSWAPEFFECT_COPY;
    pp.hDeviceWindow = static_cast<HWND>(window_handle);
    pp.Windowed = TRUE;
    pp.EnableAutoDepthStencil = FALSE;
    pp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;

    HRESULT result = impl_->d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL,
        pp.hDeviceWindow, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, &impl_->device);
    if (FAILED(result)) {
        result = impl_->d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL,
            pp.hDeviceWindow, D3DCREATE_HARDWARE_VERTEXPROCESSING, &pp, &impl_->device);
    }
    if (FAILED(result)) {
        impl_->clear();
        check_hr(result, "IDirect3D9::CreateDevice");
    }

    configure_2d_device(impl_->device);
}

void D3D9Renderer::reset(std::uint32_t width, std::uint32_t height) {
    if (!ready()) throw std::logic_error("D3D9 renderer is not initialized");
    if (width == 0 || height == 0) throw std::invalid_argument("D3D9 backbuffer dimensions must be nonzero");
    if (impl_->in_scene) throw std::logic_error("cannot reset the D3D9 device during a frame");
    impl_->presentation.BackBufferWidth = width;
    impl_->presentation.BackBufferHeight = height;
    const auto result = impl_->device->Reset(&impl_->presentation);
    check_hr(result, "IDirect3DDevice9::Reset");
    configure_2d_device(impl_->device);
}

void D3D9Renderer::upload(std::uint32_t texture_id, const resources::TibiaSprImage& image) {
    if (!ready()) throw std::logic_error("D3D9 renderer is not initialized");
    IDirect3DTexture9* texture{};
    check_hr(impl_->device->CreateTexture(32, 32, 1, 0, D3DFMT_A8R8G8B8,
        D3DPOOL_MANAGED, &texture, nullptr), "CreateTexture");
    D3DLOCKED_RECT locked{};
    const auto lock_result = texture->LockRect(0, &locked, nullptr, 0);
    if (FAILED(lock_result)) { texture->Release(); check_hr(lock_result, "LockRect"); }
    for (std::size_t y = 0; y < 32; ++y) {
        auto* row = static_cast<std::uint8_t*>(locked.pBits) + y * locked.Pitch;
        for (std::size_t x = 0; x < 32; ++x) {
            const auto source = (y * 32 + x) * 4;
            const auto target = x * 4;
            row[target] = image.rgba[source + 2];
            row[target + 1] = image.rgba[source + 1];
            row[target + 2] = image.rgba[source];
            row[target + 3] = image.rgba[source + 3];
        }
    }
    const auto unlock_result = texture->UnlockRect(0);
    if (FAILED(unlock_result)) { texture->Release(); check_hr(unlock_result, "UnlockRect"); }
    const auto found = impl_->textures.find(texture_id);
    const Impl::Texture uploaded{texture, image.content_width, image.content_height};
    if (found != impl_->textures.end()) {
        found->second.handle->Release();
        found->second = uploaded;
    }
    else {
        try { impl_->textures.emplace(texture_id, uploaded); }
        catch (...) { texture->Release(); throw; }
    }
}

void D3D9Renderer::begin_frame(std::uint32_t clear_color) {
    if (!ready()) throw std::logic_error("D3D9 renderer is not initialized");
    if (impl_->in_scene) throw std::logic_error("D3D9 frame is already active");
    check_hr(impl_->device->Clear(0, nullptr, D3DCLEAR_TARGET, clear_color, 1.0F, 0), "Clear");
    check_hr(impl_->device->BeginScene(), "BeginScene");
    impl_->in_scene = true;
}

void D3D9Renderer::draw(std::uint32_t texture_id, int x, int y, int width, int height,
    std::uint8_t opacity) {
    if (!impl_->in_scene) throw std::logic_error("begin_frame must be called before draw");
    if (width <= 0 || height <= 0 || opacity == 0) return;
    const auto found = impl_->textures.find(texture_id);
    if (found == impl_->textures.end()) throw std::out_of_range("D3D9 texture ID is not uploaded");
    const auto right_u = static_cast<float>(found->second.content_width) / 32.0F;
    const auto bottom_v = static_cast<float>(found->second.content_height) / 32.0F;
    const auto left = static_cast<float>(x) - 0.5F;
    const auto top = static_cast<float>(y) - 0.5F;
    const auto right = left + static_cast<float>(width);
    const auto bottom = top + static_cast<float>(height);
    const auto diffuse = D3DCOLOR_ARGB(opacity, 255, 255, 255);
    const std::array<Vertex, 4> vertices{{
        {left, top, 0.0F, 1.0F, diffuse, 0.0F, 0.0F},
        {right, top, 0.0F, 1.0F, diffuse, right_u, 0.0F},
        {left, bottom, 0.0F, 1.0F, diffuse, 0.0F, bottom_v},
        {right, bottom, 0.0F, 1.0F, diffuse, right_u, bottom_v},
    }};
    check_hr(impl_->device->SetTexture(0, found->second.handle), "SetTexture");
    check_hr(impl_->device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, vertices.data(), sizeof(Vertex)),
        "DrawPrimitiveUP");
}

void D3D9Renderer::draw_clipped(std::uint32_t texture_id, int destination_x,
    int destination_y, int destination_width, int destination_height,
    int clip_x, int clip_y, int clip_width, int clip_height,
    std::uint8_t opacity) {
    if (!impl_->in_scene) throw std::logic_error("begin_frame must be called before draw");
    if (destination_width <= 0 || destination_height <= 0 ||
        clip_width <= 0 || clip_height <= 0 || opacity == 0) return;
    const auto left = std::clamp<std::int64_t>(clip_x, 0, impl_->presentation.BackBufferWidth);
    const auto top = std::clamp<std::int64_t>(clip_y, 0, impl_->presentation.BackBufferHeight);
    const auto right = std::clamp<std::int64_t>(
        static_cast<std::int64_t>(clip_x) + clip_width, 0, impl_->presentation.BackBufferWidth);
    const auto bottom = std::clamp<std::int64_t>(
        static_cast<std::int64_t>(clip_y) + clip_height, 0, impl_->presentation.BackBufferHeight);
    if (left >= right || top >= bottom) return;

    DWORD previous_enabled{};
    RECT previous_rect{};
    check_hr(impl_->device->GetRenderState(D3DRS_SCISSORTESTENABLE, &previous_enabled),
        "GetRenderState(SCISSORTESTENABLE)");
    check_hr(impl_->device->GetScissorRect(&previous_rect), "GetScissorRect");
    RECT clipped_rect{static_cast<LONG>(left), static_cast<LONG>(top),
        static_cast<LONG>(right), static_cast<LONG>(bottom)};
    check_hr(impl_->device->SetScissorRect(&clipped_rect), "SetScissorRect");
    check_hr(impl_->device->SetRenderState(D3DRS_SCISSORTESTENABLE, TRUE),
        "SetRenderState(SCISSORTESTENABLE)");
    try {
        draw(texture_id, destination_x, destination_y,
            destination_width, destination_height, opacity);
    } catch (...) {
        impl_->device->SetScissorRect(&previous_rect);
        impl_->device->SetRenderState(D3DRS_SCISSORTESTENABLE, previous_enabled);
        throw;
    }
    check_hr(impl_->device->SetScissorRect(&previous_rect), "SetScissorRect");
    check_hr(impl_->device->SetRenderState(D3DRS_SCISSORTESTENABLE, previous_enabled),
        "SetRenderState(SCISSORTESTENABLE)");
}

void D3D9Renderer::draw_light_quads(std::span<const LightQuad> quads,
    int clip_x, int clip_y, int clip_width, int clip_height) {
    if (!impl_->in_scene) throw std::logic_error("begin_frame must be called before draw");
    if (quads.empty() || clip_width <= 0 || clip_height <= 0) return;
    const auto left = std::clamp<std::int64_t>(clip_x, 0, impl_->presentation.BackBufferWidth);
    const auto top = std::clamp<std::int64_t>(clip_y, 0, impl_->presentation.BackBufferHeight);
    const auto right = std::clamp<std::int64_t>(
        static_cast<std::int64_t>(clip_x) + clip_width, 0, impl_->presentation.BackBufferWidth);
    const auto bottom = std::clamp<std::int64_t>(
        static_cast<std::int64_t>(clip_y) + clip_height, 0, impl_->presentation.BackBufferHeight);
    if (left >= right || top >= bottom) return;
    DWORD previous_enabled{};
    RECT previous_rect{};
    check_hr(impl_->device->GetRenderState(D3DRS_SCISSORTESTENABLE, &previous_enabled),
        "GetRenderState(SCISSORTESTENABLE)");
    check_hr(impl_->device->GetScissorRect(&previous_rect), "GetScissorRect");
    RECT clipped_rect{static_cast<LONG>(left), static_cast<LONG>(top),
        static_cast<LONG>(right), static_cast<LONG>(bottom)};
    check_hr(impl_->device->SetScissorRect(&clipped_rect), "SetScissorRect");
    check_hr(impl_->device->SetRenderState(D3DRS_SCISSORTESTENABLE, TRUE),
        "SetRenderState(SCISSORTESTENABLE)");
    try {
        check_hr(impl_->device->SetTexture(0, nullptr), "SetTexture");
        check_hr(impl_->device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1),
            "SetTextureStageState(COLOROP)");
        check_hr(impl_->device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE),
            "SetTextureStageState(COLORARG1)");
        check_hr(impl_->device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1),
            "SetTextureStageState(ALPHAOP)");
        check_hr(impl_->device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE),
            "SetTextureStageState(ALPHAARG1)");
        check_hr(impl_->device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_DESTCOLOR),
            "SetRenderState(SRCBLEND)");
        check_hr(impl_->device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ZERO),
            "SetRenderState(DESTBLEND)");
        for (const auto& quad : quads) {
            if (quad.width <= 0 || quad.height <= 0) continue;
            const auto x = static_cast<float>(quad.x) - 0.5F;
            const auto y = static_cast<float>(quad.y) - 0.5F;
            const auto right_edge = x + static_cast<float>(quad.width);
            const auto bottom_edge = y + static_cast<float>(quad.height);
            const auto color = [](const RgbColor& value) {
                return D3DCOLOR_ARGB(255, value.red, value.green, value.blue);
            };
            const std::array<Vertex, 4> vertices{{
                {x, y, 0.0F, 1.0F, color(quad.corners[0]), 0.0F, 0.0F},
                {right_edge, y, 0.0F, 1.0F, color(quad.corners[1]), 0.0F, 0.0F},
                {x, bottom_edge, 0.0F, 1.0F, color(quad.corners[2]), 0.0F, 0.0F},
                {right_edge, bottom_edge, 0.0F, 1.0F, color(quad.corners[3]), 0.0F, 0.0F},
            }};
            check_hr(impl_->device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2,
                vertices.data(), sizeof(Vertex)), "DrawPrimitiveUP");
        }
    } catch (...) {
        configure_2d_device(impl_->device);
        impl_->device->SetScissorRect(&previous_rect);
        impl_->device->SetRenderState(D3DRS_SCISSORTESTENABLE, previous_enabled);
        throw;
    }
    configure_2d_device(impl_->device);
    check_hr(impl_->device->SetScissorRect(&previous_rect), "SetScissorRect");
    check_hr(impl_->device->SetRenderState(D3DRS_SCISSORTESTENABLE, previous_enabled),
        "SetRenderState(SCISSORTESTENABLE)");
}

bool D3D9Renderer::end_frame() {
    if (!impl_->device || !impl_->in_scene) throw std::logic_error("no D3D9 frame is active");
    check_hr(impl_->device->EndScene(), "EndScene");
    impl_->in_scene = false;
    const auto result = impl_->device->Present(nullptr, nullptr, nullptr, nullptr);
    if (result == D3DERR_DEVICELOST || result == D3DERR_DEVICENOTRESET) return false;
    check_hr(result, "Present");
    return true;
}

bool D3D9Renderer::ready() const noexcept { return impl_ && impl_->device; }

} // namespace tibia::render
