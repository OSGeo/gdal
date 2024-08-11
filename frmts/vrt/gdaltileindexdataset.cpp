/******************************************************************************
 *
 * Project:  Virtual GDAL Datasets
 * Purpose:  Tile index based VRT
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2023, Even Rouault <even dot rouault at spatialys.com>
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

/*! @cond Doxygen_Suppress */

#include <array>
#include <algorithm>
#include <limits>
#include <set>
#include <tuple>
#include <utility>
#include <vector>

#include "cpl_port.h"
#include "cpl_mem_cache.h"
#include "cpl_minixml.h"
#include "vrtdataset.h"
#include "vrt_priv.h"
#include "ogrsf_frmts.h"
#include "gdal_proxy.h"
#include "gdal_utils.h"

#if defined(__SSE2__) || defined(_M_X64)
#define USE_SSE2_OPTIM
#include <emmintrin.h>
// MSVC doesn't define __SSE4_1__, but if -arch:AVX2 is enabled, we do have SSE4.1
#if defined(__SSE4_1__) || defined(__AVX2__)
#define USE_SSE41_OPTIM
#include <smmintrin.h>
#endif
#endif

// Semantincs of indices of a GeoTransform (double[6]) matrix
constexpr int GT_TOPLEFT_X = 0;
constexpr int GT_WE_RES = 1;
constexpr int GT_ROTATION_PARAM1 = 2;
constexpr int GT_TOPLEFT_Y = 3;
constexpr int GT_ROTATION_PARAM2 = 4;
constexpr int GT_NS_RES = 5;

constexpr const char *GTI_PREFIX = "GTI:";

constexpr const char *MD_DS_TILE_INDEX_LAYER = "TILE_INDEX_LAYER";

constexpr const char *MD_RESX = "RESX";
constexpr const char *MD_RESY = "RESY";
constexpr const char *MD_BAND_COUNT = "BAND_COUNT";
constexpr const char *MD_DATA_TYPE = "DATA_TYPE";
constexpr const char *MD_NODATA = "NODATA";
constexpr const char *MD_MINX = "MINX";
constexpr const char *MD_MINY = "MINY";
constexpr const char *MD_MAXX = "MAXX";
constexpr const char *MD_MAXY = "MAXY";
constexpr const char *MD_GEOTRANSFORM = "GEOTRANSFORM";
constexpr const char *MD_XSIZE = "XSIZE";
constexpr const char *MD_YSIZE = "YSIZE";
constexpr const char *MD_COLOR_INTERPRETATION = "COLOR_INTERPRETATION";
constexpr const char *MD_SRS = "SRS";
constexpr const char *MD_LOCATION_FIELD = "LOCATION_FIELD";
constexpr const char *MD_SORT_FIELD = "SORT_FIELD";
constexpr const char *MD_SORT_FIELD_ASC = "SORT_FIELD_ASC";
constexpr const char *MD_BLOCK_X_SIZE = "BLOCKXSIZE";
constexpr const char *MD_BLOCK_Y_SIZE = "BLOCKYSIZE";
constexpr const char *MD_MASK_BAND = "MASK_BAND";
constexpr const char *MD_RESAMPLING = "RESAMPLING";

constexpr const char *const apszTIOptions[] = {MD_RESX,
                                               MD_RESY,
                                               MD_BAND_COUNT,
                                               MD_DATA_TYPE,
                                               MD_NODATA,
                                               MD_MINX,
                                               MD_MINY,
                                               MD_MAXX,
                                               MD_MAXY,
                                               MD_GEOTRANSFORM,
                                               MD_XSIZE,
                                               MD_YSIZE,
                                               MD_COLOR_INTERPRETATION,
                                               MD_SRS,
                                               MD_LOCATION_FIELD,
                                               MD_SORT_FIELD,
                                               MD_SORT_FIELD_ASC,
                                               MD_BLOCK_X_SIZE,
                                               MD_BLOCK_Y_SIZE,
                                               MD_MASK_BAND,
                                               MD_RESAMPLING};

constexpr const char *const MD_BAND_OFFSET = "OFFSET";
constexpr const char *const MD_BAND_SCALE = "SCALE";
constexpr const char *const MD_BAND_UNITTYPE = "UNITTYPE";
constexpr const char *const apszReservedBandItems[] = {
    MD_BAND_OFFSET, MD_BAND_SCALE, MD_BAND_UNITTYPE};

constexpr const char *GTI_XML_BANDCOUNT = "BandCount";
constexpr const char *GTI_XML_DATATYPE = "DataType";
constexpr const char *GTI_XML_NODATAVALUE = "NoDataValue";
constexpr const char *GTI_XML_COLORINTERP = "ColorInterp";
constexpr const char *GTI_XML_LOCATIONFIELD = "LocationField";
constexpr const char *GTI_XML_SORTFIELD = "SortField";
constexpr const char *GTI_XML_SORTFIELDASC = "SortFieldAsc";
constexpr const char *GTI_XML_MASKBAND = "MaskBand";
constexpr const char *GTI_XML_OVERVIEW_ELEMENT = "Overview";
constexpr const char *GTI_XML_OVERVIEW_DATASET = "Dataset";
constexpr const char *GTI_XML_OVERVIEW_LAYER = "Layer";
constexpr const char *GTI_XML_OVERVIEW_FACTOR = "Factor";

constexpr const char *GTI_XML_BAND_ELEMENT = "Band";
constexpr const char *GTI_XML_BAND_NUMBER = "band";
constexpr const char *GTI_XML_BAND_DATATYPE = "dataType";
constexpr const char *GTI_XML_BAND_DESCRIPTION = "Description";
constexpr const char *GTI_XML_BAND_OFFSET = "Offset";
constexpr const char *GTI_XML_BAND_SCALE = "Scale";
constexpr const char *GTI_XML_BAND_NODATAVALUE = "NoDataValue";
constexpr const char *GTI_XML_BAND_UNITTYPE = "UnitType";
constexpr const char *GTI_XML_BAND_COLORINTERP = "ColorInterp";
constexpr const char *GTI_XML_CATEGORYNAMES = "CategoryNames";
constexpr const char *GTI_XML_COLORTABLE = "ColorTable";
constexpr const char *GTI_XML_RAT = "GDALRasterAttributeTable";

/************************************************************************/
/*                           ENDS_WITH_CI()                             */
/************************************************************************/

static inline bool ENDS_WITH_CI(const char *a, const char *b)
{
    return strlen(a) >= strlen(b) && EQUAL(a + strlen(a) - strlen(b), b);
}

/************************************************************************/
/*                       GDALTileIndexDataset                           */
/************************************************************************/

class GDALTileIndexBand;

class GDALTileIndexDataset final : public GDALPamDataset
{
  public:
    GDALTileIndexDataset();
    ~GDALTileIndexDataset() override;

    bool Open(GDALOpenInfo *poOpenInfo);

    CPLErr FlushCache(bool bAtClosing) override;

    CPLErr GetGeoTransform(double *padfGeoTransform) override;
    const OGRSpatialReference *GetSpatialRef() const override;

    CPLErr IRasterIO(GDALRWFlag eRWFlag, int nXOff, int nYOff, int nXSize,
                     int nYSize, void *pData, int nBufXSize, int nBufYSize,
                     GDALDataType eBufType, int nBandCount, int *panBandMap,
                     GSpacing nPixelSpace, GSpacing nLineSpace,
                     GSpacing nBandSpace,
                     GDALRasterIOExtraArg *psExtraArg) override;

    const char *GetMetadataItem(const char *pszName,
                                const char *pszDomain) override;
    CPLErr SetMetadataItem(const char *pszName, const char *pszValue,
                           const char *pszDomain) override;
    CPLErr SetMetadata(char **papszMD, const char *pszDomain) override;

    void LoadOverviews();

    std::vector<GTISourceDesc> GetSourcesMoreRecentThan(int64_t mTime);

  private:
    friend class GDALTileIndexBand;

    //! Optional GTI XML
    CPLXMLTreeCloser m_psXMLTree{nullptr};

    //! Whether the GTI XML might be modified (by SetMetadata/SetMetadataItem)
    bool m_bXMLUpdatable = false;

    //! Whether the GTI XML has been modified (by SetMetadata/SetMetadataItem)
    bool m_bXMLModified = false;

    //! Unique string (without the process) for this tile index. Passed to
    //! GDALProxyPoolDataset to ensure that sources are unique for a given owner
    const std::string m_osUniqueHandle;

    //! Vector dataset with the sources
    std::unique_ptr<GDALDataset> m_poVectorDS{};

    //! Vector layer with the sources
    OGRLayer *m_poLayer = nullptr;

    //! Geotransform matrix of the tile index
    std::array<double, 6> m_adfGeoTransform{0, 0, 0, 0, 0, 0};

    //! Index of the "location" (or alternate name given by user) field
    //! (within m_poLayer->GetLayerDefn()), that contain source dataset names.
    int m_nLocationFieldIndex = -1;

    //! SRS of the tile index.
    OGRSpatialReference m_oSRS{};

    //! Cache from dataset name to dataset handle.
    //! Note that the dataset objects are ultimately GDALProxyPoolDataset,
    //! and that the GDALProxyPoolDataset limits the number of simultaneously
    //! opened real datasets (controlled by GDAL_MAX_DATASET_POOL_SIZE). Hence 500 is not too big.
    lru11::Cache<std::string, std::shared_ptr<GDALDataset>> m_oMapSharedSources{
        500};

    //! Mask band (e.g. for JPEG compressed + mask band)
    std::unique_ptr<GDALTileIndexBand> m_poMaskBand{};

    //! Whether all bands of the tile index have the same data type.
    bool m_bSameDataType = true;

    //! Whether all bands of the tile index have the same nodata value.
    bool m_bSameNoData = true;

    //! Minimum X of the current pixel request, in georeferenced units.
    double m_dfLastMinXFilter = std::numeric_limits<double>::quiet_NaN();

    //! Minimum Y of the current pixel request, in georeferenced units.
    double m_dfLastMinYFilter = std::numeric_limits<double>::quiet_NaN();

    //! Maximum X of the current pixel request, in georeferenced units.
    double m_dfLastMaxXFilter = std::numeric_limits<double>::quiet_NaN();

    //! Maximum Y of the current pixel request, in georeferenced units.
    double m_dfLastMaxYFilter = std::numeric_limits<double>::quiet_NaN();

    //! Index of the field (within m_poLayer->GetLayerDefn()) used to sort, or -1 if none.
    int m_nSortFieldIndex = -1;

    //! Whether sorting must be ascending (true) or descending (false).
    bool m_bSortFieldAsc = true;

    //! Resampling method by default for warping or when a source has not
    //! the same resolution as the tile index.
    std::string m_osResampling = "near";
    GDALRIOResampleAlg m_eResampling = GRIORA_NearestNeighbour;

    //! WKT2 representation of the tile index SRS (if needed, typically for on-the-fly warping).
    std::string m_osWKT{};

    //! Whether we had to open of the sources at tile index opening.
    bool m_bScannedOneFeatureAtOpening = false;

    //! Array of overview descriptors.
    //! Each descriptor is a tuple (dataset_name, concatenated_open_options, layer_name, overview_factor).
    std::vector<std::tuple<std::string, CPLStringList, std::string, double>>
        m_aoOverviewDescriptor{};

    //! Array of overview datasets.
    std::vector<std::unique_ptr<GDALDataset>> m_apoOverviews{};

    //! Cache of buffers used by VRTComplexSource to avoid memory reallocation.
    VRTSource::WorkingState m_oWorkingState{};

    //! Structure describing one of the source raster in the tile index.
    struct SourceDesc
    {
        //! Source dataset name.
        std::string osName{};

        //! Source dataset handle.
        std::shared_ptr<GDALDataset> poDS{};

        //! VRTSimpleSource or VRTComplexSource for the source.
        std::unique_ptr<VRTSimpleSource> poSource{};

        //! OGRFeature corresponding to the source in the tile index.
        std::unique_ptr<OGRFeature> poFeature{};

        //! Work buffer containing the value of the mask band for the current pixel query.
        std::vector<GByte> abyMask{};

        //! Whether the source covers the whole area of interest of the current pixel query.
        bool bCoversWholeAOI = false;

        //! Whether the source has a nodata value at least in one of its band.
        bool bHasNoData = false;

        //! Whether all bands of the source have the same nodata value.
        bool bSameNoData = false;

        //! Nodata value of all bands (when bSameNoData == true).
        double dfSameNoData = 0;

        //! Mask band of the source.
        GDALRasterBand *poMaskBand = nullptr;
    };

    //! Array of sources participating to the current pixel query.
    std::vector<SourceDesc> m_aoSourceDesc{};

    //! From a source dataset name, return its SourceDesc description structure.
    bool GetSourceDesc(const std::string &osTileName, SourceDesc &oSourceDesc);

    //! Collect sources corresponding to the georeferenced window of interest,
    //! and store them in m_aoSourceDesc[].
    bool CollectSources(double dfXOff, double dfYOff, double dfXSize,
                        double dfYSize);

    //! Sort sources according to m_nSortFieldIndex.
    void SortSourceDesc();

    //! Whether the output buffer needs to be nodata initialized, or if
    //! sources are fully covering it.
    bool NeedInitBuffer(int nBandCount, const int *panBandMap) const;

    //! Nodata initialize the output buffer.
    void InitBuffer(void *pData, int nBufXSize, int nBufYSize,
                    GDALDataType eBufType, int nBandCount,
                    const int *panBandMap, GSpacing nPixelSpace,
                    GSpacing nLineSpace, GSpacing nBandSpace) const;

    //! Whether m_poVectorDS supports SetMetadata()/SetMetadataItem()
    bool TileIndexSupportsEditingLayerMetadata() const;

    CPL_DISALLOW_COPY_ASSIGN(GDALTileIndexDataset)
};

/************************************************************************/
/*                            GDALTileIndexBand                          */
/************************************************************************/

class GDALTileIndexBand final : public GDALPamRasterBand
{
  public:
    GDALTileIndexBand(GDALTileIndexDataset *poDSIn, int nBandIn,
                      GDALDataType eDT, int nBlockXSizeIn, int nBlockYSizeIn);

    double GetNoDataValue(int *pbHasNoData) override
    {
        if (pbHasNoData)
            *pbHasNoData = m_bNoDataValueSet;
        return m_dfNoDataValue;
    }

    GDALColorInterp GetColorInterpretation() override
    {
        return m_eColorInterp;
    }

    CPLErr IReadBlock(int nBlockXOff, int nBlockYOff, void *pData) override;

    CPLErr IRasterIO(GDALRWFlag eRWFlag, int nXOff, int nYOff, int nXSize,
                     int nYSize, void *pData, int nBufXSize, int nBufYSize,
                     GDALDataType eBufType, GSpacing nPixelSpace,
                     GSpacing nLineSpace,
                     GDALRasterIOExtraArg *psExtraArg) override;

    int GetMaskFlags() override
    {
        if (m_poDS->m_poMaskBand && m_poDS->m_poMaskBand.get() != this)
            return GMF_PER_DATASET;
        return GDALPamRasterBand::GetMaskFlags();
    }

    GDALRasterBand *GetMaskBand() override
    {
        if (m_poDS->m_poMaskBand && m_poDS->m_poMaskBand.get() != this)
            return m_poDS->m_poMaskBand.get();
        return GDALPamRasterBand::GetMaskBand();
    }

    double GetOffset(int *pbHasValue) override
    {
        int bHasValue = FALSE;
        double dfVal = GDALPamRasterBand::GetOffset(&bHasValue);
        if (bHasValue)
        {
            if (pbHasValue)
                *pbHasValue = true;
            return dfVal;
        }
        if (pbHasValue)
            *pbHasValue = !std::isnan(m_dfOffset);
        return std::isnan(m_dfOffset) ? 0.0 : m_dfOffset;
    }

    double GetScale(int *pbHasValue) override
    {
        int bHasValue = FALSE;
        double dfVal = GDALPamRasterBand::GetScale(&bHasValue);
        if (bHasValue)
        {
            if (pbHasValue)
                *pbHasValue = true;
            return dfVal;
        }
        if (pbHasValue)
            *pbHasValue = !std::isnan(m_dfScale);
        return std::isnan(m_dfScale) ? 1.0 : m_dfScale;
    }

    const char *GetUnitType() override
    {
        const char *pszVal = GDALPamRasterBand::GetUnitType();
        if (pszVal && *pszVal)
            return pszVal;
        return m_osUnit.c_str();
    }

    char **GetCategoryNames() override
    {
        return m_aosCategoryNames.List();
    }

    GDALColorTable *GetColorTable() override
    {
        return m_poColorTable.get();
    }

    GDALRasterAttributeTable *GetDefaultRAT() override
    {
        return m_poRAT.get();
    }

    int GetOverviewCount() override;
    GDALRasterBand *GetOverview(int iOvr) override;

    char **GetMetadataDomainList() override;
    const char *GetMetadataItem(const char *pszName,
                                const char *pszDomain) override;
    CPLErr SetMetadataItem(const char *pszName, const char *pszValue,
                           const char *pszDomain) override;
    CPLErr SetMetadata(char **papszMD, const char *pszDomain) override;

  private:
    friend class GDALTileIndexDataset;

    //! Dataset that owns this band.
    GDALTileIndexDataset *m_poDS = nullptr;

    //! Whether a nodata value is set to this band.
    bool m_bNoDataValueSet = false;

    //! Nodata value.
    double m_dfNoDataValue = 0;

