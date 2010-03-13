/******************************************************************************
 * $Id$
 *
 * Project:  GDAL Core
 * Purpose:  Base class for raster file formats.  
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1998, 2003, Frank Warmerdam
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
#include "cpl_string.h"
#include "cpl_hash_set.h"
#include "cpl_multiproc.h"

CPL_CVSID("$Id$");

typedef struct
{
    /* PID of the thread that mark the dataset as shared */
    /* This may not be the actual PID, but the responsiblePID */
    GIntBig      nPID;
    char        *pszDescription;
    GDALAccess   eAccess;

    GDALDataset *poDS;
} SharedDatasetCtxt;

typedef struct
{
    GDALDataset *poDS;
    /* In the case of a shared dataset, memorize the PID of the thread */
    /* that marked the dataset as shared, so that we can remove it from */
    /* the phSharedDatasetSet in the destructor of the dataset, even */
    /* if GDALClose is called from a different thread */
    /* Ideally, this should be stored in the GDALDataset object itself */
    /* but this is inconvenient to change the object size at that time */
    GIntBig      nPIDCreatorForShared; 
} DatasetCtxt;

/* Set of datasets opened as shared datasets (with GDALOpenShared) */
/* The values in the set are of type SharedDatasetCtxt */
static CPLHashSet* phSharedDatasetSet = NULL; 

/* Set of all datasets created in the constructor of GDALDataset */
/* The values in the set are of type DatasetCtxt */
static CPLHashSet* phAllDatasetSet = NULL;
static void *hDLMutex = NULL;

/* Static array of all datasets. Used by GDALGetOpenDatasets */
/* Not thread-safe. See GDALGetOpenDatasets */
static GDALDataset** ppDatasets = NULL;

static unsigned long GDALSharedDatasetHashFunc(const void* elt)
{
    SharedDatasetCtxt* psStruct = (SharedDatasetCtxt*) elt;
    return (unsigned long) (CPLHashSetHashStr(psStruct->pszDescription) ^ psStruct->eAccess ^ psStruct->nPID);
}

static int GDALSharedDatasetEqualFunc(const void* elt1, const void* elt2)
{
    SharedDatasetCtxt* psStruct1 = (SharedDatasetCtxt*) elt1;
    SharedDatasetCtxt* psStruct2 = (SharedDatasetCtxt*) elt2;
    return strcmp(psStruct1->pszDescription, psStruct2->pszDescription) == 0 &&
           psStruct1->nPID == psStruct2->nPID &&
           psStruct1->eAccess == psStruct2->eAccess;
}

static void GDALSharedDatasetFreeFunc(void* elt)
{
    SharedDatasetCtxt* psStruct = (SharedDatasetCtxt*) elt;
    CPLFree(psStruct->pszDescription);
    CPLFree(psStruct);
}

static unsigned long GDALDatasetHashFunc(const void* elt)
{
    DatasetCtxt* psStruct = (DatasetCtxt*) elt;
    return (unsigned long)(GUIntBig) psStruct->poDS;
}

static int GDALDatasetEqualFunc(const void* elt1, const void* elt2)
{
    DatasetCtxt* psStruct1 = (DatasetCtxt*) elt1;
    DatasetCtxt* psStruct2 = (DatasetCtxt*) elt2;
    return psStruct1->poDS == psStruct2->poDS ;
}

static void GDALDatasetFreeFunc(void* elt)
{
    DatasetCtxt* psStruct = (DatasetCtxt*) elt;
    CPLFree(psStruct);
}

/************************************************************************/
/* Functions shared between gdalproxypool.cpp and gdaldataset.cpp */
/************************************************************************/

void** GDALGetphDLMutex();
void GDALSetResponsiblePIDForCurrentThread(GIntBig responsiblePID);
GIntBig GDALGetResponsiblePIDForCurrentThread();

/* The open-shared mutex must be used by the ProxyPool too */
void** GDALGetphDLMutex()
{
    return &hDLMutex;
}

/* The current thread will act in the behalf of the thread of PID responsiblePID */
void GDALSetResponsiblePIDForCurrentThread(GIntBig responsiblePID)
{
    GIntBig* pResponsiblePID = (GIntBig*) CPLGetTLS(CTLS_RESPONSIBLEPID);
    if (pResponsiblePID == NULL)
    {
        pResponsiblePID = (GIntBig*) CPLMalloc(sizeof(GIntBig));
        CPLSetTLS(CTLS_RESPONSIBLEPID, pResponsiblePID, TRUE);
    }
    *pResponsiblePID = responsiblePID;
}

/* Get the PID of the thread that the current thread will act in the behalf of */
/* By default : the current thread acts in the behalf of itself */
GIntBig GDALGetResponsiblePIDForCurrentThread()
{
    GIntBig* pResponsiblePID = (GIntBig*) CPLGetTLS(CTLS_RESPONSIBLEPID);
    if (pResponsiblePID == NULL)
        return CPLGetPID();
    return *pResponsiblePID;
}


/************************************************************************/
/* ==================================================================== */
/*                             GDALDataset                              */
/* ==================================================================== */
/************************************************************************/

/**
 * \class GDALDataset "gdal_priv.h"
 *
 * A dataset encapsulating one or more raster bands.  Details are
 * further discussed in the <a href="gdal_datamodel.html#GDALDataset">GDAL
 * Data Model</a>.
 *
 * Use GDALOpen() or GDALOpenShared() to create a GDALDataset for a named file,
 * or GDALDriver::Create() or GDALDriver::CreateCopy() to create a new 
 * dataset.
 */

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
    bShared = FALSE;

/* -------------------------------------------------------------------- */
/*      Add this dataset to the open dataset list.                      */
/* -------------------------------------------------------------------- */
    {
        CPLMutexHolderD( &hDLMutex );

        if (phAllDatasetSet == NULL)
            phAllDatasetSet = CPLHashSetNew(GDALDatasetHashFunc, GDALDatasetEqualFunc, GDALDatasetFreeFunc);
        DatasetCtxt* ctxt = (DatasetCtxt*)CPLMalloc(sizeof(DatasetCtxt));
        ctxt->poDS = this;
        ctxt->nPIDCreatorForShared = -1;
        CPLHashSetInsert(phAllDatasetSet, ctxt);
    }

/* -------------------------------------------------------------------- */
/*      Set forced caching flag.                                        */
/* -------------------------------------------------------------------- */
    bForceCachedIO =  CSLTestBoolean( 
        CPLGetConfigOption( "GDAL_FORCE_CACHING", "NO") );
}


/************************************************************************/
/*                            ~GDALDataset()                            */
/************************************************************************/

/**
 * \brief Destroy an open GDALDataset.
 *
 * This is the accepted method of closing a GDAL dataset and deallocating
 * all resources associated with it.
 *
 * Equivelent of the C callable GDALClose().  Except that GDALClose() first
 * decrements the reference count, and then closes only if it has dropped to
 * zero.
 *
 * For Windows users, it is not recommanded using the delete operator on the
 * dataset object because of known issues when allocating and freeing memory across
 * module boundaries. Calling GDALClose() is then a better option.
 */

GDALDataset::~GDALDataset()

