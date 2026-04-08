/******************************************************************************
 *
 * Purpose : gdal driver for reading Esri compact cache as raster
 *           based on public documentation available at
 *           https://github.com/Esri/raster-tiles-compactcache
 *
 * Author : Lucian Plesea
 *
 * Udate : 06 / 10 / 2020
 *
 *  Copyright 2020 Esri
 *
 * SPDX-License-Identifier: MIT
 *****************************************************************************/

#include "gdal_priv.h"
#include <cassert>
#include <algorithm>
#include <cmath>
#include <map>
#include <random>
#include <sstream>
#include <vector>
#include "cpl_json.h"
#include "gdal_alg.h"
#include "gdal_cpp_functions.h"
#include "gdal_proxy.h"
#include "gdal_utils.h"
#include "gdalwarper.h"
#include "cpl_vsi_virtual.h"
#include "tilematrixset.hpp"

using namespace std;

#if defined(_WIN32) && !defined(__CYGWIN__)
#include <sys/timeb.h>

namespace
{
struct CPLTimeVal
{
    time_t tv_sec; /* seconds */
    long tv_usec;  /* and microseconds */
};
}  // namespace

static int CPLGettimeofday(struct CPLTimeVal *tp, void * /* timezonep*/)
{
    struct _timeb theTime;

    _ftime(&theTime);
    tp->tv_sec = static_cast<time_t>(theTime.time);
    tp->tv_usec = theTime.millitm * 1000;
    return 0;
}
#else
#include <sys/time.h> /* for gettimeofday() */
#define CPLTimeVal timeval
#define CPLGettimeofday(t, u) gettimeofday(t, u)
#endif

CPL_C_START
void CPL_DLL GDALRegister_ESRIC();
CPL_C_END

constexpr int WBSZ = 128;              // tiles per bundle dimension
constexpr int WBSZ2 = WBSZ * WBSZ;     // 16384 tiles per bundle
constexpr int WIDXSZ = WBSZ2 * 8;      // 131072 byte index
constexpr int WHEADER = 64;            // bundle header size

namespace ESRIC
{

#define ENDS_WITH_CI(a, b)                                                     \
    (strlen(a) >= strlen(b) && EQUAL(a + strlen(a) - strlen(b), b))

// ESRI tpkx files use root.json
static int IdentifyJSON(GDALOpenInfo *poOpenInfo)
{
    if (poOpenInfo->eAccess != GA_ReadOnly || poOpenInfo->nHeaderBytes < 512)
        return false;

    // Recognize .tpkx file directly passed
    if (!STARTS_WITH(poOpenInfo->pszFilename, "/vsizip/") &&
#if !defined(FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION)
        ENDS_WITH_CI(poOpenInfo->pszFilename, ".tpkx") &&
#endif
        memcmp(poOpenInfo->pabyHeader, "PK\x03\x04", 4) == 0)
    {
        return true;
    }

#if !defined(FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION)
    if (!ENDS_WITH_CI(poOpenInfo->pszFilename, "root.json"))
        return false;
#endif
    for (int i = 0; i < 2; ++i)
    {
        const std::string osHeader(
            reinterpret_cast<char *>(poOpenInfo->pabyHeader),
            poOpenInfo->nHeaderBytes);
        if (std::string::npos != osHeader.find("tileBundlesPath"))
        {
            return true;
        }
        // If we didn't find tileBundlesPath i, the first bytes, but find
        // other elements typically of .tpkx, then ingest more bytes and
        // retry
        constexpr int MORE_BYTES = 8192;
        if (poOpenInfo->nHeaderBytes < MORE_BYTES &&
            (std::string::npos != osHeader.find("tileInfo") ||
             std::string::npos != osHeader.find("tileImageInfo")))
        {
            poOpenInfo->TryToIngest(MORE_BYTES);
        }
        else
            break;
    }
    return false;
}

// Without full XML parsing, weak, might still fail
static int IdentifyXML(GDALOpenInfo *poOpenInfo)
{
    if (poOpenInfo->eAccess != GA_ReadOnly
#if !defined(FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION)
        || !ENDS_WITH_CI(poOpenInfo->pszFilename, "conf.xml")
#endif
        || poOpenInfo->nHeaderBytes < 512)
        return false;
    CPLString header(reinterpret_cast<char *>(poOpenInfo->pabyHeader),
                     poOpenInfo->nHeaderBytes);
    return (CPLString::npos != header.find("<CacheInfo"));
}

static int Identify(GDALOpenInfo *poOpenInfo)
{
    return (IdentifyXML(poOpenInfo) || IdentifyJSON(poOpenInfo));
}

// Stub default delete, don't delete a tile cache from GDAL
static CPLErr Delete(const char *)
{
    return CE_None;
}

// Read a 32bit unsigned integer stored in little endian
// Same as CPL_LSBUINT32PTR
static inline GUInt32 u32lat(void *data)
{
    GUInt32 val;
    memcpy(&val, data, 4);
    return CPL_LSBWORD32(val);
}

struct Bundle
{
    void Init(const char *filename)
    {
        name = filename;
        fh.reset(VSIFOpenL(name.c_str(), "rb"));
        if (nullptr == fh)
            return;
        GByte header[64] = {0};
        // Check a few header locations, then read the index
        fh->Read(header, 1, 64);
        index.resize(BSZ * BSZ);
        if (3 != u32lat(header) || 5 != u32lat(header + 12) ||
            40 != u32lat(header + 32) || 0 != u32lat(header + 36) ||
            BSZ * BSZ * 8 != u32lat(header + 60) ||
            index.size() != fh->Read(index.data(), 8, index.size()))
        {
            fh.reset();
        }

        if constexpr (!CPL_IS_LSB)
        {
            for (auto &v : index)
                CPL_LSBPTR64(&v);
        }
    }

    std::vector<GUInt64> index{};
    VSIVirtualHandleUniquePtr fh{};
    bool isV2 = false;
    CPLString name{};
    const size_t BSZ = 128;
};

class ECDataset final : public GDALDataset
{
    friend class ECBand;

  public:
    ECDataset();

    CPLErr GetGeoTransform(GDALGeoTransform &gt) const override
    {
        gt = m_gt;
        return CE_None;
    }

    const OGRSpatialReference *GetSpatialRef() const override;

    static GDALDataset *Open(GDALOpenInfo *poOpenInfo);
    static GDALDataset *Open(GDALOpenInfo *poOpenInfo,
                             const char *pszDescription);

  protected:
    GDALGeoTransform m_gt{};
    CPLString dname{};
    int isV2{};  // V2 bundle format
    int BSZ{};   // Bundle size in tiles
    int TSZ{};   // Tile size in pixels
    std::vector<Bundle> bundles{};

    Bundle &GetBundle(const char *fname);

  private:
    CPLErr Initialize(CPLXMLNode *CacheInfo, bool ignoreOversizedLods);
    CPLErr InitializeFromJSON(const CPLJSONObject &oRoot,
                              bool ignoreOversizedLods);
    CPLString compression{};
    std::vector<double> resolutions{};
    int m_nMinLOD = 0;
    OGRSpatialReference oSRS{};
    std::vector<GByte> tilebuffer{};  // Last read tile, decompressed
    std::vector<GByte> filebuffer{};  // raw tile buffer

    OGREnvelope m_sInitialExtent{};
    OGREnvelope m_sFullExtent{};
};

const OGRSpatialReference *ECDataset::GetSpatialRef() const
{
    return &oSRS;
}

class ECBand final : public GDALRasterBand
{
    friend class ECDataset;

  public:
    ECBand(ECDataset *parent, int b, int level = 0);
    ~ECBand() override;

    CPLErr IReadBlock(int xblk, int yblk, void *buffer) override;

    GDALColorInterp GetColorInterpretation() override
    {
        return ci;
    }

    int GetOverviewCount() override
    {
        return static_cast<int>(overviews.size());
    }

    GDALRasterBand *GetOverview(int n) override
    {
        return (n >= 0 && n < GetOverviewCount()) ? overviews[n] : nullptr;
    }

  protected:
  private:
    int lvl{};
    GDALColorInterp ci{};

