/******************************************************************************
 *
 * Project:  GDAL Core
 * Purpose:  Implementation of GDALDriver class (and C wrappers)
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1998, 2000, Frank Warmerdam
 * Copyright (c) 2007-2014, Even Rouault <even dot rouault at spatialys.com>
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
#include "gdal.h"
#include "gdal_priv.h"
#include "gdal_rat.h"

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <sys/stat.h>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_minixml.h"
#include "cpl_progress.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "ogr_core.h"
#include "ogrsf_frmts.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                             GDALDriver()                             */
/************************************************************************/

GDALDriver::GDALDriver() :
    pfnOpen(nullptr),
    pfnCreate(nullptr),
    pfnCreateEx(nullptr),
    pfnCreateMultiDimensional(nullptr),
    pfnDelete(nullptr),
    pfnCreateCopy(nullptr),
    pDriverData(nullptr),
    pfnUnloadDriver(nullptr),
    pfnIdentify(nullptr),
    pfnIdentifyEx(nullptr),
    pfnRename(nullptr),
    pfnCopyFiles(nullptr),
    pfnOpenWithDriverArg(nullptr),
    pfnCreateVectorOnly(nullptr),
    pfnDeleteDataSource(nullptr)
{}

/************************************************************************/
/*                            ~GDALDriver()                             */
/************************************************************************/

GDALDriver::~GDALDriver()

{
    if( pfnUnloadDriver != nullptr )
        pfnUnloadDriver( this );
}

/************************************************************************/
/*                         GDALCreateDriver()                           */
/************************************************************************/

/**
 * \brief Create a GDALDriver.
 *
 * Creates a driver in the GDAL heap.
 */

GDALDriverH CPL_STDCALL GDALCreateDriver()
{
    return new GDALDriver();
}

/************************************************************************/
/*                         GDALDestroyDriver()                          */
/************************************************************************/

/**
 * \brief Destroy a GDALDriver.
 *
 * This is roughly equivalent to deleting the driver, but is guaranteed
 * to take place in the GDAL heap.  It is important this that function
 * not be called on a driver that is registered with the GDALDriverManager.
 *
 * @param hDriver the driver to destroy.
 */

void CPL_STDCALL GDALDestroyDriver( GDALDriverH hDriver )

{
    if( hDriver != nullptr )
        delete GDALDriver::FromHandle(hDriver);
}

/************************************************************************/
/*                               Create()                               */
/************************************************************************/

/**
 * \brief Create a new dataset with this driver.
 *
 * What argument values are legal for particular drivers is driver specific,
 * and there is no way to query in advance to establish legal values.
 *
 * That function will try to validate the creation option list passed to the
 * driver with the GDALValidateCreationOptions() method. This check can be
 * disabled by defining the configuration option
 * GDAL_VALIDATE_CREATION_OPTIONS=NO.
 *
 * After you have finished working with the returned dataset, it is
 * <b>required</b> to close it with GDALClose(). This does not only close the
 * file handle, but also ensures that all the data and metadata has been written
 * to the dataset (GDALFlushCache() is not sufficient for that purpose).
 *
 * In GDAL 2, the arguments nXSize, nYSize and nBands can be passed to 0 when
 * creating a vector-only dataset for a compatible driver.
 *
 * Equivalent of the C function GDALCreate().
 *
 * @param pszFilename the name of the dataset to create.  UTF-8 encoded.
 * @param nXSize width of created raster in pixels.
 * @param nYSize height of created raster in pixels.
 * @param nBands number of bands.
 * @param eType type of raster.
 * @param papszOptions list of driver specific control parameters.
 * The APPEND_SUBDATASET=YES option can be
 * specified to avoid prior destruction of existing dataset.
 *
 * @return NULL on failure, or a new GDALDataset.
 */

GDALDataset * GDALDriver::Create( const char * pszFilename,
                                  int nXSize, int nYSize, int nBands,
                                  GDALDataType eType, char ** papszOptions )

{
/* -------------------------------------------------------------------- */
/*      Does this format support creation.                              */
/* -------------------------------------------------------------------- */
    if( pfnCreate == nullptr && pfnCreateEx == nullptr && pfnCreateVectorOnly == nullptr )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "GDALDriver::Create() ... no create method implemented"
                  " for this format." );

        return nullptr;
    }
/* -------------------------------------------------------------------- */
/*      Do some rudimentary argument checking.                          */
/* -------------------------------------------------------------------- */
    if( nBands < 0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Attempt to create dataset with %d bands is illegal,"
                  "Must be >= 0.",
                  nBands );
        return nullptr;
    }

    if( GetMetadataItem(GDAL_DCAP_RASTER) != nullptr &&
        GetMetadataItem(GDAL_DCAP_VECTOR) == nullptr &&
        (nXSize < 1 || nYSize < 1) )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Attempt to create %dx%d dataset is illegal,"
                  "sizes must be larger than zero.",
                  nXSize, nYSize );
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Make sure we cleanup if there is an existing dataset of this    */
/*      name.  But even if that seems to fail we will continue since    */
/*      it might just be a corrupt file or something.                   */
/* -------------------------------------------------------------------- */
    if( !CPLFetchBool(papszOptions, "APPEND_SUBDATASET", false) )
    {
        // Someone issuing Create("foo.tif") on a
        // memory driver doesn't expect files with those names to be deleted
        // on a file system...
        // This is somewhat messy. Ideally there should be a way for the
        // driver to overload the default behavior
        if( !EQUAL(GetDescription(), "MEM") &&
            !EQUAL(GetDescription(), "Memory") &&
            // ogr2ogr -f PostgreSQL might reach the Delete method of the
            // PostgisRaster driver which is undesirable
            !EQUAL(GetDescription(), "PostgreSQL") )
        {
            QuietDelete( pszFilename );
        }
    }

/* -------------------------------------------------------------------- */
/*      Validate creation options.                                      */
/* -------------------------------------------------------------------- */
    if( CPLTestBool(CPLGetConfigOption("GDAL_VALIDATE_CREATION_OPTIONS",
                                       "YES")) )
        GDALValidateCreationOptions( this, papszOptions );

/* -------------------------------------------------------------------- */
/*      Proceed with creation.                                          */
/* -------------------------------------------------------------------- */
    CPLDebug( "GDAL", "GDALDriver::Create(%s,%s,%d,%d,%d,%s,%p)",
              GetDescription(), pszFilename, nXSize, nYSize, nBands,
              GDALGetDataTypeName( eType ),
              papszOptions );

    GDALDataset *poDS = nullptr;
    if( pfnCreateEx != nullptr )
    {
        poDS = pfnCreateEx( this, pszFilename, nXSize, nYSize, nBands, eType,
                          papszOptions );
    }
    else if( pfnCreate != nullptr )
    {
        poDS = pfnCreate( pszFilename, nXSize, nYSize, nBands, eType,
                          papszOptions );
    }
    else if( nBands < 1 )
    {
        poDS = pfnCreateVectorOnly( this, pszFilename, papszOptions );
    }

    if( poDS != nullptr )
    {
        if( poDS->GetDescription() == nullptr
            || strlen(poDS->GetDescription()) == 0 )
            poDS->SetDescription( pszFilename );

        if( poDS->poDriver == nullptr )
            poDS->poDriver = this;

        poDS->AddToDatasetOpenList();
    }

    return poDS;
}

/************************************************************************/
/*                             GDALCreate()                             */
/************************************************************************/

/**
 * \brief Create a new dataset with this driver.
 *
 * @see GDALDriver::Create()
 */

GDALDatasetH CPL_DLL CPL_STDCALL
GDALCreate( GDALDriverH hDriver, const char * pszFilename,
            int nXSize, int nYSize, int nBands, GDALDataType eBandType,
            CSLConstList papszOptions )

{
    VALIDATE_POINTER1( hDriver, "GDALCreate", nullptr );

    return
        GDALDriver::FromHandle(hDriver)->Create( pszFilename,
                                                    nXSize, nYSize, nBands,
                                                    eBandType,
                                                    const_cast<char**>(papszOptions) );
}

/************************************************************************/
/*                       CreateMultiDimensional()                       */
/************************************************************************/

/**
 * \brief Create a new multidimensional dataset with this driver.
 * 
 * Only drivers that advertise the GDAL_DCAP_MULTIDIM_RASTER capability and
 * implement the pfnCreateMultiDimensional method might return a non nullptr
 * GDALDataset.
 *
 * This is the same as the C function GDALCreateMultiDimensional().
 * 
 * @param pszFilename  the name of the dataset to create.  UTF-8 encoded.
 * @param papszRootGroupOptions driver specific options regarding the creation
 *                              of the root group. Might be nullptr.
 * @param papszOptions driver specific options regarding the creation
 *                     of the dataset. Might be nullptr.
 * @return a new dataset, or nullptr in case of failure.
 *
 * @since GDAL 3.1
 */

GDALDataset * GDALDriver::CreateMultiDimensional( const char * pszFilename,
                                                  CSLConstList papszRootGroupOptions,
                                                  CSLConstList papszOptions )

{
/* -------------------------------------------------------------------- */
/*      Does this format support creation.                              */
/* -------------------------------------------------------------------- */
    if( pfnCreateMultiDimensional== nullptr )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "GDALDriver::CreateMultiDimensional() ... "
                  "no CreateMultiDimensional method implemented "
                  "for this format." );

        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Validate creation options.                                      */
