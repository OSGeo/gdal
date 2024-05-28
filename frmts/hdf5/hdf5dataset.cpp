/******************************************************************************
 *
 * Project:  Hierarchical Data Format Release 5 (HDF5)
 * Purpose:  HDF5 Datasets. Open HDF5 file, fetch metadata and list of
 *           subdatasets.
 *           This driver initially based on code supplied by Markus Neteler
 * Author:  Denis Nadeau <denis.nadeau@gmail.com>
 *
 ******************************************************************************
 * Copyright (c) 2005, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2008-2018, Even Rouault <even.rouault at spatialys.com>
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

#include "cpl_port.h"

#include "hdf5_api.h"
#include "hdf5dataset.h"
#include "hdf5drivercore.h"
#include "hdf5vfl.h"

#include <algorithm>
#include <stdio.h>
#include <string.h>
#include <string>
#include <cctype>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_float.h"
#include "cpl_string.h"
#include "gdal.h"
#include "gdal_frmts.h"
#include "gdal_priv.h"

constexpr size_t MAX_METADATA_LEN = 32768;

#ifdef ENABLE_HDF5_GLOBAL_LOCK

/************************************************************************/
/*                          GetHDF5GlobalMutex()                        */
/************************************************************************/

std::recursive_mutex &GetHDF5GlobalMutex()
{
    static std::recursive_mutex oMutex;
    return oMutex;
}

#endif

/************************************************************************/
/*                          HDF5GetFileDriver()                         */
/************************************************************************/

hid_t HDF5GetFileDriver()
{
    return HDF5VFLGetFileDriver();
}

/************************************************************************/
/*                        HDF5UnloadFileDriver()                        */
/************************************************************************/

void HDF5UnloadFileDriver()
{
    HDF5VFLUnloadFileDriver();
}

/************************************************************************/
/*                     HDF5DatasetDriverUnload()                        */
/************************************************************************/

static void HDF5DatasetDriverUnload(GDALDriver *)
{
    HDF5UnloadFileDriver();
}

/************************************************************************/
/* ==================================================================== */
/*                              HDF5Dataset                             */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                        GDALRegister_HDF5()                           */
/************************************************************************/
void GDALRegister_HDF5()

{
    if (GDALGetDriverByName(HDF5_DRIVER_NAME) != nullptr)
        return;

    GDALDriver *poDriver = new GDALDriver();

    HDF5DriverSetCommonMetadata(poDriver);

    poDriver->pfnOpen = HDF5Dataset::Open;
    poDriver->pfnUnloadDriver = HDF5DatasetDriverUnload;
    GetGDALDriverManager()->RegisterDriver(poDriver);

#ifdef HDF5_PLUGIN
    GDALRegister_HDF5Image();
    GDALRegister_BAG();
    GDALRegister_S102();
    GDALRegister_S104();
    GDALRegister_S111();
#endif
}

/************************************************************************/
/*                           HDF5Dataset()                              */
/************************************************************************/
HDF5Dataset::HDF5Dataset()
    : hGroupID(-1), papszSubDatasets(nullptr), nDatasetType(-1),
      nSubDataCount(0), poH5RootGroup(nullptr)
{
}

/************************************************************************/
/*                            ~HDF5Dataset()                            */
/************************************************************************/
HDF5Dataset::~HDF5Dataset()
{
    HDF5_GLOBAL_LOCK();

    if (hGroupID > 0)
        H5Gclose(hGroupID);
    if (m_hHDF5 > 0)
        H5Fclose(m_hHDF5);

    CSLDestroy(papszSubDatasets);
    if (poH5RootGroup != nullptr)
    {
        DestroyH5Objects(poH5RootGroup);
        CPLFree(poH5RootGroup->pszName);
        CPLFree(poH5RootGroup->pszPath);
        CPLFree(poH5RootGroup->pszUnderscorePath);
        CPLFree(poH5RootGroup->poHchild);
        CPLFree(poH5RootGroup);
    }
}

/************************************************************************/
/*                            GetDataType()                             */
/*                                                                      */
/*      Transform HDF5 datatype to GDAL datatype                        */
/************************************************************************/
GDALDataType HDF5Dataset::GetDataType(hid_t TypeID)
{
    // Check for native types first
    if (H5Tget_class(TypeID) != H5T_COMPOUND)
    {

        if (H5Tequal(H5T_NATIVE_SCHAR, TypeID))
            return GDT_Int8;
        else if (H5Tequal(H5T_NATIVE_CHAR, TypeID) ||
                 H5Tequal(H5T_NATIVE_UCHAR, TypeID))
            return GDT_Byte;
        else if (H5Tequal(H5T_NATIVE_SHORT, TypeID))
            return GDT_Int16;
        else if (H5Tequal(H5T_NATIVE_USHORT, TypeID))
            return GDT_UInt16;
        else if (H5Tequal(H5T_NATIVE_INT, TypeID))
            return GDT_Int32;
        else if (H5Tequal(H5T_NATIVE_UINT, TypeID))
            return GDT_UInt32;
        else if (H5Tequal(H5T_NATIVE_INT64, TypeID))
            return GDT_Int64;
        else if (H5Tequal(H5T_NATIVE_UINT64, TypeID))
            return GDT_UInt64;
        else if (H5Tequal(H5T_NATIVE_LONG, TypeID))
        {
#if SIZEOF_UNSIGNED_LONG == 4
            return GDT_Int32;
#else
            return GDT_Unknown;
#endif
        }
        else if (H5Tequal(H5T_NATIVE_ULONG, TypeID))
        {
#if SIZEOF_UNSIGNED_LONG == 4
            return GDT_UInt32;
#else
            return GDT_Unknown;
#endif
        }
#ifdef HDF5_HAVE_FLOAT16
        else if (H5Tequal(H5T_NATIVE_FLOAT16, TypeID))
            return GDT_Float32;
#endif
        else if (H5Tequal(H5T_NATIVE_FLOAT, TypeID))
            return GDT_Float32;
        else if (H5Tequal(H5T_NATIVE_DOUBLE, TypeID))
            return GDT_Float64;
        else if (H5Tequal(H5T_NATIVE_LLONG, TypeID))
            return GDT_Unknown;
        else if (H5Tequal(H5T_NATIVE_ULLONG, TypeID))
            return GDT_Unknown;
    }
    else  // Parse compound type to determine if data is complex
    {
        // For complex the compound type must contain 2 elements
        if (H5Tget_nmembers(TypeID) != 2)
            return GDT_Unknown;

        // For complex the native types of both elements should be the same
        hid_t ElemTypeID = H5Tget_member_type(TypeID, 0);
        hid_t Elem2TypeID = H5Tget_member_type(TypeID, 1);
        const bool bTypeEqual = H5Tequal(ElemTypeID, Elem2TypeID) > 0;
        H5Tclose(Elem2TypeID);
        if (!bTypeEqual)
        {
            H5Tclose(ElemTypeID);
            return GDT_Unknown;
        }

        char *pszName1 = H5Tget_member_name(TypeID, 0);
        const bool bIsReal =
            pszName1 && (pszName1[0] == 'r' || pszName1[0] == 'R');
        H5free_memory(pszName1);

        char *pszName2 = H5Tget_member_name(TypeID, 1);
        const bool bIsImaginary =
            pszName2 && (pszName2[0] == 'i' || pszName2[0] == 'I');
        H5free_memory(pszName2);

        if (!bIsReal || !bIsImaginary)
        {
            H5Tclose(ElemTypeID);
            return GDT_Unknown;
        }

        // Check the native types to determine CInt16, CFloat32 or CFloat64
        GDALDataType eDataType = GDT_Unknown;

        if (H5Tequal(H5T_NATIVE_SHORT, ElemTypeID))
            eDataType = GDT_CInt16;
        else if (H5Tequal(H5T_NATIVE_INT, ElemTypeID))
            eDataType = GDT_CInt32;
        else if (H5Tequal(H5T_NATIVE_LONG, ElemTypeID))
        {
#if SIZEOF_UNSIGNED_LONG == 4
            eDataType = GDT_CInt32;
#else
            eDataType = GDT_Unknown;
#endif
        }
#ifdef HDF5_HAVE_FLOAT16
        else if (H5Tequal(H5T_NATIVE_FLOAT16, ElemTypeID))
            eDataType = GDT_CFloat32;
#endif
        else if (H5Tequal(H5T_NATIVE_FLOAT, ElemTypeID))
            eDataType = GDT_CFloat32;
        else if (H5Tequal(H5T_NATIVE_DOUBLE, ElemTypeID))
            eDataType = GDT_CFloat64;

        // Close the data type
        H5Tclose(ElemTypeID);

        return eDataType;
    }

    return GDT_Unknown;
}

