#include "render/opengl_renderer.hpp"

#include <windows.h>
#include <gl/GL.h>

#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace tibia::render {

struct OpenGLRenderer::Impl {
    struct Texture {
        GLuint id{};
        std::uint8_t content_width{32};
        std::uint8_t content_height{32};
    };

    HWND window{};
    HDC dc{};
    HGLRC context{};
    std::uint32_t width{};
    std::uint32_t height{};
    std::unordered_map<std::uint32_t, Texture> textures;
    bool in_frame{};

    void make_current() const {
        if (!dc || !context || !wglMakeCurrent(dc, context))
            throw std::runtime_error("wglMakeCurrent failed");
    }
    void clear() noexcept {
        if (context && dc) wglMakeCurrent(dc, context);
        for (const auto& [id, texture] : textures) {
            (void)id;
            if (texture.id) glDeleteTextures(1, &texture.id);
        }
        textures.clear();
        if (context) { wglMakeCurrent(nullptr, nullptr); wglDeleteContext(context); context = nullptr; }
        if (dc && window) { ReleaseDC(window, dc); dc = nullptr; }
        window = nullptr;
        in_frame = false;
    }
    void set_view() {
        glViewport(0, 0, static_cast<GLsizei>(width), static_cast<GLsizei>(height));
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(0.0, static_cast<double>(width), static_cast<double>(height), 0.0, -1.0, 1.0);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
    }
};

OpenGLRenderer::OpenGLRenderer() : impl_(std::make_unique<Impl>()) {}
OpenGLRenderer::~OpenGLRenderer() = default;

void OpenGLRenderer::initialize(void* window_handle, std::uint32_t width, std::uint32_t height) {
    if (!window_handle) throw std::invalid_argument("OpenGL renderer requires a window handle");
    if (width == 0 || height == 0) throw std::invalid_argument("OpenGL viewport dimensions must be nonzero");
    impl_->clear();
    impl_->window = static_cast<HWND>(window_handle);
    impl_->dc = GetDC(impl_->window);
    if (!impl_->dc) throw std::runtime_error("GetDC failed for OpenGL window");

    PIXELFORMATDESCRIPTOR format{};
    format.nSize = sizeof(format);
    format.nVersion = 1;
    format.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    format.iPixelType = PFD_TYPE_RGBA;
    format.cColorBits = 32;
    format.cAlphaBits = 8;
    format.iLayerType = PFD_MAIN_PLANE;
    if (GetPixelFormat(impl_->dc) == 0) {
        const auto pixel_format = ChoosePixelFormat(impl_->dc, &format);
        if (pixel_format == 0 || !SetPixelFormat(impl_->dc, pixel_format, &format)) {
            impl_->clear();
            throw std::runtime_error("could not select an OpenGL pixel format");
        }
    }
    impl_->context = wglCreateContext(impl_->dc);
    if (!impl_->context) { impl_->clear(); throw std::runtime_error("wglCreateContext failed"); }
    impl_->make_current();
    impl_->width = width;
    impl_->height = height;
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    impl_->set_view();
}

void OpenGLRenderer::resize(std::uint32_t width, std::uint32_t height) {
    if (!impl_->context) throw std::logic_error("OpenGL renderer is not initialized");
    if (width == 0 || height == 0) throw std::invalid_argument("OpenGL viewport dimensions must be nonzero");
    if (impl_->in_frame) throw std::logic_error("cannot resize the OpenGL renderer during a frame");
    impl_->make_current();
    impl_->width = width;
    impl_->height = height;
    impl_->set_view();
}

void OpenGLRenderer::upload(std::uint32_t texture_id, const resources::TibiaSprImage& image) {
    if (!impl_->context) throw std::logic_error("OpenGL renderer is not initialized");
    impl_->make_current();
    GLuint texture{};
    glGenTextures(1, &texture);
    if (!texture) throw std::runtime_error("glGenTextures failed");
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 32, 32, 0, GL_RGBA, GL_UNSIGNED_BYTE, image.rgba.data());
    if (glGetError() != GL_NO_ERROR) { glDeleteTextures(1, &texture); throw std::runtime_error("glTexImage2D failed"); }
    const auto found = impl_->textures.find(texture_id);
    const Impl::Texture uploaded{texture, image.content_width, image.content_height};
    if (found != impl_->textures.end()) {
        glDeleteTextures(1, &found->second.id);
        found->second = uploaded;
    }
    else {
        try { impl_->textures.emplace(texture_id, uploaded); }
        catch (...) { glDeleteTextures(1, &texture); throw; }
    }
}

