/****************************************************************************
 *
 * Project:  DB2 Spatial driver
 * Purpose:  Implements OGRDB2DataSource class
 * Author:   David Adler, dadler at adtechgeospatial dot com
 *
 ****************************************************************************
 * Copyright (c) 2010, Tamas Szekeres
 * Copyright (c) 2015, David Adler
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

#include "ogr_db2.h"

CPL_CVSID("$Id$")

static GPKGTileFormat GetTileFormat(const char* pszTF );

/* layer status */
#define DB2LAYERSTATUS_ORIGINAL 0
#define DB2LAYERSTATUS_INITIAL  1
#define DB2LAYERSTATUS_CREATED  2
#define DB2LAYERSTATUS_DISABLED 3

/************************************************************************/
/*                             Tiling schemes                           */
/************************************************************************/

typedef struct
{
    const char* pszName;
    int         nEPSGCode;
    double      dfMinX;
    double      dfMaxY;
    int         nTileXCountZoomLevel0;
    int         nTileYCountZoomLevel0;
    int         nTileWidth;
    int         nTileHeight;
    double      dfPixelXSizeZoomLevel0;
    double      dfPixelYSizeZoomLevel0;
} TilingSchemeDefinition;

static const TilingSchemeDefinition asTilingShemes[] =
{
    /* See http://portal.opengeospatial.org/files/?artifact_id=35326 (WMTS 1.0), Annex E.3 */
    {   "GoogleCRS84Quad",
        4326,
        -180.0, 180.0,
        1, 1,
        256, 256,
        360.0 / 256, 360.0 / 256
    },

    /* See http://portal.opengeospatial.org/files/?artifact_id=35326 (WMTS 1.0), Annex E.4 */
    {   "GoogleMapsCompatible",
        3857,
        -(156543.0339280410*256) /2, (156543.0339280410*256) /2,
        1, 1,
        256, 256,
        156543.0339280410, 156543.0339280410
    },

    /* See InspireCRS84Quad at http://inspire.ec.europa.eu/documents/Network_Services/TechnicalGuidance_ViewServices_v3.0.pdf */
    /* This is exactly the same as PseudoTMS_GlobalGeodetic */
    {   "InspireCRS84Quad",
        4326,
        -180.0, 90.0,
        2, 1,
        256, 256,
        180.0 / 256, 180.0 / 256
    },

    /* See global-geodetic at http://wiki.osgeo.org/wiki/Tile_Map_Service_Specification */
    {   "PseudoTMS_GlobalGeodetic",
        4326,
        -180.0, 90.0,
        2, 1,
        256, 256,
        180.0 / 256, 180.0 / 256
    },

    /* See global-mercator at http://wiki.osgeo.org/wiki/Tile_Map_Service_Specification */
    {   "PseudoTMS_GlobalMercator",
        3857,
        -20037508.34, 20037508.34,
        2, 2,
        256, 256,
        78271.516, 78271.516
    },
};

/************************************************************************/
/*                          OGRDB2DataSource()                 */
/************************************************************************/

OGRDB2DataSource::OGRDB2DataSource()

{
    clock1 = clock();
    time1 = time(nullptr);
    m_bIsVector = TRUE;
    m_papszTableNames = nullptr;
    m_papszSchemaNames = nullptr;
    m_papszGeomColumnNames = nullptr;
    m_papszCoordDimensions = nullptr;
    m_papszSRIds = nullptr;
    m_papszSRTexts = nullptr;
    m_pszName = nullptr;
    m_pszCatalog = nullptr;

    m_bHasMetadataTables = FALSE;
    m_nKnownSRID = 0;
    m_panSRID = nullptr;
    m_papoSRS = nullptr;

    bUseGeometryColumns = CPLTestBool(CPLGetConfigOption(
            "DB2SPATIAL_USE_GEOMETRY_COLUMNS",
            "YES"));
    bListAllTables = CPLTestBool(CPLGetConfigOption(
                                        "DB2SPATIAL_LIST_ALL_TABLES", "NO"));

// from GPKG
    m_bUpdate = FALSE;
    m_bNew = FALSE;
    m_papoLayers = nullptr;
    m_nLayers = 0;
    m_bUtf8 = FALSE;
    m_bIdentifierAsCO = FALSE;
    m_bDescriptionAsCO = FALSE;
    m_bHasReadMetadataFromStorage = FALSE;
    m_bMetadataDirty = FALSE;
    m_papszSubDatasets = nullptr;
    m_pszProjection = nullptr;
    m_bRecordInsertedInGPKGContent = FALSE;
    m_bGeoTransformValid = FALSE;
    m_nSRID = -1; /* unknown cartesian */
    m_adfGeoTransform[0] = 0.0;
    m_adfGeoTransform[1] = 1.0;
    m_adfGeoTransform[2] = 0.0;
    m_adfGeoTransform[3] = 0.0;
    m_adfGeoTransform[4] = 0.0;
    m_adfGeoTransform[5] = 1.0;
    m_nZoomLevel = -1;
    m_pabyCachedTiles = nullptr;
    for(int i=0; i<4; i++)
    {
        m_asCachedTilesDesc[i].nRow = -1;
        m_asCachedTilesDesc[i].nCol = -1;
        m_asCachedTilesDesc[i].nIdxWithinTileData = -1;
        m_asCachedTilesDesc[i].abBandDirty[0] = FALSE;
        m_asCachedTilesDesc[i].abBandDirty[1] = FALSE;
        m_asCachedTilesDesc[i].abBandDirty[2] = FALSE;
        m_asCachedTilesDesc[i].abBandDirty[3] = FALSE;
    }
    m_nShiftXTiles = 0;
    m_nShiftXPixelsMod = 0;
    m_nShiftYTiles = 0;
    m_nShiftYPixelsMod = 0;
    m_eTF = GPKG_TF_PNG_JPEG;
    m_nTileMatrixWidth = 0;
    m_nTileMatrixHeight = 0;
    m_nZLevel = 6;
    m_nQuality = 75;
    m_bDither = FALSE;
    m_poParentDS = nullptr;
    m_nOverviewCount = 0;
    m_papoOverviewDS = nullptr;
    m_bZoomOther = FALSE;
    m_bTriedEstablishingCT = FALSE;
    m_pabyHugeColorArray = nullptr;
    m_poCT = nullptr;
    m_bInWriteTile = FALSE;
    //m_hTempDB = NULL;
    m_bInFlushCache = FALSE;
    m_nTileInsertionCount = 0;
    m_osTilingScheme = "CUSTOM";
}

/************************************************************************/
/*                         ~OGRDB2DataSource()                 */
/************************************************************************/

OGRDB2DataSource::~OGRDB2DataSource()

{
    DB2_DEBUG_ENTER("OGRDB2DataSource::~OGRDB2DataSource");
    int         i;
    SetPamFlags(0);
    CPLFree( m_pszName );
    CPLFree( m_pszCatalog );
    CSLDestroy( m_papszTableNames );
    CSLDestroy( m_papszSchemaNames );
    CSLDestroy( m_papszGeomColumnNames );
    CSLDestroy( m_papszCoordDimensions );
    CSLDestroy( m_papszSRIds );
    CSLDestroy( m_papszSRTexts );

    if (!m_bIsVector)
    {
        if( m_poParentDS == nullptr && !m_osRasterTable.empty() &&
                !m_bGeoTransformValid )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                    "Raster table %s not correctly initialized due to missing call "
                    "to SetGeoTransform()",
                    m_osRasterTable.c_str());
        }

        OGRDB2DataSource::FlushCache();
        FlushMetadata();
    }

    for( i = 0; i < m_nLayers; i++ )
        delete m_papoLayers[i];

    CPLFree( m_papoLayers );

    for( i = 0; i < m_nKnownSRID; i++ )
    {
        if( m_papoSRS[i] != nullptr )
            CPLDebug("OGRDB2DataSource::~OGRDB2DataSource","m_papoSRS[%d] is not null", i);
//LATER            m_papoSRS[i]->Release(); //fails for some reason
    }
    CPLFree( m_panSRID );
    CPLFree( m_papoSRS );
    DB2_DEBUG_EXIT("OGRDB2DataSource::~OGRDB2DataSource");
}

/*======================================================================*/
/*                                                                      */
/*                         getDTime()                                   */
/*                                                                      */
/*   Return the time since last called.                                 */
/*======================================================================*/
double OGRDB2DataSource::getDTime()
{
    clock2 = clock();
    time2 = time(nullptr);
    dclock = (clock2 - clock1) / CLOCKS_PER_SEC;
    dtime  = (double)(time2  - time1 );

    clock1 = clock2;
    time1 = time2;
    strcpy(stime, ctime(&time2));   /* get current time as a string */
    stime[strlen(stime) - 1] = '\0';  /* get rid of newline */
    CPLDebug("getDTime","stime: '%s', dclock: %f, dtime: %f",
             stime, dclock, dtime);
    return dclock;
}
/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRDB2DataSource::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,ODsCCreateLayer) || EQUAL(pszCap,ODsCDeleteLayer) )
        return TRUE;
    else if( EQUAL(pszCap,ODsCRandomLayerWrite) )
        return TRUE;
    else
        return FALSE;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRDB2DataSource::GetLayer( int iLayer )

{
    CPLDebug("OGR_DB2DataSource::GetLayer", "pszLayer %d", iLayer);
    if( iLayer < 0 || iLayer >= m_nLayers )
        return nullptr;
    else
        return m_papoLayers[iLayer];
}

/************************************************************************/
/*                           GetLayerByName()                           */
/************************************************************************/
// Layer names are always uppercased - for now
OGRLayer *OGRDB2DataSource::GetLayerByName( const char* pszLayerName )

{
    if (!pszLayerName)
        return nullptr;
    char *pszTableName = nullptr;
    char *pszSchemaName = nullptr;
    OGRLayer *poLayer = nullptr;
    CPLDebug("OGR_DB2DataSource::GetLayerByName", "pszLayerName: '%s'",
             pszLayerName);
    char* pszLayerNameUpper = ToUpper(pszLayerName);
    const char* pszDotPos = strstr(pszLayerNameUpper,".");
    if ( pszDotPos != nullptr )
    {
        int length = static_cast<int>(pszDotPos - pszLayerNameUpper);
        pszSchemaName = (char*)CPLMalloc(length+1);
        strncpy(pszSchemaName, pszLayerNameUpper, length);
        pszSchemaName[length] = '\0';
        pszTableName = CPLStrdup( pszDotPos + 1 ); //skip "."
    }
    else
    {
        pszTableName = CPLStrdup( pszLayerNameUpper );
    }

    for( int iLayer = 0; iLayer < m_nLayers; iLayer++ )
    {
        if( EQUAL(pszTableName,m_papoLayers[iLayer]->GetTableName()) &&
                (pszSchemaName == nullptr ||
                 EQUAL(pszSchemaName,m_papoLayers[iLayer]->GetSchemaName())))
        {
            CPLDebug("OGR_DB2DataSource::GetLayerByName",
                     "found layer: %d; schema: '%s'; table: '%s'",
                     iLayer,m_papoLayers[iLayer]->GetSchemaName(),
                     m_papoLayers[iLayer]->GetTableName());

            poLayer = m_papoLayers[iLayer];
        }
    }

    CPLFree( pszSchemaName );
    CPLFree( pszTableName );
    CPLFree( pszLayerNameUpper );

    return poLayer;
}

/************************************************************************/
/*                    DeleteLayer(OGRDB2TableLayer * poLayer)           */
/************************************************************************/

int OGRDB2DataSource::DeleteLayer( OGRDB2TableLayer * poLayer )
{
    int iLayer = 0;
    if( poLayer == nullptr )
        return OGRERR_FAILURE;

    /* -------------------------------------------------------------------- */
    /*      Blow away our OGR structures related to the layer.  This is     */
    /*      pretty dangerous if anything has a reference to this layer!     */
    /* -------------------------------------------------------------------- */
    const char* pszTableName = poLayer->GetTableName();
    const char* pszSchemaName = poLayer->GetSchemaName();

    OGRDB2Statement oStatement( &m_oSession );

    oStatement.Appendf("DROP TABLE %s.%s", pszSchemaName, pszTableName );

    CPLDebug( "OGR_DB2DataSource::DeleteLayer", "Drop stmt: '%s'",
              oStatement.GetCommand());

    for( iLayer = 0; iLayer < m_nLayers; iLayer++ )
    {
        if (poLayer == m_papoLayers[iLayer]) break;
    }
    delete m_papoLayers[iLayer]; // free the layer object
    // move remaining layers down
    memmove( m_papoLayers + iLayer, m_papoLayers + iLayer + 1,
             sizeof(void *) * (m_nLayers - iLayer - 1) );
    m_nLayers--;

    /* -------------------------------------------------------------------- */
    /*      Remove from the database.                                       */
    /* -------------------------------------------------------------------- */

    m_oSession.BeginTransaction();

    if( !oStatement.DB2Execute("OGR_DB2DataSource::DeleteLayer") )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Error deleting layer: %s", GetSession()->GetLastError() );

        return OGRERR_FAILURE;
    }

    m_oSession.CommitTransaction();

    return OGRERR_NONE;
}

/************************************************************************/
/*                            DeleteLayer(int iLayer)                   */
/************************************************************************/

int OGRDB2DataSource::DeleteLayer( int iLayer )

{
    if( iLayer < 0 || iLayer >= m_nLayers )
        return OGRERR_FAILURE;

    return DeleteLayer(m_papoLayers[iLayer]);
}

/************************************************************************/
/*                            ICreateLayer()                            */
/************************************************************************/

OGRLayer * OGRDB2DataSource::ICreateLayer( const char * pszLayerName,
        OGRSpatialReference *poSRS,
        OGRwkbGeometryType eType,
        char ** papszOptions )

