/******************************************************************************
 *
 * Project:  OpenAir Translator
 * Purpose:  Implements OGROpenAirDataSource class
 * Author:   Even Rouault, even dot rouault at mines dash paris dot org
 *
 ******************************************************************************
 * Copyright (c) 2010, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "ogr_openair.h"
#include "cpl_conv.h"
#include "cpl_string.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                        OGROpenAirDataSource()                        */
/************************************************************************/

OGROpenAirDataSource::OGROpenAirDataSource() :
    pszName(NULL),
    papoLayers(NULL),
    nLayers(0)
{}

/************************************************************************/
/*                       ~OGROpenAirDataSource()                        */
/************************************************************************/

OGROpenAirDataSource::~OGROpenAirDataSource()

{
    for( int i = 0; i < nLayers; i++ )
        delete papoLayers[i];
    CPLFree( papoLayers );

    CPLFree( pszName );
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGROpenAirDataSource::TestCapability( const char * /* pszCap */ )
{
    return FALSE;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGROpenAirDataSource::GetLayer( int iLayer )

{
    if( iLayer < 0 || iLayer >= nLayers )
        return NULL;

    return papoLayers[iLayer];
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGROpenAirDataSource::Open( const char * pszFilename )

{
    pszName = CPLStrdup( pszFilename );

    VSILFILE* fp = VSIFOpenL(pszFilename, "rb");
    if (fp == NULL)
        return FALSE;

    VSILFILE* fp2 = VSIFOpenL(pszFilename, "rb");
    if (fp2)
    {
        nLayers = 2;
        papoLayers = (OGRLayer**) CPLMalloc(2 * sizeof(OGRLayer*));
        papoLayers[0] = new OGROpenAirLayer(fp);
        papoLayers[1] = new OGROpenAirLabelLayer(fp2);
    }
    else
    {
        VSIFCloseL(fp);
        return FALSE;
    }

    return TRUE;
}

/************************************************************************/
/*                              GetLatLon()                             */
/************************************************************************/

enum { DEGREE, MINUTE, SECOND };

bool OGROpenAirGetLatLon( const char* pszStr, double& dfLat, double& dfLon )
{
    dfLat = 0;
    dfLon = 0;

    GUIntBig nCurInt = 0;
    double dfExp = 1.;
    bool bHasExp = false;
    int nCurPart = DEGREE;
    double dfDegree = 0;
    double dfMinute = 0;
    double dfSecond = 0;
    char c = '\0';
    bool bHasLat = false;
    bool bHasLon = false;
    while((c = *pszStr) != 0)
    {
        if (c >= '0' && c <= '9')
        {
            nCurInt = nCurInt * 10U + static_cast<unsigned char>(c) - '0';
            if( nCurInt >> 60 ) // to avoid uint64 overflow at next iteration
                return FALSE;
            if( bHasExp )
                dfExp *= 10;
        }
        else if (c == '.')
        {
            bHasExp = true;
        }
        else if (c == ':')
        {
            const double dfVal = nCurInt / dfExp;
            if (nCurPart == DEGREE)
                dfDegree = dfVal;
            else if (nCurPart == MINUTE)
                dfMinute = dfVal;
            else if (nCurPart == SECOND)
                dfSecond = dfVal;
            nCurPart ++;
            nCurInt = 0;
            dfExp = 1.;
            bHasExp = false;
        }
        else if (c == ' ')
        {

        }
        else if (c == 'N' || c == 'S')
        {
            const double dfVal = nCurInt / dfExp;
            if (nCurPart == DEGREE)
                dfDegree = dfVal;
            else if (nCurPart == MINUTE)
                dfMinute = dfVal;
            else if (nCurPart == SECOND)
                dfSecond = dfVal;

            dfLat = dfDegree + dfMinute / 60 + dfSecond / 3600;
            if (c == 'S')
                dfLat = -dfLat;
            nCurInt = 0;
            dfExp = 1.;
            bHasExp = false;
            nCurPart = DEGREE;
            bHasLat = true;
        }
        else if (c == 'E' || c == 'W')
        {
            const double dfVal = nCurInt / dfExp;
            if (nCurPart == DEGREE)
                dfDegree = dfVal;
            else if (nCurPart == MINUTE)
                dfMinute = dfVal;
            else if (nCurPart == SECOND)
                dfSecond = dfVal;

            dfLon = dfDegree + dfMinute / 60 + dfSecond / 3600;
            if (c == 'W')
                dfLon = -dfLon;
            bHasLon = true;
            break;
        }

        pszStr++;
    }

    return bHasLat && bHasLon;
}