    //! Color interpretation.
    GDALColorInterp m_eColorInterp = GCI_Undefined;

    //! Cached value for GetMetadataItem("Pixel_X_Y", "LocationInfo").
    std::string m_osLastLocationInfo{};

    //! Scale value (returned by GetScale())
    double m_dfScale = std::numeric_limits<double>::quiet_NaN();

    //! Offset value (returned by GetOffset())
    double m_dfOffset = std::numeric_limits<double>::quiet_NaN();

    //! Unit type (returned by GetUnitType()).
    std::string m_osUnit{};

    //! Category names (returned by GetCategoryNames()).
    CPLStringList m_aosCategoryNames{};

    //! Color table (returned by GetColorTable()).
    std::unique_ptr<GDALColorTable> m_poColorTable{};

    //! Raster attribute table (returned by GetDefaultRAT()).
    std::unique_ptr<GDALRasterAttributeTable> m_poRAT{};

    CPL_DISALLOW_COPY_ASSIGN(GDALTileIndexBand)
};

/************************************************************************/
/*                        IsSameNaNAware()                              */
/************************************************************************/

static inline bool IsSameNaNAware(double a, double b)
{
    return a == b || (std::isnan(a) && std::isnan(b));
}

/************************************************************************/
/*                         GDALTileIndexDataset()                        */
/************************************************************************/

GDALTileIndexDataset::GDALTileIndexDataset()
    : m_osUniqueHandle(CPLSPrintf("%p", this))
{
}

/************************************************************************/
/*                        GetAbsoluteFileName()                         */
/************************************************************************/

static std::string GetAbsoluteFileName(const char *pszTileName,
                                       const char *pszVRTName)
{
    if (CPLIsFilenameRelative(pszTileName) &&
        !STARTS_WITH(pszTileName, "<VRTDataset") &&
        !STARTS_WITH(pszVRTName, "<GDALTileIndexDataset"))
    {
        const auto oSubDSInfo(GDALGetSubdatasetInfo(pszTileName));
        if (oSubDSInfo && !oSubDSInfo->GetPathComponent().empty())
        {
            const std::string osPath(oSubDSInfo->GetPathComponent());
            const std::string osRet =
                CPLIsFilenameRelative(osPath.c_str())
                    ? oSubDSInfo->ModifyPathComponent(
                          CPLProjectRelativeFilename(CPLGetPath(pszVRTName),
                                                     osPath.c_str()))
                    : std::string(pszTileName);
            GDALDestroySubdatasetInfo(oSubDSInfo);
            return osRet;
        }

        const std::string osRelativeMadeAbsolute =
            CPLProjectRelativeFilename(CPLGetPath(pszVRTName), pszTileName);
        VSIStatBufL sStat;
        if (VSIStatL(osRelativeMadeAbsolute.c_str(), &sStat) == 0)
            return osRelativeMadeAbsolute;
    }
    return pszTileName;
}

/************************************************************************/
/*                    GTIDoPaletteExpansionIfNeeded()                   */
/************************************************************************/

