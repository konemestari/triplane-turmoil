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
 * Flight layout (landscape):
 *  - Left half of the screen: floating vertical stick. Put a finger down
 *    anywhere and slide up or down to climb or dive. Sideways movement is
 *    ignored. The anchor follows the finger if it travels far, so changing
 *    direction only needs a short slide.
 *  - Right side bar: engine power (tap to toggle), bombs and guns.
 *  - Top-left corner: back button (also works in menus).
 *
 * Menus: the touch point snaps onto the nearest menu button if the finger
 * lands slightly outside it (see touch_snap_to_button).
 */

#include "io/touch.h"

enum {
    TB_ESCAPE = 0,
    TB_POWER,
    TB_BOMBS,
    TB_GUNS,
    TB_COUNT
};

struct touch_rect {
    float x, y, w, h;           /* normalized 0..1 screen coordinates */
};

static touch_rect rects[TB_COUNT];
static touch_rect stick_hint;
static int held[TB_COUNT];
static int prev_held[TB_COUNT];
static int power_tap = 0;
static int escape_tap = 0;
static int ingame_frames = 0;
static int layout_out_w = 0, layout_out_h = 0;
static int layout_ready = 0;
static int layout_vx = 0, layout_vy = 0, layout_vw = 0, layout_vh = 0;
static int power_indicator = 0;

/* Floating vertical stick (fractions of screen height). */
#define STICK_DEADZONE 0.025f
#define STICK_TRAVEL   0.06f

static int stick_active = 0;
static SDL_FingerID stick_finger = 0;
static float stick_anchor_x = 0, stick_anchor_y = 0, stick_y = 0;
static int stick_dir = 0;       /* -1 climb, 0 neutral, 1 dive */

int touch_controls_enabled(void) {
#ifdef TRIPLANE_TOUCH
    return 1;
#else
    return 0;
#endif
}

static void set_rect(touch_rect *r, float x, float y, float w, float h) {
    r->x = x;
    r->y = y;
    r->w = w;
    r->h = h;
}

static void default_layout(void) {
    touch_set_layout(1000, 460, 130, 0, 740, 460);
}

void touch_set_layout(int out_w, int out_h, int vx, int vy, int vw, int vh) {
    if (out_w <= 0 || out_h <= 0)
        return;

    layout_vx = vx;
    layout_vy = vy;
    layout_vw = vw;
    layout_vh = vh;

    float left = (float) vx / out_w;
    float right = (float) (out_w - vx - vw) / out_w;

    /* Very narrow side bars (e.g. iPad): let the buttons overlap the picture. */
    if (left < 0.10f)
        left = 0.14f;
    if (right < 0.10f)
        right = 0.14f;

    float rx = 1.0f - right;

    set_rect(&rects[TB_ESCAPE], 0.0f, 0.0f, left, 0.16f);
    set_rect(&stick_hint, 0.0f, 0.20f, left, 0.80f);

    set_rect(&rects[TB_POWER], rx, 0.0f, right, 0.24f);
    set_rect(&rects[TB_BOMBS], rx, 0.26f, right, 0.30f);
    set_rect(&rects[TB_GUNS], rx, 0.58f, right, 0.42f);

    layout_out_w = out_w;
    layout_out_h = out_h;
    layout_ready = 1;
}

static int hit(int idx, float x, float y) {
    return x >= rects[idx].x && x < rects[idx].x + rects[idx].w &&
        y >= rects[idx].y && y < rects[idx].y + rects[idx].h;
}

static void stick_move(float y) {
    stick_y = y;

    /* Drag the anchor along so reversing direction is quick. */
    if (stick_y - stick_anchor_y > STICK_TRAVEL)
        stick_anchor_y = stick_y - STICK_TRAVEL;
    if (stick_anchor_y - stick_y > STICK_TRAVEL)
        stick_anchor_y = stick_y + STICK_TRAVEL;

    float dy = stick_y - stick_anchor_y;
    if (dy < -STICK_DEADZONE)
        stick_dir = -1;
    else if (dy > STICK_DEADZONE)
        stick_dir = 1;
    else
        stick_dir = 0;
}