{
    char                *pszTableName = nullptr;
    char                *pszSchemaName = nullptr;
    const char          *pszGeomColumn = nullptr;
    int                 nCoordDimension = 3;
    CPLDebug("OGR_DB2DataSource::ICreateLayer",
             "layer name: %s",pszLayerName);
    CSLPrint(papszOptions, stderr);
    /* determine the dimension */
    if( eType == wkbFlatten(eType) )
        nCoordDimension = 2;

    if( CSLFetchNameValue( papszOptions, "DIM") != nullptr )
        nCoordDimension = atoi(CSLFetchNameValue( papszOptions, "DIM"));

    /* DB2 Schema handling:
       Extract schema name from input layer name or passed with -lco SCHEMA.
       Set layer name to "schema.table" or to "table" if schema is not
       specified
    */
    const char* pszDotPos = strstr(pszLayerName,".");
    if ( pszDotPos != nullptr )
    {
        int length = static_cast<int>(pszDotPos - pszLayerName);
        pszSchemaName = (char*)CPLMalloc(length+1);
        strncpy(pszSchemaName, pszLayerName, length);
        pszSchemaName[length] = '\0';

        /* For now, always convert layer name to uppercase table name*/
        pszTableName = ToUpper( pszDotPos + 1 );
    }
    else
    {
        pszSchemaName = nullptr;
        /* For now, always convert layer name to uppercase table name*/
        pszTableName = ToUpper( pszLayerName );
    }

    if( CSLFetchNameValue( papszOptions, "SCHEMA" ) != nullptr )
    {
        CPLFree(pszSchemaName);
        pszSchemaName = CPLStrdup(CSLFetchNameValue(papszOptions, "SCHEMA"));
    }

    /* -------------------------------------------------------------------- */
    /*      Do we already have this layer?  If so, should we blow it        */
    /*      away?                                                           */
    /* -------------------------------------------------------------------- */
    int iLayer;

    for( iLayer = 0; iLayer < m_nLayers; iLayer++ )
    {
        CPLDebug("OGR_DB2DataSource::ICreateLayer",
                 "schema: '%s'; table: '%s'",
                 m_papoLayers[iLayer]->GetSchemaName(),
                 m_papoLayers[iLayer]->GetTableName());

        if( EQUAL(pszTableName,m_papoLayers[iLayer]->GetTableName()) &&
                (pszSchemaName == nullptr ||
                 EQUAL(pszSchemaName,m_papoLayers[iLayer]->GetSchemaName())) )
        {
            CPLDebug("OGR_DB2DataSource::ICreateLayer",
                     "Found match, schema: '%s'; table: '%s'"   ,
                     pszSchemaName, pszTableName);
            if( CSLFetchNameValue( papszOptions, "OVERWRITE" ) != nullptr
                    && !EQUAL(CSLFetchNameValue(papszOptions,"OVERWRITE"),
                              "NO"))
            {
                if (!pszSchemaName)
                    pszSchemaName = CPLStrdup(m_papoLayers[iLayer]->
                                              GetSchemaName());

                DeleteLayer( iLayer );
            }
            else
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                          "Layer %s already exists, CreateLayer failed.\n"
                          "Use the layer creation option OVERWRITE=YES to "
                          "replace it.",
                          pszLayerName );

                CPLFree( pszSchemaName );
                CPLFree( pszTableName );
                return nullptr;
            }
        }
    }

    /* determine the geometry column name */
    pszGeomColumn =  CSLFetchNameValue( papszOptions, "GEOM_NAME");
    if (!pszGeomColumn)
        pszGeomColumn = "OGR_geometry";

    /* -------------------------------------------------------------------- */
    /*      Try to get the SRS Id of this spatial reference system,         */
    /*      adding to the srs table if needed.                              */
    /* -------------------------------------------------------------------- */
    int nSRSId = 0;

    if( CSLFetchNameValue( papszOptions, "SRID") != nullptr )
        nSRSId = atoi(CSLFetchNameValue( papszOptions, "SRID"));

    if( nSRSId == 0 && poSRS != nullptr )
        nSRSId = FetchSRSId( poSRS );

    OGRDB2Statement oStatement( &m_oSession );

    if (pszSchemaName != nullptr)
        oStatement.Appendf("CREATE TABLE %s.%s ",
                           pszSchemaName, pszTableName);
    else
        oStatement.Appendf("CREATE TABLE %s" ,
                           pszTableName);
    oStatement.Appendf(" (ogr_fid int not null "
//                  "primary key , "
                       "primary key GENERATED BY DEFAULT AS IDENTITY, "
                       "%s db2gse.st_%s )",
                       pszGeomColumn, OGRToOGCGeomType(eType));

    m_oSession.BeginTransaction();

    if( !oStatement.DB2Execute("OGR_DB2DataSource::ICreateLayer") )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Error creating layer: %s", GetSession()->GetLastError() );
        CPLDebug("OGR_DB2DataSource::ICreateLayer", "create failed");

        return nullptr;
    }

    m_oSession.CommitTransaction();

    // If we didn't have a schema name when the table was created,
    // get the schema that was created by implicit
    if (pszSchemaName == nullptr) {
        oStatement.Clear();
        oStatement.Appendf( "SELECT table_schema FROM db2gse.st_geometry_columns "
                            "WHERE table_name = '%s'",
                            pszTableName);

        if( oStatement.DB2Execute("OGR_DB2DataSource::ICreateLayer")
                && oStatement.Fetch())
        {
            pszSchemaName = CPLStrdup( oStatement.GetColData(0) );
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Create the layer object.                                        */
    /* -------------------------------------------------------------------- */
    OGRDB2TableLayer *poLayer = new OGRDB2TableLayer( this );

    poLayer->SetLaunderFlag( CPLFetchBool(papszOptions, "LAUNDER", true) );
    poLayer->SetPrecisionFlag(
        CPLFetchBool(papszOptions, "PRECISION", true));

    char *pszWKT = nullptr;
    if( poSRS && poSRS->exportToWkt( &pszWKT ) != OGRERR_NONE )
    {
        CPLFree(pszWKT);
        pszWKT = nullptr;
    }
    CPLDebug("OGR_DB2DataSource::ICreateLayer", "srs wkt: %s",pszWKT);
    if (poLayer->Initialize(pszSchemaName, pszTableName, pszGeomColumn,
                            nCoordDimension, nSRSId, pszWKT, eType)
            == OGRERR_FAILURE)
    {
        CPLFree( pszSchemaName );
        CPLFree( pszTableName );
        CPLFree( pszWKT );
        return nullptr;
    }

    CPLFree( pszSchemaName );
    CPLFree( pszTableName );
    CPLFree( pszWKT );

    /* -------------------------------------------------------------------- */
    /*      Add layer to data source layer list.                            */
    /* -------------------------------------------------------------------- */
    m_papoLayers = (OGRDB2TableLayer **)
                   CPLRealloc( m_papoLayers, sizeof(OGRDB2TableLayer *)
                               * (m_nLayers+1));

    m_papoLayers[m_nLayers++] = poLayer;

    return poLayer;
}

/************************************************************************/
/*                             OpenTable()                              */
/************************************************************************/

int OGRDB2DataSource::OpenTable( const char *pszSchemaName,
                                 const char *pszTableName,
                                 const char *pszGeomCol, int nCoordDimension,
                                 int nSRID, const char *pszSRText,
                                 OGRwkbGeometryType eType)
{
    /* -------------------------------------------------------------------- */
    /*      Create the layer object.                                        */
    /* -------------------------------------------------------------------- */
    OGRDB2TableLayer  *poLayer = new OGRDB2TableLayer( this );
    CPLDebug( "OGR_DB2DataSource::OpenTable",
              "pszSchemaName: '%s'; pszTableName: '%s'; pszGeomCol: '%s'",
              pszSchemaName , pszTableName, pszGeomCol);
    if( poLayer->Initialize( pszSchemaName, pszTableName, pszGeomCol,
                             nCoordDimension, nSRID, pszSRText, eType ) )
    {
        delete poLayer;
        return FALSE;
    }

    /* -------------------------------------------------------------------- */
    /*      Add layer to data source layer list.                            */
    /* -------------------------------------------------------------------- */
    m_papoLayers = (OGRDB2TableLayer **)
                   CPLRealloc( m_papoLayers,  sizeof(OGRDB2TableLayer *)
                               * (m_nLayers+1) );
    m_papoLayers[m_nLayers++] = poLayer;

    return TRUE;
}

/************************************************************************/
/*                       GetLayerCount()                                */
/************************************************************************/

int OGRDB2DataSource::GetLayerCount()
{
    return m_nLayers;
}

/************************************************************************/
/*                       ParseValue()                                   */
/************************************************************************/

int OGRDB2DataSource::ParseValue(char** ppszValue, char* pszSource,
                                 const char* pszKey, int nStart, int nNext,
                                 int nTerm, int bRemove)
{
    int nLen = static_cast<int>(strlen(pszKey));
    if ((*ppszValue) == nullptr && nStart + nLen < nNext &&
            EQUALN(pszSource + nStart, pszKey, nLen))
    {
        const int nSize = nNext - nStart - nLen;
        *ppszValue = (char*)CPLMalloc( nSize + 1 );
        if (*ppszValue)
        {
            strncpy(*ppszValue, pszSource + nStart + nLen,
                    nSize);
            (*ppszValue)[nSize] = 0;
        }

        if (bRemove)
        {
            // remove the value from the source string
            if (pszSource[nNext] == ';')
                memmove( pszSource + nStart, pszSource + nNext + 1,
                         nTerm - nNext);
            else
                memmove( pszSource + nStart, pszSource + nNext,
                         nTerm - nNext + 1);
        }
        return TRUE;
    }
    return FALSE;
}

/************************************************************************/
/*                                Create()                              */
/************************************************************************/

int OGRDB2DataSource::Create( const char * pszFilename,
                              int nXSize,
                              int nYSize,
                              int nBandsIn,
                              GDALDataType eDT,
                              char **papszOptions )
{
    CPLString osCommand;

    /* First, ensure there isn't any such file yet. */
    VSIStatBufL sStatBuf;

    CPLDebug( "OGR_DB2DataSource::Create", "pszFileName: '%s'", pszFilename);
    CPLDebug( "OGR_DB2DataSource::Create", "(%s,%s,%d,%d,%d,%s,%p)",
              GetDescription(), pszFilename, nXSize, nYSize, nBandsIn,
              GDALGetDataTypeName( eDT ),
              papszOptions );

    if (!InitializeSession(pszFilename, 0)) {
        CPLDebug( "OGR_DB2DataSource::Create",
                  "Session initialization failed");
        return FALSE;
    }

    if (!HasMetadataTables()) {
        CPLDebug( "OGR_DB2DataSource::Create",
                  "No metadata tables and create failed");
        return FALSE;
    }

    if( nBandsIn != 0 )
    {
        if( eDT != GDT_Byte )
        {
            CPLError(CE_Failure, CPLE_NotSupported, "Only Byte supported");
            return FALSE;
        }
        if( nBandsIn != 1 && nBandsIn != 2 && nBandsIn != 3 && nBandsIn != 4 )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Only 1 (Grey/ColorTable), 2 (Grey+Alpha), 3 (RGB) or 4 (RGBA) band dataset supported");
            return FALSE;
        }
    }

    //int bFileExists = FALSE;
    if( VSIStatL( pszFilename, &sStatBuf ) == 0 )
    {
        //bFileExists = TRUE;
        if( nBandsIn == 0 ||
                !CPLTestBool(CSLFetchNameValueDef(papszOptions, "APPEND_SUBDATASET", "NO")) )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "A file system object called '%s' already exists.",
                      pszFilename );

            return FALSE;
        }
    }

    m_bIsVector = FALSE;
    m_pszFilename = CPLStrdup(pszFilename);
    m_bNew = true;
    m_bUpdate = TRUE;
    eAccess = GA_Update; /* hum annoying duplication */

    if( nBandsIn != 0 )
    {
        CPLDebug( "OGR_DB2DataSource::Create", "pszFileName: '%s'", pszFilename);
        m_osRasterTable = CSLFetchNameValueDef(papszOptions,
                                               "RASTER_TABLE", "RASTERTABLE");
        m_bIdentifierAsCO = CSLFetchNameValue(papszOptions,
                                              "RASTER_IDENTIFIER" ) != nullptr;
        m_osIdentifier = CSLFetchNameValueDef(papszOptions,
                                              "RASTER_IDENTIFIER", m_osRasterTable);
        m_bDescriptionAsCO = CSLFetchNameValue(papszOptions,
                                               "RASTER_DESCRIPTION" ) != nullptr;
        m_osDescription = CSLFetchNameValueDef(papszOptions,
                                               "RASTER_DESCRIPTION", "");
        CPLDebug("OGR_DB2DataSource::Create", "m_osRasterTable: '%s'",
                 m_osRasterTable.c_str());
        CPLDebug("OGR_DB2DataSource::Create", "m_osIdentifier: '%s'",
                 m_osIdentifier.c_str());
        CPLDebug("OGR_DB2DataSource::Create", "m_osDescription: '%s'",
                 m_osDescription.c_str());
        m_oSession.BeginTransaction();
        OGRDB2Statement oStatement( &m_oSession );

// Drop the table with raster data
        oStatement.Appendf("DROP TABLE %s",
                           m_osRasterTable.c_str());

        if( !oStatement.DB2Execute("OGR_DB2DataSource::Create") )
        {
            CPLDebug("OGR_DB2DataSource::Create", "DROP failed: %s", GetSession()->GetLastError() );
        }

        oStatement.Clear();
        oStatement.Appendf("CREATE TABLE %s" ,
                           m_osRasterTable.c_str());
        oStatement.Appendf("("
                           "id INTEGER NOT NULL PRIMARY KEY GENERATED BY DEFAULT AS IDENTITY,"
                           "zoom_level INTEGER NOT NULL,"
                           "tile_column INTEGER NOT NULL,"
                           "tile_row INTEGER NOT NULL,"
                           "tile_data BLOB NOT NULL,"
                           "UNIQUE (zoom_level, tile_column, tile_row)"
                           ")");

        if( !oStatement.DB2Execute("OGR_DB2DataSource::Create") )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Error creating layer: %s", GetSession()->GetLastError() );
            CPLDebug("OGR_DB2DataSource::Create", "create failed");
            return FALSE;
        }

        // Remove entries from raster catalog tables - will cascade
        oStatement.Clear();
        oStatement.Appendf("DELETE FROM gpkg.contents WHERE table_name = '%s'",
                           m_osRasterTable.c_str());

        if( !oStatement.DB2Execute("OGR_DB2DataSource::Create") )
        {
            CPLDebug("OGR_DB2DataSource::Create", "DELETE failed: %s", GetSession()->GetLastError() );
        }
        m_oSession.CommitTransaction();

        nRasterXSize = nXSize;
        nRasterYSize = nYSize;

        const char* pszTileSize = CSLFetchNameValueDef(papszOptions, "BLOCKSIZE", "256");
        const char* pszTileWidth = CSLFetchNameValueDef(papszOptions, "BLOCKXSIZE", pszTileSize);
        const char* pszTileHeight = CSLFetchNameValueDef(papszOptions, "BLOCKYSIZE", pszTileSize);
        int nTileWidth = atoi(pszTileWidth);
        int nTileHeight = atoi(pszTileHeight);
        if( (nTileWidth < 8 || nTileWidth > 4096 || nTileHeight < 8 || nTileHeight > 4096) &&
                !CPLTestBool(CPLGetConfigOption("GPKG_ALLOW_CRAZY_SETTINGS", "NO")) )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Invalid block dimensions: %dx%d",
                     nTileWidth, nTileHeight);
            return FALSE;
        }

        m_pabyCachedTiles = (GByte*) VSI_MALLOC3_VERBOSE(4 * 4, nTileWidth, nTileHeight);
        if( m_pabyCachedTiles == nullptr )
        {
            return FALSE;
        }

        for(int i = 1; i <= nBandsIn; i ++)
            SetBand( i, new GDALDB2RasterBand(this, i, nTileWidth, nTileHeight) );
        CPLDebug("OGR_DB2DataSource::Create","setting metadata PIXEL");
        GDALPamDataset::SetMetadataItem("INTERLEAVE", "PIXEL", "IMAGE_STRUCTURE");
        GDALPamDataset::SetMetadataItem("IDENTIFIER", m_osIdentifier);
        if( !m_osDescription.empty() )
            GDALPamDataset::SetMetadataItem("DESCRIPTION", m_osDescription);

        const char* pszTF = CSLFetchNameValue(papszOptions, "TILE_FORMAT");
        if( pszTF )
            m_eTF = GetTileFormat(pszTF);

        ParseCompressionOptions(papszOptions);

        if( m_eTF == GPKG_TF_WEBP )
        {
            if( !RegisterWebPExtension() )
                return FALSE;
        }

        const char* pszTilingScheme = CSLFetchNameValue(papszOptions, "TILING_SCHEME");
        if( pszTilingScheme )
        {
            m_osTilingScheme = pszTilingScheme;
            int bFound = FALSE;
            for(size_t iScheme = 0;
                    iScheme < sizeof(asTilingShemes)/sizeof(asTilingShemes[0]);
                    iScheme++ )
            {
                if( EQUAL(m_osTilingScheme, asTilingShemes[iScheme].pszName) )
                {
                    if( nTileWidth != asTilingShemes[iScheme].nTileWidth ||
                            nTileHeight != asTilingShemes[iScheme].nTileHeight )
                    {
                        CPLError(CE_Failure, CPLE_NotSupported,
                                 "Tile dimension should be %dx%d for %s tiling scheme",
                                 asTilingShemes[iScheme].nTileWidth,
                                 asTilingShemes[iScheme].nTileHeight,
                                 m_osTilingScheme.c_str());
                        return FALSE;
                    }

                    /* Implicitly sets SRS */
                    OGRSpatialReference oSRS;
                    if( oSRS.importFromEPSG(asTilingShemes[iScheme].nEPSGCode) != OGRERR_NONE )
                        return FALSE;
                    char* pszWKT = nullptr;
                    oSRS.exportToWkt(&pszWKT);
                    SetProjection(pszWKT);
                    CPLFree(pszWKT);

                    bFound = TRUE;
                    break;
                }
            }
            if( !bFound )
                m_osTilingScheme = "CUSTOM";
        }
    }
    CPLDebug("OGR_DB2DataSource::Create","exiting");
    return TRUE;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGRDB2DataSource::Open( GDALOpenInfo* poOpenInfo )
{
    SetDescription( poOpenInfo->pszFilename );
#ifdef DEBUG_DB2
    CPLDebug("OGR_DB2DataSource::OpenNew", "papszOpenOptions");
    CSLPrint((char **) poOpenInfo->papszOpenOptions, stderr);
#endif
    CPLString osFilename( poOpenInfo->pszFilename );
    CPLString osSubdatasetTableName;
    m_bUpdate = poOpenInfo->eAccess == GA_Update;
    CPLDebug( "OGR_DB2DataSource::OpenNew",
              "pszFileName: '%s'; m_bUpdate: %d, eAccess: %d; GA_Update: %d",
              poOpenInfo->pszFilename, m_bUpdate, poOpenInfo->eAccess, GA_Update);
    eAccess = poOpenInfo->eAccess; /* hum annoying duplication */
    m_pszFilename = CPLStrdup( osFilename );

    if( poOpenInfo->nOpenFlags & GDAL_OF_VECTOR )
    {
        CPLDebug( "OGR_DB2DataSource::OpenNew", "Open vector");
        return Open(poOpenInfo->pszFilename, 0);
    }

    if( poOpenInfo->nOpenFlags & GDAL_OF_RASTER )
    {
        CPLDebug( "OGR_DB2DataSource::OpenNew", "Open raster");
        m_bIsVector = FALSE;
        if (!InitializeSession(poOpenInfo->pszFilename, 0)) {
            CPLDebug( "OGR_DB2DataSource::Open",
                      "Session initialization failed");
            return FALSE;
        }

        OGRDB2Statement oStatement( GetSession() );
        oStatement.Appendf( "SELECT c.table_name, c.identifier, c.description, c.srs_id, c.min_x, c.min_y, c.max_x, c.max_y, "
                            "tms.min_x, tms.min_y, tms.max_x, tms.max_y FROM gpkg.contents c JOIN gpkg.tile_matrix_set tms ON "
                            "c.table_name = tms.table_name WHERE data_type = 'tiles'");

        if( CSLFetchNameValue( poOpenInfo->papszOpenOptions, "TABLE") ) {
            osSubdatasetTableName = CSLFetchNameValue( poOpenInfo->papszOpenOptions, "TABLE");
        }
        if( !osSubdatasetTableName.empty() )
        {
            oStatement.Appendf(" AND c.table_name='%s'", osSubdatasetTableName.c_str());
            SetPhysicalFilename( osFilename.c_str() ); //LATER
        }

        if( !oStatement.DB2Execute("OGR_DB2DataSource::OpenNew") )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Raster query failed: %s",
                     GetSession()->GetLastError());
            return FALSE;
        }
        if( oStatement.Fetch() )
        {
            CPLDebug("OGRDB2DataSource::OpenNew",
                     "Raster table_name: %s",
                     oStatement.GetColData(0));
            const char *pszTableName = oStatement.GetColData(0);
            const char* pszIdentifier = oStatement.GetColData(1);
            const char* pszDescription = oStatement.GetColData(2);
            const char* pszSRSId = oStatement.GetColData(3);
            const char* pszMinX = oStatement.GetColData(4);
            const char* pszMinY = oStatement.GetColData(5);
            const char* pszMaxX = oStatement.GetColData(6);
            const char* pszMaxY = oStatement.GetColData(7);
            const char* pszTMSMinX = oStatement.GetColData(8);
            const char* pszTMSMinY = oStatement.GetColData(9);
            const char* pszTMSMaxX = oStatement.GetColData(10);
            const char* pszTMSMaxY = oStatement.GetColData(11);
            if( pszTableName != nullptr && pszTMSMinX != nullptr && pszTMSMinY != nullptr &&
                    pszTMSMaxX != nullptr && pszTMSMaxY != nullptr )
            {
                eAccess = GA_Update; //LATER - where should this be set?
                int bRet = OpenRaster( pszTableName, pszIdentifier, pszDescription,
                                   pszSRSId ? atoi(pszSRSId) : 0,
                                   CPLAtof(pszTMSMinX), CPLAtof(pszTMSMinY),
                                   CPLAtof(pszTMSMaxX), CPLAtof(pszTMSMaxY),
                                   pszMinX, pszMinY, pszMaxX, pszMaxY,
                                   poOpenInfo->papszOpenOptions );
                CPLDebug("OGRDB2DataSource::OpenNew","back from OpenRaster; bRet: %d", bRet);
                if (bRet) {
                    return TRUE;
                }
            }
        }
    }
    CPLError(CE_Failure, CPLE_AppDefined, "Table '%s' not found", osSubdatasetTableName.c_str());
    CPLDebug("OGRDB2DataSource::OpenNew","exiting FALSE");
    return FALSE;
}

