/******************************************************************************
 *
 * Project:  GDAL MBTiles driver
 * Purpose:  Implement GDAL MBTiles support using OGR SQLite driver
 * Author:   Even Rouault, Even Rouault <even.rouault at spatialys.com>
 *
 **********************************************************************
 * Copyright (c) 2012-2016, Even Rouault <even.rouault at spatialys.com>
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
+ * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#if defined(HAVE_SQLITE) && defined(HAVE_GEOS)
// Needed by mvtutils.h
#define HAVE_MVT_WRITE_SUPPORT
#endif

#include "gdal_frmts.h"
#include "gdal_pam.h"
#include "ogr_api.h"
#include "cpl_json.h"
#include "cpl_vsil_curl_priv.h"
#include "gpkgmbtilescommon.h"
#include "gdal_utils.h"
#include "gdalwarper.h"
#include "mvtutils.h"
#include "ogrsqlitevfs.h"

#include "zlib.h"
#include "ogrgeojsonreader.h"

#include <math.h>
#include <algorithm>
#include <memory>
#include <vector>

CPL_CVSID("$Id$")

static const char * const apszAllowedDrivers[] = {"JPEG", "PNG", nullptr};

#define SRS_EPSG_3857 "PROJCS[\"WGS 84 / Pseudo-Mercator\",GEOGCS[\"WGS 84\",DATUM[\"WGS_1984\",SPHEROID[\"WGS 84\",6378137,298.257223563,AUTHORITY[\"EPSG\",\"7030\"]],AUTHORITY[\"EPSG\",\"6326\"]],PRIMEM[\"Greenwich\",0,AUTHORITY[\"EPSG\",\"8901\"]],UNIT[\"degree\",0.0174532925199433,AUTHORITY[\"EPSG\",\"9122\"]],AUTHORITY[\"EPSG\",\"4326\"]],PROJECTION[\"Mercator_1SP\"],PARAMETER[\"central_meridian\",0],PARAMETER[\"scale_factor\",1],PARAMETER[\"false_easting\",0],PARAMETER[\"false_northing\",0],UNIT[\"metre\",1,AUTHORITY[\"EPSG\",\"9001\"]],AXIS[\"X\",EAST],AXIS[\"Y\",NORTH],EXTENSION[\"PROJ4\",\"+proj=merc +a=6378137 +b=6378137 +lat_ts=0.0 +lon_0=0.0 +x_0=0.0 +y_0=0 +k=1.0 +units=m +nadgrids=@null +wktext  +no_defs\"],AUTHORITY[\"EPSG\",\"3857\"]]"

#define SPHERICAL_RADIUS        6378137.0
#define MAX_GM                  (SPHERICAL_RADIUS * M_PI)               // 20037508.342789244

// TileMatrixSet origin : caution this is in GeoPackage / WMTS convention ! That is upper-left corner
#define TMS_ORIGIN_X        -MAX_GM
#define TMS_ORIGIN_Y         MAX_GM

#if defined(DEBUG) || defined(FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION) || defined(ALLOW_FORMAT_DUMPS)
// Enable accepting a SQL dump (starting with a "-- SQL MBTILES" line) as a valid
// file. This makes fuzzer life easier
#define ENABLE_SQL_SQLITE_FORMAT
#endif

constexpr int knDEFAULT_BLOCK_SIZE = 256;

class MBTilesBand;

/************************************************************************/
/*                         MBTILESOpenSQLiteDB()                        */
/************************************************************************/

static OGRDataSourceH MBTILESOpenSQLiteDB(const char* pszFilename,
                                          GDALAccess eAccess)
{
    const char* l_apszAllowedDrivers[] = { "SQLITE", nullptr };
    return (OGRDataSourceH)GDALOpenEx( (CPLString("SQLITE:") + pszFilename).c_str(),
                                      GDAL_OF_VECTOR | GDAL_OF_INTERNAL |
                                      ((eAccess == GA_Update) ? GDAL_OF_UPDATE : 0),
                                      l_apszAllowedDrivers, nullptr, nullptr);
}

/************************************************************************/
/* ==================================================================== */
/*                              MBTilesDataset                          */
/* ==================================================================== */
/************************************************************************/

class MBTilesDataset final: public GDALPamDataset, public GDALGPKGMBTilesLikePseudoDataset
{
    friend class MBTilesBand;
    friend class MBTilesVectorLayer;

  public:
                 MBTilesDataset();

    virtual     ~MBTilesDataset();

    virtual CPLErr GetGeoTransform(double* padfGeoTransform) override;
    virtual CPLErr SetGeoTransform( double* padfGeoTransform ) override;
    virtual const char* _GetProjectionRef() override;
    virtual CPLErr _SetProjection( const char* pszProjection ) override;
    const OGRSpatialReference* GetSpatialRef() const override {
        return GetSpatialRefFromOldGetProjectionRef();
    }
    CPLErr SetSpatialRef(const OGRSpatialReference* poSRS) override {
        return OldSetProjectionFromSetSpatialRef(poSRS);
    }

    virtual char      **GetMetadataDomainList() override;
    virtual char      **GetMetadata( const char * pszDomain = "" ) override;
    virtual const char *GetMetadataItem( const char* pszName, const char * pszDomain = "" ) override;

    virtual CPLErr    IBuildOverviews(
                        const char * pszResampling,
                        int nOverviews, int * panOverviewList,
                        int nBandsIn, CPL_UNUSED int * panBandList,
                        GDALProgressFunc pfnProgress, void * pProgressData ) override;

    virtual int                 GetLayerCount() override
                        { return static_cast<int>(m_apoLayers.size()); }
    virtual OGRLayer*           GetLayer( int ) override;

    static GDALDataset *Open( GDALOpenInfo * );
    static int          Identify( GDALOpenInfo * );
    static GDALDataset* Create( const char * pszFilename,
                                   int nXSize,
                                   int nYSize,
                                   int nBandsIn,
                                   GDALDataType eDT,
                                   char **papszOptions );
    static GDALDataset* CreateCopy( const char *pszFilename,
                                                GDALDataset *poSrcDS,
                                                int bStrict,
                                                char ** papszOptions,
                                                GDALProgressFunc pfnProgress,
                                                void * pProgressData );

    char*               FindKey(int iPixel, int iLine);

    bool                HasNonEmptyGrids();

  private:

    bool m_bWriteBounds;
    CPLString m_osBounds;
    CPLString m_osCenter;
    bool m_bWriteMinMaxZoom;
    MBTilesDataset* poMainDS;
    bool m_bGeoTransformValid;
    double m_adfGeoTransform[6];
    int m_nMinZoomLevel = 0;

    int m_nOverviewCount;
    MBTilesDataset** m_papoOverviewDS;

    OGRDataSourceH hDS;
    sqlite3* hDB;

    sqlite3_vfs*        pMyVFS;

    bool bFetchedMetadata;
    CPLStringList aosList;

    int nHasNonEmptyGrids;

    bool m_bInFlushCache;

    CPLString m_osMetadataMemFilename;
    CPLString m_osClip;
    std::vector<std::unique_ptr<OGRLayer>>      m_apoLayers;

    void ParseCompressionOptions(char** papszOptions);
    CPLErr FinalizeRasterRegistration();
    void ComputeTileAndPixelShifts();
    bool InitRaster ( MBTilesDataset* poParentDS,
                     int nZoomLevel,
                     int nBandCount,
                     int nTileSize,
                     double dfGDALMinX,
                     double dfGDALMinY,
                     double dfGDALMaxX,
                     double dfGDALMaxY );

    bool CreateInternal( const char * pszFilename,
                         int nXSize,
                         int nYSize,
                         int nBandsIn,
                         GDALDataType eDT,
                         char **papszOptions );
    void InitVector(double dfMinX, double dfMinY, double dfMaxX, double dfMaxY,
                    bool bZoomLevelFromSpatialFilter,
                    bool bJsonField);

    protected:
        // Coming from GDALGPKGMBTilesLikePseudoDataset

        virtual CPLErr                  IFlushCacheWithErrCode(bool bAtClosing) override;
        virtual int                     IGetRasterCount() override { return nBands; }
        virtual GDALRasterBand*         IGetRasterBand(int nBand) override { return GetRasterBand(nBand); }
        virtual sqlite3                *IGetDB() override { return hDB; }
        virtual bool                    IGetUpdate() override { return eAccess == GA_Update; }
        virtual bool                    ICanIWriteBlock() override;
        virtual OGRErr                  IStartTransaction() override;
        virtual OGRErr                  ICommitTransaction() override;
        virtual const char             *IGetFilename() override { return GetDescription(); }
        virtual int                     GetRowFromIntoTopConvention(int nRow) override;
};

/************************************************************************/
/* ==================================================================== */
/*                          MBTilesVectorLayer                          */
/* ==================================================================== */
/************************************************************************/

class MBTilesVectorLayer final: public OGRLayer
{
        MBTilesDataset* m_poDS = nullptr;
        OGRFeatureDefn* m_poFeatureDefn = nullptr;
        OGRLayerH       m_hTileIteratorLyr = nullptr;
        bool            m_bEOF = false;
        CPLString       m_osTmpFilename;
        GDALDatasetH    m_hTileDS = nullptr;
        GIntBig         m_nFeatureCount = -1;
        int             m_nX = 0;
        int             m_nY = 0;
        OGREnvelope     m_sExtent;
        int             m_nFilterMinX = 0;
        int             m_nFilterMinY = 0;
        int             m_nFilterMaxX = 0;
        int             m_nFilterMaxY = 0;
        int             m_nZoomLevel = 0;
        bool            m_bZoomLevelAuto = false;
        bool            m_bJsonField = false;

        OGRFeature      *GetNextRawFeature();
        OGRFeature      *GetNextSrcFeature();
        OGRFeature      *CreateFeatureFrom(OGRFeature* poSrcFeature);

    public:
            MBTilesVectorLayer(MBTilesDataset* poDS,
                               const char* pszLayerName,
                               const CPLJSONObject& oFields,
                               bool bJsonField,
                               double dfMinX, double dfMinY,
                               double dfMaxX, double dfMaxY,
                               OGRwkbGeometryType eGeomType,
                               bool bZoomLevelFromSpatialFilter);
            ~MBTilesVectorLayer();

        virtual void ResetReading() override;
        virtual OGRFeature* GetNextFeature() override;
        virtual OGRFeatureDefn* GetLayerDefn() override
                            { return m_poFeatureDefn; }
        virtual GIntBig GetFeatureCount( int bForce ) override;
        virtual int TestCapability(const char*) override;

        OGRErr              GetExtent( OGREnvelope *psExtent,
                                       int bForce ) override;
        virtual OGRErr      GetExtent( int iGeomField, OGREnvelope *psExtent,
                                   int bForce ) override
                { return OGRLayer::GetExtent(iGeomField, psExtent, bForce); }

        virtual void        SetSpatialFilter( OGRGeometry * ) override;
        virtual void        SetSpatialFilter( int iGeomField,
                                            OGRGeometry *poGeom ) override
                    { OGRLayer::SetSpatialFilter(iGeomField, poGeom); }
        virtual OGRFeature* GetFeature(GIntBig nFID) override;
};

/************************************************************************/
/* ==================================================================== */
/*                              MBTilesBand                             */
/* ==================================================================== */
/************************************************************************/

class MBTilesBand final: public GDALGPKGMBTilesLikeRasterBand
{
    friend class MBTilesDataset;

    CPLString               osLocationInfo;

  public:
    explicit                MBTilesBand( MBTilesDataset* poDS, int nTileSize );

    virtual int             GetOverviewCount() override;
    virtual GDALRasterBand* GetOverview(int nLevel) override;

    virtual char      **GetMetadataDomainList() override;
    virtual const char *GetMetadataItem( const char * pszName,
                                         const char * pszDomain = "" ) override;
};

/************************************************************************/
/*                            MBTilesBand()                             */
/************************************************************************/

MBTilesBand::MBTilesBand(MBTilesDataset* poDSIn, int nTileSize) :
      GDALGPKGMBTilesLikeRasterBand(poDSIn, nTileSize, nTileSize)
{
}

/************************************************************************/
/*                           utf8decode()                               */
/************************************************************************/

static unsigned utf8decode(const char* p, const char* end, int* len)
{
  unsigned char c = *(unsigned char*)p;
  if (c < 0x80) {
    *len = 1;
    return c;
  } else if (c < 0xc2) {
    goto FAIL;
  }
  if (p+1 >= end || (p[1]&0xc0) != 0x80) goto FAIL;
  if (c < 0xe0) {
    *len = 2;
    return
      ((p[0] & 0x1f) << 6) +
      ((p[1] & 0x3f));
  } else if (c == 0xe0) {
    if (((unsigned char*)p)[1] < 0xa0) goto FAIL;
    goto UTF8_3;
  }
#if STRICT_RFC3629
  else if (c == 0xed) {
    // RFC 3629 says surrogate chars are illegal.
    if (((unsigned char*)p)[1] >= 0xa0) goto FAIL;
    goto UTF8_3;
  } else if (c == 0xef) {
    // 0xfffe and 0xffff are also illegal characters
    if (((unsigned char*)p)[1]==0xbf &&
    ((unsigned char*)p)[2]>=0xbe) goto FAIL;
    goto UTF8_3;
  }
#endif
  else if (c < 0xf0) {
  UTF8_3:
    if (p+2 >= end || (p[2]&0xc0) != 0x80) goto FAIL;
    *len = 3;
    return
      ((p[0] & 0x0f) << 12) +
      ((p[1] & 0x3f) << 6) +
      ((p[2] & 0x3f));
  } else if (c == 0xf0) {
    if (((unsigned char*)p)[1] < 0x90) goto FAIL;
    goto UTF8_4;
  } else if (c < 0xf4) {
  UTF8_4:
    if (p+3 >= end || (p[2]&0xc0) != 0x80 || (p[3]&0xc0) != 0x80) goto FAIL;
    *len = 4;
#if STRICT_RFC3629
    // RFC 3629 says all codes ending in fffe or ffff are illegal:
    if ((p[1]&0xf)==0xf &&
    ((unsigned char*)p)[2] == 0xbf &&
    ((unsigned char*)p)[3] >= 0xbe) goto FAIL;
#endif
    return
      ((p[0] & 0x07) << 18) +
      ((p[1] & 0x3f) << 12) +
      ((p[2] & 0x3f) << 6) +
      ((p[3] & 0x3f));
  } else if (c == 0xf4) {
    if (((unsigned char*)p)[1] > 0x8f) goto FAIL; // after 0x10ffff
    goto UTF8_4;
  } else {
  FAIL:
    *len = 1;
    return 0xfffd; // Unicode REPLACEMENT CHARACTER
  }
}

/************************************************************************/
/*                          HasNonEmptyGrids()                          */
/************************************************************************/

bool MBTilesDataset::HasNonEmptyGrids()
{
    OGRLayerH hSQLLyr;
    OGRFeatureH hFeat;

    if (poMainDS)
        return poMainDS->HasNonEmptyGrids();

    if (nHasNonEmptyGrids >= 0)
        return nHasNonEmptyGrids != FALSE;

    nHasNonEmptyGrids = false;

    if (OGR_DS_GetLayerByName(hDS, "grids") == nullptr)
        return false;

    const char* pszSQL = "SELECT type FROM sqlite_master WHERE name = 'grids'";
    CPLDebug("MBTILES", "%s", pszSQL);
    hSQLLyr = OGR_DS_ExecuteSQL(hDS, pszSQL, nullptr, nullptr);
    if (hSQLLyr == nullptr)
        return false;

    hFeat = OGR_L_GetNextFeature(hSQLLyr);
    if (hFeat == nullptr || !OGR_F_IsFieldSetAndNotNull(hFeat, 0))
    {
        OGR_F_Destroy(hFeat);
        OGR_DS_ReleaseResultSet(hDS, hSQLLyr);
        return false;
    }

    bool bGridsIsView = strcmp(OGR_F_GetFieldAsString(hFeat, 0), "view") == 0;

    OGR_F_Destroy(hFeat);
    OGR_DS_ReleaseResultSet(hDS, hSQLLyr);

    nHasNonEmptyGrids = TRUE;

    /* In the case 'grids' is a view (and a join between the 'map' and 'grid_utfgrid' layers */
    /* the cost of evaluating a join is very long, even if grid_utfgrid is empty */
    /* so check it is not empty */
    if (bGridsIsView)
    {
        OGRLayerH hGridUTFGridLyr;
        hGridUTFGridLyr = OGR_DS_GetLayerByName(hDS, "grid_utfgrid");
        if (hGridUTFGridLyr != nullptr)
        {
            OGR_L_ResetReading(hGridUTFGridLyr);
            hFeat = OGR_L_GetNextFeature(hGridUTFGridLyr);
            OGR_F_Destroy(hFeat);

            nHasNonEmptyGrids = hFeat != nullptr;
        }
    }

    return nHasNonEmptyGrids != FALSE;
}