//! Do palette -> RGB(A) expansion
static bool
GTIDoPaletteExpansionIfNeeded(std::shared_ptr<GDALDataset> &poTileDS,
                              int nBandCount)
{
    if (poTileDS->GetRasterCount() == 1 &&
        (nBandCount == 3 || nBandCount == 4) &&
        poTileDS->GetRasterBand(1)->GetColorTable() != nullptr)
    {

        CPLStringList aosOptions;
        aosOptions.AddString("-of");
        aosOptions.AddString("VRT");

        aosOptions.AddString("-expand");
        aosOptions.AddString(nBandCount == 3 ? "rgb" : "rgba");

        GDALTranslateOptions *psOptions =
            GDALTranslateOptionsNew(aosOptions.List(), nullptr);
        int bUsageError = false;
        auto poRGBDS = std::unique_ptr<GDALDataset>(GDALDataset::FromHandle(
            GDALTranslate("", GDALDataset::ToHandle(poTileDS.get()), psOptions,
                          &bUsageError)));
        GDALTranslateOptionsFree(psOptions);
        if (!poRGBDS)
        {
            return false;
        }

        poTileDS.reset(poRGBDS.release());
    }
    return true;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

bool GDALTileIndexDataset::Open(GDALOpenInfo *poOpenInfo)
{
    eAccess = poOpenInfo->eAccess;

    CPLXMLNode *psRoot = nullptr;
    const char *pszIndexDataset = poOpenInfo->pszFilename;

    if (STARTS_WITH(poOpenInfo->pszFilename, GTI_PREFIX))
    {
        pszIndexDataset = poOpenInfo->pszFilename + strlen(GTI_PREFIX);
    }
    else if (STARTS_WITH(poOpenInfo->pszFilename, "<GDALTileIndexDataset"))
    {
        // CPLParseXMLString() emits an error in case of failure
        m_psXMLTree.reset(CPLParseXMLString(poOpenInfo->pszFilename));
        if (m_psXMLTree == nullptr)
            return false;
    }
    else if (poOpenInfo->nHeaderBytes > 0 &&
             strstr(reinterpret_cast<const char *>(poOpenInfo->pabyHeader),
                    "<GDALTileIndexDataset"))
    {
        // CPLParseXMLFile() emits an error in case of failure
        m_psXMLTree.reset(CPLParseXMLFile(poOpenInfo->pszFilename));
        if (m_psXMLTree == nullptr)
            return false;
        m_bXMLUpdatable = (poOpenInfo->eAccess == GA_Update);
    }

    if (m_psXMLTree)
    {
        psRoot = CPLGetXMLNode(m_psXMLTree.get(), "=GDALTileIndexDataset");
        if (psRoot == nullptr)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Missing GDALTileIndexDataset root element.");
            return false;
        }

        pszIndexDataset = CPLGetXMLValue(psRoot, "IndexDataset", nullptr);
        if (!pszIndexDataset)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Missing IndexDataset element.");
            return false;
        }
    }

    if (ENDS_WITH_CI(pszIndexDataset, ".gti.gpkg") &&
        poOpenInfo->nHeaderBytes >= 100 &&
        STARTS_WITH(reinterpret_cast<const char *>(poOpenInfo->pabyHeader),
                    "SQLite format 3"))
    {
        const char *const apszAllowedDrivers[] = {"GPKG", nullptr};
        m_poVectorDS.reset(GDALDataset::Open(
            std::string("GPKG:\"").append(pszIndexDataset).append("\"").c_str(),
            GDAL_OF_VECTOR | GDAL_OF_RASTER | GDAL_OF_VERBOSE_ERROR |
                ((poOpenInfo->nOpenFlags & GDAL_OF_UPDATE) ? GDAL_OF_UPDATE
                                                           : GDAL_OF_READONLY),
            apszAllowedDrivers));
        if (!m_poVectorDS)
        {
            return false;
        }
        if (m_poVectorDS->GetLayerCount() == 0 &&
            (m_poVectorDS->GetRasterCount() != 0 ||
             m_poVectorDS->GetMetadata("SUBDATASETS") != nullptr))
        {
            return false;
        }
    }
    else
    {
        m_poVectorDS.reset(GDALDataset::Open(
            pszIndexDataset, GDAL_OF_VECTOR | GDAL_OF_VERBOSE_ERROR |
                                 ((poOpenInfo->nOpenFlags & GDAL_OF_UPDATE)
                                      ? GDAL_OF_UPDATE
                                      : GDAL_OF_READONLY)));
        if (!m_poVectorDS)
        {
            return false;
        }
    }

    if (m_poVectorDS->GetLayerCount() == 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s has no vector layer",
                 poOpenInfo->pszFilename);
        return false;
    }

    double dfOvrFactor = 1.0;
    if (const char *pszFactor =
            CSLFetchNameValue(poOpenInfo->papszOpenOptions, "FACTOR"))
    {
        dfOvrFactor = CPLAtof(pszFactor);
        if (!(dfOvrFactor > 1.0))
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Wrong overview factor");
            return false;
        }
    }

    const char *pszLayerName;

    if ((pszLayerName = CSLFetchNameValue(poOpenInfo->papszOpenOptions,
                                          "LAYER")) != nullptr)
    {
        m_poLayer = m_poVectorDS->GetLayerByName(pszLayerName);
        if (!m_poLayer)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Layer %s does not exist",
                     pszLayerName);
            return false;
        }
    }
    else if (psRoot && (pszLayerName = CPLGetXMLValue(psRoot, "IndexLayer",
                                                      nullptr)) != nullptr)
    {
        m_poLayer = m_poVectorDS->GetLayerByName(pszLayerName);
        if (!m_poLayer)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Layer %s does not exist",
                     pszLayerName);
            return false;
        }
    }
    else if (!psRoot && (pszLayerName = m_poVectorDS->GetMetadataItem(
                             MD_DS_TILE_INDEX_LAYER)) != nullptr)
    {
        m_poLayer = m_poVectorDS->GetLayerByName(pszLayerName);
        if (!m_poLayer)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Layer %s does not exist",
                     pszLayerName);
            return false;
        }
    }
    else if (m_poVectorDS->GetLayerCount() == 1)
    {
        m_poLayer = m_poVectorDS->GetLayer(0);
        if (!m_poLayer)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Cannot open layer 0");
            return false;
        }
    }
    else
    {
        if (STARTS_WITH(poOpenInfo->pszFilename, GTI_PREFIX))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "%s has more than one layer. LAYER open option "
                     "must be defined to specify which one to "
                     "use as the tile index",
                     pszIndexDataset);
        }
        else if (psRoot)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "%s has more than one layer. IndexLayer element must be "
                     "defined to specify which one to "
                     "use as the tile index",
                     pszIndexDataset);
        }
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "%s has more than one layer. %s "
                     "metadata item must be defined to specify which one to "
                     "use as the tile index",
                     pszIndexDataset, MD_DS_TILE_INDEX_LAYER);
        }
        return false;
    }

    // Try to get the metadata from an embedded xml:GTI domain
    if (!m_psXMLTree)
    {
        char **papszMD = m_poLayer->GetMetadata("xml:GTI");
        if (papszMD && papszMD[0])
        {
            m_psXMLTree.reset(CPLParseXMLString(papszMD[0]));
            if (m_psXMLTree == nullptr)
                return false;

            psRoot = CPLGetXMLNode(m_psXMLTree.get(), "=GDALTileIndexDataset");
            if (psRoot == nullptr)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Missing GDALTileIndexDataset root element.");
                return false;
            }
        }
    }

    // Get the value of an option.
    // The order of lookup is the following one (first to last):
    // - open options
    // - XML file
    // - Layer metadata items.
    const auto GetOption = [poOpenInfo, psRoot, this](const char *pszItem)
    {
        const char *pszVal =
            CSLFetchNameValue(poOpenInfo->papszOpenOptions, pszItem);
        if (pszVal)
            return pszVal;

        if (psRoot)
        {
            pszVal = CPLGetXMLValue(psRoot, pszItem, nullptr);
            if (pszVal)
                return pszVal;

            if (EQUAL(pszItem, MD_BAND_COUNT))
                pszItem = GTI_XML_BANDCOUNT;
            else if (EQUAL(pszItem, MD_DATA_TYPE))
                pszItem = GTI_XML_DATATYPE;
            else if (EQUAL(pszItem, MD_NODATA))
                pszItem = GTI_XML_NODATAVALUE;
            else if (EQUAL(pszItem, MD_COLOR_INTERPRETATION))
                pszItem = GTI_XML_COLORINTERP;
            else if (EQUAL(pszItem, MD_LOCATION_FIELD))
                pszItem = GTI_XML_LOCATIONFIELD;
            else if (EQUAL(pszItem, MD_SORT_FIELD))
                pszItem = GTI_XML_SORTFIELD;
            else if (EQUAL(pszItem, MD_SORT_FIELD_ASC))
                pszItem = GTI_XML_SORTFIELDASC;
            else if (EQUAL(pszItem, MD_MASK_BAND))
                pszItem = GTI_XML_MASKBAND;
            pszVal = CPLGetXMLValue(psRoot, pszItem, nullptr);
            if (pszVal)
                return pszVal;
        }

        return m_poLayer->GetMetadataItem(pszItem);
    };

    const char *pszFilter = GetOption("Filter");
    if (pszFilter)
    {
        if (m_poLayer->SetAttributeFilter(pszFilter) != OGRERR_NONE)
            return false;
    }

    const char *pszLocationFieldName = GetOption(MD_LOCATION_FIELD);
    if (!pszLocationFieldName)
    {
        constexpr const char *DEFAULT_LOCATION_FIELD_NAME = "location";
        pszLocationFieldName = DEFAULT_LOCATION_FIELD_NAME;
    }
    auto poLayerDefn = m_poLayer->GetLayerDefn();
    m_nLocationFieldIndex = poLayerDefn->GetFieldIndex(pszLocationFieldName);
    if (m_nLocationFieldIndex < 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot find field %s",
                 pszLocationFieldName);
        return false;
    }
    if (poLayerDefn->GetFieldDefn(m_nLocationFieldIndex)->GetType() !=
        OFTString)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Field %s is not of type string",
                 pszLocationFieldName);
        return false;
    }

    const char *pszSortFieldName = GetOption(MD_SORT_FIELD);
    if (pszSortFieldName)
    {
        m_nSortFieldIndex = poLayerDefn->GetFieldIndex(pszSortFieldName);
        if (m_nSortFieldIndex < 0)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Cannot find field %s",
                     pszSortFieldName);
            return false;
        }

        const auto eFieldType =
            poLayerDefn->GetFieldDefn(m_nSortFieldIndex)->GetType();
        if (eFieldType != OFTString && eFieldType != OFTInteger &&
            eFieldType != OFTInteger64 && eFieldType != OFTReal &&
            eFieldType != OFTDate && eFieldType != OFTDateTime)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Unsupported type for field %s", pszSortFieldName);
            return false;
        }

        const char *pszSortFieldAsc = GetOption(MD_SORT_FIELD_ASC);
        if (pszSortFieldAsc)
        {
            m_bSortFieldAsc = CPLTestBool(pszSortFieldAsc);
        }
    }

    const char *pszResX = GetOption(MD_RESX);
    const char *pszResY = GetOption(MD_RESY);
    if (pszResX && !pszResY)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "%s metadata item defined, but not %s", MD_RESX, MD_RESY);
        return false;
    }
    if (!pszResX && pszResY)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "%s metadata item defined, but not %s", MD_RESY, MD_RESX);
        return false;
    }

    const char *pszResampling = GetOption(MD_RESAMPLING);
    if (pszResampling)
    {
        const auto nErrorCountBefore = CPLGetErrorCounter();
        m_eResampling = GDALRasterIOGetResampleAlg(pszResampling);
        if (nErrorCountBefore != CPLGetErrorCounter())
        {
            return false;
        }
        m_osResampling = pszResampling;
    }

    const char *pszMinX = GetOption(MD_MINX);
    const char *pszMinY = GetOption(MD_MINY);
    const char *pszMaxX = GetOption(MD_MAXX);
    const char *pszMaxY = GetOption(MD_MAXY);
    const int nCountMinMaxXY = (pszMinX ? 1 : 0) + (pszMinY ? 1 : 0) +
                               (pszMaxX ? 1 : 0) + (pszMaxY ? 1 : 0);
    if (nCountMinMaxXY != 0 && nCountMinMaxXY != 4)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "None or all of %s, %s, %s and %s must be specified", MD_MINX,
                 MD_MINY, MD_MAXX, MD_MAXY);
        return false;
    }

    const char *pszXSize = GetOption(MD_XSIZE);
    const char *pszYSize = GetOption(MD_YSIZE);
    const char *pszGeoTransform = GetOption(MD_GEOTRANSFORM);
    const int nCountXSizeYSizeGT =
        (pszXSize ? 1 : 0) + (pszYSize ? 1 : 0) + (pszGeoTransform ? 1 : 0);
    if (nCountXSizeYSizeGT != 0 && nCountXSizeYSizeGT != 3)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "None or all of %s, %s, %s must be specified", MD_XSIZE,
                 MD_YSIZE, MD_GEOTRANSFORM);
        return false;
    }

    const char *pszDataType = GetOption(MD_DATA_TYPE);
    const char *pszColorInterp = GetOption(MD_COLOR_INTERPRETATION);
    int nBandCount = 0;
    std::vector<GDALDataType> aeDataTypes;
    std::vector<std::pair<bool, double>> aNoData;
    std::vector<GDALColorInterp> aeColorInterp;

    const char *pszSRS = GetOption(MD_SRS);
    m_oSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    if (pszSRS)
    {
        if (m_oSRS.SetFromUserInput(
                pszSRS,
                OGRSpatialReference::SET_FROM_USER_INPUT_LIMITATIONS_get()) !=
            OGRERR_NONE)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Invalid %s", MD_SRS);
            return false;
        }
    }
    else
    {
        const auto poSRS = m_poLayer->GetSpatialRef();
        // Ignore GPKG "Undefined geographic SRS" and "Undefined Cartesian SRS"
        if (poSRS && !STARTS_WITH(poSRS->GetName(), "Undefined "))
            m_oSRS = *poSRS;
    }

    std::vector<const CPLXMLNode *> apoXMLNodeBands;
    if (psRoot)
    {
        int nExpectedBandNumber = 1;
        for (const CPLXMLNode *psIter = psRoot->psChild; psIter;
             psIter = psIter->psNext)
        {
            if (psIter->eType == CXT_Element &&
                strcmp(psIter->pszValue, GTI_XML_BAND_ELEMENT) == 0)
            {
                const char *pszBand =
                    CPLGetXMLValue(psIter, GTI_XML_BAND_NUMBER, nullptr);
                if (!pszBand)
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "%s attribute missing on %s element",
                             GTI_XML_BAND_NUMBER, GTI_XML_BAND_ELEMENT);
                    return false;
                }
                const int nBand = atoi(pszBand);
                if (nBand <= 0)
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Invalid band number");
                    return false;
                }
                if (nBand != nExpectedBandNumber)
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Invalid band number: found %d, expected %d",
                             nBand, nExpectedBandNumber);
                    return false;
                }
                apoXMLNodeBands.push_back(psIter);
                ++nExpectedBandNumber;
            }
        }
    }

    const char *pszBandCount = GetOption(MD_BAND_COUNT);
    if (pszBandCount)
        nBandCount = atoi(pszBandCount);

    if (!apoXMLNodeBands.empty())
    {
        if (!pszBandCount)
            nBandCount = static_cast<int>(apoXMLNodeBands.size());
        else if (nBandCount != static_cast<int>(apoXMLNodeBands.size()))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Inconsistent %s with actual number of %s elements",
                     GTI_XML_BANDCOUNT, GTI_XML_BAND_ELEMENT);
            return false;
        }
    }

    bool bHasMaskBand = false;
    if ((!pszBandCount && apoXMLNodeBands.empty()) ||
        (!(pszResX && pszResY) && nCountXSizeYSizeGT == 0))
    {
        CPLDebug("VRT", "Inspecting one feature due to missing metadata items");
        m_bScannedOneFeatureAtOpening = true;

        auto poFeature =
            std::unique_ptr<OGRFeature>(m_poLayer->GetNextFeature());
        if (!poFeature ||
            !poFeature->IsFieldSetAndNotNull(m_nLocationFieldIndex))
        {
            CPLError(
                CE_Failure, CPLE_AppDefined,
                "BAND_COUNT(+DATA_TYPE+COLOR_INTERPRETATION)+ (RESX+RESY or "
                "XSIZE+YSIZE+GEOTRANSFORM) metadata items "
                "missing");
            return false;
        }

        const char *pszTileName =
            poFeature->GetFieldAsString(m_nLocationFieldIndex);
        const std::string osTileName(
            GetAbsoluteFileName(pszTileName, poOpenInfo->pszFilename));
        pszTileName = osTileName.c_str();

        auto poTileDS = std::shared_ptr<GDALDataset>(
            GDALDataset::Open(pszTileName,
                              GDAL_OF_RASTER | GDAL_OF_VERBOSE_ERROR),
            GDALDatasetUniquePtrReleaser());
        if (!poTileDS)
        {
            return false;
        }

        // do palette -> RGB(A) expansion if needed
        if (!GTIDoPaletteExpansionIfNeeded(poTileDS, nBandCount))
            return false;

        const int nTileBandCount = poTileDS->GetRasterCount();
        for (int i = 0; i < nTileBandCount; ++i)
        {
            auto poTileBand = poTileDS->GetRasterBand(i + 1);
            aeDataTypes.push_back(poTileBand->GetRasterDataType());
            int bHasNoData = FALSE;
            const double dfNoData = poTileBand->GetNoDataValue(&bHasNoData);
            aNoData.emplace_back(CPL_TO_BOOL(bHasNoData), dfNoData);
            aeColorInterp.push_back(poTileBand->GetColorInterpretation());

            if (poTileBand->GetMaskFlags() == GMF_PER_DATASET)
                bHasMaskBand = true;
        }
        if (!pszBandCount && nBandCount == 0)
            nBandCount = nTileBandCount;

        auto poTileSRS = poTileDS->GetSpatialRef();
        if (!m_oSRS.IsEmpty() && poTileSRS && !m_oSRS.IsSame(poTileSRS))
        {
            CPLStringList aosOptions;
            aosOptions.AddString("-of");
            aosOptions.AddString("VRT");

            char *pszWKT = nullptr;
            const char *const apszWKTOptions[] = {"FORMAT=WKT2_2019", nullptr};
            m_oSRS.exportToWkt(&pszWKT, apszWKTOptions);
            if (pszWKT)
                m_osWKT = pszWKT;
            CPLFree(pszWKT);

            if (m_osWKT.empty())
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Cannot export VRT SRS to WKT2");
                return false;
            }

            aosOptions.AddString("-t_srs");
            aosOptions.AddString(m_osWKT.c_str());

            GDALWarpAppOptions *psWarpOptions =
                GDALWarpAppOptionsNew(aosOptions.List(), nullptr);
            GDALDatasetH ahSrcDS[] = {GDALDataset::ToHandle(poTileDS.get())};
            int bUsageError = false;
            auto poWarpDS =
                std::unique_ptr<GDALDataset>(GDALDataset::FromHandle(GDALWarp(
                    "", nullptr, 1, ahSrcDS, psWarpOptions, &bUsageError)));
            GDALWarpAppOptionsFree(psWarpOptions);
            if (!poWarpDS)
            {
                return false;
            }

            poTileDS.reset(poWarpDS.release());
            poTileSRS = poTileDS->GetSpatialRef();
            CPL_IGNORE_RET_VAL(poTileSRS);
        }

        double adfGeoTransformTile[6];
        if (poTileDS->GetGeoTransform(adfGeoTransformTile) != CE_None)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cannot find geotransform on %s", pszTileName);
            return false;
        }
        if (!(adfGeoTransformTile[GT_ROTATION_PARAM1] == 0))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "3rd value of GeoTransform of %s must be 0", pszTileName);
            return false;
        }
        if (!(adfGeoTransformTile[GT_ROTATION_PARAM2] == 0))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "5th value of GeoTransform of %s must be 0", pszTileName);
            return false;
        }
        if (!(adfGeoTransformTile[GT_NS_RES] < 0))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "6th value of GeoTransform of %s must be < 0",
                     pszTileName);
            return false;
        }

        const double dfResX = adfGeoTransformTile[GT_WE_RES];
        const double dfResY = -adfGeoTransformTile[GT_NS_RES];

        OGREnvelope sEnvelope;
        if (m_poLayer->GetExtent(&sEnvelope, /* bForce = */ false) ==
            OGRERR_FAILURE)
        {
            if (m_poLayer->GetExtent(&sEnvelope, /* bForce = */ true) ==
                OGRERR_FAILURE)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Cannot get layer extent");
                return false;
            }
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Could get layer extent, but using a slower method");
        }

        const double dfXSize = (sEnvelope.MaxX - sEnvelope.MinX) / dfResX;
        if (!(dfXSize >= 0 && dfXSize < INT_MAX))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Too small %s, or wrong layer extent", MD_RESX);
            return false;
        }

        const double dfYSize = (sEnvelope.MaxY - sEnvelope.MinY) / dfResY;
        if (!(dfYSize >= 0 && dfYSize < INT_MAX))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Too small %s, or wrong layer extent", MD_RESY);
            return false;
        }

        m_adfGeoTransform[GT_TOPLEFT_X] = sEnvelope.MinX;
        m_adfGeoTransform[GT_WE_RES] = dfResX;
        m_adfGeoTransform[GT_ROTATION_PARAM1] = 0;
        m_adfGeoTransform[GT_TOPLEFT_Y] = sEnvelope.MaxY;
        m_adfGeoTransform[GT_ROTATION_PARAM2] = 0;
        m_adfGeoTransform[GT_NS_RES] = -dfResY;
        nRasterXSize = static_cast<int>(std::ceil(dfXSize));
        nRasterYSize = static_cast<int>(std::ceil(dfYSize));
    }

    if (pszXSize && pszYSize && pszGeoTransform)
    {
        const int nXSize = atoi(pszXSize);
        if (nXSize <= 0)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "%s metadata item must be > 0", MD_XSIZE);
            return false;
        }

        const int nYSize = atoi(pszYSize);
        if (nYSize <= 0)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "%s metadata item must be > 0", MD_YSIZE);
            return false;
        }

        const CPLStringList aosTokens(
            CSLTokenizeString2(pszGeoTransform, ",", 0));
        if (aosTokens.size() != 6)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "%s metadata item must be 6 numeric values "
                     "separated with comma",
                     MD_GEOTRANSFORM);
            return false;
        }
        for (int i = 0; i < 6; ++i)
        {
            m_adfGeoTransform[i] = CPLAtof(aosTokens[i]);
        }
        if (!(m_adfGeoTransform[GT_ROTATION_PARAM1] == 0))
        {
            CPLError(CE_Failure, CPLE_AppDefined, "3rd value of %s must be 0",
                     MD_GEOTRANSFORM);
            return false;
        }
        if (!(m_adfGeoTransform[GT_ROTATION_PARAM2] == 0))
        {
            CPLError(CE_Failure, CPLE_AppDefined, "5th value of %s must be 0",
                     MD_GEOTRANSFORM);
            return false;
        }
        if (!(m_adfGeoTransform[GT_NS_RES] < 0))
        {
            CPLError(CE_Failure, CPLE_AppDefined, "6th value of %s must be < 0",
                     MD_GEOTRANSFORM);
            return false;
        }

        nRasterXSize = nXSize;
        nRasterYSize = nYSize;
    }
    else if (pszResX && pszResY)
    {
        const double dfResX = CPLAtof(pszResX);
        if (!(dfResX > 0))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "RESX metadata item must be > 0");
            return false;
        }
        const double dfResY = CPLAtof(pszResY);
        if (!(dfResY > 0))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "RESY metadata item must be > 0");
            return false;
        }

        OGREnvelope sEnvelope;

        if (nCountMinMaxXY == 4)
        {
            if (pszXSize || pszYSize || pszGeoTransform)
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Ignoring %s, %s and %s when %s, "
                         "%s, %s and %s are specified",
                         MD_XSIZE, MD_YSIZE, MD_GEOTRANSFORM, MD_MINX, MD_MINY,
                         MD_MAXX, MD_MAXY);
            }
            const double dfMinX = CPLAtof(pszMinX);
            const double dfMinY = CPLAtof(pszMinY);
            const double dfMaxX = CPLAtof(pszMaxX);
            const double dfMaxY = CPLAtof(pszMaxY);
            if (!(dfMaxX > dfMinX))
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "%s metadata item must be > %s", MD_MAXX, MD_MINX);
                return false;
            }
            if (!(dfMaxY > dfMinY))
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "%s metadata item must be > %s", MD_MAXY, MD_MINY);
                return false;
            }
            sEnvelope.MinX = dfMinX;
            sEnvelope.MinY = dfMinY;
            sEnvelope.MaxX = dfMaxX;
            sEnvelope.MaxY = dfMaxY;
        }
        else if (m_poLayer->GetExtent(&sEnvelope, /* bForce = */ false) ==
                 OGRERR_FAILURE)
        {
            if (m_poLayer->GetExtent(&sEnvelope, /* bForce = */ true) ==
                OGRERR_FAILURE)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Cannot get layer extent");
                return false;
            }
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Could get layer extent, but using a slower method");
        }

        const double dfXSize = (sEnvelope.MaxX - sEnvelope.MinX) / dfResX;
        if (!(dfXSize >= 0 && dfXSize < INT_MAX))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Too small %s, or wrong layer extent", MD_RESX);
            return false;
        }

        const double dfYSize = (sEnvelope.MaxY - sEnvelope.MinY) / dfResY;
        if (!(dfYSize >= 0 && dfYSize < INT_MAX))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Too small %s, or wrong layer extent", MD_RESY);
            return false;
        }

        m_adfGeoTransform[GT_TOPLEFT_X] = sEnvelope.MinX;
        m_adfGeoTransform[GT_WE_RES] = dfResX;
        m_adfGeoTransform[GT_ROTATION_PARAM1] = 0;
        m_adfGeoTransform[GT_TOPLEFT_Y] = sEnvelope.MaxY;
        m_adfGeoTransform[GT_ROTATION_PARAM2] = 0;
        m_adfGeoTransform[GT_NS_RES] = -dfResY;
        nRasterXSize = static_cast<int>(std::ceil(dfXSize));
        nRasterYSize = static_cast<int>(std::ceil(dfYSize));
    }

    if (nBandCount == 0 && !pszBandCount)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s metadata item missing",
                 MD_BAND_COUNT);
        return false;
    }

    if (!GDALCheckBandCount(nBandCount, false))
        return false;

    if (aeDataTypes.empty() && !pszDataType)
    {
        aeDataTypes.resize(nBandCount, GDT_Byte);
    }
    else if (pszDataType)
    {
        aeDataTypes.clear();
        const CPLStringList aosTokens(CSLTokenizeString2(pszDataType, ", ", 0));
        if (aosTokens.size() == 1)
        {
            const auto eDataType = GDALGetDataTypeByName(aosTokens[0]);
            if (eDataType == GDT_Unknown)
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Invalid value for %s",
                         MD_DATA_TYPE);
                return false;
            }
            aeDataTypes.resize(nBandCount, eDataType);
        }
        else if (aosTokens.size() == nBandCount)
        {
            for (int i = 0; i < nBandCount; ++i)
            {
                const auto eDataType = GDALGetDataTypeByName(aosTokens[i]);
                if (eDataType == GDT_Unknown)
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Invalid value for %s", MD_DATA_TYPE);
                    return false;
                }
                aeDataTypes.push_back(eDataType);
            }
        }
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Number of values in %s must be 1 or %s", MD_DATA_TYPE,
                     MD_BAND_COUNT);
            return false;
        }
    }

    const char *pszNoData = GetOption(MD_NODATA);
    if (pszNoData)
    {
        const auto IsValidNoDataStr = [](const char *pszStr)
        {
            if (EQUAL(pszStr, "inf") || EQUAL(pszStr, "-inf") ||
                EQUAL(pszStr, "nan"))
                return true;
            const auto eType = CPLGetValueType(pszStr);
            return eType == CPL_VALUE_INTEGER || eType == CPL_VALUE_REAL;
        };

        aNoData.clear();
        const CPLStringList aosTokens(CSLTokenizeString2(pszNoData, ", ", 0));
        if (aosTokens.size() == 1)
        {
            if (!EQUAL(aosTokens[0], "NONE"))
            {
                if (!IsValidNoDataStr(aosTokens[0]))
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Invalid value for %s", MD_NODATA);
                    return false;
                }
                aNoData.resize(nBandCount,
                               std::pair(true, CPLAtof(aosTokens[0])));
            }
        }
        else if (aosTokens.size() == nBandCount)
        {
            for (int i = 0; i < nBandCount; ++i)
            {
                if (EQUAL(aosTokens[i], "NONE"))
                {
                    aNoData.emplace_back(false, 0);
                }
                else if (IsValidNoDataStr(aosTokens[i]))
                {
                    aNoData.emplace_back(true, CPLAtof(aosTokens[i]));
                }
                else
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Invalid value for %s", MD_NODATA);
                    return false;
                }
            }
        }
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Number of values in %s must be 1 or %s", MD_NODATA,
                     MD_BAND_COUNT);
            return false;
        }
    }

    if (pszColorInterp)
    {
        aeColorInterp.clear();
        const CPLStringList aosTokens(
            CSLTokenizeString2(pszColorInterp, ", ", 0));
        if (aosTokens.size() == 1)
        {
            const auto eInterp = GDALGetColorInterpretationByName(aosTokens[0]);
            if (eInterp == GCI_Undefined &&
                !EQUAL(aosTokens[0],
                       GDALGetColorInterpretationName(GCI_Undefined)))
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Invalid value for %s",
                         MD_COLOR_INTERPRETATION);
                return false;
            }
            aeColorInterp.resize(nBandCount, eInterp);
        }
        else if (aosTokens.size() == nBandCount)
        {
            for (int i = 0; i < nBandCount; ++i)
            {
                const auto eInterp =
                    GDALGetColorInterpretationByName(aosTokens[i]);
                if (eInterp == GCI_Undefined &&
                    !EQUAL(aosTokens[i],
                           GDALGetColorInterpretationName(GCI_Undefined)))
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Invalid value for %s", MD_COLOR_INTERPRETATION);
                    return false;
                }
                aeColorInterp.emplace_back(eInterp);
            }
        }
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Number of values in %s must be 1 or "
                     "%s",
                     MD_COLOR_INTERPRETATION, MD_BAND_COUNT);
            return false;
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Create bands.                                                   */
    /* -------------------------------------------------------------------- */
    if (aeDataTypes.size() != static_cast<size_t>(nBandCount))
    {
        CPLError(
            CE_Failure, CPLE_AppDefined,
            "Number of data types values found not matching number of bands");
        return false;
    }
    if (!aNoData.empty() && aNoData.size() != static_cast<size_t>(nBandCount))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Number of nodata values found not matching number of bands");
        return false;
    }
    if (!aeColorInterp.empty() &&
        aeColorInterp.size() != static_cast<size_t>(nBandCount))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Number of color interpretation values found not matching "
                 "number of bands");
        return false;
    }

    int nBlockXSize = 256;
    const char *pszBlockXSize = GetOption(MD_BLOCK_X_SIZE);
    if (pszBlockXSize)
    {
        nBlockXSize = atoi(pszBlockXSize);
        if (nBlockXSize <= 0)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Invalid %s",
                     MD_BLOCK_X_SIZE);
            return false;
        }
    }

    int nBlockYSize = 256;
    const char *pszBlockYSize = GetOption(MD_BLOCK_Y_SIZE);
    if (pszBlockYSize)
    {
        nBlockYSize = atoi(pszBlockYSize);
        if (nBlockYSize <= 0)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Invalid %s",
                     MD_BLOCK_Y_SIZE);
            return false;
        }
    }

    if (nBlockXSize > INT_MAX / nBlockYSize)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Too big %s * %s",
                 MD_BLOCK_X_SIZE, MD_BLOCK_Y_SIZE);
        return false;
    }

    if (dfOvrFactor > 1.0)
    {
        m_adfGeoTransform[GT_WE_RES] *= dfOvrFactor;
        m_adfGeoTransform[GT_NS_RES] *= dfOvrFactor;
        nRasterXSize = static_cast<int>(std::ceil(nRasterXSize / dfOvrFactor));
        nRasterYSize = static_cast<int>(std::ceil(nRasterYSize / dfOvrFactor));
    }

    GDALTileIndexBand *poFirstBand = nullptr;
    for (int i = 0; i < nBandCount; ++i)
    {
        GDALDataType eDataType = aeDataTypes[i];
        if (!apoXMLNodeBands.empty())
        {
            const char *pszVal = CPLGetXMLValue(apoXMLNodeBands[i],
                                                GTI_XML_BAND_DATATYPE, nullptr);
            if (pszVal)
            {
                eDataType = GDALGetDataTypeByName(pszVal);
                if (eDataType == GDT_Unknown)
                    return false;
            }
        }
        auto poBandUniquePtr = std::make_unique<GDALTileIndexBand>(
            this, i + 1, eDataType, nBlockXSize, nBlockYSize);
        auto poBand = poBandUniquePtr.get();
        SetBand(i + 1, poBandUniquePtr.release());
        if (!poFirstBand)
            poFirstBand = poBand;
        if (poBand->GetRasterDataType() != poFirstBand->GetRasterDataType())
        {
            m_bSameDataType = false;
        }

        if (!apoXMLNodeBands.empty())
        {
            const char *pszVal = CPLGetXMLValue(
                apoXMLNodeBands[i], GTI_XML_BAND_DESCRIPTION, nullptr);
            if (pszVal)
            {
                poBand->GDALRasterBand::SetDescription(pszVal);
            }
        }

        if (!aNoData.empty() && aNoData[i].first)
        {
            poBand->m_bNoDataValueSet = true;
            poBand->m_dfNoDataValue = aNoData[i].second;
        }
        if (!apoXMLNodeBands.empty())
        {
            const char *pszVal = CPLGetXMLValue(
                apoXMLNodeBands[i], GTI_XML_BAND_NODATAVALUE, nullptr);
            if (pszVal)
            {
                poBand->m_bNoDataValueSet = true;
                poBand->m_dfNoDataValue = CPLAtof(pszVal);
            }
        }
        if (poBand->m_bNoDataValueSet != poFirstBand->m_bNoDataValueSet ||
            !IsSameNaNAware(poBand->m_dfNoDataValue,
                            poFirstBand->m_dfNoDataValue))
        {
            m_bSameNoData = false;
        }

        if (!aeColorInterp.empty())
        {
            poBand->m_eColorInterp = aeColorInterp[i];
        }
        if (!apoXMLNodeBands.empty())
        {
            const char *pszVal = CPLGetXMLValue(
                apoXMLNodeBands[i], GTI_XML_BAND_COLORINTERP, nullptr);
            if (pszVal)
            {
                poBand->m_eColorInterp =
                    GDALGetColorInterpretationByName(pszVal);
            }
        }

        if (const char *pszScale =
                GetOption(CPLSPrintf("BAND_%d_%s", i + 1, MD_BAND_SCALE)))
        {
            poBand->m_dfScale = CPLAtof(pszScale);
        }
        if (!apoXMLNodeBands.empty())
        {
            const char *pszVal =
                CPLGetXMLValue(apoXMLNodeBands[i], GTI_XML_BAND_SCALE, nullptr);
            if (pszVal)
            {
                poBand->m_dfScale = CPLAtof(pszVal);
            }
        }

        if (const char *pszOffset =
                GetOption(CPLSPrintf("BAND_%d_%s", i + 1, MD_BAND_OFFSET)))
        {
            poBand->m_dfOffset = CPLAtof(pszOffset);
        }
        if (!apoXMLNodeBands.empty())
        {
            const char *pszVal = CPLGetXMLValue(apoXMLNodeBands[i],
                                                GTI_XML_BAND_OFFSET, nullptr);
            if (pszVal)
            {
                poBand->m_dfOffset = CPLAtof(pszVal);
            }
        }

        if (const char *pszUnit =
                GetOption(CPLSPrintf("BAND_%d_%s", i + 1, MD_BAND_UNITTYPE)))
        {
            poBand->m_osUnit = pszUnit;
        }
        if (!apoXMLNodeBands.empty())
        {
            const char *pszVal = CPLGetXMLValue(apoXMLNodeBands[i],
                                                GTI_XML_BAND_UNITTYPE, nullptr);
            if (pszVal)
            {
                poBand->m_osUnit = pszVal;
            }
        }

        if (!apoXMLNodeBands.empty())
        {
            const CPLXMLNode *psBandNode = apoXMLNodeBands[i];
            poBand->oMDMD.XMLInit(psBandNode, TRUE);

            if (const CPLXMLNode *psCategoryNames =
                    CPLGetXMLNode(psBandNode, GTI_XML_CATEGORYNAMES))
            {
                poBand->m_aosCategoryNames =
                    VRTParseCategoryNames(psCategoryNames);
            }

            if (const CPLXMLNode *psColorTable =
                    CPLGetXMLNode(psBandNode, GTI_XML_COLORTABLE))
            {
                poBand->m_poColorTable = VRTParseColorTable(psColorTable);
            }

            if (const CPLXMLNode *psRAT =
                    CPLGetXMLNode(psBandNode, GTI_XML_RAT))
            {
                poBand->m_poRAT =
                    std::make_unique<GDALDefaultRasterAttributeTable>();
                poBand->m_poRAT->XMLInit(psRAT, "");
            }
        }
    }

    const char *pszMaskBand = GetOption(MD_MASK_BAND);
    if (pszMaskBand)
        bHasMaskBand = CPLTestBool(pszMaskBand);
    if (bHasMaskBand)
    {
        m_poMaskBand = std::make_unique<GDALTileIndexBand>(
            this, 0, GDT_Byte, nBlockXSize, nBlockYSize);
    }

    if (dfOvrFactor == 1.0)
    {
        if (psRoot)
        {
            for (const CPLXMLNode *psIter = psRoot->psChild; psIter;
                 psIter = psIter->psNext)
            {
                if (psIter->eType == CXT_Element &&
                    strcmp(psIter->pszValue, GTI_XML_OVERVIEW_ELEMENT) == 0)
                {
                    const char *pszDataset = CPLGetXMLValue(
                        psIter, GTI_XML_OVERVIEW_DATASET, nullptr);
                    const char *pszLayer =
                        CPLGetXMLValue(psIter, GTI_XML_OVERVIEW_LAYER, nullptr);
                    const char *pszFactor = CPLGetXMLValue(
                        psIter, GTI_XML_OVERVIEW_FACTOR, nullptr);
                    if (!pszDataset && !pszLayer && !pszFactor)
                    {
                        CPLError(
                            CE_Failure, CPLE_AppDefined,
                            "At least one of %s, %s or %s element "
                            "must be present as an %s child",
                            GTI_XML_OVERVIEW_DATASET, GTI_XML_OVERVIEW_LAYER,
                            GTI_XML_OVERVIEW_FACTOR, GTI_XML_OVERVIEW_ELEMENT);
                        return false;
                    }
                    m_aoOverviewDescriptor.emplace_back(
                        std::string(pszDataset ? pszDataset : ""),
                        CPLStringList(
                            GDALDeserializeOpenOptionsFromXML(psIter)),
                        std::string(pszLayer ? pszLayer : ""),
                        pszFactor ? CPLAtof(pszFactor) : 0.0);
                }
            }
        }
        else
        {
            for (int iOvr = 0;; ++iOvr)
            {
                const char *pszOvrDSName =
                    GetOption(CPLSPrintf("OVERVIEW_%d_DATASET", iOvr));
                const char *pszOpenOptions =
                    GetOption(CPLSPrintf("OVERVIEW_%d_OPEN_OPTIONS", iOvr));
                const char *pszOvrLayer =
                    GetOption(CPLSPrintf("OVERVIEW_%d_LAYER", iOvr));
                const char *pszOvrFactor =
                    GetOption(CPLSPrintf("OVERVIEW_%d_FACTOR", iOvr));
                if (!pszOvrDSName && !pszOvrLayer && !pszOvrFactor)
                {
                    // Before GDAL 3.9.2, we started the iteration at 1.
                    if (iOvr == 0)
                        continue;
                    break;
                }
                m_aoOverviewDescriptor.emplace_back(
                    std::string(pszOvrDSName ? pszOvrDSName : ""),
                    pszOpenOptions ? CPLStringList(CSLTokenizeString2(
                                         pszOpenOptions, ",", 0))
                                   : CPLStringList(),
                    std::string(pszOvrLayer ? pszOvrLayer : ""),
                    pszOvrFactor ? CPLAtof(pszOvrFactor) : 0.0);
            }
        }
    }

    if (psRoot)
    {
        oMDMD.XMLInit(psRoot, TRUE);
    }
    else
    {
        // Set on the dataset all metadata items from the index layer which are
        // not "reserved" keywords.
        CSLConstList papszLayerMD = m_poLayer->GetMetadata();
        for (const auto &[pszKey, pszValue] :
             cpl::IterateNameValue(papszLayerMD))
        {
            if (STARTS_WITH_CI(pszKey, "OVERVIEW_"))
            {
                continue;
            }

            bool bIsVRTItem = false;
            for (const char *pszTest : apszTIOptions)
            {
                if (EQUAL(pszKey, pszTest))
                {
                    bIsVRTItem = true;
                    break;
                }
            }
            if (!bIsVRTItem)
            {
                if (STARTS_WITH_CI(pszKey, "BAND_"))
                {
                    const int nBandNr = atoi(pszKey + strlen("BAND_"));
                    const char *pszNextUnderscore =
                        strchr(pszKey + strlen("BAND_"), '_');
                    if (pszNextUnderscore && nBandNr >= 1 && nBandNr <= nBands)
                    {
                        const char *pszKeyWithoutBand = pszNextUnderscore + 1;
                        bool bIsReservedBandItem = false;
                        for (const char *pszItem : apszReservedBandItems)
                        {
                            if (EQUAL(pszKeyWithoutBand, pszItem))
                            {
                                bIsReservedBandItem = true;
                                break;
                            }
                        }
                        if (!bIsReservedBandItem)
                        {
                            GetRasterBand(nBandNr)
                                ->GDALRasterBand::SetMetadataItem(
                                    pszKeyWithoutBand, pszValue);
                        }
                    }
                }
                else
                {
                    GDALDataset::SetMetadataItem(pszKey, pszValue);
                }
            }
        }
    }

    if (nBandCount > 1 && !GetMetadata("IMAGE_STRUCTURE"))
    {
        GDALDataset::SetMetadataItem("INTERLEAVE", "PIXEL", "IMAGE_STRUCTURE");
    }

    /* -------------------------------------------------------------------- */
    /*      Initialize any PAM information.                                 */
    /* -------------------------------------------------------------------- */
    SetDescription(poOpenInfo->pszFilename);
    TryLoadXML();

    /* -------------------------------------------------------------------- */
    /*      Check for overviews.                                            */
    /* -------------------------------------------------------------------- */
    oOvManager.Initialize(this, poOpenInfo->pszFilename);

    return true;
}