{
    int         i;

    // we don't want to report destruction of datasets that 
    // were never really open.
    if( nBands != 0 || !EQUAL(GetDescription(),"") )
    {
        if( CPLGetPID() != GDALGetResponsiblePIDForCurrentThread() )
            CPLDebug( "GDAL", 
                      "GDALClose(%s, this=%p) (pid=%d, responsiblePID=%d)", GetDescription(), this,
                      (int)CPLGetPID(), 
                      (int)GDALGetResponsiblePIDForCurrentThread() );
        else
            CPLDebug( "GDAL", 
                      "GDALClose(%s, this=%p)", GetDescription(), this );
    }

/* -------------------------------------------------------------------- */
/*      Remove dataset from the "open" dataset list.                    */
/* -------------------------------------------------------------------- */
    {
        CPLMutexHolderD( &hDLMutex );

        DatasetCtxt sStruct;
        sStruct.poDS = this;
        DatasetCtxt* psStruct = (DatasetCtxt*) CPLHashSetLookup(phAllDatasetSet, &sStruct);
        GIntBig nPIDCreatorForShared = psStruct->nPIDCreatorForShared;
        CPLHashSetRemove(phAllDatasetSet, psStruct);

        if (bShared && phSharedDatasetSet != NULL)
        {
            SharedDatasetCtxt* psStruct;
            SharedDatasetCtxt sStruct;
            sStruct.nPID = nPIDCreatorForShared;
            sStruct.eAccess = eAccess;
            sStruct.pszDescription = (char*) GetDescription();
            psStruct = (SharedDatasetCtxt*) CPLHashSetLookup(phSharedDatasetSet, &sStruct);
            if (psStruct && psStruct->poDS == this)
            {
                CPLHashSetRemove(phSharedDatasetSet, psStruct);
            }
            else
            {
                CPLDebug("GDAL", "Should not happen. Cannot find %s, this=%p in phSharedDatasetSet", GetDescription(), this);
            }
        }

        if (CPLHashSetSize(phAllDatasetSet) == 0)
        {
            CPLHashSetDestroy(phAllDatasetSet);
            phAllDatasetSet = NULL;
            if (phSharedDatasetSet)
            {
                CPLHashSetDestroy(phSharedDatasetSet);
            }
            phSharedDatasetSet = NULL;
            CPLFree(ppDatasets);
            ppDatasets = NULL;
        }
    }

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
/*                             FlushCache()                             */
/************************************************************************/

/**
 * \brief Flush all write cached data to disk.
 *
 * Any raster (or other GDAL) data written via GDAL calls, but buffered
 * internally will be written to disk.
 *
 * Using this method does not prevent use from calling GDALClose()
 * to properly close a dataset and ensure that important data not addressed
 * by FlushCache() is written in the file.
 *
 * This method is the same as the C function GDALFlushCache().
 */

void GDALDataset::FlushCache()

{
    int         i;

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
/*                           GDALFlushCache()                           */
/************************************************************************/

/**
 * \brief Flush all write cached data to disk.
 *
 * @see GDALDataset::FlushCache().
 */

void CPL_STDCALL GDALFlushCache( GDALDatasetH hDS )

{
    VALIDATE_POINTER0( hDS, "GDALFlushCache" );

    ((GDALDataset *) hDS)->FlushCache();
}

/************************************************************************/
/*                        BlockBasedFlushCache()                        */
/*                                                                      */
/*      This helper method can be called by the                         */
/*      GDALDataset::FlushCache() for particular drivers to ensure      */
/*      that buffers will be flushed in a manner suitable for pixel     */
/*      interleaved (by block) IO.  That is, if all the bands have      */
/*      the same size blocks then a given block will be flushed for     */
/*      all bands before proceeding to the next block.                  */
/************************************************************************/

void GDALDataset::BlockBasedFlushCache()

{
    GDALRasterBand *poBand1;
    int  nBlockXSize, nBlockYSize, iBand;
    
    poBand1 = GetRasterBand( 1 );
    if( poBand1 == NULL )
    {
        GDALDataset::FlushCache();
        return;
    }

    poBand1->GetBlockSize( &nBlockXSize, &nBlockYSize );
    
/* -------------------------------------------------------------------- */
/*      Verify that all bands match.                                    */
/* -------------------------------------------------------------------- */
    for( iBand = 1; iBand < nBands; iBand++ )
    {
        int nThisBlockXSize, nThisBlockYSize;
        GDALRasterBand *poBand = GetRasterBand( iBand+1 );
        
        poBand->GetBlockSize( &nThisBlockXSize, &nThisBlockYSize );
        if( nThisBlockXSize != nBlockXSize && nThisBlockYSize != nBlockYSize )
        {
            GDALDataset::FlushCache();
            return;
        }
    }

/* -------------------------------------------------------------------- */
/*      Now flush writable data.                                        */
/* -------------------------------------------------------------------- */
    for( int iY = 0; iY < poBand1->nBlocksPerColumn; iY++ )
    {
        for( int iX = 0; iX < poBand1->nBlocksPerRow; iX++ )
        {
            for( iBand = 0; iBand < nBands; iBand++ )
            {
                GDALRasterBand *poBand = GetRasterBand( iBand+1 );
                
                if( poBand->papoBlocks[iX + iY*poBand1->nBlocksPerRow] != NULL)
                {
                    CPLErr    eErr;
                    
                    eErr = poBand->FlushBlock( iX, iY );
                    
                    if( eErr != CE_None )
                        return;
                }
            }
        }
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
/*                              AddBand()                               */
/************************************************************************/

/**
 * \brief Add a band to a dataset.
 *
 * This method will add a new band to the dataset if the underlying format
 * supports this action.  Most formats do not. 
 *
 * Note that the new GDALRasterBand is not returned.  It may be fetched
 * after successful completion of the method by calling 
 * GDALDataset::GetRasterBand(GDALDataset::GetRasterCount()-1) as the newest
 * band will always be the last band.
 *
 * @param eType the data type of the pixels in the new band. 
 *
 * @param papszOptions a list of NAME=VALUE option strings.  The supported
 * options are format specific.  NULL may be passed by default.
 *
 * @return CE_None on success or CE_Failure on failure.  
 */

CPLErr GDALDataset::AddBand( GDALDataType eType, char ** papszOptions )

{
    (void) eType;
    (void) papszOptions;

    CPLError( CE_Failure, CPLE_NotSupported, 
              "Dataset does not support the AddBand() method." );

    return CE_Failure;
}

/************************************************************************/
/*                            GDALAddBand()                             */
/************************************************************************/

/**
 * \brief Add a band to a dataset.
 *
 * @see GDALDataset::AddBand().
 */

CPLErr CPL_STDCALL GDALAddBand( GDALDatasetH hDataset, 
                    GDALDataType eType, char **papszOptions )

{
    VALIDATE_POINTER1( hDataset, "GDALAddBand", CE_Failure );

    return ((GDALDataset *) hDataset)->AddBand( eType, papszOptions );
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
        int             i;
        GDALRasterBand** papoNewBands;

        if( papoBands == NULL )
            papoNewBands = (GDALRasterBand **)
                VSICalloc(sizeof(GDALRasterBand*), MAX(nNewBand,nBands));
        else
            papoNewBands = (GDALRasterBand **)
                VSIRealloc(papoBands, sizeof(GDALRasterBand*) *
                           MAX(nNewBand,nBands));
        if (papoNewBands == NULL)
        {
            CPLError(CE_Failure, CPLE_OutOfMemory,
                     "Cannot allocate band array");
            return;
        }
        papoBands = papoNewBands;

        for( i = nBands; i < nNewBand; i++ )
            papoBands[i] = NULL;

        nBands = MAX(nBands,nNewBand);
    }

/* -------------------------------------------------------------------- */
/*      Set the band.  Resetting the band is currently not permitted.   */
/* -------------------------------------------------------------------- */
    if( papoBands[nNewBand-1] != NULL )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Cannot set band %d as it is already set", nNewBand);
        return;
    }

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

 \brief Fetch raster width in pixels.

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

/**
 * \brief Fetch raster width in pixels.
 *
 * @see GDALDataset::GetRasterXSize().
 */

int CPL_STDCALL GDALGetRasterXSize( GDALDatasetH hDataset )

{
    VALIDATE_POINTER1( hDataset, "GDALGetRasterXSize", 0 );

    return ((GDALDataset *) hDataset)->GetRasterXSize();
}


/************************************************************************/
/*                           GetRasterYSize()                           */
/************************************************************************/

/**

 \brief Fetch raster height in pixels.

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

/**
 * \brief Fetch raster height in pixels.
 *
 * @see GDALDataset::GetRasterYSize().
 */

int CPL_STDCALL GDALGetRasterYSize( GDALDatasetH hDataset )

{
    VALIDATE_POINTER1( hDataset, "GDALGetRasterYSize", 0 );

    return ((GDALDataset *) hDataset)->GetRasterYSize();
}

/************************************************************************/
/*                           GetRasterBand()                            */
/************************************************************************/

/**

 \brief Fetch a band object for a dataset.

 Equivalent of the C function GDALGetRasterBand().

 @param nBandId the index number of the band to fetch, from 1 to
                GetRasterCount().

 @return the nBandId th band object

*/

GDALRasterBand * GDALDataset::GetRasterBand( int nBandId )

{
    if ( papoBands )
    {
        if( nBandId < 1 || nBandId > nBands )
        {
            CPLError( CE_Failure, CPLE_IllegalArg,
                      "GDALDataset::GetRasterBand(%d) - Illegal band #\n",
                      nBandId );
            return NULL;
        }
        else
            return( papoBands[nBandId-1] );
    }
    return NULL;
}

/************************************************************************/
/*                         GDALGetRasterBand()                          */
/************************************************************************/

/**
 * \brief Fetch a band object for a dataset.
 * @see GDALDataset::GetRasterBand().
 */

GDALRasterBandH CPL_STDCALL GDALGetRasterBand( GDALDatasetH hDS, int nBandId )

{
    VALIDATE_POINTER1( hDS, "GDALGetRasterBand", NULL );

    return( (GDALRasterBandH) ((GDALDataset *) hDS)->GetRasterBand(nBandId) );
}

/************************************************************************/
/*                           GetRasterCount()                           */
/************************************************************************/

/**
 * \brief Fetch the number of raster bands on this dataset.
 *
 * Same as the C function GDALGetRasterCount().
 *
 * @return the number of raster bands.
 */

int GDALDataset::GetRasterCount()

{
    return papoBands ? nBands : 0;
}

/************************************************************************/
/*                         GDALGetRasterCount()                         */
/************************************************************************/

/**
 * \brief Fetch the number of raster bands on this dataset.
 *
 * @see GDALDataset::GetRasterCount().
 */

int CPL_STDCALL GDALGetRasterCount( GDALDatasetH hDS )

{
    VALIDATE_POINTER1( hDS, "GDALGetRasterCount", 0 );

    return( ((GDALDataset *) hDS)->GetRasterCount() );
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

/**
 * \brief Fetch the projection definition string for this dataset.
 *
 * Same as the C function GDALGetProjectionRef().
 *
 * The returned string defines the projection coordinate system of the
 * image in OpenGIS WKT format.  It should be suitable for use with the 
 * OGRSpatialReference class.
 *
 * When a projection definition is not available an empty (but not NULL)
 * string is returned.
 *
 * @return a pointer to an internal projection reference string.  It should
 * not be altered, freed or expected to last for long. 
 *
 * @see http://www.gdal.org/ogr/osr_tutorial.html
 */

const char *GDALDataset::GetProjectionRef()

{
    return( "" );
}

/************************************************************************/
/*                        GDALGetProjectionRef()                        */
/************************************************************************/

/**
 * \brief Fetch the projection definition string for this dataset.
 *
 * @see GDALDataset::GetProjectionRef()
 */

const char * CPL_STDCALL GDALGetProjectionRef( GDALDatasetH hDS )

{
    VALIDATE_POINTER1( hDS, "GDALGetProjectionRef", NULL );

    return( ((GDALDataset *) hDS)->GetProjectionRef() );
}

/************************************************************************/
/*                           SetProjection()                            */
/************************************************************************/

/**
 * \brief Set the projection reference string for this dataset.
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
    if( !(GetMOFlags() & GMO_IGNORE_UNIMPLEMENTED) )
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "Dataset does not support the SetProjection() method." );
    return CE_Failure;
}

/************************************************************************/
/*                         GDALSetProjection()                          */
/************************************************************************/

/**
 * \brief Set the projection reference string for this dataset.
 *
 * @see GDALDataset::SetProjection()
 */

CPLErr CPL_STDCALL GDALSetProjection( GDALDatasetH hDS, const char * pszProjection )

{
    VALIDATE_POINTER1( hDS, "GDALSetProjection", CE_Failure );

    return( ((GDALDataset *) hDS)->SetProjection(pszProjection) );
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

/**
 * \brief Fetch the affine transformation coefficients.
 *
 * Fetches the coefficients for transforming between pixel/line (P,L) raster
 * space, and projection coordinates (Xp,Yp) space.
 *
 * \code
 *   Xp = padfTransform[0] + P*padfTransform[1] + L*padfTransform[2];
 *   Yp = padfTransform[3] + P*padfTransform[4] + L*padfTransform[5];
 * \endcode
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
        
    padfTransform[0] = 0.0;     /* X Origin (top left corner) */
    padfTransform[1] = 1.0;     /* X Pixel size */
    padfTransform[2] = 0.0;

    padfTransform[3] = 0.0;     /* Y Origin (top left corner) */
    padfTransform[4] = 0.0;     
    padfTransform[5] = 1.0;     /* Y Pixel Size */

    return( CE_Failure );
}

/************************************************************************/
/*                        GDALGetGeoTransform()                         */
/************************************************************************/

/**
 * \brief Fetch the affine transformation coefficients.
 *
 * @see GDALDataset::GetGeoTransform()
 */

CPLErr CPL_STDCALL GDALGetGeoTransform( GDALDatasetH hDS, double * padfTransform )

{
    VALIDATE_POINTER1( hDS, "GDALGetGeoTransform", CE_Failure );

    return( ((GDALDataset *) hDS)->GetGeoTransform(padfTransform) );
}

/************************************************************************/
/*                          SetGeoTransform()                           */
/************************************************************************/

/**
 * \brief Set the affine transformation coefficients.
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
    if( !(GetMOFlags() & GMO_IGNORE_UNIMPLEMENTED) )
        CPLError( CE_Failure, CPLE_NotSupported,
                  "SetGeoTransform() not supported for this dataset." );
    
    return( CE_Failure );
}

/************************************************************************/
/*                        GDALSetGeoTransform()                         */
/************************************************************************/

/**
 * \brief Set the affine transformation coefficients.
 *
 * @see GDALDataset::SetGeoTransform()
 */

CPLErr CPL_STDCALL 
GDALSetGeoTransform( GDALDatasetH hDS, double * padfTransform )

{
    VALIDATE_POINTER1( hDS, "GDALSetGeoTransform", CE_Failure );

    return( ((GDALDataset *) hDS)->SetGeoTransform(padfTransform) );
}

/************************************************************************/
/*                         GetInternalHandle()                          */
/************************************************************************/

/**
 * \brief Fetch a format specific internally meaningful handle.
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

/**
 * \brief Fetch a format specific internally meaningful handle.
 *
 * @see GDALDataset::GetInternalHandle()
 */

void * CPL_STDCALL 
GDALGetInternalHandle( GDALDatasetH hDS, const char * pszRequest )

{
    VALIDATE_POINTER1( hDS, "GDALGetInternalHandle", NULL );

    return( ((GDALDataset *) hDS)->GetInternalHandle(pszRequest) );
}

/************************************************************************/
/*                             GetDriver()                              */
/************************************************************************/

/**
 * \brief Fetch the driver to which this dataset relates.
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

/**
 * \brief Fetch the driver to which this dataset relates.
 *
 * @see GDALDataset::GetDriver()
 */

GDALDriverH CPL_STDCALL GDALGetDatasetDriver( GDALDatasetH hDataset )

{
    VALIDATE_POINTER1( hDataset, "GDALGetDatasetDriver", NULL );

    return (GDALDriverH) ((GDALDataset *) hDataset)->GetDriver();
}

/************************************************************************/
/*                             Reference()                              */
/************************************************************************/

/**
 * \brief Add one to dataset reference count.
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

/**
 * \brief Add one to dataset reference count.
 *
 * @see GDALDataset::Reference()
 */

int CPL_STDCALL GDALReferenceDataset( GDALDatasetH hDataset )

{
    VALIDATE_POINTER1( hDataset, "GDALReferenceDataset", 0 );

    return ((GDALDataset *) hDataset)->Reference();
}

/************************************************************************/
/*                            Dereference()                             */
/************************************************************************/

/**
 * \brief Subtract one from dataset reference count.
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

/**
 * \brief Subtract one from dataset reference count.
 *
 * @see GDALDataset::Dereference()
 */

int CPL_STDCALL GDALDereferenceDataset( GDALDatasetH hDataset )

{
    VALIDATE_POINTER1( hDataset, "GDALDereferenceDataset", 0 );

    return ((GDALDataset *) hDataset)->Dereference();
}

/************************************************************************/
/*                             GetShared()                              */
/************************************************************************/

/**
 * \brief Returns shared flag.
 *
 * @return TRUE if the GDALDataset is available for sharing, or FALSE if not.
 */

int GDALDataset::GetShared()

{
    return bShared;
}

/************************************************************************/
/*                            MarkAsShared()                            */
/************************************************************************/

/**
 * \brief Mark this dataset as available for sharing.
 */

void GDALDataset::MarkAsShared()

{
    CPLAssert( !bShared );

    bShared = TRUE;

    GIntBig nPID = GDALGetResponsiblePIDForCurrentThread();
    SharedDatasetCtxt* psStruct;

    /* Insert the dataset in the set of shared opened datasets */
    CPLMutexHolderD( &hDLMutex );
    if (phSharedDatasetSet == NULL)
        phSharedDatasetSet = CPLHashSetNew(GDALSharedDatasetHashFunc, GDALSharedDatasetEqualFunc, GDALSharedDatasetFreeFunc);

    psStruct = (SharedDatasetCtxt*)CPLMalloc(sizeof(SharedDatasetCtxt));
    psStruct->poDS = this;
    psStruct->nPID = nPID;
    psStruct->eAccess = eAccess;
    psStruct->pszDescription = CPLStrdup(GetDescription());
    if(CPLHashSetLookup(phSharedDatasetSet, psStruct) != NULL)
    {
        CPLFree(psStruct);
        CPLError(CE_Failure, CPLE_AppDefined,
                 "An existing shared dataset has already this description. This should not happen");
    }
    else
    {
        CPLHashSetInsert(phSharedDatasetSet, psStruct);

        DatasetCtxt sStruct;
        sStruct.poDS = this;
        DatasetCtxt* psStruct = (DatasetCtxt*) CPLHashSetLookup(phAllDatasetSet, &sStruct);
        psStruct->nPIDCreatorForShared = nPID;
    }
}

/************************************************************************/
/*                            GetGCPCount()                             */
/************************************************************************/

/**
 * \brief Get number of GCPs. 
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

/**
 * \brief Get number of GCPs. 
 *
 * @see GDALDataset::GetGCPCount()
 */

int CPL_STDCALL GDALGetGCPCount( GDALDatasetH hDS )

{
    VALIDATE_POINTER1( hDS, "GDALGetGCPCount", 0 );

    return ((GDALDataset *) hDS)->GetGCPCount();
}

/************************************************************************/
/*                          GetGCPProjection()                          */
/************************************************************************/

/**
 * \brief Get output projection for GCPs. 
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

/**
 * \brief Get output projection for GCPs. 
 *
 * @see GDALDataset::GetGCPProjection()
 */

const char * CPL_STDCALL GDALGetGCPProjection( GDALDatasetH hDS )

{
    VALIDATE_POINTER1( hDS, "GDALGetGCPProjection", NULL );

    return ((GDALDataset *) hDS)->GetGCPProjection();
}

/************************************************************************/
/*                               GetGCPs()                              */
/************************************************************************/

/**
 * \brief Fetch GCPs.
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

/**
 * \brief Fetch GCPs.
 *
 * @see GDALDataset::GetGCPs()
 */

const GDAL_GCP * CPL_STDCALL GDALGetGCPs( GDALDatasetH hDS )

{
    VALIDATE_POINTER1( hDS, "GDALGetGCPs", NULL );

    return ((GDALDataset *) hDS)->GetGCPs();
}


/************************************************************************/
/*                              SetGCPs()                               */
/************************************************************************/

/**
 * \brief Assign GCPs.
 *
 * This method is the same as the C function GDALSetGCPs(). 
 *
 * This method assigns the passed set of GCPs to this dataset, as well as
 * setting their coordinate system.  Internally copies are made of the
 * coordinate system and list of points, so the caller remains resposible for
 * deallocating these arguments if appropriate. 
 *
 * Most formats do not support setting of GCPs, even foramts that can 
 * handle GCPs.  These formats will return CE_Failure. 
 *
 * @param nGCPCount number of GCPs being assigned. 
 *
 * @param pasGCPList array of GCP structures being assign (nGCPCount in array).
 *
 * @param pszGCPProjection the new OGC WKT coordinate system to assign for the 
 * GCP output coordinates.  This parameter should be "" if no output coordinate
 * system is known.
 *
 * @return CE_None on success, CE_Failure on failure (including if action is
 * not supported for this format). 
 */ 

CPLErr GDALDataset::SetGCPs( int nGCPCount, 
                             const GDAL_GCP *pasGCPList,
                             const char * pszGCPProjection )

{
    (void) nGCPCount;
    (void) pasGCPList;
    (void) pszGCPProjection;

    if( !(GetMOFlags() & GMO_IGNORE_UNIMPLEMENTED) )
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "Dataset does not support the SetGCPs() method." );

    return CE_Failure;
}

/************************************************************************/
/*                            GDALSetGCPs()                             */
/************************************************************************/

/**
 * \brief Assign GCPs.
 *
 * @see GDALDataset::SetGCPs()
 */

CPLErr CPL_STDCALL GDALSetGCPs( GDALDatasetH hDS, int nGCPCount, 
                    const GDAL_GCP *pasGCPList, 
                    const char *pszGCPProjection )

{
    VALIDATE_POINTER1( hDS, "GDALSetGCPs", CE_Failure );

    return ((GDALDataset *) hDS)->SetGCPs( nGCPCount, pasGCPList, 
                                           pszGCPProjection );
}

/************************************************************************/
/*                           BuildOverviews()                           */
/************************************************************************/

/**
 * \brief Build raster overview(s)
 *
 * If the operation is unsupported for the indicated dataset, then 
 * CE_Failure is returned, and CPLGetLastErrorNo() will return 
 * CPLE_NotSupported.
 *
 * This method is the same as the C function GDALBuildOverviews().
 *
 * @param pszResampling one of "NEAREST", "GAUSS", "CUBIC", "AVERAGE", "MODE",
 * "AVERAGE_MAGPHASE" or "NONE" controlling the downsampling method applied.
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
 *
 * @see GDALRegenerateOverviews()
 */

CPLErr GDALDataset::BuildOverviews( const char *pszResampling, 
                                    int nOverviews, int *panOverviewList, 
                                    int nListBands, int *panBandList,
                                    GDALProgressFunc pfnProgress, 
                                    void * pProgressData )
    
{
    CPLErr   eErr;
    int      *panAllBandList = NULL;

    if( nListBands == 0 )
    {
        nListBands = GetRasterCount();
        panAllBandList = (int *) CPLMalloc(sizeof(int) * nListBands);
        for( int i = 0; i < nListBands; i++ )
            panAllBandList[i] = i+1;

        panBandList = panAllBandList;
    }

    if( pfnProgress == NULL )
        pfnProgress = GDALDummyProgress;

    eErr = IBuildOverviews( pszResampling, nOverviews, panOverviewList, 
                            nListBands, panBandList, pfnProgress, pProgressData );

    if( panAllBandList != NULL )
        CPLFree( panAllBandList );

    return eErr;
}

/************************************************************************/
/*                         GDALBuildOverviews()                         */
/************************************************************************/

/**
 * \brief Build raster overview(s)
 *
 * @see GDALDataset::BuildOverviews()
 */

CPLErr CPL_STDCALL GDALBuildOverviews( GDALDatasetH hDataset,
                           const char *pszResampling, 
                           int nOverviews, int *panOverviewList, 
                           int nListBands, int *panBandList,
                           GDALProgressFunc pfnProgress, 
                           void * pProgressData )

{
    VALIDATE_POINTER1( hDataset, "GDALBuildOverviews", CE_Failure );

    return ((GDALDataset *) hDataset)->BuildOverviews(
        pszResampling, nOverviews, panOverviewList, nListBands, panBandList, 
        pfnProgress, pProgressData );
}
    
/************************************************************************/
/*                          IBuildOverviews()                           */
/*                                                                      */
/*      Default implementation.                                         */
/************************************************************************/

CPLErr GDALDataset::IBuildOverviews( const char *pszResampling, 
                                     int nOverviews, int *panOverviewList, 
                                     int nListBands, int *panBandList,
                                     GDALProgressFunc pfnProgress, 
                                     void * pProgressData )
    
{
    if( oOvManager.IsInitialized() )
        return oOvManager.BuildOverviews( NULL, pszResampling, 
                                          nOverviews, panOverviewList,
                                          nListBands, panBandList,
                                          pfnProgress, pProgressData );
    else
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "BuildOverviews() not supported for this dataset." );
        
        return( CE_Failure );
    }
}

/************************************************************************/
/*                             IRasterIO()                              */
/*                                                                      */
/*      The default implementation of IRasterIO() is to pass the        */
/*      request off to each band objects rasterio methods with          */
/*      appropriate arguments.                                          */
/************************************************************************/

CPLErr GDALDataset::IRasterIO( GDALRWFlag eRWFlag,
                               int nXOff, int nYOff, int nXSize, int nYSize,
                               void * pData, int nBufXSize, int nBufYSize,
                               GDALDataType eBufType, 
                               int nBandCount, int *panBandMap,
                               int nPixelSpace, int nLineSpace, int nBandSpace)
    
{
    int iBandIndex; 
    CPLErr eErr = CE_None;
    const char* pszInterleave = NULL;

    CPLAssert( NULL != pData );

    if (nXSize == nBufXSize && nYSize == nBufYSize &&
        (pszInterleave = GetMetadataItem("INTERLEAVE", "IMAGE_STRUCTURE")) != NULL &&
        EQUAL(pszInterleave, "PIXEL"))
    {
        return BlockBasedRasterIO( eRWFlag, nXOff, nYOff, nXSize, nYSize, 
                                   pData, nBufXSize, nBufYSize,
                                   eBufType, nBandCount, panBandMap,
                                   nPixelSpace, nLineSpace, nBandSpace );
    }

    for( iBandIndex = 0; 
         iBandIndex < nBandCount && eErr == CE_None; 
         iBandIndex++ )
    {
        GDALRasterBand *poBand = GetRasterBand(panBandMap[iBandIndex]);
        GByte *pabyBandData;

        pabyBandData = ((GByte *) pData) + iBandIndex * nBandSpace;
        
        eErr = poBand->IRasterIO( eRWFlag, nXOff, nYOff, nXSize, nYSize, 
                                  (void *) pabyBandData, nBufXSize, nBufYSize,
                                  eBufType, nPixelSpace, nLineSpace );
    }

    return eErr;
}


/************************************************************************/
/*                              RasterIO()                              */
/************************************************************************/

/**
 * \brief Read/write a region of image data from multiple bands.
 *
 * This method allows reading a region of one or more GDALRasterBands from
 * this dataset into a buffer,  or writing data from a buffer into a region 
 * of the GDALRasterBands.  It automatically takes care of data type
 * translation if the data type (eBufType) of the buffer is different than
 * that of the GDALRasterBand.
 * The method also takes care of image decimation / replication if the
 * buffer size (nBufXSize x nBufYSize) is different than the size of the
 * region being accessed (nXSize x nYSize).
 *
 * The nPixelSpace, nLineSpace and nBandSpace parameters allow reading into or
 * writing from various organization of buffers. 
 *
 * For highest performance full resolution data access, read and write
 * on "block boundaries" as returned by GetBlockSize(), or use the
 * ReadBlock() and WriteBlock() methods.
 *
 * This method is the same as the C GDALDatasetRasterIO() function.
 *
 * @param eRWFlag Either GF_Read to read a region of data, or GF_Write to
 * write a region of data.
 *
 * @param nXOff The pixel offset to the top left corner of the region
 * of the band to be accessed.  This would be zero to start from the left side.
 *
 * @param nYOff The line offset to the top left corner of the region
 * of the band to be accessed.  This would be zero to start from the top.
 *
 * @param nXSize The width of the region of the band to be accessed in pixels.
 *
 * @param nYSize The height of the region of the band to be accessed in lines.
 *
 * @param pData The buffer into which the data should be read, or from which
 * it should be written.  This buffer must contain at least
 * nBufXSize * nBufYSize * nBandCount words of type eBufType.  It is organized
 * in left to right,top to bottom pixel order.  Spacing is controlled by the
 * nPixelSpace, and nLineSpace parameters.
 *
 * @param nBufXSize the width of the buffer image into which the desired region
 * is to be read, or from which it is to be written.
 *
 * @param nBufYSize the height of the buffer image into which the desired
 * region is to be read, or from which it is to be written.
 *
 * @param eBufType the type of the pixel values in the pData data buffer. The
 * pixel values will automatically be translated to/from the GDALRasterBand
 * data type as needed.
 *
 * @param nBandCount the number of bands being read or written. 
 *
 * @param panBandMap the list of nBandCount band numbers being read/written.
 * Note band numbers are 1 based. This may be NULL to select the first 
 * nBandCount bands.
 *
 * @param nPixelSpace The byte offset from the start of one pixel value in
 * pData to the start of the next pixel value within a scanline. If defaulted
 * (0) the size of the datatype eBufType is used.
 *
 * @param nLineSpace The byte offset from the start of one scanline in
 * pData to the start of the next. If defaulted (0) the size of the datatype
 * eBufType * nBufXSize is used.
 *
 * @param nBandSpace the byte offset from the start of one bands data to the
 * start of the next. If defaulted (0) the value will be 
 * nLineSpace * nBufYSize implying band sequential organization
 * of the data buffer. 
 *
 * @return CE_Failure if the access fails, otherwise CE_None.
 */

CPLErr GDALDataset::RasterIO( GDALRWFlag eRWFlag,
                              int nXOff, int nYOff, int nXSize, int nYSize,
                              void * pData, int nBufXSize, int nBufYSize,
                              GDALDataType eBufType, 
                              int nBandCount, int *panBandMap,
                              int nPixelSpace, int nLineSpace, int nBandSpace )

{
    int i = 0;
    int bNeedToFreeBandMap = FALSE;
    CPLErr eErr = CE_None;

    if( NULL == pData )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "The buffer into which the data should be read is null" );
            return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Some size values are "noop".  Lets just return to avoid         */
/*      stressing lower level functions.                                */
/* -------------------------------------------------------------------- */
    if( nXSize < 1 || nYSize < 1 || nBufXSize < 1 || nBufYSize < 1 )
    {
        CPLDebug( "GDAL", 
                  "RasterIO() skipped for odd window or buffer size.\n"
                  "  Window = (%d,%d)x%dx%d\n"
                  "  Buffer = %dx%d\n",
                  nXOff, nYOff, nXSize, nYSize, 
                  nBufXSize, nBufYSize );

        return CE_None;
    }

/* -------------------------------------------------------------------- */
/*      If pixel and line spaceing are defaulted assign reasonable      */
/*      value assuming a packed buffer.                                 */
/* -------------------------------------------------------------------- */
    if( nPixelSpace == 0 )
        nPixelSpace = GDALGetDataTypeSize( eBufType ) / 8;
    
    if( nLineSpace == 0 )
    {
        if (nPixelSpace > INT_MAX / nBufXSize)
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "Int overflow : %d x %d", nPixelSpace, nBufXSize );
            return CE_Failure;
        }
        nLineSpace = nPixelSpace * nBufXSize;
    }
    
    if( nBandSpace == 0 )
    {
        if (nLineSpace > INT_MAX / nBufYSize)
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "Int overflow : %d x %d", nLineSpace, nBufYSize );
            return CE_Failure;
        }
        nBandSpace = nLineSpace * nBufYSize;
    }

    if( panBandMap == NULL )
    {
        if (nBandCount > GetRasterCount())
        {
            CPLError( CE_Failure, CPLE_IllegalArg, 
                      "nBandCount cannot be greater than %d",
                      GetRasterCount() );
            return CE_Failure;
        }
        panBandMap = (int *) VSIMalloc2(sizeof(int), nBandCount);
        if (panBandMap == NULL)
        {
            CPLError( CE_Failure, CPLE_OutOfMemory, 
                      "Out of memory while allocating band map array" );
            return CE_Failure;
        }
        for( i = 0; i < nBandCount; i++ )
            panBandMap[i] = i+1;

        bNeedToFreeBandMap = TRUE;
    }
    
