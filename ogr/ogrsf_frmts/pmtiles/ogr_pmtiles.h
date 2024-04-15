/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implementation of PMTiles
 * Author:   Even Rouault <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2023, Planet Labs
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

#ifndef OGR_PMTILES_H_INCLUDED
#define OGR_PMTILES_H_INCLUDED

#include "gdal_priv.h"
#include "ogrsf_frmts.h"

#include "cpl_compressor.h"
#include "cpl_vsi_virtual.h"

#include "include_pmtiles.h"

#include <limits>
#include <set>
#include <stack>

// #define DEBUG_PMTILES

#define SPHERICAL_RADIUS 6378137.0
#define MAX_GM (SPHERICAL_RADIUS * M_PI)  // 20037508.342789244

#if defined(HAVE_SQLITE) && defined(HAVE_GEOS)
// Needed by mvtutils.h
#define HAVE_MVT_WRITE_SUPPORT
#endif

/************************************************************************/
/*                          OGRPMTilesDataset                           */
/************************************************************************/

class OGRPMTilesDataset final : public GDALDataset
{
  public:
    OGRPMTilesDataset() = default;

    ~OGRPMTilesDataset() override;

    bool Open(GDALOpenInfo *poOpenInfo);

    int GetLayerCount() override
    {
        return static_cast<int>(m_apoLayers.size());
    }

    OGRLayer *GetLayer(int) override;

    inline int GetMinZoomLevel() const
    {
        return m_nMinZoomLevel;
    }

    inline int GetMaxZoomLevel() const
    {
        return m_nMaxZoomLevel;
    }

    inline const pmtiles::headerv3 &GetHeader() const
    {
        return m_sHeader;
    }

    static const char *GetCompression(uint8_t nVal);

    static const char *GetTileType(const pmtiles::headerv3 &sHeader);

    inline const std::string &GetMetadataContent() const
    {
        return m_osMetadata;
    }

    inline const std::string &GetMetadataFilename() const
    {
        return m_osMetadataFilename;
    }

    inline const std::string &GetClipOpenOption() const
    {
        return m_osClipOpenOption;
    }

    /** Return a short-lived decompressed buffer for metadata or directory
     * entries or nullptr in case of error.
     */
    const std::string *ReadInternal(uint64_t nOffset, uint64_t nSize,
                                    const char *pszDataType);

    /** Return a short-lived decompressed buffer for tile data.
     *  or nullptr in case of error.
     */
    const std::string *ReadTileData(uint64_t nOffset, uint64_t nSize);

  private:
    VSIVirtualHandleUniquePtr m_poFile{};

    //! PMTiles header
    pmtiles::headerv3 m_sHeader{};

    //! JSON serialized metadata
    std::string m_osMetadata{};

    //! /vsimem/ filename with the m_osMetadata content
    std::string m_osMetadataFilename{};

    //! Value of the CLIP open option
    std::string m_osClipOpenOption{};

    //! Decompressor for metadata and directories
    const CPLCompressor *m_psInternalDecompressor = nullptr;

    //! Decompressor for tile
    const CPLCompressor *m_psTileDataDecompressor = nullptr;

    //! Last raw data read by Read()
    std::string m_osBuffer{};

    //! Last uncompressed data read by Read(). Only used if compression
    std::string m_osDecompressedBuffer{};

    std::vector<std::unique_ptr<OGRLayer>> m_apoLayers{};

    //! Minimum zoom level got from header
    int m_nMinZoomLevel = 0;

    //! Maximum zoom level got from header
    int m_nMaxZoomLevel = 0;

    /** Return a short-lived decompressed buffer, or nullptr in case of error
     */
    const std::string *Read(const CPLCompressor *psDecompressor,
                            uint64_t nOffset, uint64_t nSize,
                            const char *pszDataType);

    CPL_DISALLOW_COPY_ASSIGN(OGRPMTilesDataset)
};

/************************************************************************/
/*                        OGRPMTilesTileIterator                        */
/************************************************************************/