/************************************************************************/
/*                          IsNativeCFloat16()                          */
/************************************************************************/

/* static*/ bool HDF5Dataset::IsNativeCFloat16(hid_t hDataType)
{
#ifdef HDF5_HAVE_FLOAT16
    // For complex the compound type must contain 2 elements
    if (H5Tget_class(hDataType) != H5T_COMPOUND ||
        H5Tget_nmembers(hDataType) != 2)
        return false;

    // For complex the native types of both elements should be the same
    hid_t ElemTypeID = H5Tget_member_type(hDataType, 0);
    hid_t Elem2TypeID = H5Tget_member_type(hDataType, 1);
    const bool bRet = H5Tequal(ElemTypeID, H5T_NATIVE_FLOAT16) > 0 &&
                      H5Tequal(Elem2TypeID, H5T_NATIVE_FLOAT16) > 0;
    H5Tclose(ElemTypeID);
    H5Tclose(Elem2TypeID);
    return bRet;
#else
    CPL_IGNORE_RET_VAL(hDataType);
    return false;
#endif
}

/************************************************************************/
/*                          GetDataTypeName()                           */
/*                                                                      */
/*      Return the human readable name of data type                     */
/************************************************************************/
const char *HDF5Dataset::GetDataTypeName(hid_t TypeID)
{
    // Check for native types first
    if (H5Tget_class(TypeID) != H5T_COMPOUND)
    {
        if (H5Tequal(H5T_NATIVE_CHAR, TypeID))
            return "8-bit character";
        else if (H5Tequal(H5T_NATIVE_SCHAR, TypeID))
            return "8-bit signed character";
        else if (H5Tequal(H5T_NATIVE_UCHAR, TypeID))
            return "8-bit unsigned character";
        else if (H5Tequal(H5T_NATIVE_SHORT, TypeID))
            return "16-bit integer";
        else if (H5Tequal(H5T_NATIVE_USHORT, TypeID))
            return "16-bit unsigned integer";
        else if (H5Tequal(H5T_NATIVE_INT, TypeID))
            return "32-bit integer";
        else if (H5Tequal(H5T_NATIVE_UINT, TypeID))
            return "32-bit unsigned integer";
        else if (H5Tequal(H5T_NATIVE_INT64, TypeID))
            return "64-bit integer";
        else if (H5Tequal(H5T_NATIVE_UINT64, TypeID))
            return "64-bit unsigned integer";
        else if (H5Tequal(H5T_NATIVE_LONG, TypeID))
            return "32/64-bit integer";
        else if (H5Tequal(H5T_NATIVE_ULONG, TypeID))
            return "32/64-bit unsigned integer";
#ifdef HDF5_HAVE_FLOAT16
        else if (H5Tequal(H5T_NATIVE_FLOAT16, TypeID))
            return "16-bit floating-point";
#endif
        else if (H5Tequal(H5T_NATIVE_FLOAT, TypeID))
            return "32-bit floating-point";
        else if (H5Tequal(H5T_NATIVE_DOUBLE, TypeID))
            return "64-bit floating-point";
        else if (H5Tequal(H5T_NATIVE_LLONG, TypeID))
            return "64-bit integer";
        else if (H5Tequal(H5T_NATIVE_ULLONG, TypeID))
            return "64-bit unsigned integer";
        else if (H5Tequal(H5T_NATIVE_DOUBLE, TypeID))
            return "64-bit floating-point";
    }
    else
    {
        // For complex the compound type must contain 2 elements
        if (H5Tget_nmembers(TypeID) != 2)
            return "Unknown";

        // For complex the native types of both elements should be the same
        hid_t ElemTypeID = H5Tget_member_type(TypeID, 0);
        hid_t Elem2TypeID = H5Tget_member_type(TypeID, 1);
        const bool bTypeEqual = H5Tequal(ElemTypeID, Elem2TypeID) > 0;
        H5Tclose(Elem2TypeID);
        if (!bTypeEqual)
        {
            H5Tclose(ElemTypeID);
            return "Unknown";
        }

        // Check the native types to determine CInt16, CFloat32 or CFloat64
        if (H5Tequal(H5T_NATIVE_SHORT, ElemTypeID))
        {
            H5Tclose(ElemTypeID);
            return "complex, 16-bit integer";
        }
        else if (H5Tequal(H5T_NATIVE_INT, ElemTypeID))
        {
            H5Tclose(ElemTypeID);
            return "complex, 32-bit integer";
        }
        else if (H5Tequal(H5T_NATIVE_LONG, ElemTypeID))
        {
            H5Tclose(ElemTypeID);
            return "complex, 32/64-bit integer";
        }
#ifdef HDF5_HAVE_FLOAT16
        else if (H5Tequal(H5T_NATIVE_FLOAT16, ElemTypeID))
        {
            H5Tclose(ElemTypeID);
            return "complex, 16-bit floating-point";
        }
#endif
        else if (H5Tequal(H5T_NATIVE_FLOAT, ElemTypeID))
        {
            H5Tclose(ElemTypeID);
            return "complex, 32-bit floating-point";
        }
        else if (H5Tequal(H5T_NATIVE_DOUBLE, ElemTypeID))
        {
            H5Tclose(ElemTypeID);
            return "complex, 64-bit floating-point";
        }
    }

    return "Unknown";
}

