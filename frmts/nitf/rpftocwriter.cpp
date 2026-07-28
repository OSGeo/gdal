/******************************************************************************
 *
 * Project:  NITF Read/Write Library
 * Purpose:  Creates A.TOC RPF index for CADRG frames
 * Author:   Even Rouault, even dot rouault at spatialys dot com
 *
 **********************************************************************
 * Copyright (c) 2026, T-Kartor
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_vsi.h"
#include "gdal_dataset.h"
#include "nitflib.h"
#include "offsetpatcher.h"
#include "rpfframewriter.h"
#include "rpftocwriter.h"

#include <algorithm>
#include <cinttypes>
#include <limits>
#include <map>
#include <utility>

namespace
{
struct FrameDesc
{
    int nZone = 0;
    int nReciprocalScale = 0;
    int nFrameX = 0;
    int nFrameY = 0;
    double dfMinX = 0;
    double dfMinY = 0;
    std::string osRelativeFilename{};  // relative to osInputDirectory
    char chClassification = 'U';
};

struct MinMaxFrameXY
{
    int MinX = std::numeric_limits<int>::max();
    int MinY = std::numeric_limits<int>::max();
    int MaxX = 0;
    int MaxY = 0;
};

struct ScaleZone
{
    int nReciprocalScale = 0;
    int nZone = 0;

    bool operator<(const ScaleZone &other) const
    {
        // Sort reciprocal scale by decreasing order. This is apparently needed for
        // some viewers like Falcon Lite to be able to display A.TOC files
        // with multiple scales.
        return nReciprocalScale > other.nReciprocalScale ||
               (nReciprocalScale == other.nReciprocalScale &&
                nZone < other.nZone);
    }
};

}  // namespace

/************************************************************************/
/*                  Create_RPFTOC_LocationComponent()                   */
/************************************************************************/