/* -------------------------------------------------------------------- */
/*      Do some validation of parameters.                               */
/* -------------------------------------------------------------------- */
    if( nXOff < 0 || nXOff > INT_MAX - nXSize || nXOff + nXSize > nRasterXSize
        || nYOff < 0 || nYOff > INT_MAX - nYSize || nYOff + nYSize > nRasterYSize )
    {
        CPLError( CE_Failure, CPLE_IllegalArg,
                  "Access window out of range in RasterIO().  Requested\n"
                  "(%d,%d) of size %dx%d on raster of %dx%d.",
                  nXOff, nYOff, nXSize, nYSize, nRasterXSize, nRasterYSize );
        eErr = CE_Failure;
    }

    if( eRWFlag != GF_Read && eRWFlag != GF_Write )
    {
        CPLError( CE_Failure, CPLE_IllegalArg,
                  "eRWFlag = %d, only GF_Read (0) and GF_Write (1) are legal.",
                  eRWFlag );
        eErr = CE_Failure;
    }

    for( i = 0; i < nBandCount && eErr == CE_None; i++ )
    {
        if( panBandMap[i] < 1 || panBandMap[i] > GetRasterCount() )
        {
            CPLError( CE_Failure, CPLE_IllegalArg, 
                      "panBandMap[%d] = %d, this band does not exist on dataset.",
                      i, panBandMap[i] );
            eErr = CE_Failure;
        }

        if( eErr == CE_None && GetRasterBand( panBandMap[i] ) == NULL )
        {
            CPLError( CE_Failure, CPLE_IllegalArg, 
                      "panBandMap[%d]=%d, this band should exist but is NULL!",
                      i, panBandMap[i] );
            eErr = CE_Failure;
        }
    }

