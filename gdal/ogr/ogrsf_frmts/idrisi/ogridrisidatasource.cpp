/******************************************************************************
 * $Id$
 *
 * Project:  Idrisi Translator
 * Purpose:  Implements OGRIdrisiDataSource class
 * Author:   Even Rouault, even dot rouault at mines dash paris dot org
 *
 ******************************************************************************
 * Copyright (c) 2011-2012, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "ogr_idrisi.h"
#include "cpl_conv.h"
#include "cpl_string.h"
#include "idrisi.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                        OGRIdrisiDataSource()                         */
/************************************************************************/

OGRIdrisiDataSource::OGRIdrisiDataSource()

{
    papoLayers = NULL;
    nLayers = 0;

    pszName = NULL;
}

/************************************************************************/
/*                       ~OGRIdrisiDataSource()                         */
/************************************************************************/

OGRIdrisiDataSource::~OGRIdrisiDataSource()

{
    for( int i = 0; i < nLayers; i++ )
        delete papoLayers[i];
    CPLFree( papoLayers );

    CPLFree( pszName );
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRIdrisiDataSource::TestCapability( CPL_UNUSED const char * pszCap )
{
    return FALSE;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRIdrisiDataSource::GetLayer( int iLayer )

{
    if( iLayer < 0 || iLayer >= nLayers )
        return NULL;
    else
        return papoLayers[iLayer];
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGRIdrisiDataSource::Open( const char * pszFilename, int bUpdateIn)

{
    if (bUpdateIn)
    {
        return FALSE;
    }

// --------------------------------------------------------------------
//      Does this appear to be a .vct file?
// --------------------------------------------------------------------
    if ( !EQUAL(CPLGetExtension(pszFilename), "vct") )
        return FALSE;

    pszName = CPLStrdup( pszFilename );

    VSILFILE* fpVCT = VSIFOpenL(pszFilename, "rb");
    if (fpVCT == NULL)
        return FALSE;

    char* pszWTKString = NULL;

// --------------------------------------------------------------------
//      Look for .vdc file
// --------------------------------------------------------------------
    const char* pszVDCFilename = CPLResetExtension(pszFilename, "vdc");
    VSILFILE* fpVDC = VSIFOpenL(pszVDCFilename, "rb");
    if (fpVDC == NULL)
    {
        pszVDCFilename = CPLResetExtension(pszFilename, "VDC");
        fpVDC = VSIFOpenL(pszVDCFilename, "rb");
    }

    char** papszVDC = NULL;
    if (fpVDC != NULL)
    {
        VSIFCloseL(fpVDC);
        fpVDC = NULL;

        CPLPushErrorHandler(CPLQuietErrorHandler);
        papszVDC = CSLLoad2(pszVDCFilename, 1024, 256, NULL);
        CPLPopErrorHandler();
        CPLErrorReset();
    }

    OGRwkbGeometryType eType = wkbUnknown;

    if (papszVDC != NULL)
    {
        CSLSetNameValueSeparator( papszVDC, ":" );

        const char *pszVersion = CSLFetchNameValue( papszVDC, "file format " );

        if( pszVersion == NULL || !EQUAL( pszVersion, "IDRISI Vector A.1" ) )
        {
            CSLDestroy( papszVDC );
            VSIFCloseL(fpVCT);
            return FALSE;
        }

        const char *pszRefSystem  = CSLFetchNameValue( papszVDC, "ref. system " );
        const char *pszRefUnits   = CSLFetchNameValue( papszVDC, "ref. units  " );

        if (pszRefSystem != NULL && pszRefUnits != NULL)
            IdrisiGeoReference2Wkt( pszFilename, pszRefSystem, pszRefUnits, &pszWTKString);
    }

    GByte chType;
    if (VSIFReadL(&chType, 1, 1, fpVCT) != 1)
    {
        VSIFCloseL(fpVCT);
        CSLDestroy( papszVDC );
        return FALSE;
    }

    if (chType == 1)
        eType = wkbPoint;
    else if (chType == 2)
        eType = wkbLineString;
    else if (chType == 3)
        eType = wkbPolygon;
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Unsupport geometry type : %d",
                    (int)chType);
        VSIFCloseL(fpVCT);
        CSLDestroy( papszVDC );
        return FALSE;
    }

    const char *pszMinX       = CSLFetchNameValue( papszVDC, "min. X      " );
    const char *pszMaxX       = CSLFetchNameValue( papszVDC, "max. X      " );
    const char *pszMinY       = CSLFetchNameValue( papszVDC, "min. Y      " );
    const char *pszMaxY       = CSLFetchNameValue( papszVDC, "max. Y      " );

    OGRIdrisiLayer* poLayer = new OGRIdrisiLayer(pszFilename,
                                                 CPLGetBasename(pszFilename),
                                                 fpVCT, eType, pszWTKString);
    papoLayers = (OGRLayer**) CPLMalloc(sizeof(OGRLayer*));
    papoLayers[nLayers ++] = poLayer;

    if (pszMinX != NULL && pszMaxX != NULL && pszMinY != NULL && pszMaxY != NULL)
    {
        poLayer->SetExtent(CPLAtof(pszMinX), CPLAtof(pszMinY), CPLAtof(pszMaxX), CPLAtof(pszMaxY));
    }

    CPLFree(pszWTKString);

    CSLDestroy( papszVDC );

    return TRUE;
}
