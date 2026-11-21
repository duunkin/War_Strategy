#include "TextRenderer.hpp"
#include <iostream>

namespace render {

TextRenderer::TextRenderer() = default;

TextRenderer::~TextRenderer() {
    cleanup();
}

bool TextRenderer::init(const std::vector<std::string>& candidateFontPaths, int fontSize) {
    if (TTF_Init() != 0) {
        std::cerr << "[TextRenderer] TTF_Init loi: " << TTF_GetError() << std::endl;
        return false;
    }
    m_initialized = true;

    // Thử lần lượt các đường dẫn font ứng viên
    for (const auto& path : candidateFontPaths) {
        m_font = TTF_OpenFont(path.c_str(), fontSize);
        if (m_font) {
            std::cout << "[TextRenderer] Nap font thanh cong tu: " << path << std::endl;
            return true;
        }
    }

    std::cerr << "[TextRenderer] Canh bao: Khong tim thay file font TTF nao trong cac thu muc ung vien." << std::endl;
    return false;
}

void TextRenderer::cleanup() {
    if (m_font) {
        TTF_CloseFont(m_font);
        m_font = nullptr;
    }
    if (m_initialized) {
        TTF_Quit();
        m_initialized = false;
    }
}

void TextRenderer::renderText(SDL_Renderer* renderer, const std::string& text, int x, int y, SDL_Color color, bool drawBackground) {
    if (!renderer || !m_font || text.empty()) {
        return;
    }

    // Tạo surface từ chuỗi ký tự theo bảng mã UTF-8
    SDL_Surface* surface = TTF_RenderUTF8_Blended(m_font, text.c_str(), color);
    if (!surface) {
        return;
    }

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (!texture) {
        SDL_FreeSurface(surface);
        return;
    }

    SDL_Rect dstRect{
        .x = x,
        .y = y,
        .w = surface->w,
        .h = surface->h
    };

    // Vẽ hộp nền mờ đen phía sau chữ để dễ nhìn trên mọi nền bản đồ
    if (drawBackground) {
        SDL_Rect bgRect{
            .x = x - 4,
            .y = y - 2,
            .w = surface->w + 8,
            .h = surface->h + 4
        };
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 180);
        SDL_RenderFillRect(renderer, &bgRect);
    }

    SDL_RenderCopy(renderer, texture, nullptr, &dstRect);

    SDL_DestroyTexture(texture);
    SDL_FreeSurface(surface);
}

bool TextRenderer::isLoaded() const {
    return (m_font != nullptr);
}

} // namespace render