/************************************************************************/
/*                        GetMetadataItem()                             */
/************************************************************************/

const char *GDALTileIndexDataset::GetMetadataItem(const char *pszName,
                                                  const char *pszDomain)
{
    if (pszName && pszDomain && EQUAL(pszDomain, "__DEBUG__"))
    {
        if (EQUAL(pszName, "SCANNED_ONE_FEATURE_AT_OPENING"))
        {
            return m_bScannedOneFeatureAtOpening ? "YES" : "NO";
        }
        else if (EQUAL(pszName, "NUMBER_OF_CONTRIBUTING_SOURCES"))
        {
            return CPLSPrintf("%d", static_cast<int>(m_aoSourceDesc.size()));
        }
    }
    return GDALPamDataset::GetMetadataItem(pszName, pszDomain);
}

/************************************************************************/
/*                TileIndexSupportsEditingLayerMetadata()               */
/************************************************************************/

bool GDALTileIndexDataset::TileIndexSupportsEditingLayerMetadata() const
{
    return eAccess == GA_Update && m_poVectorDS->GetDriver() &&
           EQUAL(m_poVectorDS->GetDriver()->GetDescription(), "GPKG");
}

/************************************************************************/
/*                        SetMetadataItem()                             */
/************************************************************************/

CPLErr GDALTileIndexDataset::SetMetadataItem(const char *pszName,
                                             const char *pszValue,
                                             const char *pszDomain)
{
    if (m_bXMLUpdatable)
    {
        m_bXMLModified = true;
        return GDALDataset::SetMetadataItem(pszName, pszValue, pszDomain);
    }
    else if (TileIndexSupportsEditingLayerMetadata())
    {
        m_poLayer->SetMetadataItem(pszName, pszValue, pszDomain);
        return GDALDataset::SetMetadataItem(pszName, pszValue, pszDomain);
    }
    else
    {
        return GDALPamDataset::SetMetadataItem(pszName, pszValue, pszDomain);
    }
}

/************************************************************************/
/*                           SetMetadata()                              */
/************************************************************************/

CPLErr GDALTileIndexDataset::SetMetadata(char **papszMD, const char *pszDomain)
{
    if (m_bXMLUpdatable)
    {
        m_bXMLModified = true;
        return GDALDataset::SetMetadata(papszMD, pszDomain);
    }
    else if (TileIndexSupportsEditingLayerMetadata())
    {
        if (!pszDomain || pszDomain[0] == 0)
        {
            CPLStringList aosMD(CSLDuplicate(papszMD));

            // Reinject dataset reserved items
            for (const char *pszItem : apszTIOptions)
            {
                if (!aosMD.FetchNameValue(pszItem))
                {
                    const char *pszValue = m_poLayer->GetMetadataItem(pszItem);
                    if (pszValue)
                    {
                        aosMD.SetNameValue(pszItem, pszValue);
                    }
                }
            }

            // Reinject band metadata
            char **papszExistingLayerMD = m_poLayer->GetMetadata();
            for (int i = 0; papszExistingLayerMD && papszExistingLayerMD[i];
                 ++i)
            {
                if (STARTS_WITH_CI(papszExistingLayerMD[i], "BAND_"))
                {
                    aosMD.AddString(papszExistingLayerMD[i]);
                }
            }

            m_poLayer->SetMetadata(aosMD.List(), pszDomain);
        }
        else
        {
            m_poLayer->SetMetadata(papszMD, pszDomain);
        }
        return GDALDataset::SetMetadata(papszMD, pszDomain);
    }
    else
    {
        return GDALPamDataset::SetMetadata(papszMD, pszDomain);
    }
}

/************************************************************************/
/*                     GDALTileIndexDatasetIdentify()                   */
/************************************************************************/

static int GDALTileIndexDatasetIdentify(GDALOpenInfo *poOpenInfo)
{
    if (STARTS_WITH(poOpenInfo->pszFilename, GTI_PREFIX))
        return true;

    if (STARTS_WITH(poOpenInfo->pszFilename, "<GDALTileIndexDataset"))
        return true;

    if (poOpenInfo->nHeaderBytes >= 100 &&
        STARTS_WITH(reinterpret_cast<const char *>(poOpenInfo->pabyHeader),
                    "SQLite format 3") &&
        ENDS_WITH_CI(poOpenInfo->pszFilename, ".gti.gpkg") &&
        !STARTS_WITH(poOpenInfo->pszFilename, "GPKG:"))
    {
        // Most likely handled by GTI driver, but we can't be sure
        return GDAL_IDENTIFY_UNKNOWN;
    }

    return poOpenInfo->nHeaderBytes > 0 &&
           (poOpenInfo->nOpenFlags & GDAL_OF_RASTER) != 0 &&
           (strstr(reinterpret_cast<const char *>(poOpenInfo->pabyHeader),
                   "<GDALTileIndexDataset") ||
            ENDS_WITH_CI(poOpenInfo->pszFilename, ".gti.fgb") ||
            ENDS_WITH_CI(poOpenInfo->pszFilename, ".gti.parquet"));
}

/************************************************************************/
/*                      GDALTileIndexDatasetOpen()                       */
/************************************************************************/

static GDALDataset *GDALTileIndexDatasetOpen(GDALOpenInfo *poOpenInfo)
{
    if (GDALTileIndexDatasetIdentify(poOpenInfo) == GDAL_IDENTIFY_FALSE)
        return nullptr;
    auto poDS = std::make_unique<GDALTileIndexDataset>();
    if (!poDS->Open(poOpenInfo))
        return nullptr;
    return poDS.release();
}

/************************************************************************/
/*                          ~GDALTileIndexDataset()                      */
/************************************************************************/

GDALTileIndexDataset::~GDALTileIndexDataset()
{
    GDALTileIndexDataset::FlushCache(true);
}

/************************************************************************/
/*                              FlushCache()                            */
/************************************************************************/