/************************************************************************/
/*                                InializeSession()                     */
/************************************************************************/

int OGRDB2DataSource::InitializeSession( const char * pszNewName,
        int bTestOpen )

{
    CPLDebug( "OGR_DB2DataSource::InitializeSession",
              "pszNewName: '%s'; bTestOpen: %d",
              pszNewName, bTestOpen);

// If we already have a connection, assume that it is good and we don't
// have to do anything else.
    if (m_oSession.GetConnection()) {
        CPLDebug("OGR_DB2DataSource::InitializeSession",
                 "connected already: %p", m_oSession.GetConnection());
        return TRUE;
    }

    /* Determine if the connection string contains specific values */
    char* pszTableSpec = nullptr;
    char* pszConnectionName = CPLStrdup(pszNewName + strlen(DB2ODBC_PREFIX));
    char* pszDriver = nullptr;
    int nCurrent, nNext, nTerm;
    nCurrent = nNext = nTerm = static_cast<int>(strlen(pszConnectionName));

    while (nCurrent > 0)
    {
        --nCurrent;
        if (pszConnectionName[nCurrent] == ';')
        {
            nNext = nCurrent;
            continue;
        }

        if (ParseValue(&m_pszCatalog, pszConnectionName, "database=",
                       nCurrent, nNext, nTerm, FALSE))
            continue;

        if (ParseValue(&pszTableSpec, pszConnectionName, "tables=",
                       nCurrent, nNext, nTerm, TRUE))
            continue;

        if (ParseValue(&pszDriver, pszConnectionName, "driver=",
                       nCurrent, nNext, nTerm, FALSE))
            continue;
    }
    CPLDebug( "OGR_DB2DataSource::Open", "m_pszCatalog: '%s'", m_pszCatalog);
    CPLDebug( "OGR_DB2DataSource::Open", "pszTableSpec: '%s'", pszTableSpec);
    CPLDebug( "OGR_DB2DataSource::Open", "pszDriver: '%s'", pszDriver);
    CPLDebug( "OGR_DB2DataSource::Open", "pszConnectionName: '%s'",
              pszConnectionName);

    /* Determine if the connection string contains the database portion */
    if( m_pszCatalog == nullptr )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "'%s' does not contain the 'database' portion\n",
                  pszNewName );
        CPLFree(pszTableSpec);
        CPLFree(pszConnectionName);
        CPLFree(pszDriver);
        return FALSE;
    }

    m_pszName = CPLStrdup(pszNewName);

    // if the table parameter was specified, pull out the table names
    if( pszTableSpec != nullptr )
    {
        char          **papszTableList;
        int             i;

        papszTableList = CSLTokenizeString2( pszTableSpec, ",", 0 );

        for( i = 0; i < CSLCount(papszTableList); i++ )
        {
            char      **papszQualifiedParts;

            // Get schema and table name
            papszQualifiedParts = CSLTokenizeString2( papszTableList[i],
                                  ".", 0 );

            /* Find the geometry column name if specified */
            if( CSLCount( papszQualifiedParts ) >= 1 )
            {
                char* pszGeomColumnName = nullptr;
                char* pos = strchr(papszQualifiedParts[
                                       CSLCount( papszQualifiedParts ) - 1], '(');
                if (pos != nullptr)
                {
                    *pos = '\0';
                    pszGeomColumnName = pos+1;
                    int len = static_cast<int>(strlen(pszGeomColumnName));
                    if (len > 0)
                        pszGeomColumnName[len - 1] = '\0';
                }
                m_papszGeomColumnNames = CSLAddString( m_papszGeomColumnNames,
                                                       pszGeomColumnName ?
                                                       pszGeomColumnName : "");
            }

            if( CSLCount( papszQualifiedParts ) == 2 )
            {
                m_papszSchemaNames = CSLAddString( m_papszSchemaNames,
                                                   ToUpper(papszQualifiedParts[0]));

                m_papszTableNames = CSLAddString( m_papszTableNames,
                                                  ToUpper(papszQualifiedParts[1]));
            }
            else if( CSLCount( papszQualifiedParts ) == 1 )
            {
                m_papszSchemaNames = CSLAddString( m_papszSchemaNames, "NULL");
                m_papszTableNames = CSLAddString( m_papszTableNames,
                                                  ToUpper(papszQualifiedParts[0]));
            }

            CSLDestroy(papszQualifiedParts);
        }

        CSLDestroy(papszTableList);
    }

    CPLFree(pszTableSpec);

    /* Initialize the DB2 connection. */
    int nResult;

    nResult = m_oSession.EstablishSession( pszConnectionName, "", "" );

    CPLFree(pszDriver);

    if( !nResult )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Unable to initialize connection to the server for %s,\n"
                  "%s", pszNewName, m_oSession.GetLastError() );
        CPLFree(pszConnectionName);
        return FALSE;
    }

// Determine whether we are running on LUW or zoom
    OGRDB2Statement oStatement( &m_oSession );
    oStatement.Append("SELECT COUNT(*) FROM SYSCAT.TABLES");

// We assume that if the statement fails, the table doesn't exist

    if( !oStatement.DB2Execute("OGR_DB2DataSource::InitializeSession") )
    {
        CPLDebug("OGRDB2DataSource::InitializeSession","Must be z/OS");
        m_bIsZ = TRUE;
    } else {
        CPLDebug("OGRDB2DataSource::InitializeSession","Must be LUW");
        m_bIsZ = FALSE;
    }

    return TRUE;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGRDB2DataSource::Open( const char * pszNewName,
                            int bTestOpen )

{

    CPLAssert( m_nLayers == 0 );

    CPLDebug( "OGR_DB2DataSource::Open",
              "pszNewName: '%s'; bTestOpen: %d",
              pszNewName, bTestOpen);

    if( !STARTS_WITH_CI(pszNewName, DB2ODBC_PREFIX) )
    {
        if( !bTestOpen )
            CPLError( CE_Failure, CPLE_AppDefined,
                      "%s does not conform to DB2 naming convention,"
                      " DB2:*\n", pszNewName );
        return FALSE;
    }

    if (!InitializeSession(pszNewName, bTestOpen)) {
        CPLDebug( "OGR_DB2DataSource::Open",
                  "Session initialization failed");
        return FALSE;
    }
    char** papszTypes = nullptr;

    /* read metadata for the specified tables */
    if (m_papszTableNames != nullptr)
    {

        for( int iTable = 0;
                m_papszTableNames != nullptr && m_papszTableNames[iTable] != nullptr;
                iTable++ )
        {
            int found = FALSE;
            OGRDB2Statement oStatement( &m_oSession );
// If a table name was specified, get the information from ST_Geometry_Columns
// for this table
            oStatement.Appendf( "SELECT table_schema, column_name, 2, srs_id, "
                                "srs_name, type_name "
                                "FROM db2gse.st_geometry_columns "
                                "WHERE table_name = '%s'",
                                m_papszTableNames[iTable]);

// If the schema was specified, add it to the SELECT statement
            if (strcmp(m_papszSchemaNames[iTable], "NULL"))
                oStatement.Appendf("  AND table_schema = '%s' ",
                                   m_papszSchemaNames[iTable]);

            if( oStatement.DB2Execute("OGR_DB2DataSource::Open") )
            {
                while( oStatement.Fetch() )
                {
                    found = TRUE;

                    /* set schema for table if it was not specified */
                    if (!strcmp(m_papszSchemaNames[iTable], "NULL")) {
                        CPLFree(m_papszSchemaNames[iTable]);
                        m_papszSchemaNames[iTable] = CPLStrdup(
                                                         oStatement.GetColData(0) );
                    }
                    if (m_papszGeomColumnNames == nullptr)
                        m_papszGeomColumnNames = CSLAddString(
                                                     m_papszGeomColumnNames,
                                                     oStatement.GetColData(1) );
                    else if (*m_papszGeomColumnNames[iTable] == 0)
                    {
                        CPLFree(m_papszGeomColumnNames[iTable]);
                        m_papszGeomColumnNames[iTable] = CPLStrdup(
                                                             oStatement.GetColData(1) );
                    }

                    m_papszCoordDimensions =
                        CSLAddString( m_papszCoordDimensions,
                                      oStatement.GetColData(2, "2") );
                    m_papszSRIds =
                        CSLAddString( m_papszSRIds, oStatement.GetColData(3, "-1") );
                    m_papszSRTexts =
                        CSLAddString( m_papszSRTexts, oStatement.GetColData(4, "") );
                    // Convert the DB2 spatial type to the OGC spatial type
                    // which just entails stripping off the "ST_" at the
                    // beginning of the DB2 type name
                    char DB2SpatialType[20], OGCSpatialType [20];
                    strcpy(DB2SpatialType, oStatement.GetColData(5));
                    strcpy(OGCSpatialType, DB2SpatialType+3);
//                  CPLDebug("OGR_DB2DataSource::Open","DB2SpatialType: %s, OGCSpatialType: %s",DB2SpatialType, OGCSpatialType);
                    papszTypes = CSLAddString( papszTypes, OGCSpatialType );
                }
            }

            if (!found) {
                CPLError( CE_Failure, CPLE_AppDefined,
                          "Table %s.%s not found in "
                          "db2gse.st_geometry_columns"
                          , m_papszSchemaNames[iTable], m_papszTableNames[iTable] );
                return FALSE;
            }
        }
    }

    /* Determine the available tables if not specified. */
    if (m_papszTableNames == nullptr)
    {
        OGRDB2Statement oStatement( &m_oSession );

        oStatement.Append( "SELECT table_schema, table_name, column_name, 2, "
                           "srs_id, srs_name, type_name "
                           "FROM db2gse.st_geometry_columns");

        if( oStatement.DB2Execute("OGR_DB2DataSource::Open") )
        {
            while( oStatement.Fetch() )
            {
                m_papszSchemaNames =
                    CSLAddString( m_papszSchemaNames, oStatement.GetColData(0) );
                m_papszTableNames =
                    CSLAddString( m_papszTableNames, oStatement.GetColData(1) );
                m_papszGeomColumnNames =
                    CSLAddString( m_papszGeomColumnNames, oStatement.GetColData(2) );
                m_papszCoordDimensions =
                    CSLAddString( m_papszCoordDimensions, oStatement.GetColData(3) );
                m_papszSRIds =
                    CSLAddString( m_papszSRIds, oStatement.GetColData(4,"-1") );
                m_papszSRTexts =
                    CSLAddString( m_papszSRTexts, oStatement.GetColData(5,"") );
                char DB2SpatialType[20], OGCSpatialType [20];
                strcpy(DB2SpatialType, oStatement.GetColData(6));
                strcpy(OGCSpatialType, DB2SpatialType+3);
//              CPLDebug("OGR_DB2DataSource::Open","DB2SpatialType: %s, OGCSpatialType: %s",DB2SpatialType, OGCSpatialType);
                papszTypes =
                    CSLAddString( papszTypes, OGCSpatialType );
            }
        }
    }
    /*
    CPLDebug("OGR_DB2DataSource::Open", "m_papszSchemaNames");
    CSLPrint(m_papszSchemaNames, stderr);
    CPLDebug("OGR_DB2DataSource::Open", "m_papszTableNames");
    CSLPrint(m_papszTableNames, stderr);
    CPLDebug("OGR_DB2DataSource::Open", "m_papszGeomColumnNames");
    CSLPrint(m_papszGeomColumnNames, stderr);
    CPLDebug("OGR_DB2DataSource::Open", "m_papszSRIds");
    CSLPrint(m_papszSRIds, stderr);
    CPLDebug("OGR_DB2DataSource::Open", "m_papszSRTexts");
    CSLPrint(m_papszSRTexts, stderr);
    CPLDebug("OGR_DB2DataSource::Open", "papszTypes");
    CSLPrint(papszTypes, stderr);
    */

    int nSRId, nCoordDimension;
    char * pszSRText = nullptr;
    OGRwkbGeometryType eType;

    for( int iTable = 0;
            m_papszTableNames != nullptr && m_papszTableNames[iTable] != nullptr;
            iTable++ )
    {
        pszSRText = nullptr;
        nSRId = -1;
        if (m_papszSRIds != nullptr) {
            nSRId = atoi(m_papszSRIds[iTable]);
            CPLDebug("OGR_DB2DataSource::Open", "iTable: %d; schema: %s; "
                     "table: %s; geomCol: %s; geomType: %s; srid: '%s'",
                     iTable, m_papszSchemaNames[iTable],
                     m_papszTableNames[iTable],
                     m_papszGeomColumnNames[iTable], papszTypes[iTable],
                     m_papszSRIds[iTable]);
            // If srid is not defined it was probably because the table
            // was not registered.
            // In that case, try to get it from the actual data table.
            if (nSRId < 0) {
                OGRDB2Statement oStatement( &m_oSession );
                oStatement.Appendf( "select db2gse.st_srsid(%s) from %s.%s "
                                    "fetch first row only",
                                    m_papszGeomColumnNames[iTable],
                                    m_papszSchemaNames[iTable],
                                    m_papszTableNames[iTable] );

                if( oStatement.DB2Execute("OGR_DB2DataSource::Open")
                        && oStatement.Fetch())
                {
                    nSRId = atoi( oStatement.GetColData( 0 ) );
                    OGRDB2Statement oStatement2( &m_oSession );
                    oStatement2.Appendf( "select definition from "
                                         "db2gse.st_spatial_reference_systems "
                                         "where srs_id = %d",
                                         nSRId);
                    if( oStatement2.DB2Execute("OGR_DB2DataSource::Open")
                            && oStatement2.Fetch() )
                    {
                        if ( oStatement2.GetColData( 0 ) )
                            pszSRText = CPLStrdup(oStatement2.GetColData( 0 ));
                    }
                }
                if (nSRId < 0) { // something went wrong - didn't find srid - use default
                    nSRId = 0;
                    pszSRText = CPLStrdup("UNSPECIFIED");
                    CPLDebug("OGR_DB2DataSource::Open", "Using default srid 0");
                }
            } else {
                pszSRText = CPLStrdup(m_papszSRTexts[iTable]);
            }
        }
        CPLDebug("OGR_DB2DataSource::Open", "nSRId: %d; srText: %s",
                 nSRId, pszSRText);
        if (m_papszCoordDimensions != nullptr)
            nCoordDimension = atoi(m_papszCoordDimensions[iTable]);
        else
            nCoordDimension = 2;

        if (papszTypes != nullptr)
            eType = OGRFromOGCGeomType(papszTypes[iTable]);
        else
            eType = wkbUnknown;

        if( strlen(m_papszGeomColumnNames[iTable]) > 0 )
            OpenTable( m_papszSchemaNames[iTable], m_papszTableNames[iTable],
                       m_papszGeomColumnNames[iTable],
                       nCoordDimension, nSRId, pszSRText, eType);
        else
            OpenTable( m_papszSchemaNames[iTable], m_papszTableNames[iTable],
                       nullptr,
                       nCoordDimension, nSRId, pszSRText, wkbNone);
    }

    CPLFree(pszSRText);

    bDSUpdate = m_bUpdate; //LATER what is bDSUpdate?

    return TRUE;
}

