#include <SDL.h>
#include <iostream>
#include <memory>

using namespace std;

struct SDLDeleter {
    void operator()(SDL_Window* w) const { if (w) SDL_DestroyWindow(w); }
    void operator()(SDL_Renderer* r) const { if (r) SDL_DestroyRenderer(r); }
};

template <typename T>
using SDLPtr = unique_ptr<T, SDLDeleter>;

int main(int argc, char* argv[]) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        cerr << "SDL_Init loi: " << SDL_GetError() << '\n';
        return 1;
    }

    SDL_Rect usableBounds{};
    if (SDL_GetDisplayUsableBounds(0, &usableBounds) != 0) {
        usableBounds.w = 1280;
        usableBounds.h = 720;
    }

    int topBorder = 0;
    int leftBorder = 0;
    int bottomBorder = 0;
    int rightBorder = 0;

    SDLPtr<SDL_Window> window(SDL_CreateWindow(
        "War Strategy",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        usableBounds.w, usableBounds.h,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    ));

    if (!window) {
        cerr << "Khoi tao cua so that bai: " << SDL_GetError() << '\n';
        SDL_Quit();
        return 1;
    }

    if (SDL_GetWindowBordersSize(window.get(), &topBorder, &leftBorder, &bottomBorder, &rightBorder) == 0) {
        int fitWidth = usableBounds.w - leftBorder - rightBorder;
        int fitHeight = usableBounds.h - topBorder - bottomBorder;
        SDL_SetWindowSize(window.get(), fitWidth, fitHeight);
        SDL_SetWindowPosition(window.get(), usableBounds.x + leftBorder, usableBounds.y + topBorder);
    }

    SDLPtr<SDL_Renderer> renderer(SDL_CreateRenderer(
        window.get(), -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    ));

    if (!renderer) {
        cerr << "Khoi tao renderer that bai: " << SDL_GetError() << '\n';
        SDL_Quit();
        return 1;
    }

    bool running = true;
    SDL_Event event;

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            }
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
                running = false;
            }
        }

        int winWidth = 0;
        int winHeight = 0;
        SDL_GetRendererOutputSize(renderer.get(), &winWidth, &winHeight);

        int mapHeight = winHeight - 60;
        int mapWidth = (mapHeight * 128) / 96;
        if (mapWidth > winWidth - 60) {
            mapWidth = winWidth - 60;
            mapHeight = (mapWidth * 96) / 128;
        }

        SDL_Rect battlefieldRect{
            .x = (winWidth - mapWidth) / 2,
            .y = (winHeight - mapHeight) / 2,
            .w = mapWidth,
            .h = mapHeight
        };

        SDL_SetRenderDrawColor(renderer.get(), 255, 255, 255, 255);
        SDL_RenderClear(renderer.get());

        SDL_SetRenderDrawColor(renderer.get(), 0, 0, 0, 255);
        SDL_RenderFillRect(renderer.get(), &battlefieldRect);

        SDL_RenderPresent(renderer.get());
    }

    renderer.reset();
    window.reset();

    SDL_Quit();
    return 0;
}