/************************************************************************/
/*                         GDAL_HDF5Open()                              */
/************************************************************************/
hid_t GDAL_HDF5Open(const std::string &osFilename)
{
    hid_t hHDF5;
    // Heuristics to able datasets split over several files, using the 'family'
    // driver. If passed the first file, and it contains a single 0, or
    // ends up with 0.h5 or 0.hdf5, replace the 0 with %d and try the family
    // driver.
    if (std::count(osFilename.begin(), osFilename.end(), '0') == 1 ||
        osFilename.find("0.h5") != std::string::npos ||
        osFilename.find("0.hdf5") != std::string::npos)
    {
        const auto zero_pos = osFilename.rfind('0');
        const auto osNewName = osFilename.substr(0, zero_pos) + "%d" +
                               osFilename.substr(zero_pos + 1);
        hid_t fapl = H5Pcreate(H5P_FILE_ACCESS);
        H5Pset_fapl_family(fapl, H5F_FAMILY_DEFAULT, H5P_DEFAULT);
#ifdef HAVE_GCC_WARNING_ZERO_AS_NULL_POINTER_CONSTANT
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
#endif
        H5E_BEGIN_TRY
        {
            hHDF5 = H5Fopen(osNewName.c_str(), H5F_ACC_RDONLY, fapl);
        }
        H5E_END_TRY;
#ifdef HAVE_GCC_WARNING_ZERO_AS_NULL_POINTER_CONSTANT
#pragma GCC diagnostic pop
#endif
        H5Pclose(fapl);
        if (hHDF5 >= 0)
        {
            CPLDebug("HDF5", "Actually opening %s with 'family' driver",
                     osNewName.c_str());
            return hHDF5;
        }
    }

    hid_t fapl = H5Pcreate(H5P_FILE_ACCESS);
    H5Pset_driver(fapl, HDF5GetFileDriver(), nullptr);
    hHDF5 = H5Fopen(osFilename.c_str(), H5F_ACC_RDONLY, fapl);
    H5Pclose(fapl);
    return hHDF5;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/
GDALDataset *HDF5Dataset::Open(GDALOpenInfo *poOpenInfo)
{
    if (!HDF5DatasetIdentify(poOpenInfo))
        return nullptr;

    HDF5_GLOBAL_LOCK();

    if (poOpenInfo->nOpenFlags & GDAL_OF_MULTIDIM_RASTER)
    {
        return OpenMultiDim(poOpenInfo);
    }

    // Create datasource.
    HDF5Dataset *const poDS = new HDF5Dataset();

    poDS->SetDescription(poOpenInfo->pszFilename);

    // Try opening the dataset.
    poDS->m_hHDF5 = GDAL_HDF5Open(poOpenInfo->pszFilename);
    if (poDS->m_hHDF5 < 0)
    {
        delete poDS;
        return nullptr;
    }

    poDS->hGroupID = H5Gopen(poDS->m_hHDF5, "/");
    if (poDS->hGroupID < 0)
    {
        delete poDS;
        return nullptr;
    }

    if (HDF5EOSParser::HasHDFEOS(poDS->hGroupID))
    {
        if (poDS->m_oHDFEOSParser.Parse(poDS->hGroupID))
        {
            CPLDebug("HDF5", "Successfully parsed HDFEOS metadata");
        }
    }

    poDS->ReadGlobalAttributes(true);

    if (STARTS_WITH(poDS->m_aosMetadata.FetchNameValueDef("mission_name", ""),
                    "Sentinel 3") &&
        EQUAL(
            poDS->m_aosMetadata.FetchNameValueDef("altimeter_sensor_name", ""),
            "SRAL") &&
        EQUAL(
            poDS->m_aosMetadata.FetchNameValueDef("radiometer_sensor_name", ""),
            "MWR") &&
        GDALGetDriverByName("netCDF") != nullptr)
    {
        delete poDS;
        return nullptr;
    }

    // Safety belt if S102Dataset::Identify() failed
    if (STARTS_WITH(
            poDS->m_aosMetadata.FetchNameValueDef("productSpecification", ""),
            "INT.IHO.S-102.") &&
        GDALGetDriverByName("S102") != nullptr)
    {
        delete poDS;
        std::string osS102Filename("S102:\"");
        osS102Filename +=
            CPLString(poOpenInfo->pszFilename).replaceAll("\"", "\\\"");
        osS102Filename += '"';
        return GDALDataset::Open(osS102Filename.c_str(), GDAL_OF_RASTER);
    }

    // Safety belt if S104Dataset::Identify() failed
    if (STARTS_WITH(
            poDS->m_aosMetadata.FetchNameValueDef("productSpecification", ""),
            "INT.IHO.S-104.") &&
        GDALGetDriverByName("S104") != nullptr)
    {
        delete poDS;
        std::string osS104Filename("S104:\"");
        osS104Filename +=
            CPLString(poOpenInfo->pszFilename).replaceAll("\"", "\\\"");
        osS104Filename += '"';
        return GDALDataset::Open(osS104Filename.c_str(), GDAL_OF_RASTER);
    }

    // Safety belt if S111Dataset::Identify() failed
    if (STARTS_WITH(
            poDS->m_aosMetadata.FetchNameValueDef("productSpecification", ""),
            "INT.IHO.S-111.") &&
        GDALGetDriverByName("S111") != nullptr)
    {
        delete poDS;
        std::string osS111Filename("S111:\"");
        osS111Filename +=
            CPLString(poOpenInfo->pszFilename).replaceAll("\"", "\\\"");
        osS111Filename += '"';
        return GDALDataset::Open(osS111Filename.c_str(), GDAL_OF_RASTER);
    }

    poDS->SetMetadata(poDS->m_aosMetadata.List());

    if (CSLCount(poDS->papszSubDatasets) / 2 >= 1)
        poDS->SetMetadata(poDS->papszSubDatasets, "SUBDATASETS");

    // Make sure we don't try to do any pam stuff with this dataset.
    poDS->nPamFlags |= GPF_NOSAVE;

    // If we have single subdataset only, open it immediately.
    int nSubDatasets = CSLCount(poDS->papszSubDatasets) / 2;
    if (nSubDatasets == 1)
    {
        CPLString osDSName =
            CSLFetchNameValue(poDS->papszSubDatasets, "SUBDATASET_1_NAME");
        delete poDS;
        return GDALDataset::Open(osDSName, poOpenInfo->nOpenFlags, nullptr,
                                 poOpenInfo->papszOpenOptions, nullptr);
    }
    else
    {
        // Confirm the requested access is supported.
        if (poOpenInfo->eAccess == GA_Update)
        {
            delete poDS;
            CPLError(CE_Failure, CPLE_NotSupported,
                     "The HDF5 driver does not support update access to "
                     "existing datasets.");
            return nullptr;
        }
    }
    return poDS;
}

/************************************************************************/
/*                          DestroyH5Objects()                          */
/*                                                                      */
/*      Erase all objects                                               */
/************************************************************************/
void HDF5Dataset::DestroyH5Objects(HDF5GroupObjects *poH5Object)
{
    // Visit all objects.
    for (unsigned i = 0; i < poH5Object->nbObjs; i++)
        DestroyH5Objects(poH5Object->poHchild + i);

    if (poH5Object->poHparent == nullptr)
        return;

    // Erase some data.
    CPLFree(poH5Object->paDims);
    poH5Object->paDims = nullptr;

    CPLFree(poH5Object->pszPath);
    poH5Object->pszPath = nullptr;

    CPLFree(poH5Object->pszName);
    poH5Object->pszName = nullptr;

    CPLFree(poH5Object->pszUnderscorePath);
    poH5Object->pszUnderscorePath = nullptr;

    if (poH5Object->native > 0)
        H5Tclose(poH5Object->native);
    poH5Object->native = 0;

    // All Children are visited and can be deleted.
    if (poH5Object->nbObjs != 0)
    {
        CPLFree(poH5Object->poHchild);
        poH5Object->poHchild = nullptr;
    }
}

/************************************************************************/
/*                             CreatePath()                             */
/*                                                                      */
/*      Find Dataset path for HDopen                                    */
/************************************************************************/
static void CreatePath(HDF5GroupObjects *poH5Object)
{
    // Recurse to the root path.
    CPLString osPath;
    if (poH5Object->poHparent != nullptr)
    {
        CreatePath(poH5Object->poHparent);
        osPath = poH5Object->poHparent->pszPath;
    }

    // Add name to the path.
    if (!EQUAL(poH5Object->pszName, "/"))
    {
        osPath.append("/");
        osPath.append(poH5Object->pszName);
    }

    // Fill up path for each object.
    CPLString osUnderscoreSpaceInName;
    if (poH5Object->pszPath == nullptr)
    {
        // This is completely useless but needed if we want to keep
        // subdataset names as they have "always" been formatted,
        // with double slash at the beginning
        if (osPath.empty())
            osPath = "/";

        // Change space for underscore.
        char **papszPath =
            CSLTokenizeString2(osPath.c_str(), " ", CSLT_HONOURSTRINGS);

        for (int i = 0; papszPath[i] != nullptr; i++)
        {
            if (i > 0)
                osUnderscoreSpaceInName.append("_");
            osUnderscoreSpaceInName.append(papszPath[i]);
        }
        CSLDestroy(papszPath);

        // -1 to give room for NUL in C strings.
        constexpr size_t MAX_PATH = 8192 - 1;
        // TODO(schwehr): Is it an issue if the results are longer than 8192?
        // It appears that the output can never be longer than the source.
        if (osUnderscoreSpaceInName.size() > MAX_PATH)
            CPLError(CE_Fatal, CPLE_AppDefined,
                     "osUnderscoreSpaceInName longer than MAX_PATH: "
                     "%u > %u",
                     static_cast<unsigned int>(osUnderscoreSpaceInName.size()),
                     static_cast<unsigned int>(MAX_PATH));
        if (osPath.size() > MAX_PATH)
            CPLError(CE_Fatal, CPLE_AppDefined,
                     "osPath longer than MAX_PATH: %u > %u",
                     static_cast<unsigned int>(osPath.size()),
                     static_cast<unsigned int>(MAX_PATH));

        poH5Object->pszUnderscorePath =
            CPLStrdup(osUnderscoreSpaceInName.c_str());
        poH5Object->pszPath = CPLStrdup(osPath.c_str());
    }
}

/************************************************************************/
/*                      HDF5GroupCheckDuplicate()                       */
/*                                                                      */
/*      Returns TRUE if an ancestor has the same objno[] as passed      */
/*      in - used to avoid looping in files with "links up" #(3218).    */
/************************************************************************/

static int HDF5GroupCheckDuplicate(HDF5GroupObjects *poHparent,
                                   unsigned long *objno)

{
    while (poHparent != nullptr)
    {
        if (poHparent->objno[0] == objno[0] && poHparent->objno[1] == objno[1])
            return TRUE;

        poHparent = poHparent->poHparent;
    }

    return FALSE;
}

/************************************************************************/
/*                      HDF5CreateGroupObjs()                           */
/*                                                                      */
/*      Create HDF5 hierarchy into a linked list                        */
/************************************************************************/
herr_t HDF5CreateGroupObjs(hid_t hHDF5, const char *pszObjName,
                           void *poHObjParent)
{
    HDF5GroupObjects *const poHparent =
        static_cast<HDF5GroupObjects *>(poHObjParent);
    HDF5GroupObjects *poHchild = poHparent->poHchild;
    H5G_stat_t oStatbuf;

    if (H5Gget_objinfo(hHDF5, pszObjName, FALSE, &oStatbuf) < 0)
        return -1;

    // Look for next child.
    unsigned idx = 0;  // idx is used after the for loop.
    for (; idx < poHparent->nbObjs; idx++)
    {
        if (poHchild->pszName == nullptr)
            break;
        poHchild++;
    }

    if (idx == poHparent->nbObjs)
        return -1;  // All children parsed.

    // Save child information.
    poHchild->pszName = CPLStrdup(pszObjName);

    poHchild->nType = oStatbuf.type;
    poHchild->nIndex = idx;
    poHchild->poHparent = poHparent;
    poHchild->nRank = 0;
    poHchild->paDims = nullptr;
    poHchild->HDatatype = 0;
    poHchild->objno[0] = oStatbuf.objno[0];
    poHchild->objno[1] = oStatbuf.objno[1];
    if (poHchild->pszPath == nullptr)
    {
        CreatePath(poHchild);
    }
    if (poHparent->pszPath == nullptr)
    {
        CreatePath(poHparent);
    }

    switch (oStatbuf.type)
    {
        case H5G_LINK:
        {
            poHchild->nbAttrs = 0;
            poHchild->nbObjs = 0;
            poHchild->poHchild = nullptr;
            poHchild->nRank = 0;
            poHchild->paDims = nullptr;
            poHchild->HDatatype = 0;
            break;
        }
        case H5G_GROUP:
        {
            hid_t hGroupID = H5I_INVALID_HID;  // Identifier of group.
            if ((hGroupID = H5Gopen(hHDF5, pszObjName)) == -1)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "unable to access \"%s\" group.", pszObjName);
                return -1;
            }
            // Number of attributes in object.
            const int nbAttrs = H5Aget_num_attrs(hGroupID);
            hsize_t nbObjs = 0;  // Number of objects in a group.
            H5Gget_num_objs(hGroupID, &nbObjs);
            poHchild->nbAttrs = nbAttrs;
            poHchild->nbObjs = static_cast<int>(nbObjs);
            poHchild->nRank = 0;
            poHchild->paDims = nullptr;
            poHchild->HDatatype = 0;

            if (nbObjs > 0)
            {
                poHchild->poHchild = static_cast<HDF5GroupObjects *>(CPLCalloc(
                    static_cast<int>(nbObjs), sizeof(HDF5GroupObjects)));
                memset(poHchild->poHchild, 0,
                       static_cast<size_t>(sizeof(HDF5GroupObjects) * nbObjs));
            }
            else
            {
                poHchild->poHchild = nullptr;
            }

            if (!HDF5GroupCheckDuplicate(poHparent, oStatbuf.objno))
                H5Giterate(hHDF5, pszObjName, nullptr, HDF5CreateGroupObjs,
                           poHchild);
            else
                CPLDebug("HDF5", "avoiding link looping on node '%s'.",
                         pszObjName);

            H5Gclose(hGroupID);
            break;
        }
        case H5G_DATASET:
        {
            hid_t hDatasetID = H5I_INVALID_HID;  // Identifier of dataset.
            if ((hDatasetID = H5Dopen(hHDF5, pszObjName)) == -1)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "unable to access \"%s\" dataset.", pszObjName);
                return -1;
            }
            const int nbAttrs = H5Aget_num_attrs(hDatasetID);
            const hid_t datatype = H5Dget_type(hDatasetID);
            const hid_t dataspace = H5Dget_space(hDatasetID);
            const int n_dims = H5Sget_simple_extent_ndims(dataspace);
            const hid_t native = H5Tget_native_type(datatype, H5T_DIR_ASCEND);
            hsize_t *maxdims = nullptr;
            hsize_t *dims = nullptr;

            if (n_dims > 0)
            {
                dims =
                    static_cast<hsize_t *>(CPLCalloc(n_dims, sizeof(hsize_t)));
                maxdims =
                    static_cast<hsize_t *>(CPLCalloc(n_dims, sizeof(hsize_t)));
            }
            H5Sget_simple_extent_dims(dataspace, dims, maxdims);
            if (maxdims != nullptr)
                CPLFree(maxdims);

            if (n_dims > 0)
            {
                poHchild->nRank = n_dims;        // rank of the array
                poHchild->paDims = dims;         // dimension of the array.
                poHchild->HDatatype = datatype;  // HDF5 datatype
            }
            else
            {
                poHchild->nRank = -1;
                poHchild->paDims = nullptr;
                poHchild->HDatatype = 0;
            }
            poHchild->nbAttrs = nbAttrs;
            poHchild->nbObjs = 0;
            poHchild->poHchild = nullptr;
            poHchild->native = native;
            H5Tclose(datatype);
            H5Sclose(dataspace);
            H5Dclose(hDatasetID);
            break;
        }
        case H5G_TYPE:
        {
            poHchild->nbAttrs = 0;
            poHchild->nbObjs = 0;
            poHchild->poHchild = nullptr;
            poHchild->nRank = 0;
            poHchild->paDims = nullptr;
            poHchild->HDatatype = 0;
            break;
        }
        default:
            break;
    }

    return 0;
}

