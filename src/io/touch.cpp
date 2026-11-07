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

    (void) vy;
    (void) vh;

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

/* ------------------------------------------------------------------------ */
/* Menu button snapping                                                      */
/* ------------------------------------------------------------------------ */

#define MAX_HIT_RECTS 256
#define SNAP_DISTANCE 12        /* game pixels (screen is 320x200) */

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

static SDL_Rect to_pixels(const touch_rect *t) {
    SDL_Rect r;
    r.x = (int) (t->x * layout_out_w);
    r.y = (int) (t->y * layout_out_h);
    r.w = (int) (t->w * layout_out_w);
    r.h = (int) (t->h * layout_out_h);

    /* Small inset so neighbouring buttons look separate. */
    int inset = layout_out_h / 80 + 2;
    r.x += inset;
    r.y += inset;
    r.w -= 2 * inset;
    r.h -= 2 * inset;
    return r;
}

static void draw_box(SDL_Renderer *renderer, const SDL_Rect &r, int active) {
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, active ? 90 : 35);
    SDL_RenderFillRect(renderer, &r);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 140);
    SDL_RenderDrawRect(renderer, &r);
}

static void draw_triangle(SDL_Renderer *renderer, int cx, int cy, int s, int pointing_up, int alpha) {
    int tip = pointing_up ? cy - s : cy + s;
    int base = pointing_up ? cy + s : cy - s;

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, alpha);
    for (int t = -1; t <= 1; t++) {
        SDL_RenderDrawLine(renderer, cx + t, tip, cx - s + t, base);
        SDL_RenderDrawLine(renderer, cx + t, tip, cx + s + t, base);
        SDL_RenderDrawLine(renderer, cx - s, base + t, cx + s, base + t);
    }
}

static void draw_cross(SDL_Renderer *renderer, const SDL_Rect &r, int diagonal) {
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

static void draw_square(SDL_Renderer *renderer, const SDL_Rect &r, int filled) {
    int s = (r.w < r.h ? r.w : r.h) / 5;
    SDL_Rect q = { r.x + r.w / 2 - s, r.y + r.h / 2 - s, 2 * s, 2 * s };

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 200);
    if (filled)
        SDL_RenderFillRect(renderer, &q);
    else
        SDL_RenderDrawRect(renderer, &q);
}

static void draw_stick(SDL_Renderer *renderer) {
    int h = layout_out_h;
    int s = h / 30 + 3;

    if (!stick_active) {
        /* Faint hint in the left bar: slide up/down here. */
        SDL_Rect r = to_pixels(&stick_hint);
        int cx = r.x + r.w / 2;
        int cy = r.y + r.h / 2;
        draw_triangle(renderer, cx, cy - 2 * s, s, 1, 70);
        draw_triangle(renderer, cx, cy + 2 * s, s, 0, 70);
        return;
    }

    int ax = (int) (stick_anchor_x * layout_out_w);
    int ay = (int) (stick_anchor_y * h);
    int travel = (int) (STICK_TRAVEL * h);
    int ky = (int) (stick_y * h);

    /* Track */
    SDL_Rect track = { ax - 2, ay - travel - s, 4, 2 * (travel + s) };
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 60);
    SDL_RenderFillRect(renderer, &track);

    /* Direction arrows light up when active */
    draw_triangle(renderer, ax, ay - travel - 2 * s, s, 1, stick_dir < 0 ? 230 : 70);
    draw_triangle(renderer, ax, ay + travel + 2 * s, s, 0, stick_dir > 0 ? 230 : 70);

    /* Knob */
    SDL_Rect knob = { ax - 2 * s, ky - s, 4 * s, 2 * s };
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 120);
    SDL_RenderFillRect(renderer, &knob);
}

void touch_draw(SDL_Renderer *renderer) {
    if (!touch_controls_enabled() || !layout_ready)
        return;

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    /* Back button is always available (menus need it too). */
    SDL_Rect esc = to_pixels(&rects[TB_ESCAPE]);
    draw_box(renderer, esc, held[TB_ESCAPE]);
    draw_cross(renderer, esc, 1);

    if (ingame_frames > 0) {
        ingame_frames--;

        draw_stick(renderer);

        SDL_Rect power = to_pixels(&rects[TB_POWER]);
        draw_box(renderer, power, held[TB_POWER]);
        draw_square(renderer, power, 0);

        SDL_Rect bombs = to_pixels(&rects[TB_BOMBS]);
        draw_box(renderer, bombs, held[TB_BOMBS]);
        draw_square(renderer, bombs, 1);

        SDL_Rect guns = to_pixels(&rects[TB_GUNS]);
        draw_box(renderer, guns, held[TB_GUNS]);
        draw_cross(renderer, guns, 0);
    }

    /* Restore state: SDL_RenderClear uses the draw colour, so leaving it
       white would paint the letterbox bars white on the next frame. */
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
}
