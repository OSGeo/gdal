/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRShapeDataSource class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999,  Les Technologies SoftMap Inc.
 * Copyright (c) 2008-2013, Even Rouault <even dot rouault at spatialys.com>
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

#include "cpl_port.h"
#include "ogrshape.h"

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <set>
#include <vector>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "cpl_vsi_error.h"
#include "gdal.h"
#include "gdal_priv.h"
#include "ogr_core.h"
#include "ogr_geometry.h"
#include "ogr_spatialref.h"
#include "ogrlayerpool.h"
#include "ogrsf_frmts.h"
#include "shapefil.h"
#include "shp_vsi.h"

// #define IMMEDIATE_OPENING 1

CPL_CVSID("$Id$")

constexpr int knREFRESH_LOCK_FILE_DELAY_SEC = 10;

/************************************************************************/
/*                          DS_SHPOpen()                                */
/************************************************************************/

SHPHandle OGRShapeDataSource::DS_SHPOpen( const char * pszShapeFile,
                                          const char * pszAccess )
{
    // Do lazy shx loading for /vsicurl/
    if( STARTS_WITH(pszShapeFile, "/vsicurl/") &&
        strcmp(pszAccess, "r") == 0 )
        pszAccess = "rl";

    const bool bRestoreSHX =
        CPLTestBool( CPLGetConfigOption("SHAPE_RESTORE_SHX", "FALSE") );
    SHPHandle hSHP =
        SHPOpenLLEx( pszShapeFile, pszAccess,
                     const_cast<SAHooks *>(VSI_SHP_GetHook(b2GBLimit)),
                     bRestoreSHX );

    if( hSHP != nullptr )
        SHPSetFastModeReadObject( hSHP, TRUE );
    return hSHP;
}

/************************************************************************/
/*                           DS_DBFOpen()                               */
/************************************************************************/

DBFHandle OGRShapeDataSource::DS_DBFOpen( const char * pszDBFFile,
                                          const char * pszAccess )
{
    DBFHandle hDBF =
        DBFOpenLL( pszDBFFile, pszAccess,
                   const_cast<SAHooks *>(VSI_SHP_GetHook(b2GBLimit)) );
    return hDBF;
}

/************************************************************************/
/*                         OGRShapeDataSource()                         */
/************************************************************************/

OGRShapeDataSource::OGRShapeDataSource() :
    papoLayers(nullptr),
    nLayers(0),
    pszName(nullptr),
    bDSUpdate(false),
    bSingleFileDataSource(false),
    poPool(new OGRLayerPool()),
    b2GBLimit(CPLTestBool(CPLGetConfigOption("SHAPE_2GB_LIMIT", "FALSE")))
{}

/************************************************************************/
/*                             GetLayerNames()                          */
/************************************************************************/

std::vector<CPLString> OGRShapeDataSource::GetLayerNames() const
{
    std::vector<CPLString> res;
    const_cast<OGRShapeDataSource*>(this)->GetLayerCount();
    for( int i = 0; i < nLayers; i++ )
    {
        res.emplace_back(papoLayers[i]->GetName());
    }
    return res;
}

/************************************************************************/
/*                        ~OGRShapeDataSource()                         */
/************************************************************************/

OGRShapeDataSource::~OGRShapeDataSource()

{
    std::vector<CPLString> layerNames;
    if( !m_osTemporaryUnzipDir.empty() )
    {
        layerNames = GetLayerNames();
    }
    for( int i = 0; i < nLayers; i++ )
    {
        CPLAssert( nullptr != papoLayers[i] );

        delete papoLayers[i];
    }
    CPLFree( papoLayers );
    nLayers = 0;
    papoLayers = nullptr;

    delete poPool;

    RecompressIfNeeded(layerNames);
    RemoveLockFile();

    // Free mutex & cond
    if( m_poRefreshLockFileMutex )
    {
        CPLDestroyMutex(m_poRefreshLockFileMutex);
        m_poRefreshLockFileMutex = nullptr;
    }
    if( m_poRefreshLockFileCond )
    {
        CPLDestroyCond(m_poRefreshLockFileCond);
        m_poRefreshLockFileCond = nullptr;
    }

    CPLFree( pszName );
}

/************************************************************************/
/*                              OpenZip()                               */
/************************************************************************/

bool OGRShapeDataSource::OpenZip( GDALOpenInfo* poOpenInfo,
                                  const char* pszOriFilename )
{
    if( !Open(poOpenInfo, true) )
        return false;
    CPLFree(pszName);
    pszName = CPLStrdup(pszOriFilename);
    m_bIsZip = true;
    m_bSingleLayerZip = EQUAL(CPLGetExtension(pszOriFilename), "shz");

    if( !m_bSingleLayerZip )
    {
        CPLString osLockFile(pszName);
        osLockFile += ".gdal.lock";
        VSIStatBufL sStat;
        if( VSIStatL(osLockFile, &sStat) == 0 &&
            sStat.st_mtime < time(nullptr) - 2 * knREFRESH_LOCK_FILE_DELAY_SEC )
        {
            CPLDebug("Shape", "Deleting stalled %s", osLockFile.c_str());
            VSIUnlink(osLockFile);
        }
    }

    return true;
}

/************************************************************************/
/*                            CreateZip()                               */
/************************************************************************/