/************************************************************************/
/*                     HDF5DatasetCreateMetadataContext                 */
/************************************************************************/

struct HDF5DatasetCreateMetadataContext
{
    std::string m_osKey{};
    CPLStringList &m_aosMetadata;

    // Work variables
    std::string m_osValue{};

    explicit HDF5DatasetCreateMetadataContext(CPLStringList &aosMetadata)
        : m_aosMetadata(aosMetadata)
    {
    }
};

/************************************************************************/
/*                          HDF5AttrIterate()                           */
/************************************************************************/

static herr_t HDF5AttrIterate(hid_t hH5ObjID, const char *pszAttrName,
                              void *pContext)
{
    HDF5DatasetCreateMetadataContext *const psContext =
        static_cast<HDF5DatasetCreateMetadataContext *>(pContext);

    psContext->m_osValue.clear();

    std::string osKey(psContext->m_osKey);
    // Convert whitespaces into "_" for the attribute name component
    const CPLStringList aosTokens(CSLTokenizeString2(
        pszAttrName, " ", CSLT_STRIPLEADSPACES | CSLT_STRIPENDSPACES));
    for (int i = 0; i < aosTokens.size(); ++i)
    {
        if (!osKey.empty())
            osKey += '_';
        osKey += aosTokens[i];
    }

    const hid_t hAttrID = H5Aopen_name(hH5ObjID, pszAttrName);
    const hid_t hAttrTypeID = H5Aget_type(hAttrID);
    const hid_t hAttrNativeType =
        H5Tget_native_type(hAttrTypeID, H5T_DIR_DEFAULT);
    const hid_t hAttrSpace = H5Aget_space(hAttrID);

    if (H5Tget_class(hAttrNativeType) == H5T_VLEN)
    {
        H5Sclose(hAttrSpace);
        H5Tclose(hAttrNativeType);
        H5Tclose(hAttrTypeID);
        H5Aclose(hAttrID);
        return 0;
    }

    hsize_t nSize[64] = {};
    const unsigned int nAttrDims =
        H5Sget_simple_extent_dims(hAttrSpace, nSize, nullptr);

    unsigned int nAttrElmts = 1;
    for (hsize_t i = 0; i < nAttrDims; i++)
    {
        nAttrElmts *= static_cast<int>(nSize[i]);
    }

    if (H5Tget_class(hAttrNativeType) == H5T_STRING)
    {
        if (H5Tis_variable_str(hAttrNativeType))
        {
            char **papszStrings =
                static_cast<char **>(CPLMalloc(nAttrElmts * sizeof(char *)));

            // Read the values.
            H5Aread(hAttrID, hAttrNativeType, papszStrings);

            // Concatenate all values as one string separated by a space.
            psContext->m_osValue = papszStrings[0] ? papszStrings[0] : "{NULL}";
            for (hsize_t i = 1; i < nAttrElmts; i++)
            {
                psContext->m_osValue += " ";
                psContext->m_osValue +=
                    papszStrings[i] ? papszStrings[i] : "{NULL}";
            }

            H5Dvlen_reclaim(hAttrNativeType, hAttrSpace, H5P_DEFAULT,
                            papszStrings);
            CPLFree(papszStrings);
        }
        else
        {
            const hsize_t nAttrSize = H5Aget_storage_size(hAttrID);
            psContext->m_osValue.resize(static_cast<size_t>(nAttrSize));
            H5Aread(hAttrID, hAttrNativeType, &psContext->m_osValue[0]);
        }
    }
    else
    {
        constexpr size_t nDataLen = 32;
        char szData[nDataLen];

        void *buf = nullptr;

        if (nAttrElmts > 0)
        {
            buf = CPLMalloc(nAttrElmts * H5Tget_size(hAttrNativeType));
            H5Aread(hAttrID, hAttrNativeType, buf);
        }
        const bool bIsSCHAR = H5Tequal(H5T_NATIVE_SCHAR, hAttrNativeType) > 0;
        const bool bIsUCHAR = H5Tequal(H5T_NATIVE_UCHAR, hAttrNativeType) > 0;
        if ((bIsSCHAR || bIsUCHAR) &&
            CPLTestBool(CPLGetConfigOption("GDAL_HDF5_CHAR_AS_STRING", "NO")))
        {
            // Compatibility mode with ancient GDAL versions where we consider
            // array of SCHAR/UCHAR as strings. Likely inappropriate mode...
            for (hsize_t i = 0; i < nAttrElmts; i++)
            {
                snprintf(szData, nDataLen, "%c", static_cast<char *>(buf)[i]);
                if (psContext->m_osValue.size() > MAX_METADATA_LEN)
                {
                    CPLError(CE_Warning, CPLE_OutOfMemory,
                             "Header data too long. Truncated");
                    break;
                }
                psContext->m_osValue += szData;
            }
        }
        else if (bIsSCHAR)
        {
            for (hsize_t i = 0; i < nAttrElmts; i++)
            {
                snprintf(szData, nDataLen, "%d",
                         static_cast<signed char *>(buf)[i]);
                if (psContext->m_osValue.size() > MAX_METADATA_LEN)
                {
                    CPLError(CE_Warning, CPLE_OutOfMemory,
                             "Header data too long. Truncated");
                    break;
                }
                if (i > 0)
                    psContext->m_osValue += ' ';
                psContext->m_osValue += szData;
            }
        }
        else if (bIsUCHAR)
        {
            for (hsize_t i = 0; i < nAttrElmts; i++)
            {
                snprintf(szData, nDataLen, "%u",
                         static_cast<unsigned char *>(buf)[i]);
                if (psContext->m_osValue.size() > MAX_METADATA_LEN)
                {
                    CPLError(CE_Warning, CPLE_OutOfMemory,
                             "Header data too long. Truncated");
                    break;
                }
                if (i > 0)
                    psContext->m_osValue += ' ';
                psContext->m_osValue += szData;
            }
        }
        else if (H5Tequal(H5T_NATIVE_SHORT, hAttrNativeType) > 0)
        {
            for (hsize_t i = 0; i < nAttrElmts; i++)
            {
                snprintf(szData, nDataLen, "%d", static_cast<short *>(buf)[i]);
                if (psContext->m_osValue.size() > MAX_METADATA_LEN)
                {
                    CPLError(CE_Warning, CPLE_OutOfMemory,
                             "Header data too long. Truncated");
                    break;
                }
                if (i > 0)
                    psContext->m_osValue += ' ';
                psContext->m_osValue += szData;
            }
        }
        else if (H5Tequal(H5T_NATIVE_USHORT, hAttrNativeType) > 0)
        {
            for (hsize_t i = 0; i < nAttrElmts; i++)
            {
                snprintf(szData, nDataLen, "%u",
                         static_cast<unsigned short *>(buf)[i]);
                if (psContext->m_osValue.size() > MAX_METADATA_LEN)
                {
                    CPLError(CE_Warning, CPLE_OutOfMemory,
                             "Header data too long. Truncated");
                    break;
                }
                if (i > 0)
                    psContext->m_osValue += ' ';
                psContext->m_osValue += szData;
            }
        }
        else if (H5Tequal(H5T_NATIVE_INT, hAttrNativeType) > 0)
        {
            for (hsize_t i = 0; i < nAttrElmts; i++)
            {
                snprintf(szData, nDataLen, "%d", static_cast<int *>(buf)[i]);
                if (psContext->m_osValue.size() > MAX_METADATA_LEN)
                {
                    CPLError(CE_Warning, CPLE_OutOfMemory,
                             "Header data too long. Truncated");
                    break;
                }
                if (i > 0)
                    psContext->m_osValue += ' ';
                psContext->m_osValue += szData;
            }
        }
        else if (H5Tequal(H5T_NATIVE_UINT, hAttrNativeType) > 0)
        {
            for (hsize_t i = 0; i < nAttrElmts; i++)
            {
                snprintf(szData, nDataLen, "%u",
                         static_cast<unsigned int *>(buf)[i]);
                if (psContext->m_osValue.size() > MAX_METADATA_LEN)
                {
                    CPLError(CE_Warning, CPLE_OutOfMemory,
                             "Header data too long. Truncated");
                    break;
                }
                if (i > 0)
                    psContext->m_osValue += ' ';
                psContext->m_osValue += szData;
            }
        }
        else if (H5Tequal(H5T_NATIVE_INT64, hAttrNativeType) > 0)
        {
            for (hsize_t i = 0; i < nAttrElmts; i++)
            {
                snprintf(szData, nDataLen, CPL_FRMT_GIB,
                         static_cast<GIntBig *>(buf)[i]);
                if (psContext->m_osValue.size() > MAX_METADATA_LEN)
                {
                    CPLError(CE_Warning, CPLE_OutOfMemory,
                             "Header data too long. Truncated");
                    break;
                }
                if (i > 0)
                    psContext->m_osValue += ' ';
                psContext->m_osValue += szData;
            }
        }
        else if (H5Tequal(H5T_NATIVE_UINT64, hAttrNativeType) > 0)
        {
            for (hsize_t i = 0; i < nAttrElmts; i++)
            {
                snprintf(szData, nDataLen, CPL_FRMT_GUIB,
                         static_cast<GUIntBig *>(buf)[i]);
                if (psContext->m_osValue.size() > MAX_METADATA_LEN)
                {
                    CPLError(CE_Warning, CPLE_OutOfMemory,
                             "Header data too long. Truncated");
                    break;
                }
                if (i > 0)
                    psContext->m_osValue += ' ';
                psContext->m_osValue += szData;
            }
        }
        else if (H5Tequal(H5T_NATIVE_LONG, hAttrNativeType) > 0)
        {
            for (hsize_t i = 0; i < nAttrElmts; i++)
            {
                snprintf(szData, nDataLen, "%ld", static_cast<long *>(buf)[i]);
                if (psContext->m_osValue.size() > MAX_METADATA_LEN)
                {
                    CPLError(CE_Warning, CPLE_OutOfMemory,
                             "Header data too long. Truncated");
                    break;
                }
                if (i > 0)
                    psContext->m_osValue += ' ';
                psContext->m_osValue += szData;
            }
        }
        else if (H5Tequal(H5T_NATIVE_ULONG, hAttrNativeType) > 0)
        {
            for (hsize_t i = 0; i < nAttrElmts; i++)
            {
                snprintf(szData, nDataLen, "%lu",
                         static_cast<unsigned long *>(buf)[i]);
                if (psContext->m_osValue.size() > MAX_METADATA_LEN)
                {
                    CPLError(CE_Warning, CPLE_OutOfMemory,
                             "Header data too long. Truncated");
                    break;
                }
                if (i > 0)
                    psContext->m_osValue += ' ';
                psContext->m_osValue += szData;
            }
        }
#ifdef HDF5_HAVE_FLOAT16
        else if (H5Tequal(H5T_NATIVE_FLOAT16, hAttrNativeType) > 0)
        {
            for (hsize_t i = 0; i < nAttrElmts; i++)
            {
                const uint16_t nVal16 = static_cast<uint16_t *>(buf)[i];
                const uint32_t nVal32 = CPLHalfToFloat(nVal16);
                float fVal;
                memcpy(&fVal, &nVal32, sizeof(fVal));
                CPLsnprintf(szData, nDataLen, "%.8g", fVal);
                if (psContext->m_osValue.size() > MAX_METADATA_LEN)
                {
                    CPLError(CE_Warning, CPLE_OutOfMemory,
                             "Header data too long. Truncated");
                    break;
                }
                if (i > 0)
                    psContext->m_osValue += ' ';
                psContext->m_osValue += szData;
            }
        }
#endif
        else if (H5Tequal(H5T_NATIVE_FLOAT, hAttrNativeType) > 0)
        {
            for (hsize_t i = 0; i < nAttrElmts; i++)
            {
                CPLsnprintf(szData, nDataLen, "%.8g",
                            static_cast<float *>(buf)[i]);
                if (psContext->m_osValue.size() > MAX_METADATA_LEN)
                {
                    CPLError(CE_Warning, CPLE_OutOfMemory,
                             "Header data too long. Truncated");
                    break;
                }
                if (i > 0)
                    psContext->m_osValue += ' ';
                psContext->m_osValue += szData;
            }
        }
        else if (H5Tequal(H5T_NATIVE_DOUBLE, hAttrNativeType) > 0)
        {
            for (hsize_t i = 0; i < nAttrElmts; i++)
            {
                CPLsnprintf(szData, nDataLen, "%.15g",
                            static_cast<double *>(buf)[i]);
                if (psContext->m_osValue.size() > MAX_METADATA_LEN)
                {
                    CPLError(CE_Warning, CPLE_OutOfMemory,
                             "Header data too long. Truncated");
                    break;
                }
                if (i > 0)
                    psContext->m_osValue += ' ';
                psContext->m_osValue += szData;
            }
        }
        CPLFree(buf);
    }
    H5Sclose(hAttrSpace);
    H5Tclose(hAttrNativeType);
    H5Tclose(hAttrTypeID);
    H5Aclose(hAttrID);
    psContext->m_aosMetadata.SetNameValue(osKey.c_str(),
                                          psContext->m_osValue.c_str());

    return 0;
}

