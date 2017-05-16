/******************************************************************************
 *
 * Project:  GDAL Core
 * Purpose:  Implementation of GDALDriver class (and C wrappers)
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1998, 2000, Frank Warmerdam
 * Copyright (c) 2007-2014, Even Rouault <even dot rouault at mines-paris dot org>
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

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_minixml.h"
#include "cpl_progress.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "ogr_core.h"
#include "ogrsf_frmts.h"

CPL_CVSID("$Id$");

CPL_C_START
// TODO(schwehr): Why is this not in a header?
const char* GDALClientDatasetGetFilename( const char* pszFilename );
CPL_C_END

/************************************************************************/
/*                             GDALDriver()                             */
/************************************************************************/

GDALDriver::GDALDriver() :
    pfnOpen(NULL),
    pfnCreate(NULL),
    pfnDelete(NULL),
    pfnCreateCopy(NULL),
    pDriverData(NULL),
    pfnUnloadDriver(NULL),
    pfnIdentify(NULL),
    pfnRename(NULL),
    pfnCopyFiles(NULL),
    pfnOpenWithDriverArg(NULL),
    pfnCreateVectorOnly(NULL),
    pfnDeleteDataSource(NULL)
{}

/************************************************************************/
/*                            ~GDALDriver()                             */
/************************************************************************/

GDALDriver::~GDALDriver()

{
    if( pfnUnloadDriver != NULL )
        pfnUnloadDriver( this );
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
    if( hDriver != NULL )
        delete static_cast<GDALDriver *>(hDriver);
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
 * In some situations, the new dataset can be created in another process through
 * the \ref gdal_api_proxy mechanism.
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
    if( pfnCreate == NULL && pfnCreateVectorOnly == NULL )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "GDALDriver::Create() ... no create method implemented"
                  " for this format." );

        return NULL;
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
        return NULL;
    }

    if( GetMetadataItem(GDAL_DCAP_RASTER) != NULL &&
        GetMetadataItem(GDAL_DCAP_VECTOR) == NULL &&
        (nXSize < 1 || nYSize < 1) )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Attempt to create %dx%d dataset is illegal,"
                  "sizes must be larger than zero.",
                  nXSize, nYSize );
        return NULL;
    }

    const char* pszClientFilename = GDALClientDatasetGetFilename(pszFilename);
    if( pszClientFilename != NULL && !EQUAL(GetDescription(), "MEM") &&
        !EQUAL(GetDescription(), "VRT")  )
    {
        GDALDriver* poAPIPROXYDriver = GDALGetAPIPROXYDriver();
        if( poAPIPROXYDriver != this )
        {
            if( poAPIPROXYDriver == NULL ||
                poAPIPROXYDriver->pfnCreate == NULL )
                return NULL;
            char** papszOptionsDup = CSLDuplicate(papszOptions);
            papszOptionsDup = CSLAddNameValue(papszOptionsDup, "SERVER_DRIVER",
                                              GetDescription());
            GDALDataset* poDstDS = poAPIPROXYDriver->pfnCreate(
                pszClientFilename, nXSize, nYSize, nBands,
                eType, papszOptionsDup );

            CSLDestroy(papszOptionsDup);

            if( poDstDS != NULL )
            {
                if( poDstDS->GetDescription() == NULL
                    || strlen(poDstDS->GetDescription()) == 0 )
                    poDstDS->SetDescription( pszFilename );

                if( poDstDS->poDriver == NULL )
                    poDstDS->poDriver = poAPIPROXYDriver;
            }

            if( poDstDS != NULL || CPLGetLastErrorNo() != CPLE_NotSupported )
                return poDstDS;
        }
    }

/* -------------------------------------------------------------------- */
/*      Make sure we cleanup if there is an existing dataset of this    */
/*      name.  But even if that seems to fail we will continue since    */
/*      it might just be a corrupt file or something.                   */
/* -------------------------------------------------------------------- */
    if( !CPLFetchBool(papszOptions, "APPEND_SUBDATASET", false) )
        QuietDelete( pszFilename );

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

    GDALDataset *poDS = NULL;
    if( pfnCreate != NULL )
    {
        poDS = pfnCreate( pszFilename, nXSize, nYSize, nBands, eType,
                          papszOptions );
    }
    else if( nBands < 1 )
    {
        poDS = pfnCreateVectorOnly( this, pszFilename, papszOptions );
    }

    if( poDS != NULL )
    {
        if( poDS->GetDescription() == NULL
            || strlen(poDS->GetDescription()) == 0 )
            poDS->SetDescription( pszFilename );

        if( poDS->poDriver == NULL )
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
            char ** papszOptions )