/************************************************************************/
/*                             ExecuteSQL()                             */
/************************************************************************/

OGRLayer * OGRDB2DataSource::ExecuteSQL( const char *pszSQLCommand,
        OGRGeometry *poSpatialFilter,
        const char *pszDialect )

{
    /* -------------------------------------------------------------------- */
    /*      Use generic implementation for recognized dialects              */
    /* -------------------------------------------------------------------- */
    CPLDebug("OGRDB2DataSource::ExecuteSQL", "SQL: '%s'; dialect: '%s'",
             pszSQLCommand, pszDialect);
    if( IsGenericSQLDialect(pszDialect) )
        return GDALDataset::ExecuteSQL( pszSQLCommand, poSpatialFilter,
                                        pszDialect );

    /* -------------------------------------------------------------------- */
    /*      Special case DELLAYER: command.                                 */
    /* -------------------------------------------------------------------- */
    if( EQUALN(pszSQLCommand,"DELLAYER:",9) )
    {
        const char *pszLayerName = pszSQLCommand + 9;

        while( *pszLayerName == ' ' )
            pszLayerName++;

        OGRLayer* poLayer = GetLayerByName(pszLayerName);

        for( int iLayer = 0; iLayer < m_nLayers; iLayer++ )
        {
            if( m_papoLayers[iLayer] == poLayer )
            {
                DeleteLayer( iLayer );
                break;
            }
        }
        return nullptr;
    }

    CPLDebug( "OGRDB2DataSource::ExecuteSQL", "ExecuteSQL(%s) called.",
              pszSQLCommand );

    /* Execute the command natively */
    OGRDB2Statement *poStatement = new OGRDB2Statement( &m_oSession );
    poStatement->Append( pszSQLCommand );

    if( !poStatement->ExecuteSQL() )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "%s", m_oSession.GetLastError() );
        delete poStatement;
        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Are there result columns for this statement?                    */
    /* -------------------------------------------------------------------- */
    if( poStatement->GetColCount() == 0 )
    {
        delete poStatement;
        CPLErrorReset();
        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Create a results layer.  It will take ownership of the          */
    /*      statement.                                                      */
    /* -------------------------------------------------------------------- */

    OGRDB2SelectLayer* poLayer = new OGRDB2SelectLayer( this, poStatement );

    if( poSpatialFilter != nullptr )
        poLayer->SetSpatialFilter( poSpatialFilter );

    return poLayer;
}

/************************************************************************/
/*                          ReleaseResultSet()                          */
/************************************************************************/

void OGRDB2DataSource::ReleaseResultSet( OGRLayer * poLayer )

{
    delete poLayer;
}

/************************************************************************/
/*                            ToUpper()                                 */
/************************************************************************/

char *OGRDB2DataSource::ToUpper( const char *pszSrcName )

{
    char    *pszSafeName = CPLStrdup( pszSrcName );
    int     i;

    for( i = 0; pszSafeName[i] != '\0'; i++ )
    {
        pszSafeName[i] = (char) toupper( pszSafeName[i] );
    }

    return pszSafeName;
}

/************************************************************************/
/*                            LaunderName()                             */
/************************************************************************/

char *OGRDB2DataSource::LaunderName( const char *pszSrcName )

{
    char    *pszSafeName = CPLStrdup( pszSrcName );
    int     i;

    for( i = 0; pszSafeName[i] != '\0'; i++ )
    {
        pszSafeName[i] = (char) tolower( pszSafeName[i] );
        if( pszSafeName[i] == '-' || pszSafeName[i] == '#' )
            pszSafeName[i] = '_';
    }

    return pszSafeName;
}

/************************************************************************/
/*                      InitializeMetadataTables()                      */
/*                                                                      */
/*      Create the metadata tables (SPATIAL_REF_SYS and                 */
/*      GEOMETRY_COLUMNS).                                              */
/************************************************************************/

OGRErr OGRDB2DataSource::InitializeMetadataTables()

{
    CPLDebug( "OGR_DB2DataSource::InitializeMetadataTables", "Not supported");

    CPLError( CE_Failure, CPLE_AppDefined,
              "Dynamically creating DB2 spatial metadata tables is "
              "not supported" );
    return OGRERR_FAILURE;
}

/************************************************************************/
/*                              FetchSRS()                              */
/*                                                                      */
/*      Return a SRS corresponding to a particular id.  Note that       */
/*      reference counting should be honoured on the returned           */
/*      OGRSpatialReference, as handles may be cached.                  */
/************************************************************************/

OGRSpatialReference *OGRDB2DataSource::FetchSRS( int nId )