/* -------------------------------------------------------------------- */
    if( CPLTestBool(
            CPLGetConfigOption("GDAL_VALIDATE_CREATION_OPTIONS", "YES") ) )
    {
        const char *pszOptionList = GetMetadataItem( GDAL_DMD_MULTIDIM_DATASET_CREATIONOPTIONLIST );
        CPLString osDriver;
        osDriver.Printf("driver %s", GetDescription());
        GDALValidateOptions( pszOptionList, papszOptions,
                            "creation option", osDriver );
    }

    auto poDstDS = pfnCreateMultiDimensional(pszFilename,
                                     papszRootGroupOptions,
                                     papszOptions);

    if( poDstDS != nullptr )
    {
        if( poDstDS->GetDescription() == nullptr
            || strlen(poDstDS->GetDescription()) == 0 )
            poDstDS->SetDescription( pszFilename );

        if( poDstDS->poDriver == nullptr )
            poDstDS->poDriver = this;
    }

    return poDstDS;
}

/************************************************************************/
/*                       GDALCreateMultiDimensional()                   */
/************************************************************************/

/** \brief Create a new multidimensional dataset with this driver.
 * 
 * This is the same as the C++ method GDALDriver::CreateMultiDimensional().
 */
GDALDatasetH GDALCreateMultiDimensional( GDALDriverH hDriver,
                                                 const char * pszName,
                                                 CSLConstList papszRootGroupOptions,
                                                 CSLConstList papszOptions )
{
    VALIDATE_POINTER1( hDriver, __func__, nullptr );
    VALIDATE_POINTER1( pszName, __func__, nullptr );
    return GDALDataset::ToHandle(
        GDALDriver::FromHandle(hDriver)->CreateMultiDimensional(
            pszName, papszRootGroupOptions, papszOptions));
}

/************************************************************************/
/*                  DefaultCreateCopyMultiDimensional()                 */
/************************************************************************/

//! @cond Doxygen_Suppress

CPLErr GDALDriver::DefaultCreateCopyMultiDimensional(
                                     GDALDataset *poSrcDS,
                                     GDALDataset *poDstDS,
                                     bool bStrict,
                                     CSLConstList papszOptions,
                                     GDALProgressFunc pfnProgress,
                                     void * pProgressData )
{
    if( pfnProgress == nullptr )
        pfnProgress = GDALDummyProgress;

    auto poSrcRG = poSrcDS->GetRootGroup();
    if( !poSrcRG )
        return CE_Failure;
    auto poDstRG = poDstDS->GetRootGroup();
    if( !poDstRG )
        return CE_Failure;
    GUInt64 nCurCost = 0;
    return poDstRG->CopyFrom(poDstRG, poSrcDS, poSrcRG, bStrict,
                             nCurCost, poSrcRG->GetTotalCopyCost(),
                             pfnProgress, pProgressData, papszOptions) ?
                CE_None : CE_Failure;
}
//! @endcond


/************************************************************************/
/*                          DefaultCopyMasks()                          */
/************************************************************************/

//! @cond Doxygen_Suppress
CPLErr GDALDriver::DefaultCopyMasks( GDALDataset *poSrcDS,
                                     GDALDataset *poDstDS,
                                     int bStrict )

{
    return DefaultCopyMasks(poSrcDS, poDstDS, bStrict,
                            nullptr, nullptr, nullptr);
}

CPLErr GDALDriver::DefaultCopyMasks( GDALDataset *poSrcDS,
                                     GDALDataset *poDstDS,
                                     int bStrict,
                                     CSLConstList /*papszOptions*/,
                                     GDALProgressFunc pfnProgress,
                                     void * pProgressData )

{
    if( pfnProgress == nullptr )
        pfnProgress = GDALDummyProgress;

    int nBands = poSrcDS->GetRasterCount();
    if (nBands == 0)
        return CE_None;

/* -------------------------------------------------------------------- */
/*      Try to copy mask if it seems appropriate.                       */
/* -------------------------------------------------------------------- */
    const char* papszOptions[2] = { "COMPRESSED=YES", nullptr };
    CPLErr eErr = CE_None;

    int nTotalBandsWithMask = 0;
    for( int iBand = 0; iBand < nBands; ++iBand )
    {
        GDALRasterBand *poSrcBand = poSrcDS->GetRasterBand( iBand+1 );

        int nMaskFlags = poSrcBand->GetMaskFlags();
        if( !(nMaskFlags &
                 (GMF_ALL_VALID|GMF_PER_DATASET|GMF_ALPHA|GMF_NODATA) ) )
        {
            nTotalBandsWithMask ++;
        }
    }

    int iBandWithMask = 0;
    for( int iBand = 0;
         eErr == CE_None && iBand < nBands;
         ++iBand )
    {
        GDALRasterBand *poSrcBand = poSrcDS->GetRasterBand( iBand+1 );

        int nMaskFlags = poSrcBand->GetMaskFlags();
        if( eErr == CE_None
            && !(nMaskFlags &
                 (GMF_ALL_VALID|GMF_PER_DATASET|GMF_ALPHA|GMF_NODATA) ) )
        {
            GDALRasterBand *poDstBand = poDstDS->GetRasterBand( iBand+1 );
            if (poDstBand != nullptr)
            {
                eErr = poDstBand->CreateMaskBand( nMaskFlags );
                if( eErr == CE_None )
                {
                    // coverity[divide_by_zero]
                    void* pScaledData = GDALCreateScaledProgress(
                        double(iBandWithMask) / nTotalBandsWithMask,
                        double(iBandWithMask + 1) / nTotalBandsWithMask,
                        pfnProgress, pProgressData );
                    eErr = GDALRasterBandCopyWholeRaster(
                        poSrcBand->GetMaskBand(),
                        poDstBand->GetMaskBand(),
                        papszOptions,
                        GDALScaledProgress, pScaledData);
                    GDALDestroyScaledProgress(pScaledData);
                }
                else if( !bStrict )
                {
                    eErr = CE_None;
                }
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Try to copy a per-dataset mask if we have one.                  */
/* -------------------------------------------------------------------- */
    const int nMaskFlags = poSrcDS->GetRasterBand(1)->GetMaskFlags();
    if( eErr == CE_None
        && !(nMaskFlags & (GMF_ALL_VALID|GMF_ALPHA|GMF_NODATA) )
        && (nMaskFlags & GMF_PER_DATASET) )
    {
        eErr = poDstDS->CreateMaskBand( nMaskFlags );
        if( eErr == CE_None )
        {
            eErr = GDALRasterBandCopyWholeRaster(
                poSrcDS->GetRasterBand(1)->GetMaskBand(),
                poDstDS->GetRasterBand(1)->GetMaskBand(),
                papszOptions,
                pfnProgress, pProgressData);
        }
        else if( !bStrict )
        {
            eErr = CE_None;
        }
    }

    return eErr;
}

/************************************************************************/
/*                         DefaultCreateCopy()                          */
/************************************************************************/

GDALDataset *GDALDriver::DefaultCreateCopy( const char * pszFilename,
                                            GDALDataset * poSrcDS,
                                            int bStrict, char ** papszOptions,
                                            GDALProgressFunc pfnProgress,
                                            void * pProgressData )

{
    if( pfnProgress == nullptr )
        pfnProgress = GDALDummyProgress;

    CPLErrorReset();

/* -------------------------------------------------------------------- */
/*      Use multidimensional raster API if available.                   */
/* -------------------------------------------------------------------- */
    auto poSrcGroup = poSrcDS->GetRootGroup();
    if( poSrcGroup != nullptr && GetMetadataItem(GDAL_DCAP_MULTIDIM_RASTER) )
    {
        CPLStringList aosDatasetCO;
        for( CSLConstList papszIter = papszOptions; papszIter && *papszIter; ++papszIter )
        {
            if( !STARTS_WITH_CI(*papszIter, "ARRAY:") )
                aosDatasetCO.AddString(*papszIter);
        }
        auto poDstDS = std::unique_ptr<GDALDataset>(
            CreateMultiDimensional(pszFilename,
                                   nullptr,
                                   aosDatasetCO.List()));
        if( !poDstDS )
            return nullptr;
        auto poDstGroup = poDstDS->GetRootGroup();
        if( !poDstGroup )
            return nullptr;
        if( DefaultCreateCopyMultiDimensional(poSrcDS,
                                              poDstDS.get(),
                                              CPL_TO_BOOL(bStrict),
                                              papszOptions,
                                              pfnProgress,
                                              pProgressData) != CE_None )
            return nullptr;
        return poDstDS.release();
    }

/* -------------------------------------------------------------------- */
/*      Validate that we can create the output as requested.            */
/* -------------------------------------------------------------------- */
    const int nXSize = poSrcDS->GetRasterXSize();
    const int nYSize = poSrcDS->GetRasterYSize();
    const int nBands = poSrcDS->GetRasterCount();

    CPLDebug( "GDAL", "Using default GDALDriver::CreateCopy implementation." );

    const int nLayerCount = poSrcDS->GetLayerCount();
    if( nBands == 0 && nLayerCount == 0 &&
        GetMetadataItem(GDAL_DCAP_VECTOR) == nullptr )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "GDALDriver::DefaultCreateCopy does not support zero band" );
        return nullptr;
    }
    if( poSrcDS->GetDriver() != nullptr &&
        poSrcDS->GetDriver()->GetMetadataItem(GDAL_DCAP_RASTER) != nullptr &&
        poSrcDS->GetDriver()->GetMetadataItem(GDAL_DCAP_VECTOR) == nullptr &&
        GetMetadataItem(GDAL_DCAP_RASTER) == nullptr &&
        GetMetadataItem(GDAL_DCAP_VECTOR) != nullptr )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Source driver is raster-only whereas output driver is "
                  "vector-only" );
        return nullptr;
    }
    else if( poSrcDS->GetDriver() != nullptr &&
        poSrcDS->GetDriver()->GetMetadataItem(GDAL_DCAP_RASTER) == nullptr &&
        poSrcDS->GetDriver()->GetMetadataItem(GDAL_DCAP_VECTOR) != nullptr &&
        GetMetadataItem(GDAL_DCAP_RASTER) != nullptr &&
        GetMetadataItem(GDAL_DCAP_VECTOR) == nullptr )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Source driver is vector-only whereas output driver is "
                  "raster-only" );
        return nullptr;
    }

    if( !pfnProgress( 0.0, nullptr, pProgressData ) )
    {
        CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Propagate some specific structural metadata as options if it    */
/*      appears to be supported by the target driver and the caller     */
/*      didn't provide values.                                          */
/* -------------------------------------------------------------------- */
    char **papszCreateOptions = CSLDuplicate( papszOptions );
    const char * const apszOptItems[] = {
        "NBITS", "IMAGE_STRUCTURE",
        "PIXELTYPE", "IMAGE_STRUCTURE",
        nullptr };

    for( int iOptItem = 0;
         nBands > 0 && apszOptItems[iOptItem] != nullptr;
         iOptItem += 2 )
    {
        // does the source have this metadata item on the first band?
        const char *pszValue =
            poSrcDS->GetRasterBand(1)->GetMetadataItem(
                apszOptItems[iOptItem], apszOptItems[iOptItem+1] );

        if( pszValue == nullptr )
            continue;

        // Do not override provided value.
        if( CSLFetchNameValue( papszCreateOptions, pszValue ) != nullptr )
            continue;

        // Does this appear to be a supported creation option on this driver?
        const char *pszOptionList =
            GetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST );

        if( pszOptionList == nullptr
            || strstr(pszOptionList,apszOptItems[iOptItem]) == nullptr )
            continue;

        papszCreateOptions = CSLSetNameValue( papszCreateOptions,
                                              apszOptItems[iOptItem],
                                              pszValue );
    }

