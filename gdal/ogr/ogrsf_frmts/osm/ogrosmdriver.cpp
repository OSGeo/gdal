/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGROSMDriver class.
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 ******************************************************************************
 * Copyright (c) 2012, Even Rouault
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

#include "ogr_osm.h"
#include "cpl_conv.h"

/* g++ -DHAVE_EXPAT -fPIC -g -Wall ogr/ogrsf_frmts/osm/ogrosmdriver.cpp ogr/ogrsf_frmts/osm/ogrosmdatasource.cpp ogr/ogrsf_frmts/osm/ogrosmlayer.cpp -Iport -Igcore -Iogr -Iogr/ogrsf_frmts/osm -Iogr/ogrsf_frmts/mitab -Iogr/ogrsf_frmts -shared -o ogr_OSM.so -L. -lgdal */

extern "C" void CPL_DLL RegisterOGROSM();

CPL_CVSID("$Id$");

/************************************************************************/
/*                         ~OGROSMDriver()                           */
/************************************************************************/

OGROSMDriver::~OGROSMDriver()

{
}

/************************************************************************/
/*                              GetName()                               */
/************************************************************************/

const char *OGROSMDriver::GetName()

{
    return "OSM";
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

OGRDataSource *OGROSMDriver::Open( const char * pszFilename,
                                   int bUpdate )

{
    if (bUpdate)
        return NULL;

    OGROSMDataSource   *poDS = new OGROSMDataSource();

    if( !poDS->Open( pszFilename, bUpdate ) )
    {
        delete poDS;
        poDS = NULL;
    }

    return poDS;
}

/************************************************************************/
/*                          CreateDataSource()                          */
/************************************************************************/

OGRDataSource *OGROSMDriver::CreateDataSource( const char * pszName,
                                               char **papszOptions )

{
    return NULL;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGROSMDriver::TestCapability( const char * pszCap )

{
    return FALSE;
}

/************************************************************************/
/*                        RegisterOGROSM()                           */
/************************************************************************/

void RegisterOGROSM()
{
    if (! GDAL_CHECK_VERSION("OGR/OSM driver"))
        return;

    OGRSFDriverRegistrar::GetRegistrar()->RegisterDriver( new OGROSMDriver );
}

