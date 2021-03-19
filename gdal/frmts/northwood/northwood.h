/******************************************************************************
 * $Id$
 *
 * Project:  GRC/GRD Reader
 * Purpose:  Northwood Technologies Grid format declarations
 * Author:   Perry Casson
 *
 ******************************************************************************
 * Copyright (c) 2007, Waypoint Information Technology
 * Copyright (c) 2009-2011, Even Rouault <even dot rouault at spatialys.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#ifdef NOT_GDAL
#include <stdio.h>
#define VSILFILE    FILE
#define VSIFOpenL   fopen
#define VSIFCloseL  fclose
#define VSIFSeekL   fseek
#define VSIFReadL   fread
#else
#include "cpl_vsi.h"
#endif

typedef struct
{
    int row;
    unsigned char *r;
    unsigned char *g;
    unsigned char *b;
} NWT_RGB_ROW;

typedef struct
{
    unsigned char r;
    unsigned char g;
    unsigned char b;
} NWT_RGB;

typedef struct
{
    short h;
    short l;
    short s;
} HLS;

typedef struct
{
    float zVal;
    unsigned char r;
    unsigned char g;
    unsigned char b;
} NWT_INFLECTION;

typedef struct
{
    unsigned short usPixVal;
    unsigned char res1;            // unknown
    unsigned char r;
    unsigned char g;
    unsigned char b;
    unsigned char res2;            // unknown
    unsigned short usLen;
    char szClassName[256];
} NWT_CLASSIFIED_ITEM;

typedef struct
{
    unsigned int nNumClassifiedItems;
//  NWT_CLASSIFIED_ITEM *stClassifiedItem[4096]; //hack - it could be up to 64K
    NWT_CLASSIFIED_ITEM **stClassifiedItem;    //hack - it could be up to 64K
} NWT_CLASSIFIED_DICT;

typedef struct {
    int iBrightness;
    int iContrast;
    bool bGreyscale;
    bool bGrey;
    bool bColour;
    bool bTransparent;
    int iTransColour;
    int iTranslucency;
} RASTER_STYLE;

typedef struct
{
    char szFileName[256];
    VSILFILE *fp;
    float fVersion;
    unsigned char cFormat;  // 0x00 16 bit, 0x01 32 bit, 0x80 8 bit classified, 0x81 16 bit classified
    unsigned int nBitsPerPixel;
    unsigned int nXSide;
    unsigned int nYSide;
    double dfStepSize;
    double dfMinX;
    double dfMaxX;
    double dfMinY;
    double dfMaxY;
    float fZMin;
    float fZMax;
    float fZMinScale;
    float fZMaxScale;
    int iZUnits;
    char cDescription[32];        //??
    char cZUnits[32];                //??
    char cMICoordSys[256];
    unsigned short iNumColorInflections;
    NWT_INFLECTION stInflection[32];
    bool bHillShadeExists;
    bool bShowGradient;
    bool bShowHillShade;
    char cHillShadeBrightness;
    char cHillShadeContrast;
    float fHillShadeAzimuth;
    float fHillShadeAngle;
    NWT_CLASSIFIED_DICT *stClassDict;
    NWT_RGB_ROW stRGBRow;
    RASTER_STYLE style;
} NWT_GRID;

int nwt_ParseHeader( NWT_GRID * pGrd, const unsigned char *nwHeader );
NWT_GRID *nwtOpenGrid( char *filename );
void nwtCloseGrid( NWT_GRID * pGrd );
void nwtPrintGridHeader( NWT_GRID * pGrd );
int nwt_LoadColors( NWT_RGB * pMap, int mapSize, NWT_GRID * pGrd );
void nwt_HillShade( unsigned char *r, unsigned char *g, unsigned char *b,
                    char *h );

void createIP( int index, unsigned char r, unsigned char g, unsigned char b,
               NWT_RGB * map, int *pnWarkerMark );
void linearColor( NWT_RGB * pRGB, NWT_INFLECTION * pIPLow, NWT_INFLECTION * pIPHigh,
                      float fMid );

#define  HLSMAX   1024            /* H,L, and S vary over 0-HLSMAX */
#define  RGBMAX   255            /* R,G, and B vary over 0-RGBMAX */
               /* HLSMAX BEST IF DIVISIBLE BY 6 */
               /* RGBMAX, HLSMAX must each fit in a byte. */

   /* Hue is undefined if Saturation is 0 (grey-scale) */
   /* This value determines where the Hue scrollbar is */
   /* initially set for achromatic colors */
#define UNDEFINED (HLSMAX*2/3)

HLS RGBtoHLS (NWT_RGB rgb);
NWT_RGB HLStoRGB (HLS hls);
