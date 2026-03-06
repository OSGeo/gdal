/******************************************************************************
 *
 * Project:  NITF Read/Write Library
 * Purpose:  Creates RPFHDR, RPFIMG, RPFDES TREs and RPF image data
 * Author:   Even Rouault, even dot rouault at spatialys dot com
 *
 **********************************************************************
 * Copyright (c) 2026, T-Kartor
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "rpfframewriter.h"

#include "cpl_enumerate.h"
#include "cpl_string.h"
#include "cpl_time.h"
#include "cpl_worker_thread_pool.h"
#include "gdal_alg.h"
#include "gdal_alg_priv.h"
#include "gdal_colortable.h"
#include "gdal_dataset.h"
#include "gdal_geotransform.h"
#include "gdal_rasterband.h"
#include "gdal_thread_pool.h"
#include "gdal_utils.h"
#include "ogr_spatialref.h"
#include "offsetpatcher.h"
#include "nitfdataset.h"
#include "nitflib.h"
#include "kdtree_vq_cadrg.h"
#include "rpftocwriter.h"

#include <algorithm>
#include <array>
#include <map>
#include <mutex>
#include <utility>
#include <vector>

// Refer to following documents to (hopefully) understand this file:
// MIL-C-89038, CADRG:          http://everyspec.com/MIL-PRF/MIL-PRF-080000-99999/MIL-PRF-89038_25371/
// MIL-STD-2411, RPF:           http://everyspec.com/MIL-STD/MIL-STD-2000-2999/MIL-STD-2411_6903/
// MIL-STD-2411-1, RPF details: http://everyspec.com/MIL-STD/MIL-STD-2000-2999/MIL-STD-2411-1_6909/
// MIL-STD-2411-2, RPF-in-NITF: http://everyspec.com/MIL-STD/MIL-STD-2000-2999/MIL-STD-2411-2_6908/
// MIL-A-89007, ADRG:           http://everyspec.com/MIL-SPECS/MIL-SPECS-MIL-A/MIL-A-89007_51725/
// MIL-STD-188/199, VQ NITF:    http://everyspec.com/MIL-STD/MIL-STD-0100-0299/MIL_STD_188_199_1730/

constexpr int SUBSAMPLING = 4;
constexpr int BLOCK_SIZE = 256;
constexpr int CODEBOOK_MAX_SIZE = 4096;
constexpr int TRANSPARENT_CODEBOOK_CODE = CODEBOOK_MAX_SIZE - 1;
constexpr int TRANSPARENT_COLOR_TABLE_ENTRY = CADRG_MAX_COLOR_ENTRY_COUNT;

constexpr int CADRG_SECOND_CT_COUNT = 32;
constexpr int CADRG_THIRD_CT_COUNT = 16;

constexpr double CADRG_PITCH_IN_CM = 0.0150;  // 150 micrometers

constexpr int DEFAULT_DENSIFY_PTS = 21;

/************************************************************************/
/*                        RPFCADRGIsValidZone()                         */
/************************************************************************/

static bool RPFCADRGIsValidZone(int nZone)
{
    return nZone >= MIN_ZONE && nZone <= MAX_ZONE;
}

/************************************************************************/
/*                       RPFCADRGZoneNumToChar()                        */
/************************************************************************/

char RPFCADRGZoneNumToChar(int nZone)
{
    CPLAssert(RPFCADRGIsValidZone(nZone));
    if (nZone <= MAX_ZONE_NORTHERN_HEMISPHERE)
        return static_cast<char>('0' + nZone);
    else if (nZone < MAX_ZONE)
        return static_cast<char>('A' + (nZone - MIN_ZONE_SOUTHERN_HEMISPHERE));
    else
        return 'J';
}

/************************************************************************/
/*                       RPFCADRGZoneCharToNum()                        */
/************************************************************************/

int RPFCADRGZoneCharToNum(char chZone)
{
    if (chZone >= '1' && chZone <= '9')
        return chZone - '0';
    else if (chZone >= 'A' && chZone <= 'H')
        return (chZone - 'A') + MIN_ZONE_SOUTHERN_HEMISPHERE;
    else if (chZone >= 'a' && chZone <= 'h')
        return (chZone - 'a') + MIN_ZONE_SOUTHERN_HEMISPHERE;
    else if (chZone == 'J' || chZone == 'j')
        return MAX_ZONE;
    else
        return 0;
}

/************************************************************************/
/*                 RPFCADRGGetScaleFromDataSeriesCode()                 */
/************************************************************************/

int RPFCADRGGetScaleFromDataSeriesCode(const char *pszCode)
{
    int nVal = 0;
    const auto *psEntry = NITFGetRPFSeriesInfoFromCode(pszCode);
    if (psEntry)
    {
        nVal = NITFGetScaleFromScaleResolution(psEntry->scaleResolution);
    }
    return nVal;
}

/************************************************************************/
/*                   RPFCADRGIsKnownDataSeriesCode()                    */
/************************************************************************/

bool RPFCADRGIsKnownDataSeriesCode(const char *pszCode)
{
    return NITFIsKnownRPFDataSeriesCode(pszCode, "CADRG");
}

/************************************************************************/
/*                      CADRGInformation::Private                       */
/************************************************************************/

class CADRGInformation::Private
{
  public:
    std::vector<BucketItem<ColorTableBased4x4Pixels>> codebook{};
    std::vector<short> VQImage{};
    bool bHasTransparentPixels = false;
};

/************************************************************************/
/*                 CADRGInformation::CADRGInformation()                 */
/************************************************************************/

CADRGInformation::CADRGInformation(std::unique_ptr<Private> priv)
    : m_private(std::move(priv))
{
}

/************************************************************************/
/*                CADRGInformation::~CADRGInformation()                 */
/************************************************************************/

CADRGInformation::~CADRGInformation() = default;

/************************************************************************/
/*               CADRGInformation::HasTransparentPixels()               */
/************************************************************************/

bool CADRGInformation::HasTransparentPixels() const
{
    return m_private->bHasTransparentPixels;
}

/************************************************************************/
/*                           StrPadTruncate()                           */
/************************************************************************/

#ifndef StrPadTruncate_defined
#define StrPadTruncate_defined

static std::string StrPadTruncate(const std::string &osIn, size_t nSize)
{
    std::string osOut(osIn);
    osOut.resize(nSize, ' ');
    return osOut;
}
#endif

/************************************************************************/
/*                        Create_CADRG_RPFHDR()                         */
/************************************************************************/

void Create_CADRG_RPFHDR(GDALOffsetPatcher::OffsetPatcher *offsetPatcher,
                         const std::string &osFilename,
                         CPLStringList &aosOptions)
{
    auto poRPFHDR = offsetPatcher->CreateBuffer(
        "RPFHDR", /* bEndiannessIsLittle = */ false);
    CPLAssert(poRPFHDR);
#ifdef INCLUDE_HEADER_AND_LOCATION
    poRPFHDR->DeclareOffsetAtCurrentPosition("HEADER_COMPONENT_LOCATION");
#endif
    poRPFHDR->AppendByte(0);  // big endian order
    poRPFHDR->AppendUInt16RefForSizeOfBuffer("RPFHDR");
    poRPFHDR->AppendString(
        StrPadTruncate(CPLGetFilename(osFilename.c_str()), 12));
    poRPFHDR->AppendByte(0);  // update indicator: initial release
    poRPFHDR->AppendString("MIL-C-89038    ");  // GOVERNING_STANDARD_NUMBER
    poRPFHDR->AppendString("19941006");         // GOVERNING_STANDARD_DATE
    // SECURITY_CLASSIFICATION
    poRPFHDR->AppendString(
        StrPadTruncate(aosOptions.FetchNameValueDef("FCLASS", "U"), 1));
    poRPFHDR->AppendString(StrPadTruncate(
        aosOptions.FetchNameValueDef("SECURITY_COUNTRY_CODE", "  "), 2));
    poRPFHDR->AppendString("  ");  // SECURITY_RELEASE_MARKING
    poRPFHDR->AppendUInt32RefForOffset("LOCATION_COMPONENT_LOCATION");

    char *pszEscaped = CPLEscapeString(
        reinterpret_cast<const char *>(poRPFHDR->GetBuffer().data()),
        static_cast<int>(poRPFHDR->GetBuffer().size()),
        CPLES_BackslashQuotable);
    aosOptions.AddString(
        std::string("FILE_TRE=RPFHDR=").append(pszEscaped).c_str());
    CPLFree(pszEscaped);
}

/************************************************************************/
/*                   Create_CADRG_LocationComponent()                   */
/************************************************************************/

static void
Create_CADRG_LocationComponent(GDALOffsetPatcher::OffsetPatcher *offsetPatcher)
{
    auto poBuffer = offsetPatcher->CreateBuffer(
        "LocationComponent", /* bEndiannessIsLittle = */ false);
    CPLAssert(poBuffer);
    poBuffer->DeclareOffsetAtCurrentPosition("LOCATION_COMPONENT_LOCATION");

    static const struct
    {
        uint16_t locationId;
        const char *locationBufferName;
        const char *locationOffsetName;
    } asLocations[] = {
#ifdef INCLUDE_HEADER_AND_LOCATION
        // While it shouldn't hurt, it doesn't seem idiomatical to include
        // those locations in the location table.
        {LID_HeaderComponent /* 128 */, "RPFHDR", "HEADER_COMPONENT_LOCATION"},
        {LID_LocationComponent /* 129 */, "LocationComponent",
         "LOCATION_COMPONENT_LOCATION"},
#endif
        {LID_CoverageSectionSubheader /* 130 */, "CoverageSectionSubheader",
         "COVERAGE_SECTION_LOCATION"},
        {LID_CompressionSectionSubsection /* 131 */, "CompressionSection",
         "COMPRESSION_SECTION_LOCATION"},
        {LID_CompressionLookupSubsection /* 132 */,
         "CompressionLookupSubsection", "COMPRESSION_LOOKUP_LOCATION"},
        /* no LID_CompressionParameterSubsection = 133 in CADRG */
        {LID_ColorGrayscaleSectionSubheader /* 134 */,
         "ColorGrayscaleSectionSubheader", "COLOR_GRAYSCALE_LOCATION"},
        {LID_ColormapSubsection /* 135 */, "ColormapSubsection",
         "COLORMAP_LOCATION"},
        {LID_ImageDescriptionSubheader /* 136 */, "ImageDescriptionSubheader",
         "IMAGE_DESCRIPTION_SECTION_LOCATION"},
        {LID_ImageDisplayParametersSubheader /* 137 */,
         "ImageDisplayParametersSubheader",
         "IMAGE_DISPLAY_PARAMETERS_SECTION_LOCATION"},
        {LID_MaskSubsection /* 138 */, "MaskSubsection",
         "MASK_SUBSECTION_LOCATION"},
        {LID_ColorConverterSubsection /* 139 */, "ColorConverterSubsection",
         "COLOR_CONVERTER_SUBSECTION"},
        {LID_SpatialDataSubsection /* 140 */, "SpatialDataSubsection",
         "SPATIAL_DATA_SUBSECTION_LOCATION"},
        {LID_AttributeSectionSubheader /* 141 */, "AttributeSectionSubheader",
         "ATTRIBUTE_SECTION_SUBHEADER_LOCATION"},
        {LID_AttributeSubsection /* 142 */, "AttributeSubsection",
         "ATTRIBUTE_SUBSECTION_LOCATION"},
    };

    std::string sumOfSizes;
    uint16_t nComponents = 0;
    for (const auto &sLocation : asLocations)
    {
        ++nComponents;
        if (!sumOfSizes.empty())
            sumOfSizes += '+';
        sumOfSizes += sLocation.locationBufferName;
    }

    constexpr uint16_t COMPONENT_LOCATION_OFFSET = 14;
    constexpr uint16_t COMPONENT_LOCATION_RECORD_LENGTH = 10;
    poBuffer->AppendUInt16RefForSizeOfBuffer("LocationComponent");
    poBuffer->AppendUInt32(COMPONENT_LOCATION_OFFSET);
    poBuffer->AppendUInt16(nComponents);
    poBuffer->AppendUInt16(COMPONENT_LOCATION_RECORD_LENGTH);
    // COMPONENT_AGGREGATE_LENGTH
    poBuffer->AppendUInt32RefForSizeOfBuffer(sumOfSizes);

    for (const auto &sLocation : asLocations)
    {
        poBuffer->AppendUInt16(sLocation.locationId);
        poBuffer->AppendUInt32RefForSizeOfBuffer(sLocation.locationBufferName);
        poBuffer->AppendUInt32RefForOffset(sLocation.locationOffsetName);
    }
}

/************************************************************************/
/*                         asARCZoneDefinitions                         */
/************************************************************************/

constexpr double ARC_B = 400384;

// Content of MIL-A-89007 (ADRG specification), appendix 70, table III
static constexpr struct
{
    int nZone;  // zone number (for northern hemisphere. Add 9 for southern hemisphere)
    int minLat;     // minimum latitude of the zone
    int maxLat;     // maximum latitude of the zone
    double A;       // longitudinal pixel spacing constant at 1:1M
    double B;       // latitudinal pixel spacing constant at 1:1M
    double latRes;  // in microns
    double lonRes;  // in microns
} asARCZoneDefinitions[] = {
    {1, 0, 32, 369664, ARC_B, 99.9, 99.9},
    {2, 32, 48, 302592, ARC_B, 99.9, 99.9},
    {3, 48, 56, 245760, ARC_B, 100.0, 99.9},
    {4, 56, 64, 199168, ARC_B, 99.9, 99.9},
    {5, 64, 68, 163328, ARC_B, 99.7, 99.9},
    {6, 68, 72, 137216, ARC_B, 99.7, 99.9},
    {7, 72, 76, 110080, ARC_B, 99.8, 99.9},
    {8, 76, 80, 82432, ARC_B, 100.0, 99.9},
    {9, 80, 90, ARC_B, ARC_B, 99.9, 99.9},
};

constexpr double RATIO_PITCH_CADRG_OVER_ADRG = 150.0 / 100.0;
constexpr double REF_SCALE = 1e6;
constexpr int ADRG_BLOCK_SIZE = 512;

/************************************************************************/
/*                         GetARCZoneFromLat()                          */
/************************************************************************/

static int GetARCZoneFromLat(double dfLat)
{
    for (const auto &sZoneDef : asARCZoneDefinitions)
    {
        if (std::fabs(dfLat) >= sZoneDef.minLat &&
            std::fabs(dfLat) <= sZoneDef.maxLat)
        {
            return dfLat >= 0 ? sZoneDef.nZone
                              : sZoneDef.nZone + MAX_ZONE_NORTHERN_HEMISPHERE;
        }
    }
    return 0;
}

/************************************************************************/
/*                          GetPolarConstant()                          */
/************************************************************************/

static double GetPolarConstant(int nReciprocalScale)
{
    // Cf MIL-A-89007 (ADRG specification), appendix 70, table III
    const double N = REF_SCALE / nReciprocalScale;

    // Cf MIL-C-89038 (CADRG specification), para 60.4
    const double B_s = ARC_B * N;
    const double latCst_ADRG =
        std::ceil(B_s / ADRG_BLOCK_SIZE) * ADRG_BLOCK_SIZE;
    return std::round(latCst_ADRG * 20.0 / 360.0 / RATIO_PITCH_CADRG_OVER_ADRG /
                      ADRG_BLOCK_SIZE) *
           ADRG_BLOCK_SIZE * 360 / 20;
}

/************************************************************************/
/*                           GetYPixelSize()                            */
/************************************************************************/

/** Return the size of a pixel (in degree for non-polar zones, in meters for
 * polar zones), along the latitude/Y axis,
 * at specified scale and zone */
static double GetYPixelSize(int nZone, int nReciprocalScale)
{
    CPLAssert(RPFCADRGIsValidZone(nZone));
    const int nZoneIdx = (nZone - 1) % MAX_ZONE_NORTHERN_HEMISPHERE;

    if (nZoneIdx + 1 == MAX_ZONE_NORTHERN_HEMISPHERE)
    {
        const double polarCst = GetPolarConstant(nReciprocalScale);
        return SRS_WGS84_SEMIMAJOR * 2 * M_PI / polarCst;
    }

    const auto &sZoneDef = asARCZoneDefinitions[nZoneIdx];

    // Cf MIL-A-89007 (ADRG specification), appendix 70, table III
    const double N = REF_SCALE / nReciprocalScale;

    // Cf MIL-C-89038 (CADRG specification), para 60.1.1 and following
    const double B_s = sZoneDef.B * N;
    const double latCst_ADRG =
        std::ceil(B_s / ADRG_BLOCK_SIZE) * ADRG_BLOCK_SIZE;
    const double latCst_CADRG =
        std::round(latCst_ADRG / RATIO_PITCH_CADRG_OVER_ADRG / 4 / BLOCK_SIZE) *
        BLOCK_SIZE;
    const double latInterval = 90.0 / latCst_CADRG;

    return latInterval;
}

/************************************************************************/
/*                           GetXPixelSize()                            */
/************************************************************************/

/** Return the size of a pixel (in degree for non-polar zones, in meters for
 * polar zones), along the longitude/X axis,
 * at specified scale and zone */
static double GetXPixelSize(int nZone, int nReciprocalScale)
{
    CPLAssert(RPFCADRGIsValidZone(nZone));
    const int nZoneIdx = (nZone - MIN_ZONE) % MAX_ZONE_NORTHERN_HEMISPHERE;

    if (nZoneIdx + 1 == MAX_ZONE_NORTHERN_HEMISPHERE)
    {
        const double polarCst = GetPolarConstant(nReciprocalScale);
        return SRS_WGS84_SEMIMAJOR * 2 * M_PI / polarCst;
    }

    const auto &sZoneDef = asARCZoneDefinitions[nZoneIdx];

    // Cf MIL-A-89007 (ADRG specification), appendix 70, table III
    const double N = REF_SCALE / nReciprocalScale;

    // Cf MIL-C-89038 (CADRG specification), para 60.1.1 and following
    const double A_s = sZoneDef.A * N;
    const double lonCst_ADRG =
        std::ceil(A_s / ADRG_BLOCK_SIZE) * ADRG_BLOCK_SIZE;
    const double lonCst_CADRG =
        std::round(lonCst_ADRG / RATIO_PITCH_CADRG_OVER_ADRG / BLOCK_SIZE) *
        BLOCK_SIZE;
    const double lonInterval = 360.0 / lonCst_CADRG;

    return lonInterval;
}

/************************************************************************/
/*                  RPFGetCADRGResolutionAndInterval()                  */
/************************************************************************/

