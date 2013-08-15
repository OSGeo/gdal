/******************************************************************************
 * $Id$
 *
 * Project:  OGR
 * Purpose:  OGRGMLDriver implementation
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2002, Frank Warmerdam <warmerdam@pobox.com>
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

#include "ogr_gml.h"
#include "cpl_conv.h"
#include "cpl_multiproc.h"
#include "gmlreaderp.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                          ~OGRGMLDriver()                           */
/************************************************************************/

OGRGMLDriver::~OGRGMLDriver()

{
    if( GMLReader::hMutex != NULL )
        CPLDestroyMutex( GMLReader::hMutex );
    GMLReader::hMutex = NULL;
}

/************************************************************************/
/*                              GetName()                               */
/************************************************************************/

const char *OGRGMLDriver::GetName()

{
    return "GML";
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

OGRDataSource *OGRGMLDriver::Open( const char * pszFilename,
                                   int bUpdate )

{
    OGRGMLDataSource    *poDS;

    if( bUpdate )
        return NULL;

    poDS = new OGRGMLDataSource();

    if( !poDS->Open( pszFilename, TRUE )
        || poDS->GetLayerCount() == 0 )
    {
        delete poDS;
        return NULL;
    }
    else
        return poDS;
}

/************************************************************************/
/*                          CreateDataSource()                          */
/************************************************************************/

OGRDataSource *OGRGMLDriver::CreateDataSource( const char * pszName,
                                               char **papszOptions )

{
    OGRGMLDataSource    *poDS = new OGRGMLDataSource();

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

int OGRGMLDriver::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,ODrCCreateDataSource) )
        return TRUE;
    else
        return FALSE;
}

/************************************************************************/
/*                           RegisterOGRGML()                           */
/************************************************************************/

void RegisterOGRGML()

{
    OGRSFDriverRegistrar::GetRegistrar()->RegisterDriver( new OGRGMLDriver );
}