    // Image image;
    void AddOverviews();
    std::vector<ECBand *> overviews{};
};

ECDataset::ECDataset() : isV2(true), BSZ(128), TSZ(256)
{
}

CPLErr ECDataset::Initialize(CPLXMLNode *CacheInfo, bool ignoreOversizedLods)
{
    CPLErr error = CE_None;
    try
    {
        CPLXMLNode *CSI = CPLGetXMLNode(CacheInfo, "CacheStorageInfo");
        CPLXMLNode *TCI = CPLGetXMLNode(CacheInfo, "TileCacheInfo");
        if (!CSI || !TCI)
            throw CPLString("Error parsing cache configuration");
        auto format = CPLGetXMLValue(CSI, "StorageFormat", "");
        isV2 = EQUAL(format, "esriMapCacheStorageModeCompactV2");
        if (!isV2)
            throw CPLString("Not recognized as esri V2 bundled cache");
        if (BSZ != CPLAtof(CPLGetXMLValue(CSI, "PacketSize", "128")))
            throw CPLString("Only PacketSize of 128 is supported");
        TSZ = static_cast<int>(CPLAtof(CPLGetXMLValue(TCI, "TileCols", "256")));
        if (TSZ != CPLAtof(CPLGetXMLValue(TCI, "TileRows", "256")))
            throw CPLString("Non-square tiles are not supported");
        if (TSZ < 0 || TSZ > 8192)
            throw CPLString("Unsupported TileCols value");

        double minx = CPLAtof(CPLGetXMLValue(TCI, "TileOrigin.X", "-180"));
        double maxy = CPLAtof(CPLGetXMLValue(TCI, "TileOrigin.Y", "90"));
        // Assume symmetric coverage, check custom end
        double maxx = -minx;
        double miny = -maxy;
        const char *pszmaxx = CPLGetXMLValue(TCI, "TileEnd.X", nullptr);
        const char *pszminy = CPLGetXMLValue(TCI, "TileEnd.Y", nullptr);
        if (pszmaxx && pszminy)
        {
            maxx = CPLAtof(pszmaxx);
            miny = CPLAtof(pszminy);
        }

        CPLXMLNode *LODInfo = CPLGetXMLNode(TCI, "LODInfos.LODInfo");
        double res = 0;
        while (LODInfo)
        {
            res = CPLAtof(CPLGetXMLValue(LODInfo, "Resolution", "0"));
            if (!(res > 0))
                throw CPLString("Can't parse resolution for LOD");

            double dxsz = (maxx - minx) / res;
            double dysz = (maxy - miny) / res;
            // Allow size just above INT32_MAX to handle FP rounding. Actual size is later clamped to INT32_MAX
            double maxRasterSize = static_cast<double>(INT32_MAX) + 2;
            if (dxsz < 1 || dxsz > maxRasterSize || dysz < 1 ||
                dysz > maxRasterSize)
            {
                if (ignoreOversizedLods)
                {
                    CPLDebug(
                        "ESRIC",
                        "Skipping resolution %.10f: raster size exceeds the "
                        "GDAL limit",
                        res);
                }
                else
                {
                    throw CPLString(
                        "Too many levels, resulting raster size exceeds "
                        "the GDAL limit. Open with IGNORE_OVERSIZED_LODS=YES "
                        "to ignore this");
                }
            }
            else
            {
                resolutions.push_back(res);
            }

            LODInfo = LODInfo->psNext;
        }

        sort(resolutions.begin(), resolutions.end());
        if (resolutions.empty())
            throw CPLString("Can't parse LODInfos");

        CPLString RawProj(
            CPLGetXMLValue(TCI, "SpatialReference.WKT", "EPSG:4326"));
        if (OGRERR_NONE != oSRS.SetFromUserInput(RawProj.c_str()))
            throw CPLString("Invalid Spatial Reference");
        oSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

        // resolution is the smallest figure
        res = resolutions[0];
        m_gt = GDALGeoTransform();
        m_gt.xorig = minx;
        m_gt.yorig = maxy;
        m_gt.xscale = res;
        m_gt.yscale = -res;

        double dxsz = (maxx - minx) / res;
        double dysz = (maxy - miny) / res;

        nRasterXSize = int(std::min(dxsz, double(INT32_MAX)));
        nRasterYSize = int(std::min(dysz, double(INT32_MAX)));

        SetMetadataItem("INTERLEAVE", "PIXEL", "IMAGE_STRUCTURE");
        compression =
            CPLGetXMLValue(CacheInfo, "TileImageInfo.CacheTileFormat", "JPEG");
        SetMetadataItem("COMPRESS", compression.c_str(), "IMAGE_STRUCTURE");

        nBands = EQUAL(compression, "JPEG") ? 3 : 4;
        for (int i = 1; i <= nBands; i++)
        {
            ECBand *band = new ECBand(this, i);
            SetBand(i, band);
        }
        // Keep 4 bundle files open
        bundles.resize(4);
    }
    catch (CPLString &err)
    {
        error = CE_Failure;
        CPLError(error, CPLE_OpenFailed, "%s", err.c_str());
    }
    return error;
}

static std::unique_ptr<OGRSpatialReference>
CreateSRS(const CPLJSONObject &oSRSRoot)
{
    auto poSRS = std::make_unique<OGRSpatialReference>();

    bool bSuccess = false;
    const int nCode = oSRSRoot.GetInteger("wkid");
    // The concept of LatestWKID is explained in
    // https://support.esri.com/en/technical-article/000013950
    const int nLatestCode = oSRSRoot.GetInteger("latestWkid");

    // Try first with nLatestWKID as there is a higher chance it is a
    // EPSG code and not an ESRI one.
    if (nLatestCode > 0)
    {
        if (nLatestCode > 32767)
        {
            if (poSRS->SetFromUserInput(CPLSPrintf("ESRI:%d", nLatestCode)) ==
                OGRERR_NONE)
            {
                bSuccess = true;
            }
        }
        else if (poSRS->importFromEPSG(nLatestCode) == OGRERR_NONE)
        {
            bSuccess = true;
        }
    }
    if (!bSuccess && nCode > 0)
    {
        if (nCode > 32767)
        {
            if (poSRS->SetFromUserInput(CPLSPrintf("ESRI:%d", nCode)) ==
                OGRERR_NONE)
            {
                bSuccess = true;
            }
        }
        else if (poSRS->importFromEPSG(nCode) == OGRERR_NONE)
        {
            bSuccess = true;
        }
    }
    if (!bSuccess)
    {
        return nullptr;
    }

    poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    return poSRS;
}

CPLErr ECDataset::InitializeFromJSON(const CPLJSONObject &oRoot,
                                     bool ignoreOversizedLods)
{
    CPLErr error = CE_None;
    try
    {
        auto format = oRoot.GetString("storageInfo/storageFormat");
        isV2 = EQUAL(format.c_str(), "esriMapCacheStorageModeCompactV2");
        if (!isV2)
            throw CPLString("Not recognized as esri V2 bundled cache");
        if (BSZ != oRoot.GetInteger("storageInfo/packetSize"))
            throw CPLString("Only PacketSize of 128 is supported");

        TSZ = oRoot.GetInteger("tileInfo/rows");
        if (TSZ != oRoot.GetInteger("tileInfo/cols"))
            throw CPLString("Non-square tiles are not supported");
        if (TSZ < 0 || TSZ > 8192)
            throw CPLString("Unsupported tileInfo/rows value");

        double minx = oRoot.GetDouble("tileInfo/origin/x");
        double maxy = oRoot.GetDouble("tileInfo/origin/y");
        // Assume symmetric coverage
        double maxx = -minx;
        double miny = -maxy;

        const auto oLODs = oRoot.GetArray("tileInfo/lods");
        double res = 0;
        // we need to skip levels that don't have bundle files
        m_nMinLOD = oRoot.GetInteger("minLOD");
        if (m_nMinLOD < 0 || m_nMinLOD >= 31)
            throw CPLString("Invalid minLOD");
        const int maxLOD = std::min(oRoot.GetInteger("maxLOD"), 31);
        for (const auto &oLOD : oLODs)
        {
            const int level = oLOD.GetInteger("level");
            if (level < m_nMinLOD || level > maxLOD)
                continue;

            res = oLOD.GetDouble("resolution");
            if (!(res > 0))
                throw CPLString("Can't parse resolution for LOD");

            double dxsz = (maxx - minx) / res;
            double dysz = (maxy - miny) / res;
            // Allow size just above INT32_MAX to handle FP rounding. Actual size is later clamped to INT32_MAX
            double maxRasterSize = static_cast<double>(INT32_MAX) + 2;
            if (dxsz < 1 || dxsz > maxRasterSize || dysz < 1 ||
                dysz > maxRasterSize)
            {
                if (ignoreOversizedLods)
                {
                    CPLDebug("ESRIC",
                             "Skipping LOD with resolution %.10f: raster size "
                             "exceeds the GDAL limit",
                             res);
                    continue;
                }
                else
                {
                    throw CPLString(
                        "Too many levels, resulting raster size exceeds "
                        "the GDAL limit. Open with IGNORE_OVERSIZED_LODS=YES "
                        "to ignore this");
                }
            }

            resolutions.push_back(res);
        }
        sort(resolutions.begin(), resolutions.end());
        if (resolutions.empty())
            throw CPLString("Can't parse lods");

        {
            auto poSRS = CreateSRS(oRoot.GetObj("spatialReference"));
            if (!poSRS)
            {
                throw CPLString("Invalid Spatial Reference");
            }
            oSRS = std::move(*poSRS);
        }

        // resolution is the smallest figure
        res = resolutions[0];
        m_gt = GDALGeoTransform();
        m_gt.xorig = minx;
        m_gt.yorig = maxy;
        m_gt.xscale = res;
        m_gt.yscale = -res;

        double dxsz = (maxx - minx) / res;
        double dysz = (maxy - miny) / res;

        nRasterXSize = int(std::min(dxsz, double(INT32_MAX)));
        nRasterYSize = int(std::min(dysz, double(INT32_MAX)));

        SetMetadataItem("INTERLEAVE", "PIXEL", "IMAGE_STRUCTURE");
        compression = oRoot.GetString("tileImageInfo/format");
        SetMetadataItem("COMPRESS", compression.c_str(), "IMAGE_STRUCTURE");

        auto oInitialExtent = oRoot.GetObj("initialExtent");
        if (oInitialExtent.IsValid() &&
            oInitialExtent.GetType() == CPLJSONObject::Type::Object)
        {
            m_sInitialExtent.MinX = oInitialExtent.GetDouble("xmin");
            m_sInitialExtent.MinY = oInitialExtent.GetDouble("ymin");
            m_sInitialExtent.MaxX = oInitialExtent.GetDouble("xmax");
            m_sInitialExtent.MaxY = oInitialExtent.GetDouble("ymax");
            auto oSRSRoot = oInitialExtent.GetObj("spatialReference");
            if (oSRSRoot.IsValid())
            {
                auto poSRS = CreateSRS(oSRSRoot);
                if (!poSRS)
                {
                    throw CPLString(
                        "Invalid Spatial Reference in initialExtent");
                }
                if (!poSRS->IsSame(&oSRS))
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "Ignoring initialExtent, because its SRS is "
                             "different from the main one");
                    m_sInitialExtent = OGREnvelope();
                }
            }
        }

