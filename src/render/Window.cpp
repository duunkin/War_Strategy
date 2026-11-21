#include "Window.hpp"
#include <iostream>

namespace render {

Window::Window() = default;

Window::~Window() {
    cleanup();
}

Window::Window(Window&& other) noexcept
    : m_window(other.m_window)
    , m_renderer(other.m_renderer)
    , m_width(other.m_width)
    , m_height(other.m_height)
{
    other.m_window = nullptr;
    other.m_renderer = nullptr;
    other.m_width = 0;
    other.m_height = 0;
}

Window& Window::operator=(Window&& other) noexcept {
    if (this != &other) {
        cleanup();
        m_window = other.m_window;
        m_renderer = other.m_renderer;
        m_width = other.m_width;
        m_height = other.m_height;
        other.m_window = nullptr;
        other.m_renderer = nullptr;
        other.m_width = 0;
        other.m_height = 0;
    }
    return *this;
}

bool Window::init(const std::string& title, int width, int height) {
    m_width = width;
    m_height = height;

    // Tạo cửa sổ SDL với cờ cho phép đổi kích thước (SDL_WINDOW_RESIZABLE)
    m_window = SDL_CreateWindow(
        title.c_str(),
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        m_width,
        m_height,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );

    if (!m_window) {
        std::cerr << "[Window] Khong the tao cua so SDL: " << SDL_GetError() << std::endl;
        return false;
    }

    // Tạo bộ vẽ tăng tốc phần cứng và bật đồng bộ khung hình VSync
    m_renderer = SDL_CreateRenderer(
        m_window,
        -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );

    if (!m_renderer) {
        std::cerr << "[Window] Khong the tao SDL_Renderer: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(m_window);
        m_window = nullptr;
        return false;
    }

    // Đảm bảo kích thước nội bộ khớp với kích thước thực của viewport renderer
    SDL_GetRendererOutputSize(m_renderer, &m_width, &m_height);

    return true;
}

void Window::cleanup() {
    if (m_renderer) {
        SDL_DestroyRenderer(m_renderer);
        m_renderer = nullptr;
    }
    if (m_window) {
        SDL_DestroyWindow(m_window);
        m_window = nullptr;
    }
}

void Window::clear(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    if (m_renderer) {
        SDL_SetRenderDrawColor(m_renderer, r, g, b, a);
        SDL_RenderClear(m_renderer);
    }
}

void Window::present() {
    if (m_renderer) {
        SDL_RenderPresent(m_renderer);
    }
}

void Window::handleEvent(const SDL_Event& event) {
    if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
        if (m_renderer) {
            SDL_GetRendererOutputSize(m_renderer, &m_width, &m_height);
        }
    }
}

SDL_Window* Window::getSDLWindow() const {
    return m_window;
}

SDL_Renderer* Window::getSDLRenderer() const {
    return m_renderer;
}

int Window::getWidth() const {
    return m_width;
}

int Window::getHeight() const {
    return m_height;
}

} // namespace render