{
    CPLDebug("OGRDB2DataSource::FetchSRS", "nId: %d", nId);
    if( nId <= 0 )
        return nullptr;

    /* -------------------------------------------------------------------- */
    /*      First, we look through our SRID cache, is it there?             */
    /* -------------------------------------------------------------------- */
    int  i;

    for( i = 0; i < m_nKnownSRID; i++ )
    {
        if( m_panSRID[i] == nId )
            return m_papoSRS[i];
    }

    OGRSpatialReference *poSRS = nullptr;

    /* -------------------------------------------------------------------- */
    /*      Try looking up in spatial_ref_sys table                         */
    /* -------------------------------------------------------------------- */
    if (bUseGeometryColumns)
    {
        OGRDB2Statement oStatement( GetSession() );
        oStatement.Appendf( "SELECT definition FROM "
                            "db2gse.st_spatial_reference_systems "
                            "WHERE srs_id = %d", nId );

        if( oStatement.ExecuteSQL() && oStatement.Fetch() )
        {
            if ( oStatement.GetColData( 0 ) )
            {
                poSRS = new OGRSpatialReference();
                poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
                char* pszWKT = (char*)oStatement.GetColData( 0 );
                CPLDebug("OGR_DB2DataSource::FetchSRS", "SRS = %s", pszWKT);
                if( poSRS->importFromWkt( &pszWKT ) != OGRERR_NONE )
                {
                    delete poSRS;
                    poSRS = nullptr;
                }
            }
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Try looking up the EPSG list                                    */
    /* -------------------------------------------------------------------- */
    if (!poSRS)
    {
        poSRS = new OGRSpatialReference();
        poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        if( poSRS->importFromEPSG( nId ) != OGRERR_NONE )
        {
            delete poSRS;
            poSRS = nullptr;
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Add to the cache.                                               */
    /* -------------------------------------------------------------------- */
    if (poSRS)
    {
        m_panSRID = (int *) CPLRealloc(m_panSRID,sizeof(int) * (m_nKnownSRID+1) );
        m_papoSRS = (OGRSpatialReference **)
                    CPLRealloc(m_papoSRS, sizeof(void*) * (m_nKnownSRID + 1) );
        m_panSRID[m_nKnownSRID] = nId;
        m_papoSRS[m_nKnownSRID] = poSRS;
        m_nKnownSRID++;
        CPLDebug("OGRDB2DataSource::FetchSRS", "Add to cache; m_nKnownSRID: %d", m_nKnownSRID);
    }

    return poSRS;
}

/************************************************************************/
/*                             FetchSRSId()                             */
/*                                                                      */
/*      Fetch the id corresponding to an SRS, and if not found, add     */
/*      it to the table.                                                */
/************************************************************************/

int OGRDB2DataSource::FetchSRSId( OGRSpatialReference * poSRS)

{
    char                *pszWKT = nullptr;
    const char*         pszAuthorityName;

    if( poSRS == nullptr )
        return 0;

    OGRSpatialReference oSRS(*poSRS);
    // cppcheck-suppress uselessAssignmentPtrArg
    poSRS = nullptr;

    pszAuthorityName = oSRS.GetAuthorityName(nullptr);
    CPLDebug("OGRDB2DataSource::FetchSRSId",
        "pszAuthorityName: '%s'", pszAuthorityName);
    if( pszAuthorityName == nullptr || strlen(pszAuthorityName) == 0 )
    {
        /* -----------------------------------------------------------------*/
        /*      Try to identify an EPSG code                                */
        /* -----------------------------------------------------------------*/
        oSRS.AutoIdentifyEPSG();

        pszAuthorityName = oSRS.GetAuthorityName(nullptr);
        if (pszAuthorityName != nullptr && EQUAL(pszAuthorityName, "EPSG"))
        {
            const char* pszAuthorityCode = oSRS.GetAuthorityCode(nullptr);
            if ( pszAuthorityCode != nullptr && strlen(pszAuthorityCode) > 0 )
            {
                /* Import 'clean' SRS */
                oSRS.importFromEPSG( atoi(pszAuthorityCode) );

                pszAuthorityName = oSRS.GetAuthorityName(nullptr);
            }
        }
    }
    /* -------------------------------------------------------------------- */
    /*      Check whether the EPSG authority code is already mapped to a    */
    /*      SRS ID.                                                         */
    /* -------------------------------------------------------------------- */
    if( pszAuthorityName != nullptr && EQUAL( pszAuthorityName, "EPSG" ) )
    {
        /* For the root authority name 'EPSG', the authority code
         * should always be integral
         */
        int nAuthorityCode = atoi( oSRS.GetAuthorityCode(nullptr) );

        OGRDB2Statement oStatement( &m_oSession );
        oStatement.Appendf("SELECT srs_id "
                           "FROM db2gse.st_spatial_reference_systems WHERE "
                           "organization = '%s' "
                           "AND organization_coordsys_id = %d",
                           pszAuthorityName,
                           nAuthorityCode );

        if( oStatement.DB2Execute("OGR_DB2DataSource::FetchSRSId")
            && oStatement.Fetch() && oStatement.GetColData( 0 ) )
        {
            int nSRSId = atoi(oStatement.GetColData( 0 ));
            CPLDebug("OGR_DB2DataSource::FetchSRSId", "nSRSId = %d", nSRSId);
            return nSRSId;
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Translate SRS to WKT.                                           */
    /* -------------------------------------------------------------------- */
    if( oSRS.exportToWkt( &pszWKT ) != OGRERR_NONE )
    {
        CPLFree(pszWKT);
        return 0;
    }

    /* -------------------------------------------------------------------- */
    /*      Try to find in the existing table.                              */
    /* -------------------------------------------------------------------- */
    OGRDB2Statement oStatement( &m_oSession );

    oStatement.Append( "SELECT srs_id FROM db2gse.st_spatial_reference_systems "
                       "WHERE description = ");
    OGRDB2AppendEscaped(&oStatement, pszWKT);

    /* -------------------------------------------------------------------- */
    /*      We got it!  Return it.                                          */
    /* -------------------------------------------------------------------- */
    if( oStatement.DB2Execute("OGR_DB2DataSource::FetchSRSId") )
    {
        if ( oStatement.Fetch() && oStatement.GetColData( 0 ) )
        {
            int nSRSId = atoi(oStatement.GetColData( 0 ));
            CPLFree(pszWKT);
            return nSRSId;
        }
    }
    else
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Didn't find srs_id for %s", pszWKT );
    }
    return -1;
}

/************************************************************************/
/*                         StartTransaction()                           */
/*                                                                      */
/* Should only be called by user code. Not driver internals.            */
/************************************************************************/

OGRErr OGRDB2DataSource::StartTransaction(CPL_UNUSED int bForce)
{
    if (!m_oSession.BeginTransaction())
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Failed to start transaction: %s", m_oSession.GetLastError());
        return OGRERR_FAILURE;
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                         CommitTransaction()                          */
/*                                                                      */
/* Should only be called by user code. Not driver internals.            */
/************************************************************************/

OGRErr OGRDB2DataSource::CommitTransaction()
{
    if (!m_oSession.CommitTransaction())
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Failed to commit transaction: %s",
                  m_oSession.GetLastError() );

        for( int iLayer = 0; iLayer < m_nLayers; iLayer++ )
        {
            if( m_papoLayers[iLayer]->GetLayerStatus()
                    == DB2LAYERSTATUS_INITIAL )
                m_papoLayers[iLayer]->SetLayerStatus(DB2LAYERSTATUS_DISABLED);
        }
        return OGRERR_FAILURE;
    }

    /* set the status for the newly created layers */
    for( int iLayer = 0; iLayer < m_nLayers; iLayer++ )
    {
        if( m_papoLayers[iLayer]->GetLayerStatus() == DB2LAYERSTATUS_INITIAL )
            m_papoLayers[iLayer]->SetLayerStatus(DB2LAYERSTATUS_CREATED);
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                        RollbackTransaction()                         */
/*                                                                      */
/* Should only be called by user code. Not driver internals.            */
/************************************************************************/

OGRErr OGRDB2DataSource::RollbackTransaction()
{
    /* set the status for the newly created layers */
    for( int iLayer = 0; iLayer < m_nLayers; iLayer++ )
    {
        if( m_papoLayers[iLayer]->GetLayerStatus() == DB2LAYERSTATUS_INITIAL )
            m_papoLayers[iLayer]->SetLayerStatus(DB2LAYERSTATUS_DISABLED);
    }

    if (!m_oSession.RollbackTransaction())
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Failed to roll back transaction: %s",
                  m_oSession.GetLastError() );
        return OGRERR_FAILURE;
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                         OpenRaster()                                 */
/************************************************************************/

int OGRDB2DataSource::OpenRaster( const char* pszTableName,
                                  const char* pszIdentifier,
                                  const char* pszDescription,
                                  int nSRSId,
                                  double dfMinX,
                                  double dfMinY,
                                  double dfMaxX,
                                  double dfMaxY,
                                  const char* pszContentsMinX,
                                  const char* pszContentsMinY,
                                  const char* pszContentsMaxX,
                                  const char* pszContentsMaxY,
                                  char** papszOpenOptionsIn )
{
    if( dfMinX >= dfMaxX || dfMinY >= dfMaxY )
        return FALSE;
    CPLDebug("OGRDB2DataSource::OpenRaster", "pszTableName: '%s'", pszTableName);
    m_bRecordInsertedInGPKGContent = TRUE;
    m_nSRID = nSRSId;
    if( nSRSId > 0 )
    {
        OGRSpatialReference* poSRS = FetchSRS( nSRSId );
        CPLDebug("OGRDB2DataSource::OpenRaster", "nSRSId: '%d'", nSRSId);
        if( poSRS )
        {
            poSRS->exportToWkt(&m_pszProjection);
            CPLDebug("OGRDB2DataSource::OpenRaster", "m_pszProjection: '%s'", m_pszProjection);
//LATER            delete poSRS;
        }
    }

    OGRDB2Statement oStatement( &m_oSession );
    oStatement.Appendf( "SELECT zoom_level, pixel_x_size, pixel_y_size, "
                        "tile_width, tile_height, matrix_width, matrix_height "
                        "FROM gpkg.tile_matrix tm "
                        "WHERE table_name = '%s' AND pixel_x_size > 0 "
                        "AND pixel_y_size > 0 AND tile_width > 0 "
                        "AND tile_height > 0 AND matrix_width > 0 "
                        "AND matrix_height > 0",
                        pszTableName);
    char* pszSQL = CPLStrdup(oStatement.GetCommand());  // save to use later
    int   bGotRow = FALSE;
    const char* pszZoomLevel =  CSLFetchNameValue(papszOpenOptionsIn, "ZOOM_LEVEL");
    CPLDebug("OGRDB2DataSource::OpenRaster",
             "pszZoomLevel: '%s', m_bUpdate: %d", pszZoomLevel, m_bUpdate);

    if( pszZoomLevel )
    {
        if( m_bUpdate )
            oStatement.Appendf(" AND zoom_level <= %d", atoi(pszZoomLevel));
        else
        {
            oStatement.Appendf(" AND (zoom_level = %d OR (zoom_level < %d "
                               "AND EXISTS(SELECT 1 FROM '%s' "
                               "WHERE zoom_level = tm.zoom_level FETCH FIRST ROW ONLY)))",
                               atoi(pszZoomLevel), atoi(pszZoomLevel),
                               pszTableName);
        }
    }
    // In read-only mode, only lists non empty zoom levels
    else if( !m_bUpdate )
    {
        oStatement.Appendf(" AND EXISTS(SELECT 1 FROM %s "
                           "WHERE zoom_level = tm.zoom_level FETCH FIRST ROW ONLY)",
                           pszTableName);
    }
    else // if( pszZoomLevel == nullptr )
    {
        oStatement.Appendf(" AND zoom_level <= (SELECT MAX(zoom_level) FROM %s)",
                           pszTableName);
    }
    oStatement.Appendf(" ORDER BY zoom_level DESC");

    if( oStatement.DB2Execute("OGR_DB2DataSource::OpenRaster"))
    {
        if( oStatement.Fetch())
        {

            CPLDebug("OGR_DB2DataSource::OpenRaster", "col 0: %s",
                     oStatement.GetColData(0));
            bGotRow = TRUE;
        } else {
            CPLDebug("OGR_DB2DataSource::OpenRaster", "Fetch1 failed");
        }
    } else {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "%s", m_oSession.GetLastError() );
        CPLFree(pszSQL);
        return FALSE;
    }

    if( !bGotRow &&
            pszContentsMinX != nullptr && pszContentsMinY != nullptr &&
            pszContentsMaxX != nullptr && pszContentsMaxY != nullptr )
    {
        oStatement.Clear();
        oStatement.Append(pszSQL);
        oStatement.Append(" ORDER BY zoom_level DESC FETCH FIRST ROW ONLY");
        CPLDebug("OGR_DB2DataSource::OpenRaster", "SQL: %s",
                 oStatement.GetCommand());
        if( oStatement.DB2Execute("OGR_DB2DataSource::OpenRaster"))
        {
            if( oStatement.Fetch())
            {

                CPLDebug("OGR_DB2DataSource::OpenRaster", "col 0: %s",
                         oStatement.GetColData(0));
                bGotRow = TRUE;
            }
        } else {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "%s", m_oSession.GetLastError() );
            CPLDebug("OGR_DB2DataSource::OpenRaster", "error: %s",
                     m_oSession.GetLastError() );
            CPLFree(pszSQL);
            return FALSE;
        }
    }
    if( !bGotRow )
    {
        CPLFree(pszSQL);
        return FALSE;
    }
    CPLFree(pszSQL);

    OGRDB2Statement oStatement2( &m_oSession );
    // If USE_TILE_EXTENT=YES, then query the tile table to find which tiles
    // actually exist.
    CPLString osContentsMinX, osContentsMinY, osContentsMaxX, osContentsMaxY;
    if( CPLTestBool(CSLFetchNameValueDef(papszOpenOptionsIn, "USE_TILE_EXTENT", "NO")) )
    {
        oStatement2.Appendf(
            "SELECT MIN(tile_column), MIN(tile_row), MAX(tile_column), MAX(tile_row) FROM %s WHERE zoom_level = %d",
            pszTableName, atoi(oStatement.GetColData(0)));

        if( oStatement2.DB2Execute("OGR_DB2DataSource::OpenRaster"))
        {
            if( oStatement2.Fetch())
            {
                CPLDebug("OGR_DB2DataSource::OpenRaster", "col 0: %s",
                         oStatement2.GetColData(0));
            } else {
                return FALSE;
            }
        } else {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "%s", m_oSession.GetLastError() );
            CPLDebug("OGR_DB2DataSource::OpenRaster", "error: %s",
                     m_oSession.GetLastError() );
            return FALSE;
        }

        double dfPixelXSize = CPLAtof(oStatement.GetColData( 1));
        double dfPixelYSize = CPLAtof(oStatement.GetColData( 2));
        int nTileWidth = atoi(oStatement.GetColData( 3));
        int nTileHeight = atoi(oStatement.GetColData( 4));
        osContentsMinX = CPLSPrintf("%.18g", dfMinX + dfPixelXSize * nTileWidth * atoi(oStatement2.GetColData( 0)));
        osContentsMaxY = CPLSPrintf("%.18g", dfMaxY - dfPixelYSize * nTileHeight * atoi(oStatement2.GetColData( 1)));
        osContentsMaxX = CPLSPrintf("%.18g", dfMinX + dfPixelXSize * nTileWidth * (1 + atoi(oStatement2.GetColData( 2))));
        osContentsMinY = CPLSPrintf("%.18g", dfMaxY - dfPixelYSize * nTileHeight * (1 + atoi(oStatement2.GetColData( 3))));
        pszContentsMinX = osContentsMinX.c_str();
        pszContentsMinY = osContentsMinY.c_str();
        pszContentsMaxX = osContentsMaxX.c_str();
        pszContentsMaxY = osContentsMaxY.c_str();
    }

    if(! InitRaster ( nullptr, pszTableName, dfMinX, dfMinY, dfMaxX, dfMaxY,
                      pszContentsMinX, pszContentsMinY, pszContentsMaxX, pszContentsMaxY,
                      papszOpenOptionsIn, &oStatement, 0) )
    {
        return FALSE;
    }

    CheckUnknownExtensions(TRUE);

    // Do this after CheckUnknownExtensions() so that m_eTF is set to GPKG_TF_WEBP
    // if the table already registers the gpkg_webp extension
    const char* pszTF = CSLFetchNameValue(papszOpenOptionsIn, "TILE_FORMAT");
    if( pszTF )
    {
        if( !m_bUpdate )
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "DRIVER open option ignored in read-only mode");
        }
        else
        {
            GPKGTileFormat eTF = GetTileFormat(pszTF);
            if( eTF == GPKG_TF_WEBP && m_eTF != eTF )
            {
                if( !RegisterWebPExtension() )
                    return FALSE;
            }
            m_eTF = eTF;
        }
    }

    ParseCompressionOptions(papszOpenOptionsIn);
    CPLDebug("OGR_DB2DataSource::OpenRaster", "after ParseCompress");
    m_osWHERE = CSLFetchNameValueDef(papszOpenOptionsIn, "WHERE", "");

    // Set metadata
    if( pszIdentifier && pszIdentifier[0] )
        GDALPamDataset::SetMetadataItem("IDENTIFIER", pszIdentifier);
    if( pszDescription && pszDescription[0] )
        GDALPamDataset::SetMetadataItem("DESCRIPTION", pszDescription);
    CPLDebug("OGR_DB2DataSource::OpenRaster", "check for overviews");
    // Add overviews
    for( int i = 1; oStatement.Fetch(); i++ )
    {
        CPLDebug("OGR_DB2DataSource::OpenRaster",
                 "fetch overview row: %d", i);
        OGRDB2DataSource* poOvrDS = new OGRDB2DataSource();
        poOvrDS->InitRaster ( this, pszTableName, dfMinX, dfMinY, dfMaxX, dfMaxY,
                              pszContentsMinX, pszContentsMinY, pszContentsMaxX, pszContentsMaxY,
                              papszOpenOptionsIn, &oStatement, i);

        m_papoOverviewDS = (OGRDB2DataSource**) CPLRealloc(m_papoOverviewDS,
                           sizeof(OGRDB2DataSource*) * (m_nOverviewCount+1));
        m_papoOverviewDS[m_nOverviewCount ++] = poOvrDS;

        int nTileWidth, nTileHeight;
        poOvrDS->GetRasterBand(1)->GetBlockSize(&nTileWidth, &nTileHeight);
        if( poOvrDS->GetRasterXSize() < nTileWidth &&
                poOvrDS->GetRasterYSize() < nTileHeight )
        {
            break;
        }
    }

    CPLDebug("OGR_DB2DataSource::OpenRaster", "exiting");

    return TRUE;
}

/************************************************************************/
/*                         InitRaster()                                 */
/************************************************************************/

int OGRDB2DataSource::InitRaster ( OGRDB2DataSource* poParentDS,
                                   const char* pszTableName,
                                   double dfMinX,
                                   double dfMinY,
                                   double dfMaxX,
                                   double dfMaxY,
                                   const char* pszContentsMinX,
                                   const char* pszContentsMinY,
                                   const char* pszContentsMaxX,
                                   const char* pszContentsMaxY,
                                   char** papszOpenOptionsIn,
                                   OGRDB2Statement* oStatement,
                                   int nIdxInResult )
{
    m_osRasterTable = pszTableName;
    m_dfTMSMinX = dfMinX;
    m_dfTMSMaxY = dfMaxY;
    CPLDebug("OGRDB2DataSource::InitRaster1", "nIdxInResult: %d", nIdxInResult);
    if (nIdxInResult > 0) {
        CPLDebug("OGRDB2DataSource::InitRaster1",
                 "Serious problem as we don't support nIdxInResult");
    }
    int nZoomLevel = atoi(oStatement->GetColData( 0));
    double dfPixelXSize = CPLAtof(oStatement->GetColData( 1));
    double dfPixelYSize = CPLAtof(oStatement->GetColData( 2));
    int nTileWidth = atoi(oStatement->GetColData( 3));
    int nTileHeight = atoi(oStatement->GetColData( 4));
    int nTileMatrixWidth = atoi(oStatement->GetColData( 5));
    int nTileMatrixHeight = atoi(oStatement->GetColData( 6));

    /* Use content bounds in priority over tile_matrix_set bounds */
    double dfGDALMinX = dfMinX;
    double dfGDALMinY = dfMinY;
    double dfGDALMaxX = dfMaxX;
    double dfGDALMaxY = dfMaxY;
    pszContentsMinX = CSLFetchNameValueDef(papszOpenOptionsIn, "MINX", pszContentsMinX);
    pszContentsMinY = CSLFetchNameValueDef(papszOpenOptionsIn, "MINY", pszContentsMinY);
    pszContentsMaxX = CSLFetchNameValueDef(papszOpenOptionsIn, "MAXX", pszContentsMaxX);
    pszContentsMaxY = CSLFetchNameValueDef(papszOpenOptionsIn, "MAXY", pszContentsMaxY);
    if( pszContentsMinX != nullptr && pszContentsMinY != nullptr &&
            pszContentsMaxX != nullptr && pszContentsMaxY != nullptr )
    {
        dfGDALMinX = CPLAtof(pszContentsMinX);
        dfGDALMinY = CPLAtof(pszContentsMinY);
        dfGDALMaxX = CPLAtof(pszContentsMaxX);
        dfGDALMaxY = CPLAtof(pszContentsMaxY);
    }
    if( dfGDALMinX >= dfGDALMaxX || dfGDALMinY >= dfGDALMaxY )
    {
        return FALSE;
    }

    int nBandCount = atoi(CSLFetchNameValueDef(papszOpenOptionsIn, "BAND_COUNT", "4"));
    if( nBandCount != 1 && nBandCount != 2 && nBandCount != 3 && nBandCount != 4 )
        nBandCount = 4;

    return InitRaster(poParentDS, pszTableName, nZoomLevel, nBandCount, dfMinX, dfMaxY,
                      dfPixelXSize, dfPixelYSize, nTileWidth, nTileHeight,
                      nTileMatrixWidth, nTileMatrixHeight,
                      dfGDALMinX, dfGDALMinY, dfGDALMaxX, dfGDALMaxY );
}

/************************************************************************/
/*                         InitRaster()                                 */
/************************************************************************/

int OGRDB2DataSource::InitRaster ( OGRDB2DataSource* poParentDS,
                                   const char* pszTableName,
                                   int nZoomLevel,
                                   int nBandCount,
                                   double dfTMSMinX,
                                   double dfTMSMaxY,
                                   double dfPixelXSize,
                                   double dfPixelYSize,
                                   int nTileWidth,
                                   int nTileHeight,
                                   int nTileMatrixWidth,
                                   int nTileMatrixHeight,
                                   double dfGDALMinX,
                                   double dfGDALMinY,
                                   double dfGDALMaxX,
                                   double dfGDALMaxY )
{
    CPLDebug("OGRDB2DataSource::InitRaster2","Entering");
    m_osRasterTable = pszTableName;
    m_dfTMSMinX = dfTMSMinX;
    m_dfTMSMaxY = dfTMSMaxY;
    m_nZoomLevel = nZoomLevel;
    m_nTileMatrixWidth = nTileMatrixWidth;
    m_nTileMatrixHeight = nTileMatrixHeight;

    m_bGeoTransformValid = TRUE;
    m_adfGeoTransform[0] = dfGDALMinX;
    m_adfGeoTransform[1] = dfPixelXSize;
    m_adfGeoTransform[3] = dfGDALMaxY;
    m_adfGeoTransform[5] = -dfPixelYSize;
    double dfRasterXSize = 0.5 + (dfGDALMaxX - dfGDALMinX) / dfPixelXSize;
    double dfRasterYSize = 0.5 + (dfGDALMaxY - dfGDALMinY) / dfPixelYSize;
    if( dfRasterXSize > INT_MAX || dfRasterYSize > INT_MAX )
        return FALSE;
    nRasterXSize = (int)dfRasterXSize;
    nRasterYSize = (int)dfRasterYSize;

    m_pabyCachedTiles = (GByte*) VSI_MALLOC3_VERBOSE(4 * 4, nTileWidth, nTileHeight);
    if( m_pabyCachedTiles == nullptr )
    {
        return FALSE;
    }

    for(int i = 1; i <= nBandCount; i ++)
        SetBand( i, new GDALDB2RasterBand(this, i, nTileWidth, nTileHeight) );

    ComputeTileAndPixelShifts();

    GDALPamDataset::SetMetadataItem("INTERLEAVE", "PIXEL", "IMAGE_STRUCTURE");
    GDALPamDataset::SetMetadataItem("ZOOM_LEVEL", CPLSPrintf("%d", m_nZoomLevel));

    if( poParentDS )
    {
        m_poParentDS = poParentDS;
        m_bUpdate = poParentDS->m_bUpdate;
        eAccess = poParentDS->eAccess;
        m_eTF = poParentDS->m_eTF;
        m_nQuality = poParentDS->m_nQuality;
        m_nZLevel = poParentDS->m_nZLevel;
        m_bDither = poParentDS->m_bDither;
        /*m_nSRID = poParentDS->m_nSRID;*/
        m_osWHERE = poParentDS->m_osWHERE;
        SetDescription(CPLSPrintf("%s - zoom_level=%d",
                                  poParentDS->GetDescription(), m_nZoomLevel));
    }

    return TRUE;
}

/************************************************************************/
/*                         GetTileFormat()                              */
/************************************************************************/

static GPKGTileFormat GetTileFormat(const char* pszTF )
{
    GPKGTileFormat eTF = GPKG_TF_PNG_JPEG;
    if( pszTF )
    {
        if( EQUAL(pszTF, "PNG_JPEG") )
            eTF = GPKG_TF_PNG_JPEG;
        else if( EQUAL(pszTF, "PNG") )
            eTF = GPKG_TF_PNG;
        else if( EQUAL(pszTF, "PNG8") )
            eTF = GPKG_TF_PNG8;
        else if( EQUAL(pszTF, "JPEG") )
            eTF = GPKG_TF_JPEG;
        else if( EQUAL(pszTF, "WEBP") )
            eTF = GPKG_TF_WEBP;
    }
    return eTF;
}

/************************************************************************/
/*                          RegisterWebPExtension()                     */
/************************************************************************/

int OGRDB2DataSource::RegisterWebPExtension()
{
    CPLDebug("OGRDB2DataSource::RegisterWebPExtension", "NO-OP");

    CreateExtensionsTableIfNecessary();
#ifdef LATER
    char* pszSQL = sqlite3_mprintf(
                       "INSERT INTO gpkg_extensions "
                       "(table_name, column_name, extension_name, definition, scope) "
                       "VALUES "
                       "('%q', 'tile_data', 'gpkg_webp', 'GeoPackage 1.0 Specification Annex P', 'read-write')",
                       m_osRasterTable.c_str());
    OGRErr eErr = SQLCommand(hDB, pszSQL);
    sqlite3_free(pszSQL);
    if ( OGRERR_NONE != eErr )
        return FALSE;
#endif
    return TRUE;
}

/************************************************************************/
/*                    CheckUnknownExtensions()                          */
/************************************************************************/

void OGRDB2DataSource::CheckUnknownExtensions(int /*bCheckRasterTable*/)
{
    if( !HasExtensionsTable() )
        return;
    CPLDebug("OGRDB2DataSource::CheckUnknownExtensions","NO-OP");
#ifdef LATER
    char* pszSQL;
    if( !bCheckRasterTable)
        pszSQL = sqlite3_mprintf(
                     "SELECT extension_name, definition, scope FROM gpkg_extensions WHERE table_name IS NULL AND extension_name != 'gdal_aspatial'");
    else
        pszSQL = sqlite3_mprintf(
                     "SELECT extension_name, definition, scope FROM gpkg_extensions WHERE table_name = '%q'",
                     m_osRasterTable.c_str());

    SQLResult oResultTable;
    OGRErr err = SQLQuery(GetDB(), pszSQL, &oResultTable);
    sqlite3_free(pszSQL);
    if ( err == OGRERR_NONE && oResultTable.nRowCount > 0 )
    {
        for(int i=0; i<oResultTable.nRowCount; i++)
        {
            const char* pszExtName = SQLResultGetValue(&oResultTable, 0, i);
            const char* pszDefinition = SQLResultGetValue(&oResultTable, 1, i);
            const char* pszScope = SQLResultGetValue(&oResultTable, 2, i);
            if( pszExtName == NULL ) pszExtName = "(null)";
            if( pszDefinition == NULL ) pszDefinition = "(null)";
            if( pszScope == NULL ) pszScope = "(null)";

            if( EQUAL(pszExtName, "gpkg_webp") )
            {
                if( GDALGetDriverByName("WEBP") == NULL )
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "Table %s contains WEBP tiles, but GDAL configured "
                             "without WEBP support. Data will be missing",
                             m_osRasterTable.c_str());
                }
                m_eTF = GPKG_TF_WEBP;
                continue;
            }
            if( EQUAL(pszExtName, "gpkg_zoom_other") )
            {
                m_bZoomOther = TRUE;
                continue;
            }

            if( GetUpdate() && EQUAL(pszScope, "write-only") )
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Database relies on the '%s' (%s) extension that should "
                         "be implemented for safe write-support, but is not currently. "
                         "Update of that database are strongly discouraged to avoid corruption.",
                         pszExtName, pszDefinition);
            }
            else if( GetUpdate() && EQUAL(pszScope, "read-write") )
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Database relies on the '%s' (%s) extension that should "
                         "be implemented in order to read/write it safely, but is not currently. "
                         "Some data may be missing while reading that database, and updates are strongly discouraged.",
                         pszExtName, pszDefinition);
            }
            else if( EQUAL(pszScope, "read-write") )
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Database relies on the '%s' (%s) extension that should "
                         "be implemented in order to read it safely, but is not currently. "
                         "Some data may be missing while reading that database.",
                         pszExtName, pszDefinition);
            }
        }
    }
    SQLResultFree(&oResultTable);
