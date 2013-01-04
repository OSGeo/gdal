/******************************************************************************
 * $Id$
 *
 * Project:  Arc/Info Generate Translator
 * Purpose:  Implements OGRARCGENDataSource class
 * Author:   Even Rouault, even dot rouault at mines dash paris dot org
 *
 ******************************************************************************
 * Copyright (c) 2011, Even Rouault <even dot rouault at mines dash paris dot org>
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
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMARCGENS OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#include "ogr_arcgen.h"
#include "cpl_conv.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                          OGRARCGENDataSource()                          */
/************************************************************************/

OGRARCGENDataSource::OGRARCGENDataSource()

{
    papoLayers = NULL;
    nLayers = 0;

    pszName = NULL;
}

/************************************************************************/
/*                         ~OGRARCGENDataSource()                          */
/************************************************************************/

OGRARCGENDataSource::~OGRARCGENDataSource()

{
    for( int i = 0; i < nLayers; i++ )
        delete papoLayers[i];
    CPLFree( papoLayers );

    CPLFree( pszName );
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRARCGENDataSource::TestCapability( const char * pszCap )

{
    return FALSE;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRARCGENDataSource::GetLayer( int iLayer )

{
    if( iLayer < 0 || iLayer >= nLayers )
        return NULL;
    else
        return papoLayers[iLayer];
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGRARCGENDataSource::Open( const char * pszFilename, int bUpdateIn)

{
    if (bUpdateIn)
    {
        return FALSE;
    }

    pszName = CPLStrdup( pszFilename );

// -------------------------------------------------------------------- 
//      Does this appear to be a Arc/Info generate file?
// --------------------------------------------------------------------

    VSILFILE* fp = VSIFOpenL(pszFilename, "rb");
    if (fp == NULL)
        return FALSE;

    /* Check that the first line is compatible with a generate file */
    /* and in particular contain >= 32 && <= 127 bytes */
    char szFirstLine[256+1];
    int nRet = VSIFReadL(szFirstLine, 1, 256, fp);
    szFirstLine[nRet] = '\0';

    int i;
    int bFoundEOL = FALSE;
    for(i=0;szFirstLine[i] != '\0';i++)
    {
        if (szFirstLine[i] == '\n' || szFirstLine[i] == '\r')
        {
            bFoundEOL = TRUE;
            szFirstLine[i] = '\0';
            break;
        }
        if (szFirstLine[i] < 32)
        {
            VSIFCloseL(fp);
            return FALSE;
        }
    }

    if (!bFoundEOL)
    {
        VSIFCloseL(fp);
        return FALSE;
    }

    char** papszTokens = CSLTokenizeString2( szFirstLine, " ,", 0 );
    int nTokens = CSLCount(papszTokens);
    if (nTokens != 1 && nTokens != 3 && nTokens != 4)
    {
        VSIFCloseL(fp);
        CSLDestroy(papszTokens);
        return FALSE;
    }
    for(int i=0;i<nTokens;i++)
    {
        if( CPLGetValueType(papszTokens[i]) == CPL_VALUE_STRING )
        {
            VSIFCloseL(fp);
            CSLDestroy(papszTokens);
            return FALSE;
        }
    }
    CSLDestroy(papszTokens);

    /* Go to end of file, and count the number of END keywords */
    /* If there's 1, it's a point layer */
    /* If there's 2, it's a linestring or polygon layer */
    VSIFSeekL( fp, 0, SEEK_END );
    vsi_l_offset nSize = VSIFTellL(fp);
    if (nSize < 10)
    {
        VSIFCloseL(fp);
        return FALSE;
    }
    char szBuffer[10+1];
    VSIFSeekL( fp, nSize - 10, SEEK_SET );
    VSIFReadL( szBuffer, 1, 10, fp );
    szBuffer[10] = '\0';

    VSIFSeekL( fp, 0, SEEK_SET );

    OGRwkbGeometryType eType;
    const char* szPtr = szBuffer;
    const char* szEnd = strstr(szPtr, "END");
    if (szEnd == NULL) szEnd = strstr(szPtr, "end");
    if (szEnd == NULL)
    {
        VSIFCloseL(fp);
        return FALSE;
    }
    szPtr = szEnd + 3;
    szEnd = strstr(szPtr, "END");
    if (szEnd == NULL) szEnd = strstr(szPtr, "end");
    if (szEnd == NULL)
    {
        const char* pszLine = CPLReadLine2L(fp,256,NULL);
        if (pszLine == NULL)
        {
            VSIFCloseL(fp);
            return FALSE;
        }
        char** papszTokens = CSLTokenizeString2( pszLine, " ,", 0 );
        int nTokens = CSLCount(papszTokens);
        CSLDestroy(papszTokens);

        if (nTokens == 3)
            eType = wkbPoint;
        else if (nTokens == 4)
            eType = wkbPoint25D;
        else
        {
            VSIFCloseL(fp);
            return FALSE;
        }
    }
    else
    {
        int nLineNumber = 0;
        eType = wkbUnknown;
        CPLString osFirstX, osFirstY;
        CPLString osLastX, osLastY;
        int bIs3D = FALSE;
        const char* pszLine;
        while((pszLine = CPLReadLine2L(fp,256,NULL)) != NULL)
        {
            nLineNumber ++;
            if (nLineNumber == 2)
            {
                char** papszTokens = CSLTokenizeString2( pszLine, " ,", 0 );
                int nTokens = CSLCount(papszTokens);
                if (nTokens == 2 || nTokens == 3)
                {
                    if (nTokens == 3)
                        bIs3D = TRUE;
                    osFirstX = papszTokens[0];
                    osFirstY = papszTokens[1];
                }
                CSLDestroy(papszTokens);
                if (nTokens != 2 && nTokens != 3)
                    break;
            }
            else if (nLineNumber > 2)
            {
                if (EQUAL(pszLine, "END"))
                {
                    if (osFirstX.compare(osLastX) == 0 &&
                        osFirstY.compare(osLastY) == 0)
                        eType = (bIs3D) ? wkbPolygon25D : wkbPolygon;
                    else
                        eType = (bIs3D) ? wkbLineString25D : wkbLineString;
                    break;
                }

                char** papszTokens = CSLTokenizeString2( pszLine, " ,", 0 );
                int nTokens = CSLCount(papszTokens);
                if (nTokens == 2 || nTokens == 3)
                {
                    osLastX = papszTokens[0];
                    osLastY = papszTokens[1];
                }
                CSLDestroy(papszTokens);
                if (nTokens != 2 && nTokens != 3)
                    break;
            }
        }
        if (eType == wkbUnknown)
        {
            VSIFCloseL(fp);
            return FALSE;
        }
    }

    VSIFSeekL( fp, 0, SEEK_SET );

    nLayers = 1;
    papoLayers = (OGRLayer**) CPLMalloc(sizeof(OGRLayer*));
    papoLayers[0] = new OGRARCGENLayer(pszName, fp, eType);

    return TRUE;
}