/************************************************************************/
/*                             FindKey()                                */
/************************************************************************/

char* MBTilesDataset::FindKey(int iPixel, int iLine)
{
    int nBlockXSize;
    int nBlockYSize;
    GetRasterBand(1)->GetBlockSize(&nBlockXSize, &nBlockYSize);

    // Compute shift between GDAL origin and TileMatrixSet origin
    // Caution this is in GeoPackage / WMTS convention ! That is upper-left corner
    const int nShiftXPixels = (int)floor(0.5 + (m_adfGeoTransform[0] - TMS_ORIGIN_X) /  m_adfGeoTransform[1]);
    const int nShiftYPixelsFromGPKGOrigin = (int)floor(0.5 + (m_adfGeoTransform[3] - TMS_ORIGIN_Y) /  m_adfGeoTransform[5]);

    const int iLineFromGPKGOrigin = iLine + nShiftYPixelsFromGPKGOrigin;
    const int iLineFromMBTilesOrigin = m_nTileMatrixHeight * nBlockYSize - 1 - iLineFromGPKGOrigin;
    const int iPixelFromMBTilesOrigin = iPixel + nShiftXPixels;

    const int nTileColumn = iPixelFromMBTilesOrigin / nBlockXSize;
    const int nTileRow = iLineFromMBTilesOrigin / nBlockYSize;
    int nColInTile = iPixelFromMBTilesOrigin % nBlockXSize;
    int nRowInTile = nBlockYSize - 1 - (iLineFromMBTilesOrigin % nBlockYSize);

    char* pszKey = nullptr;

    OGRLayerH hSQLLyr;
    OGRFeatureH hFeat;
    json_object* poGrid = nullptr;
    int i;

    /* See https://github.com/mapbox/utfgrid-spec/blob/master/1.0/utfgrid.md */
    /* for the explanation of the following process */
    const char* pszSQL =
        CPLSPrintf("SELECT grid FROM grids WHERE "
                   "zoom_level = %d AND tile_column = %d AND tile_row = %d",
                   m_nZoomLevel, nTileColumn, nTileRow);
    CPLDebug("MBTILES", "%s", pszSQL);
    hSQLLyr = OGR_DS_ExecuteSQL(hDS, pszSQL, nullptr, nullptr);
    if (hSQLLyr == nullptr)
        return nullptr;

    hFeat = OGR_L_GetNextFeature(hSQLLyr);
    if (hFeat == nullptr || !OGR_F_IsFieldSetAndNotNull(hFeat, 0))
    {
        OGR_F_Destroy(hFeat);
        OGR_DS_ReleaseResultSet(hDS, hSQLLyr);
        return nullptr;
    }

    int nDataSize = 0;
    GByte* pabyData = OGR_F_GetFieldAsBinary(hFeat, 0, &nDataSize);

    int nUncompressedSize = nBlockXSize*nBlockYSize;
    GByte* pabyUncompressed = (GByte*)VSIMalloc(nUncompressedSize + 1);
    if( pabyUncompressed == nullptr )
    {
        OGR_F_Destroy(hFeat);
        OGR_DS_ReleaseResultSet(hDS, hSQLLyr);
        return nullptr;
    }

    z_stream sStream;
    memset(&sStream, 0, sizeof(sStream));
    if( inflateInit(&sStream) != Z_OK )
    {
        OGR_F_Destroy(hFeat);
        OGR_DS_ReleaseResultSet(hDS, hSQLLyr);
        CPLFree(pabyUncompressed);
        return nullptr;
    }
    sStream.next_in   = pabyData;
    sStream.avail_in  = nDataSize;
    sStream.next_out  = pabyUncompressed;
    sStream.avail_out = nUncompressedSize;
    int nStatus = inflate(&sStream, Z_FINISH);
    inflateEnd(&sStream);
    if (nStatus != Z_OK && nStatus != Z_STREAM_END)
    {
        CPLDebug("MBTILES", "Error unzipping grid");
        nUncompressedSize = 0;
        pabyUncompressed[nUncompressedSize] = 0;
    }
    else
    {
        nUncompressedSize -= sStream.avail_out;
        pabyUncompressed[nUncompressedSize] = 0;
        //CPLDebug("MBTILES", "Grid size = %d", nUncompressedSize);
        //CPLDebug("MBTILES", "Grid value = %s", (const char*)pabyUncompressed);
    }

    json_object* jsobj = nullptr;

    if (nUncompressedSize == 0)
    {
        goto end;
    }

    if( !OGRJSonParse((const char*)pabyUncompressed, &jsobj, true) )
    {
        goto end;
    }

    if (json_object_is_type(jsobj, json_type_object))
    {
        poGrid = CPL_json_object_object_get(jsobj, "grid");
    }
    if (poGrid != nullptr && json_object_is_type(poGrid, json_type_array))
    {
        int nFactor;
        json_object* poRow;
        char* pszRow = nullptr;

        const int nLines = static_cast<int>(json_object_array_length(poGrid));
        if (nLines == 0)
            goto end;

        nFactor = nBlockXSize / nLines;
        nRowInTile /= nFactor;
        nColInTile /= nFactor;

        poRow = json_object_array_get_idx(poGrid, nRowInTile);

        /* Extract line of interest in grid */
        if (poRow != nullptr && json_object_is_type(poRow, json_type_string))
        {
            pszRow = CPLStrdup(json_object_get_string(poRow));
        }

        if (pszRow == nullptr)
            goto end;

        /* Unapply JSON encoding */
        for (i = 0; pszRow[i] != '\0'; i++)
        {
            unsigned char c = ((GByte*)pszRow)[i];
            if (c >= 93) c--;
            if (c >= 35) c--;
            if (c < 32)
            {
                CPLDebug("MBTILES", "Invalid character at byte %d", i);
                break;
            }
            c -= 32;
            ((GByte*)pszRow)[i] = c;
        }

        if (pszRow[i] == '\0')
        {
            char* pszEnd = pszRow + i;

            int iCol = 0;
            i = 0;
            int nKey = -1;
            while(pszRow + i < pszEnd)
            {
                int len = 0;
                unsigned int res = utf8decode(pszRow + i, pszEnd, &len);

                /* Invalid UTF8 ? */
                if (res > 127 && len == 1)
                    break;

                if (iCol == nColInTile)
                {
                    nKey = (int)res;
                    //CPLDebug("MBTILES", "Key index = %d", nKey);
                    break;
                }
                i += len;
                iCol ++;
            }

            /* Find key */
            json_object* poKeys = CPL_json_object_object_get(jsobj, "keys");
            if (nKey >= 0 && poKeys != nullptr &&
                json_object_is_type(poKeys, json_type_array) &&
                nKey < static_cast<int>(json_object_array_length(poKeys)))
            {
                json_object* poKey = json_object_array_get_idx(poKeys, nKey);
                if (poKey != nullptr && json_object_is_type(poKey, json_type_string))
                {
                    pszKey = CPLStrdup(json_object_get_string(poKey));
                }
            }
        }

        CPLFree(pszRow);
    }

end:
    if (jsobj)
        json_object_put(jsobj);
    VSIFree(pabyUncompressed);
    if (hFeat)
        OGR_F_Destroy(hFeat);
    if (hSQLLyr)
        OGR_DS_ReleaseResultSet(hDS, hSQLLyr);

    return pszKey;
}

/************************************************************************/
/*                      GetMetadataDomainList()                         */
/************************************************************************/

char **MBTilesBand::GetMetadataDomainList()
{
    return CSLAddString(GDALPamRasterBand::GetMetadataDomainList(), "LocationInfo");
}

/************************************************************************/
/*                         GetMetadataItem()                            */
/************************************************************************/

const char *MBTilesBand::GetMetadataItem( const char * pszName,
                                          const char * pszDomain )
{
    MBTilesDataset* poGDS = (MBTilesDataset*) poDS;

/* ==================================================================== */
/*      LocationInfo handling.                                          */
/* ==================================================================== */
    if( pszDomain != nullptr
        && EQUAL(pszDomain,"LocationInfo")
        && (STARTS_WITH_CI(pszName, "Pixel_") || STARTS_WITH_CI(pszName, "GeoPixel_")) )
    {
        int iPixel, iLine;

        if (!poGDS->HasNonEmptyGrids())
            return nullptr;

/* -------------------------------------------------------------------- */
/*      What pixel are we aiming at?                                    */
/* -------------------------------------------------------------------- */
        if( STARTS_WITH_CI(pszName, "Pixel_") )
        {
            if( sscanf( pszName+6, "%d_%d", &iPixel, &iLine ) != 2 )
                return nullptr;
        }
        else if( STARTS_WITH_CI(pszName, "GeoPixel_") )
        {
            double adfGeoTransform[6];
            double adfInvGeoTransform[6];
            double dfGeoX, dfGeoY;

            dfGeoX = CPLAtof(pszName + 9);
            const char* pszUnderscore = strchr(pszName + 9, '_');
            if( !pszUnderscore )
                return nullptr;
            dfGeoY = CPLAtof(pszUnderscore + 1);

            if( GetDataset() == nullptr )
                return nullptr;

            if( GetDataset()->GetGeoTransform( adfGeoTransform ) != CE_None )
                return nullptr;

            if( !GDALInvGeoTransform( adfGeoTransform, adfInvGeoTransform ) )
                return nullptr;

            iPixel = (int) floor(
                adfInvGeoTransform[0]
                + adfInvGeoTransform[1] * dfGeoX
                + adfInvGeoTransform[2] * dfGeoY );
            iLine = (int) floor(
                adfInvGeoTransform[3]
                + adfInvGeoTransform[4] * dfGeoX
                + adfInvGeoTransform[5] * dfGeoY );
        }
        else
            return nullptr;

        if( iPixel < 0 || iLine < 0
            || iPixel >= GetXSize()
            || iLine >= GetYSize() )
            return nullptr;

        char* pszKey = poGDS->FindKey(iPixel, iLine);

        if (pszKey != nullptr)
        {
            //CPLDebug("MBTILES", "Key = %s", pszKey);

            osLocationInfo = "<LocationInfo>";
            osLocationInfo += "<Key>";
            char* pszXMLEscaped = CPLEscapeString(pszKey, -1, CPLES_XML_BUT_QUOTES);
            osLocationInfo += pszXMLEscaped;
            CPLFree(pszXMLEscaped);
            osLocationInfo += "</Key>";

            if (OGR_DS_GetLayerByName(poGDS->hDS, "grid_data") != nullptr &&
                strchr(pszKey, '\'') == nullptr)
            {
                OGRLayerH hSQLLyr;
                OGRFeatureH hFeat;

                const char* pszSQL =
                    CPLSPrintf("SELECT key_json FROM keymap WHERE "
                               "key_name = '%s'",
                               pszKey);
                CPLDebug("MBTILES", "%s", pszSQL);
                hSQLLyr = OGR_DS_ExecuteSQL(poGDS->hDS, pszSQL, nullptr, nullptr);
                if (hSQLLyr)
                {
                    hFeat = OGR_L_GetNextFeature(hSQLLyr);
                    if (hFeat != nullptr && OGR_F_IsFieldSetAndNotNull(hFeat, 0))
                    {
                        const char* pszJSon = OGR_F_GetFieldAsString(hFeat, 0);
                        //CPLDebug("MBTILES", "JSon = %s", pszJSon);

                        osLocationInfo += "<JSon>";
#ifdef CPLES_XML_BUT_QUOTES
                        pszXMLEscaped = CPLEscapeString(pszJSon, -1, CPLES_XML_BUT_QUOTES);
#else
                        pszXMLEscaped = CPLEscapeString(pszJSon, -1, CPLES_XML);
#endif
                        osLocationInfo += pszXMLEscaped;
                        CPLFree(pszXMLEscaped);
                        osLocationInfo += "</JSon>";
                    }
                    OGR_F_Destroy(hFeat);
                }
                OGR_DS_ReleaseResultSet(poGDS->hDS, hSQLLyr);
            }

            osLocationInfo += "</LocationInfo>";

            CPLFree(pszKey);

            return osLocationInfo.c_str();
        }

        return nullptr;
    }
    else
        return GDALPamRasterBand::GetMetadataItem(pszName, pszDomain);
}

/************************************************************************/
/*                         GetOverviewCount()                           */
/************************************************************************/

int MBTilesBand::GetOverviewCount()
{
    MBTilesDataset* poGDS = (MBTilesDataset*) poDS;

    if (poGDS->m_nOverviewCount >= 1)
        return poGDS->m_nOverviewCount;
    else
        return GDALPamRasterBand::GetOverviewCount();
}

/************************************************************************/
/*                              GetOverview()                           */
/************************************************************************/

GDALRasterBand* MBTilesBand::GetOverview(int nLevel)
{
    MBTilesDataset* poGDS = (MBTilesDataset*) poDS;

    if (poGDS->m_nOverviewCount == 0)
        return GDALPamRasterBand::GetOverview(nLevel);

    if (nLevel < 0 || nLevel >= poGDS->m_nOverviewCount)
        return nullptr;

    GDALDataset* poOvrDS = poGDS->m_papoOverviewDS[nLevel];
    if (poOvrDS)
        return poOvrDS->GetRasterBand(nBand);
    else
        return nullptr;
}

/************************************************************************/
/*                         MBTilesDataset()                          */
/************************************************************************/

MBTilesDataset::MBTilesDataset()
{
    m_bWriteBounds = true;
    m_bWriteMinMaxZoom = true;
    poMainDS = nullptr;
    m_nOverviewCount = 0;
    hDS = nullptr;
    m_papoOverviewDS = nullptr;
    bFetchedMetadata = false;
    nHasNonEmptyGrids = -1;
    hDB = nullptr;
    pMyVFS = nullptr;

    m_bGeoTransformValid = false;
    m_adfGeoTransform[0] = 0.0;
    m_adfGeoTransform[1] = 1.0;
    m_adfGeoTransform[2] = 0.0;
    m_adfGeoTransform[3] = 0.0;
    m_adfGeoTransform[4] = 0.0;
    m_adfGeoTransform[5] = 1.0;
    m_bInFlushCache = false;

    m_osRasterTable = "tiles";
    m_eTF = GPKG_TF_PNG;
}

/************************************************************************/
/*                        ~MBTilesDataset()                             */
/************************************************************************/

MBTilesDataset::~MBTilesDataset()
{
    // Need to explicitly clear it before close hDS
    m_apoLayers.clear();

    FlushCache(true);

    if (poMainDS == nullptr)
    {
        if (m_papoOverviewDS)
        {
            for(int i=0;i<m_nOverviewCount;i++)
                delete m_papoOverviewDS[i];
            CPLFree(m_papoOverviewDS);
        }

        if (hDS != nullptr)
        {
            OGRReleaseDataSource(hDS);
            hDB = nullptr;
        }
        if( hDB != nullptr )
        {
            sqlite3_close(hDB);

            if (pMyVFS)
            {
                sqlite3_vfs_unregister(pMyVFS);
                CPLFree(pMyVFS->pAppData);
                CPLFree(pMyVFS);
            }
        }
    }

    if( !m_osMetadataMemFilename.empty() )
    {
        VSIUnlink(m_osMetadataMemFilename);
    }
}

/************************************************************************/
/*                         IStartTransaction()                          */
/************************************************************************/

OGRErr MBTilesDataset::IStartTransaction()
{
    char *pszErrMsg = nullptr;
    const int rc = sqlite3_exec( hDB, "BEGIN", nullptr, nullptr, &pszErrMsg );
    if( rc != SQLITE_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "%s transaction failed: %s",
                  "BEGIN", pszErrMsg );
        sqlite3_free( pszErrMsg );
        return OGRERR_FAILURE;
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                         ICommitTransaction()                         */
/************************************************************************/

OGRErr MBTilesDataset::ICommitTransaction()
{
    char *pszErrMsg = nullptr;
    const int rc = sqlite3_exec( hDB, "COMMIT", nullptr, nullptr, &pszErrMsg );
    if( rc != SQLITE_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "%s transaction failed: %s",
                  "COMMIT", pszErrMsg );
        sqlite3_free( pszErrMsg );
        return OGRERR_FAILURE;
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                         ICanIWriteBlock()                            */
/************************************************************************/

bool MBTilesDataset::ICanIWriteBlock()
{
    if( eAccess != GA_Update )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "IWriteBlock() not supported on dataset opened in read-only mode");
        return false;
    }

    if( !m_bGeoTransformValid )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "IWriteBlock() not supported if georeferencing not set");
        return false;
    }
    return true;
}

