/******************************************************************************
 * $Id$
 *
 * Project:  GTM Driver
 * Purpose:  Implementation of OGRGTMDriver class.
 * Author:   Leonardo de Paula Rosa Piga; http://lampiao.lsc.ic.unicamp.br/~piga
 *
 ******************************************************************************
 * Copyright (c) 2009, Leonardo de Paula Rosa Piga
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
#include "ogr_gtm.h"
#include "cpl_conv.h"
#include "cpl_error.h"

/************************************************************************/
/*                          ~OGRGTMDriver()                           */
/************************************************************************/

OGRGTMDriver::~OGRGTMDriver()
{
}

/************************************************************************/
/*                              GetName()                               */
/************************************************************************/

const char *OGRGTMDriver::GetName()
{
    return "GPSTrackMaker";
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

OGRDataSource *OGRGTMDriver::Open( const char * pszName, int bUpdate )
{
    if (bUpdate)
    {
        return NULL;
    }
    
    OGRGTMDataSource *poDS = new OGRGTMDataSource();

    if( !poDS->Open( pszName, bUpdate ) )
    {
        delete poDS;
        poDS = NULL;
    }
    return poDS;
}

/************************************************************************/
/*                          CreateDataSource()                          */
/************************************************************************/

OGRDataSource *OGRGTMDriver::CreateDataSource( const char* pszName,
                                               char** papszOptions )
{
    CPLAssert( NULL != pszName );
    CPLDebug( "GTM", "Attempt to create: %s", pszName );
    
    OGRGTMDataSource *poDS = new OGRGTMDataSource();

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

int OGRGTMDriver::TestCapability( const char* pszCap )
{
    if( EQUAL(pszCap, ODrCCreateDataSource) )
        return TRUE;
    else
        return FALSE;
}

/************************************************************************/
/*                           RegisterOGRGTM()                           */
/************************************************************************/

void RegisterOGRGTM()
{
    OGRSFDriverRegistrar::GetRegistrar()->RegisterDriver( new OGRGTMDriver );
}