#endif
}

/************************************************************************/
/*                        ParseCompressionOptions()                     */
/************************************************************************/

void OGRDB2DataSource::ParseCompressionOptions(char** papszOptions)
{
    const char* pszZLevel = CSLFetchNameValue(papszOptions, "ZLEVEL");
    if( pszZLevel )
        m_nZLevel = atoi(pszZLevel);

    const char* pszQuality = CSLFetchNameValue(papszOptions, "QUALITY");
    if( pszQuality )
        m_nQuality = atoi(pszQuality);

    const char* pszDither = CSLFetchNameValue(papszOptions, "DITHER");
    if( pszDither )
        m_bDither = CPLTestBool(pszDither);
}

/************************************************************************/
/*                      ComputeTileAndPixelShifts()                     */
/************************************************************************/

void OGRDB2DataSource::ComputeTileAndPixelShifts()
{
    CPLDebug("OGRDB2DataSource::ComputeTileAndPixelShifts", "Entering");
    int nTileWidth, nTileHeight;
    GetRasterBand(1)->GetBlockSize(&nTileWidth, &nTileHeight);

    // Compute shift between GDAL origin and TileMatrixSet origin
    int nShiftXPixels = (int)floor(0.5 + (m_adfGeoTransform[0] - m_dfTMSMinX) /  m_adfGeoTransform[1]);
    m_nShiftXTiles = (int)floor(1.0 * nShiftXPixels / nTileWidth);
    m_nShiftXPixelsMod = ((nShiftXPixels % nTileWidth) + nTileWidth) % nTileWidth;
    int nShiftYPixels = (int)floor(0.5 + (m_adfGeoTransform[3] - m_dfTMSMaxY) /  m_adfGeoTransform[5]);
    m_nShiftYTiles = (int)floor(1.0 * nShiftYPixels / nTileHeight);
    m_nShiftYPixelsMod = ((nShiftYPixels % nTileHeight) + nTileHeight) % nTileHeight;
}

/************************************************************************/
/*                  CreateExtensionsTableIfNecessary()                  */
/************************************************************************/

OGRErr OGRDB2DataSource::CreateExtensionsTableIfNecessary()
{

    /* Check if the table gpkg_extensions exists */
    if( HasExtensionsTable() )
        return OGRERR_NONE;
    CPLDebug("OGRDB2DataSource::CreateExtensionsTableIfNecessary", "NO-OP");
#ifdef LATER
    /* Requirement 79 : Every extension of a GeoPackage SHALL be registered */
    /* in a corresponding row in the gpkg_extensions table. The absence of a */
    /* gpkg_extensions table or the absence of rows in gpkg_extensions table */
    /* SHALL both indicate the absence of extensions to a GeoPackage. */
    const char* pszCreateGpkgExtensions =
        "CREATE TABLE gpkg_extensions ("
        "table_name TEXT,"
        "column_name TEXT,"
        "extension_name TEXT NOT NULL,"
        "definition TEXT NOT NULL,"
        "scope TEXT NOT NULL,"
        "CONSTRAINT ge_tce UNIQUE (table_name, column_name, extension_name)"
        ")";

    return SQLCommand(hDB, pszCreateGpkgExtensions);
#endif
    return OGRERR_NONE;
}

/************************************************************************/
/*                         HasExtensionsTable()                         */
/************************************************************************/

int OGRDB2DataSource::HasExtensionsTable()
{
    CPLDebug("OGRDB2DataSource::HasExtensionsTable", "NO-OP");
#ifdef LATER
    SQLResult oResultTable;
    OGRErr err = SQLQuery(hDB,
                          "SELECT * FROM sqlite_master WHERE name = 'gpkg_extensions' "
                          "AND type IN ('table', 'view')", &oResultTable);
    int bHasExtensionsTable = ( err == OGRERR_NONE && oResultTable.nRowCount == 1 );
    SQLResultFree(&oResultTable);
    return bHasExtensionsTable;
#endif
    return OGRERR_NONE;
}

/************************************************************************/
/*                             FlushCache()                             */
/************************************************************************/

void OGRDB2DataSource::FlushCache()
{
    DB2_DEBUG_ENTER("OGRDB2DataSource::FlushCache");
    FlushCacheWithErrCode();
    DB2_DEBUG_EXIT("OGRDB2DataSource::FlushCache");
}

CPLErr OGRDB2DataSource::FlushCacheWithErrCode()

{
    CPLDebug("OGRDB2DataSource::FlushCacheWithErrCode","m_bInFlushCache %d", m_bInFlushCache);

    if( m_bInFlushCache )
        return CE_None;
    m_bInFlushCache = TRUE;
    // Short circuit GDALPamDataset to avoid serialization to .aux.xml
    CPLDebug("OGRDB2DataSource::FlushCacheWithErrCode","calling GDALDataset::FlushCache");
    GDALDataset::FlushCache();
/* Not sure what this has to do with raster operations
    for( int i = 0; i < m_nLayers; i++ )
    {
        m_papoLayers[i]->RunDeferredCreationIfNecessary();
        m_papoLayers[i]->CreateSpatialIndexIfNecessary();
    }
*/

    CPLErr eErr = CE_None;
    CPLDebug("OGRDB2DataSource::FlushCacheWithErrCode","m_bUpdate %d", m_bUpdate);

    if( m_bUpdate )
    {
        if( m_nShiftXPixelsMod || m_nShiftYPixelsMod )
        {
            eErr = FlushRemainingShiftedTiles();
        }
        else
        {
            eErr = WriteTile();
        }
    }

    OGRDB2DataSource* poMainDS = m_poParentDS ? m_poParentDS : this;
    CPLDebug("OGRDB2DataSource::FlushCacheWithErrCode",
             "m_nTileInsertionCount: %d", poMainDS->m_nTileInsertionCount);

    if( poMainDS->m_nTileInsertionCount )
    {
        poMainDS->SoftCommitTransaction();
        poMainDS->m_nTileInsertionCount = 0;
    }

    m_bInFlushCache = FALSE;
    CPLDebug("OGRDB2DataSource::FlushCacheWithErrCode","exiting; eErr: %d", eErr);

    return eErr;
}

/************************************************************************/
/*                         SoftStartTransaction()                       */
/************************************************************************/

int OGRDB2DataSource::SoftStartTransaction()
{
    CPLDebug("OGRDB2DataSource::SoftStartTransaction", "enter");
    return m_oSession.BeginTransaction();
}

/************************************************************************/
/*                         SoftCommitTransaction()                      */
/************************************************************************/

int OGRDB2DataSource::SoftCommitTransaction()
{
    CPLDebug("OGRDB2DataSource::SoftCommitTransaction", "enter");
    return m_oSession.CommitTransaction();
}

/************************************************************************/
/*                         SoftRollbackTransaction()                    */
/************************************************************************/

int OGRDB2DataSource::SoftRollbackTransaction()
{
    CPLDebug("OGRDB2DataSource::SoftRollbackTransaction", "enter");
    return m_oSession.RollbackTransaction();
}

/************************************************************************/
/*                            CreateCopy()                              */
/************************************************************************/

typedef struct
{
    const char*         pszName;
    GDALResampleAlg     eResampleAlg;
} WarpResamplingAlg;

static const WarpResamplingAlg asResamplingAlg[] =
{
    { "BILINEAR", GRA_Bilinear },
    { "CUBIC", GRA_Cubic },
    { "CUBICSPLINE", GRA_CubicSpline },
    { "LANCZOS", GRA_Lanczos },
    { "MODE", GRA_Mode },
    { "AVERAGE", GRA_Average },
    { "RMS", GRA_RMS },
};

static void DumpStringList(char **papszStrList)
{
    if (papszStrList == nullptr)
        return ;

    while( *papszStrList != nullptr )
    {
        CPLDebug("DumpStringList",": '%s'", *papszStrList);
        ++papszStrList;
    }
    return ;
}

GDALDataset* OGRDB2DataSource::CreateCopy( const char *pszFilename,
        GDALDataset *poSrcDS,
        int bStrict,
        char ** papszOptions,
        GDALProgressFunc pfnProgress,
        void * pProgressData )
{
    CPLDebug("OGRDB2DataSource::CreateCopy","pszFilename: '%s'", pszFilename);
    CPLDebug("OGRDB2DataSource::CreateCopy","srcDescription: '%s'", poSrcDS->GetDescription());
    const char* pszTilingScheme =
        CSLFetchNameValueDef(papszOptions, "TILING_SCHEME", "CUSTOM");
    DumpStringList(papszOptions);
    char** papszUpdatedOptions = CSLDuplicate(papszOptions);
    if( CSLFetchNameValue(papszOptions, "RASTER_TABLE") == nullptr )
    {
        papszUpdatedOptions = CSLSetNameValue(papszUpdatedOptions,
                                              "RASTER_TABLE",
                                              CPLGetBasename(poSrcDS->GetDescription()));
    }
    DumpStringList(papszUpdatedOptions);
    if( EQUAL(pszTilingScheme, "CUSTOM") )
    {
        GDALDriver* poThisDriver = (GDALDriver*)GDALGetDriverByName("DB2ODBC");
        if( !poThisDriver )
        {
            CSLDestroy(papszUpdatedOptions);
            return nullptr;
        }
        CPLDebug("OGRDB2DataSource::CreateCopy","calling DefaultCreateCopy");

        GDALDataset* poDS = poThisDriver->DefaultCreateCopy(
                                pszFilename, poSrcDS, bStrict,
                                papszUpdatedOptions, pfnProgress, pProgressData );
        CPLDebug("OGRDB2DataSource::CreateCopy","returned from DefaultCreateCopy");
        CSLDestroy(papszUpdatedOptions);
        return poDS;
    }

    int nBands = poSrcDS->GetRasterCount();
    if( nBands != 1 && nBands != 2 && nBands != 3 && nBands != 4 )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Only 1 (Grey/ColorTable), 2 (Grey+Alpha), 3 (RGB) or 4 (RGBA) band dataset supported");
        CSLDestroy(papszUpdatedOptions);
        return nullptr;
    }

    int nEPSGCode = 0;
    size_t iScheme = 0;
    const size_t nTilingSchemeCount =
        sizeof(asTilingShemes)/sizeof(asTilingShemes[0]);
    for(;
            iScheme < nTilingSchemeCount;
            iScheme++ )
    {
        if( EQUAL(pszTilingScheme, asTilingShemes[iScheme].pszName) )
        {
            nEPSGCode = asTilingShemes[iScheme].nEPSGCode;
            break;
        }
    }
    if( iScheme == nTilingSchemeCount )
    {
        CSLDestroy(papszUpdatedOptions);
        return nullptr;
    }

    OGRSpatialReference oSRS;
    if( oSRS.importFromEPSG(nEPSGCode) != OGRERR_NONE )
    {
        CSLDestroy(papszUpdatedOptions);
        return nullptr;
    }
    char* pszWKT = nullptr;
    oSRS.exportToWkt(&pszWKT);
    char** papszTO = CSLSetNameValue( nullptr, "DST_SRS", pszWKT );
    void* hTransformArg =
        GDALCreateGenImgProjTransformer2( poSrcDS, nullptr, papszTO );
    if( hTransformArg == nullptr )
    {
        CSLDestroy(papszUpdatedOptions);
        CPLFree(pszWKT);
        CSLDestroy(papszTO);
        return nullptr;
    }

    GDALTransformerInfo* psInfo = (GDALTransformerInfo*)hTransformArg;
    double adfGeoTransform[6];
    double adfExtent[4];
    int    nXSize, nYSize;

    if ( GDALSuggestedWarpOutput2( poSrcDS,
                                   psInfo->pfnTransform, hTransformArg,
                                   adfGeoTransform,
                                   &nXSize, &nYSize,
                                   adfExtent, 0 ) != CE_None )
    {
        CSLDestroy(papszUpdatedOptions);
        CPLFree(pszWKT);
        CSLDestroy(papszTO);
        GDALDestroyGenImgProjTransformer( hTransformArg );
        return nullptr;
    }

    GDALDestroyGenImgProjTransformer( hTransformArg );
    hTransformArg = nullptr;

    int nZoomLevel;
    double dfComputedRes = adfGeoTransform[1];
    double dfPrevRes = 0, dfRes = 0;
    for(nZoomLevel = 0; nZoomLevel < 25; nZoomLevel++)
    {
        dfRes = asTilingShemes[iScheme].dfPixelXSizeZoomLevel0 / (1 << nZoomLevel);
        if( dfComputedRes > dfRes )
            break;
        dfPrevRes = dfRes;
    }
    if( nZoomLevel == 25 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Could not find an appropriate zoom level");
        CSLDestroy(papszUpdatedOptions);
        CPLFree(pszWKT);
        CSLDestroy(papszTO);
        return nullptr;
    }

    const char* pszZoomLevelStrategy = CSLFetchNameValueDef(papszOptions,
                                       "ZOOM_LEVEL_STRATEGY",
                                       "AUTO");
    if( fabs( dfComputedRes - dfRes ) / dfRes > 1e-8 )
    {
        if( EQUAL(pszZoomLevelStrategy, "LOWER") )
        {
            if( nZoomLevel > 0 )
                nZoomLevel --;
        }
        else if( EQUAL(pszZoomLevelStrategy, "UPPER") )
        {
            /* do nothing */
        }
        else if( nZoomLevel > 0 )
        {
            if( dfPrevRes / dfComputedRes < dfComputedRes / dfRes )
                nZoomLevel --;
        }
    }

    dfRes = asTilingShemes[iScheme].dfPixelXSizeZoomLevel0 / (1 << nZoomLevel);

    double dfMinX = adfExtent[0];
    double dfMinY = adfExtent[1];
    double dfMaxX = adfExtent[2];
    double dfMaxY = adfExtent[3];

    nXSize = (int) ( 0.5 + ( dfMaxX - dfMinX ) / dfRes );
    nYSize = (int) ( 0.5 + ( dfMaxY - dfMinY ) / dfRes );
    adfGeoTransform[1] = dfRes;
    adfGeoTransform[5] = -dfRes;

    int nTargetBands = nBands;
    /* For grey level or RGB, if there's reprojection involved, add an alpha */
    /* channel */
    if( (nBands == 1 && poSrcDS->GetRasterBand(1)->GetColorTable() == nullptr) ||
            nBands == 3 )
    {
        OGRSpatialReference oSrcSRS;
        oSrcSRS.SetFromUserInput(poSrcDS->GetProjectionRef());
        oSrcSRS.AutoIdentifyEPSG();
        if( oSrcSRS.GetAuthorityCode(nullptr) == nullptr ||
                atoi(oSrcSRS.GetAuthorityCode(nullptr)) != nEPSGCode )
        {
            nTargetBands ++;
        }
    }

    OGRDB2DataSource* poDS = new OGRDB2DataSource();
    if( !(poDS->Create( pszFilename, nXSize, nYSize, nTargetBands, GDT_Byte,
                        papszUpdatedOptions )) )
    {
        delete poDS;
        CSLDestroy(papszUpdatedOptions);
        CPLFree(pszWKT);
        CSLDestroy(papszTO);
        return nullptr;
    }
    CSLDestroy(papszUpdatedOptions);
    papszUpdatedOptions = nullptr;
    poDS->SetGeoTransform(adfGeoTransform);
    poDS->SetProjection(pszWKT);
    CPLFree(pszWKT);
    pszWKT = nullptr;

    hTransformArg =
        GDALCreateGenImgProjTransformer2( poSrcDS, poDS, papszTO );
    CSLDestroy(papszTO);
    if( hTransformArg == nullptr )
    {
        delete poDS;
        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Warp the transformer with a linear approximator                 */
    /* -------------------------------------------------------------------- */
    hTransformArg =
        GDALCreateApproxTransformer( GDALGenImgProjTransform,
                                     hTransformArg, 0.125 );
    GDALApproxTransformerOwnsSubtransformer(hTransformArg, TRUE);

    /* -------------------------------------------------------------------- */
    /*      Setup warp options.                                             */
    /* -------------------------------------------------------------------- */
    GDALWarpOptions *psWO = GDALCreateWarpOptions();

    psWO->papszWarpOptions = nullptr;
    psWO->eWorkingDataType = GDT_Byte;

    GDALResampleAlg eResampleAlg = GRA_Bilinear;
    const char* pszResampling = CSLFetchNameValue(papszOptions, "RESAMPLING");
    if( pszResampling )
    {
        for(size_t iAlg = 0; iAlg < sizeof(asResamplingAlg)/sizeof(asResamplingAlg[0]); iAlg ++)
        {
            if( EQUAL(pszResampling, asResamplingAlg[iAlg].pszName) )
            {
                eResampleAlg = asResamplingAlg[iAlg].eResampleAlg;
                break;
            }
        }
    }
    psWO->eResampleAlg = eResampleAlg;

    psWO->hSrcDS = poSrcDS;
    psWO->hDstDS = poDS;

    psWO->pfnTransformer = GDALApproxTransform;
    psWO->pTransformerArg = hTransformArg;

    psWO->pfnProgress = pfnProgress;
    psWO->pProgressArg = pProgressData;

    /* -------------------------------------------------------------------- */
    /*      Setup band mapping.                                             */
    /* -------------------------------------------------------------------- */

    if( nBands == 2 || nBands == 4 )
        psWO->nBandCount = nBands - 1;
    else
        psWO->nBandCount = nBands;

    psWO->panSrcBands = (int *) CPLMalloc(psWO->nBandCount*sizeof(int));
    psWO->panDstBands = (int *) CPLMalloc(psWO->nBandCount*sizeof(int));

    for( int i = 0; i < psWO->nBandCount; i++ )
    {
        psWO->panSrcBands[i] = i+1;
        psWO->panDstBands[i] = i+1;
    }

    if( nBands == 2 || nBands == 4 )
    {
        psWO->nSrcAlphaBand = nBands;
    }
    if( nTargetBands == 2 || nTargetBands == 4 )
    {
        psWO->nDstAlphaBand = nTargetBands;
    }

    /* -------------------------------------------------------------------- */
    /*      Initialize and execute the warp.                                */
    /* -------------------------------------------------------------------- */
    GDALWarpOperation oWO;

    CPLErr eErr = oWO.Initialize( psWO );
    if( eErr == CE_None )
    {
        /*if( bMulti )
            eErr = oWO.ChunkAndWarpMulti( 0, 0, nXSize, nYSize );
        else*/
        eErr = oWO.ChunkAndWarpImage( 0, 0, nXSize, nYSize );
    }
    if (eErr != CE_None)
    {
        delete poDS;
        poDS = nullptr;
    }

    GDALDestroyTransformer( hTransformArg );
    GDALDestroyWarpOptions( psWO );

    return poDS;
}

