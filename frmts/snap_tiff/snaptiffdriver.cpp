// SPDX-License-Identifier: MIT
// Copyright 2024, Even Rouault <even.rouault at spatialys.com>

#include "cpl_port.h"
#include "cpl_minixml.h"
#include "cpl_vsi_virtual.h"
#include "gdal_pam.h"
#include "rawdataset.h"

#define LIBERTIFF_NS GDAL_libertiff
#include "../../third_party/libertiff/libertiff.hpp"

#include <algorithm>
#include <cmath>

constexpr const char *SNAP_TIFF_PREFIX = "SNAP_TIFF:";

// Non-standard TIFF tag holding DIMAP XML for SNAP TIFF products
constexpr uint16_t DIMAP_TAG = 65000;

// Number of values per GCP in the GeoTIFFTiePoints tag
constexpr int VALUES_PER_GCP = 6;

constexpr int GCP_PIXEL = 0;
constexpr int GCP_LINE = 1;
// constexpr int GCP_DEPTH = 2;
constexpr int GCP_X = 3;
constexpr int GCP_Y = 4;
constexpr int GCP_Z = 5;

/************************************************************************/
/*                            SNAPTIFFDataset                           */
/************************************************************************/

class SNAPTIFFDataset final : public GDALPamDataset
{
  public:
    SNAPTIFFDataset() = default;

    static int Identify(GDALOpenInfo *poOpenInfo);
    static GDALDataset *Open(GDALOpenInfo *poOpenInfo);

    char **GetMetadataDomainList() override;
    char **GetMetadata(const char *pszDomain = "") override;
    const char *GetMetadataItem(const char *pszName,
                                const char *pszDomain = "") override;

    const OGRSpatialReference *GetGCPSpatialRef() const override
    {
        return m_oSRS.IsEmpty() ? nullptr : &m_oSRS;
    }

    int GetGCPCount() override
    {
        return int(m_asGCPs.size());
    }

    const GDAL_GCP *GetGCPs() override
    {
        return gdal::GCP::c_ptr(m_asGCPs);
    }

  private:
    VSIVirtualHandleUniquePtr m_poFile{};
    std::unique_ptr<const LIBERTIFF_NS::Image> m_poImage{};

    //! Whether this dataset is actually the geolocation array
    bool m_bIsGeolocArray = false;

    //! Content of "xml:DIMAP" metadata domain
    CPLStringList m_aosDIMAPMetadata{};

    CPLStringList m_aosGEOLOCATION{};
    int m_nGeolocArrayWidth = 0;
    int m_nGeolocArrayHeight = 0;

    CPLStringList m_aosSUBDATASETS{};

    std::vector<gdal::GCP> m_asGCPs{};
    OGRSpatialReference m_oSRS{};

    bool GetGeolocationMetadata();

    void ReadSRS();
};

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/

