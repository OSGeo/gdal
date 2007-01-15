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
 ****************************************************************************/

#include "gdal_priv.h"

CPL_CVSID("$Id$");

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
/*                               Create()                               */
/************************************************************************/

/**
 * Create a new dataset with this driver.
 *
 * What argument values are legal for particular drivers is driver specific,
 * and there is no way to query in advance to establish legal values.
 *
 * Equivelent of the C function GDALCreate().
 * 
 * @param pszFilename the name of the dataset to create.
 * @param nXSize width of created raster in pixels.
 * @param nYSize height of created raster in pixels.
 * @param nBands number of bands.
 * @param eType type of raster.
 * @param papszParmList list of driver specific control parameters.
 *
 * @return NULL on failure, or a new GDALDataset.
 */

GDALDataset * GDALDriver::Create( const char * pszFilename,
                                  int nXSize, int nYSize, int nBands,
                                  GDALDataType eType, char ** papszParmList )

{
    CPLLocaleC  oLocaleForcer;

    /* notdef: should add a bunch of error checking here */

    if( pfnCreate == NULL )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "GDALDriver::Create() ... no create method implemented"
                  " for this format.\n" );

        return NULL;
    }
    else
    {
        GDALDataset *poDS;

        CPLDebug( "GDAL", "GDALDriver::Create(%s,%s,%d,%d,%d,%s,%p)",
                  GetDescription(), pszFilename, nXSize, nYSize, nBands, 
                  GDALGetDataTypeName( eType ), 
                  papszParmList );
              
        poDS = pfnCreate( pszFilename, nXSize, nYSize, nBands, eType,
                          papszParmList );

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
}

/************************************************************************/
/*                             GDALCreate()                             */
/************************************************************************/

/**
 * @see GDALDriver::Create()
 */

GDALDatasetH CPL_DLL CPL_STDCALL 
GDALCreate( GDALDriverH hDriver, const char * pszFilename,
            int nXSize, int nYSize, int nBands, GDALDataType eBandType,
            char ** papszOptions )

{
    return( ((GDALDriver *) hDriver)->Create( pszFilename,
                                              nXSize, nYSize, nBands,
                                              eBandType, papszOptions ) );
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

/* -------------------------------------------------------------------- */
/*      Create destination dataset.                                     */
/* -------------------------------------------------------------------- */
    GDALDataset  *poDstDS;
    int          nXSize = poSrcDS->GetRasterXSize();
    int          nYSize = poSrcDS->GetRasterYSize();
    GDALDataType eType = poSrcDS->GetRasterBand(1)->GetRasterDataType();
    CPLErr       eErr;

    CPLDebug( "GDAL", "Using default GDALDriver::CreateCopy implementation." );

    if( !pfnProgress( 0.0, NULL, pProgressData ) )
    {
        CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
        return NULL;
    }

    poDstDS = Create( pszFilename, nXSize, nYSize, 
                      poSrcDS->GetRasterCount(), eType, papszOptions );

    if( poDstDS == NULL )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Try setting the projection and geotransform if it seems         */
/*      suitable.  For now we don't try and copy GCPs, though I         */
/*      suppose we should. Also copy metadata.                          */
/* -------------------------------------------------------------------- */
    double      adfGeoTransform[6];

    if( poSrcDS->GetGeoTransform( adfGeoTransform ) == CE_None 
        && (adfGeoTransform[0] != 0.0 
            || adfGeoTransform[1] != 1.0
            || adfGeoTransform[2] != 0.0
            || adfGeoTransform[3] != 0.0
            || adfGeoTransform[4] != 0.0
            || adfGeoTransform[5] != 1.0) )
    {
        poDstDS->SetGeoTransform( adfGeoTransform );
    }

    if( poSrcDS->GetProjectionRef() != NULL
        && strlen(poSrcDS->GetProjectionRef()) > 0 )
    {
        poDstDS->SetProjection( poSrcDS->GetProjectionRef() );
    }

    poDstDS->SetMetadata( poSrcDS->GetMetadata() );

/* -------------------------------------------------------------------- */
/*      Loop copying bands.                                             */
/* -------------------------------------------------------------------- */
    for( int iBand = 0; iBand < poSrcDS->GetRasterCount(); iBand++ )
    {
        GDALRasterBand *poSrcBand = poSrcDS->GetRasterBand( iBand+1 );
        GDALRasterBand *poDstBand = poDstDS->GetRasterBand( iBand+1 );

/* -------------------------------------------------------------------- */
/*      Do we need to copy a colortable or other metadata?              */
/* -------------------------------------------------------------------- */
        GDALColorTable *poCT;
        char** catNames;
        int bSuccess;
        double dfValue;

        poCT = poSrcBand->GetColorTable();
        if( poCT != NULL )
            poDstBand->SetColorTable( poCT );

        if( !bStrict )
            CPLPushErrorHandler( CPLQuietErrorHandler );

        if( strlen(poSrcBand->GetDescription()) > 0 )
            poDstBand->SetDescription( poSrcBand->GetDescription() );

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

        if( poSrcBand->GetColorInterpretation() != GCI_Undefined )
            poDstBand->SetColorInterpretation( 
                poSrcBand->GetColorInterpretation() );

        catNames = poSrcBand->GetCategoryNames();
        if (0 != catNames)
            poDstBand->SetCategoryNames(catNames);

        if( !bStrict )
            CPLPopErrorHandler();

/* -------------------------------------------------------------------- */
/*      Copy image data.                                                */
/* -------------------------------------------------------------------- */
        void           *pData;

        pData = VSIMalloc(nXSize * GDALGetDataTypeSize(eType) / 8);
        if( pData == NULL )
        {
            CPLError( CE_Failure, CPLE_OutOfMemory,
                      "CreateCopy(): Out of memory allocating %d byte line buffer.\n",
                      nXSize * GDALGetDataTypeSize(eType) / 8 );
            delete poDstDS;
            Delete( pszFilename );
            return NULL;
        }

        for( int iLine = 0; iLine < nYSize; iLine++ )
        {
            eErr = poSrcBand->RasterIO( GF_Read, 0, iLine, nXSize, 1, 
                                        pData, nXSize, 1, eType, 0, 0 );
            if( eErr != CE_None )
            {
                delete poDstDS;
                Delete( pszFilename );
                return NULL;
            }
            
            eErr = poDstBand->RasterIO( GF_Write, 0, iLine, nXSize, 1, 
                                        pData, nXSize, 1, eType, 0, 0 );

            if( eErr != CE_None )
            {
                delete poDstDS;
                return NULL;
            }

            if( !pfnProgress( (iBand + (iLine+1) / (double) nYSize)
                              / (double) poSrcDS->GetRasterCount(), 
                              NULL, pProgressData ) )
            {
                CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
                delete poDstDS;
                Delete( pszFilename );
                return NULL;
            }
        }

        CPLFree( pData );
    }

    return poDstDS;
}