//! Iterator to browse through tiles
class OGRPMTilesTileIterator
{
  public:
    //! Constructor to iterate over all tiles (possibly limited to a zoom level
    explicit OGRPMTilesTileIterator(OGRPMTilesDataset *poDS,
                                    int nZoomLevel = -1)
        : m_poDS(poDS), m_nZoomLevel(nZoomLevel)
    {
    }

    //! Constructor with a window of interest in tile coordinates
    OGRPMTilesTileIterator(OGRPMTilesDataset *poDS, int nZoomLevel, int nMinX,
                           int nMinY, int nMaxX, int nMaxY)
        : m_poDS(poDS), m_nZoomLevel(nZoomLevel), m_nMinX(nMinX),
          m_nMinY(nMinY), m_nMaxX(nMaxX), m_nMaxY(nMaxY)
    {
    }

    /** Return the (z, x, y, offset, length) of the next tile.
     *
     * If entry_zxy.offset == 0, the iteration has stopped.
     */
    pmtiles::entry_zxy GetNextTile(uint32_t *pnRunLength = nullptr);

    void SkipRunLength();

#ifdef DEBUG_PMTILES
    void DumpTiles();
#endif

  private:
    // Input parameters
    OGRPMTilesDataset *m_poDS = nullptr;
    int m_nZoomLevel = -1;
    int m_nMinX = -1;
    int m_nMinY = -1;
    int m_nMaxX = -1;
    int m_nMaxY = -1;

    // Used when iterating over tile id is inefficient
    int m_nCurX = -1;
    int m_nCurY = -1;

    // for sanity checks. Must be increasing when walking through entries
    static constexpr uint64_t INVALID_LAST_TILE_ID =
        std::numeric_limits<uint64_t>::max();
    uint64_t m_nLastTileId = INVALID_LAST_TILE_ID;

    // Computed values from zoom leven and min/max x/y
    uint64_t m_nMinTileId = std::numeric_limits<uint64_t>::max();
    uint64_t m_nMaxTileId = 0;

    bool m_bEOF = false;

    // State of exploration of a directory
    struct DirectoryContext
    {
        // Entries, either tiles (sEntry.run_length > 0) or subdiretories
        // (sEntry.run_length == 0)
        std::vector<pmtiles::entryv3> sEntries{};

        // Next index of sEntries[] to explore
        uint32_t nIdxInEntries = 0;

        // For tiles, value between 0 and
        // sEntries[nIdxInEntries].run_length - 1
        uint32_t nIdxInRunLength = 0;
    };

    // Stack of directories: bottom is root directory, and then we
    // push subdiretories we browse throw
    std::stack<DirectoryContext> m_aoStack{};

    bool LoadRootDirectory();

    CPL_DISALLOW_COPY_ASSIGN(OGRPMTilesTileIterator)
};

/************************************************************************/
/*                        OGRPMTilesVectorLayer                         */
/************************************************************************/

