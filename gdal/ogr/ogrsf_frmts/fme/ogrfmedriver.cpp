/******************************************************************************
 * $Id$
 *
 * Project:  FMEObjects Translator
 * Purpose:  Implementations of the OGRFMEDriver class.
 * Author:   Frank Warmerdam, warmerda@home.com
 *
 ******************************************************************************
 * Copyright (c) 1999, 2001 Safe Software Inc.
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#include "fme2ogr.h"
#include "cpl_error.h"

CPL_CVSID("$Id$");

/************************************************************************/
/* ==================================================================== */
/*                            OGRFMEDriver                              */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                           ~OGRFMEDriver()                            */
/************************************************************************/

OGRFMEDriver::~OGRFMEDriver()

{
}

/************************************************************************/
/*                              GetName()                               */
/************************************************************************/

const char *OGRFMEDriver::GetName()

{
    return "FMEObjects Gateway";
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRFMEDriver::TestCapability( const char * )

{
    return FALSE;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

OGRDataSource *OGRFMEDriver::Open( const char * pszFilename, int bUpdate )

{
    OGRFMEDataSource    *poDS = new OGRFMEDataSource;

    if( !poDS->Open( pszFilename ) )
    {
        delete poDS;
        return NULL;
    }

    if( bUpdate )
    {
        delete poDS;

        CPLError( CE_Failure, CPLE_OpenFailed,
                  "FMEObjects Driver doesn't support update." );
        return NULL;
    }
    
    return poDS;
}

/************************************************************************/
/*                           RegisterOGRFME()                           */
/************************************************************************/

void RegisterOGRFME()

{
    if (! GDAL_CHECK_VERSION("FME driver"))
        return;
    OGRSFDriverRegistrar::GetRegistrar()->RegisterDriver( new OGRFMEDriver );
}
