/******************************************************************************
 *
 * Project:  Zarr Reader
 * Purpose:  All code for Zarr Reader
 * Author:   David Brochart, david.brochart@gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2021, David Brochart <david.brochart@gmail.com>
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
#include "gdal_frmts.h"
#include "gdal_pam.h"

#include "xtensor-zarr/xzarr_hierarchy.hpp"
#include "xtensor-zarr/xzarr_gdal_store.hpp"
#include "xtensor-zarr/xzarr_file_system_store.hpp"
#include "xtensor-io/xio_gzip.hpp"
#include "xtensor-io/xio_zlib.hpp"
#include "xtensor-io/xio_blosc.hpp"

#include <algorithm>

CPL_CVSID("$Id$")

namespace
{
    void xzarr_register_compressors()
    {
        static bool xzarr_compressor_registered = false;
        if (!xzarr_compressor_registered)
        {
            xt::xzarr_register_compressor<xt::xzarr_file_system_store, xt::xio_gzip_config>();
            xt::xzarr_register_compressor<xt::xzarr_file_system_store, xt::xio_zlib_config>();
            xt::xzarr_register_compressor<xt::xzarr_file_system_store, xt::xio_blosc_config>();
            xt::xzarr_register_compressor<xt::xzarr_gdal_store, xt::xio_gzip_config>();
            xt::xzarr_register_compressor<xt::xzarr_gdal_store, xt::xio_zlib_config>();
            xt::xzarr_register_compressor<xt::xzarr_gdal_store, xt::xio_blosc_config>();
            xzarr_compressor_registered = true;
        }
    }

    template <typename T>
    void assign_chunk(void* pImage, xt::zarray& z, int nBlockYSize, int nBlockXSize,
                      int nBlockYOff, int nBlockXOff,
                      xt::dynamic_shape<std::size_t>& chunk_shape)
    {
        try
        {
            xt::xstrided_slice_vector sv(
                {xt::range(chunk_shape[0] * nBlockYOff,
                           chunk_shape[0] * (nBlockYOff + 1)),
                 xt::range(chunk_shape[1] * nBlockXOff,
                           chunk_shape[1] * (nBlockXOff + 1))});
            xt::zarray chunk = xt::strided_view(z, sv);
            xt::xarray<T> typed_chunk = chunk.get_array<T>();
            for( int i = 0; i < nBlockYSize; i++ )
            {
                for( int j = 0; j < nBlockXSize; j++ )
                {
                    static_cast<T*>(pImage)[i * chunk_shape[1] + j] = typed_chunk(i, j);
                }
            }
        }
        catch (const std::runtime_error& e)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Error while processing Zarr array: %s", e.what());
        }
    }
}

/************************************************************************/
/* ==================================================================== */
/*                              ZarrDataset                             */
/* ==================================================================== */
/************************************************************************/

template <class T>
class ZarrRasterBand;

class ZarrDataset final: public GDALPamDataset
{
    friend class ZarrRasterBand<xt::xzarr_file_system_store>;
    friend class ZarrRasterBand<xt::xzarr_gdal_store>;

    VSILFILE    *fp;
    GByte       abyHeader[1012];

public:
        ZarrDataset();
        ~ZarrDataset() override;

    template <class T>
    static GDALDataset* get_hierarchy(T& store, GDALOpenInfo* poOpenInfo);
    static GDALDataset *Open( GDALOpenInfo * );
    static int Identify( GDALOpenInfo * );

    const char *_GetProjectionRef() override;
    const OGRSpatialReference* GetSpatialRef() const override {
        return GetSpatialRefFromOldGetProjectionRef();
    }
};

/************************************************************************/
/* ==================================================================== */
/*                            ZarrRasterBand                            */
/* ==================================================================== */
/************************************************************************/

template <class T>
class ZarrRasterBand final: public GDALPamRasterBand
{
    friend class ZarrDataset;

    xt::xzarr_hierarchy<T> m_zarr_hierarchy;

public:
    ZarrRasterBand( ZarrDataset *, int, xt::xzarr_hierarchy<T>& );
    ~ZarrRasterBand() override;

    CPLErr IReadBlock( int, int, void * ) override;
};

/************************************************************************/
/*                           ZarrRasterBand()                           */
/************************************************************************/

template <class T>
ZarrRasterBand<T>::ZarrRasterBand( ZarrDataset *poDSIn, int nBandIn,
                                   xt::xzarr_hierarchy<T>& zarr_hierarchy ) :
    m_zarr_hierarchy(zarr_hierarchy)
{
    poDS = poDSIn;
    nBand = nBandIn;

    auto z = zarr_hierarchy.get_array("");
    std::string data_type = z.get_metadata()["data_type"];
    // remove endianness
    if ((data_type[0] == '<') || (data_type[0] == '>'))
        data_type = data_type.substr(1);
    if (data_type == "f8")
        eDataType = GDT_Float64;
    else if (data_type == "f4")
        eDataType = GDT_Float32;
    else if (data_type == "i4")
        eDataType = GDT_Int32;
    else if (data_type == "i2")
        eDataType = GDT_Int16;
    else if (data_type == "u4")
        eDataType = GDT_UInt32;
    else if (data_type == "u2")
        eDataType = GDT_UInt16;
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Unsupported Zarr data type.\n");
    }

    auto chunk_shape = z.as_chunked_array().chunk_shape();
    nBlockXSize = chunk_shape[1];
    nBlockYSize = chunk_shape[0];
}

/************************************************************************/
/*                          ~ZarrRasterBand()                            */
/************************************************************************/

