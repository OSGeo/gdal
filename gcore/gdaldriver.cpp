/******************************************************************************
 * $Id$
 *
 * Project:  GDAL Core
 * Purpose:  Implementation of GDALDriver class (and C wrappers)
 * Author:   Frank Warmerdam, warmerda@home.com
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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.19  2001/07/18 04:04:30  warmerda
 * added CPL_CVSID
 *
 * Revision 1.18  2001/02/15 16:30:34  warmerda
 * added create debug message
 *
 * Revision 1.17  2000/10/06 15:26:49  warmerda
 * make buffer size for copying image data the exact size, fixing bug with complex data
 *
 * Revision 1.16  2000/07/13 17:34:11  warmerda
 * Set description for CopyCreate() method.
 *
 * Revision 1.15  2000/07/13 17:27:48  warmerda
 * added SetDescription after create
 *
 * Revision 1.14  2000/06/27 16:47:28  warmerda
 * added cancel support for CopyCreate progress func
 *
 * Revision 1.13  2000/06/26 18:47:14  warmerda
 * Ensure pszHelpTopic is initialized
 *
 * Revision 1.12  2000/04/30 23:22:16  warmerda
 * added CreateCopy support
 *
 * Revision 1.11  2000/03/06 02:21:15  warmerda
 * Added help topic C function
 *
 * Revision 1.10  2000/01/31 16:24:01  warmerda
 * use failure, not fatal
 *
 * Revision 1.9  2000/01/31 15:00:25  warmerda
 * added some documentation
 *
 * Revision 1.8  2000/01/31 14:24:36  warmerda
 * implemented dataset delete
 *
 * Revision 1.7  2000/01/13 04:13:10  pgs
 * added initialization of pfnCreate = NULL to prevent run-time crash when format doesn't support creating a file
 *
 * Revision 1.6  1999/12/08 14:40:50  warmerda
 * Fixed error message.
 *
 * Revision 1.5  1999/10/21 13:22:10  warmerda
 * Added GDALGetDriverShort/LongName().
 *
 * Revision 1.4  1999/01/11 15:36:50  warmerda
 * Added GDALCreate()
 *
 * Revision 1.3  1998/12/31 18:54:53  warmerda
 * Flesh out create method.
 *
 * Revision 1.2  1998/12/06 22:17:32  warmerda
 * Add stub Create() method
 *
 * Revision 1.1  1998/12/03 18:32:01  warmerda
 * New
 *
 */

#include "gdal_priv.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                             GDALDriver()                             */
/************************************************************************/

GDALDriver::GDALDriver()

{
    pszShortName = NULL;
    pszLongName = NULL;
    pszHelpTopic = NULL;

    pfnOpen = NULL;
    pfnCreate = NULL;
    pfnDelete = NULL;
    pfnCreateCopy = NULL;
}

/************************************************************************/
/*                            ~GDALDriver()                             */
/************************************************************************/

GDALDriver::~GDALDriver()

{
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
                  pszShortName, pszFilename, nXSize, nYSize, nBands, 
                  GDALGetDataTypeName( eType ), 
                  papszParmList );
              
        poDS = pfnCreate( pszFilename, nXSize, nYSize, nBands, eType,
                          papszParmList );

        if( poDS != NULL )
            poDS->SetDescription( pszFilename );

        return poDS;
    }
}

/************************************************************************/
/*                             GDALCreate()                             */
/************************************************************************/

GDALDatasetH CPL_DLL GDALCreate( GDALDriverH hDriver,
                                 const char * pszFilename,
                                 int nXSize, int nYSize, int nBands,
                                 GDALDataType eBandType,
                                 char ** papszOptions )

{
    return( ((GDALDriver *) hDriver)->Create( pszFilename,
                                              nXSize, nYSize, nBands,
                                              eBandType, papszOptions ) );
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
        }

        return poDstDS;
    }
    
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
/*      suppose we should.                                              */
/* -------------------------------------------------------------------- */
    double	adfGeoTransform[6];

    if( poSrcDS->GetGeoTransform( adfGeoTransform ) == CE_None )
    {
        poDstDS->SetGeoTransform( adfGeoTransform );
    }

    if( poSrcDS->GetProjectionRef() != NULL
        && strlen(poSrcDS->GetProjectionRef()) > 0 )
    {
        poDstDS->SetProjection( poSrcDS->GetProjectionRef() );
    }

/* -------------------------------------------------------------------- */
/*      Loop copying bands.                                             */
/* -------------------------------------------------------------------- */
    for( int iBand = 0; iBand < poSrcDS->GetRasterCount(); iBand++ )
    {
        GDALRasterBand *poSrcBand = poSrcDS->GetRasterBand( iBand+1 );
        GDALRasterBand *poDstBand = poDstDS->GetRasterBand( iBand+1 );

        void           *pData;

        pData = CPLMalloc(nXSize * GDALGetDataTypeSize(eType) / 8);

        for( int iLine = 0; iLine < nYSize; iLine++ )
        {
            eErr = poSrcBand->RasterIO( GF_Read, 0, iLine, nXSize, 1, 
                                        pData, nXSize, 1, eType, 0, 0 );
            if( eErr != CE_None )
            {
                return NULL;
            }
            
            eErr = poDstBand->RasterIO( GF_Write, 0, iLine, nXSize, 1, 
                                        pData, nXSize, 1, eType, 0, 0 );

            if( eErr != CE_None )
            {
                return NULL;
            }

            if( !pfnProgress( (iBand + iLine / (double) nYSize)
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
/*                           GDALCreateCopy()                           */
/************************************************************************/

GDALDatasetH GDALCreateCopy( GDALDriverH hDriver, 
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
        VSIStatBuf	sStat;

        if( VSIStat( pszFilename, &sStat ) == 0 && VSI_ISREG( sStat.st_mode ) )
        {
            if( VSIUnlink( pszFilename ) == 0 )
                return CE_None;
            else
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                          "%s: Attempt to unlink %s failed.\n",
                          pszShortName, pszFilename );
                return CE_Failure;
            }
        }
        else
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "%s: Unable to delete %s, not a file.\n",
                      pszShortName, pszFilename );
            return CE_Failure;
        }
    }
}

/************************************************************************/
/*                             GDALDelete()                             */
/************************************************************************/

CPLErr GDALDeleteDataset( GDALDriverH hDriver, const char * pszFilename )

{
    return ((GDALDriver *) hDriver)->Delete( pszFilename );
}

/************************************************************************/
/*                       GDALGetDriverShortName()                       */
/************************************************************************/

const char * GDALGetDriverShortName( GDALDriverH hDriver )

{
    if( hDriver == NULL )
        return NULL;
    else
        return ((GDALDriver *) hDriver)->pszShortName;
}

/************************************************************************/
/*                       GDALGetDriverLongName()                        */
/************************************************************************/

const char * GDALGetDriverLongName( GDALDriverH hDriver )

{
    if( hDriver == NULL )
        return NULL;
    else
        return ((GDALDriver *) hDriver)->pszLongName;
}

/************************************************************************/
/*                       GDALGetDriverHelpTopic()                       */
/************************************************************************/

const char * GDALGetDriverHelpTopic( GDALDriverH hDriver )

{
    if( hDriver == NULL )
        return NULL;
    else
        return ((GDALDriver *) hDriver)->pszHelpTopic;
}