/************************************************************************/
/*                         IFlushCacheWithErrCode()                     */
/************************************************************************/

CPLErr MBTilesDataset::IFlushCacheWithErrCode(bool bAtClosing)

{
    if( m_bInFlushCache )
        return CE_None;
    m_bInFlushCache = true;
    // Short circuit GDALPamDataset to avoid serialization to .aux.xml
    GDALDataset::FlushCache(bAtClosing);

    CPLErr eErr = FlushTiles();

    m_bInFlushCache = false;
    return eErr;
}

/************************************************************************/
/*                         ICanIWriteBlock()                            */
/************************************************************************/

int MBTilesDataset::GetRowFromIntoTopConvention(int nRow)
{
    return m_nTileMatrixHeight - 1 - nRow;
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr MBTilesDataset::GetGeoTransform(double* padfGeoTransform)
{
    memcpy(padfGeoTransform, m_adfGeoTransform, 6 * sizeof(double));
    return ( m_bGeoTransformValid ) ? CE_None : CE_Failure;
}

/************************************************************************/
/*                     SphericalMercatorToLongLat()                     */
/************************************************************************/

static void SphericalMercatorToLongLat(double* x, double* y)
{
  double lng = *x / SPHERICAL_RADIUS / M_PI * 180;
  double lat = 2 * (atan(exp(*y / SPHERICAL_RADIUS)) - M_PI / 4) / M_PI * 180;
  *x = lng;
  *y = lat;
}

/************************************************************************/
/*                     LongLatToSphericalMercator()                     */
/************************************************************************/

static void LongLatToSphericalMercator(double* x, double* y)
{
  double X = SPHERICAL_RADIUS * (*x) / 180 * M_PI;
  double Y = SPHERICAL_RADIUS * log( tan(M_PI / 4 + 0.5 * (*y) / 180 * M_PI) );
  *x = X;
  *y = Y;
}

/************************************************************************/
/*                          SetGeoTransform()                           */
/************************************************************************/

CPLErr MBTilesDataset::SetGeoTransform( double* padfGeoTransform )
{
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

    if( m_bWriteBounds )
    {
        CPLString osBounds(m_osBounds);
        if( osBounds.empty() )
        {
            double minx = padfGeoTransform[0];
            double miny = padfGeoTransform[3] + nRasterYSize * padfGeoTransform[5];
            double maxx = padfGeoTransform[0] + nRasterXSize * padfGeoTransform[1];
            double maxy = padfGeoTransform[3];

            SphericalMercatorToLongLat(&minx, &miny);
            SphericalMercatorToLongLat(&maxx, &maxy);
            if( fabs(minx + 180) < 1e-7 )
            {
                minx = -180.0;
            }
            if(  fabs(maxx - 180) < 1e-7 )
            {
                maxx = 180.0;
            }

            // Clamp latitude so that when transformed back to EPSG:3857, we don't
            // have too big northings
            double tmpx = 0.0;
            double ok_maxy = MAX_GM;
            SphericalMercatorToLongLat(&tmpx, &ok_maxy);
            if( maxy > ok_maxy)
                maxy = ok_maxy;
            if( miny < -ok_maxy)
                miny = -ok_maxy;

            osBounds.Printf("%.18g,%.18g,%.18g,%.18g",minx, miny, maxx, maxy );
        }

        char* pszSQL = sqlite3_mprintf(
            "INSERT INTO metadata (name, value) VALUES ('bounds', '%q')",
            osBounds.c_str() );
        sqlite3_exec( hDB, pszSQL, nullptr, nullptr, nullptr );
        sqlite3_free(pszSQL);

        if( !m_osCenter.empty() )
        {
            pszSQL = sqlite3_mprintf(
                "INSERT INTO metadata (name, value) VALUES ('center', '%q')",
                m_osCenter.c_str() );
            sqlite3_exec( hDB, pszSQL, nullptr, nullptr, nullptr );
            sqlite3_free(pszSQL);
        }
    }

    int nBlockXSize;
    int nBlockYSize;
    GetRasterBand(1)->GetBlockSize(&nBlockXSize, &nBlockYSize);
    const double dfPixelXSizeZoomLevel0 = 2 * MAX_GM / nBlockXSize;
    const double dfPixelYSizeZoomLevel0 = 2 * MAX_GM / nBlockYSize;
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
                  "Could not find an appropriate zoom level that matches raster pixel size");
        return CE_Failure;
    }

    memcpy(m_adfGeoTransform, padfGeoTransform, 6 * sizeof(double));
    m_bGeoTransformValid = true;

    return FinalizeRasterRegistration();
}

/************************************************************************/
/*                      ComputeTileAndPixelShifts()                     */
/************************************************************************/

void MBTilesDataset::ComputeTileAndPixelShifts()
{
    int nTileWidth, nTileHeight;
    GetRasterBand(1)->GetBlockSize(&nTileWidth, &nTileHeight);

    // Compute shift between GDAL origin and TileMatrixSet origin
    // Caution this is in GeoPackage / WMTS convention ! That is upper-left corner
    int nShiftXPixels = (int)floor(0.5 + (m_adfGeoTransform[0] - TMS_ORIGIN_X) /  m_adfGeoTransform[1]);
    m_nShiftXTiles = (int)floor(1.0 * nShiftXPixels / nTileWidth);
    m_nShiftXPixelsMod = ((nShiftXPixels % nTileWidth) + nTileWidth) % nTileWidth;
    int nShiftYPixels = (int)floor(0.5 + (m_adfGeoTransform[3] - TMS_ORIGIN_Y) /  m_adfGeoTransform[5]);
    m_nShiftYTiles = (int)floor(1.0 * nShiftYPixels / nTileHeight);
    m_nShiftYPixelsMod = ((nShiftYPixels % nTileHeight) + nTileHeight) % nTileHeight;
}

/************************************************************************/
/*                      FinalizeRasterRegistration()                    */
/************************************************************************/

CPLErr MBTilesDataset::FinalizeRasterRegistration()
{
    m_nTileMatrixWidth = (1 << m_nZoomLevel);
    m_nTileMatrixHeight = (1 << m_nZoomLevel);

    ComputeTileAndPixelShifts();

    double dfGDALMinX = m_adfGeoTransform[0];
    double dfGDALMinY = m_adfGeoTransform[3] + nRasterYSize * m_adfGeoTransform[5];
    double dfGDALMaxX = m_adfGeoTransform[0] + nRasterXSize * m_adfGeoTransform[1];
    double dfGDALMaxY = m_adfGeoTransform[3];

    m_nOverviewCount = m_nZoomLevel;
    m_papoOverviewDS = (MBTilesDataset**) CPLCalloc(sizeof(MBTilesDataset*),
                                                           m_nOverviewCount);

    if( m_bWriteMinMaxZoom )
    {
        char* pszSQL = sqlite3_mprintf(
            "INSERT INTO metadata (name, value) VALUES ('minzoom', '%d')", m_nZoomLevel );
        sqlite3_exec( hDB, pszSQL, nullptr, nullptr, nullptr );
        sqlite3_free(pszSQL);
        pszSQL = sqlite3_mprintf(
            "INSERT INTO metadata (name, value) VALUES ('maxzoom', '%d')", m_nZoomLevel );
        sqlite3_exec( hDB, pszSQL, nullptr, nullptr, nullptr );
        sqlite3_free(pszSQL);
    }

    for(int i=0; i<m_nOverviewCount; i++)
    {
        MBTilesDataset* poOvrDS = new MBTilesDataset();
        poOvrDS->ShareLockWithParentDataset(this);
        int nBlockSize;
        GetRasterBand(1)->GetBlockSize(&nBlockSize, &nBlockSize);
        poOvrDS->InitRaster ( this, i, nBands,
                              nBlockSize,
                              dfGDALMinX, dfGDALMinY,
                              dfGDALMaxX, dfGDALMaxY );

        m_papoOverviewDS[m_nZoomLevel-1-i] = poOvrDS;
    }

    return CE_None;
}

/************************************************************************/
/*                         InitRaster()                                 */
/************************************************************************/

bool MBTilesDataset::InitRaster ( MBTilesDataset* poParentDS,
                                 int nZoomLevel,
                                 int nBandCount,
                                 int nTileSize,
                                 double dfGDALMinX,
                                 double dfGDALMinY,
                                 double dfGDALMaxX,
                                 double dfGDALMaxY )
{
    m_nZoomLevel = nZoomLevel;
    m_nTileMatrixWidth = 1 << nZoomLevel;
    m_nTileMatrixHeight = 1 << nZoomLevel;

    const int nTileWidth = nTileSize;
    const int nTileHeight = nTileSize;
    const double dfPixelXSize = 2 * MAX_GM / nTileWidth / (1 << nZoomLevel);
    const double dfPixelYSize = 2 * MAX_GM / nTileHeight / (1 << nZoomLevel);

    m_bGeoTransformValid = true;
    m_adfGeoTransform[0] = dfGDALMinX;
    m_adfGeoTransform[1] = dfPixelXSize;
    m_adfGeoTransform[3] = dfGDALMaxY;
    m_adfGeoTransform[5] = -dfPixelYSize;
    double dfRasterXSize = 0.5 + (dfGDALMaxX - dfGDALMinX) / dfPixelXSize;
    double dfRasterYSize = 0.5 + (dfGDALMaxY - dfGDALMinY) / dfPixelYSize;
    if( dfRasterXSize > INT_MAX || dfRasterYSize > INT_MAX )
        return false;
    nRasterXSize = (int)dfRasterXSize;
    nRasterYSize = (int)dfRasterYSize;

    m_pabyCachedTiles = (GByte*) VSI_MALLOC3_VERBOSE(4 * 4, nTileWidth, nTileHeight);
    if( m_pabyCachedTiles == nullptr )
    {
        return false;
    }

    if( poParentDS )
    {
        eAccess = poParentDS->eAccess;
    }

    for(int i = 1; i <= nBandCount; i ++)
        SetBand( i, new MBTilesBand(this, nTileSize) );

    ComputeTileAndPixelShifts();

    GDALDataset::SetMetadataItem("INTERLEAVE", "PIXEL", "IMAGE_STRUCTURE");
    GDALDataset::SetMetadataItem("ZOOM_LEVEL", CPLSPrintf("%d", m_nZoomLevel));

    if( poParentDS )
    {
        m_poParentDS = poParentDS;
        poMainDS = poParentDS;
        hDS = poParentDS->hDS;
        hDB = poParentDS->hDB;
        m_eTF = poParentDS->m_eTF;
        m_nQuality = poParentDS->m_nQuality;
        m_nZLevel = poParentDS->m_nZLevel;
        m_bDither = poParentDS->m_bDither;
        m_osWHERE = poParentDS->m_osWHERE;
        SetDescription(CPLSPrintf("%s - zoom_level=%d",
                                  poParentDS->GetDescription(), m_nZoomLevel));
    }

    return true;
}

/************************************************************************/
/*                         GetProjectionRef()                           */
/************************************************************************/

const char* MBTilesDataset::_GetProjectionRef()
{
    return SRS_EPSG_3857;
}

/************************************************************************/
/*                           SetProjection()                            */
/************************************************************************/

CPLErr MBTilesDataset::_SetProjection( const char* pszProjection )
{
    if( eAccess != GA_Update )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "SetProjection() not supported on read-only dataset");
        return CE_Failure;
    }

    OGRSpatialReference oSRS;
    if( oSRS.SetFromUserInput(pszProjection) != OGRERR_NONE )
        return CE_Failure;
    if( oSRS.GetAuthorityName(nullptr) == nullptr ||
        !EQUAL(oSRS.GetAuthorityName(nullptr), "EPSG") ||
        oSRS.GetAuthorityCode(nullptr) == nullptr ||
        !EQUAL(oSRS.GetAuthorityCode(nullptr), "3857") )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Only EPSG:3857 supported on MBTiles dataset");
        return CE_Failure;
    }
    return CE_None;
}

/************************************************************************/
/*                      GetMetadataDomainList()                         */
/************************************************************************/

char **MBTilesDataset::GetMetadataDomainList()
{
    return BuildMetadataDomainList(GDALDataset::GetMetadataDomainList(),
                                   TRUE,
                                   "", nullptr);
}

/************************************************************************/
/*                            GetMetadata()                             */
/************************************************************************/

char** MBTilesDataset::GetMetadata( const char * pszDomain )
{
    if (pszDomain != nullptr && !EQUAL(pszDomain, ""))
        return GDALPamDataset::GetMetadata(pszDomain);

    if (bFetchedMetadata)
        return aosList.List();

    bFetchedMetadata = true;
    aosList = CPLStringList(GDALPamDataset::GetMetadata(), FALSE);

    OGRLayerH hSQLLyr = OGR_DS_ExecuteSQL(hDS,
            "SELECT name, value FROM metadata WHERE name != 'json' LIMIT 1000", nullptr, nullptr);
    if (hSQLLyr == nullptr)
        return nullptr;

    if (OGR_FD_GetFieldCount(OGR_L_GetLayerDefn(hSQLLyr)) != 2)
    {
        OGR_DS_ReleaseResultSet(hDS, hSQLLyr);
        return nullptr;
    }

    OGRFeatureH hFeat;
    while( (hFeat = OGR_L_GetNextFeature(hSQLLyr)) != nullptr )
    {
        if (OGR_F_IsFieldSetAndNotNull(hFeat, 0) && OGR_F_IsFieldSetAndNotNull(hFeat, 1))
        {
            CPLString osName = OGR_F_GetFieldAsString(hFeat, 0);
            CPLString osValue = OGR_F_GetFieldAsString(hFeat, 1);
            if (osName[0] != '\0' &&
                !STARTS_WITH(osValue, "function(") &&
                strstr(osValue, "<img ") == nullptr &&
                strstr(osValue, "<p>") == nullptr &&
                strstr(osValue, "</p>") == nullptr &&
                strstr(osValue, "<div") == nullptr)
            {
                aosList.AddNameValue(osName, osValue);
            }
        }
        OGR_F_Destroy(hFeat);
    }
    OGR_DS_ReleaseResultSet(hDS, hSQLLyr);

    return aosList.List();
}

/************************************************************************/
/*                         GetMetadataItem()                            */
/************************************************************************/

