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

using namespace std;

CPL_C_START
void CPL_DLL GDALRegister_ESRIC();
CPL_C_END

namespace ESRIC {

#define ENDS_WITH_CI(a,b) (strlen(a) >= strlen(b) && EQUAL(a + strlen(a) - strlen(b), b))

// Without full XML parsing, weak, might still fail
static int Identify(GDALOpenInfo* poOpenInfo) {
    if (poOpenInfo->eAccess != GA_ReadOnly
#if !defined(FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION)
        || !ENDS_WITH_CI(poOpenInfo->pszFilename, "conf.xml")
#endif
        || poOpenInfo->nHeaderBytes < 512
        )
        return false;
    CPLString header(reinterpret_cast<char*>(poOpenInfo->pabyHeader), poOpenInfo->nHeaderBytes);
    return (CPLString::npos != header.find("<CacheInfo"));
}

    // Stub default delete, don't delete a tile cache from GDAL
    static CPLErr Delete(const char*) { return CE_None; }

// Read a 32bit unsigned integer stored in little endian
// Same as CPL_LSBUINT32PTR
static inline GUInt32 u32lat(void* data) {
    GUInt32 val;
    memcpy(&val, data, 4);
    return CPL_LSBWORD32(val);
}

struct Bundle {
    Bundle() : fh(nullptr), isV2(true) {}
    ~Bundle() {
        if (fh)
            VSIFCloseL(fh);
        fh = nullptr;
    }
    void Init(const char* filename) {
        if (fh)
            VSIFCloseL(fh);
        name = filename;
        fh = VSIFOpenL(name.c_str(), "rb");
        if (nullptr == fh)
            return;
        GByte header[64] = { 0 };
        // Check a few header locations, then read the index
        VSIFReadL(header, 1, 64, fh);
        index.resize(BSZ * BSZ);
        if (3 != u32lat(header)
            || 5 != u32lat(header + 12)
            || 40 != u32lat(header + 32)
            || 0 != u32lat(header + 36)
            || BSZ * BSZ != u32lat(header + 4)
            || BSZ * BSZ * 8 != u32lat(header + 60)
            || index.size() != VSIFReadL(index.data(), 8, index.size(), fh))
        {
            VSIFCloseL(fh);
            fh = nullptr;
        }

#if !CPL_IS_LSB
        for (auto& v : index)
            CPL_LSBPTR64(&v);
        return;
#endif

    }
    std::vector<GUInt64> index;
    VSILFILE* fh;
    bool isV2;
    CPLString name;
    const size_t BSZ = 128;
};

class ECDataset final : public GDALDataset {
    friend class ECBand;

public:
    ECDataset();
    virtual ~ECDataset() {}

    CPLErr GetGeoTransform(double* gt) override {
        memcpy(gt, GeoTransform, sizeof(GeoTransform));
        return CE_None;
    }

    virtual const OGRSpatialReference* GetSpatialRef() const override {
        return &oSRS;
    }

    static GDALDataset* Open(GDALOpenInfo* poOpenInfo);

protected:
    double GeoTransform[6];
    CPLString dname;
    int isV2; // V2 bundle format
    int BSZ; // Bundle size in tiles
    int TSZ; // Tile size in pixels
    std::vector<Bundle> bundles;

    Bundle& GetBundle(const char* fname);

private:
    CPLErr Initialize(CPLXMLNode* CacheInfo);
    CPLString compression;
    std::vector<double> resolutions;
    OGRSpatialReference oSRS;
    std::vector<GByte> tilebuffer; // Last read tile, decompressed
    std::vector<GByte> filebuffer; // raw tile buffer
};

class ECBand final : public GDALRasterBand {
    friend class ECDataset;

public:
    ECBand(ECDataset* parent, int b, int level = 0);
    virtual ~ECBand();