void touch_update(void) {
    if (!touch_controls_enabled())
        return;

    if (!layout_ready)
        default_layout();

    int now[TB_COUNT] = { 0 };
    int stick_seen = 0;

    int devices = SDL_GetNumTouchDevices();
    for (int d = 0; d < devices; d++) {
        SDL_TouchID id = SDL_GetTouchDevice(d);
        int fingers = SDL_GetNumTouchFingers(id);

        for (int f = 0; f < fingers; f++) {
            SDL_Finger *finger = SDL_GetTouchFinger(id, f);
            if (finger == NULL)
                continue;

            if (stick_active && finger->id == stick_finger) {
                stick_seen = 1;
                stick_move(finger->y);
                continue;
            }

            /* New finger on the left half (not on the back button) grabs the stick. */
            if (!stick_active && finger->x < 0.5f && !hit(TB_ESCAPE, finger->x, finger->y)) {
                stick_active = 1;
                stick_seen = 1;
                stick_finger = finger->id;
                stick_anchor_x = finger->x;
                stick_anchor_y = finger->y;
                stick_move(finger->y);
                continue;
            }

            for (int b = 0; b < TB_COUNT; b++)
                if (hit(b, finger->x, finger->y))
                    now[b] = 1;
        }
    }

    if (!stick_seen) {
        stick_active = 0;
        stick_dir = 0;
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
    return stick_active && stick_dir < 0;
}

int touch_down(void) {
    return stick_active && stick_dir > 0;
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

void touch_set_power_indicator(int on) {
    power_indicator = on;
}

/* ------------------------------------------------------------------------ */
/* Menu button snapping                                                      */
/* ------------------------------------------------------------------------ */

#define MAX_HIT_RECTS 256
#define SNAP_DISTANCE 20        /* game pixels (screen is 320x200) */

static int hit_cur[MAX_HIT_RECTS][4];
static int hit_cur_count = 0;
static int hit_prev[MAX_HIT_RECTS][4];
static int hit_prev_count = 0;
static int hit_empty_frames = 0;

void touch_hit_register(int x1, int x2, int y1, int y2) {
    for (int i = 0; i < hit_cur_count; i++)
        if (hit_cur[i][0] == x1 && hit_cur[i][1] == x2 && hit_cur[i][2] == y1 && hit_cur[i][3] == y2)
            return;

    if (hit_cur_count >= MAX_HIT_RECTS)
        return;

    hit_cur[hit_cur_count][0] = x1;
    hit_cur[hit_cur_count][1] = x2;
    hit_cur[hit_cur_count][2] = y1;
    hit_cur[hit_cur_count][3] = y2;
    hit_cur_count++;
}

int touch_hit(int x, int y, int x1, int x2, int y1, int y2) {
    touch_hit_register(x1, x2, y1, y2);
    return x >= x1 && x <= x2 && y >= y1 && y <= y2;
}

void touch_hit_arrow_row(int y1, int y2) {
    touch_hit_register(214, 221, y1, y2);
    touch_hit_register(224, 231, y1, y2);
}

void touch_hit_new_frame(void) {
    if (hit_cur_count > 0) {
        for (int i = 0; i < hit_cur_count; i++)
            for (int k = 0; k < 4; k++)
                hit_prev[i][k] = hit_cur[i][k];
        hit_prev_count = hit_cur_count;
        hit_empty_frames = 0;
    }
    else if (++hit_empty_frames > 3) {
        /* Screen without registered buttons: stop snapping. */
        hit_prev_count = 0;
    }

    hit_cur_count = 0;
}

static int clampi(int v, int lo, int hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

void touch_snap_to_button(int *x, int *y) {
    if (hit_prev_count == 0)
        return;

    int best = -1;
    int best_dist = SNAP_DISTANCE * SNAP_DISTANCE + 1;

    for (int i = 0; i < hit_prev_count; i++) {
        int *r = hit_prev[i];

        /* Already on a button: keep the exact position (sliders need it). */
        if (*x >= r[0] && *x <= r[1] && *y >= r[2] && *y <= r[3])
            return;

        int dx = *x - clampi(*x, r[0], r[1]);
        int dy = *y - clampi(*y, r[2], r[3]);
        int dist = dx * dx + dy * dy;

        if (dist < best_dist) {
            best_dist = dist;
            best = i;
        }
    }

    if (best < 0)
        return;

    /* Move onto the nearest button, slightly inside its edge. */
    int *r = hit_prev[best];
    int mx = (r[1] - r[0] >= 4) ? 2 : 0;
    int my = (r[3] - r[2] >= 4) ? 2 : 0;
    *x = clampi(*x, r[0] + mx, r[1] - mx);
    *y = clampi(*y, r[2] + my, r[3] - my);
}

/* ------------------------------------------------------------------------ */
/* Drawing                                                                   */
/* ------------------------------------------------------------------------ */

/* Palette: deep navy backdrop, dark translucent buttons, warm cream icons
   and an amber highlight for pressed/active state. */
#define BG_R 16
#define BG_G 20
#define BG_B 32

static const SDL_Color COL_BUTTON = { 24, 30, 46, 190 };
static const SDL_Color COL_BUTTON_ON = { 58, 52, 40, 215 };
static const SDL_Color COL_ICON = { 236, 224, 196, 255 };
static const SDL_Color COL_ACTIVE = { 255, 184, 72, 255 };

enum {
    ICON_LEVER = 0,
    ICON_BOMB,
    ICON_GUNS,
    ICON_BACK,
    ICON_CHEVRON,
    ICON_COUNT
};

/* 16x16 pixel-art icons: '#' solid, '+' half transparent, '.' empty. */
static const char *icon_art[ICON_COUNT][16] = {
    {                           /* engine lever (off = tilted back) */
        ".##.............",
        "####............",
        "####............",
        ".###............",
        "...##...........",
        "....##..........",
        ".....##.........",
        "......##........",
        ".......##.......",
        "........##......",
        ".....######.....",
        "....########....",
        "...##########...",
        "..############..",
        "..############..",
        "................",
    },
    {                           /* aerial bomb, tail fins up */
        "..##........##..",
        "..###......###..",
        "..####....####..",
        "...####..####...",
        "....########....",
        "......####......",
        ".....######.....",
        "....########....",
        "...##########...",
        "...##########...",
        "...#.########...",
        "...#.########...",
        "...##########...",
        "....########....",
        ".....######.....",
        "......####......",
    },
    {                           /* crosshair */
        ".......##.......",
        ".......##.......",
        ".....######.....",
        "....##.##.##....",
        "...##......##...",
        "..##........##..",
        "..#..........#..",
        "#####..##..#####",
        "#####..##..#####",
        "..#..........#..",
        "..##........##..",
        "...##......##...",
        "....##.##.##....",
        ".....######.....",
        ".......##.......",
        ".......##.......",
    },
    {                           /* back (X) */
        "................",
        ".##..........##.",
        ".###........###.",
        "..###......###..",
        "...###....###...",
        "....###..###....",
        ".....######.....",
        "......####......",
        "......####......",
        ".....######.....",
        "....###..###....",
        "...###....###...",
        "..###......###..",
        ".###........###.",
        ".##..........##.",
        "................",
    },
    {                           /* chevron up */
        "................",
        "................",
        "................",
        "................",
        ".......##.......",
        "......####......",
        ".....######.....",
        "....###..###....",
        "...###....###...",
        "..###......###..",
        ".###........###.",
        ".##..........##.",
        "................",
        "................",
        "................",
        "................",
    },
};

static SDL_Renderer *tex_renderer = NULL;
static SDL_Texture *icon_tex[ICON_COUNT];
static SDL_Texture *circle_tex = NULL;
static SDL_Texture *ambient_tex = NULL;

#define CIRCLE_SIZE 32
#define AMBIENT_ROWS 50

static SDL_Texture *make_texture(SDL_Renderer *renderer, int w, int h, const Uint32 *pixels, int linear) {
    SDL_Texture *t = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, w, h);
    if (t == NULL)
        return NULL;
    SDL_UpdateTexture(t, NULL, pixels, w * sizeof(Uint32));
    SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(t, linear ? SDL_ScaleModeLinear : SDL_ScaleModeNearest);
    return t;
}

static void ensure_textures(SDL_Renderer *renderer) {
    if (tex_renderer == renderer)
        return;

    tex_renderer = renderer;

    Uint32 px[CIRCLE_SIZE * CIRCLE_SIZE];

    for (int i = 0; i < ICON_COUNT; i++) {
        for (int y = 0; y < 16; y++)
            for (int x = 0; x < 16; x++) {
                char c = icon_art[i][y][x];
                Uint32 a = (c == '#') ? 255 : (c == '+' ? 128 : 0);
                px[y * 16 + x] = (a << 24) | 0xFFFFFF;
            }
        icon_tex[i] = make_texture(renderer, 16, 16, px, 0);
    }

    /* Anti-aliased filled circle used for rounded button corners. */
    float rad = CIRCLE_SIZE / 2.0f;
    for (int y = 0; y < CIRCLE_SIZE; y++)
        for (int x = 0; x < CIRCLE_SIZE; x++) {
            float dx = x + 0.5f - rad, dy = y + 0.5f - rad;
            float d = SDL_sqrtf(dx * dx + dy * dy);
            float a = rad - d + 0.5f;
            a = a < 0 ? 0 : (a > 1 ? 1 : a);
            px[y * CIRCLE_SIZE + x] = ((Uint32) (a * 255) << 24) | 0xFFFFFF;
        }
    circle_tex = make_texture(renderer, CIRCLE_SIZE, CIRCLE_SIZE, px, 1);

    ambient_tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, 2, AMBIENT_ROWS);
    if (ambient_tex) {
        SDL_SetTextureBlendMode(ambient_tex, SDL_BLENDMODE_BLEND);
        SDL_SetTextureScaleMode(ambient_tex, SDL_ScaleModeLinear);
    }
}

static void set_tex_color(SDL_Texture *t, SDL_Color c) {
    SDL_SetTextureColorMod(t, c.r, c.g, c.b);
    SDL_SetTextureAlphaMod(t, c.a);
}

static void fill_round_rect(SDL_Renderer *renderer, const SDL_Rect &r, int radius, SDL_Color c) {
    if (radius * 2 > r.w)
        radius = r.w / 2;
    if (radius * 2 > r.h)
        radius = r.h / 2;

    SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, c.a);

    SDL_Rect mid = { r.x + radius, r.y, r.w - 2 * radius, r.h };
    SDL_RenderFillRect(renderer, &mid);
    SDL_Rect left = { r.x, r.y + radius, radius, r.h - 2 * radius };
    SDL_RenderFillRect(renderer, &left);
    SDL_Rect right = { r.x + r.w - radius, r.y + radius, radius, r.h - 2 * radius };
    SDL_RenderFillRect(renderer, &right);

    if (circle_tex == NULL || radius <= 0)
        return;

    set_tex_color(circle_tex, c);
    int h = CIRCLE_SIZE / 2;
    SDL_Rect src[4] = { { 0, 0, h, h }, { h, 0, h, h }, { 0, h, h, h }, { h, h, h, h } };
    SDL_Rect dst[4] = {
        { r.x, r.y, radius, radius },
        { r.x + r.w - radius, r.y, radius, radius },
        { r.x, r.y + r.h - radius, radius, radius },
        { r.x + r.w - radius, r.y + r.h - radius, radius, radius },
    };
    for (int i = 0; i < 4; i++)
        SDL_RenderCopy(renderer, circle_tex, &src[i], &dst[i]);
}

