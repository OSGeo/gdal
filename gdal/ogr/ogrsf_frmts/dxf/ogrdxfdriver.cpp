/******************************************************************************
 * $Id$
 *
 * Project:  DXF Translator
 * Purpose:  Implements OGRDXFDriver.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2009, Frank Warmerdam <warmerdam@pobox.com>
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

#include "ogr_dxf.h"
#include "cpl_conv.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                          ~OGRDXFDriver()                          */
/************************************************************************/

OGRDXFDriver::~OGRDXFDriver()

{
}

/************************************************************************/
/*                              GetName()                               */
/************************************************************************/

const char *OGRDXFDriver::GetName()

{
    return "DXF";
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

OGRDataSource *OGRDXFDriver::Open( const char * pszFilename, int bUpdate )

{
    OGRDXFDataSource   *poDS = new OGRDXFDataSource();

    if( !poDS->Open( pszFilename ) )
    {
        delete poDS;
        poDS = NULL;
    }

    return poDS;
}

/************************************************************************/
/*                          CreateDataSource()                          */
/************************************************************************/

OGRDataSource *OGRDXFDriver::CreateDataSource( const char * pszName,
                                               char **papszOptions )

{
    OGRDXFWriterDS *poDS = new OGRDXFWriterDS();

    if( poDS->Open( pszName, papszOptions ) )
        return poDS;
    else
    {
        delete poDS;
        return NULL;
    }
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRDXFDriver::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,ODrCCreateDataSource) )
        return TRUE;
    else
        return FALSE;
}

/************************************************************************/
/*                           RegisterOGRDXF()                           */
/************************************************************************/

void RegisterOGRDXF()

{
    OGRSFDriverRegistrar::GetRegistrar()->RegisterDriver( new OGRDXFDriver );
}

