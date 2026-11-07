/*
 * Triplane Classic - a side-scrolling dogfighting game.
 * Copyright (C) 1996,1997,2009  Dodekaedron Software Creations Oy
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * On-screen touch controls (used on touch-only devices such as iPhone).
 */

#ifndef TOUCH_H
#define TOUCH_H

#include <SDL.h>

/* Nonzero when on-screen touch controls are enabled for this build. */
int touch_controls_enabled(void);

/* Read finger state; called from update_key_state() and menu helpers. */
void touch_update(void);

/* Flight controls (up/down come from the left-side slide stick). */
int touch_up(void);
int touch_down(void);
int touch_guns(void);
int touch_bombs(void);

/* Returns 1 once per tap of the engine power button. */
int touch_consume_power_toggle(void);

/* Returns 1 once per tap of the escape/back button. */
int touch_consume_escape(void);

/* Called every flight frame so the flight buttons are drawn. */
void touch_mark_ingame(void);

/* Menu button snapping: menus register their button rectangles (in game
   coordinates, 320x200) as they test them; a touch that lands slightly
   outside a button is moved onto the nearest one. */
void touch_hit_register(int x1, int x2, int y1, int y2);
int touch_hit(int x, int y, int x1, int x2, int y1, int y2);
void touch_hit_arrow_row(int y1, int y2);
void touch_hit_new_frame(void);
void touch_snap_to_button(int *x, int *y);

/* Called by do_all() with the renderer output size and game viewport. */
void touch_set_layout(int out_w, int out_h, int vx, int vy, int vw, int vh);

/* Engine power state shown on the power button. */
void touch_set_power_indicator(int on);

/* Soft side-bar backdrop drawn before the game picture. */
void touch_draw_backdrop(SDL_Renderer *renderer, SDL_Surface *surface, int out_w, int out_h, const SDL_Rect *pic);

/* Draw the overlay on top of the game picture (before present). */
void touch_draw(SDL_Renderer *renderer);

#endif