void OpenGLRenderer::begin_frame(std::uint32_t clear_color) {
    if (!impl_->context) throw std::logic_error("OpenGL renderer is not initialized");
    if (impl_->in_frame) throw std::logic_error("OpenGL frame is already active");
    impl_->make_current();
    const auto a = static_cast<float>((clear_color >> 24U) & 0xffU) / 255.0F;
    const auto r = static_cast<float>((clear_color >> 16U) & 0xffU) / 255.0F;
    const auto g = static_cast<float>((clear_color >> 8U) & 0xffU) / 255.0F;
    const auto b = static_cast<float>(clear_color & 0xffU) / 255.0F;
    glClearColor(r, g, b, a);
    glClear(GL_COLOR_BUFFER_BIT);
    glLoadIdentity();
    impl_->in_frame = true;
}

void OpenGLRenderer::draw(std::uint32_t texture_id, int x, int y, int width, int height,
    std::uint8_t opacity) {
    if (!impl_->in_frame) throw std::logic_error("begin_frame must be called before draw");
    if (width <= 0 || height <= 0 || opacity == 0) return;
    const auto found = impl_->textures.find(texture_id);
    if (found == impl_->textures.end()) throw std::out_of_range("OpenGL texture ID is not uploaded");
    glBindTexture(GL_TEXTURE_2D, found->second.id);
    glColor4ub(255, 255, 255, opacity);
    const auto right_u = static_cast<float>(found->second.content_width) / 32.0F;
    const auto bottom_v = static_cast<float>(found->second.content_height) / 32.0F;
    glBegin(GL_QUADS);
    glTexCoord2f(0.0F, 0.0F); glVertex2i(x, y);
    const auto right = static_cast<float>(x) + static_cast<float>(width);
    const auto bottom = static_cast<float>(y) + static_cast<float>(height);
    glTexCoord2f(right_u, 0.0F); glVertex2f(right, static_cast<float>(y));
    glTexCoord2f(right_u, bottom_v); glVertex2f(right, bottom);
    glTexCoord2f(0.0F, bottom_v); glVertex2f(static_cast<float>(x), bottom);
    glEnd();
}

void OpenGLRenderer::draw_clipped(std::uint32_t texture_id, int destination_x,
    int destination_y, int destination_width, int destination_height,
    int clip_x, int clip_y, int clip_width, int clip_height,
    std::uint8_t opacity) {
    if (!impl_->in_frame) throw std::logic_error("begin_frame must be called before draw");
    if (destination_width <= 0 || destination_height <= 0 ||
        clip_width <= 0 || clip_height <= 0 || opacity == 0) return;
    const auto left = std::clamp<std::int64_t>(clip_x, 0, impl_->width);
    const auto top = std::clamp<std::int64_t>(clip_y, 0, impl_->height);
    const auto right = std::clamp<std::int64_t>(
        static_cast<std::int64_t>(clip_x) + clip_width, 0, impl_->width);
    const auto bottom = std::clamp<std::int64_t>(
        static_cast<std::int64_t>(clip_y) + clip_height, 0, impl_->height);
    if (left >= right || top >= bottom) return;

    const auto was_enabled = glIsEnabled(GL_SCISSOR_TEST) == GL_TRUE;
    GLint previous_box[4]{};
    glGetIntegerv(GL_SCISSOR_BOX, previous_box);
    glEnable(GL_SCISSOR_TEST);
    glScissor(static_cast<GLint>(left),
        static_cast<GLint>(impl_->height - bottom),
        static_cast<GLsizei>(right - left), static_cast<GLsizei>(bottom - top));
    try {
        draw(texture_id, destination_x, destination_y,
            destination_width, destination_height, opacity);
    } catch (...) {
        glScissor(previous_box[0], previous_box[1], previous_box[2], previous_box[3]);
        if (!was_enabled) glDisable(GL_SCISSOR_TEST);
        throw;
    }
    glScissor(previous_box[0], previous_box[1], previous_box[2], previous_box[3]);
    if (!was_enabled) glDisable(GL_SCISSOR_TEST);
}

