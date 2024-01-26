/* sprite.c -- Functions for manipulating and writing sprites.
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
#include "sprite.h"

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include <limits.h>

#include "quakepal.h"

int32_t const FRAME_SINGLE = 0;
int32_t const FRAME_GROUP = 1;

/* .spr file header. */
struct header
{
    char    ident[4];   /* "IDSP" */
    int32_t version;    /* = 1 for Quake, 2 for HL */
    int32_t alignment;  /* 0: vp parallel upright
                           1: facing upright
                           2: vp parallel
                           3: oriented
                           4: vp parallel oriented */ 
    int32_t hlTexType;  /* 0: normal
                           1: additive
                           2: index alpha
                           3: alpha test */
    float   radius;     /* bounding radius */
    int32_t maxWidth;   /* width/height that can hold all frames */
    int32_t maxHeight;  
    int32_t nFrames;    /* number of frames */
    float   beamLength; /* ?? */
    int32_t syncType;   /* 0 = synchronized, 1 = random */
};

struct singleFrame
{
    int32_t frameType; /* 0 for single frame */
    struct Spr_image image;
};

struct groupFrame
{
    int32_t frameType;  /* 1 for group frame */
    int32_t nImages;
    float *imgKeys;
    struct Spr_image *images;
};

union frame
{
    int32_t frameType;
    struct singleFrame single;
    struct groupFrame group;
};

struct Spr_Sprite
{
    struct header *header;
    struct Spr_palette palette;
    union frame *frames;
    int32_t offsetX;
    int32_t offsetY;
};

/* Functions */

float dist(int32_t dx, int32_t dy)
{
    return sqrtf(dx*dx + dy*dy);
}

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
        int32_t offsetY)
{
    struct header *header;
    struct Spr_Sprite *sprite;
    int32_t dx = offsetX, dy = offsetY;
    struct Spr_color *newColors;

    /* select furthest corner from upper left */
    if (-2*offsetX < maxWidth)
        dx+= maxWidth;
    if (2*offsetY < maxHeight)
        dy+= maxHeight;

    header = malloc(sizeof(*header));
    *header = (struct header) {
        .ident = { 'I', 'D', 'S', 'P' },
        .version = ver,
        .alignment = alignment,
        .hlTexType = texType,
        .radius = dist(dx, dy), 
        .maxWidth = maxWidth,
        .maxHeight = maxHeight,
        .nFrames = 0, 
        .beamLength = 0, 
        .syncType = syncType
    };

    newColors = malloc(sizeof(*newColors) * palColorCt);
    memcpy(newColors, colors, sizeof(*colors) * palColorCt);

    sprite = malloc(sizeof(*sprite));
    *sprite = (struct Spr_Sprite) {
        header,
        (struct Spr_palette) {palColorCt, newColors},
        NULL,
        offsetX,
        offsetY
    };

    return sprite;
}

/* free frame's images, including raster */
void freeFrame(union frame frame)
{
    if (frame.frameType == FRAME_SINGLE) {
        free(frame.single.image.raster);
    }
    else {
        for (int i = 0; i < frame.group.nImages; i++)
            free(frame.group.images[i].raster);
        free(frame.group.images);
        free(frame.group.imgKeys);
    }
}

void Spr_free(struct Spr_Sprite *sprite)
{
    struct header *header = sprite->header;
    for (int i = 0; i < header->nFrames; i++)
        freeFrame(sprite->frames[i]);
    free(sprite->frames);
    free(sprite->palette.colors);
    free(sprite->header);
    free(sprite);
}

static void appendFrame(struct Spr_Sprite *sprite, union frame frame)
{
    int32_t nFrames = sprite->header->nFrames;
    sprite->frames = realloc(sprite->frames,
            sizeof(*sprite->frames) * (nFrames + 1));
    sprite->frames[nFrames] = frame;
    sprite->header->nFrames++;
}

void Spr_appendSingleFrame(struct Spr_Sprite *sprite,
        struct Spr_image const *img)
{
    union frame frame;
    size_t rasterSz = img->width * img->height;
    frame.single = (struct singleFrame) { FRAME_SINGLE, *img };
    frame.single.image.raster = malloc(rasterSz);
    memcpy(frame.single.image.raster, img->raster, rasterSz);
    appendFrame(sprite, frame);
}

