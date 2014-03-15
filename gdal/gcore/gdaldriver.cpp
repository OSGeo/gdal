/******************************************************************************
 * $Id$
 *
 * Project:  GDAL Core
 * Purpose:  Implementation of GDALDriver class (and C wrappers)
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1998, 2000, Frank Warmerdam
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

#include "gdal_priv.h"

CPL_CVSID("$Id$");

CPL_C_START
const char* GDALClientDatasetGetFilename(const char* pszFilename);
CPL_C_END

/************************************************************************/
/*                             GDALDriver()                             */
/************************************************************************/

GDALDriver::GDALDriver()

{
    pfnOpen = NULL;
    pfnCreate = NULL;
    pfnDelete = NULL;
    pfnCreateCopy = NULL;
    pfnUnloadDriver = NULL;
    pDriverData = NULL;
    pfnIdentify = NULL;
    pfnRename = NULL;
    pfnCopyFiles = NULL;
}

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
 * This is roughly equivelent to deleting the driver, but is guaranteed
 * to take place in the GDAL heap.  It is important this that function
 * not be called on a driver that is registered with the GDALDriverManager.
 * 
 * @param hDriver the driver to destroy.
 */

void CPL_STDCALL GDALDestroyDriver( GDALDriverH hDriver )

{
    VALIDATE_POINTER0( hDriver, "GDALDestroyDriver" );

    delete ((GDALDriver *) hDriver);
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
 * That function will try to validate the creation option list passed to the driver
 * with the GDALValidateCreationOptions() method. This check can be disabled
 * by defining the configuration option GDAL_VALIDATE_CREATION_OPTIONS=NO.
 *
 * After you have finished working with the returned dataset, it is <b>required</b>
 * to close it with GDALClose(). This does not only close the file handle, but
 * also ensures that all the data and metadata has been written to the dataset
 * (GDALFlushCache() is not sufficient for that purpose).
 *
 * In some situations, the new dataset can be created in another process through the
 * \ref gdal_api_proxy mechanism.
 *
 * Equivelent of the C function GDALCreate().
 * 
 * @param pszFilename the name of the dataset to create.  UTF-8 encoded.
 * @param nXSize width of created raster in pixels.
 * @param nYSize height of created raster in pixels.
 * @param nBands number of bands.
 * @param eType type of raster.
 * @param papszOptions list of driver specific control parameters.
 *
 * @return NULL on failure, or a new GDALDataset.
 */

GDALDataset * GDALDriver::Create( const char * pszFilename,
                                  int nXSize, int nYSize, int nBands,
                                  GDALDataType eType, char ** papszOptions )

{
    CPLLocaleC  oLocaleForcer;

/* -------------------------------------------------------------------- */
/*      Does this format support creation.                              */
/* -------------------------------------------------------------------- */
    if( pfnCreate == NULL )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "GDALDriver::Create() ... no create method implemented"
                  " for this format.\n" );

        return NULL;
    }
/* -------------------------------------------------------------------- */
/*      Do some rudimentary argument checking.                          */
/* -------------------------------------------------------------------- */
    if (nBands < 0)
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Attempt to create dataset with %d bands is illegal,"
                  "Must be >= 0.",
                  nBands );
        return NULL;
    }

    if( nXSize < 1 || nYSize < 1 )
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
            if( poAPIPROXYDriver == NULL || poAPIPROXYDriver->pfnCreate == NULL )
                return NULL;
            char** papszOptionsDup = CSLDuplicate(papszOptions);
            papszOptionsDup = CSLAddNameValue(papszOptionsDup, "SERVER_DRIVER",
                                               GetDescription());
            GDALDataset* poDstDS = poAPIPROXYDriver->pfnCreate(
                pszClientFilename, nXSize, nYSize, nBands,
                eType, papszOptionsDup);

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
    if( !CSLFetchBoolean(papszOptions, "APPEND_SUBDATASET", FALSE) )
        QuietDelete( pszFilename );

