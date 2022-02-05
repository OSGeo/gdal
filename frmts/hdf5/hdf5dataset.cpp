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
#include "hdf5vfl.h"

#include <algorithm>
#include <stdio.h>
#include <string.h>
#include <string>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_string.h"
#include "gdal.h"
#include "gdal_frmts.h"
#include "gdal_priv.h"

CPL_CVSID("$Id$")

constexpr size_t MAX_METADATA_LEN = 32768;

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

static void HDF5DatasetDriverUnload(GDALDriver*)
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
    if( GDALGetDriverByName("HDF5") != nullptr )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription("HDF5");
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME,
                              "Hierarchical Data Format Release 5");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drivers/raster/hdf5.html");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSIONS, "h5 hdf5");
    poDriver->SetMetadataItem(GDAL_DMD_SUBDATASETS, "YES");
    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

    poDriver->SetMetadataItem( GDAL_DCAP_MULTIDIM_RASTER, "YES" );

    poDriver->pfnOpen = HDF5Dataset::Open;
    poDriver->pfnIdentify = HDF5Dataset::Identify;
    poDriver->pfnUnloadDriver = HDF5DatasetDriverUnload;
    GetGDALDriverManager()->RegisterDriver(poDriver);

#ifdef HDF5_PLUGIN
    GDALRegister_HDF5Image();
    GDALRegister_BAG();
#endif
}

/************************************************************************/
/*                           HDF5Dataset()                              */
/************************************************************************/
HDF5Dataset::HDF5Dataset() :
    hHDF5(-1),
    hGroupID(-1),
    papszSubDatasets(nullptr),
    bIsHDFEOS(FALSE),
    nDatasetType(-1),
    nSubDataCount(0),
    poH5RootGroup(nullptr),
    papszMetadata(nullptr),
    poH5CurrentObject(nullptr)
{}

/************************************************************************/
/*                            ~HDF5Dataset()                            */
/************************************************************************/
HDF5Dataset::~HDF5Dataset()
{
    CSLDestroy(papszMetadata);
    if( hGroupID > 0 )
        H5Gclose(hGroupID);
    if( hHDF5 > 0 )
        H5Fclose(hHDF5);

    CSLDestroy(papszSubDatasets);
    if( poH5RootGroup != nullptr )
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
    //Check for native types first
    if (H5Tget_class(TypeID) != H5T_COMPOUND)
    {

        if( H5Tequal(H5T_NATIVE_CHAR,        TypeID) )
            return GDT_Byte;
        else if( H5Tequal(H5T_NATIVE_SCHAR,  TypeID) )
            return GDT_Byte;
        else if( H5Tequal(H5T_NATIVE_UCHAR,  TypeID) )
            return GDT_Byte;
        else if( H5Tequal(H5T_NATIVE_SHORT,  TypeID) )
            return GDT_Int16;
        else if( H5Tequal(H5T_NATIVE_USHORT, TypeID) )
            return GDT_UInt16;
        else if( H5Tequal(H5T_NATIVE_INT,    TypeID) )
            return GDT_Int32;
        else if( H5Tequal(H5T_NATIVE_UINT,   TypeID) )
            return GDT_UInt32;
        else if( H5Tequal(H5T_NATIVE_INT64,  TypeID) )
            return GDT_Int64;
        else if( H5Tequal(H5T_NATIVE_UINT64, TypeID) )
            return GDT_UInt64;
        else if( H5Tequal(H5T_NATIVE_LONG,   TypeID) )
        {
#if SIZEOF_UNSIGNED_LONG == 4
            return GDT_Int32;
#else
            return GDT_Unknown;
#endif
        }
        else if( H5Tequal(H5T_NATIVE_ULONG,  TypeID) )
        {
#if SIZEOF_UNSIGNED_LONG == 4
            return GDT_UInt32;
#else
            return GDT_Unknown;
#endif
        }
        else if( H5Tequal(H5T_NATIVE_FLOAT,  TypeID) )
            return GDT_Float32;
        else if( H5Tequal(H5T_NATIVE_DOUBLE, TypeID) )
            return GDT_Float64;
        else if( H5Tequal(H5T_NATIVE_LLONG,  TypeID) )
            return GDT_Unknown;
        else if( H5Tequal(H5T_NATIVE_ULLONG, TypeID) )
            return GDT_Unknown;
    }
    else  //Parse compound type to determine if data is complex
    {
        //For complex the compound type must contain 2 elements
        if ( H5Tget_nmembers(TypeID) != 2 )
            return GDT_Unknown;

        //For complex the native types of both elements should be the same
        hid_t ElemTypeID = H5Tget_member_type(TypeID, 0);
        hid_t Elem2TypeID = H5Tget_member_type(TypeID, 1);
        const bool bTypeEqual = H5Tequal( ElemTypeID, Elem2TypeID) > 0;
        H5Tclose(Elem2TypeID);
        if ( !bTypeEqual )
        {
            H5Tclose(ElemTypeID);
            return GDT_Unknown;
        }

        char* pszName1 = H5Tget_member_name(TypeID, 0);
        const bool bIsReal = pszName1 && (pszName1[0] == 'r' ||pszName1[0] == 'R');
        H5free_memory(pszName1);

        char* pszName2 = H5Tget_member_name(TypeID, 1);
        const bool bIsImaginary = pszName2 && (pszName2[0] == 'i' ||pszName2[0] == 'I');
        H5free_memory(pszName2);

        if( !bIsReal || !bIsImaginary)
        {
            H5Tclose(ElemTypeID);
            return GDT_Unknown;
        }

        //Check the native types to determine CInt16, CFloat32 or CFloat64
        GDALDataType eDataType = GDT_Unknown;

        if ( H5Tequal(H5T_NATIVE_SHORT, ElemTypeID) )
            eDataType = GDT_CInt16;
        else if ( H5Tequal(H5T_NATIVE_INT, ElemTypeID) )
            eDataType = GDT_CInt32;
        else if ( H5Tequal(H5T_NATIVE_LONG, ElemTypeID) )
        {
#if SIZEOF_UNSIGNED_LONG == 4
            eDataType = GDT_CInt32;
#else
            eDataType = GDT_Unknown;
#endif
        }
        else if ( H5Tequal(H5T_NATIVE_FLOAT, ElemTypeID) )
            eDataType = GDT_CFloat32;
        else if ( H5Tequal(H5T_NATIVE_DOUBLE, ElemTypeID) )
            eDataType = GDT_CFloat64;

        //Close the data type
        H5Tclose(ElemTypeID);

        return eDataType;
    }

    return GDT_Unknown;
}