{
    VALIDATE_POINTER1( hDriver, "GDALCreate", NULL );

    return
        static_cast<GDALDriver *>(hDriver)->Create( pszFilename,
                                                    nXSize, nYSize, nBands,
                                                    eBandType, papszOptions );
}

/************************************************************************/
/*                          DefaultCopyMasks()                          */
/************************************************************************/

//! @cond Doxygen_Suppress
CPLErr GDALDriver::DefaultCopyMasks( GDALDataset *poSrcDS,
                                     GDALDataset *poDstDS,
                                     int bStrict )

{
    int nBands = poSrcDS->GetRasterCount();
    if (nBands == 0)
        return CE_None;

/* -------------------------------------------------------------------- */
/*      Try to copy mask if it seems appropriate.                       */
/* -------------------------------------------------------------------- */
    const char* papszOptions[2] = { "COMPRESSED=YES", NULL };
    CPLErr eErr = CE_None;

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
            if (poDstBand != NULL)
            {
                eErr = poDstBand->CreateMaskBand( nMaskFlags );
                if( eErr == CE_None )
                {
                    eErr = GDALRasterBandCopyWholeRaster(
                        poSrcBand->GetMaskBand(),
                        poDstBand->GetMaskBand(),
                        papszOptions,
                        GDALDummyProgress, NULL);
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
                GDALDummyProgress, NULL);
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
    if( pfnProgress == NULL )
        pfnProgress = GDALDummyProgress;

    CPLErrorReset();

/* -------------------------------------------------------------------- */
/*      Validate that we can create the output as requested.            */
/* -------------------------------------------------------------------- */
    const int nXSize = poSrcDS->GetRasterXSize();
    const int nYSize = poSrcDS->GetRasterYSize();
    const int nBands = poSrcDS->GetRasterCount();

    CPLDebug( "GDAL", "Using default GDALDriver::CreateCopy implementation." );

    const int nLayerCount = poSrcDS->GetLayerCount();
    if( nBands == 0 && nLayerCount == 0 &&
        GetMetadataItem(GDAL_DCAP_VECTOR) == NULL )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "GDALDriver::DefaultCreateCopy does not support zero band" );
        return NULL;
    }
    if( poSrcDS->GetDriver() != NULL &&
        poSrcDS->GetDriver()->GetMetadataItem(GDAL_DCAP_RASTER) != NULL &&
        poSrcDS->GetDriver()->GetMetadataItem(GDAL_DCAP_VECTOR) == NULL &&
        GetMetadataItem(GDAL_DCAP_RASTER) == NULL &&
        GetMetadataItem(GDAL_DCAP_VECTOR) != NULL )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Source driver is raster-only whereas output driver is "
                  "vector-only" );
        return NULL;
    }
    else if( poSrcDS->GetDriver() != NULL &&
        poSrcDS->GetDriver()->GetMetadataItem(GDAL_DCAP_RASTER) == NULL &&
        poSrcDS->GetDriver()->GetMetadataItem(GDAL_DCAP_VECTOR) != NULL &&
        GetMetadataItem(GDAL_DCAP_RASTER) != NULL &&
        GetMetadataItem(GDAL_DCAP_VECTOR) == NULL )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Source driver is vector-only whereas output driver is "
                  "raster-only" );
        return NULL;
    }

    if( !pfnProgress( 0.0, NULL, pProgressData ) )
    {
        CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
        return NULL;
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
        NULL };

    for( int iOptItem = 0;
         nBands > 0 && apszOptItems[iOptItem] != NULL;
         iOptItem += 2 )
    {
        // does the source have this metadata item on the first band?
        const char *pszValue =
            poSrcDS->GetRasterBand(1)->GetMetadataItem(
                apszOptItems[iOptItem], apszOptItems[iOptItem+1] );

        if( pszValue == NULL )
            continue;

        // Do not override provided value.
        if( CSLFetchNameValue( papszCreateOptions, pszValue ) != NULL )
            continue;

        // Does this appear to be a supported creation option on this driver?
        const char *pszOptionList =
            GetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST );

        if( pszOptionList == NULL
            || strstr(pszOptionList,apszOptItems[iOptItem]) == NULL )
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

    if( poDstDS == NULL )
        return NULL;

    int nDstBands = poDstDS->GetRasterCount();
    CPLErr eErr = CE_None;
    if( nDstBands != nBands )
    {
        if( GetMetadataItem(GDAL_DCAP_RASTER) != NULL )
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
        && poSrcDS->GetProjectionRef() != NULL
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
    if( poSrcDS->GetMetadata() != NULL )
        poDstDS->SetMetadata( poSrcDS->GetMetadata() );

/* -------------------------------------------------------------------- */
/*      Copy transportable special domain metadata (RPCs).  It would    */
/*      be nice to copy geolocation, but it is pretty fragile.          */
/* -------------------------------------------------------------------- */
    char **papszMD = poSrcDS->GetMetadata( "RPC" );
    if( papszMD )
        poDstDS->SetMetadata( papszMD, "RPC" );

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
        if( poCT != NULL )
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
        if (NULL != papszCatNames)
            poDstBand->SetCategoryNames( papszCatNames );

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
                                           NULL, pfnProgress, pProgressData );

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

                if( poLayer == NULL )
                    continue;

                poDstDS->CopyLayer( poLayer, poLayer->GetName(), NULL );
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Try to cleanup the output dataset if the translation failed.    */
/* -------------------------------------------------------------------- */
    if( eErr != CE_None )
    {
        delete poDstDS;
        Delete( pszFilename );
        return NULL;
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
 * In some situations, the new dataset can be created in another process through
 * the \ref gdal_api_proxy mechanism.
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
    if( pfnProgress == NULL )
        pfnProgress = GDALDummyProgress;

    const char* pszClientFilename = GDALClientDatasetGetFilename(pszFilename);
    if( pszClientFilename != NULL && !EQUAL(GetDescription(), "MEM") &&
        !EQUAL(GetDescription(), "VRT") )
    {
        GDALDriver* poAPIPROXYDriver = GDALGetAPIPROXYDriver();
        if( poAPIPROXYDriver != this )
        {
            if( poAPIPROXYDriver->pfnCreateCopy == NULL )
                return NULL;
            char** papszOptionsDup = CSLDuplicate(papszOptions);
            papszOptionsDup = CSLAddNameValue(papszOptionsDup, "SERVER_DRIVER",
                                              GetDescription());
            GDALDataset* const poDstDS = poAPIPROXYDriver->pfnCreateCopy(
                pszClientFilename, poSrcDS, bStrict, papszOptionsDup,
                pfnProgress, pProgressData);
            if( poDstDS != NULL )
            {
                if( poDstDS->GetDescription() == NULL
                    || strlen(poDstDS->GetDescription()) == 0 )
                    poDstDS->SetDescription( pszFilename );

                if( poDstDS->poDriver == NULL )
                    poDstDS->poDriver = poAPIPROXYDriver;
            }

            CSLDestroy(papszOptionsDup);
            if( poDstDS != NULL || CPLGetLastErrorNo() != CPLE_NotSupported )
                return poDstDS;
        }
    }

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
        QuietDelete( pszFilename );

    char** papszOptionsToDelete = NULL;
    int iIdxQuietDeleteOnCreateCopy =
        CSLPartialFindString(papszOptions, "QUIET_DELETE_ON_CREATE_COPY=");
    if( iIdxQuietDeleteOnCreateCopy >= 0 )
    {
        if( papszOptionsToDelete == NULL )
            papszOptionsToDelete = CSLDuplicate(papszOptions);
        papszOptions =
            CSLRemoveStrings(papszOptionsToDelete, iIdxQuietDeleteOnCreateCopy,
                             1, NULL);
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
        if( papszOptionsToDelete == NULL )
            papszOptionsToDelete = CSLDuplicate(papszOptions);
        papszOptions =
            CSLRemoveStrings(papszOptionsToDelete, iIdxInternalDataset,
                             1, NULL);
        papszOptionsToDelete = papszOptions;
    }

/* -------------------------------------------------------------------- */
/*      Validate creation options.                                      */
/* -------------------------------------------------------------------- */
    if( CPLTestBool(
            CPLGetConfigOption("GDAL_VALIDATE_CREATION_OPTIONS", "YES") ) )
        GDALValidateCreationOptions( this, papszOptions );

/* -------------------------------------------------------------------- */
/*      If the format provides a CreateCopy() method use that,          */
/*      otherwise fallback to the internal implementation using the     */
/*      Create() method.                                                */
/* -------------------------------------------------------------------- */
    GDALDataset *poDstDS = NULL;
    if( pfnCreateCopy != NULL &&
        !CPLTestBool(CPLGetConfigOption("GDAL_DEFAULT_CREATE_COPY", "NO")) )
    {
        poDstDS = pfnCreateCopy( pszFilename, poSrcDS, bStrict, papszOptions,
                                 pfnProgress, pProgressData );
        if( poDstDS != NULL )
        {
            if( poDstDS->GetDescription() == NULL
                || strlen(poDstDS->GetDescription()) == 0 )
                poDstDS->SetDescription( pszFilename );

            if( poDstDS->poDriver == NULL )
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
                                         int bStrict, char ** papszOptions,
                                         GDALProgressFunc pfnProgress,
                                         void * pProgressData )

{
    VALIDATE_POINTER1( hDriver, "GDALCreateCopy", NULL );
    VALIDATE_POINTER1( hSrcDS, "GDALCreateCopy", NULL );

    return static_cast<GDALDriver *>(hDriver)->
        CreateCopy( pszFilename, static_cast<GDALDataset *>(hSrcDS),
                    bStrict, papszOptions, pfnProgress, pProgressData );
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
 * @return CE_None if the dataset does not exist, or is deleted without issues.
 */

CPLErr GDALDriver::QuietDelete( const char *pszName )

{
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
        static_cast<GDALDriver *>( GDALIdentifyDriver( pszName, NULL ) );
    CPLPopErrorHandler();

    if( poDriver == NULL )
        return CE_None;

    CPLDebug( "GDAL", "QuietDelete(%s) invoking Delete()", pszName );

    const bool bQuiet =
        !bExists && poDriver->pfnDelete == NULL &&
        poDriver->pfnDeleteDataSource == NULL;
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
 * database objects, or whatever is appropriate.  The default behaviour when
 * no driver specific behaviour is provided is to attempt to delete the
 * passed name as a single file.
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
    if( pfnDelete != NULL )
        return pfnDelete( pszFilename );
    else if( pfnDeleteDataSource != NULL )
        return pfnDeleteDataSource( this, pszFilename );

/* -------------------------------------------------------------------- */
/*      Collect file list.                                              */
/* -------------------------------------------------------------------- */
    GDALDatasetH hDS = static_cast<GDALDataset *>(
        GDALOpenEx(pszFilename, 0, NULL, NULL, NULL) );

    if( hDS == NULL )
    {
        if( CPLGetLastErrorNo() == 0 )
            CPLError( CE_Failure, CPLE_OpenFailed,
                      "Unable to open %s to obtain file list.", pszFilename );

        return CE_Failure;
    }

    char **papszFileList = GDALGetFileList( hDS );

    GDALClose( hDS );
    hDS = NULL;

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
    for( int i = 0; papszFileList[i] != NULL; ++i )
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
    if( hDriver == NULL )
        hDriver = GDALIdentifyDriver( pszFilename, NULL );

    if( hDriver == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "No identifiable driver for %s.",
                  pszFilename );
        return CE_Failure;
    }

    return static_cast<GDALDriver *>(hDriver)->Delete( pszFilename );
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
    GDALDatasetH hDS = static_cast<GDALDataset *>(
        GDALOpen(pszOldName, GA_ReadOnly) );

    if( hDS == NULL )
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

    if( papszNewFileList == NULL )
        return CE_Failure;

    for( int i = 0; papszFileList[i] != NULL; ++i )
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
    if( pfnRename != NULL )
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
    if( hDriver == NULL )
        hDriver = GDALIdentifyDriver( pszOldName, NULL );

    if( hDriver == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "No identifiable driver for %s.",
                  pszOldName );
        return CE_Failure;
    }

    return static_cast<GDALDriver *>(hDriver)->Rename( pszNewName, pszOldName );
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
    GDALDatasetH hDS = static_cast<GDALDataset *>(
        GDALOpen(pszOldName,GA_ReadOnly) );

    if( hDS == NULL )
    {
        if( CPLGetLastErrorNo() == 0 )
            CPLError( CE_Failure, CPLE_OpenFailed,
                      "Unable to open %s to obtain file list.", pszOldName );

        return CE_Failure;
    }

    char **papszFileList = GDALGetFileList( hDS );

    GDALClose( hDS );
    hDS = NULL;

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

    if( papszNewFileList == NULL )
        return CE_Failure;

    for( int i = 0; papszFileList[i] != NULL; ++i )
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
    if( pfnCopyFiles != NULL )
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
    if( hDriver == NULL )
        hDriver = GDALIdentifyDriver( pszOldName, NULL );

    if( hDriver == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "No identifiable driver for %s.",
                  pszOldName );
        return CE_Failure;
    }

    return static_cast<GDALDriver *>(hDriver)->
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
    VALIDATE_POINTER1( hDriver, "GDALGetDriverShortName", NULL );

    return static_cast<GDALDriver *>(hDriver)->GetDescription();
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
    VALIDATE_POINTER1( hDriver, "GDALGetDriverLongName", NULL );

    const char *pszLongName =
        static_cast<GDALDriver *>(hDriver)->
            GetMetadataItem( GDAL_DMD_LONGNAME );

    if( pszLongName == NULL )
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
    VALIDATE_POINTER1( hDriver, "GDALGetDriverHelpTopic", NULL );

    return static_cast<GDALDriver *>(hDriver)->
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
    VALIDATE_POINTER1( hDriver, "GDALGetDriverCreationOptionList", NULL );

    const char *pszOptionList =
        static_cast<GDALDriver *>(hDriver)->
            GetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST );

    if( pszOptionList == NULL )
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
                                             char** papszCreationOptions )
{
    VALIDATE_POINTER1( hDriver, "GDALValidateCreationOptions", FALSE );
    const char *pszOptionList =
        static_cast<GDALDriver *>(hDriver)->GetMetadataItem(
            GDAL_DMD_CREATIONOPTIONLIST );
    CPLString osDriver;
    osDriver.Printf("driver %s",
                    static_cast<GDALDriver *>(hDriver)->GetDescription());
    char** papszOptionsToValidate = papszCreationOptions;
    char** papszOptionsToFree = NULL;
    if( CSLFetchNameValue( papszCreationOptions, "APPEND_SUBDATASET") )
    {
        papszOptionsToFree =
            CSLSetNameValue(CSLDuplicate(papszCreationOptions),
                            "APPEND_SUBDATASET", NULL);
        papszOptionsToValidate = papszOptionsToFree;
    }
    const bool bRet = CPL_TO_BOOL(
        GDALValidateOptions(
            pszOptionList,
            const_cast<const char* const*>(papszOptionsToValidate),
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
    const char *pszOptionList = static_cast<GDALDriver *>(hDriver)->
        GetMetadataItem( GDAL_DMD_OPENOPTIONLIST );
    CPLString osDriver;
    osDriver.Printf("driver %s",
                    static_cast<GDALDriver *>(hDriver)->GetDescription());
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
    if( papszOptionsToValidate == NULL || *papszOptionsToValidate == NULL)
        return TRUE;
    if( pszOptionList == NULL )
        return TRUE;

    CPLXMLNode* psNode = CPLParseXMLString(pszOptionList);
    if (psNode == NULL)
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "Could not parse %s list of %s. Assuming options are valid.",
                 pszErrorMessageOptionType, pszErrorMessageContainerName);
        return TRUE;
    }

    bool bRet = true;
    while(*papszOptionsToValidate)
    {
        char* pszKey = NULL;
        const char* pszValue = CPLParseNameValue(*papszOptionsToValidate, &pszKey);
        if (pszKey == NULL)
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
        if (psChildNode == NULL)
        {
            if( bWarnIfMissingKey &&
                (!EQUAL(pszErrorMessageOptionType, "open option") ||
                 CPLFetchBool((char**)papszOptionsToValidate,
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

        const char* pszType = CPLGetXMLValue(psChildNode, "type", NULL);
        const char* pszMin = CPLGetXMLValue(psChildNode, "min", NULL);
        const char* pszMax = CPLGetXMLValue(psChildNode, "max", NULL);
        if (pszType != NULL)
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
                char* endPtr = NULL;
                double dfVal = CPLStrtod(pszValue, &endPtr);
                if ( !(endPtr == NULL || *endPtr == '\0') )
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
                const char* pszMaxSize = CPLGetXMLValue(psChildNode, "maxsize", NULL);
                if (pszMaxSize != NULL)
                {
                    if ((int)strlen(pszValue) > atoi(pszMaxSize))
                    {
                        CPLError(CE_Warning, CPLE_NotSupported,
                             "'%s' is of size %d, whereas maximum size for %s %s is %d.",
                             pszValue, (int)strlen(pszValue), pszKey,
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
                    char **papszFileList )

{
    return GDALIdentifyDriverEx( pszFilename, 0, NULL, papszFileList );
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
    CPLAssert( NULL != poDM );
    GDALOpenInfo oOpenInfo( pszFilename, GA_ReadOnly, (char**)papszFileList );

    CPLErrorReset();

    const int nDriverCount = poDM->GetDriverCount();

    // First pass: only use drivers that have a pfnIdentify implementation.
    for( int iDriver = -1; iDriver < nDriverCount; ++iDriver )
    {
        GDALDriver* poDriver;

        if( iDriver < 0 )
            poDriver = GDALGetAPIPROXYDriver();
        else
        {
            poDriver = poDM->GetDriver( iDriver );
            if (papszAllowedDrivers != NULL &&
                CSLFindString((char**)papszAllowedDrivers,
                              GDALGetDriverShortName(poDriver)) == -1)
                continue;
        }

        VALIDATE_POINTER1( poDriver, "GDALIdentifyDriver", NULL );

        if( poDriver->pfnIdentify == NULL )
        {
            continue;
        }

        if (papszAllowedDrivers != NULL &&
            CSLFindString((char**)papszAllowedDrivers,
                          GDALGetDriverShortName(poDriver)) == -1)
            continue;
        if( (nIdentifyFlags & GDAL_OF_RASTER) != 0 &&
            (nIdentifyFlags & GDAL_OF_VECTOR) == 0 &&
            poDriver->GetMetadataItem(GDAL_DCAP_RASTER) == NULL )
            continue;
        if( (nIdentifyFlags & GDAL_OF_VECTOR) != 0 &&
            (nIdentifyFlags & GDAL_OF_RASTER) == 0 &&
            poDriver->GetMetadataItem(GDAL_DCAP_VECTOR) == NULL )
            continue;

        if( poDriver->pfnIdentify( &oOpenInfo ) > 0 )
            return poDriver;
    }

    // Second pass: slow method.
    for( int iDriver = -1; iDriver < nDriverCount; ++iDriver )
    {
        GDALDriver* poDriver;

        if( iDriver < 0 )
            poDriver = GDALGetAPIPROXYDriver();
        else
        {
            poDriver = poDM->GetDriver( iDriver );
            if (papszAllowedDrivers != NULL &&
                CSLFindString((char**)papszAllowedDrivers,
                              GDALGetDriverShortName(poDriver)) == -1)
                continue;
        }

        VALIDATE_POINTER1( poDriver, "GDALIdentifyDriver", NULL );

        if( (nIdentifyFlags & GDAL_OF_RASTER) != 0 &&
            (nIdentifyFlags & GDAL_OF_VECTOR) == 0 &&
            poDriver->GetMetadataItem(GDAL_DCAP_RASTER) == NULL )
            continue;
        if( (nIdentifyFlags & GDAL_OF_VECTOR) != 0 &&
            (nIdentifyFlags & GDAL_OF_RASTER) == 0 &&
            poDriver->GetMetadataItem(GDAL_DCAP_VECTOR) == NULL )
            continue;

        if( poDriver->pfnIdentify != NULL )
        {
            if( poDriver->pfnIdentify( &oOpenInfo ) == 0 )
                continue;
        }

        GDALDataset     *poDS;
        if( poDriver->pfnOpen != NULL )
        {
            poDS = poDriver->pfnOpen( &oOpenInfo );
            if( poDS != NULL )
            {
                delete poDS;
                return (GDALDriverH) poDriver;
            }

            if( CPLGetLastErrorNo() != 0 )
                return NULL;
        }
        else if( poDriver->pfnOpenWithDriverArg != NULL )
        {
            poDS = poDriver->pfnOpenWithDriverArg( poDriver, &oOpenInfo );
            if( poDS != NULL )
            {
                delete poDS;
                return (GDALDriverH) poDriver;
            }

            if( CPLGetLastErrorNo() != 0 )
                return NULL;
        }
    }

    return NULL;
}

/************************************************************************/
/*                          SetMetadataItem()                           */
/************************************************************************/

CPLErr GDALDriver::SetMetadataItem( const char *pszName,
                                    const char *pszValue,
                                    const char *pszDomain )

{
    if( pszDomain == NULL || pszDomain[0] == '\0' )
    {
        /* Automatically sets GDAL_DMD_EXTENSIONS from GDAL_DMD_EXTENSION */
        if( EQUAL(pszName, GDAL_DMD_EXTENSION) &&
            GDALMajorObject::GetMetadataItem(GDAL_DMD_EXTENSIONS) == NULL )
        {
            GDALMajorObject::SetMetadataItem(GDAL_DMD_EXTENSIONS, pszValue);
        }
    }
    return GDALMajorObject::SetMetadataItem(pszName, pszValue, pszDomain);
}