const char *MBTilesDataset::GetMetadataItem( const char* pszName, const char * pszDomain )
{
    if( pszDomain == nullptr || EQUAL(pszDomain, "") )
    {
        const char* pszValue = CSLFetchNameValue( GetMetadata(), pszName );
        if( pszValue )
            return pszValue;
    }
    return GDALPamDataset::GetMetadataItem(pszName, pszDomain);
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *MBTilesDataset::GetLayer( int iLayer )

{
    if( iLayer < 0 || iLayer >= GetLayerCount() )
        return nullptr;
    return m_apoLayers[iLayer].get();
}

/************************************************************************/
/*                        MBTilesVectorLayer()                          */
/************************************************************************/

MBTilesVectorLayer::MBTilesVectorLayer(MBTilesDataset* poDS,
                                       const char* pszLayerName,
                                       const CPLJSONObject& oFields,
                                       bool bJsonField,
                                       double dfMinX, double dfMinY,
                                       double dfMaxX, double dfMaxY,
                                       OGRwkbGeometryType eGeomType,
                                       bool bZoomLevelFromSpatialFilter):
    m_poDS(poDS),
    m_poFeatureDefn( new OGRFeatureDefn(pszLayerName) ),
    m_bJsonField(bJsonField)
{
    SetDescription(pszLayerName);
    m_poFeatureDefn->SetGeomType(eGeomType);
    OGRSpatialReference* poSRS = new OGRSpatialReference();
    poSRS->SetFromUserInput(SRS_EPSG_3857);
    m_poFeatureDefn->GetGeomFieldDefn(0)->SetSpatialRef(poSRS);
    poSRS->Release();
    m_poFeatureDefn->Reference();

    if( m_bJsonField )
    {
        OGRFieldDefn oFieldDefnId("mvt_id", OFTInteger64);
        m_poFeatureDefn->AddFieldDefn(&oFieldDefnId);
    }
    else
    {
        OGRMVTInitFields(m_poFeatureDefn, oFields);
    }

    m_sExtent.MinX = dfMinX;
    m_sExtent.MinY = dfMinY;
    m_sExtent.MaxX = dfMaxX;
    m_sExtent.MaxY = dfMaxY;

    m_nZoomLevel = m_poDS->m_nZoomLevel;
    m_bZoomLevelAuto = bZoomLevelFromSpatialFilter;
    MBTilesVectorLayer::SetSpatialFilter(nullptr);

    // If the metadata contains an empty fields object, this may be a sign
    // that it doesn't know the schema. In that case check if a tile has
    // attributes, and in that case create a json field.
    if( !m_bJsonField && oFields.IsValid() && oFields.GetChildren().empty() )
    {
        m_bJsonField = true;
        OGRFeature* poSrcFeature = GetNextSrcFeature();
        m_bJsonField = false;

        if( poSrcFeature )
        {
            // There is at least the mvt_id field
            if( poSrcFeature->GetFieldCount() > 1 )
            {
                m_bJsonField = true;
            }
            delete poSrcFeature;
        }
        MBTilesVectorLayer::ResetReading();
    }

    if( m_bJsonField )
    {
        OGRFieldDefn oFieldDefn("json", OFTString);
        m_poFeatureDefn->AddFieldDefn(&oFieldDefn);
    }
}

/************************************************************************/
/*                       ~MBTilesVectorLayer()                          */
/************************************************************************/

MBTilesVectorLayer::~MBTilesVectorLayer()
{
    m_poFeatureDefn->Release();
    if( m_hTileIteratorLyr )
        OGR_DS_ReleaseResultSet(m_poDS->hDS, m_hTileIteratorLyr);
    if( !m_osTmpFilename.empty() )
    {
        VSIUnlink(m_osTmpFilename);
    }
    if( m_hTileDS )
        GDALClose( m_hTileDS );
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int MBTilesVectorLayer::TestCapability(const char* pszCap)
{
    if( EQUAL(pszCap, OLCStringsAsUTF8) ||
        EQUAL(pszCap, OLCFastSpatialFilter) ||
        EQUAL(pszCap, OLCFastGetExtent))
    {
        return TRUE;
    }
    return FALSE;
}

/************************************************************************/
/*                             GetExtent()                              */
/************************************************************************/

OGRErr MBTilesVectorLayer::GetExtent( OGREnvelope *psExtent, int )
{
    *psExtent = m_sExtent;
    return OGRERR_NONE;
}

/************************************************************************/
/*                          ResetReading()                              */
/************************************************************************/

void MBTilesVectorLayer::ResetReading()
{
    if( m_hTileDS )
        GDALClose( m_hTileDS );
    m_hTileDS = nullptr;
    m_bEOF = false;
    if( m_hTileIteratorLyr )
        OGR_DS_ReleaseResultSet(m_poDS->hDS, m_hTileIteratorLyr);
    CPLString osSQL;
    osSQL.Printf("SELECT tile_column, tile_row, tile_data FROM tiles "
                 "WHERE zoom_level = %d "
                 "AND tile_column BETWEEN %d AND %d "
                 "AND tile_row BETWEEN %d AND %d",
                 m_nZoomLevel,
                 m_nFilterMinX, m_nFilterMaxX,
                 m_nFilterMinY, m_nFilterMaxY);
    m_hTileIteratorLyr = OGR_DS_ExecuteSQL(m_poDS->hDS,
        osSQL.c_str(), nullptr, nullptr);
}

/************************************************************************/
/*                         SetSpatialFilter()                           */
/************************************************************************/

void MBTilesVectorLayer::SetSpatialFilter( OGRGeometry * poGeomIn )
{
    OGRLayer::SetSpatialFilter(poGeomIn);

    if( m_poFilterGeom != nullptr &&
             m_sFilterEnvelope.MinX <= -MAX_GM &&
             m_sFilterEnvelope.MinY <= -MAX_GM &&
             m_sFilterEnvelope.MaxX >= MAX_GM &&
             m_sFilterEnvelope.MaxY >= MAX_GM )
    {
        if( m_bZoomLevelAuto )
        {
            m_nZoomLevel = m_poDS->m_nMinZoomLevel;
        }
        m_nFilterMinX = 0;
        m_nFilterMinY = 0;
        m_nFilterMaxX = (1 << m_nZoomLevel) - 1;
        m_nFilterMaxY = (1 << m_nZoomLevel) - 1;
    }
    else if( m_poFilterGeom != nullptr &&
             m_sFilterEnvelope.MinX >= -10 * MAX_GM &&
             m_sFilterEnvelope.MinY >= -10 * MAX_GM &&
             m_sFilterEnvelope.MaxX <= 10 * MAX_GM &&
             m_sFilterEnvelope.MaxY <= 10 * MAX_GM )
    {
        if( m_bZoomLevelAuto )
        {
            double dfExtent =
                std::min(m_sFilterEnvelope.MaxX - m_sFilterEnvelope.MinX,
                         m_sFilterEnvelope.MaxY - m_sFilterEnvelope.MinY);
            m_nZoomLevel = std::max(m_poDS->m_nMinZoomLevel,
                std::min(
                    static_cast<int>(
                        0.5 + log(2 * MAX_GM / dfExtent) / log(2.0)),
                    m_poDS->m_nZoomLevel));
            CPLDebug("MBTILES", "Zoom level = %d", m_nZoomLevel);
        }
        const double dfTileDim = 2 * MAX_GM / (1 << m_nZoomLevel);
        m_nFilterMinX = std::max(0, static_cast<int>(
            floor((m_sFilterEnvelope.MinX + MAX_GM) / dfTileDim)));
        m_nFilterMinY = std::max(0, static_cast<int>(
            floor((m_sFilterEnvelope.MinY + MAX_GM) / dfTileDim)));
        m_nFilterMaxX = std::min(static_cast<int>(
            ceil((m_sFilterEnvelope.MaxX + MAX_GM) / dfTileDim)),
            (1 << m_nZoomLevel) - 1);
        m_nFilterMaxY = std::min(static_cast<int>(
            ceil((m_sFilterEnvelope.MaxY + MAX_GM) / dfTileDim)),
            (1 << m_nZoomLevel) - 1);
    }
    else
    {
        if( m_bZoomLevelAuto )
        {
            m_nZoomLevel = m_poDS->m_nZoomLevel;
        }
        m_nFilterMinX = 0;
        m_nFilterMinY = 0;
        m_nFilterMaxX = (1 << m_nZoomLevel) - 1;
        m_nFilterMaxY = (1 << m_nZoomLevel) - 1;
    }
}

/************************************************************************/
/*                          GetNextFeature()                            */
/************************************************************************/

OGRFeature* MBTilesVectorLayer::GetNextFeature()
{
    while( true )
    {
        OGRFeature *poFeature = GetNextRawFeature();
        if (poFeature == nullptr)
            return nullptr;

        if((m_poFilterGeom == nullptr
            || FilterGeometry( poFeature->GetGeometryRef() ) )
        && (m_poAttrQuery == nullptr
            || m_poAttrQuery->Evaluate( poFeature )) )
        {
            return poFeature;
        }

        delete poFeature;
    }
}

/************************************************************************/
/*                         GetFeatureCount()                            */
/************************************************************************/

GIntBig MBTilesVectorLayer::GetFeatureCount( int bForce )
{
    if( m_poFilterGeom == nullptr && m_poAttrQuery == nullptr )
    {
        if( m_nFeatureCount < 0 )
        {
            m_nFeatureCount = 0;
            ResetReading();
            while( m_hTileIteratorLyr != nullptr )
            {
                OGRFeatureH hFeat = OGR_L_GetNextFeature(m_hTileIteratorLyr);
                if( hFeat == nullptr )
                {
                    break;
                }
                m_nX = OGR_F_GetFieldAsInteger(hFeat, 0);
                // MBTiles y origin is bottom based, whereas MVT directory
                // is top based
                m_nY = (1 << m_nZoomLevel) - 1 -
                            OGR_F_GetFieldAsInteger(hFeat, 1);
                int nDataSize = 0;
                GByte* pabyData = OGR_F_GetFieldAsBinary(hFeat, 2, &nDataSize);
                GByte* pabyDataDup = static_cast<GByte*>(CPLMalloc(nDataSize));
                memcpy(pabyDataDup, pabyData, nDataSize);
                OGR_F_Destroy(hFeat);

                if( !m_osTmpFilename.empty() )
                {
                    VSIUnlink(m_osTmpFilename);
                }
                m_osTmpFilename = CPLSPrintf("/vsimem/mvt_%p_%d_%d.pbf",
                                            this, m_nX, m_nY);
                VSIFCloseL(VSIFileFromMemBuffer(m_osTmpFilename,
                                                pabyDataDup, nDataSize,
                                                true));

                const char* l_apszAllowedDrivers[] = { "MVT", nullptr };
                if( m_hTileDS )
                    GDALClose( m_hTileDS );
                char** papszOpenOptions = nullptr;
                papszOpenOptions = CSLSetNameValue(papszOpenOptions,
                    "METADATA_FILE", m_poDS->m_osMetadataMemFilename.c_str());
                m_hTileDS = GDALOpenEx( ("MVT:" + m_osTmpFilename).c_str(),
                                    GDAL_OF_VECTOR | GDAL_OF_INTERNAL,
                                    l_apszAllowedDrivers,
                                    papszOpenOptions, nullptr);
                CSLDestroy(papszOpenOptions);
                if( m_hTileDS )
                {
                    OGRLayerH hLayer =
                        GDALDatasetGetLayerByName(m_hTileDS, GetName());
                    if( hLayer )
                    {
                        m_nFeatureCount += OGR_L_GetFeatureCount(hLayer, true);
                    }
                    GDALClose(m_hTileDS);
                    m_hTileDS = nullptr;
                }
            }
            ResetReading();
        }
        return m_nFeatureCount;
    }
    return OGRLayer::GetFeatureCount( bForce );
}

/************************************************************************/
/*                        GetNextSrcFeature()                           */
/************************************************************************/

OGRFeature* MBTilesVectorLayer::GetNextSrcFeature()
{
    if( m_bEOF )
    {
        return nullptr;
    }
    if( m_hTileIteratorLyr == nullptr )
    {
        ResetReading();
        if( m_hTileIteratorLyr == nullptr )
        {
            return nullptr;
        }
    }

    OGRFeatureH hTileFeat = nullptr;
    if( m_hTileDS == nullptr ||
        (hTileFeat =
            OGR_L_GetNextFeature(
                GDALDatasetGetLayerByName(m_hTileDS, GetName()))) == nullptr )
    {
        while( true )
        {
            OGRFeatureH hFeat = OGR_L_GetNextFeature(m_hTileIteratorLyr);
            if( hFeat == nullptr )
            {
                m_bEOF = true;
                return nullptr;
            }
            m_nX = OGR_F_GetFieldAsInteger(hFeat, 0);
            // MBTiles y origin is bottom based, whereas MVT directory
            // is top based
            m_nY = (1 << m_nZoomLevel) - 1 -
                        OGR_F_GetFieldAsInteger(hFeat, 1);
            CPLDebug("MBTiles", "X=%d, Y=%d", m_nX, m_nY);

            int nDataSize = 0;
            GByte* pabyData = OGR_F_GetFieldAsBinary(hFeat, 2, &nDataSize);
            GByte* pabyDataDup = static_cast<GByte*>(CPLMalloc(nDataSize));
            memcpy(pabyDataDup, pabyData, nDataSize);
            OGR_F_Destroy(hFeat);

            if( !m_osTmpFilename.empty() )
            {
                VSIUnlink(m_osTmpFilename);
            }
            m_osTmpFilename = CPLSPrintf("/vsimem/mvt_%p_%d_%d.pbf",
                                         this, m_nX, m_nY);
            VSIFCloseL(VSIFileFromMemBuffer(m_osTmpFilename,
                                            pabyDataDup, nDataSize,
                                            true));

            const char* l_apszAllowedDrivers[] = { "MVT", nullptr };
            if( m_hTileDS )
                GDALClose( m_hTileDS );
            char** papszOpenOptions = nullptr;
            papszOpenOptions = CSLSetNameValue(papszOpenOptions,
                "X", CPLSPrintf("%d", m_nX));
            papszOpenOptions = CSLSetNameValue(papszOpenOptions,
                "Y", CPLSPrintf("%d", m_nY));
            papszOpenOptions = CSLSetNameValue(papszOpenOptions,
                "Z", CPLSPrintf("%d", m_nZoomLevel));
            papszOpenOptions = CSLSetNameValue(papszOpenOptions,
                "METADATA_FILE",
                m_bJsonField ? "": m_poDS->m_osMetadataMemFilename.c_str());
            if( !m_poDS->m_osClip.empty() )
            {
                papszOpenOptions = CSLSetNameValue(papszOpenOptions,
                    "CLIP", m_poDS->m_osClip);
            }
            m_hTileDS = GDALOpenEx( ("MVT:" + m_osTmpFilename).c_str(),
                                GDAL_OF_VECTOR | GDAL_OF_INTERNAL,
                                l_apszAllowedDrivers,
                                papszOpenOptions, nullptr);
            CSLDestroy(papszOpenOptions);
            if( m_hTileDS )
            {
                if( GDALDatasetGetLayerByName(m_hTileDS, GetName()) )
                {
                    hTileFeat = OGR_L_GetNextFeature(
                        GDALDatasetGetLayerByName(m_hTileDS, GetName()));
                    if( hTileFeat )
                        break;
                }
                GDALClose(m_hTileDS);
                m_hTileDS = nullptr;
            }
        }
    }

    return reinterpret_cast<OGRFeature*>(hTileFeat);
}

/************************************************************************/
/*                         CreateFeatureFrom()                          */
/************************************************************************/

OGRFeature* MBTilesVectorLayer::CreateFeatureFrom(OGRFeature* poSrcFeature)
{

    return OGRMVTCreateFeatureFrom(poSrcFeature, m_poFeatureDefn,
                                   m_bJsonField, GetSpatialRef());
}

/************************************************************************/
/*                        GetNextRawFeature()                           */
/************************************************************************/

OGRFeature* MBTilesVectorLayer::GetNextRawFeature()
{
    OGRFeature* poSrcFeat = GetNextSrcFeature();
    if( poSrcFeat == nullptr )
        return nullptr;

    const GIntBig nFIDBase = (static_cast<GIntBig>(m_nY) <<
                            m_nZoomLevel) | m_nX;
    OGRFeature* poFeature = CreateFeatureFrom(poSrcFeat);
    poFeature->SetFID(
        (poSrcFeat->GetFID() << (2 * m_nZoomLevel)) | nFIDBase );
    delete poSrcFeat;

    return poFeature;
}

/************************************************************************/
/*                           GetFeature()                               */
/************************************************************************/

OGRFeature* MBTilesVectorLayer::GetFeature(GIntBig nFID)
{
    const int nZ = m_nZoomLevel;
    const int nX = static_cast<int>(nFID & ((1 << nZ)-1));
    const int nY = static_cast<int>((nFID >> nZ) & ((1 << nZ)-1));
    const GIntBig nTileFID = nFID >> (2 * nZ);

    CPLString osSQL;
    osSQL.Printf("SELECT tile_data FROM tiles "
                 "WHERE zoom_level = %d AND "
                 "tile_column = %d AND tile_row = %d",
                 m_nZoomLevel,
                 nX,
                 (1 << nZ) - 1 - nY);
    auto hSQLLyr = OGR_DS_ExecuteSQL(m_poDS->hDS, osSQL.c_str(),
                                     nullptr, nullptr);
    if( hSQLLyr == nullptr )
        return nullptr;
    auto hFeat = OGR_L_GetNextFeature(hSQLLyr);
    if( hFeat == nullptr )
    {
        OGR_DS_ReleaseResultSet(m_poDS->hDS, hSQLLyr);
        return nullptr;
    }
    int nDataSize = 0;
    GByte* pabyData = OGR_F_GetFieldAsBinary(hFeat, 0, &nDataSize);
    GByte* pabyDataDup = static_cast<GByte*>(CPLMalloc(nDataSize));
    memcpy(pabyDataDup, pabyData, nDataSize);
    OGR_F_Destroy(hFeat);
    OGR_DS_ReleaseResultSet(m_poDS->hDS, hSQLLyr);

    CPLString osTmpFilename =
        CPLSPrintf("/vsimem/mvt_getfeature_%p_%d_%d.pbf", this, nX, nY);
    VSIFCloseL(VSIFileFromMemBuffer(osTmpFilename,
                                    pabyDataDup, nDataSize,
                                    true));

    const char* l_apszAllowedDrivers[] = { "MVT", nullptr };
    char** papszOpenOptions = nullptr;
    papszOpenOptions = CSLSetNameValue(papszOpenOptions,
        "X", CPLSPrintf("%d", nX));
    papszOpenOptions = CSLSetNameValue(papszOpenOptions,
        "Y", CPLSPrintf("%d", nY));
    papszOpenOptions = CSLSetNameValue(papszOpenOptions,
        "Z", CPLSPrintf("%d", m_nZoomLevel));
    papszOpenOptions = CSLSetNameValue(papszOpenOptions,
        "METADATA_FILE",
        m_bJsonField ? "" : m_poDS->m_osMetadataMemFilename.c_str());
    if( !m_poDS->m_osClip.empty() )
    {
        papszOpenOptions = CSLSetNameValue(papszOpenOptions,
            "CLIP", m_poDS->m_osClip);
    }
    auto hTileDS = GDALOpenEx(("MVT:" + osTmpFilename).c_str(),
                        GDAL_OF_VECTOR | GDAL_OF_INTERNAL,
                        l_apszAllowedDrivers,
                        papszOpenOptions, nullptr);
    CSLDestroy(papszOpenOptions);

    OGRFeature* poFeature = nullptr;
    if( hTileDS )
    {
        OGRLayerH hLayer = GDALDatasetGetLayerByName(hTileDS, GetName());
        if( hLayer )
        {
            OGRFeature* poUnderlyingFeature = reinterpret_cast<OGRFeature*>(
                OGR_L_GetFeature(hLayer, nTileFID));
            if( poUnderlyingFeature )
            {
                poFeature = CreateFeatureFrom( poUnderlyingFeature );
                poFeature->SetFID(nFID);
            }
            delete poUnderlyingFeature;
        }
    }
    GDALClose(hTileDS);

    VSIUnlink(osTmpFilename);

    return poFeature;
}

/************************************************************************/
/*                           InitVector()                               */
/************************************************************************/

void MBTilesDataset::InitVector(double dfMinX, double dfMinY,
                                double dfMaxX, double dfMaxY,
                                bool bZoomLevelFromSpatialFilter,
                                bool bJsonField)
{
    const char* pszSQL =
        "SELECT value FROM metadata WHERE name = 'json'";
    CPLDebug("MBTILES", "%s", pszSQL);
    CPLJSONDocument oJsonDoc;
    CPLJSONDocument oDoc;
    auto hSQLLyr = OGR_DS_ExecuteSQL(hDS, pszSQL, nullptr, nullptr);
    if (hSQLLyr)
    {
        auto hFeat = OGR_L_GetNextFeature(hSQLLyr);
        if (hFeat)
        {
            auto pszJson = OGR_F_GetFieldAsString(hFeat, 0);
            oDoc.GetRoot().Add( "json", pszJson );
            CPL_IGNORE_RET_VAL(oJsonDoc.LoadMemory(
                    reinterpret_cast<const GByte*>(pszJson)));
            OGR_F_Destroy(hFeat);
        }
        OGR_DS_ReleaseResultSet(hDS, hSQLLyr);
    }

    m_osMetadataMemFilename =
        CPLSPrintf("/vsimem/%p_metadata.json", this);
    oDoc.Save(m_osMetadataMemFilename);

    CPLJSONArray oVectorLayers;
    oVectorLayers.Deinit();

    CPLJSONArray oTileStatLayers;
    oTileStatLayers.Deinit();

    oVectorLayers =
        oJsonDoc.GetRoot().GetArray("vector_layers");

    oTileStatLayers =
        oJsonDoc.GetRoot().GetArray("tilestats/layers");

    for( int i = 0; i < oVectorLayers.Size(); i++ )
    {
        CPLJSONObject oId = oVectorLayers[i].GetObj("id");
        if( oId.IsValid() && oId.GetType() ==
                CPLJSONObject::Type::String )
        {
            OGRwkbGeometryType eGeomType = wkbUnknown;
            if( oTileStatLayers.IsValid() )
            {
                eGeomType = OGRMVTFindGeomTypeFromTileStat(
                    oTileStatLayers, oId.ToString().c_str());
            }

            CPLJSONObject oFields = oVectorLayers[i].GetObj("fields");
            m_apoLayers.push_back( std::unique_ptr<OGRLayer>(
                new MBTilesVectorLayer(this,
                                       oId.ToString().c_str(),
                                       oFields,
                                       bJsonField,
                                       dfMinX, dfMinY, dfMaxX, dfMaxY,
                                       eGeomType,
                                       bZoomLevelFromSpatialFilter))
            );
        }
    }
}

/************************************************************************/
/*                             Identify()                               */
/************************************************************************/

int MBTilesDataset::Identify(GDALOpenInfo* poOpenInfo)
{
#ifdef ENABLE_SQL_SQLITE_FORMAT
    if( poOpenInfo->pabyHeader &&
        STARTS_WITH((const char*)poOpenInfo->pabyHeader, "-- SQL MBTILES") )
    {
        return TRUE;
    }
#endif

    if ( (EQUAL(CPLGetExtension(poOpenInfo->pszFilename), "MBTILES") ||
      // Allow direct Amazon S3 signed URLs that contains .mbtiles in the middle of the URL
          strstr(poOpenInfo->pszFilename, ".mbtiles") != nullptr) &&
        poOpenInfo->nHeaderBytes >= 1024 &&
        poOpenInfo->pabyHeader &&
        STARTS_WITH_CI((const char*)poOpenInfo->pabyHeader, "SQLite Format 3"))
    {
        return TRUE;
    }

    return FALSE;
}

/************************************************************************/
/*                        MBTilesGetMinMaxZoomLevel()                   */
/************************************************************************/

static
int MBTilesGetMinMaxZoomLevel(OGRDataSourceH hDS, int bHasMap,
                                int &nMinLevel, int &nMaxLevel)
{
    OGRLayerH hSQLLyr;
    OGRFeatureH hFeat;
    int bHasMinMaxLevel = FALSE;

    const char* pszSQL =
        "SELECT value FROM metadata WHERE name = 'minzoom' UNION ALL "
        "SELECT value FROM metadata WHERE name = 'maxzoom'";
    CPLDebug("MBTILES", "%s", pszSQL);
    hSQLLyr = OGR_DS_ExecuteSQL(hDS, pszSQL, nullptr, nullptr);
    if (hSQLLyr)
    {
        hFeat = OGR_L_GetNextFeature(hSQLLyr);
        if (hFeat)
        {
            int bHasMinLevel = FALSE;
            if (OGR_F_IsFieldSetAndNotNull(hFeat, 0))
            {
                nMinLevel = OGR_F_GetFieldAsInteger(hFeat, 0);
                bHasMinLevel = TRUE;
            }
            OGR_F_Destroy(hFeat);

            if (bHasMinLevel)
            {
                hFeat = OGR_L_GetNextFeature(hSQLLyr);
                if (hFeat)
                {
                    if (OGR_F_IsFieldSetAndNotNull(hFeat, 0))
                    {
                        nMaxLevel = OGR_F_GetFieldAsInteger(hFeat, 0);
                        bHasMinMaxLevel = TRUE;
                    }
                    OGR_F_Destroy(hFeat);
                }
            }
        }

        OGR_DS_ReleaseResultSet(hDS, hSQLLyr);
    }

    if( !bHasMinMaxLevel )
    {
#define OPTIMIZED_FOR_VSICURL
#ifdef  OPTIMIZED_FOR_VSICURL
        int iLevel;
        for(iLevel = 0; nMinLevel < 0 && iLevel <= 32; iLevel ++)
        {
            pszSQL = CPLSPrintf(
                "SELECT zoom_level FROM %s WHERE zoom_level = %d LIMIT 1",
                (bHasMap) ? "map" : "tiles", iLevel);
            CPLDebug("MBTILES", "%s", pszSQL);
            hSQLLyr = OGR_DS_ExecuteSQL(hDS, pszSQL, nullptr, nullptr);
            if (hSQLLyr)
            {
                hFeat = OGR_L_GetNextFeature(hSQLLyr);
                if (hFeat)
                {
                    nMinLevel = iLevel;
                    OGR_F_Destroy(hFeat);
                }
                OGR_DS_ReleaseResultSet(hDS, hSQLLyr);
            }
        }

        if (nMinLevel < 0)
            return FALSE;

        for(iLevel = 32; nMaxLevel < 0 && iLevel >= nMinLevel; iLevel --)
        {
            pszSQL = CPLSPrintf(
                "SELECT zoom_level FROM %s WHERE zoom_level = %d LIMIT 1",
                (bHasMap) ? "map" : "tiles", iLevel);
            CPLDebug("MBTILES", "%s", pszSQL);
            hSQLLyr = OGR_DS_ExecuteSQL(hDS, pszSQL, nullptr, nullptr);
            if (hSQLLyr)
            {
                hFeat = OGR_L_GetNextFeature(hSQLLyr);
                if (hFeat)
                {
                    nMaxLevel = iLevel;
                    bHasMinMaxLevel = TRUE;
                    OGR_F_Destroy(hFeat);
                }
                OGR_DS_ReleaseResultSet(hDS, hSQLLyr);
            }
        }
#else
        pszSQL = "SELECT min(zoom_level), max(zoom_level) FROM tiles";
        CPLDebug("MBTILES", "%s", pszSQL);
        hSQLLyr = OGR_DS_ExecuteSQL(hDS, pszSQL, NULL, nullptr);
        if (hSQLLyr == NULL)
        {
            return FALSE;
        }

        hFeat = OGR_L_GetNextFeature(hSQLLyr);
        if (hFeat == NULL)
        {
            OGR_DS_ReleaseResultSet(hDS, hSQLLyr);
            return FALSE;
        }

        if (OGR_F_IsFieldSetAndNotNull(hFeat, 0) && OGR_F_IsFieldSetAndNotNull(hFeat, 1))
        {
            nMinLevel = OGR_F_GetFieldAsInteger(hFeat, 0);
            nMaxLevel = OGR_F_GetFieldAsInteger(hFeat, 1);
            bHasMinMaxLevel = TRUE;
        }

        OGR_F_Destroy(hFeat);
        OGR_DS_ReleaseResultSet(hDS, hSQLLyr);
#endif
    }

    return bHasMinMaxLevel;
}

/************************************************************************/
/*                   MBTilesTileCoordToWorldCoord()                     */
/************************************************************************/

static double MBTilesTileCoordToWorldCoord(double dfTileCoord, int nZoomLevel)
{
    return -MAX_GM + 2 * MAX_GM * (dfTileCoord / (1 << nZoomLevel));
}

/************************************************************************/
/*                   MBTilesWorldCoordToTileCoord()                     */
/************************************************************************/

static double MBTilesWorldCoordToTileCoord(double dfWorldCoord, int nZoomLevel)
{
    return (dfWorldCoord + MAX_GM) / (2 * MAX_GM) * (1 << nZoomLevel);
}

/************************************************************************/
/*                           MBTilesGetBounds()                         */
/************************************************************************/

static
bool MBTilesGetBounds(OGRDataSourceH hDS, bool bUseBounds,
                     int nMaxLevel,
                     double& minX, double& minY,
                     double& maxX, double& maxY)
{
    bool bHasBounds = false;
    OGRLayerH hSQLLyr;
    OGRFeatureH hFeat;

    if( bUseBounds )
    {
        const char* pszSQL = "SELECT value FROM metadata WHERE name = 'bounds'";
        CPLDebug("MBTILES", "%s", pszSQL);
        hSQLLyr = OGR_DS_ExecuteSQL(hDS, pszSQL, nullptr, nullptr);
        if (hSQLLyr)
        {
            hFeat = OGR_L_GetNextFeature(hSQLLyr);
            if (hFeat != nullptr)
            {
                const char* pszBounds = OGR_F_GetFieldAsString(hFeat, 0);
                char** papszTokens = CSLTokenizeString2(pszBounds, ",", 0);
                if (CSLCount(papszTokens) != 4 ||
                    fabs(CPLAtof(papszTokens[0])) > 180 ||
                    fabs(CPLAtof(papszTokens[1])) >= 89.99 ||
                    fabs(CPLAtof(papszTokens[2])) > 180 ||
                    fabs(CPLAtof(papszTokens[3])) >= 89.99 ||
                    CPLAtof(papszTokens[0]) > CPLAtof(papszTokens[2]) ||
                    CPLAtof(papszTokens[1]) > CPLAtof(papszTokens[3]))
                {
                    CPLError(CE_Warning, CPLE_AppDefined, "Invalid value for 'bounds' metadata. Ignoring it and fall back to present tile extent");
                }
                else
                {
                    minX = CPLAtof(papszTokens[0]);
                    minY = CPLAtof(papszTokens[1]);
                    maxX = CPLAtof(papszTokens[2]);
                    maxY = CPLAtof(papszTokens[3]);
                    LongLatToSphericalMercator(&minX, &minY);
                    LongLatToSphericalMercator(&maxX, &maxY);

                    // Clamp northings
                    if( maxY > MAX_GM)
                        maxY = MAX_GM;
                    if( minY < -MAX_GM)
                        minY = -MAX_GM;

                    bHasBounds = true;
                }

                CSLDestroy(papszTokens);

                OGR_F_Destroy(hFeat);
            }
            OGR_DS_ReleaseResultSet(hDS, hSQLLyr);
        }
    }

    if (!bHasBounds)
    {
        const char* pszSQL =
            CPLSPrintf("SELECT min(tile_column), max(tile_column), "
                       "min(tile_row), max(tile_row) FROM tiles "
                       "WHERE zoom_level = %d", nMaxLevel);
        CPLDebug("MBTILES", "%s", pszSQL);
        hSQLLyr = OGR_DS_ExecuteSQL(hDS, pszSQL, nullptr, nullptr);
        if (hSQLLyr == nullptr)
        {
            return false;
        }

        hFeat = OGR_L_GetNextFeature(hSQLLyr);
        if (hFeat == nullptr)
        {
            OGR_DS_ReleaseResultSet(hDS, hSQLLyr);
            return false;
        }

        if (OGR_F_IsFieldSetAndNotNull(hFeat, 0) &&
            OGR_F_IsFieldSetAndNotNull(hFeat, 1) &&
            OGR_F_IsFieldSetAndNotNull(hFeat, 2) &&
            OGR_F_IsFieldSetAndNotNull(hFeat, 3))
        {
            int nMinTileCol = OGR_F_GetFieldAsInteger(hFeat, 0);
            int nMaxTileCol = OGR_F_GetFieldAsInteger(hFeat, 1);
            int nMinTileRow = OGR_F_GetFieldAsInteger(hFeat, 2);
            int nMaxTileRow = OGR_F_GetFieldAsInteger(hFeat, 3);
            if( nMaxTileCol < INT_MAX && nMaxTileRow < INT_MAX )
            {
                minX = MBTilesTileCoordToWorldCoord(nMinTileCol, nMaxLevel);
                minY = MBTilesTileCoordToWorldCoord(nMinTileRow, nMaxLevel);
                maxX = MBTilesTileCoordToWorldCoord(nMaxTileCol + 1, nMaxLevel);
                maxY = MBTilesTileCoordToWorldCoord(nMaxTileRow + 1, nMaxLevel);
                bHasBounds = true;
            }
        }

        OGR_F_Destroy(hFeat);
        OGR_DS_ReleaseResultSet(hDS, hSQLLyr);
    }

    return bHasBounds;
}

/************************************************************************/
/*                        MBTilesCurlReadCbk()                          */
/************************************************************************/

typedef struct
{
    int nBands;
    int nSize;
} TileProperties;

/* We spy the data received by CURL for the initial request where we try */
/* to get a first tile to see its characteristics. We just need the header */
/* to determine that, so let's make VSICurl stop reading after we have found it */

static int MBTilesCurlReadCbk(CPL_UNUSED VSILFILE* fp,
                              void *pabyBuffer, size_t nBufferSize,
                              void* pfnUserData)
{
    TileProperties* psTP = static_cast<TileProperties*>(pfnUserData);

    const GByte abyPNGSig[] = { 0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, /* PNG signature */
                                0x00, 0x00, 0x00, 0x0D, /* IHDR length */
                                0x49, 0x48, 0x44, 0x52  /* IHDR chunk */ };

    /* JPEG SOF0 (Start Of Frame 0) marker */
    const GByte abyJPEG1CompSig[] = { 0xFF, 0xC0, /* marker */
                                      0x00, 0x0B, /* data length = 8 + 1 * 3 */
                                      0x08,       /* depth : 8 bit */ };
    const GByte abyJPEG3CompSig[] = { 0xFF, 0xC0, /* marker */
                                      0x00, 0x11, /* data length = 8 + 3 * 3 */
                                      0x08,       /* depth : 8 bit */ };

    int i;
    for(i = 0; i < (int)nBufferSize - (int)sizeof(abyPNGSig); i++)
    {
        if (memcmp(((GByte*)pabyBuffer) + i, abyPNGSig, sizeof(abyPNGSig)) == 0 &&
            i + sizeof(abyPNGSig) + 4 + 4 + 1 + 1 < nBufferSize)
        {
            GByte* ptr = ((GByte*)(pabyBuffer)) + i + (int)sizeof(abyPNGSig);

            int nWidth;
            memcpy(&nWidth, ptr, 4);
            CPL_MSBPTR32(&nWidth);
            ptr += 4;

            int nHeight;
            memcpy(&nHeight, ptr, 4);
            CPL_MSBPTR32(&nHeight);
            ptr += 4;

            GByte nDepth = *ptr;
            ptr += 1;

            GByte nColorType = *ptr;
            CPLDebug("MBTILES", "PNG: nWidth=%d nHeight=%d depth=%d nColorType=%d",
                        nWidth, nHeight, nDepth, nColorType);

            psTP->nBands = -2;
            psTP->nSize = nWidth;
            if (nWidth == nHeight && nDepth == 8)
            {
                if (nColorType == 0)
                    psTP->nBands = 1; /* Gray */
                else if (nColorType == 2)
                    psTP->nBands = 3; /* RGB */
                else if (nColorType == 3)
                {
                    /* This might also be a color table with transparency */
                    /* but we cannot tell ! */
                    psTP->nBands = -1;
                    return TRUE;
                }
                else if (nColorType == 4)
                    psTP->nBands = 2; /* Gray + alpha */
                else if (nColorType == 6)
                    psTP->nBands = 4; /* RGB + alpha */
            }

            return FALSE;
        }
    }

    for(i = 0; i < (int)nBufferSize - ((int)sizeof(abyJPEG1CompSig) + 5); i++)
    {
        if (memcmp(((GByte*)pabyBuffer) + i, abyJPEG1CompSig, sizeof(abyJPEG1CompSig)) == 0 &&
            ((GByte*)pabyBuffer)[sizeof(abyJPEG1CompSig) + 4] == 1)
        {
            GUInt16 nWidth;
            memcpy(&nWidth, &(((GByte*)pabyBuffer)[sizeof(abyJPEG1CompSig)]), 2);
            CPL_MSBPTR16(&nWidth);
            GUInt16 nHeight;
            memcpy(&nHeight, &(((GByte*)pabyBuffer)[sizeof(abyJPEG1CompSig) + 2]), 2);
            CPL_MSBPTR16(&nHeight);

            CPLDebug("MBTILES", "JPEG: nWidth=%d nHeight=%d depth=%d nBands=%d",
                      nWidth, nHeight, 8, 1);

            psTP->nBands = -2;
            if( nWidth == nHeight )
            {
                psTP->nSize = nWidth;
                psTP->nBands = 1;
            }

            return FALSE;
        }
        else if (memcmp(((GByte*)pabyBuffer) + i, abyJPEG3CompSig, sizeof(abyJPEG3CompSig)) == 0 &&
            ((GByte*)pabyBuffer)[sizeof(abyJPEG3CompSig) + 4] == 3)
        {
            GUInt16 nWidth;
            memcpy(&nWidth, &(((GByte*)pabyBuffer)[sizeof(abyJPEG3CompSig)]), 2);
            CPL_MSBPTR16(&nWidth);
            GUInt16 nHeight;
            memcpy(&nHeight, &(((GByte*)pabyBuffer)[sizeof(abyJPEG3CompSig) + 2]), 2);
            CPL_MSBPTR16(&nHeight);

            CPLDebug("MBTILES", "JPEG: nWidth=%d nHeight=%d depth=%d nBands=%d",
                      nWidth, nHeight, 8, 3);

            psTP->nBands = -2;
            if( nWidth == nHeight )
            {
                psTP->nSize = nWidth;
                psTP->nBands = 3;
            }

            return FALSE;
        }
    }

    return TRUE;
}

/************************************************************************/
/*                  MBTilesGetBandCountAndTileSize()                    */
/************************************************************************/

static
int MBTilesGetBandCountAndTileSize(
                        bool bIsVSICURL,
                        OGRDataSourceH &hDS,
                        int nMaxLevel,
                        int nMinTileRow, int nMaxTileRow,
                        int nMinTileCol, int nMaxTileCol,
                        int &nTileSize)
{
    OGRLayerH hSQLLyr;
    OGRFeatureH hFeat;
    VSILFILE* fpCURLOGR = nullptr;
    int bFirstSelect = TRUE;

    int nBands = -1;
    nTileSize = 0;

    /* Small trick to get the VSILFILE associated with the OGR SQLite */
    /* DB */
    CPLString osDSName(OGR_DS_GetName(hDS));
    if (bIsVSICURL)
    {
        CPLErrorReset();
        CPLPushErrorHandler(CPLQuietErrorHandler);
        hSQLLyr = OGR_DS_ExecuteSQL(hDS, "GetVSILFILE()", nullptr, nullptr);
        CPLPopErrorHandler();
        CPLErrorReset();
        if (hSQLLyr != nullptr)
        {
            hFeat = OGR_L_GetNextFeature(hSQLLyr);
            if (hFeat)
            {
                if (OGR_F_IsFieldSetAndNotNull(hFeat, 0))
                {
                    const char* pszPointer = OGR_F_GetFieldAsString(hFeat, 0);
                    fpCURLOGR = (VSILFILE* )CPLScanPointer( pszPointer, static_cast<int>(strlen(pszPointer)) );
                }
                OGR_F_Destroy(hFeat);
            }
            OGR_DS_ReleaseResultSet(hDS, hSQLLyr);
        }
    }

    const char* pszSQL =
        CPLSPrintf("SELECT tile_data FROM tiles WHERE "
                   "tile_column = %d AND tile_row = %d AND zoom_level = %d",
                   nMinTileCol / 2 + nMaxTileCol / 2,
                   nMinTileRow / 2 + nMaxTileRow / 2,
                   nMaxLevel);
    CPLDebug("MBTILES", "%s", pszSQL);

    if (fpCURLOGR)
    {
        /* Install a spy on the file connection that will intercept */
        /* PNG or JPEG headers, to interrupt their downloading */
        /* once the header is found. Speeds up dataset opening. */
        CPLErrorReset();
        TileProperties tp;
        tp.nBands = -1;
        tp.nSize = 0;
        VSICurlInstallReadCbk(fpCURLOGR, MBTilesCurlReadCbk, &tp, TRUE);
        nBands = tp.nBands;
        nTileSize = tp.nSize;

        CPLErrorReset();
        CPLPushErrorHandler(CPLQuietErrorHandler);
        hSQLLyr = OGR_DS_ExecuteSQL(hDS, pszSQL, nullptr, nullptr);
        CPLPopErrorHandler();

        VSICurlUninstallReadCbk(fpCURLOGR);

        /* Did the spy intercept something interesting ? */
        // cppcheck-suppress knownConditionTrueFalse
        if (nBands != -1)
        {
            CPLErrorReset();

            OGR_DS_ReleaseResultSet(hDS, hSQLLyr);
            hSQLLyr = nullptr;

            // Re-open OGR SQLite DB, because with our spy we have simulated an
            // I/O error that SQLite will have difficulties to recover within
            // the existing connection.  This will be fast because
            // the /vsicurl/ cache has cached the already read blocks.
            OGRReleaseDataSource(hDS);
            hDS = MBTILESOpenSQLiteDB(osDSName.c_str(), GA_ReadOnly);
            if (hDS == nullptr)
                return -1;

            /* Unrecognized form of PNG. Error out */
            if (nBands <= 0)
                return -1;

            return nBands;
        }
        else if (CPLGetLastErrorType() == CE_Failure)
        {
            CPLError(CE_Failure, CPLGetLastErrorNo(), "%s", CPLGetLastErrorMsg());
        }
    }
    else
    {
        hSQLLyr = OGR_DS_ExecuteSQL(hDS, pszSQL, nullptr, nullptr);
    }

    while( true )
    {
        if (hSQLLyr == nullptr && bFirstSelect)
        {
            bFirstSelect = FALSE;
            pszSQL = CPLSPrintf("SELECT tile_data FROM tiles WHERE "
                                "zoom_level = %d LIMIT 1", nMaxLevel);
            CPLDebug("MBTILES", "%s", pszSQL);
            hSQLLyr = OGR_DS_ExecuteSQL(hDS, pszSQL, nullptr, nullptr);
            if (hSQLLyr == nullptr)
                return -1;
        }

        hFeat = OGR_L_GetNextFeature(hSQLLyr);
        if (hFeat == nullptr)
        {
            OGR_DS_ReleaseResultSet(hDS, hSQLLyr);
            hSQLLyr = nullptr;
            if( !bFirstSelect )
                return -1;
        }
        else
            break;
    }

    CPLString osMemFileName;
    osMemFileName.Printf("/vsimem/%p", hSQLLyr);

    int nDataSize = 0;
    GByte* pabyData = OGR_F_GetFieldAsBinary(hFeat, 0, &nDataSize);

    VSIFCloseL(VSIFileFromMemBuffer( osMemFileName.c_str(), pabyData,
                                    nDataSize, FALSE));

    GDALDatasetH hDSTile = GDALOpenEx(osMemFileName.c_str(), GDAL_OF_RASTER,
                                          apszAllowedDrivers, nullptr, nullptr);
    if (hDSTile == nullptr)
    {
        VSIUnlink(osMemFileName.c_str());
        OGR_F_Destroy(hFeat);
        OGR_DS_ReleaseResultSet(hDS, hSQLLyr);
        return -1;
    }

    nBands = GDALGetRasterCount(hDSTile);

    if ((nBands != 1 && nBands != 2 && nBands != 3 && nBands != 4) ||
        GDALGetRasterXSize(hDSTile) != GDALGetRasterYSize(hDSTile) ||
        GDALGetRasterDataType(GDALGetRasterBand(hDSTile, 1)) != GDT_Byte)
    {
        CPLError(CE_Failure, CPLE_NotSupported, "Unsupported tile characteristics");
        GDALClose(hDSTile);
        VSIUnlink(osMemFileName.c_str());
        OGR_F_Destroy(hFeat);
        OGR_DS_ReleaseResultSet(hDS, hSQLLyr);
        return -1;
    }

    nTileSize = GDALGetRasterXSize(hDSTile);
    GDALColorTableH hCT = GDALGetRasterColorTable(GDALGetRasterBand(hDSTile, 1));
    if (nBands == 1 && hCT != nullptr)
    {
        nBands = 3;
        if( GDALGetColorEntryCount(hCT) > 0 )
        {
            /* Typical of paletted PNG with transparency */
            const GDALColorEntry* psEntry = GDALGetColorEntry( hCT, 0 );
            if( psEntry->c4 == 0 )
                nBands = 4;
        }
    }

    GDALClose(hDSTile);
    VSIUnlink(osMemFileName.c_str());
    OGR_F_Destroy(hFeat);
    OGR_DS_ReleaseResultSet(hDS, hSQLLyr);

    return nBands;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset* MBTilesDataset::Open(GDALOpenInfo* poOpenInfo)
{
    CPLString osFileName;
    CPLString osTableName;

    if (!Identify(poOpenInfo))
        return nullptr;

    if( (poOpenInfo->nOpenFlags & GDAL_OF_VECTOR) != 0 &&
        (poOpenInfo->nOpenFlags & GDAL_OF_RASTER) == 0 &&
        (poOpenInfo->nOpenFlags & GDAL_OF_UPDATE) != 0 )
    {
        return nullptr;
    }

    if (OGRGetDriverCount() == 0)
        OGRRegisterAll();

/* -------------------------------------------------------------------- */
/*      Open underlying OGR DB                                          */
/* -------------------------------------------------------------------- */

    OGRDataSourceH hDS = MBTILESOpenSQLiteDB(poOpenInfo->pszFilename,
                                             poOpenInfo->eAccess);

    MBTilesDataset* poDS = nullptr;

    if (hDS == nullptr)
        goto end;

/* -------------------------------------------------------------------- */
/*      Build dataset                                                   */
/* -------------------------------------------------------------------- */
    {
        CPLString osMetadataTableName, osRasterTableName;
        CPLString osSQL;
        OGRLayerH hMetadataLyr, hRasterLyr;
        OGRFeatureH hFeat;
        int nBands;
        OGRLayerH hSQLLyr = nullptr;
        int nMinLevel = -1;
        int nMaxLevel = -1;
        int bHasMinMaxLevel = FALSE;
        int bHasMap;

        osMetadataTableName = "metadata";

        hMetadataLyr = OGR_DS_GetLayerByName(hDS, osMetadataTableName.c_str());
        if (hMetadataLyr == nullptr)
            goto end;

        osRasterTableName += "tiles";

        hRasterLyr = OGR_DS_GetLayerByName(hDS, osRasterTableName.c_str());
        if (hRasterLyr == nullptr)
            goto end;

        bHasMap = OGR_DS_GetLayerByName(hDS, "map") != nullptr;
        if (bHasMap)
        {
            bHasMap = FALSE;

            hSQLLyr = OGR_DS_ExecuteSQL(hDS, "SELECT type FROM sqlite_master WHERE name = 'tiles'", nullptr, nullptr);
            if (hSQLLyr != nullptr)
            {
                hFeat = OGR_L_GetNextFeature(hSQLLyr);
                if (hFeat)
                {
                    if (OGR_F_IsFieldSetAndNotNull(hFeat, 0))
                    {
                        bHasMap = strcmp(OGR_F_GetFieldAsString(hFeat, 0),
                                         "view") == 0;
                        if (!bHasMap)
                        {
                            CPLDebug("MBTILES", "Weird! 'tiles' is not a view, but 'map' exists");
                        }
                    }
                    OGR_F_Destroy(hFeat);
                }
                OGR_DS_ReleaseResultSet(hDS, hSQLLyr);
            }
        }

/* -------------------------------------------------------------------- */
/*      Get minimum and maximum zoom levels                             */
/* -------------------------------------------------------------------- */

        bHasMinMaxLevel = MBTilesGetMinMaxZoomLevel(hDS, bHasMap,
                                                    nMinLevel, nMaxLevel);

        const char* pszZoomLevel = CSLFetchNameValue(poOpenInfo->papszOpenOptions, "ZOOM_LEVEL");
        if( pszZoomLevel != nullptr )
            nMaxLevel = atoi(pszZoomLevel);

        if (bHasMinMaxLevel && (nMinLevel < 0 || nMinLevel > nMaxLevel))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Inconsistent values : min(zoom_level) = %d, max(zoom_level) = %d",
                     nMinLevel, nMaxLevel);
            goto end;
        }

        if (bHasMinMaxLevel && nMaxLevel > 22)
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "zoom_level > 22 not supported");
            goto end;
        }

        if (!bHasMinMaxLevel)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Cannot find min and max zoom_level");
            goto end;
        }