void RPFGetCADRGResolutionAndInterval(int nZone, int nReciprocalScale,
                                      double &latResolution,
                                      double &lonResolution,
                                      double &latInterval, double &lonInterval)
{
    CPLAssert(nReciprocalScale > 0);
    CPLAssert(RPFCADRGIsValidZone(nZone));
    const int nZoneIdx = (nZone - MIN_ZONE) % MAX_ZONE_NORTHERN_HEMISPHERE;
    const auto &sZoneDef = asARCZoneDefinitions[nZoneIdx];

    // Cf MIL-A-89007 (ADRG specification), appendix 70, table III
    const double N = REF_SCALE / std::max(1, nReciprocalScale);

    // Cf MIL-C-89038 (CADRG specification), para 60.1.1 and following
    const double B_s = sZoneDef.B * N;
    const double latCst_ADRG =
        std::ceil(B_s / ADRG_BLOCK_SIZE) * ADRG_BLOCK_SIZE;
    const double latCst_CADRG =
        std::round(latCst_ADRG / RATIO_PITCH_CADRG_OVER_ADRG / 4 / BLOCK_SIZE) *
        BLOCK_SIZE;
    double latResolutionLocal =
        sZoneDef.latRes / N * latCst_ADRG / (4 * latCst_CADRG);

    const double A_s = sZoneDef.A * N;
    const double lonCst_ADRG =
        std::ceil(A_s / ADRG_BLOCK_SIZE) * ADRG_BLOCK_SIZE;
    const double lonCst_CADRG =
        std::round(lonCst_ADRG / RATIO_PITCH_CADRG_OVER_ADRG / BLOCK_SIZE) *
        BLOCK_SIZE;
    double lonResolutionLocal =
        sZoneDef.lonRes / N * lonCst_ADRG / lonCst_CADRG;

    double latIntervalLocal = 90.0 / latCst_CADRG;
    double lonIntervalLocal = 360.0 / lonCst_CADRG;

    if (nZoneIdx + MIN_ZONE == MAX_ZONE_NORTHERN_HEMISPHERE)
    {
        lonResolutionLocal = latResolutionLocal;
        lonIntervalLocal = latIntervalLocal;
    }

    latResolution = latResolutionLocal;
    lonResolution = lonResolutionLocal;
    latInterval = latIntervalLocal;
    lonInterval = lonIntervalLocal;
}

/************************************************************************/
/*                      GetMinMaxLatWithOverlap()                       */
/************************************************************************/

/** Return the actual minimum and maximum latitude of a zone, for a given
 * reciprocal scale, taking into potential overlap between zones.
 */
static std::pair<double, double> GetMinMaxLatWithOverlap(int nZone,
                                                         int nReciprocalScale)
{
    if (nZone == MAX_ZONE_NORTHERN_HEMISPHERE)
    {
        return std::pair(80.0, 90.0);
    }
    else if (nZone == MAX_ZONE)
    {
        return std::pair(-90.0, -80.0);
    }
    CPLAssert(RPFCADRGIsValidZone(nZone));
    const int nZoneIdx = (nZone - MIN_ZONE) % MAX_ZONE_NORTHERN_HEMISPHERE;
    const auto &sZoneDef = asARCZoneDefinitions[nZoneIdx];

    const double latInterval = GetYPixelSize(nZone, nReciprocalScale);
    const double deltaLatFrame = latInterval * CADRG_FRAME_PIXEL_COUNT;

    const double dfMinLat =
        std::floor(sZoneDef.minLat / deltaLatFrame) * deltaLatFrame;
    const double dfMaxLat =
        std::ceil(sZoneDef.maxLat / deltaLatFrame) * deltaLatFrame;
    return nZone >= MIN_ZONE_SOUTHERN_HEMISPHERE
               ? std::pair(-dfMaxLat, -dfMinLat)
               : std::pair(dfMinLat, dfMaxLat);
}

/************************************************************************/
/*                         GetPolarFrameCount()                         */
/************************************************************************/

constexpr double EPSILON_1Em3 = 1e-3;

static int GetPolarFrameCount(int nReciprocalScale)
{
    const double numberSubFrames = std::round(
        GetPolarConstant(nReciprocalScale) * 20.0 / 360.0 / BLOCK_SIZE);
    constexpr int SUBFRAMES_PER_FRAME = CADRG_FRAME_PIXEL_COUNT / BLOCK_SIZE;
    int numberFrames = static_cast<int>(
        std::ceil(numberSubFrames / SUBFRAMES_PER_FRAME - EPSILON_1Em3));
    if ((numberFrames % 2) == 0)
        ++numberFrames;
    return numberFrames;
}

/************************************************************************/
/*                        GetFrameCountAlongX()                         */
/************************************************************************/

constexpr double dfMinLonZone = -180.0;
constexpr double dfMaxLonZone = 180.0;

static int GetFrameCountAlongX(int nZone, int nReciprocalScale)
{
    if (nZone == MAX_ZONE_NORTHERN_HEMISPHERE || nZone == MAX_ZONE)
        return GetPolarFrameCount(nReciprocalScale);
    const double lonInterval = GetXPixelSize(nZone, nReciprocalScale);
    const double eastWestPixelCst = 360.0 / lonInterval;
    CPLDebugOnly("CADRG", "eastWestPixelCst=%f, count=%f", eastWestPixelCst,
                 eastWestPixelCst / CADRG_FRAME_PIXEL_COUNT);
    const int nFrameCount = static_cast<int>(
        std::ceil(eastWestPixelCst / CADRG_FRAME_PIXEL_COUNT - EPSILON_1Em3));
    return nFrameCount;
}

/************************************************************************/
/*                        GetFrameCountAlongY()                         */
/************************************************************************/

static int GetFrameCountAlongY(int nZone, int nReciprocalScale)
{
    if (nZone == MAX_ZONE_NORTHERN_HEMISPHERE || nZone == MAX_ZONE)
        return GetPolarFrameCount(nReciprocalScale);
    const double latInterval = GetYPixelSize(nZone, nReciprocalScale);
    const double deltaLatFrame = latInterval * CADRG_FRAME_PIXEL_COUNT;
    const auto [dfMinLatZone, dfMaxLatZone] =
        GetMinMaxLatWithOverlap(nZone, nReciprocalScale);
    return std::max(1, static_cast<int>(std::ceil(dfMaxLatZone - dfMinLatZone) /
                                            deltaLatFrame -
                                        EPSILON_1Em3));
}

/************************************************************************/
/*                    RPFGetCADRGFramesForEnvelope()                    */
/************************************************************************/

enum class HemiphereType
{
    BOTH,
    NORTH,
    SOUTH,
};

static std::vector<RPFFrameDef>
RPFGetCADRGFramesForEnvelope(int nZoneIn, int nReciprocalScale,
                             GDALDataset *poSrcDS, HemiphereType hemisphere)
{
    CPLAssert(nZoneIn == 0 || RPFCADRGIsValidZone(nZoneIn));

    OGREnvelope sExtentNativeCRS;
    OGREnvelope sExtentWGS84;
    if (poSrcDS->GetExtent(&sExtentNativeCRS) != CE_None ||
        poSrcDS->GetExtentWGS84LongLat(&sExtentWGS84) != CE_None)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot get dataset extent");
        return {};
    }

    if (hemisphere == HemiphereType::BOTH)
    {
        if (sExtentWGS84.MinY < 0 && sExtentWGS84.MaxY > 0)
        {
            std::vector<RPFFrameDef> res1, res2;
            if (nZoneIn == 0 || (nZoneIn >= MIN_ZONE_SOUTHERN_HEMISPHERE))
            {
                res1 = RPFGetCADRGFramesForEnvelope(
                    nZoneIn, nReciprocalScale, poSrcDS, HemiphereType::SOUTH);
            }
            if (nZoneIn == 0 || (nZoneIn >= MIN_ZONE &&
                                 nZoneIn <= MAX_ZONE_NORTHERN_HEMISPHERE))
            {
                res2 = RPFGetCADRGFramesForEnvelope(
                    nZoneIn, nReciprocalScale, poSrcDS, HemiphereType::NORTH);
            }
            res1.insert(res1.end(), res2.begin(), res2.end());
            return res1;
        }
        else if (sExtentWGS84.MinY >= 0)
            hemisphere = HemiphereType::NORTH;
        else
            hemisphere = HemiphereType::SOUTH;
    }

    CPLAssert(hemisphere == HemiphereType::NORTH ||
              hemisphere == HemiphereType::SOUTH);

    if (!(sExtentWGS84.MinX >= dfMinLonZone &&
          sExtentWGS84.MinX < dfMaxLonZone))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Minimum longitude of extent not in [%f,%f[ range",
                 dfMinLonZone, dfMaxLonZone);
        return {};
    }

    double dfLastMaxLatZone = 0;
    std::vector<RPFFrameDef> res;
    const double dfYMinNorth = (hemisphere == HemiphereType::NORTH)
                                   ? std::max(0.0, sExtentWGS84.MinY)
                                   : std::max(0.0, -sExtentWGS84.MaxY);
    const double dfYMaxNorth = (hemisphere == HemiphereType::NORTH)
                                   ? sExtentWGS84.MaxY
                                   : -sExtentWGS84.MinY;
    const int nZoneOffset =
        (hemisphere == HemiphereType::NORTH) ? 0 : MAX_ZONE_NORTHERN_HEMISPHERE;
    CPLDebugOnly("CADRG", "Source minLat=%f, maxLat=%f", sExtentWGS84.MinY,
                 sExtentWGS84.MaxY);

    for (int nZoneNorth = MIN_ZONE;
         nZoneNorth < MAX_ZONE_NORTHERN_HEMISPHERE &&
         nZoneIn != MAX_ZONE_NORTHERN_HEMISPHERE && nZoneIn != MAX_ZONE;
         ++nZoneNorth)
    {
        const int nZone = nZoneIn ? nZoneIn : nZoneNorth + nZoneOffset;
        const int nZoneIdx = (nZone - MIN_ZONE) % MAX_ZONE_NORTHERN_HEMISPHERE;
        const auto &sZoneDef = asARCZoneDefinitions[nZoneIdx];
        if (dfYMinNorth < sZoneDef.maxLat && dfYMaxNorth > sZoneDef.minLat)
        {
            const auto [dfMinLatZone, dfMaxLatZone] =
                GetMinMaxLatWithOverlap(nZone, nReciprocalScale);
            CPLDebugOnly("CADRG",
                         "Zone %d: minLat_nominal=%d, maxLat_nominal=%d, "
                         "minLat_overlap=%f, maxLat_overlap=%f",
                         nZone,
                         (hemisphere == HemiphereType::NORTH) ? sZoneDef.minLat
                                                              : sZoneDef.maxLat,
                         (hemisphere == HemiphereType::NORTH)
                             ? sZoneDef.maxLat
                             : -sZoneDef.minLat,
                         dfMinLatZone, dfMaxLatZone);
            const double dfMaxLatZoneNorth =
                std::max(std::fabs(dfMinLatZone), std::fabs(dfMaxLatZone));
            if (dfMaxLatZoneNorth == dfLastMaxLatZone)
            {
                // Skip zone if fully within a previous one
                // This is the case of zones 5, 6 and 8 at scale 5 M.
                // See MIL-C-89038 page 70
                CPLDebugOnly(
                    "CADRG",
                    "Skipping zone %d as fully contained in previous one",
                    nZone);
                continue;
            }
            dfLastMaxLatZone = dfMaxLatZoneNorth;
            const double latInterval = GetYPixelSize(nZone, nReciprocalScale);
            const double deltaLatFrame = latInterval * CADRG_FRAME_PIXEL_COUNT;
            const double dfFrameMinY =
                (std::max(sExtentWGS84.MinY, dfMinLatZone) - dfMinLatZone) /
                deltaLatFrame;
            const double dfFrameMaxY =
                (std::min(sExtentWGS84.MaxY, dfMaxLatZone) - dfMinLatZone) /
                deltaLatFrame;
            CPLDebugOnly("CADRG", "dfFrameMinY = %f, dfFrameMaxY=%f",
                         dfFrameMinY, dfFrameMaxY);
            const int nFrameMinY = static_cast<int>(dfFrameMinY + EPSILON_1Em3);
            const int nFrameMaxY = static_cast<int>(dfFrameMaxY - EPSILON_1Em3);

            const double lonInterval = GetXPixelSize(nZone, nReciprocalScale);
            const double deltaLonFrame = lonInterval * CADRG_FRAME_PIXEL_COUNT;
            const double dfFrameMinX =
                (std::max(sExtentWGS84.MinX, dfMinLonZone) - dfMinLonZone) /
                deltaLonFrame;
            const double dfFrameMaxX =
                (std::min(sExtentWGS84.MaxX, dfMaxLonZone) - dfMinLonZone) /
                deltaLonFrame;
            CPLDebugOnly("CADRG", "dfFrameMinX = %f, dfFrameMaxX=%f",
                         dfFrameMinX, dfFrameMaxX);
            const int nFrameMinX = static_cast<int>(dfFrameMinX + EPSILON_1Em3);
            const int nFrameMaxX = static_cast<int>(dfFrameMaxX - EPSILON_1Em3);

            RPFFrameDef sDef;
            sDef.nZone = nZone;
            sDef.nReciprocalScale = nReciprocalScale;
            sDef.nFrameMinX = nFrameMinX;
            sDef.nFrameMinY = nFrameMinY;
            sDef.nFrameMaxX = nFrameMaxX;
            sDef.nFrameMaxY = nFrameMaxY;
            sDef.dfResX = lonInterval;
            sDef.dfResY = latInterval;
            CPLDebug("CADRG", "Zone %d: frame (x,y)=(%d,%d) to (%d,%d)", nZone,
                     nFrameMinX, nFrameMinY, nFrameMaxX, nFrameMaxY);
            res.push_back(sDef);
        }
        if (nZoneIn)
            break;
    }

    // Polar zone
    constexpr double MIN_POLAR_LAT_MANUAL = 70;
    constexpr double MIN_POLAR_LAT_AUTO = 80;
    if ((nZoneIn == 0 && dfYMaxNorth > MIN_POLAR_LAT_AUTO) ||
        (nZoneIn == MAX_ZONE_NORTHERN_HEMISPHERE + nZoneOffset &&
         dfYMaxNorth > MIN_POLAR_LAT_MANUAL))
    {
        const int nZone = MAX_ZONE_NORTHERN_HEMISPHERE + nZoneOffset;
        OGRSpatialReference oPolarSRS;
        oPolarSRS.importFromWkt(hemisphere == HemiphereType::NORTH
                                    ? pszNorthPolarProjection
                                    : pszSouthPolarProjection);
        const auto poSrcSRS = poSrcDS->GetSpatialRef();
        auto poCT = std::unique_ptr<OGRCoordinateTransformation>(
            OGRCreateCoordinateTransformation(poSrcSRS, &oPolarSRS));
        double dfXMin = 0;
        double dfYMin = 0;
        double dfXMax = 0;
        double dfYMax = 0;

        if (poSrcSRS->IsGeographic())
        {
            if (hemisphere == HemiphereType::NORTH)
                sExtentNativeCRS.MinY =
                    std::max(MIN_POLAR_LAT_MANUAL, sExtentNativeCRS.MinY);
            else
                sExtentNativeCRS.MaxY =
                    std::min(-MIN_POLAR_LAT_MANUAL, sExtentNativeCRS.MaxY);
        }

        if (poCT->TransformBounds(sExtentNativeCRS.MinX, sExtentNativeCRS.MinY,
                                  sExtentNativeCRS.MaxX, sExtentNativeCRS.MaxY,
                                  &dfXMin, &dfYMin, &dfXMax, &dfYMax,
                                  DEFAULT_DENSIFY_PTS))
        {
            const int numberFrames = GetPolarFrameCount(nReciprocalScale);

            // Cf MIL-C-89038 (CADRG specification), para 30.5
            const double R = numberFrames / 2.0 * CADRG_FRAME_PIXEL_COUNT;

            RPFFrameDef sDef;
            sDef.nZone = nZone;
            sDef.nReciprocalScale = nReciprocalScale;
            sDef.dfResX = GetXPixelSize(nZone, nReciprocalScale);
            // will lead to same value as dfResX
            sDef.dfResY = GetYPixelSize(nZone, nReciprocalScale);
            sDef.nFrameMinX =
                std::clamp(static_cast<int>((dfXMin / sDef.dfResX + R) /
                                                CADRG_FRAME_PIXEL_COUNT +
                                            EPSILON_1Em3),
                           0, numberFrames - 1);
            sDef.nFrameMinY =
                std::clamp(static_cast<int>((dfYMin / sDef.dfResY + R) /
                                                CADRG_FRAME_PIXEL_COUNT +
                                            EPSILON_1Em3),
                           0, numberFrames - 1);
            sDef.nFrameMaxX =
                std::clamp(static_cast<int>((dfXMax / sDef.dfResX + R) /
                                                CADRG_FRAME_PIXEL_COUNT -
                                            EPSILON_1Em3),
                           0, numberFrames - 1);
            sDef.nFrameMaxY =
                std::clamp(static_cast<int>((dfYMax / sDef.dfResY + R) /
                                                CADRG_FRAME_PIXEL_COUNT -
                                            EPSILON_1Em3),
                           0, numberFrames - 1);
            CPLDebug("CADRG", "Zone %d: frame (x,y)=(%d,%d) to (%d,%d)",
                     sDef.nZone, sDef.nFrameMinX, sDef.nFrameMinY,
                     sDef.nFrameMaxX, sDef.nFrameMaxY);
            res.push_back(sDef);
        }
        else if (nZoneIn == 0)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cannot reproject source dataset extent to polar "
                     "Azimuthal Equidistant");
        }
    }

    return res;
}

/** Given a dataset and a reciprocal scale (e.g. 1,000,000), returns the min/max
 * frame coordinate indices in all zones that intersect that area of interest
 * (when nZoneIn is 0), or in the specified zone (when nZoneIn is not 0)
 */
std::vector<RPFFrameDef> RPFGetCADRGFramesForEnvelope(int nZoneIn,
                                                      int nReciprocalScale,
                                                      GDALDataset *poSrcDS)
{
    return RPFGetCADRGFramesForEnvelope(nZoneIn, nReciprocalScale, poSrcDS,
                                        HemiphereType::BOTH);
}

/************************************************************************/
/*                   RPFGetCADRGFrameNumberAsString()                   */
/************************************************************************/

/** Returns the 5 first character of the filename corresponding to the
 * frame specified by the provided parameters.
 */
