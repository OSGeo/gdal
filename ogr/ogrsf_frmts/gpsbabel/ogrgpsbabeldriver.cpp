/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRGPSbabelDriver class.
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 ******************************************************************************
 * Copyright (c) 2010, Even Rouault
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

#include "ogr_gpsbabel.h"
#include "cpl_conv.h"

// g++ -g -Wall -fPIC  ogr/ogrsf_frmts/gpsbabel/*.cpp -shared -o ogr_GPSBabel.so -Iport -Igcore -Iogr -Iogr/ogrsf_frmts -Iogr/ogrsf_frmts/gpsbabel -L. -lgdal

CPL_CVSID("$Id$");

/************************************************************************/
/*                         ~OGRGPSBabelDriver()                           */
/************************************************************************/

OGRGPSBabelDriver::~OGRGPSBabelDriver()

{
}

/************************************************************************/
/*                              GetName()                               */
/************************************************************************/

const char *OGRGPSBabelDriver::GetName()

{
    return "GPSBabel";
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

OGRDataSource *OGRGPSBabelDriver::Open( const char * pszFilename,
                                   int bUpdate )

{
    if (bUpdate)
        return NULL;

    OGRGPSBabelDataSource   *poDS = new OGRGPSBabelDataSource();

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

/*
OGRDataSource *OGRGPSBabelDriver::CreateDataSource( const char * pszName,
                                               char **papszOptions )

{
    OGRGPSBabelWriteDataSource   *poDS = new OGRGPSBabelWriteDataSource();

    if( !poDS->Create( pszName, papszOptions ) )
    {
        delete poDS;
        poDS = NULL;
    }

    return poDS;
}
*/

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRGPSBabelDriver::TestCapability( const char * pszCap )

{
    /*if( EQUAL(pszCap,ODrCCreateDataSource) )
        return TRUE;
    else*/
        return FALSE;
}

/************************************************************************/
/*                        RegisterOGRGPSBabel()                         */
/************************************************************************/

void RegisterOGRGPSBabel()
{
    if (! GDAL_CHECK_VERSION("OGR/GPSBabel driver"))
        return;

    OGRSFDriverRegistrar::GetRegistrar()->RegisterDriver( new OGRGPSBabelDriver );
}