bool OGRShapeDataSource::CreateZip( const char* pszOriFilename )
{
    CPLAssert( nLayers == 0 );
    pszName = CPLStrdup( pszOriFilename );

    void* hZIP = CPLCreateZip(pszName, nullptr);
    if( !hZIP )
        return false;
    if( CPLCloseZip(hZIP) != CE_None )
        return false;
    bDSUpdate = true;
    m_bIsZip = true;
    m_bSingleLayerZip = EQUAL(CPLGetExtension(pszOriFilename), "shz");
    return true;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

bool OGRShapeDataSource::Open( GDALOpenInfo* poOpenInfo,
                              bool bTestOpen, bool bForceSingleFileDataSource )

{
    CPLAssert( nLayers == 0 );

    const char * pszNewName = poOpenInfo->pszFilename;
    const bool bUpdate = poOpenInfo->eAccess == GA_Update;
    CPLAssert( papszOpenOptions == nullptr );
    papszOpenOptions = CSLDuplicate( poOpenInfo->papszOpenOptions );

    pszName = CPLStrdup( pszNewName );

    bDSUpdate = bUpdate;

    bSingleFileDataSource = CPL_TO_BOOL(bForceSingleFileDataSource);

/* -------------------------------------------------------------------- */
/*      If bSingleFileDataSource is TRUE we don't try to do anything    */
/*      else.                                                           */
/*      This is only utilized when the OGRShapeDriver::Create()         */
/*      method wants to create a stub OGRShapeDataSource for a          */
/*      single shapefile.  The driver will take care of creating the    */
/*      file by calling ICreateLayer().                                 */
/* -------------------------------------------------------------------- */
    if( bSingleFileDataSource )
        return true;

/* -------------------------------------------------------------------- */
/*      Is the given path a directory or a regular file?                */
/* -------------------------------------------------------------------- */
    if( !poOpenInfo->bStatOK )
    {
        if( !bTestOpen )
            CPLError( CE_Failure, CPLE_AppDefined,
                      "%s is neither a file or directory, Shape access failed.",
                      pszNewName );

        return false;
    }

/* -------------------------------------------------------------------- */
/*      Build a list of filenames we figure are Shape files.            */
/* -------------------------------------------------------------------- */
    if( !poOpenInfo->bIsDirectory )
    {
        if( !OpenFile( pszNewName, bUpdate ) )
        {
            if( !bTestOpen )
                CPLError(
                    CE_Failure, CPLE_OpenFailed,
                    "Failed to open shapefile %s.  "
                    "It may be corrupt or read-only file accessed in "
                    "update mode.",
                    pszNewName );

            return false;
        }

        bSingleFileDataSource = true;

        return true;
    }
    else
    {
        char **papszCandidates = VSIReadDir( pszNewName );
        const int nCandidateCount = CSLCount( papszCandidates );
        bool bMightBeOldCoverage = false;
        std::set<CPLString> osLayerNameSet;

        for( int iCan = 0; iCan < nCandidateCount; iCan++ )
        {
            const char *pszCandidate = papszCandidates[iCan];
            const char *pszLayerName = CPLGetBasename(pszCandidate);
            CPLString osLayerName(pszLayerName);
#ifdef WIN32
            // On Windows, as filenames are case insensitive, a shapefile layer
            // can be made of foo.shp and FOO.DBF, so to detect unique layer
            // names, put them upper case in the unique set used for detection.
            osLayerName.toupper();
#endif

            if( EQUAL(pszCandidate,"ARC") )
                bMightBeOldCoverage = true;

            if( strlen(pszCandidate) < 4
                || !EQUAL(pszCandidate+strlen(pszCandidate)-4,".shp") )
                continue;

            char *pszFilename =
                CPLStrdup(CPLFormFilename(pszNewName, pszCandidate, nullptr));

            osLayerNameSet.insert(osLayerName);
#ifdef IMMEDIATE_OPENING
            if( !OpenFile( pszFilename, bUpdate )
                && !bTestOpen )
            {
                CPLError(
                    CE_Failure, CPLE_OpenFailed,
                    "Failed to open shapefile %s.  "
                    "It may be corrupt or read-only file accessed in "
                    "update mode.",
                    pszFilename );
                CPLFree( pszFilename );
                CSLDestroy( papszCandidates );
                return false;
            }
#else
            oVectorLayerName.push_back(pszFilename);
#endif
            CPLFree( pszFilename );
        }

        // Try and .dbf files without apparent associated shapefiles.
        for( int iCan = 0; iCan < nCandidateCount; iCan++ )
        {
            const char  *pszCandidate = papszCandidates[iCan];
            const char  *pszLayerName = CPLGetBasename(pszCandidate);
            CPLString osLayerName(pszLayerName);
#ifdef WIN32
            osLayerName.toupper();
#endif

            // We don't consume .dbf files in a directory that looks like
            // an old style Arc/Info (for PC?) that unless we found at least
            // some shapefiles.  See Bug 493.
            if( bMightBeOldCoverage && osLayerNameSet.empty() )
                continue;

            if( strlen(pszCandidate) < 4
                || !EQUAL(pszCandidate+strlen(pszCandidate)-4, ".dbf") )
                continue;

            if( osLayerNameSet.find(osLayerName) != osLayerNameSet.end() )
                continue;

            // We don't want to access .dbf files with an associated .tab
            // file, or it will never get recognised as a mapinfo dataset.
            bool bFoundTAB = false;
            for( int iCan2 = 0; iCan2 < nCandidateCount; iCan2++ )
            {
                const char *pszCandidate2 = papszCandidates[iCan2];

                if( EQUALN(pszCandidate2, pszLayerName, strlen(pszLayerName))
                    && EQUAL(pszCandidate2 + strlen(pszLayerName), ".tab") )
                    bFoundTAB = true;
            }

            if( bFoundTAB )
                continue;

            char *pszFilename =
                CPLStrdup(CPLFormFilename(pszNewName, pszCandidate, nullptr));

            osLayerNameSet.insert(osLayerName);

#ifdef IMMEDIATE_OPENING
            if( !OpenFile( pszFilename, bUpdate )
                && !bTestOpen )
            {
                CPLError(
                    CE_Failure, CPLE_OpenFailed,
                    "Failed to open dbf file %s.  "
                    "It may be corrupt or read-only file accessed in "
                    "update mode.",
                    pszFilename );
                CPLFree( pszFilename );
                CSLDestroy( papszCandidates );
                return false;
            }
#else
            oVectorLayerName.push_back(pszFilename);
#endif
            CPLFree( pszFilename );
        }

        CSLDestroy( papszCandidates );

#ifdef IMMEDIATE_OPENING
        const int nDirLayers = nLayers;
#else
        const int nDirLayers = static_cast<int>(oVectorLayerName.size());
#endif

        CPLErrorReset();

        return nDirLayers > 0 || !bTestOpen;
    }
}

/************************************************************************/
/*                              OpenFile()                              */
/************************************************************************/

bool OGRShapeDataSource::OpenFile( const char *pszNewName, bool bUpdate )

{
    const char *pszExtension = CPLGetExtension( pszNewName );

    if( !EQUAL(pszExtension,"shp") && !EQUAL(pszExtension,"shx")
        && !EQUAL(pszExtension,"dbf") )
        return false;

/* -------------------------------------------------------------------- */
/*      SHPOpen() should include better (CPL based) error reporting,    */
/*      and we should be trying to distinguish at this point whether    */
/*      failure is a result of trying to open a non-shapefile, or       */
/*      whether it was a shapefile and we want to report the error      */
/*      up.                                                             */
/*                                                                      */
/*      Care is taken to suppress the error and only reissue it if      */
/*      we think it is appropriate.                                     */
/* -------------------------------------------------------------------- */
    const bool bRealUpdateAccess = bUpdate &&
        (!IsZip() || !GetTemporaryUnzipDir().empty());
    CPLErrorReset();
    CPLPushErrorHandler( CPLQuietErrorHandler );
    SHPHandle hSHP = bRealUpdateAccess ?
        DS_SHPOpen( pszNewName, "r+" ) :
        DS_SHPOpen( pszNewName, "r" );
    CPLPopErrorHandler();

    const bool bRestoreSHX =
        CPLTestBool( CPLGetConfigOption("SHAPE_RESTORE_SHX", "FALSE") );
    if( bRestoreSHX && EQUAL(CPLGetExtension(pszNewName),"dbf") &&
        CPLGetLastErrorMsg()[0] != '\0' )
    {
        CPLString osMsg = CPLGetLastErrorMsg();

        CPLError( CE_Warning, CPLE_AppDefined, "%s", osMsg.c_str() );
    }
    else
    {
        if( hSHP == nullptr
            && (!EQUAL(CPLGetExtension(pszNewName),"dbf")
                || strstr(CPLGetLastErrorMsg(),".shp") == nullptr) )
        {
            CPLString osMsg = CPLGetLastErrorMsg();

            CPLError( CE_Failure, CPLE_OpenFailed, "%s", osMsg.c_str() );

            return false;
        }
        CPLErrorReset();
    }

/* -------------------------------------------------------------------- */
/*      Open the .dbf file, if it exists.  To open a dbf file, the      */
/*      filename has to either refer to a successfully opened shp       */
/*      file or has to refer to the actual .dbf file.                   */
/* -------------------------------------------------------------------- */
    DBFHandle hDBF = nullptr;
    if( hSHP != nullptr || EQUAL(CPLGetExtension(pszNewName), "dbf") )
    {
        if( bRealUpdateAccess )
        {
            hDBF = DS_DBFOpen( pszNewName, "r+" );
            if( hSHP != nullptr && hDBF == nullptr )
            {
                for( int i = 0; i < 2; i++ )
                {
                    VSIStatBufL sStat;
                    const char* pszDBFName =
                        CPLResetExtension(pszNewName,
                                          (i == 0 ) ? "dbf" : "DBF");
                    VSILFILE* fp = nullptr;
                    if( VSIStatExL( pszDBFName, &sStat,
                                    VSI_STAT_EXISTS_FLAG) == 0 )
                    {
                        fp = VSIFOpenL(pszDBFName, "r+");
                        if( fp == nullptr )
                        {
                            CPLError(
                                CE_Failure, CPLE_OpenFailed,
                                "%s exists, "
                                "but cannot be opened in update mode",
                                pszDBFName );
                            SHPClose(hSHP);
                            return false;
                        }
                        VSIFCloseL(fp);
                        break;
                    }
                }
            }
        }
        else
        {
            hDBF = DS_DBFOpen( pszNewName, "r" );
        }
    }
    else
    {
        hDBF = nullptr;
    }

    if( hDBF == nullptr && hSHP == nullptr )
        return false;

/* -------------------------------------------------------------------- */
/*      Create the layer object.                                        */
/* -------------------------------------------------------------------- */
    OGRShapeLayer *poLayer =
        new OGRShapeLayer( this, pszNewName, hSHP, hDBF, nullptr, false, bUpdate,
                           wkbNone );
    poLayer->SetModificationDate(
        CSLFetchNameValue( papszOpenOptions, "DBF_DATE_LAST_UPDATE" ) );
    poLayer->SetAutoRepack(
        CPLFetchBool( papszOpenOptions, "AUTO_REPACK", true ) );
    poLayer->SetWriteDBFEOFChar(
        CPLFetchBool( papszOpenOptions, "DBF_EOF_CHAR", true ) );

/* -------------------------------------------------------------------- */
/*      Add layer to data source layer list.                            */
/* -------------------------------------------------------------------- */
    AddLayer(poLayer);

    return true;
}

/************************************************************************/
/*                             AddLayer()                               */
/************************************************************************/

void OGRShapeDataSource::AddLayer( OGRShapeLayer* poLayer )
{
    papoLayers = reinterpret_cast<OGRShapeLayer **>(
        CPLRealloc( papoLayers,  sizeof(OGRShapeLayer *) * (nLayers+1) ) );
    papoLayers[nLayers++] = poLayer;

    // If we reach the limit, then register all the already opened layers
    // Technically this code would not be necessary if there was not the
    // following initial test in SetLastUsedLayer() :
    //      if (nLayers < MAX_SIMULTANEOUSLY_OPENED_LAYERS)
    //         return;
    if( nLayers == poPool->GetMaxSimultaneouslyOpened() &&
        poPool->GetSize() == 0 )
    {
        for( int i = 0; i < nLayers; i++ )
            poPool->SetLastUsedLayer(papoLayers[i]);
    }
}

/************************************************************************/
/*                        LaunderLayerName()                            */
/************************************************************************/

static CPLString LaunderLayerName(const char* pszLayerName)
{
    std::string osRet(CPLLaunderForFilename(pszLayerName, nullptr));
    if( osRet != pszLayerName )
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "Invalid layer name for a shapefile: %s. Laundered to %s.",
                 pszLayerName, osRet.c_str());
    }
    return osRet;
}