/* -------------------------------------------------------------------- */
/*      We are being forced to use cached IO instead of a driver        */
/*      specific implementation.                                        */
/* -------------------------------------------------------------------- */
    if( bForceCachedIO )
    {
        eErr = 
            BlockBasedRasterIO( eRWFlag, nXOff, nYOff, nXSize, nYSize,
                                pData, nBufXSize, nBufYSize, eBufType,
                                nBandCount, panBandMap,
                                nPixelSpace, nLineSpace, nBandSpace );
    }

/* -------------------------------------------------------------------- */
/*      Call the format specific function.                              */
/* -------------------------------------------------------------------- */
    else if( eErr == CE_None )
    {
        eErr = 
            IRasterIO( eRWFlag, nXOff, nYOff, nXSize, nYSize,
                       pData, nBufXSize, nBufYSize, eBufType,
                       nBandCount, panBandMap,
                       nPixelSpace, nLineSpace, nBandSpace );
    }

/* -------------------------------------------------------------------- */
/*      Cleanup                                                         */
/* -------------------------------------------------------------------- */
    if( bNeedToFreeBandMap )
        CPLFree( panBandMap );

    return eErr;
}

/************************************************************************/
/*                        GDALDatasetRasterIO()                         */
/************************************************************************/

/**
 * \brief Read/write a region of image data from multiple bands.
 *
 * @see GDALDataset::RasterIO()
 */