class OGRPMTilesVectorLayer final
    : public OGRLayer,
      public OGRGetNextFeatureThroughRaw<OGRPMTilesVectorLayer>
{
  public:
    OGRPMTilesVectorLayer(OGRPMTilesDataset *poDS, const char *pszLayerName,
                          const CPLJSONObject &oFields,
                          const CPLJSONArray &oAttributesFromTileStats,
                          bool bJsonField, double dfMinX, double dfMinY,
                          double dfMaxX, double dfMaxY,
                          OGRwkbGeometryType eGeomType, int nZoomLevel,
                          bool bZoomLevelFromSpatialFilter);
    ~OGRPMTilesVectorLayer();

    void ResetReading() override;

    OGRFeature *GetNextRawFeature();
    DEFINE_GET_NEXT_FEATURE_THROUGH_RAW(OGRPMTilesVectorLayer)

    OGRFeatureDefn *GetLayerDefn() override
    {
        return m_poFeatureDefn;
    }

    int TestCapability(const char *) override;

    OGRErr GetExtent(OGREnvelope *psExtent, int bForce) override;

    OGRErr GetExtent(int iGeomField, OGREnvelope *psExtent, int bForce) override
    {
        return OGRLayer::GetExtent(iGeomField, psExtent, bForce);
    }

    void SetSpatialFilter(OGRGeometry *) override;

    void SetSpatialFilter(int iGeomField, OGRGeometry *poGeom) override
    {
        OGRLayer::SetSpatialFilter(iGeomField, poGeom);
    }

    GIntBig GetFeatureCount(int bForce) override;

    OGRFeature *GetFeature(GIntBig nFID) override;

    static OGRwkbGeometryType GuessGeometryType(OGRPMTilesDataset *poDS,
                                                const char *pszLayerName,
                                                int nZoomLevel);

  private:
    OGRPMTilesDataset *m_poDS = nullptr;
    OGRFeatureDefn *m_poFeatureDefn = nullptr;

    //! Iterator over tiles
    std::unique_ptr<OGRPMTilesTileIterator> m_poTileIterator{};

    //! Total feature count (may over-estimate due to not applying clipping)
    GIntBig m_nFeatureCount = -1;

    //! X tile value of currently opened tile
    uint32_t m_nX = 0;

    //! Y tile value of currently opened tile
    uint32_t m_nY = 0;

    //! Offset of the currently opened tile
    uint64_t m_nLastTileOffset = 0;

    //! Uncompressed MVT tile
    std::string m_osTileData{};

    //! In-memory MVT dataset of the currently opened tile
    std::unique_ptr<GDALDataset> m_poTileDS{};

    //! Layer of m_poTileDS
    OGRLayer *m_poTileLayer = nullptr;

    //! Layer extent
    OGREnvelope m_sExtent{};

    //! Minimum X tile value corresponding to m_sFilterEnvelope
    int m_nFilterMinX = 0;

    //! Minimum Y tile value corresponding to m_sFilterEnvelope
    int m_nFilterMinY = 0;

    //! Maximum X tile value corresponding to m_sFilterEnvelope
    int m_nFilterMaxX = 0;

    //! Maximum Y tile value corresponding to m_sFilterEnvelope
    int m_nFilterMaxY = 0;

    //! Currently used zoom level
    int m_nZoomLevel = 0;

    //! Whether we should auto-adapt m_nZoomLevel from the spatial filter extent
    bool m_bZoomLevelAuto = false;

    //! Whether we should expose the tile fields in a "json" field
    bool m_bJsonField = false;

    std::unique_ptr<OGRFeature> GetNextSrcFeature();
    std::unique_ptr<OGRFeature> CreateFeatureFrom(OGRFeature *poSrcFeature);
    GIntBig GetTotalFeatureCount() const;
    void ExtentToTileExtent(const OGREnvelope &sEnvelope, int &nTileMinX,
                            int &nTileMinY, int &nTileMaxX,
                            int &nTileMaxY) const;

    CPL_DISALLOW_COPY_ASSIGN(OGRPMTilesVectorLayer)
};

#ifdef HAVE_MVT_WRITE_SUPPORT

/************************************************************************/
/*                     OGRPMTilesWriterDataset                          */
/************************************************************************/

class OGRPMTilesWriterDataset final : public GDALDataset
{
    std::unique_ptr<GDALDataset> m_poMBTilesWriterDataset{};

  public:
    OGRPMTilesWriterDataset() = default;

    ~OGRPMTilesWriterDataset() override;

    bool Create(const char *pszFilename, CSLConstList papszOptions);

    CPLErr Close() override;

    OGRLayer *ICreateLayer(const char *pszName,
                           const OGRGeomFieldDefn *poGeomFieldDefn,
                           CSLConstList papszOptions) override;

    int TestCapability(const char *) override;
};

#endif  // HAVE_MVT_WRITE_SUPPORT

#endif  // OGR_PMTILES_H_INCLUDED