/* -------------------------------------------------------------------- */
/*      Create destination dataset.                                     */
/* -------------------------------------------------------------------- */
    GDALDataType eType = GDT_Unknown;

    if( nBands > 0 )
        eType = poSrcDS->GetRasterBand(1)->GetRasterDataType();
    GDALDataset  *poDstDS = Create( pszFilename, nXSize, nYSize,
                                    nBands, eType, papszCreateOptions );

    CSLDestroy(papszCreateOptions);

    if( poDstDS == nullptr )
        return nullptr;

    int nDstBands = poDstDS->GetRasterCount();
    CPLErr eErr = CE_None;
    if( nDstBands != nBands )
    {
        if( GetMetadataItem(GDAL_DCAP_RASTER) != nullptr )
        {
            // Should not happen for a well-behaved driver.
            CPLError(
                CE_Failure, CPLE_AppDefined,
                "Output driver created only %d bands whereas %d were expected",
                nDstBands, nBands );
            eErr = CE_Failure;
        }
        nDstBands = 0;
    }

/* -------------------------------------------------------------------- */
/*      Try setting the projection and geotransform if it seems         */
/*      suitable.                                                       */
/* -------------------------------------------------------------------- */
    double adfGeoTransform[6] = {};

    if( nDstBands == 0 && !bStrict )
        CPLPushErrorHandler(CPLQuietErrorHandler);

    if( eErr == CE_None
        && poSrcDS->GetGeoTransform( adfGeoTransform ) == CE_None
        // TODO(schwehr): The default value check should be a function.
        && (adfGeoTransform[0] != 0.0
            || adfGeoTransform[1] != 1.0
            || adfGeoTransform[2] != 0.0
            || adfGeoTransform[3] != 0.0
            || adfGeoTransform[4] != 0.0
            || adfGeoTransform[5] != 1.0) )
    {
        eErr = poDstDS->SetGeoTransform( adfGeoTransform );
        if( !bStrict )
            eErr = CE_None;
    }

    if( eErr == CE_None
        && poSrcDS->GetProjectionRef() != nullptr
        && strlen(poSrcDS->GetProjectionRef()) > 0 )
    {
        eErr = poDstDS->SetProjection( poSrcDS->GetProjectionRef() );
        if( !bStrict )
            eErr = CE_None;
    }

/* -------------------------------------------------------------------- */
/*      Copy GCPs.                                                      */
/* -------------------------------------------------------------------- */
    if( poSrcDS->GetGCPCount() > 0 && eErr == CE_None )
    {
        eErr = poDstDS->SetGCPs( poSrcDS->GetGCPCount(),
                                 poSrcDS->GetGCPs(),
                                 poSrcDS->GetGCPProjection() );
        if( !bStrict )
            eErr = CE_None;
    }

    if( nDstBands == 0 && !bStrict )
        CPLPopErrorHandler();

/* -------------------------------------------------------------------- */
/*      Copy metadata.                                                  */
/* -------------------------------------------------------------------- */
    if( poSrcDS->GetMetadata() != nullptr )
        poDstDS->SetMetadata( poSrcDS->GetMetadata() );

/* -------------------------------------------------------------------- */
/*      Copy transportable special domain metadata (RPCs).  It would    */
/*      be nice to copy geolocation, but it is pretty fragile.          */
/* -------------------------------------------------------------------- */
    char **papszMD = poSrcDS->GetMetadata( "RPC" );
    if( papszMD )
        poDstDS->SetMetadata( papszMD, "RPC" );

/* -------------------------------------------------------------------- */
/*      Copy XMPmetadata.                                               */
/* -------------------------------------------------------------------- */
    char** papszXMP = poSrcDS->GetMetadata("xml:XMP");
    if (papszXMP != nullptr && *papszXMP != nullptr)
    {
        poDstDS->SetMetadata(papszXMP, "xml:XMP");
    }
    
/* -------------------------------------------------------------------- */
/*      Loop copying bands.                                             */
/* -------------------------------------------------------------------- */
    for( int iBand = 0;
         eErr == CE_None && iBand < nDstBands;
         ++iBand )
    {
        GDALRasterBand * const poSrcBand = poSrcDS->GetRasterBand( iBand+1 );
        GDALRasterBand * const poDstBand = poDstDS->GetRasterBand( iBand+1 );

/* -------------------------------------------------------------------- */
/*      Do we need to copy a colortable.                                */
/* -------------------------------------------------------------------- */
        GDALColorTable * const poCT = poSrcBand->GetColorTable();
        if( poCT != nullptr )
            poDstBand->SetColorTable( poCT );

/* -------------------------------------------------------------------- */
/*      Do we need to copy other metadata?  Most of this is             */
/*      non-critical, so lets not bother folks if it fails are we       */
/*      are not in strict mode.                                         */
/* -------------------------------------------------------------------- */
        if( !bStrict )
            CPLPushErrorHandler( CPLQuietErrorHandler );

        if( strlen(poSrcBand->GetDescription()) > 0 )
            poDstBand->SetDescription( poSrcBand->GetDescription() );

        if( CSLCount(poSrcBand->GetMetadata()) > 0 )
            poDstBand->SetMetadata( poSrcBand->GetMetadata() );

        int bSuccess = FALSE;
        double dfValue = poSrcBand->GetOffset( &bSuccess );
        if( bSuccess && dfValue != 0.0 )
            poDstBand->SetOffset( dfValue );

        dfValue = poSrcBand->GetScale( &bSuccess );
        if( bSuccess && dfValue != 1.0 )
            poDstBand->SetScale( dfValue );

        dfValue = poSrcBand->GetNoDataValue( &bSuccess );
        if( bSuccess )
            poDstBand->SetNoDataValue( dfValue );

        if( poSrcBand->GetColorInterpretation() != GCI_Undefined
            && poSrcBand->GetColorInterpretation()
            != poDstBand->GetColorInterpretation() )
            poDstBand->SetColorInterpretation(
                poSrcBand->GetColorInterpretation() );

        char** papszCatNames = poSrcBand->GetCategoryNames();
        if (nullptr != papszCatNames)
            poDstBand->SetCategoryNames( papszCatNames );

        // Only copy RAT if it is of reasonable size to fit in memory
        GDALRasterAttributeTable* poRAT = poSrcBand->GetDefaultRAT();
        if( poRAT != nullptr &&
            static_cast<GIntBig>(poRAT->GetColumnCount()) *
                poRAT->GetRowCount() < 1024 * 1024 )
        {
            poDstBand->SetDefaultRAT(poRAT);
        }

        if( !bStrict )
        {
            CPLPopErrorHandler();
            CPLErrorReset();
        }
        else
        {
            eErr = CPLGetLastErrorType();
        }
    }

/* -------------------------------------------------------------------- */
/*      Copy image data.                                                */
/* -------------------------------------------------------------------- */
    if( eErr == CE_None && nDstBands > 0 )
        eErr = GDALDatasetCopyWholeRaster( poSrcDS, poDstDS,
                                           nullptr, pfnProgress, pProgressData );

/* -------------------------------------------------------------------- */
/*      Should we copy some masks over?                                 */
/* -------------------------------------------------------------------- */
    if( eErr == CE_None && nDstBands > 0 )
        eErr = DefaultCopyMasks( poSrcDS, poDstDS, eErr );