std::string RPFGetCADRGFrameNumberAsString(int nZone, int nReciprocalScale,
                                           int nFrameX, int nFrameY)
{
    // Cf MIL-C-89038, page 60, 30.6 "Frame naming convention"

    const int nFrameCountAlongX = GetFrameCountAlongX(nZone, nReciprocalScale);
    const int nFrameCountAlongY = GetFrameCountAlongY(nZone, nReciprocalScale);
    CPLDebugOnly("CADRG",
                 "Zone %d -> nFrameCountAlongX = %d, nFrameCountAlongY = %d",
                 nZone, nFrameCountAlongX, nFrameCountAlongY);
    CPL_IGNORE_RET_VAL(nFrameCountAlongY);
    CPLAssert(nFrameX >= 0 && nFrameX < nFrameCountAlongX);
    CPLAssert(nFrameY >= 0 && nFrameY < nFrameCountAlongY);
    const int nFrameIdx = nFrameX + nFrameY * nFrameCountAlongX;
    CPLDebugOnly("CADRG", "Frame number (%d, %d) -> %d", nFrameX, nFrameY,
                 nFrameIdx);

    std::string osRes;

    constexpr int BASE_34 = 34;
    // clang-format off
    // letters 'I' and 'O' are omitted to avoid confusiong with one and zero
    constexpr char ALPHABET_BASE_34[] = {
        '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
        'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',      'J',
        'K', 'L', 'M', 'N',      'P', 'Q', 'R', 'S', 'T',
        'U', 'V', 'W', 'X', 'Y', 'Z'
    };
    // clang-format on
    static_assert(sizeof(ALPHABET_BASE_34) == BASE_34);

    int nCur = nFrameIdx;
    do
    {
        osRes += ALPHABET_BASE_34[nCur % BASE_34];
        nCur /= BASE_34;
    } while (nCur > 0);

    std::reverse(osRes.begin(), osRes.end());
    // Pad to 5 characters with leading zeroes.
    while (osRes.size() < 5)
        osRes = '0' + osRes;
    return osRes;
}

/************************************************************************/
/*                       RPFGetCADRGFrameExtent()                       */
/************************************************************************/

void RPFGetCADRGFrameExtent(int nZone, int nReciprocalScale, int nFrameX,
                            int nFrameY, double &dfXMin, double &dfYMin,
                            double &dfXMax, double &dfYMax)
{
    CPLAssert(RPFCADRGIsValidZone(nZone));
    const double dfXRes = GetXPixelSize(nZone, nReciprocalScale);
    const double dfYRes = GetYPixelSize(nZone, nReciprocalScale);

    double dfXMinLocal, dfYMinLocal;
    if (nZone == MAX_ZONE_NORTHERN_HEMISPHERE || nZone == MAX_ZONE)
    {
        const int numberFrames = GetPolarFrameCount(nReciprocalScale);
        const double dfXYOrigin =
            -numberFrames / 2.0 * dfXRes * CADRG_FRAME_PIXEL_COUNT;
        dfXMinLocal = dfXYOrigin + nFrameX * dfXRes * CADRG_FRAME_PIXEL_COUNT;
        dfYMinLocal = dfXYOrigin + nFrameY * dfYRes * CADRG_FRAME_PIXEL_COUNT;
    }
    else
    {
        const auto [dfMinLatZone, ignored] =
            GetMinMaxLatWithOverlap(nZone, nReciprocalScale);
        dfXMinLocal = dfMinLonZone + nFrameX * dfXRes * CADRG_FRAME_PIXEL_COUNT;
        dfYMinLocal = dfMinLatZone + nFrameY * dfYRes * CADRG_FRAME_PIXEL_COUNT;
    }

    dfXMin = dfXMinLocal;
    dfYMin = dfYMinLocal;
    dfXMax = dfXMinLocal + dfXRes * CADRG_FRAME_PIXEL_COUNT;
    dfYMax = dfYMinLocal + dfYRes * CADRG_FRAME_PIXEL_COUNT;
}

/************************************************************************/
/*                 RPFGetCADRGClosestReciprocalScale()                  */
/************************************************************************/

int RPFGetCADRGClosestReciprocalScale(GDALDataset *poSrcDS,
                                      double dfDPIOverride, bool &bGotDPI)
{
    bGotDPI = false;

    constexpr double INCH_TO_CM = 2.54;
    double dfDPI = dfDPIOverride;
    if (dfDPI <= 0)
    {
        const char *pszUnit =
            poSrcDS->GetMetadataItem("TIFFTAG_RESOLUTIONUNIT");
        const char *pszYRes = poSrcDS->GetMetadataItem("TIFFTAG_YRESOLUTION");
        if (pszUnit && pszYRes)
        {
            if (EQUAL(pszUnit, "2"))  // Inch
                dfDPI = CPLAtof(pszYRes);
            else if (EQUAL(pszUnit, "3"))  // Centimeter
            {
                dfDPI = CPLAtof(pszYRes) * INCH_TO_CM;
            }
        }
    }
    if (dfDPI <= 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Failed to estimate a reciprocal scale due to lack of DPI "
                 "information. Please specify the DPI creation option.\n"
                 "For reference, ADRG DPI is 254 and CADRG DPI is 169.333");
        return 0;
    }

    bGotDPI = true;

    constexpr double CADRG_DPI = INCH_TO_CM / CADRG_PITCH_IN_CM;

    GDALGeoTransform gt;
    if (poSrcDS->GetGeoTransform(gt) != CE_None)
    {

        CPLError(CE_Failure, CPLE_AppDefined, "Cannot get geotransform");
        return 0;
    }

    const double dfYRes = std::fabs(gt[5]);
    const double dfYResAtNominalCADRGDPI = dfYRes * dfDPI / CADRG_DPI;

    std::set<int> anSetReciprocalScales;
    for (int nIdx = 0;; ++nIdx)
    {
        const auto *psEntry = NITFGetRPFSeriesInfoFromIndex(nIdx);
        if (!psEntry)
            break;
        if (EQUAL(psEntry->rpfDataType, "CADRG"))
        {
            const int nVal =
                NITFGetScaleFromScaleResolution(psEntry->scaleResolution);
            if (nVal)
                anSetReciprocalScales.insert(nVal);
        }
    }

    int nCandidateReciprocalScale = 0;
    double dfBestProximityRatio = 0;

    // We tolerate up to a factor of 2 between the CADRG resolution at a
    //given scale and the dataset resolution
    constexpr double MAX_PROXIMITY_RATIO = 2;

    for (int nReciprocalScale : anSetReciprocalScales)
    {
        // This is actually zone independent
        const double dfLatInterval =
            GetYPixelSize(/* nZone = */ 1, nReciprocalScale);
        if (nCandidateReciprocalScale == 0 &&
            dfLatInterval / dfYResAtNominalCADRGDPI > MAX_PROXIMITY_RATIO)
        {
            break;
        }
        if (dfYResAtNominalCADRGDPI <= dfLatInterval)
        {
            const double dfThisProximityRatio =
                dfLatInterval / dfYResAtNominalCADRGDPI;
            if (nCandidateReciprocalScale == 0 ||
                dfThisProximityRatio < dfBestProximityRatio)
            {
                nCandidateReciprocalScale = nReciprocalScale;
            }
            break;
        }
        else
        {
            const double dfThisProximityRatio =
                dfYResAtNominalCADRGDPI / dfLatInterval;
            if (dfThisProximityRatio < MAX_PROXIMITY_RATIO &&
                (nCandidateReciprocalScale == 0 ||
                 dfThisProximityRatio < dfBestProximityRatio))
            {
                nCandidateReciprocalScale = nReciprocalScale;
                dfBestProximityRatio = dfThisProximityRatio;
            }
        }
    }

    if (nCandidateReciprocalScale == 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find a pre-established scale matching source dataset "
                 "pixel size and scan resolution");
    }
    return nCandidateReciprocalScale;
}

/************************************************************************/
/*                    Create_CADRG_CoverageSection()                    */
/************************************************************************/

static bool Create_CADRG_CoverageSection(
    GDALOffsetPatcher::OffsetPatcher *offsetPatcher, GDALDataset *poSrcDS,
    const std::string &osFilename, const CPLStringList &aosOptions,
    int nReciprocalScale)
{
    auto poBuffer = offsetPatcher->CreateBuffer(
        "CoverageSectionSubheader", /* bEndiannessIsLittle = */ false);
    CPLAssert(poBuffer);
    poBuffer->DeclareOffsetAtCurrentPosition("COVERAGE_SECTION_LOCATION");

    OGREnvelope sExtent;
    const auto poSrcSRS = poSrcDS->GetSpatialRef();
    if (!poSrcSRS || poSrcDS->GetExtent(&sExtent) != CE_None)
    {
        CPLAssert(false);  // already checked in calling sites
        return false;
    }

    double dfULX = sExtent.MinX;
    double dfULY = sExtent.MaxY;
    double dfLLX = sExtent.MinX;
    double dfLLY = sExtent.MinY;
    double dfURX = sExtent.MaxX;
    double dfURY = sExtent.MaxY;
    double dfLRX = sExtent.MaxX;
    double dfLRY = sExtent.MinY;

    OGRSpatialReference oSRS_WGS84;
    oSRS_WGS84.SetWellKnownGeogCS("WGS84");
    oSRS_WGS84.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    if (!poSrcSRS->IsSame(&oSRS_WGS84))
    {
        auto poCT = std::unique_ptr<OGRCoordinateTransformation>(
            OGRCreateCoordinateTransformation(poSrcSRS, &oSRS_WGS84));
        if (!(poCT && poCT->Transform(1, &dfULX, &dfULY) &&
              poCT->Transform(1, &dfLLX, &dfLLY) &&
              poCT->Transform(1, &dfURX, &dfURY) &&
              poCT->Transform(1, &dfLRX, &dfLRY)))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Create_CADRG_CoverageSection(): cannot compute corner "
                     "lat/lon");
            return false;
        }
    }

    const auto RoundIfCloseToInt = [](double dfX)
    {
        double dfRounded = std::round(dfX);
        if (std::abs(dfX - dfRounded) < 1e-12)
            return dfRounded;
        return dfX;
    };

    const auto NormalizeToMinusPlus180 = [](double dfLon)
    {
        constexpr double EPSILON_SMALL = 1e-9;
        if (dfLon < -180 - EPSILON_SMALL)
            dfLon += 360;
        else if (dfLon < -180)
            dfLon = -180;
        else if (dfLon > 180 + EPSILON_SMALL)
            dfLon -= 360;
        else if (dfLon > 180)
            dfLon = 180;
        return dfLon;
    };

    int nZone = atoi(aosOptions.FetchNameValueDef("ZONE", "0"));
    if (nZone < MIN_ZONE || nZone > MAX_ZONE)
    {
        const std::string osExt = CPLGetExtensionSafe(osFilename.c_str());
        if (osExt.size() == 3)
            nZone = RPFCADRGZoneCharToNum(osExt.back());
        if (nZone < MIN_ZONE || nZone > MAX_ZONE)
        {
            const double dfMeanLat = (dfULY + dfLLY) / 2;
            nZone = GetARCZoneFromLat(dfMeanLat);
            if (nZone == 0)
                return false;
        }
    }

    // Upper left corner lat, lon
    poBuffer->AppendFloat64(RoundIfCloseToInt(dfULY));
    poBuffer->AppendFloat64(RoundIfCloseToInt(NormalizeToMinusPlus180(dfULX)));
    // Lower left corner lat, lon
    poBuffer->AppendFloat64(RoundIfCloseToInt(dfLLY));
    poBuffer->AppendFloat64(RoundIfCloseToInt(NormalizeToMinusPlus180(dfLLX)));
    // Upper right corner lat, lon
    poBuffer->AppendFloat64(RoundIfCloseToInt(dfURY));
    poBuffer->AppendFloat64(RoundIfCloseToInt(NormalizeToMinusPlus180(dfURX)));
    // Lower right corner lat, lon
    poBuffer->AppendFloat64(RoundIfCloseToInt(dfLRY));
    poBuffer->AppendFloat64(RoundIfCloseToInt(NormalizeToMinusPlus180(dfLRX)));

    double latResolution = 0;
    double lonResolution = 0;
    double latInterval = 0;
    double lonInterval = 0;
    RPFGetCADRGResolutionAndInterval(nZone, nReciprocalScale, latResolution,
                                     lonResolution, latInterval, lonInterval);

    poBuffer->AppendFloat64(latResolution);
    poBuffer->AppendFloat64(lonResolution);
    if (!poSrcSRS->IsSame(&oSRS_WGS84))
    {
        poBuffer->AppendFloat64(latInterval);
        poBuffer->AppendFloat64(lonInterval);
    }
    else
    {
        GDALGeoTransform gt;
        CPL_IGNORE_RET_VAL(poSrcDS->GetGeoTransform(gt));

        // Theoretical value: latInterval = 90.0 / latCst_CADRG;
        poBuffer->AppendFloat64(std::fabs(gt[5]));
        // Theoretical value: lonInterval = 360.0 / lonCst_CADRG;
        poBuffer->AppendFloat64(gt[1]);
    }
    return true;
}

/************************************************************************/
/*                 Create_CADRG_ColorGrayscaleSection()                 */
/************************************************************************/

constexpr GByte NUM_COLOR_TABLES = 3;
constexpr GByte NUM_COLOR_CONVERTERS = 2;

static void Create_CADRG_ColorGrayscaleSection(
    GDALOffsetPatcher::OffsetPatcher *offsetPatcher)
{
    auto poBuffer = offsetPatcher->CreateBuffer(
        "ColorGrayscaleSectionSubheader", /* bEndiannessIsLittle = */ false);
    CPLAssert(poBuffer);
    poBuffer->DeclareOffsetAtCurrentPosition("COLOR_GRAYSCALE_LOCATION");
    poBuffer->AppendByte(NUM_COLOR_TABLES);
    poBuffer->AppendByte(NUM_COLOR_CONVERTERS);
    // EXTERNAL_COLOR_GRAYSCALE_FILENAME
    poBuffer->AppendString(std::string(12, ' '));
}

/************************************************************************/
/*                Create_CADRG_ImageDescriptionSection()                */
/************************************************************************/

static void Create_CADRG_ImageDescriptionSection(
    GDALOffsetPatcher::OffsetPatcher *offsetPatcher, GDALDataset *poSrcDS,
    bool bHasTransparentPixels)
{
    CPLAssert((poSrcDS->GetRasterXSize() % BLOCK_SIZE) == 0);
    CPLAssert((poSrcDS->GetRasterYSize() % BLOCK_SIZE) == 0);
    CPLAssert(poSrcDS->GetRasterXSize() <= UINT16_MAX * BLOCK_SIZE);
    CPLAssert(poSrcDS->GetRasterYSize() <= UINT16_MAX * BLOCK_SIZE);
    const uint16_t nSubFramesPerRow =
        static_cast<uint16_t>(poSrcDS->GetRasterXSize() / BLOCK_SIZE);
    const uint16_t nSubFramesPerCol =
        static_cast<uint16_t>(poSrcDS->GetRasterYSize() / BLOCK_SIZE);
    CPLAssert(nSubFramesPerRow * nSubFramesPerCol < UINT16_MAX);

    auto poBuffer = offsetPatcher->CreateBuffer(
        "ImageDescriptionSubheader", /* bEndiannessIsLittle = */ false);
    CPLAssert(poBuffer);
    poBuffer->DeclareOffsetAtCurrentPosition(
        "IMAGE_DESCRIPTION_SECTION_LOCATION");
    poBuffer->AppendUInt16(1);  // NUMBER_OF_SPECTRAL_GROUPS
    poBuffer->AppendUInt16(static_cast<uint16_t>(
        nSubFramesPerRow * nSubFramesPerCol));  // NUMBER_OF_SUBFRAME_TABLES
    poBuffer->AppendUInt16(1);  // NUMBER_OF_SPECTRAL_BAND_TABLES
    poBuffer->AppendUInt16(1);  // NUMBER_OF_SPECTRAL_BAND_LINES_PER_IMAGE_ROW
    poBuffer->AppendUInt16(
        nSubFramesPerRow);  // NUMBER_OF_SUBFRAME_IN_EAST_WEST_DIRECTION
    poBuffer->AppendUInt16(
        nSubFramesPerCol);  // NUMBER_OF_SUBFRAME_IN_NORTH_SOUTH_DIRECTION
    poBuffer->AppendUInt32(
        BLOCK_SIZE);  // NUMBER_OF_OUTPUT_COLUMNS_PER_SUBFRAME
    poBuffer->AppendUInt32(BLOCK_SIZE);  // NUMBER_OF_OUTPUT_ROWS_PER_SUBFRAME
    poBuffer->AppendUInt32(UINT32_MAX);  // SUBFRAME_MASK_TABLE_OFFSET
    if (!bHasTransparentPixels)
    {
        poBuffer->AppendUInt32(UINT32_MAX);  // TRANSPARENCY_MASK_TABLE_OFFSET
    }
    else
    {
        // Offset in bytes from the beginning of the mask subsection to the
        // beginning of the transparency mask table.
        poBuffer->AppendUInt32(7);
    }
}

/************************************************************************/
/*                    Create_CADRG_ColormapSection()                    */
/************************************************************************/

constexpr uint16_t SZ_UINT16 = 2;
constexpr uint16_t SZ_UINT32 = 4;
constexpr uint32_t COLORMAP_OFFSET_TABLE_OFFSET = SZ_UINT32 + SZ_UINT16;
constexpr uint16_t COLOR_GRAYSCALE_OFFSET_RECORD_LENGTH = 17;