int SNAPTIFFDataset::Identify(GDALOpenInfo *poOpenInfo)
{
    if (STARTS_WITH(poOpenInfo->pszFilename, SNAP_TIFF_PREFIX))
        return true;

    if (poOpenInfo->fpL == nullptr || poOpenInfo->nHeaderBytes < 16 ||
        // BigEndian classic TIFF
        memcmp(poOpenInfo->pabyHeader, "\x4D\x4D\x00\x2A", 4) != 0)
    {
        return false;
    }

    struct MyFileReader final : public LIBERTIFF_NS::FileReader
    {
        const GDALOpenInfo *m_poOpenInfo;

        explicit MyFileReader(const GDALOpenInfo *poOpenInfoIn)
            : m_poOpenInfo(poOpenInfoIn)
        {
        }

        uint64_t size() const override
        {
            return m_poOpenInfo->nHeaderBytes;
        }

        size_t read(uint64_t offset, size_t count, void *buffer) const override
        {
            if (offset + count >
                static_cast<uint64_t>(m_poOpenInfo->nHeaderBytes))
                return 0;
            memcpy(buffer,
                   m_poOpenInfo->pabyHeader + static_cast<size_t>(offset),
                   count);
            return count;
        }

        CPL_DISALLOW_COPY_ASSIGN(MyFileReader)
    };

    auto f = std::make_shared<const MyFileReader>(poOpenInfo);
#ifdef DEBUG
    // Just to increase coverage testing
    CPLAssert(f->size() == uint64_t(poOpenInfo->nHeaderBytes));
    char dummy;
    CPLAssert(f->read(poOpenInfo->nHeaderBytes, 1, &dummy) == 0);
#endif
    auto image = LIBERTIFF_NS::open</*acceptBigTIFF = */ false>(std::move(f));
    // Checks that it is a single-band Float32 uncompressed dataset, made
    // of a single strip.
    if (!image || image->nextImageOffset() != 0 ||
        image->compression() != LIBERTIFF_NS::Compression::None ||
        image->sampleFormat() != LIBERTIFF_NS::SampleFormat::IEEEFP ||
        image->samplesPerPixel() != 1 || image->bitsPerSample() != 32 ||
        image->isTiled() || image->strileCount() != 1 || image->width() == 0 ||
        image->width() > INT_MAX / sizeof(float) || image->height() == 0 ||
        image->height() > INT_MAX || image->rowsPerStrip() != image->height() ||
        !image->tag(LIBERTIFF_NS::TagCode::GeoTIFFPixelScale) ||
        !image->tag(LIBERTIFF_NS::TagCode::GeoTIFFTiePoints) ||
        !image->tag(LIBERTIFF_NS::TagCode::GeoTIFFGeoKeyDirectory) ||
        !image->tag(DIMAP_TAG))
    {
        return false;
    }

    return true;
}

/************************************************************************/
/*                               Open()                                 */
/************************************************************************/