        auto oFullExtent = oRoot.GetObj("fullExtent");
        if (oFullExtent.IsValid() &&
            oFullExtent.GetType() == CPLJSONObject::Type::Object)
        {
            m_sFullExtent.MinX = oFullExtent.GetDouble("xmin");
            m_sFullExtent.MinY = oFullExtent.GetDouble("ymin");
            m_sFullExtent.MaxX = oFullExtent.GetDouble("xmax");
            m_sFullExtent.MaxY = oFullExtent.GetDouble("ymax");
            auto oSRSRoot = oFullExtent.GetObj("spatialReference");
            if (oSRSRoot.IsValid())
            {
                auto poSRS = CreateSRS(oSRSRoot);
                if (!poSRS)
                {
                    throw CPLString("Invalid Spatial Reference in fullExtent");
                }
                if (!poSRS->IsSame(&oSRS))
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "Ignoring fullExtent, because its SRS is "
                             "different from the main one");
                    m_sFullExtent = OGREnvelope();
                }
            }
        }

        nBands = EQUAL(compression, "JPEG") ? 3 : 4;
        for (int i = 1; i <= nBands; i++)
        {
            ECBand *band = new ECBand(this, i);
            SetBand(i, band);
        }
        // Keep 4 bundle files open
        bundles.resize(4);
    }
    catch (CPLString &err)
    {
        error = CE_Failure;
        CPLError(error, CPLE_OpenFailed, "%s", err.c_str());
    }
    return error;
}

class ESRICProxyRasterBand final : public GDALProxyRasterBand
{
  private:
    GDALRasterBand *m_poUnderlyingBand = nullptr;

    CPL_DISALLOW_COPY_ASSIGN(ESRICProxyRasterBand)

  protected:
    GDALRasterBand *RefUnderlyingRasterBand(bool /*bForceOpen*/) const override;

  public:
    explicit ESRICProxyRasterBand(GDALRasterBand *poUnderlyingBand)
        : m_poUnderlyingBand(poUnderlyingBand)
    {
        nBand = poUnderlyingBand->GetBand();
        eDataType = poUnderlyingBand->GetRasterDataType();
        nRasterXSize = poUnderlyingBand->GetXSize();
        nRasterYSize = poUnderlyingBand->GetYSize();
        poUnderlyingBand->GetBlockSize(&nBlockXSize, &nBlockYSize);
    }
};

GDALRasterBand *
ESRICProxyRasterBand::RefUnderlyingRasterBand(bool /*bForceOpen*/) const
{
    return m_poUnderlyingBand;
}

class ESRICProxyDataset final : public GDALProxyDataset
{
  private:
    // m_poSrcDS must be placed before m_poUnderlyingDS for proper destruction
    // as m_poUnderlyingDS references m_poSrcDS
    std::unique_ptr<GDALDataset> m_poSrcDS{};
    std::unique_ptr<GDALDataset> m_poUnderlyingDS{};
    CPLStringList m_aosFileList{};

  protected:
    GDALDataset *RefUnderlyingDataset() const override;

  public:
    ESRICProxyDataset(GDALDataset *poSrcDS, GDALDataset *poUnderlyingDS,
                      const char *pszDescription)
        : m_poSrcDS(poSrcDS), m_poUnderlyingDS(poUnderlyingDS)
    {
        nRasterXSize = poUnderlyingDS->GetRasterXSize();
        nRasterYSize = poUnderlyingDS->GetRasterYSize();
        for (int i = 0; i < poUnderlyingDS->GetRasterCount(); ++i)
            SetBand(i + 1, new ESRICProxyRasterBand(
                               poUnderlyingDS->GetRasterBand(i + 1)));
        m_aosFileList.AddString(pszDescription);
    }

    GDALDriver *GetDriver() override
    {
        return GDALDriver::FromHandle(GDALGetDriverByName("ESRIC"));
    }

    char **GetFileList() override
    {
        return CSLDuplicate(m_aosFileList.List());
    }
};

GDALDataset *ESRICProxyDataset::RefUnderlyingDataset() const
{
    return m_poUnderlyingDS.get();
}

GDALDataset *ECDataset::Open(GDALOpenInfo *poOpenInfo)
{
    return Open(poOpenInfo, poOpenInfo->pszFilename);
}

GDALDataset *ECDataset::Open(GDALOpenInfo *poOpenInfo,
                             const char *pszDescription)
{
    bool ignoreOversizedLods = CSLFetchBoolean(poOpenInfo->papszOpenOptions,
                                               "IGNORE_OVERSIZED_LODS", FALSE);
    if (IdentifyXML(poOpenInfo))
    {
        CPLXMLNode *config = CPLParseXMLFile(poOpenInfo->pszFilename);
        if (!config)  // Error was reported from parsing XML
            return nullptr;
        CPLXMLNode *CacheInfo = CPLGetXMLNode(config, "=CacheInfo");
        if (!CacheInfo)
        {
            CPLError(
                CE_Warning, CPLE_OpenFailed,
                "Error parsing configuration, can't find CacheInfo element");
            CPLDestroyXMLNode(config);
            return nullptr;
        }
        auto ds = new ECDataset();
        ds->dname = CPLGetDirnameSafe(poOpenInfo->pszFilename) + "/_alllayers";
        CPLErr error = ds->Initialize(CacheInfo, ignoreOversizedLods);
        CPLDestroyXMLNode(config);
        if (CE_None != error)
        {
            delete ds;
            ds = nullptr;
        }
        return ds;
    }
    else if (IdentifyJSON(poOpenInfo))
    {
        // Recognize .tpkx file directly passed
        if (!STARTS_WITH(poOpenInfo->pszFilename, "/vsizip/") &&
#if !defined(FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION)
            ENDS_WITH_CI(poOpenInfo->pszFilename, ".tpkx") &&
#endif
            memcmp(poOpenInfo->pabyHeader, "PK\x03\x04", 4) == 0)
        {
            GDALOpenInfo oOpenInfo((std::string("/vsizip/{") +
                                    poOpenInfo->pszFilename + "}/root.json")
                                       .c_str(),
                                   GA_ReadOnly);
            oOpenInfo.papszOpenOptions = poOpenInfo->papszOpenOptions;
            return Open(&oOpenInfo, pszDescription);
        }

        CPLJSONDocument oJSONDocument;
        if (!oJSONDocument.Load(poOpenInfo->pszFilename))
        {
            CPLError(CE_Warning, CPLE_OpenFailed,
                     "Error parsing configuration");
            return nullptr;
        }

        const CPLJSONObject &oRoot = oJSONDocument.GetRoot();
        if (!oRoot.IsValid())
        {
            CPLError(CE_Warning, CPLE_OpenFailed, "Invalid json document root");
            return nullptr;
        }

        auto ds = std::make_unique<ECDataset>();
        auto tileBundlesPath = oRoot.GetString("tileBundlesPath");
        // Strip leading relative path indicator (if present)
        if (tileBundlesPath.substr(0, 2) == "./")
        {
            tileBundlesPath.erase(0, 2);
        }

        ds->dname.Printf("%s/%s",
                         CPLGetDirnameSafe(poOpenInfo->pszFilename).c_str(),
                         tileBundlesPath.c_str());
        CPLErr error = ds->InitializeFromJSON(oRoot, ignoreOversizedLods);
        if (CE_None != error)
        {
            return nullptr;
        }

        const bool bIsFullExtentValid =
            (ds->m_sFullExtent.IsInit() &&
             ds->m_sFullExtent.MinX < ds->m_sFullExtent.MaxX &&
             ds->m_sFullExtent.MinY < ds->m_sFullExtent.MaxY);
        const char *pszExtentSource =
            CSLFetchNameValue(poOpenInfo->papszOpenOptions, "EXTENT_SOURCE");

        CPLStringList aosOptions;
        if ((!pszExtentSource && bIsFullExtentValid) ||
            (pszExtentSource && EQUAL(pszExtentSource, "FULL_EXTENT")))
        {
            if (!bIsFullExtentValid)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "fullExtent is not valid");
                return nullptr;
            }
            aosOptions.AddString("-projwin");
            aosOptions.AddString(CPLSPrintf("%.17g", ds->m_sFullExtent.MinX));
            aosOptions.AddString(CPLSPrintf("%.17g", ds->m_sFullExtent.MaxY));
            aosOptions.AddString(CPLSPrintf("%.17g", ds->m_sFullExtent.MaxX));
            aosOptions.AddString(CPLSPrintf("%.17g", ds->m_sFullExtent.MinY));
        }
        else if (pszExtentSource && EQUAL(pszExtentSource, "INITIAL_EXTENT"))
        {
            const bool bIsInitialExtentValid =
                (ds->m_sInitialExtent.IsInit() &&
                 ds->m_sInitialExtent.MinX < ds->m_sInitialExtent.MaxX &&
                 ds->m_sInitialExtent.MinY < ds->m_sInitialExtent.MaxY);
            if (!bIsInitialExtentValid)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "initialExtent is not valid");
                return nullptr;
            }
            aosOptions.AddString("-projwin");
            aosOptions.AddString(
                CPLSPrintf("%.17g", ds->m_sInitialExtent.MinX));
            aosOptions.AddString(
                CPLSPrintf("%.17g", ds->m_sInitialExtent.MaxY));
            aosOptions.AddString(
                CPLSPrintf("%.17g", ds->m_sInitialExtent.MaxX));
            aosOptions.AddString(
                CPLSPrintf("%.17g", ds->m_sInitialExtent.MinY));
        }

        if (!aosOptions.empty())
        {
            aosOptions.AddString("-of");
            aosOptions.AddString("VRT");
            aosOptions.AddString("-co");
            aosOptions.AddString(CPLSPrintf("BLOCKXSIZE=%d", ds->TSZ));
            aosOptions.AddString("-co");
            aosOptions.AddString(CPLSPrintf("BLOCKYSIZE=%d", ds->TSZ));
            auto psOptions =
                GDALTranslateOptionsNew(aosOptions.List(), nullptr);
            auto hDS = GDALTranslate("", GDALDataset::ToHandle(ds.get()),
                                     psOptions, nullptr);
            GDALTranslateOptionsFree(psOptions);
            if (!hDS)
            {
                return nullptr;
            }
            return new ESRICProxyDataset(
                ds.release(), GDALDataset::FromHandle(hDS), pszDescription);
        }
        return ds.release();
    }
    return nullptr;
}