static void Create_CADRG_ColormapSection(
    GDALOffsetPatcher::OffsetPatcher *offsetPatcher, GDALDataset *poSrcDS,
    bool bHasTransparentPixels,
    const std::vector<BucketItem<ColorTableBased4x4Pixels>> &codebook,
    const CADRGCreateCopyContext &copyContext)
{
    auto poBuffer = offsetPatcher->CreateBuffer(
        "ColormapSubsection", /* bEndiannessIsLittle = */ false);
    CPLAssert(poBuffer);
    poBuffer->DeclareOffsetAtCurrentPosition("COLORMAP_LOCATION");
    poBuffer->AppendUInt32(COLORMAP_OFFSET_TABLE_OFFSET);
    poBuffer->AppendUInt16(COLOR_GRAYSCALE_OFFSET_RECORD_LENGTH);
    CPLAssert(poBuffer->GetBuffer().size() == COLORMAP_OFFSET_TABLE_OFFSET);

    uint32_t nColorTableOffset =
        COLORMAP_OFFSET_TABLE_OFFSET +
        NUM_COLOR_TABLES * COLOR_GRAYSCALE_OFFSET_RECORD_LENGTH;
    // 4=R,G,B,M
    constexpr GByte COLOR_TABLE_ENTRY_SIZE = 4;
    uint32_t nHistogramTableOffset =
        COLORMAP_OFFSET_TABLE_OFFSET +
        NUM_COLOR_TABLES * COLOR_GRAYSCALE_OFFSET_RECORD_LENGTH +
        (CADRG_MAX_COLOR_ENTRY_COUNT + CADRG_SECOND_CT_COUNT +
         CADRG_THIRD_CT_COUNT) *
            COLOR_TABLE_ENTRY_SIZE;
    for (int i = 0; i < NUM_COLOR_TABLES; ++i)
    {
        poBuffer->AppendUInt16(2);  // color/grayscale table id
        // number of colors
        const uint32_t nColorCount = (i == 0)   ? CADRG_MAX_COLOR_ENTRY_COUNT
                                     : (i == 1) ? CADRG_SECOND_CT_COUNT
                                                : CADRG_THIRD_CT_COUNT;
        poBuffer->AppendUInt32(nColorCount);
        poBuffer->AppendByte(
            COLOR_TABLE_ENTRY_SIZE);  // color/grayscale element length
        poBuffer->AppendUInt16(static_cast<uint16_t>(
            sizeof(uint32_t)));  // histogram record length
        // color/grayscale table offset
        poBuffer->AppendUInt32(nColorTableOffset);
        nColorTableOffset += nColorCount * COLOR_TABLE_ENTRY_SIZE;
        // histogram table offset
        poBuffer->AppendUInt32(nHistogramTableOffset);
        nHistogramTableOffset +=
            nColorCount * static_cast<uint32_t>(sizeof(uint32_t));
    }
    CPLAssert(poBuffer->GetBuffer().size() ==
              COLORMAP_OFFSET_TABLE_OFFSET +
                  NUM_COLOR_TABLES * COLOR_GRAYSCALE_OFFSET_RECORD_LENGTH);

    // Write color tables
    for (int iCT = 0; iCT < NUM_COLOR_TABLES; ++iCT)
    {
        const auto poCT = (iCT == 0)
                              ? poSrcDS->GetRasterBand(1)->GetColorTable()
                          : (iCT == 1) ? &(copyContext.oCT2)
                                       : &(copyContext.oCT3);
        const int nMaxCTEntries =
            (iCT == 0)
                ? CADRG_MAX_COLOR_ENTRY_COUNT + (bHasTransparentPixels ? 1 : 0)
            : (iCT == 1) ? CADRG_SECOND_CT_COUNT
                         : CADRG_THIRD_CT_COUNT;
        for (int i = 0; i < nMaxCTEntries; ++i)
        {
            if (i < poCT->GetColorEntryCount())
            {
                const auto psEntry = poCT->GetColorEntry(i);
                poBuffer->AppendByte(static_cast<GByte>(psEntry->c1));
                poBuffer->AppendByte(static_cast<GByte>(psEntry->c2));
                poBuffer->AppendByte(static_cast<GByte>(psEntry->c3));
                // Standard formula to convert R,G,B to gray scale level
                const int M = (psEntry->c1 * 299 + psEntry->c2 * 587 +
                               psEntry->c3 * 114) /
                              1000;
                poBuffer->AppendByte(static_cast<GByte>(M));
            }
            else
            {
                poBuffer->AppendUInt32(0);
            }
        }
    }

    // Compute the number of pixels in the output image per colormap entry
    // (exclude the entry for transparent pixels)
    std::vector<uint32_t> anHistogram(CADRG_MAX_COLOR_ENTRY_COUNT);
    std::vector<uint32_t> anHistogram2(CADRG_SECOND_CT_COUNT);
    std::vector<uint32_t> anHistogram3(CADRG_THIRD_CT_COUNT);
    size_t nTotalCount = 0;
    for (const auto &[i, item] : cpl::enumerate(codebook))
    {
        if (bHasTransparentPixels && i == TRANSPARENT_CODEBOOK_CODE)
        {
            nTotalCount += item.m_count;
        }
        else
        {
            for (GByte byVal : item.m_vec.vals())
            {
                anHistogram[byVal] += item.m_count;
                nTotalCount += item.m_count;
                anHistogram2[copyContext.anMapCT1ToCT2[byVal]] += item.m_count;
                anHistogram3[copyContext.anMapCT1ToCT3[byVal]] += item.m_count;
            }
        }
    }
    CPLAssert(nTotalCount == static_cast<size_t>(poSrcDS->GetRasterXSize()) *
                                 poSrcDS->GetRasterYSize());
    CPL_IGNORE_RET_VAL(nTotalCount);

    // Write histograms
    for (auto nCount : anHistogram)
    {
        poBuffer->AppendUInt32(nCount);
    }
    for (auto nCount : anHistogram2)
    {
        poBuffer->AppendUInt32(nCount);
    }
    for (auto nCount : anHistogram3)
    {
        poBuffer->AppendUInt32(nCount);
    }
}

/************************************************************************/
/*               Create_CADRG_ColorConverterSubsection()                */
/************************************************************************/

static void Create_CADRG_ColorConverterSubsection(
    GDALOffsetPatcher::OffsetPatcher *offsetPatcher, bool bHasTransparentPixels,
    const CADRGCreateCopyContext &copyContext)
{
    auto poBuffer = offsetPatcher->CreateBuffer(
        "ColorConverterSubsection", /* bEndiannessIsLittle = */ false);
    CPLAssert(poBuffer);
    poBuffer->DeclareOffsetAtCurrentPosition("COLOR_CONVERTER_SUBSECTION");

    constexpr uint32_t COLOR_CONVERTER_OFFSET_TABLE_OFFSET =
        SZ_UINT32 + SZ_UINT16 + SZ_UINT16;
    poBuffer->AppendUInt32(COLOR_CONVERTER_OFFSET_TABLE_OFFSET);
    constexpr uint16_t COLOR_CONVERTER_OFFSET_RECORD_LENGTH =
        SZ_UINT16 + SZ_UINT32 + SZ_UINT32 + SZ_UINT32;
    poBuffer->AppendUInt16(COLOR_CONVERTER_OFFSET_RECORD_LENGTH);
    constexpr uint16_t COLOR_CONVERTER_RECORD_LENGTH = SZ_UINT32;
    poBuffer->AppendUInt16(COLOR_CONVERTER_RECORD_LENGTH);
    const int numberColorConverterRecords =
        CADRG_MAX_COLOR_ENTRY_COUNT + (bHasTransparentPixels ? 1 : 0);

    for (int i = 0; i < NUM_COLOR_CONVERTERS; ++i)
    {
        constexpr uint16_t COLOR_CONVERTER_TABLE_ID = 5;
        poBuffer->AppendUInt16(COLOR_CONVERTER_TABLE_ID);
        poBuffer->AppendUInt32(numberColorConverterRecords);
        uint32_t colorConverterTableOffset =
            COLOR_CONVERTER_OFFSET_TABLE_OFFSET +
            2 * COLOR_CONVERTER_OFFSET_RECORD_LENGTH +
            i * numberColorConverterRecords * COLOR_CONVERTER_RECORD_LENGTH;
        poBuffer->AppendUInt32(colorConverterTableOffset);
        constexpr uint32_t SOURCE_OFFSET_TABLE_OFFSET =
            COLORMAP_OFFSET_TABLE_OFFSET;
        poBuffer->AppendUInt32(SOURCE_OFFSET_TABLE_OFFSET);
        const uint32_t targetOffsetTableOffset =
            COLORMAP_OFFSET_TABLE_OFFSET +
            i * COLOR_GRAYSCALE_OFFSET_RECORD_LENGTH;
        poBuffer->AppendUInt32(targetOffsetTableOffset);
    }

    for (int iCvt = 0; iCvt < NUM_COLOR_CONVERTERS; ++iCvt)
    {
        for (int j = 0; j < numberColorConverterRecords; ++j)
        {
            if (j == CADRG_MAX_COLOR_ENTRY_COUNT)
            {
                // It is not specified what we should do about the transparent
                // entry...
                poBuffer->AppendUInt32(0);
            }
            else
            {
                poBuffer->AppendUInt32((iCvt == 0)
                                           ? copyContext.anMapCT1ToCT2[j]
                                           : copyContext.anMapCT1ToCT3[j]);
            }
        }
    }
}

/************************************************************************/
/*                    Perform_CADRG_VQ_Compression()                    */
/************************************************************************/

static bool Perform_CADRG_VQ_Compression(
    GDALDataset *poSrcDS,
    std::vector<BucketItem<ColorTableBased4x4Pixels>> &codebook,
    std::vector<short> &VQImage, bool &bHasTransparentPixels)
{
    const int nY = poSrcDS->GetRasterYSize();
    const int nX = poSrcDS->GetRasterXSize();
    CPLAssert((nY % SUBSAMPLING) == 0);
    CPLAssert((nX % SUBSAMPLING) == 0);
    CPLAssert(nX < INT_MAX / nY);

    auto poBand = poSrcDS->GetRasterBand(1);

    std::vector<GByte> pixels;
    if (poBand->ReadRaster(pixels) != CE_None)
        return false;

    const auto poCT = poBand->GetColorTable();
    CPLAssert(poCT);
    std::vector<GByte> vR, vG, vB;
    const int nColorCount =
        std::min(CADRG_MAX_COLOR_ENTRY_COUNT, poCT->GetColorEntryCount());
    const bool bHasTransparentEntry =
        poCT->GetColorEntryCount() >= CADRG_MAX_COLOR_ENTRY_COUNT + 1 &&
        poCT->GetColorEntry(TRANSPARENT_COLOR_TABLE_ENTRY)->c1 == 0 &&
        poCT->GetColorEntry(TRANSPARENT_COLOR_TABLE_ENTRY)->c2 == 0 &&
        poCT->GetColorEntry(TRANSPARENT_COLOR_TABLE_ENTRY)->c3 == 0 &&
        poCT->GetColorEntry(TRANSPARENT_COLOR_TABLE_ENTRY)->c4 == 0;
    for (int i = 0; i < nColorCount; ++i)
    {
        const auto entry = poCT->GetColorEntry(i);
        vR.push_back(static_cast<GByte>(entry->c1));
        vG.push_back(static_cast<GByte>(entry->c2));
        vB.push_back(static_cast<GByte>(entry->c3));
    }
    ColorTableBased4x4Pixels ctxt(vR, vG, vB);

    struct Occurrences
    {
        // number of 4x4 pixel blocks using those 4x4 pixel values
        int nCount = 0;
        // Point to indices in the output image that use that 4x4 pixel blocks
        std::vector<int> anIndicesToOutputImage{};
    };

    const int nVQImgHeight = nY / SUBSAMPLING;
    const int nVQImgWidth = nX / SUBSAMPLING;
    CPLAssert(nVQImgWidth > 0);
    CPLAssert(nVQImgHeight > 0);
    CPLAssert(nVQImgWidth < INT_MAX / nVQImgHeight);
    VQImage.resize(nVQImgHeight * nVQImgWidth);

    // Collect all the occurrences of 4x4 pixel values into a map indexed by them
    std::map<Vector<ColorTableBased4x4Pixels>, Occurrences> vectorMap;
    bHasTransparentPixels = false;
    std::array<GByte, SUBSAMPLING * SUBSAMPLING> vals;
    std::fill(vals.begin(), vals.end(), static_cast<GByte>(0));
    int nTransparentPixels = 0;
    for (int j = 0, nOutputIdx = 0; j < nVQImgHeight; ++j)
    {
        for (int i = 0; i < nVQImgWidth; ++i, ++nOutputIdx)
        {
            for (int y = 0; y < SUBSAMPLING; ++y)
            {
                for (int x = 0; x < SUBSAMPLING; ++x)
                {
                    const GByte val = pixels[(j * SUBSAMPLING + y) * nX +
                                             (i * SUBSAMPLING + x)];
                    if (bHasTransparentEntry &&
                        val == TRANSPARENT_COLOR_TABLE_ENTRY)
                    {
                        // As soon as one of the pixels in the 4x4 block is
                        // transparent, the whole block is flagged as transparent
                        bHasTransparentPixels = true;
                        VQImage[nOutputIdx] = TRANSPARENT_CODEBOOK_CODE;
                    }
                    else
                    {
                        if (val >= nColorCount)
                        {
                            CPLError(CE_Failure, CPLE_AppDefined,
                                     "Out of range pixel value found: %d", val);
                            return false;
                        }
                        vals[SUBSAMPLING * y + x] = val;
                    }
                }
            }
            if (VQImage[nOutputIdx] == TRANSPARENT_CODEBOOK_CODE)
            {
                nTransparentPixels += SUBSAMPLING * SUBSAMPLING;
            }
            else
            {
                auto &elt = vectorMap[Vector<ColorTableBased4x4Pixels>(vals)];
                ++elt.nCount;
                elt.anIndicesToOutputImage.push_back(nOutputIdx);
            }
        }
    }

    // Convert that map into a std::vector
    std::vector<BucketItem<ColorTableBased4x4Pixels>> vectors;
    vectors.reserve(vectorMap.size());
    for (auto &[key, value] : vectorMap)
    {
        vectors.emplace_back(key, value.nCount,
                             std::move(value.anIndicesToOutputImage));
    }
    vectorMap.clear();

    // Create the KD-Tree
    PNNKDTree<ColorTableBased4x4Pixels> kdtree;

    // Insert the initial items
    const bool bEmptyImage = vectors.empty();
    int nCodeCount = kdtree.insert(std::move(vectors), ctxt);
    if (!bEmptyImage && nCodeCount == 0)
        return false;

    // Reduce to the maximum target
    const int nMaxCodes =
        bHasTransparentPixels ? CODEBOOK_MAX_SIZE - 1 : CODEBOOK_MAX_SIZE;
    if (nCodeCount > nMaxCodes)
    {
        const int nNewCodeCount = kdtree.cluster(nCodeCount, nMaxCodes, ctxt);
        if (nNewCodeCount == 0)
            return false;
        CPLDebug("NITF", "VQ compression: reducing from %d codes to %d",
                 nCodeCount, nNewCodeCount);
    }
    else
    {
        CPLDebug("NITF",
                 "Already less than %d codes. VQ compression is lossless",
                 nMaxCodes);
    }

    // Create the code book and the target VQ-compressed image.
    codebook.reserve(CODEBOOK_MAX_SIZE);
    kdtree.iterateOverLeaves(
        [&codebook, &VQImage](PNNKDTree<ColorTableBased4x4Pixels> &node)
        {
            for (auto &item : node.bucketItems())
            {
                const int i = static_cast<int>(codebook.size());
                for (const auto idx : item.m_origVectorIndices)
                {
                    VQImage[idx] = static_cast<short>(i);
                }
                codebook.push_back(std::move(item));
            }
        });

    // Add dummy entries until CODEBOOK_MAX_SIZE is reached. In theory we
    // could provide less code if we don't reach it, but for broader
    // compatibility it seems best to go up to the typical max value.
    // Furthermore when there is transparency, the CADRG spec mentions 4095
    // to be reserved for a transparent 4x4 kernel pointing to color table entry
    // 216.
    while (codebook.size() < CODEBOOK_MAX_SIZE)
    {
        codebook.emplace_back(
            Vector<ColorTableBased4x4Pixels>(
                filled_array<GByte,
                             Vector<ColorTableBased4x4Pixels>::PIX_COUNT>(0)),
            0, std::vector<int>());
    }

    if (bHasTransparentPixels)
    {
        codebook[TRANSPARENT_CODEBOOK_CODE]
            .m_vec = Vector<ColorTableBased4x4Pixels>(
            filled_array<GByte, Vector<ColorTableBased4x4Pixels>::PIX_COUNT>(
                static_cast<GByte>(TRANSPARENT_COLOR_TABLE_ENTRY)));
        codebook[TRANSPARENT_CODEBOOK_CODE].m_count = nTransparentPixels;
        codebook[TRANSPARENT_CODEBOOK_CODE].m_origVectorIndices.clear();
    }

    return true;
}

/************************************************************************/
/*                      RPFFrameCreateCADRG_TREs()                      */
/************************************************************************/

std::unique_ptr<CADRGInformation>
RPFFrameCreateCADRG_TREs(GDALOffsetPatcher::OffsetPatcher *offsetPatcher,
                         const std::string &osFilename, GDALDataset *poSrcDS,
                         CPLStringList &aosOptions,
                         const CADRGCreateCopyContext &copyContext)
{
    auto priv = std::make_unique<CADRGInformation::Private>();
    if (!Perform_CADRG_VQ_Compression(poSrcDS, priv->codebook, priv->VQImage,
                                      priv->bHasTransparentPixels))
    {
        return nullptr;
    }

    Create_CADRG_RPFHDR(offsetPatcher, osFilename, aosOptions);

    // Create buffers that will be written into file by RPFFrameWriteCADRG_RPFIMG()s
    Create_CADRG_LocationComponent(offsetPatcher);
    if (!Create_CADRG_CoverageSection(offsetPatcher, poSrcDS, osFilename,
                                      aosOptions, copyContext.nReciprocalScale))
        return nullptr;
    Create_CADRG_ColorGrayscaleSection(offsetPatcher);
    Create_CADRG_ColormapSection(offsetPatcher, poSrcDS,
                                 priv->bHasTransparentPixels, priv->codebook,
                                 copyContext);
    Create_CADRG_ColorConverterSubsection(
        offsetPatcher, priv->bHasTransparentPixels, copyContext);
    Create_CADRG_ImageDescriptionSection(offsetPatcher, poSrcDS,
                                         priv->bHasTransparentPixels);
    return std::make_unique<CADRGInformation>(std::move(priv));
}

/************************************************************************/
/*                     RPFFrameWriteCADRG_RPFIMG()                      */
/************************************************************************/

