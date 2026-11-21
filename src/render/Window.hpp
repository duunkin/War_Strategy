#pragma once

#if __has_include(<SDL2/SDL.h>)
#include <SDL2/SDL.h>
#else
#include <SDL.h>
#endif

#include <cstdint>
#include <string>

namespace render {

/**
 * @brief Lớp Window chịu trách nhiệm quản lý cửa sổ hiển thị và bộ dựng hình (SDL_Renderer).
 */
class Window {
public:
    Window();
    ~Window();

    // Không cho phép copy để tránh hủy hai lần con trỏ SDL
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    // Cho phép move
    Window(Window&& other) noexcept;
    Window& operator=(Window&& other) noexcept;

    // Khởi tạo cửa sổ SDL2 với tiêu đề, chiều rộng và chiều cao
    bool init(const std::string& title, int width, int height);

    // Dọn dẹp tài nguyên cửa sổ và renderer
    void cleanup();

    // Xóa màn hình với màu nền chỉ định (mặc định đen)
    void clear(uint8_t r = 0, uint8_t g = 0, uint8_t b = 0, uint8_t a = 255);

    // Đẩy buffer hình ảnh lên màn hình (hiển thị frame)
    void present();

    // Cập nhật lại kích thước hiển thị khi người dùng co giãn cửa sổ
    void handleEvent(const SDL_Event& event);

    [[nodiscard]] SDL_Window* getSDLWindow() const;
    [[nodiscard]] SDL_Renderer* getSDLRenderer() const;
    [[nodiscard]] int getWidth() const;
    [[nodiscard]] int getHeight() const;

private:
    SDL_Window* m_window{nullptr};
    SDL_Renderer* m_renderer{nullptr};
    int m_width{0};
    int m_height{0};
};

} // namespace render
