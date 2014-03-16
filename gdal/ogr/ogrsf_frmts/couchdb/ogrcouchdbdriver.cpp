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

#include "ogr_couchdb.h"

// g++ -g -Wall -fPIC -shared -o ogr_CouchDB.so -Iport -Igcore -Iogr -Iogr/ogrsf_frmts -Iogr/ogrsf_frmts/couchdb ogr/ogrsf_frmts/couchdb/*.c* -L. -lgdal -Iogr/ogrsf_frmts/geojson/jsonc

CPL_CVSID("$Id$");

extern "C" void RegisterOGRCouchDB();

/************************************************************************/
/*                         ~OGRCouchDBDriver()                          */
/************************************************************************/

OGRCouchDBDriver::~OGRCouchDBDriver()

{
}

/************************************************************************/
/*                              GetName()                               */
/************************************************************************/

const char *OGRCouchDBDriver::GetName()

{
    return "CouchDB";
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

OGRDataSource *OGRCouchDBDriver::Open( const char * pszFilename, int bUpdate )

{
    OGRCouchDBDataSource   *poDS = new OGRCouchDBDataSource();

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

OGRDataSource *OGRCouchDBDriver::CreateDataSource( const char * pszName,
                                               char **papszOptions )

{
    OGRCouchDBDataSource   *poDS = new OGRCouchDBDataSource();

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

int OGRCouchDBDriver::TestCapability( const char * pszCap )

{
    if (EQUAL(pszCap, ODrCCreateDataSource))
        return TRUE;

    return FALSE;
}

/************************************************************************/
/*                         RegisterOGRCouchDB()                         */
/************************************************************************/

void RegisterOGRCouchDB()

{
    OGRSFDriverRegistrar::GetRegistrar()->RegisterDriver( new OGRCouchDBDriver );
}

