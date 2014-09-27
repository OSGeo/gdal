/******************************************************************************
 * $Id$
 *
 * Project:  SEG-P1 / UKOOA P1-90 Translator
 * Purpose:  Implements OGRSEGUKOOADataSource class
 * Author:   Even Rouault, even dot rouault at mines dash paris dot org
 *
 ******************************************************************************
 * Copyright (c) 2011, Even Rouault <even dot rouault at mines-paris dot org>
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
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMSEGUKOOAS OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#include "ogr_segukooa.h"
#include "cpl_conv.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                        OGRSEGUKOOADataSource()                       */
/************************************************************************/

OGRSEGUKOOADataSource::OGRSEGUKOOADataSource()

{
    papoLayers = NULL;
    nLayers = 0;

    pszName = NULL;
}

/************************************************************************/
/*                       ~OGRSEGUKOOADataSource()                       */
/************************************************************************/

OGRSEGUKOOADataSource::~OGRSEGUKOOADataSource()

{
    for( int i = 0; i < nLayers; i++ )
        delete papoLayers[i];
    CPLFree( papoLayers );

    CPLFree( pszName );
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRSEGUKOOADataSource::TestCapability( CPL_UNUSED const char * pszCap )
{
    return FALSE;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRSEGUKOOADataSource::GetLayer( int iLayer )

{
    if( iLayer < 0 || iLayer >= nLayers )
        return NULL;
    else
        return papoLayers[iLayer];
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGRSEGUKOOADataSource::Open( const char * pszFilename )

{
    pszName = CPLStrdup( pszFilename );

    VSILFILE* fp = VSIFOpenL(pszFilename, "rb");
    if (fp == NULL)
        return FALSE;

    const char* pszLine;
    CPLPushErrorHandler(CPLQuietErrorHandler);
    pszLine = CPLReadLine2L(fp,81,NULL);
    CPLPopErrorHandler();
    CPLErrorReset();

    /* Both UKOOA P1/90 and SEG-P1 begins by a H character */
    if (pszLine == NULL || pszLine[0] != 'H')
    {
        VSIFCloseL(fp);
        return FALSE;
    }

// --------------------------------------------------------------------
//      Does this appear to be a UKOOA P1/90 file?
// --------------------------------------------------------------------

    if (strncmp(pszLine, "H0100 ", 6) == 0)
    {
        VSIFSeekL( fp, 0, SEEK_SET );

        VSILFILE* fp2 = VSIFOpenL(pszFilename, "rb");
        if (fp2 == NULL)
        {
            VSIFCloseL(fp);
            return FALSE;
        }

        nLayers = 2;
        papoLayers = (OGRLayer**) CPLMalloc(2 * sizeof(OGRLayer*));
        papoLayers[0] = new OGRUKOOAP190Layer(pszName, fp);
        papoLayers[1] = new OGRSEGUKOOALineLayer(pszName,
                                         new OGRUKOOAP190Layer(pszName, fp2));

        return TRUE;
    }

// --------------------------------------------------------------------
//      Does this appear to be a SEG-P1 file?
// --------------------------------------------------------------------

    /* Check first 20 header lines, and fetch the first point */
    for(int iLine = 0; iLine < 21; iLine ++)
    {
        const char* szPtr = pszLine;
        for(;*szPtr != '\0';szPtr++)
        {
            if (*szPtr != 9 && *szPtr < 32)
            {
                VSIFCloseL(fp);
                return FALSE;
            }
        }

        if (iLine == 20)
            break;

        CPLPushErrorHandler(CPLQuietErrorHandler);
        pszLine = CPLReadLine2L(fp,81,NULL);
        CPLPopErrorHandler();
        CPLErrorReset();
        if (pszLine == NULL)
        {
            VSIFCloseL(fp);
            return FALSE;
        }
    }

    char* pszExpandedLine = OGRSEGP1Layer::ExpandTabs(pszLine);
    int nLatitudeCol = OGRSEGP1Layer::DetectLatitudeColumn(pszExpandedLine);
    CPLFree(pszExpandedLine);

    if (nLatitudeCol > 0)
    {
        VSIFSeekL( fp, 0, SEEK_SET );

        VSILFILE* fp2 = VSIFOpenL(pszFilename, "rb");
        if (fp2 == NULL)
        {
            VSIFCloseL(fp);
            return FALSE;
        }

        nLayers = 2;
        papoLayers = (OGRLayer**) CPLMalloc(2 * sizeof(OGRLayer*));
        papoLayers[0] = new OGRSEGP1Layer(pszName, fp, nLatitudeCol);
        papoLayers[1] = new OGRSEGUKOOALineLayer(pszName,
                                         new OGRSEGP1Layer(pszName, fp2,
                                                           nLatitudeCol));

        return TRUE;
    }

    VSIFCloseL(fp);
    return FALSE;
}