static void
Create_RPFTOC_LocationComponent(GDALOffsetPatcher::OffsetPatcher &offsetPatcher)
{
    auto poBuffer = offsetPatcher.CreateBuffer(
        "LocationComponent", /* bEndiannessIsLittle = */ false);
    CPLAssert(poBuffer);
    poBuffer->DeclareOffsetAtCurrentPosition("LOCATION_COMPONENT_LOCATION");

    static const struct
    {
        uint16_t locationId;
        const char *locationBufferName;
        const char *locationOffsetName;
    } asLocations[] = {
        {LID_BoundaryRectangleSectionSubheader /* 148 */,
         "BoundaryRectangleSectionSubheader",
         "BOUNDARY_RECTANGLE_SECTION_SUBHEADER_LOCATION"},
        {LID_BoundaryRectangleTable /* 149 */, "BoundaryRectangleTable",
         "BOUNDARY_RECTANGLE_TABLE_LOCATION"},
        {LID_FrameFileIndexSectionSubHeader /* 150 */,
         "FrameFileIndexSectionSubHeader",
         "FRAME_FILE_INDEX_SECTION_SUBHEADER_LOCATION"},
        {LID_FrameFileIndexSubsection /* 151 */, "FrameFileIndexSubsection",
         "FRAME_FILE_INDEX_SUBSECTION_LOCATION"},
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
/*          Create_RPFTOC_BoundaryRectangleSectionSubheader()           */
/************************************************************************/

static void Create_RPFTOC_BoundaryRectangleSectionSubheader(
    GDALOffsetPatcher::OffsetPatcher &offsetPatcher,
    size_t nNumberOfBoundaryRectangles)
{
    auto poBuffer = offsetPatcher.CreateBuffer(
        "BoundaryRectangleSectionSubheader", /* bEndiannessIsLittle = */ false);
    CPLAssert(poBuffer);
    poBuffer->DeclareOffsetAtCurrentPosition(
        "BOUNDARY_RECTANGLE_SECTION_SUBHEADER_LOCATION");
    constexpr uint32_t BOUNDARY_RECTANGLE_TABLE_OFFSET = 0;
    poBuffer->AppendUInt32(BOUNDARY_RECTANGLE_TABLE_OFFSET);
    poBuffer->AppendUInt16(static_cast<uint16_t>(nNumberOfBoundaryRectangles));
    constexpr uint16_t BOUNDARY_RECTANGLE_RECORD_LENGTH = 132;
    poBuffer->AppendUInt16(BOUNDARY_RECTANGLE_RECORD_LENGTH);
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
/*                Create_RPFTOC_BoundaryRectangleTable()                */
/************************************************************************/

static void Create_RPFTOC_BoundaryRectangleTable(
    GDALOffsetPatcher::OffsetPatcher &offsetPatcher,
    const std::string &osProducer,
    const std::map<ScaleZone, MinMaxFrameXY> &oMapScaleZoneToMinMaxFrameXY)
{
    auto poBuffer = offsetPatcher.CreateBuffer(
        "BoundaryRectangleTable", /* bEndiannessIsLittle = */ false);
    CPLAssert(poBuffer);
    poBuffer->DeclareOffsetAtCurrentPosition(
        "BOUNDARY_RECTANGLE_TABLE_LOCATION");

    for (const auto &[scaleZone, extent] : oMapScaleZoneToMinMaxFrameXY)
    {
        poBuffer->AppendString("CADRG");  // PRODUCT_DATA_TYPE
        poBuffer->AppendString("55:1 ");  // COMPRESSION_RATIO

        std::string osScaleOrResolution;
        const int nReciprocalScale = scaleZone.nReciprocalScale;
        if (nReciprocalScale >= Million && (nReciprocalScale % Million) == 0)
            osScaleOrResolution =
                CPLSPrintf("1:%dM", nReciprocalScale / Million);
        else if (nReciprocalScale >= Kilo && (nReciprocalScale % Kilo) == 0)
            osScaleOrResolution = CPLSPrintf("1:%dK", nReciprocalScale / Kilo);
        else
            osScaleOrResolution = CPLSPrintf("1:%d", nReciprocalScale);
        poBuffer->AppendString(StrPadTruncate(osScaleOrResolution, 12));

        const int nZone = scaleZone.nZone;
        poBuffer->AppendString(CPLSPrintf("%c", RPFCADRGZoneNumToChar(nZone)));
        poBuffer->AppendString(StrPadTruncate(osProducer, 5));

        double dfXMin = 0;
        double dfYMin = 0;
        double dfXMax = 0;
        double dfYMax = 0;
        double dfUnused = 0;
        RPFGetCADRGFrameExtent(nZone, nReciprocalScale, extent.MinX,
                               extent.MinY, dfXMin, dfYMin, dfUnused, dfUnused);
        RPFGetCADRGFrameExtent(nZone, nReciprocalScale, extent.MaxX,
                               extent.MaxY, dfUnused, dfUnused, dfXMax, dfYMax);

        double dfULX = dfXMin;
        double dfULY = dfYMax;
        double dfLLX = dfXMin;
        double dfLLY = dfYMin;
        double dfURX = dfXMax;
        double dfURY = dfYMax;
        double dfLRX = dfXMax;
        double dfLRY = dfYMin;

        if (nZone == MAX_ZONE_NORTHERN_HEMISPHERE || nZone == MAX_ZONE)
        {
            OGRSpatialReference oPolarSRS;
            oPolarSRS.importFromWkt(nZone == MAX_ZONE_NORTHERN_HEMISPHERE
                                        ? pszNorthPolarProjection
                                        : pszSouthPolarProjection);
            OGRSpatialReference oSRS_WGS84;
            oSRS_WGS84.SetWellKnownGeogCS("WGS84");
            oSRS_WGS84.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
            auto poCT = std::unique_ptr<OGRCoordinateTransformation>(
                OGRCreateCoordinateTransformation(&oPolarSRS, &oSRS_WGS84));
            poCT->Transform(1, &dfULX, &dfULY);
            poCT->Transform(1, &dfLLX, &dfLLY);
            poCT->Transform(1, &dfURX, &dfURY);
            poCT->Transform(1, &dfLRX, &dfLRY);
        }

        poBuffer->AppendFloat64(dfULY);  // NORTHWEST_LATITUDE
        poBuffer->AppendFloat64(dfULX);  // NORTHWEST_LONGITUDE

        poBuffer->AppendFloat64(dfLLY);  // SOUTHWEST_LATITUDE
        poBuffer->AppendFloat64(dfLLX);  // SOUTHWEST_LONGITUDE

        poBuffer->AppendFloat64(dfURY);  // NORTHEAST_LATITUDE
        poBuffer->AppendFloat64(dfURX);  // NORTHEAST_LONGITUDE

        poBuffer->AppendFloat64(dfLRY);  // SOUTHEAST_LATITUDE
        poBuffer->AppendFloat64(dfLRX);  // SOUTHEAST_LONGITUDE

        double latResolution = 0;
        double lonResolution = 0;
        double latInterval = 0;
        double lonInterval = 0;
        RPFGetCADRGResolutionAndInterval(nZone, nReciprocalScale, latResolution,
                                         lonResolution, latInterval,
                                         lonInterval);

        poBuffer->AppendFloat64(latResolution);
        poBuffer->AppendFloat64(lonResolution);
        poBuffer->AppendFloat64(latInterval);
        poBuffer->AppendFloat64(lonInterval);

        const int nCountY = extent.MaxY - extent.MinY + 1;
        poBuffer->AppendUInt32(static_cast<uint32_t>(nCountY));

        const int nCountX = extent.MaxX - extent.MinX + 1;
        poBuffer->AppendUInt32(static_cast<uint32_t>(nCountX));
    }
}

/************************************************************************/
/*            Create_RPFTOC_FrameFileIndexSectionSubHeader()            */
/************************************************************************/

static void Create_RPFTOC_FrameFileIndexSectionSubHeader(
    GDALOffsetPatcher::OffsetPatcher &offsetPatcher,
    char chHighestClassification, size_t nCountFrames, uint16_t nCountSubdirs)
{
    auto poBuffer = offsetPatcher.CreateBuffer(
        "FrameFileIndexSectionSubHeader", /* bEndiannessIsLittle = */ false);
    CPLAssert(poBuffer);
    poBuffer->DeclareOffsetAtCurrentPosition(
        "FRAME_FILE_INDEX_SECTION_SUBHEADER_LOCATION");

    poBuffer->AppendString(CPLSPrintf("%c", chHighestClassification));
    constexpr uint32_t FRAME_FILE_INDEX_TABLE_OFFSET = 0;
    poBuffer->AppendUInt32(FRAME_FILE_INDEX_TABLE_OFFSET);
    poBuffer->AppendUInt32(static_cast<uint32_t>(nCountFrames));
    poBuffer->AppendUInt16(nCountSubdirs);
    constexpr uint16_t FRAME_FILE_INDEX_RECORD_LENGTH = 33;
    poBuffer->AppendUInt16(FRAME_FILE_INDEX_RECORD_LENGTH);
}

/************************************************************************/
/*                             GetGEOREF()                              */
/************************************************************************/

/** Return coordinate as a World Geographic Reference System (GEOREF) string
 * as described in paragraph 5.4 of DMA TM 8358.1
 * (https://everyspec.com/DoD/DOD-General/download.php?spec=DMA_TM-8358.1.006300.PDF)
 */
static std::string GetGEOREF(double dfLon, double dfLat)
{
    // clang-format off
    // letters 'I' and 'O' are omitted to avoid confusiong with one and zero
    constexpr char ALPHABET_WITHOUT_IO[] = {
        'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',      'J',
        'K', 'L', 'M', 'N',      'P', 'Q', 'R', 'S', 'T',
        'U', 'V', 'W', 'X', 'Y', 'Z'
    };
    // clang-format on

    std::string osRes;

    constexpr double LON_ORIGIN = -180;
    constexpr double LAT_ORIGIN = -90;
    constexpr int QUADRANGLE_SIZE = 15;  // degree
    constexpr double EPSILON = 1e-5;

    // Longitude zone
    {
        const int nIdx =
            static_cast<int>(dfLon - LON_ORIGIN + EPSILON) / QUADRANGLE_SIZE;
        CPLAssert(nIdx >= 0 && nIdx < 24);
        osRes += ALPHABET_WITHOUT_IO[nIdx];
    }

    // Latitude band
    {
        const int nIdx =
            static_cast<int>(dfLat - LAT_ORIGIN + EPSILON) / QUADRANGLE_SIZE;
        CPLAssert(nIdx >= 0 && nIdx < 12);
        osRes += ALPHABET_WITHOUT_IO[nIdx];
    }

    // Longitude index within 15x15 degree quadrangle
    {
        const int nIdx =
            static_cast<int>(dfLon - LON_ORIGIN + EPSILON) % QUADRANGLE_SIZE;
        osRes += ALPHABET_WITHOUT_IO[nIdx];
    }

    // Latitude index within 15x15 degree quadrangle
    {
        const int nIdx =
            static_cast<int>(dfLat - LAT_ORIGIN + EPSILON) % QUADRANGLE_SIZE;
        osRes += ALPHABET_WITHOUT_IO[nIdx];
    }

    // Longitude minutes
    {
        constexpr int MINUTES_IN_DEGREE = 60;
        const int nMinutes =
            static_cast<int>((dfLon - LON_ORIGIN) * MINUTES_IN_DEGREE +
                             EPSILON) %
            MINUTES_IN_DEGREE;
        osRes += CPLSPrintf("%02d", nMinutes);
    }

    return osRes;
}

/************************************************************************/
/*               Create_RPFTOC_FrameFileIndexSubsection()               */
/************************************************************************/

static void Create_RPFTOC_FrameFileIndexSubsection(
    GDALOffsetPatcher::OffsetPatcher &offsetPatcher,
    const std::string &osSecurityCountryCode,
    const std::map<ScaleZone, std::vector<FrameDesc>> &oMapScaleZoneToFrames,
    const std::map<ScaleZone, MinMaxFrameXY> &oMapScaleZoneToMinMaxFrameXY,
    const std::map<std::string, int> &oMapSubdirToIdx)
{
    auto poBuffer = offsetPatcher.CreateBuffer(
        "FrameFileIndexSubsection", /* bEndiannessIsLittle = */ false);
    CPLAssert(poBuffer);
    poBuffer->DeclareOffsetAtCurrentPosition(
        "FRAME_FILE_INDEX_SUBSECTION_LOCATION");

    std::map<ScaleZone, uint16_t> oMapScaleZoneToIdx;
    for ([[maybe_unused]] const auto &[scaleZone, unused] :
         oMapScaleZoneToFrames)
    {
        if (!cpl::contains(oMapScaleZoneToIdx, scaleZone))
        {
            oMapScaleZoneToIdx[scaleZone] =
                static_cast<uint16_t>(oMapScaleZoneToIdx.size());
        }
    }

    for (const auto &[scaleZone, framesDesc] : oMapScaleZoneToFrames)
    {
        const auto oIterMapScaleZoneToMinMaxFrameXY =
            oMapScaleZoneToMinMaxFrameXY.find(scaleZone);
        CPLAssert(oIterMapScaleZoneToMinMaxFrameXY !=
                  oMapScaleZoneToMinMaxFrameXY.end());
        const auto &oMinMaxFrameXY = oIterMapScaleZoneToMinMaxFrameXY->second;

        for (const auto &frameDesc : framesDesc)
        {
            const auto oIterMapScaleZoneToIdx =
                oMapScaleZoneToIdx.find(scaleZone);
            CPLAssert(oIterMapScaleZoneToIdx != oMapScaleZoneToIdx.end());
            poBuffer->AppendUInt16(oIterMapScaleZoneToIdx->second);
            poBuffer->AppendUInt16(
                static_cast<uint16_t>(frameDesc.nFrameY - oMinMaxFrameXY.MinY));
            poBuffer->AppendUInt16(
                static_cast<uint16_t>(frameDesc.nFrameX - oMinMaxFrameXY.MinX));

            const std::string osSubdir =
                CPLGetPathSafe(frameDesc.osRelativeFilename.c_str());
            const auto oIterSubdirToIdx = oMapSubdirToIdx.find(osSubdir);
            CPLAssert(oIterSubdirToIdx != oMapSubdirToIdx.end());
            poBuffer->AppendUInt32RefForOffset(
                CPLSPrintf("PATHNAME_RECORD_OFFSET_%d",
                           oIterSubdirToIdx->second),
                /* bRelativeToStartOfBuffer = */ true);
            poBuffer->AppendString(StrPadTruncate(
                CPLGetFilename(frameDesc.osRelativeFilename.c_str()), 12));
            const std::string osGeographicLocation =
                GetGEOREF(frameDesc.dfMinX, frameDesc.dfMinY);
            CPLAssert(osGeographicLocation.size() == 6);
            poBuffer->AppendString(StrPadTruncate(osGeographicLocation, 6));
            poBuffer->AppendString("U");  // FRAME_FILE_SECURITY_CLASSIFICATION
            poBuffer->AppendString(StrPadTruncate(osSecurityCountryCode, 2));
            // FRAME_FILE_SECURITY_RELEASE_MARKING
            poBuffer->AppendString("  ");
        }
    }

    struct SortedDirPrefixes
    {
        int nIdx = 0;
        std::string osSubdir{};
    };

    std::vector<SortedDirPrefixes> asSortedDirPrefixes;
    for (const auto &[osSubdir, nIdx] : oMapSubdirToIdx)
    {
        SortedDirPrefixes s;
        s.nIdx = nIdx;
        s.osSubdir = osSubdir;
        asSortedDirPrefixes.push_back(std::move(s));
    }
    std::sort(asSortedDirPrefixes.begin(), asSortedDirPrefixes.end(),
              [](const SortedDirPrefixes &a, const SortedDirPrefixes &b)
              { return a.nIdx < b.nIdx; });

    for (const auto &sortedDirPrefix : asSortedDirPrefixes)
    {
        poBuffer->DeclareOffsetAtCurrentPosition(
            CPLSPrintf("PATHNAME_RECORD_OFFSET_%d", sortedDirPrefix.nIdx));
        std::string osPath =
            "./" + CPLString(sortedDirPrefix.osSubdir).replaceAll('\\', '/');
        if (osPath.back() != '/')
            osPath += '/';
        poBuffer->AppendUInt16(static_cast<uint16_t>(osPath.size()));
        poBuffer->AppendString(osPath);
    }
}

/************************************************************************/
/*                         RPCTOCCreateRPFDES()                         */
/************************************************************************/

static bool RPCTOCCreateRPFDES(
    VSILFILE *fp, GDALOffsetPatcher::OffsetPatcher &offsetPatcher,
    const std::string &osProducer, const std::string &osSecurityCountryCode,
    const std::map<ScaleZone, std::vector<FrameDesc>> &oMapScaleZoneToFrames,
    const std::map<ScaleZone, MinMaxFrameXY> &oMapScaleZoneToMinMaxFrameXY)
{
    (void)oMapScaleZoneToFrames;

    bool bOK = fp->Seek(0, SEEK_END) == 0;

    const char *pszDESHeader = RPFFrameWriteGetDESHeader();
    bOK &=
        fp->Write(pszDESHeader, strlen(pszDESHeader)) == strlen(pszDESHeader);

    const auto nOffsetTRESize = fp->Tell() + strlen("RPFDES");
    // xxxxx is a placeholder for the TRE size, patched later with the actual
    // size.
    constexpr const char *pszRPFDESTREStart = "RPFDESxxxxx";
    bOK &= fp->Write(pszRPFDESTREStart, 1, strlen(pszRPFDESTREStart)) ==
           strlen(pszRPFDESTREStart);

    // Associate an index to each subdir name used by frames
    std::map<std::string, int> oMapSubdirToIdx;
    size_t nCountFrames = 0;

    // From lowest to highest classification level
    constexpr const char achClassifications[] = {
        'U',  // Unclassified
        'R',  // Restricted
        'C',  // Confidential
        'S',  // Secret
        'T',  // Top Secret
    };
    std::map<char, unsigned> oMapClassificationToLevel;
    for (unsigned i = 0; i < CPL_ARRAYSIZE(achClassifications); ++i)
        oMapClassificationToLevel[achClassifications[i]] = i;

    unsigned nHighestClassification = 0;

    for ([[maybe_unused]] const auto &[unused, framesDesc] :
         oMapScaleZoneToFrames)
    {
        for (const auto &frameDesc : framesDesc)
        {
            const std::string osSubdir =
                CPLGetPathSafe(frameDesc.osRelativeFilename.c_str());
            if (!cpl::contains(oMapSubdirToIdx, osSubdir))
            {
                oMapSubdirToIdx[osSubdir] =
                    static_cast<int>(oMapSubdirToIdx.size());
            }

            const auto oClassificationIter =
                oMapClassificationToLevel.find(frameDesc.chClassification);
            if (oClassificationIter == oMapClassificationToLevel.end())
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Unknown classification level '%c' for %s",
                         frameDesc.chClassification,
                         frameDesc.osRelativeFilename.c_str());
            }
            else
            {
                nHighestClassification = std::max(nHighestClassification,
                                                  oClassificationIter->second);
            }
        }
        nCountFrames += framesDesc.size();
    }
    if (oMapSubdirToIdx.size() > std::numeric_limits<uint16_t>::max())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Too many subdirectories: %u. Only up to %u are allowed",
                 static_cast<unsigned>(oMapSubdirToIdx.size()),
                 std::numeric_limits<uint16_t>::max());
        return false;
    }

    // Create RPF sections
    Create_RPFTOC_LocationComponent(offsetPatcher);
    Create_RPFTOC_BoundaryRectangleSectionSubheader(
        offsetPatcher, oMapScaleZoneToMinMaxFrameXY.size());
    Create_RPFTOC_BoundaryRectangleTable(offsetPatcher, osProducer,
                                         oMapScaleZoneToMinMaxFrameXY);
    const char chHighestClassification =
        achClassifications[nHighestClassification];
    Create_RPFTOC_FrameFileIndexSectionSubHeader(
        offsetPatcher, chHighestClassification, nCountFrames,
        static_cast<uint16_t>(oMapSubdirToIdx.size()));
    Create_RPFTOC_FrameFileIndexSubsection(
        offsetPatcher, osSecurityCountryCode, oMapScaleZoneToFrames,
        oMapScaleZoneToMinMaxFrameXY, oMapSubdirToIdx);

    // Write RPF sections
    size_t nTREDataSize = 0;
    for (const char *pszName :
         {"LocationComponent", "BoundaryRectangleSectionSubheader",
          "BoundaryRectangleTable", "FrameFileIndexSectionSubHeader",
          "FrameFileIndexSubsection"})
    {
        const auto poBuffer = offsetPatcher.GetBufferFromName(pszName);
        CPLAssert(poBuffer);
        poBuffer->DeclareBufferWrittenAtPosition(fp->Tell());
        bOK &= fp->Write(poBuffer->GetBuffer().data(),
                         poBuffer->GetBuffer().size()) ==
               poBuffer->GetBuffer().size();
        nTREDataSize += poBuffer->GetBuffer().size();
    }

    // Patch the size of the RPFDES TRE data
    if (nTREDataSize <= 99999)
    {
        bOK &= fp->Seek(nOffsetTRESize, SEEK_SET) == 0;
        const std::string osTRESize =
            CPLSPrintf("%05d", static_cast<int>(nTREDataSize));
        bOK &=
            fp->Write(osTRESize.c_str(), osTRESize.size()) == osTRESize.size();
    }
    else
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "RPFDES TRE size exceeds 99999 bytes. Some readers might not "
                 "be able to read the A.TOC file correctly");
    }

    // Update LDSH and LD in the NITF Header

    // NUMI offset is at a fixed offset 360 (unless there is a FSDWNG field)
    constexpr vsi_l_offset nNumIOffset = 360;
    constexpr vsi_l_offset nNumGOffset = nNumIOffset + 3;
    // the last + 3 is for NUMX field, which is not used
    constexpr vsi_l_offset nNumTOffset = nNumGOffset + 3 + 3;
    constexpr vsi_l_offset nNumDESOffset = nNumTOffset + 3;
    constexpr auto nOffsetLDSH = nNumDESOffset + 3;

    constexpr int iDES = 0;
    bOK &= fp->Seek(nOffsetLDSH + iDES * 13, SEEK_SET) == 0;
    bOK &= fp->Write(CPLSPrintf("%04d", static_cast<int>(strlen(pszDESHeader))),
                     4) == 4;
    bOK &= fp->Write(
               CPLSPrintf("%09d", static_cast<int>(nTREDataSize +
                                                   strlen(pszRPFDESTREStart))),
               9) == 9;

    // Update total file length
    bOK &= fp->Seek(0, SEEK_END) == 0;
    const uint64_t nFileLen = fp->Tell();
    CPLString osFileLen = CPLString().Printf("%012" PRIu64, nFileLen);
    constexpr vsi_l_offset FILE_LENGTH_OFFSET = 342;
    bOK &= fp->Seek(FILE_LENGTH_OFFSET, SEEK_SET) == 0;
    bOK &= fp->Write(osFileLen.data(), osFileLen.size()) == osFileLen.size();

    return bOK;
}