/* -------------------------------------------------------------------- */
/*      Copy vector layers                                              */
/* -------------------------------------------------------------------- */
    if( eErr == CE_None )
    {
        if( nLayerCount > 0 && poDstDS->TestCapability(ODsCCreateLayer) )
        {
            for( int iLayer = 0; iLayer < nLayerCount; ++iLayer )
            {
                OGRLayer *poLayer = poSrcDS->GetLayer(iLayer);

                if( poLayer == nullptr )
                    continue;

                poDstDS->CopyLayer( poLayer, poLayer->GetName(), nullptr );
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Try to cleanup the output dataset if the translation failed.    */
/* -------------------------------------------------------------------- */
    if( eErr != CE_None )
    {
        delete poDstDS;
        if( !CPLFetchBool(papszOptions, "APPEND_SUBDATASET", false) )
        {
            // Only delete if creating a new file
            Delete( pszFilename );
        }
        return nullptr;
    }
    else
    {
        CPLErrorReset();
    }

    return poDstDS;
}
//! @endcond

/************************************************************************/
/*                             CreateCopy()                             */
/************************************************************************/

/**
 * \brief Create a copy of a dataset.
 *
 * This method will attempt to create a copy of a raster dataset with the
 * indicated filename, and in this drivers format.  Band number, size,
 * type, projection, geotransform and so forth are all to be copied from
 * the provided template dataset.
 *
 * Note that many sequential write once formats (such as JPEG and PNG) don't
 * implement the Create() method but do implement this CreateCopy() method.
 * If the driver doesn't implement CreateCopy(), but does implement Create()
 * then the default CreateCopy() mechanism built on calling Create() will
 * be used.
 * So to test if CreateCopy() is available, you can test if GDAL_DCAP_CREATECOPY
 * or GDAL_DCAP_CREATE is set in the GDAL metadata.
 *
 * It is intended that CreateCopy() will often be used with a source dataset
 * which is a virtual dataset allowing configuration of band types, and other
 * information without actually duplicating raster data (see the VRT driver).
 * This is what is done by the gdal_translate utility for example.
 *
 * That function will try to validate the creation option list passed to the
 * driver with the GDALValidateCreationOptions() method. This check can be
 * disabled by defining the configuration option
 * GDAL_VALIDATE_CREATION_OPTIONS=NO.
 *
 * After you have finished working with the returned dataset, it is
 * <b>required</b> to close it with GDALClose(). This does not only close the
 * file handle, but also ensures that all the data and metadata has been written
 * to the dataset (GDALFlushCache() is not sufficient for that purpose).
 *
 * For multidimensional datasets, papszOptions can contain array creation options,
 * if they are prefixed with "ARRAY:". \see GDALGroup::CopyFrom() documentation
 * for further details regarding such options.
 *
 * @param pszFilename the name for the new dataset.  UTF-8 encoded.
 * @param poSrcDS the dataset being duplicated.
 * @param bStrict TRUE if the copy must be strictly equivalent, or more
 * normally FALSE indicating that the copy may adapt as needed for the
 * output format.
 * @param papszOptions additional format dependent options controlling
 * creation of the output file. The APPEND_SUBDATASET=YES option can be
 * specified to avoid prior destruction of existing dataset.
 * @param pfnProgress a function to be used to report progress of the copy.
 * @param pProgressData application data passed into progress function.
 *
 * @return a pointer to the newly created dataset (may be read-only access).
 */

GDALDataset *GDALDriver::CreateCopy( const char * pszFilename,
                                     GDALDataset * poSrcDS,
                                     int bStrict, char ** papszOptions,
                                     GDALProgressFunc pfnProgress,
                                     void * pProgressData )

{
    if( pfnProgress == nullptr )
        pfnProgress = GDALDummyProgress;

/* -------------------------------------------------------------------- */
/*      Make sure we cleanup if there is an existing dataset of this    */
/*      name.  But even if that seems to fail we will continue since    */
/*      it might just be a corrupt file or something.                   */
/* -------------------------------------------------------------------- */
    const bool bAppendSubdataset =
        CPLFetchBool(const_cast<const char **>(papszOptions),
                     "APPEND_SUBDATASET", false);
    if( !bAppendSubdataset &&
        CPLFetchBool(const_cast<const char **>(papszOptions),
                     "QUIET_DELETE_ON_CREATE_COPY", true) )
    {
        // Someone issuing CreateCopy("foo.tif") on a
        // memory driver doesn't expect files with those names to be deleted
        // on a file system...
        // This is somewhat messy. Ideally there should be a way for the
        // driver to overload the default behavior
        if( !EQUAL(GetDescription(), "MEM") &&
            !EQUAL(GetDescription(), "Memory") )
        {
            QuietDelete( pszFilename );
        }
    }

    char** papszOptionsToDelete = nullptr;
    int iIdxQuietDeleteOnCreateCopy =
        CSLPartialFindString(papszOptions, "QUIET_DELETE_ON_CREATE_COPY=");
    if( iIdxQuietDeleteOnCreateCopy >= 0 )
    {
        //if( papszOptionsToDelete == nullptr )
            papszOptionsToDelete = CSLDuplicate(papszOptions);
        papszOptions =
            CSLRemoveStrings(papszOptionsToDelete, iIdxQuietDeleteOnCreateCopy,
                             1, nullptr);
        papszOptionsToDelete = papszOptions;
    }

/* -------------------------------------------------------------------- */
/*      If _INTERNAL_DATASET=YES, the returned dataset will not be      */
/*      registered in the global list of open datasets.                 */
/* -------------------------------------------------------------------- */
    const int iIdxInternalDataset =
        CSLPartialFindString(papszOptions, "_INTERNAL_DATASET=");
    bool bInternalDataset = false;
    if( iIdxInternalDataset >= 0 )
    {
        bInternalDataset =
            CPLFetchBool(const_cast<const char **>(papszOptions),
                         "_INTERNAL_DATASET", false);
        if( papszOptionsToDelete == nullptr )
            papszOptionsToDelete = CSLDuplicate(papszOptions);
        papszOptions =
            CSLRemoveStrings(papszOptionsToDelete, iIdxInternalDataset,
                             1, nullptr);
        papszOptionsToDelete = papszOptions;
    }

/* -------------------------------------------------------------------- */
/*      Validate creation options.                                      */
/* -------------------------------------------------------------------- */
    if( CPLTestBool(
            CPLGetConfigOption("GDAL_VALIDATE_CREATION_OPTIONS", "YES") ) )
    {
        auto poSrcGroup = poSrcDS->GetRootGroup();
        if( poSrcGroup != nullptr && GetMetadataItem(GDAL_DCAP_MULTIDIM_RASTER) )
        {
            CPLStringList aosDatasetCO;
            for( CSLConstList papszIter = papszOptions; papszIter && *papszIter; ++papszIter )
            {
                if( !STARTS_WITH_CI(*papszIter, "ARRAY:") )
                    aosDatasetCO.AddString(*papszIter);
            }
            GDALValidateCreationOptions( this, aosDatasetCO.List() );
        }
        else
        {
            GDALValidateCreationOptions( this, papszOptions );
        }
    }

/* -------------------------------------------------------------------- */
/*      Advise the source raster that we are going to read it completely */
/* -------------------------------------------------------------------- */

    const int nXSize = poSrcDS->GetRasterXSize();
    const int nYSize = poSrcDS->GetRasterYSize();
    const int nBandCount = poSrcDS->GetRasterCount();
    GDALDataType eDT = GDT_Unknown;
    if( nBandCount > 0 )
    {
        GDALRasterBand* poSrcBand = poSrcDS->GetRasterBand(1);
        if( poSrcBand )
            eDT = poSrcBand->GetRasterDataType();
    }
    poSrcDS->AdviseRead( 0, 0, nXSize, nYSize, nXSize, nYSize, eDT,
                         nBandCount, nullptr, nullptr );

/* -------------------------------------------------------------------- */
/*      If the format provides a CreateCopy() method use that,          */
/*      otherwise fallback to the internal implementation using the     */
/*      Create() method.                                                */
/* -------------------------------------------------------------------- */
    GDALDataset *poDstDS = nullptr;
    if( pfnCreateCopy != nullptr &&
        !CPLTestBool(CPLGetConfigOption("GDAL_DEFAULT_CREATE_COPY", "NO")) )
    {
        poDstDS = pfnCreateCopy( pszFilename, poSrcDS, bStrict, papszOptions,
                                 pfnProgress, pProgressData );
        if( poDstDS != nullptr )
        {
            if( poDstDS->GetDescription() == nullptr
                || strlen(poDstDS->GetDescription()) == 0 )
                poDstDS->SetDescription( pszFilename );

            if( poDstDS->poDriver == nullptr )
                poDstDS->poDriver = this;

            if( !bInternalDataset )
                poDstDS->AddToDatasetOpenList();
        }
    }
    else
    {
        poDstDS = DefaultCreateCopy( pszFilename, poSrcDS, bStrict,
                                  papszOptions, pfnProgress, pProgressData );
    }

    CSLDestroy(papszOptionsToDelete);
    return poDstDS;
}

/************************************************************************/
/*                           GDALCreateCopy()                           */
/************************************************************************/

/**
 * \brief Create a copy of a dataset.
 *
 * @see GDALDriver::CreateCopy()
 */

GDALDatasetH CPL_STDCALL GDALCreateCopy( GDALDriverH hDriver,
                                         const char * pszFilename,
                                         GDALDatasetH hSrcDS,
                                         int bStrict, CSLConstList papszOptions,
                                         GDALProgressFunc pfnProgress,
                                         void * pProgressData )

{
    VALIDATE_POINTER1( hDriver, "GDALCreateCopy", nullptr );
    VALIDATE_POINTER1( hSrcDS, "GDALCreateCopy", nullptr );

    return GDALDriver::FromHandle(hDriver)->
        CreateCopy( pszFilename, GDALDataset::FromHandle(hSrcDS),
                    bStrict, const_cast<char**>(papszOptions),
                    pfnProgress, pProgressData );
}

/************************************************************************/
/*                            QuietDelete()                             */
/************************************************************************/

/**
 * \brief Delete dataset if found.
 *
 * This is a helper method primarily used by Create() and
 * CreateCopy() to predelete any dataset of the name soon to be
 * created.  It will attempt to delete the named dataset if
 * one is found, otherwise it does nothing.  An error is only
 * returned if the dataset is found but the delete fails.
 *
 * This is a static method and it doesn't matter what driver instance
 * it is invoked on.  It will attempt to discover the correct driver
 * using Identify().
 *
 * @param pszName the dataset name to try and delete.
 * @param papszAllowedDrivers NULL to consider all candidate drivers, or a NULL
 * terminated list of strings with the driver short names that must be
 * considered. (Note: functionality currently broken. Argument considered as NULL)
 * @return CE_None if the dataset does not exist, or is deleted without issues.
 */

CPLErr GDALDriver::QuietDelete( const char *pszName,
                                const char *const *papszAllowedDrivers )

{
    // FIXME! GDALIdentifyDriver() accepts a file list, not a driver list
    CPL_IGNORE_RET_VAL(papszAllowedDrivers);

    VSIStatBufL sStat;
    const bool bExists =
        VSIStatExL(pszName, &sStat,
                   VSI_STAT_EXISTS_FLAG | VSI_STAT_NATURE_FLAG) == 0;

#ifdef S_ISFIFO
    if( bExists && S_ISFIFO(sStat.st_mode) )
        return CE_None;
#endif

    if( bExists &&
        VSI_ISDIR(sStat.st_mode) )
    {
        // It is not desirable to remove directories quietly.  Necessary to
        // avoid ogr_mitab_12 to destroy file created at ogr_mitab_7.
        return CE_None;
    }

    CPLPushErrorHandler(CPLQuietErrorHandler);
    GDALDriver * const poDriver =
        GDALDriver::FromHandle( GDALIdentifyDriver( pszName, nullptr ) );
    CPLPopErrorHandler();

    if( poDriver == nullptr )
        return CE_None;

    CPLDebug( "GDAL", "QuietDelete(%s) invoking Delete()", pszName );

    const bool bQuiet =
        !bExists && poDriver->pfnDelete == nullptr &&
        poDriver->pfnDeleteDataSource == nullptr;
    if( bQuiet )
        CPLPushErrorHandler(CPLQuietErrorHandler);
    CPLErr eErr = poDriver->Delete( pszName );
    if( bQuiet )
    {
        CPLPopErrorHandler();
        CPLErrorReset();
        eErr = CE_None;
    }
    return eErr;
}

/************************************************************************/
/*                               Delete()                               */
/************************************************************************/

/**
 * \brief Delete named dataset.
 *
 * The driver will attempt to delete the named dataset in a driver specific
 * fashion.  Full featured drivers will delete all associated files,
 * database objects, or whatever is appropriate.  The default behavior when
 * no driver specific behavior is provided is to attempt to delete all the
 * files that are returned by GDALGetFileList() on the dataset handle.
 *
 * It is unwise to have open dataset handles on this dataset when it is
 * deleted.
 *
 * Equivalent of the C function GDALDeleteDataset().
 *
 * @param pszFilename name of dataset to delete.
 *
 * @return CE_None on success, or CE_Failure if the operation fails.
 */

CPLErr GDALDriver::Delete( const char * pszFilename )

{
    if( pfnDelete != nullptr )
        return pfnDelete( pszFilename );
    else if( pfnDeleteDataSource != nullptr )
        return pfnDeleteDataSource( this, pszFilename );

/* -------------------------------------------------------------------- */
/*      Collect file list.                                              */
/* -------------------------------------------------------------------- */
    GDALDatasetH hDS = GDALOpenEx(pszFilename, 0, nullptr, nullptr, nullptr);

    if( hDS == nullptr )
    {
        if( CPLGetLastErrorNo() == 0 )
            CPLError( CE_Failure, CPLE_OpenFailed,
                      "Unable to open %s to obtain file list.", pszFilename );

        return CE_Failure;
    }

    char **papszFileList = GDALGetFileList( hDS );

    GDALClose( hDS );
    hDS = nullptr;

    if( CSLCount( papszFileList ) == 0 )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Unable to determine files associated with %s, "
                  "delete fails.", pszFilename );
        CSLDestroy( papszFileList );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Delete all files.                                               */
/* -------------------------------------------------------------------- */
    CPLErr eErr = CE_None;
    for( int i = 0; papszFileList[i] != nullptr; ++i )
    {
        if( VSIUnlink( papszFileList[i] ) != 0 )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Deleting %s failed:\n%s",
                      papszFileList[i],
                      VSIStrerror(errno) );
            eErr = CE_Failure;
        }
    }

    CSLDestroy( papszFileList );

    return eErr;
}