/************************************************************************/
/*                         GetProjectionRef()                           */
/************************************************************************/

const char* OGRDB2DataSource::_GetProjectionRef()
{
    return (m_pszProjection) ? m_pszProjection : "";
}

/************************************************************************/
/*                           SetProjection()                            */
/************************************************************************/

CPLErr OGRDB2DataSource::_SetProjection( const char* pszProjection )
{
    CPLDebug("OGRDB2DataSource::SetProjection",
            "pszProjection: '%s'", pszProjection);
    if( nBands == 0)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "SetProjection() not supported on a dataset with 0 band");
        return CE_Failure;
    }
    if( eAccess != GA_Update )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "SetProjection() not supported on read-only dataset");
        return CE_Failure;
    }

    int nSRID;
    if( pszProjection == nullptr || pszProjection[0] == '\0' )
    {
        nSRID = -1;
    }
    else
    {
        OGRSpatialReference oSRS;
        if( oSRS.SetFromUserInput(pszProjection) != OGRERR_NONE )
            return CE_Failure;
        nSRID = FetchSRSId( &oSRS );
    }

    for(size_t iScheme = 0;
            iScheme < sizeof(asTilingShemes)/sizeof(asTilingShemes[0]);
            iScheme++ )
    {
        if( EQUAL(m_osTilingScheme, asTilingShemes[iScheme].pszName) )
        {
            if( nSRID != asTilingShemes[iScheme].nEPSGCode )
            {
                CPLError(CE_Failure, CPLE_NotSupported,
                         "Projection should be EPSG:%d for %s tiling scheme",
                         asTilingShemes[iScheme].nEPSGCode,
                         m_osTilingScheme.c_str());
                return CE_Failure;
            }
        }
    }

    m_nSRID = nSRID;
    CPLFree(m_pszProjection);
    m_pszProjection = pszProjection ? CPLStrdup(pszProjection): CPLStrdup("");

    if( m_bRecordInsertedInGPKGContent )
    {

        OGRDB2Statement oStatement( GetSession() );
        oStatement.Appendf( "UPDATE gpkg.contents SET srs_id = %d "
                            "WHERE table_name = '%s'",
                            m_nSRID, m_osRasterTable.c_str());

        if( !oStatement.DB2Execute("OGRDB2DataSource::SetProjection") )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Set projection failed in gpkg.contents "
                     "for table %s: %s",
                     m_osRasterTable.c_str(),
                     GetSession()->GetLastError());
            CPLDebug("OGRDB2DataSource::SetProjection",
                     "Set projection failed in gpkg.contents "
                     "for table %s: %s",
                     m_osRasterTable.c_str(),
                     GetSession()->GetLastError());
            return CE_Failure;
        }
        oStatement.Clear();
        oStatement.Appendf( "UPDATE gpkg.tile_matrix_set SET srs_id = %d "
                            "WHERE table_name = '%s'",
                            m_nSRID, m_osRasterTable.c_str());

        if( !oStatement.DB2Execute("OGRDB2DataSource::SetProjection") )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Set projection in gpkg.tile_matrix_set failed "
                     "for table %s: %s",
                     m_osRasterTable.c_str(),
                     GetSession()->GetLastError());
            CPLDebug("OGRDB2DataSource::SetProjection",
                     "Set projection in gpkg.tile_matrix_set failed "
                     "for table %s: %s",
                     m_osRasterTable.c_str(),
                     GetSession()->GetLastError());
            return CE_Failure;
        }
    }
    return CE_None;
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr OGRDB2DataSource::GetGeoTransform( double* padfGeoTransform )
{
    memcpy(padfGeoTransform, m_adfGeoTransform, 6 * sizeof(double));
    if( !m_bGeoTransformValid )
        return CE_Failure;
    else
        return CE_None;
}

/************************************************************************/
/*                          SetGeoTransform()                           */
/************************************************************************/

CPLErr OGRDB2DataSource::SetGeoTransform( double* padfGeoTransform )
{
    if( nBands == 0)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "SetGeoTransform() not supported on a dataset with 0 band");
        return CE_Failure;
    }
    if( eAccess != GA_Update )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "SetGeoTransform() not supported on read-only dataset");
        return CE_Failure;
    }
    if( m_bGeoTransformValid )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Cannot modify geotransform once set");
        return CE_Failure;
    }
    if( padfGeoTransform[2] != 0.0 || padfGeoTransform[4] != 0 ||
            padfGeoTransform[5] > 0.0 )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Only north-up non rotated geotransform supported");
        return CE_Failure;
    }

    for(size_t iScheme = 0;
            iScheme < sizeof(asTilingShemes)/sizeof(asTilingShemes[0]);
            iScheme++ )
    {
        if( EQUAL(m_osTilingScheme, asTilingShemes[iScheme].pszName) )
        {
            double dfPixelXSizeZoomLevel0 = asTilingShemes[iScheme].dfPixelXSizeZoomLevel0;
            double dfPixelYSizeZoomLevel0 = asTilingShemes[iScheme].dfPixelYSizeZoomLevel0;
            for( m_nZoomLevel = 0; m_nZoomLevel < 25; m_nZoomLevel++ )
            {
                double dfExpectedPixelXSize = dfPixelXSizeZoomLevel0 / (1 << m_nZoomLevel);
                double dfExpectedPixelYSize = dfPixelYSizeZoomLevel0 / (1 << m_nZoomLevel);
                if( fabs( padfGeoTransform[1] - dfExpectedPixelXSize ) < 1e-8 * dfExpectedPixelXSize &&
                        fabs( fabs(padfGeoTransform[5]) - dfExpectedPixelYSize ) < 1e-8 * dfExpectedPixelYSize )
                {
                    break;
                }
            }
            if( m_nZoomLevel == 25 )
            {
                m_nZoomLevel = -1;
                CPLError(CE_Failure, CPLE_NotSupported,
                         "Could not find an appropriate zoom level of %s tiling scheme that matches raster pixel size",
                         m_osTilingScheme.c_str());
                return CE_Failure;
            }
            break;
        }
    }

    memcpy(m_adfGeoTransform, padfGeoTransform, 6 * sizeof(double));
    m_bGeoTransformValid = TRUE;

    return FinalizeRasterRegistration();
}

/************************************************************************/
/*                      FinalizeRasterRegistration()                    */
/************************************************************************/

CPLErr OGRDB2DataSource::FinalizeRasterRegistration()
{
    CPLDebug("OGRDB2DataSource::FinalizeRasterRegistration","Entering");

    m_dfTMSMinX = m_adfGeoTransform[0];
    m_dfTMSMaxY = m_adfGeoTransform[3];

    int nTileWidth, nTileHeight;
    GetRasterBand(1)->GetBlockSize(&nTileWidth, &nTileHeight);
    m_nTileMatrixWidth = (nRasterXSize + nTileWidth - 1) / nTileWidth;
    m_nTileMatrixHeight = (nRasterYSize + nTileHeight - 1) / nTileHeight;
    CPLDebug("OGRDB2DataSource::FinalizeRasterRegistration",
             "m_nZoomLevel: %d; nTileWidth: %d; nTileHeight %d",
             m_nZoomLevel, nTileWidth, nTileHeight);
    CPLDebug("OGRDB2DataSource::FinalizeRasterRegistration",
             "nRasterXSize: %d; nRasterYSize %d",
             nRasterXSize, nRasterYSize);
    if( m_nZoomLevel < 0 )
    {
        m_nZoomLevel = 0;
        while( (nRasterXSize >> m_nZoomLevel) > nTileWidth ||
                (nRasterYSize >> m_nZoomLevel) > nTileHeight )
            m_nZoomLevel ++;
    }

    double dfPixelXSizeZoomLevel0 = m_adfGeoTransform[1] * (1 << m_nZoomLevel);
    double dfPixelYSizeZoomLevel0 = fabs(m_adfGeoTransform[5]) * (1 << m_nZoomLevel);
    int nTileXCountZoomLevel0 = ((nRasterXSize >> m_nZoomLevel) + nTileWidth - 1) / nTileWidth;
    int nTileYCountZoomLevel0 = ((nRasterYSize >> m_nZoomLevel) + nTileHeight - 1) / nTileHeight;

    for(size_t iScheme = 0;
            iScheme < sizeof(asTilingShemes)/sizeof(asTilingShemes[0]);
            iScheme++ )
    {
        if( EQUAL(m_osTilingScheme, asTilingShemes[iScheme].pszName) )
        {
            CPLAssert( m_nZoomLevel >= 0 );
            m_dfTMSMinX = asTilingShemes[iScheme].dfMinX;
            m_dfTMSMaxY = asTilingShemes[iScheme].dfMaxY;
            dfPixelXSizeZoomLevel0 = asTilingShemes[iScheme].dfPixelXSizeZoomLevel0;
            dfPixelYSizeZoomLevel0 = asTilingShemes[iScheme].dfPixelYSizeZoomLevel0;
            nTileXCountZoomLevel0 = asTilingShemes[iScheme].nTileXCountZoomLevel0;
            nTileYCountZoomLevel0 = asTilingShemes[iScheme].nTileYCountZoomLevel0;
            m_nTileMatrixWidth = nTileXCountZoomLevel0 * (1 << m_nZoomLevel);
            m_nTileMatrixHeight = nTileYCountZoomLevel0 * (1 << m_nZoomLevel);
            break;
        }
    }

    ComputeTileAndPixelShifts();

    double dfGDALMinX = m_adfGeoTransform[0];
    double dfGDALMinY = m_adfGeoTransform[3] + nRasterYSize * m_adfGeoTransform[5];
    double dfGDALMaxX = m_adfGeoTransform[0] + nRasterXSize * m_adfGeoTransform[1];
    double dfGDALMaxY = m_adfGeoTransform[3];

    OGRDB2Statement oStatement( GetSession() );
    oStatement.Appendf( "INSERT INTO gpkg.contents "
                        "(table_name,data_type,identifier,description,min_x,min_y,max_x,max_y,srs_id) VALUES "
                        "('%s','tiles','%s','%s',%.18g,%.18g,%.18g,%.18g,%d)",
                        m_osRasterTable.c_str(),
                        m_osIdentifier.c_str(),
                        m_osDescription.c_str(),
                        dfGDALMinX, dfGDALMinY, dfGDALMaxX, dfGDALMaxY,
                        m_nSRID);

    if( !oStatement.DB2Execute("OGRDB2DataSource::FinalizeRasterRegistration") )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Insert into gpkg.contents failed");
        CPLDebug("OGRDB2DataSource::FinalizeRasterRegistration",
                 "Insert into gpkg.contents failed;"
                 "error: %s; ",
                 GetSession()->GetLastError());
        return CE_Failure;
    }

    double dfTMSMaxX = m_dfTMSMinX + nTileXCountZoomLevel0 * nTileWidth * dfPixelXSizeZoomLevel0;
    double dfTMSMinY = m_dfTMSMaxY - nTileYCountZoomLevel0 * nTileHeight * dfPixelYSizeZoomLevel0;

    oStatement.Clear();
    oStatement.Appendf( "INSERT INTO gpkg.tile_matrix_set "
                        "(table_name,srs_id,min_x,min_y,max_x,max_y) VALUES "
                        "('%s',%d,%.18g,%.18g,%.18g,%.18g)",
                        m_osRasterTable.c_str(), m_nSRID,
                        m_dfTMSMinX,dfTMSMinY,dfTMSMaxX,m_dfTMSMaxY);

    if( !oStatement.DB2Execute("OGRDB2DataSource::FinalizeRasterRegistration") )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Insert into gpkg.tile_matrix_set failed");
        CPLDebug("OGRDB2DataSource::FinalizeRasterRegistration",
                 "Insert into gpkg.tile_matrix_set failed;"
                 "error: %s; ",
                 GetSession()->GetLastError());
        return CE_Failure;
    }

    m_papoOverviewDS = (OGRDB2DataSource**) CPLCalloc(sizeof(OGRDB2DataSource*),
                       m_nZoomLevel);
    CPLDebug("OGRDB2DataSource::FinalizeRasterRegistration",
             "m_nZoomLevel: %d", m_nZoomLevel);
    for(int i=0; i<=m_nZoomLevel; i++)
    {
        double dfPixelXSizeZoomLevel, dfPixelYSizeZoomLevel;
        int nTileMatrixWidth, nTileMatrixHeight;
        if( EQUAL(m_osTilingScheme, "CUSTOM") )
        {
            dfPixelXSizeZoomLevel = m_adfGeoTransform[1] * (1 << (m_nZoomLevel-i));
            dfPixelYSizeZoomLevel = fabs(m_adfGeoTransform[5]) * (1 << (m_nZoomLevel-i));
            nTileMatrixWidth = ((nRasterXSize >> (m_nZoomLevel-i)) + nTileWidth - 1) / nTileWidth;
            nTileMatrixHeight = ((nRasterYSize >> (m_nZoomLevel-i)) + nTileHeight - 1) / nTileHeight;
        }
        else
        {
            dfPixelXSizeZoomLevel = dfPixelXSizeZoomLevel0 / (1 << i);
            dfPixelYSizeZoomLevel = dfPixelYSizeZoomLevel0 / (1 << i);
            nTileMatrixWidth = nTileXCountZoomLevel0 * (1 << i);
            nTileMatrixHeight = nTileYCountZoomLevel0 * (1 << i);
        }
        oStatement.Clear();
        oStatement.Appendf( "INSERT INTO gpkg.tile_matrix "
                            "(table_name,zoom_level,matrix_width,matrix_height, "
                            "tile_width,tile_height,pixel_x_size,pixel_y_size) "
                            "VALUES "
                            "('%s',%d,%d,%d,%d,%d,%.18g,%.18g)",
                            m_osRasterTable.c_str(),i,nTileMatrixWidth,
                            nTileMatrixHeight,
                            nTileWidth,nTileHeight,dfPixelXSizeZoomLevel,
                            dfPixelYSizeZoomLevel);

        if( !oStatement.DB2Execute("OGRDB2DataSource::FinalizeRasterRegistration") )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Insert into gpkg.tile_matrix_set failed");
            CPLDebug("OGRDB2DataSource::FinalizeRasterRegistration",
                     "Insert into gpkg.tile_matrix_set failed;"
                     "error: %s; ",
                     GetSession()->GetLastError());
            return CE_Failure;
        }

        if( i < m_nZoomLevel )
        {
            OGRDB2DataSource* poOvrDS = new OGRDB2DataSource();
            poOvrDS->InitRaster ( this, m_osRasterTable, i, nBands,
                                  m_dfTMSMinX, m_dfTMSMaxY,
                                  dfPixelXSizeZoomLevel, dfPixelYSizeZoomLevel,
                                  nTileWidth, nTileHeight,
                                  nTileMatrixWidth,nTileMatrixHeight,
                                  dfGDALMinX, dfGDALMinY,
                                  dfGDALMaxX, dfGDALMaxY );

            m_papoOverviewDS[m_nZoomLevel-1-i] = poOvrDS;
        }
    }

    SoftCommitTransaction();

    m_nOverviewCount = m_nZoomLevel;
    m_bRecordInsertedInGPKGContent = TRUE;
    return CE_None;
}