GDALDataset *SNAPTIFFDataset::Open(GDALOpenInfo *poOpenInfo)
{
    if (poOpenInfo->eAccess == GA_Update || !Identify(poOpenInfo))
        return nullptr;

    bool bIsGeolocation = false;
    // Check if it is SNAP_TIFF:"filename":{subdataset_component} syntax
    if (STARTS_WITH(poOpenInfo->pszFilename, SNAP_TIFF_PREFIX))
    {
        const CPLStringList aosTokens(CSLTokenizeString2(
            poOpenInfo->pszFilename, ":", CSLT_HONOURSTRINGS));
        if (aosTokens.size() != 3)
            return nullptr;
        bIsGeolocation = EQUAL(aosTokens[2], "GEOLOCATION");
        if (!bIsGeolocation && !EQUAL(aosTokens[2], "MAIN"))
            return nullptr;
        GDALOpenInfo oOpenInfo(aosTokens[1], GA_ReadOnly);
        if (!Identify(&oOpenInfo))
            return nullptr;
        std::swap(poOpenInfo->fpL, oOpenInfo.fpL);
        if (!poOpenInfo->fpL)
            return nullptr;
    }

    struct MyFileReader final : public LIBERTIFF_NS::FileReader
    {
        VSILFILE *m_fp;
        mutable uint64_t m_nFileSize = 0;

        explicit MyFileReader(VSILFILE *fp) : m_fp(fp)
        {
        }

        uint64_t size() const override
        {
            if (m_nFileSize == 0)
            {
                m_fp->Seek(0, SEEK_END);
                m_nFileSize = m_fp->Tell();
            }
            return m_nFileSize;
        }

        size_t read(uint64_t offset, size_t count, void *buffer) const override
        {
            return m_fp->Seek(offset, SEEK_SET) == 0
                       ? m_fp->Read(buffer, 1, count)
                       : 0;
        }

        CPL_DISALLOW_COPY_ASSIGN(MyFileReader)
    };

    auto f = std::make_shared<const MyFileReader>(poOpenInfo->fpL);
#ifdef DEBUG
    // Just to increase coverage testing
    char dummy;
    CPLAssert(f->read(f->size(), 1, &dummy) == 0);
#endif

    auto poDS = std::make_unique<SNAPTIFFDataset>();

    poDS->m_poImage = LIBERTIFF_NS::open(std::move(f));
    if (!poDS->m_poImage)
        return nullptr;
    poDS->m_poFile.reset(poOpenInfo->fpL);
    poOpenInfo->fpL = nullptr;

    poDS->nRasterXSize = static_cast<int>(poDS->m_poImage->width());
    poDS->nRasterYSize = static_cast<int>(poDS->m_poImage->height());
    poDS->SetDescription(poOpenInfo->pszFilename);

    if (bIsGeolocation)
    {
        poDS->m_bIsGeolocArray = true;
        if (!poDS->GetGeolocationMetadata())
        {
            return nullptr;
        }
        poDS->nRasterXSize = poDS->m_nGeolocArrayWidth;
        poDS->nRasterYSize = poDS->m_nGeolocArrayHeight;

        const auto *psTag =
            poDS->m_poImage->tag(LIBERTIFF_NS::TagCode::GeoTIFFTiePoints);

        for (int iBand = 1; iBand <= 2; ++iBand)
        {
            auto poBand = std::make_unique<RawRasterBand>(
                poDS->m_poFile.get(),
                psTag->value_offset +
                    (iBand == 1 ? GCP_X : GCP_Y) * sizeof(double),
                int(sizeof(double)) * VALUES_PER_GCP,
                int(sizeof(double)) * VALUES_PER_GCP * poDS->nRasterXSize,
                GDT_Float64, !poDS->m_poImage->mustByteSwap(),
                poDS->nRasterXSize, poDS->nRasterYSize,
                RawRasterBand::OwnFP::NO);
            if (!poBand->IsValid())
                return nullptr;
            if (iBand == 1)
                poBand->SetDescription("Longitude");
            else
                poBand->SetDescription("Latitude");
            poDS->SetBand(iBand, std::move(poBand));
        }

        return poDS.release();
    }

    poDS->ReadSRS();
    poDS->GetGeolocationMetadata();

    {
        bool okStrileOffset = false;
        auto poBand = std::make_unique<RawRasterBand>(
            poDS->m_poFile.get(),
            poDS->m_poImage->strileOffset(0, okStrileOffset),
            int(sizeof(float)), int(sizeof(float)) * poDS->nRasterXSize,
            GDT_Float32, !poDS->m_poImage->mustByteSwap(), poDS->nRasterXSize,
            poDS->nRasterYSize, RawRasterBand::OwnFP::NO);
        if (!poBand->IsValid())
            return nullptr;
        poDS->SetBand(1, std::move(poBand));
    }
    auto poBand = poDS->papoBands[0];

    const auto &psImageDescription =
        poDS->m_poImage->tag(LIBERTIFF_NS::TagCode::ImageDescription);
    if (psImageDescription &&
        psImageDescription->type == LIBERTIFF_NS::TagType::ASCII &&
        !psImageDescription->invalid_value_offset &&
        // Sanity check
        psImageDescription->count < 100 * 1000)
    {
        bool ok = true;
        const std::string s =
            poDS->m_poImage->readTagAsString(*psImageDescription, ok);
        if (ok)
        {
            poDS->GDALDataset::SetMetadataItem("IMAGE_DESCRIPTION", s.c_str());
        }
    }

    const auto *psDimapTag = poDS->m_poImage->tag(DIMAP_TAG);
    if (psDimapTag && psDimapTag->type == LIBERTIFF_NS::TagType::ASCII &&
        !psDimapTag->invalid_value_offset)
    {
        try
        {
            bool ok = true;
            // Just read the first 10 kB max to fetch essential band metadata
            const std::string s = poDS->m_poImage->readContext()->readString(
                psDimapTag->value_offset,
                static_cast<size_t>(
                    std::min<uint64_t>(psDimapTag->count, 10000)),
                ok);
            const auto posStart = s.find("<Spectral_Band_Info>");
            if (posStart != std::string::npos)
            {
                const char *markerEnd = "</Spectral_Band_Info>";
                const auto posEnd = s.find(markerEnd, posStart);
                if (posEnd != std::string::npos)
                {
                    const std::string osSubstr = s.substr(
                        posStart, posEnd - posStart + strlen(markerEnd));
                    CPLXMLTreeCloser oRoot(CPLParseXMLString(osSubstr.c_str()));
                    if (oRoot)
                    {
                        const char *pszNoDataValueUsed = CPLGetXMLValue(
                            oRoot.get(), "NO_DATA_VALUE_USED", nullptr);
                        const char *pszNoDataValue = CPLGetXMLValue(
                            oRoot.get(), "NO_DATA_VALUE", nullptr);
                        if (pszNoDataValueUsed && pszNoDataValue &&
                            CPLTestBool(pszNoDataValueUsed))
                        {
                            poBand->SetNoDataValue(CPLAtof(pszNoDataValue));
                        }

                        if (const char *pszScalingFactor = CPLGetXMLValue(
                                oRoot.get(), "SCALING_FACTOR", nullptr))
                        {
                            poBand->SetScale(CPLAtof(pszScalingFactor));
                        }

                        if (const char *pszScalingOffset = CPLGetXMLValue(
                                oRoot.get(), "SCALING_OFFSET", nullptr))
                        {
                            poBand->SetOffset(CPLAtof(pszScalingOffset));
                        }

                        if (const char *pszBandName = CPLGetXMLValue(
                                oRoot.get(), "BAND_NAME", nullptr))
                        {
                            poBand->SetDescription(pszBandName);
                        }

                        if (const char *pszUnit = CPLGetXMLValue(
                                oRoot.get(), "PHYSICAL_UNIT", nullptr))
                        {
                            poBand->SetUnitType(pszUnit);
                        }
                    }
                }
            }
        }
        catch (const std::exception &)
        {
        }
    }

    // Initialize PAM
    poDS->SetDescription(poOpenInfo->pszFilename);
    poDS->TryLoadXML();

    // Check for overviews.
    poDS->oOvManager.Initialize(poDS.get(), poOpenInfo->pszFilename);

    return poDS.release();
}

