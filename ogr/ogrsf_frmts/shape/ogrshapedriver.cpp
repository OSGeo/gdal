/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRShapeDriver class.
 * Author:   Frank Warmerdam, warmerda@home.com
 *
 ******************************************************************************
 * Copyright (c) 1999,  Les Technologies SoftMap Inc.
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
 * Revision 1.3  2001/07/18 04:55:16  warmerda
 * added CPL_CSVID
 *
 * Revision 1.2  2000/01/26 22:06:32  warmerda
 * add directory creation, fix open of empty dir
 *
 * Revision 1.1  1999/11/04 21:16:11  warmerda
 * New
 *
 */

#include "ogrshape.h"
#include "cpl_conv.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                          ~OGRShapeDriver()                           */
/************************************************************************/

OGRShapeDriver::~OGRShapeDriver()

{
}

/************************************************************************/
/*                              GetName()                               */
/************************************************************************/

const char *OGRShapeDriver::GetName()

{
    return "ESRI Shapefile";
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

OGRDataSource *OGRShapeDriver::Open( const char * pszFilename,
                                     int bUpdate )

{
    OGRShapeDataSource	*poDS;

    poDS = new OGRShapeDataSource();

    if( !poDS->Open( pszFilename, bUpdate, TRUE )
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

OGRDataSource *OGRShapeDriver::CreateDataSource( const char * pszName,
                                                 char ** /* papszOptions */ )

{
    VSIStatBuf	stat;

/* -------------------------------------------------------------------- */
/*      Verify that the target is a valid directory.                    */
/* -------------------------------------------------------------------- */
    if( VSIStat( pszName, &stat ) == 0 )
    {
        if( !VSI_ISDIR(stat.st_mode) )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "%s is not a directory.\n",
                      pszName );
            
            return NULL;
        }
    }
    else
    {
        if( VSIMkdir( pszName, 0755 ) != 0 )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Failed to create directory %s\n"
                      "for shapefile datastore.\n",
                      pszName );
            
            return NULL;
        }
    }

/* -------------------------------------------------------------------- */
/*      Return a new OGRDataSource()                                    */
/* -------------------------------------------------------------------- */
    OGRShapeDataSource	*poDS = new OGRShapeDataSource();

    if( !poDS->Open( pszName, TRUE, FALSE ) )
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

int OGRShapeDriver::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,ODrCCreateDataSource) )
        return TRUE;
    else
        return FALSE;
}

/************************************************************************/
/*                          RegisterOGRShape()                          */
/************************************************************************/

void RegisterOGRShape()

{
    OGRSFDriverRegistrar::GetRegistrar()->RegisterDriver( new OGRShapeDriver );
}

