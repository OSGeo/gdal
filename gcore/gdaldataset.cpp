/******************************************************************************
 * $Id$
 *
 * Project:  GDAL Core
 * Purpose:  Base class for raster file formats.  
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
 * Revision 1.22  2001/01/10 22:24:37  warmerda
 * Patched GDALDataset::FlushCache() to recover gracefully if papoBands
 * doesn't exist yet matching nBands.
 *
 * Revision 1.21  2000/10/06 15:27:13  warmerda
 * default bands to same access as dataset in SetBand()
 *
 * Revision 1.20  2000/08/09 16:26:00  warmerda
 * debug message on dataset cleanup
 *
 * Revision 1.19  2000/07/11 14:35:43  warmerda
 * added documentation
 *
 * Revision 1.18  2000/06/27 16:46:56  warmerda
 * default to using dummy progress func
 *
 * Revision 1.17  2000/06/26 21:44:50  warmerda
 * make progress func save for overviews
 *
 * Revision 1.16  2000/06/26 18:47:31  warmerda
 * added GDALBuildOverviews
 *
 * Revision 1.15  2000/04/21 21:56:23  warmerda
 * move metadata to GDALMajorObject, added BuildOverviews
 *
 * Revision 1.14  2000/03/31 13:42:06  warmerda
 * added gcp support methods
 *
 * Revision 1.13  2000/03/23 16:53:55  warmerda
 * default geotransform is 0,1,0,0,0,1
 *
 * Revision 1.12  2000/03/06 21:50:10  warmerda
 * fixed bug with setting nBands
 *
 * Revision 1.11  2000/03/06 02:20:56  warmerda
 * added reference counting
 *
 * Revision 1.10  2000/02/28 16:34:49  warmerda
 * set the nRasterX/YSize in bands
 *
 * Revision 1.9  1999/11/11 21:59:07  warmerda
 * added GetDriver() for datasets
 *
 * Revision 1.8  1999/10/01 14:44:02  warmerda
 * added documentation
 *
 * Revision 1.7  1999/05/17 01:43:10  warmerda
 * fixed GDALSetGeoTransform()
 *
 * Revision 1.6  1999/05/16 20:04:58  warmerda
 * Don't emit an error message when SetProjection() is called for datasets
 * that don't implement the call.
 *
 * Revision 1.5  1999/04/21 04:16:51  warmerda
 * experimental docs
 *
 * Revision 1.4  1999/01/11 15:37:55  warmerda
 * fixed log keyword
 */

#include "gdal_priv.h"
#include "cpl_string.h"

/************************************************************************/
/*                            GDALDataset()                             */
/************************************************************************/

GDALDataset::GDALDataset()

{
    poDriver = NULL;
    eAccess = GA_ReadOnly;
    nRasterXSize = 512;
    nRasterYSize = 512;
    nBands = 0;
    papoBands = NULL;
    nRefCount = 1;
}

/************************************************************************/
/*                            ~GDALDataset()                            */
/************************************************************************/

/**
 * Destroy an open GDALDataset.
 *
 * This is the accepted method of closing a GDAL dataset and deallocating
 * all resources associated with it.
 *
 * Equivelent of the C callable GDALClose().
 */

GDALDataset::~GDALDataset()

{
    int		i;

    CPLDebug( "GDAL", "GDALClose(%s)\n", GetDescription() );

/* -------------------------------------------------------------------- */
/*      Destroy the raster bands if they exist.                         */
/* -------------------------------------------------------------------- */
    for( i = 0; i < nBands && papoBands != NULL; i++ )
    {
        if( papoBands[i] != NULL )
            delete papoBands[i];
    }

    CPLFree( papoBands );
}

/************************************************************************/
/*                             GDALClose()                              */
/************************************************************************/

void GDALClose( GDALDatasetH hDS )

{
    delete ((GDALDataset *) hDS);
}

/************************************************************************/
/*                             FlushCache()                             */
/************************************************************************/

/**
 * Flush all write cached data to disk.
 *
 * Any raster (or other GDAL) data written via GDAL calls, but buffered
 * internally will be written to disk.
 */

void GDALDataset::FlushCache()

{
    int		i;

    // This sometimes happens if a dataset is destroyed before completely
    // built. 

    if( papoBands == NULL )
        return;

    for( i = 0; i < nBands; i++ )
    {
        if( papoBands[i] != NULL )
            papoBands[i]->FlushCache();
    }
}