CPLErr GDALTileIndexDataset::FlushCache(bool bAtClosing)
{
    CPLErr eErr = CE_None;
    if (bAtClosing && m_bXMLModified)
    {
        CPLXMLNode *psRoot =
            CPLGetXMLNode(m_psXMLTree.get(), "=GDALTileIndexDataset");

        // Suppress existing dataset metadata
        while (true)
        {
            CPLXMLNode *psExistingMetadata = CPLGetXMLNode(psRoot, "Metadata");
            if (!psExistingMetadata)
                break;
            CPLRemoveXMLChild(psRoot, psExistingMetadata);
        }

        // Serialize new dataset metadata
        if (CPLXMLNode *psMD = oMDMD.Serialize())
            CPLAddXMLChild(psRoot, psMD);

        // Update existing band metadata
        if (CPLGetXMLNode(psRoot, GTI_XML_BAND_ELEMENT))
        {
            for (CPLXMLNode *psIter = psRoot->psChild; psIter;
                 psIter = psIter->psNext)
            {
                if (psIter->eType == CXT_Element &&
                    strcmp(psIter->pszValue, GTI_XML_BAND_ELEMENT))
                {
                    const char *pszBand =
                        CPLGetXMLValue(psIter, GTI_XML_BAND_NUMBER, nullptr);
                    if (pszBand)
                    {
                        const int nBand = atoi(pszBand);
                        if (nBand >= 1 && nBand <= nBands)
                        {
                            while (true)
                            {
                                CPLXMLNode *psExistingMetadata =
                                    CPLGetXMLNode(psIter, "Metadata");
                                if (!psExistingMetadata)
                                    break;
                                CPLRemoveXMLChild(psIter, psExistingMetadata);
                            }

                            auto poBand = cpl::down_cast<GDALTileIndexBand *>(
                                papoBands[nBand - 1]);
                            if (CPLXMLNode *psMD = poBand->oMDMD.Serialize())
                                CPLAddXMLChild(psIter, psMD);
                        }
                    }
                }
            }
        }
        else
        {
            // Create new band objects if they have metadata
            std::vector<CPLXMLTreeCloser> aoBandXML;
            bool bHasBandMD = false;
            for (int i = 1; i <= nBands; ++i)
            {
                auto poBand =
                    cpl::down_cast<GDALTileIndexBand *>(papoBands[i - 1]);
                auto psMD = poBand->oMDMD.Serialize();
                if (psMD)
                    bHasBandMD = true;
                aoBandXML.emplace_back(CPLXMLTreeCloser(psMD));
            }
            if (bHasBandMD)
            {
                for (int i = 1; i <= nBands; ++i)
                {
                    auto poBand =
                        cpl::down_cast<GDALTileIndexBand *>(papoBands[i - 1]);

                    CPLXMLNode *psBand = CPLCreateXMLNode(psRoot, CXT_Element,
                                                          GTI_XML_BAND_ELEMENT);
                    CPLAddXMLAttributeAndValue(psBand, GTI_XML_BAND_NUMBER,
                                               CPLSPrintf("%d", i));
                    CPLAddXMLAttributeAndValue(
                        psBand, GTI_XML_BAND_DATATYPE,
                        GDALGetDataTypeName(poBand->GetRasterDataType()));

                    const char *pszDescription = poBand->GetDescription();
                    if (pszDescription && pszDescription[0])
                        CPLSetXMLValue(psBand, GTI_XML_BAND_DESCRIPTION,
                                       pszDescription);

                    const auto eColorInterp = poBand->GetColorInterpretation();
                    if (eColorInterp != GCI_Undefined)
                        CPLSetXMLValue(
                            psBand, GTI_XML_BAND_COLORINTERP,
                            GDALGetColorInterpretationName(eColorInterp));

                    if (!std::isnan(poBand->m_dfOffset))
                        CPLSetXMLValue(psBand, GTI_XML_BAND_OFFSET,
                                       CPLSPrintf("%.16g", poBand->m_dfOffset));

                    if (!std::isnan(poBand->m_dfScale))
                        CPLSetXMLValue(psBand, GTI_XML_BAND_SCALE,
                                       CPLSPrintf("%.16g", poBand->m_dfScale));

                    if (!poBand->m_osUnit.empty())
                        CPLSetXMLValue(psBand, GTI_XML_BAND_UNITTYPE,
                                       poBand->m_osUnit.c_str());

                    if (poBand->m_bNoDataValueSet)
                    {
                        CPLSetXMLValue(
                            psBand, GTI_XML_BAND_NODATAVALUE,
                            VRTSerializeNoData(poBand->m_dfNoDataValue,
                                               poBand->GetRasterDataType(), 18)
                                .c_str());
                    }
                    if (aoBandXML[i - 1])
                    {
                        CPLAddXMLChild(psBand, aoBandXML[i - 1].release());
                    }
                }
            }
        }

        if (!CPLSerializeXMLTreeToFile(m_psXMLTree.get(), GetDescription()))
            eErr = CE_Failure;
    }

    // We also clear the cache of opened sources, in case the user would
    // change the content of a source and would want the GTI dataset to see
    // the refreshed content.
    m_oMapSharedSources.clear();
    m_dfLastMinXFilter = std::numeric_limits<double>::quiet_NaN();
    m_dfLastMinYFilter = std::numeric_limits<double>::quiet_NaN();
    m_dfLastMaxXFilter = std::numeric_limits<double>::quiet_NaN();
    m_dfLastMaxYFilter = std::numeric_limits<double>::quiet_NaN();
    m_aoSourceDesc.clear();
    if (GDALPamDataset::FlushCache(bAtClosing) != CE_None)
        eErr = CE_Failure;
    return eErr;
}

/************************************************************************/
/*                            LoadOverviews()                           */
/************************************************************************/

void GDALTileIndexDataset::LoadOverviews()
{
    if (m_apoOverviews.empty() && !m_aoOverviewDescriptor.empty())
    {
        for (const auto &[osDSName, aosOpenOptions, osLyrName, dfFactor] :
             m_aoOverviewDescriptor)
        {
            CPLStringList aosNewOpenOptions(aosOpenOptions);
            if (dfFactor != 0)
            {
                aosNewOpenOptions.SetNameValue("@FACTOR",
                                               CPLSPrintf("%.18g", dfFactor));
            }
            if (!osLyrName.empty())
            {
                aosNewOpenOptions.SetNameValue("@LAYER", osLyrName.c_str());
            }

            std::unique_ptr<GDALDataset> poOvrDS(GDALDataset::Open(
                !osDSName.empty() ? osDSName.c_str() : GetDescription(),
                GDAL_OF_RASTER | GDAL_OF_VERBOSE_ERROR, nullptr,
                aosNewOpenOptions.List(), nullptr));

            const auto IsSmaller =
                [](const GDALDataset *a, const GDALDataset *b)
            {
                return (a->GetRasterXSize() < b->GetRasterXSize() &&
                        a->GetRasterYSize() <= b->GetRasterYSize()) ||
                       (a->GetRasterYSize() < b->GetRasterYSize() &&
                        a->GetRasterXSize() <= b->GetRasterXSize());
            };

            if (poOvrDS &&
                ((m_apoOverviews.empty() && IsSmaller(poOvrDS.get(), this)) ||
                 ((!m_apoOverviews.empty() &&
                   IsSmaller(poOvrDS.get(), m_apoOverviews.back().get())))))
            {
                if (poOvrDS->GetRasterCount() == GetRasterCount())
                {
                    m_apoOverviews.emplace_back(std::move(poOvrDS));
                    // Add the overviews of the overview, unless the OVERVIEW_LEVEL
                    // option option is specified
                    if (aosOpenOptions.FetchNameValue("OVERVIEW_LEVEL") ==
                        nullptr)
                    {
                        const int nOverviewCount = m_apoOverviews.back()
                                                       ->GetRasterBand(1)
                                                       ->GetOverviewCount();
                        for (int i = 0; i < nOverviewCount; ++i)
                        {
                            aosNewOpenOptions.SetNameValue("OVERVIEW_LEVEL",
                                                           CPLSPrintf("%d", i));
                            std::unique_ptr<GDALDataset> poOvrOfOvrDS(
                                GDALDataset::Open(
                                    !osDSName.empty() ? osDSName.c_str()
                                                      : GetDescription(),
                                    GDAL_OF_RASTER | GDAL_OF_VERBOSE_ERROR,
                                    nullptr, aosNewOpenOptions.List(),
                                    nullptr));
                            if (poOvrOfOvrDS &&
                                poOvrOfOvrDS->GetRasterCount() ==
                                    GetRasterCount() &&
                                IsSmaller(poOvrOfOvrDS.get(),
                                          m_apoOverviews.back().get()))
                            {
                                m_apoOverviews.emplace_back(
                                    std::move(poOvrOfOvrDS));
                            }
                        }
                    }
                }
                else
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "%s has not the same number of bands as %s",
                             poOvrDS->GetDescription(), GetDescription());
                }
            }
        }
    }
}

/************************************************************************/
/*                          GetOverviewCount()                          */
/************************************************************************/

int GDALTileIndexBand::GetOverviewCount()
{
    const int nPAMOverviews = GDALPamRasterBand::GetOverviewCount();
    if (nPAMOverviews)
        return nPAMOverviews;

    m_poDS->LoadOverviews();
    return static_cast<int>(m_poDS->m_apoOverviews.size());
}

/************************************************************************/
/*                             GetOverview()                            */
/************************************************************************/

GDALRasterBand *GDALTileIndexBand::GetOverview(int iOvr)
{
    if (iOvr < 0 || iOvr >= GetOverviewCount())
        return nullptr;

    const int nPAMOverviews = GDALPamRasterBand::GetOverviewCount();
    if (nPAMOverviews)
        return GDALPamRasterBand::GetOverview(iOvr);

    if (nBand == 0)
    {
        auto poBand = m_poDS->m_apoOverviews[iOvr]->GetRasterBand(1);
        if (!poBand)
            return nullptr;
        return poBand->GetMaskBand();
    }
    else
    {
        return m_poDS->m_apoOverviews[iOvr]->GetRasterBand(nBand);
    }
}

/************************************************************************/
/*                           GetGeoTransform()                          */
/************************************************************************/

CPLErr GDALTileIndexDataset::GetGeoTransform(double *padfGeoTransform)
{
    memcpy(padfGeoTransform, m_adfGeoTransform.data(), 6 * sizeof(double));
    return CE_None;
}

/************************************************************************/
/*                            GetSpatialRef()                           */
/************************************************************************/

const OGRSpatialReference *GDALTileIndexDataset::GetSpatialRef() const
{
    return m_oSRS.IsEmpty() ? nullptr : &m_oSRS;
}

/************************************************************************/
/*                           GDALTileIndexBand()                         */
/************************************************************************/