void Spr_appendGroupFrame(struct Spr_Sprite *sprite, float const *delays,
        struct Spr_image const *imgs, size_t nImages)
{
    union frame frame;
    size_t imagesSz = sizeof(struct Spr_image) * nImages;
    float *imgKeys = malloc(sizeof(float) * nImages);
    float keyTime = 0;
    frame.group = (struct groupFrame) { FRAME_GROUP, nImages, imgKeys, NULL };
    frame.group.images =
            (struct Spr_image *)malloc(imagesSz);
    memcpy(frame.group.images, imgs, imagesSz);
    for (int i = 0; i < nImages; i++) {
        size_t rasterSz = imgs[i].width * imgs[i].height;
        keyTime+= delays[i] > 0 ? delays[i] : FLT_MIN;
        imgKeys[i] = keyTime;
        frame.group.images[i].raster = malloc(rasterSz);
        memcpy(frame.group.images[i].raster, imgs[i].raster, rasterSz);
    }
    appendFrame(sprite, frame);
}

static void errMsg(char const *filename, char const *message,
        Spr_onError_fp errCB)
{
    char const *separator = ": ";
    size_t sepLen = strlen(separator);
    size_t fnLen = strlen(filename);
    size_t msgLen = strlen(message);
    char *fullMsg = malloc(fnLen + sepLen + msgLen + 1);
    strcpy(fullMsg, filename);
    strcpy(fullMsg + fnLen, separator);
    strcpy(fullMsg + fnLen + sepLen, message);
    errCB(fullMsg);
}

/* Helper macros for local I/O operations.  They are very situtional: they
 * reference local variables from the context in which they are expanded, and
 * they force functions to return with 1
 */
#define MS_ERR_MSG_OPEN() {\
    errMsg(filename, "Failed to open file.", errCB);\
    return 1;\
}

#define MS_ERR_MSG_READ() {\
    errMsg(filename, "Read failure.", errCB);\
    return 1;\
}

#define MS_ERR_MSG_WRITE() {\
    errMsg(filename, "Write failure.", errCB);\
    return 1;\
}

static int writeImage(struct Spr_Sprite const *sprite, struct Spr_image img,
        FILE *file, char const *filename, Spr_onError_fp errCB)
{
    size_t rasterSz = img.width * img.height;
    int32_t totalOffsetX = img.offsetX + sprite->offsetX;
    int32_t totalOffsetY = img.offsetY + sprite->offsetY;

    if (fwrite((void *)&totalOffsetX, sizeof(totalOffsetX), 1, file) < 1)
        MS_ERR_MSG_WRITE();
    if (fwrite((void *)&totalOffsetY, sizeof(totalOffsetY), 1, file) < 1)
        MS_ERR_MSG_WRITE();
    if (fwrite((void *)&img.width, sizeof(img.width), 1, file) < 1) 
        MS_ERR_MSG_WRITE();
    if (fwrite((void *)&img.height, sizeof(img.height), 1, file) < 1)
        MS_ERR_MSG_WRITE();
    if (fwrite((void *)img.raster, 1, rasterSz, file) < rasterSz)
        MS_ERR_MSG_WRITE();
    return 0;
}

static int writeHeader(struct header const *hdr, FILE *file,
        char const *filename, Spr_onError_fp errCB)
{
    if (fwrite((void *)&hdr->ident, sizeof(hdr->ident), 1, file) < 1)
        MS_ERR_MSG_WRITE();
    if (fwrite((void *)&hdr->version, sizeof(hdr->version), 1, file) < 1)
        MS_ERR_MSG_WRITE();
    if (fwrite((void *)&hdr->alignment, sizeof(hdr->alignment), 1, file) < 1)
        MS_ERR_MSG_WRITE();

    if (hdr->version == SPR_VER_HL) {
        if (fwrite((void *)&hdr->hlTexType, sizeof(hdr->hlTexType), 1, file)<1)
            MS_ERR_MSG_WRITE();
    }

    if (fwrite((void *)&hdr->radius, sizeof(hdr->radius), 1, file) < 1)
        MS_ERR_MSG_WRITE();
    if (fwrite((void *)&hdr->maxWidth, sizeof(hdr->maxWidth), 1, file) < 1)
        MS_ERR_MSG_WRITE();
    if (fwrite((void *)&hdr->maxHeight, sizeof(hdr->maxHeight), 1, file) < 1)
        MS_ERR_MSG_WRITE();
    if (fwrite((void *)&hdr->nFrames, sizeof(hdr->nFrames), 1, file) < 1)
        MS_ERR_MSG_WRITE();
    if (fwrite((void *)&hdr->beamLength, sizeof(hdr->beamLength), 1, file) < 1)
        MS_ERR_MSG_WRITE();
    if (fwrite((void *)&hdr->syncType, sizeof(hdr->syncType), 1, file) < 1)
        MS_ERR_MSG_WRITE();
    return 0;
}

