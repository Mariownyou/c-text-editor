#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include <SDL.h>

#include "./la.h"

#define STB_IMAGE_IMPLEMENTATION
#include "./stb_image.h"

#include "./buffer.h"

#define FONT_WIDTH 128
#define FONT_HEIGHT 64
#define FONT_COLS 18
#define FONT_ROWS 7
#define FONT_CHAR_WIDTH (FONT_WIDTH / FONT_COLS)
#define FONT_CHAR_HEIGHT (FONT_HEIGHT / FONT_ROWS)
#define FONT_SCALE 2.5f

void scc(int code)
{
    if (code < 0)
    {
        fprintf(stderr, "SDL ERROR: %s\n", SDL_GetError());
        exit(1);
    }
}

void *scp(void *ptr)
{
    if (ptr == NULL)
    {
        fprintf(stderr, "SDL ERROR: %s\n", SDL_GetError());
        exit(1);
    }

    return ptr;
}

SDL_Surface *surface_from_file(const char *file_path)
{
    int width, height, n;
    unsigned char *pixels = stbi_load(file_path, &width, &height, &n, STBI_rgb_alpha);
    if (pixels == NULL) {
        fprintf(stderr, "ERROR: Could not load file %s: %s\n",
                file_path, stbi_failure_reason());
        exit(1);
    }

    Uint32 rmask, gmask, bmask, amask;
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
    rmask = 0xff000000;
    gmask = 0x00ff0000;
    bmask = 0x0000ff00;
    amask = 0x000000ff;
#else
    rmask = 0x000000ff;
    gmask = 0x0000ff00;
    bmask = 0x00ff0000;
    amask = 0xff000000;
#endif

    int depth = 32;
    int pitch = 4*width;

    return scp(SDL_CreateRGBSurfaceFrom(
        (void*)pixels, width, height, depth, pitch,
        rmask, gmask, bmask, amask));
}

#define ASCII_DISPLAY_LOW 32
#define ASCII_DISPLAY_HIGH 126

typedef struct {
    SDL_Texture *sprite;
    SDL_Rect glyph_table[ASCII_DISPLAY_HIGH - ASCII_DISPLAY_LOW + 1];
} Font;

Font font_load_from_file(SDL_Renderer *renderer, const char *file_path)
{
    Font font = {0};
    SDL_Surface *font_surface = surface_from_file(file_path);
    SDL_SetColorKey(font_surface, SDL_TRUE, 0xFF000000);
    font.sprite = scp(SDL_CreateTextureFromSurface(renderer, font_surface));
    SDL_FreeSurface(font_surface);

    for (size_t ascii = ASCII_DISPLAY_LOW; ascii <= ASCII_DISPLAY_HIGH; ++ascii) {
        const size_t index = ascii - ASCII_DISPLAY_LOW;
        const size_t col = index % FONT_COLS;
        const size_t row = index / FONT_COLS;

        font.glyph_table[index] = (SDL_Rect) {
            .x = col * FONT_CHAR_WIDTH,
            .y = row * FONT_CHAR_HEIGHT,
            .w = FONT_CHAR_WIDTH,
            .h = FONT_CHAR_HEIGHT
        };
    }

    return font;
}

void render_char(SDL_Renderer *renderer, const Font *font, char c, Vec2f pos, float scale)
{
    SDL_Rect dst = {
        .x = (int) floorf(pos.x),
        .y = (int) floorf(pos.y),
        .w = (int) floorf(FONT_CHAR_WIDTH * scale),
        .h = (int) floorf(FONT_CHAR_HEIGHT * scale)
    };

    size_t index = '?' - ASCII_DISPLAY_LOW;
    if (ASCII_DISPLAY_LOW <= c && c <= ASCII_DISPLAY_HIGH) {
        index = c - ASCII_DISPLAY_LOW;
    }

    scc(SDL_RenderCopy(renderer, font->sprite, &font->glyph_table[index], &dst));
}

void set_texture_color(SDL_Texture *texture, Uint32 color)
{
    scc(SDL_SetTextureColorMod(
        texture,
        (color >> (8 * 0)) & 0xff,
        (color >> (8 * 1)) & 0xff,
        (color >> (8 * 2)) & 0xff));

    scc(SDL_SetTextureAlphaMod(texture, (color >> (8 * 3)) & 0xff));
}

void render_text_sized(SDL_Renderer *renderer, const Font *font, const char *text, size_t text_size, Vec2f pos, Uint32 color, float scale)
{
    set_texture_color(font->sprite, color);

    Vec2f pen = pos;
    for (size_t i = 0; i < text_size; ++i) {
        render_char(renderer, font, text[i], pen, scale);
        pen.x += FONT_CHAR_WIDTH * scale;
    }
}

Line line = {0};
size_t cursor = 0;

#define UNHEX(color) \
    ((color) >> (8 * 0)) & 0xFF, \
    ((color) >> (8 * 1)) & 0xFF, \
    ((color) >> (8 * 2)) & 0xFF, \
    ((color) >> (8 * 3)) & 0xFF

void render_cursor(SDL_Renderer *renderer, const Font *font)
{
    const Vec2f pos = vec2f(cursor * FONT_SCALE * FONT_CHAR_WIDTH, 0.0f);

    const SDL_Rect rect = {
        .x = (int) floorf(pos.x),
        .y = (int) floorf(pos.y),
        .w = FONT_CHAR_WIDTH * FONT_SCALE,
        .h = FONT_CHAR_HEIGHT * FONT_SCALE
    };

    scc(SDL_SetRenderDrawColor(renderer, UNHEX(0xFFFFFFFF)));
    scc(SDL_RenderFillRect(renderer, &rect));

    set_texture_color(font->sprite, 0xFF000000);
    if (cursor < line.size) {
        render_char(renderer, font, line.chars[cursor], pos, FONT_SCALE);
    }
}

int main(int argc, char **argv)
{
    scc(SDL_Init(SDL_INIT_VIDEO));

    SDL_Window *window = scp(SDL_CreateWindow("Text Editor", 0, 0, 800, 600, SDL_WINDOW_RESIZABLE));
    SDL_Renderer *renderer = scp(SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED));

    Font font = font_load_from_file(renderer, "./charmap-oldschool_white.png");

    bool quit = false;
    while (!quit) {
        SDL_Event event = {0};
        while (SDL_PollEvent(&event)) {
            switch (event.type){
            case SDL_QUIT: {
                quit = true;
            } break;

            case SDL_KEYDOWN: {
                switch (event.key.keysym.sym) {
                case SDLK_BACKSPACE: {
                    line_backspace(&line, cursor);
                    if (cursor > 0) {
                        cursor -= 1;
                    }
                } break;

                case SDLK_LEFT: {
                    if (cursor > 0) {
                        cursor -= 1;
                    }
                } break;

                case SDLK_RIGHT: {
                    if (cursor < line.size) {
                        cursor += 1;
                    }
                } break;
                }
            } break;

            case SDL_TEXTINPUT: {
                line_insert_text_after(&line, event.text.text, cursor);
                cursor += strlen(event.text.text);
            } break;
            }
        }

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
        SDL_RenderClear(renderer);

        render_text_sized(renderer, &font, line.chars, line.size, vec2f(0.0, 0.0), 0xffffffff, FONT_SCALE);
        render_cursor(renderer, &font);

        SDL_RenderPresent(renderer);
    }

    SDL_Quit();

    return 0;
}
