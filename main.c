#include <gif_lib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

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

static char *gifFileName = NULL;
static char *sprFileName = NULL;
static char *palFileName = NULL;
static char *originString = NULL;

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
            else {
                fprintf(stderr, "Unknown argument \"%s\"\n", argv[i]);
                return 3;
            }
        }
        else if (gifFileName == NULL) {
            gifFileName = argv[i];
        }
        else if (sprFileName == NULL) {
            sprFileName = argv[i];
        }
        else {
            return 4;
        }
    }
    
    if (sprFileName == NULL)
        return 5;
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
        x = strtof(strtok(originString, ","), NULL);
        char *yToken = strtok(NULL, ",");
        if (yToken == NULL) {
            fputs("Missing origin Y.\n", stderr);
            exit(EXIT_FAILURE);
        }
        y = atof(yToken);
        if (isfinite(x) == 0) {
            fputs("Origin X is not finite.\n", stderr);
            exit(EXIT_FAILURE);
        }
        if (isfinite(y) == 0) {
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
    struct Spr_Color *sprPalette;
    struct Spr_Image *images;
    struct Vec2D origin;
    char paletteLookup[SPR_PAL_SIZE];

    int loadErr = loadArgs(argc, argv);
    if (loadErr != 0) {
        fprintf(stderr, "%d\n", loadErr);
        fputs("USAGE: gif2spr [-p|-palette PALFILE] [-origin X,Y] "
              "GIFFILE SPRFILE\n", stderr);
        fputs("    PALFILE  Palette lump.\n", stderr);
        fputs("    X        Decimal origin X component.\n", stderr);
        fputs("    Y        Decimal origin Y component.\n", stderr);
        fputs("    GIFFILE  Input GIF file.\n", stderr);
        fputs("    SPRFILE  Output SPRITE file.\n", stderr);
        exit(EXIT_FAILURE);
    }

    gifFile = DGifOpenFileName(gifFileName, &err);

    if (gifFile == NULL) {
        fprintf(stderr, "%s:\n", gifFileName);
        fprintf(stderr, "%s\n", GifErrorString(err));
        exit(EXIT_FAILURE);
    }
    
    if (DGifSlurp(gifFile) == GIF_ERROR) {
        fprintf(stderr, "%s:\n", gifFileName);
        fprintf(stderr, "%s\n", GifErrorString(err));
        exit(EXIT_FAILURE);
    }

    sprPalette = malloc(sizeof(*sprPalette) * SPR_PAL_SIZE);
    if (palFileName != NULL)
        Spr_ReadPalette(palFileName, sprPalette, sprFatalError);
    else
        Spr_DefaultPalette(sprPalette);

    origin = parseOrigin(originString);
    sprite = Spr_New(
            SPR_ALIGN_VP_PARALLEL,
            gifFile->SWidth,
            gifFile->SHeight,
            SPR_SYNC_YES,
            sprPalette,
            (int32_t)(  -origin.x  * gifFile->SWidth),
            (int32_t)((1-origin.y) * gifFile->SHeight) );

    gifColorMap = gifFile->SColorMap;
    for (int i = 0; i < gifColorMap->ColorCount; i++) {
        struct Spr_Color color;
        color.rgb[0] = gifColorMap->Colors[i].Red;
        color.rgb[1] = gifColorMap->Colors[i].Green;
        color.rgb[2] = gifColorMap->Colors[i].Blue;
        paletteLookup[i] = Spr_NearestIndex(sprPalette, color);
    }

    images = malloc(sizeof(*images) * gifFile->ImageCount);
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

        for (size_t j = 0; j < pixCount; j++) {
            char color = gifImage.RasterBits[j];
            if (color == gifTransIndex)
                images[i].raster[j] = (char)SPR_TRANS_INDEX;
            else
                images[i].raster[j] = paletteLookup[(int)color];
        }
    }

    Spr_AppendGroupFrame(sprite, (float)1/gifFile->ImageCount, images,
            gifFile->ImageCount);
    Spr_Write(sprite, sprFileName, sprFatalError);
    Spr_Free(sprite);

    return EXIT_SUCCESS;
}