/* -------------------------------------------------------------------- */
/*      Get bounds                                                      */
/* -------------------------------------------------------------------- */
        double dfMinX = 0.0;
        double dfMinY = 0.0;
        double dfMaxX = 0.0;
        double dfMaxY = 0.0;
        bool bUseBounds = CPLFetchBool(const_cast<const char**>(poOpenInfo->papszOpenOptions),
                                      "USE_BOUNDS", true);
        const char* pszMinX = CSLFetchNameValue(poOpenInfo->papszOpenOptions, "MINX");
        const char* pszMinY = CSLFetchNameValue(poOpenInfo->papszOpenOptions, "MINY");
        const char* pszMaxX = CSLFetchNameValue(poOpenInfo->papszOpenOptions, "MAXX");
        const char* pszMaxY = CSLFetchNameValue(poOpenInfo->papszOpenOptions, "MAXY");
        bool bHasBounds;
        if( pszMinX != nullptr && pszMinY != nullptr && pszMaxX != nullptr && pszMaxY != nullptr )
        {
            bHasBounds = true;
        }
        else
        {
            bHasBounds = MBTilesGetBounds(hDS, bUseBounds, nMaxLevel,
                                          dfMinX, dfMinY,
                                          dfMaxX, dfMaxY);
        }
        if (!bHasBounds)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Cannot find min and max tile numbers");
            goto end;
        }
        if( pszMinX != nullptr ) dfMinX = CPLAtof(pszMinX);
        if( pszMinY != nullptr ) dfMinY = CPLAtof(pszMinY);
        if( pszMaxX != nullptr ) dfMaxX = CPLAtof(pszMaxX);
        if( pszMaxY != nullptr ) dfMaxY = CPLAtof(pszMaxY);