CPLErr CPL_STDCALL 							
GDALDatasetRasterIO( GDALDatasetH hDS, GDALRWFlag eRWFlag,
                     int nXOff, int nYOff, int nXSize, int nYSize,
                     void * pData, int nBufXSize, int nBufYSize,
                     GDALDataType eBufType,
                     int nBandCount, int *panBandMap,
                     int nPixelSpace, int nLineSpace, int nBandSpace )
    
{
    VALIDATE_POINTER1( hDS, "GDALDatasetRasterIO", CE_Failure );

    GDALDataset    *poDS = (GDALDataset *) hDS;
    
    return( poDS->RasterIO( eRWFlag, nXOff, nYOff, nXSize, nYSize,
                            pData, nBufXSize, nBufYSize, eBufType,
                            nBandCount, panBandMap, 
                            nPixelSpace, nLineSpace, nBandSpace ) );
}
                     
/************************************************************************/
/*                          GetOpenDatasets()                           */
/************************************************************************/

static int GDALGetOpenDatasetsForeach(void* elt, void* user_data)
{
    int* pnIndex = (int*) user_data;
    ppDatasets[*pnIndex] = (GDALDataset*) elt;

    (*pnIndex) ++;

    return TRUE;
}

/**
 * \brief Fetch all open GDAL dataset handles.
 *
 * This method is the same as the C function GDALGetOpenDatasets().
 *
 * NOTE: This method is not thread safe.  The returned list may changed
 * at any time.
 *
 * @param pnCount integer into which to place the count of dataset pointers
 * being returned.
 *
 * @return a pointer to an array of dataset handles. 
 */

