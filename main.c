/* main.c -- gif2spr entrypoint.
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <errno.h>

#ifdef COMPILE_GIFLIB
#	include "giflib-5.1.4/lib/gif_lib.h"
#else
#	include <gif_lib.h>
#endif

#include "sprite.h"

struct DVec2D { double x; double y; };

struct Rect {
    int width;
    int height;
    int left;
    int top;
    uint8_t *raster;
};

void sprNonFatalError(char const *errString)
{
    fputs(errString, stderr);
}

void sprFatalError(char const *errString)
{
    sprNonFatalError(errString);
    exit(EXIT_FAILURE);
}

#define N_ALIGNMENTS 5
static const char *ALIGNMENT_NAMES[N_ALIGNMENTS] = {
    "vp-parallel-upright",
    "upright",
    "vp-parallel",
    "oriented",
    "vp-parallel-oriented" };

static char *gifFileName = NULL;
static char *sprFileName = NULL;
static char *palFileName = NULL;
static char *originString = NULL;
static char *alignmentOption = NULL;
static enum Spr_version version = SPR_VER_QUAKE; 

static void blit
(uint8_t *buffer, const uint8_t *frame, int bufW, int bufH,
 int frameW, int frameH, int left, int top, int transparent, int bgIndex)
{
    for (int fx = 0; fx < frameW; fx++)
    for (int fy = 0; fy < frameH; fy++) {
        int bx = left + fx;
        int by = top + fy;

        if (bx >= 0 && bx < bufW && by >= 0 && by < bufH) {
            uint8_t color = frame[fx + frameW * fy];
            if (color != transparent) {
                buffer[bx + bufW * by] = color;
            } else if (bgIndex >= 0) {
                buffer[bx + bufW * by] = (uint8_t)bgIndex;
            }
        }
    }
}

static void sampleRect
(const uint8_t *buffer, uint8_t *rectRaster, int bufW, int bufH,
 struct Rect rect, int transparent, uint8_t *lookup)
{
    for (int rx = 0; rx < rect.width; rx++)
    for (int ry = 0; ry < rect.height; ry++) {
        int bx = rect.left + rx;
        int by = rect.top + ry;

        if (bx >= 0 && bx < bufW && by >= 0 && by < bufH) {
            uint8_t color = buffer[bx + bufW * by];
            if (color == transparent) {
                rectRaster[rx + rect.width * ry] = SPR_TRANS_IDX;
            }
            else {
                rectRaster[rx + rect.width * ry] = lookup[color];
            }
        }
        else {
            rectRaster[rx + rect.width * ry] = SPR_TRANS_IDX;
        }
    }
}

static struct Rect minRect
(const uint8_t *buffer, int bufW, int bufH, int transparent)
{
    int left = 0;
    int right = bufW-1;
    int top = 0;
    int bottom = bufW-1;
    int x, y;
    bool done = false;

    x = left;
    while (x < bufW && !done) {
        for (y = 0; y < bufH && !done; y++) {
            if (buffer[x + bufW * y] != transparent) {
                left = x;
                done = true;
            }
        }
        x++;
    }

    done = false;
    x = right;
    while (x > left && !done) {
        for (y = 0; y < bufH && !done; y++) {
            if (buffer[x + bufW * y] != transparent) {
                right = x;
                done = true;
            }
        }
        x--;
    }

    done = false;
    y = top;
    while (y < bufH && !done) {
        for (x = left; x <= right && !done; x++) {
            if (buffer[x + bufW * y] != transparent) {
                top = y;
                done = true;
            }
        }
        y++;
    }

    done = false;
    y = bottom;
    while (y > top && !done) {
        for (x = left; x <= right && !done; x++) {
            if (buffer[x + bufW * y] != transparent) {
                bottom = y;
                done = true;
            }
        }
        y--;
    }

    struct Rect rect;
    rect.width = right >= left ? right - left + 1 : 0;
    rect.height = bottom >= top ? bottom - top + 1 : 0;
    rect.left = left;
    rect.top = top;
    return rect;
}

static int loadArgs(int argc, char *argv[])
{
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            if (strcmp(argv[i], "-origin") == 0) {
                i++;
                if (i >= argc)
                    return 1;
                originString = argv[i];
            }
            else if (strcmp(argv[i], "-palette") == 0 ||
                     strcmp(argv[i], "-p") == 0) {
                i++;
                if (i >= argc)
                    return 2;
                palFileName = argv[i];
            }
            else if (strcmp(argv[i], "-alignment") == 0 ||
                     strcmp(argv[i], "-a") == 0) {
                i++;
                if (i >= argc)
                    return 3;
                alignmentOption = argv[i];
            }
            else if (strcmp(argv[i], "-hl") == 0) {
                version = SPR_VER_HL;
            }
            else if (strcmp(argv[i], "-quake") == 0) {
                version = SPR_VER_QUAKE;
            }
            else {
                fprintf(stderr, "Unknown option \"%s\"\n", argv[i]);
                return 4;
            }
        }
        else if (gifFileName == NULL) {
            gifFileName = argv[i];
        }
        else if (sprFileName == NULL) {
            sprFileName = argv[i];
        }
        else {
            return 5;
        }
    }
    
    if (sprFileName == NULL)
        return 6;
    else
        return 0;
}

static struct DVec2D parseOrigin(char *originString)
{
    double x, y;
    if (originString == NULL) {
        x = 0.5;
        y = 0.5;
    }
    else {
        char *end;
        char *xToken = strtok(originString, ",");
        char *yToken = strtok(NULL, ",");
        if (yToken == NULL) {
            fputs("Missing origin Y.\n", stderr);
            exit(EXIT_FAILURE);
        }

        errno = 0;
        x = strtod(xToken, &end);
        if (errno == ERANGE) {
            fputs("Origin X is out of range.\n", stderr);
            exit(EXIT_FAILURE);
        } else if (end == xToken)
        {
            fputs("Origin X is not a number.\n", stderr);
            exit(EXIT_FAILURE);
        } else if (isfinite(x) == 0)
        {
            fputs("Origin X is not finite.\n", stderr);
            exit(EXIT_FAILURE);
        }

        errno = 0;
        y = strtod(yToken, &end);
        if (errno == ERANGE) {
            fputs("Origin Y is out of range.\n", stderr);
            exit(EXIT_FAILURE);
        } else if (end == yToken)
        {
            fputs("Origin Y is not a number.\n", stderr);
            exit(EXIT_FAILURE);
        } else if (isfinite(y) == 0)
        {
            fputs("Origin Y is not finite.\n", stderr);
            exit(EXIT_FAILURE);
        }
    }
    return (struct DVec2D){ x, y };
}

int main(int argc, char *argv[])
{
    int err; /* gif error code */
    GifFileType *gifFile; /* GIF data read/decoded from file */
    ColorMapObject *gifColorMap;
    struct Spr_Sprite *sprite;
    uint16_t colorCt; /* number of colors */
    static struct Spr_color colors[SPR_MAX_PAL_SIZE];
    struct Spr_image *images;
    float *delays;
    struct DVec2D origin;
    int alignment = -1;
    static uint8_t paletteLookup[SPR_MAX_PAL_SIZE];
    uint8_t *imgBuffer;
    uint8_t *prevBuffer;
    size_t canvasPixCount;
    uint8_t gifBgIndex = 0;

    int loadErr = loadArgs(argc, argv);

    if (loadErr != 0) {
        /*              1         2         3         4         5         6
         *     123456789012345678901234567890123456789012345678901234567890123*/
        fputs("USAGE: gif2spr [-a|-alignment ALIGNMENT] [-p|-palette PALFILE] "
        /*             7       
         *       4567890123456*/
                "[-origin X,Y]\n", stderr);
        fputs("       [-quake] [-hl] GIFFILE SPRFILE\n\n", stderr);
        fputs("    ALIGNMENT Sprite orientation. Options "
                "(defaults to vp-parallel):\n", stderr);
        for (int i = 0; i < N_ALIGNMENTS; i++)
            fprintf(stderr, "        %s\n", ALIGNMENT_NAMES[i]);
        fputs("    PALFILE   Palette lump. Defaults to Quake palette.\n",
                stderr);
        fputs("    X         Decimal origin X component. "
                "Defaults to 0.5 (center).\n", stderr);
        fputs("    Y         Decimal origin Y component. "
                "Defaults to 0.5 (center).\n", stderr);
        fputs("    -quake    Write sprite in Quake format (default).\n",
                stderr);
        fputs("    -hl       Write sprite in Half-Life format.\n", stderr);
        fputs("    GIFFILE   Input GIF file.\n", stderr);
        fputs("    SPRFILE   Output SPRITE file.\n", stderr);
        exit(EXIT_FAILURE);
    }

    gifFile = DGifOpenFileName(gifFileName, &err);

    if (gifFile == NULL) {
        fprintf(stderr, "%s:\n", gifFileName);
        fprintf(stderr, "%s.\n", GifErrorString(err));
        exit(EXIT_FAILURE);
    }
    
    if (DGifSlurp(gifFile) == GIF_ERROR) {
        fprintf(stderr, "%s:\n", gifFileName);
        fputs("Failed to load file.\n", stderr);
        exit(EXIT_FAILURE);
    }

    /* try to use global color map, use 1st frame's if global is null */
    gifColorMap = gifFile->SColorMap;
    if (gifColorMap == (void *)0) {
        gifColorMap = gifFile->SavedImages[0].ImageDesc.ColorMap;
    }

    if (version == SPR_VER_HL) {
        colorCt = (uint16_t)(gifColorMap->ColorCount);
        for (int i = 0; i < colorCt; i++) {
            /* copy colors */
            colors[i].rgb[0] = gifColorMap->Colors[i].Red;
            colors[i].rgb[1] = gifColorMap->Colors[i].Green;
            colors[i].rgb[2] = gifColorMap->Colors[i].Blue;
        }
    }
    else {
        colorCt = SPR_Q_PAL_SIZE;
        if (palFileName != NULL)
            Spr_readPalette(palFileName, colors, sprFatalError);
        else
            Spr_defaultQPalette(colors);
    }

    origin = parseOrigin(originString);

    if (alignmentOption == NULL) {
        alignment = SPR_ALIGN_VP_PARALLEL;
    }
    else
    {
        for ( int i = 0; i < N_ALIGNMENTS && alignment == -1; i++) {
            if (strcmp(alignmentOption, ALIGNMENT_NAMES[i]) == 0)
                alignment = i;
        }
        if (alignment == -1) {
            fprintf(stderr, "Unknown alignment type \"%s\"\n", alignmentOption);
            exit(EXIT_FAILURE);
        }
    }

    sprite = Spr_new(
            version,
            alignment,
            SPR_TEX_ALPHA_TEST,
            gifFile->SWidth,
            gifFile->SHeight,
            SPR_SYNC_RANDOM,
            colorCt,
            colors,
            (int32_t)floor(  -origin.x  * gifFile->SWidth),
            (int32_t)floor((1-origin.y) * gifFile->SHeight) );

    canvasPixCount = gifFile->SWidth * gifFile->SHeight;
    imgBuffer = malloc(canvasPixCount);
    prevBuffer = malloc(canvasPixCount);

    images = malloc(sizeof(*images) * gifFile->ImageCount);
    delays = malloc(sizeof(*delays) * gifFile->ImageCount);

    for (int i = 0; i < gifFile->ImageCount; i++) {
        SavedImage gifImage = gifFile->SavedImages[i];
        GifImageDesc imgDesc = gifImage.ImageDesc;
        GraphicsControlBlock gcb;
        ColorMapObject *localColorMap = imgDesc.ColorMap;
        int gifTransIndex;
        int gifDelay;
        int disposal;

        if (localColorMap == (void *)0)
            localColorMap = gifColorMap;

        for (int i = 0; i < localColorMap->ColorCount; i++) {
            struct Spr_color color;
            color.rgb[0] = localColorMap->Colors[i].Red;
            color.rgb[1] = localColorMap->Colors[i].Green;
            color.rgb[2] = localColorMap->Colors[i].Blue;
            paletteLookup[i] = Spr_nearestIndex(sprite, color);
        }


        if (DGifSavedExtensionToGCB(gifFile, i, &gcb) == GIF_ERROR) {
            gifTransIndex = -1;
            disposal = DISPOSAL_UNSPECIFIED;
            gifDelay = 8;
        }
        else {
            gifTransIndex = gcb.TransparentColor;
            disposal = gcb.DisposalMode;
            gifDelay = gcb.DelayTime;
        }

        /* Seems GIMP and browsers treat background as transparent */
        gifBgIndex = gifTransIndex;
        delays[i] = gifDelay * 0.01; /* convert to seconds */

        if (i == 0 || disposal == DISPOSAL_UNSPECIFIED ||
                disposal == DISPOSE_BACKGROUND) {
            if (disposal == DISPOSE_BACKGROUND) {
                memset(imgBuffer, gifBgIndex, canvasPixCount);
            }
            else {
                memset(imgBuffer, gifTransIndex, canvasPixCount);
            }
        }

        if (disposal == DISPOSE_PREVIOUS) {
            memcpy(prevBuffer, imgBuffer, canvasPixCount);
        }

        blit(imgBuffer, gifImage.RasterBits, gifFile->SWidth, gifFile->SHeight,
                imgDesc.Width, imgDesc.Height, imgDesc.Left, imgDesc.Top,
                gifTransIndex,
                disposal == DISPOSE_BACKGROUND ? gifBgIndex : -1);

        struct Rect rect = minRect(imgBuffer, gifFile->SWidth, gifFile->SHeight,
                gifTransIndex);

        images[i].offsetX =  rect.left;
        images[i].offsetY = -rect.top;
        images[i].width  = rect.width;
        images[i].height = rect.height;
        images[i].raster = malloc(rect.width * rect.height);

        sampleRect(imgBuffer, images[i].raster, gifFile->SWidth,
                gifFile->SHeight, rect, gifTransIndex, paletteLookup);

        if (disposal == DISPOSE_PREVIOUS) {
            memcpy(imgBuffer, prevBuffer, canvasPixCount);
        }

        /* OLD CODE
        
        for (size_t j = 0; j < pixCount; j++) {
            char color = gifImage.RasterBits[j];
            if (color == gifTransIndex)
                images[i].raster[j] = (uint8_t)SPR_TRANS_IDX;
            else
                images[i].raster[j] = paletteLookup[(int)color];
        }

        /OLD CODE */
    }

    if (version == SPR_VER_QUAKE) {
        Spr_appendGroupFrame(sprite, delays, images, gifFile->ImageCount);
    }
    else {
        for (int i = 0; i < gifFile->ImageCount; i++) {
            Spr_appendSingleFrame(sprite, images + i);
        }
        Spr_appendSingleFrame(sprite, images); /* dummy frame */
    }

    Spr_write(sprite, sprFileName, sprFatalError);
    Spr_free(sprite);

    return EXIT_SUCCESS;
}