/* -------------------------------------------------------------------- */
/*      Get number of bands                                             */
/* -------------------------------------------------------------------- */
        int nMinTileCol = static_cast<int>(MBTilesWorldCoordToTileCoord( dfMinX, nMaxLevel ));
        int nMinTileRow = static_cast<int>(MBTilesWorldCoordToTileCoord( dfMinY, nMaxLevel ));
        int nMaxTileCol = static_cast<int>(MBTilesWorldCoordToTileCoord( dfMaxX, nMaxLevel ));
        int nMaxTileRow = static_cast<int>(MBTilesWorldCoordToTileCoord( dfMaxY, nMaxLevel ));
        int nTileSize = 0;
        nBands = MBTilesGetBandCountAndTileSize(
            STARTS_WITH_CI(poOpenInfo->pszFilename, "/vsicurl/"),
            hDS, nMaxLevel,
            nMinTileRow, nMaxTileRow,
            nMinTileCol, nMaxTileCol,
            nTileSize);
        bool bFoundRasterTile = nBands > 0;
        if( !bFoundRasterTile )
            nTileSize = knDEFAULT_BLOCK_SIZE;

        // Force 4 bands by default (see #6119)
        nBands = 4;

        const char* pszBandCount = CSLFetchNameValueDef(
            poOpenInfo->papszOpenOptions, "BAND_COUNT",
            CPLGetConfigOption("MBTILES_BAND_COUNT", nullptr));
        if( pszBandCount )
        {
            int nTmpBands = atoi(pszBandCount);
            if( nTmpBands >= 1 && nTmpBands <= 4 )
                nBands = nTmpBands;
        }

        if( poOpenInfo->eAccess == GA_Update )
        {
            // So that we can edit all potential overviews
            nMinLevel = 0;
        }