/************************************************************************/
/*                           CreateMetadata()                           */
/************************************************************************/
CPLErr HDF5Dataset::CreateMetadata(hid_t hHDF5, HDF5GroupObjects *poH5Object,
                                   int nType, bool bPrefixWithDatasetName,
                                   CPLStringList &aosMetadata)
{

    if (!poH5Object->pszPath)
        return CE_None;

    if (EQUAL(poH5Object->pszPath, ""))
        return CE_None;

    const int nbAttrs = poH5Object->nbAttrs;

    HDF5DatasetCreateMetadataContext sContext(aosMetadata);

    if (bPrefixWithDatasetName)
    {
        // Convert "/" into "_" for the path component
        const char *pszPath = poH5Object->pszUnderscorePath;
        if (pszPath != nullptr && strlen(pszPath) > 0)
        {
            const CPLStringList aosTokens(
                CSLTokenizeString2(pszPath, "/", CSLT_HONOURSTRINGS));
            for (int i = 0; i < aosTokens.size(); ++i)
            {
                if (i != 0)
                    sContext.m_osKey += '_';
                sContext.m_osKey += aosTokens[i];
            }
        }
    }

    switch (nType)
    {
        case H5G_GROUP:
            if (nbAttrs > 0)
            {
                // Identifier of group.
                const hid_t l_hGroupID = H5Gopen(hHDF5, poH5Object->pszPath);
                H5Aiterate(l_hGroupID, nullptr, HDF5AttrIterate, &sContext);
                H5Gclose(l_hGroupID);
            }
            break;
        case H5G_DATASET:
            if (nbAttrs > 0)
            {
                const hid_t hDatasetID = H5Dopen(hHDF5, poH5Object->pszPath);
                H5Aiterate(hDatasetID, nullptr, HDF5AttrIterate, &sContext);
                H5Dclose(hDatasetID);
            }
            break;

        default:
            break;
    }

    return CE_None;
}

