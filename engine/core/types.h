/*
 * Minimal Tiny Engine compatible types for the Micropolis Playdate build.
 * This is a from-scratch shim covering only the API surface the game uses.
 */
#ifndef TINY_TYPES_H
#define TINY_TYPES_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

typedef struct { int x, y; } vec2i_t;
typedef struct { float x, y; } vec2_t;
typedef struct { uint8_t r, g, b, a; } rgba_t;

static inline vec2i_t vec2i(int x, int y) { vec2i_t v = { x, y }; return v; }
static inline vec2_t vec2(float x, float y) { vec2_t v = { x, y }; return v; }

static inline rgba_t rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    rgba_t c = { r, g, b, a };
    return c;
}
static inline rgba_t rgba_white(void) { return rgba(255, 255, 255, 255); }
static inline rgba_t rgba_black(void) { return rgba(0, 0, 0, 255); }

#endif