GDALTileIndexBand::GDALTileIndexBand(GDALTileIndexDataset *poDSIn, int nBandIn,
                                     GDALDataType eDT, int nBlockXSizeIn,
                                     int nBlockYSizeIn)
{
    m_poDS = poDSIn;
    nBand = nBandIn;
    eDataType = eDT;
    nRasterXSize = poDSIn->GetRasterXSize();
    nRasterYSize = poDSIn->GetRasterYSize();
    nBlockXSize = nBlockXSizeIn;
    nBlockYSize = nBlockYSizeIn;
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr GDALTileIndexBand::IReadBlock(int nBlockXOff, int nBlockYOff,
                                     void *pImage)

{
    const int nPixelSize = GDALGetDataTypeSizeBytes(eDataType);

    int nReadXSize = nBlockXSize;
    int nReadYSize = nBlockYSize;
    GetActualBlockSize(nBlockXOff, nBlockYOff, &nReadXSize, &nReadYSize);

    GDALRasterIOExtraArg sExtraArg;
    INIT_RASTERIO_EXTRA_ARG(sExtraArg);

    return IRasterIO(
        GF_Read, nBlockXOff * nBlockXSize, nBlockYOff * nBlockYSize, nReadXSize,
        nReadYSize, pImage, nReadXSize, nReadYSize, eDataType, nPixelSize,
        static_cast<GSpacing>(nPixelSize) * nBlockXSize, &sExtraArg);
}

/************************************************************************/
/*                             IRasterIO()                              */
/************************************************************************/

CPLErr GDALTileIndexBand::IRasterIO(GDALRWFlag eRWFlag, int nXOff, int nYOff,
                                    int nXSize, int nYSize, void *pData,
                                    int nBufXSize, int nBufYSize,
                                    GDALDataType eBufType, GSpacing nPixelSpace,
                                    GSpacing nLineSpace,
                                    GDALRasterIOExtraArg *psExtraArg)
{
    int anBand[] = {nBand};

    return m_poDS->IRasterIO(eRWFlag, nXOff, nYOff, nXSize, nYSize, pData,
                             nBufXSize, nBufYSize, eBufType, 1, anBand,
                             nPixelSpace, nLineSpace, 0, psExtraArg);
}

/************************************************************************/
/*                      GetMetadataDomainList()                         */
/************************************************************************/

char **GDALTileIndexBand::GetMetadataDomainList()
{
    return CSLAddString(GDALRasterBand::GetMetadataDomainList(),
                        "LocationInfo");
}

/************************************************************************/
/*                          GetMetadataItem()                           */
/************************************************************************/

const char *GDALTileIndexBand::GetMetadataItem(const char *pszName,
                                               const char *pszDomain)

{
    /* ==================================================================== */
    /*      LocationInfo handling.                                          */
    /* ==================================================================== */
    if (pszDomain != nullptr && EQUAL(pszDomain, "LocationInfo") &&
        (STARTS_WITH_CI(pszName, "Pixel_") ||
         STARTS_WITH_CI(pszName, "GeoPixel_")))
    {
        // What pixel are we aiming at?
        int iPixel = 0;
        int iLine = 0;

        if (STARTS_WITH_CI(pszName, "Pixel_"))
        {
            pszName += strlen("Pixel_");
            iPixel = atoi(pszName);
            const char *const pszUnderscore = strchr(pszName, '_');
            if (!pszUnderscore)
                return nullptr;
            iLine = atoi(pszUnderscore + 1);
        }
        else if (STARTS_WITH_CI(pszName, "GeoPixel_"))
        {
            pszName += strlen("GeoPixel_");
            const double dfGeoX = CPLAtof(pszName);
            const char *const pszUnderscore = strchr(pszName, '_');
            if (!pszUnderscore)
                return nullptr;
            const double dfGeoY = CPLAtof(pszUnderscore + 1);

            double adfInvGeoTransform[6] = {0.0};
            if (!GDALInvGeoTransform(m_poDS->m_adfGeoTransform.data(),
                                     adfInvGeoTransform))
                return nullptr;

            iPixel = static_cast<int>(floor(adfInvGeoTransform[0] +
                                            adfInvGeoTransform[1] * dfGeoX +
                                            adfInvGeoTransform[2] * dfGeoY));
            iLine = static_cast<int>(floor(adfInvGeoTransform[3] +
                                           adfInvGeoTransform[4] * dfGeoX +
                                           adfInvGeoTransform[5] * dfGeoY));
        }
        else
        {
            return nullptr;
        }

        if (iPixel < 0 || iLine < 0 || iPixel >= GetXSize() ||
            iLine >= GetYSize())
            return nullptr;

        if (!m_poDS->CollectSources(iPixel, iLine, 1, 1))
            return nullptr;

        // Format into XML.
        m_osLastLocationInfo = "<LocationInfo>";

        if (!m_poDS->m_aoSourceDesc.empty())
        {
            const auto AddSource =
                [&](const GDALTileIndexDataset::SourceDesc &oSourceDesc)
            {
                m_osLastLocationInfo += "<File>";
                char *const pszXMLEscaped =
                    CPLEscapeString(oSourceDesc.osName.c_str(), -1, CPLES_XML);
                m_osLastLocationInfo += pszXMLEscaped;
                CPLFree(pszXMLEscaped);
                m_osLastLocationInfo += "</File>";
            };

            const int anBand[] = {nBand};
            if (!m_poDS->NeedInitBuffer(1, anBand))
            {
                AddSource(m_poDS->m_aoSourceDesc.back());
            }
            else
            {
                for (const auto &oSourceDesc : m_poDS->m_aoSourceDesc)
                {
                    if (oSourceDesc.poDS)
                        AddSource(oSourceDesc);
                }
            }
        }

        m_osLastLocationInfo += "</LocationInfo>";

        return m_osLastLocationInfo.c_str();
    }

    return GDALPamRasterBand::GetMetadataItem(pszName, pszDomain);
}

/************************************************************************/
/*                        SetMetadataItem()                             */
/************************************************************************/

CPLErr GDALTileIndexBand::SetMetadataItem(const char *pszName,
                                          const char *pszValue,
                                          const char *pszDomain)
{
    if (nBand > 0 && m_poDS->m_bXMLUpdatable)
    {
        m_poDS->m_bXMLModified = true;
        return GDALRasterBand::SetMetadataItem(pszName, pszValue, pszDomain);
    }
    else if (nBand > 0 && m_poDS->TileIndexSupportsEditingLayerMetadata())
    {
        m_poDS->m_poLayer->SetMetadataItem(
            CPLSPrintf("BAND_%d_%s", nBand, pszName), pszValue, pszDomain);
        return GDALRasterBand::SetMetadataItem(pszName, pszValue, pszDomain);
    }
    else
    {
        return GDALPamRasterBand::SetMetadataItem(pszName, pszValue, pszDomain);
    }
}

/************************************************************************/
/*                           SetMetadata()                              */
/************************************************************************/

CPLErr GDALTileIndexBand::SetMetadata(char **papszMD, const char *pszDomain)
{
    if (nBand > 0 && m_poDS->m_bXMLUpdatable)
    {
        m_poDS->m_bXMLModified = true;
        return GDALRasterBand::SetMetadata(papszMD, pszDomain);
    }
    else if (nBand > 0 && m_poDS->TileIndexSupportsEditingLayerMetadata())
    {
        CPLStringList aosMD;

        if (!pszDomain || pszDomain[0] == 0)
        {
            // Reinject dataset metadata
            char **papszLayerMD = m_poDS->m_poLayer->GetMetadata(pszDomain);
            for (const char *const *papszIter = papszLayerMD;
                 papszIter && *papszIter; ++papszIter)
            {
                if (!STARTS_WITH(*papszIter, "BAND_") ||
                    STARTS_WITH(*papszIter, MD_BAND_COUNT))
                    aosMD.AddString(*papszIter);
            }
        }

        for (int i = 0; papszMD && papszMD[i]; ++i)
        {
            aosMD.AddString(CPLSPrintf("BAND_%d_%s", nBand, papszMD[i]));
        }

        if (!pszDomain || pszDomain[0] == 0)
        {
            for (const char *pszItem : apszReservedBandItems)
            {
                const char *pszKey = CPLSPrintf("BAND_%d_%s", nBand, pszItem);
                if (!aosMD.FetchNameValue(pszKey))
                {
                    if (const char *pszVal =
                            m_poDS->m_poLayer->GetMetadataItem(pszKey))
                    {
                        aosMD.SetNameValue(pszKey, pszVal);
                    }
                }
            }
        }

        m_poDS->m_poLayer->SetMetadata(aosMD.List(), pszDomain);
        return GDALRasterBand::SetMetadata(papszMD, pszDomain);
    }
    else
    {
        return GDALPamRasterBand::SetMetadata(papszMD, pszDomain);
    }
}

/************************************************************************/
/*                         GetSrcDstWin()                               */
/************************************************************************/

static bool GetSrcDstWin(const double adfTileGT[6], int nTileXSize,
                         int nTileYSize, const double adfVRTGT[6],
                         int nVRTXSize, int nVRTYSize, double *pdfSrcXOff,
                         double *pdfSrcYOff, double *pdfSrcXSize,
                         double *pdfSrcYSize, double *pdfDstXOff,
                         double *pdfDstYOff, double *pdfDstXSize,
                         double *pdfDstYSize)
{
    const double minX = adfVRTGT[GT_TOPLEFT_X];
    const double we_res = adfVRTGT[GT_WE_RES];
    const double maxX = minX + nVRTXSize * we_res;
    const double maxY = adfVRTGT[GT_TOPLEFT_Y];
    const double ns_res = adfVRTGT[GT_NS_RES];
    const double minY = maxY + nVRTYSize * ns_res;

    /* Check that the destination bounding box intersects the source bounding
     * box */
    if (adfTileGT[GT_TOPLEFT_X] + nTileXSize * adfTileGT[GT_WE_RES] <= minX)
        return false;
    if (adfTileGT[GT_TOPLEFT_X] >= maxX)
        return false;
    if (adfTileGT[GT_TOPLEFT_Y] + nTileYSize * adfTileGT[GT_NS_RES] >= maxY)
        return false;
    if (adfTileGT[GT_TOPLEFT_Y] <= minY)
        return false;

    if (adfTileGT[GT_TOPLEFT_X] < minX)
    {
        *pdfSrcXOff = (minX - adfTileGT[GT_TOPLEFT_X]) / adfTileGT[GT_WE_RES];
        *pdfDstXOff = 0.0;
    }
    else
    {
        *pdfSrcXOff = 0.0;
        *pdfDstXOff = ((adfTileGT[GT_TOPLEFT_X] - minX) / we_res);
    }
    if (maxY < adfTileGT[GT_TOPLEFT_Y])
    {
        *pdfSrcYOff = (adfTileGT[GT_TOPLEFT_Y] - maxY) / -adfTileGT[GT_NS_RES];
        *pdfDstYOff = 0.0;
    }
    else
    {
        *pdfSrcYOff = 0.0;
        *pdfDstYOff = ((maxY - adfTileGT[GT_TOPLEFT_Y]) / -ns_res);
    }

    *pdfSrcXSize = nTileXSize;
    *pdfSrcYSize = nTileYSize;
    if (*pdfSrcXOff > 0)
        *pdfSrcXSize -= *pdfSrcXOff;
    if (*pdfSrcYOff > 0)
        *pdfSrcYSize -= *pdfSrcYOff;

    const double dfSrcToDstXSize = adfTileGT[GT_WE_RES] / we_res;
    *pdfDstXSize = *pdfSrcXSize * dfSrcToDstXSize;
    const double dfSrcToDstYSize = adfTileGT[GT_NS_RES] / ns_res;
    *pdfDstYSize = *pdfSrcYSize * dfSrcToDstYSize;

    if (*pdfDstXOff + *pdfDstXSize > nVRTXSize)
    {
        *pdfDstXSize = nVRTXSize - *pdfDstXOff;
        *pdfSrcXSize = *pdfDstXSize / dfSrcToDstXSize;
    }

    if (*pdfDstYOff + *pdfDstYSize > nVRTYSize)
    {
        *pdfDstYSize = nVRTYSize - *pdfDstYOff;
        *pdfSrcYSize = *pdfDstYSize / dfSrcToDstYSize;
    }

    return *pdfSrcXSize > 0 && *pdfDstXSize > 0 && *pdfSrcYSize > 0 &&
           *pdfDstYSize > 0;
}

/************************************************************************/
/*                   GDALDatasetCastToGTIDataset()                    */
/************************************************************************/

GDALTileIndexDataset *GDALDatasetCastToGTIDataset(GDALDataset *poDS)
{
    return dynamic_cast<GDALTileIndexDataset *>(poDS);
}

/************************************************************************/
/*                   GTIGetSourcesMoreRecentThan()                    */
/************************************************************************/

std::vector<GTISourceDesc>
GTIGetSourcesMoreRecentThan(GDALTileIndexDataset *poDS, int64_t mTime)
{
    return poDS->GetSourcesMoreRecentThan(mTime);
}

/************************************************************************/
/*                       GetSourcesMoreRecentThan()                     */
/************************************************************************/

std::vector<GTISourceDesc>
GDALTileIndexDataset::GetSourcesMoreRecentThan(int64_t mTime)
{
    std::vector<GTISourceDesc> oRes;

    m_poLayer->SetSpatialFilter(nullptr);
    for (auto &&poFeature : m_poLayer)
    {
        if (!poFeature->IsFieldSetAndNotNull(m_nLocationFieldIndex))
        {
            continue;
        }

        auto poGeom = poFeature->GetGeometryRef();
        if (!poGeom || poGeom->IsEmpty())
            continue;

        OGREnvelope sEnvelope;
        poGeom->getEnvelope(&sEnvelope);

        double dfXOff = (sEnvelope.MinX - m_adfGeoTransform[GT_TOPLEFT_X]) /
                        m_adfGeoTransform[GT_WE_RES];
        if (dfXOff >= nRasterXSize)
            continue;

        double dfYOff = (sEnvelope.MaxY - m_adfGeoTransform[GT_TOPLEFT_Y]) /
                        m_adfGeoTransform[GT_NS_RES];
        if (dfYOff >= nRasterYSize)
            continue;

        double dfXSize =
            (sEnvelope.MaxX - sEnvelope.MinX) / m_adfGeoTransform[GT_WE_RES];
        if (dfXOff < 0)
        {
            dfXSize += dfXOff;
            dfXOff = 0;
            if (dfXSize <= 0)
                continue;
        }

        double dfYSize = (sEnvelope.MaxY - sEnvelope.MinY) /
                         std::fabs(m_adfGeoTransform[GT_NS_RES]);
        if (dfYOff < 0)
        {
            dfYSize += dfYOff;
            dfYOff = 0;
            if (dfYSize <= 0)
                continue;
        }

        const char *pszTileName =
            poFeature->GetFieldAsString(m_nLocationFieldIndex);
        const std::string osTileName(
            GetAbsoluteFileName(pszTileName, GetDescription()));
        VSIStatBufL sStatSource;
        if (VSIStatL(osTileName.c_str(), &sStatSource) != 0 ||
            sStatSource.st_mtime <= mTime)
        {
            continue;
        }

        constexpr double EPS = 1e-8;
        GTISourceDesc oSourceDesc;
        oSourceDesc.osFilename = osTileName;
        oSourceDesc.nDstXOff = static_cast<int>(dfXOff + EPS);
        oSourceDesc.nDstYOff = static_cast<int>(dfYOff + EPS);
        oSourceDesc.nDstXSize = static_cast<int>(dfXSize + 0.5);
        oSourceDesc.nDstYSize = static_cast<int>(dfYSize + 0.5);
        oRes.emplace_back(std::move(oSourceDesc));
    }

    return oRes;
}

/************************************************************************/
/*                         GetSourceDesc()                              */
/************************************************************************/

bool GDALTileIndexDataset::GetSourceDesc(const std::string &osTileName,
                                         SourceDesc &oSourceDesc)
{
    std::shared_ptr<GDALDataset> poTileDS;
    if (!m_oMapSharedSources.tryGet(osTileName, poTileDS))
    {
        poTileDS = std::shared_ptr<GDALDataset>(
            GDALProxyPoolDataset::Create(
                osTileName.c_str(), nullptr, GA_ReadOnly,
                /* bShared = */ true, m_osUniqueHandle.c_str()),
            GDALDatasetUniquePtrReleaser());
        if (!poTileDS || poTileDS->GetRasterCount() == 0)
        {
            return false;
        }

        // do palette -> RGB(A) expansion if needed
        if (!GTIDoPaletteExpansionIfNeeded(poTileDS, nBands))
            return false;

        const OGRSpatialReference *poTileSRS;
        if (!m_oSRS.IsEmpty() &&
            (poTileSRS = poTileDS->GetSpatialRef()) != nullptr &&
            !m_oSRS.IsSame(poTileSRS))
        {
            CPLDebug("VRT",
                     "Tile %s has not the same SRS as the VRT. "
                     "Proceed to on-the-fly warping",
                     osTileName.c_str());

            CPLStringList aosOptions;
            aosOptions.AddString("-of");
            aosOptions.AddString("VRT");

            if ((poTileDS->GetRasterBand(1)->GetColorTable() == nullptr &&
                 poTileDS->GetRasterBand(1)->GetCategoryNames() == nullptr) ||
                m_eResampling == GRIORA_Mode)
            {
                aosOptions.AddString("-r");
                aosOptions.AddString(m_osResampling.c_str());
            }

            if (m_osWKT.empty())
            {
                char *pszWKT = nullptr;
                const char *const apszWKTOptions[] = {"FORMAT=WKT2_2019",
                                                      nullptr};
                m_oSRS.exportToWkt(&pszWKT, apszWKTOptions);
                if (pszWKT)
                    m_osWKT = pszWKT;
                CPLFree(pszWKT);
            }
            if (m_osWKT.empty())
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Cannot export VRT SRS to WKT2");
                return false;
            }

            aosOptions.AddString("-t_srs");
            aosOptions.AddString(m_osWKT.c_str());

            // First pass to get the extent of the tile in the
            // target VRT SRS
            GDALWarpAppOptions *psWarpOptions =
                GDALWarpAppOptionsNew(aosOptions.List(), nullptr);
            GDALDatasetH ahSrcDS[] = {GDALDataset::ToHandle(poTileDS.get())};
            int bUsageError = false;
            auto poWarpDS =
                std::unique_ptr<GDALDataset>(GDALDataset::FromHandle(GDALWarp(
                    "", nullptr, 1, ahSrcDS, psWarpOptions, &bUsageError)));
            GDALWarpAppOptionsFree(psWarpOptions);
            if (!poWarpDS)
            {
                return false;
            }

            // Second pass to create a warped source VRT whose
            // extent is aligned on the one of the target VRT
            double adfWarpDSGeoTransform[6];
            const auto eErr = poWarpDS->GetGeoTransform(adfWarpDSGeoTransform);
            CPL_IGNORE_RET_VAL(eErr);
            CPLAssert(eErr == CE_None);
            const double dfVRTMinX = m_adfGeoTransform[GT_TOPLEFT_X];
            const double dfVRTResX = m_adfGeoTransform[GT_WE_RES];
            const double dfVRTMaxY = m_adfGeoTransform[GT_TOPLEFT_Y];
            const double dfVRTResYAbs = -m_adfGeoTransform[GT_NS_RES];
            const double dfWarpMinX =
                std::floor((adfWarpDSGeoTransform[GT_TOPLEFT_X] - dfVRTMinX) /
                           dfVRTResX) *
                    dfVRTResX +
                dfVRTMinX;
            const double dfWarpMaxX =
                std::ceil((adfWarpDSGeoTransform[GT_TOPLEFT_X] +
                           adfWarpDSGeoTransform[GT_WE_RES] *
                               poWarpDS->GetRasterXSize() -
                           dfVRTMinX) /
                          dfVRTResX) *
                    dfVRTResX +
                dfVRTMinX;
            const double dfWarpMaxY =
                dfVRTMaxY -
                std::floor((dfVRTMaxY - adfWarpDSGeoTransform[GT_TOPLEFT_Y]) /
                           dfVRTResYAbs) *
                    dfVRTResYAbs;
            const double dfWarpMinY =
                dfVRTMaxY -
                std::ceil((dfVRTMaxY - (adfWarpDSGeoTransform[GT_TOPLEFT_Y] +
                                        adfWarpDSGeoTransform[GT_NS_RES] *
                                            poWarpDS->GetRasterYSize())) /
                          dfVRTResYAbs) *
                    dfVRTResYAbs;

            aosOptions.AddString("-te");
            aosOptions.AddString(CPLSPrintf("%.18g", dfWarpMinX));
            aosOptions.AddString(CPLSPrintf("%.18g", dfWarpMinY));
            aosOptions.AddString(CPLSPrintf("%.18g", dfWarpMaxX));
            aosOptions.AddString(CPLSPrintf("%.18g", dfWarpMaxY));

            aosOptions.AddString("-tr");
            aosOptions.AddString(CPLSPrintf("%.18g", dfVRTResX));
            aosOptions.AddString(CPLSPrintf("%.18g", dfVRTResYAbs));

            aosOptions.AddString("-dstalpha");

            psWarpOptions = GDALWarpAppOptionsNew(aosOptions.List(), nullptr);
            poWarpDS.reset(GDALDataset::FromHandle(GDALWarp(
                "", nullptr, 1, ahSrcDS, psWarpOptions, &bUsageError)));
            GDALWarpAppOptionsFree(psWarpOptions);
            if (!poWarpDS)
            {
                return false;
            }

            poTileDS.reset(poWarpDS.release());
        }

        m_oMapSharedSources.insert(osTileName, poTileDS);
    }

    double adfGeoTransformTile[6];
    if (poTileDS->GetGeoTransform(adfGeoTransformTile) != CE_None)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s lacks geotransform",
                 osTileName.c_str());
        return false;
    }

    bool bHasNoData = false;
    bool bSameNoData = true;
    double dfNoDataValue = 0;
    GDALRasterBand *poMaskBand = nullptr;
    const int nBandCount = poTileDS->GetRasterCount();
    for (int iBand = 0; iBand < nBandCount; ++iBand)
    {
        auto poTileBand = poTileDS->GetRasterBand(iBand + 1);
        int bThisBandHasNoData = false;
        const double dfThisBandNoDataValue =
            poTileBand->GetNoDataValue(&bThisBandHasNoData);
        if (bThisBandHasNoData)
        {
            bHasNoData = true;
            dfNoDataValue = dfThisBandNoDataValue;
        }
        if (iBand > 0 &&
            (static_cast<int>(bThisBandHasNoData) !=
                 static_cast<int>(bHasNoData) ||
             (bHasNoData &&
              !IsSameNaNAware(dfNoDataValue, dfThisBandNoDataValue))))
        {
            bSameNoData = false;
        }

        if (poTileBand->GetMaskFlags() == GMF_PER_DATASET)
            poMaskBand = poTileBand->GetMaskBand();
        else if (poTileBand->GetColorInterpretation() == GCI_AlphaBand)
            poMaskBand = poTileBand;
    }

    std::unique_ptr<VRTSimpleSource> poSource;
    if (!bHasNoData)
    {
        poSource = std::make_unique<VRTSimpleSource>();
    }
    else
    {
        auto poComplexSource = std::make_unique<VRTComplexSource>();
        poComplexSource->SetNoDataValue(dfNoDataValue);
        poSource = std::move(poComplexSource);
    }

    if (!GetSrcDstWin(adfGeoTransformTile, poTileDS->GetRasterXSize(),
                      poTileDS->GetRasterYSize(), m_adfGeoTransform.data(),
                      GetRasterXSize(), GetRasterYSize(),
                      &poSource->m_dfSrcXOff, &poSource->m_dfSrcYOff,
                      &poSource->m_dfSrcXSize, &poSource->m_dfSrcYSize,
                      &poSource->m_dfDstXOff, &poSource->m_dfDstYOff,
                      &poSource->m_dfDstXSize, &poSource->m_dfDstYSize))
    {
        // Should not happen on a consistent tile index
        CPLDebug("VRT", "Tile %s does not actually intersect area of interest",
                 osTileName.c_str());
        return false;
    }

    oSourceDesc.osName = osTileName;
    oSourceDesc.poDS = std::move(poTileDS);
    oSourceDesc.poSource = std::move(poSource);
    oSourceDesc.bHasNoData = bHasNoData;
    oSourceDesc.bSameNoData = bSameNoData;
    if (bSameNoData)
        oSourceDesc.dfSameNoData = dfNoDataValue;
    oSourceDesc.poMaskBand = poMaskBand;
    return true;
}

