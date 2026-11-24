#include <assert.h>
#include <stdio.h>

#include "pixel_scale.h"

static unsigned grey(const SDL_Surface *image, int x, int y)
{
    const uint32_t *row = (const uint32_t *)((const uint8_t *)image->pixels + y * image->pitch);
    const uint32_t pixel = row[x];
    assert((pixel >> 16 & 255) == (pixel >> 8 & 255) && (pixel >> 8 & 255) == (pixel & 255));
    return pixel & 255;
}

int main(void)
{
    assert(SDL_Init(SDL_INIT_VIDEO));
    SDL_Window *window = SDL_CreateWindow("Xenon 2 pixel scaling test", 12, 9, SDL_WINDOW_HIDDEN | SDL_WINDOW_RESIZABLE);
    assert(window);
    SDL_Renderer *renderer = SDL_CreateRenderer(window, "software");
    assert(renderer);
    SDL_Texture *source = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, 4, 3);
    assert(source);
    uint32_t pixels[4 * 3];
    for (int y = 0; y < 3; y++)
        for (int x = 0; x < 4; x++)
            pixels[y * 4 + x] = x & 1 ? 0xffffffffu : 0xff000000u;
    assert(SDL_UpdateTexture(source, NULL, pixels, 4 * 4));
    assert(SDL_SetRenderLogicalPresentation(renderer, 4, 3, SDL_LOGICAL_PRESENTATION_LETTERBOX));
    PixelScale scale;
    assert(pixel_scale_open(&scale, renderer, source, 4, 3));

    const int sizes[][2] = {{12, 9}, {10, 8}, {24, 18}, {1, 1}};
    for (unsigned i = 0; i < sizeof sizes / sizeof sizes[0]; i++) {
        assert(SDL_SetWindowSize(window, sizes[i][0], sizes[i][1]));
        SDL_PumpEvents();
        assert(pixel_scale_draw(&scale, source));
        assert(SDL_SetRenderLogicalPresentation(renderer, 0, 0, SDL_LOGICAL_PRESENTATION_DISABLED));
        SDL_Surface *read = SDL_RenderReadPixels(renderer, NULL);
        assert(read);
        SDL_Surface *image = SDL_ConvertSurface(read, SDL_PIXELFORMAT_ARGB8888);
        assert(image && image->w == sizes[i][0] && image->h == sizes[i][1]);
        if (i == 0 || i == 2) {
            int repeat = image->w / 4;
            for (int y = 0; y < image->h; y++)
                for (int x = 0; x < image->w; x++)
                    assert(grey(image, x, y) == ((x / repeat) & 1 ? 255u : 0u));
        } else if (i == 1) {
            assert(grey(image, 0, 4) == 0 && grey(image, 4, 4) > 225);
            assert(grey(image, 5, 4) < 30 && grey(image, 9, 4) == 255);
            unsigned edge = grey(image, 2, 4);
            assert(edge > 90 && edge < 165);
        }
        SDL_DestroySurface(image);
        SDL_DestroySurface(read);
        assert(SDL_RenderPresent(renderer));
        assert(SDL_SetRenderLogicalPresentation(renderer, 4, 3, SDL_LOGICAL_PRESENTATION_LETTERBOX));
    }
    pixel_scale_close(&scale);
    SDL_DestroyTexture(source);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    puts("Integer pixel copies, fractional boundaries and resize passed.");
    return 0;
}