/************************************************************************/
/*                        RPFTOCCollectFrames()                         */
/************************************************************************/

static bool RPFTOCCollectFrames(
    VSIDIR *psDir, const std::string &osInputDirectory,
    const int nReciprocalScale,
    std::map<ScaleZone, std::vector<FrameDesc>> &oMapScaleZoneToFrames,
    std::map<ScaleZone, MinMaxFrameXY> &oMapScaleZoneToMinMaxFrameXY)
{

    while (const VSIDIREntry *psEntry = VSIGetNextDirEntry(psDir))
    {
        if (VSI_ISDIR(psEntry->nMode) ||
            EQUAL(CPLGetFilename(psEntry->pszName), "A.TOC"))
            continue;
        const char *const apszAllowedDrivers[] = {"NITF", nullptr};
        const std::string osFullFilename = CPLFormFilenameSafe(
            osInputDirectory.c_str(), psEntry->pszName, nullptr);
        auto poDS = std::unique_ptr<GDALDataset>(GDALDataset::Open(
            osFullFilename.c_str(), GDAL_OF_RASTER, apszAllowedDrivers));
        if (!poDS)
            continue;

        const std::string osFilenamePart =
            CPLGetFilename(osFullFilename.c_str());
        if (osFilenamePart.size() != 12)
        {
            CPLDebug("RPFTOC", "%s filename is not 12 character long",
                     osFullFilename.c_str());
            continue;
        }

        if (poDS->GetRasterXSize() != CADRG_FRAME_PIXEL_COUNT ||
            poDS->GetRasterYSize() != CADRG_FRAME_PIXEL_COUNT)
        {
            CPLDebug("RPFTOC", "%s has not the dimensions of a CADRG frame",
                     osFullFilename.c_str());
            continue;
        }

        const std::string osDataSeriesCode(osFilenamePart.substr(9, 2));
        if (!RPFCADRGIsKnownDataSeriesCode(osDataSeriesCode.c_str()))
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Data series code '%s' in %s extension is a unknown CADRG "
                     "series code",
                     osDataSeriesCode.c_str(), osFullFilename.c_str());
        }

        int nThisScale = nReciprocalScale;
        if (nThisScale == 0)
        {
            nThisScale =
                RPFCADRGGetScaleFromDataSeriesCode(osDataSeriesCode.c_str());
            if (nThisScale == 0)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Scale cannot be inferred from filename %s. Specify "
                         "the 'scale' argument",
                         osFullFilename.c_str());
                return false;
            }
        }

        const int nZone = RPFCADRGZoneCharToNum(osFilenamePart.back());
        if (nZone == 0)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "CADRG zone cannot be inferred from last character of "
                     "filename %s.",
                     osFullFilename.c_str());
            return false;
        }

        OGREnvelope sExtentWGS84;
        if (poDS->GetExtentWGS84LongLat(&sExtentWGS84) != CE_None)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cannot get dataset extent for %s",
                     osFullFilename.c_str());
            return false;
        }

        const auto frameDefinitions =
            RPFGetCADRGFramesForEnvelope(nZone, nThisScale, poDS.get());
        if (frameDefinitions.empty())
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cannot establish CADRG frames intersecting dataset "
                     "extent for %s",
                     osFullFilename.c_str());
            return false;
        }
        if (frameDefinitions.size() != 1)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Extent of file %s does not match a single CADRG frame",
                     osFullFilename.c_str());
            return false;
        }

        const std::string osExpectedFilenameStart =
            RPFGetCADRGFrameNumberAsString(nZone, nThisScale,
                                           frameDefinitions[0].nFrameMinX,
                                           frameDefinitions[0].nFrameMinY);
        if (!cpl::starts_with(CPLString(osFilenamePart).toupper(),
                              osExpectedFilenameStart))
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Filename part of %s should begin with %s",
                     osFullFilename.c_str(), osExpectedFilenameStart.c_str());
        }

        // Store needed metadata on the frame
        FrameDesc desc;
        desc.nZone = nZone;
        desc.nReciprocalScale = nThisScale;
        desc.nFrameX = frameDefinitions[0].nFrameMinX;
        desc.nFrameY = frameDefinitions[0].nFrameMinY;
        desc.dfMinX = sExtentWGS84.MinX;
        desc.dfMinY = sExtentWGS84.MinY;
        desc.osRelativeFilename = psEntry->pszName;
        const char *pszClassification = poDS->GetMetadataItem("FCLASS");
        if (pszClassification)
            desc.chClassification = pszClassification[0];

        // Update min and max frame indices for this (scale, zone) pair
        auto &sMinMaxFrameXY =
            oMapScaleZoneToMinMaxFrameXY[{nThisScale, nZone}];
        sMinMaxFrameXY.MinX = std::min(sMinMaxFrameXY.MinX, desc.nFrameX);
        sMinMaxFrameXY.MinY = std::min(sMinMaxFrameXY.MinY, desc.nFrameY);
        sMinMaxFrameXY.MaxX = std::max(sMinMaxFrameXY.MaxX, desc.nFrameX);
        sMinMaxFrameXY.MaxY = std::max(sMinMaxFrameXY.MaxY, desc.nFrameY);

        oMapScaleZoneToFrames[{nThisScale, nZone}].push_back(std::move(desc));
    }

    // For each (scale, zone) pair, sort by increasing y and then x
    // to have a reproducible output
    for ([[maybe_unused]] auto &[unused, frameDescs] : oMapScaleZoneToFrames)
    {
        std::sort(frameDescs.begin(), frameDescs.end(),
                  [](const FrameDesc &a, const FrameDesc &b)
                  {
                      return a.nFrameY < b.nFrameY ||
                             (a.nFrameY == b.nFrameY && a.nFrameX < b.nFrameX);
                  });
    }

    return true;
}