/************************************************************************/
/*                        CollectSources()                              */
/************************************************************************/

bool GDALTileIndexDataset::CollectSources(double dfXOff, double dfYOff,
                                          double dfXSize, double dfYSize)
{
    const double dfMinX =
        m_adfGeoTransform[GT_TOPLEFT_X] + dfXOff * m_adfGeoTransform[GT_WE_RES];
    const double dfMaxX = dfMinX + dfXSize * m_adfGeoTransform[GT_WE_RES];
    const double dfMaxY =
        m_adfGeoTransform[GT_TOPLEFT_Y] + dfYOff * m_adfGeoTransform[GT_NS_RES];
    const double dfMinY = dfMaxY + dfYSize * m_adfGeoTransform[GT_NS_RES];

    if (dfMinX == m_dfLastMinXFilter && dfMinY == m_dfLastMinYFilter &&
        dfMaxX == m_dfLastMaxXFilter && dfMaxY == m_dfLastMaxYFilter)
    {
        return true;
    }

    m_dfLastMinXFilter = dfMinX;
    m_dfLastMinYFilter = dfMinY;
    m_dfLastMaxXFilter = dfMaxX;
    m_dfLastMaxYFilter = dfMaxY;

    m_poLayer->SetSpatialFilterRect(dfMinX, dfMinY, dfMaxX, dfMaxY);
    m_poLayer->ResetReading();

    m_aoSourceDesc.clear();
    while (true)
    {
        auto poFeature =
            std::unique_ptr<OGRFeature>(m_poLayer->GetNextFeature());
        if (!poFeature)
            break;
        if (!poFeature->IsFieldSetAndNotNull(m_nLocationFieldIndex))
        {
            continue;
        }

        SourceDesc oSourceDesc;
        oSourceDesc.poFeature = std::move(poFeature);
        m_aoSourceDesc.emplace_back(std::move(oSourceDesc));

        if (m_aoSourceDesc.size() > 10 * 1000 * 1000)
        {
            // Safety belt...
            CPLError(CE_Failure, CPLE_AppDefined,
                     "More than 10 million contributing sources to a "
                     "single RasterIO() request is not supported");
            return false;
        }
    }

    if (m_aoSourceDesc.size() > 1)
    {
        SortSourceDesc();
    }

    // Try to find the last (most prioritary) fully opaque source covering
    // the whole AOI. We only need to start rendering from it.
    size_t i = m_aoSourceDesc.size();
    while (i > 0)
    {
        --i;
        auto &poFeature = m_aoSourceDesc[i].poFeature;
        const char *pszTileName =
            poFeature->GetFieldAsString(m_nLocationFieldIndex);
        const std::string osTileName(
            GetAbsoluteFileName(pszTileName, GetDescription()));

        SourceDesc oSourceDesc;
        if (!GetSourceDesc(osTileName, oSourceDesc))
            continue;

        const auto &poSource = oSourceDesc.poSource;
        if (dfXOff >= poSource->m_dfDstXOff + poSource->m_dfDstXSize ||
            dfYOff >= poSource->m_dfDstYOff + poSource->m_dfDstYSize ||
            poSource->m_dfDstXOff >= dfXOff + dfXSize ||
            poSource->m_dfDstYOff >= dfYOff + dfYSize)
        {
            // Can happen as some spatial filters select slightly more features
            // than strictly needed.
            continue;
        }

        const bool bCoversWholeAOI =
            (poSource->m_dfDstXOff <= dfXOff &&
             poSource->m_dfDstYOff <= dfYOff &&
             poSource->m_dfDstXOff + poSource->m_dfDstXSize >=
                 dfXOff + dfXSize &&
             poSource->m_dfDstYOff + poSource->m_dfDstYSize >=
                 dfYOff + dfYSize);
        oSourceDesc.bCoversWholeAOI = bCoversWholeAOI;

        m_aoSourceDesc[i] = std::move(oSourceDesc);

        if (m_aoSourceDesc[i].bCoversWholeAOI &&
            !m_aoSourceDesc[i].bHasNoData && !m_aoSourceDesc[i].poMaskBand)
        {
            break;
        }
    }

    if (i > 0)
    {
        // Remove sources that will not be rendered
        m_aoSourceDesc.erase(m_aoSourceDesc.begin(),
                             m_aoSourceDesc.begin() + i);
    }

    // Truncate the array when its last elements have no dataset
    i = m_aoSourceDesc.size();
    while (i > 0)
    {
        --i;
        if (!m_aoSourceDesc[i].poDS)
        {
            m_aoSourceDesc.resize(i);
            break;
        }
    }

    return true;
}

/************************************************************************/
/*                          SortSourceDesc()                            */
/************************************************************************/

void GDALTileIndexDataset::SortSourceDesc()
{
    const auto eFieldType = m_nSortFieldIndex >= 0
                                ? m_poLayer->GetLayerDefn()
                                      ->GetFieldDefn(m_nSortFieldIndex)
                                      ->GetType()
                                : OFTMaxType;
    std::sort(
        m_aoSourceDesc.begin(), m_aoSourceDesc.end(),
        [this, eFieldType](const SourceDesc &a, const SourceDesc &b)
        {
            const auto &poFeatureA = (m_bSortFieldAsc ? a : b).poFeature;
            const auto &poFeatureB = (m_bSortFieldAsc ? b : a).poFeature;
            if (m_nSortFieldIndex >= 0 &&
                poFeatureA->IsFieldSetAndNotNull(m_nSortFieldIndex) &&
                poFeatureB->IsFieldSetAndNotNull(m_nSortFieldIndex))
            {
                if (eFieldType == OFTString)
                {
                    const int nCmp =
                        strcmp(poFeatureA->GetFieldAsString(m_nSortFieldIndex),
                               poFeatureB->GetFieldAsString(m_nSortFieldIndex));
                    if (nCmp < 0)
                        return true;
                    if (nCmp > 0)
                        return false;
                }
                else if (eFieldType == OFTInteger || eFieldType == OFTInteger64)
                {
                    const auto nA =
                        poFeatureA->GetFieldAsInteger64(m_nSortFieldIndex);
                    const auto nB =
                        poFeatureB->GetFieldAsInteger64(m_nSortFieldIndex);
                    if (nA < nB)
                        return true;
                    if (nA > nB)
                        return false;
                }
                else if (eFieldType == OFTReal)
                {
                    const auto dfA =
                        poFeatureA->GetFieldAsDouble(m_nSortFieldIndex);
                    const auto dfB =
                        poFeatureB->GetFieldAsDouble(m_nSortFieldIndex);
                    if (dfA < dfB)
                        return true;
                    if (dfA > dfB)
                        return false;
                }
                else if (eFieldType == OFTDate || eFieldType == OFTDateTime)
                {
                    const auto poFieldA =
                        poFeatureA->GetRawFieldRef(m_nSortFieldIndex);
                    const auto poFieldB =
                        poFeatureB->GetRawFieldRef(m_nSortFieldIndex);

#define COMPARE_DATE_COMPONENT(comp)                                           \
    do                                                                         \
    {                                                                          \
        if (poFieldA->Date.comp < poFieldB->Date.comp)                         \
            return true;                                                       \
        if (poFieldA->Date.comp > poFieldB->Date.comp)                         \
            return false;                                                      \
    } while (0)

                    COMPARE_DATE_COMPONENT(Year);
                    COMPARE_DATE_COMPONENT(Month);
                    COMPARE_DATE_COMPONENT(Day);
                    COMPARE_DATE_COMPONENT(Hour);
                    COMPARE_DATE_COMPONENT(Minute);
                    COMPARE_DATE_COMPONENT(Second);
                }
                else
                {
                    CPLAssert(false);
                }
            }
            return poFeatureA->GetFID() < poFeatureB->GetFID();
        });
}

/************************************************************************/
/*                   CompositeSrcWithMaskIntoDest()                     */
/************************************************************************/

static void
CompositeSrcWithMaskIntoDest(const int nOutXSize, const int nOutYSize,
                             const GDALDataType eBufType,
                             const int nBufTypeSize, const GSpacing nPixelSpace,
                             const GSpacing nLineSpace, const GByte *pabySrc,
                             const GByte *const pabyMask, GByte *const pabyDest)
{
    size_t iMaskIdx = 0;
    if (eBufType == GDT_Byte)
    {
        // Optimization for byte case
        for (int iY = 0; iY < nOutYSize; iY++)
        {
            GByte *pabyDestLine =
                pabyDest + static_cast<GPtrDiff_t>(iY * nLineSpace);
            int iX = 0;
#ifdef USE_SSE2_OPTIM
            if (nPixelSpace == 1)
            {
                // SSE2 version up to 6 times faster than portable version
                const auto xmm_zero = _mm_setzero_si128();
                constexpr int SIZEOF_REG = static_cast<int>(sizeof(xmm_zero));
                for (; iX + SIZEOF_REG <= nOutXSize; iX += SIZEOF_REG)
                {
                    auto xmm_mask = _mm_loadu_si128(
                        reinterpret_cast<__m128i const *>(pabyMask + iMaskIdx));
                    const auto xmm_src = _mm_loadu_si128(
                        reinterpret_cast<__m128i const *>(pabySrc));
                    auto xmm_dst = _mm_loadu_si128(
                        reinterpret_cast<__m128i const *>(pabyDestLine));
#ifdef USE_SSE41_OPTIM
                    xmm_dst = _mm_blendv_epi8(xmm_dst, xmm_src, xmm_mask);
#else
                    // mask[i] = 0 becomes 255, and mask[i] != 0 becomes 0
                    xmm_mask = _mm_cmpeq_epi8(xmm_mask, xmm_zero);
                    // dst_data[i] = (mask[i] & dst_data[i]) |
                    //               (~mask[i] & src_data[i])
                    // That is:
                    // dst_data[i] = dst_data[i] when mask[i] = 255
                    // dst_data[i] = src_data[i] when mask[i] = 0
                    xmm_dst = _mm_or_si128(_mm_and_si128(xmm_mask, xmm_dst),
                                           _mm_andnot_si128(xmm_mask, xmm_src));
#endif
                    _mm_storeu_si128(reinterpret_cast<__m128i *>(pabyDestLine),
                                     xmm_dst);
                    pabyDestLine += SIZEOF_REG;
                    pabySrc += SIZEOF_REG;
                    iMaskIdx += SIZEOF_REG;
                }
            }
#endif
            for (; iX < nOutXSize; iX++)
            {
                if (pabyMask[iMaskIdx])
                {
                    *pabyDestLine = *pabySrc;
                }
                pabyDestLine += static_cast<GPtrDiff_t>(nPixelSpace);
                pabySrc++;
                iMaskIdx++;
            }
        }
    }
    else
    {
        for (int iY = 0; iY < nOutYSize; iY++)
        {
            GByte *pabyDestLine =
                pabyDest + static_cast<GPtrDiff_t>(iY * nLineSpace);
            for (int iX = 0; iX < nOutXSize; iX++)
            {
                if (pabyMask[iMaskIdx])
                {
                    memcpy(pabyDestLine, pabySrc, nBufTypeSize);
                }
                pabyDestLine += static_cast<GPtrDiff_t>(nPixelSpace);
                pabySrc += nBufTypeSize;
                iMaskIdx++;
            }
        }
    }
}

/************************************************************************/
/*                         NeedInitBuffer()                             */
/************************************************************************/

// Must be called after CollectSources()
bool GDALTileIndexDataset::NeedInitBuffer(int nBandCount,
                                          const int *panBandMap) const
{
    bool bNeedInitBuffer = true;
    // If the last source (that is the most prioritary one) covers at least
    // the window of interest and is fully opaque, then we don't need to
    // initialize the buffer, and can directly render that source.
    int bHasNoData = false;
    if (!m_aoSourceDesc.empty() && m_aoSourceDesc.back().bCoversWholeAOI &&
        (!m_aoSourceDesc.back().bHasNoData ||
         // Also, if there's a single source and that the VRT bands and the
         // source bands have the same nodata value, we can skip initialization.
         (m_aoSourceDesc.size() == 1 && m_aoSourceDesc.back().bSameNoData &&
          m_bSameNoData && m_bSameDataType &&
          IsSameNaNAware(papoBands[0]->GetNoDataValue(&bHasNoData),
                         m_aoSourceDesc.back().dfSameNoData) &&
          bHasNoData)) &&
        (!m_aoSourceDesc.back().poMaskBand ||
         // Also, if there's a single source that has a mask band, and the VRT
         // bands have no-nodata or a 0-nodata value, we can skip
         // initialization.
         (m_aoSourceDesc.size() == 1 && m_bSameDataType &&
          !(nBandCount == 1 && panBandMap[0] == 0) && m_bSameNoData &&
          papoBands[0]->GetNoDataValue(&bHasNoData) == 0)))
    {
        bNeedInitBuffer = false;
    }
    return bNeedInitBuffer;
}

/************************************************************************/
/*                            InitBuffer()                              */
/************************************************************************/

void GDALTileIndexDataset::InitBuffer(void *pData, int nBufXSize, int nBufYSize,
                                      GDALDataType eBufType, int nBandCount,
                                      const int *panBandMap,
                                      GSpacing nPixelSpace, GSpacing nLineSpace,
                                      GSpacing nBandSpace) const
{
    const int nBufTypeSize = GDALGetDataTypeSizeBytes(eBufType);
    if (m_bSameNoData && nBandCount > 1 &&
        ((nPixelSpace == nBufTypeSize &&
          nLineSpace == nBufXSize * nPixelSpace &&
          nBandSpace == nBufYSize * nLineSpace) ||
         (nBandSpace == nBufTypeSize &&
          nPixelSpace == nBandCount * nBandSpace &&
          nLineSpace == nBufXSize * nPixelSpace)))
    {
        const int nBandNr = panBandMap[0];
        auto poVRTBand =
            nBandNr == 0
                ? m_poMaskBand.get()
                : cpl::down_cast<GDALTileIndexBand *>(papoBands[nBandNr - 1]);
        const double dfNoData = poVRTBand->m_dfNoDataValue;
        if (dfNoData == 0.0)
        {
            memset(pData, 0,
                   static_cast<size_t>(nBufXSize) * nBufYSize * nBandCount *
                       nBufTypeSize);
        }
        else
        {
            GDALCopyWords64(
                &dfNoData, GDT_Float64, 0, pData, eBufType, nBufTypeSize,
                static_cast<size_t>(nBufXSize) * nBufYSize * nBandCount);
        }
    }
    else
    {
        for (int i = 0; i < nBandCount; ++i)
        {
            const int nBandNr = panBandMap[i];
            auto poVRTBand = nBandNr == 0 ? m_poMaskBand.get()
                                          : cpl::down_cast<GDALTileIndexBand *>(
                                                papoBands[nBandNr - 1]);
            GByte *pabyBandData = static_cast<GByte *>(pData) + i * nBandSpace;
            if (nPixelSpace == nBufTypeSize &&
                poVRTBand->m_dfNoDataValue == 0.0)
            {
                if (nLineSpace == nBufXSize * nPixelSpace)
                {
                    memset(pabyBandData, 0,
                           static_cast<size_t>(nBufYSize * nLineSpace));
                }
                else
                {
                    for (int iLine = 0; iLine < nBufYSize; iLine++)
                    {
                        memset(static_cast<GByte *>(pabyBandData) +
                                   static_cast<GIntBig>(iLine) * nLineSpace,
                               0, static_cast<size_t>(nBufXSize * nPixelSpace));
                    }
                }
            }
            else
            {
                double dfWriteValue = poVRTBand->m_dfNoDataValue;

                for (int iLine = 0; iLine < nBufYSize; iLine++)
                {
                    GDALCopyWords(&dfWriteValue, GDT_Float64, 0,
                                  static_cast<GByte *>(pabyBandData) +
                                      static_cast<GIntBig>(nLineSpace) * iLine,
                                  eBufType, static_cast<int>(nPixelSpace),
                                  nBufXSize);
                }
            }
        }
    }
}

/************************************************************************/
/*                             IRasterIO()                              */
/************************************************************************/