GDALDataset **GDALDataset::GetOpenDatasets( int *pnCount )

{
    CPLMutexHolderD( &hDLMutex );

    if (phAllDatasetSet != NULL)
    {
        int nIndex = 0;
        *pnCount = CPLHashSetSize(phAllDatasetSet);
        ppDatasets = (GDALDataset**) CPLRealloc(ppDatasets, (*pnCount) * sizeof(GDALDataset*));
        CPLHashSetForeach(phAllDatasetSet, GDALGetOpenDatasetsForeach, &nIndex);
        return ppDatasets;
    }
    else
    {
        *pnCount = 0;
        return NULL;
    }
}

/************************************************************************/
/*                        GDALGetOpenDatasets()                         */
/************************************************************************/

/**
 * \brief Fetch all open GDAL dataset handles.
 *
 * @see GDALDataset::GetOpenDatasets()
 */

void CPL_STDCALL GDALGetOpenDatasets( GDALDatasetH **ppahDSList, int *pnCount )

{
    VALIDATE_POINTER0( ppahDSList, "GDALGetOpenDatasets" );
    VALIDATE_POINTER0( pnCount, "GDALGetOpenDatasets" );

    *ppahDSList = (GDALDatasetH *) GDALDataset::GetOpenDatasets( pnCount);
}

/************************************************************************/
/*                             GDALGetAccess()                          */
/************************************************************************/

/**
 * \brief Return access flag
 *
 * @see GDALDataset::GetAccess()
 */

int CPL_STDCALL GDALGetAccess( GDALDatasetH hDS )
{
    VALIDATE_POINTER1( hDS, "GDALGetAccess", 0 );

    return ((GDALDataset *) hDS)->GetAccess();
}

/************************************************************************/
/*                             AdviseRead()                             */
/************************************************************************/

/**
 * \brief Advise driver of upcoming read requests.
 *
 * Some GDAL drivers operate more efficiently if they know in advance what 
 * set of upcoming read requests will be made.  The AdviseRead() method allows
 * an application to notify the driver of the region and bands of interest, 
 * and at what resolution the region will be read.  
 *
 * Many drivers just ignore the AdviseRead() call, but it can dramatically
 * accelerate access via some drivers.  
 *
 * @param nXOff The pixel offset to the top left corner of the region
 * of the band to be accessed.  This would be zero to start from the left side.
 *
 * @param nYOff The line offset to the top left corner of the region
 * of the band to be accessed.  This would be zero to start from the top.
 *
 * @param nXSize The width of the region of the band to be accessed in pixels.
 *
 * @param nYSize The height of the region of the band to be accessed in lines.
 *
 * @param nBufXSize the width of the buffer image into which the desired region
 * is to be read, or from which it is to be written.
 *
 * @param nBufYSize the height of the buffer image into which the desired
 * region is to be read, or from which it is to be written.
 *
 * @param eBufType the type of the pixel values in the pData data buffer.  The
 * pixel values will automatically be translated to/from the GDALRasterBand
 * data type as needed.
 *
 * @param nBandCount the number of bands being read or written. 
 *
 * @param panBandMap the list of nBandCount band numbers being read/written.
 * Note band numbers are 1 based.   This may be NULL to select the first 
 * nBandCount bands.
 *
 * @param papszOptions a list of name=value strings with special control 
 * options.  Normally this is NULL.
 *
 * @return CE_Failure if the request is invalid and CE_None if it works or
 * is ignored. 
 */

CPLErr GDALDataset::AdviseRead( int nXOff, int nYOff, int nXSize, int nYSize,
                                int nBufXSize, int nBufYSize, 
                                GDALDataType eDT, 
                                int nBandCount, int *panBandMap,
                                char **papszOptions )

{
    int iBand;

    for( iBand = 0; iBand < nBandCount; iBand++ )
    {
        CPLErr eErr;
        GDALRasterBand *poBand;

        if( panBandMap == NULL )
            poBand = GetRasterBand(iBand+1);
        else
            poBand = GetRasterBand(panBandMap[iBand]);

        eErr = poBand->AdviseRead( nXOff, nYOff, nXSize, nYSize,
                                   nBufXSize, nBufYSize, eDT, papszOptions );

        if( eErr != CE_None )
            return eErr;
    }
    
    return CE_None;
}

/************************************************************************/
/*                       GDALDatasetAdviseRead()                        */
/************************************************************************/

/**
 * \brief Advise driver of upcoming read requests.
 *
 * @see GDALDataset::AdviseRead()
 */