/************************************************************************/
/*                           ICreateLayer()                             */
/************************************************************************/

OGRLayer *
OGRShapeDataSource::ICreateLayer( const char * pszLayerName,
                                  OGRSpatialReference *poSRS,
                                  OGRwkbGeometryType eType,
                                  char ** papszOptions )

{
    // To ensure that existing layers are created.
    GetLayerCount();

/* -------------------------------------------------------------------- */
/*      Check that the layer doesn't already exist.                     */
/* -------------------------------------------------------------------- */
    if (GetLayerByName(pszLayerName) != nullptr)
    {
        CPLError( CE_Failure, CPLE_AppDefined, "Layer '%s' already exists",
                  pszLayerName);
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Verify we are in update mode.                                   */
/* -------------------------------------------------------------------- */
    if( !bDSUpdate )
    {
        CPLError( CE_Failure, CPLE_NoWriteAccess,
                  "Data source %s opened read-only.  "
                  "New layer %s cannot be created.",
                  pszName, pszLayerName );

        return nullptr;
    }

    if( m_bIsZip && m_bSingleLayerZip && nLayers == 1 )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  ".shz only supports one single layer");
        return nullptr;
    }

    if( !UncompressIfNeeded() )
        return nullptr;

/* -------------------------------------------------------------------- */
/*      Figure out what type of layer we need.                          */
/* -------------------------------------------------------------------- */
    int nShapeType = -1;

    if( wkbFlatten(eType) == wkbUnknown || eType == wkbLineString )
        nShapeType = SHPT_ARC;
    else if( eType == wkbPoint )
        nShapeType = SHPT_POINT;
    else if( eType == wkbPolygon || eType == wkbTriangle )
        nShapeType = SHPT_POLYGON;
    else if( eType == wkbMultiPoint )
        nShapeType = SHPT_MULTIPOINT;
    else if( eType == wkbPoint25D )
        nShapeType = SHPT_POINTZ;
    else if( eType == wkbPointM )
        nShapeType = SHPT_POINTM;
    else if( eType == wkbPointZM )
        nShapeType = SHPT_POINTZ;
    else if( eType == wkbLineString25D )
        nShapeType = SHPT_ARCZ;
    else if( eType == wkbLineStringM )
        nShapeType = SHPT_ARCM;
    else if( eType == wkbLineStringZM )
        nShapeType = SHPT_ARCZ;
    else if( eType == wkbMultiLineString )
        nShapeType = SHPT_ARC;
    else if( eType == wkbMultiLineString25D )
        nShapeType = SHPT_ARCZ;
    else if( eType == wkbMultiLineStringM )
        nShapeType = SHPT_ARCM;
    else if( eType == wkbMultiLineStringZM )
        nShapeType = SHPT_ARCZ;
    else if( eType == wkbPolygon25D || eType == wkbTriangleZ )
        nShapeType = SHPT_POLYGONZ;
    else if( eType == wkbPolygonM || eType == wkbTriangleM )
        nShapeType = SHPT_POLYGONM;
    else if( eType == wkbPolygonZM || eType == wkbTriangleZM )
        nShapeType = SHPT_POLYGONZ;
    else if( eType == wkbMultiPolygon )
        nShapeType = SHPT_POLYGON;
    else if( eType == wkbMultiPolygon25D )
        nShapeType = SHPT_POLYGONZ;
    else if( eType == wkbMultiPolygonM )
        nShapeType = SHPT_POLYGONM;
    else if( eType == wkbMultiPolygonZM )
        nShapeType = SHPT_POLYGONZ;
    else if( eType == wkbMultiPoint25D )
        nShapeType = SHPT_MULTIPOINTZ;
    else if( eType == wkbMultiPointM )
        nShapeType = SHPT_MULTIPOINTM;
    else if( eType == wkbMultiPointZM )
        nShapeType = SHPT_MULTIPOINTZ;
    else if( wkbFlatten(eType) == wkbTIN ||
             wkbFlatten(eType) == wkbPolyhedralSurface )
        nShapeType = SHPT_MULTIPATCH;
    else if( eType == wkbNone )
        nShapeType = SHPT_NULL;