/************************************************************************/
/*                          GetDataTypeName()                           */
/*                                                                      */
/*      Return the human readable name of data type                     */
/************************************************************************/
const char *HDF5Dataset::GetDataTypeName(hid_t TypeID)
{
    //Check for native types first
    if (H5Tget_class(TypeID) != H5T_COMPOUND)
    {
        if( H5Tequal(H5T_NATIVE_CHAR,        TypeID) )
            return "8-bit character";
        else if( H5Tequal(H5T_NATIVE_SCHAR,  TypeID) )
            return "8-bit signed character";
        else if( H5Tequal(H5T_NATIVE_UCHAR,  TypeID) )
            return "8-bit unsigned character";
        else if( H5Tequal(H5T_NATIVE_SHORT,  TypeID) )
            return "16-bit integer";
        else if( H5Tequal(H5T_NATIVE_USHORT, TypeID) )
            return "16-bit unsigned integer";
        else if( H5Tequal(H5T_NATIVE_INT,    TypeID) )
            return "32-bit integer";
        else if( H5Tequal(H5T_NATIVE_UINT,   TypeID) )
            return "32-bit unsigned integer";
        else if( H5Tequal(H5T_NATIVE_LONG,   TypeID) )
            return "32/64-bit integer";
        else if( H5Tequal(H5T_NATIVE_ULONG,  TypeID) )
            return "32/64-bit unsigned integer";
        else if( H5Tequal(H5T_NATIVE_FLOAT,  TypeID) )
            return "32-bit floating-point";
        else if( H5Tequal(H5T_NATIVE_DOUBLE, TypeID) )
            return "64-bit floating-point";
        else if( H5Tequal(H5T_NATIVE_LLONG,  TypeID) )
            return "64-bit integer";
        else if( H5Tequal(H5T_NATIVE_ULLONG, TypeID) )
            return "64-bit unsigned integer";
        else if( H5Tequal(H5T_NATIVE_DOUBLE, TypeID) )
            return "64-bit floating-point";
    }
    else
    {
        //For complex the compound type must contain 2 elements
        if ( H5Tget_nmembers(TypeID) != 2 )
            return "Unknown";

        //For complex the native types of both elements should be the same
        hid_t ElemTypeID = H5Tget_member_type(TypeID, 0);
        hid_t Elem2TypeID = H5Tget_member_type(TypeID, 1);
        const bool bTypeEqual = H5Tequal( ElemTypeID, Elem2TypeID) > 0;
        H5Tclose(Elem2TypeID);
        if ( !bTypeEqual )
        {
            H5Tclose(ElemTypeID);
            return "Unknown";
        }

        //Check the native types to determine CInt16, CFloat32 or CFloat64
        if ( H5Tequal(H5T_NATIVE_SHORT, ElemTypeID) )
        {
            H5Tclose(ElemTypeID);
            return "complex, 16-bit integer";
        }
        else if ( H5Tequal(H5T_NATIVE_INT, ElemTypeID) )
        {
            H5Tclose(ElemTypeID);
            return "complex, 32-bit integer";
        }
        else if ( H5Tequal(H5T_NATIVE_LONG, ElemTypeID) )
        {
            H5Tclose(ElemTypeID);
            return "complex, 32/64-bit integer";
        }
        else if ( H5Tequal(H5T_NATIVE_FLOAT, ElemTypeID) )
        {
            H5Tclose(ElemTypeID);
            return "complex, 32-bit floating-point";
        }
        else if ( H5Tequal(H5T_NATIVE_DOUBLE, ElemTypeID) )
        {
            H5Tclose(ElemTypeID);
            return "complex, 64-bit floating-point";
        }
    }


    return "Unknown";
}

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/

int HDF5Dataset::Identify( GDALOpenInfo * poOpenInfo )