bool RPFFrameWriteCADRG_RPFIMG(GDALOffsetPatcher::OffsetPatcher *offsetPatcher,
                               VSILFILE *fp, int &nUDIDL)
{
    std::vector<GDALOffsetPatcher::OffsetPatcherBuffer *> apoBuffers;
    int nContentLength = 0;
    for (const char *pszName :
         {"LocationComponent", "CoverageSectionSubheader",
          "ColorGrayscaleSectionSubheader", "ColormapSubsection",
          "ColorConverterSubsection", "ImageDescriptionSubheader"})
    {
        const auto poBuffer = offsetPatcher->GetBufferFromName(pszName);
        CPLAssert(poBuffer);
        apoBuffers.push_back(poBuffer);
        nContentLength += static_cast<int>(poBuffer->GetBuffer().size());
    }

    CPLAssert(nContentLength <= 99999);
    constexpr const char *pszUDOFL = "000";
    const char *pszTREPrefix = CPLSPrintf("RPFIMG%05d", nContentLength);
    nUDIDL = static_cast<int>(strlen(pszUDOFL) + strlen(pszTREPrefix) +
                              nContentLength);

    // UDIDL
    bool bOK = fp->Write(CPLSPrintf("%05d", nUDIDL), 1, 5) == 5;

    // UDOFL
    bOK = bOK && fp->Write(pszUDOFL, 1, strlen(pszUDOFL)) == strlen(pszUDOFL);

    // UDID
    bOK = bOK && fp->Write(pszTREPrefix, 1, strlen(pszTREPrefix)) ==
                     strlen(pszTREPrefix);

    for (auto *poBuffer : apoBuffers)
    {
        poBuffer->DeclareBufferWrittenAtPosition(VSIFTellL(fp));
        bOK = bOK && VSIFWriteL(poBuffer->GetBuffer().data(), 1,
                                poBuffer->GetBuffer().size(),
                                fp) == poBuffer->GetBuffer().size();
    }

    return bOK;
}

/************************************************************************/
/*                     Write_CADRG_MaskSubsection()                     */
/************************************************************************/

static bool
Write_CADRG_MaskSubsection(GDALOffsetPatcher::OffsetPatcher *offsetPatcher,
                           VSILFILE *fp, GDALDataset *poSrcDS,
                           bool bHasTransparentPixels,
                           const std::vector<short> &VQImage)
{
    auto poBuffer = offsetPatcher->CreateBuffer(
        "MaskSubsection", /* bEndiannessIsLittle = */ false);
    CPLAssert(poBuffer);
    poBuffer->DeclareBufferWrittenAtPosition(fp->Tell());
    poBuffer->DeclareOffsetAtCurrentPosition("MASK_SUBSECTION_LOCATION");
    poBuffer->AppendUInt16(0);  // SUBFRAME_SEQUENCE_RECORD_LENGTH
    poBuffer->AppendUInt16(0);  // TRANSPARENCY_SEQUENCE_RECORD_LENGTH
    if (bHasTransparentPixels)
    {
        poBuffer->AppendUInt16(8);  // TRANSPARENT_OUTPUT_PIXEL_CODE_LENGTH
        // TRANSPARENT_OUTPUT_PIXEL_CODE
        poBuffer->AppendByte(static_cast<GByte>(TRANSPARENT_COLOR_TABLE_ENTRY));

        const int nWidth = poSrcDS->GetRasterXSize();
        const int nHeight = poSrcDS->GetRasterYSize();
        CPLAssert((nWidth % BLOCK_SIZE) == 0);
        CPLAssert((nHeight % BLOCK_SIZE) == 0);
        const int nSubFramesPerRow = nWidth / BLOCK_SIZE;
        const int nSubFramesPerCol = nHeight / BLOCK_SIZE;
        static_assert((BLOCK_SIZE % SUBSAMPLING) == 0);
        constexpr int SUBFRAME_YSIZE = BLOCK_SIZE / SUBSAMPLING;
        constexpr int SUBFRAME_XSIZE = BLOCK_SIZE / SUBSAMPLING;
        const int nPixelsPerRow = nSubFramesPerRow * SUBFRAME_XSIZE;
        constexpr GByte CODE_WORD_BIT_LENGTH = 12;
        static_assert((1 << CODE_WORD_BIT_LENGTH) == CODEBOOK_MAX_SIZE);
        static_assert(
            ((SUBFRAME_XSIZE * SUBFRAME_YSIZE * CODE_WORD_BIT_LENGTH) % 8) ==
            0);
        constexpr int SIZEOF_SUBFRAME_IN_BYTES =
            (SUBFRAME_XSIZE * SUBFRAME_YSIZE * CODE_WORD_BIT_LENGTH) / 8;

        CPLAssert(VQImage.size() == static_cast<size_t>(nSubFramesPerRow) *
                                        nSubFramesPerCol * SUBFRAME_YSIZE *
                                        SUBFRAME_XSIZE);

        for (int yBlock = 0, nIdxBlock = 0; yBlock < nSubFramesPerCol; ++yBlock)
        {
            int nOffsetBlock = yBlock * SUBFRAME_YSIZE * nPixelsPerRow;
            for (int xBlock = 0; xBlock < nSubFramesPerRow;
                 ++xBlock, nOffsetBlock += SUBFRAME_XSIZE, ++nIdxBlock)
            {
                bool bBlockHasTransparentPixels = false;
                for (int ySubBlock = 0; ySubBlock < SUBFRAME_YSIZE; ySubBlock++)
                {
                    int nOffset = nOffsetBlock + ySubBlock * nPixelsPerRow;
                    for (int xSubBlock = 0; xSubBlock < SUBFRAME_XSIZE;
                         ++xSubBlock, ++nOffset)
                    {
                        if (VQImage[nOffset] == TRANSPARENT_CODEBOOK_CODE)
                            bBlockHasTransparentPixels = true;
                    }
                }
                // Cf MIL-STD-2411 page 23
                if (!bBlockHasTransparentPixels)
                    poBuffer->AppendUInt32(UINT32_MAX);
                else
                    poBuffer->AppendUInt32(nIdxBlock *
                                           SIZEOF_SUBFRAME_IN_BYTES);
            }
        }
    }
    else
    {
        poBuffer->AppendUInt16(0);  // TRANSPARENT_OUTPUT_PIXEL_CODE_LENGTH
    }

    return fp->Write(poBuffer->GetBuffer().data(), 1,
                     poBuffer->GetBuffer().size()) ==
           poBuffer->GetBuffer().size();
}

/************************************************************************/
/*                   Write_CADRG_CompressionSection()                   */
/************************************************************************/

constexpr uint16_t NUMBER_OF_COMPRESSION_LOOKUP_OFFSET_RECORDS = 4;

static bool
Write_CADRG_CompressionSection(GDALOffsetPatcher::OffsetPatcher *offsetPatcher,
                               VSILFILE *fp)
{
    auto poBuffer = offsetPatcher->CreateBuffer(
        "CompressionSection", /* bEndiannessIsLittle = */ false);
    CPLAssert(poBuffer);
    poBuffer->DeclareBufferWrittenAtPosition(fp->Tell());
    poBuffer->DeclareOffsetAtCurrentPosition("COMPRESSION_SECTION_LOCATION");
    poBuffer->AppendUInt16(1);  // COMPRESSION_ALGORITHM_ID = VQ
    poBuffer->AppendUInt16(NUMBER_OF_COMPRESSION_LOOKUP_OFFSET_RECORDS);
    // NUMBER_OF_COMPRESSION_PARAMETER_OFFSET_RECORDS
    poBuffer->AppendUInt16(0);

    return fp->Write(poBuffer->GetBuffer().data(), 1,
                     poBuffer->GetBuffer().size()) ==
           poBuffer->GetBuffer().size();
}

/************************************************************************/
/*             Write_CADRG_ImageDisplayParametersSection()              */
/************************************************************************/

static bool Write_CADRG_ImageDisplayParametersSection(
    GDALOffsetPatcher::OffsetPatcher *offsetPatcher, VSILFILE *fp)
{
    auto poBuffer = offsetPatcher->CreateBuffer(
        "ImageDisplayParametersSubheader", /* bEndiannessIsLittle = */ false);
    CPLAssert(poBuffer);
    poBuffer->DeclareBufferWrittenAtPosition(fp->Tell());
    poBuffer->DeclareOffsetAtCurrentPosition(
        "IMAGE_DISPLAY_PARAMETERS_SECTION_LOCATION");
    poBuffer->AppendUInt32(BLOCK_SIZE / SUBSAMPLING);  // NUMBER_OF_IMAGE_ROWS
    poBuffer->AppendUInt32(BLOCK_SIZE /
                           SUBSAMPLING);  // NUMBER_OF_CODES_PER_ROW
    constexpr GByte CODE_WORD_BIT_LENGTH = 12;
    static_assert((1 << CODE_WORD_BIT_LENGTH) == CODEBOOK_MAX_SIZE);
    poBuffer->AppendByte(CODE_WORD_BIT_LENGTH);  // IMAGE_CODE_BIT_LENGTH

    return fp->Write(poBuffer->GetBuffer().data(), 1,
                     poBuffer->GetBuffer().size()) ==
           poBuffer->GetBuffer().size();
}

/************************************************************************/
/*              Write_CADRG_CompressionLookupSubSection()               */
/************************************************************************/

static bool Write_CADRG_CompressionLookupSubSection(
    GDALOffsetPatcher::OffsetPatcher *offsetPatcher, VSILFILE *fp,
    const std::vector<BucketItem<ColorTableBased4x4Pixels>> &codebook)
{
    auto poBuffer = offsetPatcher->CreateBuffer(
        "CompressionLookupSubsection", /* bEndiannessIsLittle = */ false);
    CPLAssert(poBuffer);
    poBuffer->DeclareBufferWrittenAtPosition(fp->Tell());
    poBuffer->DeclareOffsetAtCurrentPosition("COMPRESSION_LOOKUP_LOCATION");
    poBuffer->AppendUInt32(6);  // COMPRESSION_LOOKUP_OFFSET_TABLE_OFFSET
    // COMPRESSION_LOOKUP_TABLE_OFFSET_RECORD_LENGTH
    poBuffer->AppendUInt16(14);

    constexpr int OFFSET_OF_FIRST_LOOKUP_TABLE = 62;
    constexpr uint16_t NUMBER_OF_VALUES_PER_COMPRESSION_RECORDS =
        static_cast<uint16_t>(SUBSAMPLING);
    for (int i = 0; i < NUMBER_OF_COMPRESSION_LOOKUP_OFFSET_RECORDS; ++i)
    {
        // COMPRESSION_LOOKUP_TABLE_ID
        poBuffer->AppendUInt16(static_cast<uint16_t>(i + 1));
        poBuffer->AppendUInt32(
            CODEBOOK_MAX_SIZE);  // NUMBER_OF_COMPRESSION_LOOKUP_RECORDS
        poBuffer->AppendUInt16(NUMBER_OF_VALUES_PER_COMPRESSION_RECORDS);
        poBuffer->AppendUInt16(8);  // COMPRESSION_RECORD_VALUE_BIT_LENGTH
        poBuffer->AppendUInt32(OFFSET_OF_FIRST_LOOKUP_TABLE +
                               CODEBOOK_MAX_SIZE *
                                   NUMBER_OF_VALUES_PER_COMPRESSION_RECORDS *
                                   i);  // COMPRESSION_LOOKUP_TABLE_OFFSET
    }

    for (int row = 0; row < SUBSAMPLING; ++row)
    {
        int i = 0;
        for (; i < static_cast<int>(codebook.size()); ++i)
        {
            for (int j = 0; j < SUBSAMPLING; ++j)
            {
                poBuffer->AppendByte(
                    codebook[i].m_vec.val(row * SUBSAMPLING + j));
            }
        }
        for (; i < CODEBOOK_MAX_SIZE; ++i)
        {
            poBuffer->AppendUInt32(0);
        }
    }

    return fp->Write(poBuffer->GetBuffer().data(), 1,
                     poBuffer->GetBuffer().size()) ==
           poBuffer->GetBuffer().size();
}

/************************************************************************/
/*                 Write_CADRG_SpatialDataSubsection()                  */
/************************************************************************/

static bool Write_CADRG_SpatialDataSubsection(
    GDALOffsetPatcher::OffsetPatcher *offsetPatcher, VSILFILE *fp,
    GDALDataset *poSrcDS, const std::vector<short> &VQImage)
{
    auto poBuffer = offsetPatcher->CreateBuffer(
        "SpatialDataSubsection", /* bEndiannessIsLittle = */ false);
    CPLAssert(poBuffer);
    poBuffer->DeclareBufferWrittenAtPosition(fp->Tell());
    poBuffer->DeclareOffsetAtCurrentPosition(
        "SPATIAL_DATA_SUBSECTION_LOCATION");

    const int nWidth = poSrcDS->GetRasterXSize();
    const int nHeight = poSrcDS->GetRasterYSize();
    CPLAssert((nWidth % BLOCK_SIZE) == 0);
    CPLAssert((nHeight % BLOCK_SIZE) == 0);
    const int nSubFramesPerRow = nWidth / BLOCK_SIZE;
    const int nSubFramesPerCol = nHeight / BLOCK_SIZE;
    static_assert((BLOCK_SIZE % SUBSAMPLING) == 0);
    constexpr int SUBFRAME_YSIZE = BLOCK_SIZE / SUBSAMPLING;
    constexpr int SUBFRAME_XSIZE = BLOCK_SIZE / SUBSAMPLING;
    const int nPixelsPerRow = nSubFramesPerRow * SUBFRAME_XSIZE;
    for (int yBlock = 0; yBlock < nSubFramesPerCol; ++yBlock)
    {
        int nOffsetBlock = yBlock * SUBFRAME_YSIZE * nPixelsPerRow;
        for (int xBlock = 0; xBlock < nSubFramesPerRow;
             ++xBlock, nOffsetBlock += SUBFRAME_XSIZE)
        {
            for (int ySubBlock = 0; ySubBlock < SUBFRAME_YSIZE; ySubBlock++)
            {
                int nOffset = nOffsetBlock + ySubBlock * nPixelsPerRow;
                // Combine 2 codes of 12 bits each into 3 bytes
                // This is the reverse of function NITFUncompressVQTile()
                for (int xSubBlock = 0; xSubBlock < SUBFRAME_XSIZE;
                     xSubBlock += 2, nOffset += 2)
                {
                    const int v1 = VQImage[nOffset + 0];
                    const int v2 = VQImage[nOffset + 1];
                    poBuffer->AppendByte(static_cast<GByte>(v1 >> 4));
                    poBuffer->AppendByte(
                        static_cast<GByte>(((v1 & 0xF) << 4) | (v2 >> 8)));
                    poBuffer->AppendByte(static_cast<GByte>(v2 & 0xFF));
                }
            }
        }
    }

    return fp->Write(poBuffer->GetBuffer().data(), 1,
                     poBuffer->GetBuffer().size()) ==
           poBuffer->GetBuffer().size();
}

/************************************************************************/
/*                  RPFFrameWriteCADRG_ImageContent()                   */
/************************************************************************/

bool RPFFrameWriteCADRG_ImageContent(
    GDALOffsetPatcher::OffsetPatcher *offsetPatcher, VSILFILE *fp,
    GDALDataset *poSrcDS, CADRGInformation *info)
{
    return fp->Seek(0, SEEK_END) == 0 &&
           Write_CADRG_MaskSubsection(offsetPatcher, fp, poSrcDS,
                                      info->m_private->bHasTransparentPixels,
                                      info->m_private->VQImage) &&
           Write_CADRG_CompressionSection(offsetPatcher, fp) &&
           Write_CADRG_ImageDisplayParametersSection(offsetPatcher, fp) &&
           Write_CADRG_CompressionLookupSubSection(offsetPatcher, fp,
                                                   info->m_private->codebook) &&
           Write_CADRG_SpatialDataSubsection(offsetPatcher, fp, poSrcDS,
                                             info->m_private->VQImage);
}

/************************************************************************/
/*                             RPFAttribute                             */
/************************************************************************/

namespace
{
struct RPFAttribute
{
    uint16_t nAttrId = 0;
    uint8_t nParamId = 0;
    std::string osValue{};

    RPFAttribute(int nAttrIdIn, int nParamIdIn, const std::string &osValueIn)
        : nAttrId(static_cast<uint16_t>(nAttrIdIn)),
          nParamId(static_cast<uint8_t>(nParamIdIn)), osValue(osValueIn)
    {
    }
};
}  // namespace

/************************************************************************/
/*                     RPFFrameWriteGetDESHeader()                      */
/************************************************************************/

const char *RPFFrameWriteGetDESHeader()
{

    constexpr const char *pszDESHeader =
        "DE"                                           // Segment type
        "Registered Extensions    "                    // DESID
        "01"                                           // DESVER
        "U"                                            // DECLAS
        "  "                                           // DESCLSY
        "           "                                  // DESCODE
        "  "                                           // DESCTLH
        "                    "                         // DESREL
        "  "                                           // DESDCDT
        "        "                                     // DESDCDT
        "    "                                         // DESDCXM
        " "                                            // DESDG
        "        "                                     // DESDGDT
        "                                           "  // DESCLTX
        " "                                            // DESCATP
        "                                        "     // DESCAUT
        " "                                            // DESCRSN
        "        "                                     // DESSRDT
        "               "                              // DESCTLN
        "UDID  "                                       // DESOVFL
        "001"                                          // DESITEM
        "0000"                                         // DESSHL
        ;

    return pszDESHeader;
}

/************************************************************************/
/*                     RPFFrameWriteCADRG_RPFDES()                      */
/************************************************************************/