// Fetch a reference to an initialized bundle, based on file name
// The returned bundle could still have an invalid file handle, if the
// target bundle is not valid
Bundle &ECDataset::GetBundle(const char *fname)
{
    for (auto &bundle : bundles)
    {
        // If a bundle is missing, it still occupies a slot, with fh == nullptr
        if (EQUAL(bundle.name.c_str(), fname))
            return bundle;
    }
    // Not found, look for an empty // missing slot
    for (auto &bundle : bundles)
    {
        if (nullptr == bundle.fh)
        {
            bundle.Init(fname);
            return bundle;
        }
    }
    // No empties, eject one
    Bundle &bundle = bundles[
#ifndef __COVERITY__
        rand() % bundles.size()
#else
        0
#endif
    ];
    bundle.Init(fname);
    return bundle;
}

ECBand::~ECBand()
{
    for (auto ovr : overviews)
        if (ovr)
            delete ovr;
    overviews.clear();
}

ECBand::ECBand(ECDataset *parent, int b, int level)
    : lvl(level), ci(GCI_Undefined)
{
    static const GDALColorInterp rgba[4] = {GCI_RedBand, GCI_GreenBand,
                                            GCI_BlueBand, GCI_AlphaBand};
    static const GDALColorInterp la[2] = {GCI_GrayIndex, GCI_AlphaBand};
    poDS = parent;
    nBand = b;

    double factor = parent->resolutions[0] / parent->resolutions[lvl];
    nRasterXSize = static_cast<int>(parent->nRasterXSize * factor + 0.5);
    nRasterYSize = static_cast<int>(parent->nRasterYSize * factor + 0.5);
    nBlockXSize = nBlockYSize = parent->TSZ;

    // Default color interpretation
    assert(b - 1 >= 0);
    if (parent->nBands >= 3)
    {
        assert(b - 1 < static_cast<int>(CPL_ARRAYSIZE(rgba)));
        ci = rgba[b - 1];
    }
    else
    {
        assert(b - 1 < static_cast<int>(CPL_ARRAYSIZE(la)));
        ci = la[b - 1];
    }
    if (0 == lvl)
        AddOverviews();
}

void ECBand::AddOverviews()
{
    auto parent = cpl::down_cast<ECDataset *>(poDS);
    for (size_t i = 1; i < parent->resolutions.size(); i++)
    {
        ECBand *ovl = new ECBand(parent, nBand, int(i));
        if (!ovl)
            break;
        overviews.push_back(ovl);
    }
}

CPLErr ECBand::IReadBlock(int nBlockXOff, int nBlockYOff, void *pData)
{
    auto parent = cpl::down_cast<ECDataset *>(poDS);
    auto &buffer = parent->tilebuffer;
    auto TSZ = parent->TSZ;
    auto BSZ = parent->BSZ;
    size_t nBytes = size_t(TSZ) * TSZ;

    buffer.resize(nBytes * parent->nBands);

    const int lxx = parent->m_nMinLOD +
                    static_cast<int>(parent->resolutions.size() - lvl - 1);
    int bx, by;
    bx = (nBlockXOff / BSZ) * BSZ;
    by = (nBlockYOff / BSZ) * BSZ;
    CPLString fname;
    fname = CPLString().Printf("%s/L%02d/R%04xC%04x.bundle",
                               parent->dname.c_str(), lxx, by, bx);
    Bundle &bundle = parent->GetBundle(fname);
    if (nullptr == bundle.fh)
    {  // This is not an error in general, bundles can be missing
        CPLDebug("ESRIC", "Can't open bundle %s", fname.c_str());
        memset(pData, 0, nBytes);
        return CE_None;
    }
    int block = static_cast<int>((nBlockYOff % BSZ) * BSZ + (nBlockXOff % BSZ));
    GUInt64 offset = bundle.index[block] & 0xffffffffffull;
    GUInt64 size = bundle.index[block] >> 40;
    if (0 == size)
    {
        memset(pData, 0, nBytes);
        return CE_None;
    }
    auto &fbuffer = parent->filebuffer;
    fbuffer.resize(size_t(size));
    bundle.fh->Seek(offset, SEEK_SET);
    if (size != bundle.fh->Read(fbuffer.data(), size_t(1), size_t(size)))
    {
        CPLError(CE_Failure, CPLE_FileIO,
                 "Error reading tile, reading " CPL_FRMT_GUIB
                 " at " CPL_FRMT_GUIB,
                 GUInt64(size), GUInt64(offset));
        return CE_Failure;
    }
    const CPLString magic(VSIMemGenerateHiddenFilename("esric.tmp"));
    auto mfh = VSIFileFromMemBuffer(magic.c_str(), fbuffer.data(), size, false);
    VSIFCloseL(mfh);
    // Can't open a raster by handle?
    auto inds = GDALOpen(magic.c_str(), GA_ReadOnly);
    if (!inds)
    {
        VSIUnlink(magic.c_str());
        CPLError(CE_Failure, CPLE_FileIO, "Error opening tile");
        return CE_Failure;
    }
    // Duplicate first band if not sufficient bands are provided
    auto inbands = GDALGetRasterCount(inds);
    int ubands[4] = {1, 1, 1, 1};
    int *usebands = nullptr;
    int bandcount = parent->nBands;
    GDALColorTableH hCT = nullptr;
    if (inbands != bandcount)
    {
        // Opaque if output expects alpha channel
        if (0 == bandcount % 2)
        {
            fill(buffer.begin(), buffer.end(), GByte(255));
            bandcount--;
        }
        if (3 == inbands)
        {
            // Lacking opacity, copy the first three bands
            ubands[1] = 2;
            ubands[2] = 3;
            usebands = ubands;
        }
        else if (1 == inbands)
        {
            // Grayscale, expecting color
            usebands = ubands;
            // Check for the color table of 1 band rasters
            hCT = GDALGetRasterColorTable(GDALGetRasterBand(inds, 1));
        }
    }

    auto errcode = CE_None;
    if (nullptr != hCT)
    {
        // Expand color indexed to RGB(A)
        errcode = GDALDatasetRasterIO(
            inds, GF_Read, 0, 0, TSZ, TSZ, buffer.data(), TSZ, TSZ, GDT_UInt8,
            1, usebands, parent->nBands, parent->nBands * TSZ, 1);
        if (CE_None == errcode)
        {
            GByte abyCT[4 * 256];
            GByte *pabyTileData = buffer.data();
            const int nEntries = std::min(256, GDALGetColorEntryCount(hCT));
            for (int i = 0; i < nEntries; i++)
            {
                const GDALColorEntry *psEntry = GDALGetColorEntry(hCT, i);
                abyCT[4 * i] = static_cast<GByte>(psEntry->c1);
                abyCT[4 * i + 1] = static_cast<GByte>(psEntry->c2);
                abyCT[4 * i + 2] = static_cast<GByte>(psEntry->c3);
                abyCT[4 * i + 3] = static_cast<GByte>(psEntry->c4);
            }
            for (int i = nEntries; i < 256; i++)
            {
                abyCT[4 * i] = 0;
                abyCT[4 * i + 1] = 0;
                abyCT[4 * i + 2] = 0;
                abyCT[4 * i + 3] = 0;
            }

            if (parent->nBands == 4)
            {
                for (size_t i = 0; i < nBytes; i++)
                {
                    const GByte byVal = pabyTileData[4 * i];
                    pabyTileData[4 * i] = abyCT[4 * byVal];
                    pabyTileData[4 * i + 1] = abyCT[4 * byVal + 1];
                    pabyTileData[4 * i + 2] = abyCT[4 * byVal + 2];
                    pabyTileData[4 * i + 3] = abyCT[4 * byVal + 3];
                }
            }
            else if (parent->nBands == 3)
            {
                for (size_t i = 0; i < nBytes; i++)
                {
                    const GByte byVal = pabyTileData[3 * i];
                    pabyTileData[3 * i] = abyCT[4 * byVal];
                    pabyTileData[3 * i + 1] = abyCT[4 * byVal + 1];
                    pabyTileData[3 * i + 2] = abyCT[4 * byVal + 2];
                }
            }
            else
            {
                // Assuming grayscale output
                for (size_t i = 0; i < nBytes; i++)
                {
                    const GByte byVal = pabyTileData[i];
                    pabyTileData[i] = abyCT[4 * byVal];
                }
            }
        }
    }
    else
    {
        errcode = GDALDatasetRasterIO(
            inds, GF_Read, 0, 0, TSZ, TSZ, buffer.data(), TSZ, TSZ, GDT_UInt8,
            bandcount, usebands, parent->nBands, parent->nBands * TSZ, 1);
    }
    GDALClose(inds);
    VSIUnlink(magic.c_str());
    // Error while unpacking tile
    if (CE_None != errcode)
        return errcode;

    for (int iBand = 1; iBand <= parent->nBands; iBand++)
    {
        auto band = parent->GetRasterBand(iBand);
        if (lvl)
            band = band->GetOverview(lvl - 1);
        GDALRasterBlock *poBlock = nullptr;
        if (band != this)
        {
            poBlock = band->GetLockedBlockRef(nBlockXOff, nBlockYOff, 1);
            if (poBlock != nullptr)
            {
                GDALCopyWords(buffer.data() + iBand - 1, GDT_UInt8,
                              parent->nBands, poBlock->GetDataRef(), GDT_UInt8,
                              1, TSZ * TSZ);
                poBlock->DropLock();
            }
        }
        else
        {
            GDALCopyWords(buffer.data() + iBand - 1, GDT_UInt8, parent->nBands,
                          pData, GDT_UInt8, 1, TSZ * TSZ);
        }
    }

    return CE_None;
}  // IReadBlock