/* -------------------------------------------------------------------- */
/*      Validate creation options.                                      */
/* -------------------------------------------------------------------- */
    if (CSLTestBoolean(CPLGetConfigOption("GDAL_VALIDATE_CREATION_OPTIONS", "YES")))
        GDALValidateCreationOptions( this, papszOptions );

/* -------------------------------------------------------------------- */
/*      Proceed with creation.                                          */
/* -------------------------------------------------------------------- */
    GDALDataset *poDS;

    CPLDebug( "GDAL", "GDALDriver::Create(%s,%s,%d,%d,%d,%s,%p)",
              GetDescription(), pszFilename, nXSize, nYSize, nBands, 
              GDALGetDataTypeName( eType ), 
              papszOptions );
    
    poDS = pfnCreate( pszFilename, nXSize, nYSize, nBands, eType,
                      papszOptions );

    if( poDS != NULL )
    {
        if( poDS->GetDescription() == NULL
            || strlen(poDS->GetDescription()) == 0 )
            poDS->SetDescription( pszFilename );
        
        if( poDS->poDriver == NULL )
            poDS->poDriver = this;
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

    return( ((GDALDriver *) hDriver)->Create( pszFilename,
                                              nXSize, nYSize, nBands,
                                              eBandType, papszOptions ) );
}

/************************************************************************/
/*                          DefaultCopyMasks()                          */
/************************************************************************/

CPLErr GDALDriver::DefaultCopyMasks( GDALDataset *poSrcDS,
                                     GDALDataset *poDstDS,
                                     int bStrict )

{
    CPLErr eErr = CE_None;	

    int nBands = poSrcDS->GetRasterCount();
    if (nBands == 0)
        return CE_None;

    const char* papszOptions[2] = { "COMPRESSED=YES", NULL };

/* -------------------------------------------------------------------- */
/*      Try to copy mask if it seems appropriate.                       */
/* -------------------------------------------------------------------- */
    for( int iBand = 0; 
         eErr == CE_None && iBand < nBands; 
         iBand++ )
    {
        GDALRasterBand *poSrcBand = poSrcDS->GetRasterBand( iBand+1 );

        int nMaskFlags = poSrcBand->GetMaskFlags();
        if( eErr == CE_None
            && !(nMaskFlags & (GMF_ALL_VALID|GMF_PER_DATASET|GMF_ALPHA|GMF_NODATA) ) )
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
                        (char**)papszOptions,
                        GDALDummyProgress, NULL);
                }
                else if( !bStrict )
                    eErr = CE_None;
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Try to copy a per-dataset mask if we have one.                  */
/* -------------------------------------------------------------------- */
    int nMaskFlags = poSrcDS->GetRasterBand(1)->GetMaskFlags();
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
                (char**)papszOptions,
                GDALDummyProgress, NULL);
        }
        else if( !bStrict )
            eErr = CE_None;
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

    if( !pfnProgress( 0.0, NULL, pProgressData ) )
    {
        CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Validate that we can create the output as requested.            */
/* -------------------------------------------------------------------- */
    int          nXSize = poSrcDS->GetRasterXSize();
    int          nYSize = poSrcDS->GetRasterYSize();
    int          nBands = poSrcDS->GetRasterCount();

    CPLDebug( "GDAL", "Using default GDALDriver::CreateCopy implementation." );

    if (nBands == 0)
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "GDALDriver::DefaultCreateCopy does not support zero band" );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Propogate some specific structural metadata as options if it    */
/*      appears to be supported by the target driver and the caller     */
/*      didn't provide values.                                          */
/* -------------------------------------------------------------------- */
    char **papszCreateOptions = CSLDuplicate( papszOptions );
    int  iOptItem;
    static const char *apszOptItems[] = {
        "NBITS", "IMAGE_STRUCTURE",
        "PIXELTYPE", "IMAGE_STRUCTURE", 
        NULL };

    for( iOptItem = 0; apszOptItems[iOptItem] != NULL; iOptItem += 2 )
    {
        // does the source have this metadata item on the first band?
        const char *pszValue = 
            poSrcDS->GetRasterBand(1)->GetMetadataItem( 
                apszOptItems[iOptItem], apszOptItems[iOptItem+1] );

        if( pszValue == NULL )
            continue;

        // do not override provided value.
        if( CSLFetchNameValue( papszCreateOptions, pszValue ) != NULL )
            continue;

        // Does this appear to be a supported creation option on this driver?
        const char *pszOptionList =
            GetMetadataItem( GDAL_DMD_CREATIONDATATYPES );

        if( pszOptionList == NULL 
            || strstr(pszOptionList,apszOptItems[iOptItem]) != NULL )
            continue;

        papszCreateOptions = CSLSetNameValue( papszCreateOptions,
                                              apszOptItems[iOptItem], 
                                              pszValue );
    }
    