/************************************************************************/
/*                             CreateCopy()                             */
/************************************************************************/

/**
 * Create a copy of a dataset.
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
 * It is intended that CreateCopy() would often be used with a source dataset
 * which is a virtual dataset allowing configuration of band types, and
 * other information without actually duplicating raster data.  This virtual
 * dataset format hasn't yet been implemented at the time of this documentation
 * being written. 
 *
 * @param pszFilename the name for the new dataset. 
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

/* -------------------------------------------------------------------- */
/*      If the format provides a CreateCopy() method use that,          */
/*      otherwise fallback to the internal implementation using the     */
/*      Create() method.                                                */
/* -------------------------------------------------------------------- */
    if( pfnCreateCopy != NULL )
    {
        GDALDataset *poDstDS;

        poDstDS = pfnCreateCopy( pszFilename, poSrcDS, bStrict, papszOptions,
                                 pfnProgress, pProgressData );
        if( poDstDS != NULL )
        {
            if( poDstDS->GetDescription() == NULL 
                || strlen(poDstDS->GetDescription()) > 0 )
                poDstDS->SetDescription( pszFilename );

            if( poDstDS->poDriver == NULL )
                poDstDS->poDriver = this;
        }

        return poDstDS;
    }
    else
        return DefaultCreateCopy( pszFilename, poSrcDS, bStrict, 
                                  papszOptions, pfnProgress, pProgressData );
}

/************************************************************************/
/*                           GDALCreateCopy()                           */
/************************************************************************/

/**
 * @see GDALDriver::CreateCopy()
 */

GDALDatasetH CPL_STDCALL GDALCreateCopy( GDALDriverH hDriver, 
                             const char * pszFilename, 
                             GDALDatasetH hSrcDS, 
                             int bStrict, char ** papszOptions,
                             GDALProgressFunc pfnProgress,
                             void * pProgressData )

{
    return (GDALDatasetH) ((GDALDriver *) hDriver)->
        CreateCopy( pszFilename, (GDALDataset *) hSrcDS, bStrict, papszOptions,
                    pfnProgress, pProgressData );
}

/************************************************************************/
/*                               Delete()                               */
/************************************************************************/

/**
 * Delete named dataset.
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
    else
    {
        VSIStatBufL     sStat;

        if( VSIStatL( pszFilename, &sStat ) == 0 
            && VSI_ISREG( sStat.st_mode ) )
        {
            if( VSIUnlink( pszFilename ) == 0 )
                return CE_None;
            else
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                          "%s: Attempt to unlink %s failed.\n",
                          GetDescription(), pszFilename );
                return CE_Failure;
            }
        }
        else
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "%s: Unable to delete %s, not a file.\n",
                      GetDescription(), pszFilename );
            return CE_Failure;
        }
    }
}

/************************************************************************/
/*                             GDALDelete()                             */
/************************************************************************/

/**
 * @see GDALDriver::Delete()
 */

CPLErr CPL_STDCALL GDALDeleteDataset( GDALDriverH hDriver, const char * pszFilename )

{
    return ((GDALDriver *) hDriver)->Delete( pszFilename );
}

/************************************************************************/
/*                       GDALGetDriverShortName()                       */
/************************************************************************/

const char * CPL_STDCALL GDALGetDriverShortName( GDALDriverH hDriver )

{
    if( hDriver == NULL )
        return NULL;
    else
        return ((GDALDriver *) hDriver)->GetDescription();
}

/************************************************************************/
/*                       GDALGetDriverLongName()                        */
/************************************************************************/

const char * CPL_STDCALL GDALGetDriverLongName( GDALDriverH hDriver )

{
    if( hDriver == NULL )
        return NULL;

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

const char * CPL_STDCALL GDALGetDriverHelpTopic( GDALDriverH hDriver )

{
    if( hDriver == NULL )
        return NULL;

    return ((GDALDriver *) hDriver)->GetMetadataItem( GDAL_DMD_HELPTOPIC );
}

/************************************************************************/
/*                   GDALGetDriverCreationOptionList()                  */
/************************************************************************/

const char * CPL_STDCALL GDALGetDriverCreationOptionList( GDALDriverH hDriver )

{
    if( hDriver == NULL )
        return NULL;

    const char *pszOptionList = 
        ((GDALDriver *) hDriver)->GetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST );

    if( pszOptionList == NULL )
        return "";
    else
        return pszOptionList;
}