/************************************************************************/
/*                           BundleWriter                               */
/************************************************************************/

// Compact Cache V2 bundle writer.
// See https://github.com/Esri/raster-tiles-compactcache

struct BundleWriter
{
    std::string osTempPath{};
    VSIVirtualHandleUniquePtr fp{};
    std::vector<GUInt64> aIndex{};
    GUInt64 nCurrentOffset = 0;
    GUInt32 nMaxTileSize = 0;

    bool Init(const std::string &osPath)
    {
        osTempPath = osPath;
        fp.reset(VSIFOpenL(osPath.c_str(), "wb"));
        if (!fp)
        {
            CPLError(CE_Failure, CPLE_FileIO, "Failed to create bundle %s",
                     osPath.c_str());
            return false;
        }

        GByte abyHeader[WHEADER] = {};
        auto setU32 = [&](int off, GUInt32 v)
        {
            CPL_LSBPTR32(&v);
            memcpy(abyHeader + off, &v, 4);
        };
        auto setU64 = [&](int off, GUInt64 v)
        {
            CPL_LSBPTR64(&v);
            memcpy(abyHeader + off, &v, 8);
        };

        // Write 64-byte header (little-endian)
        setU32(0, 3);                         // version 32-byte
        setU32(4, WBSZ2);                     // record count 32-byte
        setU32(8, 0);                         // maxRecordSize 32-byte
        setU32(12, 5);                        // offset byte count 32-byte
        setU64(16, 0);                        // slack space 64-byte
        setU64(24, WHEADER + WIDXSZ);         // file size (initial) 64-byte
        setU64(32, 40);                       // user header offset 64-byte
        setU32(40, 20 + WIDXSZ);              // user header size 32-byte
        setU32(44, 3);                        // legacy 32-byte
        setU32(48, 16);                       // legacy 32-byte
        setU32(52, WBSZ2);                    // legacy 32-byte
        setU32(56, 5);                        // legacy 32-byte
        setU32(60, WIDXSZ);                   // index size 32-byte
        fp->Write(abyHeader, 1, WHEADER);

        // Write empty index
        aIndex.assign(WBSZ2, 0);
        std::vector<GByte> zeros(WIDXSZ, 0);
        fp->Write(zeros.data(), 1, WIDXSZ);

        nCurrentOffset = WHEADER + WIDXSZ;
        nMaxTileSize = 0;
        return true;
    }

    bool WriteTile(int nRow, int nCol, const GByte *pabyData, GUInt32 nSize)
    {
        if (!fp || nSize == 0)
            return false;

        // Write 4-byte LE size prefix followed by tile data
        GUInt32 nSizeLE = nSize;
        CPL_LSBPTR32(&nSizeLE);
        fp->Write(&nSizeLE, 1, 4);
        fp->Write(pabyData, 1, nSize);

        // Index offset points to the tile data, not the 4-byte size prefix
        nCurrentOffset += 4;
        const int nIdx = (nRow % WBSZ) * WBSZ + (nCol % WBSZ);
        aIndex[nIdx] = nCurrentOffset | (static_cast<GUInt64>(nSize) << 40);
        nCurrentOffset += nSize;

        if (nSize > nMaxTileSize)
            nMaxTileSize = nSize;
        return true;
    }

    bool Finalize()
    {
        if (!fp)
            return false;

        // Update maxRecordSize at offset 8
        GUInt32 nMaxLE = nMaxTileSize;
        CPL_LSBPTR32(&nMaxLE);
        fp->Seek(static_cast<vsi_l_offset>(8), SEEK_SET);
        fp->Write(&nMaxLE, 1, 4);

        // Update file size at offset 24
        GUInt64 nFileSizeLE = nCurrentOffset;
        CPL_LSBPTR64(&nFileSizeLE);
        fp->Seek(static_cast<vsi_l_offset>(24), SEEK_SET);
        fp->Write(&nFileSizeLE, 1, 8);

        // Write completed index at offset 64
        fp->Seek(static_cast<vsi_l_offset>(WHEADER), SEEK_SET);
        if constexpr (!CPL_IS_LSB)
        {
            for (auto &v : aIndex)
                CPL_LSBPTR64(&v);
        }
        fp->Write(aIndex.data(), 8, WBSZ2);

        fp.reset();
        return true;
    }
};

/************************************************************************/
/*                       BuildRootJSON()                               */
/************************************************************************/

static bool BuildRootJSON(
    const CPLString &tempDir,
    const char* pszFilename,
    const std::vector<gdal::TileMatrixSet::TileMatrix> &aoTMList,
    int nMinLOD, int nMaxLOD, const char *pszFormat, int nQuality,
    int nEPSGCode, const double adfExtent[4])
{
    CPLJSONDocument oDoc;
    CPLJSONObject oRoot = oDoc.GetRoot();

    // Use the base filename as the item name and title
    const std::string osBaseName =
        CPLGetBasenameSafe(pszFilename);

    oRoot.Add("name", osBaseName);
    oRoot.Add("version", "1.0"); // Current version based on https://github.com/Esri/tile-package-spec/blob/master/docs/root.md

    oRoot.Add("tileBundlesPath", "tile");
    oRoot.Add("minLOD", nMinLOD);
    oRoot.Add("maxLOD", nMaxLOD);

    // spatialReference
    const auto addSR = [&](CPLJSONObject &oParent)
    {
        CPLJSONObject oSR;
        oSR.Add("wkid", nEPSGCode);
        oSR.Add("latestWkid", nEPSGCode);
        oParent.Add("spatialReference", oSR);
    };

    addSR(oRoot);

    // tileInfo: origin, tile size, LODs
    const auto &oTM0 = aoTMList[0];
    CPLJSONObject oTileInfo;
    oTileInfo.Add("rows", oTM0.mTileWidth);
    oTileInfo.Add("cols", oTM0.mTileWidth);
    oTileInfo.Add("dpi", 96);

    CPLJSONObject oOrigin;
    oOrigin.Add("x", oTM0.mTopLeftX);
    oOrigin.Add("y", oTM0.mTopLeftY);
    oTileInfo.Add("origin", oOrigin);
    addSR(oTileInfo);

    CPLJSONArray oLods;
    for (int i = nMinLOD; i <= nMaxLOD; i++)
    {
        const double dfRes = aoTMList[i].mResX;
        CPLJSONObject oLod;
        oLod.Add("level", i);
        oLod.Add("resolution", dfRes);
        oLods.Add(oLod);
    }
    oTileInfo.Add("lods", oLods);
    oRoot.Add("tileInfo", oTileInfo);

    // storageInfo
    CPLJSONObject oStorage;
    oStorage.Add("storageFormat", "esriMapCacheStorageModeCompactV2");
    oStorage.Add("packetSize", WBSZ);
    oRoot.Add("storageInfo", oStorage);

    // tileImageInfo
    CPLJSONObject oImageInfo;
    oImageInfo.Add("format", pszFormat);
    oImageInfo.Add("compression", nQuality);
    oRoot.Add("tileImageInfo", oImageInfo);

    // fullExtent / initialExtent
    for (const char *pszKey : {"fullExtent", "initialExtent"})
    {
        CPLJSONObject oExt;
        oExt.Add("xmin", adfExtent[0]);
        oExt.Add("ymin", adfExtent[1]);
        oExt.Add("xmax", adfExtent[2]);
        oExt.Add("ymax", adfExtent[3]);
        addSR(oExt);
        oRoot.Add(pszKey, oExt);
    }

    return oDoc.Save(tempDir + "/root.json");
}

/************************************************************************/
/*                          GenerateUUID()                              */
/************************************************************************/

// Generate an RFC 4122 version-4 UUID.
// Inspired by OFGDBGenerateUUID in the OpenFileGDB driver.
static std::string GenerateUUID()
{
    static uint32_t nCounter = 0;
    struct CPLTimeVal tv;
    memset(&tv, 0, sizeof(tv));

    std::stringstream ss;
    ss << std::hex;

    // First half: xxxxxxxx-xxxx-4xxx
    {
        CPLGettimeofday(&tv, nullptr);
        ++nCounter;
        std::mt19937 gen(nCounter +
                         static_cast<unsigned>(tv.tv_sec ^ tv.tv_usec));
        std::uniform_int_distribution<> dis(0, 15);

        for (int i = 0; i < 8; i++)
            ss << dis(gen);
        ss << "-";
        for (int i = 0; i < 4; i++)
            ss << dis(gen);
        ss << "-4";
        for (int i = 0; i < 3; i++)
            ss << dis(gen);
    }

    // Second half: {8-b}xxx-xxxxxxxxxxxx
    {
        CPLGettimeofday(&tv, nullptr);
        ++nCounter;
        std::mt19937 gen(nCounter +
                         static_cast<unsigned>(tv.tv_sec ^ tv.tv_usec));
        std::uniform_int_distribution<> dis(0, 15);
        std::uniform_int_distribution<> dis2(8, 11);

        ss << "-";
        ss << dis2(gen);
        for (int i = 0; i < 3; i++)
            ss << dis(gen);
        ss << "-";
        for (int i = 0; i < 12; i++)
            ss << dis(gen);
    }

    return ss.str();
}