/************************************************************************/
/*                         GDALDeleteDataset()                          */
/************************************************************************/

/**
 * \brief Delete named dataset.
 *
 * @see GDALDriver::Delete()
 */

CPLErr CPL_STDCALL GDALDeleteDataset( GDALDriverH hDriver,
                                      const char * pszFilename )

{
    if( hDriver == nullptr )
        hDriver = GDALIdentifyDriver( pszFilename, nullptr );

    if( hDriver == nullptr )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "No identifiable driver for %s.",
                  pszFilename );
        return CE_Failure;
    }

    return GDALDriver::FromHandle(hDriver)->Delete( pszFilename );
}

/************************************************************************/
/*                           DefaultRename()                            */
/*                                                                      */
/*      The generic implementation based on the file list used when     */
/*      there is no format specific implementation.                     */
/************************************************************************/

//! @cond Doxygen_Suppress
CPLErr GDALDriver::DefaultRename( const char * pszNewName,
                                  const char *pszOldName )

{
/* -------------------------------------------------------------------- */
/*      Collect file list.                                              */
/* -------------------------------------------------------------------- */
    GDALDatasetH hDS = GDALOpen(pszOldName, GA_ReadOnly);

    if( hDS == nullptr )
    {
        if( CPLGetLastErrorNo() == 0 )
            CPLError( CE_Failure, CPLE_OpenFailed,
                      "Unable to open %s to obtain file list.", pszOldName );

        return CE_Failure;
    }

    char **papszFileList = GDALGetFileList( hDS );

    GDALClose( hDS );

    if( CSLCount( papszFileList ) == 0 )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Unable to determine files associated with %s,\n"
                  "rename fails.", pszOldName );

        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Produce a list of new filenames that correspond to the old      */
/*      names.                                                          */
/* -------------------------------------------------------------------- */
    CPLErr eErr = CE_None;
    char **papszNewFileList =
        CPLCorrespondingPaths( pszOldName, pszNewName, papszFileList );

    if( papszNewFileList == nullptr )
        return CE_Failure;

    for( int i = 0; papszFileList[i] != nullptr; ++i )
    {
        if( CPLMoveFile( papszNewFileList[i], papszFileList[i] ) != 0 )
        {
            eErr = CE_Failure;
            // Try to put the ones we moved back.
            for( --i; i >= 0; i-- )
                CPLMoveFile( papszFileList[i], papszNewFileList[i] );
            break;
        }
    }

    CSLDestroy( papszNewFileList );
    CSLDestroy( papszFileList );

    return eErr;
}
//! @endcond

/************************************************************************/
/*                               Rename()                               */
/************************************************************************/

/**
 * \brief Rename a dataset.
 *
 * Rename a dataset. This may including moving the dataset to a new directory
 * or even a new filesystem.
 *
 * It is unwise to have open dataset handles on this dataset when it is
 * being renamed.
 *
 * Equivalent of the C function GDALRenameDataset().
 *
 * @param pszNewName new name for the dataset.
 * @param pszOldName old name for the dataset.
 *
 * @return CE_None on success, or CE_Failure if the operation fails.
 */

CPLErr GDALDriver::Rename( const char * pszNewName, const char *pszOldName )

{
    if( pfnRename != nullptr )
        return pfnRename( pszNewName, pszOldName );

    return DefaultRename( pszNewName, pszOldName );
}

/************************************************************************/
/*                         GDALRenameDataset()                          */
/************************************************************************/

/**
 * \brief Rename a dataset.
 *
 * @see GDALDriver::Rename()
 */

CPLErr CPL_STDCALL GDALRenameDataset( GDALDriverH hDriver,
                                      const char * pszNewName,
                                      const char * pszOldName )

{
    if( hDriver == nullptr )
        hDriver = GDALIdentifyDriver( pszOldName, nullptr );

    if( hDriver == nullptr )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "No identifiable driver for %s.",
                  pszOldName );
        return CE_Failure;
    }

    return GDALDriver::FromHandle(hDriver)->Rename( pszNewName, pszOldName );
}

/************************************************************************/
/*                          DefaultCopyFiles()                          */
/*                                                                      */
/*      The default implementation based on file lists used when        */
/*      there is no format specific implementation.                     */
/************************************************************************/

//! @cond Doxygen_Suppress
CPLErr GDALDriver::DefaultCopyFiles( const char *pszNewName,
                                     const char *pszOldName )

{
/* -------------------------------------------------------------------- */
/*      Collect file list.                                              */
/* -------------------------------------------------------------------- */
    GDALDatasetH hDS = GDALOpen(pszOldName,GA_ReadOnly);

    if( hDS == nullptr )
    {
        if( CPLGetLastErrorNo() == 0 )
            CPLError( CE_Failure, CPLE_OpenFailed,
                      "Unable to open %s to obtain file list.", pszOldName );

        return CE_Failure;
    }

    char **papszFileList = GDALGetFileList( hDS );

    GDALClose( hDS );
    hDS = nullptr;

    if( CSLCount( papszFileList ) == 0 )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Unable to determine files associated with %s,\n"
                  "rename fails.", pszOldName );

        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Produce a list of new filenames that correspond to the old      */