int Spr_write(struct Spr_Sprite *sprite, char const *filename,
        Spr_onError_fp errCB)
{
    FILE *file = fopen(filename, "wb");
    if (file == NULL)
        MS_ERR_MSG_OPEN();
//    if (fwrite((void *)sprite->header, sizeof(*sprite->header), 1, file) < 1)
//        MS_ERR_MSG_WRITE();
    if (writeHeader(sprite->header, file, filename, errCB))
        return 1;
    if (sprite->header->version == SPR_VER_HL) {
        if (fwrite((void *)&sprite->palette.colorCt,
                    sizeof(sprite->palette.colorCt), 1, file) < 1)
            MS_ERR_MSG_WRITE();
        if (fwrite((void *)sprite->palette.colors,
                    sizeof(*sprite->palette.colors), sprite->palette.colorCt,
                    file) < 1)
            MS_ERR_MSG_WRITE();
    }
    for (size_t i = 0; i < sprite->header->nFrames; i++) {
        union frame *frame = sprite->frames + i;
        if (fwrite((void *)&frame->frameType,
                sizeof(frame->frameType), 1, file) < 1) {
            MS_ERR_MSG_WRITE();
        }
        if (frame->frameType == FRAME_SINGLE) {
            if (writeImage(sprite, frame->single.image,
                    file, filename, errCB) != 0) {
                return 1;
            }
        }
        else {
            int32_t nImages = frame->group.nImages;
            float *imgKeys = frame->group.imgKeys;
            if (fwrite((void *)&nImages, sizeof(nImages), 1, file) < 1)
                MS_ERR_MSG_WRITE();
            if (fwrite(imgKeys, sizeof(*imgKeys), nImages, file) < nImages) {
                MS_ERR_MSG_WRITE();
            }
            for (int j = 0; j < nImages; j++) {
                if (writeImage(sprite, frame->group.images[j],
                        file, filename, errCB) != 0) {
                    return 1;
                }
            }
        }
    }
    fclose(file);
    return 0;
}

int Spr_readPalette(char const *filename, struct Spr_color *colors,
        Spr_onError_fp errCB)
{
    FILE *file = fopen(filename, "rb");
    if (file == NULL)
        MS_ERR_MSG_OPEN();
    if (fread(colors, sizeof(*colors), SPR_Q_PAL_SIZE, file) < SPR_Q_PAL_SIZE)
        MS_ERR_MSG_READ();
    fclose(file);
    return 0;
}

void Spr_defaultQPalette(struct Spr_color *colors)
{
    memcpy(colors, QUAKEPAL, sizeof(*colors) * SPR_Q_PAL_SIZE);
}

// R_WEIGHT**2 + G_WEIGHT**2 + B_WEIGHT**2 <= 2**31 / 255**2  
#define R_WEIGHT 29
#define G_WEIGHT 59
#define B_WEIGHT 11
double colorDistance(struct Spr_color color1, struct Spr_color color2)
{
    int deltas[3];
    for (int i = 0; i < 3; i++) {
        deltas[i] = (int)(color1.rgb[i]) - color2.rgb[i];
        deltas[i]*= deltas[i];
    }
    return sqrt(
            R_WEIGHT * R_WEIGHT * deltas[0] + 
            G_WEIGHT * G_WEIGHT * deltas[1] + 
            B_WEIGHT * B_WEIGHT * deltas[2]);
}

uint8_t Spr_nearestIndex(struct Spr_Sprite *sprite, struct Spr_color color)
{
    struct Spr_palette pal = sprite->palette;
    double minDist = INT_MAX;
    char nearestIndex = 0;
    for (int i = 0; i < pal.colorCt; i++) {
        if (i != pal.colorCt - 1) {
            double distance = colorDistance(pal.colors[i], color);
            if (distance < minDist) {
                minDist = distance;
                nearestIndex = i;
            }
        }
    }
    return nearestIndex;
}

uint8_t Spr_brightness(struct Spr_color color) 
{
    uint32_t maxBright = R_WEIGHT * 255 + G_WEIGHT * 255 + B_WEIGHT * 255;
    uint32_t bright =
        R_WEIGHT * color.rgb[0] +
        G_WEIGHT * color.rgb[1] +
        B_WEIGHT * color.rgb[2];
    return 255 * bright / maxBright;
}
