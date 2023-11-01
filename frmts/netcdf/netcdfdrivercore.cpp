/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  netCDF driver
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

#include "ogrsf_frmts.h"
#include "gdal_priv.h"

#include "netcdfdrivercore.h"

/************************************************************************/
/*                      netCDFIdentifyFormat()                          */
/************************************************************************/

NetCDFFormatEnum netCDFIdentifyFormat(GDALOpenInfo *poOpenInfo, bool bCheckExt)
{
    // Does this appear to be a netcdf file? If so, which format?
    // http://www.unidata.ucar.edu/software/netcdf/docs/faq.html#fv1_5

    if (STARTS_WITH_CI(poOpenInfo->pszFilename, "NETCDF:"))
        return NCDF_FORMAT_UNKNOWN;
    if (poOpenInfo->nHeaderBytes < 4)
        return NCDF_FORMAT_NONE;
    const char *pszHeader =
        reinterpret_cast<const char *>(poOpenInfo->pabyHeader);

#ifdef ENABLE_NCDUMP
    if (poOpenInfo->fpL != nullptr && STARTS_WITH(pszHeader, "netcdf ") &&
        strstr(pszHeader, "dimensions:") && strstr(pszHeader, "variables:"))
    {
#ifdef NETCDF_HAS_NC4
        if (strstr(pszHeader, "// NC4C"))
            return NCDF_FORMAT_NC4C;
        else if (strstr(pszHeader, "// NC4"))
            return NCDF_FORMAT_NC4;
        else
#endif  // NETCDF_HAS_NC4
            return NCDF_FORMAT_NC;
    }
#endif  // ENABLE_NCDUMP

#ifdef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
    // We don't necessarily want to catch bugs in libnetcdf ...
    if (CPLGetConfigOption("DISABLE_OPEN_REAL_NETCDF_FILES", nullptr))
    {
        return NCDF_FORMAT_NONE;
    }
#endif

    constexpr char achHDF5Signature[] = "\211HDF\r\n\032\n";

    if (STARTS_WITH_CI(pszHeader, "CDF\001"))
    {
        // In case the netCDF driver is registered before the GMT driver,
        // avoid opening GMT files.
        if (GDALGetDriverByName("GMT") != nullptr)
        {
            bool bFoundZ = false;
            bool bFoundDimension = false;
            for (int i = 0; i < poOpenInfo->nHeaderBytes - 11; i++)
            {
                if (poOpenInfo->pabyHeader[i] == 1 &&
                    poOpenInfo->pabyHeader[i + 1] == 'z' &&
                    poOpenInfo->pabyHeader[i + 2] == 0)
                    bFoundZ = true;
                else if (poOpenInfo->pabyHeader[i] == 9 &&
                         memcmp((const char *)poOpenInfo->pabyHeader + i + 1,
                                "dimension", 9) == 0 &&
                         poOpenInfo->pabyHeader[i + 10] == 0)
                    bFoundDimension = true;
            }
            if (bFoundZ && bFoundDimension)
                return NCDF_FORMAT_UNKNOWN;
        }

        return NCDF_FORMAT_NC;
    }
    else if (STARTS_WITH_CI(pszHeader, "CDF\002"))
    {
        return NCDF_FORMAT_NC2;
    }
    else if (STARTS_WITH_CI(pszHeader, achHDF5Signature) ||
             (poOpenInfo->nHeaderBytes > 512 + 8 &&
              memcmp(pszHeader + 512, achHDF5Signature, 8) == 0))
    {
        // Requires netCDF-4/HDF5 support in libnetcdf (not just libnetcdf-v4).
        // If HDF5 is not supported in GDAL, this driver will try to open the
        // file Else, make sure this driver does not try to open HDF5 files If
        // user really wants to open with this driver, use NETCDF:file.h5
        // format.  This check should be relaxed, but there is no clear way to
        // make a difference.

// Check for HDF5 support in GDAL.
#ifdef HAVE_HDF5
        if (bCheckExt)
        {
            // Check by default.
            const char *pszExtension = CPLGetExtension(poOpenInfo->pszFilename);
            if (!(EQUAL(pszExtension, "nc") || EQUAL(pszExtension, "cdf") ||
                  EQUAL(pszExtension, "nc2") || EQUAL(pszExtension, "nc4") ||
                  EQUAL(pszExtension, "nc3") || EQUAL(pszExtension, "grd") ||
                  EQUAL(pszExtension, "gmac")))
            {
                if (GDALGetDriverByName("HDF5") != nullptr)
                {
                    return NCDF_FORMAT_HDF5;
                }
            }
        }
#endif

// Check for netcdf-4 support in libnetcdf.
#ifdef NETCDF_HAS_NC4
        return NCDF_FORMAT_NC4;
#else
        return NCDF_FORMAT_HDF5;
#endif
    }
    else if (STARTS_WITH_CI(pszHeader, "\016\003\023\001"))
    {
        // Requires HDF4 support in libnetcdf, but if HF4 is supported by GDAL
        // don't try to open.
        // If user really wants to open with this driver, use NETCDF:file.hdf
        // syntax.

// Check for HDF4 support in GDAL.
#ifdef HAVE_HDF4
        if (bCheckExt && GDALGetDriverByName("HDF4") != nullptr)
        {
            // Check by default.
            // Always treat as HDF4 file.
            return NCDF_FORMAT_HDF4;
        }
#endif

// Check for HDF4 support in libnetcdf.
#ifdef NETCDF_HAS_HDF4
        return NCDF_FORMAT_NC4;
#else
        return NCDF_FORMAT_HDF4;
#endif
    }

    // The HDF5 signature of netCDF 4 files can be at offsets 512, 1024, 2048,
    // etc.
    const char *pszExtension = CPLGetExtension(poOpenInfo->pszFilename);
    if (poOpenInfo->fpL != nullptr &&
        (!bCheckExt || EQUAL(pszExtension, "nc") ||
         EQUAL(pszExtension, "cdf") || EQUAL(pszExtension, "nc4")))
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
            if (memcmp(abyBuf, achHDF5Signature, 8) == 0)
            {
#ifdef NETCDF_HAS_NC4
                return NCDF_FORMAT_NC4;
#else
                return NCDF_FORMAT_HDF5;
#endif
            }
            nOffset *= 2;
        }
    }

    return NCDF_FORMAT_NONE;
}

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/