/************************************************************************/
/*                       HDF5FindDatasetObjectsbyPath()                 */
/*      Find object by name                                             */
/************************************************************************/
HDF5GroupObjects *
HDF5Dataset::HDF5FindDatasetObjectsbyPath(HDF5GroupObjects *poH5Objects,
                                          const char *pszDatasetPath)
{
    if (poH5Objects->nType == H5G_DATASET &&
        EQUAL(poH5Objects->pszUnderscorePath, pszDatasetPath))
    {

#ifdef DEBUG_VERBOSE
        printf("found it! %p\n", poH5Objects); /*ok*/
#endif
        return poH5Objects;
    }

    HDF5Dataset *const poDS = this;

    if (poH5Objects->nbObjs > 0)
    {
        for (unsigned int i = 0; i < poH5Objects->nbObjs; i++)
        {
            HDF5GroupObjects *poObjectsFound =
                poDS->HDF5FindDatasetObjectsbyPath(poH5Objects->poHchild + i,
                                                   pszDatasetPath);
            // Is this our dataset?
            if (poObjectsFound != nullptr)
                return poObjectsFound;
        }
    }
    // Dataset has not been found.
    return nullptr;
}

/************************************************************************/
/*                       HDF5FindDatasetObjects()                       */
/*      Find object by name                                             */
/************************************************************************/
HDF5GroupObjects *
HDF5Dataset::HDF5FindDatasetObjects(HDF5GroupObjects *poH5Objects,
                                    const char *pszDatasetName)
{
    if (poH5Objects->nType == H5G_DATASET &&
        EQUAL(poH5Objects->pszName, pszDatasetName))
    {

#ifdef DEBUG_VERBOSE
        printf("found it! %p\n", poH5Objects); /*ok*/
#endif
        return poH5Objects;
    }

    HDF5Dataset *poDS = this;

    if (poH5Objects->nbObjs > 0)
    {
        for (unsigned int i = 0; i < poH5Objects->nbObjs; i++)
        {
            HDF5GroupObjects *poObjectsFound = poDS->HDF5FindDatasetObjects(
                poH5Objects->poHchild + i, pszDatasetName);
            // Is this our dataset?
            if (poObjectsFound != nullptr)
                return poObjectsFound;
        }
    }

    // Dataset has not been found.
    return nullptr;
}

