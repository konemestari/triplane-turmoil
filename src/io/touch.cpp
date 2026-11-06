/*
 * Triplane Classic - a side-scrolling dogfighting game.
 * Copyright (C) 1996,1997,2009  Dodekaedron Software Creations Oy
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * On-screen touch controls for touch-only devices.
 *
 * Layout (landscape): the game picture is letterboxed in the middle of
 * the screen. The left side bar holds climb/dive, the right side bar
 * holds engine power, bombs and guns. A small back button sits in the
 * top-left corner. Whole column halves are hit areas so that slightly
 * missed taps still count.
 */

#include "io/touch.h"

enum {
    TB_ESCAPE = 0,
    TB_UP,
    TB_DOWN,
    TB_POWER,
    TB_BOMBS,
    TB_GUNS,
    TB_COUNT
};

struct touch_rect {
    float x, y, w, h;           /* normalized 0..1 screen coordinates */
};

static touch_rect rects[TB_COUNT];
static int held[TB_COUNT];
static int prev_held[TB_COUNT];
static int power_tap = 0;
static int escape_tap = 0;
static int ingame_frames = 0;
static int layout_out_w = 0, layout_out_h = 0;
static int layout_ready = 0;

int touch_controls_enabled(void) {
#ifdef TRIPLANE_TOUCH
    return 1;
#else
    return 0;
#endif
}

static void set_rect(int idx, float x, float y, float w, float h) {
    rects[idx].x = x;
    rects[idx].y = y;
    rects[idx].w = w;
    rects[idx].h = h;
}

static void default_layout(void) {
    touch_set_layout(1000, 460, 160, 0, 680, 460);
}

void touch_set_layout(int out_w, int out_h, int vx, int vy, int vw, int vh) {
    if (out_w <= 0 || out_h <= 0)
        return;

    (void) vy;
    (void) vh;

    float left = (float) vx / out_w;
    float right = (float) (out_w - vx - vw) / out_w;

    /* Narrow side bars (e.g. iPad): let the buttons overlap the picture. */
    if (left < 0.14f)
        left = 0.16f;
    if (right < 0.14f)
        right = 0.16f;

    float rx = 1.0f - right;

    set_rect(TB_ESCAPE, 0.0f, 0.0f, left, 0.14f);
    set_rect(TB_UP, 0.0f, 0.16f, left, 0.41f);
    set_rect(TB_DOWN, 0.0f, 0.59f, left, 0.41f);

    set_rect(TB_POWER, rx, 0.0f, right, 0.24f);
    set_rect(TB_BOMBS, rx, 0.26f, right, 0.30f);
    set_rect(TB_GUNS, rx, 0.58f, right, 0.42f);

    layout_out_w = out_w;
    layout_out_h = out_h;
    layout_ready = 1;
}

static int hit(int idx, float x, float y) {
    return x >= rects[idx].x && x < rects[idx].x + rects[idx].w &&
        y >= rects[idx].y && y < rects[idx].y + rects[idx].h;
}

void touch_update(void) {
    if (!touch_controls_enabled())
        return;

    if (!layout_ready)
        default_layout();

    int now[TB_COUNT] = { 0 };

    int devices = SDL_GetNumTouchDevices();
    for (int d = 0; d < devices; d++) {
        SDL_TouchID id = SDL_GetTouchDevice(d);
        int fingers = SDL_GetNumTouchFingers(id);

        for (int f = 0; f < fingers; f++) {
            SDL_Finger *finger = SDL_GetTouchFinger(id, f);
            if (finger == NULL)
                continue;

            for (int b = 0; b < TB_COUNT; b++)
                if (hit(b, finger->x, finger->y))
                    now[b] = 1;
        }
    }

    for (int b = 0; b < TB_COUNT; b++) {
        prev_held[b] = held[b];
        held[b] = now[b];
    }

    /* Only count power taps during flight, so menu taps don't leak in. */
    if (held[TB_POWER] && !prev_held[TB_POWER] && ingame_frames > 0)
        power_tap = 1;

    if (held[TB_ESCAPE] && !prev_held[TB_ESCAPE])
        escape_tap = 1;
}

int touch_up(void) {
    return held[TB_UP];
}

int touch_down(void) {
    return held[TB_DOWN];
}

int touch_guns(void) {
    return held[TB_GUNS];
}

int touch_bombs(void) {
    return held[TB_BOMBS];
}

