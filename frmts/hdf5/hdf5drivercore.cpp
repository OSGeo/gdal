/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  HDF5 driver
 * Author:   Even Rouault, <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2023, Even Rouault, <even.rouault at spatialys.com>
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

#include "hdf5drivercore.h"

/************************************************************************/
/*                          HDF5DatasetIdentify()                       */
/************************************************************************/

int HDF5DatasetIdentify(GDALOpenInfo *poOpenInfo)

{
    if ((poOpenInfo->nOpenFlags & GDAL_OF_MULTIDIM_RASTER) &&
        STARTS_WITH(poOpenInfo->pszFilename, "HDF5:"))
    {
        return TRUE;
    }

    // Is it an HDF5 file?
    constexpr char achSignature[] = "\211HDF\r\n\032\n";

    if (!poOpenInfo->pabyHeader)
        return FALSE;

    const CPLString osExt(CPLGetExtension(poOpenInfo->pszFilename));

    const auto IsRecognizedByNetCDFDriver = [&osExt, poOpenInfo]()
    {
        if ((EQUAL(osExt, "NC") || EQUAL(osExt, "CDF") || EQUAL(osExt, "NC4") ||
             EQUAL(osExt, "gmac")) &&
            GDALGetDriverByName("netCDF") != nullptr)
        {
            const char *const apszAllowedDriver[] = {"netCDF", nullptr};
            CPLPushErrorHandler(CPLQuietErrorHandler);
            GDALDatasetH hDS = GDALOpenEx(
                poOpenInfo->pszFilename,
                GDAL_OF_RASTER | GDAL_OF_MULTIDIM_RASTER | GDAL_OF_VECTOR,
                apszAllowedDriver, nullptr, nullptr);
            CPLPopErrorHandler();
            if (hDS)
            {
                GDALClose(hDS);
                return true;
            }
        }
        return false;
    };

    if (memcmp(poOpenInfo->pabyHeader, achSignature, 8) == 0 ||
        (poOpenInfo->nHeaderBytes > 512 + 8 &&
         memcmp(poOpenInfo->pabyHeader + 512, achSignature, 8) == 0))
    {
        // The tests to avoid opening KEA and BAG drivers are not
        // necessary when drivers are built in the core lib, as they
        // are registered after HDF5, but in the case of plugins, we
        // cannot do assumptions about the registration order.

        // Avoid opening kea files if the kea driver is available.
        if (EQUAL(osExt, "KEA") && GDALGetDriverByName("KEA") != nullptr)
        {
            return FALSE;
        }

        // Avoid opening BAG files if the bag driver is available.
        if (EQUAL(osExt, "BAG") && GDALGetDriverByName("BAG") != nullptr)
        {
            return FALSE;
        }

        // Avoid opening NC files if the netCDF driver is available and
        // they are recognized by it.
        if (IsRecognizedByNetCDFDriver())
        {
            return FALSE;
        }

        return TRUE;
    }

    if (memcmp(poOpenInfo->pabyHeader, "<HDF_UserBlock>", 15) == 0)
    {
        return TRUE;
    }

    // The HDF5 signature can be at offsets 512, 1024, 2048, etc.
    if (poOpenInfo->fpL != nullptr &&
        (EQUAL(osExt, "h5") || EQUAL(osExt, "hdf5") || EQUAL(osExt, "nc") ||
         EQUAL(osExt, "cdf") || EQUAL(osExt, "nc4")))
    {
        vsi_l_offset nOffset = 512;
        for (int i = 0; i < 64; i++)
        {
            GByte abyBuf[8];
            if (VSIFSeekL(poOpenInfo->fpL, nOffset, SEEK_SET) != 0 ||
                VSIFReadL(abyBuf, 1, 8, poOpenInfo->fpL) != 8)
            {
                break;
            }
            if (memcmp(abyBuf, achSignature, 8) == 0)
            {
                // Avoid opening NC files if the netCDF driver is available and
                // they are recognized by it.
                if (IsRecognizedByNetCDFDriver())
                {
                    return FALSE;
                }

                return TRUE;
            }
            nOffset *= 2;
        }
    }

    return FALSE;
}