int netCDFDatasetIdentify(GDALOpenInfo *poOpenInfo)

{
    if (STARTS_WITH_CI(poOpenInfo->pszFilename, "NETCDF:"))
    {
        return TRUE;
    }
    const NetCDFFormatEnum eTmpFormat =
        netCDFIdentifyFormat(poOpenInfo,
                             /* bCheckExt = */ true);
    if (NCDF_FORMAT_NC == eTmpFormat || NCDF_FORMAT_NC2 == eTmpFormat ||
        NCDF_FORMAT_NC4 == eTmpFormat || NCDF_FORMAT_NC4C == eTmpFormat)
        return TRUE;

    return FALSE;
}

/************************************************************************/
/*                    NCDFDriverGetSubdatasetInfo()                     */
/************************************************************************/

struct NCDFDriverSubdatasetInfo : public GDALSubdatasetInfo
{
  public:
    explicit NCDFDriverSubdatasetInfo(const std::string &fileName)
        : GDALSubdatasetInfo(fileName)
    {
    }

    // GDALSubdatasetInfo interface
  private:
    void parseFileName() override
    {

        if (!STARTS_WITH_CI(m_fileName.c_str(), "NETCDF:"))
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

GDALSubdatasetInfo *NCDFDriverGetSubdatasetInfo(const char *pszFileName)
{
    if (STARTS_WITH_CI(pszFileName, "NETCDF:"))
    {
        std::unique_ptr<GDALSubdatasetInfo> info =
            std::make_unique<NCDFDriverSubdatasetInfo>(pszFileName);
        if (!info->GetSubdatasetComponent().empty() &&
            !info->GetPathComponent().empty())
        {
            return info.release();
        }
    }
    return nullptr;
}

/************************************************************************/
/*                    DeclareDeferredNetCDFPlugin()                     */
/************************************************************************/

#ifdef PLUGIN_FILENAME
void DeclareDeferredNetCDFPlugin()
{
    if (GDALGetDriverByName(DRIVER_NAME) != nullptr)
    {
        return;
    }
    GDALPluginDriverFeatures oFeatures;
    oFeatures.pfnIdentify = netCDFDatasetIdentify;
    oFeatures.pfnGetSubdatasetInfoFunc = NCDFDriverGetSubdatasetInfo;
    oFeatures.pszLongName = LONG_NAME;
    oFeatures.pszExtensions = EXTENSIONS;
    oFeatures.pszOpenOptionList = OPENOPTIONLIST;
    oFeatures.bHasRasterCapabilities = true;
    oFeatures.bHasVectorCapabilities = true;
#ifdef NETCDF_HAS_NC4
    oFeatures.bHasMultiDimRasterCapabilities = true;
#endif
    oFeatures.bHasCreate = true;
    oFeatures.bHasCreateMultiDimensional = true;
    oFeatures.bHasCreateCopy = true;
    oFeatures.bHasSubdatasets = true;
    GetGDALDriverManager()->DeclareDeferredPluginDriver(
        DRIVER_NAME, PLUGIN_FILENAME, oFeatures);
}
#endif
