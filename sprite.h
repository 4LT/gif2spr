/* sprite.h -- Sprite manipulation interface.
 * version 0.2
 * 
 * This is free and unencumbered software released into the public domain.
 *
 * Anyone is free to copy, modify, publish, use, compile, sell, or
 * distribute this software, either in source code form or as a compiled
 * binary, for any purpose, commercial or non-commercial, and by any
 * means.
 * 
 * In jurisdictions that recognize copyright laws, the author or authors
 * of this software dedicate any and all copyright interest in the
 * software to the public domain. We make this dedication for the benefit
 * of the public at large and to the detriment of our heirs and
 * successors. We intend this dedication to be an overt act of
 * relinquishment in perpetuity of all present and future rights to this
 * software under copyright law.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 * 
 * For more information, please refer to <http://unlicense.org/>
 */
/* sprite.h - Data structures and functions for building and writing Quake
 * sprite files.
 */
#ifndef SPRITE_H_
#define SPRITE_H_

#include <stdint.h>
#include <stdlib.h>

/* Constants and Enums */

/* Number of colors in a palette. (Quake) */
#define SPR_Q_PAL_SIZE 256
#define SPR_MAX_PAL_SIZE 256
#define SPR_TRANS_IDX 255

enum Spr_alignment
{
    SPR_ALIGN_VP_PARALLEL_UPRIGHT = 0,
    SPR_ALIGN_UPRIGHT,
    SPR_ALIGN_VP_PARALLEL,
    SPR_ALIGN_ORIENTED,
    SPR_ALIGN_VP_PARALLEL_ORIENTED
};

enum Spr_version
{
    SPR_VER_QUAKE = 1,
    SPR_VER_HL = 2
};

enum Spr_hlTextureType
{
    SPR_TEX_NORMAL = 0,
    SPR_TEX_ADDITIVE,
    SPR_TEX_INDEX_ALPHA,
    SPR_TEX_ALPHA_TEST
};

enum Spr_syncType
{
    SPR_SYNC_YES = 0,
    SPR_SYNC_RANDOM
};

/* Function Pointer Typedefs */

/* File access error callback. DO NOT shallow copy errString for use after
 * callback, allocation may be temporary.
 */
typedef void (*Spr_onError_fp)(char const *errString);

/* Structs */

struct Spr_Sprite;

struct Spr_color
{
    uint8_t rgb[3];
};

struct Spr_palette
{
    uint16_t colorCt;
    struct Spr_color *colors;
};

struct Spr_image
{
    int32_t offsetX; /* image's local offsets, added to sprite's offsets */
    int32_t offsetY;
    int32_t width;
    int32_t height;
    uint8_t *raster; /* list of palette indices */
};

/* Functions */

/* Allocate memory for and create a new sprite.
 * alignment - Sprite alignment in 3D space.
 * maxWidth/maxHeight - Dimensions of sprite to fit all images.
 * syncType - Select all same frame, or random?
 * palette - List of byte-size colormap indices, 256 bytes in length.
 * offsetX/offsetY - Offset applied to each image, where the image's upper left
 *     corner is centered on the origin when offsets = {0, 0}.
 */
struct Spr_Sprite *Spr_new(
        enum Spr_version ver,
        enum Spr_alignment alignment,
        enum Spr_hlTextureType texType,
        int32_t maxWidth,
        int32_t maxHeight,
        enum Spr_syncType syncType,
        uint16_t palColorCt,
        struct Spr_color const *colors,
        int32_t offsetX,
        int32_t offsetY);

/* Deallocate memory used by the sprite, and all members recursively, except for
 * palette.
 */
void Spr_free(struct Spr_Sprite *sprite);

/* Append a new frame to the sprite, copying image data provided. */
void Spr_appendSingleFrame(struct Spr_Sprite *sprite,
    struct Spr_image const *img);

/* Append a group of nImages frames, copying image data provided.
 * delays - delay for each image in seconds
 */
void Spr_appendGroupFrame(struct Spr_Sprite *sprite, float const *delays,
        struct Spr_image const *imgs, size_t nImages);

/* Write a sprite out to file.
 * errCB - Callback called on error.
 * errStream - Stream to print error messages to.
 * Returns 0 on success, 1 on failure.
 */
int Spr_write(struct Spr_Sprite *sprite, char const *filename,
        Spr_onError_fp errCB);

/* Read a raw 256-color 24bpp palette from file.
 * palette - Must have 256 colors (768 bytes) allocated.
 * Returns 0 on success, 1 on failure to read.
 */
int Spr_readPalette(char const *filename, struct Spr_color *colors,
        Spr_onError_fp errCB);

/* Get default Quake color palette.
 * palette - Must have 256 colors (768 bytes) allocated.
 */
void Spr_defaultQPalette(struct Spr_color *colors);

/* Find index in the sprite's palette with nearest color to the color provided.
 */
uint8_t Spr_nearestIndex(struct Spr_Sprite *sprite, struct Spr_color color);

/* Get 0-255 brightness of color using nearestIndex's color weights.
 */
uint8_t Spr_brightness(struct Spr_color color);

#endif