    virtual CPLErr IReadBlock(int xblk, int yblk, void* buffer) override;
    virtual GDALColorInterp GetColorInterpretation() override { return ci; }
    virtual int GetOverviewCount() override { return static_cast<int>(overviews.size()); }
    virtual GDALRasterBand* GetOverview(int n) override {
        return (n >= 0 && n < GetOverviewCount()) ? overviews[n] : nullptr;
    }
protected:
private:
    int lvl;
    GDALColorInterp ci;

    //Image image;
    void AddOverviews();
    std::vector<ECBand*> overviews;
};

ECDataset::ECDataset() : isV2(true), BSZ(128), TSZ(256)
{
    double gt[6] = { 0, 1, 0, 0, 0, 1 };
    memcpy(GeoTransform, gt, sizeof(gt));
}

CPLErr ECDataset::Initialize(CPLXMLNode* CacheInfo) {
    CPLErr error = CE_None;
    try {
        CPLXMLNode* CSI = CPLGetXMLNode(CacheInfo, "CacheStorageInfo");
        CPLXMLNode* TCI = CPLGetXMLNode(CacheInfo, "TileCacheInfo");
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

        CPLXMLNode* LODInfo = CPLGetXMLNode(TCI, "LODInfos.LODInfo");
        double res = 0;
        while (LODInfo) {
            res = CPLAtof(CPLGetXMLValue(LODInfo, "Resolution", "0"));
            if (!(res > 0))
                throw CPLString("Can't parse resolution for LOD");
            resolutions.push_back(res);
            LODInfo = LODInfo->psNext;
        }
        sort(resolutions.begin(), resolutions.end());
        if (resolutions.empty())
            throw CPLString("Can't parse LODInfos");

        CPLString RawProj(CPLGetXMLValue(TCI, "SpatialReference.WKT", "EPSG:4326"));
        if (OGRERR_NONE != oSRS.SetFromUserInput(RawProj.c_str()))
            throw CPLString("Invalid Spatial Reference");
        oSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

        // resolution is the smallest figure
        res = resolutions[0];
        double gt[6] = { 0, 1, 0, 0, 0, 1 };
        gt[0] = CPLAtof(CPLGetXMLValue(TCI, "TileOrigin.X", "-180"));
        gt[3] = CPLAtof(CPLGetXMLValue(TCI, "TileOrigin.Y", "90"));
        gt[1] = res;
        gt[5] = -res;
        memcpy(GeoTransform, gt, sizeof(gt));

        // Assume symmetric coverage, check custom end
        double maxx = -gt[0];
        double miny = -gt[3];
        const char* pszmaxx = CPLGetXMLValue(TCI, "TileEnd.X", nullptr);
        const char* pszminy = CPLGetXMLValue(TCI, "TileEnd.Y", nullptr);
        if (pszmaxx && pszminy) {
            maxx = CPLAtof(pszmaxx);
            miny = CPLAtof(pszminy);
        }

        double dxsz = (maxx - gt[0]) / res;
        double dysz = (gt[3] - miny) / res;
        if (dxsz < 1 || dxsz > INT32_MAX || dysz < 1 || dysz > INT32_MAX)
            throw CPLString("Too many levels, resulting raster size exceeds the GDAL limit");

        nRasterXSize = int(dxsz);
        nRasterYSize = int(dysz);

        SetMetadataItem("INTERLEAVE", "PIXEL", "IMAGE_STRUCTURE");
        compression = CPLGetXMLValue(CacheInfo, "TileImageInfo.CacheTileFormat", "JPEG");
        SetMetadataItem("COMPRESS", compression.c_str(), "IMAGE_STRUCTURE");

        nBands = EQUAL(compression, "JPEG") ? 3 : 4;
        for (int i = 1; i <= nBands; i++) {
            ECBand* band = new ECBand(this, i);
            SetBand(i, band);
        }
        // Keep 4 bundle files open
        bundles.resize(4);
    }
    catch (CPLString& err) {
        error = CE_Failure;
        CPLError(error, CPLE_OpenFailed, "%s", err.c_str());
    }
    return error;
}

GDALDataset* ECDataset::Open(GDALOpenInfo* poOpenInfo)
{
    if (!Identify(poOpenInfo))
        return nullptr;

    CPLXMLNode* config = CPLParseXMLFile(poOpenInfo->pszFilename);
    if (!config) // Error was reported from parsing XML
        return nullptr;
    CPLXMLNode* CacheInfo = CPLGetXMLNode(config, "=CacheInfo");
    if (!CacheInfo) {
        CPLError(CE_Warning, CPLE_OpenFailed, "Error parsing configuration, can't find CacheInfo element");
        CPLDestroyXMLNode(config);
        return nullptr;
    }
    auto ds = new ECDataset();
    ds->dname.Printf("%s/_alllayers", CPLGetDirname(poOpenInfo->pszFilename));
    CPLErr error = ds->Initialize(CacheInfo);
    CPLDestroyXMLNode(config);
    if (CE_None != error) {
        delete ds;
        ds = nullptr;
    }
    return ds;
}

// Fetch a reference to an initialized bundle, based on file name
// The returned bundle could still have an invalid file handle, if the
// target bundle is not valid
Bundle& ECDataset::GetBundle(const char* fname) {
    for (auto& bundle : bundles) {
        // If a bundle is missing, it still occupies a slot, with fh == nullptr
        if (EQUAL(bundle.name.c_str(), fname))
            return bundle;
    }
    // Not found, look for an empty // missing slot
    for (auto& bundle : bundles) {
        if (nullptr == bundle.fh) {
            bundle.Init(fname);
            return bundle;
        }
    }
    // No empties, eject one
    // coverity[dont_call]
    Bundle& bundle = bundles[rand() % bundles.size()];
    bundle.Init(fname);
    return bundle;
}
ECBand::~ECBand() {
    for (auto ovr : overviews)
        if (ovr)
            delete ovr;
    overviews.clear();
}

ECBand::ECBand(ECDataset* parent, int b, int level) : lvl(level), ci(GCI_Undefined) {
    static const GDALColorInterp rgba[4] = { GCI_RedBand, GCI_GreenBand, GCI_BlueBand, GCI_AlphaBand };
    static const GDALColorInterp la[2] = { GCI_GrayIndex, GCI_AlphaBand };
    poDS = parent;
    nBand = b;

    double factor = parent->resolutions[0] / parent->resolutions[lvl];
    nRasterXSize = static_cast<int>(parent->nRasterXSize * factor + 0.5);
    nRasterYSize = static_cast<int>(parent->nRasterYSize * factor + 0.5);
    nBlockXSize = nBlockYSize = 256;

    // Default color interpretation
    assert( b - 1 >= 0 );
    if( parent->nBands >= 3 )
    {
        assert( b - 1 < static_cast<int>(CPL_ARRAYSIZE(rgba)) );
        ci = rgba[b - 1];
    }
    else
    {
        assert( b - 1 < static_cast<int>(CPL_ARRAYSIZE(la)) );
        ci = la[b - 1];
    }
    if (0 == lvl)
        AddOverviews();
}

void ECBand::AddOverviews() {
    auto parent = reinterpret_cast<ECDataset*>(poDS);
    for (size_t i = 1; i < parent->resolutions.size(); i++) {
        ECBand* ovl = new ECBand(parent, nBand, int(i));
        if (!ovl)
            break;
        overviews.push_back(ovl);
    }
}

CPLErr ECBand::IReadBlock(int nBlockXOff, int nBlockYOff, void* pData) {
    auto parent = reinterpret_cast<ECDataset*>(poDS);
    auto& buffer = parent->tilebuffer;
    auto TSZ = parent->TSZ;
    auto BSZ = parent->BSZ;
    size_t nBytes = size_t(TSZ) * TSZ;

    buffer.resize(nBytes * parent->nBands);

    int lxx = static_cast<int>(parent->resolutions.size() - lvl - 1);
    int bx, by;
    bx = (nBlockXOff / BSZ) * BSZ;
    by = (nBlockYOff / BSZ) * BSZ;
    CPLString fname;
    fname = CPLString().Printf("%s/L%02d/R%04xC%04x.bundle", parent->dname.c_str(), lxx, by, bx);
    Bundle& bundle = parent->GetBundle(fname);
    if (nullptr == bundle.fh) { // This is not an error in general, bundles can be missing
        CPLDebug("ESRIC", "Can't open bundle %s", fname.c_str());
        memset(pData, 0, nBytes);
        return CE_None;
    }
    int block = static_cast<int>((nBlockYOff % BSZ) * BSZ + (nBlockXOff % BSZ));
    GUInt64 offset = bundle.index[block] & 0xffffffffffull;
    GUInt64 size = bundle.index[block] >> 40;
    if (0 == size) {
        memset(pData, 0, nBytes);
        return CE_None;
    }
    auto& fbuffer = parent->filebuffer;
    fbuffer.resize(size_t(size));
    VSIFSeekL(bundle.fh, offset, SEEK_SET);
    if (size != VSIFReadL(fbuffer.data(), size_t(1), size_t(size), bundle.fh)) {
        CPLError(CE_Failure, CPLE_FileIO,
            "Error reading tile, reading " CPL_FRMT_GUIB " at " CPL_FRMT_GUIB,
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
    if (!inds) {
        VSIUnlink(magic.c_str());
        CPLError(CE_Failure, CPLE_FileIO, "Error opening tile");
        return CE_Failure;
    }
    // Duplicate first band if not sufficient bands are provided
    auto inbands = GDALGetRasterCount(inds);
    int ubands[4] = { 1, 1, 1, 1 };
    int* usebands = nullptr;
    int bandcount = parent->nBands;
    if (inbands != bandcount) {
        // Opaque if output expects alpha channel
        if (0 == bandcount % 2) {
            fill(buffer.begin(), buffer.end(), GByte(255));
            bandcount--;
        }
        if (3 == inbands) {
            // Lacking opacity, copy the first three bands
            ubands[1] = 2;
            ubands[2] = 3;
            usebands = ubands;
        }
        else if (1 == inbands) {
            // Grayscale, expecting color
            usebands = ubands;
        }
    }

    auto errcode =
        GDALDatasetRasterIO(inds, GF_Read, 0, 0, TSZ, TSZ, buffer.data(), TSZ, TSZ, GDT_Byte,
            bandcount, usebands, parent->nBands, parent->nBands * TSZ, 1);
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
        GDALRasterBlock* poBlock = nullptr;
        if (band != this) {
            poBlock = band->GetLockedBlockRef(nBlockXOff, nBlockYOff, 1);
            if (poBlock != nullptr) {
                GDALCopyWords(buffer.data() + iBand - 1, GDT_Byte, parent->nBands,
                    poBlock->GetDataRef(), GDT_Byte, 1, TSZ * TSZ);
                poBlock->DropLock();
            }
        }
        else {
            GDALCopyWords(buffer.data() + iBand - 1, GDT_Byte, parent->nBands,
                pData, GDT_Byte, 1, TSZ * TSZ);
        }
    }

    return CE_None;
} // IReadBlock

} // namespace

void CPL_DLL GDALRegister_ESRIC() {
    if (GDALGetDriverByName("ESRIC") != nullptr)
        return;

    auto poDriver = new GDALDriver;

    poDriver->SetDescription("ESRIC");
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_VECTOR, "NO");
    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "Esri Compact Cache");

    poDriver->pfnIdentify = ESRIC::Identify;
    poDriver->pfnOpen = ESRIC::ECDataset::Open;
    poDriver->pfnDelete = ESRIC::Delete;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