bool RPFFrameWriteCADRG_RPFDES(GDALOffsetPatcher::OffsetPatcher *offsetPatcher,
                               VSILFILE *fp, vsi_l_offset nOffsetLDSH,
                               const CPLStringList &aosOptions,
                               int nReciprocalScale)
{
    bool bOK = fp->Seek(0, SEEK_END) == 0;

    const char *pszDESHeader = RPFFrameWriteGetDESHeader();
    bOK &= fp->Write(pszDESHeader, 1, strlen(pszDESHeader)) ==
           strlen(pszDESHeader);

    std::string osDESData("RPFDES");
    std::string osDESDataPayload;
    const auto nPosAttributeSectionSubheader =
        fp->Tell() + osDESData.size() + strlen("XXXXX");

    auto poBufferASSH = offsetPatcher->CreateBuffer(
        "AttributeSectionSubheader", /* bEndiannessIsLittle = */ false);
    CPLAssert(poBufferASSH);
    poBufferASSH->DeclareBufferWrittenAtPosition(nPosAttributeSectionSubheader);
    poBufferASSH->DeclareOffsetAtCurrentPosition(
        "ATTRIBUTE_SECTION_SUBHEADER_LOCATION");

    std::vector<RPFAttribute> asAttributes;

    const auto GetYYYMMDDDate = [](const std::string &osValue) -> std::string
    {
        if (EQUAL(osValue.c_str(), "NOW"))
        {
            time_t unixTime;
            time(&unixTime);
            struct tm brokenDownTime;
            CPLUnixTimeToYMDHMS(unixTime, &brokenDownTime);
            return CPLString().Printf(
                "%04d%02d%02d", brokenDownTime.tm_year + 1900,
                brokenDownTime.tm_mon + 1, brokenDownTime.tm_mday);
        }
        else
        {
            return osValue;
        }
    };

    {
        const char *pszV =
            aosOptions.FetchNameValueDef("CURRENCY_DATE", "20260101");
        if (pszV && pszV[0])
        {
            asAttributes.emplace_back(1, 1,
                                      StrPadTruncate(GetYYYMMDDDate(pszV), 8));
        }
    }

    {
        const char *pszV =
            aosOptions.FetchNameValueDef("PRODUCTION_DATE", "20260101");
        if (pszV && pszV[0])
        {
            asAttributes.emplace_back(2, 1,
                                      StrPadTruncate(GetYYYMMDDDate(pszV), 8));
        }
    }

    {
        const char *pszV =
            aosOptions.FetchNameValueDef("SIGNIFICANT_DATE", "20260101");
        if (pszV && pszV[0])
        {
            asAttributes.emplace_back(3, 1,
                                      StrPadTruncate(GetYYYMMDDDate(pszV), 8));
        }
    }

    if (const char *pszV = aosOptions.FetchNameValue("DATA_SERIES_DESIGNATION"))
    {
        asAttributes.emplace_back(4, 1, StrPadTruncate(pszV, 10));
    }
    else if (const char *pszSeriesCode =
                 aosOptions.FetchNameValue("SERIES_CODE"))
    {
        const auto *psEntry = NITFGetRPFSeriesInfoFromCode(pszSeriesCode);
        if (psEntry && psEntry->abbreviation[0])
        {
            // If the data series abbreviation doesn't contain a scale indication,
            // add it.
            std::string osVal(psEntry->abbreviation);
            if (osVal.find('0') != std::string::npos)
            {
                if (nReciprocalScale >= Million)
                    osVal += CPLSPrintf(" %dM", nReciprocalScale / Million);
                else if (nReciprocalScale >= Kilo)
                    osVal += CPLSPrintf(" %dK", nReciprocalScale / Kilo);
            }

            asAttributes.emplace_back(4, 1, StrPadTruncate(osVal, 10));
        }
    }

    if (const char *pszV = aosOptions.FetchNameValue("MAP_DESIGNATION"))
    {
        asAttributes.emplace_back(4, 2, StrPadTruncate(pszV, 8));
    }

    // Horizontal datum code
    asAttributes.emplace_back(7, 1, StrPadTruncate("WGE", 4));

    const auto nAttrCount = static_cast<uint16_t>(asAttributes.size());
    poBufferASSH->AppendUInt16(nAttrCount);
    poBufferASSH->AppendUInt16(0);  // NUMBER_OF_EXPLICIT_AREAL_COVERAGE_RECORDS
    poBufferASSH->AppendUInt32(0);  // ATTRIBUTE_OFFSET_TABLE_OFFSET
    constexpr uint16_t ATTRIBUTE_OFFSET_RECORD_LENGTH =
        static_cast<uint16_t>(8);
    poBufferASSH->AppendUInt16(ATTRIBUTE_OFFSET_RECORD_LENGTH);

    osDESDataPayload.insert(
        osDESDataPayload.end(),
        reinterpret_cast<const char *>(poBufferASSH->GetBuffer().data()),
        reinterpret_cast<const char *>(poBufferASSH->GetBuffer().data() +
                                       poBufferASSH->GetBuffer().size()));

    auto poBufferAS = offsetPatcher->CreateBuffer(
        "AttributeSubsection", /* bEndiannessIsLittle = */ false);
    CPLAssert(poBufferAS);
    poBufferAS->DeclareBufferWrittenAtPosition(
        nPosAttributeSectionSubheader + poBufferASSH->GetBuffer().size());
    poBufferAS->DeclareOffsetAtCurrentPosition("ATTRIBUTE_SUBSECTION_LOCATION");

    size_t nAttrValueOffset =
        ATTRIBUTE_OFFSET_RECORD_LENGTH * asAttributes.size();

    // Attribute definitions
    for (const auto &sAttr : asAttributes)
    {
        poBufferAS->AppendUInt16(sAttr.nAttrId);
        poBufferAS->AppendByte(sAttr.nParamId);
        poBufferAS->AppendByte(0);  // Areal coverage sequence number
        poBufferAS->AppendUInt32(static_cast<uint32_t>(
            nAttrValueOffset));  // Attribute record offset
        nAttrValueOffset += sAttr.osValue.size();
    }

    // Attribute values
    for (const auto &sAttr : asAttributes)
    {
        poBufferAS->AppendString(sAttr.osValue);
    }

    osDESDataPayload.insert(
        osDESDataPayload.end(),
        reinterpret_cast<const char *>(poBufferAS->GetBuffer().data()),
        reinterpret_cast<const char *>(poBufferAS->GetBuffer().data() +
                                       poBufferAS->GetBuffer().size()));

    CPLAssert(osDESDataPayload.size() <= 99999U);
    osDESData += CPLSPrintf("%05d", static_cast<int>(osDESDataPayload.size()));
    osDESData += osDESDataPayload;
    bOK &=
        fp->Write(osDESData.c_str(), 1, osDESData.size()) == osDESData.size();

    // Update LDSH and LD in the NITF Header
    const int iDES = 0;
    bOK &= fp->Seek(nOffsetLDSH + iDES * 13, SEEK_SET) == 0;
    bOK &= fp->Write(CPLSPrintf("%04d", static_cast<int>(strlen(pszDESHeader))),
                     1, 4) == 4;
    bOK &= fp->Write(CPLSPrintf("%09d", static_cast<int>(osDESData.size())), 1,
                     9) == 9;

    return bOK;
}

/************************************************************************/
/*                      CADRGGetWarpedVRTDataset()                      */
/************************************************************************/

static std::unique_ptr<GDALDataset>
CADRGGetWarpedVRTDataset(GDALDataset *poSrcDS, int nZone, double dfXMin,
                         double dfYMin, double dfXMax, double dfYMax,
                         double dfResX, double dfResY,
                         const char *pszResampling)
{
    CPLStringList aosWarpArgs;
    aosWarpArgs.push_back("-of");
    aosWarpArgs.push_back("VRT");
    aosWarpArgs.push_back("-t_srs");
    if (nZone == MAX_ZONE_NORTHERN_HEMISPHERE)
        aosWarpArgs.push_back(pszNorthPolarProjection);
    else if (nZone == MAX_ZONE)
        aosWarpArgs.push_back(pszSouthPolarProjection);
    else
        aosWarpArgs.push_back("EPSG:4326");
    aosWarpArgs.push_back("-te");
    aosWarpArgs.push_back(CPLSPrintf("%.17g", dfXMin));
    aosWarpArgs.push_back(CPLSPrintf("%.17g", dfYMin));
    aosWarpArgs.push_back(CPLSPrintf("%.17g", dfXMax));
    aosWarpArgs.push_back(CPLSPrintf("%.17g", dfYMax));
    aosWarpArgs.push_back("-tr");
    aosWarpArgs.push_back(CPLSPrintf("%.17g", dfResX));
    aosWarpArgs.push_back(CPLSPrintf("%.17g", dfResY));
    if (poSrcDS->GetRasterBand(1)->GetColorTable())
    {
        aosWarpArgs.push_back("-dstnodata");
        aosWarpArgs.push_back(CPLSPrintf("%d", TRANSPARENT_COLOR_TABLE_ENTRY));
    }
    else
    {
        aosWarpArgs.push_back("-r");
        aosWarpArgs.push_back(CPLString(pszResampling).tolower());
        if (poSrcDS->GetRasterCount() == 3)
            aosWarpArgs.push_back("-dstalpha");
    }
    std::unique_ptr<GDALWarpAppOptions, decltype(&GDALWarpAppOptionsFree)>
        psWarpOptions(GDALWarpAppOptionsNew(aosWarpArgs.List(), nullptr),
                      GDALWarpAppOptionsFree);
    std::unique_ptr<GDALDataset> poWarpedDS;
    if (psWarpOptions)
    {
        GDALDatasetH hSrcDS = GDALDataset::ToHandle(poSrcDS);
        poWarpedDS.reset(GDALDataset::FromHandle(
            GDALWarp("", nullptr, 1, &hSrcDS, psWarpOptions.get(), nullptr)));
    }
    return poWarpedDS;
}

/************************************************************************/
/*                       CADRGGetClippedDataset()                       */
/************************************************************************/

static std::unique_ptr<GDALDataset>
CADRGGetClippedDataset(GDALDataset *poSrcDS, double dfXMin, double dfYMin,
                       double dfXMax, double dfYMax)
{
    CPLStringList aosTranslateArgs;
    aosTranslateArgs.push_back("-of");
    aosTranslateArgs.push_back("MEM");
    aosTranslateArgs.push_back("-projwin");
    aosTranslateArgs.push_back(CPLSPrintf("%.17g", dfXMin));
    aosTranslateArgs.push_back(CPLSPrintf("%.17g", dfYMax));
    aosTranslateArgs.push_back(CPLSPrintf("%.17g", dfXMax));
    aosTranslateArgs.push_back(CPLSPrintf("%.17g", dfYMin));
    std::unique_ptr<GDALTranslateOptions, decltype(&GDALTranslateOptionsFree)>
        psTranslateOptions(
            GDALTranslateOptionsNew(aosTranslateArgs.List(), nullptr),
            GDALTranslateOptionsFree);
    std::unique_ptr<GDALDataset> poClippedDS;
    if (psTranslateOptions)
    {
        poClippedDS.reset(GDALDataset::FromHandle(
            GDALTranslate("", GDALDataset::ToHandle(poSrcDS),
                          psTranslateOptions.get(), nullptr)));
    }
    return poClippedDS;
}

/************************************************************************/
/*                      CADRGGetPalettedDataset()                       */
/************************************************************************/

static std::unique_ptr<GDALDataset>
CADRGGetPalettedDataset(GDALDataset *poSrcDS, GDALColorTable *poCT,
                        int nColorQuantizationBits)
{
    CPLAssert(poSrcDS->GetRasterCount() == 3 || poSrcDS->GetRasterCount() == 4);
    auto poMemDrv = GetGDALDriverManager()->GetDriverByName("MEM");
    std::unique_ptr<GDALDataset> poPalettedDS(
        poMemDrv->Create("", poSrcDS->GetRasterXSize(),
                         poSrcDS->GetRasterYSize(), 1, GDT_UInt8, nullptr));
    if (poPalettedDS)
    {
        poPalettedDS->SetSpatialRef(poSrcDS->GetSpatialRef());
        GDALGeoTransform gt;
        if (poSrcDS->GetGeoTransform(gt) == CE_None)
            poPalettedDS->SetGeoTransform(gt);
        poPalettedDS->GetRasterBand(1)->SetColorTable(poCT);
        poPalettedDS->GetRasterBand(1)->SetNoDataValue(
            TRANSPARENT_COLOR_TABLE_ENTRY);
        GDALDitherRGB2PCTInternal(
            GDALRasterBand::ToHandle(poSrcDS->GetRasterBand(1)),
            GDALRasterBand::ToHandle(poSrcDS->GetRasterBand(2)),
            GDALRasterBand::ToHandle(poSrcDS->GetRasterBand(3)),
            GDALRasterBand::ToHandle(poPalettedDS->GetRasterBand(1)),
            GDALColorTable::ToHandle(poCT), nColorQuantizationBits, nullptr,
            /* dither = */ false, nullptr, nullptr);
    }
    return poPalettedDS;
}

/************************************************************************/
/*                            CADRG_RGB_Type                            */
/************************************************************************/

struct CADRG_RGB_Type
{
};

/************************************************************************/
/*                        Vector<CADRG_RGB_Type>                        */
/************************************************************************/

template <> class Vector<CADRG_RGB_Type>
{
  private:
    GByte m_R = 0;
    GByte m_G = 0;
    GByte m_B = 0;

    Vector() = default;

  public:
    explicit Vector(GByte R, GByte G, GByte B) : m_R(R), m_G(G), m_B(B)
    {
    }

    static constexpr int DIM_COUNT /* specialize */ = 3;

    static constexpr bool getReturnUInt8 /* specialize */ = true;

    static constexpr bool hasComputeFourSquaredDistances = false;

    inline int get(int i, const CADRG_RGB_Type &) const /* specialize */
    {
        return i == 0 ? m_R : i == 1 ? m_G : m_B;
    }

    inline GByte r() const
    {
        return m_R;
    }

    inline GByte g() const
    {
        return m_G;
    }

    inline GByte b() const
    {
        return m_B;
    }

    /************************************************************************/
    /*                          squared_distance()                          */
    /************************************************************************/

    int squared_distance(const Vector &other,
                         const CADRG_RGB_Type &) const /* specialize */
    {
        const int nSqDist1 = square(m_R - other.m_R);
        const int nSqDist2 = square(m_G - other.m_G);
        const int nSqDist3 = square(m_B - other.m_B);
        return nSqDist1 + nSqDist2 + nSqDist3;
    }

    /************************************************************************/
    /*                              centroid()                              */
    /************************************************************************/

    static Vector centroid(const Vector &a, int nA, const Vector &b, int nB,
                           const CADRG_RGB_Type &) /* specialize */
    {
        Vector res;
        res.m_R = static_cast<GByte>((static_cast<uint64_t>(a.m_R) * nA +
                                      static_cast<uint64_t>(b.m_R) * nB +
                                      (nA + nB) / 2) /
                                     (nA + nB));
        res.m_G = static_cast<GByte>((static_cast<uint64_t>(a.m_G) * nA +
                                      static_cast<uint64_t>(b.m_G) * nB +
                                      (nA + nB) / 2) /
                                     (nA + nB));
        res.m_B = static_cast<GByte>((static_cast<uint64_t>(a.m_B) * nA +
                                      static_cast<uint64_t>(b.m_B) * nB +
                                      (nA + nB) / 2) /
                                     (nA + nB));
        return res;
    }

    /************************************************************************/
    /*                           operator == ()                             */
    /************************************************************************/

    inline bool operator==(const Vector &other) const
    {
        return m_R == other.m_R && m_G == other.m_G && m_B == other.m_B;
    }

    /************************************************************************/
    /*                           operator < ()                              */
    /************************************************************************/

    // Purely arbitrary for the purpose of distinguishing a vector from
    // another one
    inline bool operator<(const Vector &other) const
    {
        const int nA = (m_R) | (m_G << 8) | (m_B << 16);
        const int nB = (m_R) | (other.m_G << 8) | (other.m_B << 16);
        return nA < nB;
    }
};

/************************************************************************/
/*                         ComputeColorTables()                         */
/************************************************************************/

static bool ComputeColorTables(GDALDataset *poSrcDS, GDALColorTable &oCT,
                               int nColorQuantizationBits,
                               GDALProgressFunc pfnProgress,
                               void *pProgressData,
                               CADRGCreateCopyContext &copyContext)