/* -------------------------------------------------------------------- */
/*      Set dataset attributes                                          */
/* -------------------------------------------------------------------- */

        poDS = new MBTilesDataset();
        poDS->eAccess = poOpenInfo->eAccess;
        poDS->hDS = hDS;
        poDS->hDB = (sqlite3*) GDALGetInternalHandle( (GDALDatasetH)hDS, "SQLITE_HANDLE" );
        CPLAssert(poDS->hDB != nullptr);

        /* poDS will release it from now */
        hDS = nullptr;

        poDS->m_osClip = CSLFetchNameValueDef(
            poOpenInfo->papszOpenOptions, "CLIP", "");
        poDS->m_nMinZoomLevel = nMinLevel;
        bool bRasterOK =
            poDS->InitRaster ( nullptr, nMaxLevel, nBands, nTileSize,
                               dfMinX, dfMinY, dfMaxX, dfMaxY );

        const char* pszFormat = poDS->GetMetadataItem("format");
        if( pszFormat != nullptr && EQUAL(pszFormat, "pbf") )
        {
            if( (poOpenInfo->nOpenFlags & GDAL_OF_VECTOR) == 0 )
            {
                CPLDebug("MBTiles",
                         "This files contain vector tiles, "
                         "but driver open in raster-only mode");
                delete poDS;
                return nullptr;
            }
            poDS->InitVector(dfMinX, dfMinY, dfMaxX, dfMaxY,
                CPLFetchBool(poOpenInfo->papszOpenOptions, "ZOOM_LEVEL_AUTO",
                    CPLTestBool(CPLGetConfigOption(
                        "MVT_ZOOM_LEVEL_AUTO", "NO"))),
                CPLFetchBool(poOpenInfo->papszOpenOptions, "JSON_FIELD",
                             false)
            );
        }
        else if( (pszFormat != nullptr && !EQUAL(pszFormat, "pbf")) ||
                 bFoundRasterTile )
        {
            if( (poOpenInfo->nOpenFlags & GDAL_OF_RASTER) == 0 )
            {
                CPLDebug("MBTiles",
                         "This files contain raster tiles, "
                         "but driver open in vector-only mode");
                delete poDS;
                return nullptr;
            }
        }

        if( (pszFormat == nullptr || !EQUAL(pszFormat, "pbf")) &&
            !bRasterOK )
        {
            delete poDS;
            return nullptr;
        }

        if( poDS->eAccess == GA_Update )
        {
            if( pszFormat != nullptr && (EQUAL(pszFormat, "jpg") || EQUAL(pszFormat, "jpeg")) )
            {
                poDS->m_eTF = GPKG_TF_JPEG;
            }

            const char* pszTF = CSLFetchNameValue(poOpenInfo->papszOpenOptions, "TILE_FORMAT");
            if( pszTF )
            {
                poDS->m_eTF = GDALGPKGMBTilesGetTileFormat(pszTF);
                if( (pszFormat != nullptr && (EQUAL(pszFormat, "jpg") || EQUAL(pszFormat, "jpeg")) &&
                     poDS->m_eTF != GPKG_TF_JPEG) ||
                    (pszFormat != nullptr && EQUAL(pszFormat, "png") && poDS->m_eTF == GPKG_TF_JPEG) )
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "Format metadata = '%s', but TILE_FORMAT='%s'",
                             pszFormat, pszTF);
                }
            }

            poDS->ParseCompressionOptions(poOpenInfo->papszOpenOptions);
        }

/* -------------------------------------------------------------------- */
/*      Add overview levels as internal datasets                        */
/* -------------------------------------------------------------------- */
        for( int iLevel = nMaxLevel - 1; iLevel >= nMinLevel; iLevel-- )
        {
            MBTilesDataset* poOvrDS = new MBTilesDataset();
            poOvrDS->ShareLockWithParentDataset(poDS);
            poOvrDS->InitRaster ( poDS, iLevel, nBands, nTileSize,
                                  dfMinX, dfMinY, dfMaxX, dfMaxY );

            poDS->m_papoOverviewDS = (MBTilesDataset**) CPLRealloc(poDS->m_papoOverviewDS,
                            sizeof(MBTilesDataset*) * (poDS->m_nOverviewCount+1));
            poDS->m_papoOverviewDS[poDS->m_nOverviewCount ++] = poOvrDS;

            if( poOvrDS->GetRasterXSize() < 256 &&
                poOvrDS->GetRasterYSize() < 256 )
            {
                break;
            }
        }

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
        poDS->SetDescription( poOpenInfo->pszFilename );

        if ( !STARTS_WITH_CI(poOpenInfo->pszFilename, "/vsicurl/") )
            poDS->TryLoadXML();
        else
        {
            poDS->SetPamFlags(poDS->GetPamFlags() & ~GPF_DIRTY);
        }
    }

end:
    if (hDS)
        OGRReleaseDataSource(hDS);

    return poDS;
}

/************************************************************************/
/*                                Create()                              */
/************************************************************************/

GDALDataset* MBTilesDataset::Create( const char * pszFilename,
                                   int nXSize,
                                   int nYSize,
                                   int nBandsIn,
                                   GDALDataType eDT,
                                   char **papszOptions )
{
#ifdef HAVE_MVT_WRITE_SUPPORT
    if( nXSize == 0 && nYSize == 0 && nBandsIn == 0 && eDT == GDT_Unknown )
    {
        char** papszOptionsMod = CSLDuplicate(papszOptions);
        papszOptionsMod = CSLSetNameValue(papszOptionsMod,
                                          "FORMAT", "MBTILES");
        GDALDataset* poRet = OGRMVTWriterDatasetCreate(
                                         pszFilename, nXSize, nYSize,
                                         nBandsIn, eDT, papszOptionsMod);
        CSLDestroy(papszOptionsMod);
        return poRet;
    }
#endif

    MBTilesDataset* poDS = new MBTilesDataset();
    if( !poDS->CreateInternal(pszFilename, nXSize, nYSize, nBandsIn, eDT, papszOptions) )
    {
        delete poDS;
        poDS = nullptr;
    }
    return poDS;
}

/************************************************************************/
/*                            CreateInternal()                          */
/************************************************************************/

bool MBTilesDataset::CreateInternal( const char * pszFilename,
                                   int nXSize,
                                   int nYSize,
                                   int nBandsIn,
                                   GDALDataType eDT,
                                   char **papszOptions )
{
    if( eDT != GDT_Byte )
    {
        CPLError(CE_Failure, CPLE_NotSupported, "Only Byte supported");
        return false;
    }
    if( nBandsIn != 1 && nBandsIn != 2 && nBandsIn != 3 && nBandsIn != 4 )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                  "Only 1 (Grey/ColorTable), 2 (Grey+Alpha), 3 (RGB) or 4 (RGBA) band dataset supported");
        return false;
    }

    // for test/debug purposes only. true is the nominal value
    m_bPNGSupports2Bands = CPLTestBool(CPLGetConfigOption("MBTILES_PNG_SUPPORTS_2BANDS", "TRUE"));
    m_bPNGSupportsCT = CPLTestBool(CPLGetConfigOption("MBTILES_PNG_SUPPORTS_CT", "TRUE"));
    m_bWriteBounds = CPLFetchBool(const_cast<const char**>(papszOptions), "WRITE_BOUNDS", true);
    m_bWriteMinMaxZoom = CPLFetchBool(const_cast<const char**>(papszOptions), "WRITE_MINMAXZOOM", true);
    int nBlockSize = std::max(64,
        std::min(8192,atoi(CSLFetchNameValueDef(papszOptions,
                    "BLOCKSIZE", CPLSPrintf("%d", knDEFAULT_BLOCK_SIZE)))));
    m_osBounds = CSLFetchNameValueDef(papszOptions, "BOUNDS", "");
    m_osCenter = CSLFetchNameValueDef(papszOptions, "CENTER", "");

    VSIUnlink( pszFilename );
    SetDescription( pszFilename );

    int rc;
    if (STARTS_WITH(pszFilename, "/vsi"))
    {
        pMyVFS = OGRSQLiteCreateVFS(nullptr, nullptr);
        sqlite3_vfs_register(pMyVFS, 0);
        rc = sqlite3_open_v2( pszFilename, &hDB, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, pMyVFS->zName );
    }
    else
    {
        rc = sqlite3_open( pszFilename, &hDB );
    }

    if( rc != SQLITE_OK )
    {
        CPLError(CE_Failure, CPLE_FileIO, "Cannot create %s", pszFilename);
        return false;
    }

    sqlite3_exec( hDB, "PRAGMA synchronous = OFF", nullptr, nullptr, nullptr );

    rc = sqlite3_exec( hDB, "CREATE TABLE tiles ("
          "zoom_level INTEGER NOT NULL,"
          "tile_column INTEGER NOT NULL,"
          "tile_row INTEGER NOT NULL,"
          "tile_data BLOB NOT NULL,"
          "UNIQUE (zoom_level, tile_column, tile_row) )", nullptr, nullptr, nullptr );
    if( rc != SQLITE_OK )
    {
        CPLError(CE_Failure, CPLE_FileIO, "Cannot create tiles table");
        return false;
    }

    rc = sqlite3_exec( hDB, "CREATE TABLE metadata (name TEXT, value TEXT)", nullptr, nullptr, nullptr );
    if( rc != SQLITE_OK )
    {
        CPLError(CE_Failure, CPLE_FileIO, "Cannot create metadata table");
        return false;
    }

    const char* pszName = CSLFetchNameValueDef(papszOptions, "NAME",
                                               CPLGetBasename(pszFilename));
    char* pszSQL = sqlite3_mprintf(
        "INSERT INTO metadata (name, value) VALUES ('name', '%q')", pszName );
    sqlite3_exec( hDB, pszSQL, nullptr, nullptr, nullptr );
    sqlite3_free(pszSQL);

    const char* pszType = CSLFetchNameValueDef(papszOptions, "TYPE",
                                               "overlay");
    pszSQL = sqlite3_mprintf(
        "INSERT INTO metadata (name, value) VALUES ('type', '%q')", pszType );
    sqlite3_exec( hDB, pszSQL, nullptr, nullptr, nullptr );
    sqlite3_free(pszSQL);

    const char* pszDescription = CSLFetchNameValueDef(papszOptions, "DESCRIPTION",
                                               CPLGetBasename(pszFilename));
    pszSQL = sqlite3_mprintf(
        "INSERT INTO metadata (name, value) VALUES ('description', '%q')", pszDescription );
    sqlite3_exec( hDB, pszSQL, nullptr, nullptr, nullptr );
    sqlite3_free(pszSQL);

    const char* pszVersion = CSLFetchNameValueDef(papszOptions, "VERSION", "1.1");
    pszSQL = sqlite3_mprintf(
        "INSERT INTO metadata (name, value) VALUES ('version', '%q')", pszVersion );
    sqlite3_exec( hDB, pszSQL, nullptr, nullptr, nullptr );
    sqlite3_free(pszSQL);

    const char* pszTF = CSLFetchNameValue(papszOptions, "TILE_FORMAT");
    if( pszTF )
        m_eTF = GDALGPKGMBTilesGetTileFormat(pszTF);

    const char* pszFormat = CSLFetchNameValueDef(papszOptions, "FORMAT",
                                        (m_eTF == GPKG_TF_JPEG) ? "jpg" : "png" );
    pszSQL = sqlite3_mprintf(
        "INSERT INTO metadata (name, value) VALUES ('format', '%q')", pszFormat );
    sqlite3_exec( hDB, pszSQL, nullptr, nullptr, nullptr );
    sqlite3_free(pszSQL);

    m_bNew = true;
    eAccess = GA_Update;
    nRasterXSize = nXSize;
    nRasterYSize = nYSize;

    m_pabyCachedTiles = (GByte*) VSI_MALLOC3_VERBOSE(4 * 4, nBlockSize, nBlockSize);
    if( m_pabyCachedTiles == nullptr )
    {
        return false;
    }

    for(int i = 1; i <= nBandsIn; i ++)
        SetBand( i, new MBTilesBand(this, nBlockSize) );

    ParseCompressionOptions(papszOptions);

    return true;
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
    { "NEAREST", GRA_NearestNeighbour },
    { "BILINEAR", GRA_Bilinear },
    { "CUBIC", GRA_Cubic },
    { "CUBICSPLINE", GRA_CubicSpline },
    { "LANCZOS", GRA_Lanczos },
    { "MODE", GRA_Mode },
    { "AVERAGE", GRA_Average },
    { "RMS", GRA_RMS },
};

