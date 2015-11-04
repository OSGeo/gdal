/******************************************************************************
 * $Id$
 *
 * Project:  GDAL MBTiles driver
 * Purpose:  Implement GDAL MBTiles support using OGR SQLite driver
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 **********************************************************************
 * Copyright (c) 2012-2013, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "gdal_frmts.h"
#include "gdal_pam.h"
#include "ogr_api.h"
#include "cpl_vsil_curl_priv.h"

#include "zlib.h"
#include "json.h"

#include <math.h>

extern "C" void GDALRegister_MBTiles();

CPL_CVSID("$Id$");

static const char * const apszAllowedDrivers[] = {"JPEG", "PNG", NULL};

class MBTilesBand;

/************************************************************************/
/*                         MBTILESOpenSQLiteDB()                        */
/************************************************************************/

static OGRDataSourceH MBTILESOpenSQLiteDB(const char* pszFilename,
                                      GDALAccess eAccess)
{
    const char* apszAllowedDrivers[] = { "SQLITE", NULL };
    return (OGRDataSourceH)GDALOpenEx(pszFilename,
                                      GDAL_OF_VECTOR |
                                      ((eAccess == GA_Update) ? GDAL_OF_UPDATE : 0),
                                      apszAllowedDrivers, NULL, NULL);
}

/************************************************************************/
/* ==================================================================== */
/*                              MBTilesDataset                          */
/* ==================================================================== */
/************************************************************************/

class MBTilesDataset : public GDALPamDataset
{
    friend class MBTilesBand;

  public:
                 MBTilesDataset();
                 MBTilesDataset(MBTilesDataset* poMainDS, int nLevel);

    virtual     ~MBTilesDataset();

    virtual CPLErr GetGeoTransform(double* padfGeoTransform);
    virtual const char* GetProjectionRef();
    
    virtual char      **GetMetadataDomainList();
    virtual char      **GetMetadata( const char * pszDomain = "" );

    static GDALDataset *Open( GDALOpenInfo * );
    static int          Identify( GDALOpenInfo * );

    char*               FindKey(int iPixel, int iLine,
                                int& nTileColumn, int& nTileRow, int& nZoomLevel);
    void                ComputeTileColTileRowZoomLevel( int nBlockXOff,
                                                        int nBlockYOff,
                                                        int &nTileColumn,
                                                        int &nTileRow,
                                                        int &nZoomLevel );
    int                 HasNonEmptyGrids();

  protected:
    virtual int         CloseDependentDatasets();

  private:

    int bMustFree;
    MBTilesDataset* poMainDS;
    int nLevel;
    int nMinTileCol, nMinTileRow;
    int nMinLevel;

    char** papszMetadata;
    char** papszImageStructure;

    int nResolutions;
    MBTilesDataset** papoOverviews;

    OGRDataSourceH hDS;

    int bFetchedMetadata;
    CPLStringList aosList;

    int bHasNonEmptyGrids;
};

/************************************************************************/
/* ==================================================================== */
/*                              MBTilesBand                             */
/* ==================================================================== */
/************************************************************************/

class MBTilesBand: public GDALPamRasterBand
{
    friend class MBTilesDataset;

    CPLString               osLocationInfo;

  public:
                            MBTilesBand( MBTilesDataset* poDS, int nBand,
                                            GDALDataType eDataType,
                                            int nBlockXSize, int nBlockYSize);

    virtual GDALColorInterp GetColorInterpretation();

    virtual int             GetOverviewCount();
    virtual GDALRasterBand* GetOverview(int nLevel);

    virtual CPLErr          IReadBlock( int, int, void * );

    virtual char      **GetMetadataDomainList();
    virtual const char *GetMetadataItem( const char * pszName,
                                         const char * pszDomain = "" );
};

/************************************************************************/
/*                            MBTilesBand()                          */
/************************************************************************/

MBTilesBand::MBTilesBand(MBTilesDataset* poDS, int nBand,
                                GDALDataType eDataType,
                                int nBlockXSize, int nBlockYSize)
{
    this->poDS = poDS;
    this->nBand = nBand;
    this->eDataType = eDataType;
    this->nBlockXSize = nBlockXSize;
    this->nBlockYSize = nBlockYSize;
}

/************************************************************************/
/*                            IReadBlock()                              */
/************************************************************************/