/* -------------------------------------------------------------------- */
/*      Create destination dataset.                                     */
/* -------------------------------------------------------------------- */
    GDALDataset  *poDstDS;
    GDALDataType eType;
    CPLErr       eErr = CE_None;

    eType = poSrcDS->GetRasterBand(1)->GetRasterDataType();
    poDstDS = Create( pszFilename, nXSize, nYSize, 
                      nBands, eType, papszCreateOptions );
                      
    CSLDestroy(papszCreateOptions);

    if( poDstDS == NULL )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Try setting the projection and geotransform if it seems         */
/*      suitable.                                                       */
/* -------------------------------------------------------------------- */
    double      adfGeoTransform[6];

    if( eErr == CE_None
        && poSrcDS->GetGeoTransform( adfGeoTransform ) == CE_None 
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

/* -------------------------------------------------------------------- */
/*      Copy metadata.                                                  */
/* -------------------------------------------------------------------- */
    if( poSrcDS->GetMetadata() != NULL )
        poDstDS->SetMetadata( poSrcDS->GetMetadata() );

/* -------------------------------------------------------------------- */
/*      Copy transportable special domain metadata (RPCs).  It would    */
/*      be nice to copy geolocation, but is is pretty fragile.          */
/* -------------------------------------------------------------------- */
    char **papszMD = poSrcDS->GetMetadata( "RPC" );
    if( papszMD )
        poDstDS->SetMetadata( papszMD, "RPC" );

/* -------------------------------------------------------------------- */
/*      Loop copying bands.                                             */
/* -------------------------------------------------------------------- */
    for( int iBand = 0; 
         eErr == CE_None && iBand < nBands; 
         iBand++ )
    {
        GDALRasterBand *poSrcBand = poSrcDS->GetRasterBand( iBand+1 );
        GDALRasterBand *poDstBand = poDstDS->GetRasterBand( iBand+1 );

/* -------------------------------------------------------------------- */
/*      Do we need to copy a colortable.                                */
/* -------------------------------------------------------------------- */
        GDALColorTable *poCT;
        int bSuccess;
        double dfValue;

        poCT = poSrcBand->GetColorTable();
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

        dfValue = poSrcBand->GetOffset( &bSuccess );
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

        char** papszCatNames;
        papszCatNames = poSrcBand->GetCategoryNames();
        if (0 != papszCatNames)
            poDstBand->SetCategoryNames( papszCatNames );

        if( !bStrict )
        {
            CPLPopErrorHandler();
            CPLErrorReset();
        }
        else 
            eErr = CPLGetLastErrorType();
    }

/* -------------------------------------------------------------------- */
/*      Copy image data.                                                */
/* -------------------------------------------------------------------- */
    if( eErr == CE_None )
        eErr = GDALDatasetCopyWholeRaster( (GDALDatasetH) poSrcDS, 
                                           (GDALDatasetH) poDstDS, 
                                           NULL, pfnProgress, pProgressData );

/* -------------------------------------------------------------------- */
/*      Should we copy some masks over?                                 */
/* -------------------------------------------------------------------- */
    if( eErr == CE_None )
        eErr = DefaultCopyMasks( poSrcDS, poDstDS, eErr );

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
        CPLErrorReset();

    return poDstDS;
}

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
 *
 * It is intended that CreateCopy() will often be used with a source dataset
 * which is a virtual dataset allowing configuration of band types, and
 * other information without actually duplicating raster data (see the VRT driver).
 * This is what is done by the gdal_translate utility for example.
 *
 * That function will try to validate the creation option list passed to the driver
 * with the GDALValidateCreationOptions() method. This check can be disabled
 * by defining the configuration option GDAL_VALIDATE_CREATION_OPTIONS=NO.
 *
 * After you have finished working with the returned dataset, it is <b>required</b>
 * to close it with GDALClose(). This does not only close the file handle, but
 * also ensures that all the data and metadata has been written to the dataset
 * (GDALFlushCache() is not sufficient for that purpose).
 *
 * In some situations, the new dataset can be created in another process through the
 * \ref gdal_api_proxy mechanism.
 *
 * @param pszFilename the name for the new dataset.  UTF-8 encoded.
 * @param poSrcDS the dataset being duplicated. 
 * @param bStrict TRUE if the copy must be strictly equivelent, or more
 * normally FALSE indicating that the copy may adapt as needed for the 
 * output format. 
 * @param papszOptions additional format dependent options controlling 
 * creation of the output file. 
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
    CPLLocaleC  oLocaleForcer;

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
            GDALDataset* poDstDS = poAPIPROXYDriver->pfnCreateCopy(
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
    if( !CSLFetchBoolean(papszOptions, "APPEND_SUBDATASET", FALSE) &&
        CSLFetchBoolean(papszOptions, "QUIET_DELETE_ON_CREATE_COPY", TRUE) )
        QuietDelete( pszFilename );

    int iIdxQuietDeleteOnCreateCopy = 
        CSLPartialFindString(papszOptions, "QUIET_DELETE_ON_CREATE_COPY=");
    char** papszOptionsToDelete = NULL;
    if( iIdxQuietDeleteOnCreateCopy >= 0 )
    {
        papszOptions = CSLRemoveStrings(CSLDuplicate(papszOptions), iIdxQuietDeleteOnCreateCopy, 1, NULL);
        papszOptionsToDelete = papszOptions;
    }
    
/* -------------------------------------------------------------------- */
/*      Validate creation options.                                      */
/* -------------------------------------------------------------------- */
    if (CSLTestBoolean(CPLGetConfigOption("GDAL_VALIDATE_CREATION_OPTIONS", "YES")))
        GDALValidateCreationOptions( this, papszOptions);

/* -------------------------------------------------------------------- */
/*      If the format provides a CreateCopy() method use that,          */
/*      otherwise fallback to the internal implementation using the     */
/*      Create() method.                                                */
/* -------------------------------------------------------------------- */
    GDALDataset *poDstDS;
    if( pfnCreateCopy != NULL && !CSLTestBoolean(CPLGetConfigOption("GDAL_DEFAULT_CREATE_COPY", "NO")) )
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
        }
    }
    else
        poDstDS = DefaultCreateCopy( pszFilename, poSrcDS, bStrict, 
                                  papszOptions, pfnProgress, pProgressData );

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
    
    return (GDALDatasetH) ((GDALDriver *) hDriver)->
        CreateCopy( pszFilename, (GDALDataset *) hSrcDS, bStrict, papszOptions,
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
 * @return CE_None if the dataset does not exist, or is deleted without issues.
 */

CPLErr GDALDriver::QuietDelete( const char *pszName )

{
    GDALDriver *poDriver = (GDALDriver*) GDALIdentifyDriver( pszName, NULL );

    if( poDriver == NULL )
        return CE_None;

    CPLDebug( "GDAL", "QuietDelete(%s) invoking Delete()", pszName );

    return poDriver->Delete( pszName );
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
 * Equivelent of the C function GDALDeleteDataset().
 *
 * @param pszFilename name of dataset to delete.
 *
 * @return CE_None on success, or CE_Failure if the operation fails.
 */

CPLErr GDALDriver::Delete( const char * pszFilename )

{
    if( pfnDelete != NULL )
        return pfnDelete( pszFilename );

/* -------------------------------------------------------------------- */
/*      Collect file list.                                              */
/* -------------------------------------------------------------------- */
    GDALDatasetH hDS = (GDALDataset *) GDALOpen(pszFilename,GA_ReadOnly);
        
    if( hDS == NULL )
    {
        if( CPLGetLastErrorNo() == 0 )
            CPLError( CE_Failure, CPLE_OpenFailed, 
                      "Unable to open %s to obtain file list.", pszFilename );

        return CE_Failure;
    }

    char **papszFileList = GDALGetFileList( hDS );
        
    GDALClose( hDS );

    if( CSLCount( papszFileList ) == 0 )
    {
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "Unable to determine files associated with %s,\n"
                  "delete fails.", pszFilename );

        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Delete all files.                                               */
/* -------------------------------------------------------------------- */
    int i;

    for( i = 0; papszFileList[i] != NULL; i++ )
    {
        if( VSIUnlink( papszFileList[i] ) != 0 )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Deleting %s failed:\n%s",
                      papszFileList[i],
                      VSIStrerror(errno) );
            CSLDestroy( papszFileList );
            return CE_Failure;
        }
    }

    CSLDestroy( papszFileList );

    return CE_None;
}