{
    std::vector<GUIntBig> anPixelCountPerColorTableEntry;
    if (poSrcDS->GetRasterCount() >= 3)
    {
        if (GDALComputeMedianCutPCT(
                GDALRasterBand::ToHandle(poSrcDS->GetRasterBand(1)),
                GDALRasterBand::ToHandle(poSrcDS->GetRasterBand(2)),
                GDALRasterBand::ToHandle(poSrcDS->GetRasterBand(3)), nullptr,
                nullptr, nullptr, nullptr, CADRG_MAX_COLOR_ENTRY_COUNT,
                nColorQuantizationBits, static_cast<GUIntBig *>(nullptr),
                GDALColorTable::ToHandle(&oCT), pfnProgress, pProgressData,
                &anPixelCountPerColorTableEntry) != CE_None)
        {
            return false;
        }
    }
    else
    {
        // Compute histogram of the source dataset
        const auto poCT = poSrcDS->GetRasterBand(1)->GetColorTable();
        CPLAssert(poCT);
        const int nColors =
            std::min(poCT->GetColorEntryCount(), CADRG_MAX_COLOR_ENTRY_COUNT);
        for (int i = 0; i < nColors; ++i)
        {
            oCT.SetColorEntry(i, poCT->GetColorEntry(i));
        }
        anPixelCountPerColorTableEntry.resize(nColors);
        const int nXSize = poSrcDS->GetRasterXSize();
        const int nYSize = poSrcDS->GetRasterYSize();
        std::vector<GByte> abyLine(nXSize);
        for (int iY = 0; iY < nYSize; ++iY)
        {
            if (poSrcDS->GetRasterBand(1)->RasterIO(
                    GF_Read, 0, iY, nXSize, 1, abyLine.data(), nXSize, 1,
                    GDT_UInt8, 0, 0, nullptr) != CE_None)
            {
                return false;
            }
            for (int iX = 0; iX < nXSize; ++iX)
            {
                const int nVal = abyLine[iX];
                if (nVal < nColors)
                    ++anPixelCountPerColorTableEntry[nVal];
            }
        }
    }

    std::vector<BucketItem<CADRG_RGB_Type>> vectors;
    const int nColors =
        std::min(oCT.GetColorEntryCount(), CADRG_MAX_COLOR_ENTRY_COUNT);
    CPLAssert(anPixelCountPerColorTableEntry.size() >=
              static_cast<size_t>(nColors));

    GUIntBig nTotalCount = 0;
    for (int i = 0; i < nColors; ++i)
    {
        nTotalCount += anPixelCountPerColorTableEntry[i];
    }

    // 3 for R,G,B
    std::map<std::array<GByte, 3>, std::vector<int>> oMapUniqueEntries;
    for (int i = 0; i < nColors; ++i)
    {
        const auto entry = oCT.GetColorEntry(i);
        if (entry->c4 && anPixelCountPerColorTableEntry[i])
        {
            const auto R = static_cast<GByte>(entry->c1);
            const auto G = static_cast<GByte>(entry->c2);
            const auto B = static_cast<GByte>(entry->c3);
            oMapUniqueEntries[std::array<GByte, 3>{R, G, B}].push_back(i);
        }
    }

    const int nUniqueColors = static_cast<int>(oMapUniqueEntries.size());
    for (auto &[RGB, indices] : oMapUniqueEntries)
    {
        uint64_t nThisEntryCount = 0;
        for (int idx : indices)
            nThisEntryCount += anPixelCountPerColorTableEntry[idx];

        // Rescale pixel counts for primary color table so that their sum
        // does not exceed INT_MAX, as this is the type of m_count in the
        // BucketItem class.
        const int nCountRescaled = std::max(
            1, static_cast<int>(
                   static_cast<double>(nThisEntryCount) /
                   static_cast<double>(std::max<GUIntBig>(1, nTotalCount)) *
                   (INT_MAX - nUniqueColors)));
        Vector<CADRG_RGB_Type> v(RGB[0], RGB[1], RGB[2]);
        vectors.emplace_back(v, nCountRescaled, std::move(indices));
    }

    // Create the KD-Tree
    PNNKDTree<CADRG_RGB_Type> kdtree;
    CADRG_RGB_Type ctxt;

    int nCodeCount = kdtree.insert(std::move(vectors), ctxt);

    // Compute 32 entry color table
    if (nCodeCount > CADRG_SECOND_CT_COUNT)
    {
        nCodeCount = kdtree.cluster(nCodeCount, CADRG_SECOND_CT_COUNT, ctxt);
        if (nCodeCount == 0)
            return false;
    }

    copyContext.oCT2 = GDALColorTable();
    copyContext.anMapCT1ToCT2.clear();
    copyContext.anMapCT1ToCT2.resize(CADRG_MAX_COLOR_ENTRY_COUNT);
    kdtree.iterateOverLeaves(
        [&oCT, &copyContext](PNNKDTree<CADRG_RGB_Type> &node)
        {
            CPL_IGNORE_RET_VAL(oCT);
            int i = copyContext.oCT2.GetColorEntryCount();
            for (auto &item : node.bucketItems())
            {
                for (const auto idx : item.m_origVectorIndices)
                {
#ifdef DEBUG_VERBOSE
                    const auto psSrcEntry = oCT.GetColorEntry(idx);
                    CPLDebugOnly(
                        "CADRG", "Second CT: %d: %d %d %d <-- %d: %d %d %d", i,
                        item.m_vec.r(), item.m_vec.g(), item.m_vec.b(), idx,
                        psSrcEntry->c1, psSrcEntry->c2, psSrcEntry->c3);
#endif
                    copyContext.anMapCT1ToCT2[idx] = i;
                }
                GDALColorEntry sEntry;
                sEntry.c1 = item.m_vec.r();
                sEntry.c2 = item.m_vec.g();
                sEntry.c3 = item.m_vec.b();
                sEntry.c4 = 255;
                copyContext.oCT2.SetColorEntry(i, &sEntry);
                ++i;
            }
        });

    // Compute 16 entry color table
    if (nCodeCount > CADRG_THIRD_CT_COUNT)
    {
        nCodeCount = kdtree.cluster(nCodeCount, CADRG_THIRD_CT_COUNT, ctxt);
        if (nCodeCount == 0)
            return false;
    }

    copyContext.oCT3 = GDALColorTable();
    copyContext.anMapCT1ToCT3.clear();
    copyContext.anMapCT1ToCT3.resize(CADRG_MAX_COLOR_ENTRY_COUNT);
    kdtree.iterateOverLeaves(
        [&oCT, &copyContext](PNNKDTree<CADRG_RGB_Type> &node)
        {
            CPL_IGNORE_RET_VAL(oCT);
            int i = copyContext.oCT3.GetColorEntryCount();
            for (auto &item : node.bucketItems())
            {
                for (const auto idx : item.m_origVectorIndices)
                {
#ifdef DEBUG_VERBOSE
                    const auto psSrcEntry = oCT.GetColorEntry(idx);
                    CPLDebugOnly(
                        "CADRG", "Third CT: %d: %d %d %d <-- %d: %d %d %d", i,
                        item.m_vec.r(), item.m_vec.g(), item.m_vec.b(), idx,
                        psSrcEntry->c1, psSrcEntry->c2, psSrcEntry->c3);
#endif
                    copyContext.anMapCT1ToCT3[idx] = i;
                }
                GDALColorEntry sEntry;
                sEntry.c1 = item.m_vec.r();
                sEntry.c2 = item.m_vec.g();
                sEntry.c3 = item.m_vec.b();
                sEntry.c4 = 255;
                copyContext.oCT3.SetColorEntry(i, &sEntry);
                ++i;
            }
        });

    // Add transparency entry to primary color table
    GDALColorEntry sEntry = {0, 0, 0, 0};
    oCT.SetColorEntry(TRANSPARENT_COLOR_TABLE_ENTRY, &sEntry);

    return true;
}

/************************************************************************/
/*                          CADRGCreateCopy()                           */
/************************************************************************/

#ifndef NITFDUMP_BUILD
std::variant<bool, std::unique_ptr<GDALDataset>>
CADRGCreateCopy(const char *pszFilename, GDALDataset *poSrcDS, int bStrict,
                CSLConstList papszOptions, GDALProgressFunc pfnProgress,
                void *pProgressData, int nRecLevel,
                CADRGCreateCopyContext *copyContext)
{
    int &nReciprocalScale = copyContext->nReciprocalScale;

    if (poSrcDS->GetRasterBand(1)->GetRasterDataType() != GDT_UInt8)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "CADRG only supports datasets of UInt8 data type");
        return false;
    }

    const auto poSrcSRS = poSrcDS->GetSpatialRef();
    if (!poSrcSRS)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "CADRG only supports datasets with a CRS");
        return false;
    }

    GDALGeoTransform srcGT;
    if (poSrcDS->GetGeoTransform(srcGT) != CE_None)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "CADRG only supports datasets with a geotransform");
        return false;
    }

    double dfDPIOverride = 0;
    const char *pszDPI = CSLFetchNameValue(papszOptions, "DPI");
    if (pszDPI)
    {
        dfDPIOverride = CPLAtof(pszDPI);
        if (!(dfDPIOverride >= 1 && dfDPIOverride <= 7200))
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Invalid value for DPI: %s",
                     pszDPI);
            return false;
        }
    }

    const char *pszSeriesCode =
        CSLFetchNameValueDef(papszOptions, "SERIES_CODE", "MM");

    const std::string osResampling =
        CSLFetchNameValueDef(papszOptions, "RESAMPLING", "CUBIC");

    const char *pszScale = CSLFetchNameValue(papszOptions, "SCALE");
    if (pszScale)
    {
        if (EQUAL(pszScale, "GUESS"))
        {
            bool bGotDPI = false;
            nReciprocalScale = RPFGetCADRGClosestReciprocalScale(
                poSrcDS, dfDPIOverride, bGotDPI);
            if (nReciprocalScale <= 0)
            {
                // Error message emitted by RPFGetCADRGClosestReciprocalScale()
                return false;
            }
            else
            {
                CPLDebug("CADRG", "Guessed reciprocal scale: %d",
                         nReciprocalScale);
            }
        }
        else
        {
            nReciprocalScale = atoi(pszScale);
            if (!(nReciprocalScale >= Kilo && nReciprocalScale <= 20 * Million))
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Invalid value for SCALE: %s", pszScale);
                return false;
            }
        }

        const int nTheoreticalScale =
            RPFCADRGGetScaleFromDataSeriesCode(pszSeriesCode);
        if (nTheoreticalScale != 0 && nTheoreticalScale != nReciprocalScale &&
            nRecLevel == 0)
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Theoretical reciprocal scale from data series code %s is "
                     "%d, whereas SCALE has been specified to %d",
                     pszSeriesCode, nTheoreticalScale, nReciprocalScale);
        }
    }
    else
    {
        nReciprocalScale = RPFCADRGGetScaleFromDataSeriesCode(pszSeriesCode);
    }

    if (pszDPI && !(pszScale && EQUAL(pszScale, "GUESS")))
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "DPI is ignored when SCALE is not set to GUESS");
    }

    const CPLString osExtUpperCase(
        CPLString(CPLGetExtensionSafe(pszFilename)).toupper());
    bool bLooksLikeCADRGFilename = (osExtUpperCase == "NTF");
    if (!bLooksLikeCADRGFilename && osExtUpperCase.size() == 3 &&
        RPFCADRGZoneCharToNum(osExtUpperCase[2]) > 0)
    {
        bLooksLikeCADRGFilename =
            RPFCADRGIsKnownDataSeriesCode(osExtUpperCase.substr(0, 2).c_str());
    }

    const bool bColorTablePerFrame = CPLTestBool(
        CSLFetchNameValueDef(papszOptions, "COLOR_TABLE_PER_FRAME", "NO"));

    if (!(poSrcDS->GetRasterXSize() == CADRG_FRAME_PIXEL_COUNT &&
          poSrcDS->GetRasterYSize() == CADRG_FRAME_PIXEL_COUNT))
    {
        VSIStatBufL sStat;
        if (VSIStatL(pszFilename, &sStat) == 0)
        {
            if (VSI_ISREG(sStat.st_mode))
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Given that source dataset dimension do not match "
                         "a %dx%d frame, several frames will be generated "
                         "and thus the output filename should be a "
                         "directory name",
                         CADRG_FRAME_PIXEL_COUNT, CADRG_FRAME_PIXEL_COUNT);
                return false;
            }
        }
        else
        {
            if (bLooksLikeCADRGFilename)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Given that source dataset dimension do not match "
                         "a %dx%d frame, several frames will be "
                         "generated and thus the output filename "
                         "should be a directory name (without a NITF or "
                         "CADRG file extension)",
                         CADRG_FRAME_PIXEL_COUNT, CADRG_FRAME_PIXEL_COUNT);
                return false;
            }
        }
    }

    const auto poCT = poSrcDS->GetRasterBand(1)->GetColorTable();
    if (poCT && poCT->GetColorEntryCount() > CADRG_MAX_COLOR_ENTRY_COUNT)
    {
        for (int i = CADRG_MAX_COLOR_ENTRY_COUNT;
             i < poCT->GetColorEntryCount(); ++i)
        {
            const auto psEntry = poCT->GetColorEntry(i);
            if (psEntry->c1 != 0 || psEntry->c2 != 0 || psEntry->c3 != 0)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "CADRG only supports up to %d entries in color table "
                         "(and an extra optional transparent one)",
                         CADRG_MAX_COLOR_ENTRY_COUNT);
                return false;
            }
        }
    }

    GDALColorTable oCT;
    double dfLastPct = 0;

    constexpr int DEFAULT_QUANTIZATION_BITS = 5;
    const char *pszBits =
        CSLFetchNameValue(papszOptions, "COLOR_QUANTIZATION_BITS");
    int nColorQuantizationBits = DEFAULT_QUANTIZATION_BITS;
    if (pszBits)
    {
        nColorQuantizationBits = atoi(pszBits);
        if (nColorQuantizationBits < 5 || nColorQuantizationBits > 8)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "COLOR_QUANTIZATION_BITS value must be between 5 and 8");
            return false;
        }
    }

    class NITFDummyDataset final : public GDALDataset
    {
      public:
        NITFDummyDataset() = default;
    };

    const bool bInputIsShapedAndNameForCADRG =
        poSrcDS->GetRasterXSize() == CADRG_FRAME_PIXEL_COUNT &&
        poSrcDS->GetRasterYSize() == CADRG_FRAME_PIXEL_COUNT &&
        bLooksLikeCADRGFilename;

    // Check if it is a whole blank frame, and do not generate it.
    if (bInputIsShapedAndNameForCADRG)
    {
        bool bIsBlank = false;

        if (poSrcDS->GetRasterCount() == 4)
        {
            std::vector<GByte> abyData;
            if (poSrcDS->GetRasterBand(4)->ReadRaster(abyData) != CE_None)
                return false;

            bIsBlank = GDALBufferHasOnlyNoData(
                abyData.data(), 0, poSrcDS->GetRasterXSize(),
                poSrcDS->GetRasterYSize(),
                /* stride = */ poSrcDS->GetRasterXSize(),
                /* nComponents = */ 1,
                /* nBitsPerSample = */ 8, GSF_UNSIGNED_INT);
        }
        else if (poSrcDS->GetRasterCount() == 3)
        {
            std::vector<GByte> abyData;
            try
            {
                abyData.resize(poSrcDS->GetRasterCount() *
                               poSrcDS->GetRasterXSize() *
                               poSrcDS->GetRasterYSize());
            }
            catch (const std::exception &)
            {
                CPLError(CE_Failure, CPLE_OutOfMemory,
                         "Out of memory when allocating temporary buffer");
                return false;
            }
            if (poSrcDS->RasterIO(
                    GF_Read, 0, 0, poSrcDS->GetRasterXSize(),
                    poSrcDS->GetRasterYSize(), abyData.data(),
                    poSrcDS->GetRasterXSize(), poSrcDS->GetRasterYSize(),
                    GDT_UInt8, poSrcDS->GetRasterCount(), nullptr,
                    poSrcDS->GetRasterCount(),
                    poSrcDS->GetRasterXSize() * poSrcDS->GetRasterCount(), 1,
                    nullptr) != CE_None)
            {
                return false;
            }

            // Both (0,0,0) or (255,255,255) are considered blank
            bIsBlank = GDALBufferHasOnlyNoData(
                           abyData.data(), 0, poSrcDS->GetRasterXSize(),
                           poSrcDS->GetRasterYSize(),
                           /* stride = */ poSrcDS->GetRasterXSize(),
                           /* nComponents = */ poSrcDS->GetRasterCount(),
                           /* nBitsPerSample = */ 8, GSF_UNSIGNED_INT) ||
                       GDALBufferHasOnlyNoData(
                           abyData.data(), 255, poSrcDS->GetRasterXSize(),
                           poSrcDS->GetRasterYSize(),
                           /* stride = */ poSrcDS->GetRasterXSize(),
                           /* nComponents = */ poSrcDS->GetRasterCount(),
                           /* nBitsPerSample = */ 8, GSF_UNSIGNED_INT);
        }
        else if (poCT &&
                 poCT->GetColorEntryCount() > CADRG_MAX_COLOR_ENTRY_COUNT)
        {
            std::vector<GByte> abyData;
            if (poSrcDS->GetRasterBand(1)->ReadRaster(abyData) != CE_None)
                return false;

            bIsBlank = GDALBufferHasOnlyNoData(
                abyData.data(), TRANSPARENT_COLOR_TABLE_ENTRY,
                poSrcDS->GetRasterXSize(), poSrcDS->GetRasterYSize(),
                /* stride = */ poSrcDS->GetRasterXSize(),
                /* nComponents = */ 1,
                /* nBitsPerSample = */ 8, GSF_UNSIGNED_INT);
        }

        if (bIsBlank)
        {
            CPLDebug("CADRG", "Skipping generation of %s as it is blank",
                     pszFilename);
            return std::make_unique<NITFDummyDataset>();
        }
    }

    if (poSrcDS->GetRasterCount() == 1)
    {
        if (!poCT)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "CADRG only supports single band input datasets that "
                     "have an associated color table");
            return false;
        }
        if (copyContext->oCT2.GetColorEntryCount() == 0)
        {
            // First time we go through this code path, we need to compute
            // second and third color table from primary one.
            dfLastPct = 0.1;
            std::unique_ptr<void, decltype(&GDALDestroyScaledProgress)>
                pScaledData(GDALCreateScaledProgress(0, dfLastPct, pfnProgress,
                                                     pProgressData),
                            GDALDestroyScaledProgress);
            if (!ComputeColorTables(poSrcDS, oCT, nColorQuantizationBits,
                                    GDALScaledProgress, pScaledData.get(),
                                    *(copyContext)))
            {
                return false;
            }
        }
    }
    else if (poSrcDS->GetRasterCount() >= 3 &&
             poSrcDS->GetRasterBand(1)->GetColorInterpretation() ==
                 GCI_RedBand &&
             poSrcDS->GetRasterBand(2)->GetColorInterpretation() ==
                 GCI_GreenBand &&
             poSrcDS->GetRasterBand(3)->GetColorInterpretation() ==
                 GCI_BlueBand &&
             (poSrcDS->GetRasterCount() == 3 ||
              (poSrcDS->GetRasterCount() == 4 &&
               poSrcDS->GetRasterBand(4)->GetColorInterpretation() ==
                   GCI_AlphaBand)))
    {
        if (!bColorTablePerFrame || bInputIsShapedAndNameForCADRG)
        {
            dfLastPct = 0.1;
            std::unique_ptr<void, decltype(&GDALDestroyScaledProgress)>
                pScaledData(GDALCreateScaledProgress(0, dfLastPct, pfnProgress,
                                                     pProgressData),
                            GDALDestroyScaledProgress);
            if (!ComputeColorTables(poSrcDS, oCT, nColorQuantizationBits,
                                    GDALScaledProgress, pScaledData.get(),
                                    *(copyContext)))
            {
                return false;
            }
        }
    }
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "CADRG only supports single-band paletted, RGB or RGBA "
                 "input datasets");
        return false;
    }

    if (bInputIsShapedAndNameForCADRG)
    {
        if (!poCT)
        {
            auto poPalettedDS =
                CADRGGetPalettedDataset(poSrcDS, &oCT, nColorQuantizationBits);
            if (!poPalettedDS)
                return false;
            std::unique_ptr<void, decltype(&GDALDestroyScaledProgress)>
                pScaledData(GDALCreateScaledProgress(
                                dfLastPct, 1.0, pfnProgress, pProgressData),
                            GDALDestroyScaledProgress);
            return NITFDataset::CreateCopy(
                pszFilename, poPalettedDS.get(), bStrict, papszOptions,
                GDALScaledProgress, pScaledData.get(), nRecLevel + 1,
                copyContext);
        }

        return true;
    }
    else
    {
        if (nReciprocalScale == 0)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "SCALE must be defined");
            return nullptr;
        }

        const char *pszProducerCodeId =
            CSLFetchNameValueDef(papszOptions, "PRODUCER_CODE_ID", "0");
        const char *pszVersionNumber =
            CSLFetchNameValueDef(papszOptions, "VERSION_NUMBER", "01");

        OGREnvelope sExtentNativeCRS;
        OGREnvelope sExtentWGS84;
        if (poSrcDS->GetExtent(&sExtentNativeCRS) != CE_None ||
            poSrcDS->GetExtentWGS84LongLat(&sExtentWGS84) != CE_None)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Cannot get dataset extent");
            return false;
        }

        const char *pszZone = CSLFetchNameValue(papszOptions, "ZONE");
        const int nZoneHintCandidate =
            pszZone && strlen(pszZone) == 1 ? RPFCADRGZoneCharToNum(pszZone[0])
            : pszZone && strlen(pszZone) == 2 ? atoi(pszZone)
                                              : 0;
        const int nZoneHint =
            RPFCADRGIsValidZone(nZoneHintCandidate) ? nZoneHintCandidate : 0;
        auto frameDefinitions =
            RPFGetCADRGFramesForEnvelope(nZoneHint, nReciprocalScale, poSrcDS);

        if (nZoneHint)
        {
            // Only/mostly for debugging
            const char *pszFrameX = CSLFetchNameValue(papszOptions, "FRAME_X");
            const char *pszFrameY = CSLFetchNameValue(papszOptions, "FRAME_Y");
            if (pszFrameX && pszFrameY)
            {
                RPFFrameDef frameDef;
                frameDef.nReciprocalScale = nReciprocalScale;
                frameDef.nZone = nZoneHint;
                frameDef.nFrameMinX = atoi(pszFrameX);
                frameDef.nFrameMinY = atoi(pszFrameY);
                frameDef.nFrameMaxX = atoi(pszFrameX);
                frameDef.nFrameMaxY = atoi(pszFrameY);
                frameDef.dfResX = GetXPixelSize(nZoneHint, nReciprocalScale);
                frameDef.dfResY = GetYPixelSize(nZoneHint, nReciprocalScale);

                frameDefinitions.clear();
                frameDefinitions.push_back(std::move(frameDef));
            }
        }

        if (frameDefinitions.empty())
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cannot establish CADRG frames intersecting dataset "
                     "extent");
            return false;
        }
        else if (poSrcDS->GetRasterXSize() == CADRG_FRAME_PIXEL_COUNT &&
                 poSrcDS->GetRasterYSize() == CADRG_FRAME_PIXEL_COUNT &&
                 frameDefinitions.size() == 1 &&
                 frameDefinitions[0].nFrameMinX ==
                     frameDefinitions[0].nFrameMaxX &&
                 frameDefinitions[0].nFrameMinY ==
                     frameDefinitions[0].nFrameMaxY)
        {
            // poSrcDS already properly aligned
            if (poCT && CPLGetExtensionSafe(pszFilename).size() == 3)
                return true;

            std::string osFilename(pszFilename);
            if (!bLooksLikeCADRGFilename)
            {
                const int nZone = frameDefinitions[0].nZone;
                VSIMkdir(pszFilename, 0755);
                const std::string osRPFDir =
                    CPLFormFilenameSafe(pszFilename, "RPF", nullptr);
                VSIMkdir(osRPFDir.c_str(), 0755);
                const std::string osZoneDir = CPLFormFilenameSafe(
                    osRPFDir.c_str(),
                    CPLSPrintf("ZONE%c", RPFCADRGZoneNumToChar(nZone)),
                    nullptr);
                VSIMkdir(osZoneDir.c_str(), 0755);
                VSIStatBufL sStat;
                if (VSIStatL(osZoneDir.c_str(), &sStat) != 0 ||
                    !VSI_ISDIR(sStat.st_mode))
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Cannot create directory %s", osZoneDir.c_str());
                    return false;
                }
                osFilename = CPLFormFilenameSafe(
                    osZoneDir.c_str(),
                    RPFGetCADRGFrameNumberAsString(
                        nZone, nReciprocalScale, frameDefinitions[0].nFrameMinX,
                        frameDefinitions[0].nFrameMinY)
                        .c_str(),
                    nullptr);
                osFilename += pszVersionNumber;
                osFilename += pszProducerCodeId;
                osFilename += '.';
                osFilename += pszSeriesCode;
                osFilename += RPFCADRGZoneNumToChar(nZone);
                CPLDebug("CADRG", "Creating file %s", osFilename.c_str());
            }

            if (bColorTablePerFrame)
            {
                return NITFDataset::CreateCopy(
                    osFilename.c_str(), poSrcDS, bStrict, papszOptions,
                    pfnProgress, pProgressData, nRecLevel + 1, copyContext);
            }
            else if (poCT)
            {
                return NITFDataset::CreateCopy(
                    osFilename.c_str(), poSrcDS, bStrict, papszOptions,
                    pfnProgress, pProgressData, nRecLevel + 1, copyContext);
            }
            else
            {
                auto poPalettedDS = CADRGGetPalettedDataset(
                    poSrcDS, &oCT, nColorQuantizationBits);
                if (!poPalettedDS)
                    return false;
                return NITFDataset::CreateCopy(
                    osFilename.c_str(), poPalettedDS.get(), bStrict,
                    papszOptions, pfnProgress, pProgressData, nRecLevel + 1,
                    copyContext);
            }
        }
        else
        {
            int nTotalFrameCount = 0;
            for (const auto &frameDef : frameDefinitions)
            {
                nTotalFrameCount +=
                    (frameDef.nFrameMaxX - frameDef.nFrameMinX + 1) *
                    (frameDef.nFrameMaxY - frameDef.nFrameMinY + 1);
            }
            CPLDebug("CADRG", "%d frame(s) to generate", nTotalFrameCount);

            int nCurFrameCounter = 0;

            VSIMkdir(pszFilename, 0755);
            const std::string osRPFDir =
                CPLFormFilenameSafe(pszFilename, "RPF", nullptr);
            VSIMkdir(osRPFDir.c_str(), 0755);

            bool bMissingFramesFound = false;

            for (const auto &frameDef : frameDefinitions)
            {
                double dfXMin = 0, dfYMin = 0, dfXMax = 0, dfYMax = 0;
                double dfTmpX, dfTmpY;
                RPFGetCADRGFrameExtent(frameDef.nZone,
                                       frameDef.nReciprocalScale,
                                       frameDef.nFrameMinX, frameDef.nFrameMinY,
                                       dfXMin, dfYMin, dfTmpX, dfTmpY);
                RPFGetCADRGFrameExtent(frameDef.nZone,
                                       frameDef.nReciprocalScale,
                                       frameDef.nFrameMaxX, frameDef.nFrameMaxY,
                                       dfTmpX, dfTmpY, dfXMax, dfYMax);

                CPLDebugOnly("CADRG", "Zone %d extent: %f,%f,%f,%f",
                             frameDef.nZone, dfXMin, dfYMin, dfXMax, dfYMax);

                auto poWarpedDS = CADRGGetWarpedVRTDataset(
                    poSrcDS, frameDef.nZone, dfXMin, dfYMin, dfXMax, dfYMax,
                    frameDef.dfResX, frameDef.dfResY, osResampling.c_str());
                if (!poWarpedDS)
                    return false;

                if ((poWarpedDS->GetRasterXSize() % CADRG_FRAME_PIXEL_COUNT) !=
                        0 ||
                    (poWarpedDS->GetRasterYSize() % CADRG_FRAME_PIXEL_COUNT) !=
                        0)
                {
                    // Should not happen unless there's a bug in our code
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Warped dataset dimension is %dx%d, which is "
                             "not a multiple of %dx%d",
                             poWarpedDS->GetRasterXSize(),
                             poWarpedDS->GetRasterYSize(),
                             CADRG_FRAME_PIXEL_COUNT, CADRG_FRAME_PIXEL_COUNT);
                    return false;
                }

                const std::string osZoneDir = CPLFormFilenameSafe(
                    osRPFDir.c_str(),
                    CPLSPrintf("ZONE%c", RPFCADRGZoneNumToChar(frameDef.nZone)),
                    nullptr);
                VSIStatBufL sStat;
                const bool bDirectoryAlreadyExists =
                    (VSIStatL(osZoneDir.c_str(), &sStat) == 0);
                if (!bDirectoryAlreadyExists)
                {
                    VSIMkdir(osZoneDir.c_str(), 0755);
                    if (VSIStatL(osZoneDir.c_str(), &sStat) != 0 ||
                        !VSI_ISDIR(sStat.st_mode))
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "Cannot create directory %s",
                                 osZoneDir.c_str());
                        return false;
                    }
                }

                CPLStringList aosOptions(papszOptions);
                aosOptions.SetNameValue("ZONE",
                                        CPLSPrintf("%d", frameDef.nZone));

                int nFrameCountThisZone =
                    (frameDef.nFrameMaxY - frameDef.nFrameMinY + 1) *
                    (frameDef.nFrameMaxX - frameDef.nFrameMinX + 1);

