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
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this softwareand associated documentation files(the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and /or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions :
 *
 * The above copyright noticeand this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *****************************************************************************/

#include "gdal_priv.h"
#include <cassert>
#include <vector>
#include <algorithm>
#include "cpl_json.h"
#include "gdal_proxy.h"
#include "gdal_utils.h"

using namespace std;

CPL_C_START
void CPL_DLL GDALRegister_ESRIC();
CPL_C_END

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
    Bundle() : fh(nullptr), isV2(true), isTpkx(false)
    {
    }

    ~Bundle()
    {
        if (fh)
            VSIFCloseL(fh);
        fh = nullptr;
    }

    void Init(const char *filename)
    {
        if (fh)
            VSIFCloseL(fh);
        name = filename;
        fh = VSIFOpenL(name.c_str(), "rb");
        if (nullptr == fh)
            return;
        GByte header[64] = {0};
        // Check a few header locations, then read the index
        VSIFReadL(header, 1, 64, fh);
        index.resize(BSZ * BSZ);
        if (3 != u32lat(header) || 5 != u32lat(header + 12) ||
            40 != u32lat(header + 32) || 0 != u32lat(header + 36) ||
            (!isTpkx &&
             BSZ * BSZ != u32lat(header + 4)) || /* skip this check for tpkx */
            BSZ * BSZ * 8 != u32lat(header + 60) ||
            index.size() != VSIFReadL(index.data(), 8, index.size(), fh))
        {
            VSIFCloseL(fh);
            fh = nullptr;
        }

#if !CPL_IS_LSB
        for (auto &v : index)
            CPL_LSBPTR64(&v);
        return;
#endif
    }

    std::vector<GUInt64> index;
    VSILFILE *fh;
    bool isV2;
    bool isTpkx;
    CPLString name;
    const size_t BSZ = 128;
};

class ECDataset final : public GDALDataset
{
    friend class ECBand;

  public:
    ECDataset();

    virtual ~ECDataset()
    {
    }

    CPLErr GetGeoTransform(double *gt) override
    {
        memcpy(gt, GeoTransform, sizeof(GeoTransform));
        return CE_None;
    }

    virtual const OGRSpatialReference *GetSpatialRef() const override
    {
        return &oSRS;
    }

    static GDALDataset *Open(GDALOpenInfo *poOpenInfo);
    static GDALDataset *Open(GDALOpenInfo *poOpenInfo,
                             const char *pszDescription);

  protected:
    double GeoTransform[6];
    CPLString dname;
    int isV2;  // V2 bundle format
    int BSZ;   // Bundle size in tiles
    int TSZ;   // Tile size in pixels
    std::vector<Bundle> bundles;

    Bundle &GetBundle(const char *fname);

  private:
    CPLErr Initialize(CPLXMLNode *CacheInfo);
    CPLErr InitializeFromJSON(const CPLJSONObject &oRoot);
    CPLString compression;
    std::vector<double> resolutions;
    int m_nMinLOD = 0;
    OGRSpatialReference oSRS;
    std::vector<GByte> tilebuffer;  // Last read tile, decompressed
    std::vector<GByte> filebuffer;  // raw tile buffer

    OGREnvelope m_sInitialExtent{};
    OGREnvelope m_sFullExtent{};
};

class ECBand final : public GDALRasterBand
{
    friend class ECDataset;

  public:
    ECBand(ECDataset *parent, int b, int level = 0);
    virtual ~ECBand();

    virtual CPLErr IReadBlock(int xblk, int yblk, void *buffer) override;

    virtual GDALColorInterp GetColorInterpretation() override
    {
        return ci;
    }

    virtual int GetOverviewCount() override
    {
        return static_cast<int>(overviews.size());
    }

    virtual GDALRasterBand *GetOverview(int n) override
    {
        return (n >= 0 && n < GetOverviewCount()) ? overviews[n] : nullptr;
    }

  protected:
  private:
    int lvl;
    GDALColorInterp ci;

    // Image image;
    void AddOverviews();
    std::vector<ECBand *> overviews;
};

