/******************************************************************************
 * $Id$
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

CPL_CVSID("$Id$");

/************************************************************************/
/*                        OGROpenAirDataSource()                        */
/************************************************************************/

OGROpenAirDataSource::OGROpenAirDataSource()

{
    papoLayers = NULL;
    nLayers = 0;

    pszName = NULL;
}

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

int OGROpenAirDataSource::TestCapability( CPL_UNUSED const char * pszCap )
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
    else
        return papoLayers[iLayer];
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGROpenAirDataSource::Open( const char * pszFilename, int bUpdateIn)

{
    if (bUpdateIn)
    {
        return FALSE;
    }

    pszName = CPLStrdup( pszFilename );

// -------------------------------------------------------------------- 
//      Does this appear to be an openair file?
// --------------------------------------------------------------------

    VSILFILE* fp = VSIFOpenL(pszFilename, "rb");
    if (fp == NULL)
        return FALSE;

    char szBuffer[10000];
    int nbRead = (int)VSIFReadL(szBuffer, 1, sizeof(szBuffer) - 1, fp);
    szBuffer[nbRead] = '\0';

    int bIsOpenAir = (strstr(szBuffer, "\nAC ") != NULL &&
                  strstr(szBuffer, "\nAN ") != NULL &&
                  strstr(szBuffer, "\nAL ") != NULL &&
                  strstr(szBuffer, "\nAH") != NULL);

    if (bIsOpenAir)
    {
        VSIFSeekL( fp, 0, SEEK_SET );

        VSILFILE* fp2 = VSIFOpenL(pszFilename, "rb");
        if (fp2)
        {
            nLayers = 2;
            papoLayers = (OGRLayer**) CPLMalloc(2 * sizeof(OGRLayer*));
            papoLayers[0] = new OGROpenAirLayer(fp);
            papoLayers[1] = new OGROpenAirLabelLayer(fp2);
        }
    }
    else
        VSIFCloseL(fp);

    return bIsOpenAir;
}


/************************************************************************/
/*                              GetLatLon()                             */
/************************************************************************/

enum { DEGREE, MINUTE, SECOND };

int OGROpenAirGetLatLon(const char* pszStr, double& dfLat, double& dfLon)
{
    dfLat = 0;
    dfLon = 0;

    int nCurInt = 0;
    double dfExp = 1.;
    int bHasExp = FALSE;
    int nCurPart = DEGREE;
    double dfDegree = 0, dfMinute = 0, dfSecond = 0;
    char c;
    int bHasLat = FALSE, bHasLon = FALSE;
    while((c = *pszStr) != 0)
    {
        if (c >= '0' && c <= '9')
        {
            nCurInt = nCurInt * 10 + c - '0';
            if (bHasExp)
                dfExp *= 10;
        }
        else if (c == '.')
        {
            bHasExp = TRUE;
        }
        else if (c == ':')
        {
            double dfVal = nCurInt / dfExp;
            if (nCurPart == DEGREE)
                dfDegree = dfVal;
            else if (nCurPart == MINUTE)
                dfMinute = dfVal;
            else if (nCurPart == SECOND)
                dfSecond = dfVal;
            nCurPart ++;
            nCurInt = 0;
            dfExp = 1.;
            bHasExp = FALSE;
        }
        else if (c == ' ')
        {

        }
        else if (c == 'N' || c == 'S')
        {
            double dfVal = nCurInt / dfExp;
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
            bHasExp = FALSE;
            nCurPart = DEGREE;
            bHasLat = TRUE;
        }
        else if (c == 'E' || c == 'W')
        {
            double dfVal = nCurInt / dfExp;
            if (nCurPart == DEGREE)
                dfDegree = dfVal;
            else if (nCurPart == MINUTE)
                dfMinute = dfVal;
            else if (nCurPart == SECOND)
                dfSecond = dfVal;

            dfLon = dfDegree + dfMinute / 60 + dfSecond / 3600;
            if (c == 'W')
                dfLon = -dfLon;
            bHasLon = TRUE;
            break;
        }

        pszStr++;
    }

    return bHasLat && bHasLon;
}
