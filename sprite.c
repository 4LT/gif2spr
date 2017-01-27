/* sprite.c -- Functions for manipulating and writing sprites.
 * version 0.1, January 22nd, 2017
 * 
 * Copyright (C) 2017 Seth Rader
 * 
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the authors be held liable for any damages
 * arising from the use of this software.
 * 
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 * 
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 * 
 * Seth Rader      rader.seth@gmail.com
 */
#include "sprite.h"

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include <float.h>

#include "defpal.h"

int32_t const FRAME_SINGLE = 0;
int32_t const FRAME_GROUP = 1;

/* .spr file header. */
struct Header
{
    char    ident[4];   /* "IDSP" */
    int32_t version;    /* = 1 */
    int32_t alignment;  /* 0: vp parallel upright
                           1: facing upright
                           2: vp parallel
                           3: oriented
                           4: vp parallel oriented */ 
    float   radius;     /* bounding radius */
    int32_t maxWidth;   /* width/height that can hold all frames */
    int32_t maxHeight;  
    int32_t nFrames;    /* number of frames */
    float   beamLength; /* ?? */
    int32_t syncType;   /* 0 = synchronized, 1 = random */
};

struct SingleFrame
{
    int32_t frameType; /* 0 for single frame */
    struct Spr_Image image;
};

struct GroupFrame
{
    int32_t frameType;  /* 1 for group frame */
    int32_t nImages;
    float *imgKeys;
    struct Spr_Image *images;
};

union Frame
{
    int32_t frameType;
    struct SingleFrame single;
    struct GroupFrame group;
};

struct Spr_Sprite
{
    struct Header *header;
    struct Spr_Color *palette;
    union Frame *frames;
    int32_t offsetX;
    int32_t offsetY;
};

/* Functions */

float dist(int32_t dx, int32_t dy)
{
    return sqrtf(dx*dx + dy*dy);
}

struct Spr_Sprite *Spr_New(
        enum Spr_Alignment alignment,
        int32_t maxWidth,
        int32_t maxHeight,
        enum Spr_SyncType syncType,
        struct Spr_Color *palette,
        int32_t offsetX,
        int32_t offsetY)
{
    struct Header *header;
    struct Spr_Sprite *sprite;
    int32_t dx = offsetX, dy = offsetY;

    /* select furthest corner from upper left */
    if (-2*offsetX < maxWidth)
        dx+= maxWidth;
    if (2*offsetY < maxHeight)
        dy+= maxHeight;

    header = malloc(sizeof(*header));
    *header = (struct Header) {
        .ident = "IDSP",
        .version = 1,
        .alignment = alignment,
        .radius = dist(dx, dy), 
        .maxWidth = maxWidth,
        .maxHeight = maxHeight,
        .nFrames = 0, 
        .beamLength = 0, 
        .syncType = syncType
    };

    sprite = malloc(sizeof(*sprite));
    *sprite = (struct Spr_Sprite) {
        header,
        palette,
        NULL,
        offsetX,
        offsetY
    };

    return sprite;
}

/* free frame's images, including raster */
void freeFrame(union Frame frame)
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

void Spr_Free(struct Spr_Sprite *sprite)
{
    struct Header *header = sprite->header;
    for (int i = 0; i < header->nFrames; i++)
        freeFrame(sprite->frames[i]);
    free(sprite->frames);
    free(sprite->header);
}

static void appendFrame(struct Spr_Sprite *sprite, union Frame frame)
{
    int32_t nFrames = sprite->header->nFrames;
    sprite->frames = realloc(sprite->frames,
            sizeof(*sprite->frames) * (nFrames + 1));
    sprite->frames[nFrames] = frame;
    sprite->header->nFrames++;
}

void Spr_AppendSingleFrame(struct Spr_Sprite *sprite,
        struct Spr_Image const *img)
{
    union Frame frame;
    size_t rasterSz = img->width * img->height;
    frame.single = (struct SingleFrame) { FRAME_SINGLE, *img };
    frame.single.image.raster = malloc(rasterSz);
    memcpy(&frame.single.image.raster, &img->raster, rasterSz);
    appendFrame(sprite, frame);
}

void Spr_AppendGroupFrame(struct Spr_Sprite *sprite, float const *delays,
        struct Spr_Image const *imgs, size_t nImages)
{
    union Frame frame;
    size_t imagesSz = sizeof(struct Spr_Image) * nImages;
    float *imgKeys = malloc(sizeof(float) * nImages);
    float keyTime = 0;
    frame.group = (struct GroupFrame) { FRAME_GROUP, nImages, imgKeys, NULL };
    frame.group.images =
            (struct Spr_Image *)malloc(imagesSz);
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
        Spr_OnError_fp errCB)
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

static int writeImage(struct Spr_Sprite const *sprite, struct Spr_Image img,
        FILE *file, char const *filename, Spr_OnError_fp errCB)
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

int Spr_Write(struct Spr_Sprite *sprite, char const *filename,
        Spr_OnError_fp errCB)
{
    FILE *file = fopen(filename, "wb");
    if (file == NULL)
        MS_ERR_MSG_OPEN();
    if (fwrite((void *)sprite->header, sizeof(*sprite->header), 1, file) < 1)
        MS_ERR_MSG_WRITE();
    for (size_t i = 0; i < sprite->header->nFrames; i++) {
        union Frame *frame = sprite->frames + i;
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

int Spr_ReadPalette(char const *filename, struct Spr_Color *palette,
        Spr_OnError_fp errCB)
{
    FILE *file = fopen(filename, "rb");
    if (file == NULL)
        MS_ERR_MSG_OPEN();
    if (fread(palette, sizeof(*palette), SPR_PAL_SIZE, file) < SPR_PAL_SIZE)
        MS_ERR_MSG_READ();
    fclose(file);
    return 0;
}

void Spr_DefaultPalette(struct Spr_Color *palette)
{
    memcpy(palette, DEFPAL, sizeof(*palette) * SPR_PAL_SIZE);
}

double colorDistance(struct Spr_Color color1, struct Spr_Color color2)
{
    int deltas[3];
    for (int i = 0; i < 3; i++) {
        deltas[i] = (int)(color1.rgb[i]) - color2.rgb[i];
        deltas[i]*= deltas[i];
    }
    return sqrt(deltas[0] + deltas[1] + deltas[2]);
}

char Spr_NearestIndex(struct Spr_Color *palette, struct Spr_Color color)
{
    double minDist = 255 * 3;
    char nearestIndex = 0;
    for (int i = 0; i < SPR_PAL_SIZE; i++) {
        if (i != SPR_TRANS_INDEX) {
            double distance = colorDistance(palette[i], color);
            if (distance < minDist) {
                minDist = distance;
                nearestIndex = i;
            }
        }
    }
    return nearestIndex;
}