/************************************************************************/
/*                         GDALDeleteDataset()                          */
/************************************************************************/

/**
 * \brief Delete named dataset.
 *
 * @see GDALDriver::Delete()
 */

CPLErr CPL_STDCALL GDALDeleteDataset( GDALDriverH hDriver, const char * pszFilename )

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

    return ((GDALDriver *) hDriver)->Delete( pszFilename );
}

/************************************************************************/
/*                           DefaultRename()                            */
/*                                                                      */
/*      The generic implementation based on the file list used when     */
/*      there is no format specific implementation.                     */
/************************************************************************/

CPLErr GDALDriver::DefaultRename( const char * pszNewName, 
                                  const char *pszOldName )

{
/* -------------------------------------------------------------------- */
/*      Collect file list.                                              */
/* -------------------------------------------------------------------- */
    GDALDatasetH hDS = (GDALDataset *) GDALOpen(pszOldName,GA_ReadOnly);
        
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
    int i;
    char **papszNewFileList = 
        CPLCorrespondingPaths( pszOldName, pszNewName, papszFileList );

    if( papszNewFileList == NULL )
        return CE_Failure;

    for( i = 0; papszFileList[i] != NULL; i++ )
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
 * Equivelent of the C function GDALRenameDataset().
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
    else
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

    return ((GDALDriver *) hDriver)->Rename( pszNewName, pszOldName );
}