/************************************************************************/
/*                     HDF5ImageDatasetIdentify()                       */
/************************************************************************/

int HDF5ImageDatasetIdentify(GDALOpenInfo *poOpenInfo)

{
    if (!STARTS_WITH_CI(poOpenInfo->pszFilename, "HDF5:"))
        return FALSE;

    return TRUE;
}

/************************************************************************/
/*                    HDF5DriverGetSubdatasetInfo()                     */
/************************************************************************/

struct HDF5DriverSubdatasetInfo : public GDALSubdatasetInfo
{
  public:
    explicit HDF5DriverSubdatasetInfo(const std::string &fileName)
        : GDALSubdatasetInfo(fileName)
    {
    }

    // GDALSubdatasetInfo interface
  private:
    void parseFileName() override
    {

        if (!STARTS_WITH_CI(m_fileName.c_str(), "HDF5:"))
        {
            return;
        }

        CPLStringList aosParts{CSLTokenizeString2(m_fileName.c_str(), ":", 0)};
        const int iPartsCount{CSLCount(aosParts)};

        if (iPartsCount >= 3)
        {

            m_driverPrefixComponent = aosParts[0];

            int subdatasetIndex{2};
            const bool hasDriveLetter{
                (strlen(aosParts[1]) == 2 && std::isalpha(aosParts[1][1])) ||
                (strlen(aosParts[1]) == 1 && std::isalpha(aosParts[1][0]))};

            m_pathComponent = aosParts[1];

            if (hasDriveLetter)
            {
                m_pathComponent.append(":");
                m_pathComponent.append(aosParts[2]);
                subdatasetIndex++;
            }

            m_subdatasetComponent = aosParts[subdatasetIndex];

            // Append any remaining part
            for (int i = subdatasetIndex + 1; i < iPartsCount; ++i)
            {
                m_subdatasetComponent.append(":");
                m_subdatasetComponent.append(aosParts[i]);
            }
        }
    }
};

GDALSubdatasetInfo *HDF5DriverGetSubdatasetInfo(const char *pszFileName)
{
    if (STARTS_WITH_CI(pszFileName, "HDF5:"))
    {
        std::unique_ptr<GDALSubdatasetInfo> info =
            std::make_unique<HDF5DriverSubdatasetInfo>(pszFileName);
        if (!info->GetSubdatasetComponent().empty() &&
            !info->GetPathComponent().empty())
        {
            return info.release();
        }
    }
    return nullptr;
}

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/

int S102DatasetIdentify(GDALOpenInfo *poOpenInfo)

{
    if (STARTS_WITH(poOpenInfo->pszFilename, "S102:"))
        return TRUE;

    // Is it an HDF5 file?
    static const char achSignature[] = "\211HDF\r\n\032\n";

    if (poOpenInfo->pabyHeader == nullptr ||
        memcmp(poOpenInfo->pabyHeader, achSignature, 8) != 0)
        return FALSE;

    // GDAL_S102_IDENTIFY can be set to NO only for tests, to test that
    // HDF5Dataset::Open() can redirect to S102 if the below logic fails
    if (CPLTestBool(CPLGetConfigOption("GDAL_S102_IDENTIFY", "YES")))
    {
        // The below identification logic may be a bit fragile...
        // Works at least on:
        // - /vsis3/noaa-s102-pds/ed2.1.0/national_bathymetric_source/boston/dcf2/tiles/102US00_US4MA1GC.h5
        // - https://datahub.admiralty.co.uk/portal/sharing/rest/content/items/6fd07bde26124d48820b6dee60695389/data (S-102_Liverpool_Trial_Cells.zip)
        const int nLenBC = static_cast<int>(strlen("BathymetryCoverage\0") + 1);
        const int nLenGroupF = static_cast<int>(strlen("Group_F\0") + 1);
        bool bFoundBathymetryCoverage = false;
        bool bFoundGroupF = false;
        for (int i = 0; i < poOpenInfo->nHeaderBytes - nLenBC; ++i)
        {
            if (poOpenInfo->pabyHeader[i] == 'B' &&
                memcmp(poOpenInfo->pabyHeader + i, "BathymetryCoverage\0",
                       nLenBC) == 0)
            {
                bFoundBathymetryCoverage = true;
                if (bFoundGroupF)
                    return true;
            }
            if (poOpenInfo->pabyHeader[i] == 'G' &&
                memcmp(poOpenInfo->pabyHeader + i, "Group_F\0", nLenGroupF) ==
                    0)
            {
                bFoundGroupF = true;
                if (bFoundBathymetryCoverage)
                    return true;
            }
        }
    }

    return false;
}