/************************************************************************/
/*                      GetMetadataDomainList()                         */
/************************************************************************/

char **SNAPTIFFDataset::GetMetadataDomainList()
{
    return BuildMetadataDomainList(GDALPamDataset::GetMetadataDomainList(),
                                   TRUE, "GEOLOCATION", "SUBDATASETS",
                                   "xml:DIMAP", nullptr);
}

/************************************************************************/
/*                      GetGeolocationMetadata()                        */
/************************************************************************/

// (Partially) read the content of the GeoTIFFTiePoints tag to check if the
// tie points form a regular geolocation array, and extract the width, height,
// and spacing of that geolocation array. Also fills the m_aosGEOLOCATION
// metadata domain
bool SNAPTIFFDataset::GetGeolocationMetadata()
{
    const auto *psTag = m_poImage->tag(LIBERTIFF_NS::TagCode::GeoTIFFTiePoints);
    if (psTag && psTag->type == LIBERTIFF_NS::TagType::Double &&
        !psTag->invalid_value_offset && (psTag->count % VALUES_PER_GCP) == 0 &&
        // Sanity check
        psTag->count <= uint64_t(nRasterXSize) * nRasterYSize * VALUES_PER_GCP)
    {
        const int numGCPs = int(psTag->count / VALUES_PER_GCP);

        // Assume that the geolocation array has the same proportion as the
        // main array
        const double dfGCPArrayWidth =
            sqrt(double(nRasterXSize) * numGCPs / nRasterYSize);
        const double dfGCPArrayHeight =
            sqrt(double(nRasterYSize) * numGCPs / nRasterXSize);
        if (dfGCPArrayWidth > INT_MAX || dfGCPArrayHeight > INT_MAX)
        {
            return false;
        }

        const int nGCPArrayWidth = int(std::round(dfGCPArrayWidth));
        const int nGCPArrayHeight = int(std::round(dfGCPArrayHeight));
        constexpr int NUM_LINES = 3;
        if (nGCPArrayWidth * nGCPArrayHeight != numGCPs ||
            nGCPArrayHeight < NUM_LINES)
        {
            return false;
        }

        bool ok = true;
        // Just read the first 3 lines of the geolocation array
        const auto numValuesPerLine = nGCPArrayWidth * VALUES_PER_GCP;
        const auto values = m_poImage->readContext()->readArray<double>(
            psTag->value_offset, numValuesPerLine * NUM_LINES, ok);
        if (!ok || values.empty())
            return false;

        if (values[GCP_LINE] != 0.5 && values[GCP_PIXEL] != 0.5)
            return false;

        constexpr double RELATIVE_TOLERANCE = 1e-5;
        constexpr double PIXEL_TOLERANCE = 1e-3;

        // Check that pixel spacing is constant on all three first lines
        const double pixelSpacing =
            values[VALUES_PER_GCP + GCP_PIXEL] - values[GCP_PIXEL];
        if (!(pixelSpacing >= 1))
            return false;
        if (std::fabs(pixelSpacing * (nGCPArrayWidth - 1) -
                      (nRasterXSize - 1)) > PIXEL_TOLERANCE)
            return false;

        double adfY[NUM_LINES];
        for (int iLine = 0; iLine < NUM_LINES; ++iLine)
        {
            adfY[iLine] = values[iLine * numValuesPerLine + GCP_LINE];
            for (int i = iLine * numValuesPerLine + VALUES_PER_GCP;
                 i < (iLine + 1) * numValuesPerLine; i += VALUES_PER_GCP)
            {
                if (values[i + GCP_LINE] !=
                    values[i - VALUES_PER_GCP + GCP_LINE])
                {
                    return false;
                }
                const double pixelSpacingNew =
                    values[i + GCP_PIXEL] -
                    values[i - VALUES_PER_GCP + GCP_PIXEL];
                if (std::fabs(pixelSpacingNew - pixelSpacing) >
                    RELATIVE_TOLERANCE * std::fabs(pixelSpacing))
                {
                    return false;
                }
            }
        }

        // Check that line spacing is constant on the three first lines
        const double lineSpacing = adfY[1] - adfY[0];
        if (!(lineSpacing >= 1))
            return false;
        if (std::fabs(lineSpacing * (nGCPArrayHeight - 1) -
                      (nRasterYSize - 1)) > PIXEL_TOLERANCE)
            return false;

        for (int iLine = 1; iLine + 1 < NUM_LINES; ++iLine)
        {
            const double lineSpacingNew = adfY[iLine + 1] - adfY[iLine];
            if (std::fabs(lineSpacingNew - lineSpacing) >
                RELATIVE_TOLERANCE * std::fabs(lineSpacing))
            {
                return false;
            }
        }

        // Read last line
        const auto lastLineValue = m_poImage->readContext()->readArray<double>(
            psTag->value_offset + uint64_t(nGCPArrayHeight - 1) *
                                      numValuesPerLine * sizeof(double),
            numValuesPerLine, ok);
        if (!ok)
            return false;

        if (!m_bIsGeolocArray)
        {
            // Expose the 4 corner GCPs for rough georeferencing.

            const uint32_t nShift = numValuesPerLine - VALUES_PER_GCP;
            m_asGCPs.emplace_back("TL", "Top Left", values[GCP_PIXEL],
                                  values[GCP_LINE], values[GCP_X],
                                  values[GCP_Y], values[GCP_Z]);
            m_asGCPs.emplace_back(
                "TR", "Top Right", values[nShift + GCP_PIXEL],
                values[nShift + GCP_LINE], values[nShift + GCP_X],
                values[nShift + GCP_Y], values[nShift + GCP_Z]);
            m_asGCPs.emplace_back("BL", "Bottom Left", lastLineValue[GCP_PIXEL],
                                  lastLineValue[GCP_LINE], lastLineValue[GCP_X],
                                  lastLineValue[GCP_Y], lastLineValue[GCP_Z]);
            m_asGCPs.emplace_back(
                "BR", "Bottom Right", lastLineValue[nShift + GCP_PIXEL],
                lastLineValue[nShift + GCP_LINE], lastLineValue[nShift + GCP_X],
                lastLineValue[nShift + GCP_Y], lastLineValue[nShift + GCP_Z]);
        }

        m_nGeolocArrayWidth = nGCPArrayWidth;
        m_nGeolocArrayHeight = nGCPArrayHeight;

        if (!m_bIsGeolocArray)
        {
            if (!m_oSRS.IsEmpty())
            {
                char *pszWKT = nullptr;
                m_oSRS.exportToWkt(&pszWKT);
                m_aosGEOLOCATION.SetNameValue("SRS", pszWKT);
                CPLFree(pszWKT);
            }

            m_aosGEOLOCATION.SetNameValue(
                "X_DATASET", CPLSPrintf("%s\"%s\":GEOLOCATION",
                                        SNAP_TIFF_PREFIX, GetDescription()));
            m_aosGEOLOCATION.SetNameValue("X_BAND", "1");
            m_aosGEOLOCATION.SetNameValue(
                "Y_DATASET", CPLSPrintf("%s\"%s\":GEOLOCATION",
                                        SNAP_TIFF_PREFIX, GetDescription()));
            m_aosGEOLOCATION.SetNameValue("Y_BAND", "2");
            m_aosGEOLOCATION.SetNameValue("PIXEL_OFFSET", "0");
            m_aosGEOLOCATION.SetNameValue("PIXEL_STEP",
                                          CPLSPrintf("%.17g", pixelSpacing));
            m_aosGEOLOCATION.SetNameValue("LINE_OFFSET", "0");
            m_aosGEOLOCATION.SetNameValue("LINE_STEP",
                                          CPLSPrintf("%.17g", lineSpacing));
        }

        return true;
    }
    return false;
}

