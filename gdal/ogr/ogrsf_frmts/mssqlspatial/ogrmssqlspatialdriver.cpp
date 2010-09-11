/******************************************************************************
 * $Id$
 *
 * Project:  MSSQL Spatial driver
 * Purpose:  Definition of classes for OGR MSSQL Spatial driver.
 * Author:   Tamas Szekeres, szekerest at gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2010, Tamas Szekeres
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

#include "ogr_mssqlspatial.h"
#include "cpl_conv.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                           ~OGRMSSQLSpatialDriver()                   */
/************************************************************************/

OGRMSSQLSpatialDriver::~OGRMSSQLSpatialDriver()

{
}

/************************************************************************/
/*                              GetName()                               */
/************************************************************************/

const char *OGRMSSQLSpatialDriver::GetName()

{
    return "MSSQLSpatial";
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

OGRDataSource *OGRMSSQLSpatialDriver::Open( const char * pszFilename, int bUpdate )

{
    OGRMSSQLSpatialDataSource     *poDS;

    if( !EQUALN(pszFilename,"MSSQL:",6) )
        return NULL;

    poDS = new OGRMSSQLSpatialDataSource();

    if( !poDS->Open( pszFilename, bUpdate, TRUE ) )
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

OGRDataSource *OGRMSSQLSpatialDriver::CreateDataSource( const char * pszName,
                                               char **papszOptions )

{
    OGRMSSQLSpatialDataSource   *poDS = new OGRMSSQLSpatialDataSource();

    if( !EQUALN(pszName,"MSSQL:",6) )
        return NULL;

    if( !poDS->Open( pszName, TRUE, TRUE ) )
    {
        delete poDS;
        CPLError( CE_Failure, CPLE_AppDefined, 
         "MSSQL Spatial driver doesn't currently support database creation.\n"
                  "Please create database with the Microsoft SQL Server Client Tools." );
        return NULL;
    }

    return poDS;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRMSSQLSpatialDriver::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,ODrCCreateDataSource) )
        return TRUE;
    else
        return FALSE;
}


/************************************************************************/
/*                           RegisterOGRMSSQLSpatial()                  */
/************************************************************************/

void RegisterOGRMSSQLSpatial()

{
    if (! GDAL_CHECK_VERSION("OGR/MSSQLSpatial driver"))
        return;
    OGRSFDriverRegistrar::GetRegistrar()->RegisterDriver( new OGRMSSQLSpatialDriver );
}