/************************************************************************/
/*                          DefaultCopyFiles()                          */
/*                                                                      */
/*      The default implementation based on file lists used when        */
/*      there is no format specific implementation.                     */
/************************************************************************/

CPLErr GDALDriver::DefaultCopyFiles( const char * pszNewName, 
                                     const char *pszOldName )

{
/* -------------------------------------------------------------------- */
/*      Collect file list.                                              */
/* -------------------------------------------------------------------- */
    GDALDatasetH hDS = (GDALDataset *) GDALOpen(pszOldName,GA_ReadOnly);
        
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
    int i;
    char **papszNewFileList = 
        CPLCorrespondingPaths( pszOldName, pszNewName, papszFileList );

    if( papszNewFileList == NULL )
        return CE_Failure;

    for( i = 0; papszFileList[i] != NULL; i++ )
    {
        if( CPLCopyFile( papszNewFileList[i], papszFileList[i] ) != 0 )
        {
            eErr = CE_Failure;
            // Try to put the ones we moved back. 
            for( --i; i >= 0; i-- )
                VSIUnlink( papszNewFileList[i] );
            break;
        }
    }

    CSLDestroy( papszNewFileList );
    CSLDestroy( papszFileList );

    return eErr;
}

/************************************************************************/
/*                             CopyFiles()                              */
/************************************************************************/

