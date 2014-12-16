/******************************************************************************
 * $Id$
 *
 * Project:  CouchDB Translator
 * Purpose:  Implements OGRCouchDBDriver.
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
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#include "ogr_cloudant.h"

CPL_CVSID("$Id$");

extern "C" void RegisterOGRCloudant();

/************************************************************************/
/*                         ~OGRCloudantDriver()                          */
/************************************************************************/

OGRCloudantDriver::~OGRCloudantDriver()

{
}

/************************************************************************/
/*                              GetName()                               */
/************************************************************************/

const char *OGRCloudantDriver::GetName()

{
    return "Cloudant";
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

OGRDataSource *OGRCloudantDriver::Open( const char * pszFilename, int bUpdate )

{
    if (!EQUALN(pszFilename, "cloudant:", 9))
        return NULL;

    OGRCloudantDataSource   *poDS = new OGRCloudantDataSource();

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

OGRDataSource *OGRCloudantDriver::CreateDataSource( const char * pszName,
                                                   CPL_UNUSED char **papszOptions )
{
    OGRCloudantDataSource   *poDS = new OGRCloudantDataSource();

    if( !poDS->Open( pszName, TRUE ) )
    {
        delete poDS;
        poDS = NULL;
    }

    return poDS;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRCloudantDriver::TestCapability( const char * pszCap )

{
    if (EQUAL(pszCap, ODrCCreateDataSource))
        return TRUE;

    return FALSE;
}

/************************************************************************/
/*                         RegisterOGRCloudant()                         */
/************************************************************************/

void RegisterOGRCloudant()

{
    OGRSFDriver* poDriver = new OGRCloudantDriver;
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, "Cloudant / CouchDB" );
    OGRSFDriverRegistrar::GetRegistrar()->RegisterDriver( poDriver );
}