/*      names.                                                          */
/* -------------------------------------------------------------------- */
    CPLErr eErr = CE_None;
    char **papszNewFileList =
        CPLCorrespondingPaths( pszOldName, pszNewName, papszFileList );

    if( papszNewFileList == nullptr )
        return CE_Failure;

    for( int i = 0; papszFileList[i] != nullptr; ++i )
    {
        if( CPLCopyFile( papszNewFileList[i], papszFileList[i] ) != 0 )
        {
            eErr = CE_Failure;
            // Try to put the ones we moved back.
            for( --i; i >= 0; --i )
                VSIUnlink( papszNewFileList[i] );
            break;
        }
    }

    CSLDestroy( papszNewFileList );
    CSLDestroy( papszFileList );

    return eErr;
}
//! @endcond

/************************************************************************/
/*                             CopyFiles()                              */
/************************************************************************/

/**
 * \brief Copy the files of a dataset.
 *
 * Copy all the files associated with a dataset.
 *
 * Equivalent of the C function GDALCopyDatasetFiles().
 *
 * @param pszNewName new name for the dataset.
 * @param pszOldName old name for the dataset.
 *
 * @return CE_None on success, or CE_Failure if the operation fails.
 */

CPLErr GDALDriver::CopyFiles( const char *pszNewName, const char *pszOldName )

{
    if( pfnCopyFiles != nullptr )
        return pfnCopyFiles( pszNewName, pszOldName );

    return DefaultCopyFiles( pszNewName, pszOldName );
}

/************************************************************************/
/*                        GDALCopyDatasetFiles()                        */
/************************************************************************/

/**
 * \brief Copy the files of a dataset.
 *
 * @see GDALDriver::CopyFiles()
 */

CPLErr CPL_STDCALL GDALCopyDatasetFiles( GDALDriverH hDriver,
                                         const char *pszNewName,
                                         const char *pszOldName )

{
    if( hDriver == nullptr )
        hDriver = GDALIdentifyDriver( pszOldName, nullptr );

    if( hDriver == nullptr )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "No identifiable driver for %s.",
                  pszOldName );
        return CE_Failure;
    }

    return GDALDriver::FromHandle(hDriver)->
        CopyFiles( pszNewName, pszOldName );
}

/************************************************************************/
/*                       GDALGetDriverShortName()                       */
/************************************************************************/

/**
 * \brief Return the short name of a driver
 *
 * This is the string that can be
 * passed to the GDALGetDriverByName() function.
 *
 * For the GeoTIFF driver, this is "GTiff"
 *
 * @param hDriver the handle of the driver
 * @return the short name of the driver. The
 *         returned string should not be freed and is owned by the driver.
 */

const char * CPL_STDCALL GDALGetDriverShortName( GDALDriverH hDriver )

{
    VALIDATE_POINTER1( hDriver, "GDALGetDriverShortName", nullptr );

    return GDALDriver::FromHandle(hDriver)->GetDescription();
}

/************************************************************************/
/*                       GDALGetDriverLongName()                        */
/************************************************************************/

/**
 * \brief Return the long name of a driver
 *
 * For the GeoTIFF driver, this is "GeoTIFF"
 *
 * @param hDriver the handle of the driver
 * @return the long name of the driver or empty string. The
 *         returned string should not be freed and is owned by the driver.
 */

const char * CPL_STDCALL GDALGetDriverLongName( GDALDriverH hDriver )

{
    VALIDATE_POINTER1( hDriver, "GDALGetDriverLongName", nullptr );

    const char *pszLongName =
        GDALDriver::FromHandle(hDriver)->
            GetMetadataItem( GDAL_DMD_LONGNAME );

    if( pszLongName == nullptr )
        return "";

    return pszLongName;
}

/************************************************************************/
/*                       GDALGetDriverHelpTopic()                       */
/************************************************************************/

/**
 * \brief Return the URL to the help that describes the driver
 *
 * That URL is relative to the GDAL documentation directory.
 *
 * For the GeoTIFF driver, this is "frmt_gtiff.html"
 *
 * @param hDriver the handle of the driver
 * @return the URL to the help that describes the driver or NULL. The
 *         returned string should not be freed and is owned by the driver.
 */

const char * CPL_STDCALL GDALGetDriverHelpTopic( GDALDriverH hDriver )

{
    VALIDATE_POINTER1( hDriver, "GDALGetDriverHelpTopic", nullptr );

    return GDALDriver::FromHandle(hDriver)->
        GetMetadataItem( GDAL_DMD_HELPTOPIC );
}

/************************************************************************/
/*                   GDALGetDriverCreationOptionList()                  */
/************************************************************************/

/**
 * \brief Return the list of creation options of the driver
 *
 * Return the list of creation options of the driver used by Create() and
 * CreateCopy() as an XML string
 *
 * @param hDriver the handle of the driver
 * @return an XML string that describes the list of creation options or
 *         empty string. The returned string should not be freed and is
 *         owned by the driver.
 */

const char * CPL_STDCALL GDALGetDriverCreationOptionList( GDALDriverH hDriver )

{
    VALIDATE_POINTER1( hDriver, "GDALGetDriverCreationOptionList", nullptr );

    const char *pszOptionList =
        GDALDriver::FromHandle(hDriver)->
            GetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST );

    if( pszOptionList == nullptr )
        return "";

    return pszOptionList;
}

/************************************************************************/
/*                   GDALValidateCreationOptions()                      */
/************************************************************************/

/**
 * \brief Validate the list of creation options that are handled by a driver
 *
 * This is a helper method primarily used by Create() and
 * CreateCopy() to validate that the passed in list of creation options
 * is compatible with the GDAL_DMD_CREATIONOPTIONLIST metadata item defined
 * by some drivers. @see GDALGetDriverCreationOptionList()
 *
 * If the GDAL_DMD_CREATIONOPTIONLIST metadata item is not defined, this
 * function will return TRUE. Otherwise it will check that the keys and values
 * in the list of creation options are compatible with the capabilities declared
 * by the GDAL_DMD_CREATIONOPTIONLIST metadata item. In case of incompatibility
 * a (non fatal) warning will be emitted and FALSE will be returned.
 *
 * @param hDriver the handle of the driver with whom the lists of creation option
 *                must be validated
 * @param papszCreationOptions the list of creation options. An array of strings,
 *                             whose last element is a NULL pointer
 * @return TRUE if the list of creation options is compatible with the Create()
 *         and CreateCopy() method of the driver, FALSE otherwise.
 */

int CPL_STDCALL GDALValidateCreationOptions( GDALDriverH hDriver,
                                             CSLConstList papszCreationOptions )
{
    VALIDATE_POINTER1( hDriver, "GDALValidateCreationOptions", FALSE );
    const char *pszOptionList =
        GDALDriver::FromHandle(hDriver)->GetMetadataItem(
            GDAL_DMD_CREATIONOPTIONLIST );
    CPLString osDriver;
    osDriver.Printf("driver %s",
                    GDALDriver::FromHandle(hDriver)->GetDescription());
    CSLConstList papszOptionsToValidate = papszCreationOptions;
    char** papszOptionsToFree = nullptr;
    if( CSLFetchNameValue( papszCreationOptions, "APPEND_SUBDATASET") )
    {
        papszOptionsToFree =
            CSLSetNameValue(CSLDuplicate(papszCreationOptions),
                            "APPEND_SUBDATASET", nullptr);
        papszOptionsToValidate = papszOptionsToFree;
    }
    const bool bRet = CPL_TO_BOOL(
        GDALValidateOptions(
            pszOptionList,
            papszOptionsToValidate,
            "creation option",
            osDriver ) );
    CSLDestroy(papszOptionsToFree);
    return bRet;
}

/************************************************************************/
/*                     GDALValidateOpenOptions()                        */
/************************************************************************/

int GDALValidateOpenOptions( GDALDriverH hDriver,
                             const char* const* papszOpenOptions)
{
    VALIDATE_POINTER1( hDriver, "GDALValidateOpenOptions", FALSE );
    const char *pszOptionList = GDALDriver::FromHandle(hDriver)->
        GetMetadataItem( GDAL_DMD_OPENOPTIONLIST );
    CPLString osDriver;
    osDriver.Printf("driver %s",
                    GDALDriver::FromHandle(hDriver)->GetDescription());
    return GDALValidateOptions( pszOptionList, papszOpenOptions,
                                "open option",
                                osDriver );
}

/************************************************************************/
/*                           GDALValidateOptions()                      */
/************************************************************************/