static void draw_icon(SDL_Renderer *renderer, int icon, int cx, int cy, int size, SDL_Color c, SDL_RendererFlip flip) {
    if (icon_tex[icon] == NULL)
        return;

    /* Whole multiples of 16 keep the pixel art crisp. */
    if (size >= 32)
        size = size / 16 * 16;

    SDL_Rect dst = { cx - size / 2, cy - size / 2, size, size };
    set_tex_color(icon_tex[icon], c);
    SDL_RenderCopyEx(renderer, icon_tex[icon], NULL, &dst, 0, NULL, flip);
}

static SDL_Rect to_pixels(const touch_rect *t) {
    SDL_Rect r;
    r.x = (int) (t->x * layout_out_w);
    r.y = (int) (t->y * layout_out_h);
    r.w = (int) (t->w * layout_out_w);
    r.h = (int) (t->h * layout_out_h);

    /* Gap so neighbouring buttons look separate. */
    int inset = layout_out_h / 60 + 3;
    r.x += inset;
    r.y += inset;
    r.w -= 2 * inset;
    r.h -= 2 * inset;
    return r;
}

static void draw_button(SDL_Renderer *renderer, const touch_rect *t, int icon, int pressed, int lit,
                        SDL_RendererFlip flip = SDL_FLIP_NONE) {
    SDL_Rect r = to_pixels(t);
    int m = r.w < r.h ? r.w : r.h;

    fill_round_rect(renderer, r, m / 5, (pressed || lit) ? COL_BUTTON_ON : COL_BUTTON);

    SDL_Color ic = (pressed || lit) ? COL_ACTIVE : COL_ICON;
    draw_icon(renderer, icon, r.x + r.w / 2, r.y + r.h / 2, m * 55 / 100, ic, flip);
}

