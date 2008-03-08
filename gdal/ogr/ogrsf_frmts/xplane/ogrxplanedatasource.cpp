/******************************************************************************
 * $Id: ogrxplanedatasource.cpp
 *
 * Project:  X-Plane aeronautical data reader
 * Purpose:  Implements OGRXPlaneDataSource class
 * Author:   Even Rouault, even dot rouault at mines dash paris dot org
 *
 ******************************************************************************
 * Copyright (c) 2008, Even Rouault
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

#include "ogr_xplane.h"
#include "ogr_xplane_reader.h"

/************************************************************************/
/*                          OGRXPlaneDataSource()                          */
/************************************************************************/

OGRXPlaneDataSource::OGRXPlaneDataSource()

{
    pszName = NULL;
    papoLayers = NULL;
    nLayers = 0;
}

/************************************************************************/
/*                         ~OGRXPlaneDataSource()                          */
/************************************************************************/

OGRXPlaneDataSource::~OGRXPlaneDataSource()

{
    CPLFree( pszName );
    for( int i = 0; i < nLayers; i++ )
        delete papoLayers[i];
    CPLFree( papoLayers );
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRXPlaneDataSource::GetLayer( int iLayer )

{
    if( iLayer < 0 || iLayer >= nLayers )
        return NULL;
    else
        return papoLayers[iLayer];
}

/************************************************************************/
/*                           RegisterLayer()                            */
/************************************************************************/

void OGRXPlaneDataSource::RegisterLayer(OGRXPlaneLayer* poLayer)
{
    papoLayers = (OGRXPlaneLayer**) CPLRealloc(papoLayers,
                                    (nLayers + 1) * sizeof(OGRXPlaneLayer*));
    papoLayers[nLayers++] = poLayer;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGRXPlaneDataSource::Open( const char * pszFilename )

{
    const char* pszShortFilename = CPLGetFilename(pszFilename);
    if (EQUAL(pszShortFilename, "nav.dat"))
    {
        int bRet = OGRXPlaneParseNavFile(this, pszFilename);
        if (bRet)
            pszName = CPLStrdup(pszFilename);
        return bRet;
    }
    else if (EQUAL(pszShortFilename, "apt.dat"))
    {
        int bRet = OGRXPlaneParseAptFile(this, pszFilename);
        if (bRet)
            pszName = CPLStrdup(pszFilename);
        return bRet;
    }
    else
    {
        return FALSE;
    }
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRXPlaneDataSource::TestCapability( const char * pszCap )

{
    return FALSE;
}