GDALDataset* MBTilesDataset::CreateCopy( const char *pszFilename,
                                         GDALDataset *poSrcDS,
                                         int /*bStrict*/,
                                         char ** papszOptions,
                                         GDALProgressFunc pfnProgress,
                                         void * pProgressData )
{

    int nBands = poSrcDS->GetRasterCount();
    if( nBands != 1 && nBands != 2 && nBands != 3 && nBands != 4 )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                    "Only 1 (Grey/ColorTable), 2 (Grey+Alpha), 3 (RGB) or 4 (RGBA) band dataset supported");
        return nullptr;
    }

    char** papszTO = CSLSetNameValue( nullptr, "DST_SRS", SRS_EPSG_3857 );

    void* hTransformArg = nullptr;

    // Hack to compensate for GDALSuggestedWarpOutput2() failure (or not
    // ideal suggestion with PROJ 8) when reprojecting latitude = +/- 90 to
    // EPSG:3857.
    double adfSrcGeoTransform[6] = {0,0,0,0,0,0};
    std::unique_ptr<GDALDataset> poTmpDS;
    bool bModifiedMaxLat = false;
    bool bModifiedMinLat = false;
    const auto poSrcSRS = poSrcDS->GetSpatialRef();
    if( poSrcDS->GetGeoTransform(adfSrcGeoTransform) == CE_None &&
        adfSrcGeoTransform[2] == 0 &&
        adfSrcGeoTransform[4] == 0 &&
        adfSrcGeoTransform[5] < 0 )
    {
        if( poSrcSRS && poSrcSRS->IsGeographic() )
        {
            double maxLat = adfSrcGeoTransform[3];
            double minLat = adfSrcGeoTransform[3] +
                                    poSrcDS->GetRasterYSize() *
                                    adfSrcGeoTransform[5];
            // Corresponds to the latitude of MAX_GM
            constexpr double MAX_LAT = 85.0511287798066;
            if( maxLat > MAX_LAT )
            {
                maxLat = MAX_LAT;
                bModifiedMaxLat = true;
            }
            if( minLat < -MAX_LAT )
            {
                minLat = -MAX_LAT;
                bModifiedMinLat = true;
            }
            if( bModifiedMaxLat || bModifiedMinLat )
            {
                CPLStringList aosOptions;
                aosOptions.AddString("-of");
                aosOptions.AddString("VRT");
                aosOptions.AddString("-projwin");
                aosOptions.AddString(CPLSPrintf("%.18g", adfSrcGeoTransform[0]));
                aosOptions.AddString(CPLSPrintf("%.18g", maxLat));
                aosOptions.AddString(CPLSPrintf("%.18g", adfSrcGeoTransform[0] + poSrcDS->GetRasterXSize() * adfSrcGeoTransform[1]));
                aosOptions.AddString(CPLSPrintf("%.18g", minLat));
                auto psOptions = GDALTranslateOptionsNew(aosOptions.List(), nullptr);
                poTmpDS.reset(GDALDataset::FromHandle(
                    GDALTranslate("", GDALDataset::ToHandle(poSrcDS), psOptions, nullptr)));
                GDALTranslateOptionsFree(psOptions);
                if( poTmpDS )
                {
                    hTransformArg = GDALCreateGenImgProjTransformer2(
                        GDALDataset::FromHandle(poTmpDS.get()), nullptr, papszTO );
                }
            }
        }
    }
    if( hTransformArg == nullptr )
    {
        hTransformArg =
            GDALCreateGenImgProjTransformer2( poSrcDS, nullptr, papszTO );
    }
    if( hTransformArg == nullptr )
    {
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
        CSLDestroy(papszTO);
        GDALDestroyGenImgProjTransformer( hTransformArg );
        return nullptr;
    }

    GDALDestroyGenImgProjTransformer( hTransformArg );
    hTransformArg = nullptr;
    poTmpDS.reset();

    if( bModifiedMaxLat || bModifiedMinLat )
    {
        if( bModifiedMaxLat )
        {
            const double maxNorthing = MAX_GM;
            adfGeoTransform[3] = maxNorthing;
            adfExtent[3] = maxNorthing;
        }
        if( bModifiedMinLat )
        {
            const double minNorthing = -MAX_GM;
            adfExtent[1] = minNorthing;
        }

        if( poSrcSRS && poSrcSRS->IsGeographic() )
        {
            if( adfSrcGeoTransform[0] + poSrcDS->GetRasterXSize() * adfSrcGeoTransform[1] == 180 )
            {
                adfExtent[2] = MAX_GM;
            }
        }
    }

    int nZoomLevel;
    double dfComputedRes = adfGeoTransform[1];
    double dfPrevRes = 0.0;
    double dfRes = 0.0;
    int nBlockSize = std::max(64,
        std::min(8192,atoi(CSLFetchNameValueDef(papszOptions,
                    "BLOCKSIZE", CPLSPrintf("%d", knDEFAULT_BLOCK_SIZE)))));
    const double dfPixelXSizeZoomLevel0 = 2 * MAX_GM / nBlockSize;
    for(nZoomLevel = 0; nZoomLevel < 25; nZoomLevel++)
    {
        dfRes = dfPixelXSizeZoomLevel0 / (1 << nZoomLevel);
        if( dfComputedRes > dfRes )
            break;
        dfPrevRes = dfRes;
    }
    if( nZoomLevel == 25 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Could not find an appropriate zoom level");
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

    dfRes = dfPixelXSizeZoomLevel0 / (1 << nZoomLevel);

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
            atoi(oSrcSRS.GetAuthorityCode(nullptr)) != 3857 )
        {
            nTargetBands ++;
        }
    }

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

    if( nBands == 1 && poSrcDS->GetRasterBand(1)->GetColorTable() != nullptr &&
        eResampleAlg != GRA_NearestNeighbour && eResampleAlg != GRA_Mode )
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "Input dataset has a color table, which will likely lead to "
                 "bad results when using a resampling method other than "
                 "nearest neighbour or mode. Converting the dataset to 24/32 bit "
                 "(e.g. with gdal_translate -expand rgb/rgba) is advised.");
    }

    GDALDataset* poDS = Create( pszFilename, nXSize, nYSize, nTargetBands, GDT_Byte,
                                   papszOptions );
    if( poDS == nullptr )
    {
        CSLDestroy(papszTO);
        return nullptr;
    }
    poDS->SetGeoTransform(adfGeoTransform);
    if( nTargetBands == 1 && nBands == 1 && poSrcDS->GetRasterBand(1)->GetColorTable() != nullptr )
    {
        poDS->GetRasterBand(1)->SetColorTable( poSrcDS->GetRasterBand(1)->GetColorTable() );
    }

    hTransformArg =
        GDALCreateGenImgProjTransformer2( poSrcDS, poDS, papszTO );
    CSLDestroy(papszTO);
    if( hTransformArg == nullptr )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "GDALCreateGenImgProjTransformer2 failed");
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

    psWO->papszWarpOptions = CSLSetNameValue(nullptr, "OPTIMIZE_SIZE", "YES");
    psWO->eWorkingDataType = GDT_Byte;

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
/*                        ParseCompressionOptions()                     */
/************************************************************************/

void MBTilesDataset::ParseCompressionOptions(char** papszOptions)
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

CPLErr MBTilesDataset::IBuildOverviews(
                        const char * pszResampling,
                        int nOverviews, int * panOverviewList,
                        int nBandsIn, int * /*panBandList*/,
                        GDALProgressFunc pfnProgress, void * pProgressData )
{
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

    if( nOverviews == 0 )
    {
        for(int i=0;i<m_nOverviewCount;i++)
            m_papoOverviewDS[i]->FlushCache(false);
        char* pszSQL = sqlite3_mprintf("DELETE FROM 'tiles' WHERE zoom_level < %d",
                                       m_nZoomLevel);
        char* pszErrMsg = nullptr;
        int ret = sqlite3_exec(hDB, pszSQL, nullptr, nullptr, &pszErrMsg);
        sqlite3_free(pszSQL);
        if( ret != SQLITE_OK )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Failure: %s",
                     pszErrMsg ? pszErrMsg : "");
            sqlite3_free(pszErrMsg);
            return CE_Failure;
        }

        int nRows = 0;
        int nCols = 0;
        char** papszResult = nullptr;
        sqlite3_get_table(hDB, "SELECT * FROM metadata WHERE name = 'minzoom'", &papszResult, &nRows, &nCols, nullptr);
        sqlite3_free_table(papszResult);
        if( nRows == 1 )
        {
            sqlite3_exec(hDB, "DELETE FROM metadata WHERE name = 'minzoom'", nullptr, nullptr, nullptr);
            pszSQL = sqlite3_mprintf(
                "INSERT INTO metadata (name, value) VALUES ('minzoom', '%d')", m_nZoomLevel );
            sqlite3_exec( hDB, pszSQL, nullptr, nullptr, nullptr );
            sqlite3_free(pszSQL);
        }

        return CE_None;
    }

    if( nBandsIn != nBands )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Generation of overviews only"
                  "supported when operating on all bands." );
        return CE_Failure;
    }

    if( m_nOverviewCount == 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Image too small to support overviews");
        return CE_Failure;
    }

    FlushCache(false);
    for(int i=0;i<nOverviews;i++)
    {
        if( panOverviewList[i] < 2 )
        {
            CPLError(CE_Failure, CPLE_IllegalArg, "Overview factor '%d' must be >= 2",
                     panOverviewList[i]);
            return CE_Failure;
        }

        if( GetFloorPowerOfTwo( panOverviewList[i] ) != panOverviewList[i] )
        {
            CPLError(CE_Failure, CPLE_IllegalArg, "Overview factor '%d' is not a power of 2",
                     panOverviewList[i]);
            return CE_Failure;
        }
    }

    GDALRasterBand*** papapoOverviewBands = (GDALRasterBand ***) CPLCalloc(sizeof(void*),nBands);
    int iCurOverview = 0;
    int nMinZoom = m_nZoomLevel;
    for( int i = 0; i < m_nOverviewCount; i++ )
    {
        MBTilesDataset* poODS = m_papoOverviewDS[i];
        if( poODS->m_nZoomLevel < nMinZoom )
            nMinZoom = poODS->m_nZoomLevel;
    }
    for( int iBand = 0; iBand < nBands; iBand++ )
    {
        papapoOverviewBands[iBand] = (GDALRasterBand **) CPLCalloc(sizeof(void*),nOverviews);
        iCurOverview = 0;
        for(int i=0;i<nOverviews;i++)
        {
            int nVal = panOverviewList[i];
            int iOvr = -1;
            while( nVal > 1 )
            {
                nVal >>= 1;
                iOvr ++;
            }
            if( iOvr >= m_nOverviewCount )
            {
                continue;
            }
            MBTilesDataset* poODS = m_papoOverviewDS[iOvr];
            papapoOverviewBands[iBand][iCurOverview] = poODS->GetRasterBand(iBand+1);
            iCurOverview++ ;
        }
    }

    CPLErr eErr = GDALRegenerateOverviewsMultiBand(nBands, papoBands,
                                     iCurOverview, papapoOverviewBands,
                                     pszResampling, pfnProgress, pProgressData );

    for( int iBand = 0; iBand < nBands; iBand++ )
    {
        CPLFree(papapoOverviewBands[iBand]);
    }
    CPLFree(papapoOverviewBands);

    if( eErr == CE_None )
    {
        int nRows = 0;
        int nCols = 0;
        char** papszResult = nullptr;
        sqlite3_get_table(hDB, "SELECT * FROM metadata WHERE name = 'minzoom' LIMIT 2", &papszResult, &nRows, &nCols, nullptr);
        sqlite3_free_table(papszResult);
        if( nRows == 1 )
        {
            sqlite3_exec(hDB, "DELETE FROM metadata WHERE name = 'minzoom'", nullptr, nullptr, nullptr);
            char* pszSQL = sqlite3_mprintf(
                "INSERT INTO metadata (name, value) VALUES ('minzoom', '%d')", nMinZoom );
            sqlite3_exec( hDB, pszSQL, nullptr, nullptr, nullptr );
            sqlite3_free(pszSQL);
        }
    }

    return eErr;
}

/************************************************************************/
/*                       GDALRegister_MBTiles()                         */
/************************************************************************/

void GDALRegister_MBTiles()

{
    if( !GDAL_CHECK_VERSION( "MBTiles driver" ) )
        return;

    if( GDALGetDriverByName( "MBTiles" ) != nullptr )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "MBTiles" );
    poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
    poDriver->SetMetadataItem( GDAL_DCAP_VECTOR, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, "MBTiles" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "drivers/raster/mbtiles.html" );
    poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "mbtiles" );
    poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES, "Byte" );

#define COMPRESSION_OPTIONS \
"  <Option name='TILE_FORMAT' scope='raster' type='string-select' description='Format to use to create tiles' default='PNG'>" \
"    <Value>PNG</Value>" \
"    <Value>PNG8</Value>" \
"    <Value>JPEG</Value>" \
"  </Option>" \
"  <Option name='QUALITY' scope='raster' type='int' min='1' max='100' description='Quality for JPEG tiles' default='75'/>" \
"  <Option name='ZLEVEL' scope='raster' type='int' min='1' max='9' description='DEFLATE compression level for PNG tiles' default='6'/>" \
"  <Option name='DITHER' scope='raster' type='boolean' description='Whether to apply Floyd-Steinberg dithering (for TILE_FORMAT=PNG8)' default='NO'/>" \

    poDriver->SetMetadataItem( GDAL_DMD_OPENOPTIONLIST, "<OpenOptionList>"
"  <Option name='ZOOM_LEVEL' scope='raster,vector' type='integer' description='Zoom level of full resolution. If not specified, maximum non-empty zoom level'/>"
"  <Option name='BAND_COUNT' scope='raster' type='string-select' description='Number of raster bands' default='AUTO'>"
"    <Value>AUTO</Value>"
"    <Value>1</Value>"
"    <Value>2</Value>"
"    <Value>3</Value>"
"    <Value>4</Value>"
"  </Option>"
"  <Option name='MINX' scope='raster,vector' type='float' description='Minimum X of area of interest'/>"
"  <Option name='MINY' scope='raster,vector' type='float' description='Minimum Y of area of interest'/>"
"  <Option name='MAXX' scope='raster,vector' type='float' description='Maximum X of area of interest'/>"
"  <Option name='MAXY' scope='raster,vector' type='float' description='Maximum Y of area of interest'/>"
"  <Option name='USE_BOUNDS' scope='raster,vector' type='boolean' description='Whether to use the bounds metadata, when available, to determine the AOI' default='YES'/>"
COMPRESSION_OPTIONS
"  <Option name='CLIP' scope='vector' type='boolean' "
    "description='Whether to clip geometries to tile extent' default='YES'/>"
"  <Option name='ZOOM_LEVEL_AUTO' scope='vector' type='boolean' "
    "description='Whether to auto-select the zoom level for vector layers "
    "according to spatial filter extent. Only for display purpose' "
    "default='NO'/>"
"  <Option name='JSON_FIELD' scope='vector' type='string' description='For vector layers, "
        "whether to put all attributes as a serialized JSon dictionary'/>"
"</OpenOptionList>");

    poDriver->SetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST, "<CreationOptionList>"
"  <Option name='NAME' scope='raster,vector' type='string' description='Tileset name'/>"
"  <Option name='DESCRIPTION' scope='raster,vector' type='string' description='A description of the layer'/>"
"  <Option name='TYPE' scope='raster,vector' type='string-select' description='Layer type' default='overlay'>"
"    <Value>overlay</Value>"
"    <Value>baselayer</Value>"
"  </Option>"
"  <Option name='VERSION' scope='raster' type='string' description='The version of the tileset, as a plain number' default='1.1'/>"
"  <Option name='BLOCKSIZE' scope='raster' type='int' description='Block size in pixels' default='256' min='64' max='8192'/>"
COMPRESSION_OPTIONS
"  <Option name='ZOOM_LEVEL_STRATEGY' scope='raster' type='string-select' description='Strategy to determine zoom level.' default='AUTO'>"
"    <Value>AUTO</Value>"
"    <Value>LOWER</Value>"
"    <Value>UPPER</Value>"
"  </Option>"
"  <Option name='RESAMPLING' scope='raster' type='string-select' description='Resampling algorithm.' default='BILINEAR'>"
"    <Value>NEAREST</Value>"
"    <Value>BILINEAR</Value>"
"    <Value>CUBIC</Value>"
"    <Value>CUBICSPLINE</Value>"
"    <Value>LANCZOS</Value>"
"    <Value>MODE</Value>"
"    <Value>AVERAGE</Value>"
"  </Option>"
"  <Option name='WRITE_BOUNDS' scope='raster' type='boolean' description='Whether to write the bounds metadata' default='YES'/>"
"  <Option name='WRITE_MINMAXZOOM' scope='raster' type='boolean' description='Whether to write the minzoom and maxzoom metadata' default='YES'/>"
"  <Option name='BOUNDS' scope='raster,vector' type='string' "
        "description='Override default value for bounds metadata item'/>"
"  <Option name='CENTER' scope='raster,vector' type='string' "
        "description='Override default value for center metadata item'/>"
#ifdef HAVE_MVT_WRITE_SUPPORT
MVT_MBTILES_COMMON_DSCO
#endif
"</CreationOptionList>");
    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

#ifdef HAVE_MVT_WRITE_SUPPORT
    poDriver->SetMetadataItem( GDAL_DMD_CREATIONFIELDDATATYPES,
                               "Integer Integer64 Real String" );
    poDriver->SetMetadataItem( GDAL_DMD_CREATIONFIELDDATASUBTYPES,
                               "Boolean Float32" );

    poDriver->SetMetadataItem( GDAL_DS_LAYER_CREATIONOPTIONLIST, MVT_LCO);
#endif

#ifdef ENABLE_SQL_SQLITE_FORMAT
    poDriver->SetMetadataItem("ENABLE_SQL_SQLITE_FORMAT", "YES");
#endif

    poDriver->pfnOpen = MBTilesDataset::Open;
    poDriver->pfnIdentify = MBTilesDataset::Identify;
    poDriver->pfnCreateCopy = MBTilesDataset::CreateCopy;
    poDriver->pfnCreate = MBTilesDataset::Create;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