int touch_consume_power_toggle(void) {
    int r = power_tap;
    power_tap = 0;
    return r;
}

int touch_consume_escape(void) {
    if (!touch_controls_enabled())
        return 0;

    SDL_PumpEvents();
    touch_update();

    int r = escape_tap;
    escape_tap = 0;
    return r;
}

void touch_mark_ingame(void) {
    ingame_frames = 2;
}

static SDL_Rect to_pixels(int idx) {
    SDL_Rect r;
    r.x = (int) (rects[idx].x * layout_out_w);
    r.y = (int) (rects[idx].y * layout_out_h);
    r.w = (int) (rects[idx].w * layout_out_w);
    r.h = (int) (rects[idx].h * layout_out_h);

    /* Small inset so neighbouring buttons look separate. */
    int inset = layout_out_h / 80 + 2;
    r.x += inset;
    r.y += inset;
    r.w -= 2 * inset;
    r.h -= 2 * inset;
    return r;
}

static void draw_box(SDL_Renderer *renderer, int idx, int active) {
    SDL_Rect r = to_pixels(idx);

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, active ? 90 : 35);
    SDL_RenderFillRect(renderer, &r);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 140);
    SDL_RenderDrawRect(renderer, &r);
}

static void draw_triangle(SDL_Renderer *renderer, int idx, int pointing_up) {
    SDL_Rect r = to_pixels(idx);
    int cx = r.x + r.w / 2;
    int cy = r.y + r.h / 2;
    int s = (r.w < r.h ? r.w : r.h) / 4;
    int tip = pointing_up ? cy - s : cy + s;
    int base = pointing_up ? cy + s : cy - s;

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 200);
    for (int t = -1; t <= 1; t++) {
        SDL_RenderDrawLine(renderer, cx + t, tip, cx - s + t, base);
        SDL_RenderDrawLine(renderer, cx + t, tip, cx + s + t, base);
        SDL_RenderDrawLine(renderer, cx - s, base + t, cx + s, base + t);
    }
}

static void draw_cross(SDL_Renderer *renderer, int idx, int diagonal) {
    SDL_Rect r = to_pixels(idx);
    int cx = r.x + r.w / 2;
    int cy = r.y + r.h / 2;
    int s = (r.w < r.h ? r.w : r.h) / 4;

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 200);
    for (int t = -1; t <= 1; t++) {
        if (diagonal) {
            SDL_RenderDrawLine(renderer, cx - s + t, cy - s, cx + s + t, cy + s);
            SDL_RenderDrawLine(renderer, cx + s + t, cy - s, cx - s + t, cy + s);
        }
        else {
            SDL_RenderDrawLine(renderer, cx - s, cy + t, cx + s, cy + t);
            SDL_RenderDrawLine(renderer, cx + t, cy - s, cx + t, cy + s);
        }
    }
}

static void draw_square(SDL_Renderer *renderer, int idx, int filled) {
    SDL_Rect r = to_pixels(idx);
    int s = (r.w < r.h ? r.w : r.h) / 5;
    SDL_Rect q = { r.x + r.w / 2 - s, r.y + r.h / 2 - s, 2 * s, 2 * s };

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 200);
    if (filled)
        SDL_RenderFillRect(renderer, &q);
    else
        SDL_RenderDrawRect(renderer, &q);
}

void touch_draw(SDL_Renderer *renderer) {
    if (!touch_controls_enabled() || !layout_ready)
        return;

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    /* Back button is always available (menus need it too). */
    draw_box(renderer, TB_ESCAPE, held[TB_ESCAPE]);
    draw_cross(renderer, TB_ESCAPE, 1);

    if (ingame_frames > 0) {
        ingame_frames--;

        draw_box(renderer, TB_UP, held[TB_UP]);
        draw_triangle(renderer, TB_UP, 1);

        draw_box(renderer, TB_DOWN, held[TB_DOWN]);
        draw_triangle(renderer, TB_DOWN, 0);

        draw_box(renderer, TB_POWER, held[TB_POWER]);
        draw_square(renderer, TB_POWER, 0);

        draw_box(renderer, TB_BOMBS, held[TB_BOMBS]);
        draw_square(renderer, TB_BOMBS, 1);

        draw_box(renderer, TB_GUNS, held[TB_GUNS]);
        draw_cross(renderer, TB_GUNS, 0);
    }

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}