/**
 * \brief Copy the files of a dataset.
 *
 * Copy all the files associated with a dataset.
 *
 * Equivelent of the C function GDALCopyDatasetFiles().
 *
 * @param pszNewName new name for the dataset.
 * @param pszOldName old name for the dataset.
 *
 * @return CE_None on success, or CE_Failure if the operation fails.
 */

CPLErr GDALDriver::CopyFiles( const char * pszNewName, const char *pszOldName )

{
    if( pfnCopyFiles != NULL )
        return pfnCopyFiles( pszNewName, pszOldName );
    else
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

    return ((GDALDriver *) hDriver)->CopyFiles( pszNewName, pszOldName );
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

    return ((GDALDriver *) hDriver)->GetDescription();
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
        ((GDALDriver *) hDriver)->GetMetadataItem( GDAL_DMD_LONGNAME );

    if( pszLongName == NULL )
        return "";
    else
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

    return ((GDALDriver *) hDriver)->GetMetadataItem( GDAL_DMD_HELPTOPIC );
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
        ((GDALDriver *) hDriver)->GetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST );

    if( pszOptionList == NULL )
        return "";
    else
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
 * a (non fatal) warning will be emited and FALSE will be returned.
 *
 * @param hDriver the handle of the driver with whom the lists of creation option
 *                must be validated
 * @param papszCreationOptions the list of creation options. An array of strings,
 *                             whose last element is a NULL pointer
 * @return TRUE if the list of creation options is compatible with the Create()
 *         and CreateCopy() method of the driver, FALSE otherwise.
 */

