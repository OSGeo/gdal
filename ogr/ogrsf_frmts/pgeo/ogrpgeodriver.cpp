/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements Personal Geodatabase driver.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.2  2005/09/24 04:59:55  fwarmerdam
 * support DSN-less connection directly to .mdb files
 *
 * Revision 1.1  2005/09/05 19:34:17  fwarmerdam
 * New
 *
 */

#include "ogr_pgeo.h"
#include "cpl_conv.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                            ~OGRODBCDriver()                            */
/************************************************************************/

OGRPGeoDriver::~OGRPGeoDriver()

{
}

/************************************************************************/
/*                              GetName()                               */
/************************************************************************/

const char *OGRPGeoDriver::GetName()

{
    return "PGeo";
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

OGRDataSource *OGRPGeoDriver::Open( const char * pszFilename,
                                    int bUpdate )

{
    OGRPGeoDataSource     *poDS;

    if( !EQUALN(pszFilename,"PGEO:",5) 
        && !EQUAL(CPLGetExtension(pszFilename),"mdb") )
        return NULL;

    poDS = new OGRPGeoDataSource();

    if( !poDS->Open( pszFilename, bUpdate, TRUE ) )
    {
        delete poDS;
        return NULL;
    }
    else
        return poDS;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRPGeoDriver::TestCapability( const char * pszCap )

{
    return FALSE;
}

/************************************************************************/
/*                           RegisterOGRODBC()                          */
/************************************************************************/

void RegisterOGRPGeo()

{
    OGRSFDriverRegistrar::GetRegistrar()->RegisterDriver( new OGRPGeoDriver );
}

