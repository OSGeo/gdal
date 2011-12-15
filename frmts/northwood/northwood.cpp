/******************************************************************************
 * $Id$
 *
 * Project:  GRC/GRD Reader
 * Purpose:  Northwood Format basic implementation
 * Author:   Perry Casson
 *
 ******************************************************************************
 * Copyright (c) 2007, Waypoint Information Technology
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


//#ifndef MSVC
#include "gdal_pam.h"
//#endif

#include "northwood.h"


int nwt_ParseHeader( NWT_GRID * pGrd, char *nwtHeader )
{
    int i;
    unsigned short usTmp;
    double dfTmp;
    unsigned char cTmp[256];

    if( nwtHeader[4] == '1' )
        pGrd->cFormat = 0x00;        // grd - surface type
    else if( nwtHeader[4] == '8' )
        pGrd->cFormat = 0x80;        //  grc classified type

    pGrd->stClassDict = NULL;

    memcpy( (void *) &pGrd->fVersion, (void *) &nwtHeader[5],
              sizeof(pGrd->fVersion) );
    CPL_LSBPTR32(&pGrd->fVersion);

    memcpy( (void *) &usTmp, (void *) &nwtHeader[9], 2 );
    CPL_LSBPTR16(&usTmp);
    pGrd->nXSide = (unsigned int) usTmp;
    if( pGrd->nXSide == 0 )
    {
        memcpy( (void *) &pGrd->nXSide, (void *) &nwtHeader[128],
                sizeof(pGrd->nXSide) );
        CPL_LSBPTR32(&pGrd->nXSide);
    }

    memcpy( (void *) &usTmp, (void *) &nwtHeader[11], 2 );
    CPL_LSBPTR16(&usTmp);
    pGrd->nYSide = (unsigned int) usTmp;
    if( pGrd->nYSide == 0 )
    {
        memcpy( (void *) &pGrd->nYSide, (void *) &nwtHeader[132],
                sizeof(pGrd->nYSide) );
        CPL_LSBPTR32(&pGrd->nYSide);
    }

    memcpy( (void *) &pGrd->dfMinX, (void *) &nwtHeader[13],
            sizeof(pGrd->dfMinX) );
    CPL_LSBPTR64(&pGrd->dfMinX);
    memcpy( (void *) &pGrd->dfMaxX, (void *) &nwtHeader[21],
            sizeof(pGrd->dfMaxX) );
    CPL_LSBPTR64(&pGrd->dfMaxX);
    memcpy( (void *) &pGrd->dfMinY, (void *) &nwtHeader[29],
            sizeof(pGrd->dfMinY) );
    CPL_LSBPTR64(&pGrd->dfMinY);
    memcpy( (void *) &pGrd->dfMaxY, (void *) &nwtHeader[37],
            sizeof(pGrd->dfMaxY) );
    CPL_LSBPTR64(&pGrd->dfMaxY);

    pGrd->dfStepSize = (pGrd->dfMaxX - pGrd->dfMinX) / (pGrd->nXSide - 1);
    dfTmp = (pGrd->dfMaxY - pGrd->dfMinY) / (pGrd->nYSide - 1);

    memcpy( (void *) &pGrd->fZMin, (void *) &nwtHeader[45],
            sizeof(pGrd->fZMin) );
    CPL_LSBPTR32(&pGrd->fZMin);
    memcpy( (void *) &pGrd->fZMax, (void *) &nwtHeader[49],
            sizeof(pGrd->fZMax) );
    CPL_LSBPTR32(&pGrd->fZMax);
    memcpy( (void *) &pGrd->fZMinScale, (void *) &nwtHeader[53],
            sizeof(pGrd->fZMinScale) );
    CPL_LSBPTR32(&pGrd->fZMinScale);
    memcpy( (void *) &pGrd->fZMaxScale, (void *) &nwtHeader[57],
            sizeof(pGrd->fZMaxScale) );
    CPL_LSBPTR32(&pGrd->fZMaxScale);

    memcpy( (void *) &pGrd->cDescription, (void *) &nwtHeader[61],
            sizeof(pGrd->cDescription) );
    memcpy( (void *) &pGrd->cZUnits, (void *) &nwtHeader[93],
            sizeof(pGrd->cZUnits) );

    memcpy( (void *) &i, (void *) &nwtHeader[136], 4 );
    CPL_LSBPTR32(&i);

    if( i == 1129336130 )
    {                            //BMPC
        if( nwtHeader[140] & 0x01 )
        {
            pGrd->cHillShadeBrightness = nwtHeader[144];
            pGrd->cHillShadeContrast = nwtHeader[145];
        }
    }

    memcpy( (void *) &pGrd->cMICoordSys, (void *) &nwtHeader[256],
            sizeof(pGrd->cMICoordSys) );
    pGrd->cMICoordSys[sizeof(pGrd->cMICoordSys)-1] = '\0';

    pGrd->iZUnits = nwtHeader[512];

    if( nwtHeader[513] & 0x80 )
        pGrd->bShowGradient = true;

    if( nwtHeader[513] & 0x40 )
        pGrd->bShowHillShade = true;

    if( nwtHeader[513] & 0x20 )
        pGrd->bHillShadeExists = true;

    memcpy( (void *) &pGrd->iNumColorInflections, (void *) &nwtHeader[516],
            2 );
    CPL_LSBPTR16(&pGrd->iNumColorInflections);

    if (pGrd->iNumColorInflections > 32)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Corrupt header");
        pGrd->iNumColorInflections = (unsigned short)i;
        return FALSE;
    }
    
    for( i = 0; i < pGrd->iNumColorInflections; i++ )
    {
        
        memcpy( (void *) &pGrd->stInflection[i].zVal,
                (void *) &nwtHeader[518 + (7 * i)], 4 );
        CPL_LSBPTR32(&pGrd->stInflection[i].zVal);
        memcpy( (void *) &pGrd->stInflection[i].r,
                (void *) &nwtHeader[522 + (7 * i)], 1 );
        memcpy( (void *) &pGrd->stInflection[i].g,
                (void *) &nwtHeader[523 + (7 * i)], 1 );
        memcpy( (void *) &pGrd->stInflection[i].b,
                (void *) &nwtHeader[524 + (7 * i)], 1 );
    }

    memcpy( (void *) &pGrd->fHillShadeAzimuth, (void *) &nwtHeader[966],
            sizeof(pGrd->fHillShadeAzimuth) );
    CPL_LSBPTR32(&pGrd->fHillShadeAzimuth);
    memcpy( (void *) &pGrd->fHillShadeAngle, (void *) &nwtHeader[970],
            sizeof(pGrd->fHillShadeAngle) );
    CPL_LSBPTR32(&pGrd->fHillShadeAngle);

    pGrd->cFormat += nwtHeader[1023];    // the msb for grd/grc was already set


    // there are more types than this - need to build other types for testing
    if( pGrd->cFormat & 0x80 )
    {
        if( nwtHeader[1023] == 0 )
            pGrd->nBitsPerPixel = 16;
        else
            pGrd->nBitsPerPixel = nwtHeader[1023] * 4;
    }
    else
        pGrd->nBitsPerPixel = nwtHeader[1023] * 8;


    if( pGrd->cFormat & 0x80 )        // if is GRC load the Dictionary
    {
        VSIFSeekL( pGrd->fp,
               1024 + (pGrd->nXSide * pGrd->nYSide) * pGrd->nBitsPerPixel / 8,
               SEEK_SET );

        if( !VSIFReadL( &usTmp, 2, 1, pGrd->fp) )
            return FALSE;
        CPL_LSBPTR16(&usTmp);
        pGrd->stClassDict =
            (NWT_CLASSIFIED_DICT *) calloc( sizeof(NWT_CLASSIFIED_DICT), 1 );

        pGrd->stClassDict->nNumClassifiedItems = usTmp;

        pGrd->stClassDict->stClassifedItem =
            (NWT_CLASSIFIED_ITEM **) calloc( sizeof(NWT_CLASSIFIED_ITEM *),
                                             pGrd->
                                             stClassDict->nNumClassifiedItems +
                                             1 );

        //load the dictionary
        for( usTmp=0; usTmp < pGrd->stClassDict->nNumClassifiedItems; usTmp++ )
        {
            pGrd->stClassDict->stClassifedItem[usTmp] =
              (NWT_CLASSIFIED_ITEM *) calloc( sizeof(NWT_CLASSIFIED_ITEM), 1 );
            if( !VSIFReadL( &cTmp, 9, 1, pGrd->fp ) )
                return FALSE;
            memcpy( (void *) &pGrd->stClassDict->
                    stClassifedItem[usTmp]->usPixVal, (void *) &cTmp[0], 2 );
            CPL_LSBPTR16(&pGrd->stClassDict->stClassifedItem[usTmp]->usPixVal);
            memcpy( (void *) &pGrd->stClassDict->stClassifedItem[usTmp]->res1,
                    (void *) &cTmp[2], 1 );
            memcpy( (void *) &pGrd->stClassDict->stClassifedItem[usTmp]->r,
                    (void *) &cTmp[3], 1 );
            memcpy( (void *) &pGrd->stClassDict->stClassifedItem[usTmp]->g,
                    (void *) &cTmp[4], 1 );
            memcpy( (void *) &pGrd->stClassDict->stClassifedItem[usTmp]->b,
                    (void *) &cTmp[5], 1 );
            memcpy( (void *) &pGrd->stClassDict->stClassifedItem[usTmp]->res2,
                    (void *) &cTmp[6], 1 );
            memcpy( (void *) &pGrd->stClassDict->stClassifedItem[usTmp]->usLen,
                    (void *) &cTmp[7], 2 );
            CPL_LSBPTR16(&pGrd->stClassDict->stClassifedItem[usTmp]->usLen);
                    
            if ( pGrd->stClassDict->stClassifedItem[usTmp]->usLen > 256)
                return FALSE;

            if( !VSIFReadL( &pGrd->stClassDict->stClassifedItem[usTmp]->szClassName,
                        pGrd->stClassDict->stClassifedItem[usTmp]->usLen,
                        1, pGrd->fp ) )
                return FALSE;
                
            pGrd->stClassDict->stClassifedItem[usTmp]->szClassName[255] = '\0';
        }
    }
    
    return TRUE;
}


// Create a color gradient ranging from ZMin to Zmax using the color
// inflections defined in grid
int nwt_LoadColors( NWT_RGB * pMap, int mapSize, NWT_GRID * pGrd )
{
    int i;
    NWT_RGB sColor;
    int nWarkerMark = 0;

    createIP( 0, 255, 255, 255, pMap, &nWarkerMark );
    // If Zmin is less than the 1st inflection use the 1st inflections color to
    // the start of the ramp
    if( pGrd->fZMin <= pGrd->stInflection[0].zVal )
    {
        createIP( 1, pGrd->stInflection[0].r,
                     pGrd->stInflection[0].g,
                     pGrd->stInflection[0].b, pMap, &nWarkerMark );
    }
    // find what inflections zmin is between
    for( i = 0; i < pGrd->iNumColorInflections; i++ )
    {
        if( pGrd->fZMin < pGrd->stInflection[i].zVal )
        {
            // then we must be between i and i-1
            linearColor( &sColor, &pGrd->stInflection[i - 1],
                                  &pGrd->stInflection[i],
                                  pGrd->fZMin );
            createIP( 1, sColor.r, sColor.g, sColor.b, pMap, &nWarkerMark );
            break;
        }
    }
    // the interesting case of zmin beig higher than the max inflection value
    if( i >= pGrd->iNumColorInflections )
    {
        createIP( 1,
                  pGrd->stInflection[pGrd->iNumColorInflections - 1].r,
                  pGrd->stInflection[pGrd->iNumColorInflections - 1].g,
                  pGrd->stInflection[pGrd->iNumColorInflections - 1].b,
                  pMap, &nWarkerMark );
        createIP( mapSize - 1,
                  pGrd->stInflection[pGrd->iNumColorInflections - 1].r,
                  pGrd->stInflection[pGrd->iNumColorInflections - 1].g,
                  pGrd->stInflection[pGrd->iNumColorInflections - 1].b,
                  pMap, &nWarkerMark );
    }
    else
    {
        int index = 0;
        for( ; i < pGrd->iNumColorInflections; i++ )
        {
            if( pGrd->fZMax < pGrd->stInflection[i].zVal )
            {
                // then we must be between i and i-1
                linearColor( &sColor, &pGrd->stInflection[i - 1],
                                      &pGrd->stInflection[i], pGrd->fZMax );
                index = mapSize - 1;
                createIP( index, sColor.r, sColor.g, sColor.b, pMap,
                           &nWarkerMark );
                break;
            }
            // save the inflections between zmin and zmax
            index = (int)( ( (pGrd->stInflection[i].zVal - pGrd->fZMin) /
                                              (pGrd->fZMax - pGrd->fZMin) )
                           * mapSize);
                           
            if ( index >= mapSize )
                index = mapSize - 1;
            createIP( index,
                      pGrd->stInflection[i].r,
                      pGrd->stInflection[i].g,
                      pGrd->stInflection[i].b,
                      pMap, &nWarkerMark );
        }
        if( index < mapSize - 1 )
            createIP( mapSize - 1,
                      pGrd->stInflection[pGrd->iNumColorInflections - 1].r,
                      pGrd->stInflection[pGrd->iNumColorInflections - 1].g,
                      pGrd->stInflection[pGrd->iNumColorInflections - 1].b,
                      pMap, &nWarkerMark );
    }
    return 0;
}

//solve for a color between pIPLow and pIPHigh
void linearColor( NWT_RGB * pRGB, NWT_INFLECTION * pIPLow, NWT_INFLECTION * pIPHigh,
                      float fMid )
{
    if( fMid < pIPLow->zVal )
    {
        pRGB->r = pIPLow->r;
        pRGB->g = pIPLow->g;
        pRGB->b = pIPLow->b;
    }
    else if( fMid > pIPHigh->zVal )
    {
        pRGB->r = pIPHigh->r;
        pRGB->g = pIPHigh->g;
        pRGB->b = pIPHigh->b;
    }
    else
    {
        float scale = (fMid - pIPLow->zVal) / (pIPHigh->zVal - pIPLow->zVal);
        pRGB->r = (unsigned char)
                (scale * (pIPHigh->r - pIPLow->r) + pIPLow->r + 0.5);
        pRGB->g = (unsigned char)
                (scale * (pIPHigh->g - pIPLow->g) + pIPLow->g + 0.5);
        pRGB->b = (unsigned char)
                (scale * (pIPHigh->b - pIPLow->b) + pIPLow->b + 0.5);
    }
}

// insert IP's into the map filling as we go
void createIP( int index, unsigned char r, unsigned char g, unsigned char b,
               NWT_RGB * map, int *pnWarkerMark )
{
    int i;

    if( index == 0 )
    {
        map[0].r = r;
        map[0].g = g;
        map[0].b = b;
        *pnWarkerMark = 0;
        return;
    }

    if( index <= *pnWarkerMark )
        return;

    int wm = *pnWarkerMark;

    float rslope = (float)(r - map[wm].r) / (float)(index - wm);
    float gslope = (float)(g - map[wm].g) / (float)(index - wm);
    float bslope = (float)(b - map[wm].b) / (float)(index - wm);
    for( i = wm + 1; i < index; i++)
    {
        map[i].r = map[wm].r + (unsigned char)(((i - wm) * rslope) + 0.5);
        map[i].g = map[wm].g + (unsigned char)(((i - wm) * gslope) + 0.5);
        map[i].b = map[wm].b + (unsigned char)(((i - wm) * bslope) + 0.5);
    }
    map[index].r = r;
    map[index].g = g;
    map[index].b = b;
    *pnWarkerMark = index;
    return;
}

void nwt_HillShade( unsigned char *r, unsigned char *g, unsigned char *b,
                    char *h )
{
    HLS hls;
    NWT_RGB rgb;
    rgb.r = *r;
    rgb.g = *g;
    rgb.b = *b;
    hls = RGBtoHLS( rgb );
    hls.l += ((short) *h) * HLSMAX / 256;
    rgb = HLStoRGB( hls );

    *r = rgb.r;
    *g = rgb.g;
    *b = rgb.b;
    return;
}


NWT_GRID *nwtOpenGrid( char *filename )
{
    NWT_GRID *pGrd;
    char nwtHeader[1024];
    VSILFILE *fp;

    if( (fp = VSIFOpenL( filename, "rb" )) == NULL )
    {
        fprintf( stderr, "\nCan't open %s\n", filename );
        return NULL;
    }

    if( !VSIFReadL( nwtHeader, 1024, 1, fp ) )
        return NULL;

    if( nwtHeader[0] != 'H' ||
        nwtHeader[1] != 'G' ||
        nwtHeader[2] != 'P' ||
        nwtHeader[3] != 'C' )
          return NULL;

    pGrd = (NWT_GRID *) calloc( sizeof(NWT_GRID), 1 );

    if( nwtHeader[4] == '1' )
        pGrd->cFormat = 0x00;        // grd - surface type
    else if( nwtHeader[4] == '8' )
        pGrd->cFormat = 0x80;        //  grc classified type
    else
    {
        fprintf( stderr, "\nUnhandled Northwood format type = %0xd\n",
                 nwtHeader[4] );
        if( pGrd )
            free( pGrd );
        return NULL;
    }

    strcpy( pGrd->szFileName, filename );
    pGrd->fp = fp;
    nwt_ParseHeader( pGrd, nwtHeader );

    return pGrd;
}

//close the file and free the mem
void nwtCloseGrid( NWT_GRID * pGrd )
{
    unsigned short usTmp;

    if( (pGrd->cFormat & 0x80) && pGrd->stClassDict )        // if is GRC - free the Dictionary
    {
        for( usTmp = 0; usTmp < pGrd->stClassDict->nNumClassifiedItems; usTmp++ )
        {
            free( pGrd->stClassDict->stClassifedItem[usTmp] );
        }
        free( pGrd->stClassDict->stClassifedItem );
        free( pGrd->stClassDict );
    }
    if( pGrd->fp )
        VSIFCloseL( pGrd->fp );
    free( pGrd );
        return;
}

void nwtGetRow( NWT_GRID * pGrd )
{

}

void nwtPrintGridHeader( NWT_GRID * pGrd )
{
    int i;

    if( pGrd->cFormat & 0x80 )
    {
        printf( "\n%s\n\nGrid type is Classified ", pGrd->szFileName );
        if( pGrd->cFormat == 0x81 )
            printf( "4 bit (Less than 16 Classes)" );
        else if( pGrd->cFormat == 0x82 )
            printf( "8 bit (Less than 256 Classes)" );
        else if( pGrd->cFormat == 0x84 )
            printf( "16 bit (Less than 65536 Classes)" );
        else
        {
            printf( "GRC - Unhandled Format or Type %d", pGrd->cFormat );
            return;
        }
    }
    else
    {
        printf( "\n%s\n\nGrid type is Numeric ", pGrd->szFileName );
        if( pGrd->cFormat == 0x00 )
            printf( "16 bit (Standard Percision)" );
        else if( pGrd->cFormat == 0x01 )
            printf( "32 bit (High Percision)" );
        else
        {
            printf( "GRD - Unhandled Format or Type %d", pGrd->cFormat );
            return;
        }
    }
    printf( "\nDim (x,y) = (%d,%d)", pGrd->nXSide, pGrd->nYSide );
    printf( "\nStep Size = %f", pGrd->dfStepSize );
    printf( "\nBounds = (%f,%f) (%f,%f)", pGrd->dfMinX, pGrd->dfMinY,
            pGrd->dfMaxX, pGrd->dfMaxY );
    printf( "\nCoordinate System = %s", pGrd->cMICoordSys );

    if( !(pGrd->cFormat & 0x80) )    // print the numeric specific stuff
    {
        printf( "\nMin Z = %f Max Z = %f Z Units = %d \"%s\"", pGrd->fZMin,
                pGrd->fZMax, pGrd->iZUnits, pGrd->cZUnits );

        printf( "\n\nDisplay Mode =" );
        if( pGrd->bShowGradient )
            printf( " Color Gradient" );

        if( pGrd->bShowGradient && pGrd->bShowHillShade )
            printf( " and" );

        if( pGrd->bShowHillShade )
            printf( " Hill Shading" );

        for( i = 0; i < pGrd->iNumColorInflections; i++ )
        {
            printf( "\nColor Inflection %d - %f (%d,%d,%d)", i + 1,
                    pGrd->stInflection[i].zVal, pGrd->stInflection[i].r,
                    pGrd->stInflection[i].g, pGrd->stInflection[i].b );
        }

        if( pGrd->bHillShadeExists )
        {
            printf("\n\nHill Shade Azumith = %.1f Inclination = %.1f "
                   "Brightness = %d Contrast = %d",
                   pGrd->fHillShadeAzimuth, pGrd->fHillShadeAngle,
                   pGrd->cHillShadeBrightness, pGrd->cHillShadeContrast );
        }
        else
            printf( "\n\nNo Hill Shade Data" );
    }
    else                            // print the classified specific stuff
    {
        printf( "\nNumber of Classes defined = %d",
                pGrd->stClassDict->nNumClassifiedItems );
        for( i = 0; i < (int) pGrd->stClassDict->nNumClassifiedItems; i++ )
        {
            printf( "\n%s - (%d,%d,%d)  Raw = %d  %d %d",
                    pGrd->stClassDict->stClassifedItem[i]->szClassName,
                    pGrd->stClassDict->stClassifedItem[i]->r,
                    pGrd->stClassDict->stClassifedItem[i]->g,
                    pGrd->stClassDict->stClassifedItem[i]->b,
                    pGrd->stClassDict->stClassifedItem[i]->usPixVal,
                    pGrd->stClassDict->stClassifedItem[i]->res1,
                    pGrd->stClassDict->stClassifedItem[i]->res2 );
        }
    }
}

HLS RGBtoHLS( NWT_RGB rgb )
{
    short R, G, B;                /* input RGB values */
    HLS hls;
    unsigned char cMax, cMin;        /* max and min RGB values */
    short Rdelta, Gdelta, Bdelta;    /* intermediate value: % of spread from max */
    /* get R, G, and B out of DWORD */
    R = rgb.r;
    G = rgb.g;
    B = rgb.b;

    /* calculate lightness */
    cMax = (unsigned char) MAX( MAX(R,G), B );
    cMin = (unsigned char) MIN( MIN(R,G), B );
    hls.l = (((cMax + cMin) * HLSMAX) + RGBMAX) / (2 * RGBMAX);

    if( cMax == cMin )
    {                            /* r=g=b --> achromatic case */
        hls.s = 0;                /* saturation */
        hls.h = UNDEFINED;        /* hue */
    }
    else
    {                            /* chromatic case */
        /* saturation */
        if( hls.l <= (HLSMAX / 2) )
            hls.s =
              (((cMax - cMin) * HLSMAX) + ((cMax + cMin) / 2)) / (cMax + cMin);
        else
            hls.s= (((cMax - cMin) * HLSMAX) + ((2 * RGBMAX - cMax - cMin) / 2))
              / (2 * RGBMAX - cMax - cMin);

        /* hue */
        Rdelta =
            (((cMax - R) * (HLSMAX / 6)) + ((cMax - cMin) / 2)) / (cMax - cMin);
        Gdelta =
            (((cMax - G) * (HLSMAX / 6)) + ((cMax - cMin) / 2)) / (cMax - cMin);
        Bdelta =
            (((cMax - B) * (HLSMAX / 6)) + ((cMax - cMin) / 2)) / (cMax - cMin);

        if( R == cMax )
            hls.h = Bdelta - Gdelta;
        else if( G == cMax )
            hls.h = (HLSMAX / 3) + Rdelta - Bdelta;
        else                        /* B == cMax */
            hls.h = ((2 * HLSMAX) / 3) + Gdelta - Rdelta;

        if( hls.h < 0 )
            hls.h += HLSMAX;
        if( hls.h > HLSMAX )
            hls.h -= HLSMAX;
    }
    return hls;
}