/************************************************************************/
/*                            RPFTOCCreate()                            */
/************************************************************************/

bool RPFTOCCreate(const std::string &osInputDirectory,
                  const std::string &osOutputFilename,
                  const char chIndexClassification, const int nReciprocalScale,
                  const std::string &osProducerID,
                  const std::string &osProducerName,
                  const std::string &osSecurityCountryCode,
                  bool bDoNotCreateIfNoFrame)
{
    std::unique_ptr<VSIDIR, decltype(&VSICloseDir)> psDir(
        VSIOpenDir(osInputDirectory.c_str(), -1 /* unlimited recursion */,
                   nullptr),
        VSICloseDir);
    if (!psDir)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "%s is not a directory or cannot be opened",
                 osInputDirectory.c_str());
        return false;
    }

    std::map<ScaleZone, std::vector<FrameDesc>> oMapScaleZoneToFrames;
    std::map<ScaleZone, MinMaxFrameXY> oMapScaleZoneToMinMaxFrameXY;
    if (!RPFTOCCollectFrames(psDir.get(), osInputDirectory, nReciprocalScale,
                             oMapScaleZoneToFrames,
                             oMapScaleZoneToMinMaxFrameXY))
    {
        return false;
    }

    if (oMapScaleZoneToFrames.empty())
    {
        if (bDoNotCreateIfNoFrame)
        {
            return true;
        }
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined, "No CADRG frame found in %s",
                     osInputDirectory.c_str());
            return false;
        }
    }

    GDALOffsetPatcher::OffsetPatcher offsetPatcher;

    CPLStringList aosOptions;
    aosOptions.SetNameValue("FHDR", "NITF02.00");
    aosOptions.SetNameValue("NUMI", "0");
    aosOptions.SetNameValue("NUMDES", "1");
    constexpr const char pszLeftPaddedATOC[] = "       A.TOC";
    static_assert(sizeof(pszLeftPaddedATOC) == 12 + 1);
    aosOptions.SetNameValue("FCLASS", CPLSPrintf("%c", chIndexClassification));
    aosOptions.SetNameValue("FDT", "11111111ZJAN26");
    aosOptions.SetNameValue("FTITLE", pszLeftPaddedATOC);
    if (!osProducerID.empty())
        aosOptions.SetNameValue("OSTAID", osProducerID.c_str());
    if (!osProducerName.empty())
        aosOptions.SetNameValue("ONAME", osProducerName.c_str());
    Create_CADRG_RPFHDR(&offsetPatcher, pszLeftPaddedATOC, aosOptions);
    if (!NITFCreateEx(osOutputFilename.c_str(), /* nPixels = */ 0,
                      /* nLines = */ 0, /* nBands = */ 0,
                      /* nBitsPerSample = */ 0, /* PVType = */ nullptr,
                      aosOptions.List(), /* pnIndex = */ nullptr,
                      /* pnImageCount = */ nullptr,
                      /* pnImageOffset = */ nullptr, /* pnICOffset = */ nullptr,
                      &offsetPatcher))
    {
        return false;
    }

    auto fp = VSIFilesystemHandler::OpenStatic(osOutputFilename.c_str(), "rb+");
    return fp != nullptr &&
           RPCTOCCreateRPFDES(fp.get(), offsetPatcher, osProducerID,
                              osSecurityCountryCode, oMapScaleZoneToFrames,
                              oMapScaleZoneToMinMaxFrameXY) &&
           offsetPatcher.Finalize(fp.get()) && fp->Close() == 0;
}