/************************************************************************/
/*                        BAGDatasetIdentify()                          */
/************************************************************************/

int BAGDatasetIdentify(GDALOpenInfo *poOpenInfo)

{
    if (STARTS_WITH(poOpenInfo->pszFilename, "BAG:"))
        return TRUE;

    // Is it an HDF5 file?
    static const char achSignature[] = "\211HDF\r\n\032\n";

    if (poOpenInfo->pabyHeader == nullptr ||
        memcmp(poOpenInfo->pabyHeader, achSignature, 8) != 0)
        return FALSE;

    // Does it have the extension .bag?
    if (!EQUAL(CPLGetExtension(poOpenInfo->pszFilename), "bag"))
        return FALSE;

    return TRUE;
}

/************************************************************************/
/*                    DeclareDeferredHDF5Plugin()                       */
/************************************************************************/

#ifdef PLUGIN_FILENAME
void DeclareDeferredHDF5Plugin()
{
    if (GDALGetDriverByName(HDF5_DRIVER_NAME) != nullptr)
    {
        return;
    }
    {
        GDALPluginDriverFeatures oFeatures;
        oFeatures.pfnIdentify = HDF5DatasetIdentify;
        oFeatures.pfnGetSubdatasetInfoFunc = HDF5DriverGetSubdatasetInfo;
        oFeatures.pszLongName = HDF5_LONG_NAME;
        oFeatures.pszExtensions = HDF5_EXTENSIONS;
        oFeatures.bHasRasterCapabilities = true;
        oFeatures.bHasMultiDimRasterCapabilities = true;
        oFeatures.bHasSubdatasets = true;
        GetGDALDriverManager()->DeclareDeferredPluginDriver(
            HDF5_DRIVER_NAME, PLUGIN_FILENAME, oFeatures);
    }
    {
        GDALPluginDriverFeatures oFeatures;
        oFeatures.pfnIdentify = HDF5ImageDatasetIdentify;
        oFeatures.pszLongName = HDF5_IMAGE_LONG_NAME;
        oFeatures.bHasRasterCapabilities = true;
        GetGDALDriverManager()->DeclareDeferredPluginDriver(
            HDF5_IMAGE_DRIVER_NAME, PLUGIN_FILENAME, oFeatures);
    }
    {
        GDALPluginDriverFeatures oFeatures;
        oFeatures.pfnIdentify = BAGDatasetIdentify;
        oFeatures.pszLongName = BAG_LONG_NAME;
        oFeatures.pszExtensions = BAG_EXTENSIONS;
        oFeatures.pszOpenOptionList = BAG_OPENOPTIONLIST;
        oFeatures.bHasRasterCapabilities = true;
        oFeatures.bHasVectorCapabilities = true;
        oFeatures.bHasMultiDimRasterCapabilities = true;
        oFeatures.bHasCreate = true;
        oFeatures.bHasCreateCopy = true;
        GetGDALDriverManager()->DeclareDeferredPluginDriver(
            BAG_DRIVER_NAME, PLUGIN_FILENAME, oFeatures);
    }
    {
        GDALPluginDriverFeatures oFeatures;
        oFeatures.pfnIdentify = S102DatasetIdentify;
        oFeatures.pszLongName = S102_LONG_NAME;
        oFeatures.pszExtensions = S102_EXTENSIONS;
        oFeatures.bHasRasterCapabilities = true;
        oFeatures.bHasMultiDimRasterCapabilities = true;
        GetGDALDriverManager()->DeclareDeferredPluginDriver(
            S102_DRIVER_NAME, PLUGIN_FILENAME, oFeatures);
    }
}
#endif