CPLErr MBTilesBand::IReadBlock( int nBlockXOff, int nBlockYOff, void * pImage)
{
    MBTilesDataset* poGDS = (MBTilesDataset*) poDS;

    int bGotTile = FALSE;
    CPLAssert(eDataType == GDT_Byte);

    int nTileColumn, nTileRow, nZoomLevel;
    poGDS->ComputeTileColTileRowZoomLevel(nBlockXOff, nBlockYOff,
                                          nTileColumn, nTileRow, nZoomLevel);

    const char* pszSQL = CPLSPrintf("SELECT tile_data FROM tiles WHERE "
                                    "tile_column = %d AND tile_row = %d AND zoom_level=%d",
                                    nTileColumn, nTileRow, nZoomLevel);
    CPLDebug("MBTILES", "nBand=%d, nBlockXOff=%d, nBlockYOff=%d, %s",
             nBand, nBlockXOff, nBlockYOff, pszSQL);
    OGRLayerH hSQLLyr = OGR_DS_ExecuteSQL(poGDS->hDS, pszSQL, NULL, NULL);

    OGRFeatureH hFeat = hSQLLyr ? OGR_L_GetNextFeature(hSQLLyr) : NULL;
    CPLErr eErr = CE_None;
    if (hFeat != NULL)
    {
        CPLString osMemFileName;
        osMemFileName.Printf("/vsimem/%p", this);

        int nDataSize = 0;
        GByte* pabyData = OGR_F_GetFieldAsBinary(hFeat, 0, &nDataSize);

        VSILFILE * fp = VSIFileFromMemBuffer( osMemFileName.c_str(), pabyData,
                                            nDataSize, FALSE);
        VSIFCloseL(fp);

        GDALDatasetH hDSTile = GDALOpenEx(osMemFileName.c_str(),
                                          GDAL_OF_RASTER | GDAL_OF_INTERNAL,
                                          apszAllowedDrivers, NULL, NULL);
        if (hDSTile != NULL)
        {
            int nTileBands = GDALGetRasterCount(hDSTile);
            if (nTileBands == 4 && poGDS->nBands == 3)
                nTileBands = 3;

            if (GDALGetRasterXSize(hDSTile) == nBlockXSize &&
                GDALGetRasterYSize(hDSTile) == nBlockYSize &&
                (nTileBands == poGDS->nBands ||
                 (nTileBands == 1 && (poGDS->nBands == 3 || poGDS->nBands == 4)) ||
                 (nTileBands == 3 && poGDS->nBands == 4)))
            {
                int iBand;
                void* pSrcImage = NULL;
                GByte abyTranslation[256][4];

                bGotTile = TRUE;

                GDALColorTableH hCT = GDALGetRasterColorTable(GDALGetRasterBand(hDSTile, 1));
                if (nTileBands == 1 && (poGDS->nBands == 3 || poGDS->nBands == 4))
                {
                    if (hCT != NULL)
                        pSrcImage = CPLMalloc(nBlockXSize * nBlockYSize);
                    iBand = 1;
                }
                else
                    iBand = nBand;

                if (nTileBands == 3 && poGDS->nBands == 4 && iBand == 4)
                    memset(pImage, 255, nBlockXSize * nBlockYSize);
                else
                {
                    eErr = GDALRasterIO(GDALGetRasterBand(hDSTile, iBand), GF_Read,
                                0, 0, nBlockXSize, nBlockYSize,
                                pImage, nBlockXSize, nBlockYSize, eDataType, 0, 0);
                }

                if (pSrcImage != NULL && hCT != NULL)
                {
                    int i;
                    memcpy(pSrcImage, pImage, nBlockXSize * nBlockYSize);

                    int nEntryCount = GDALGetColorEntryCount( hCT );
                    if (nEntryCount > 256)
                        nEntryCount = 256;
                    for(i = 0; i < nEntryCount; i++)
                    {
                        const GDALColorEntry* psEntry = GDALGetColorEntry( hCT, i );
                        abyTranslation[i][0] = (GByte) psEntry->c1;
                        abyTranslation[i][1] = (GByte) psEntry->c2;
                        abyTranslation[i][2] = (GByte) psEntry->c3;
                        abyTranslation[i][3] = (GByte) psEntry->c4;
                    }
                    for(; i < 256; i++)
                    {
                        abyTranslation[i][0] = 0;
                        abyTranslation[i][1] = 0;
                        abyTranslation[i][2] = 0;
                        abyTranslation[i][3] = 0;
                    }

                    for(i = 0; i < nBlockXSize * nBlockYSize; i++)
                    {
                        ((GByte*)pImage)[i] = abyTranslation[((GByte*)pSrcImage)[i]][nBand-1];
                    }
                }

                for(int iOtherBand=1;iOtherBand<=poGDS->nBands && eErr == CE_None;iOtherBand++)
                {
                    GDALRasterBlock *poBlock;

                    if (iOtherBand == nBand)
                        continue;

                    poBlock = ((MBTilesBand*)poGDS->GetRasterBand(iOtherBand))->
                        TryGetLockedBlockRef(nBlockXOff,nBlockYOff);

                    if (poBlock != NULL)
                    {
                        poBlock->DropLock();
                        continue;
                    }

                    poBlock = poGDS->GetRasterBand(iOtherBand)->
                        GetLockedBlockRef(nBlockXOff,nBlockYOff, TRUE);
                    if (poBlock == NULL)
                        break;

                    GByte* pabySrcBlock = (GByte *) poBlock->GetDataRef();
                    if( pabySrcBlock == NULL )
                    {
                        poBlock->DropLock();
                        break;
                    }

                    if (nTileBands == 3 && poGDS->nBands == 4 && iOtherBand == 4)
                        memset(pabySrcBlock, 255, nBlockXSize * nBlockYSize);
                    else if (nTileBands == 1 && (poGDS->nBands == 3 || poGDS->nBands == 4))
                    {
                        int i;
                        if (pSrcImage)
                        {
                            for(i = 0; i < nBlockXSize * nBlockYSize; i++)
                            {
                                ((GByte*)pabySrcBlock)[i] =
                                    abyTranslation[((GByte*)pSrcImage)[i]][iOtherBand-1];
                            }
                        }
                        else
                            memcpy(pabySrcBlock, pImage, nBlockXSize * nBlockYSize);
                    }
                    else
                    {
                        eErr = GDALRasterIO(GDALGetRasterBand(hDSTile, iOtherBand), GF_Read,
                            0, 0, nBlockXSize, nBlockYSize,
                            pabySrcBlock, nBlockXSize, nBlockYSize, eDataType, 0, 0);
                    }

                    poBlock->DropLock();
                }

                CPLFree(pSrcImage);
            }
            else if (GDALGetRasterXSize(hDSTile) == nBlockXSize &&
                     GDALGetRasterYSize(hDSTile) == nBlockYSize &&
                     (nTileBands == 3 && poGDS->nBands == 1))
            {
                bGotTile = TRUE;

                GByte* pabyRGBImage = (GByte*)CPLMalloc(3 * nBlockXSize * nBlockYSize);
                eErr = GDALDatasetRasterIO(hDSTile, GF_Read,
                                    0, 0, nBlockXSize, nBlockYSize,
                                    pabyRGBImage, nBlockXSize, nBlockYSize, eDataType,
                                    3, NULL, 3, 3 * nBlockXSize, 1);
                for(int i=0;i<nBlockXSize*nBlockYSize;i++)
                {
                    int R = pabyRGBImage[3*i];
                    int G = pabyRGBImage[3*i+1];
                    int B = pabyRGBImage[3*i+2];
                    GByte Y = (GByte)((213 * R + 715 * G + 72 * B) / 1000);
                    ((GByte*)pImage)[i] = Y;
                }
                CPLFree(pabyRGBImage);
            }
            else
            {
                CPLDebug("MBTILES", "tile size = %d, tile height = %d, tile bands = %d",
                         GDALGetRasterXSize(hDSTile), GDALGetRasterYSize(hDSTile),
                         GDALGetRasterCount(hDSTile));
            }
            GDALClose(hDSTile);
        }

        VSIUnlink( osMemFileName.c_str() );

        OGR_F_Destroy(hFeat);
    }

    OGR_DS_ReleaseResultSet(poGDS->hDS, hSQLLyr);

    if (!bGotTile)
    {
        memset(pImage, (nBand == 4) ? 0 : 255, nBlockXSize * nBlockYSize);

        for(int iOtherBand=1;iOtherBand<=poGDS->nBands;iOtherBand++)
        {
            GDALRasterBlock *poBlock;

            if (iOtherBand == nBand)
                continue;

            poBlock = poGDS->GetRasterBand(iOtherBand)->
                GetLockedBlockRef(nBlockXOff,nBlockYOff, TRUE);
            if (poBlock == NULL)
                break;

            GByte* pabySrcBlock = (GByte *) poBlock->GetDataRef();
            if( pabySrcBlock == NULL )
            {
                poBlock->DropLock();
                break;
            }

            memset(pabySrcBlock, (iOtherBand == 4) ? 0 : 255,
                   nBlockXSize * nBlockYSize);

            poBlock->DropLock();
        }
    }

    return eErr;
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
#if STRICT_RFC3629
  } else if (c == 0xed) {
    // RFC 3629 says surrogate chars are illegal.
    if (((unsigned char*)p)[1] >= 0xa0) goto FAIL;
    goto UTF8_3;
  } else if (c == 0xef) {
    // 0xfffe and 0xffff are also illegal characters
    if (((unsigned char*)p)[1]==0xbf &&
    ((unsigned char*)p)[2]>=0xbe) goto FAIL;
    goto UTF8_3;
#endif
  } else if (c < 0xf0) {
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
/*                  ComputeTileColTileRowZoomLevel()                    */
/************************************************************************/

void MBTilesDataset::ComputeTileColTileRowZoomLevel(int nBlockXOff,
                                                    int nBlockYOff,
                                                    int &nTileColumn,
                                                    int &nTileRow,
                                                    int &nZoomLevel)
{
    const int nBlockYSize = 256;

    int _nMinLevel = (poMainDS) ? poMainDS->nMinLevel : nMinLevel;
    int _nMinTileCol = (poMainDS) ? poMainDS->nMinTileCol : nMinTileCol;
    int _nMinTileRow = (poMainDS) ? poMainDS->nMinTileRow : nMinTileRow;
    _nMinTileCol >>= nLevel;

    nTileColumn = nBlockXOff + _nMinTileCol;
    nTileRow = (((nRasterYSize / nBlockYSize - 1 - nBlockYOff) << nLevel) + _nMinTileRow) >> nLevel;
    nZoomLevel = ((poMainDS) ? poMainDS->nResolutions : nResolutions) - nLevel + _nMinLevel;
}

/************************************************************************/
/*                          HasNonEmptyGrids()                          */
/************************************************************************/

int MBTilesDataset::HasNonEmptyGrids()
{
    OGRLayerH hSQLLyr;
    OGRFeatureH hFeat;
    const char* pszSQL;

    if (poMainDS)
        return poMainDS->HasNonEmptyGrids();

    if (bHasNonEmptyGrids >= 0)
        return bHasNonEmptyGrids;

    bHasNonEmptyGrids = FALSE;

    if (OGR_DS_GetLayerByName(hDS, "grids") == NULL)
        return FALSE;

    pszSQL = "SELECT type FROM sqlite_master WHERE name = 'grids'";
    CPLDebug("MBTILES", "%s", pszSQL);
    hSQLLyr = OGR_DS_ExecuteSQL(hDS, pszSQL, NULL, NULL);
    if (hSQLLyr == NULL)
        return FALSE;

    hFeat = OGR_L_GetNextFeature(hSQLLyr);
    if (hFeat == NULL || !OGR_F_IsFieldSet(hFeat, 0))
    {
        OGR_F_Destroy(hFeat);
        OGR_DS_ReleaseResultSet(hDS, hSQLLyr);
        return FALSE;
    }

    int bGridsIsView = strcmp(OGR_F_GetFieldAsString(hFeat, 0), "view") == 0;

    OGR_F_Destroy(hFeat);
    OGR_DS_ReleaseResultSet(hDS, hSQLLyr);

    bHasNonEmptyGrids = TRUE;

    /* In the case 'grids' is a view (and a join between the 'map' and 'grid_utfgrid' layers */
    /* the cost of evaluating a join is very long, even if grid_utfgrid is empty */
    /* so check it is not empty */
    if (bGridsIsView)
    {
        OGRLayerH hGridUTFGridLyr;
        hGridUTFGridLyr = OGR_DS_GetLayerByName(hDS, "grid_utfgrid");
        if (hGridUTFGridLyr != NULL)
        {
            OGR_L_ResetReading(hGridUTFGridLyr);
            hFeat = OGR_L_GetNextFeature(hGridUTFGridLyr);
            OGR_F_Destroy(hFeat);

            bHasNonEmptyGrids = hFeat != NULL;
        }
    }

    return bHasNonEmptyGrids;
}

/************************************************************************/
/*                             FindKey()                                */
/************************************************************************/

char* MBTilesDataset::FindKey(int iPixel, int iLine,
                              int& nTileColumn, int& nTileRow, int& nZoomLevel)
{
    const int nBlockXSize = 256, nBlockYSize = 256;
    int nBlockXOff = iPixel / nBlockXSize;
    int nBlockYOff = iLine / nBlockYSize;

    int nColInBlock = iPixel % nBlockXSize;
    int nRowInBlock = iLine % nBlockXSize;

    ComputeTileColTileRowZoomLevel(nBlockXOff, nBlockYOff,
                                   nTileColumn, nTileRow, nZoomLevel);

    char* pszKey = NULL;

    OGRLayerH hSQLLyr;
    OGRFeatureH hFeat;
    const char* pszSQL;
    json_object* poGrid = NULL;
    int i;

    /* See https://github.com/mapbox/utfgrid-spec/blob/master/1.0/utfgrid.md */
    /* for the explanation of the following processings */

    pszSQL = CPLSPrintf("SELECT grid FROM grids WHERE "
                        "zoom_level = %d AND tile_column = %d AND tile_row = %d",
                        nZoomLevel, nTileColumn, nTileRow);
    CPLDebug("MBTILES", "%s", pszSQL);
    hSQLLyr = OGR_DS_ExecuteSQL(hDS, pszSQL, NULL, NULL);
    if (hSQLLyr == NULL)
        return NULL;

    hFeat = OGR_L_GetNextFeature(hSQLLyr);
    if (hFeat == NULL || !OGR_F_IsFieldSet(hFeat, 0))
    {
        OGR_F_Destroy(hFeat);
        OGR_DS_ReleaseResultSet(hDS, hSQLLyr);
        return NULL;
    }

    int nDataSize = 0;
    GByte* pabyData = OGR_F_GetFieldAsBinary(hFeat, 0, &nDataSize);

    int nUncompressedSize = 256*256;
    GByte* pabyUncompressed = (GByte*)CPLMalloc(nUncompressedSize + 1);

    z_stream sStream;
    memset(&sStream, 0, sizeof(sStream));
    inflateInit(&sStream);
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

    struct json_tokener *jstok = NULL;
    json_object* jsobj = NULL;

    if (nUncompressedSize == 0)
    {
        goto end;
    }

    jstok = json_tokener_new();
    jsobj = json_tokener_parse_ex(jstok, (const char*)pabyUncompressed, -1);
    if( jstok->err != json_tokener_success)
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                    "JSON parsing error: %s (at offset %d)",
                    json_tokener_error_desc(jstok->err),
                    jstok->char_offset);
        json_tokener_free(jstok);

        goto end;
    }

    json_tokener_free(jstok);

    if (json_object_is_type(jsobj, json_type_object))
    {
        poGrid = json_object_object_get(jsobj, "grid");
    }
    if (poGrid != NULL && json_object_is_type(poGrid, json_type_array))
    {
        int nLines;
        int nFactor;
        json_object* poRow;
        char* pszRow = NULL;

        nLines = json_object_array_length(poGrid);
        if (nLines == 0)
            goto end;

        nFactor = 256 / nLines;
        nRowInBlock /= nFactor;
        nColInBlock /= nFactor;

        poRow = json_object_array_get_idx(poGrid, nRowInBlock);

        /* Extract line of interest in grid */
        if (poRow != NULL && json_object_is_type(poRow, json_type_string))
        {
            pszRow = CPLStrdup(json_object_get_string(poRow));
        }

        if (pszRow == NULL)
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

                if (iCol == nColInBlock)
                {
                    nKey = (int)res;
                    //CPLDebug("MBTILES", "Key index = %d", nKey);
                    break;
                }
                i += len;
                iCol ++;
            }

            /* Find key */
            json_object* poKeys = json_object_object_get(jsobj, "keys");
            if (nKey >= 0 && poKeys != NULL &&
                json_object_is_type(poKeys, json_type_array) &&
                nKey < json_object_array_length(poKeys))
            {
                json_object* poKey = json_object_array_get_idx(poKeys, nKey);
                if (poKey != NULL && json_object_is_type(poKey, json_type_string))
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
    if (pabyUncompressed)
        CPLFree(pabyUncompressed);
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
    if( pszDomain != NULL
        && EQUAL(pszDomain,"LocationInfo")
        && (STARTS_WITH_CI(pszName, "Pixel_") || STARTS_WITH_CI(pszName, "GeoPixel_")) )
    {
        int iPixel, iLine;

        if (!poGDS->HasNonEmptyGrids())
            return NULL;

/* -------------------------------------------------------------------- */
/*      What pixel are we aiming at?                                    */
/* -------------------------------------------------------------------- */
        if( STARTS_WITH_CI(pszName, "Pixel_") )
        {
            if( sscanf( pszName+6, "%d_%d", &iPixel, &iLine ) != 2 )
                return NULL;
        }
        else if( STARTS_WITH_CI(pszName, "GeoPixel_") )
        {
            double adfGeoTransform[6];
            double adfInvGeoTransform[6];
            double dfGeoX, dfGeoY;

            dfGeoX = CPLAtof(pszName + 9);
            const char* pszUnderscore = strchr(pszName + 9, '_');
            if( !pszUnderscore )
                return NULL;
            dfGeoY = CPLAtof(pszUnderscore + 1);

            if( GetDataset() == NULL )
                return NULL;

            if( GetDataset()->GetGeoTransform( adfGeoTransform ) != CE_None )
                return NULL;

            if( !GDALInvGeoTransform( adfGeoTransform, adfInvGeoTransform ) )
                return NULL;

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
            return NULL;

        if( iPixel < 0 || iLine < 0
            || iPixel >= GetXSize()
            || iLine >= GetYSize() )
            return NULL;

        int nTileColumn = -1, nTileRow = -1, nZoomLevel = -1;
        char* pszKey = poGDS->FindKey(iPixel, iLine, nTileColumn, nTileRow, nZoomLevel);

        if (pszKey != NULL)
        {
            //CPLDebug("MBTILES", "Key = %s", pszKey);

            osLocationInfo = "<LocationInfo>";
            osLocationInfo += "<Key>";
            char* pszXMLEscaped = CPLEscapeString(pszKey, -1, CPLES_XML_BUT_QUOTES);
            osLocationInfo += pszXMLEscaped;
            CPLFree(pszXMLEscaped);
            osLocationInfo += "</Key>";

            if (OGR_DS_GetLayerByName(poGDS->hDS, "grid_data") != NULL &&
                strchr(pszKey, '\'') == NULL)
            {
                const char* pszSQL;
                OGRLayerH hSQLLyr;
                OGRFeatureH hFeat;

                pszSQL = CPLSPrintf("SELECT key_json FROM keymap WHERE "
                                    "key_name = '%s'",
                                    pszKey);
                CPLDebug("MBTILES", "%s", pszSQL);
                hSQLLyr = OGR_DS_ExecuteSQL(poGDS->hDS, pszSQL, NULL, NULL);
                if (hSQLLyr)
                {
                    hFeat = OGR_L_GetNextFeature(hSQLLyr);
                    if (hFeat != NULL && OGR_F_IsFieldSet(hFeat, 0))
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

        return NULL;
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

    if (poGDS->nResolutions >= 1)
        return poGDS->nResolutions;
    else
        return GDALPamRasterBand::GetOverviewCount();
}

/************************************************************************/
/*                              GetOverview()                           */
/************************************************************************/

GDALRasterBand* MBTilesBand::GetOverview(int nLevel)
{
    MBTilesDataset* poGDS = (MBTilesDataset*) poDS;

    if (poGDS->nResolutions == 0)
        return GDALPamRasterBand::GetOverview(nLevel);

    if (nLevel < 0 || nLevel >= poGDS->nResolutions)
        return NULL;

    GDALDataset* poOvrDS = poGDS->papoOverviews[nLevel];
    if (poOvrDS)
        return poOvrDS->GetRasterBand(nBand);
    else
        return NULL;
}

/************************************************************************/
/*                   GetColorInterpretation()                           */
/************************************************************************/

GDALColorInterp MBTilesBand::GetColorInterpretation()
{
    MBTilesDataset* poGDS = (MBTilesDataset*) poDS;
    if (poGDS->nBands == 1)
    {
        return GCI_GrayIndex;
    }
    else if (poGDS->nBands == 3 || poGDS->nBands == 4)
    {
        if (nBand == 1)
            return GCI_RedBand;
        else if (nBand == 2)
            return GCI_GreenBand;
        else if (nBand == 3)
            return GCI_BlueBand;
        else if (nBand == 4)
            return GCI_AlphaBand;
    }

    return GCI_Undefined;
}

/************************************************************************/
/*                         MBTilesDataset()                          */
/************************************************************************/

MBTilesDataset::MBTilesDataset()
{
    bMustFree = FALSE;
    nLevel = 0;
    poMainDS = NULL;
    nResolutions = 0;
    hDS = NULL;
    papoOverviews = NULL;
    papszMetadata = NULL;
    papszImageStructure =
        CSLAddString(NULL, "INTERLEAVE=PIXEL");
    nMinTileCol = nMinTileRow = 0;
    nMinLevel = 0;
    bFetchedMetadata = FALSE;
    bHasNonEmptyGrids = -1;
}

/************************************************************************/
/*                          MBTilesDataset()                            */
/************************************************************************/

MBTilesDataset::MBTilesDataset(MBTilesDataset* poMainDS, int nLevel)
{
    bMustFree = FALSE;
    this->nLevel = nLevel;
    this->poMainDS = poMainDS;
    nResolutions = poMainDS->nResolutions - nLevel;
    hDS = poMainDS->hDS;
    papoOverviews = poMainDS->papoOverviews + nLevel;
    papszMetadata = poMainDS->papszMetadata;
    papszImageStructure =  poMainDS->papszImageStructure;

    nRasterXSize = poMainDS->nRasterXSize / (1 << nLevel);
    nRasterYSize = poMainDS->nRasterYSize / (1 << nLevel);
    nMinTileCol = nMinTileRow = 0;
    nMinLevel = 0;
    bFetchedMetadata = FALSE;
    bHasNonEmptyGrids = -1;
}

/************************************************************************/
/*                        ~MBTilesDataset()                             */
/************************************************************************/

MBTilesDataset::~MBTilesDataset()
{
    CloseDependentDatasets();
}

/************************************************************************/
/*                      CloseDependentDatasets()                        */
/************************************************************************/

int MBTilesDataset::CloseDependentDatasets()
{
    int bRet = GDALPamDataset::CloseDependentDatasets();

    if (poMainDS == NULL && !bMustFree)
    {
        CSLDestroy(papszMetadata);
        papszMetadata = NULL;
        CSLDestroy(papszImageStructure);
        papszImageStructure = NULL;

        int i;

        if (papoOverviews)
        {
            for(i=0;i<nResolutions;i++)
            {
                if (papoOverviews[i] != NULL &&
                    papoOverviews[i]->bMustFree)
                {
                    papoOverviews[i]->poMainDS = NULL;
                }
                delete papoOverviews[i];
            }
            CPLFree(papoOverviews);
            papoOverviews = NULL;
            nResolutions = 0;
            bRet = TRUE;
        }

        if (hDS != NULL)
            OGRReleaseDataSource(hDS);
        hDS = NULL;
    }
    else if (poMainDS != NULL && bMustFree)
    {
        poMainDS->papoOverviews[nLevel-1] = NULL;
        delete poMainDS;
        poMainDS = NULL;
        bRet = TRUE;
    }

    return bRet;
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

#define MAX_GM 20037508.34

CPLErr MBTilesDataset::GetGeoTransform(double* padfGeoTransform)
{
    int nMaxLevel = nMinLevel + nResolutions;
    if (nMaxLevel == 0)
    {
        padfGeoTransform[0] = -MAX_GM;
        padfGeoTransform[1] = 2 * MAX_GM / nRasterXSize;
        padfGeoTransform[2] = 0;
        padfGeoTransform[3] = MAX_GM;
        padfGeoTransform[4] = 0;
        padfGeoTransform[5] = -2 * MAX_GM / nRasterYSize;
    }
    else
    {
        int nMaxTileCol = nMinTileCol + nRasterXSize / 256;
        int nMaxTileRow = nMinTileRow + nRasterYSize / 256;
        int nMiddleTile = (1 << nMaxLevel) / 2;
        padfGeoTransform[0] = 2 * MAX_GM * (nMinTileCol - nMiddleTile) / (1 << nMaxLevel);
        padfGeoTransform[1] = 2 * MAX_GM * (nMaxTileCol - nMinTileCol) / (1 << nMaxLevel) / nRasterXSize;
        padfGeoTransform[2] = 0;
        padfGeoTransform[3] = 2 * MAX_GM * (nMaxTileRow - nMiddleTile) / (1 << nMaxLevel);
        padfGeoTransform[4] = 0;
        padfGeoTransform[5] = -2 * MAX_GM * (nMaxTileRow - nMinTileRow) / (1 << nMaxLevel) / nRasterYSize;
    }
    return CE_None;
}

/************************************************************************/
/*                         GetProjectionRef()                           */
/************************************************************************/

const char* MBTilesDataset::GetProjectionRef()
{
    return "PROJCS[\"WGS 84 / Pseudo-Mercator\",GEOGCS[\"WGS 84\",DATUM[\"WGS_1984\",SPHEROID[\"WGS 84\",6378137,298.257223563,AUTHORITY[\"EPSG\",\"7030\"]],AUTHORITY[\"EPSG\",\"6326\"]],PRIMEM[\"Greenwich\",0,AUTHORITY[\"EPSG\",\"8901\"]],UNIT[\"degree\",0.0174532925199433,AUTHORITY[\"EPSG\",\"9122\"]],AUTHORITY[\"EPSG\",\"4326\"]],PROJECTION[\"Mercator_1SP\"],PARAMETER[\"central_meridian\",0],PARAMETER[\"scale_factor\",1],PARAMETER[\"false_easting\",0],PARAMETER[\"false_northing\",0],UNIT[\"metre\",1,AUTHORITY[\"EPSG\",\"9001\"]],AXIS[\"X\",EAST],AXIS[\"Y\",NORTH],EXTENSION[\"PROJ4\",\"+proj=merc +a=6378137 +b=6378137 +lat_ts=0.0 +lon_0=0.0 +x_0=0.0 +y_0=0 +k=1.0 +units=m +nadgrids=@null +wktext  +no_defs\"],AUTHORITY[\"EPSG\",\"3857\"]]";
}

/************************************************************************/
/*                      GetMetadataDomainList()                         */
/************************************************************************/

char **MBTilesDataset::GetMetadataDomainList()
{
    return BuildMetadataDomainList(GDALDataset::GetMetadataDomainList(),
                                   TRUE,
                                   "", NULL);
}

/************************************************************************/
/*                            GetMetadata()                             */
/************************************************************************/

char** MBTilesDataset::GetMetadata( const char * pszDomain )
{
    if (pszDomain != NULL && !EQUAL(pszDomain, ""))
        return GDALPamDataset::GetMetadata(pszDomain);

    if (bFetchedMetadata)
        return aosList.List();

    bFetchedMetadata = TRUE;

    OGRLayerH hSQLLyr = OGR_DS_ExecuteSQL(hDS,
            "SELECT name, value FROM metadata", NULL, NULL);
    if (hSQLLyr == NULL)
        return NULL;

    if (OGR_FD_GetFieldCount(OGR_L_GetLayerDefn(hSQLLyr)) != 2)
    {
        OGR_DS_ReleaseResultSet(hDS, hSQLLyr);
        return NULL;
    }

    OGRFeatureH hFeat;
    while( (hFeat = OGR_L_GetNextFeature(hSQLLyr)) != NULL )
    {
        if (OGR_F_IsFieldSet(hFeat, 0) && OGR_F_IsFieldSet(hFeat, 1))
        {
            const char* pszName = OGR_F_GetFieldAsString(hFeat, 0);
            const char* pszValue = OGR_F_GetFieldAsString(hFeat, 1);
            if (pszValue[0] != '\0' &&
                !STARTS_WITH(pszValue, "function(") &&
                strstr(pszValue, "<img ") == NULL &&
                strstr(pszValue, "<p>") == NULL &&
                strstr(pszValue, "</p>") == NULL &&
                strstr(pszValue, "<div") == NULL)
            {
                aosList.AddNameValue(pszName, pszValue);
            }
        }
        OGR_F_Destroy(hFeat);
    }
    OGR_DS_ReleaseResultSet(hDS, hSQLLyr);

    return aosList.List();
}

/************************************************************************/
/*                             Identify()                               */
/************************************************************************/

int MBTilesDataset::Identify(GDALOpenInfo* poOpenInfo)
{
    if (EQUAL(CPLGetExtension(poOpenInfo->pszFilename), "MBTILES") &&
        poOpenInfo->nHeaderBytes >= 1024 &&
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
    const char* pszSQL;
    OGRLayerH hSQLLyr;
    OGRFeatureH hFeat;
    int bHasMinMaxLevel = FALSE;

    pszSQL = "SELECT value FROM metadata WHERE name = 'minzoom' UNION ALL "
             "SELECT value FROM metadata WHERE name = 'maxzoom'";
    CPLDebug("MBTILES", "%s", pszSQL);
    hSQLLyr = OGR_DS_ExecuteSQL(hDS, pszSQL, NULL, NULL);
    if (hSQLLyr)
    {
        hFeat = OGR_L_GetNextFeature(hSQLLyr);
        if (hFeat)
        {
            int bHasMinLevel = FALSE;
            if (OGR_F_IsFieldSet(hFeat, 0))
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
                    if (OGR_F_IsFieldSet(hFeat, 0))
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
        for(iLevel = 0; nMinLevel < 0 && iLevel < 16; iLevel ++)
        {
            pszSQL = CPLSPrintf(
                "SELECT zoom_level FROM %s WHERE zoom_level = %d LIMIT 1",
                (bHasMap) ? "map" : "tiles", iLevel);
            CPLDebug("MBTILES", "%s", pszSQL);
            hSQLLyr = OGR_DS_ExecuteSQL(hDS, pszSQL, NULL, NULL);
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
            hSQLLyr = OGR_DS_ExecuteSQL(hDS, pszSQL, NULL, NULL);
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
        hSQLLyr = OGR_DS_ExecuteSQL(hDS, pszSQL, NULL, NULL);
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

        if (OGR_F_IsFieldSet(hFeat, 0) && OGR_F_IsFieldSet(hFeat, 1))
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
/*                           MBTilesGetBounds()                         */
/************************************************************************/

static
int MBTilesGetBounds(OGRDataSourceH hDS,
                     CPL_UNUSED int nMinLevel,
                     int nMaxLevel,
                     int& nMinTileRow, int& nMaxTileRow,
                     int& nMinTileCol, int &nMaxTileCol)
{
    const char* pszSQL;
    int bHasBounds = FALSE;
    OGRLayerH hSQLLyr;
    OGRFeatureH hFeat;

    pszSQL = "SELECT value FROM metadata WHERE name = 'bounds'";
    CPLDebug("MBTILES", "%s", pszSQL);
    hSQLLyr = OGR_DS_ExecuteSQL(hDS, pszSQL, NULL, NULL);
    if (hSQLLyr)
    {
        hFeat = OGR_L_GetNextFeature(hSQLLyr);
        if (hFeat != NULL)
        {
            const char* pszBounds = OGR_F_GetFieldAsString(hFeat, 0);
            char** papszTokens = CSLTokenizeString2(pszBounds, ",", 0);
            if (CSLCount(papszTokens) != 4 ||
                fabs(CPLAtof(papszTokens[0])) > 180 ||
                fabs(CPLAtof(papszTokens[1])) > 86 ||
                fabs(CPLAtof(papszTokens[2])) > 180 ||
                fabs(CPLAtof(papszTokens[3])) > 86 ||
                CPLAtof(papszTokens[0]) > CPLAtof(papszTokens[2]) ||
                CPLAtof(papszTokens[1]) > CPLAtof(papszTokens[3]))
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Invalid value for 'bounds' metadata");
                CSLDestroy(papszTokens);
                OGR_F_Destroy(hFeat);
                OGR_DS_ReleaseResultSet(hDS, hSQLLyr);
                return FALSE;
            }

            #define FORTPI      0.78539816339744833
            /* Latitude to Google-mercator northing */
            #define LAT_TO_NORTHING(lat) \
                6378137 * log(tan(FORTPI + .5 * (lat) / 180 * (4 * FORTPI)))

            nMinTileCol = (int)(((CPLAtof(papszTokens[0]) + 180) / 360) * (1 << nMaxLevel));
            nMaxTileCol = (int)(((CPLAtof(papszTokens[2]) + 180) / 360) * (1 << nMaxLevel));
            nMinTileRow = (int)(0.5 + ((LAT_TO_NORTHING(CPLAtof(papszTokens[1])) + MAX_GM) / (2* MAX_GM)) * (1 << nMaxLevel));
            nMaxTileRow = (int)(0.5 + ((LAT_TO_NORTHING(CPLAtof(papszTokens[3])) + MAX_GM) / (2* MAX_GM)) * (1 << nMaxLevel));

            bHasBounds = TRUE;

            CSLDestroy(papszTokens);

            OGR_F_Destroy(hFeat);
        }
        OGR_DS_ReleaseResultSet(hDS, hSQLLyr);
    }

    if (!bHasBounds)
    {
        pszSQL = CPLSPrintf("SELECT min(tile_column), max(tile_column), "
                            "min(tile_row), max(tile_row) FROM tiles "
                            "WHERE zoom_level = %d", nMaxLevel);
        CPLDebug("MBTILES", "%s", pszSQL);
        hSQLLyr = OGR_DS_ExecuteSQL(hDS, pszSQL, NULL, NULL);
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

        if (OGR_F_IsFieldSet(hFeat, 0) &&
            OGR_F_IsFieldSet(hFeat, 1) &&
            OGR_F_IsFieldSet(hFeat, 2) &&
            OGR_F_IsFieldSet(hFeat, 3))
        {
            nMinTileCol = OGR_F_GetFieldAsInteger(hFeat, 0);
            nMaxTileCol = OGR_F_GetFieldAsInteger(hFeat, 1) + 1;
            nMinTileRow = OGR_F_GetFieldAsInteger(hFeat, 2);
            nMaxTileRow = OGR_F_GetFieldAsInteger(hFeat, 3) + 1;
            bHasBounds = TRUE;
        }

        OGR_F_Destroy(hFeat);
        OGR_DS_ReleaseResultSet(hDS, hSQLLyr);
    }

    return bHasBounds;
}

/************************************************************************/
/*                        MBTilesCurlReadCbk()                          */
/************************************************************************/

/* We spy the data received by CURL for the initial request where we try */
/* to get a first tile to see its characteristics. We just need the header */
/* to determine that, so let's make VSICurl stop reading after we have found it */

static int MBTilesCurlReadCbk(CPL_UNUSED VSILFILE* fp,
                              void *pabyBuffer, size_t nBufferSize,
                              void* pfnUserData)
{
    const GByte abyPNGSig[] = { 0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, /* PNG signature */
                                0x00, 0x00, 0x00, 0x0D, /* IHDR length */
                                0x49, 0x48, 0x44, 0x52  /* IHDR chunk */ };

    /* JPEG SOF0 (Start Of Frame 0) marker */
    const GByte abyJPEG1CompSig[] = { 0xFF, 0xC0, /* marker */
                                      0x00, 0x0B, /* data length = 8 + 1 * 3 */
                                      0x08,       /* depth : 8 bit */
                                      0x01, 0x00, /* width : 256 */
                                      0x01, 0x00, /* height : 256 */
                                      0x01        /* components : 1 */
                                    };
    const GByte abyJPEG3CompSig[] = { 0xFF, 0xC0, /* marker */
                                      0x00, 0x11, /* data length = 8 + 3 * 3 */
                                      0x08,       /* depth : 8 bit */
                                      0x01, 0x00, /* width : 256 */
                                      0x01, 0x00, /* width : 256 */
                                      0x03        /* components : 3 */
                                     };

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

            int* pnBands = (int*) pfnUserData;
            *pnBands = -2;
            if (nWidth == 256 && nHeight == 256 && nDepth == 8)
            {
                if (nColorType == 0)
                    *pnBands = 1; /* Gray */
                else if (nColorType == 2)
                    *pnBands = 3; /* RGB */
                else if (nColorType == 3)
                {
                    /* This might also be a color table with transparency */
                    /* but we cannot tell ! */
                    *pnBands = -1;
                    return TRUE;
                }
                else if (nColorType == 4)
                    *pnBands = 2; /* Gray + alpha */
                else if (nColorType == 6)
                    *pnBands = 4; /* RGB + alpha */
            }

            return FALSE;
        }
    }

    for(i = 0; i < (int)nBufferSize - (int)sizeof(abyJPEG1CompSig); i++)
    {
        if (memcmp(((GByte*)pabyBuffer) + i, abyJPEG1CompSig, sizeof(abyJPEG1CompSig)) == 0)
        {
            CPLDebug("MBTILES", "JPEG: nWidth=%d nHeight=%d depth=%d nBands=%d",
                        256, 256, 8, 1);

            int* pnBands = (int*) pfnUserData;
            *pnBands = 1;

            return FALSE;
        }
        else if (memcmp(((GByte*)pabyBuffer) + i, abyJPEG3CompSig, sizeof(abyJPEG3CompSig)) == 0)
        {
            CPLDebug("MBTILES", "JPEG: nWidth=%d nHeight=%d depth=%d nBands=%d",
                        256, 256, 8, 3);

            int* pnBands = (int*) pfnUserData;
            *pnBands = 3;

            return FALSE;
        }
    }

    return TRUE;
}

/************************************************************************/
/*                        MBTilesGetBandCount()                         */
/************************************************************************/

static
int MBTilesGetBandCount(OGRDataSourceH &hDS,
                        CPL_UNUSED int nMinLevel,
                        int nMaxLevel,
                        int nMinTileRow, int nMaxTileRow,
                        int nMinTileCol, int nMaxTileCol)
{
    OGRLayerH hSQLLyr;
    OGRFeatureH hFeat;
    const char* pszSQL;
    VSILFILE* fpCURLOGR = NULL;
    int bFirstSelect = TRUE;

    int nBands = -1;

    /* Small trick to get the VSILFILE associated with the OGR SQLite */
    /* DB */
    CPLString osDSName(OGR_DS_GetName(hDS));
    if (STARTS_WITH(osDSName.c_str(), "/vsicurl/"))
    {
        CPLErrorReset();
        CPLPushErrorHandler(CPLQuietErrorHandler);
        hSQLLyr = OGR_DS_ExecuteSQL(hDS, "GetVSILFILE()", NULL, NULL);
        CPLPopErrorHandler();
        CPLErrorReset();
        if (hSQLLyr != NULL)
        {
            hFeat = OGR_L_GetNextFeature(hSQLLyr);
            if (hFeat)
            {
                if (OGR_F_IsFieldSet(hFeat, 0))
                {
                    const char* pszPointer = OGR_F_GetFieldAsString(hFeat, 0);
                    fpCURLOGR = (VSILFILE* )CPLScanPointer( pszPointer, strlen(pszPointer) );
                }
                OGR_F_Destroy(hFeat);
            }
            OGR_DS_ReleaseResultSet(hDS, hSQLLyr);
        }
    }

    pszSQL = CPLSPrintf("SELECT tile_data FROM tiles WHERE "
                            "tile_column = %d AND tile_row = %d AND zoom_level = %d",
                            (nMinTileCol  + nMaxTileCol) / 2,
                            (nMinTileRow  + nMaxTileRow) / 2,
                            nMaxLevel);
    CPLDebug("MBTILES", "%s", pszSQL);

    if (fpCURLOGR)
    {
        /* Install a spy on the file connexion that will intercept */
        /* PNG or JPEG headers, to interrupt their downloading */
        /* once the header is found. Speeds up dataset opening. */
        CPLErrorReset();
        VSICurlInstallReadCbk(fpCURLOGR, MBTilesCurlReadCbk, &nBands, TRUE);

        CPLErrorReset();
        CPLPushErrorHandler(CPLQuietErrorHandler);
        hSQLLyr = OGR_DS_ExecuteSQL(hDS, pszSQL, NULL, NULL);
        CPLPopErrorHandler();

        VSICurlUninstallReadCbk(fpCURLOGR);

        /* Did the spy intercept something interesting ? */
        if (nBands != -1)
        {
            CPLErrorReset();

            OGR_DS_ReleaseResultSet(hDS, hSQLLyr);
            hSQLLyr = NULL;

            /* Re-open OGR SQLite DB, because with our spy we have simulated an I/O error */
            /* that SQLite will have difficulies to recover within the existing connection */
            /* No worry ! This will be fast because the /vsicurl/ cache has cached the already */
            /* read blocks */
            OGRReleaseDataSource(hDS);
            hDS = MBTILESOpenSQLiteDB(osDSName.c_str(), GA_ReadOnly);
            if (hDS == NULL)
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
        hSQLLyr = OGR_DS_ExecuteSQL(hDS, pszSQL, NULL, NULL);
    }

    while( TRUE )
    {
        if (hSQLLyr == NULL && bFirstSelect)
        {
            bFirstSelect = FALSE;
            pszSQL = CPLSPrintf("SELECT tile_data FROM tiles WHERE "
                                "zoom_level = %d LIMIT 1", nMaxLevel);
            CPLDebug("MBTILES", "%s", pszSQL);
            hSQLLyr = OGR_DS_ExecuteSQL(hDS, pszSQL, NULL, NULL);
            if (hSQLLyr == NULL)
                return -1;
        }

        hFeat = OGR_L_GetNextFeature(hSQLLyr);
        if (hFeat == NULL)
        {
            OGR_DS_ReleaseResultSet(hDS, hSQLLyr);
            hSQLLyr = NULL;
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
                                          apszAllowedDrivers, NULL, NULL);
    if (hDSTile == NULL)
    {
        VSIUnlink(osMemFileName.c_str());
        OGR_F_Destroy(hFeat);
        OGR_DS_ReleaseResultSet(hDS, hSQLLyr);
        return -1;
    }

    nBands = GDALGetRasterCount(hDSTile);

    if ((nBands != 1 && nBands != 3 && nBands != 4) ||
        GDALGetRasterXSize(hDSTile) != 256 ||
        GDALGetRasterYSize(hDSTile) != 256 ||
        GDALGetRasterDataType(GDALGetRasterBand(hDSTile, 1)) != GDT_Byte)
    {
        CPLError(CE_Failure, CPLE_NotSupported, "Unsupported tile characteristics");
        GDALClose(hDSTile);
        VSIUnlink(osMemFileName.c_str());
        OGR_F_Destroy(hFeat);
        OGR_DS_ReleaseResultSet(hDS, hSQLLyr);
        return -1;
    }

    GDALColorTableH hCT = GDALGetRasterColorTable(GDALGetRasterBand(hDSTile, 1));
    if (nBands == 1 && hCT != NULL)
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
        return NULL;

    if (OGRGetDriverCount() == 0)
        OGRRegisterAll();

/* -------------------------------------------------------------------- */
/*      Open underlying OGR DB                                          */
/* -------------------------------------------------------------------- */

    OGRDataSourceH hDS = MBTILESOpenSQLiteDB(poOpenInfo->pszFilename, GA_ReadOnly);

    MBTilesDataset* poDS = NULL;

    if (hDS == NULL)
        goto end;

/* -------------------------------------------------------------------- */
/*      Build dataset                                                   */
/* -------------------------------------------------------------------- */
    {
        CPLString osMetadataTableName, osRasterTableName;
        CPLString osSQL;
        OGRLayerH hMetadataLyr, hRasterLyr;
        OGRFeatureH hFeat;
        int nResolutions;
        int iBand, nBands, nBlockXSize, nBlockYSize;
        GDALDataType eDataType;
        OGRLayerH hSQLLyr = NULL;
        int nMinLevel = -1, nMaxLevel = -1;
        int nMinTileRow = 0, nMaxTileRow = 0, nMinTileCol = 0, nMaxTileCol = 0;
        int bHasBounds = FALSE;
        int bHasMinMaxLevel = FALSE;
        int bHasMap;
        const char* pszBandCount;

        osMetadataTableName = "metadata";

        hMetadataLyr = OGR_DS_GetLayerByName(hDS, osMetadataTableName.c_str());
        if (hMetadataLyr == NULL)
            goto end;

        osRasterTableName += "tiles";

        hRasterLyr = OGR_DS_GetLayerByName(hDS, osRasterTableName.c_str());
        if (hRasterLyr == NULL)
            goto end;

        bHasMap = OGR_DS_GetLayerByName(hDS, "map") != NULL;
        if (bHasMap)
        {
            bHasMap = FALSE;

            hSQLLyr = OGR_DS_ExecuteSQL(hDS, "SELECT type FROM sqlite_master WHERE name = 'tiles'", NULL, NULL);
            if (hSQLLyr != NULL)
            {
                hFeat = OGR_L_GetNextFeature(hSQLLyr);
                if (hFeat)
                {
                    if (OGR_F_IsFieldSet(hFeat, 0))
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

        bHasBounds = MBTilesGetBounds(hDS, nMinLevel, nMaxLevel,
                                      nMinTileRow, nMaxTileRow,
                                      nMinTileCol, nMaxTileCol);
        if (!bHasBounds)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Cannot find min and max tile numbers");
            goto end;
        }

/* -------------------------------------------------------------------- */
/*      Get number of bands                                             */
/* -------------------------------------------------------------------- */

        pszBandCount = CPLGetConfigOption("MBTILES_BAND_COUNT", "-1");
        nBands = atoi(pszBandCount);

        if( ! (nBands == 1 || nBands == 3 || nBands == 4) )
        {
            nBands = MBTilesGetBandCount(hDS, nMinLevel, nMaxLevel,
                                         nMinTileRow, nMaxTileRow,
                                         nMinTileCol, nMaxTileCol);
            if (nBands < 0)
                goto end;
        }

/* -------------------------------------------------------------------- */
/*      Set dataset attributes                                          */
/* -------------------------------------------------------------------- */

        poDS = new MBTilesDataset();
        poDS->eAccess = poOpenInfo->eAccess;
        poDS->hDS = hDS;

        /* poDS will release it from now */
        hDS = NULL;

/* -------------------------------------------------------------------- */
/*      Store resolutions                                               */
/* -------------------------------------------------------------------- */
        poDS->nMinLevel = nMinLevel;
        poDS->nResolutions = nResolutions = nMaxLevel - nMinLevel;

/* -------------------------------------------------------------------- */
/*      Round bounds to the lowest zoom level                           */
/* -------------------------------------------------------------------- */

        //CPLDebug("MBTILES", "%d %d %d %d", nMinTileCol, nMinTileRow, nMaxTileCol, nMaxTileRow);
        nMinTileCol = (int)(1.0 * nMinTileCol / (1 << nResolutions)) * (1 << nResolutions);
        nMinTileRow = (int)(1.0 * nMinTileRow / (1 << nResolutions)) * (1 << nResolutions);
        nMaxTileCol = (int)ceil(1.0 * nMaxTileCol / (1 << nResolutions)) * (1 << nResolutions);
        nMaxTileRow = (int)ceil(1.0 * nMaxTileRow / (1 << nResolutions)) * (1 << nResolutions);

/* -------------------------------------------------------------------- */
/*      Compute raster size, geotransform and projection                */
/* -------------------------------------------------------------------- */
        poDS->nMinTileCol = nMinTileCol;
        poDS->nMinTileRow = nMinTileRow;
        poDS->nRasterXSize = (nMaxTileCol-nMinTileCol) * 256;
        poDS->nRasterYSize = (nMaxTileRow-nMinTileRow) * 256;

        nBlockXSize = nBlockYSize = 256;
        eDataType = GDT_Byte;

/* -------------------------------------------------------------------- */
/*      Add bands                                                       */
/* -------------------------------------------------------------------- */

        for(iBand=0;iBand<nBands;iBand++)
            poDS->SetBand(iBand+1, new MBTilesBand(poDS, iBand+1, eDataType,
                                                  nBlockXSize, nBlockYSize));

/* -------------------------------------------------------------------- */
/*      Add overview levels as internal datasets                        */
/* -------------------------------------------------------------------- */
        if (nResolutions >= 1)
        {
            poDS->papoOverviews = (MBTilesDataset**)
                CPLCalloc(nResolutions, sizeof(MBTilesDataset*));
            int nLev;
            for(nLev=1;nLev<=nResolutions;nLev++)
            {
                poDS->papoOverviews[nLev-1] = new MBTilesDataset(poDS, nLev);

                for(iBand=0;iBand<nBands;iBand++)
                {
                    poDS->papoOverviews[nLev-1]->SetBand(iBand+1,
                        new MBTilesBand(poDS->papoOverviews[nLev-1], iBand+1, eDataType,
                                           nBlockXSize, nBlockYSize));
                }
            }
        }

        poDS->SetMetadata(poDS->papszImageStructure, "IMAGE_STRUCTURE");

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
/*                       GDALRegister_MBTiles()                         */
/************************************************************************/

void GDALRegister_MBTiles()

{
    GDALDriver  *poDriver;

    if (! GDAL_CHECK_VERSION("MBTiles driver"))
        return;

    if( GDALGetDriverByName( "MBTiles" ) == NULL )
    {
        poDriver = new GDALDriver();

        poDriver->SetDescription( "MBTiles" );
        poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                                   "MBTiles" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC,
                                   "frmt_mbtiles.html" );
        poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "mbtiles" );

        poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

        poDriver->pfnOpen = MBTilesDataset::Open;
        poDriver->pfnIdentify = MBTilesDataset::Identify;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}
