/******************************************************************************
 * $Id$
 *
 * Project:  KML Driver
 * Purpose:  Implementation of OGRKMLDriver class.
 * Author:   Christopher Condit, condit@sdsc.edu;
 *           Jens Oberender, j.obi@troja.net
 *
 ******************************************************************************
 * Copyright (c) 2006, Christopher Condit
 *               2007, Jens Oberender
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

OGRDataSource *OGRKMLDriver::Open( const char * pszName, int bUpdate )
{
    CPLAssert( NULL != pszName );

    OGRKMLDataSource* poDS = NULL;

#ifdef HAVE_EXPAT
    if( bUpdate )
        return NULL;

    poDS = new OGRKMLDataSource();

    if( poDS->Open( pszName, TRUE ) )
    {
        /*if( poDS->GetLayerCount() == 0 )
        {
            CPLError( CE_Failure, CPLE_OpenFailed, 
                "No layers in KML file: %s.", pszName );

            delete poDS;
            poDS = NULL;
        }*/
    }
    else
    {
        delete poDS;
        poDS = NULL;
    }
#endif

    return poDS;
}

/************************************************************************/
/*                          CreateDataSource()                          */
/************************************************************************/

OGRDataSource *OGRKMLDriver::CreateDataSource( const char* pszName,
                                               char** papszOptions )
{
    CPLAssert( NULL != pszName );
    CPLDebug( "KML", "Attempt to create: %s", pszName );
    
    OGRKMLDataSource *poDS = new OGRKMLDataSource();

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

int OGRKMLDriver::TestCapability( const char* pszCap )
{
    if( EQUAL(pszCap, ODrCCreateDataSource) )
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