void OpenGLRenderer::draw_light_quads(std::span<const LightQuad> quads,
    int clip_x, int clip_y, int clip_width, int clip_height) {
    if (!impl_->in_frame) throw std::logic_error("begin_frame must be called before draw");
    if (quads.empty() || clip_width <= 0 || clip_height <= 0) return;
    const auto left = std::clamp<std::int64_t>(clip_x, 0, impl_->width);
    const auto top = std::clamp<std::int64_t>(clip_y, 0, impl_->height);
    const auto right = std::clamp<std::int64_t>(
        static_cast<std::int64_t>(clip_x) + clip_width, 0, impl_->width);
    const auto bottom = std::clamp<std::int64_t>(
        static_cast<std::int64_t>(clip_y) + clip_height, 0, impl_->height);
    if (left >= right || top >= bottom) return;
    const auto was_scissor_enabled = glIsEnabled(GL_SCISSOR_TEST) == GL_TRUE;
    const auto was_texture_enabled = glIsEnabled(GL_TEXTURE_2D) == GL_TRUE;
    const auto was_blend_enabled = glIsEnabled(GL_BLEND) == GL_TRUE;
    GLint previous_box[4]{};
    GLint previous_source_blend{};
    GLint previous_destination_blend{};
    glGetIntegerv(GL_SCISSOR_BOX, previous_box);
    glGetIntegerv(GL_BLEND_SRC, &previous_source_blend);
    glGetIntegerv(GL_BLEND_DST, &previous_destination_blend);
    glEnable(GL_SCISSOR_TEST);
    glScissor(static_cast<GLint>(left), static_cast<GLint>(impl_->height - bottom),
        static_cast<GLsizei>(right - left), static_cast<GLsizei>(bottom - top));
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_DST_COLOR, GL_ZERO);
    glBegin(GL_QUADS);
    for (const auto& quad : quads) {
        if (quad.width <= 0 || quad.height <= 0) continue;
        const auto right_edge = static_cast<float>(quad.x) + quad.width;
        const auto bottom_edge = static_cast<float>(quad.y) + quad.height;
        glColor3ub(quad.corners[0].red, quad.corners[0].green, quad.corners[0].blue);
        glVertex2i(quad.x, quad.y);
        glColor3ub(quad.corners[1].red, quad.corners[1].green, quad.corners[1].blue);
        glVertex2f(right_edge, static_cast<float>(quad.y));
        glColor3ub(quad.corners[3].red, quad.corners[3].green, quad.corners[3].blue);
        glVertex2f(right_edge, bottom_edge);
        glColor3ub(quad.corners[2].red, quad.corners[2].green, quad.corners[2].blue);
        glVertex2f(static_cast<float>(quad.x), bottom_edge);
    }
    glEnd();
    glBlendFunc(static_cast<GLenum>(previous_source_blend),
        static_cast<GLenum>(previous_destination_blend));
    if (!was_blend_enabled) glDisable(GL_BLEND);
    if (was_texture_enabled) glEnable(GL_TEXTURE_2D);
    if (!was_scissor_enabled) glDisable(GL_SCISSOR_TEST);
    else glScissor(previous_box[0], previous_box[1], previous_box[2], previous_box[3]);
}

void OpenGLRenderer::end_frame() {
    if (!impl_->context || !impl_->in_frame) throw std::logic_error("no OpenGL frame is active");
    glFlush();
    impl_->in_frame = false;
    if (!SwapBuffers(impl_->dc)) throw std::runtime_error("SwapBuffers failed");
}

} // namespace tibia::render