/* -------------------------------------------------------------------- */
/*      Has the application overridden this with a special creation     */
/*      option?                                                         */
/* -------------------------------------------------------------------- */
    const char *pszOverride = CSLFetchNameValue( papszOptions, "SHPT" );

    if( pszOverride == nullptr )
    {
        /* ignore */;
    }
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
    else if( EQUAL(pszOverride,"POINTM") )
    {
        nShapeType = SHPT_POINTM;
        eType = wkbPointM;
    }
    else if( EQUAL(pszOverride,"ARCM") )
    {
        nShapeType = SHPT_ARCM;
        eType = wkbLineStringM;
    }
    else if( EQUAL(pszOverride,"POLYGONM") )
    {
        nShapeType = SHPT_POLYGONM;
        eType = wkbPolygonM;
    }
    else if( EQUAL(pszOverride,"MULTIPOINTM") )
    {
        nShapeType = SHPT_MULTIPOINTM;
        eType = wkbMultiPointM;
    }
    else if( EQUAL(pszOverride,"POINTZM") )
    {
        nShapeType = SHPT_POINTZ;
        eType = wkbPointZM;
    }
    else if( EQUAL(pszOverride,"ARCZM") )
    {
        nShapeType = SHPT_ARCZ;
        eType = wkbLineStringZM;
    }
    else if( EQUAL(pszOverride,"POLYGONZM") )
    {
        nShapeType = SHPT_POLYGONZ;
        eType = wkbPolygonZM;
    }
    else if( EQUAL(pszOverride,"MULTIPOINTZM") )
    {
        nShapeType = SHPT_MULTIPOINTZ;
        eType = wkbMultiPointZM;
    }
    else if( EQUAL(pszOverride,"MULTIPATCH") )
    {
        nShapeType = SHPT_MULTIPATCH;
        eType = wkbUnknown; // not ideal...
    }
    else if( EQUAL(pszOverride,"NONE") || EQUAL(pszOverride,"NULL") )
    {
        nShapeType = SHPT_NULL;
        eType = wkbNone;
    }
    else
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Unknown SHPT value of `%s' passed to Shapefile layer"
                  "creation.  Creation aborted.",
                  pszOverride );

        return nullptr;
    }

    if( nShapeType == -1 )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Geometry type of `%s' not supported in shapefiles.  "
                  "Type can be overridden with a layer creation option "
                  "of SHPT=POINT/ARC/POLYGON/MULTIPOINT/POINTZ/ARCZ/POLYGONZ/"
                  "MULTIPOINTZ/MULTIPATCH.",
                  OGRGeometryTypeToName(eType) );
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      What filename do we use, excluding the extension?               */
/* -------------------------------------------------------------------- */
    char *pszFilenameWithoutExt = nullptr;

    if( bSingleFileDataSource && nLayers == 0 )
    {
        char *pszPath = CPLStrdup(CPLGetPath(pszName));
        char *pszFBasename = CPLStrdup(CPLGetBasename(pszName));

        pszFilenameWithoutExt =
            CPLStrdup(CPLFormFilename(pszPath, pszFBasename, nullptr));

        CPLFree( pszFBasename );
        CPLFree( pszPath );
    }
    else if( bSingleFileDataSource )
    {
        // This is a very weird use case : the user creates/open a datasource
        // made of a single shapefile 'foo.shp' and wants to add a new layer
        // to it, 'bar'. So we create a new shapefile 'bar.shp' in the same
        // directory as 'foo.shp'
        // So technically, we will not be any longer a single file
        // datasource ... Ahem ahem.
        char *pszPath = CPLStrdup(CPLGetPath(pszName));
        pszFilenameWithoutExt =
            CPLStrdup(CPLFormFilename(pszPath, LaunderLayerName(pszLayerName).c_str(), nullptr));
        CPLFree( pszPath );
    }
    else
    {
        CPLString osDir( m_osTemporaryUnzipDir.empty() ? pszName : m_osTemporaryUnzipDir );
        pszFilenameWithoutExt =
            CPLStrdup(CPLFormFilename(osDir, LaunderLayerName(pszLayerName).c_str(), nullptr));
    }

/* -------------------------------------------------------------------- */
/*      Create the shapefile.                                           */
/* -------------------------------------------------------------------- */
    const bool l_b2GBLimit =
        CPLTestBool(CSLFetchNameValueDef( papszOptions, "2GB_LIMIT", "FALSE" ));

    SHPHandle hSHP = nullptr;

    if( nShapeType != SHPT_NULL )
    {
        char *pszFilename =
            CPLStrdup(CPLFormFilename( nullptr, pszFilenameWithoutExt, "shp" ));

        hSHP = SHPCreateLL(
            pszFilename, nShapeType,
            const_cast<SAHooks *>(VSI_SHP_GetHook(l_b2GBLimit)) );

        if( hSHP == nullptr )
        {
            CPLFree( pszFilename );
            CPLFree( pszFilenameWithoutExt );
            return nullptr;
        }

        SHPSetFastModeReadObject( hSHP, TRUE );

        CPLFree( pszFilename );
    }

/* -------------------------------------------------------------------- */
/*      Has a specific LDID been specified by the caller?               */
/* -------------------------------------------------------------------- */
    const char *pszLDID = CSLFetchNameValue( papszOptions, "ENCODING" );