int GDALValidateOptions( const char* pszOptionList,
                         const char* const* papszOptionsToValidate,
                         const char* pszErrorMessageOptionType,
                         const char* pszErrorMessageContainerName)
{
    if( papszOptionsToValidate == nullptr || *papszOptionsToValidate == nullptr)
        return TRUE;
    if( pszOptionList == nullptr )
        return TRUE;

    CPLXMLNode* psNode = CPLParseXMLString(pszOptionList);
    if (psNode == nullptr)
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "Could not parse %s list of %s. Assuming options are valid.",
                 pszErrorMessageOptionType, pszErrorMessageContainerName);
        return TRUE;
    }

    bool bRet = true;
    while(*papszOptionsToValidate)
    {
        char* pszKey = nullptr;
        const char* pszValue = CPLParseNameValue(*papszOptionsToValidate, &pszKey);
        if (pszKey == nullptr)
        {
            CPLError(CE_Warning, CPLE_NotSupported,
                     "%s '%s' is not formatted with the key=value format",
                     pszErrorMessageOptionType,
                     *papszOptionsToValidate);
            bRet = false;

            ++papszOptionsToValidate;
            continue;
        }

        if( EQUAL(pszKey, "VALIDATE_OPEN_OPTIONS") )
        {
            ++papszOptionsToValidate;
            CPLFree(pszKey);
            continue;
        }

        // Must we be forgiving in case of missing option ?
        bool bWarnIfMissingKey = true;
        if( pszKey[0] == '@' )
        {
            bWarnIfMissingKey = false;
            memmove(pszKey, pszKey + 1, strlen(pszKey+1)+1);
        }

        CPLXMLNode* psChildNode = psNode->psChild;
        while(psChildNode)
        {
            if (EQUAL(psChildNode->pszValue, "OPTION"))
            {
                const char* pszOptionName = CPLGetXMLValue(psChildNode, "name", "");
                /* For option names terminated by wildcard (NITF BLOCKA option names for example) */
                if (strlen(pszOptionName) > 0 &&
                    pszOptionName[strlen(pszOptionName) - 1] == '*' &&
                    EQUALN(pszOptionName, pszKey, strlen(pszOptionName) - 1))
                {
                    break;
                }

                /* For option names beginning by a wildcard */
                if( pszOptionName[0] == '*' &&
                    strlen(pszKey) > strlen(pszOptionName) &&
                    EQUAL( pszKey + strlen(pszKey) - strlen(pszOptionName + 1), pszOptionName + 1 ) )
                {
                    break;
                }

                if (EQUAL(pszOptionName, pszKey) )
                {
                    break;
                }
                const char* pszAlias = CPLGetXMLValue(psChildNode, "alias",
                            CPLGetXMLValue(psChildNode, "deprecated_alias", ""));
                if (EQUAL(pszAlias, pszKey) )
                {
                    CPLDebug("GDAL", "Using deprecated alias '%s'. New name is '%s'",
                             pszAlias, pszOptionName);
                    break;
                }
            }
            psChildNode = psChildNode->psNext;
        }
        if (psChildNode == nullptr)
        {
            if( bWarnIfMissingKey &&
                (!EQUAL(pszErrorMessageOptionType, "open option") ||
                 CPLFetchBool(papszOptionsToValidate,
                              "VALIDATE_OPEN_OPTIONS", true)) )
            {
                CPLError(CE_Warning, CPLE_NotSupported,
                        "%s does not support %s %s",
                        pszErrorMessageContainerName,
                        pszErrorMessageOptionType,
                        pszKey);
                bRet = false;
            }

            CPLFree(pszKey);
            ++papszOptionsToValidate;
            continue;
        }

#ifdef DEBUG
        CPLXMLNode* psChildSubNode = psChildNode->psChild;
        while(psChildSubNode)
        {
            if( psChildSubNode->eType == CXT_Attribute )
            {
                if( !(EQUAL(psChildSubNode->pszValue, "name") ||
                      EQUAL(psChildSubNode->pszValue, "alias") ||
                      EQUAL(psChildSubNode->pszValue, "deprecated_alias") ||
                      EQUAL(psChildSubNode->pszValue, "alt_config_option") ||
                      EQUAL(psChildSubNode->pszValue, "description") ||
                      EQUAL(psChildSubNode->pszValue, "type") ||
                      EQUAL(psChildSubNode->pszValue, "min") ||
                      EQUAL(psChildSubNode->pszValue, "max") ||
                      EQUAL(psChildSubNode->pszValue, "default") ||
                      EQUAL(psChildSubNode->pszValue, "maxsize") ||
                      EQUAL(psChildSubNode->pszValue, "required") ||
                      EQUAL(psChildSubNode->pszValue, "scope")) )
                {
                    /* Driver error */
                    CPLError(CE_Warning, CPLE_NotSupported,
                             "%s : unhandled attribute '%s' for %s %s.",
                             pszErrorMessageContainerName,
                             psChildSubNode->pszValue,
                             pszKey,
                             pszErrorMessageOptionType);
                }
            }
            psChildSubNode = psChildSubNode->psNext;
        }
#endif

        const char* pszType = CPLGetXMLValue(psChildNode, "type", nullptr);
        const char* pszMin = CPLGetXMLValue(psChildNode, "min", nullptr);
        const char* pszMax = CPLGetXMLValue(psChildNode, "max", nullptr);
        if (pszType != nullptr)
        {
            if (EQUAL(pszType, "INT") || EQUAL(pszType, "INTEGER"))
            {
                const char* pszValueIter = pszValue;
                while (*pszValueIter)
                {
                    if (!((*pszValueIter >= '0' && *pszValueIter <= '9') ||
                           *pszValueIter == '+' || *pszValueIter == '-'))
                    {
                        CPLError(CE_Warning, CPLE_NotSupported,
                             "'%s' is an unexpected value for %s %s of type int.",
                             pszValue, pszKey, pszErrorMessageOptionType);
                        bRet = false;
                        break;
                    }
                    ++pszValueIter;
                }
                if( *pszValueIter == '\0' )
                {
                    if( pszMin && atoi(pszValue) < atoi(pszMin) )
                    {
                        CPLError(CE_Warning, CPLE_NotSupported,
                             "'%s' is an unexpected value for %s %s that should be >= %s.",
                             pszValue, pszKey, pszErrorMessageOptionType, pszMin);
                        bRet = false;
                    }
                    if( pszMax && atoi(pszValue) > atoi(pszMax) )
                    {
                        CPLError(CE_Warning, CPLE_NotSupported,
                             "'%s' is an unexpected value for %s %s that should be <= %s.",
                             pszValue, pszKey, pszErrorMessageOptionType, pszMax);
                        bRet = false;
                    }
                }
            }
            else if (EQUAL(pszType, "UNSIGNED INT"))
            {
                const char* pszValueIter = pszValue;
                while (*pszValueIter)
                {
                    if (!((*pszValueIter >= '0' && *pszValueIter <= '9') ||
                           *pszValueIter == '+'))
                    {
                        CPLError(CE_Warning, CPLE_NotSupported,
                             "'%s' is an unexpected value for %s %s of type unsigned int.",
                             pszValue, pszKey, pszErrorMessageOptionType);
                        bRet = false;
                        break;
                    }
                    ++pszValueIter;
                }
                if( *pszValueIter == '\0' )
                {
                    if( pszMin && atoi(pszValue) < atoi(pszMin) )
                    {
                        CPLError(CE_Warning, CPLE_NotSupported,
                            "'%s' is an unexpected value for %s %s that should be >= %s.",
                            pszValue, pszKey, pszErrorMessageOptionType, pszMin);
                        bRet = false;
                    }
                    if( pszMax && atoi(pszValue) > atoi(pszMax) )
                    {
                        CPLError(CE_Warning, CPLE_NotSupported,
                            "'%s' is an unexpected value for %s %s that should be <= %s.",
                            pszValue, pszKey, pszErrorMessageOptionType, pszMax);
                        bRet = false;
                    }
                }
            }
            else if (EQUAL(pszType, "FLOAT"))
            {
                char* endPtr = nullptr;
                double dfVal = CPLStrtod(pszValue, &endPtr);
                if ( !(endPtr == nullptr || *endPtr == '\0') )
                {
                    CPLError(CE_Warning, CPLE_NotSupported,
                             "'%s' is an unexpected value for %s %s of type float.",
                             pszValue, pszKey, pszErrorMessageOptionType);
                    bRet = false;
                }
                else
                {
                    if( pszMin && dfVal < CPLAtof(pszMin) )
                    {
                        CPLError(CE_Warning, CPLE_NotSupported,
                             "'%s' is an unexpected value for %s %s that should be >= %s.",
                             pszValue, pszKey, pszErrorMessageOptionType, pszMin);
                        bRet = false;
                    }
                    if( pszMax && dfVal > CPLAtof(pszMax) )
                    {
                        CPLError(CE_Warning, CPLE_NotSupported,
                             "'%s' is an unexpected value for %s %s that should be <= %s.",
                             pszValue, pszKey, pszErrorMessageOptionType, pszMax);
                        bRet = false;
                    }
                }
            }
            else if (EQUAL(pszType, "BOOLEAN"))
            {
                if (!(EQUAL(pszValue, "ON") || EQUAL(pszValue, "TRUE") || EQUAL(pszValue, "YES") ||
                      EQUAL(pszValue, "OFF") || EQUAL(pszValue, "FALSE") || EQUAL(pszValue, "NO")))
                {
                    CPLError(CE_Warning, CPLE_NotSupported,
                             "'%s' is an unexpected value for %s %s of type boolean.",
                             pszValue, pszKey, pszErrorMessageOptionType);
                    bRet = false;
                }
            }
            else if (EQUAL(pszType, "STRING-SELECT"))
            {
                bool bMatchFound = false;
                CPLXMLNode* psStringSelect = psChildNode->psChild;
                while(psStringSelect)
                {
                    if (psStringSelect->eType == CXT_Element &&
                        EQUAL(psStringSelect->pszValue, "Value"))
                    {
                        CPLXMLNode* psOptionNode = psStringSelect->psChild;
                        while(psOptionNode)
                        {
                            if (psOptionNode->eType == CXT_Text &&
                                EQUAL(psOptionNode->pszValue, pszValue))
                            {
                                bMatchFound = true;
                                break;
                            }
                            if( psOptionNode->eType == CXT_Attribute &&
                                (EQUAL(psOptionNode->pszValue, "alias") ||
                                 EQUAL(psOptionNode->pszValue, "deprecated_alias") ) &&
                                 EQUAL(psOptionNode->psChild->pszValue, pszValue) )
                            {
                                bMatchFound = true;
                                break;
                            }
                            psOptionNode = psOptionNode->psNext;
                        }
                        if (bMatchFound)
                            break;
                    }
                    psStringSelect = psStringSelect->psNext;
                }
                if (!bMatchFound)
                {
                    CPLError(CE_Warning, CPLE_NotSupported,
                             "'%s' is an unexpected value for %s %s of type string-select.",
                             pszValue, pszKey, pszErrorMessageOptionType);
                    bRet = false;
                }
            }
            else if (EQUAL(pszType, "STRING"))
            {
                const char* pszMaxSize = CPLGetXMLValue(psChildNode, "maxsize", nullptr);
                if (pszMaxSize != nullptr)
                {
                    if (static_cast<int>(strlen(pszValue)) > atoi(pszMaxSize))
                    {
                        CPLError(CE_Warning, CPLE_NotSupported,
                             "'%s' is of size %d, whereas maximum size for %s %s is %d.",
                             pszValue, static_cast<int>(strlen(pszValue)), pszKey,
                                 pszErrorMessageOptionType, atoi(pszMaxSize));
                        bRet = false;
                    }
                }
            }
            else
            {
                /* Driver error */
                CPLError(CE_Warning, CPLE_NotSupported,
                         "%s : type '%s' for %s %s is not recognized.",
                         pszErrorMessageContainerName,
                         pszType,
                         pszKey,
                         pszErrorMessageOptionType);
            }
        }
        else
        {
            /* Driver error */
            CPLError(CE_Warning, CPLE_NotSupported,
                     "%s : no type for %s %s.",
                     pszErrorMessageContainerName,
                     pszKey,
                     pszErrorMessageOptionType);
        }
        CPLFree(pszKey);
        ++papszOptionsToValidate;
    }

    CPLDestroyXMLNode(psNode);
    return bRet ? TRUE : FALSE;
}