/************************************************************************/
/*                          RasterInitialize()                          */
/*                                                                      */
/*      Initialize raster size                                          */
/************************************************************************/

void GDALDataset::RasterInitialize( int nXSize, int nYSize )

{
    CPLAssert( nXSize > 0 && nYSize > 0 );
    
    nRasterXSize = nXSize;
    nRasterYSize = nYSize;
}

/************************************************************************/
/*                              SetBand()                               */
/*                                                                      */
/*      Set a band in the band array, updating the band count, and      */
/*      array size appropriately.                                       */
/************************************************************************/

void GDALDataset::SetBand( int nNewBand, GDALRasterBand * poBand )

{
/* -------------------------------------------------------------------- */
/*      Do we need to grow the bands list?                              */
/* -------------------------------------------------------------------- */
    if( nBands < nNewBand || papoBands == NULL ) {
        int		i;

        if( papoBands == NULL )
            papoBands = (GDALRasterBand **)
                VSICalloc(sizeof(GDALRasterBand*), MAX(nNewBand,nBands));
        else
            papoBands = (GDALRasterBand **)
                VSIRealloc(papoBands, sizeof(GDALRasterBand*) *
                           MAX(nNewBand,nBands));

        for( i = nBands; i < nNewBand; i++ )
            papoBands[i] = NULL;

        nBands = MAX(nBands,nNewBand);
    }

/* -------------------------------------------------------------------- */
/*      Set the band.  Resetting the band is currently not permitted.   */
/* -------------------------------------------------------------------- */
    CPLAssert( papoBands[nNewBand-1] == NULL );

    papoBands[nNewBand-1] = poBand;

/* -------------------------------------------------------------------- */
/*      Set back reference information on the raster band.  Note        */
/*      that the GDALDataset is a friend of the GDALRasterBand          */
/*      specifically to allow this.                                     */
/* -------------------------------------------------------------------- */
    poBand->nBand = nNewBand;
    poBand->poDS = this;
    poBand->nRasterXSize = nRasterXSize;
    poBand->nRasterYSize = nRasterYSize;
    poBand->eAccess = eAccess; /* default access to be same as dataset */
}

/************************************************************************/
/*                           GetRasterXSize()                           */
/************************************************************************/

/**

 Fetch raster width in pixels.

 Equivelent of the C function GDALGetRasterXSize().

 @return the width in pixels of raster bands in this GDALDataset.

*/

int GDALDataset::GetRasterXSize()

{
    return nRasterXSize;
}

/************************************************************************/
/*                         GDALGetRasterXSize()                         */
/************************************************************************/

int GDALGetRasterXSize( GDALDatasetH hDataset )

{
    return ((GDALDataset *) hDataset)->GetRasterXSize();
}


/************************************************************************/
/*                           GetRasterYSize()                           */
/************************************************************************/

/**

 Fetch raster height in pixels.

 Equivelent of the C function GDALGetRasterYSize().

 @return the height in pixels of raster bands in this GDALDataset.

*/

int GDALDataset::GetRasterYSize()

{
    return nRasterYSize;
}

/************************************************************************/
/*                         GDALGetRasterYSize()                         */
/************************************************************************/

int GDALGetRasterYSize( GDALDatasetH hDataset )

{
    return ((GDALDataset *) hDataset)->GetRasterYSize();
}

/************************************************************************/
/*                           GetRasterBand()                            */
/************************************************************************/

/**

 Fetch a band object for a dataset.

 Equivelent of the C function GDALGetRasterBand().

 @param nBandId the index number of the band to fetch, from 1 to
                GetRasterCount().

 @return the height in pixels of raster bands in this GDALDataset.

*/

GDALRasterBand * GDALDataset::GetRasterBand( int nBandId )

{
    if( nBandId < 1 || nBandId > nBands )
    {
        CPLError( CE_Fatal, CPLE_IllegalArg,
                  "GDALDataset::GetRasterBand(%d) - Illegal band #\n",
                  nBandId );
    }

    return( papoBands[nBandId-1] );
}

/************************************************************************/
/*                         GDALGetRasterBand()                          */
/************************************************************************/

GDALRasterBandH GDALGetRasterBand( GDALDatasetH hDS, int nBandId )

{
    return( (GDALRasterBandH) ((GDALDataset *) hDS)->GetRasterBand(nBandId) );
}

/************************************************************************/
/*                           GetRasterCount()                           */
/************************************************************************/

/**
 * Fetch the number of raster bands on this dataset.
 *
 * Same as the C function GDALGetRasterCount().
 *
 * @return the number of raster bands.
 */

