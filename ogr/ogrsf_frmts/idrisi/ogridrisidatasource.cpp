/******************************************************************************
 *
 * Project:  Idrisi Translator
 * Purpose:  Implements OGRIdrisiDataSource class
 * Author:   Even Rouault, even dot rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2011-2012, Even Rouault <even dot rouault at spatialys.com>
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

#include "cpl_conv.h"
#include "cpl_string.h"
#include "idrisi.h"
#include "ogr_idrisi.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                        OGRIdrisiDataSource()                         */
/************************************************************************/

OGRIdrisiDataSource::OGRIdrisiDataSource() :
    pszName(nullptr),
    papoLayers(nullptr),
    nLayers(0)
{}

/************************************************************************/
/*                       ~OGRIdrisiDataSource()                         */
/************************************************************************/

OGRIdrisiDataSource::~OGRIdrisiDataSource()

{
    CPLFree( pszName );
    for( int i = 0; i < nLayers; i++ )
        delete papoLayers[i];
    CPLFree( papoLayers );
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRIdrisiDataSource::TestCapability( const char * /* pszCap */ )
{
    return FALSE;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRIdrisiDataSource::GetLayer( int iLayer )

{
    if( iLayer < 0 || iLayer >= nLayers )
        return nullptr;

    return papoLayers[iLayer];
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGRIdrisiDataSource::Open( const char * pszFilename )

{
    pszName = CPLStrdup( pszFilename );

    VSILFILE* fpVCT = VSIFOpenL(pszFilename, "rb");
    if (fpVCT == nullptr)
        return FALSE;

// --------------------------------------------------------------------
//      Look for .vdc file
// --------------------------------------------------------------------
    const char* pszVDCFilename = CPLResetExtension(pszFilename, "vdc");
    VSILFILE* fpVDC = VSIFOpenL(pszVDCFilename, "rb");
    if (fpVDC == nullptr)
    {
        pszVDCFilename = CPLResetExtension(pszFilename, "VDC");
        fpVDC = VSIFOpenL(pszVDCFilename, "rb");
    }

    char** papszVDC = nullptr;
    if (fpVDC != nullptr)
    {
        VSIFCloseL(fpVDC);
        fpVDC = nullptr;

        CPLPushErrorHandler(CPLQuietErrorHandler);
        papszVDC = CSLLoad2(pszVDCFilename, 1024, 256, nullptr);
        CPLPopErrorHandler();
        CPLErrorReset();
    }

    OGRwkbGeometryType eType = wkbUnknown;

    char* pszWTKString = nullptr;
    if (papszVDC != nullptr)
    {
        CSLSetNameValueSeparator( papszVDC, ":" );

        const char *pszVersion = CSLFetchNameValue( papszVDC, "file format" );

        if( pszVersion == nullptr || !EQUAL( pszVersion, "IDRISI Vector A.1" ) )
        {
            CSLDestroy( papszVDC );
            VSIFCloseL(fpVCT);
            return FALSE;
        }

        const char *pszRefSystem
            = CSLFetchNameValue( papszVDC, "ref. system" );
        const char *pszRefUnits = CSLFetchNameValue( papszVDC, "ref. units" );

        if (pszRefSystem != nullptr && pszRefUnits != nullptr)
            IdrisiGeoReference2Wkt( pszFilename, pszRefSystem, pszRefUnits,
                                    &pszWTKString);
    }

    GByte chType = 0;
    if (VSIFReadL(&chType, 1, 1, fpVCT) != 1)
    {
        VSIFCloseL(fpVCT);
        CSLDestroy( papszVDC );
        CPLFree(pszWTKString);
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
        CPLError( CE_Failure, CPLE_AppDefined, "Unsupported geometry type : %d",
                  static_cast<int>(chType) );
        VSIFCloseL(fpVCT);
        CSLDestroy( papszVDC );
        CPLFree(pszWTKString);
        return FALSE;
    }

    const char *pszMinX = CSLFetchNameValue( papszVDC, "min. X" );
    const char *pszMaxX = CSLFetchNameValue( papszVDC, "max. X" );
    const char *pszMinY = CSLFetchNameValue( papszVDC, "min. Y" );
    const char *pszMaxY = CSLFetchNameValue( papszVDC, "max. Y" );

    OGRIdrisiLayer* poLayer = new OGRIdrisiLayer(pszFilename,
                                                 CPLGetBasename(pszFilename),
                                                 fpVCT, eType, pszWTKString);
    papoLayers = static_cast<OGRLayer**>( CPLMalloc(sizeof(OGRLayer*)) );
    papoLayers[nLayers ++] = poLayer;

    if( pszMinX != nullptr && pszMaxX != nullptr && pszMinY != nullptr &&
        pszMaxY != nullptr)
    {
        poLayer->SetExtent(
            CPLAtof(pszMinX), CPLAtof(pszMinY), CPLAtof(pszMaxX),
            CPLAtof(pszMaxY) );
    }

    CPLFree(pszWTKString);

    CSLDestroy( papszVDC );

    return TRUE;
}