/* -------------------------------------------------------------------- */
/*      Create a DBF file.                                              */
/* -------------------------------------------------------------------- */
    char *pszFilename =
        CPLStrdup(CPLFormFilename( nullptr, pszFilenameWithoutExt, "dbf" ));

    DBFHandle hDBF =
        DBFCreateLL( pszFilename, (pszLDID != nullptr) ? pszLDID : "LDID/87",
                     const_cast<SAHooks *>(VSI_SHP_GetHook(b2GBLimit)) );

    if( hDBF == nullptr )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Failed to open Shape DBF file `%s'.",
                  pszFilename );
        CPLFree( pszFilename );
        CPLFree( pszFilenameWithoutExt );
        SHPClose(hSHP);
        return nullptr;
    }

    CPLFree( pszFilename );

/* -------------------------------------------------------------------- */
/*      Create the .prj file, if required.                              */
/* -------------------------------------------------------------------- */
    if( poSRS != nullptr )
    {
        CPLString osPrjFile =
            CPLFormFilename( nullptr, pszFilenameWithoutExt, "prj");

        poSRS = poSRS->Clone();
        poSRS->morphToESRI();

        char *pszWKT = nullptr;
        VSILFILE *fp = nullptr;
        if( poSRS->exportToWkt( &pszWKT ) == OGRERR_NONE
            && (fp = VSIFOpenL( osPrjFile, "wt" )) != nullptr )
        {
            VSIFWriteL( pszWKT, strlen(pszWKT), 1, fp );
            VSIFCloseL( fp );
        }

        CPLFree( pszWKT );

        poSRS->morphFromESRI();
    }

/* -------------------------------------------------------------------- */
/*      Create the layer object.                                        */
/* -------------------------------------------------------------------- */
    // OGRShapeLayer constructor expects a filename with an extension (that
    // could be random actually), otherwise this is going to cause problems with
    // layer names that have a dot (not speaking about the one before the shp)
    pszFilename =
        CPLStrdup(CPLFormFilename( nullptr, pszFilenameWithoutExt, "shp" ));

    OGRShapeLayer *poLayer =
        new OGRShapeLayer( this, pszFilename, hSHP, hDBF, poSRS,
                           true, true, eType );
    if( poSRS != nullptr )
    {
        poSRS->Release();
    }

    CPLFree( pszFilenameWithoutExt );
    CPLFree( pszFilename );

    poLayer->SetResizeAtClose(
        CPLFetchBool( papszOptions, "RESIZE", false ) );
    poLayer->CreateSpatialIndexAtClose(
        CPLFetchBool( papszOptions, "SPATIAL_INDEX", false ) );
    poLayer->SetModificationDate(
        CSLFetchNameValue( papszOptions, "DBF_DATE_LAST_UPDATE" ) );
    poLayer->SetAutoRepack(
        CPLFetchBool( papszOptions, "AUTO_REPACK", true ) );
    poLayer->SetWriteDBFEOFChar(
        CPLFetchBool( papszOptions, "DBF_EOF_CHAR", true ) );

/* -------------------------------------------------------------------- */
/*      Add layer to data source layer list.                            */
/* -------------------------------------------------------------------- */
    AddLayer(poLayer);

    return poLayer;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRShapeDataSource::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,ODsCCreateLayer) )
        return bDSUpdate && !(m_bIsZip && m_bSingleLayerZip && nLayers == 1);
    if( EQUAL(pszCap,ODsCDeleteLayer) )
        return bDSUpdate && !(m_bIsZip && m_bSingleLayerZip);
    if( EQUAL(pszCap,ODsCMeasuredGeometries) )
        return TRUE;
    if( EQUAL(pszCap,ODsCRandomLayerWrite) )
        return bDSUpdate;

    return FALSE;
}

/************************************************************************/
/*                            GetLayerCount()                           */
/************************************************************************/

int OGRShapeDataSource::GetLayerCount()

{
#ifndef IMMEDIATE_OPENING
    if( !oVectorLayerName.empty() )
    {
        for( size_t i = 0; i < oVectorLayerName.size(); i++ )
        {
            const char* pszFilename = oVectorLayerName[i].c_str();
            const char* pszLayerName = CPLGetBasename(pszFilename);

            int j = 0;  // Used after for.
            for( ; j < nLayers; j++ )
            {
                if( strcmp(papoLayers[j]->GetName(), pszLayerName) == 0 )
                    break;
            }
            if( j < nLayers )
                continue;

            if( !OpenFile( pszFilename, bDSUpdate ) )
            {
                CPLError( CE_Failure, CPLE_OpenFailed,
                          "Failed to open file %s."
                          "It may be corrupt or read-only file accessed in "
                          "update mode.",
                          pszFilename );
            }
        }
        oVectorLayerName.resize(0);
    }
#endif

    return nLayers;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRShapeDataSource::GetLayer( int iLayer )

{
    // To ensure that existing layers are created.
    GetLayerCount();

    if( iLayer < 0 || iLayer >= nLayers )
        return nullptr;

    return papoLayers[iLayer];
}

/************************************************************************/
/*                           GetLayerByName()                           */
/************************************************************************/

OGRLayer *OGRShapeDataSource::GetLayerByName( const char * pszLayerNameIn )
{
#ifndef IMMEDIATE_OPENING
    if( !oVectorLayerName.empty() )
    {
        for( int j = 0; j < nLayers; j++ )
        {
            if (strcmp(papoLayers[j]->GetName(), pszLayerNameIn) == 0)
            {
                return papoLayers[j];
            }
        }

        for( int j = 0; j < 2; j++ )
        {
            for( size_t i = 0; i < oVectorLayerName.size(); i++ )
            {
                const char* pszFilename = oVectorLayerName[i].c_str();
                const char* pszLayerName = CPLGetBasename(pszFilename);

                if( j == 0 )
                {
                    if(strcmp(pszLayerName, pszLayerNameIn) != 0)
                        continue;
                }
                else
                {
                    if( !EQUAL(pszLayerName, pszLayerNameIn) )
                        continue;
                }

                if( !OpenFile( pszFilename, bDSUpdate ) )
                {
                    CPLError( CE_Failure, CPLE_OpenFailed,
                              "Failed to open file %s.  "
                              "It may be corrupt or read-only file accessed in "
                              "update mode.",
                              pszFilename );
                    return nullptr;
                }

                return papoLayers[nLayers - 1];
            }
        }

        return nullptr;
    }
#endif

    return OGRDataSource::GetLayerByName(pszLayerNameIn);
}

/************************************************************************/
/*                             ExecuteSQL()                             */
/*                                                                      */
/*      We override this to provide special handling of CREATE          */
/*      SPATIAL INDEX commands.  Support forms are:                     */
/*                                                                      */
/*        CREATE SPATIAL INDEX ON layer_name [DEPTH n]                  */
/*        DROP SPATIAL INDEX ON layer_name                              */
/*        REPACK layer_name                                             */
/*        RECOMPUTE EXTENT ON layer_name                                */
/************************************************************************/

OGRLayer * OGRShapeDataSource::ExecuteSQL( const char *pszStatement,
                                           OGRGeometry *poSpatialFilter,
                                           const char *pszDialect )

{
    if( EQUAL(pszStatement, "UNCOMPRESS") )
    {
        CPL_IGNORE_RET_VAL(UncompressIfNeeded());
        return nullptr;
    }

    if( EQUAL(pszStatement, "RECOMPRESS") )
    {
        RecompressIfNeeded(GetLayerNames());
        return nullptr;
    }
/* ==================================================================== */
/*      Handle command to drop a spatial index.                         */
/* ==================================================================== */
    if( STARTS_WITH_CI(pszStatement, "REPACK ") )
    {
        OGRShapeLayer *poLayer = cpl::down_cast<OGRShapeLayer *>(
            GetLayerByName( pszStatement + 7 ));

        if( poLayer != nullptr )
        {
            if( poLayer->Repack() != OGRERR_NONE )
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                          "REPACK of layer '%s' failed.",
                          pszStatement + 7 );
            }
        }
        else
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "No such layer as '%s' in REPACK.",
                      pszStatement + 7 );
        }
        return nullptr;
    }

/* ==================================================================== */
/*      Handle command to shrink columns to their minimum size.         */
/* ==================================================================== */
    if( STARTS_WITH_CI(pszStatement, "RESIZE ") )
    {
        OGRShapeLayer *poLayer = cpl::down_cast<OGRShapeLayer *>(
            GetLayerByName( pszStatement + 7 ));

        if( poLayer != nullptr )
        {
            poLayer->ResizeDBF();
        }
        else
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "No such layer as '%s' in RESIZE.",
                      pszStatement + 7 );
        }
        return nullptr;
    }

/* ==================================================================== */
/*      Handle command to recompute extent                             */
/* ==================================================================== */
    if( STARTS_WITH_CI(pszStatement, "RECOMPUTE EXTENT ON ") )
    {
        OGRShapeLayer *poLayer = cpl::down_cast<OGRShapeLayer *>(
            GetLayerByName( pszStatement + 20 ));

        if( poLayer != nullptr )
        {
            poLayer->RecomputeExtent();
        }
        else
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "No such layer as '%s' in RECOMPUTE EXTENT.",
                      pszStatement + 20 );
        }
        return nullptr;
    }

/* ==================================================================== */
/*      Handle command to drop a spatial index.                         */
/* ==================================================================== */
    if( STARTS_WITH_CI(pszStatement, "DROP SPATIAL INDEX ON ") )
    {
        OGRShapeLayer *poLayer = cpl::down_cast<OGRShapeLayer *>(
            GetLayerByName( pszStatement + 22 ));

        if( poLayer != nullptr )
        {
            poLayer->DropSpatialIndex();
        }
        else
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "No such layer as '%s' in DROP SPATIAL INDEX.",
                      pszStatement + 22 );
        }
        return nullptr;
    }

/* ==================================================================== */
/*      Handle all commands except spatial index creation generically.  */
/* ==================================================================== */
    if( !STARTS_WITH_CI(pszStatement, "CREATE SPATIAL INDEX ON ") )
    {
        char **papszTokens = CSLTokenizeString( pszStatement );
        if( CSLCount(papszTokens) >=4
            && (EQUAL(papszTokens[0],"CREATE") || EQUAL(papszTokens[0],"DROP"))
            && EQUAL(papszTokens[1],"INDEX")
            && EQUAL(papszTokens[2],"ON") )
        {
            OGRShapeLayer *poLayer = cpl::down_cast<OGRShapeLayer *>(
                GetLayerByName(papszTokens[3]));
            if( poLayer != nullptr )
                poLayer->InitializeIndexSupport( poLayer->GetFullName() );
        }
        CSLDestroy( papszTokens );

        return OGRDataSource::ExecuteSQL( pszStatement, poSpatialFilter,
                                          pszDialect );
    }

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
                  "Should be of form 'CREATE SPATIAL INDEX ON <table> "
                  "[DEPTH <n>]'",
                  pszStatement );
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Get depth if provided.                                          */
/* -------------------------------------------------------------------- */
    const int nDepth =
        CSLCount(papszTokens) == 7 ? atoi(papszTokens[6]) : 0;

/* -------------------------------------------------------------------- */
/*      What layer are we operating on.                                 */
/* -------------------------------------------------------------------- */
    OGRShapeLayer *poLayer = cpl::down_cast<OGRShapeLayer *>(
        GetLayerByName(papszTokens[4]));

    if( poLayer == nullptr )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Layer %s not recognised.",
                  papszTokens[4] );
        CSLDestroy( papszTokens );
        return nullptr;
    }

    CSLDestroy( papszTokens );

    poLayer->CreateSpatialIndex( nDepth );
    return nullptr;
}

/************************************************************************/
/*                     GetExtensionsForDeletion()                       */
/************************************************************************/

const char* const* OGRShapeDataSource::GetExtensionsForDeletion()
{
    static const char * const apszExtensions[] =
        { "shp", "shx", "dbf", "sbn", "sbx", "prj", "idm", "ind",
          "qix", "cpg",
          "qpj", // QGIS projection file
          nullptr };
    return apszExtensions;
}

/************************************************************************/
/*                            DeleteLayer()                             */
/************************************************************************/

OGRErr OGRShapeDataSource::DeleteLayer( int iLayer )

{
/* -------------------------------------------------------------------- */
/*      Verify we are in update mode.                                   */
/* -------------------------------------------------------------------- */
    if( !bDSUpdate )
    {
        CPLError( CE_Failure, CPLE_NoWriteAccess,
                  "Data source %s opened read-only.  "
                  "Layer %d cannot be deleted.",
                  pszName, iLayer );

        return OGRERR_FAILURE;
    }

    // To ensure that existing layers are created.
    GetLayerCount();

    if( iLayer < 0 || iLayer >= nLayers )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Layer %d not in legal range of 0 to %d.",
                  iLayer, nLayers-1 );
        return OGRERR_FAILURE;
    }

    if( m_bIsZip && m_bSingleLayerZip )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  ".shz does not support layer deletion");
        return OGRERR_FAILURE;
    }

    if( !UncompressIfNeeded() )
        return OGRERR_FAILURE;

    OGRShapeLayer* poLayerToDelete = papoLayers[iLayer];

    char * const pszFilename = CPLStrdup(poLayerToDelete->GetFullName());

    delete poLayerToDelete;

    while( iLayer < nLayers - 1 )
    {
        papoLayers[iLayer] = papoLayers[iLayer+1];
        iLayer++;
    }

    nLayers--;

    const char * const* papszExtensions =
        OGRShapeDataSource::GetExtensionsForDeletion();
    for( int iExt = 0; papszExtensions[iExt] != nullptr; iExt++ )
    {
        const char *pszFile = CPLResetExtension(pszFilename,
                                                papszExtensions[iExt]);
        VSIStatBufL sStatBuf;
        if( VSIStatL( pszFile, &sStatBuf ) == 0 )
            VSIUnlink( pszFile );
    }

    CPLFree( pszFilename );

    return OGRERR_NONE;
}

/************************************************************************/
/*                          SetLastUsedLayer()                          */
/************************************************************************/

void OGRShapeDataSource::SetLastUsedLayer( OGRShapeLayer* poLayer )
{
    // We could remove that check and things would still work in
    // 99.99% cases.
    // The only rationale for that test is to avoid breaking applications that
    // would deal with layers of the same datasource in different threads. In
    // GDAL < 1.9.0, this would work in most cases I can imagine as shapefile
    // layers are pretty much independent from each others (although it has
    // never been guaranteed to be a valid use case, and the shape driver is
    // likely more the exception than the rule in permitting accessing layers
    // from different threads !)  Anyway the LRU list mechanism leaves the door
    // open to concurrent accesses to it so when the datasource has not many
    // layers, we don't try to build the LRU list to avoid concurrency issues. I
    // haven't bothered making the analysis of how a mutex could be used to
    // protect that (my intuition is that it would need to be placed at the
    // beginning of OGRShapeLayer::TouchLayer() ).
    if (nLayers < poPool->GetMaxSimultaneouslyOpened())
        return;

    poPool->SetLastUsedLayer(poLayer);
}

/************************************************************************/
//                            GetFileList()                             */
/************************************************************************/

char** OGRShapeDataSource::GetFileList()
{
    if( m_bIsZip )
    {
        return CSLAddString(nullptr, pszName);
    }
    CPLStringList oFileList;
    GetLayerCount();
    for( int i = 0; i < nLayers; i++ )
    {
        OGRShapeLayer* poLayer = papoLayers[i];
        poLayer->AddToFileList(oFileList);
    }
    return oFileList.StealList();
}

/************************************************************************/
//                          RefreshLockFile()                            */
/************************************************************************/

void OGRShapeDataSource::RefreshLockFile(void* _self)
{
    OGRShapeDataSource* self = static_cast<OGRShapeDataSource*>(_self);
    CPLAssert(self->m_psLockFile);
    CPLAcquireMutex(self->m_poRefreshLockFileMutex, 1000);
    CPLCondSignal(self->m_poRefreshLockFileCond);
    unsigned int nInc = 0;
    while(!(self->m_bExitRefreshLockFileThread))
    {
        auto ret = CPLCondTimedWait(self->m_poRefreshLockFileCond,
                                    self->m_poRefreshLockFileMutex,
                                    self->m_dfRefreshLockDelay);
        if( ret == COND_TIMED_WAIT_TIME_OUT )
        {
            CPLAssert(self->m_psLockFile);
            VSIFSeekL(self->m_psLockFile, 0, SEEK_SET);
            CPLString osTime;
            nInc++;
            osTime.Printf(CPL_FRMT_GUIB ", %u\n",
                          static_cast<GUIntBig>(time(nullptr)),
                          nInc);
            VSIFWriteL(osTime.data(), 1, osTime.size(), self->m_psLockFile);
            VSIFFlushL(self->m_psLockFile);
        }
    }
    CPLReleaseMutex(self->m_poRefreshLockFileMutex);
}

/************************************************************************/
//                            RemoveLockFile()                          */
/************************************************************************/

void OGRShapeDataSource::RemoveLockFile()
{
    if( !m_psLockFile )
        return;

    // Ask the thread to terminate
    CPLAcquireMutex(m_poRefreshLockFileMutex, 1000);
    m_bExitRefreshLockFileThread = true;
    CPLCondSignal(m_poRefreshLockFileCond);
    CPLReleaseMutex(m_poRefreshLockFileMutex);
    CPLJoinThread(m_hRefreshLockFileThread);
    m_hRefreshLockFileThread = nullptr;

    // Close and remove lock file
    VSIFCloseL(m_psLockFile);
    m_psLockFile = nullptr;
    CPLString osLockFile(pszName);
    osLockFile += ".gdal.lock";
    VSIUnlink(osLockFile);
}

/************************************************************************/
//                         UncompressIfNeeded()                         */
/************************************************************************/

bool OGRShapeDataSource::UncompressIfNeeded()
{
    if( !bDSUpdate || !m_bIsZip || !m_osTemporaryUnzipDir.empty() )
        return true;

    GetLayerCount();

    auto returnError = [this]()
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot uncompress %s", pszName);
        return false;
    };

    if( nLayers > 1 )
    {
        CPLString osLockFile(pszName);
        osLockFile += ".gdal.lock";
        VSIStatBufL sStat;
        if( VSIStatL(osLockFile, &sStat) == 0 &&
            sStat.st_mtime > time(nullptr) - 2 * knREFRESH_LOCK_FILE_DELAY_SEC )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cannot edit %s. Another task is editing it", pszName);
            return false;
        }
        if( !m_poRefreshLockFileMutex )
        {
            m_poRefreshLockFileMutex = CPLCreateMutex();
            if( !m_poRefreshLockFileMutex )
                return false;
            CPLReleaseMutex(m_poRefreshLockFileMutex);
        }
        if( !m_poRefreshLockFileCond )
        {
            m_poRefreshLockFileCond = CPLCreateCond();
            if( !m_poRefreshLockFileCond )
                return false;
        }
        auto f = VSIFOpenL(osLockFile, "wb");
        if( !f )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cannot create lock file");
            return false;
        }
        m_psLockFile = f;
        m_bExitRefreshLockFileThread = false;
        // Config option mostly for testing purposes
        // coverity[tainted_data]
        m_dfRefreshLockDelay = CPLAtof(
            CPLGetConfigOption("OGR_SHAPE_LOCK_DELAY",
                            CPLSPrintf("%d", knREFRESH_LOCK_FILE_DELAY_SEC)));
        m_hRefreshLockFileThread = CPLCreateJoinableThread(
            OGRShapeDataSource::RefreshLockFile, this);
        if( !m_hRefreshLockFileThread )
        {
            VSIFCloseL(m_psLockFile);
            m_psLockFile = nullptr;
            VSIUnlink(osLockFile);
        }
        else
        {
            CPLAcquireMutex(m_poRefreshLockFileMutex, 1000);
            CPLCondWait(m_poRefreshLockFileCond, m_poRefreshLockFileMutex);
            CPLReleaseMutex(m_poRefreshLockFileMutex);
        }
    }

    CPLString osVSIZipDirname(GetVSIZipPrefixeDir());
    vsi_l_offset nTotalUncompressedSize = 0;
    CPLStringList aosFiles( VSIReadDir( osVSIZipDirname ));
    for( int i = 0; i < aosFiles.size(); i++ )
    {
        const char* pszFilename = aosFiles[i];
        if( !EQUAL(pszFilename, ".") && !EQUAL(pszFilename, "..") )
        {
            CPLString osSrcFile( CPLFormFilename(
                osVSIZipDirname, pszFilename, nullptr) );
            VSIStatBufL sStat;
            if( VSIStatL(osSrcFile, &sStat) == 0 )
            {
                nTotalUncompressedSize += sStat.st_size;
            }
        }
    }

    CPLString osTemporaryDir(pszName);
    osTemporaryDir += "_tmp_uncompressed";

    const char* pszUseVsimem = CPLGetConfigOption("OGR_SHAPE_USE_VSIMEM_FOR_TEMP", "AUTO");
    if( EQUAL(pszUseVsimem, "YES") ||
        (EQUAL(pszUseVsimem, "AUTO") &&
         nTotalUncompressedSize > 0 &&
         nTotalUncompressedSize < static_cast<GUIntBig>(CPLGetUsablePhysicalRAM() / 10)) )
    {
        osTemporaryDir = CPLSPrintf("/vsimem/_shapedriver/%p", this);
    }
    CPLDebug("Shape", "Uncompressing to %s", osTemporaryDir.c_str());

    VSIRmdirRecursive(osTemporaryDir);
    if( VSIMkdir(osTemporaryDir, 0755) != 0 )
        return returnError();
    for( int i = 0; i < aosFiles.size(); i++ )
    {
        const char* pszFilename = aosFiles[i];
        if( !EQUAL(pszFilename, ".") && !EQUAL(pszFilename, "..") )
        {
            CPLString osSrcFile( CPLFormFilename(
                osVSIZipDirname, pszFilename, nullptr) );
            CPLString osDestFile( CPLFormFilename(
                osTemporaryDir, pszFilename, nullptr) );
            if( CPLCopyFile(osDestFile, osSrcFile) != 0 )
            {
                VSIRmdirRecursive(osTemporaryDir);
                return returnError();
            }
        }
    }

    m_osTemporaryUnzipDir = osTemporaryDir;

    for( int i = 0; i < nLayers; i++ )
    {
        OGRShapeLayer* poLayer = papoLayers[i];
        poLayer->UpdateFollowingDeOrRecompression();
    }

    return true;
}

/************************************************************************/
//                         RecompressIfNeeded()                         */
/************************************************************************/

bool OGRShapeDataSource::RecompressIfNeeded(const std::vector<CPLString>& layerNames)
{
    if( !bDSUpdate || !m_bIsZip || m_osTemporaryUnzipDir.empty() )
        return true;

    auto returnError = [this]()
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot recompress %s", pszName);
        RemoveLockFile();
        return false;
    };

    CPLStringList aosFiles( VSIReadDir( m_osTemporaryUnzipDir ));
    CPLString osTmpZip(m_osTemporaryUnzipDir + ".zip");
    VSIUnlink(osTmpZip);
    CPLString osTmpZipWithVSIZip( "/vsizip/{" + osTmpZip + '}' );

    std::map<CPLString, int> oMapLayerOrder;
    for( size_t i = 0; i < layerNames.size(); i++ )
        oMapLayerOrder[layerNames[i]] = static_cast<int>(i);

    std::vector<CPLString> sortedFiles;
    vsi_l_offset nTotalUncompressedSize = 0;
    for(int i = 0; i < aosFiles.size(); i++ )
    {
        sortedFiles.emplace_back(aosFiles[i]);
        CPLString osSrcFile( CPLFormFilename(
                m_osTemporaryUnzipDir, aosFiles[i], nullptr) );
        VSIStatBufL sStat;
        if( VSIStatL(osSrcFile, &sStat) == 0 )
        {
            nTotalUncompressedSize += sStat.st_size;
        }
    }

    // Sort files by their layer orders, and then for files of the same layer,
    // make shp appear first, and then by filename order
    std::sort(sortedFiles.begin(), sortedFiles.end(),
        [&oMapLayerOrder](const CPLString& a, const CPLString& b)
        {
            int iA = INT_MAX;
            auto oIterA = oMapLayerOrder.find(CPLGetBasename(a));
            if( oIterA != oMapLayerOrder.end() )
                iA = oIterA->second;
            int iB = INT_MAX;
            auto oIterB = oMapLayerOrder.find(CPLGetBasename(b));
            if( oIterB != oMapLayerOrder.end() )
                iB = oIterB->second;
            if( iA < iB )
                return true;
            if( iA > iB )
                return false;
            if( iA != INT_MAX )
            {
                const char* pszExtA = CPLGetExtension(a);
                const char* pszExtB = CPLGetExtension(b);
                if( EQUAL(pszExtA, "shp") )
                    return true;
                if( EQUAL(pszExtB, "shp") )
                    return false;
            }
            return a < b;
        }
    );

    CPLConfigOptionSetter oZIP64Setter(
        "CPL_CREATE_ZIP64",
        nTotalUncompressedSize < 4000U * 1000 * 1000 ? "NO" : "YES", true);

    /* Maintain a handle on the ZIP opened */
    VSILFILE* fpZIP = VSIFOpenExL(osTmpZipWithVSIZip, "wb", true);
    if (fpZIP == nullptr)
    {
        CPLError(CE_Failure, CPLE_FileIO,
                "Cannot create %s: %s", osTmpZipWithVSIZip.c_str(),
                 VSIGetLastErrorMsg());
        return returnError();
    }

    for( const auto& osFilename: sortedFiles )
    {
        const char* pszFilename = osFilename.c_str();
        if( !EQUAL(pszFilename, ".") && !EQUAL(pszFilename, "..") )
        {
            CPLString osSrcFile( CPLFormFilename(
                m_osTemporaryUnzipDir, pszFilename, nullptr) );
            CPLString osDestFile( CPLFormFilename(
                osTmpZipWithVSIZip, pszFilename, nullptr) );
            if( CPLCopyFile(osDestFile, osSrcFile) != 0 )
            {
                VSIFCloseL(fpZIP);
                return returnError();
            }
        }
    }

    VSIFCloseL(fpZIP);

    const bool bOverwrite =
        CPLTestBool(CPLGetConfigOption("OGR_SHAPE_PACK_IN_PLACE",
#ifdef WIN32
                                        "YES"
#else
                                        "NO"
#endif
                                        ));
    if( bOverwrite )
    {
        VSILFILE* fpTarget = nullptr;
        for( int i = 0; i < 10; i++ )
        {
            fpTarget = VSIFOpenL(pszName, "rb+");
            if( fpTarget )
                break;
            CPLSleep(0.1);
        }
        if( !fpTarget )
            return returnError();
        bool bCopyOK = CopyInPlace(fpTarget, osTmpZip);
        VSIFCloseL(fpTarget);
        VSIUnlink(osTmpZip);
        if( !bCopyOK )
        {
            return returnError();
        }
    }
    else
    {
        if( VSIUnlink(pszName) != 0 ||
            CPLMoveFile(pszName, osTmpZip) != 0 )
        {
            return returnError();
        }
    }

    VSIRmdirRecursive(m_osTemporaryUnzipDir);
    m_osTemporaryUnzipDir.clear();

    for( int i = 0; i < nLayers; i++ )
    {
        OGRShapeLayer* poLayer = papoLayers[i];
        poLayer->UpdateFollowingDeOrRecompression();
    }

    RemoveLockFile();

    return true;
}

/************************************************************************/
/*                            CopyInPlace()                             */
/************************************************************************/

bool OGRShapeDataSource::CopyInPlace( VSILFILE* fpTarget,
                                      const CPLString& osSourceFilename )
{
    return CPL_TO_BOOL(VSIOverwriteFile(fpTarget, osSourceFilename.c_str()));
}