int GDALDataset::GetRasterCount()

{
    return( nBands );
}

/************************************************************************/
/*                         GDALGetRasterCount()                         */
/************************************************************************/

int GDALGetRasterCount( GDALDatasetH hDS )

{
    return( ((GDALDataset *) hDS)->GetRasterCount() );
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

/**
 * Fetch the projection definition string for this dataset.
 *
 * Same as the C function GDALGetProjectionRef().
 *
 * The returned string defines the projection coordinate system of the
 * image in either PROJ.4 format or OpenGIS WKT format.  It should be
 * suitable for use with the GDALProjDef object to reproject positions.
 *
 * When a projection definition is not available an empty (but not NULL)
 * string is returned.
 *
 * @return a pointer to an internal projection reference string.  It should
 * not be altered, freed or expected to last for long. 
 */

const char *GDALDataset::GetProjectionRef()

{
    return( "" );
}

/************************************************************************/
/*                        GDALGetProjectionRef()                        */
/************************************************************************/

const char *GDALGetProjectionRef( GDALDatasetH hDS )

{
    return( ((GDALDataset *) hDS)->GetProjectionRef() );
}

/************************************************************************/
/*                           SetProjection()                            */
/************************************************************************/

/**
 * Set the projection reference string for this dataset.
 *
 * The string should be in OGC WKT or PROJ.4 format.  An error may occur
 * because of incorrectly specified projection strings, because the dataset
 * is not writable, or because the dataset does not support the indicated
 * projection.  Many formats do not support writing projections.
 *
 * This method is the same as the C GDALSetProjection() function. 
 *
 * @param pszProjection projection reference string.
 *
 * @return CE_Failure if an error occurs, otherwise CE_None.
 */

CPLErr GDALDataset::SetProjection( const char * )

{
    return CE_Failure;
}

/************************************************************************/
/*                         GDALSetProjection()                          */
/************************************************************************/

CPLErr GDALSetProjection( GDALDatasetH hDS, const char * pszProjection )

{
    return( ((GDALDataset *) hDS)->SetProjection(pszProjection) );
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

/**
 * Fetch the affine transformation coefficients.
 *
 * Fetches the coefficients for transforming between pixel/line (P,L) raster
 * space, and projection coordinates (Xp,Yp) space.
 *
 *  Xp = padfTransform[0] + P*padfTransform[1] + L*padfTransform[2];
 *  Yp = padfTransform[3] + P*padfTransform[4] + L*padfTransform[5];
 *
 * In a north up image, padfTransform[1] is the pixel width, and
 * padfTransform[5] is the pixel height.  The upper left corner of the
 * upper left pixel is at position (padfTransform[0],padfTransform[3]).
 *
 * The default transform is (0,1,0,0,0,1) and should be returned even when
 * a CE_Failure error is returned, such as for formats that don't support
 * transformation to projection coordinates.
 *
 * NOTE: GetGeoTransform() isn't expressive enough to handle the variety of
 * OGC Grid Coverages pixel/line to projection transformation schemes.
 * Eventually this method will be depreciated in favour of a more general
 * scheme.
 *
 * This method does the same thing as the C GDALGetGeoTransform() function.
 *
 * @param padfTransform an existing six double buffer into which the
 * transformation will be placed.
 *
 * @return CE_None on success, or CE_Failure if no transform can be fetched.
 */

CPLErr GDALDataset::GetGeoTransform( double * padfTransform )

{
    CPLAssert( padfTransform != NULL );
        
    padfTransform[0] = 0.0;	/* X Origin (top left corner) */
    padfTransform[1] = 1.0;	/* X Pixel size */
    padfTransform[2] = 0.0;

    padfTransform[3] = 0.0;	/* Y Origin (top left corner) */
    padfTransform[4] = 0.0;	
    padfTransform[5] = 1.0;	/* Y Pixel Size */

    return( CE_Failure );
}

/************************************************************************/
/*                        GDALGetGeoTransform()                         */
/************************************************************************/

CPLErr GDALGetGeoTransform( GDALDatasetH hDS, double * padfTransform )

{
    return( ((GDALDataset *) hDS)->GetGeoTransform(padfTransform) );
}

/************************************************************************/
/*                          SetGeoTransform()                           */
/************************************************************************/

/**
 * Set the affine transformation coefficients.
 *
 * See GetGeoTransform() for details on the meaning of the padfTransform
 * coefficients.
 *
 * This method does the same thing as the C GDALSetGeoTransform() function.
 *
 * @param padfTransform a six double buffer containing the transformation
 * coefficients to be written with the dataset.
 *
 * @return CE_None on success, or CE_Failure if this transform cannot be
 * written.
 */

CPLErr GDALDataset::SetGeoTransform( double * )

{
    CPLError( CE_Failure, CPLE_NotSupported,
              "SetGeoTransform() not supported for this dataset." );
    
    return( CE_Failure );
}

/************************************************************************/
/*                        GDALSetGeoTransform()                         */
/************************************************************************/

CPLErr GDALSetGeoTransform( GDALDatasetH hDS, double * padfTransform )

{
    return( ((GDALDataset *) hDS)->SetGeoTransform(padfTransform) );
}

/************************************************************************/
/*                         GetInternalHandle()                          */
/************************************************************************/

/**
 * Fetch a format specific internally meaningful handle.
 *
 * This method is the same as the C GDALGetInternalHandle() method. 
 *
 * @param pszHandleName the handle name desired.  The meaningful names
 * will be specific to the file format.
 *
 * @return the desired handle value, or NULL if not recognised/supported.
 */

void *GDALDataset::GetInternalHandle( const char * )

{
    return( NULL );
}

/************************************************************************/
/*                       GDALGetInternalHandle()                        */
/************************************************************************/

void *GDALGetInternalHandle( GDALDatasetH hDS, const char * pszRequest )

{
    return( ((GDALDataset *) hDS)->GetInternalHandle(pszRequest) );
}

/************************************************************************/
/*                             GetDriver()                              */
/************************************************************************/

/**
 * Fetch the driver to which this dataset relates.
 *
 * This method is the same as the C GDALGetDatasetDriver() function.
 *
 * @return the driver on which the dataset was created with GDALOpen() or
 * GDALCreate().
 */

GDALDriver * GDALDataset::GetDriver()

{
    return poDriver;
}

/************************************************************************/
/*                        GDALGetDatasetDriver()                        */
/************************************************************************/

GDALDriverH GDALGetDatasetDriver( GDALDatasetH hDataset )

{
    return (GDALDriverH) ((GDALDataset *) hDataset)->GetDriver();
}

/************************************************************************/
/*                             Reference()                              */
/************************************************************************/

/**
 * Add one to dataset reference count.
 *
 * The reference is one after instantiation.
 *
 * This method is the same as the C GDALReferenceDataset() function.
 *
 * @return the post-increment reference count.
 */

int GDALDataset::Reference()

{
    return ++nRefCount;
}

/************************************************************************/
/*                        GDALReferenceDataset()                        */
/************************************************************************/

int GDALReferenceDataset( GDALDatasetH hDataset )

{
    return ((GDALDataset *) hDataset)->Reference();
}

/************************************************************************/
/*                             Reference()                              */
/************************************************************************/

/**
 * Subtract one from dataset reference count.
 *
 * The reference is one after instantiation.  Generally when the reference
 * count has dropped to zero the dataset may be safely deleted (closed).
 *
 * This method is the same as the C GDALDereferenceDataset() function.
 *
 * @return the post-decrement reference count.
 */

int GDALDataset::Dereference()

{
    return --nRefCount;
}

/************************************************************************/
/*                       GDALDereferenceDataset()                       */
/************************************************************************/

int GDALDereferenceDataset( GDALDatasetH hDataset )

{
    return ((GDALDataset *) hDataset)->Dereference();
}

/************************************************************************/
/*                            GetGCPCount()                             */
/************************************************************************/

/**
 * Get number of GCPs. 
 *
 * This method is the same as the C function GDALGetGCPCount(). 
 *
 * @return number of GCPs for this dataset.  Zero if there are none.
 */

int GDALDataset::GetGCPCount()

{
    return 0;
}

/************************************************************************/
/*                          GDALGetGCPCount()                           */
/************************************************************************/

int GDALGetGCPCount( GDALDatasetH hDS )

{
    return ((GDALDataset *) hDS)->GetGCPCount();
}

/************************************************************************/
/*                          GetGCPProjection()                          */
/************************************************************************/

/**
 * Get output projection for GCPs. 
 *
 * This method is the same as the C function GDALGetGCPProjection(). 
 *
 * The projection string follows the normal rules from GetProjectionRef().
 * 
 * @return internal projection string or "" if there are no GCPs. 
 */

const char *GDALDataset::GetGCPProjection()

{
    return "";
}

/************************************************************************/
/*                        GDALGetGCPProjection()                        */
/************************************************************************/

const char *GDALGetGCPProjection( GDALDatasetH hDS )

{
    return ((GDALDataset *) hDS)->GetGCPProjection();
}

/************************************************************************/
/*                               GetGCPs()                              */
/************************************************************************/

/**
 * Fetch GCPs.
 *
 * This method is the same as the C function GDALGetGCPs(). 
 *
 * @return pointer to internal GCP structure list.  It should not be modified, 
 * and may change on the next GDAL call. 
 */ 

const GDAL_GCP *GDALDataset::GetGCPs()

{
    return NULL;
}

/************************************************************************/
/*                            GDALGetGCPs()                             */
/************************************************************************/

const GDAL_GCP *GDALGetGCPs( GDALDatasetH hDS )

{
    return ((GDALDataset *) hDS)->GetGCPs();
}

/************************************************************************/
/*                           BuildOverviews()                           */
/************************************************************************/

/**
 * Build raster overview(s)
 *
 * If the operation is unsupported for the indicated dataset, then 
 * CE_Failure is returned, and CPLGetLastError() will return CPLE_NonSupported.
 *
 * @param pszResampling one of "NEAREST", "AVERAGE" or "MODE" controlling
 * the downsampling method applied.
 * @param nOverviews number of overviews to build. 
 * @param panOverviewList the list of overview decimation factors to build. 
 * @param nBand number of bands to build overviews for in panBandList.  Build
 * for all bands if this is 0. 
 * @param panBandList list of band numbers. 
 * @param pfnProgress a function to call to report progress, or NULL.
 * @param pProgressData application data to pass to the progress function.
 *
 * @return CE_None on success or CE_Failure if the operation doesn't work.
 *
 * For example, to build overview level 2, 4 and 8 on all bands the following
 * call could be made:
 * <pre>
 *   int       anOverviewList[3] = { 2, 4, 8 };
 *   
 *   poDataset->BuildOverviews( "NEAREST", 3, anOverviewList, 0, NULL, 
 *                              GDALDummyProgress, NULL );
 * </pre>
 */

CPLErr GDALDataset::BuildOverviews( const char *pszResampling, 
                                    int nOverviews, int *panOverviewList, 
                                    int nBands, int *panBandList,
                                    GDALProgressFunc pfnProgress, 
                                    void * pProgressData )
    
{
    CPLErr   eErr;
    int      *panAllBandList = NULL;

    if( nBands == 0 )
    {
        nBands = GetRasterCount();
        panAllBandList = (int *) CPLMalloc(sizeof(int) * nBands);
        for( int i = 0; i < nBands; i++ )
            panAllBandList[i] = i+1;

        panBandList = panAllBandList;
    }

    if( pfnProgress == NULL )
        pfnProgress = GDALDummyProgress;

    eErr = IBuildOverviews( pszResampling, nOverviews, panOverviewList, 
                            nBands, panBandList, pfnProgress, pProgressData );

    if( panAllBandList != NULL )
        CPLFree( panAllBandList );

    return eErr;
}

/************************************************************************/
/*                         GDALBuildOverviews()                         */
/************************************************************************/

CPLErr GDALBuildOverviews( GDALDatasetH hDataset,
                           const char *pszResampling, 
                           int nOverviews, int *panOverviewList, 
                           int nBands, int *panBandList,
                           GDALProgressFunc pfnProgress, 
                           void * pProgressData )

{
    return ((GDALDataset *) hDataset)->BuildOverviews(
        pszResampling, nOverviews, panOverviewList, nBands, panBandList, 
        pfnProgress, pProgressData );
}
    
/************************************************************************/
/*                          IBuildOverviews()                           */
/*                                                                      */
/*      Default implementation.                                         */
/************************************************************************/

CPLErr GDALDataset::IBuildOverviews( const char *pszResampling, 
                                     int nOverviews, int *panOverviewList, 
                                     int nBands, int *panBandList,
                                     GDALProgressFunc pfnProgress, 
                                     void * pProgressData )
    
{
    if( pfnProgress == NULL )
        pfnProgress = GDALDummyProgress;

    if( oOvManager.IsInitialized() )
        return oOvManager.BuildOverviews( NULL, pszResampling, 
                                          nOverviews, panOverviewList,
                                          nBands, panBandList,
                                          pfnProgress, pProgressData );
    else
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "BuildOverviews() not supported for this dataset." );
        
        return( CE_Failure );
    }
}

