/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRODBCDriver class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
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

#include "ogr_odbc.h"
#include "cpl_conv.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                     OGRODBCDriverIdentify()                          */
/************************************************************************/

static int OGRODBCDriverIdentify( GDALOpenInfo* poOpenInfo )

{
    if (STARTS_WITH_CI(poOpenInfo->pszFilename, "PGEO:"))
    {
        return FALSE;
    }

    if( STARTS_WITH_CI(poOpenInfo->pszFilename, "ODBC:") )
        return TRUE;

    const char* psExtension(CPLGetExtension(poOpenInfo->pszFilename));
    if( EQUAL(psExtension,"mdb") )
        return -1; // Could potentially be a PGeo MDB database

    if ( OGRODBCDataSource::IsSupportedMsAccessFileExtension( psExtension ) )
        return TRUE; // An Access database which isn't a .MDB file (we checked that above), so this is the only candidate driver

    // doesn't start with "ODBC:", and isn't an access database => not supported
    return FALSE;
}


/************************************************************************/
/*                      OGRODBCDriverOpen()                             */
/************************************************************************/

static GDALDataset *OGRODBCDriverOpen( GDALOpenInfo* poOpenInfo )

{
    if (OGRODBCDriverIdentify(poOpenInfo) == FALSE)
        return nullptr;

    OGRODBCDataSource *poDS = new OGRODBCDataSource();

    if( !poDS->Open( poOpenInfo ) )
    {
        delete poDS;
        return nullptr;
    }
    else
        return poDS;
}


/************************************************************************/
/*                           RegisterOGRODBC()                            */
/************************************************************************/

void RegisterOGRODBC()

{
    if( GDALGetDriverByName( "ODBC" ) != nullptr )
        return;

    GDALDriver* poDriver = new GDALDriver;

    poDriver->SetDescription( "ODBC" );
    poDriver->SetMetadataItem(GDAL_DCAP_VECTOR, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_CONNECTION_PREFIX, "ODBC:");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSIONS, "mdb accdb" );
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drivers/vector/odbc.html");
    poDriver->SetMetadataItem( GDAL_DCAP_MULTIPLE_VECTOR_LAYERS, "YES" );

    poDriver->SetMetadataItem( GDAL_DMD_OPENOPTIONLIST, "<OpenOptionList>"
"  <Option name='LIST_ALL_TABLES' type='string-select' scope='vector' description='Whether all tables, including system and internal tables (such as MSys* tables) should be listed' default='NO'>"
"    <Value>YES</Value>"
"    <Value>NO</Value>"
"  </Option>"
"</OpenOptionList>");

    poDriver->pfnOpen = OGRODBCDriverOpen;
    poDriver->pfnIdentify = OGRODBCDriverIdentify;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