/************************************************************************/
/*                         GDALIdentifyDriver()                         */
/************************************************************************/

/**
 * \brief Identify the driver that can open a raster file.
 *
 * This function will try to identify the driver that can open the passed file
 * name by invoking the Identify method of each registered GDALDriver in turn.
 * The first driver that successful identifies the file name will be returned.
 * If all drivers fail then NULL is returned.
 *
 * In order to reduce the need for such searches touch the operating system
 * file system machinery, it is possible to give an optional list of files.
 * This is the list of all files at the same level in the file system as the
 * target file, including the target file. The filenames will not include any
 * path components, are essentially just the output of VSIReadDir() on the
 * parent directory. If the target object does not have filesystem semantics
 * then the file list should be NULL.
 *
 * @param pszFilename the name of the file to access.  In the case of
 * exotic drivers this may not refer to a physical file, but instead contain
 * information for the driver on how to access a dataset.
 *
 * @param papszFileList an array of strings, whose last element is the NULL
 * pointer.  These strings are filenames that are auxiliary to the main
 * filename. The passed value may be NULL.
 *
 * @return A GDALDriverH handle or NULL on failure.  For C++ applications
 * this handle can be cast to a GDALDriver *.
 */

GDALDriverH CPL_STDCALL
GDALIdentifyDriver( const char * pszFilename,
                    CSLConstList papszFileList )

{
    return GDALIdentifyDriverEx( pszFilename, 0, nullptr, papszFileList );
}

/************************************************************************/
/*                         GDALIdentifyDriverEx()                       */
/************************************************************************/

/**
 * \brief Identify the driver that can open a raster file.
 *
 * This function will try to identify the driver that can open the passed file
 * name by invoking the Identify method of each registered GDALDriver in turn.
 * The first driver that successful identifies the file name will be returned.
 * If all drivers fail then NULL is returned.
 *
 * In order to reduce the need for such searches touch the operating system
 * file system machinery, it is possible to give an optional list of files.
 * This is the list of all files at the same level in the file system as the
 * target file, including the target file. The filenames will not include any
 * path components, are essentially just the output of VSIReadDir() on the
 * parent directory. If the target object does not have filesystem semantics
 * then the file list should be NULL.
 *
 * @param pszFilename the name of the file to access.  In the case of
 * exotic drivers this may not refer to a physical file, but instead contain
 * information for the driver on how to access a dataset.
 *
 * @param nIdentifyFlags a combination of GDAL_OF_RASTER for raster drivers
 * or GDAL_OF_VECTOR for vector drivers. If none of the value is specified,
 * both kinds are implied.
 *
 * @param papszAllowedDrivers NULL to consider all candidate drivers, or a NULL
 * terminated list of strings with the driver short names that must be considered.
 *
 * @param papszFileList an array of strings, whose last element is the NULL
 * pointer.  These strings are filenames that are auxiliary to the main
 * filename. The passed value may be NULL.
 *
 * @return A GDALDriverH handle or NULL on failure.  For C++ applications
 * this handle can be cast to a GDALDriver *.
 *
 * @since GDAL 2.2
 */

GDALDriverH CPL_STDCALL
GDALIdentifyDriverEx( const char* pszFilename,
                      unsigned int nIdentifyFlags,
                      const char* const* papszAllowedDrivers,
                      const char* const* papszFileList )
{
    GDALDriverManager  *poDM = GetGDALDriverManager();
    CPLAssert( nullptr != poDM );
    GDALOpenInfo oOpenInfo( pszFilename, GA_ReadOnly, papszFileList );

    CPLErrorReset();

    const int nDriverCount = poDM->GetDriverCount();

    // First pass: only use drivers that have a pfnIdentify implementation.
    for( int iDriver = 0; iDriver < nDriverCount; ++iDriver )
    {
        GDALDriver* poDriver = poDM->GetDriver( iDriver );
        if (papszAllowedDrivers != nullptr &&
            CSLFindString(papszAllowedDrivers,
                            GDALGetDriverShortName(poDriver)) == -1)
        {
            continue;
        }

        VALIDATE_POINTER1( poDriver, "GDALIdentifyDriver", nullptr );

        if( poDriver->pfnIdentify == nullptr && poDriver->pfnIdentifyEx == nullptr)
        {
            continue;
        }

        if (papszAllowedDrivers != nullptr &&
            CSLFindString(papszAllowedDrivers,
                          GDALGetDriverShortName(poDriver)) == -1)
            continue;
        if( (nIdentifyFlags & GDAL_OF_RASTER) != 0 &&
            (nIdentifyFlags & GDAL_OF_VECTOR) == 0 &&
            poDriver->GetMetadataItem(GDAL_DCAP_RASTER) == nullptr )
            continue;
        if( (nIdentifyFlags & GDAL_OF_VECTOR) != 0 &&
            (nIdentifyFlags & GDAL_OF_RASTER) == 0 &&
            poDriver->GetMetadataItem(GDAL_DCAP_VECTOR) == nullptr )
            continue;

        if( poDriver->pfnIdentifyEx )
        {
            if( poDriver->pfnIdentifyEx( poDriver, &oOpenInfo ) > 0 )
                return poDriver;
        }
        else
        {
            if( poDriver->pfnIdentify( &oOpenInfo ) > 0 )
                return poDriver;
        }
    }

    // Second pass: slow method.
    for( int iDriver = 0; iDriver < nDriverCount; ++iDriver )
    {
        GDALDriver* poDriver = poDM->GetDriver( iDriver );
        if (papszAllowedDrivers != nullptr &&
            CSLFindString(papszAllowedDrivers,
                            GDALGetDriverShortName(poDriver)) == -1)
        {
                continue;
        }

        VALIDATE_POINTER1( poDriver, "GDALIdentifyDriver", nullptr );

        if( (nIdentifyFlags & GDAL_OF_RASTER) != 0 &&
            (nIdentifyFlags & GDAL_OF_VECTOR) == 0 &&
            poDriver->GetMetadataItem(GDAL_DCAP_RASTER) == nullptr )
            continue;
        if( (nIdentifyFlags & GDAL_OF_VECTOR) != 0 &&
            (nIdentifyFlags & GDAL_OF_RASTER) == 0 &&
            poDriver->GetMetadataItem(GDAL_DCAP_VECTOR) == nullptr )
            continue;

        if( poDriver->pfnIdentifyEx != nullptr )
        {
            if( poDriver->pfnIdentifyEx( poDriver, &oOpenInfo ) == 0 )
                continue;
        }
        else if( poDriver->pfnIdentify != nullptr )
        {
            if( poDriver->pfnIdentify( &oOpenInfo ) == 0 )
                continue;
        }

        GDALDataset     *poDS;
        if( poDriver->pfnOpen != nullptr )
        {
            poDS = poDriver->pfnOpen( &oOpenInfo );
            if( poDS != nullptr )
            {
                delete poDS;
                return GDALDriver::ToHandle(poDriver);
            }

            if( CPLGetLastErrorNo() != 0 )
                return nullptr;
        }
        else if( poDriver->pfnOpenWithDriverArg != nullptr )
        {
            poDS = poDriver->pfnOpenWithDriverArg( poDriver, &oOpenInfo );
            if( poDS != nullptr )
            {
                delete poDS;
                return GDALDriver::ToHandle(poDriver);
            }

            if( CPLGetLastErrorNo() != 0 )
                return nullptr;
        }
    }

    return nullptr;
}

/************************************************************************/
/*                          SetMetadataItem()                           */
/************************************************************************/

CPLErr GDALDriver::SetMetadataItem( const char *pszName,
                                    const char *pszValue,
                                    const char *pszDomain )

{
    if( pszDomain == nullptr || pszDomain[0] == '\0' )
    {
        /* Automatically sets GDAL_DMD_EXTENSIONS from GDAL_DMD_EXTENSION */
        if( EQUAL(pszName, GDAL_DMD_EXTENSION) &&
            GDALMajorObject::GetMetadataItem(GDAL_DMD_EXTENSIONS) == nullptr )
        {
            GDALMajorObject::SetMetadataItem(GDAL_DMD_EXTENSIONS, pszValue);
        }
    }
    return GDALMajorObject::SetMetadataItem(pszName, pszValue, pszDomain);
}
