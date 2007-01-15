/******************************************************************************
 * $Id$
 *
 * Project:  UK NTF Reader
 * Purpose:  Implements OGRNTFDriver
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
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
 * Revision 1.5  2006/03/28 22:59:55  fwarmerdam
 * updated contact info
 *
 * Revision 1.4  2001/07/18 04:55:16  warmerda
 * added CPL_CSVID
 *
 * Revision 1.3  2001/01/19 20:31:12  warmerda
 * expand tabs
 *
 * Revision 1.2  1999/11/04 21:11:52  warmerda
 * Added TestCapability().
 *
 * Revision 1.1  1999/08/28 03:13:35  warmerda
 * New
 *
 */

#include "ntf.h"
#include "cpl_conv.h"

CPL_CVSID("$Id$");

/************************************************************************/
/* ==================================================================== */
/*                            OGRNTFDriver                              */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                           ~OGRNTFDriver()                            */
/************************************************************************/

OGRNTFDriver::~OGRNTFDriver()

{
}

/************************************************************************/
/*                              GetName()                               */
/************************************************************************/

const char *OGRNTFDriver::GetName()

{
    return "UK .NTF";
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRNTFDriver::TestCapability( const char * )

{
    return FALSE;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

OGRDataSource *OGRNTFDriver::Open( const char * pszFilename, int bUpdate )

{
    OGRNTFDataSource    *poDS = new OGRNTFDataSource;

    if( !poDS->Open( pszFilename, TRUE ) )
    {
        delete poDS;
        poDS = NULL;
    }

    if( poDS != NULL && bUpdate )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "NTF Driver doesn't support update." );
        delete poDS;
        poDS = NULL;
    }
    
    return poDS;
}

/************************************************************************/
/*                           RegisterOGRNTF()                           */
/************************************************************************/

void RegisterOGRNTF()

{
    OGRSFDriverRegistrar::GetRegistrar()->RegisterDriver( new OGRNTFDriver );
}