/************************************************************************/
/*                      BuildItemInfoJSON()                            */
/************************************************************************/

static bool BuildItemInfoJSON(const CPLString& tempDir, const char* pszFilename)
{
    CPLJSONDocument oDoc;
    CPLJSONObject oRoot = oDoc.GetRoot();

    // Use the base filename as the item name and title
    const std::string osBaseName =
        CPLGetBasenameSafe(pszFilename);

    oRoot.Add("name", osBaseName);
    oRoot.Add("version", "1.0"); // Current version based on https://github.com/Esri/tile-package-spec/blob/master/docs/iteminfo.md

    oRoot.Add("guid", GenerateUUID());

    // Unix time in milliseconds
    oRoot.Add("created",
              static_cast<GInt64>(time(nullptr)) * 1000);
    oRoot.Add("title", osBaseName);
    oRoot.Add("type", "Compact Tile Package");

    // The following keywords are the minimum required to be recognized as a tile package
    CPLJSONArray oKeywords;
    oKeywords.Add("Compact Tile Package");
    oKeywords.Add("Tile Package");
    oKeywords.Add("tpkx");
    oRoot.Add("typekeywords", oKeywords);

    return oDoc.Save(tempDir + "/iteminfo.json");
}

/************************************************************************/
/*                         PackageTpkx()                                */
/************************************************************************/

// Zip the contents of osTempDir into the .tpkx file at pszFilename.
// A valid tpkx will contain a root.json, iteminfo.json, and a tile
// subdirectory with LOD subdirectories the contain the bundle files.
static bool PackageTpkx(const char *pszFilename,
                         const std::string &osTempDir)
{
    void *hZIP = CPLCreateZip(pszFilename, nullptr);
    if (!hZIP)
    {
        CPLError(CE_Failure, CPLE_FileIO, "ESRIC: cannot create %s",
                 pszFilename);
        return false;
    }

    bool bOK = true;

    // Loop over all files in osTempDir and add them to the ZIP
    char **papszFiles = VSIReadDirRecursive(osTempDir.c_str());
    for (int i = 0; papszFiles && papszFiles[i] && bOK; i++)
    {
        const std::string osSrcPath =
            CPLFormFilenameSafe(osTempDir.c_str(), papszFiles[i], nullptr);

        VSIStatBufL sStat;
        if (VSIStatL(osSrcPath.c_str(), &sStat) != 0 ||
            VSI_ISDIR(sStat.st_mode))
            continue;  // skip directories

        if (CPLAddFileInZip(hZIP, papszFiles[i], osSrcPath.c_str(),
                            nullptr, nullptr, nullptr,
                            nullptr) != CE_None)
        {
            CPLError(CE_Failure, CPLE_FileIO,
                     "ESRIC: failed to add %s to archive", papszFiles[i]);
            bOK = false;
            break;
        }
    }
    CSLDestroy(papszFiles);

    if (CPLCloseZip(hZIP) != CE_None)
    {
        CPLError(CE_Failure, CPLE_FileIO,
                 "ESRIC: failed to finalize %s", pszFilename);
        bOK = false;
    }

    return bOK;
}

/************************************************************************/
/*                            CreateCopy()                              */
/************************************************************************/

