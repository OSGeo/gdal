/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRShapeDataSource class.
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
 * Revision 1.29  2006/01/06 19:07:37  fwarmerdam
 * Pass wkbNone to OGRShapeLayer as the requested type when opening
 * existing files so their type won't get reset by the "first feature"
 * logic in CreateFeature() when appending to empty shapefiles.
 *
 * Revision 1.28  2005/12/14 16:17:52  fwarmerdam
 * Fixed wkbMultiPolygon to map to POLYGON, not POLYGONZ.
 *
 * Revision 1.27  2005/11/07 17:05:26  fwarmerdam
 * Ensure morphToESRI() is used on spatial reference before writing .prj file.
 *
 * Revision 1.26  2005/01/04 03:43:22  fwarmerdam
 * added support for create/destroy spatial index sql commands
 *
 * Revision 1.25  2005/01/03 22:26:21  fwarmerdam
 * updated to use spatial indexing
 *
 * Revision 1.24  2004/03/01 18:41:00  warmerda
 * added fix for bug 493: ignore pc arcinfo coverages
 *
 * Revision 1.23  2003/07/13 23:06:28  warmerda
 * Added case for wkbMultiPoint25D to SHPT_MULTIPOINTZ.
 *
 * Revision 1.22  2003/05/27 21:39:53  warmerda
 * added support for writing MULTILINESTRINGs as ARCs
 *
 * Revision 1.21  2003/05/21 04:03:54  warmerda
 * expand tabs
 *
 * Revision 1.20  2003/03/27 22:12:14  warmerda
 * Yow ... fix to second last fix.
 *
 * Revision 1.19  2003/03/27 22:11:39  warmerda
 * Fixed similar bug to the last.
 *
 * Revision 1.18  2003/03/27 22:09:43  warmerda
 * fixed shallow copy problem with return result from CPLGetBasename()
 * as per http://bugzilla.remotesensing.org/show_bug.cgi?id=310.
 *
 * Revision 1.17  2003/03/20 19:11:30  warmerda
 * free pszBasename after it is used
 *
 * Revision 1.16  2003/03/04 05:49:05  warmerda
 * added attribute indexing support
 *
 * Revision 1.15  2002/08/12 14:16:10  warmerda
 * Only recognise .dbf files if the file is directly named with the right
 * extension, or if it is in a directory, and there is no like-named .tab file.
 * http://www2.dmsolutions.on.ca:8003/bugzilla/show_bug.cgi?id=1271
 *
 * Revision 1.14  2002/04/17 15:41:05  warmerda
 * Tread multipolygons as shape type polygon.
 *
 * Revision 1.13  2002/04/17 15:40:27  warmerda
 * Added support for 2.5D polygons.
 *
 * Revision 1.12  2002/04/05 20:28:35  warmerda
 * ensure that eType is set properly if SHPT= options given
 *
 * Revision 1.11  2002/03/27 21:04:38  warmerda
 * Added support for reading, and creating lone .dbf files for wkbNone geometry
 * layers.  Added support for creating a single .shp file instead of a directory
 * if a path ending in .shp is passed to the data source create method.
 *
 * Revision 1.10  2001/12/12 17:24:08  warmerda
 * use CPLStat, not VSIStat
 *
 * Revision 1.9  2001/12/12 02:51:07  warmerda
 * avoid memory leaks
 *
 * Revision 1.8  2001/12/11 21:45:36  warmerda
 * fixed unlikely memory leak of pszWKT
 *
 * Revision 1.7  2001/09/04 15:35:14  warmerda
 * add support for deferring geometry type selection till first feature
 *
 * Revision 1.6  2001/07/18 04:55:16  warmerda
 * added CPL_CSVID
 *
 * Revision 1.5  2001/03/16 22:16:10  warmerda
 * added support for ESRI .prj files
 *
 * Revision 1.4  2000/11/02 16:26:12  warmerda
 * fixed geometry type message
 *
 * Revision 1.3  2000/08/29 15:11:47  warmerda
 * added Z types for SHPT
 *
 * Revision 1.2  2000/03/14 21:37:16  warmerda
 * added SHPT= option support
 *
 * Revision 1.1  1999/11/04 21:16:11  warmerda
 * New
 *
 */