/************************************************************************/
/*                             ReadSRS()                                */
/************************************************************************/

// Simplified GeoTIFF SRS reader, assuming the SRS is encoded as a EPSG code
void SNAPTIFFDataset::ReadSRS()
{
    const auto &psGeoKeysTag =
        m_poImage->tag(LIBERTIFF_NS::TagCode::GeoTIFFGeoKeyDirectory);
    constexpr int VALUES_PER_GEOKEY = 4;
    if (psGeoKeysTag && psGeoKeysTag->type == LIBERTIFF_NS::TagType::Short &&
        !psGeoKeysTag->invalid_value_offset &&
        psGeoKeysTag->count >= VALUES_PER_GEOKEY &&
        (psGeoKeysTag->count % VALUES_PER_GEOKEY) == 0 &&
        // Sanity check
        psGeoKeysTag->count < 1000)
    {
        bool ok = true;
        const auto values =
            m_poImage->readTagAsVector<uint16_t>(*psGeoKeysTag, ok);
        if (values.size() >= 4)
        {
            const uint16_t geokeysCount = values[3];
            constexpr uint16_t GEOTIFF_KEY_DIRECTORY_VERSION_V1 = 1;
            constexpr uint16_t GEOTIFF_KEY_VERSION_MAJOR_V1 = 1;
            if (values[0] == GEOTIFF_KEY_DIRECTORY_VERSION_V1 &&
                // GeoTIFF 1.x
                values[1] == GEOTIFF_KEY_VERSION_MAJOR_V1 &&
                geokeysCount == psGeoKeysTag->count / VALUES_PER_GEOKEY - 1)
            {
                constexpr uint16_t GeoTIFFTypeShort = 0;
                constexpr uint16_t GeodeticCRSGeoKey = 2048;
                constexpr uint16_t ProjectedCRSGeoKey = 3072;
                uint16_t nEPSGCode = 0;
                for (uint32_t i = 1; i <= geokeysCount; ++i)
                {
                    const auto geokey = values[VALUES_PER_GEOKEY * i];
                    const auto geokeyType = values[VALUES_PER_GEOKEY * i + 1];
                    const auto geokeyCount = values[VALUES_PER_GEOKEY * i + 2];
                    const auto geokeyValue = values[VALUES_PER_GEOKEY * i + 3];
                    if ((geokey == GeodeticCRSGeoKey ||
                         geokey == ProjectedCRSGeoKey) &&
                        geokeyType == GeoTIFFTypeShort && geokeyCount == 1 &&
                        geokeyValue > 0)
                    {
                        nEPSGCode = geokeyValue;
                        if (geokey == ProjectedCRSGeoKey)
                            break;
                    }
                }
                if (nEPSGCode > 0)
                {
                    m_oSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
                    m_oSRS.importFromEPSG(nEPSGCode);
                }
            }
        }
    }
}