CPLErr CPL_STDCALL 
GDALDatasetAdviseRead( GDALDatasetH hDS, 
                       int nXOff, int nYOff, int nXSize, int nYSize,
                       int nBufXSize, int nBufYSize, GDALDataType eDT, 
                       int nBandCount, int *panBandMap,char **papszOptions )
    
{
    VALIDATE_POINTER1( hDS, "GDALDatasetAdviseRead", CE_Failure );

    return ((GDALDataset *) hDS)->AdviseRead( nXOff, nYOff, nXSize, nYSize, 
                                              nBufXSize, nBufYSize, eDT, 
                                              nBandCount, panBandMap, 
                                              papszOptions );
}

/************************************************************************/
/*                            GetFileList()                             */
/************************************************************************/

/**
 * \brief Fetch files forming dataset.
 *
 * Returns a list of files believed to be part of this dataset.  If it returns
 * an empty list of files it means there is believed to be no local file
 * system files associated with the dataset (for instance a virtual dataset).
 * The returned file list is owned by the caller and should be deallocated
 * with CSLDestroy().
 * 
 * The returned filenames will normally be relative or absolute paths 
 * depending on the path used to originally open the dataset.
 *
 * This method is the same as the C GDALGetFileList() function.
 *
 * @return NULL or a NULL terminated array of file names. 
 */

char **GDALDataset::GetFileList()

{
    CPLString osMainFilename = GetDescription();
    int bMainFileReal;
    VSIStatBufL  sStat;

/* -------------------------------------------------------------------- */
/*      Is the main filename even a real filesystem object?             */
/* -------------------------------------------------------------------- */
    bMainFileReal = VSIStatL( osMainFilename, &sStat ) == 0;

/* -------------------------------------------------------------------- */
/*      Form new list.                                                  */
/* -------------------------------------------------------------------- */
    char **papszList = NULL;

    if( bMainFileReal )
        papszList = CSLAddString( papszList, osMainFilename );

/* -------------------------------------------------------------------- */
/*      Do we have a known overview file?                               */
/* -------------------------------------------------------------------- */
    if( oOvManager.IsInitialized() && oOvManager.poODS != NULL )
    {
        char **papszOvrList = oOvManager.poODS->GetFileList();
        papszList = CSLInsertStrings( papszList, -1, papszOvrList );
        CSLDestroy( papszOvrList );
    }

/* -------------------------------------------------------------------- */
/*      Do we have a known overview file?                               */
/* -------------------------------------------------------------------- */
    if( oOvManager.HaveMaskFile() )
    {
        char **papszMskList = oOvManager.poMaskDS->GetFileList();
        papszList = CSLInsertStrings( papszList, -1, papszMskList );
        CSLDestroy( papszMskList );
    }

/* -------------------------------------------------------------------- */
/*      Do we have a world file?                                        */
/* -------------------------------------------------------------------- */
    if( bMainFileReal )
    {
        const char* pszExtension = CPLGetExtension( osMainFilename );
        if( strlen(pszExtension) > 2 )
        {
            // first + last + 'w'
            char szDerivedExtension[4];
            szDerivedExtension[0] = pszExtension[0];
            szDerivedExtension[1] = pszExtension[strlen(pszExtension)-1];
            szDerivedExtension[2] = 'w';
            szDerivedExtension[3] = '\0';
            CPLString osWorldFilename = CPLResetExtension( osMainFilename, szDerivedExtension );
            
            if( VSIStatL( osWorldFilename, &sStat ) == 0 )
                papszList = CSLAddString( papszList, osWorldFilename );
        }
    }

    return papszList;
}

/************************************************************************/
/*                          GDALGetFileList()                           */
/************************************************************************/

/**
 * \brief Fetch files forming dataset.
 *
 * @see GDALDataset::GetFileList()
 */

char ** CPL_STDCALL GDALGetFileList( GDALDatasetH hDS )

{
    VALIDATE_POINTER1( hDS, "GDALGetFileList", NULL );

    return ((GDALDataset *) hDS)->GetFileList();
}

/************************************************************************/
/*                           CreateMaskBand()                           */
/************************************************************************/

/**
 * \brief Adds a mask band to the dataset
 *
 * The default implementation of the CreateMaskBand() method is implemented
 * based on similar rules to the .ovr handling implemented using the
 * GDALDefaultOverviews object. A TIFF file with the extension .msk will
 * be created with the same basename as the original file, and it will have
 * one band.
 * The mask images will be deflate compressed tiled images with the same
 * block size as the original image if possible.
 *
 * @since GDAL 1.5.0
 *
 * @param nFlags ignored. GMF_PER_DATASET will be assumed.
 * @return CE_None on success or CE_Failure on an error.
 *
 * @see http://trac.osgeo.org/gdal/wiki/rfc15_nodatabitmask
 *
 */
CPLErr GDALDataset::CreateMaskBand( int nFlags )

{
    if( oOvManager.IsInitialized() )
        return oOvManager.CreateMaskBand( nFlags, -1 );

    CPLError( CE_Failure, CPLE_NotSupported,
              "CreateMaskBand() not supported for this dataset." );
    
    return( CE_Failure );
}

/************************************************************************/
/*                     GDALCreateDatasetMaskBand()                      */
/************************************************************************/

/**
 * \brief Adds a mask band to the dataset
 * @see GDALDataset::CreateMaskBand()
 */
CPLErr CPL_STDCALL GDALCreateDatasetMaskBand( GDALDatasetH hDS, int nFlags )

{
    VALIDATE_POINTER1( hDS, "GDALCreateDatasetMaskBand", CE_Failure );

    return ((GDALDataset *) hDS)->CreateMaskBand( nFlags );
}

/************************************************************************/
/*                              GDALOpen()                              */
/************************************************************************/

/**
 * \brief Open a raster file as a GDALDataset.
 *
 * This function will try to open the passed file, or virtual dataset
 * name by invoking the Open method of each registered GDALDriver in turn. 
 * The first successful open will result in a returned dataset.  If all
 * drivers fail then NULL is returned.
 *
 * Several recommandations :
 * <ul>
 * <li>If you open a dataset object with GA_Update access, it is not recommanded
 * to open a new dataset on the same underlying file.</li>
 * <li>The returned dataset should only be accessed by one thread at a time. If you
 * want to use it from different threads, you must add all necessary code (mutexes, etc.)
 * to avoid concurrent use of the object. (Some drivers, such as GeoTIFF, maintain internal
 * state variables that are updated each time a new block is read, thus preventing concurrent
 * use.) </li>
 * </ul>
 *
 * \sa GDALOpenShared()
 *
 * @param pszFilename the name of the file to access.  In the case of
 * exotic drivers this may not refer to a physical file, but instead contain
 * information for the driver on how to access a dataset.
 *
 * @param eAccess the desired access, either GA_Update or GA_ReadOnly.  Many
 * drivers support only read only access.
 *
 * @return A GDALDatasetH handle or NULL on failure.  For C++ applications
 * this handle can be cast to a GDALDataset *. 
 */

GDALDatasetH CPL_STDCALL 
GDALOpen( const char * pszFilename, GDALAccess eAccess )

{
    VALIDATE_POINTER1( pszFilename, "GDALOpen", NULL );
        
    int         iDriver;
    GDALDriverManager *poDM = GetGDALDriverManager();
    GDALOpenInfo oOpenInfo( pszFilename, eAccess );
    CPLLocaleC  oLocaleForcer;

    CPLErrorReset();
    CPLAssert( NULL != poDM );

    for( iDriver = 0; iDriver < poDM->GetDriverCount(); iDriver++ )
    {
        GDALDriver      *poDriver = poDM->GetDriver( iDriver );
        GDALDataset     *poDS;

        if ( poDriver->pfnOpen == NULL )
            continue;

        poDS = poDriver->pfnOpen( &oOpenInfo );
        if( poDS != NULL )
        {
            if( strlen(poDS->GetDescription()) == 0 )
                poDS->SetDescription( pszFilename );

            if( poDS->poDriver == NULL )
                poDS->poDriver = poDriver;

            
            if( CPLGetPID() != GDALGetResponsiblePIDForCurrentThread() )
                CPLDebug( "GDAL", "GDALOpen(%s, this=%p) succeeds as %s (pid=%d, responsiblePID=%d).",
                          pszFilename, poDS, poDriver->GetDescription(),
                          (int)CPLGetPID(), (int)GDALGetResponsiblePIDForCurrentThread() );
            else
                CPLDebug( "GDAL", "GDALOpen(%s, this=%p) succeeds as %s.",
                          pszFilename, poDS, poDriver->GetDescription() );

            return (GDALDatasetH) poDS;
        }

        if( CPLGetLastErrorNo() != 0 )
            return NULL;
    }

    if( oOpenInfo.bStatOK )
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "`%s' not recognised as a supported file format.\n",
                  pszFilename );
    else
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "`%s' does not exist in the file system,\n"
                  "and is not recognised as a supported dataset name.\n",
                  pszFilename );
              
    return NULL;
}