CPLErr GDALTileIndexDataset::IRasterIO(GDALRWFlag eRWFlag, int nXOff, int nYOff,
                                       int nXSize, int nYSize, void *pData,
                                       int nBufXSize, int nBufYSize,
                                       GDALDataType eBufType, int nBandCount,
                                       int *panBandMap, GSpacing nPixelSpace,
                                       GSpacing nLineSpace, GSpacing nBandSpace,
                                       GDALRasterIOExtraArg *psExtraArg)
{
    if (eRWFlag != GF_Read)
        return CE_Failure;

    if (nBufXSize < nXSize && nBufYSize < nYSize && AreOverviewsEnabled())
    {
        int bTried = FALSE;
        const CPLErr eErr = TryOverviewRasterIO(
            eRWFlag, nXOff, nYOff, nXSize, nYSize, pData, nBufXSize, nBufYSize,
            eBufType, nBandCount, panBandMap, nPixelSpace, nLineSpace,
            nBandSpace, psExtraArg, &bTried);
        if (bTried)
            return eErr;
    }

    double dfXOff = nXOff;
    double dfYOff = nYOff;
    double dfXSize = nXSize;
    double dfYSize = nYSize;
    if (psExtraArg->bFloatingPointWindowValidity)
    {
        dfXOff = psExtraArg->dfXOff;
        dfYOff = psExtraArg->dfYOff;
        dfXSize = psExtraArg->dfXSize;
        dfYSize = psExtraArg->dfYSize;
    }

    if (!CollectSources(dfXOff, dfYOff, dfXSize, dfYSize))
    {
        return CE_Failure;
    }

    // We might be called with nBandCount == 1 && panBandMap[0] == 0
    // to mean m_poMaskBand
    int nBandNrMax = 0;
    for (int i = 0; i < nBandCount; ++i)
    {
        const int nBandNr = panBandMap[i];
        nBandNrMax = std::max(nBandNrMax, nBandNr);
    }

    const bool bNeedInitBuffer = NeedInitBuffer(nBandCount, panBandMap);

    const auto RenderSource = [this, bNeedInitBuffer, nBandNrMax, nXOff, nYOff,
                               nXSize, nYSize, dfXOff, dfYOff, dfXSize, dfYSize,
                               nBufXSize, nBufYSize, pData, eBufType,
                               nBandCount, panBandMap, nPixelSpace, nLineSpace,
                               nBandSpace, psExtraArg](SourceDesc &oSourceDesc)
    {
        auto &poTileDS = oSourceDesc.poDS;
        auto &poSource = oSourceDesc.poSource;
        auto poComplexSource = dynamic_cast<VRTComplexSource *>(poSource.get());
        CPLErr eErr = CE_None;

        if (poTileDS->GetRasterCount() + 1 == nBandNrMax &&
            GetRasterBand(nBandNrMax)->GetColorInterpretation() ==
                GCI_AlphaBand &&
            GetRasterBand(nBandNrMax)->GetRasterDataType() == GDT_Byte)
        {
            // Special case when there's typically a mix of RGB and RGBA source
            // datasets and we read a RGB one.
            for (int iBand = 0; iBand < nBandCount && eErr == CE_None; ++iBand)
            {
                const int nBandNr = panBandMap[iBand];
                if (nBandNr == nBandNrMax)
                {
                    // The window we will actually request from the source raster band.
                    double dfReqXOff = 0.0;
                    double dfReqYOff = 0.0;
                    double dfReqXSize = 0.0;
                    double dfReqYSize = 0.0;
                    int nReqXOff = 0;
                    int nReqYOff = 0;
                    int nReqXSize = 0;
                    int nReqYSize = 0;

                    // The window we will actual set _within_ the pData buffer.
                    int nOutXOff = 0;
                    int nOutYOff = 0;
                    int nOutXSize = 0;
                    int nOutYSize = 0;

                    bool bError = false;

                    auto poTileBand = poTileDS->GetRasterBand(1);
                    poSource->SetRasterBand(poTileBand, false);
                    if (poSource->GetSrcDstWindow(
                            dfXOff, dfYOff, dfXSize, dfYSize, nBufXSize,
                            nBufYSize, &dfReqXOff, &dfReqYOff, &dfReqXSize,
                            &dfReqYSize, &nReqXOff, &nReqYOff, &nReqXSize,
                            &nReqYSize, &nOutXOff, &nOutYOff, &nOutXSize,
                            &nOutYSize, bError))
                    {
                        GByte *pabyOut =
                            static_cast<GByte *>(pData) +
                            static_cast<GPtrDiff_t>(iBand * nBandSpace +
                                                    nOutXOff * nPixelSpace +
                                                    nOutYOff * nLineSpace);

                        constexpr GByte n255 = 255;
                        for (int iY = 0; iY < nOutYSize; iY++)
                        {
                            GDALCopyWords(&n255, GDT_Byte, 0,
                                          pabyOut + static_cast<GPtrDiff_t>(
                                                        iY * nLineSpace),
                                          eBufType,
                                          static_cast<int>(nPixelSpace),
                                          nOutXSize);
                        }
                    }
                }
                else
                {
                    auto poTileBand = poTileDS->GetRasterBand(nBandNr);
                    if (poComplexSource)
                    {
                        int bHasNoData = false;
                        const double dfNoDataValue =
                            poTileBand->GetNoDataValue(&bHasNoData);
                        poComplexSource->SetNoDataValue(
                            bHasNoData ? dfNoDataValue : VRT_NODATA_UNSET);
                    }
                    poSource->SetRasterBand(poTileBand, false);

                    GDALRasterIOExtraArg sExtraArg;
                    INIT_RASTERIO_EXTRA_ARG(sExtraArg);
                    if (psExtraArg->eResampleAlg != GRIORA_NearestNeighbour)
                        sExtraArg.eResampleAlg = psExtraArg->eResampleAlg;
                    else
                        sExtraArg.eResampleAlg = m_eResampling;

                    GByte *pabyBandData =
                        static_cast<GByte *>(pData) + iBand * nBandSpace;
                    eErr = poSource->RasterIO(
                        poTileBand->GetRasterDataType(), nXOff, nYOff, nXSize,
                        nYSize, pabyBandData, nBufXSize, nBufYSize, eBufType,
                        nPixelSpace, nLineSpace, &sExtraArg, m_oWorkingState);
                }
            }
            return eErr;
        }
        else if (poTileDS->GetRasterCount() < nBandNrMax)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "%s has not enough bands.",
                     oSourceDesc.osName.c_str());
            return CE_Failure;
        }

        if ((oSourceDesc.poMaskBand && bNeedInitBuffer) || nBandNrMax == 0)
        {
            // The window we will actually request from the source raster band.
            double dfReqXOff = 0.0;
            double dfReqYOff = 0.0;
            double dfReqXSize = 0.0;
            double dfReqYSize = 0.0;
            int nReqXOff = 0;
            int nReqYOff = 0;
            int nReqXSize = 0;
            int nReqYSize = 0;

            // The window we will actual set _within_ the pData buffer.
            int nOutXOff = 0;
            int nOutYOff = 0;
            int nOutXSize = 0;
            int nOutYSize = 0;

            bool bError = false;

            auto poFirstTileBand = poTileDS->GetRasterBand(1);
            poSource->SetRasterBand(poFirstTileBand, false);
            if (poSource->GetSrcDstWindow(
                    dfXOff, dfYOff, dfXSize, dfYSize, nBufXSize, nBufYSize,
                    &dfReqXOff, &dfReqYOff, &dfReqXSize, &dfReqYSize, &nReqXOff,
                    &nReqYOff, &nReqXSize, &nReqYSize, &nOutXOff, &nOutYOff,
                    &nOutXSize, &nOutYSize, bError))
            {
                int iMaskBandIdx = -1;
                if (eBufType == GDT_Byte && nBandNrMax == 0)
                {
                    // when called from m_poMaskBand
                    iMaskBandIdx = 0;
                }
                else if (oSourceDesc.poMaskBand)
                {
                    // If we request a Byte buffer and the mask band is actually
                    // one of the queried bands of this request, we can save
                    // requesting it separately.
                    const int nMaskBandNr = oSourceDesc.poMaskBand->GetBand();
                    if (eBufType == GDT_Byte && nMaskBandNr >= 1 &&
                        nMaskBandNr <= poTileDS->GetRasterCount() &&
                        poTileDS->GetRasterBand(nMaskBandNr) ==
                            oSourceDesc.poMaskBand)
                    {
                        for (int iBand = 0; iBand < nBandCount; ++iBand)
                        {
                            if (panBandMap[iBand] == nMaskBandNr)
                            {
                                iMaskBandIdx = iBand;
                                break;
                            }
                        }
                    }
                }

                GDALRasterIOExtraArg sExtraArg;
                INIT_RASTERIO_EXTRA_ARG(sExtraArg);
                if (psExtraArg->eResampleAlg != GRIORA_NearestNeighbour)
                    sExtraArg.eResampleAlg = psExtraArg->eResampleAlg;
                else
                    sExtraArg.eResampleAlg = m_eResampling;
                sExtraArg.bFloatingPointWindowValidity = TRUE;
                sExtraArg.dfXOff = dfReqXOff;
                sExtraArg.dfYOff = dfReqYOff;
                sExtraArg.dfXSize = dfReqXSize;
                sExtraArg.dfYSize = dfReqYSize;

                if (iMaskBandIdx < 0 && oSourceDesc.abyMask.empty() &&
                    oSourceDesc.poMaskBand)
                {
                    // Fetch the mask band
                    try
                    {
                        oSourceDesc.abyMask.resize(
                            static_cast<size_t>(nOutXSize) * nOutYSize);
                    }
                    catch (const std::bad_alloc &)
                    {
                        CPLError(CE_Failure, CPLE_OutOfMemory,
                                 "Cannot allocate working buffer for mask");
                        return CE_Failure;
                    }

                    if (oSourceDesc.poMaskBand->RasterIO(
                            GF_Read, nReqXOff, nReqYOff, nReqXSize, nReqYSize,
                            oSourceDesc.abyMask.data(), nOutXSize, nOutYSize,
                            GDT_Byte, 0, 0, &sExtraArg) != CE_None)
                    {
                        oSourceDesc.abyMask.clear();
                        return CE_Failure;
                    }
                }

                // Allocate a temporary contiguous buffer to receive pixel data
                const int nBufTypeSize = GDALGetDataTypeSizeBytes(eBufType);
                const size_t nWorkBufferBandSize =
                    static_cast<size_t>(nOutXSize) * nOutYSize * nBufTypeSize;
                std::vector<GByte> abyWorkBuffer;
                try
                {
                    abyWorkBuffer.resize(nBandCount * nWorkBufferBandSize);
                }
                catch (const std::bad_alloc &)
                {
                    CPLError(CE_Failure, CPLE_OutOfMemory,
                             "Cannot allocate working buffer");
                    return CE_Failure;
                }

                const GByte *const pabyMask =
                    iMaskBandIdx >= 0 ? abyWorkBuffer.data() +
                                            iMaskBandIdx * nWorkBufferBandSize
                                      : oSourceDesc.abyMask.data();

                if (nBandNrMax == 0)
                {
                    // Special case when called from m_poMaskBand
                    if (poTileDS->GetRasterBand(1)->GetMaskBand()->RasterIO(
                            GF_Read, nReqXOff, nReqYOff, nReqXSize, nReqYSize,
                            abyWorkBuffer.data(), nOutXSize, nOutYSize,
                            eBufType, 0, 0, &sExtraArg) != CE_None)
                    {
                        return CE_Failure;
                    }
                }
                else if (poTileDS->RasterIO(
                             GF_Read, nReqXOff, nReqYOff, nReqXSize, nReqYSize,
                             abyWorkBuffer.data(), nOutXSize, nOutYSize,
                             eBufType, nBandCount, panBandMap, 0, 0, 0,
                             &sExtraArg) != CE_None)
                {
                    return CE_Failure;
                }

                // Compose the temporary contiguous buffer into the target
                // buffer, taking into account the mask
                GByte *pabyOut =
                    static_cast<GByte *>(pData) +
                    static_cast<GPtrDiff_t>(nOutXOff * nPixelSpace +
                                            nOutYOff * nLineSpace);

                for (int iBand = 0; iBand < nBandCount && eErr == CE_None;
                     ++iBand)
                {
                    GByte *pabyDestBand =
                        pabyOut + static_cast<GPtrDiff_t>(iBand * nBandSpace);
                    const GByte *pabySrc =
                        abyWorkBuffer.data() + iBand * nWorkBufferBandSize;

                    CompositeSrcWithMaskIntoDest(nOutXSize, nOutYSize, eBufType,
                                                 nBufTypeSize, nPixelSpace,
                                                 nLineSpace, pabySrc, pabyMask,
                                                 pabyDestBand);
                }
            }
        }
        else if (m_bSameDataType && !bNeedInitBuffer && oSourceDesc.bHasNoData)
        {
            // We create a non-VRTComplexSource SimpleSource copy of poSource
            // to be able to call DatasetRasterIO()
            VRTSimpleSource oSimpleSource(poSource.get(), 1.0, 1.0);

            GDALRasterIOExtraArg sExtraArg;
            INIT_RASTERIO_EXTRA_ARG(sExtraArg);
            if (psExtraArg->eResampleAlg != GRIORA_NearestNeighbour)
                sExtraArg.eResampleAlg = psExtraArg->eResampleAlg;
            else
                sExtraArg.eResampleAlg = m_eResampling;

            auto poTileBand = poTileDS->GetRasterBand(panBandMap[0]);
            oSimpleSource.SetRasterBand(poTileBand, false);
            eErr = oSimpleSource.DatasetRasterIO(
                papoBands[0]->GetRasterDataType(), nXOff, nYOff, nXSize, nYSize,
                pData, nBufXSize, nBufYSize, eBufType, nBandCount, panBandMap,
                nPixelSpace, nLineSpace, nBandSpace, &sExtraArg);
        }
        else if (m_bSameDataType && !poComplexSource)
        {
            auto poTileBand = poTileDS->GetRasterBand(panBandMap[0]);
            poSource->SetRasterBand(poTileBand, false);

            GDALRasterIOExtraArg sExtraArg;
            INIT_RASTERIO_EXTRA_ARG(sExtraArg);
            if (poTileBand->GetColorTable())
                sExtraArg.eResampleAlg = GRIORA_NearestNeighbour;
            else if (psExtraArg->eResampleAlg != GRIORA_NearestNeighbour)
                sExtraArg.eResampleAlg = psExtraArg->eResampleAlg;
            else
                sExtraArg.eResampleAlg = m_eResampling;

            eErr = poSource->DatasetRasterIO(
                papoBands[0]->GetRasterDataType(), nXOff, nYOff, nXSize, nYSize,
                pData, nBufXSize, nBufYSize, eBufType, nBandCount, panBandMap,
                nPixelSpace, nLineSpace, nBandSpace, &sExtraArg);
        }
        else
        {
            for (int i = 0; i < nBandCount && eErr == CE_None; ++i)
            {
                const int nBandNr = panBandMap[i];
                GByte *pabyBandData =
                    static_cast<GByte *>(pData) + i * nBandSpace;
                auto poTileBand = poTileDS->GetRasterBand(nBandNr);
                if (poComplexSource)
                {
                    int bHasNoData = false;
                    const double dfNoDataValue =
                        poTileBand->GetNoDataValue(&bHasNoData);
                    poComplexSource->SetNoDataValue(
                        bHasNoData ? dfNoDataValue : VRT_NODATA_UNSET);
                }
                poSource->SetRasterBand(poTileBand, false);

                GDALRasterIOExtraArg sExtraArg;
                INIT_RASTERIO_EXTRA_ARG(sExtraArg);
                if (poTileBand->GetColorTable())
                    sExtraArg.eResampleAlg = GRIORA_NearestNeighbour;
                else if (psExtraArg->eResampleAlg != GRIORA_NearestNeighbour)
                    sExtraArg.eResampleAlg = psExtraArg->eResampleAlg;
                else
                    sExtraArg.eResampleAlg = m_eResampling;

                eErr = poSource->RasterIO(
                    papoBands[nBandNr - 1]->GetRasterDataType(), nXOff, nYOff,
                    nXSize, nYSize, pabyBandData, nBufXSize, nBufYSize,
                    eBufType, nPixelSpace, nLineSpace, &sExtraArg,
                    m_oWorkingState);
            }
        }
        return eErr;
    };

    if (!bNeedInitBuffer)
    {
        return RenderSource(m_aoSourceDesc.back());
    }
    else
    {
        InitBuffer(pData, nBufXSize, nBufYSize, eBufType, nBandCount,
                   panBandMap, nPixelSpace, nLineSpace, nBandSpace);

        // Now render from bottom of the stack to top.
        for (auto &oSourceDesc : m_aoSourceDesc)
        {
            if (oSourceDesc.poDS && RenderSource(oSourceDesc) != CE_None)
                return CE_Failure;
        }

        return CE_None;
    }
}

/************************************************************************/
/*                         GDALRegister_GTI()                           */
/************************************************************************/

void GDALRegister_GTI()
{
    if (GDALGetDriverByName("GTI") != nullptr)
        return;

    auto poDriver = std::make_unique<VRTDriver>();

    poDriver->SetDescription("GTI");
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "GDAL Raster Tile Index");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSIONS, "gti.gpkg gti.fgb gti");
    poDriver->SetMetadataItem(GDAL_DMD_CONNECTION_PREFIX, GTI_PREFIX);
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drivers/raster/gti.html");

    poDriver->pfnOpen = GDALTileIndexDatasetOpen;
    poDriver->pfnIdentify = GDALTileIndexDatasetIdentify;

    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");

    poDriver->SetMetadataItem(GDAL_DMD_OPENOPTIONLIST,
                              "<OpenOptionList>"
                              "  <Option name='LAYER' type='string'/>"
                              "  <Option name='LOCATION_FIELD' type='string'/>"
                              "  <Option name='SORT_FIELD' type='string'/>"
                              "  <Option name='SORT_FIELD_ASC' type='boolean'/>"
                              "  <Option name='FILTER' type='string'/>"
                              "  <Option name='RESX' type='float'/>"
                              "  <Option name='RESY' type='float'/>"
                              "  <Option name='MINX' type='float'/>"
                              "  <Option name='MINY' type='float'/>"
                              "  <Option name='MAXX' type='float'/>"
                              "  <Option name='MAXY' type='float'/>"
                              "</OpenOptionList>");

    GetGDALDriverManager()->RegisterDriver(poDriver.release());
}

/*! @endcond */
