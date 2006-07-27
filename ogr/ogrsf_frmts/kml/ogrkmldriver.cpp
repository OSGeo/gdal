/******************************************************************************
 * $Id$
 *
 * Project:  KML Driver
 * Purpose:  Implementation of OGRKMLDriver class.
 * Author:   Christopher Condit, condit@sdsc.edu
 *
 ******************************************************************************
 * Copyright (c) 2006, Christopher Condit
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
 * Revision 1.2  2006/07/27 19:53:01  mloskot
 * Added common file header to KML driver source files.
 *
 *
 */
#include "ogr_kml.h"
#include "cpl_conv.h"
#include "cpl_error.h"

/************************************************************************/
/*                          ~OGRKMLDriver()                           */
/************************************************************************/
OGRKMLDriver::~OGRKMLDriver()
{
}

/************************************************************************/
/*                              GetName()                               */
/************************************************************************/
const char *OGRKMLDriver::GetName()
{
    return "KML";
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/
OGRDataSource *OGRKMLDriver::Open( const char * pszFilename,
                                   int bUpdate )
{
    CPLAssert( NULL != pszFilename );
    CPLDebug( "KML", "Attempt to open: %s", pszFilename );
    
    OGRKMLDataSource    *poDS = NULL;

    if( bUpdate )
        return NULL;

    poDS = new OGRKMLDataSource();

    if( !poDS->Open( pszFilename, TRUE )
        || poDS->GetLayerCount() == 0 )
    {
        delete poDS;
        return NULL;
    }
    
    return poDS;
}

/************************************************************************/
/*                          CreateDataSource()                          */
/************************************************************************/
OGRDataSource *OGRKMLDriver::CreateDataSource( const char * pszName,
                                               char **papszOptions )
{
    CPLAssert( NULL != pszName );
    CPLDebug( "KML", "Attempt to create: %s", pszName );
    
    OGRKMLDataSource *poDS = new OGRKMLDataSource();

    if( !poDS->Create( pszName, papszOptions ) )
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
int OGRKMLDriver::TestCapability( const char * pszCap )
{
    if( EQUAL(pszCap,ODrCCreateDataSource) )
        return TRUE;
    else
        return FALSE;
}

/************************************************************************/
/*                           RegisterOGRKML()                           */
/************************************************************************/
void RegisterOGRKML()
{
    OGRSFDriverRegistrar::GetRegistrar()->RegisterDriver( new OGRKMLDriver );
}