/************************************************************************/
/*                           GDALOpenShared()                           */
/************************************************************************/

/**
 * \brief Open a raster file as a GDALDataset.
 *
 * This function works the same as GDALOpen(), but allows the sharing of
 * GDALDataset handles for a dataset with other callers to GDALOpenShared().
 * 
 * In particular, GDALOpenShared() will first consult it's list of currently
 * open and shared GDALDataset's, and if the GetDescription() name for one
 * exactly matches the pszFilename passed to GDALOpenShared() it will be
 * referenced and returned.
 *
 * Starting with GDAL 1.6.0, if GDALOpenShared() is called on the same pszFilename
 * from two different threads, a different GDALDataset object will be returned as
 * it is not safe to use the same dataset from different threads, unless the user
 * does explicitely use mutexes in its code.
 *
 * \sa GDALOpen()
 *
 * @param pszFilename the name of the file to access.  In the case of
 * exotic drivers this may not refer to a physical file, but instead contain
 * information for the driver on how to access a dataset.
 *
 * @param eAccess the desired access, either GA_Update or GA_ReadOnly.  Many
 * drivers support only read only access.
 *
 * @return A GDALDatasetH handle or NULL on failure.  For C++ applications
 * this handle can be cast to a GDALDataset *. 
 */
 
GDALDatasetH CPL_STDCALL 
GDALOpenShared( const char *pszFilename, GDALAccess eAccess )

{
    VALIDATE_POINTER1( pszFilename, "GDALOpenShared", NULL );

/* -------------------------------------------------------------------- */
/*      First scan the existing list to see if it could already         */
/*      contain the requested dataset.                                  */
/* -------------------------------------------------------------------- */
    {
        CPLMutexHolderD( &hDLMutex );

        if (phSharedDatasetSet != NULL)
        {
            GIntBig nThisPID = GDALGetResponsiblePIDForCurrentThread();
            SharedDatasetCtxt* psStruct;
            SharedDatasetCtxt sStruct;

            sStruct.nPID = nThisPID;
            sStruct.pszDescription = (char*) pszFilename;
            sStruct.eAccess = eAccess;
            psStruct = (SharedDatasetCtxt*) CPLHashSetLookup(phSharedDatasetSet, &sStruct);
            if (psStruct == NULL && eAccess == GA_ReadOnly)
            {
                sStruct.eAccess = GA_Update;
                psStruct = (SharedDatasetCtxt*) CPLHashSetLookup(phSharedDatasetSet, &sStruct);
            }
            if (psStruct)
            {
                psStruct->poDS->Reference();
                return psStruct->poDS;
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Try opening the the requested dataset.                          */
/* -------------------------------------------------------------------- */
    GDALDataset *poDataset;

    poDataset = (GDALDataset *) GDALOpen( pszFilename, eAccess );
    if( poDataset != NULL )
    {
        if (strcmp(pszFilename, poDataset->GetDescription()) != 0)
        {
            CPLError(CE_Warning, CPLE_NotSupported,
                     "A dataset opened by GDALOpenShared should have the same filename (%s) "
                     "and description (%s)",
                     pszFilename, poDataset->GetDescription());
        }
        else
        {
            poDataset->MarkAsShared();
        }
    }
    
    return (GDALDatasetH) poDataset;
}

/************************************************************************/
/*                             GDALClose()                              */
/************************************************************************/

/**
 * \brief Close GDAL dataset. 
 *
 * For non-shared datasets (opened with GDALOpen()) the dataset is closed
 * using the C++ "delete" operator, recovering all dataset related resources.  
 * For shared datasets (opened with GDALOpenShared()) the dataset is 
 * dereferenced, and closed only if the referenced count has dropped below 1.
 *
 * @param hDS The dataset to close.  May be cast from a "GDALDataset *". 
 */

void CPL_STDCALL GDALClose( GDALDatasetH hDS )

{
    VALIDATE_POINTER0( hDS, "GDALClose" );

    GDALDataset *poDS = (GDALDataset *) hDS;
    CPLMutexHolderD( &hDLMutex );
    CPLLocaleC  oLocaleForcer;

    if (poDS->GetShared())
    {
/* -------------------------------------------------------------------- */
/*      If this file is in the shared dataset list then dereference     */
/*      it, and only delete/remote it if the reference count has        */
/*      dropped to zero.                                                */
/* -------------------------------------------------------------------- */
        if( poDS->Dereference() > 0 )
            return;

        delete poDS;
        return;
    }

/* -------------------------------------------------------------------- */
/*      This is not shared dataset, so directly delete it.              */
/* -------------------------------------------------------------------- */
    delete poDS;
}

/************************************************************************/
/*                        GDALDumpOpenDataset()                         */
/************************************************************************/

static int GDALDumpOpenSharedDatasetsForeach(void* elt, void* user_data)
{
    SharedDatasetCtxt* psStruct = (SharedDatasetCtxt*) elt;
    FILE *fp = (FILE*) user_data;
    const char *pszDriverName;
    GDALDataset *poDS = psStruct->poDS;

    if( poDS->GetDriver() == NULL )
        pszDriverName = "DriverIsNULL";
    else
        pszDriverName = poDS->GetDriver()->GetDescription();

    poDS->Reference();
    VSIFPrintf( fp, "  %d %c %-6s %7d %dx%dx%d %s\n", 
                poDS->Dereference(), 
                poDS->GetShared() ? 'S' : 'N',
                pszDriverName, 
                (int)psStruct->nPID,
                poDS->GetRasterXSize(),
                poDS->GetRasterYSize(),
                poDS->GetRasterCount(),
                poDS->GetDescription() );

    return TRUE;
}


static int GDALDumpOpenDatasetsForeach(void* elt, void* user_data)
{
    DatasetCtxt* psStruct = (DatasetCtxt*) elt;
    FILE *fp = (FILE*) user_data;
    const char *pszDriverName;
    GDALDataset *poDS = psStruct->poDS;

    /* Don't list shared datasets. They have already been listed by */
    /* GDALDumpOpenSharedDatasetsForeach */
    if (poDS->GetShared())
        return TRUE;

    if( poDS->GetDriver() == NULL )
        pszDriverName = "DriverIsNULL";
    else
        pszDriverName = poDS->GetDriver()->GetDescription();

    poDS->Reference();
    VSIFPrintf( fp, "  %d %c %-6s %7d %dx%dx%d %s\n", 
                poDS->Dereference(), 
                poDS->GetShared() ? 'S' : 'N',
                pszDriverName, 
                -1,
                poDS->GetRasterXSize(),
                poDS->GetRasterYSize(),
                poDS->GetRasterCount(),
                poDS->GetDescription() );

    return TRUE;
}

/**
 * \brief List open datasets.
 *
 * Dumps a list of all open datasets (shared or not) to the indicated 
 * text file (may be stdout or stderr).   This function is primariliy intended
 * to assist in debugging "dataset leaks" and reference counting issues. 
 * The information reported includes the dataset name, referenced count, 
 * shared status, driver name, size, and band count. 
 */

int CPL_STDCALL GDALDumpOpenDatasets( FILE *fp )
   
{
    VALIDATE_POINTER1( fp, "GDALDumpOpenDatasets", 0 );

    CPLMutexHolderD( &hDLMutex );

    if (phAllDatasetSet != NULL)
    {
        VSIFPrintf( fp, "Open GDAL Datasets:\n" );
        CPLHashSetForeach(phAllDatasetSet, GDALDumpOpenDatasetsForeach, fp);
        if (phSharedDatasetSet != NULL)
        {
            CPLHashSetForeach(phSharedDatasetSet, GDALDumpOpenSharedDatasetsForeach, fp);
        }
        return CPLHashSetSize(phAllDatasetSet);
    }
    else
    {
        return 0;
    }
}