{
    if( (poOpenInfo->nOpenFlags & GDAL_OF_MULTIDIM_RASTER) &&
        STARTS_WITH(poOpenInfo->pszFilename, "HDF5:") )
    {
        return TRUE;
    }

    // Is it an HDF5 file?
    constexpr char achSignature[] = "\211HDF\r\n\032\n";

    if( !poOpenInfo->pabyHeader )
        return FALSE;

    const CPLString osExt(CPLGetExtension(poOpenInfo->pszFilename));

    const auto IsRecognizedByNetCDFDriver = [&osExt, poOpenInfo]()
    {
        if( (EQUAL(osExt, "NC") ||
             EQUAL(osExt, "CDF") ||
             EQUAL(osExt, "NC4") ||
             EQUAL(osExt, "gmac")) &&
            GDALGetDriverByName("netCDF") != nullptr )
        {
            const char *const apszAllowedDriver[] = { "netCDF", nullptr };
            CPLPushErrorHandler(CPLQuietErrorHandler);
            GDALDatasetH hDS = GDALOpenEx(poOpenInfo->pszFilename,
                                          GDAL_OF_RASTER | GDAL_OF_MULTIDIM_RASTER | GDAL_OF_VECTOR,
                                          apszAllowedDriver, nullptr, nullptr);
            CPLPopErrorHandler();
            if( hDS )
            {
                GDALClose(hDS);
                return true;
            }
        }
        return false;
    };

    if( memcmp(poOpenInfo->pabyHeader, achSignature, 8) == 0 )
    {
        // The tests to avoid opening KEA and BAG drivers are not
        // necessary when drivers are built in the core lib, as they
        // are registered after HDF5, but in the case of plugins, we
        // cannot do assumptions about the registration order.

        // Avoid opening kea files if the kea driver is available.
        if( EQUAL(osExt, "KEA") && GDALGetDriverByName("KEA") != nullptr )
        {
            return FALSE;
        }

        // Avoid opening BAG files if the bag driver is available.
        if( EQUAL(osExt, "BAG") && GDALGetDriverByName("BAG") != nullptr )
        {
            return FALSE;
        }

        // Avoid opening NC files if the netCDF driver is available and
        // they are recognized by it.
        if( IsRecognizedByNetCDFDriver() )
        {
            return FALSE;
        }

        return TRUE;
    }

    if( memcmp(poOpenInfo->pabyHeader, "<HDF_UserBlock>", 15) == 0)
    {
        if( H5Fis_hdf5(poOpenInfo->pszFilename) )
          return TRUE;
    }

    // The HDF5 signature can be at offsets 512, 1024, 2048, etc.
    if( poOpenInfo->fpL != nullptr &&
        (EQUAL(osExt, "h5") || EQUAL(osExt, "hdf5") ||
         EQUAL(osExt, "nc") || EQUAL(osExt, "cdf") || EQUAL(osExt, "nc4")) )
    {
        vsi_l_offset nOffset = 512;
        for(int i = 0; i < 64; i++)
        {
            GByte abyBuf[8];
            if( VSIFSeekL(poOpenInfo->fpL, nOffset, SEEK_SET) != 0 ||
                VSIFReadL(abyBuf, 1, 8, poOpenInfo->fpL) != 8 )
            {
                break;
            }
            if( memcmp(abyBuf, achSignature, 8) == 0 )
            {
                // Avoid opening NC files if the netCDF driver is available and
                // they are recognized by it.
                if( IsRecognizedByNetCDFDriver() )
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
/*                         GDAL_HDF5Open()                              */
/************************************************************************/
hid_t GDAL_HDF5Open(const std::string& osFilename )
{
    hid_t hHDF5;
    // Heuristics to able datasets split over several files, using the 'family'
    // driver. If passed the first file, and it contains a single 0, or
    // ends up with 0.h5 or 0.hdf5, replace the 0 with %d and try the family driver.
    if( std::count(osFilename.begin(), osFilename.end(), '0') == 1 ||
        osFilename.find("0.h5") != std::string::npos ||
        osFilename.find("0.hdf5") != std::string::npos )
    {
        const auto zero_pos = osFilename.rfind('0');
        const auto osNewName = osFilename.substr(0, zero_pos) + "%d" + osFilename.substr(zero_pos+1);
        hid_t fapl = H5Pcreate(H5P_FILE_ACCESS);
        H5Pset_fapl_family(fapl, H5F_FAMILY_DEFAULT, H5P_DEFAULT);
#ifdef HAVE_GCC_WARNING_ZERO_AS_NULL_POINTER_CONSTANT
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
#endif
        H5E_BEGIN_TRY {
            hHDF5 = H5Fopen(osNewName.c_str(), H5F_ACC_RDONLY, fapl);
        } H5E_END_TRY;
#ifdef HAVE_GCC_WARNING_ZERO_AS_NULL_POINTER_CONSTANT
#pragma GCC diagnostic pop
#endif
        H5Pclose(fapl);
        if( hHDF5 >= 0 )
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
GDALDataset *HDF5Dataset::Open( GDALOpenInfo *poOpenInfo )
{
    if( !Identify(poOpenInfo) )
        return nullptr;

    if( poOpenInfo->nOpenFlags & GDAL_OF_MULTIDIM_RASTER )
    {
        return OpenMultiDim(poOpenInfo);
    }

    // Create datasource.
    HDF5Dataset *const poDS = new HDF5Dataset();

    poDS->SetDescription(poOpenInfo->pszFilename);

    // Try opening the dataset.
    poDS->hHDF5 = GDAL_HDF5Open(poOpenInfo->pszFilename);
    if( poDS->hHDF5 < 0 )
    {
        delete poDS;
        return nullptr;
    }

    poDS->hGroupID = H5Gopen(poDS->hHDF5, "/");
    if( poDS->hGroupID < 0 )
    {
        poDS->bIsHDFEOS = false;
        delete poDS;
        return nullptr;
    }

    poDS->bIsHDFEOS = true;
    poDS->ReadGlobalAttributes(true);

    poDS->SetMetadata(poDS->papszMetadata);

    if( STARTS_WITH(CSLFetchNameValueDef(poDS->papszMetadata, "mission_name", ""), "Sentinel 3") &&
        EQUAL(CSLFetchNameValueDef(poDS->papszMetadata, "altimeter_sensor_name", ""), "SRAL") &&
        EQUAL(CSLFetchNameValueDef(poDS->papszMetadata, "radiometer_sensor_name", ""), "MWR") &&
        GDALGetDriverByName("netCDF") != nullptr )
    {
        delete poDS;
        return nullptr;
    }

    if ( CSLCount(poDS->papszSubDatasets) / 2 >= 1 )
        poDS->SetMetadata(poDS->papszSubDatasets, "SUBDATASETS");

    // Make sure we don't try to do any pam stuff with this dataset.
    poDS->nPamFlags |= GPF_NOSAVE;

    // If we have single subdataset only, open it immediately.
    int nSubDatasets = CSLCount(poDS->papszSubDatasets) / 2;
    if( nSubDatasets == 1 )
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
        if( poOpenInfo->eAccess == GA_Update )
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
void HDF5Dataset::DestroyH5Objects( HDF5GroupObjects *poH5Object )
{
    // Visit all objects.
    for( unsigned i = 0; i < poH5Object->nbObjs; i++ )
        DestroyH5Objects(poH5Object->poHchild + i);

    if( poH5Object->poHparent ==nullptr )
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

    if( poH5Object->native > 0 )
        H5Tclose(poH5Object->native);
    poH5Object->native = 0;

    // All Children are visited and can be deleted.
    if( poH5Object->nbObjs != 0 )
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
static void CreatePath( HDF5GroupObjects *poH5Object )
{
    // Recurse to the root path.
    CPLString osPath;
    if( poH5Object->poHparent != nullptr )
    {
        CreatePath(poH5Object->poHparent);
        osPath = poH5Object->poHparent->pszPath;
    }

    // Add name to the path.
    if( !EQUAL(poH5Object->pszName, "/") )
    {
        osPath.append("/");
        osPath.append(poH5Object->pszName);
    }

    // Fill up path for each object.
    CPLString osUnderscoreSpaceInName;
    if( poH5Object->pszPath == nullptr )
    {
        // This is completely useless but needed if we want to keep
        // subdataset names as they have "always" been formatted,
        // with double slash at the beginning
        if( osPath.empty() )
            osPath = "/";

        // Change space for underscore.
        char **papszPath =
            CSLTokenizeString2(osPath.c_str(), " ", CSLT_HONOURSTRINGS);

        for( int i = 0; papszPath[i] != nullptr ; i++ )
        {
            if( i > 0 )
                osUnderscoreSpaceInName.append("_");
            osUnderscoreSpaceInName.append(papszPath[i]);
        }
        CSLDestroy(papszPath);

        // -1 to give room for NUL in C strings.
        constexpr size_t MAX_PATH = 8192 - 1;
        // TODO(schwehr): Is it an issue if the results are longer than 8192?
        // It appears that the output can never be longer than the source.
        if( osUnderscoreSpaceInName.size() > MAX_PATH )
            CPLError(CE_Fatal, CPLE_AppDefined,
                     "osUnderscoreSpaceInName longer than MAX_PATH: "
                     "%u > %u",
                     static_cast<unsigned int>(osUnderscoreSpaceInName.size()),
                     static_cast<unsigned int>(MAX_PATH));
        if( osPath.size() > MAX_PATH )
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

static int HDF5GroupCheckDuplicate( HDF5GroupObjects *poHparent,
                                    unsigned long *objno )

{
    while( poHparent != nullptr )
    {
        if( poHparent->objno[0] == objno[0] &&
            poHparent->objno[1] == objno[1] )
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
herr_t HDF5CreateGroupObjs( hid_t hHDF5, const char *pszObjName,
                            void *poHObjParent)
{
    HDF5GroupObjects *const poHparent =
        static_cast<HDF5GroupObjects *>(poHObjParent);
    HDF5GroupObjects *poHchild = poHparent->poHchild;
    H5G_stat_t oStatbuf;

    if( H5Gget_objinfo(hHDF5, pszObjName, FALSE, &oStatbuf) < 0  )
        return -1;

    // Look for next child.
    unsigned idx = 0;  // idx is used after the for loop.
    for( ; idx < poHparent->nbObjs; idx++ )
    {
        if( poHchild->pszName == nullptr ) break;
        poHchild++;
    }

    if( idx == poHparent->nbObjs )
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
    if( poHchild->pszPath == nullptr )
    {
        CreatePath(poHchild);
    }
    if( poHparent->pszPath == nullptr )
    {
        CreatePath(poHparent);
    }

    switch( oStatbuf.type )
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
        if( (hGroupID = H5Gopen(hHDF5, pszObjName)) == -1)
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

        if( nbObjs > 0 )
        {
            poHchild->poHchild = static_cast<HDF5GroupObjects *>(
                CPLCalloc(static_cast<int>(nbObjs), sizeof(HDF5GroupObjects)));
            memset(poHchild->poHchild, 0,
                   static_cast<size_t>(sizeof(HDF5GroupObjects) * nbObjs));
        }
        else
        {
            poHchild->poHchild = nullptr;
        }

        if( !HDF5GroupCheckDuplicate(poHparent, oStatbuf.objno) )
            H5Giterate(hHDF5, pszObjName, nullptr, HDF5CreateGroupObjs, poHchild);
        else
            CPLDebug("HDF5", "avoiding link looping on node '%s'.", pszObjName);

        H5Gclose(hGroupID);
        break;
    }
    case H5G_DATASET:
    {
        hid_t hDatasetID = H5I_INVALID_HID;  // Identifier of dataset.
        if( (hDatasetID = H5Dopen(hHDF5, pszObjName)) == -1)
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

        if( n_dims > 0 )
        {
            dims = static_cast<hsize_t *>(CPLCalloc(n_dims, sizeof(hsize_t)));
            maxdims =
                static_cast<hsize_t *>(CPLCalloc(n_dims, sizeof(hsize_t)));
        }
        H5Sget_simple_extent_dims(dataspace, dims, maxdims);
        if( maxdims != nullptr )
            CPLFree(maxdims);

        if( n_dims > 0 )
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
/*                          HDF5AttrIterate()                           */
/************************************************************************/

static herr_t HDF5AttrIterate( hid_t hH5ObjID,
                               const char *pszAttrName,
                               // TODO(schwehr): void * -> HDF5Dataset *
                               void *pDS )
{
    char **papszTokens = nullptr;
    CPLString osKey;
    HDF5Dataset *const poDS = static_cast<HDF5Dataset *>(pDS);

    // Convert "/" into "_" for the path component
    const char *pszPath = poDS->poH5CurrentObject->pszUnderscorePath;
    if(pszPath != nullptr && strlen(pszPath) > 0)
    {
        papszTokens = CSLTokenizeString2(pszPath, "/", CSLT_HONOURSTRINGS);

        for( hsize_t i = 0; papszTokens != nullptr && papszTokens[i] != nullptr; ++i )
        {
            if( i != 0)
                osKey += '_';
            osKey += papszTokens[i];
        }
        CSLDestroy(papszTokens);
    }

    // Convert whitespaces into "_" for the attribute name component
    papszTokens = CSLTokenizeString2(
        pszAttrName, " ", CSLT_STRIPLEADSPACES | CSLT_STRIPENDSPACES);
    for( hsize_t i = 0; papszTokens != nullptr && papszTokens[i] != nullptr; ++i )
    {
        if(!osKey.empty())
            osKey += '_';
        osKey += papszTokens[i];
    }
    CSLDestroy(papszTokens);

    const hid_t hAttrID = H5Aopen_name(hH5ObjID, pszAttrName);
    const hid_t hAttrTypeID = H5Aget_type(hAttrID);
    const hid_t hAttrNativeType =
        H5Tget_native_type(hAttrTypeID, H5T_DIR_DEFAULT);
    const hid_t hAttrSpace = H5Aget_space(hAttrID);

    if( H5Tget_class(hAttrNativeType) == H5T_VLEN )
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
    for( hsize_t i = 0; i < nAttrDims; i++ )
    {
        nAttrElmts *= static_cast<int>(nSize[i]);
    }

    char *szData = nullptr;
    char *szValue = nullptr;

    if( H5Tget_class(hAttrNativeType) == H5T_STRING )
    {
        if ( H5Tis_variable_str(hAttrNativeType) )
        {
            char **papszStrings =
                static_cast<char **>(CPLMalloc(nAttrElmts * sizeof(char *)));

            // Read the values.
            H5Aread(hAttrID, hAttrNativeType, papszStrings);

            // Concatenate all values as one string separated by a space.
            CPLString osVal = papszStrings[0] ? papszStrings[0] : "{NULL}";
            for( hsize_t i = 1; i < nAttrElmts; i++ )
            {
                osVal += " ";
                osVal += papszStrings[i] ? papszStrings[i] : "{NULL}";
            }

            szValue = static_cast<char *>(CPLMalloc(osVal.length() + 1));
            strcpy(szValue, osVal.c_str());

            H5Dvlen_reclaim(hAttrNativeType, hAttrSpace, H5P_DEFAULT,
                            papszStrings);
            CPLFree(papszStrings);
        }
        else
        {
            const hsize_t nAttrSize = H5Aget_storage_size(hAttrID);
            szValue = static_cast<char *>(CPLMalloc((size_t)(nAttrSize + 1)));
            H5Aread(hAttrID, hAttrNativeType, szValue);
            szValue[nAttrSize] = '\0';
        }
    }
    else
    {
        const size_t nDataLen = 8192;
        void *buf = nullptr;

        if( nAttrElmts > 0 )
        {
            buf = CPLMalloc(nAttrElmts * H5Tget_size(hAttrNativeType));
            szData = static_cast<char *>(CPLMalloc(nDataLen));
            szValue = static_cast<char *>(CPLMalloc(MAX_METADATA_LEN));
            szData[0] = '\0';
            szValue[0] = '\0';
            H5Aread(hAttrID, hAttrNativeType, buf);
        }
        const bool bIsSCHAR = H5Tequal(H5T_NATIVE_SCHAR, hAttrNativeType) > 0;
        const bool bIsUCHAR = H5Tequal(H5T_NATIVE_UCHAR, hAttrNativeType) > 0;
        if( (bIsSCHAR || bIsUCHAR) &&
            CPLTestBool(CPLGetConfigOption("GDAL_HDF5_CHAR_AS_STRING", "NO")) )
        {
            // Compatibility mode with ancient GDAL versions where we consider
            // array of SCHAR/UCHAR as strings. Likely inappropriate mode...
            for( hsize_t i = 0; i < nAttrElmts; i++ )
            {
                snprintf(szData, nDataLen, "%c",
                         static_cast<char *>(buf)[i]);
                if( CPLStrlcat(szValue, szData, MAX_METADATA_LEN) >=
                    MAX_METADATA_LEN )
                    CPLError(CE_Warning, CPLE_OutOfMemory,
                             "Header data too long. Truncated");
            }
        }
        else if( bIsSCHAR )
        {
            for( hsize_t i = 0; i < nAttrElmts; i++ )
            {
                snprintf(szData, nDataLen, "%d ",
                         static_cast<signed char *>(buf)[i]);
                if( CPLStrlcat(szValue, szData, MAX_METADATA_LEN) >=
                    MAX_METADATA_LEN )
                    CPLError(CE_Warning, CPLE_OutOfMemory,
                             "Header data too long. Truncated");
            }
        }
        else if( bIsUCHAR )
        {
            for( hsize_t i = 0; i < nAttrElmts; i++ )
            {
                snprintf(szData, nDataLen, "%u ",
                         static_cast<unsigned char *>(buf)[i]);
                if( CPLStrlcat(szValue, szData, MAX_METADATA_LEN) >=
                    MAX_METADATA_LEN )
                    CPLError(CE_Warning, CPLE_OutOfMemory,
                             "Header data too long. Truncated");
            }
        }
        else if( H5Tequal(H5T_NATIVE_SHORT, hAttrNativeType) > 0 )
        {
            for( hsize_t i = 0; i < nAttrElmts; i++ )
            {
                snprintf(szData, nDataLen, "%d ", static_cast<short *>(buf)[i]);
                if( CPLStrlcat(szValue, szData, MAX_METADATA_LEN) >=
                    MAX_METADATA_LEN )
                    CPLError(CE_Warning, CPLE_OutOfMemory,
                             "Header data too long. Truncated");
            }
        }
        else if( H5Tequal(H5T_NATIVE_USHORT, hAttrNativeType) > 0 )
        {
            for( hsize_t i = 0; i < nAttrElmts; i++ )
            {
              snprintf(szData, nDataLen, "%u ",
                       static_cast<unsigned short *>(buf)[i]);
                if( CPLStrlcat(szValue, szData, MAX_METADATA_LEN) >=
                    MAX_METADATA_LEN )
                    CPLError(CE_Warning, CPLE_OutOfMemory,
                             "Header data too long. Truncated");
            }
        }
        else if( H5Tequal(H5T_NATIVE_INT, hAttrNativeType) > 0 )
        {
            for( hsize_t i=0; i < nAttrElmts; i++ )
            {
                snprintf(szData, nDataLen, "%d ", static_cast<int *>(buf)[i]);
                if( CPLStrlcat(szValue, szData, MAX_METADATA_LEN) >=
                    MAX_METADATA_LEN )
                    CPLError(CE_Warning, CPLE_OutOfMemory,
                             "Header data too long. Truncated");
            }
        }
        else if( H5Tequal(H5T_NATIVE_UINT, hAttrNativeType) > 0 )
        {
            for( hsize_t i = 0; i < nAttrElmts; i++ )
            {
                snprintf(szData, nDataLen, "%u ",
                         static_cast<unsigned int *>(buf)[i]);
                if( CPLStrlcat(szValue, szData, MAX_METADATA_LEN) >=
                    MAX_METADATA_LEN )
                    CPLError(CE_Warning, CPLE_OutOfMemory,
                             "Header data too long. Truncated");
            }
        }
        else if( H5Tequal(H5T_NATIVE_LONG, hAttrNativeType) > 0 )
        {
            for( hsize_t i = 0; i < nAttrElmts; i++ )
            {
                snprintf(szData, nDataLen, "%ld ", static_cast<long *>(buf)[i]);
                if( CPLStrlcat(szValue, szData, MAX_METADATA_LEN) >=
                    MAX_METADATA_LEN )
                    CPLError(CE_Warning, CPLE_OutOfMemory,
                             "Header data too long. Truncated");
            }
        }
        else if( H5Tequal(H5T_NATIVE_ULONG, hAttrNativeType) > 0 )
        {
            for( hsize_t i = 0; i < nAttrElmts; i++ ) {
                snprintf(szData, nDataLen, "%lu ",
                         static_cast<unsigned long *>(buf)[i]);
                if( CPLStrlcat(szValue, szData, MAX_METADATA_LEN) >=
                    MAX_METADATA_LEN )
                    CPLError(CE_Warning, CPLE_OutOfMemory,
                             "Header data too long. Truncated");
            }
        }
        else if( H5Tequal(H5T_NATIVE_FLOAT, hAttrNativeType) > 0 )
        {
            for( hsize_t i = 0; i < nAttrElmts; i++ )
            {
                CPLsnprintf(szData, nDataLen, "%.8g ",
                            static_cast<float *>(buf)[i]);
                if( CPLStrlcat(szValue, szData, MAX_METADATA_LEN) >=
                    MAX_METADATA_LEN )
                    CPLError(CE_Warning, CPLE_OutOfMemory,
                             "Header data too long. Truncated");
            }
        }
        else if( H5Tequal(H5T_NATIVE_DOUBLE, hAttrNativeType) > 0 )
        {
            for( hsize_t i = 0; i < nAttrElmts; i++ )
            {
                CPLsnprintf(szData, nDataLen, "%.15g ",
                            static_cast<double *>(buf)[i]);
                if( CPLStrlcat(szValue, szData, MAX_METADATA_LEN) >=
                    MAX_METADATA_LEN )
                    CPLError(CE_Warning, CPLE_OutOfMemory,
                             "Header data too long. Truncated");
            }
        }
        CPLFree(buf);
    }
    H5Sclose(hAttrSpace);
    H5Tclose(hAttrNativeType);
    H5Tclose(hAttrTypeID);
    H5Aclose(hAttrID);
    poDS->papszMetadata = CSLSetNameValue(poDS->papszMetadata, osKey, szValue);

    CPLFree(szData);
    CPLFree(szValue);

    return 0;
}

/************************************************************************/
/*                           CreateMetadata()                           */
/************************************************************************/
CPLErr HDF5Dataset::CreateMetadata( HDF5GroupObjects *poH5Object, int nType)
{

    if( !poH5Object->pszPath )
        return CE_None;

    poH5CurrentObject = poH5Object;

    if( EQUAL(poH5Object->pszPath, "") )
        return CE_None;

    HDF5Dataset *const poDS = this;
    const int nbAttrs = poH5Object->nbAttrs;

    switch( nType )
    {
    case H5G_GROUP:
        if( nbAttrs > 0 )
        {
            // Identifier of group.
            const hid_t l_hGroupID = H5Gopen(hHDF5, poH5Object->pszPath);
            H5Aiterate(l_hGroupID, nullptr, HDF5AttrIterate, poDS);
            H5Gclose(l_hGroupID);
        }
        break;
    case H5G_DATASET:
        if( nbAttrs > 0 )
        {
            const hid_t hDatasetID = H5Dopen(hHDF5, poH5Object->pszPath);
            H5Aiterate(hDatasetID, nullptr, HDF5AttrIterate, poDS);
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
HDF5Dataset::HDF5FindDatasetObjectsbyPath( HDF5GroupObjects *poH5Objects,
                                           const char *pszDatasetPath )
{
    if( poH5Objects->nType == H5G_DATASET &&
        EQUAL(poH5Objects->pszUnderscorePath, pszDatasetPath) )
    {

#ifdef DEBUG_VERBOSE
        printf("found it! %p\n", poH5Objects); /*ok*/
#endif
        return poH5Objects;
    }

    HDF5Dataset *const poDS = this;

    if( poH5Objects->nbObjs > 0 )
    {
        for( unsigned int i = 0; i <poH5Objects->nbObjs; i++ )   {
            HDF5GroupObjects *poObjectsFound =
                poDS->HDF5FindDatasetObjectsbyPath(poH5Objects->poHchild + i,
                                                   pszDatasetPath);
            // Is this our dataset?
            if( poObjectsFound != nullptr )
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
HDF5Dataset::HDF5FindDatasetObjects( HDF5GroupObjects *poH5Objects,
                                     const char *pszDatasetName )
{
    if( poH5Objects->nType == H5G_DATASET &&
        EQUAL(poH5Objects->pszName, pszDatasetName) )
    {

#ifdef DEBUG_VERBOSE
        printf("found it! %p\n", poH5Objects); /*ok*/
#endif
        return poH5Objects;
    }

    HDF5Dataset *poDS = this;

    if( poH5Objects->nbObjs > 0 )
    {
        for( unsigned int i = 0; i <poH5Objects->nbObjs; i++ )
        {
            HDF5GroupObjects *poObjectsFound = poDS->HDF5FindDatasetObjects(
                poH5Objects->poHchild + i, pszDatasetName);
            // Is this our dataset?
            if( poObjectsFound != nullptr )
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
CPLErr HDF5Dataset::HDF5ListGroupObjects( HDF5GroupObjects *poRootGroup,
                                          int bSUBDATASET )
{
    HDF5Dataset *poDS = this;

    if( poRootGroup->nbObjs > 0 )
        for( hsize_t i = 0; i < poRootGroup->nbObjs; i++ )
        {
            poDS->HDF5ListGroupObjects(poRootGroup->poHchild + i, bSUBDATASET);
        }

    if( poRootGroup->nType == H5G_GROUP ) {
        CreateMetadata(poRootGroup, H5G_GROUP);
    }

    // Create Sub dataset list.

    if( poRootGroup->nType == H5G_DATASET && bSUBDATASET &&
        poDS->GetDataType(poRootGroup->native) == GDT_Unknown )
    {
        CPLDebug("HDF5", "Skipping unsupported %s of type %s",
                 poRootGroup->pszUnderscorePath,
                 poDS->GetDataTypeName(poRootGroup->native));
    }
    else if( poRootGroup->nType == H5G_DATASET && bSUBDATASET )
    {
        CreateMetadata(poRootGroup, H5G_DATASET);

        char szTemp[8192];  // TODO(schwehr): Get this off of the stack.
        switch( poRootGroup->nRank )
        {
        case 2:
            snprintf(szTemp, sizeof(szTemp), "%dx%d",
                     static_cast<int>(poRootGroup->paDims[0]),
                     static_cast<int>(poRootGroup->paDims[1]));
            break;
        case 3:
            snprintf(szTemp, sizeof(szTemp), "%dx%dx%d",
                     static_cast<int>(poRootGroup->paDims[0]),
                     static_cast<int>(poRootGroup->paDims[1]),
                     static_cast<int>(poRootGroup->paDims[2]));
            break;
        default:
            return CE_None;
        }

        const std::string osDim = szTemp;

        snprintf(szTemp, sizeof(szTemp), "SUBDATASET_%d_NAME",
                 ++(poDS->nSubDataCount));

        poDS->papszSubDatasets =
            CSLSetNameValue(poDS->papszSubDatasets, szTemp,
                            CPLSPrintf("HDF5:\"%s\":%s",
                                       poDS->GetDescription(),
                                       poRootGroup->pszUnderscorePath));

        snprintf(szTemp, sizeof(szTemp), "SUBDATASET_%d_DESC",
                 poDS->nSubDataCount);

        poDS->papszSubDatasets = CSLSetNameValue(
            poDS->papszSubDatasets, szTemp,
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

    if( hHDF5 < 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "hHDF5 < 0!");
        return CE_None;
    }

    H5G_stat_t oStatbuf = {{0, 0}, {0, 0}, 0, H5G_UNKNOWN, 0, 0, {0, 0, 0, 0}};

    if( H5Gget_objinfo(hHDF5, "/", FALSE, &oStatbuf) < 0 )
        return CE_Failure;
    poRootGroup->objno[0] = oStatbuf.objno[0];
    poRootGroup->objno[1] = oStatbuf.objno[1];

    if( hGroupID > 0 )
        H5Gclose(hGroupID);
    hGroupID = H5Gopen(hHDF5, "/");
    if( hGroupID < 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "hGroupId <0!");
        return CE_None;
    }

    poRootGroup->nbAttrs = H5Aget_num_attrs(hGroupID);

    H5Gget_num_objs(hGroupID, &(poRootGroup->nbObjs));

    if( poRootGroup->nbObjs > 0 )
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
    if( nSlashPos != CPLString::npos )
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

    const hid_t hObjAttrID = H5Oopen(hHDF5, osObjName.c_str(), H5P_DEFAULT);

    CPLErr retVal = CE_Failure;

    if(hObjAttrID < 0)
    {
        CPLError(CE_Failure, CPLE_OpenFailed,
                 "Object %s could not be opened", pszAttrFullPath);
        retVal = CE_Failure;
    }
    else
    {
        // Open attribute handler by name, from the object handler opened
        // earlier.
        const hid_t hAttrID = H5Aopen_name(hObjAttrID, osAttrName.c_str());

        // Check for errors opening the attribute.
        if( hAttrID < 0 )
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

            if( !H5Tequal(H5T_NATIVE_DOUBLE, hAttrNativeType) )
            {
                CPLError(CE_Failure, CPLE_OpenFailed,
                         "Attribute %s is not of type double",
                         pszAttrFullPath);
                retVal = CE_Failure;
            }
            else
            {
                // Get the amount of elements.
                unsigned int nAttrElmts = 1;
                for( hsize_t i = 0; i < nAttrDims; i++ )
                {
                    // For multidimensional attributes
                    nAttrElmts *= static_cast<unsigned int>(nSize[i]);
                }

                if(nLen != nullptr)
                    *nLen = nAttrElmts;

                *pdfValues = static_cast<double *>(
                    CPLMalloc(nAttrElmts * sizeof(double)));

                // Read the attribute contents
                if( H5Aread(hAttrID, hAttrNativeType, *pdfValues) < 0 )
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