GDALDataset *CreateCopy(const char* pszFilename,
                        GDALDataset* poSrcDS, int bStrict,
                        CSLConstList papszOptions,
                        GDALProgressFunc pfnProgress,
                        void* pProgressData)
{
    if (!pfnProgress)
        pfnProgress = GDALDummyProgress;

    int nBands = poSrcDS->GetRasterCount();
    if (nBands < 1 || nBands > 4)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "ESRIC: source must have 1 to 4 bands, got %d", nBands);
        return nullptr;
    }

    // Detect tiling scheme from source SRS
    const OGRSpatialReference *poSrcSRS = poSrcDS->GetSpatialRef();
    int nEPSGCode = 0;
    if (poSrcSRS)
    {
        OGRSpatialReference oSRS(*poSrcSRS);
        oSRS.AutoIdentifyEPSG();
        const char *pszAuthName = oSRS.GetAuthorityName(nullptr);
        const char *pszAuthCode = oSRS.GetAuthorityCode(nullptr);
        if (pszAuthName && EQUAL(pszAuthName, "EPSG") && pszAuthCode)
            nEPSGCode = atoi(pszAuthCode);
    }

    if (nEPSGCode != 3857 && nEPSGCode != 4326)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "ESRIC: SRS is limited to EPSG:3857 (Web Mercator) or "
                 "EPSG:4326 (WGS84 Geographic)");
        return nullptr;
    }

    // Get the tiling scheme definition.
    // TODO: Can we support custom tiling schemes?
    const auto poTMS = gdal::TileMatrixSet::parse(
        (nEPSGCode == 3857) ? "GoogleMapsCompatible" : "WorldCRS84Quad");
    if (!poTMS)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "ESRIC: failed to determine SRS definition");
        return nullptr;
    }
    const auto &aoTMList = poTMS->tileMatrixList();

    // Determine finest LOD based on source pixel size.
    // TODO: Add creation options to allow specifying min/maxLOD
    GDALGeoTransform gt;
    poSrcDS->GetGeoTransform(gt);
    const double dfSrcPixelSize = std::abs(gt.xscale);

    int nMaxLOD = 0;
    for (int i = 0; i < static_cast<int>(aoTMList.size()); i++)
    {
        if (aoTMList[i].mResX >= dfSrcPixelSize)
        {
            nMaxLOD = i;
        }
        else
        {
            break;
        }
    }

    CPLDebug("ESRIC", "Writing LODs 0 to %d", nMaxLOD);

    // Parse creation options
    const char *pszFormat =
        CSLFetchNameValueDef(papszOptions, "TILE_FORMAT", "JPEG");

    // Create temp directory for bundle files and metadata
    const std::string osTempDir = CPLGenerateTempFilenameSafe("esric");
    if (VSIMkdirRecursive(osTempDir.c_str(), 0755) != 0)
    {
        CPLError(CE_Failure, CPLE_FileIO,
                 "ESRIC: Failed to create temp directory %s", osTempDir.c_str());
        VSIRmdirRecursive(osTempDir.c_str());
        return nullptr;
    }

    // Create tile subdirectory
    const std::string osTileDir =
        CPLFormFilenameSafe(osTempDir.c_str(), "tile", nullptr);
    if (VSIMkdir(osTileDir.c_str(), 0755) != 0)
    {
        CPLError(CE_Failure, CPLE_FileIO,
                 "ESRIC: failed to create directory %s", osTileDir.c_str());
        VSIRmdirRecursive(osTempDir.c_str());
        return nullptr;
    }

    // Compute source extent in target CRS
    OGRSpatialReference oTargetSRS;
    oTargetSRS.importFromEPSG(nEPSGCode);
    oTargetSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

    char *pszTargetWKTRaw = nullptr;
    oTargetSRS.exportToWkt(&pszTargetWKTRaw);
    const std::string osTargetWKT(pszTargetWKTRaw);
    CPLFree(pszTargetWKTRaw);

    CPLStringList aosTO;
    aosTO.SetNameValue("DST_SRS", osTargetWKT.c_str());

    void *hTransformArg = GDALCreateGenImgProjTransformer2(
        GDALDataset::ToHandle(poSrcDS), nullptr, aosTO.List());
    if (!hTransformArg)
    {
        VSIRmdirRecursive(osTempDir.c_str());
        return nullptr;
    }

    double adfDstGT[6] = {};
    int nDstPixels = 0;
    int nDstLines = 0;
    double adfExtent[4] = {};  // minx, miny, maxx, maxy
    CPLErr eErr = GDALSuggestedWarpOutput2(
        GDALDataset::ToHandle(poSrcDS), GDALGenImgProjTransform,
        hTransformArg, adfDstGT, &nDstPixels, &nDstLines, adfExtent, 0);
    GDALDestroyGenImgProjTransformer(hTransformArg);
    hTransformArg = nullptr;

    if (eErr != CE_None)
    {
        VSIRmdirRecursive(osTempDir.c_str());
        return nullptr;
    }

    // Get tile encoder driver
    const bool bJPEG = EQUAL(pszFormat, "JPEG");
    const int nTileBands = bJPEG ? 3 : 4;
    GDALDriverH hTileDriver = GDALGetDriverByName(bJPEG ? "JPEG" : "PNG");
    if (!hTileDriver)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "ESRIC: %s driver not available", bJPEG ? "JPEG" : "PNG");
        VSIRmdirRecursive(osTempDir.c_str());
        return nullptr;
    }

    // Expand paletted 1-band sources to RGB(A) so that resampling operates on colour values
    // instead of palette indices
    GDALDataset *poEffectiveSrcDS = poSrcDS;
    GDALDatasetH hExpandedDS = nullptr;
    if (nBands == 1 &&
        poSrcDS->GetRasterBand(1)->GetColorTable() != nullptr)
    {
        CPLStringList aosTranslateOpts;
        aosTranslateOpts.AddString("-of");
        aosTranslateOpts.AddString("VRT");
        aosTranslateOpts.AddString("-expand");
        aosTranslateOpts.AddString(bJPEG ? "rgb" : "rgba");
        auto psTranslateOpts =
            GDALTranslateOptionsNew(aosTranslateOpts.List(), nullptr);
        hExpandedDS = GDALTranslate(
            "", GDALDataset::ToHandle(poSrcDS), psTranslateOpts, nullptr);
        GDALTranslateOptionsFree(psTranslateOpts);
        if (hExpandedDS)
        {
            poEffectiveSrcDS = GDALDataset::FromHandle(hExpandedDS);
            nBands = poEffectiveSrcDS->GetRasterCount();
        }
    }

    const bool bSrcHasAlpha = (nBands == 2 || nBands == 4);
    const bool bAddAlpha = (!bJPEG && !bSrcHasAlpha);
    int bHasNoData = FALSE;
    const double dfSrcNoData =
        poEffectiveSrcDS->GetRasterBand(1)->GetNoDataValue(&bHasNoData);

    // Collect source overview datasets.
    // Higher indices are progressively coarser.
    std::vector<GDALDatasetUniquePtr> apoOverviewDS;
    const int nOverviews =
        poEffectiveSrcDS->GetRasterBand(1)->GetOverviewCount();
    for (int i = 0; i < nOverviews; ++i)
    {
        GDALDatasetUniquePtr poOvrDS(
            GDALCreateOverviewDataset(poEffectiveSrcDS, i, true));
        if (poOvrDS)
            apoOverviewDS.emplace_back(std::move(poOvrDS));
    }

    // Count total tiles
    int nTotalTiles = 0;
    for (int iLOD = 0; iLOD <= nMaxLOD; iLOD++)
    {
        const auto &oTM = aoTMList[iLOD];
        const double dfTileExtX = oTM.mTileWidth * oTM.mResX;
        const double dfTileExtY = oTM.mTileHeight * oTM.mResY;

        const int nMinCol = std::max(
            0, static_cast<int>(
                   floor((adfExtent[0] - oTM.mTopLeftX) / dfTileExtX)));
        const int nMaxCol = std::min(
            oTM.mMatrixWidth - 1,
            static_cast<int>(
                floor((adfExtent[2] - oTM.mTopLeftX) / dfTileExtX)));
        const int nMinRow = std::max(
            0, static_cast<int>(
                   floor((oTM.mTopLeftY - adfExtent[3]) / dfTileExtY)));
        const int nMaxRow = std::min(
            oTM.mMatrixHeight - 1,
            static_cast<int>(
                floor((oTM.mTopLeftY - adfExtent[1]) / dfTileExtY)));

        if (nMinCol <= nMaxCol && nMinRow <= nMaxRow)
            nTotalTiles +=
                (nMaxCol - nMinCol + 1) * (nMaxRow - nMinRow + 1);
    }

    if (nTotalTiles == 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "ESRIC: source extent does not overlap any tiles");
        VSIRmdirRecursive(osTempDir.c_str());
        return nullptr;
    }

    CPLDebug("ESRIC", "Total tiles to process: %d", nTotalTiles);

    // Generate tiles for each LOD
    int nTilesWritten = 0;
    eErr = CE_None;

    for (int iLOD = 0; iLOD <= nMaxLOD && eErr == CE_None; iLOD++)
    {
        const auto &oTM = aoTMList[iLOD];
        const int nTileW = oTM.mTileWidth;
        const int nTileH = oTM.mTileHeight;
        const double dfResX = oTM.mResX;
        const double dfResY = oTM.mResY;
        const double dfTileExtX = nTileW * dfResX;
        const double dfTileExtY = nTileH * dfResY;
        const size_t nTilePixels =
            static_cast<size_t>(nTileW) * nTileH;

        // Select the coarsest source overview whose resolution does
        // not exceed the target. Fall back to the base dataset.
        GDALDataset *poWarpSrcDS = poEffectiveSrcDS;
        for (const auto &poOvrDS : apoOverviewDS)
        {
            GDALGeoTransform gtOvr;
            if (poOvrDS->GetGeoTransform(gtOvr) != CE_None)
                continue;
            const double dfOvrRes = std::abs(gtOvr.xscale);
            if (dfOvrRes <= dfResX * (1.0 + 1e-10))
                poWarpSrcDS = poOvrDS.get();
            else
                break;
        }

        GDALGeoTransform gtSrc;
        poWarpSrcDS->GetGeoTransform(gtSrc);
        const double dfSrcRes = std::abs(gtSrc.xscale);
        const bool bExactResolutionMatch =
            std::abs(dfSrcRes - dfResX) <= 1e-10 * dfResX;

        // Compute tile range overlapping source extent
        const int nMinCol = std::max(
            0, static_cast<int>(
                   floor((adfExtent[0] - oTM.mTopLeftX) / dfTileExtX)));
        const int nMaxCol = std::min(
            oTM.mMatrixWidth - 1,
            static_cast<int>(
                floor((adfExtent[2] - oTM.mTopLeftX) / dfTileExtX)));
        const int nMinRow = std::max(
            0, static_cast<int>(
                   floor((oTM.mTopLeftY - adfExtent[3]) / dfTileExtY)));
        const int nMaxRow = std::min(
            oTM.mMatrixHeight - 1,
            static_cast<int>(
                floor((oTM.mTopLeftY - adfExtent[1]) / dfTileExtY)));

        if (nMinCol > nMaxCol || nMinRow > nMaxRow)
            continue;

        // Create LOD subdirectory
        const std::string osLODDir = CPLFormFilenameSafe(
            osTileDir.c_str(), CPLSPrintf("L%02d", iLOD), nullptr);
        if (VSIMkdir(osLODDir.c_str(), 0755) != 0)
        {
            CPLError(CE_Failure, CPLE_FileIO,
                     "ESRIC: failed to create directory %s",
                     osLODDir.c_str());
            eErr = CE_Failure;
            break;
        }

        // Build warped VRT covering the tiles at this LOD
        const double dfAlignedMinX =
            oTM.mTopLeftX + nMinCol * dfTileExtX;
        const double dfAlignedMaxY =
            oTM.mTopLeftY - nMinRow * dfTileExtY;
        const int nPixelsX = (nMaxCol - nMinCol + 1) * nTileW;
        const int nPixelsY = (nMaxRow - nMinRow + 1) * nTileH;

        double adfLODGT[6] = {dfAlignedMinX, dfResX, 0.0,
                               dfAlignedMaxY, 0.0,   -dfResY};

        void *hLODTransform = GDALCreateGenImgProjTransformer2(
            GDALDataset::ToHandle(poWarpSrcDS), nullptr, aosTO.List());
        if (!hLODTransform)
        {
            eErr = CE_Failure;
            break;
        }
        GDALSetGenImgProjTransformerDstGeoTransform(hLODTransform,
                                                    adfLODGT);

        void *hApproxTransform = GDALCreateApproxTransformer(
            GDALGenImgProjTransform, hLODTransform, 0.125);
        GDALApproxTransformerOwnsSubtransformer(hApproxTransform, TRUE);

        auto psWO = GDALCreateWarpOptions();
        psWO->hSrcDS = GDALDataset::ToHandle(poWarpSrcDS);
        psWO->eResampleAlg =
            bExactResolutionMatch ? GRA_NearestNeighbour : GRA_Bilinear;
        psWO->eWorkingDataType = GDT_Byte;

        if (bSrcHasAlpha)
        {
            // Source has alpha. Warp color bands only
            psWO->nBandCount = nBands - 1;
            psWO->nSrcAlphaBand = nBands;
            psWO->nDstAlphaBand = nBands;
        }
        else if (bAddAlpha)
        {
            // PNG without source alpha. Warp all source bands
            // and expand to alpha
            psWO->nBandCount = nBands;
            psWO->nDstAlphaBand = nBands + 1;
        }
        else
        {
            // JPEG. No alpha
            psWO->nBandCount = nBands;
        }

        psWO->panSrcBands = static_cast<int *>(
            CPLMalloc(sizeof(int) * psWO->nBandCount));
        psWO->panDstBands = static_cast<int *>(
            CPLMalloc(sizeof(int) * psWO->nBandCount));
        for (int i = 0; i < psWO->nBandCount; i++)
        {
            psWO->panSrcBands[i] = i + 1;
            psWO->panDstBands[i] = i + 1;
        }
        psWO->pTransformerArg = hApproxTransform;
        psWO->pfnTransformer = GDALApproxTransform;
        psWO->papszWarpOptions =
            CSLSetNameValue(nullptr, "INIT_DEST", "0");

        // Set the source no data if it exists
        if (bHasNoData)
        {
            psWO->padfSrcNoDataReal = static_cast<double *>(
                CPLMalloc(sizeof(double) * psWO->nBandCount));
            for (int i = 0; i < psWO->nBandCount; i++)
                psWO->padfSrcNoDataReal[i] = dfSrcNoData;
        }

        GDALDatasetH hWarpedDS = GDALCreateWarpedVRT(
            GDALDataset::ToHandle(poWarpSrcDS), nPixelsX,
            nPixelsY, adfLODGT, psWO);
        GDALDestroyWarpOptions(psWO);

        if (!hWarpedDS)
        {
            GDALDestroyApproxTransformer(hApproxTransform);
            CPLError(CE_Failure, CPLE_AppDefined,
                     "ESRIC: failed to create warped VRT for LOD %d",
                     iLOD);
            eErr = CE_Failure;
            break;
        }

        // Map of Bundle writers for this LOD. Key is the file path
        std::map<std::string, BundleWriter> oBundles;

        // Query the actual VRT band count
        const int nVRTBands = GDALGetRasterCount(hWarpedDS);
        // Number of color bands in the VRT
        const int nColorBands =
            (nTileBands == 4) ? nVRTBands - 1 : nVRTBands;

        for (int nRow = nMinRow;
             nRow <= nMaxRow && eErr == CE_None; nRow++)
        {
            for (int nCol = nMinCol;
                 nCol <= nMaxCol && eErr == CE_None; nCol++)
            {
                const int nXOff = (nCol - nMinCol) * nTileW;
                const int nYOff = (nRow - nMinRow) * nTileH;

                // Read all VRT bands
                std::vector<GByte> abySrc(nTilePixels * nVRTBands, 0);
                if (GDALDatasetRasterIO(
                        hWarpedDS, GF_Read, nXOff, nYOff, nTileW,
                        nTileH, abySrc.data(), nTileW, nTileH,
                        GDT_Byte, nVRTBands, nullptr, 1, nTileW,
                        static_cast<GSpacing>(nTilePixels)) != CE_None)
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "ESRIC: failed to read tile data at "
                             "LOD %d row %d col %d",
                             iLOD, nRow, nCol);
                    eErr = CE_Failure;
                    break;
                }

                // Skip empty tiles
                // PNG: skip when alpha is all zero
                // JPEG: skip when all bytes are zero (consistent with reader returning zeroed
                //          pixels for missing tiles).
                bool bEmpty = false;
                if (nTileBands == 4)
                {
                    const GByte *pabyAlpha =
                        abySrc.data() +
                        static_cast<size_t>(nVRTBands - 1) *
                            nTilePixels;
                    bEmpty = std::all_of(
                        pabyAlpha, pabyAlpha + nTilePixels,
                        [](GByte b) { return b == 0; });
                }
                else
                {
                    // JPEG
                    bEmpty = std::all_of(
                        abySrc.begin(), abySrc.end(),
                        [](GByte b) { return b == 0; });
                }

                if (bEmpty)
                {
                    nTilesWritten++;
                    if (!pfnProgress(
                            static_cast<double>(nTilesWritten) /
                                nTotalTiles,
                            "", pProgressData))
                    {
                        eErr = CE_Failure;
                    }
                    continue;
                }

                // Create MEM dataset and map VRT bands to tile bands
                GDALDriverH hMemDrv = GDALGetDriverByName("MEM");
                GDALDatasetH hMemDS =
                    GDALCreate(hMemDrv, "", nTileW, nTileH, nTileBands,
                               GDT_Byte, nullptr);

                for (int iBand = 0; iBand < nTileBands; iBand++)
                {
                    GDALRasterBandH hBand =
                        GDALGetRasterBand(hMemDS, iBand + 1);

                    if (iBand < 3)
                    {
                        // RGB. Pick VRT color band index
                        const int nSrcIdx =
                            (nColorBands >= 3) ? iBand : 0;
                        GDALRasterIO(
                            hBand, GF_Write, 0, 0, nTileW, nTileH,
                            abySrc.data() + nSrcIdx * nTilePixels,
                            nTileW, nTileH, GDT_Byte, 0, 0);
                    }
                    else
                    {
                        // Alpha band
                        GDALRasterIO(
                            hBand, GF_Write, 0, 0, nTileW, nTileH,
                            abySrc.data() +
                                static_cast<size_t>(nVRTBands - 1) *
                                    nTilePixels,
                            nTileW, nTileH, GDT_Byte, 0, 0);
                    }
                }

                // Set color interpretation for correct encoding
                if (nTileBands >= 3)
                {
                    GDALSetRasterColorInterpretation(
                        GDALGetRasterBand(hMemDS, 1), GCI_RedBand);
                    GDALSetRasterColorInterpretation(
                        GDALGetRasterBand(hMemDS, 2), GCI_GreenBand);
                    GDALSetRasterColorInterpretation(
                        GDALGetRasterBand(hMemDS, 3), GCI_BlueBand);
                }
                if (nTileBands == 4)
                    GDALSetRasterColorInterpretation(
                        GDALGetRasterBand(hMemDS, 4), GCI_AlphaBand);

                // Encode tile to JPEG/PNG
                const std::string osMemFile(
                    VSIMemGenerateHiddenFilename("esric_tile"));
                GDALDatasetH hOutDS =
                    GDALCreateCopy(hTileDriver, osMemFile.c_str(),
                                   hMemDS, FALSE, nullptr, nullptr,
                                   nullptr);
                if (!hOutDS)
                {
                    GDALClose(hMemDS);
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "ESRIC: failed to encode tile at "
                             "LOD %d row %d col %d",
                             iLOD, nRow, nCol);
                    eErr = CE_Failure;
                    break;
                }
                GDALClose(hOutDS);
                GDALClose(hMemDS);

                // Retrieve encoded blob
                vsi_l_offset nBlobSize = 0;
                GByte *pabyBlob = VSIGetMemFileBuffer(
                    osMemFile.c_str(), &nBlobSize, TRUE);

                if (pabyBlob && nBlobSize > 0)
                {
                    // Determine which bundle this tile belongs to
                    const int nBundleRow = (nRow / WBSZ) * WBSZ;
                    const int nBundleCol = (nCol / WBSZ) * WBSZ;
                    const std::string osBundlePath = CPLSPrintf(
                        "%s/R%04xC%04x.bundle", osLODDir.c_str(),
                        nBundleRow, nBundleCol);

                    // Get or create bundle writer
                    auto oIt = oBundles.find(osBundlePath);
                    if (oIt == oBundles.end())
                    {
                        auto oRes = oBundles.emplace(osBundlePath,
                                                     BundleWriter());
                        oIt = oRes.first;
                        if (!oIt->second.Init(osBundlePath))
                        {
                            CPLFree(pabyBlob);
                            eErr = CE_Failure;
                            break;
                        }
                    }

                    if (!oIt->second.WriteTile(
                            nRow, nCol, pabyBlob,
                            static_cast<GUInt32>(nBlobSize)))
                    {
                        CPLError(
                            CE_Failure, CPLE_FileIO,
                            "ESRIC: failed to write tile to bundle "
                            "at LOD %d row %d col %d",
                            iLOD, nRow, nCol);
                        CPLFree(pabyBlob);
                        eErr = CE_Failure;
                        break;
                    }
                }

                if(eErr == CE_None)
                {
                    CPLFree(pabyBlob);
                }

                nTilesWritten++;
                if (!pfnProgress(
                        static_cast<double>(nTilesWritten) /
                            nTotalTiles,
                        "", pProgressData))
                {
                    eErr = CE_Failure;
                }
            }
        }

        // Finalize all bundles for this LOD
        for (auto &[osPath, oWriter] : oBundles)
        {
            oWriter.Finalize();
        }

        // VRTWarpedDataset::CloseDependentDatasets() destroys the
        // transformer stored in the warper options, so we must not
        // destroy it again here.
        GDALClose(hWarpedDS);
    }

    if (hExpandedDS)
        GDALClose(hExpandedDS);

    // ----------------------------------------------------------------
    // Write JSON metadata
    // ----------------------------------------------------------------
    if (eErr == CE_None)
    {
        if (!BuildRootJSON(osTempDir, pszFilename, aoTMList,
                           0, nMaxLOD, pszFormat, 75,
                           nEPSGCode, adfExtent))
        {
            CPLError(CE_Failure, CPLE_FileIO, "Failed to write root.json");
            eErr = CE_Failure;
        }
    }

    if (eErr == CE_None)
    {
        if (!BuildItemInfoJSON(osTempDir, pszFilename))
        {
            CPLError(CE_Failure, CPLE_FileIO,
                     "Failed to write iteminfo.json");
            eErr = CE_Failure;
        }
    }

    // Package temp directory into .tpkx ZIP archive
    if (eErr == CE_None)
    {
        if (!PackageTpkx(pszFilename, osTempDir))
            eErr = CE_Failure;
    }

    // Clean up temp directory
    VSIRmdirRecursive(osTempDir.c_str());

    if (eErr != CE_None)
        return nullptr;

    // Open the created .tpkx and return it
    GDALOpenInfo oOpenInfo(pszFilename, GA_ReadOnly);
    return ECDataset::Open(&oOpenInfo);
}

}  // namespace ESRIC

