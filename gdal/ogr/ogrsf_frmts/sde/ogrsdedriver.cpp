/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRSDEDriver class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 * Copyright (c) 2008, Shawn Gervais <project10@project10.net>
 * Copyright (c) 2008, Howard Butler <hobu.inc@gmail.com>
 *
 ******************************************************************************
 * Copyright (c) 2005, Frank Warmerdam <warmerdam@pobox.com>
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

#include "ogr_sde.h"
#include "cpl_conv.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                            ~OGRSDEDriver()                            */
/************************************************************************/

OGRSDEDriver::~OGRSDEDriver()

{
}

/************************************************************************/
/*                              GetName()                               */
/************************************************************************/

const char *OGRSDEDriver::GetName()

{
    return "OGR_SDE";
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

OGRDataSource *OGRSDEDriver::Open( const char * pszFilename,
                                   int bUpdate )

{
    OGRSDEDataSource     *poDS;

    poDS = new OGRSDEDataSource();

    if( !poDS->Open( pszFilename, bUpdate ) )
    {
        delete poDS;
        return NULL;
    }
    else
        return poDS;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

OGRDataSource *OGRSDEDriver::CreateDataSource( const char * pszName,
                                               char **papszOptions)

{
    OGRSDEDataSource     *poDS;

    poDS = new OGRSDEDataSource();

    if( !poDS->Open( pszName, TRUE ) )
    {
        delete poDS;
        CPLError( CE_Failure, CPLE_AppDefined,
          "ArcSDE driver doesn't currently support database or service "
          "creation.  Please create the service before using.");
        return NULL;
    }
    else
        return poDS;
}
/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRSDEDriver::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap, ODsCCreateLayer) )
        return TRUE;
    if( EQUAL(pszCap, ODsCDeleteLayer) )
        return true;
    if( EQUAL(pszCap, ODrCCreateDataSource) )
        return TRUE;

    return FALSE;
}

/************************************************************************/
/*                           RegisterOGRSDE()                            */
/************************************************************************/

void RegisterOGRSDE()

{
    if( !GDAL_CHECK_VERSION("OGR SDE") )
        return;

    OGRSFDriverRegistrar::GetRegistrar()->RegisterDriver( new OGRSDEDriver );
}