static void draw_stick_cue(SDL_Renderer *renderer) {
    /* Static, low-contrast hint: slide up/down anywhere on the left. */
    SDL_Rect r = to_pixels(&stick_hint);
    int cx = r.x + r.w / 2;
    int size = (r.w * 45 / 100);
    if (size > 64)
        size = 64;

    int top = r.y + size / 2;
    int bottom = r.y + r.h - size / 2;

    SDL_Color idle = COL_ICON;
    idle.a = 70;
    SDL_Color on = COL_ACTIVE;
    on.a = 230;

    draw_icon(renderer, ICON_CHEVRON, cx, top, size, (stick_active && stick_dir < 0) ? on : idle, SDL_FLIP_NONE);
    draw_icon(renderer, ICON_CHEVRON, cx, bottom, size, (stick_active && stick_dir > 0) ? on : idle, SDL_FLIP_VERTICAL);

    /* Dotted guide line between the chevrons */
    int dot = layout_out_h / 120 + 2;
    int y0 = top + size / 2 + dot * 2;
    int y1 = bottom - size / 2 - dot * 2;
    SDL_SetRenderDrawColor(renderer, COL_ICON.r, COL_ICON.g, COL_ICON.b, stick_active ? 70 : 40);
    for (int y = y0; y < y1; y += dot * 3) {
        SDL_Rect d = { cx - dot / 2, y, dot, dot };
        SDL_RenderFillRect(renderer, &d);
    }
}

