/******************************************************************************
 * $Id$
 *
 * Project:  Google Maps Engine API Driver
 * Purpose:  OGRGMEDriver Implementation.
 * Author:   Frank Warmerdam <warmerdam@pobox.com>
 *           (derived from GFT driver by Even)
 *
 ******************************************************************************
 * Copyright (c) 2013, Frank Warmerdam <warmerdam@pobox.com>
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

#include "ogr_gme.h"

CPL_CVSID("$Id$");

extern "C" void RegisterOGRGME();

/************************************************************************/
/*                           ~OGRGMEDriver()                            */
/************************************************************************/

OGRGMEDriver::~OGRGMEDriver()

{
}

/************************************************************************/
/*                              GetName()                               */
/************************************************************************/

const char *OGRGMEDriver::GetName()

{
    return "GME";
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

OGRDataSource *OGRGMEDriver::Open( const char * pszFilename, int bUpdate )

{
    OGRGMEDataSource   *poDS = new OGRGMEDataSource();

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

OGRDataSource *OGRGMEDriver::CreateDataSource( const char * pszName,
                                               char **papszOptions )

{
    OGRGMEDataSource   *poDS = new OGRGMEDataSource();

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

int OGRGMEDriver::TestCapability( const char * pszCap )

{
    if (EQUAL(pszCap, ODrCCreateDataSource))
        return TRUE;

    return FALSE;
}

/************************************************************************/
/*                           RegisterOGRGME()                           */
/************************************************************************/

void RegisterOGRGME()

{
    OGRSFDriverRegistrar::GetRegistrar()->RegisterDriver( new OGRGMEDriver );
}