int CPL_STDCALL GDALValidateCreationOptions( GDALDriverH hDriver,
                                             char** papszCreationOptions)
{
    int bRet = TRUE;
    VALIDATE_POINTER1( hDriver, "GDALValidateCreationOptions", FALSE );

    const char *pszOptionList = 
        ((GDALDriver *) hDriver)->GetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST );

    if( papszCreationOptions == NULL || *papszCreationOptions == NULL)
        return TRUE;
    if( pszOptionList == NULL )
        return TRUE;
    
    CPLXMLNode* psNode = CPLParseXMLString(pszOptionList);
    if (psNode == NULL)
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "Could not parse creation option list of driver %s. Assuming creation options are valid.",
                 ((GDALDriver *) hDriver)->GetDescription());
        return TRUE;
    }

    while(*papszCreationOptions)
    {
        char* pszKey = NULL;
        const char* pszValue = CPLParseNameValue(*papszCreationOptions, &pszKey);
        if (pszKey == NULL)
        {
            CPLError(CE_Warning, CPLE_NotSupported,
                     "Creation option '%s' is not formatted with the key=value format",
                     *papszCreationOptions);
            bRet = FALSE;

            papszCreationOptions ++;
            continue;
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

                if (EQUAL(pszOptionName, pszKey) ||
                    EQUAL(CPLGetXMLValue(psChildNode, "alias", ""), pszKey))
                {
                    break;
                }
            }
            psChildNode = psChildNode->psNext;
        }
        if (psChildNode == NULL)
        {
            CPLError(CE_Warning, CPLE_NotSupported,
                     "Driver %s does not support %s creation option",
                     ((GDALDriver *) hDriver)->GetDescription(),
                     pszKey);
            CPLFree(pszKey);
            bRet = FALSE;

            papszCreationOptions ++;
            continue;
        }
        const char* pszType = CPLGetXMLValue(psChildNode, "type", NULL);
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
                             "'%s' is an unexpected value for %s creation option of type int.",
                             pszValue, pszKey);
                        bRet = FALSE;
                        break;
                    }
                    pszValueIter++;
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
                             "'%s' is an unexpected value for %s creation option of type unsigned int.",
                             pszValue, pszKey);
                        bRet = FALSE;
                        break;
                    }
                    pszValueIter++;
                }
            }
            else if (EQUAL(pszType, "FLOAT"))
            {
                char* endPtr = NULL;
                CPLStrtod(pszValue, &endPtr);
                if ( !(endPtr == NULL || *endPtr == '\0') )
                {
                    CPLError(CE_Warning, CPLE_NotSupported,
                             "'%s' is an unexpected value for %s creation option of type float.",
                             pszValue, pszKey);
                    bRet = FALSE;
                }
            }
            else if (EQUAL(pszType, "BOOLEAN"))
            {
                if (!(EQUAL(pszValue, "ON") || EQUAL(pszValue, "TRUE") || EQUAL(pszValue, "YES") ||
                      EQUAL(pszValue, "OFF") || EQUAL(pszValue, "FALSE") || EQUAL(pszValue, "NO")))
                {
                    CPLError(CE_Warning, CPLE_NotSupported,
                             "'%s' is an unexpected value for %s creation option of type boolean.",
                             pszValue, pszKey);
                    bRet = FALSE;
                }
            }
            else if (EQUAL(pszType, "STRING-SELECT"))
            {
                int bMatchFound = FALSE;
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
                                bMatchFound = TRUE;
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
                             "'%s' is an unexpected value for %s creation option of type string-select.",
                             pszValue, pszKey);
                    bRet = FALSE;
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
                             "'%s' is of size %d, whereas maximum size for %s creation option is %d.",
                             pszValue, (int)strlen(pszValue), pszKey, atoi(pszMaxSize));
                        bRet = FALSE;
                    }
                }
            }
            else
            {
                /* Driver error */
                CPLError(CE_Warning, CPLE_NotSupported,
                     "Driver %s : type '%s' for %s creation option is not recognized.",
                     ((GDALDriver *) hDriver)->GetDescription(),
                     pszType,
                     pszKey);
            }
        }
        else
        {
            /* Driver error */
            CPLError(CE_Warning, CPLE_NotSupported,
                     "Driver %s : no type for %s creation option.",
                     ((GDALDriver *) hDriver)->GetDescription(),
                     pszKey);
        }
        CPLFree(pszKey);
        papszCreationOptions++;
    }

    CPLDestroyXMLNode(psNode);
    return bRet;
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
 * path components, are an essentially just the output of CPLReadDir() on the
 * parent directory. If the target object does not have filesystem semantics
 * then the file list should be NULL.
 *
 * @param pszFilename the name of the file to access.  In the case of
 * exotic drivers this may not refer to a physical file, but instead contain
 * information for the driver on how to access a dataset.
 *
 * @param papszFileList an array of strings, whose last element is the NULL pointer.
 * These strings are filenames that are auxiliary to the main filename. The passed
 * value may be NULL.
 *
 * @return A GDALDriverH handle or NULL on failure.  For C++ applications
 * this handle can be cast to a GDALDriver *. 
 */


GDALDriverH CPL_STDCALL 
GDALIdentifyDriver( const char * pszFilename, 
                    char **papszFileList )

{
    int         	iDriver;
    GDALDriverManager  *poDM = GetGDALDriverManager();
    GDALOpenInfo        oOpenInfo( pszFilename, GA_ReadOnly, papszFileList );
    CPLLocaleC          oLocaleForcer;

    CPLErrorReset();
    CPLAssert( NULL != poDM );
    
    for( iDriver = -1; iDriver < poDM->GetDriverCount(); iDriver++ )
    {
        GDALDriver      *poDriver;
        GDALDataset     *poDS;

        if( iDriver < 0 )
            poDriver = GDALGetAPIPROXYDriver();
        else
            poDriver = poDM->GetDriver( iDriver );

        VALIDATE_POINTER1( poDriver, "GDALIdentifyDriver", NULL );

        if( poDriver->pfnIdentify != NULL )
        {
            if( poDriver->pfnIdentify( &oOpenInfo ) )
                return (GDALDriverH) poDriver;
        }
        else if( poDriver->pfnOpen != NULL )
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
    }

    return NULL;
}
