/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRMemDriver class.
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
#include "gdal.h"
#include "ogr_core.h"
#include "ogrsf_frmts.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                          ~OGRMemDriver()                             */
/************************************************************************/

OGRMemDriver::~OGRMemDriver() {}

/************************************************************************/
/*                              GetName()                               */
/************************************************************************/

const char *OGRMemDriver::GetName() { return "Memory"; }

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

OGRDataSource *OGRMemDriver::Open( const char * /* pszFilename */, int )
{
    return nullptr;
}

/************************************************************************/
/*                          CreateDataSource()                          */
/************************************************************************/

OGRDataSource *OGRMemDriver::CreateDataSource( const char *pszName,
                                               char **papszOptions )

{
    return new OGRMemDataSource(pszName, papszOptions);
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRMemDriver::TestCapability( const char *pszCap )

{
    if( EQUAL(pszCap, ODrCCreateDataSource) )
        return TRUE;

    return FALSE;
}

/************************************************************************/
/*                           RegisterOGRMem()                           */
/************************************************************************/

void RegisterOGRMEM()

{
    if( GDALGetDriverByName("Memory") != nullptr )
        return;

    OGRSFDriver *poDriver = new OGRMemDriver;

    poDriver->SetMetadataItem(
        GDAL_DMD_CREATIONFIELDDATATYPES,
        "Integer Integer64 Real String Date DateTime Time IntegerList "
        "Integer64List RealList StringList Binary");

    poDriver->SetMetadataItem(
        GDAL_DS_LAYER_CREATIONOPTIONLIST,
        "<LayerCreationOptionList>"
        "  <Option name='ADVERTIZE_UTF8' type='boolean' description='Whether "
        "the layer will contain UTF-8 strings' default='NO'/>"
        "</LayerCreationOptionList>");

    poDriver->SetMetadataItem( GDAL_DCAP_COORDINATE_EPOCH, "YES" );
    poDriver->SetMetadataItem( GDAL_DCAP_MULTIPLE_VECTOR_LAYERS, "YES" );

    poDriver->SetMetadataItem( GDAL_DCAP_FIELD_DOMAINS, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_CREATION_FIELD_DOMAIN_TYPES, "Coded Range Glob" );

    poDriver->SetMetadataItem( GDAL_DMD_ALTER_GEOM_FIELD_DEFN_FLAGS, "Name Type Nullable SRS CoordinateEpoch" );

    OGRSFDriverRegistrar::GetRegistrar()->RegisterDriver(poDriver);
}