/************************************************************************/
/*                        HDF5ListGroupObjects()                        */
/*                                                                      */
/*      List all objects in HDF5                                        */
/************************************************************************/
CPLErr HDF5Dataset::HDF5ListGroupObjects(HDF5GroupObjects *poRootGroup,
                                         int bSUBDATASET)
{
    HDF5Dataset *poDS = this;

    if (poRootGroup->nbObjs > 0)
        for (hsize_t i = 0; i < poRootGroup->nbObjs; i++)
        {
            poDS->HDF5ListGroupObjects(poRootGroup->poHchild + i, bSUBDATASET);
        }

    if (poRootGroup->nType == H5G_GROUP)
    {
        CreateMetadata(m_hHDF5, poRootGroup, H5G_GROUP, true, m_aosMetadata);
    }

    // Create Sub dataset list.

    if (poRootGroup->nType == H5G_DATASET && bSUBDATASET &&
        poDS->GetDataType(poRootGroup->native) == GDT_Unknown)
    {
        if (!EQUAL(poRootGroup->pszUnderscorePath,
                   "//HDFEOS_INFORMATION/StructMetadata.0"))
        {
            CPLDebug("HDF5", "Skipping unsupported %s of type %s",
                     poRootGroup->pszUnderscorePath,
                     poDS->GetDataTypeName(poRootGroup->native));
        }
    }
    else if (poRootGroup->nType == H5G_DATASET && bSUBDATASET)
    {
        CreateMetadata(m_hHDF5, poRootGroup, H5G_DATASET, true, m_aosMetadata);

        CPLString osStr;
        switch (poRootGroup->nRank)
        {
            case 2:
                osStr.Printf("%dx%d", static_cast<int>(poRootGroup->paDims[0]),
                             static_cast<int>(poRootGroup->paDims[1]));
                break;
            case 3:
                osStr.Printf("%dx%dx%d",
                             static_cast<int>(poRootGroup->paDims[0]),
                             static_cast<int>(poRootGroup->paDims[1]),
                             static_cast<int>(poRootGroup->paDims[2]));
                break;
            default:
                return CE_None;
        }

        HDF5EOSParser::GridMetadata oGridMetadata;
        HDF5EOSParser::SwathDataFieldMetadata oSwathDataFieldMetadata;
        if (m_oHDFEOSParser.GetDataModel() == HDF5EOSParser::DataModel::GRID &&
            m_oHDFEOSParser.GetGridMetadata(poRootGroup->pszUnderscorePath,
                                            oGridMetadata) &&
            static_cast<int>(oGridMetadata.aoDimensions.size()) ==
                poRootGroup->nRank)
        {
            int nXDimSize = 0;
            int nYDimSize = 0;
            int nOtherDimSize = 0;
            std::string osOtherDimName;
            for (const auto &oDim : oGridMetadata.aoDimensions)
            {
                if (oDim.osName == "XDim")
                    nXDimSize = oDim.nSize;
                else if (oDim.osName == "YDim")
                    nYDimSize = oDim.nSize;
                else
                {
                    osOtherDimName = oDim.osName;
                    nOtherDimSize = oDim.nSize;
                }
            }
            switch (poRootGroup->nRank)
            {
                case 2:
                    osStr.Printf("(y=%d)x(x=%d)", nYDimSize, nXDimSize);
                    break;
                case 3:
                {
                    if (osOtherDimName == oGridMetadata.aoDimensions[0].osName)
                        osStr.Printf("(%s=%d)x(y=%d)x(x=%d)",
                                     osOtherDimName.c_str(), nOtherDimSize,
                                     nYDimSize, nXDimSize);
                    else
                        osStr.Printf("(y=%d)x(x=%d)x(%s=%d)", nYDimSize,
                                     nXDimSize, osOtherDimName.c_str(),
                                     nOtherDimSize);
                    break;
                }
                default:
                    break;
            }
        }
        else if (m_oHDFEOSParser.GetDataModel() ==
                     HDF5EOSParser::DataModel::SWATH &&
                 m_oHDFEOSParser.GetSwathDataFieldMetadata(
                     poRootGroup->pszUnderscorePath, oSwathDataFieldMetadata) &&
                 static_cast<int>(
                     oSwathDataFieldMetadata.aoDimensions.size()) ==
                     poRootGroup->nRank &&
                 oSwathDataFieldMetadata.iXDim >= 0 &&
                 oSwathDataFieldMetadata.iYDim >= 0)
        {
            const std::string &osXDimName =
                oSwathDataFieldMetadata
                    .aoDimensions[oSwathDataFieldMetadata.iXDim]
                    .osName;
            const int nXDimSize =
                oSwathDataFieldMetadata
                    .aoDimensions[oSwathDataFieldMetadata.iXDim]
                    .nSize;
            const std::string &osYDimName =
                oSwathDataFieldMetadata
                    .aoDimensions[oSwathDataFieldMetadata.iYDim]
                    .osName;
            const int nYDimSize =
                oSwathDataFieldMetadata
                    .aoDimensions[oSwathDataFieldMetadata.iYDim]
                    .nSize;
            switch (poRootGroup->nRank)
            {
                case 2:
                    osStr.Printf("(%s=%d)x(%s=%d)", osYDimName.c_str(),
                                 nYDimSize, osXDimName.c_str(), nXDimSize);
                    break;
                case 3:
                {
                    const std::string &osOtherDimName =
                        oSwathDataFieldMetadata
                            .aoDimensions[oSwathDataFieldMetadata.iOtherDim]
                            .osName;
                    const int nOtherDimSize =
                        oSwathDataFieldMetadata
                            .aoDimensions[oSwathDataFieldMetadata.iOtherDim]
                            .nSize;
                    if (oSwathDataFieldMetadata.iOtherDim == 0)
                    {
                        osStr.Printf("(%s=%d)x(%s=%d)x(%s=%d)",
                                     osOtherDimName.c_str(), nOtherDimSize,
                                     osYDimName.c_str(), nYDimSize,
                                     osXDimName.c_str(), nXDimSize);
                    }
                    else
                    {
                        osStr.Printf("(%s=%d)x(%s=%d)x(%s=%d)",
                                     osYDimName.c_str(), nYDimSize,
                                     osXDimName.c_str(), nXDimSize,
                                     osOtherDimName.c_str(), nOtherDimSize);
                    }
                    break;
                }
                default:
                    break;
            }
        }

        const std::string osDim = osStr;

        osStr.Printf("SUBDATASET_%d_NAME", ++(poDS->nSubDataCount));

        poDS->papszSubDatasets =
            CSLSetNameValue(poDS->papszSubDatasets, osStr.c_str(),
                            CPLSPrintf("HDF5:\"%s\":%s", poDS->GetDescription(),
                                       poRootGroup->pszUnderscorePath));

        osStr.Printf("SUBDATASET_%d_DESC", poDS->nSubDataCount);

        poDS->papszSubDatasets = CSLSetNameValue(
            poDS->papszSubDatasets, osStr.c_str(),
            CPLSPrintf("[%s] %s (%s)", osDim.c_str(),
                       poRootGroup->pszUnderscorePath,
                       poDS->GetDataTypeName(poRootGroup->native)));
    }

    return CE_None;
}

