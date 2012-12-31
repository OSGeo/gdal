/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRSQLiteDriver class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 *
 * Contributor: Alessandro Furieri, a.furieri@lqt.it
 * Portions of this module properly supporting SpatiaLite DB creation
 * Developed for Faunalia ( http://www.faunalia.it) with funding from 
 * Regione Toscana - Settore SISTEMA INFORMATIVO TERRITORIALE ED AMBIENTALE
 *
 ******************************************************************************
 * Copyright (c) 2004, Frank Warmerdam <warmerdam@pobox.com>
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

#include "ogr_sqlite.h"
#include "cpl_conv.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                            ~OGRSQLiteDriver()                        */
/************************************************************************/

OGRSQLiteDriver::~OGRSQLiteDriver()

{
}

/************************************************************************/
/*                              GetName()                               */
/************************************************************************/

const char *OGRSQLiteDriver::GetName()

{
    return "SQLite";
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

OGRDataSource *OGRSQLiteDriver::Open( const char * pszFilename,
                                     int bUpdate )

{

/* -------------------------------------------------------------------- */
/*      Check VirtualShape:xxx.shp syntax                               */
/* -------------------------------------------------------------------- */
    int nLen = (int) strlen(pszFilename);
    if (EQUALN(pszFilename, "VirtualShape:", strlen( "VirtualShape:" )) &&
        nLen > 4 && EQUAL(pszFilename + nLen - 4, ".SHP"))
    {
        OGRSQLiteDataSource     *poDS;

        poDS = new OGRSQLiteDataSource();

        char** papszOptions = CSLAddString(NULL, "SPATIALITE=YES");
        int nRet = poDS->Create( ":memory:", papszOptions );
        poDS->SetName(pszFilename);
        CSLDestroy(papszOptions);
        if (!nRet)
        {
            delete poDS;
            return NULL;
        }

        char* pszShapeFilename = CPLStrdup(pszFilename + strlen( "VirtualShape:" ));
        OGRDataSource* poShapeDS = OGRSFDriverRegistrar::Open(pszShapeFilename);
        if (poShapeDS == NULL)
        {
            CPLFree(pszShapeFilename);
            delete poDS;
            return NULL;
        }
        delete poShapeDS;

        char* pszLastDot = strrchr(pszShapeFilename, '.');
        if (pszLastDot)
            *pszLastDot = '\0';

        const char* pszTableName = CPLGetBasename(pszShapeFilename);

        char* pszSQL = CPLStrdup(CPLSPrintf("CREATE VIRTUAL TABLE %s USING VirtualShape(%s, CP1252, -1)",
                                            pszTableName, pszShapeFilename));
        poDS->ExecuteSQL(pszSQL, NULL, NULL);
        CPLFree(pszSQL);
        CPLFree(pszShapeFilename);
        return poDS;
    }

/* -------------------------------------------------------------------- */
/*      Verify that the target is a real file, and has an               */
/*      appropriate magic string at the beginning.                      */
/* -------------------------------------------------------------------- */
    if( !EQUAL(pszFilename, ":memory:") )
    {
        char szHeader[16];

#ifdef HAVE_SQLITE_VFS
        VSILFILE *fpDB;
        fpDB = VSIFOpenL( pszFilename, "rb" );
        if( fpDB == NULL )
            return NULL;
        
        if( VSIFReadL( szHeader, 1, 16, fpDB ) != 16 )
            memset( szHeader, 0, 16 );
        
        VSIFCloseL( fpDB );
#else
        FILE *fpDB;
        fpDB = VSIFOpen( pszFilename, "rb" );
        if( fpDB == NULL )
            return NULL;

        if( VSIFRead( szHeader, 1, 16, fpDB ) != 16 )
            memset( szHeader, 0, 16 );

        VSIFClose( fpDB );
#endif
    
        if( strncmp( szHeader, "SQLite format 3", 15 ) != 0 )
            return NULL;
    }

/* -------------------------------------------------------------------- */
/*      We think this is really an SQLite database, go ahead and try    */
/*      and open it.                                                    */
/* -------------------------------------------------------------------- */
    OGRSQLiteDataSource     *poDS;

    poDS = new OGRSQLiteDataSource();

    if( !poDS->Open( pszFilename, bUpdate ) )
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

OGRDataSource *OGRSQLiteDriver::CreateDataSource( const char * pszName,
                                                  char **papszOptions )

{
/* -------------------------------------------------------------------- */
/*      First, ensure there isn't any such file yet.                    */
/* -------------------------------------------------------------------- */
    VSIStatBufL sStatBuf;

    if( VSIStatL( pszName, &sStatBuf ) == 0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "It seems a file system object called '%s' already exists.",
                  pszName );

        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Try to create datasource.                                       */
/* -------------------------------------------------------------------- */
    OGRSQLiteDataSource     *poDS;

    poDS = new OGRSQLiteDataSource();

    if( !poDS->Create( pszName, papszOptions ) )
    {
        delete poDS;
        return NULL;
    }
    else
        return poDS;
}

/************************************************************************/
/*                         DeleteDataSource()                           */
/************************************************************************/

OGRErr OGRSQLiteDriver::DeleteDataSource( const char *pszName )
{
    if (VSIUnlink( pszName ) == 0)
        return OGRERR_NONE;
    else
        return OGRERR_FAILURE;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRSQLiteDriver::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,ODrCCreateDataSource) )
        return TRUE;
    else if( EQUAL(pszCap,ODrCDeleteDataSource) )
        return TRUE;
    else
        return FALSE;
}

/************************************************************************/
/*                         RegisterOGRSQLite()                          */
/************************************************************************/

void RegisterOGRSQLite()

{
    if (! GDAL_CHECK_VERSION("SQLite driver"))
        return;
    OGRSFDriverRegistrar::GetRegistrar()->RegisterDriver( new OGRSQLiteDriver );
}

