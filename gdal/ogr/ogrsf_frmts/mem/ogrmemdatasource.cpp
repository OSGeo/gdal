/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRMemDataSource class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
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

#include "cpl_port.h"
#include "ogr_mem.h"

#include "cpl_conv.h"
#include "cpl_string.h"
#include "ogr_core.h"
#include "ogr_spatialref.h"
#include "ogrsf_frmts.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                          OGRMemDataSource()                          */
/************************************************************************/

OGRMemDataSource::OGRMemDataSource( const char *pszFilename,
                                    char ** /* papszOptions */ ) :
    papoLayers(nullptr),
    nLayers(0),
    pszName(CPLStrdup(pszFilename))
{}

/************************************************************************/
/*                         ~OGRMemDataSource()                          */
/************************************************************************/

OGRMemDataSource::~OGRMemDataSource()

{
    CPLFree(pszName);

    for( int i = 0; i < nLayers; i++ )
        delete papoLayers[i];

    CPLFree(papoLayers);
}

/************************************************************************/
/*                           ICreateLayer()                             */
/************************************************************************/

OGRLayer *
OGRMemDataSource::ICreateLayer( const char *pszLayerName,
                                OGRSpatialReference *poSRSIn,
                                OGRwkbGeometryType eType,
                                char **papszOptions )
{
    // Create the layer object.
    OGRSpatialReference* poSRS = poSRSIn;
    if( poSRS )
    {
        poSRS = poSRS->Clone();
        poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    }
    OGRMemLayer *poLayer = new OGRMemLayer(pszLayerName, poSRS, eType);
    if( poSRS )
    {
        poSRS->Release();
    }

    if( CPLFetchBool(papszOptions, "ADVERTIZE_UTF8", false) )
        poLayer->SetAdvertizeUTF8(true);

    // Add layer to data source layer list.
    papoLayers = static_cast<OGRMemLayer **>(
        CPLRealloc(papoLayers, sizeof(OGRMemLayer *) * (nLayers + 1)));

    papoLayers[nLayers++] = poLayer;

    return poLayer;
}

/************************************************************************/
/*                            DeleteLayer()                             */
/************************************************************************/

OGRErr OGRMemDataSource::DeleteLayer( int iLayer )

{
    if( iLayer >= 0 && iLayer < nLayers )
    {
        delete papoLayers[iLayer];

        for( int i = iLayer + 1; i < nLayers; ++i )
            papoLayers[i - 1] = papoLayers[i];

        --nLayers;

        return OGRERR_NONE;
    }

    return OGRERR_FAILURE;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRMemDataSource::TestCapability( const char *pszCap )

{
    if( EQUAL(pszCap, ODsCCreateLayer) )
        return TRUE;
    else if( EQUAL(pszCap, ODsCDeleteLayer) )
        return TRUE;
    else if( EQUAL(pszCap, ODsCCreateGeomFieldAfterCreateLayer) )
        return TRUE;
    else if( EQUAL(pszCap, ODsCCurveGeometries) )
        return TRUE;
    else if( EQUAL(pszCap, ODsCMeasuredGeometries) )
        return TRUE;
    else if( EQUAL(pszCap, ODsCRandomLayerWrite) )
        return TRUE;
    else if( EQUAL(pszCap, ODsCAddFieldDomain) )
        return TRUE;

    return FALSE;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRMemDataSource::GetLayer( int iLayer )

{
    if( iLayer < 0 || iLayer >= nLayers )
        return nullptr;

    return papoLayers[iLayer];
}

/************************************************************************/
/*                           AddFieldDomain()                           */
/************************************************************************/

bool OGRMemDataSource::AddFieldDomain(std::unique_ptr<OGRFieldDomain>&& domain,
                                      std::string& failureReason)
{
    if( GetFieldDomain(domain->GetName()) != nullptr )
    {
        failureReason = "A domain of identical name already exists";
        return false;
    }
    const auto domainName = domain->GetName();
    m_oMapFieldDomains[domainName] = std::move(domain);
    return true;
}