/************************************************************************/
/*                       ReadGlobalAttributes()                         */
/************************************************************************/
CPLErr HDF5Dataset::ReadGlobalAttributes(int bSUBDATASET)
{
    HDF5GroupObjects *poRootGroup =
        static_cast<HDF5GroupObjects *>(CPLCalloc(sizeof(HDF5GroupObjects), 1));

    poH5RootGroup = poRootGroup;
    poRootGroup->pszName = CPLStrdup("/");
    poRootGroup->nType = H5G_GROUP;
    poRootGroup->poHparent = nullptr;
    poRootGroup->pszPath = nullptr;
    poRootGroup->pszUnderscorePath = nullptr;

    if (m_hHDF5 < 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "hHDF5 < 0!");
        return CE_None;
    }

    H5G_stat_t oStatbuf = {{0, 0}, {0, 0}, 0, H5G_UNKNOWN, 0, 0, {0, 0, 0, 0}};

    if (H5Gget_objinfo(m_hHDF5, "/", FALSE, &oStatbuf) < 0)
        return CE_Failure;
    poRootGroup->objno[0] = oStatbuf.objno[0];
    poRootGroup->objno[1] = oStatbuf.objno[1];

    if (hGroupID > 0)
        H5Gclose(hGroupID);
    hGroupID = H5Gopen(m_hHDF5, "/");
    if (hGroupID < 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "hGroupId <0!");
        return CE_None;
    }

    poRootGroup->nbAttrs = H5Aget_num_attrs(hGroupID);

    H5Gget_num_objs(hGroupID, &(poRootGroup->nbObjs));

    if (poRootGroup->nbObjs > 0)
    {
        poRootGroup->poHchild = static_cast<HDF5GroupObjects *>(
            CPLCalloc(static_cast<size_t>(poRootGroup->nbObjs),
                      sizeof(HDF5GroupObjects)));
        H5Giterate(hGroupID, "/", nullptr, HDF5CreateGroupObjs, poRootGroup);
    }
    else
    {
        poRootGroup->poHchild = nullptr;
    }

    HDF5ListGroupObjects(poRootGroup, bSUBDATASET);
    return CE_None;
}

/**
 * Reads an array of double attributes from the HDF5 metadata.
 * It reads the attributes directly on its binary form directly,
 * thus avoiding string conversions.
 *
 * Important: It allocates the memory for the attributes internally,
 * so the caller must free the returned array after using it.
 * @param pszAttrFullPath Name of the attribute to be read.
 *        the attribute name must be the form:
 *            root attribute name
 *            SUBDATASET/subdataset attribute name
 * @param pdfValues pointer which will store the array of doubles read.
 * @param nLen it stores the length of the array read. If NULL it doesn't
 *        inform the length of the array.
 * @return CPLErr CE_None in case of success, CE_Failure in case of failure
 */
CPLErr HDF5Dataset::HDF5ReadDoubleAttr(const char *pszAttrFullPath,
                                       double **pdfValues, int *nLen)
{
    CPLString osAttrFullPath(pszAttrFullPath);

    // Search for the last "/" in order to get the path to the attribute.
    const size_t nSlashPos = osAttrFullPath.find_last_of("/");

    CPLString osObjName;
    CPLString osAttrName;

    // If objects name have been found.
    if (nSlashPos != CPLString::npos)
    {
        // Split Object name (dataset, group).
        osObjName = osAttrFullPath.substr(0, nSlashPos);
        // Split attribute name.
        osAttrName = osAttrFullPath.substr(nSlashPos + 1);
    }
    else
    {
        // By default the group is root, and
        // the attribute is the full path.
        osObjName = "/";
        osAttrName = pszAttrFullPath;
    }

    const hid_t hObjAttrID = H5Oopen(m_hHDF5, osObjName.c_str(), H5P_DEFAULT);

    CPLErr retVal = CE_Failure;

    if (hObjAttrID < 0)
    {
        CPLError(CE_Failure, CPLE_OpenFailed, "Object %s could not be opened",
                 pszAttrFullPath);
        retVal = CE_Failure;
    }
    else
    {
        // Open attribute handler by name, from the object handler opened
        // earlier.
        const hid_t hAttrID = H5Aopen_name(hObjAttrID, osAttrName.c_str());

        // Check for errors opening the attribute.
        if (hAttrID < 0)
        {
            CPLError(CE_Failure, CPLE_OpenFailed,
                     "Attribute %s could not be opened", pszAttrFullPath);
            retVal = CE_Failure;
        }
        else
        {
            const hid_t hAttrTypeID = H5Aget_type(hAttrID);
            const hid_t hAttrNativeType =
                H5Tget_native_type(hAttrTypeID, H5T_DIR_DEFAULT);
            const hid_t hAttrSpace = H5Aget_space(hAttrID);
            hsize_t nSize[64] = {};
            const unsigned int nAttrDims =
                H5Sget_simple_extent_dims(hAttrSpace, nSize, nullptr);

            if (!H5Tequal(H5T_NATIVE_DOUBLE, hAttrNativeType))
            {
                CPLError(CE_Failure, CPLE_OpenFailed,
                         "Attribute %s is not of type double", pszAttrFullPath);
                retVal = CE_Failure;
            }
            else
            {
                // Get the amount of elements.
                unsigned int nAttrElmts = 1;
                for (hsize_t i = 0; i < nAttrDims; i++)
                {
                    // For multidimensional attributes
                    nAttrElmts *= static_cast<unsigned int>(nSize[i]);
                }

                if (nLen != nullptr)
                    *nLen = nAttrElmts;

                *pdfValues = static_cast<double *>(
                    CPLMalloc(nAttrElmts * sizeof(double)));

                // Read the attribute contents
                if (H5Aread(hAttrID, hAttrNativeType, *pdfValues) < 0)
                {
                    CPLError(CE_Failure, CPLE_OpenFailed,
                             "Attribute %s could not be opened",
                             pszAttrFullPath);
                    retVal = CE_Failure;
                }
                else
                {
                    retVal = CE_None;
                }
            }

            H5Tclose(hAttrNativeType);
            H5Tclose(hAttrTypeID);
            H5Sclose(hAttrSpace);
            H5Aclose(hAttrID);
        }
        H5Oclose(hObjAttrID);
    }

    return retVal;
}