void touch_draw_backdrop(SDL_Renderer *renderer, SDL_Surface *surface, int out_w, int out_h, const SDL_Rect *pic) {
    if (!touch_controls_enabled() || surface == NULL)
        return;

    ensure_textures(renderer);
    if (ambient_tex == NULL)
        return;

    /* Average the outermost picture columns into a tiny texture; drawn with
       linear filtering it gives a soft "ambient light" in the side bars. */
    Uint32 amb[2 * AMBIENT_ROWS];
    int band = surface->w / 80 + 1;     /* columns averaged per side */
    int rows = surface->h / AMBIENT_ROWS;
    if (rows < 1)
        rows = 1;

    for (int ar = 0; ar < AMBIENT_ROWS; ar++) {
        for (int side = 0; side < 2; side++) {
            unsigned sr = 0, sg = 0, sb = 0, n = 0;
            int x0 = side == 0 ? 0 : surface->w - band;
            for (int y = ar * rows; y < (ar + 1) * rows && y < surface->h; y++) {
                Uint32 *line = (Uint32 *) ((Uint8 *) surface->pixels + y * surface->pitch);
                for (int x = x0; x < x0 + band; x++) {
                    Uint8 r, g, b;
                    SDL_GetRGB(line[x], surface->format, &r, &g, &b);
                    sr += r;
                    sg += g;
                    sb += b;
                    n++;
                }
            }
            if (n == 0)
                n = 1;
            amb[ar * 2 + side] = 0xFF000000u | ((sr / n) << 16) | ((sg / n) << 8) | (sb / n);
        }
    }
    SDL_UpdateTexture(ambient_tex, NULL, amb, 2 * sizeof(Uint32));


    const int bands = 28;
    int left = pic->x;
    int right_start = pic->x + pic->w;
    int right = out_w - right_start;

    for (int i = 0; i < bands; i++) {
        float t0 = (float) i / bands, t1 = (float) (i + 1) / bands;
        float t = (t0 + t1) * 0.5f;
        float fall = 1.0f - t;
        int alpha = (int) (255 * fall * fall);
        SDL_SetTextureAlphaMod(ambient_tex, (Uint8) alpha);

        /* Full picture colour at the edge, easing into a slightly cool tint. */
        float k = t * 2.0f > 1.0f ? 1.0f : t * 2.0f;
        SDL_SetTextureColorMod(ambient_tex, (Uint8) (255 - 65 * k), (Uint8) (255 - 55 * k), (Uint8) (255 - 30 * k));

        if (left > 0) {
            int xa = left - (int) (t1 * left), xb = left - (int) (t0 * left);
            SDL_Rect src = { 0, 0, 1, AMBIENT_ROWS };
            SDL_Rect dst = { xa, pic->y, xb - xa, pic->h };
            if (dst.w > 0)
                SDL_RenderCopy(renderer, ambient_tex, &src, &dst);
        }
        if (right > 0) {
            int xa = right_start + (int) (t0 * right), xb = right_start + (int) (t1 * right);
            SDL_Rect src = { 1, 0, 1, AMBIENT_ROWS };
            SDL_Rect dst = { xa, pic->y, xb - xa, pic->h };
            if (dst.w > 0)
                SDL_RenderCopy(renderer, ambient_tex, &src, &dst);
        }
    }

    (void) out_h;
}

void touch_draw(SDL_Renderer *renderer) {
    if (!touch_controls_enabled() || !layout_ready)
        return;

    ensure_textures(renderer);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    /* Back button is always available (menus need it too). */
    draw_button(renderer, &rects[TB_ESCAPE], ICON_BACK, held[TB_ESCAPE], 0);

    if (ingame_frames > 0) {
        ingame_frames--;

        draw_stick_cue(renderer);
        /* Lever tilts forward (mirrored) while the engine is running. */
        draw_button(renderer, &rects[TB_POWER], ICON_LEVER, held[TB_POWER], power_indicator,
                    power_indicator ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
        draw_button(renderer, &rects[TB_BOMBS], ICON_BOMB, held[TB_BOMBS], 0);
        draw_button(renderer, &rects[TB_GUNS], ICON_GUNS, held[TB_GUNS], 0);
    }

    /* Restore state: SDL_RenderClear uses the draw colour, so this also sets
       the backdrop colour for the next frame. */
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(renderer, BG_R, BG_G, BG_B, 255);
}