/* utility routine for HLStoRGB */
short HueToRGB( short n1, short n2, short hue )
{
    /* range check: note values passed add/subtract thirds of range */
    if( hue < 0 )
        hue += HLSMAX;

    if( hue > HLSMAX )
        hue -= HLSMAX;

    /* return r,g, or b value from this tridrant */
    if( hue < (HLSMAX / 6) )
        return (n1 + (((n2 - n1) * hue + (HLSMAX / 12)) / (HLSMAX / 6)));
    if( hue < (HLSMAX / 2) )
        return (n2);
    if( hue < ((HLSMAX * 2) / 3) )
        return (n1 +
                (((n2 - n1) * (((HLSMAX * 2) / 3) - hue) +
                (HLSMAX / 12)) / (HLSMAX / 6)));
    else
        return (n1);
}

NWT_RGB HLStoRGB( HLS hls )
{
    NWT_RGB rgb;
    short Magic1, Magic2;            /* calculated magic numbers (really!) */

    if( hls.s == 0 )
    {                            /* achromatic case */
        rgb.r = rgb.g = rgb.b = (unsigned char) ((hls.l * RGBMAX) / HLSMAX);
        if( hls.h != UNDEFINED )
        {
            /* ERROR */
        }
    }
    else
    {                            /* chromatic case */
        /* set up magic numbers */
        if( hls.l <= (HLSMAX / 2) )
            Magic2 = (hls.l * (HLSMAX + hls.s) + (HLSMAX / 2)) / HLSMAX;
        else
            Magic2 = hls.l + hls.s - ((hls.l * hls.s) + (HLSMAX / 2)) / HLSMAX;
        Magic1 = 2 * hls.l - Magic2;

        /* get RGB, change units from HLSMAX to RGBMAX */
        rgb.r = (unsigned char) ((HueToRGB (Magic1, Magic2, hls.h + (HLSMAX / 3)) * RGBMAX + (HLSMAX / 2)) / HLSMAX);
        rgb.g = (unsigned char) ((HueToRGB (Magic1, Magic2, hls.h) * RGBMAX + (HLSMAX / 2)) / HLSMAX);
        rgb.b = (unsigned char) ((HueToRGB (Magic1, Magic2, hls.h - (HLSMAX / 3)) * RGBMAX + (HLSMAX / 2)) / HLSMAX);
    }

    return rgb;
}