template <class T>
ZarrRasterBand<T>::~ZarrRasterBand() { }

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

template <class T>
CPLErr ZarrRasterBand<T>::IReadBlock( CPL_UNUSED int nBlockXOff,
                                   int nBlockYOff,
                                   void * pImage )

{
    xt::zarray z = m_zarr_hierarchy.get_array("");
    auto chunk_shape = z.as_chunked_array().chunk_shape();

    switch (eDataType)
    {
        case GDT_Float64:
            assign_chunk<double>(pImage, z, nBlockYSize, nBlockXSize,
                                 nBlockYOff, nBlockXOff, chunk_shape);
            break;
        case GDT_Float32:
            assign_chunk<float>(pImage, z, nBlockYSize, nBlockXSize,
                                nBlockYOff, nBlockXOff, chunk_shape);
            break;
        case GDT_Int32:
            assign_chunk<int32_t>(pImage, z, nBlockYSize, nBlockXSize,
                                  nBlockYOff, nBlockXOff, chunk_shape);
            break;
        case GDT_UInt32:
            assign_chunk<uint32_t>(pImage, z, nBlockYSize, nBlockXSize,
                                   nBlockYOff, nBlockXOff, chunk_shape);
            break;
        case GDT_Int16:
            assign_chunk<int16_t>(pImage, z, nBlockYSize, nBlockXSize,
                                  nBlockYOff, nBlockXOff, chunk_shape);
            break;
        case GDT_UInt16:
            assign_chunk<uint16_t>(pImage, z, nBlockYSize, nBlockXSize,
                                   nBlockYOff, nBlockXOff, chunk_shape);
            break;
        default:
            break;
    }

    return CE_None;
}

/************************************************************************/
/* ==================================================================== */
/*                              ZarrDataset                             */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                            ZarrDataset()                             */
/************************************************************************/

ZarrDataset::ZarrDataset() :
    fp(nullptr)
{
    xzarr_register_compressors();
    std::memset(abyHeader, 0, sizeof(abyHeader));
}

/************************************************************************/
/*                           ~ZarrDataset()                             */
/************************************************************************/

ZarrDataset::~ZarrDataset()

{
    FlushCache();
    if( fp != nullptr )
        CPL_IGNORE_RET_VAL(VSIFCloseL(fp));
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *ZarrDataset::_GetProjectionRef()

{
    return "";
}

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/

int ZarrDataset::Identify( GDALOpenInfo *poOpenInfo )

{
    // Check that we are accessing a directory,
    // and that it contains a ".zarray" file.
    if( !poOpenInfo->bIsDirectory )
    {
        return FALSE;
    }

    CPLString osMDFilename = CPLFormCIFilename( poOpenInfo->pszFilename,
                                                ".zarray", nullptr );

    VSIStatBufL sStat;
    if( VSIStatL( osMDFilename, &sStat ) == 0 )
        return TRUE;

    return FALSE;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

template <class T>
GDALDataset* ZarrDataset::get_hierarchy(T& store, GDALOpenInfo* poOpenInfo)
{
    ZarrDataset* poDS = nullptr;

    try
    {
        // Create a corresponding GDALDataset.
        poDS = new ZarrDataset();

        auto zarr_hierarchy = xt::get_zarr_hierarchy(store, "2");
        auto zarr_array = zarr_hierarchy.get_array("");

        auto ndim = zarr_array.shape().size();
        if( ndim != 2 )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Zarr array has %ld dimensions, should be 2",
                     ndim);
            delete poDS;
            return nullptr;
        }
        poDS->nRasterXSize = zarr_array.shape()[1];
        poDS->nRasterYSize = zarr_array.shape()[0];

        // Create band information objects.
        poDS->SetBand(1, new ZarrRasterBand<T>(poDS, 1, zarr_hierarchy));

        // Initialize any PAM information.
        poDS->SetDescription(poOpenInfo->pszFilename);
        poDS->TryLoadXML();

        // Check for overviews.
        poDS->oOvManager.Initialize(poDS, poOpenInfo->pszFilename);

        return poDS;
    }
    catch (const std::runtime_error& e)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Error loading Zarr store: %s", e.what());
        if (poDS != nullptr)
        {
            delete poDS;
        }
        return nullptr;
    }
}

GDALDataset *ZarrDataset::Open( GDALOpenInfo *poOpenInfo )
{
    // Confirm that this is a Zarr array.
    if (!Identify(poOpenInfo))
        return nullptr;

    // Confirm the requested access is supported.
    if( poOpenInfo->eAccess == GA_Update )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "The Zarr driver does not support update access to existing "
                 "datasets.");
        return nullptr;
    }

    // Open the Zarr hierarchy.
    try
    {
        if (STARTS_WITH_CI(poOpenInfo->pszFilename, "/VSI"))
        {
            auto store = xt::xzarr_gdal_store(poOpenInfo->pszFilename);
            return get_hierarchy(store, poOpenInfo);
        }
        else
        {
            auto store = xt::xzarr_file_system_store(poOpenInfo->pszFilename);
            return get_hierarchy(store, poOpenInfo);
        }
    }
    catch (const std::runtime_error& e)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Error loading Zarr store: %s", e.what());
        return nullptr;
    }
}

/************************************************************************/
/*                          GDALRegister_Zarr()                         */
/************************************************************************/

void GDALRegister_Zarr()

{
    if( GDALGetDriverByName("Zarr") != nullptr )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription("Zarr");
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "Zarr store");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drivers/raster/zarr.html");
    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");

    poDriver->pfnOpen = ZarrDataset::Open;
    poDriver->pfnIdentify = ZarrDataset::Identify;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