/************************************************************************/
/*                            GetMetadata()                             */
/************************************************************************/

char **SNAPTIFFDataset::GetMetadata(const char *pszDomain)
{
    if (!m_bIsGeolocArray)
    {
        if (pszDomain && EQUAL(pszDomain, "xml:DIMAP"))
        {
            if (m_aosDIMAPMetadata.empty())
            {
                const auto *psDimapTag = m_poImage->tag(DIMAP_TAG);
                if (psDimapTag &&
                    psDimapTag->type == LIBERTIFF_NS::TagType::ASCII &&
                    !psDimapTag->invalid_value_offset &&
                    // Sanity check
                    psDimapTag->count < 100 * 1000 * 1000)
                {
                    try
                    {
                        bool ok = true;
                        const std::string s =
                            m_poImage->readTagAsString(*psDimapTag, ok);
                        if (ok)
                        {
                            m_aosDIMAPMetadata.AddString(s.c_str());
                        }
                    }
                    catch (const std::exception &)
                    {
                        CPLError(CE_Failure, CPLE_OutOfMemory,
                                 "Out of memory in GetMetadata()");
                    }
                }
            }
            return m_aosDIMAPMetadata.List();
        }
        else if (pszDomain && EQUAL(pszDomain, "GEOLOCATION"))
        {
            return m_aosGEOLOCATION.List();
        }
        else if (pszDomain && EQUAL(pszDomain, "SUBDATASETS"))
        {
            if (m_aosSUBDATASETS.empty() && GetGeolocationMetadata())
            {
                m_aosSUBDATASETS.SetNameValue("SUBDATASET_1_NAME",
                                              CPLSPrintf("%s\"%s\":MAIN",
                                                         SNAP_TIFF_PREFIX,
                                                         GetDescription()));
                m_aosSUBDATASETS.SetNameValue("SUBDATASET_1_DESC",
                                              std::string("Main content of ")
                                                  .append(GetDescription())
                                                  .c_str());

                m_aosSUBDATASETS.SetNameValue(
                    "SUBDATASET_2_NAME",
                    m_aosGEOLOCATION.FetchNameValue("X_DATASET"));
                m_aosSUBDATASETS.SetNameValue(
                    "SUBDATASET_2_DESC", std::string("Geolocation array of ")
                                             .append(GetDescription())
                                             .c_str());
            }
            return m_aosSUBDATASETS.List();
        }
    }

    return GDALPamDataset::GetMetadata(pszDomain);
}

