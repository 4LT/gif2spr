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
#include <math.h>
#include <errno.h>

#ifdef COMPILE_GIFLIB
#	include "giflib-5.1.4/lib/gif_lib.h"
#else
#	include <gif_lib.h>
#endif

#include "sprite.h"

struct Vec2D { double x; double y; };

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

int loadArgs(int argc, char *argv[])
{
    /* TODO: learn posix libs */
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

struct Vec2D parseOrigin(char *originString)
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
    return (struct Vec2D){ x, y };
}

int main(int argc, char *argv[])
{
    int err; /* gif error code */
    GifFileType *gifFile; /* GIF data read/decoded from file */
    ColorMapObject *gifColorMap;
    struct Spr_Sprite *sprite;
    struct Spr_color *sprPalette;
    struct Spr_image *images;
    float *delays;
    struct Vec2D origin;
    int alignment = -1;
    char paletteLookup[SPR_PAL_SIZE];
    int loadErr = loadArgs(argc, argv);

    if (loadErr != 0) {
        /*              1         2         3         4         5         6
         *     123456789012345678901234567890123456789012345678901234567890123*/
        fputs("USAGE: gif2spr [-a|-alignment ALIGNMENT] [-p|-palette PALFILE] "
        /*             7       
         *       4567890123456*/
                "[-origin X,Y]\n", stderr);
        fputs("       GIFFILE SPRFILE\n\n", stderr);
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

    sprPalette = malloc(sizeof(*sprPalette) * SPR_PAL_SIZE);
    if (palFileName != NULL)
        Spr_readPalette(palFileName, sprPalette, sprFatalError);
    else
        Spr_defaultPalette(sprPalette);

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
            alignment,
            gifFile->SWidth,
            gifFile->SHeight,
            SPR_SYNC_YES,
            sprPalette,
            (int32_t)floor(  -origin.x  * gifFile->SWidth),
            (int32_t)floor((1-origin.y) * gifFile->SHeight) );

    gifColorMap = gifFile->SColorMap;
    for (int i = 0; i < gifColorMap->ColorCount; i++) {
        struct Spr_color color;
        color.rgb[0] = gifColorMap->Colors[i].Red;
        color.rgb[1] = gifColorMap->Colors[i].Green;
        color.rgb[2] = gifColorMap->Colors[i].Blue;
        paletteLookup[i] = Spr_nearestIndex(sprPalette, color);
    }

    images = malloc(sizeof(*images) * gifFile->ImageCount);
    delays = malloc(sizeof(*delays) * gifFile->ImageCount);
    for (int i = 0; i < gifFile->ImageCount; i++) {
        SavedImage gifImage = gifFile->SavedImages[i];
        GifImageDesc imgDesc = gifImage.ImageDesc;
        GraphicsControlBlock gcb;
        int gifTransIndex;
        size_t pixCount = imgDesc.Width * imgDesc.Height;

        images[i].offsetX =  imgDesc.Left;
        images[i].offsetY = -imgDesc.Top;
        images[i].width  = imgDesc.Width;
        images[i].height = imgDesc.Height;
        images[i].raster = malloc(pixCount);

        if (DGifSavedExtensionToGCB(gifFile, i, &gcb) == GIF_ERROR)
            gifTransIndex = -1;
        else
            gifTransIndex = gcb.TransparentColor;

        delays[i] = gcb.DelayTime * 0.01; /* convert to seconds */

        for (size_t j = 0; j < pixCount; j++) {
            char color = gifImage.RasterBits[j];
            if (color == gifTransIndex)
                images[i].raster[j] = (char)SPR_TRANS_INDEX;
            else
                images[i].raster[j] = paletteLookup[(int)color];
        }
    }

    Spr_appendGroupFrame(sprite, delays, images,
            gifFile->ImageCount);
    Spr_write(sprite, sprFileName, sprFatalError);
    Spr_free(sprite);

    return EXIT_SUCCESS;
}
