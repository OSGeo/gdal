/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRDWGDriver class.
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
 ****************************************************************************/

#include "ogr_dwg.h"
#include "cpl_conv.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                            OGRDWGDriver()                            */
/************************************************************************/

OGRDWGDriver::OGRDWGDriver( const char *pszName )

{
    osOutClass = pszName;
}

/************************************************************************/
/*                            ~OGRDWGDriver()                            */
/************************************************************************/

OGRDWGDriver::~OGRDWGDriver()

{
}

/************************************************************************/
/*                              GetName()                               */
/************************************************************************/

const char *OGRDWGDriver::GetName()

{
    return osOutClass;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

OGRDataSource *OGRDWGDriver::Open( const char * pszFilename,
                                     int bUpdate )

{
    if( EQUAL(CPLGetExtension(pszFilename),"dxf") 
        || EQUAL(CPLGetExtension(pszFilename),"dwg") )
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "DXF/DWG reading not yet implemented." );
    
    return NULL;
}

/************************************************************************/
/*                          CreateDataSource()                          */
/************************************************************************/

OGRDataSource *OGRDWGDriver::CreateDataSource( const char * pszName,
                                              char **papszOptions )

{
    OGRWritableDWGDataSource     *poDS;

    poDS = new OGRWritableDWGDataSource( osOutClass );

    if( !poDS->Create( pszName, papszOptions ) )
    {
        delete poDS;
        poDS = NULL;
    }

    return poDS;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRDWGDriver::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,ODrCCreateDataSource) )
        return TRUE;
    else
        return FALSE;
}

/************************************************************************/
/*                           RegisterOGRDXFDWG()                        */
/************************************************************************/

void RegisterOGRDXFDWG()

{
    OGRSFDriverRegistrar::GetRegistrar()->RegisterDriver( 
        new OGRDWGDriver( "DWG" ) );
    OGRSFDriverRegistrar::GetRegistrar()->RegisterDriver( 
        new OGRDWGDriver( "DXF" ) );
}