#include "ogrshape.h"
#include "cpl_conv.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                         OGRShapeDataSource()                         */
/************************************************************************/

OGRShapeDataSource::OGRShapeDataSource()

{
    pszName = NULL;
    papoLayers = NULL;
    nLayers = 0;
    bSingleNewFile = FALSE;
}

/************************************************************************/
/*                        ~OGRShapeDataSource()                         */
/************************************************************************/

OGRShapeDataSource::~OGRShapeDataSource()

{
    CPLFree( pszName );

    for( int i = 0; i < nLayers; i++ )
        delete papoLayers[i];
    
    CPLFree( papoLayers );
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGRShapeDataSource::Open( const char * pszNewName, int bUpdate,
                              int bTestOpen, int bSingleNewFileIn )

{
    VSIStatBuf  stat;
    
    CPLAssert( nLayers == 0 );
    
    pszName = CPLStrdup( pszNewName );

    bDSUpdate = bUpdate;

    bSingleNewFile = bSingleNewFileIn;

/* -------------------------------------------------------------------- */
/*      If bSingleNewFile is TRUE we don't try to do anything else.     */
/*      This is only utilized when the OGRShapeDriver::Create()         */
/*      method wants to create a stub OGRShapeDataSource for a          */
/*      single shapefile.  The driver will take care of creating the    */
/*      file by calling CreateLayer().                                  */
/* -------------------------------------------------------------------- */
    if( bSingleNewFile )
        return TRUE;
    
/* -------------------------------------------------------------------- */
/*      Is the given path a directory or a regular file?                */
/* -------------------------------------------------------------------- */
    if( CPLStat( pszNewName, &stat ) != 0 
        || (!VSI_ISDIR(stat.st_mode) && !VSI_ISREG(stat.st_mode)) )
    {
        if( !bTestOpen )
            CPLError( CE_Failure, CPLE_AppDefined,
                   "%s is neither a file or directory, Shape access failed.\n",
                      pszNewName );

        return FALSE;
    }
    
/* -------------------------------------------------------------------- */
/*      Build a list of filenames we figure are Shape files.            */
/* -------------------------------------------------------------------- */
    if( VSI_ISREG(stat.st_mode) )
    {
        if( !OpenFile( pszNewName, bUpdate, bTestOpen ) )
        {
            if( !bTestOpen )
                CPLError( CE_Failure, CPLE_OpenFailed,
                          "Failed to open shapefile %s.\n"
                          "It may be corrupt.\n",
                          pszNewName );

            return FALSE;
        }

        return TRUE;
    }
    else
    {
        char      **papszCandidates = CPLReadDir( pszNewName );
        int       iCan, nCandidateCount = CSLCount( papszCandidates );
        int       bMightBeOldCoverage = FALSE;

        for( iCan = 0; iCan < nCandidateCount; iCan++ )
        {
            char        *pszFilename;
            const char  *pszCandidate = papszCandidates[iCan];

            if( EQUAL(pszCandidate,"ARC") )
                bMightBeOldCoverage = TRUE;

            if( strlen(pszCandidate) < 4
                || !EQUAL(pszCandidate+strlen(pszCandidate)-4,".shp") )
                continue;

            pszFilename =
                CPLStrdup(CPLFormFilename(pszNewName, pszCandidate, NULL));

            if( !OpenFile( pszFilename, bUpdate, bTestOpen )
                && !bTestOpen )
            {
                CPLError( CE_Failure, CPLE_OpenFailed,
                          "Failed to open shapefile %s.\n"
                          "It may be corrupt.\n",
                          pszFilename );
                CPLFree( pszFilename );
                return FALSE;
            }
            
            CPLFree( pszFilename );
        }

        // Try and .dbf files without apparent associated shapefiles. 
        for( iCan = 0; iCan < nCandidateCount; iCan++ )
        {
            char        *pszFilename;
            const char  *pszCandidate = papszCandidates[iCan];
            const char  *pszLayerName;
            int         iLayer, bGotAlready = FALSE;

            // We don't consume .dbf files in a directory that looks like
            // an old style Arc/Info (for PC?) that unless we found at least
            // some shapefiles.  See Bug 493. 
            if( bMightBeOldCoverage && nLayers == 0 )
                continue;

            if( strlen(pszCandidate) < 4
                || !EQUAL(pszCandidate+strlen(pszCandidate)-4,".dbf") )
                continue;

            pszLayerName = CPLGetBasename(pszCandidate);
            for( iLayer = 0; iLayer < nLayers; iLayer++ )
            {
                if( EQUAL(pszLayerName,
                          GetLayer(iLayer)->GetLayerDefn()->GetName()) )
                    bGotAlready = TRUE;
            }
            
            if( bGotAlready )
                continue;

            // We don't want to access .dbf files with an associated .tab
            // file, or it will never get recognised as a mapinfo dataset.
            int  iCan2, bFoundTAB = FALSE;
            for( iCan2 = 0; iCan2 < nCandidateCount; iCan2++ )
            {
                const char *pszCandidate2 = papszCandidates[iCan2];

                if( EQUALN(pszCandidate2,pszLayerName,strlen(pszLayerName))
                    && EQUAL(pszCandidate2 + strlen(pszLayerName), ".tab") )
                    bFoundTAB = TRUE;
            }

            if( bFoundTAB )
                continue;
            
            pszFilename =
                CPLStrdup(CPLFormFilename(pszNewName, pszCandidate, NULL));

            if( !OpenFile( pszFilename, bUpdate, bTestOpen )
                && !bTestOpen )
            {
                CPLError( CE_Failure, CPLE_OpenFailed,
                          "Failed to open dbf file %s.\n"
                          "It may be corrupt.\n",
                          pszFilename );
                CPLFree( pszFilename );
                return FALSE;
            }
            
            CPLFree( pszFilename );
        }

        CSLDestroy( papszCandidates );
        
        if( !bTestOpen && nLayers == 0 && !bUpdate )
        {
            CPLError( CE_Failure, CPLE_OpenFailed,
                      "No Shapefiles found in directory %s\n",
                      pszNewName );
        }
    }

    return nLayers > 0 || bUpdate;
}

/************************************************************************/
/*                              OpenFile()                              */
/************************************************************************/

int OGRShapeDataSource::OpenFile( const char *pszNewName, int bUpdate,
                                  int bTestOpen )

{
    SHPHandle   hSHP;
    DBFHandle   hDBF;
    const char *pszExtension = CPLGetExtension( pszNewName );

    (void) bTestOpen;

    if( !EQUAL(pszExtension,"shp") && !EQUAL(pszExtension,"shx")
        && !EQUAL(pszExtension,"dbf") )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      SHPOpen() should include better (CPL based) error reporting,    */
/*      and we should be trying to distinquish at this point whether    */
/*      failure is a result of trying to open a non-shapefile, or       */
/*      whether it was a shapefile and we want to report the error up.  */
/* -------------------------------------------------------------------- */
    if( bUpdate )
        hSHP = SHPOpen( pszNewName, "r+" );
    else
        hSHP = SHPOpen( pszNewName, "r" );

    if( hSHP == NULL && !EQUAL(CPLGetExtension(pszNewName),"dbf") )
        return FALSE;
    
/* -------------------------------------------------------------------- */
/*      Open the .dbf file, if it exists.  To open a dbf file, the      */
/*      filename has to either refer to a successfully opened shp       */
/*      file or has to refer to the actual .dbf file.                   */
/* -------------------------------------------------------------------- */
    if( hSHP != NULL || EQUAL(CPLGetExtension(pszNewName),"dbf") )
    {
        if( bUpdate )
            hDBF = DBFOpen( pszNewName, "r+" );
        else
            hDBF = DBFOpen( pszNewName, "r" );
    }
    else
        hDBF = NULL;
        
    if( hDBF == NULL && hSHP == NULL )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Is there an associated .prj file we can read?                   */
/* -------------------------------------------------------------------- */
    OGRSpatialReference *poSRS = NULL;
    const char  *pszPrjFile = CPLResetExtension( pszNewName, "prj" );
    FILE        *fp;

    fp = VSIFOpen( pszPrjFile, "r" );
    if( fp != NULL )
    {
        char    **papszLines;

        VSIFClose( fp );
        
        papszLines = CSLLoad( pszPrjFile );

        poSRS = new OGRSpatialReference();
        if( poSRS->importFromESRI( papszLines ) != OGRERR_NONE )
        {
            delete poSRS;
            poSRS = NULL;
        }
        CSLDestroy( papszLines );
    }

/* -------------------------------------------------------------------- */
/*      Create the layer object.                                        */
/* -------------------------------------------------------------------- */
    OGRShapeLayer       *poLayer;

    poLayer = new OGRShapeLayer( pszNewName, hSHP, hDBF, poSRS, bUpdate,
                                 wkbNone );


    poLayer->InitializeIndexSupport( pszNewName );

/* -------------------------------------------------------------------- */
/*      Add layer to data source layer list.                            */
/* -------------------------------------------------------------------- */
    papoLayers = (OGRShapeLayer **)
        CPLRealloc( papoLayers,  sizeof(OGRShapeLayer *) * (nLayers+1) );
    papoLayers[nLayers++] = poLayer;
    
    return TRUE;
}

/************************************************************************/
/*                            CreateLayer()                             */
/************************************************************************/

OGRLayer *
OGRShapeDataSource::CreateLayer( const char * pszLayerName,
                                 OGRSpatialReference *poSRS,
                                 OGRwkbGeometryType eType,
                                 char ** papszOptions )

{
    SHPHandle   hSHP;
    DBFHandle   hDBF;
    int         nShapeType;

/* -------------------------------------------------------------------- */
/*      Verify we are in update mode.                                   */
/* -------------------------------------------------------------------- */
    if( !bDSUpdate )
    {
        CPLError( CE_Failure, CPLE_NoWriteAccess,
                  "Data source %s opened read-only.\n"
                  "New layer %s cannot be created.\n",
                  pszName, pszLayerName );

        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Figure out what type of layer we need.                          */
/* -------------------------------------------------------------------- */
    if( eType == wkbUnknown || eType == wkbLineString )
        nShapeType = SHPT_ARC;
    else if( eType == wkbPoint )
        nShapeType = SHPT_POINT;
    else if( eType == wkbPolygon )
        nShapeType = SHPT_POLYGON;
    else if( eType == wkbMultiPoint )
        nShapeType = SHPT_MULTIPOINT;
    else if( eType == wkbPoint25D )
        nShapeType = SHPT_POINTZ;
    else if( eType == wkbLineString25D )
        nShapeType = SHPT_ARCZ;
    else if( eType == wkbMultiLineString )
        nShapeType = SHPT_ARC;
    else if( eType == wkbMultiLineString25D )
        nShapeType = SHPT_ARCZ;
    else if( eType == wkbPolygon25D )
        nShapeType = SHPT_POLYGONZ;
    else if( eType == wkbMultiPolygon )
        nShapeType = SHPT_POLYGON;
    else if( eType == wkbMultiPolygon25D )
        nShapeType = SHPT_POLYGONZ;
    else if( eType == wkbMultiPoint25D )
        nShapeType = SHPT_MULTIPOINTZ;
    else if( eType == wkbNone )
        nShapeType = SHPT_NULL;
    else
        nShapeType = -1;

/* -------------------------------------------------------------------- */
/*      Has the application overridden this with a special creation     */
/*      option?                                                         */
/* -------------------------------------------------------------------- */
    const char *pszOverride = CSLFetchNameValue( papszOptions, "SHPT" );

    if( pszOverride == NULL )
        /* ignore */;
    else if( EQUAL(pszOverride,"POINT") )
    {
        nShapeType = SHPT_POINT;
        eType = wkbPoint;
    }
    else if( EQUAL(pszOverride,"ARC") )
    {
        nShapeType = SHPT_ARC;
        eType = wkbLineString;
    }
    else if( EQUAL(pszOverride,"POLYGON") )
    {
        nShapeType = SHPT_POLYGON;
        eType = wkbPolygon;
    }
    else if( EQUAL(pszOverride,"MULTIPOINT") )
    {
        nShapeType = SHPT_MULTIPOINT;
        eType = wkbMultiPoint;
    }
    else if( EQUAL(pszOverride,"POINTZ") )
    {
        nShapeType = SHPT_POINTZ;
        eType = wkbPoint25D;
    }
    else if( EQUAL(pszOverride,"ARCZ") )
    {
        nShapeType = SHPT_ARCZ;
        eType = wkbLineString25D;
    }
    else if( EQUAL(pszOverride,"POLYGONZ") )
    {
        nShapeType = SHPT_POLYGONZ;
        eType = wkbPolygon25D;
    }
    else if( EQUAL(pszOverride,"MULTIPOINTZ") )
    {
        nShapeType = SHPT_MULTIPOINTZ;
        eType = wkbMultiPoint25D;
    }
    else if( EQUAL(pszOverride,"NONE") )
    {
        nShapeType = SHPT_NULL;
    }
    else
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Unknown SHPT value of `%s' passed to Shapefile layer\n"
                  "creation.  Creation aborted.\n",
                  pszOverride );

        return NULL;
    }

    if( nShapeType == -1 )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Geometry type of `%s' not supported in shapefiles.\n"
                  "Type can be overridden with a layer creation option\n"
                  "of SHPT=POINT/ARC/POLYGON/MULTIPOINT.\n",
                  OGRGeometryTypeToName(eType) );
        return NULL;
    }
    
/* -------------------------------------------------------------------- */
/*      What filename do we use, excluding the extension?               */
/* -------------------------------------------------------------------- */
    char *pszBasename;

    if( bSingleNewFile && nLayers == 0 )
    {
        char *pszPath = CPLStrdup(CPLGetPath(pszName));
        char *pszFBasename = CPLStrdup(CPLGetBasename(pszName));

        pszBasename = CPLStrdup(CPLFormFilename(pszPath, pszFBasename, NULL));

        CPLFree( pszFBasename );
        CPLFree( pszPath );
    }
    else if( bSingleNewFile )
    {
        char *pszPath = CPLStrdup(CPLGetPath(pszName));
        pszBasename = CPLStrdup(CPLFormFilename(pszPath,pszLayerName,NULL));
        CPLFree( pszPath );
    }
    else
        pszBasename = CPLStrdup(CPLFormFilename(pszName,pszLayerName,NULL));

/* -------------------------------------------------------------------- */
/*      Create the shapefile.                                           */
/* -------------------------------------------------------------------- */
    char        *pszFilename;

    if( nShapeType != SHPT_NULL )
    {
        pszFilename = CPLStrdup(CPLFormFilename( NULL, pszBasename, "shp" ));

        hSHP = SHPCreate( pszFilename, nShapeType );
        
        if( hSHP == NULL )
        {
            CPLError( CE_Failure, CPLE_OpenFailed,
                      "Failed to open Shapefile `%s'.\n",
                      pszFilename );
            CPLFree( pszFilename );
            CPLFree( pszBasename );
            return NULL;
        }
        CPLFree( pszFilename );
    }
    else
        hSHP = NULL;

/* -------------------------------------------------------------------- */
/*      Create a DBF file.                                              */
/* -------------------------------------------------------------------- */
    pszFilename = CPLStrdup(CPLFormFilename( NULL, pszBasename, "dbf" ));
    
    hDBF = DBFCreate( pszFilename );

    if( hDBF == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Failed to open Shape DBF file `%s'.\n",
                  pszFilename );
        CPLFree( pszFilename );
        CPLFree( pszBasename );
        return NULL;
    }

    CPLFree( pszFilename );

/* -------------------------------------------------------------------- */
/*      Create the .prj file, if required.                              */
/* -------------------------------------------------------------------- */
    if( poSRS != NULL )
    {
        char    *pszWKT = NULL;
        const char *pszPrjFile = CPLFormFilename( NULL, pszBasename, "prj");
        FILE    *fp;

        /* the shape layer needs it's own copy */
        poSRS = poSRS->Clone();
        poSRS->morphToESRI();

        if( poSRS->exportToWkt( &pszWKT ) == OGRERR_NONE 
            && (fp = VSIFOpen( pszPrjFile, "wt" )) != NULL )
        {
            VSIFWrite( pszWKT, strlen(pszWKT), 1, fp );
            VSIFClose( fp );
        }

        CPLFree( pszWKT );
    }

/* -------------------------------------------------------------------- */
/*      Create the layer object.                                        */
/* -------------------------------------------------------------------- */
    OGRShapeLayer       *poLayer;

    poLayer = new OGRShapeLayer( pszBasename, hSHP, hDBF, poSRS, TRUE,
                                 eType );
    
    poLayer->InitializeIndexSupport( pszBasename );

    CPLFree( pszBasename );

/* -------------------------------------------------------------------- */
/*      Add layer to data source layer list.                            */
/* -------------------------------------------------------------------- */
    papoLayers = (OGRShapeLayer **)
        CPLRealloc( papoLayers,  sizeof(OGRShapeLayer *) * (nLayers+1) );
    
    papoLayers[nLayers++] = poLayer;

    return poLayer;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRShapeDataSource::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,ODsCCreateLayer) )
        return TRUE;
    else
        return FALSE;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRShapeDataSource::GetLayer( int iLayer )

{
    if( iLayer < 0 || iLayer >= nLayers )
        return NULL;
    else
        return papoLayers[iLayer];
}

/************************************************************************/
/*                             ExecuteSQL()                             */
/*                                                                      */
/*      We override this to provide special handling of CREATE          */
/*      SPATIAL INDEX commands.  Support forms are:                     */
/*                                                                      */
/*        CREATE SPATIAL INDEX ON layer_name [DEPTH n]                  */
/*                                                                      */
/************************************************************************/

OGRLayer * OGRShapeDataSource::ExecuteSQL( const char *pszStatement,
                                           OGRGeometry *poSpatialFilter,
                                           const char *pszDialect )

{
/* ==================================================================== */
/*      Handle command to drop a spatial index.                         */
/* ==================================================================== */
    if( EQUALN(pszStatement, "DROP SPATIAL INDEX ON ", 22) )
    {
        OGRShapeLayer *poLayer = (OGRShapeLayer *) 
            GetLayerByName( pszStatement + 22 );

        if( poLayer != NULL )
            poLayer->DropSpatialIndex();
        else
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "No such layer as '%s' in DROP SPATIAL INDEX.", 
                      pszStatement + 19 );
        }
        return NULL;
    }
    
/* ==================================================================== */
/*      Handle all comands except spatial index creation generically.   */
/* ==================================================================== */
    if( !EQUALN(pszStatement,"CREATE SPATIAL INDEX ON ",24) )
        return OGRDataSource::ExecuteSQL( pszStatement, poSpatialFilter, 
                                          pszDialect );

/* -------------------------------------------------------------------- */
/*      Parse into keywords.                                            */
/* -------------------------------------------------------------------- */
    char **papszTokens = CSLTokenizeString( pszStatement );
    
    if( CSLCount(papszTokens) < 5
        || !EQUAL(papszTokens[0],"CREATE")
        || !EQUAL(papszTokens[1],"SPATIAL")
        || !EQUAL(papszTokens[2],"INDEX") 
        || !EQUAL(papszTokens[3],"ON") 
        || CSLCount(papszTokens) > 7 
        || (CSLCount(papszTokens) == 7 && !EQUAL(papszTokens[5],"DEPTH")) )
    {
        CSLDestroy( papszTokens );
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Syntax error in CREATE SPATIAL INDEX command.\n"
                  "Was '%s'\n"
                  "Should be of form 'CREATE SPATIAL INDEX ON <table> [DEPTH <n>]'",
                  pszStatement );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Get depth if provided.                                          */
/* -------------------------------------------------------------------- */
    int nDepth = 0;
    if( CSLCount(papszTokens) == 7 )
        nDepth = atoi(papszTokens[6]);

/* -------------------------------------------------------------------- */
/*      What layer are we operating on.                                 */
/* -------------------------------------------------------------------- */
    OGRShapeLayer *poLayer = (OGRShapeLayer *) GetLayerByName(papszTokens[4]);
    CSLDestroy( papszTokens );

    if( poLayer == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Layer %s not recognised.", 
                  papszTokens[4] );
        return NULL;
    }

    poLayer->CreateSpatialIndex( nDepth );
    return NULL;
}