ECDataset::ECDataset() : isV2(true), BSZ(128), TSZ(256)
{
    double gt[6] = {0, 1, 0, 0, 0, 1};
    memcpy(GeoTransform, gt, sizeof(gt));
}

CPLErr ECDataset::Initialize(CPLXMLNode *CacheInfo)
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

        CPLXMLNode *LODInfo = CPLGetXMLNode(TCI, "LODInfos.LODInfo");
        double res = 0;
        while (LODInfo)
        {
            res = CPLAtof(CPLGetXMLValue(LODInfo, "Resolution", "0"));
            if (!(res > 0))
                throw CPLString("Can't parse resolution for LOD");
            resolutions.push_back(res);
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
        double gt[6] = {0, 1, 0, 0, 0, 1};
        gt[0] = CPLAtof(CPLGetXMLValue(TCI, "TileOrigin.X", "-180"));
        gt[3] = CPLAtof(CPLGetXMLValue(TCI, "TileOrigin.Y", "90"));
        gt[1] = res;
        gt[5] = -res;
        memcpy(GeoTransform, gt, sizeof(gt));

        // Assume symmetric coverage, check custom end
        double maxx = -gt[0];
        double miny = -gt[3];
        const char *pszmaxx = CPLGetXMLValue(TCI, "TileEnd.X", nullptr);
        const char *pszminy = CPLGetXMLValue(TCI, "TileEnd.Y", nullptr);
        if (pszmaxx && pszminy)
        {
            maxx = CPLAtof(pszmaxx);
            miny = CPLAtof(pszminy);
        }

        double dxsz = (maxx - gt[0]) / res;
        double dysz = (gt[3] - miny) / res;
        if (dxsz < 1 || dxsz > INT32_MAX || dysz < 1 || dysz > INT32_MAX)
            throw CPLString("Too many levels, resulting raster size exceeds "
                            "the GDAL limit");

        nRasterXSize = int(dxsz);
        nRasterYSize = int(dysz);

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

CPLErr ECDataset::InitializeFromJSON(const CPLJSONObject &oRoot)
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

        const auto oLODs = oRoot.GetArray("tileInfo/lods");
        double res = 0;
        // we need to skip levels that don't have bundle files
        m_nMinLOD = oRoot.GetInteger("minLOD");
        if (m_nMinLOD < 0 || m_nMinLOD >= 31)
            throw CPLString("Invalid minLOD");
        const int maxLOD = std::min(oRoot.GetInteger("maxLOD"), 31);
        for (const auto &oLOD : oLODs)
        {
            res = oLOD.GetDouble("resolution");
            if (!(res > 0))
                throw CPLString("Can't parse resolution for LOD");
            const int level = oLOD.GetInteger("level");
            if (level >= m_nMinLOD && level <= maxLOD)
            {
                resolutions.push_back(res);
            }
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
        double gt[6] = {0, 1, 0, 0, 0, 1};
        gt[0] = oRoot.GetDouble("tileInfo/origin/x");
        gt[3] = oRoot.GetDouble("tileInfo/origin/y");
        gt[1] = res;
        gt[5] = -res;
        memcpy(GeoTransform, gt, sizeof(gt));

        // Assume symmetric coverage
        double maxx = -gt[0];
        double miny = -gt[3];

        double dxsz = (maxx - gt[0]) / res;
        double dysz = (gt[3] - miny) / res;
        if (dxsz < 1 || dxsz > INT32_MAX || dysz < 1 || dysz > INT32_MAX)
            throw CPLString("Too many levels, resulting raster size exceeds "
                            "the GDAL limit");

        nRasterXSize = int(dxsz);
        nRasterYSize = int(dysz);

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
        // Set the tile package flag in the bundles
        for (auto &bundle : bundles)
        {
            bundle.isTpkx = true;
        }
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

  protected:
    GDALRasterBand *RefUnderlyingRasterBand(bool /*bForceOpen*/) const override
    {
        return m_poUnderlyingBand;
    }

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

class ESRICProxyDataset final : public GDALProxyDataset
{
  private:
    std::unique_ptr<GDALDataset> m_poUnderlyingDS{};
    CPLStringList m_aosFileList{};

  protected:
    GDALDataset *RefUnderlyingDataset() const override
    {
        return m_poUnderlyingDS.get();
    }

  public:
    ESRICProxyDataset(GDALDataset *poUnderlyingDS, const char *pszDescription)
        : m_poUnderlyingDS(poUnderlyingDS)
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

GDALDataset *ECDataset::Open(GDALOpenInfo *poOpenInfo)
{
    return Open(poOpenInfo, poOpenInfo->pszFilename);
}

GDALDataset *ECDataset::Open(GDALOpenInfo *poOpenInfo,
                             const char *pszDescription)
{
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
        ds->dname.Printf("%s/_alllayers",
                         CPLGetDirname(poOpenInfo->pszFilename));
        CPLErr error = ds->Initialize(CacheInfo);
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

        ds->dname.Printf("%s/%s", CPLGetDirname(poOpenInfo->pszFilename),
                         tileBundlesPath.c_str());
        CPLErr error = ds->InitializeFromJSON(oRoot);
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
            aosOptions.AddString(CPLSPrintf("%.18g", ds->m_sFullExtent.MinX));
            aosOptions.AddString(CPLSPrintf("%.18g", ds->m_sFullExtent.MaxY));
            aosOptions.AddString(CPLSPrintf("%.18g", ds->m_sFullExtent.MaxX));
            aosOptions.AddString(CPLSPrintf("%.18g", ds->m_sFullExtent.MinY));
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
                CPLSPrintf("%.18g", ds->m_sInitialExtent.MinX));
            aosOptions.AddString(
                CPLSPrintf("%.18g", ds->m_sInitialExtent.MaxY));
            aosOptions.AddString(
                CPLSPrintf("%.18g", ds->m_sInitialExtent.MaxX));
            aosOptions.AddString(
                CPLSPrintf("%.18g", ds->m_sInitialExtent.MinY));
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
            auto hDS = GDALTranslate("", GDALDataset::ToHandle(ds.release()),
                                     psOptions, nullptr);
            GDALTranslateOptionsFree(psOptions);
            if (!hDS)
            {
                return nullptr;
            }
            return new ESRICProxyDataset(GDALDataset::FromHandle(hDS),
                                         pszDescription);
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
    // coverity[dont_call]
    Bundle &bundle = bundles[rand() % bundles.size()];
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
    auto parent = reinterpret_cast<ECDataset *>(poDS);
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
    auto parent = reinterpret_cast<ECDataset *>(poDS);
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
    VSIFSeekL(bundle.fh, offset, SEEK_SET);
    if (size != VSIFReadL(fbuffer.data(), size_t(1), size_t(size), bundle.fh))
    {
        CPLError(CE_Failure, CPLE_FileIO,
                 "Error reading tile, reading " CPL_FRMT_GUIB
                 " at " CPL_FRMT_GUIB,
                 GUInt64(size), GUInt64(offset));
        return CE_Failure;
    }
    CPLString magic;
    // Should use some sort of unique
    magic.Printf("/vsimem/esric_%p.tmp", this);
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
            inds, GF_Read, 0, 0, TSZ, TSZ, buffer.data(), TSZ, TSZ, GDT_Byte, 1,
            usebands, parent->nBands, parent->nBands * TSZ, 1);
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
            inds, GF_Read, 0, 0, TSZ, TSZ, buffer.data(), TSZ, TSZ, GDT_Byte,
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
                GDALCopyWords(buffer.data() + iBand - 1, GDT_Byte,
                              parent->nBands, poBlock->GetDataRef(), GDT_Byte,
                              1, TSZ * TSZ);
                poBlock->DropLock();
            }
        }
        else
        {
            GDALCopyWords(buffer.data() + iBand - 1, GDT_Byte, parent->nBands,
                          pData, GDT_Byte, 1, TSZ * TSZ);
        }
    }

    return CE_None;
}  // IReadBlock

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
        "</OpenOptionList>");
    poDriver->pfnIdentify = ESRIC::Identify;
    poDriver->pfnOpen = ESRIC::ECDataset::Open;
    poDriver->pfnDelete = ESRIC::Delete;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