/************************************************************************/
/*                          IBuildOverviews()                           */
/************************************************************************/

static int GetFloorPowerOfTwo(int n)
{
    int p2 = 1;
    while( (n = n >> 1) > 0 )
    {
        p2 <<= 1;
    }
    return p2;
}

CPLErr OGRDB2DataSource::IBuildOverviews(
    const char * pszResampling,
    int nOverviews, int * panOverviewList,
    int nBandsIn, CPL_UNUSED int * panBandList,
    GDALProgressFunc pfnProgress, void * pProgressData )
{
    CPLDebug("OGRDB2DataSource::IBuildOverviews",
             "nOverviews: %d; m_nOverviewCount: %d",
             nOverviews, m_nOverviewCount);
    if( GetAccess() != GA_Update )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Overview building not supported on a database opened in read-only mode");
        return CE_Failure;
    }
    if( m_poParentDS != nullptr )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Overview building not supported on overview dataset");
        return CE_Failure;
    }
    OGRDB2Statement oStatement( &m_oSession );
    if( nOverviews == 0 )
    {
        for(int i=0; i<m_nOverviewCount; i++)
            m_papoOverviewDS[i]->FlushCache();
        oStatement.Appendf("DELETE FROM %s WHERE zoom_level < %d",
                           m_osRasterTable.c_str(),
                           m_nZoomLevel);
#ifdef DEBUG_SQL
        CPLDebug("OGRDB2DataSource::IBuildOverviews", "stmt: '%s'",oStatement.GetCommand());
#endif
        if( !oStatement.ExecuteSQL() )
        {
            CPLDebug("OGRDB2DataSource::IBuildOverviews", "DELETE failed: %s", GetSession()->GetLastError() );
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Delete of overviews failed: %s", GetSession()->GetLastError() );
            return CE_Failure;
        }

        return CE_None;
    }

    if( nBandsIn != nBands )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Generation of overviews in GPKG only"
                  "supported when operating on all bands." );
        return CE_Failure;
    }

    if( m_nOverviewCount == 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Image too small to support overviews");
        return CE_Failure;
    }

    FlushCache();
    for(int i=0; i<nOverviews; i++)
    {
        if( panOverviewList[i] < 2 )
        {
            CPLError(CE_Failure, CPLE_IllegalArg, "Overview factor must be >= 2");
            return CE_Failure;
        }

        int bFound = FALSE;
        int jCandidate = -1;
        int nMaxOvFactor = 0;
        for(int j=0; j<m_nOverviewCount; j++)
        {
            int    nOvFactor;

            GDALDataset* poODS = m_papoOverviewDS[j];

            nOvFactor = (int)
                        (0.5 + GetRasterXSize() / (double) poODS->GetRasterXSize());
            nMaxOvFactor = nOvFactor;

            if( nOvFactor == panOverviewList[i]
                    || nOvFactor == GDALOvLevelAdjust2( panOverviewList[i],
                            GetRasterXSize(),
                            GetRasterYSize() ) )
            {
                bFound = TRUE;
                break;
            }

            if( jCandidate < 0 && nOvFactor > panOverviewList[i] )
                jCandidate = j;
        }

        if( !bFound )
        {
            /* Mostly for debug */
            if( !CPLTestBool(CPLGetConfigOption("ALLOW_GPKG_ZOOM_OTHER_EXTENSION", "YES")) )
            {
                CPLString osOvrList;
                for(int j=0; j<m_nOverviewCount; j++)
                {
                    int    nOvFactor;

                    GDALDataset* poODS = m_papoOverviewDS[j];

                    /* Compute overview factor */
                    nOvFactor = (int)
                                (0.5 + GetRasterXSize() / (double) poODS->GetRasterXSize());
                    int nODSXSize = (int)(0.5 + GetRasterXSize() / (double) nOvFactor);
                    if( nODSXSize != poODS->GetRasterXSize() )
                    {
                        int nOvFactorPowerOfTwo = GetFloorPowerOfTwo(nOvFactor);
                        nODSXSize = (int)(0.5 + GetRasterXSize() / (double) nOvFactorPowerOfTwo);
                        if( nODSXSize == poODS->GetRasterXSize() )
                            nOvFactor = nOvFactorPowerOfTwo;
                        else
                        {
                            nOvFactorPowerOfTwo <<= 1;
                            nODSXSize = (int)(0.5 + GetRasterXSize() / (double) nOvFactorPowerOfTwo);
                            if( nODSXSize == poODS->GetRasterXSize() )
                                nOvFactor = nOvFactorPowerOfTwo;
                        }
                    }
                    if( j != 0 )
                        osOvrList += " ";
                    osOvrList += CPLSPrintf("%d", nOvFactor);
                }
                CPLError(CE_Failure, CPLE_NotSupported,
                         "Only overviews %s can be computed", osOvrList.c_str());
                return CE_Failure;
            }
            else
            {
                int nOvFactor = panOverviewList[i];
                if( jCandidate < 0 )
                    jCandidate = m_nOverviewCount;

                int nOvXSize = GetRasterXSize() / nOvFactor;
                int nOvYSize = GetRasterYSize() / nOvFactor;
                if( nOvXSize < 8 || nOvYSize < 8)
                {
                    CPLError(CE_Failure, CPLE_NotSupported,
                             "Too big overview factor : %d. Would result in a %dx%d overview",
                             nOvFactor, nOvXSize, nOvYSize);
                    return CE_Failure;
                }
                if( !(jCandidate == m_nOverviewCount && nOvFactor == 2 * nMaxOvFactor) &&
                        !m_bZoomOther )
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "Use of overview factor %d cause gpkg_zoom_other extension to be needed",
                             nOvFactor);
                    RegisterZoomOtherExtension();
                    m_bZoomOther = TRUE;
                }

                SoftStartTransaction();

                CPLAssert(jCandidate > 0);
                int nNewZoomLevel = m_papoOverviewDS[jCandidate-1]->m_nZoomLevel;

                for(int k=0; k<=jCandidate; k++)
                {
                    oStatement.Appendf( "UPDATE gpkg.tile_matrix SET zoom_level = %d "
                                        "WHERE table_name = %s AND zoom_level = %d",
                                        m_nZoomLevel - k + 1,
                                        m_osRasterTable.c_str(),
                                        m_nZoomLevel - k);
#ifdef DEBUG_SQL
                    CPLDebug("OGRDB2DataSource::IBuildOverviews",
                             "stmt: '%s'", oStatement.GetCommand());
#endif
                    if( !oStatement.ExecuteSQL() )
                    {
                        SoftRollbackTransaction();
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "updating tile_matrix failed "
                                 "for table %s: %s ",
                                 m_osRasterTable.c_str(),
                                 GetSession()->GetLastError());
                        CPLDebug("OGRDB2DataSource::IBuildOverviews",
                                 "updating tile_matrix failed "
                                 "for table %s: %s ",
                                 m_osRasterTable.c_str(),
                                 GetSession()->GetLastError());
                        return CE_Failure;
                    }
                    oStatement.Clear();

                    oStatement.Appendf( "UPDATE %s SET zoom_level = %d "
                                        "WHERE zoom_level = %d",
                                        m_osRasterTable.c_str(),
                                        m_nZoomLevel - k + 1,
                                        m_nZoomLevel - k);
#ifdef DEBUG_SQL
                    CPLDebug("OGRDB2DataSource::IBuildOverviews",
                             "stmt: '%s'", oStatement.GetCommand());
#endif
                    if( !oStatement.ExecuteSQL() )
                    {
                        SoftRollbackTransaction();
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "update failed "
                                 "for table %s: %s ",
                                 m_osRasterTable.c_str(),
                                 GetSession()->GetLastError());
                        CPLDebug("OGRDB2DataSource::IBuildOverviews",
                                 "update failed "
                                 "for table %s: %s ",
                                 m_osRasterTable.c_str(),
                                 GetSession()->GetLastError());
                        return CE_Failure;
                    }
                }

                double dfGDALMinX = m_adfGeoTransform[0];
                double dfGDALMinY = m_adfGeoTransform[3] + nRasterYSize * m_adfGeoTransform[5];
                double dfGDALMaxX = m_adfGeoTransform[0] + nRasterXSize * m_adfGeoTransform[1];
                double dfGDALMaxY = m_adfGeoTransform[3];
                double dfPixelXSizeZoomLevel = m_adfGeoTransform[1] * nOvFactor;
                double dfPixelYSizeZoomLevel = fabs(m_adfGeoTransform[5]) * nOvFactor;
                int nTileWidth, nTileHeight;
                GetRasterBand(1)->GetBlockSize(&nTileWidth, &nTileHeight);
                int nTileMatrixWidth = (nOvXSize + nTileWidth - 1) / nTileWidth;
                int nTileMatrixHeight = (nOvYSize + nTileHeight - 1) / nTileHeight;
                oStatement.Clear();

                oStatement.Appendf( "INSERT INTO gpkg.tile_matrix "
                                    "(table_name,zoom_level,matrix_width,matrix_height,tile_width,tile_height,pixel_x_size,pixel_y_size) VALUES "
                                    "(%s,%d,%d,%d,%d,%d,%.18g,%.18g)",
                                    m_osRasterTable.c_str(),nNewZoomLevel,nTileMatrixWidth,nTileMatrixHeight,
                                    nTileWidth,nTileHeight,dfPixelXSizeZoomLevel,dfPixelYSizeZoomLevel);
#ifdef DEBUG_SQL
                CPLDebug("OGRDB2DataSource::IBuildOverviews",
                         "stmt: '%s'", oStatement.GetCommand());
#endif
                if( !oStatement.ExecuteSQL() )
                {
                    SoftRollbackTransaction();
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "insert into tile_matrix failed "
                             "for table %s: %s ",
                             m_osRasterTable.c_str(),
                             GetSession()->GetLastError());
                    CPLDebug("OGRDB2DataSource::IBuildOverviews",
                             "insert into tile_matrix failed "
                             "for table %s: %s ",
                             m_osRasterTable.c_str(),
                             GetSession()->GetLastError());
                    return CE_Failure;
                }

                SoftCommitTransaction();

                m_nZoomLevel ++; /* this change our zoom level as well as previous overviews */
                for(int k=0; k<jCandidate; k++)
                    m_papoOverviewDS[k]->m_nZoomLevel ++;

                OGRDB2DataSource* poOvrDS = new OGRDB2DataSource();
                poOvrDS->InitRaster ( this, m_osRasterTable,
                                      nNewZoomLevel, nBands,
                                      m_dfTMSMinX, m_dfTMSMaxY,
                                      dfPixelXSizeZoomLevel, dfPixelYSizeZoomLevel,
                                      nTileWidth, nTileHeight,
                                      nTileMatrixWidth,nTileMatrixHeight,
                                      dfGDALMinX, dfGDALMinY,
                                      dfGDALMaxX, dfGDALMaxY );
                m_papoOverviewDS = (OGRDB2DataSource**) CPLRealloc(
                                       m_papoOverviewDS, sizeof(OGRDB2DataSource*) * (m_nOverviewCount+1));

                if( jCandidate < m_nOverviewCount )
                {
                    memmove(m_papoOverviewDS + jCandidate + 1,
                            m_papoOverviewDS + jCandidate,
                            sizeof(OGRDB2DataSource*) * (m_nOverviewCount-jCandidate));
                }
                m_papoOverviewDS[jCandidate] = poOvrDS;
                m_nOverviewCount ++;
            }
        }
    }

    GDALRasterBand*** papapoOverviewBands = (GDALRasterBand ***) CPLCalloc(sizeof(void*),nBands);
    for( int iBand = 0; iBand < nBands; iBand++ )
    {
        papapoOverviewBands[iBand] = (GDALRasterBand **) CPLCalloc(sizeof(void*),nOverviews);
        int iCurOverview = 0;
        for(int i=0; i<nOverviews; i++)
        {
            int   j;
            for( j = 0; j < m_nOverviewCount; j++ )
            {
                int    nOvFactor;
                GDALDataset* poODS = m_papoOverviewDS[j];

                nOvFactor = GDALComputeOvFactor(poODS->GetRasterXSize(),
                                                GetRasterXSize(),
                                                poODS->GetRasterYSize(),
                                                GetRasterYSize());

                if( nOvFactor == panOverviewList[i]
                        || nOvFactor == GDALOvLevelAdjust2( panOverviewList[i],
                                GetRasterXSize(),
                                GetRasterYSize() ) )
                {
                    papapoOverviewBands[iBand][iCurOverview] = poODS->GetRasterBand(iBand+1);
                    iCurOverview++ ;
                    break;
                }
            }
            CPLAssert(j < m_nOverviewCount);
        }
        CPLAssert(iCurOverview == nOverviews);
    }

    CPLErr eErr = GDALRegenerateOverviewsMultiBand(nBands, papoBands,
                  nOverviews, papapoOverviewBands,
                  pszResampling, pfnProgress, pProgressData );

    for( int iBand = 0; iBand < nBands; iBand++ )
    {
        CPLFree(papapoOverviewBands[iBand]);
    }
    CPLFree(papapoOverviewBands);

    return eErr;
}

/************************************************************************/
/*                       RegisterZoomOtherExtension()                   */
/************************************************************************/

int OGRDB2DataSource::RegisterZoomOtherExtension()
{
    CPLDebug("OGRDB2DataSource::RegisterZoomOtherExtension", "NO-OP");
    CreateExtensionsTableIfNecessary();
#ifdef LATER
    char* pszSQL = sqlite3_mprintf(
                       "INSERT INTO gpkg_extensions "
                       "(table_name, extension_name, definition, scope) "
                       "VALUES "
                       "('%q', 'gpkg_zoom_other', 'GeoPackage 1.0 Specification Annex O', 'read-write')",
                       m_osRasterTable.c_str());
    OGRErr eErr = SQLCommand(hDB, pszSQL);
    sqlite3_free(pszSQL);
    if ( OGRERR_NONE != eErr )
        return FALSE;
#endif
    return TRUE;
}

/************************************************************************/
/*                  CreateGDALAspatialExtension()                       */
/************************************************************************/

OGRErr OGRDB2DataSource::CreateGDALAspatialExtension()
{
    CreateExtensionsTableIfNecessary();
    CPLDebug("OGRDB2DataSource::CreateGDALAspatialExtension", "NO-OP");
#ifdef LATER
    if( HasGDALAspatialExtension() )
        return OGRERR_NONE;

    const char* pszCreateAspatialExtension =
        "INSERT INTO gpkg_extensions "
        "(table_name, column_name, extension_name, definition, scope) "
        "VALUES "
        "(NULL, NULL, 'gdal_aspatial', 'http://gdal.org/geopackage_aspatial.html', 'read-write')";

    return SQLCommand(hDB, pszCreateAspatialExtension);
#endif
    return OGRERR_NONE;
}