void CPL_DLL GDALRegister_ESRIC()
{
    if (GDALGetDriverByName("ESRIC") != nullptr)
        return;

    auto poDriver = new GDALDriver;

    poDriver->SetDescription("ESRIC");
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "Esri Compact Cache");

    poDriver->SetMetadataItem(GDAL_DMD_EXTENSIONS, "json tpkx");

    poDriver->SetMetadataItem(
        GDAL_DMD_OPENOPTIONLIST,
        "<OpenOptionList>"
        "  <Option name='EXTENT_SOURCE' type='string-select' "
        "description='Which source is used to determine the extent' "
        "default='FULL_EXTENT'>"
        "    <Value>FULL_EXTENT</Value>"
        "    <Value>INITIAL_EXTENT</Value>"
        "    <Value>TILING_SCHEME</Value>"
        "  </Option>"
        "  <Option name='IGNORE_OVERSIZED_LODS' type='boolean' "
        "description='Whether to silently ignore LODs that exceed the "
        "maximum size supported by GDAL (INT32_MAX)' "
        "default='NO'>"
        "  </Option>"
        "</OpenOptionList>");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATECOPY, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_CREATIONDATATYPES, "Byte");
    poDriver->SetMetadataItem(
        GDAL_DMD_CREATIONOPTIONLIST,
        "<CreationOptionList>"
        "  <Option name='TILE_FORMAT' type='string-select' default='JPEG' "
        "description='Tile image encoding format'>"
        "    <Value>JPEG</Value>"
        "    <Value>PNG</Value>"
        "  </Option>"
        "</CreationOptionList>");

    poDriver->pfnIdentify = ESRIC::Identify;
    poDriver->pfnOpen = ESRIC::ECDataset::Open;
    poDriver->pfnCreateCopy = ESRIC::CreateCopy;
    poDriver->pfnDelete = ESRIC::Delete;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