#ifdef __COVERITY__
                // Coverity has false positives about lambda captures by references
                // being done without the lock being taken
                CPL_IGNORE_RET_VAL(nFrameCountThisZone);
#else
                // Limit to 4 as it doesn't scale beyond
                const int nNumThreads =
                    std::min(GDALGetNumThreads(/* nMaxThreads = */ 4,
                                               /* bDefaultToAllCPUs = */ true),
                             nFrameCountThisZone);

                CPLJobQueuePtr jobQueue;
                std::unique_ptr<CPLWorkerThreadPool> poTP;
                if (nNumThreads > 1)
                {
                    poTP = std::make_unique<CPLWorkerThreadPool>();
                    if (poTP->Setup(nNumThreads, nullptr, nullptr))
                    {
                        jobQueue = poTP->CreateJobQueue();
                    }
                }
#endif
                std::mutex oMutex;
                bool bError = false;
                bool bErrorLocal = false;
                int nCurFrameThisZone = 0;
                std::string osErrorMsg;
                nFrameCountThisZone = 0;
                int nNonEmptyFrameCountThisZone = 0;

                std::unique_ptr<OGRCoordinateTransformation> poReprojCTToSrcSRS;
                OGRSpatialReference oSRS;
                if (frameDef.nZone == MAX_ZONE_NORTHERN_HEMISPHERE)
                    oSRS.importFromWkt(pszNorthPolarProjection);
                else if (frameDef.nZone == MAX_ZONE)
                    oSRS.importFromWkt(pszSouthPolarProjection);
                else
                    oSRS.importFromEPSG(4326);
                oSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
                if (!poSrcSRS->IsSame(&oSRS) ||
                    frameDef.nZone == MAX_ZONE_NORTHERN_HEMISPHERE ||
                    frameDef.nZone == MAX_ZONE)
                {
                    poReprojCTToSrcSRS.reset(
                        OGRCreateCoordinateTransformation(&oSRS, poSrcSRS));
                }

                for (int iY = frameDef.nFrameMinY;
                     !bErrorLocal && iY <= frameDef.nFrameMaxY; ++iY)
                {
                    for (int iX = frameDef.nFrameMinX;
                         !bErrorLocal && iX <= frameDef.nFrameMaxX; ++iX)
                    {
                        std::string osFilename = CPLFormFilenameSafe(
                            osZoneDir.c_str(),
                            RPFGetCADRGFrameNumberAsString(
                                frameDef.nZone, nReciprocalScale, iX, iY)
                                .c_str(),
                            nullptr);
                        osFilename += pszVersionNumber;
                        osFilename += pszProducerCodeId;
                        osFilename += '.';
                        osFilename += pszSeriesCode;
                        osFilename += RPFCADRGZoneNumToChar(frameDef.nZone);

                        double dfFrameMinX = 0;
                        double dfFrameMinY = 0;
                        double dfFrameMaxX = 0;
                        double dfFrameMaxY = 0;
                        RPFGetCADRGFrameExtent(
                            frameDef.nZone, frameDef.nReciprocalScale, iX, iY,
                            dfFrameMinX, dfFrameMinY, dfFrameMaxX, dfFrameMaxY);

                        bool bFrameFullyInSrcDS;
                        if (poReprojCTToSrcSRS)
                        {
                            OGREnvelope sFrameExtentInSrcSRS;
                            poReprojCTToSrcSRS->TransformBounds(
                                dfFrameMinX, dfFrameMinY, dfFrameMaxX,
                                dfFrameMaxY, &sFrameExtentInSrcSRS.MinX,
                                &sFrameExtentInSrcSRS.MinY,
                                &sFrameExtentInSrcSRS.MaxX,
                                &sFrameExtentInSrcSRS.MaxY,
                                DEFAULT_DENSIFY_PTS);

                            if (!sFrameExtentInSrcSRS.Intersects(
                                    sExtentNativeCRS))
                            {
                                CPLDebug(
                                    "CADRG",
                                    "Skipping creation of file %s as it does "
                                    "not intersect source raster extent",
                                    osFilename.c_str());
                                continue;
                            }
                            bFrameFullyInSrcDS =
                                sExtentNativeCRS.Contains(sFrameExtentInSrcSRS);
                        }
                        else
                        {
                            bFrameFullyInSrcDS =
                                dfFrameMinX >= sExtentWGS84.MinX &&
                                dfFrameMinY >= sExtentWGS84.MinY &&
                                dfFrameMaxX <= sExtentWGS84.MaxX &&
                                dfFrameMaxY <= sExtentWGS84.MaxY;
                        }

                        ++nFrameCountThisZone;

                        const auto task =
                            [osFilename = std::move(osFilename), copyContext,
                             dfFrameMinX, dfFrameMinY, dfFrameMaxX, dfFrameMaxY,
                             bFrameFullyInSrcDS, poCT, nRecLevel,
                             nColorQuantizationBits, bStrict,
                             bColorTablePerFrame, &oMutex, &poWarpedDS,
                             &nCurFrameThisZone, &nCurFrameCounter, &bError,
                             &oCT, &aosOptions, &bMissingFramesFound,
                             &osErrorMsg, &nNonEmptyFrameCountThisZone]()
                        {
#ifdef __COVERITY__
#define LOCK()                                                                 \
    do                                                                         \
    {                                                                          \
        (void)oMutex;                                                          \
    } while (0)
#else
#define LOCK() std::lock_guard oLock(oMutex)
#endif
                            {
                                LOCK();
                                if (bError)
                                    return;
                            }
                            CPLDebug("CADRG", "Creating file %s",
                                     osFilename.c_str());

                            CPLDebugOnly("CADRG", "Frame extent: %f %f %f %f",
                                         dfFrameMinX, dfFrameMinY, dfFrameMaxX,
                                         dfFrameMaxY);

                            std::unique_ptr<GDALDataset> poClippedDS;
                            {
                                // Lock because poWarpedDS is not thread-safe
                                LOCK();
                                poClippedDS = CADRGGetClippedDataset(
                                    poWarpedDS.get(), dfFrameMinX, dfFrameMinY,
                                    dfFrameMaxX, dfFrameMaxY);
                            }
                            if (poClippedDS)
                            {
                                if (poCT)
                                {
                                    if (!bFrameFullyInSrcDS)
                                    {
                                        // If the CADRG frame is not strictly inside
                                        // the zone extent, add a transparent color
                                        poClippedDS->GetRasterBand(1)
                                            ->SetNoDataValue(
                                                TRANSPARENT_COLOR_TABLE_ENTRY);
                                        GDALColorEntry sEntry = {0, 0, 0, 0};
                                        poClippedDS->GetRasterBand(1)
                                            ->GetColorTable()
                                            ->SetColorEntry(
                                                TRANSPARENT_COLOR_TABLE_ENTRY,
                                                &sEntry);
                                    }
                                }
                                else if (!bColorTablePerFrame)
                                {
                                    poClippedDS = CADRGGetPalettedDataset(
                                        poClippedDS.get(), &oCT,
                                        nColorQuantizationBits);
                                }
                            }

                            std::unique_ptr<GDALDataset> poDS_CADRG;
                            if (poClippedDS)
                            {
                                CADRGCreateCopyContext copyContextCopy(
                                    *copyContext);
                                poDS_CADRG = NITFDataset::CreateCopy(
                                    osFilename.c_str(), poClippedDS.get(),
                                    bStrict, aosOptions.List(), nullptr,
                                    nullptr, nRecLevel + 1, copyContext);
                            }
                            if (!poDS_CADRG)
                            {
                                LOCK();
                                if (osErrorMsg.empty())
                                    osErrorMsg = CPLGetLastErrorMsg();
                                bError = true;
                                return;
                            }
                            const bool bIsEmpty =
                                dynamic_cast<NITFDummyDataset *>(
                                    poDS_CADRG.get()) != nullptr;
                            poDS_CADRG.reset();
                            VSIUnlink((osFilename + ".aux.xml").c_str());

                            LOCK();
                            ++nCurFrameThisZone;
                            ++nCurFrameCounter;
                            if (bIsEmpty)
                            {
                                bMissingFramesFound = true;
                            }
                            else
                            {
                                ++nNonEmptyFrameCountThisZone;
                            }
                        };

#ifndef __COVERITY__
                        if (jobQueue)
                        {
                            if (!jobQueue->SubmitJob(task))
                            {
                                {
                                    std::lock_guard oLock(oMutex);
                                    bError = true;
                                }
                                bErrorLocal = true;
                            }
                        }
                        else
#endif
                        {
                            task();
                            if (bError)
                                return false;
                            if (pfnProgress &&
                                !pfnProgress(
                                    dfLastPct +
                                        (1.0 - dfLastPct) * nCurFrameCounter /
                                            std::max(1, nTotalFrameCount),
                                    "", pProgressData))
                            {
                                return false;
                            }
                        }
                    }
                }

#ifndef __COVERITY__
                if (jobQueue)
                {
                    while (true)
                    {
                        {
                            std::lock_guard oLock(oMutex);
                            if (pfnProgress &&
                                !pfnProgress(
                                    dfLastPct +
                                        (1.0 - dfLastPct) * nCurFrameCounter /
                                            std::max(1, nTotalFrameCount),
                                    "", pProgressData))
                            {
                                bError = true;
                            }
                            if (bError ||
                                nCurFrameThisZone == nFrameCountThisZone)
                                break;
                        }
                        jobQueue->WaitEvent();
                    }

                    jobQueue->WaitCompletion();
                    std::lock_guard oLock(oMutex);
                    if (bError)
                    {
                        if (!osErrorMsg.empty())
                        {
                            CPLError(CE_Failure, CPLE_AppDefined, "%s",
                                     osErrorMsg.c_str());
                        }
                        return false;
                    }
                }
#endif

                if (!bDirectoryAlreadyExists &&
                    nNonEmptyFrameCountThisZone == 0)
                {
                    VSIRmdir(osZoneDir.c_str());
                }
            }

            const char *pszClassification =
                CSLFetchNameValueDef(papszOptions, "FSCLAS", "U");
            const char chIndexClassification =
                pszClassification && pszClassification[0] ? pszClassification[0]
                                                          : 'U';
            if (nCurFrameCounter > 0 &&
                !RPFTOCCreate(
                    osRPFDir,
                    CPLFormFilenameSafe(osRPFDir.c_str(), "A.TOC", nullptr),
                    chIndexClassification, nReciprocalScale,
                    CSLFetchNameValueDef(papszOptions, "OSTAID",
                                         ""),  // producer id
                    CSLFetchNameValueDef(papszOptions, "ONAME",
                                         ""),  // producer name
                    CSLFetchNameValueDef(papszOptions, "SECURITY_COUNTRY_CODE",
                                         ""),
                    /* bDoNotCreateIfNoFrame =*/bMissingFramesFound))
            {
                return nullptr;
            }

            return std::make_unique<NITFDummyDataset>();
        }
    }
}
#endif