/************************************************************************/
/*                         GetMetadataItem()                            */
/************************************************************************/

const char *SNAPTIFFDataset::GetMetadataItem(const char *pszName,
                                             const char *pszDomain)
{
    if (!m_bIsGeolocArray && pszDomain &&
        (EQUAL(pszDomain, "GEOLOCATION") || EQUAL(pszDomain, "SUBDATASETS")))
    {
        return CSLFetchNameValue(GetMetadata(pszDomain), pszName);
    }
    return GDALPamDataset::GetMetadataItem(pszName, pszDomain);
}

/************************************************************************/
/*                     GDALRegister_SNAP_TIFF()                         */
/************************************************************************/

void GDALRegister_SNAP_TIFF()
{
    if (GDALGetDriverByName("SNAP_TIFF") != nullptr)
        return;

    GDALDriver *poDriver = new GDALDriver();
    poDriver->SetDescription("SNAP_TIFF");
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME,
                              "Sentinel Application Processing GeoTIFF");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC,
                              "drivers/raster/snap_tiff.html");
    // Declaring the tif extension confuses QGIS
    // Cf https://github.com/qgis/QGIS/issues/59112
    // This driver is of too marginal usage to justify causing chaos downstream.
    // poDriver->SetMetadataItem(GDAL_DMD_MIMETYPE, "image/tiff");
    // poDriver->SetMetadataItem(GDAL_DMD_EXTENSIONS, "tif tiff");
    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");

    poDriver->pfnOpen = SNAPTIFFDataset::Open;
    poDriver->pfnIdentify = SNAPTIFFDataset::Identify;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
