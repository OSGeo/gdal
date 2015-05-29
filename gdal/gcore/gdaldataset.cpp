/******************************************************************************
 * $Id$
 *
 * Project:  GDAL Core
 * Purpose:  Base class for raster file formats.  
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1998, 2003, Frank Warmerdam
 * Copyright (c) 2008-2013, Even Rouault <even dot rouault at mines-paris dot org>
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
#include "ogr_featurestyle.h"
#include "swq.h"
#include "ogr_gensql.h"
#include "ogr_attrind.h"
#include "ogr_p.h"
#include "ogrunionlayer.h"
#include "ograpispy.h"

#ifdef SQLITE_ENABLED
#include "../sqlite/ogrsqliteexecutesql.h"
#endif

#include <map>

CPL_CVSID("$Id$");

CPL_C_START
GDALAsyncReader *
GDALGetDefaultAsyncReader( GDALDataset *poDS,
                             int nXOff, int nYOff,
                             int nXSize, int nYSize,
                             void *pBuf,
                             int nBufXSize, int nBufYSize,
                             GDALDataType eBufType,
                             int nBandCount, int* panBandMap,
                             int nPixelSpace, int nLineSpace,
                             int nBandSpace, char **papszOptions);
CPL_C_END

typedef struct
{
    /* PID of the thread that mark the dataset as shared */
    /* This may not be the actual PID, but the responsiblePID */
    GIntBig      nPID;
    char        *pszDescription;
    GDALAccess   eAccess;

    GDALDataset *poDS;
} SharedDatasetCtxt;

/* Set of datasets opened as shared datasets (with GDALOpenShared) */
/* The values in the set are of type SharedDatasetCtxt */
static CPLHashSet* phSharedDatasetSet = NULL; 

/* Set of all datasets created in the constructor of GDALDataset */
/* In the case of a shared dataset, memorize the PID of the thread */
/* that marked the dataset as shared, so that we can remove it from */
/* the phSharedDatasetSet in the destructor of the dataset, even */
/* if GDALClose is called from a different thread */
static std::map<GDALDataset*, GIntBig>* poAllDatasetMap = NULL;

static CPLMutex *hDLMutex = NULL;

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

/************************************************************************/
/* Functions shared between gdalproxypool.cpp and gdaldataset.cpp */
/************************************************************************/

/* The open-shared mutex must be used by the ProxyPool too */
CPLMutex** GDALGetphDLMutex()
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
    bIsInternal = TRUE;
    bSuppressOnClose = FALSE;
    bReserved1 = FALSE;
    bReserved2 = FALSE;
    papszOpenOptions = NULL;

/* -------------------------------------------------------------------- */
/*      Set forced caching flag.                                        */
/* -------------------------------------------------------------------- */
    bForceCachedIO =  CSLTestBoolean( 
        CPLGetConfigOption( "GDAL_FORCE_CACHING", "NO") );
    
    m_poStyleTable = NULL;
    m_hMutex = NULL;
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
 * Equivalent of the C callable GDALClose().  Except that GDALClose() first
 * decrements the reference count, and then closes only if it has dropped to
 * zero.
 *
 * For Windows users, it is not recommended to use the delete operator on the
 * dataset object because of known issues when allocating and freeing memory across
 * module boundaries. Calling GDALClose() is then a better option.
 */

GDALDataset::~GDALDataset()

{
    int         i;

    // we don't want to report destruction of datasets that 
    // were never really open or meant as internal
    if( !bIsInternal && ( nBands != 0 || !EQUAL(GetDescription(),"") ) )
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

    if( bSuppressOnClose )
        VSIUnlink(GetDescription());

/* -------------------------------------------------------------------- */
/*      Remove dataset from the "open" dataset list.                    */
/* -------------------------------------------------------------------- */
    if( !bIsInternal )
    {
        CPLMutexHolderD( &hDLMutex );
        if( poAllDatasetMap )
        {
            std::map<GDALDataset*, GIntBig>::iterator oIter = poAllDatasetMap->find(this);
            CPLAssert(oIter != poAllDatasetMap->end());
            GIntBig nPIDCreatorForShared = oIter->second;
            poAllDatasetMap->erase(oIter);

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

            if (poAllDatasetMap->size() == 0)
            {
                delete poAllDatasetMap;
                poAllDatasetMap = NULL;
                if (phSharedDatasetSet)
                {
                    CPLHashSetDestroy(phSharedDatasetSet);
                }
                phSharedDatasetSet = NULL;
                CPLFree(ppDatasets);
                ppDatasets = NULL;
            }
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

    if ( m_poStyleTable )
    {
        delete m_poStyleTable;
        m_poStyleTable = NULL;
    }

    if( m_hMutex != NULL )
        CPLDestroyMutex( m_hMutex );

    CSLDestroy( papszOpenOptions );
}

/************************************************************************/
/*                      AddToDatasetOpenList()                          */
/************************************************************************/

void GDALDataset::AddToDatasetOpenList()
{
/* -------------------------------------------------------------------- */
/*      Add this dataset to the open dataset list.                      */
/* -------------------------------------------------------------------- */
    bIsInternal = FALSE;

    CPLMutexHolderD( &hDLMutex );

    if (poAllDatasetMap == NULL)
        poAllDatasetMap = new std::map<GDALDataset*, GIntBig>;
    (*poAllDatasetMap)[this] = -1;
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
 * The default implementation of this method just calls the FlushCache() method
 * on each of the raster bands and the SyncToDisk() method
 * on each of the layers.  Conceptionally, calling FlushCache() on a dataset
 * should include any work that might be accomplished by calling SyncToDisk()
 * on layers in that dataset.
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

    if( papoBands != NULL )
    {
        for( i = 0; i < nBands; i++ )
        {
            if( papoBands[i] != NULL )
                papoBands[i]->FlushCache();
        }
    }

    int nLayers = GetLayerCount();
    if( nLayers > 0 )
    {
        CPLMutexHolderD( &m_hMutex );
        for( i = 0; i < nLayers ; i++ )
        {
            OGRLayer *poLayer = GetLayer(i);

            if( poLayer )
            {
                poLayer->SyncToDisk();
            }
        }
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

                CPLErr    eErr;

                eErr = poBand->FlushBlock( iX, iY );

                if( eErr != CE_None )
                    return;
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
 * GDALDataset::GetRasterBand(GDALDataset::GetRasterCount()) as the newest
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

    ReportError( CE_Failure, CPLE_NotSupported,
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
            ReportError(CE_Failure, CPLE_OutOfMemory,
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
        ReportError(CE_Failure, CPLE_NotSupported,
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

 Equivalent of the C function GDALGetRasterXSize().

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

 Equivalent of the C function GDALGetRasterYSize().

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
            ReportError( CE_Failure, CPLE_IllegalArg,
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

CPLErr GDALDataset::SetProjection( CPL_UNUSED const char * pszProjection )
{
    if( !(GetMOFlags() & GMO_IGNORE_UNIMPLEMENTED) )
        ReportError( CE_Failure, CPLE_NotSupported,
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

CPLErr GDALDataset::SetGeoTransform( CPL_UNUSED double * padfTransform )

{
    if( !(GetMOFlags() & GMO_IGNORE_UNIMPLEMENTED) )
        ReportError( CE_Failure, CPLE_NotSupported,
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

void *GDALDataset::GetInternalHandle( CPL_UNUSED const char * pszHandleName )

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
    if( bIsInternal )
        return;

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
        ReportError(CE_Failure, CPLE_AppDefined,
                 "An existing shared dataset already has this description. This should not happen.");
    }
    else
    {
        CPLHashSetInsert(phSharedDatasetSet, psStruct);

        (*poAllDatasetMap)[this] = nPID;
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
 *  It should not be altered, freed or expected to last for long.
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
 * coordinate system and list of points, so the caller remains responsible for
 * deallocating these arguments if appropriate. 
 *
 * Most formats do not support setting of GCPs, even formats that can 
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
        ReportError( CE_Failure, CPLE_NotSupported,
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
 * @param nListBands number of bands to build overviews for in panBandList.  Build
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
        ReportError( CE_Failure, CPLE_NotSupported,
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
                               GSpacing nPixelSpace, GSpacing nLineSpace,
                               GSpacing nBandSpace,
                               GDALRasterIOExtraArg* psExtraArg )
    
{
    int iBandIndex; 
    CPLErr eErr = CE_None;
    const char* pszInterleave = NULL;

    CPLAssert( NULL != pData );

    if (nXSize == nBufXSize && nYSize == nBufYSize && nBandCount > 1 &&
        (pszInterleave = GetMetadataItem("INTERLEAVE", "IMAGE_STRUCTURE")) != NULL &&
        EQUAL(pszInterleave, "PIXEL"))
    {
        return BlockBasedRasterIO( eRWFlag, nXOff, nYOff, nXSize, nYSize, 
                                   pData, nBufXSize, nBufYSize,
                                   eBufType, nBandCount, panBandMap,
                                   nPixelSpace, nLineSpace, nBandSpace,
                                   psExtraArg );
    }

    GDALProgressFunc  pfnProgressGlobal = psExtraArg->pfnProgress;
    void             *pProgressDataGlobal = psExtraArg->pProgressData;

    for( iBandIndex = 0; 
         iBandIndex < nBandCount && eErr == CE_None; 
         iBandIndex++ )
    {
        GDALRasterBand *poBand = GetRasterBand(panBandMap[iBandIndex]);
        GByte *pabyBandData;

        if (poBand == NULL)
        {
            eErr = CE_Failure;
            break;
        }

        pabyBandData = ((GByte *) pData) + iBandIndex * nBandSpace;

        if( nBandCount > 1 )
        {
            psExtraArg->pfnProgress = GDALScaledProgress;
            psExtraArg->pProgressData = 
                GDALCreateScaledProgress( 1.0 * iBandIndex / nBandCount,
                                        1.0 * (iBandIndex + 1) / nBandCount,
                                        pfnProgressGlobal,
                                        pProgressDataGlobal );
            if( psExtraArg->pProgressData == NULL )
                psExtraArg->pfnProgress = NULL;
        }

        eErr = poBand->IRasterIO( eRWFlag, nXOff, nYOff, nXSize, nYSize, 
                                  (void *) pabyBandData, nBufXSize, nBufYSize,
                                  eBufType, nPixelSpace, nLineSpace,
                                  psExtraArg );

        if( nBandCount > 1 )
            GDALDestroyScaledProgress( psExtraArg->pProgressData );
    }
    
    psExtraArg->pfnProgress = pfnProgressGlobal;
    psExtraArg->pProgressData = pProgressDataGlobal;

    return eErr;
}

/************************************************************************/
/*               ValidateRasterIOOrAdviseReadParameters()               */
/************************************************************************/

CPLErr GDALDataset::ValidateRasterIOOrAdviseReadParameters(
                               const char* pszCallingFunc,
                               int* pbStopProcessingOnCENone,
                               int nXOff, int nYOff, int nXSize, int nYSize,
                               int nBufXSize, int nBufYSize, 
                               int nBandCount, int *panBandMap)
{

/* -------------------------------------------------------------------- */
/*      Some size values are "noop".  Lets just return to avoid         */
/*      stressing lower level functions.                                */
/* -------------------------------------------------------------------- */
    if( nXSize < 1 || nYSize < 1 || nBufXSize < 1 || nBufYSize < 1 )
    {
        CPLDebug( "GDAL", 
                  "%s skipped for odd window or buffer size.\n"
                  "  Window = (%d,%d)x%dx%d\n"
                  "  Buffer = %dx%d\n",
                  pszCallingFunc,
                  nXOff, nYOff, nXSize, nYSize, 
                  nBufXSize, nBufYSize );

        *pbStopProcessingOnCENone = TRUE;
        return CE_None;
    }

    CPLErr eErr = CE_None;
    *pbStopProcessingOnCENone = FALSE;

    if( nXOff < 0 || nXOff > INT_MAX - nXSize || nXOff + nXSize > nRasterXSize
        || nYOff < 0 || nYOff > INT_MAX - nYSize || nYOff + nYSize > nRasterYSize )
    {
        ReportError( CE_Failure, CPLE_IllegalArg,
                  "Access window out of range in %s.  Requested\n"
                  "(%d,%d) of size %dx%d on raster of %dx%d.",
                  pszCallingFunc, nXOff, nYOff, nXSize, nYSize, nRasterXSize, nRasterYSize );
        eErr = CE_Failure;
    }

    if( panBandMap == NULL && nBandCount > GetRasterCount() )
    {
        ReportError( CE_Failure, CPLE_IllegalArg,
                    "%s: nBandCount cannot be greater than %d",
                    pszCallingFunc, GetRasterCount() );
        eErr = CE_Failure;
    }

    for( int i = 0; i < nBandCount && eErr == CE_None; i++ )
    {
        int iBand = (panBandMap != NULL) ? panBandMap[i] : i + 1;
        if( iBand < 1 || iBand > GetRasterCount() )
        {
            ReportError( CE_Failure, CPLE_IllegalArg,
                      "%s: panBandMap[%d] = %d, this band does not exist on dataset.",
                      pszCallingFunc, i, iBand );
            eErr = CE_Failure;
        }

        if( eErr == CE_None && GetRasterBand( iBand ) == NULL )
        {
            ReportError( CE_Failure, CPLE_IllegalArg,
                      "%s: panBandMap[%d]=%d, this band should exist but is NULL!",
                      pszCallingFunc, i, iBand );
            eErr = CE_Failure;
        }
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
 * This method is the same as the C GDALDatasetRasterIO() or
 * GDALDatasetRasterIOEx() functions.
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
 * @param psExtraArg (new in GDAL 2.0) pointer to a GDALRasterIOExtraArg structure with additional
 * arguments to specify resampling and progress callback, or NULL for default
 * behaviour. The GDAL_RASTERIO_RESAMPLING configuration option can also be defined
 * to override the default resampling to one of BILINEAR, CUBIC, CUBICSPLINE,
 * LANCZOS, AVERAGE or MODE.
 *
 * @return CE_Failure if the access fails, otherwise CE_None.
 */

CPLErr GDALDataset::RasterIO( GDALRWFlag eRWFlag,
                              int nXOff, int nYOff, int nXSize, int nYSize,
                              void * pData, int nBufXSize, int nBufYSize,
                              GDALDataType eBufType, 
                              int nBandCount, int *panBandMap,
                              GSpacing nPixelSpace, GSpacing nLineSpace,
                              GSpacing nBandSpace,
                              GDALRasterIOExtraArg* psExtraArg )

{
    int i = 0;
    int bNeedToFreeBandMap = FALSE;
    CPLErr eErr = CE_None;
    
    GDALRasterIOExtraArg sExtraArg;
    if( psExtraArg == NULL )
    {
        INIT_RASTERIO_EXTRA_ARG(sExtraArg);
        psExtraArg = &sExtraArg;
    }
    else if( psExtraArg->nVersion != RASTERIO_EXTRA_ARG_CURRENT_VERSION )
    {
        ReportError( CE_Failure, CPLE_AppDefined,
                     "Unhandled version of GDALRasterIOExtraArg" );
        return CE_Failure;
    }

    GDALRasterIOExtraArgSetResampleAlg(psExtraArg, nXSize, nYSize,
                                       nBufXSize, nBufYSize);

    if( NULL == pData )
    {
        ReportError( CE_Failure, CPLE_AppDefined,
                  "The buffer into which the data should be read is null" );
        return CE_Failure;
    }
    
/* -------------------------------------------------------------------- */
/*      Do some validation of parameters.                               */
/* -------------------------------------------------------------------- */

    if( eRWFlag != GF_Read && eRWFlag != GF_Write )
    {
        ReportError( CE_Failure, CPLE_IllegalArg,
                  "eRWFlag = %d, only GF_Read (0) and GF_Write (1) are legal.",
                  eRWFlag );
        return CE_Failure;
    }

    int bStopProcessing = FALSE;
    eErr = ValidateRasterIOOrAdviseReadParameters( "RasterIO()", &bStopProcessing,
                                                    nXOff, nYOff, nXSize, nYSize,
                                                    nBufXSize, nBufYSize, 
                                                    nBandCount, panBandMap);
    if( eErr != CE_None || bStopProcessing )
        return eErr;

/* -------------------------------------------------------------------- */
/*      If pixel and line spacing are defaulted assign reasonable      */
/*      value assuming a packed buffer.                                 */
/* -------------------------------------------------------------------- */
    if( nPixelSpace == 0 )
        nPixelSpace = GDALGetDataTypeSize( eBufType ) / 8;
    
    if( nLineSpace == 0 )
    {
        nLineSpace = nPixelSpace * nBufXSize;
    }

    if( nBandSpace == 0 && nBandCount > 1 )
    {
        nBandSpace = nLineSpace * nBufYSize;
    }

    if( panBandMap == NULL )
    {
        panBandMap = (int *) VSIMalloc2(sizeof(int), nBandCount);
        if (panBandMap == NULL)
        {
            ReportError( CE_Failure, CPLE_OutOfMemory,
                      "Out of memory while allocating band map array" );
            return CE_Failure;
        }
        for( i = 0; i < nBandCount; i++ )
            panBandMap[i] = i+1;

        bNeedToFreeBandMap = TRUE;
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
                                nPixelSpace, nLineSpace, nBandSpace,
                                psExtraArg );
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
                       nPixelSpace, nLineSpace, nBandSpace,
                       psExtraArg );
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
 * Use GDALDatasetRasterIOEx() if 64 bit spacings or extra arguments (resampling
 * resolution, progress callback, etc. are needed)
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
                            nPixelSpace, nLineSpace, nBandSpace, NULL ) );
}

/************************************************************************/
/*                       GDALDatasetRasterIOEx()                        */
/************************************************************************/

/**
 * \brief Read/write a region of image data from multiple bands.
 *
 * @see GDALDataset::RasterIO()
 * @since GDAL 2.0
 */

CPLErr CPL_STDCALL
GDALDatasetRasterIOEx( GDALDatasetH hDS, GDALRWFlag eRWFlag,
                       int nXOff, int nYOff, int nXSize, int nYSize,
                       void * pData, int nBufXSize, int nBufYSize,
                       GDALDataType eBufType,
                       int nBandCount, int *panBandMap,
                       GSpacing nPixelSpace, GSpacing nLineSpace,
                       GSpacing nBandSpace,
                       GDALRasterIOExtraArg* psExtraArg )
    
{
    VALIDATE_POINTER1( hDS, "GDALDatasetRasterIOEx", CE_Failure );

    GDALDataset    *poDS = (GDALDataset *) hDS;

    return( poDS->RasterIO( eRWFlag, nXOff, nYOff, nXSize, nYSize,
                            pData, nBufXSize, nBufYSize, eBufType,
                            nBandCount, panBandMap, 
                            nPixelSpace, nLineSpace, nBandSpace, psExtraArg ) );
}
/************************************************************************/
/*                          GetOpenDatasets()                           */
/************************************************************************/

/**
 * \brief Fetch all open GDAL dataset handles.
 *
 * This method is the same as the C function GDALGetOpenDatasets().
 *
 * NOTE: This method is not thread safe.  The returned list may change
 * at any time and it should not be freed.
 *
 * @param pnCount integer into which to place the count of dataset pointers
 * being returned.
 *
 * @return a pointer to an array of dataset handles. 
 */

GDALDataset **GDALDataset::GetOpenDatasets( int *pnCount )

{
    CPLMutexHolderD( &hDLMutex );

    if (poAllDatasetMap != NULL)
    {
        int i = 0;
        *pnCount = poAllDatasetMap->size();
        ppDatasets = (GDALDataset**) CPLRealloc(ppDatasets, (*pnCount) * sizeof(GDALDataset*));
        std::map<GDALDataset*, GIntBig>::iterator oIter = poAllDatasetMap->begin();
        for(; oIter != poAllDatasetMap->end(); ++oIter, i++ )
            ppDatasets[i] = oIter->first;
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
/*                        GDALCleanOpenDatasetsList()                   */
/************************************************************************/

/* Useful when called from the child of a fork(), to avoid closing */
/* the datasets of the parent at the child termination */
void GDALNullifyOpenDatasetsList()
{
    poAllDatasetMap = NULL;
    phSharedDatasetSet = NULL;
    ppDatasets = NULL;
    hDLMutex = NULL;
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
                                GDALDataType eBufType, 
                                int nBandCount, int *panBandMap,
                                char **papszOptions )

{
    int iBand;

/* -------------------------------------------------------------------- */
/*      Do some validation of parameters.                               */
/* -------------------------------------------------------------------- */
    int bStopProcessing = FALSE;
    CPLErr eErr = ValidateRasterIOOrAdviseReadParameters( "AdviseRead()", &bStopProcessing,
                                                    nXOff, nYOff, nXSize, nYSize,
                                                    nBufXSize, nBufYSize, 
                                                    nBandCount, panBandMap);
    if( eErr != CE_None || bStopProcessing )
        return eErr;

    for( iBand = 0; iBand < nBandCount; iBand++ )
    {
        CPLErr eErr;
        GDALRasterBand *poBand;

        if( panBandMap == NULL )
            poBand = GetRasterBand(iBand+1);
        else
            poBand = GetRasterBand(panBandMap[iBand]);

        eErr = poBand->AdviseRead( nXOff, nYOff, nXSize, nYSize,
                                   nBufXSize, nBufYSize, eBufType, papszOptions );

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
 * depending on the path used to originally open the dataset.  The strings
 * will be UTF-8 encoded.
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
    bMainFileReal = VSIStatExL( osMainFilename, &sStat, VSI_STAT_EXISTS_FLAG ) == 0;

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
        char **papszIter = papszMskList;
        while( papszIter && *papszIter )
        {
            if( CSLFindString( papszList, *papszIter ) < 0 )
                papszList = CSLAddString( papszList, *papszIter );
            papszIter ++;
        }
        CSLDestroy( papszMskList );
    }

/* -------------------------------------------------------------------- */
/*      Do we have a world file?                                        */
/* -------------------------------------------------------------------- */
    if( bMainFileReal &&
        !GDALCanFileAcceptSidecarFile(osMainFilename) )
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

            if (oOvManager.papszInitSiblingFiles)
            {
                int iSibling = CSLFindString(oOvManager.papszInitSiblingFiles,
                                             CPLGetFilename(osWorldFilename));
                if (iSibling >= 0)
                {
                    osWorldFilename.resize(strlen(osWorldFilename) -
                                           strlen(oOvManager.papszInitSiblingFiles[iSibling]));
                    osWorldFilename += oOvManager.papszInitSiblingFiles[iSibling];
                    papszList = CSLAddString( papszList, osWorldFilename );
                }
            }
            else if( VSIStatExL( osWorldFilename, &sStat, VSI_STAT_EXISTS_FLAG ) == 0 )
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
 * Note that if you got a mask band with a previous call to GetMaskBand(),
 * it might be invalidated by CreateMaskBand(). So you have to call GetMaskBand()
 * again.
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
    {
        CPLErr eErr = oOvManager.CreateMaskBand( nFlags, -1 );
        if (eErr != CE_None)
            return eErr;

        /* Invalidate existing raster band masks */
        int i;
        for(i=0;i<nBands;i++)
        {
            GDALRasterBand* poBand = papoBands[i];
            if (poBand->bOwnMask)
                delete poBand->poMask;
            poBand->bOwnMask = false;
            poBand->poMask = NULL;
        }

        return CE_None;
    }

    ReportError( CE_Failure, CPLE_NotSupported,
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
 * drivers fail then NULL is returned and an error is issued.
 *
 * Several recommendations :
 * <ul>
 * <li>If you open a dataset object with GA_Update access, it is not recommended
 * to open a new dataset on the same underlying file.</li>
 * <li>The returned dataset should only be accessed by one thread at a time. If you
 * want to use it from different threads, you must add all necessary code (mutexes, etc.)
 * to avoid concurrent use of the object. (Some drivers, such as GeoTIFF, maintain internal
 * state variables that are updated each time a new block is read, thus preventing concurrent
 * use.) </li>
 * </ul>
 *
 * For drivers supporting the VSI virtual file API, it is possible to open
 * a file in a .zip archive (see VSIInstallZipFileHandler()), in a .tar/.tar.gz/.tgz archive
 * (see VSIInstallTarFileHandler()) or on a HTTP / FTP server (see VSIInstallCurlFileHandler())
 *
 * In some situations (dealing with unverified data), the datasets can be opened in another
 * process through the \ref gdal_api_proxy mechanism.
 *
 * \sa GDALOpenShared()
 * \sa GDALOpenEx()
 *
 * @param pszFilename the name of the file to access.  In the case of
 * exotic drivers this may not refer to a physical file, but instead contain
 * information for the driver on how to access a dataset.  It should be in UTF-8
 * encoding.
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
    return GDALOpenEx( pszFilename,
                       GDAL_OF_RASTER |
                       (eAccess == GA_Update ? GDAL_OF_UPDATE : 0) |
                       GDAL_OF_VERBOSE_ERROR,
                       NULL, NULL, NULL );
}


/************************************************************************/
/*                             GDALOpenEx()                             */
/************************************************************************/

/**
 * \brief Open a raster or vector file as a GDALDataset.
 *
 * This function will try to open the passed file, or virtual dataset
 * name by invoking the Open method of each registered GDALDriver in turn. 
 * The first successful open will result in a returned dataset.  If all
 * drivers fail then NULL is returned and an error is issued.
 *
 * Several recommendations :
 * <ul>
 * <li>If you open a dataset object with GDAL_OF_UPDATE access, it is not recommended
 * to open a new dataset on the same underlying file.</li>
 * <li>The returned dataset should only be accessed by one thread at a time. If you
 * want to use it from different threads, you must add all necessary code (mutexes, etc.)
 * to avoid concurrent use of the object. (Some drivers, such as GeoTIFF, maintain internal
 * state variables that are updated each time a new block is read, thus preventing concurrent
 * use.) </li>
 * </ul>
 *
 * For drivers supporting the VSI virtual file API, it is possible to open
 * a file in a .zip archive (see VSIInstallZipFileHandler()), in a .tar/.tar.gz/.tgz archive
 * (see VSIInstallTarFileHandler()) or on a HTTP / FTP server (see VSIInstallCurlFileHandler())
 *
 * In some situations (dealing with unverified data), the datasets can be opened in another
 * process through the \ref gdal_api_proxy mechanism.
 *
 * In order to reduce the need for searches through the operating system
 * file system machinery, it is possible to give an optional list of files with
 * the papszSiblingFiles parameter.
 * This is the list of all files at the same level in the file system as the
 * target file, including the target file. The filenames must not include any
 * path components, are an essentially just the output of CPLReadDir() on the
 * parent directory. If the target object does not have filesystem semantics
 * then the file list should be NULL.
 *
 * @param pszFilename the name of the file to access.  In the case of
 * exotic drivers this may not refer to a physical file, but instead contain
 * information for the driver on how to access a dataset.  It should be in UTF-8
 * encoding.
 *
 * @param nOpenFlags a combination of GDAL_OF_ flags that may be combined through
 * logical or operator.
 * <ul>
 * <li>Driver kind: GDAL_OF_RASTER for raster drivers, GDAL_OF_VECTOR for vector drivers.
 *     If none of the value is specified, both kinds are implied.</li>
 * <li>Access mode: GDAL_OF_READONLY (exclusive)or GDAL_OF_UPDATE.</li>
 * <li>Shared mode: GDAL_OF_SHARED. If set, it allows the sharing of
 *  GDALDataset handles for a dataset with other callers that have set GDAL_OF_SHARED.
 * In particular, GDALOpenEx() will first consult its list of currently
 * open and shared GDALDataset's, and if the GetDescription() name for one
 * exactly matches the pszFilename passed to GDALOpenEx() it will be
 * referenced and returned, if GDALOpenEx() is called from the same thread.</li>
 * <li>Verbose error: GDAL_OF_VERBOSE_ERROR. If set, a failed attempt to open the
 * file will lead to an error message to be reported.</li>
 * </ul>
 *
 * @param papszAllowedDrivers NULL to consider all candidate drivers, or a NULL
 * terminated list of strings with the driver short names that must be considered.
 *
 * @param papszOpenOptions NULL, or a NULL terminated list of strings with open
 * options passed to candidate drivers. An option exists for all drivers,
 * OVERVIEW_LEVEL=level, to select a particular overview level of a dataset.
 * The level index starts at 0. The level number can be suffixed by "only" to specify that
 * only this overview level must be visible, and not sub-levels.
 * Open options are validated by default, and a warning is emitted in case the
 * option is not recognized. In some scenarios, it might be not desirable (e.g.
 * when not knowing which driver will open the file), so the special open option
 * VALIDATE_OPEN_OPTIONS can be set to NO to avoid such warnings.
 *
 * @param papszSiblingFiles  NULL, or a NULL terminated list of strings that are
 * filenames that are auxiliary to the main filename. If NULL is passed, a probing
 * of the file system will be done.
 *
 * @return A GDALDatasetH handle or NULL on failure.  For C++ applications
 * this handle can be cast to a GDALDataset *. 
 *
 * @since GDAL 2.0
 */

GDALDatasetH CPL_STDCALL GDALOpenEx( const char* pszFilename,
                                 unsigned int nOpenFlags,
                                 const char* const* papszAllowedDrivers,
                                 const char* const* papszOpenOptions,
                                 const char* const* papszSiblingFiles )
{
    VALIDATE_POINTER1( pszFilename, "GDALOpen", NULL );

/* -------------------------------------------------------------------- */
/*      In case of shared dataset, first scan the existing list to see  */
/*      if it could already contain the requested dataset.              */
/* -------------------------------------------------------------------- */
    if( nOpenFlags & GDAL_OF_SHARED )
    {
        if( nOpenFlags & GDAL_OF_INTERNAL )
        {
            CPLError(CE_Failure, CPLE_IllegalArg, "GDAL_OF_SHARED and GDAL_OF_INTERNAL are exclusive");
            return NULL;
        }
        
        CPLMutexHolderD( &hDLMutex );

        if (phSharedDatasetSet != NULL)
        {
            GIntBig nThisPID = GDALGetResponsiblePIDForCurrentThread();
            SharedDatasetCtxt* psStruct;
            SharedDatasetCtxt sStruct;

            sStruct.nPID = nThisPID;
            sStruct.pszDescription = (char*) pszFilename;
            sStruct.eAccess = (nOpenFlags & GDAL_OF_UPDATE) ? GA_Update : GA_ReadOnly;
            psStruct = (SharedDatasetCtxt*) CPLHashSetLookup(phSharedDatasetSet, &sStruct);
            if (psStruct == NULL && (nOpenFlags & GDAL_OF_UPDATE) == 0)
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

    /* If no driver kind is specified, assume all are to be probed */
    if( (nOpenFlags & GDAL_OF_KIND_MASK) == 0 )
        nOpenFlags |= GDAL_OF_KIND_MASK;

    {
        int* pnRecCount = (int*)CPLGetTLS( CTLS_GDALDATASET_REC_PROTECT_MAP );
        if( pnRecCount == NULL )
        {
            pnRecCount = (int*) CPLMalloc(sizeof(int));
            *pnRecCount = 0;
            CPLSetTLS( CTLS_GDALDATASET_REC_PROTECT_MAP, pnRecCount, TRUE );
        }
        if( *pnRecCount == 100 )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "GDALOpen() called with too many recursion levels");
            return NULL;
        }
        (*pnRecCount) ++;
    }

    int         iDriver;
    GDALDriverManager *poDM = GetGDALDriverManager();
    //CPLLocaleC  oLocaleForcer;

    CPLErrorReset();
    CPLAssert( NULL != poDM );

    /* Build GDALOpenInfo just now to avoid useless file stat'ing if a */
    /* shared dataset was asked before */
    GDALOpenInfo oOpenInfo(pszFilename,
                           nOpenFlags,
                           (char**) papszSiblingFiles);
    oOpenInfo.papszOpenOptions = (char**) papszOpenOptions;

    for( iDriver = -1; iDriver < poDM->GetDriverCount(); iDriver++ )
    {
        GDALDriver      *poDriver;
        GDALDataset     *poDS;

        if( iDriver < 0 )
            poDriver = GDALGetAPIPROXYDriver();
        else
        {
            poDriver = poDM->GetDriver( iDriver );
            if (papszAllowedDrivers != NULL &&
                CSLFindString((char**)papszAllowedDrivers, GDALGetDriverShortName(poDriver)) == -1)
                continue;
        }

        if( (nOpenFlags & GDAL_OF_RASTER) != 0 &&
            (nOpenFlags & GDAL_OF_VECTOR) == 0 &&
            poDriver->GetMetadataItem(GDAL_DCAP_RASTER) == NULL )
            continue;
        if( (nOpenFlags & GDAL_OF_VECTOR) != 0 &&
            (nOpenFlags & GDAL_OF_RASTER) == 0 &&
            poDriver->GetMetadataItem(GDAL_DCAP_VECTOR) == NULL )
            continue;

        /* Remove general OVERVIEW_LEVEL open options from list before */
        /* passing it to the driver, if it isn't a driver specific option already */
        char** papszTmpOpenOptions = NULL;
        if( CSLFetchNameValue((char**)papszOpenOptions, "OVERVIEW_LEVEL") != NULL &&
            (poDriver->GetMetadataItem(GDAL_DMD_OPENOPTIONLIST) == NULL ||
             CPLString(poDriver->GetMetadataItem(GDAL_DMD_OPENOPTIONLIST)).ifind("OVERVIEW_LEVEL") == std::string::npos) )
        {
            papszTmpOpenOptions = CSLDuplicate((char**)papszOpenOptions);
            papszTmpOpenOptions = CSLSetNameValue(papszTmpOpenOptions, "OVERVIEW_LEVEL", NULL);
            oOpenInfo.papszOpenOptions = papszTmpOpenOptions;
        }

        int bIdentifyRes =
            ( poDriver->pfnIdentify && poDriver->pfnIdentify(&oOpenInfo) > 0 );
        if( bIdentifyRes )
        {
            GDALValidateOpenOptions( poDriver, oOpenInfo.papszOpenOptions );
        }

        if ( poDriver->pfnOpen != NULL )
        {
            poDS = poDriver->pfnOpen( &oOpenInfo );
            // If we couldn't determine for sure with Identify() (it returned -1)
            // but that Open() managed to open the file, post validate options.
            if( poDS != NULL && poDriver->pfnIdentify && !bIdentifyRes )
                GDALValidateOpenOptions( poDriver, oOpenInfo.papszOpenOptions );
        }
        else if( poDriver->pfnOpenWithDriverArg != NULL )
        {
            poDS = poDriver->pfnOpenWithDriverArg( poDriver, &oOpenInfo );
        }
        else
        {
            CSLDestroy(papszTmpOpenOptions);
            oOpenInfo.papszOpenOptions = (char**) papszOpenOptions;
            continue;
        }

        CSLDestroy(papszTmpOpenOptions);
        oOpenInfo.papszOpenOptions = (char**) papszOpenOptions;

        if( poDS != NULL )
        {
            if( strlen(poDS->GetDescription()) == 0 )
                poDS->SetDescription( pszFilename );

            if( poDS->poDriver == NULL )
                poDS->poDriver = poDriver;

            if( poDS->papszOpenOptions == NULL )
                poDS->papszOpenOptions = CSLDuplicate((char**)papszOpenOptions);

            if( !(nOpenFlags & GDAL_OF_INTERNAL) )
            {
                if( CPLGetPID() != GDALGetResponsiblePIDForCurrentThread() )
                    CPLDebug( "GDAL", "GDALOpen(%s, this=%p) succeeds as %s (pid=%d, responsiblePID=%d).",
                              pszFilename, poDS, poDriver->GetDescription(),
                              (int)CPLGetPID(), (int)GDALGetResponsiblePIDForCurrentThread() );
                else
                    CPLDebug( "GDAL", "GDALOpen(%s, this=%p) succeeds as %s.",
                              pszFilename, poDS, poDriver->GetDescription() );

                poDS->AddToDatasetOpenList();
            }

            int* pnRecCount = (int*)CPLGetTLS( CTLS_GDALDATASET_REC_PROTECT_MAP );
            if( pnRecCount )
                (*pnRecCount) --;

            if( nOpenFlags & GDAL_OF_SHARED )
            {
                if (strcmp(pszFilename, poDS->GetDescription()) != 0)
                {
                    CPLError(CE_Warning, CPLE_NotSupported,
                            "A dataset opened by GDALOpenShared should have the same filename (%s) "
                            "and description (%s)",
                            pszFilename, poDS->GetDescription());
                }
                else
                {
                    poDS->MarkAsShared();
                }
            }

            /* Deal with generic OVERVIEW_LEVEL open option, unless it is */
            /* driver specific */
            if( CSLFetchNameValue((char**) papszOpenOptions, "OVERVIEW_LEVEL") != NULL &&
                (poDriver->GetMetadataItem(GDAL_DMD_OPENOPTIONLIST) == NULL ||
                CPLString(poDriver->GetMetadataItem(GDAL_DMD_OPENOPTIONLIST)).ifind("OVERVIEW_LEVEL") == std::string::npos) )
            {
                CPLString osVal(CSLFetchNameValue((char**) papszOpenOptions, "OVERVIEW_LEVEL"));
                int nOvrLevel = atoi(osVal);
                int bThisLevelOnly = osVal.ifind("only") != std::string::npos;
                GDALDataset* poOvrDS = GDALCreateOverviewDataset(poDS, nOvrLevel, bThisLevelOnly, TRUE);
                if( poOvrDS == NULL )
                {
                    if( nOpenFlags & GDAL_OF_VERBOSE_ERROR )
                    {
                        CPLError( CE_Failure, CPLE_OpenFailed,
                                  "Cannot open overview level %d of %s",
                                  nOvrLevel, pszFilename );
                    }
                    GDALClose( poDS );
                    poDS = NULL;
                }
                else
                {
                    poDS = poOvrDS;
                }
            }

            return (GDALDatasetH) poDS;
        }

        if( CPLGetLastErrorNo() != 0 )
        {
            int* pnRecCount = (int*)CPLGetTLS( CTLS_GDALDATASET_REC_PROTECT_MAP );
            if( pnRecCount )
                (*pnRecCount) --;

            return NULL;
        }
    }

    if( nOpenFlags & GDAL_OF_VERBOSE_ERROR )
    {
        if( oOpenInfo.bStatOK )
            CPLError( CE_Failure, CPLE_OpenFailed,
                    "`%s' not recognised as a supported file format.\n",
                    pszFilename );
        else
            CPLError( CE_Failure, CPLE_OpenFailed,
                    "`%s' does not exist in the file system,\n"
                    "and is not recognised as a supported dataset name.\n",
                    pszFilename );
    }

    int* pnRecCount = (int*)CPLGetTLS( CTLS_GDALDATASET_REC_PROTECT_MAP );
    if( pnRecCount )
        (*pnRecCount) --;

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
 * does explicitly use mutexes in its code.
 *
 * For drivers supporting the VSI virtual file API, it is possible to open
 * a file in a .zip archive (see VSIInstallZipFileHandler()), in a .tar/.tar.gz/.tgz archive
 * (see VSIInstallTarFileHandler()) or on a HTTP / FTP server (see VSIInstallCurlFileHandler())
 *
 * In some situations (dealing with unverified data), the datasets can be opened in another
 * process through the \ref gdal_api_proxy mechanism.
 *
 * \sa GDALOpen()
 * \sa GDALOpenEx()
 *
 * @param pszFilename the name of the file to access.  In the case of
 * exotic drivers this may not refer to a physical file, but instead contain
 * information for the driver on how to access a dataset.  It should be in 
 * UTF-8 encoding.
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
    return GDALOpenEx( pszFilename,
                       GDAL_OF_RASTER |
                       (eAccess == GA_Update ? GDAL_OF_UPDATE : 0) |
                       GDAL_OF_SHARED |
                       GDAL_OF_VERBOSE_ERROR,
                       NULL, NULL, NULL );
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
    if( hDS == NULL )
        return;

    GDALDataset *poDS = (GDALDataset *) hDS;

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


static int GDALDumpOpenDatasetsForeach(GDALDataset* poDS, FILE *fp)
{
    const char *pszDriverName;

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
 * text file (may be stdout or stderr).   This function is primarily intended
 * to assist in debugging "dataset leaks" and reference counting issues. 
 * The information reported includes the dataset name, referenced count, 
 * shared status, driver name, size, and band count. 
 */

int CPL_STDCALL GDALDumpOpenDatasets( FILE *fp )
   
{
    VALIDATE_POINTER1( fp, "GDALDumpOpenDatasets", 0 );

    CPLMutexHolderD( &hDLMutex );

    if (poAllDatasetMap != NULL)
    {
        VSIFPrintf( fp, "Open GDAL Datasets:\n" );
        std::map<GDALDataset*, GIntBig>::iterator oIter = poAllDatasetMap->begin();
        for(; oIter != poAllDatasetMap->end(); ++oIter )
        {
            GDALDumpOpenDatasetsForeach(oIter->first, fp);
        }
        if (phSharedDatasetSet != NULL)
        {
            CPLHashSetForeach(phSharedDatasetSet, GDALDumpOpenSharedDatasetsForeach, fp);
        }
        return (int)poAllDatasetMap->size();
    }
    else
    {
        return 0;
    }
}

/************************************************************************/
/*                        BeginAsyncReader()                          */
/************************************************************************/

/**
 * \brief Sets up an asynchronous data request
 *
 * This method establish an asynchronous raster read request for the
 * indicated window on the dataset into the indicated buffer.  The parameters
 * for windowing, buffer size, buffer type and buffer organization are similar
 * to those for GDALDataset::RasterIO(); however, this call only launches
 * the request and filling the buffer is accomplished via calls to 
 * GetNextUpdatedRegion() on the return GDALAsyncReader session object.
 * 
 * Once all processing for the created session is complete, or if no further
 * refinement of the request is required, the GDALAsyncReader object should
 * be destroyed with the GDALDataset::EndAsyncReader() method. 
 * 
 * Note that the data buffer (pData) will potentially continue to be 
 * updated as long as the session lives, but it is not deallocated when
 * the session (GDALAsyncReader) is destroyed with EndAsyncReader().  It
 * should be deallocated by the application at that point. 
 *
 * Additional information on asynchronous IO in GDAL may be found at: 
 *   http://trac.osgeo.org/gdal/wiki/rfc24_progressive_data_support
 * 
 * This method is the same as the C GDALBeginAsyncReader() function.
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
 * @param pBuf The buffer into which the data should be read. This buffer must 
 * contain at least nBufXSize * nBufYSize * nBandCount words of type eBufType.  
 * It is organized in left to right,top to bottom pixel order.  Spacing is 
 * controlled by the nPixelSpace, and nLineSpace parameters.
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
 * @param nPixelSpace The byte offset from the start of one pixel value in
 * pData to the start of the next pixel value within a scanline.  If defaulted
 * (0) the size of the datatype eBufType is used.
 *
 * @param nLineSpace The byte offset from the start of one scanline in
 * pData to the start of the next.  If defaulted the size of the datatype
 * eBufType * nBufXSize is used.
 *
 * @param nBandSpace the byte offset from the start of one bands data to the
 * start of the next.  If defaulted (zero) the value will be 
 * nLineSpace * nBufYSize implying band sequential organization
 * of the data buffer. 
 *
 * @param papszOptions Driver specific control options in a string list or NULL.
 * Consult driver documentation for options supported.
 * 
 * @return The GDALAsyncReader object representing the request.
 */

GDALAsyncReader* 
GDALDataset::BeginAsyncReader(int nXOff, int nYOff,
                              int nXSize, int nYSize,
                              void *pBuf,
                              int nBufXSize, int nBufYSize,
                              GDALDataType eBufType,
                              int nBandCount, int* panBandMap,
                              int nPixelSpace, int nLineSpace,
                              int nBandSpace, char **papszOptions)
{
    // See gdaldefaultasync.cpp

    return
        GDALGetDefaultAsyncReader( this, 
                                     nXOff, nYOff, nXSize, nYSize,
                                     pBuf, nBufXSize, nBufYSize, eBufType,
                                     nBandCount, panBandMap,
                                     nPixelSpace, nLineSpace, nBandSpace,
                                     papszOptions );
}

/************************************************************************/
/*                        GDALBeginAsyncReader()                      */
/************************************************************************/

GDALAsyncReaderH CPL_STDCALL 
GDALBeginAsyncReader(GDALDatasetH hDS, int xOff, int yOff,
                       int xSize, int ySize,
                       void *pBuf,
                       int bufXSize, int bufYSize,
                       GDALDataType bufType,
                       int nBandCount, int* bandMap,
                       int nPixelSpace, int nLineSpace,
                       int nBandSpace,
                       char **papszOptions)

{
    VALIDATE_POINTER1( hDS, "GDALDataset", NULL );
    return (GDALAsyncReaderH)((GDALDataset *) hDS)->
        BeginAsyncReader(xOff, yOff,
                           xSize, ySize,
                           pBuf, bufXSize, bufYSize,
                           bufType, nBandCount, bandMap,
                           nPixelSpace, nLineSpace,
                           nBandSpace, papszOptions);
}

/************************************************************************/
/*                        EndAsyncReader()                            */
/************************************************************************/

/**
 * End asynchronous request.
 *
 * This method destroys an asynchronous io request and recovers all 
 * resources associated with it.
 * 
 * This method is the same as the C function GDALEndAsyncReader(). 
 *
 * @param poARIO pointer to a GDALAsyncReader
 */

void GDALDataset::EndAsyncReader(GDALAsyncReader *poARIO )
{
    delete poARIO;
}

/************************************************************************/
/*                        GDALEndAsyncReader()                        */
/************************************************************************/
void CPL_STDCALL GDALEndAsyncReader(GDALDatasetH hDS, GDALAsyncReaderH hAsyncReaderH)
{
    VALIDATE_POINTER0( hDS, "GDALDataset" );
    VALIDATE_POINTER0( hAsyncReaderH, "GDALAsyncReader" );
    ((GDALDataset *) hDS) -> EndAsyncReader((GDALAsyncReader *)hAsyncReaderH);	
}

/************************************************************************/
/*                       CloseDependentDatasets()                       */
/************************************************************************/

/**
 * Drop references to any other datasets referenced by this dataset.
 *
 * This method should release any reference to other datasets (e.g. a VRT
 * dataset to its sources), but not close the current dataset itself.
 *
 * If at least, one reference to a dependent dataset has been dropped,
 * this method should return TRUE. Otherwise it *should* return FALSE.
 * (Failure to return the proper value might result in infinite loop)
 *
 * This method can be called several times on a given dataset. After
 * the first time, it should not do anything and return FALSE.
 *
 * The driver implementation may choose to destroy its raster bands,
 * so be careful not to call any method on the raster bands afterwards.
 *
 * Basically the only safe action you can do after calling CloseDependantDatasets()
 * is to call the destructor.
 *
 * Note: the only legitimate caller of CloseDependantDatasets() is
 * GDALDriverManager::~GDALDriverManager()
 *
 * @return TRUE if at least one reference to another dataset has been dropped.
 */
int GDALDataset::CloseDependentDatasets()
{
    return oOvManager.CloseDependentDatasets();
}

/************************************************************************/
/*                            ReportError()                             */
/************************************************************************/

/**
 * \brief Emits an error related to a dataset.
 *
 * This function is a wrapper for regular CPLError(). The only difference
 * with CPLError() is that it prepends the error message with the dataset
 * name.
 *
 * @param eErrClass one of CE_Warning, CE_Failure or CE_Fatal.
 * @param err_no the error number (CPLE_*) from cpl_error.h.
 * @param fmt a printf() style format string.  Any additional arguments
 * will be treated as arguments to fill in this format in a manner
 * similar to printf().
 *
 * @since GDAL 1.9.0
 */

void GDALDataset::ReportError(CPLErr eErrClass, int err_no, const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);

    char szNewFmt[256];
    const char* pszDSName = GetDescription();
    if (strlen(fmt) + strlen(pszDSName) + 3 >= sizeof(szNewFmt) - 1)
        pszDSName = CPLGetFilename(pszDSName);
    if (pszDSName[0] != '\0' &&
        strlen(fmt) + strlen(pszDSName) + 3 < sizeof(szNewFmt) - 1)
    {
        snprintf(szNewFmt, sizeof(szNewFmt), "%s: %s",
                 pszDSName, fmt);
        CPLErrorV( eErrClass, err_no, szNewFmt, args );
    }
    else
    {
        CPLErrorV( eErrClass, err_no, fmt, args );
    }
    va_end(args);
}

/************************************************************************/
/*                            GetDriverName()                           */
/************************************************************************/

const char* GDALDataset::GetDriverName()
{
    if( poDriver )
        return poDriver->GetDescription();
    return "";
}

/************************************************************************/
/*                     GDALDatasetReleaseResultSet()                    */
/************************************************************************/

/**
 \brief Release results of ExecuteSQL().

 This function should only be used to deallocate OGRLayers resulting from
 an ExecuteSQL() call on the same GDALDataset.  Failure to deallocate a
 results set before destroying the GDALDataset may cause errors. 

 This function is the same as the C++ method GDALDataset::ReleaseResultSet()

 @since GDAL 2.0

 @param hDS the dataset handle.
 @param poResultsSet the result of a previous ExecuteSQL() call.

*/ 
void GDALDatasetReleaseResultSet( GDALDatasetH hDS, OGRLayerH hLayer )

{
    VALIDATE_POINTER0( hDS, "GDALDatasetReleaseResultSet" );

    ((GDALDataset *) hDS)->ReleaseResultSet( (OGRLayer *) hLayer );
}

/************************************************************************/
/*                     GDALDatasetTestCapability()                      */
/************************************************************************/

/**
 \brief Test if capability is available.

 One of the following dataset capability names can be passed into this
 function, and a TRUE or FALSE value will be returned indicating whether or not
 the capability is available for this object.

 <ul>
  <li> <b>ODsCCreateLayer</b>: True if this datasource can create new layers.<p>
  <li> <b>ODsCDeleteLayer</b>: True if this datasource can delete existing layers.<p>
  <li> <b>ODsCCreateGeomFieldAfterCreateLayer</b>: True if the layers of this
        datasource support CreateGeomField() just after layer creation.<p>
  <li> <b>ODsCCurveGeometries</b>: True if this datasource supports curve geometries.<p>
  <li> <b>ODsCTransactions</b>: True if this datasource supports (efficient) transactions.<p>
  <li> <b>ODsCEmulatedTransactions</b>: True if this datasource supports transactions through emulation.<p>
 </ul>

 The \#define macro forms of the capability names should be used in preference
 to the strings themselves to avoid mispelling.

 This function is the same as the C++ method GDALDataset::TestCapability()

 @since GDAL 2.0

 @param hDS the dataset handle.
 @param pszCapability the capability to test.

 @return TRUE if capability available otherwise FALSE.

*/ 
int GDALDatasetTestCapability( GDALDatasetH hDS, const char *pszCap )

{
    VALIDATE_POINTER1( hDS, "GDALDatasetTestCapability", 0 );
    VALIDATE_POINTER1( pszCap, "GDALDatasetTestCapability", 0 );

    return ((GDALDataset *) hDS)->TestCapability( pszCap );
}

/************************************************************************/
/*                       GDALDatasetGetLayerCount()                     */
/************************************************************************/

/**
 \brief Get the number of layers in this dataset.

 This function is the same as the C++ method GDALDataset::GetLayerCount()
 
 @since GDAL 2.0

 @param hDS the dataset handle.
 @return layer count.
*/

int GDALDatasetGetLayerCount( GDALDatasetH hDS )

{
    VALIDATE_POINTER1( hDS, "GDALDatasetH", 0 );

    return ((GDALDataset *)hDS)->GetLayerCount();
}

/************************************************************************/
/*                        GDALDatasetGetLayer()                         */
/************************************************************************/

/**
 \brief Fetch a layer by index.

 The returned layer remains owned by the 
 GDALDataset and should not be deleted by the application.

 This function is the same as the C++ method GDALDataset::GetLayer()
 
 @since GDAL 2.0

 @param hDS the dataset handle.
 @param iLayer a layer number between 0 and GetLayerCount()-1.

 @return the layer, or NULL if iLayer is out of range or an error occurs.
*/

OGRLayerH GDALDatasetGetLayer( GDALDatasetH hDS, int iLayer )

{
    VALIDATE_POINTER1( hDS, "GDALDatasetGetLayer", NULL );

    return (OGRLayerH) ((GDALDataset*)hDS)->GetLayer( iLayer );
}

/************************************************************************/
/*                     GDALDatasetGetLayerByName()                      */
/************************************************************************/

/**
 \brief Fetch a layer by name.

 The returned layer remains owned by the 
 GDALDataset and should not be deleted by the application.

 This function is the same as the C++ method GDALDataset::GetLayerByName()
 
 @since GDAL 2.0

 @param hDS the dataset handle.
 @param pszLayerName the layer name of the layer to fetch.

 @return the layer, or NULL if Layer is not found or an error occurs.
*/

OGRLayerH GDALDatasetGetLayerByName( GDALDatasetH hDS, const char *pszName )

{
    VALIDATE_POINTER1( hDS, "GDALDatasetGetLayerByName", NULL );

    return (OGRLayerH) ((GDALDataset *) hDS)->GetLayerByName( pszName );
}

/************************************************************************/
/*                        GDALDatasetDeleteLayer()                      */
/************************************************************************/

/**
 \brief Delete the indicated layer from the datasource.

 If this function is supported
 the ODsCDeleteLayer capability will test TRUE on the GDALDataset.

 This method is the same as the C++ method GDALDataset::DeleteLayer().
 
 @since GDAL 2.0

 @param hDS the dataset handle.
 @param iLayer the index of the layer to delete. 

 @return OGRERR_NONE on success, or OGRERR_UNSUPPORTED_OPERATION if deleting
 layers is not supported for this datasource.

*/
OGRErr GDALDatasetDeleteLayer( GDALDatasetH hDS, int iLayer )

{
    VALIDATE_POINTER1( hDS, "GDALDatasetH", OGRERR_INVALID_HANDLE );

    return ((GDALDataset *) hDS)->DeleteLayer( iLayer );
}

/************************************************************************/
/*                            CreateLayer()                             */
/************************************************************************/

/**
\brief This method attempts to create a new layer on the dataset with the indicated name, coordinate system, geometry type.

The papszOptions argument
can be used to control driver specific creation options.  These options are
normally documented in the format specific documentation. 

 In GDAL 2.0, drivers should extend the ICreateLayer() method and not CreateLayer().
 CreateLayer() adds validation of layer creation options, before delegating the
 actual work to ICreateLayer().

 This method is the same as the C function GDALDatasetCreateLayer() and the
 deprecated OGR_DS_CreateLayer(). 
 
 In GDAL 1.X, this method used to be in the OGRDataSource class.

 @param hDS the dataset handle
 @param pszName the name for the new layer.  This should ideally not 
match any existing layer on the datasource.
 @param poSpatialRef the coordinate system to use for the new layer, or NULL if
no coordinate system is available. 
 @param eGType the geometry type for the layer.  Use wkbUnknown if there
are no constraints on the types geometry to be written. 
 @param papszOptions a StringList of name=value options.  Options are driver
specific.

 @return NULL is returned on failure, or a new OGRLayer handle on success. 

<b>Example:</b>

\code
#include "gdal.h" 
#include "cpl_string.h"

...

        OGRLayer *poLayer;
        char     **papszOptions;

        if( !poDS->TestCapability( ODsCCreateLayer ) )
        {
        ...
        }

        papszOptions = CSLSetNameValue( papszOptions, "DIM", "2" );
        poLayer = poDS->CreateLayer( "NewLayer", NULL, wkbUnknown,
                                     papszOptions );
        CSLDestroy( papszOptions );

        if( poLayer == NULL )
        {
            ...
        }        
\endcode
*/

OGRLayer *GDALDataset::CreateLayer( const char * pszName,
                                      OGRSpatialReference * poSpatialRef,
                                      OGRwkbGeometryType eGType,
                                      char **papszOptions )

{
    ValidateLayerCreationOptions( papszOptions );

    if( OGR_GT_IsNonLinear(eGType) && !TestCapability(ODsCCurveGeometries) )
    {
        eGType = OGR_GT_GetLinear(eGType);
    }

    OGRLayer* poLayer = ICreateLayer(pszName, poSpatialRef, eGType, papszOptions);
#ifdef DEBUG
    if( poLayer != NULL && OGR_GT_IsNonLinear(poLayer->GetGeomType()) &&
        !poLayer->TestCapability(OLCCurveGeometries) )
    {
        CPLError( CE_Warning, CPLE_AppDefined,
                  "Inconsistant driver: Layer geometry type is non-linear, but "
                  "TestCapability(OLCCurveGeometries) returns FALSE." );
    }
#endif

    return poLayer;
}

/************************************************************************/
/*                         GDALDatasetCreateLayer()                     */
/************************************************************************/

/**
\brief This function attempts to create a new layer on the dataset with the indicated name, coordinate system, geometry type.

The papszOptions argument
can be used to control driver specific creation options.  These options are
normally documented in the format specific documentation. 

 This method is the same as the C++ method GDALDataset::CreateLayer().
 
 @since GDAL 2.0

 @param hDS the dataset handle
 @param pszName the name for the new layer.  This should ideally not 
match any existing layer on the datasource.
 @param poSpatialRef the coordinate system to use for the new layer, or NULL if
no coordinate system is available. 
 @param eGType the geometry type for the layer.  Use wkbUnknown if there
are no constraints on the types geometry to be written. 
 @param papszOptions a StringList of name=value options.  Options are driver
specific.

 @return NULL is returned on failure, or a new OGRLayer handle on success. 

<b>Example:</b>

\code
#include "gdal.h" 
#include "cpl_string.h"

...

        OGRLayer *poLayer;
        char     **papszOptions;

        if( !poDS->TestCapability( ODsCCreateLayer ) )
        {
        ...
        }

        papszOptions = CSLSetNameValue( papszOptions, "DIM", "2" );
        poLayer = poDS->CreateLayer( "NewLayer", NULL, wkbUnknown,
                                     papszOptions );
        CSLDestroy( papszOptions );

        if( poLayer == NULL )
        {
            ...
        }        
\endcode
*/

OGRLayerH GDALDatasetCreateLayer( GDALDatasetH hDS, 
                              const char * pszName,
                              OGRSpatialReferenceH hSpatialRef,
                              OGRwkbGeometryType eType,
                              char ** papszOptions )

{
    VALIDATE_POINTER1( hDS, "GDALDatasetCreateLayer", NULL );

    if (pszName == NULL)
    {
        CPLError ( CE_Failure, CPLE_ObjectNull, "Name was NULL in GDALDatasetCreateLayer");
        return 0;
    }
    return (OGRLayerH) ((GDALDataset *)hDS)->CreateLayer( 
        pszName, (OGRSpatialReference *) hSpatialRef, eType, papszOptions );
}


/************************************************************************/
/*                         GDALDatasetCopyLayer()                       */
/************************************************************************/

/**
 \brief Duplicate an existing layer.

 This function creates a new layer, duplicate the field definitions of the
 source layer and then duplicate each features of the source layer.
 The papszOptions argument
 can be used to control driver specific creation options.  These options are
 normally documented in the format specific documentation.
 The source layer may come from another dataset.

 This method is the same as the C++ method GDALDataset::CopyLayer()
 
 @since GDAL 2.0

 @param hDS the dataset handle. 
 @param hSrcLayer source layer.
 @param pszNewName the name of the layer to create.
 @param papszOptions a StringList of name=value options.  Options are driver
                     specific.

 @return an handle to the layer, or NULL if an error occurs.
*/
OGRLayerH GDALDatasetCopyLayer( GDALDatasetH hDS, 
                                OGRLayerH hSrcLayer, const char *pszNewName,
                                char **papszOptions )

{
    VALIDATE_POINTER1( hDS, "OGR_DS_CopyGDALDatasetCopyLayerLayer", NULL );
    VALIDATE_POINTER1( hSrcLayer, "GDALDatasetCopyLayer", NULL );
    VALIDATE_POINTER1( pszNewName, "GDALDatasetCopyLayer", NULL );

    return (OGRLayerH) 
        ((GDALDataset *) hDS)->CopyLayer( (OGRLayer *) hSrcLayer, 
                                            pszNewName, papszOptions );
}

/************************************************************************/
/*                        GDALDatasetExecuteSQL()                       */
/************************************************************************/

/**
 \brief Execute an SQL statement against the data store. 

 The result of an SQL query is either NULL for statements that are in error,
 or that have no results set, or an OGRLayer pointer representing a results
 set from the query.  Note that this OGRLayer is in addition to the layers
 in the data store and must be destroyed with 
 ReleaseResultSet() before the dataset is closed
 (destroyed).  

 This method is the same as the C++ method GDALDataset::ExecuteSQL()

 For more information on the SQL dialect supported internally by OGR
 review the <a href="ogr_sql.html">OGR SQL</a> document.  Some drivers (ie.
 Oracle and PostGIS) pass the SQL directly through to the underlying RDBMS.

 Starting with OGR 1.10, the <a href="ogr_sql_sqlite.html">SQLITE dialect</a>
 can also be used.
 
 @since GDAL 2.0
 
 @param hDS the dataset handle.
 @param pszStatement the SQL statement to execute. 
 @param hSpatialFilter geometry which represents a spatial filter. Can be NULL.
 @param pszDialect allows control of the statement dialect. If set to NULL, the
OGR SQL engine will be used, except for RDBMS drivers that will use their dedicated SQL engine,
unless OGRSQL is explicitely passed as the dialect. Starting with OGR 1.10, the SQLITE dialect
can also be used.

 @return an OGRLayer containing the results of the query.  Deallocate with
 ReleaseResultSet().

*/

OGRLayerH GDALDatasetExecuteSQL( GDALDatasetH hDS, 
                             const char *pszStatement,
                             OGRGeometryH hSpatialFilter,
                             const char *pszDialect )

{
    VALIDATE_POINTER1( hDS, "GDALDatasetExecuteSQL", NULL );

    return (OGRLayerH) 
        ((GDALDataset *)hDS)->ExecuteSQL( pszStatement,
                                            (OGRGeometry *) hSpatialFilter,
                                            pszDialect );
}

/************************************************************************/
/*                      GDALDatasetGetStyleTable()                      */
/************************************************************************/

/**
 \brief Returns dataset style table.
 
 This function is the same as the C++ method GDALDataset::GetStyleTable()
 
 @since GDAL 2.0
 
 @param hDS the dataset handle
 @return handle to a style table which should not be modified or freed by the
 caller.
*/

OGRStyleTableH GDALDatasetGetStyleTable( GDALDatasetH hDS )

{
    VALIDATE_POINTER1( hDS, "OGR_DS_GetStyleTable", NULL );
    
    return (OGRStyleTableH) ((GDALDataset *) hDS)->GetStyleTable( );
}

/************************************************************************/
/*                    GDALDatasetSetStyleTableDirectly()                */
/************************************************************************/

/**
 \brief Set dataset style table.
 
 This function operate exactly as GDALDatasetSetStyleTable() except that it
 assumes ownership of the passed table.
 
 This function is the same as the C++ method GDALDataset::SetStyleTableDirectly()
 
 @since GDAL 2.0
 
 @param hDS the dataset handle
 @param hStyleTable style table handle to set

*/

void GDALDatasetSetStyleTableDirectly( GDALDatasetH hDS,
                                   OGRStyleTableH hStyleTable )

{
    VALIDATE_POINTER0( hDS, "OGR_DS_SetStyleTableDirectly" );
    
    ((GDALDataset *) hDS)->SetStyleTableDirectly( (OGRStyleTable *) hStyleTable);
}

/************************************************************************/
/*                     GDALDatasetSetStyleTable()                       */
/************************************************************************/

/**
 \brief Set dataset style table.
 
 This function operate exactly as GDALDatasetSetStyleTableDirectly() except that it
 assumes ownership of the passed table.
 
 This function is the same as the C++ method GDALDataset::SetStyleTable()
 
 @since GDAL 2.0
 
 @param hDS the dataset handle
 @param hStyleTable style table handle to set

*/

void GDALDatasetSetStyleTable( GDALDatasetH hDS, OGRStyleTableH hStyleTable )

{
    VALIDATE_POINTER0( hDS, "OGR_DS_SetStyleTable" );
    VALIDATE_POINTER0( hStyleTable, "OGR_DS_SetStyleTable" );
    
    ((GDALDataset *) hDS)->SetStyleTable( (OGRStyleTable *) hStyleTable);
}

/************************************************************************/
/*                    ValidateLayerCreationOptions()                    */
/************************************************************************/

int GDALDataset::ValidateLayerCreationOptions( const char* const* papszLCO )
{
    const char *pszOptionList = GetMetadataItem( GDAL_DS_LAYER_CREATIONOPTIONLIST );
    if( pszOptionList == NULL && poDriver != NULL )
    {
        pszOptionList = 
             poDriver->GetMetadataItem( GDAL_DS_LAYER_CREATIONOPTIONLIST );
    }
    CPLString osDataset;
    osDataset.Printf("dataset %s", GetDescription());
    return GDALValidateOptions( pszOptionList, papszLCO,
                                "layer creation option",
                                osDataset );
}

/************************************************************************/
/*                              Release()                               */
/************************************************************************/

/**
 \fn OGRErr OGRDataSource::Release();

\brief Drop a reference to this dataset, and if the reference count drops to one close (destroy) the dataset.

This method is the same as the C function OGRReleaseDataSource().

@deprecated. In GDAL 2, use GDALClose() instead

@return OGRERR_NONE on success or an error code. 
*/

OGRErr GDALDataset::Release()

{
    GDALClose( (GDALDatasetH) this );
    return OGRERR_NONE;
}

/************************************************************************/
/*                            GetRefCount()                             */
/************************************************************************/

/**
\brief Fetch reference count.

This method is the same as the C function OGR_DS_GetRefCount().

In GDAL 1.X, this method used to be in the OGRDataSource class.

@return the current reference count for the datasource object itself.
*/


int GDALDataset::GetRefCount() const

{
    return nRefCount;
}

/************************************************************************/
/*                         GetSummaryRefCount()                         */
/************************************************************************/

/**
\brief Fetch reference count of datasource and all owned layers.

This method is the same as the C function  OGR_DS_GetSummaryRefCount().

In GDAL 1.X, this method used to be in the OGRDataSource class.

@deprecated
 
@return the current summary reference count for the datasource and its layers.
*/

int GDALDataset::GetSummaryRefCount() const

{
    CPLMutexHolderD( (CPLMutex**) &m_hMutex );
    int nSummaryCount = nRefCount;
    int iLayer;
    GDALDataset *poUseThis = (GDALDataset *) this;

    for( iLayer=0; iLayer < poUseThis->GetLayerCount(); iLayer++ )
        nSummaryCount += poUseThis->GetLayer( iLayer )->GetRefCount();

    return nSummaryCount;
}

/************************************************************************/
/*                           ICreateLayer()                             */
/************************************************************************/

/**
\brief This method attempts to create a new layer on the dataset with the indicated name, coordinate system, geometry type.

This method is reserved to implementation by drivers.

The papszOptions argument
can be used to control driver specific creation options.  These options are
normally documented in the format specific documentation. 

 @param pszName the name for the new layer.  This should ideally not 
match any existing layer on the datasource.
 @param poSpatialRef the coordinate system to use for the new layer, or NULL if
no coordinate system is available. 
 @param eGType the geometry type for the layer.  Use wkbUnknown if there
are no constraints on the types geometry to be written. 
 @param papszOptions a StringList of name=value options.  Options are driver
specific.

 @return NULL is returned on failure, or a new OGRLayer handle on success. 
 
 @since GDAL 2.0
*/

OGRLayer *GDALDataset::ICreateLayer( const char * pszName,
                                      OGRSpatialReference * poSpatialRef,
                                      OGRwkbGeometryType eGType,
                                      char **papszOptions )

{
    (void) eGType;
    (void) poSpatialRef;
    (void) pszName;
    (void) papszOptions;

    CPLError( CE_Failure, CPLE_NotSupported,
              "CreateLayer() not supported by this dataset." );
              
    return NULL;
}

/************************************************************************/
/*                             CopyLayer()                              */
/************************************************************************/

/**
 \brief Duplicate an existing layer.

 This method creates a new layer, duplicate the field definitions of the
 source layer and then duplicate each features of the source layer.
 The papszOptions argument
 can be used to control driver specific creation options.  These options are
 normally documented in the format specific documentation.
 The source layer may come from another dataset.

 This method is the same as the C function GDALDatasetCopyLayer() and the
 deprecated OGR_DS_CopyLayer().

 In GDAL 1.X, this method used to be in the OGRDataSource class.
 
 @param poSrcLayer source layer.
 @param pszNewName the name of the layer to create.
 @param papszOptions a StringList of name=value options.  Options are driver
                     specific.

 @return an handle to the layer, or NULL if an error occurs.
*/

OGRLayer *GDALDataset::CopyLayer( OGRLayer *poSrcLayer, 
                                    const char *pszNewName, 
                                    char **papszOptions )

{
    OGRFeatureDefn *poSrcDefn = poSrcLayer->GetLayerDefn();
    OGRLayer *poDstLayer = NULL;

/* -------------------------------------------------------------------- */
/*      Create the layer.                                               */
/* -------------------------------------------------------------------- */
    if( !TestCapability( ODsCCreateLayer ) )
    {
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "This datasource does not support creation of layers." );
        return NULL;
    }

    CPLErrorReset();
    if( poSrcDefn->GetGeomFieldCount() > 1 &&
        TestCapability(ODsCCreateGeomFieldAfterCreateLayer) )
    {
        poDstLayer =ICreateLayer( pszNewName, NULL, wkbNone, papszOptions );
    }
    else
    {
        poDstLayer =ICreateLayer( pszNewName, poSrcLayer->GetSpatialRef(),
                                  poSrcDefn->GetGeomType(), papszOptions );
    }
    
    if( poDstLayer == NULL )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Add fields.  Default to copy all fields, and make sure to       */
/*      establish a mapping between indices, rather than names, in      */
/*      case the target datasource has altered it (e.g. Shapefile       */
/*      limited to 10 char field names).                                */
/* -------------------------------------------------------------------- */
    int         nSrcFieldCount = poSrcDefn->GetFieldCount();
    int         nDstFieldCount = 0;
    int         iField, *panMap;

    // Initialize the index-to-index map to -1's
    panMap = (int *) CPLMalloc( sizeof(int) * nSrcFieldCount );
    for( iField=0; iField < nSrcFieldCount; iField++)
        panMap[iField] = -1;

    /* Caution : at the time of writing, the MapInfo driver */
    /* returns NULL until a field has been added */
    OGRFeatureDefn* poDstFDefn = poDstLayer->GetLayerDefn();
    if (poDstFDefn)
        nDstFieldCount = poDstFDefn->GetFieldCount();    
    for( iField = 0; iField < nSrcFieldCount; iField++ )
    {
        OGRFieldDefn* poSrcFieldDefn = poSrcDefn->GetFieldDefn(iField);
        OGRFieldDefn oFieldDefn( poSrcFieldDefn );

        /* The field may have been already created at layer creation */
        int iDstField = -1;
        if (poDstFDefn)
            iDstField = poDstFDefn->GetFieldIndex(oFieldDefn.GetNameRef());
        if (iDstField >= 0)
        {
            panMap[iField] = iDstField;
        }
        else if (poDstLayer->CreateField( &oFieldDefn ) == OGRERR_NONE)
        {
            /* now that we've created a field, GetLayerDefn() won't return NULL */
            if (poDstFDefn == NULL)
                poDstFDefn = poDstLayer->GetLayerDefn();

            /* Sanity check : if it fails, the driver is buggy */
            if (poDstFDefn != NULL &&
                poDstFDefn->GetFieldCount() != nDstFieldCount + 1)
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "The output driver has claimed to have added the %s field, but it did not!",
                         oFieldDefn.GetNameRef() );
            }
            else
            {
                panMap[iField] = nDstFieldCount;
                nDstFieldCount ++;
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Create geometry fields.                                         */
/* -------------------------------------------------------------------- */
    if( poSrcDefn->GetGeomFieldCount() > 1 &&
        TestCapability(ODsCCreateGeomFieldAfterCreateLayer) )
    {
        int nSrcGeomFieldCount = poSrcDefn->GetGeomFieldCount();
        for( iField = 0; iField < nSrcGeomFieldCount; iField++ )
        {
            poDstLayer->CreateGeomField( poSrcDefn->GetGeomFieldDefn(iField) );
        }
    }

/* -------------------------------------------------------------------- */
/*      Check if the destination layer supports transactions and set a  */
/*      default number of features in a single transaction.             */
/* -------------------------------------------------------------------- */
    int nGroupTransactions = 0;
    if( poDstLayer->TestCapability( OLCTransactions ) )
        nGroupTransactions = 128;

/* -------------------------------------------------------------------- */
/*      Transfer features.                                              */
/* -------------------------------------------------------------------- */
    OGRFeature  *poFeature;

    poSrcLayer->ResetReading();

    if( nGroupTransactions <= 0 )
    {
      while( TRUE )
      {
        OGRFeature      *poDstFeature = NULL;

        poFeature = poSrcLayer->GetNextFeature();
        
        if( poFeature == NULL )
            break;

        CPLErrorReset();
        poDstFeature = OGRFeature::CreateFeature( poDstLayer->GetLayerDefn() );

        if( poDstFeature->SetFrom( poFeature, panMap, TRUE ) != OGRERR_NONE )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Unable to translate feature " CPL_FRMT_GIB " from layer %s.\n",
                      poFeature->GetFID(), poSrcDefn->GetName() );
            OGRFeature::DestroyFeature( poFeature );
            CPLFree(panMap);
            return poDstLayer;
        }

        poDstFeature->SetFID( poFeature->GetFID() );

        OGRFeature::DestroyFeature( poFeature );

        CPLErrorReset();
        if( poDstLayer->CreateFeature( poDstFeature ) != OGRERR_NONE )
        {
            OGRFeature::DestroyFeature( poDstFeature );
            CPLFree(panMap);
            return poDstLayer;
        }

        OGRFeature::DestroyFeature( poDstFeature );
      }
    }
    else
    {
      int i, bStopTransfer = FALSE, bStopTransaction = FALSE;
      int nFeatCount = 0; // Number of features in the temporary array
      int nFeaturesToAdd = 0;
      OGRFeature **papoDstFeature =
          (OGRFeature **)CPLCalloc(sizeof(OGRFeature *), nGroupTransactions);
      while( !bStopTransfer )
      {
/* -------------------------------------------------------------------- */
/*      Fill the array with features                                    */
/* -------------------------------------------------------------------- */
        for( nFeatCount = 0; nFeatCount < nGroupTransactions; nFeatCount++ )
        {
            poFeature = poSrcLayer->GetNextFeature();

            if( poFeature == NULL )
            {
                bStopTransfer = 1;
                break;
            }

            CPLErrorReset();
            papoDstFeature[nFeatCount] =
                        OGRFeature::CreateFeature( poDstLayer->GetLayerDefn() );

            if( papoDstFeature[nFeatCount]->SetFrom( poFeature, panMap, TRUE ) != OGRERR_NONE )
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                          "Unable to translate feature " CPL_FRMT_GIB " from layer %s.\n",
                          poFeature->GetFID(), poSrcDefn->GetName() );
                OGRFeature::DestroyFeature( poFeature );
                bStopTransfer = TRUE;
                break;
            }

            papoDstFeature[nFeatCount]->SetFID( poFeature->GetFID() );

            OGRFeature::DestroyFeature( poFeature );
        }
        nFeaturesToAdd = nFeatCount;

        CPLErrorReset();
        bStopTransaction = FALSE;
        while( !bStopTransaction )
        {
            bStopTransaction = TRUE;
            poDstLayer->StartTransaction();
            for( i = 0; i < nFeaturesToAdd; i++ )
            {
                if( poDstLayer->CreateFeature( papoDstFeature[i] ) != OGRERR_NONE )
                {
                    nFeaturesToAdd = i;
                    bStopTransfer = TRUE;
                    bStopTransaction = FALSE;
                }
            }
            if( bStopTransaction )
                poDstLayer->CommitTransaction();
            else
                poDstLayer->RollbackTransaction();
        }

        for( i = 0; i < nFeatCount; i++ )
            OGRFeature::DestroyFeature( papoDstFeature[i] );
      }
      CPLFree(papoDstFeature);
    }

    CPLFree(panMap);

    return poDstLayer;
}

/************************************************************************/
/*                            DeleteLayer()                             */
/************************************************************************/

/**
 \brief Delete the indicated layer from the datasource.

 If this method is supported
 the ODsCDeleteLayer capability will test TRUE on the GDALDataset.

 This method is the same as the C function GDALDatasetDeleteLayer() and the
 deprecated OGR_DS_DeleteLayer().
 
 In GDAL 1.X, this method used to be in the OGRDataSource class.

 @param iLayer the index of the layer to delete. 

 @return OGRERR_NONE on success, or OGRERR_UNSUPPORTED_OPERATION if deleting
 layers is not supported for this datasource.

*/
OGRErr GDALDataset::DeleteLayer( int iLayer )

{
    (void) iLayer;
    CPLError( CE_Failure, CPLE_NotSupported,
              "DeleteLayer() not supported by this dataset." );
              
    return OGRERR_UNSUPPORTED_OPERATION;
}

/************************************************************************/
/*                           GetLayerByName()                           */
/************************************************************************/

/**
 \brief Fetch a layer by name.

 The returned layer remains owned by the 
 GDALDataset and should not be deleted by the application.

 This method is the same as the C function GDALDatasetGetLayerByName() and the
 deprecated OGR_DS_GetLayerByName().
 
 In GDAL 1.X, this method used to be in the OGRDataSource class.

 @param pszLayerName the layer name of the layer to fetch.

 @return the layer, or NULL if Layer is not found or an error occurs.
*/

OGRLayer *GDALDataset::GetLayerByName( const char *pszName )

{
    CPLMutexHolderD( &m_hMutex );

    if ( ! pszName )
        return NULL;

    int  i;

    /* first a case sensitive check */
    for( i = 0; i < GetLayerCount(); i++ )
    {
        OGRLayer *poLayer = GetLayer(i);

        if( strcmp( pszName, poLayer->GetName() ) == 0 )
            return poLayer;
    }

    /* then case insensitive */
    for( i = 0; i < GetLayerCount(); i++ )
    {
        OGRLayer *poLayer = GetLayer(i);

        if( EQUAL( pszName, poLayer->GetName() ) )
            return poLayer;
    }

    return NULL;
}

/************************************************************************/
/*                       ProcessSQLCreateIndex()                        */
/*                                                                      */
/*      The correct syntax for creating an index in our dialect of      */
/*      SQL is:                                                         */
/*                                                                      */
/*        CREATE INDEX ON <layername> USING <columnname>                */
/************************************************************************/

OGRErr GDALDataset::ProcessSQLCreateIndex( const char *pszSQLCommand )

{
    char **papszTokens = CSLTokenizeString( pszSQLCommand );

/* -------------------------------------------------------------------- */
/*      Do some general syntax checking.                                */
/* -------------------------------------------------------------------- */
    if( CSLCount(papszTokens) != 6 
        || !EQUAL(papszTokens[0],"CREATE")
        || !EQUAL(papszTokens[1],"INDEX")
        || !EQUAL(papszTokens[2],"ON")
        || !EQUAL(papszTokens[4],"USING") )
    {
        CSLDestroy( papszTokens );
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Syntax error in CREATE INDEX command.\n"
                  "Was '%s'\n"
                  "Should be of form 'CREATE INDEX ON <table> USING <field>'",
                  pszSQLCommand );
        return OGRERR_FAILURE;
    }

/* -------------------------------------------------------------------- */
/*      Find the named layer.                                           */
/* -------------------------------------------------------------------- */
    int  i;
    OGRLayer *poLayer = NULL;

    {
        CPLMutexHolderD( &m_hMutex );

        for( i = 0; i < GetLayerCount(); i++ )
        {
            poLayer = GetLayer(i);
            
            if( EQUAL(poLayer->GetName(),papszTokens[3]) )
                break;
        }
        
        if( i >= GetLayerCount() )
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "CREATE INDEX ON failed, no such layer as `%s'.",
                      papszTokens[3] );
            CSLDestroy( papszTokens );
            return OGRERR_FAILURE;
        }
    }

/* -------------------------------------------------------------------- */
/*      Does this layer even support attribute indexes?                 */
/* -------------------------------------------------------------------- */
    if( poLayer->GetIndex() == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "CREATE INDEX ON not supported by this driver." );
        CSLDestroy( papszTokens );
        return OGRERR_FAILURE;
    }

/* -------------------------------------------------------------------- */
/*      Find the named field.                                           */
/* -------------------------------------------------------------------- */
    for( i = 0; i < poLayer->GetLayerDefn()->GetFieldCount(); i++ )
    {
        if( EQUAL(papszTokens[5],
                  poLayer->GetLayerDefn()->GetFieldDefn(i)->GetNameRef()) )
            break;
    }

    CSLDestroy( papszTokens );

    if( i >= poLayer->GetLayerDefn()->GetFieldCount() )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "`%s' failed, field not found.",
                  pszSQLCommand );
        return OGRERR_FAILURE;
    }

/* -------------------------------------------------------------------- */
/*      Attempt to create the index.                                    */
/* -------------------------------------------------------------------- */
    OGRErr eErr;

    eErr = poLayer->GetIndex()->CreateIndex( i );
    if( eErr == OGRERR_NONE )
        eErr = poLayer->GetIndex()->IndexAllFeatures( i );
    else
    {
        if( strlen(CPLGetLastErrorMsg()) == 0 )
            CPLError( CE_Failure, CPLE_AppDefined,
                    "Cannot '%s'", pszSQLCommand);
    }

    return eErr;
}

/************************************************************************/
/*                        ProcessSQLDropIndex()                         */
/*                                                                      */
/*      The correct syntax for droping one or more indexes in           */
/*      the OGR SQL dialect is:                                         */
/*                                                                      */
/*          DROP INDEX ON <layername> [USING <columnname>]              */
/************************************************************************/

OGRErr GDALDataset::ProcessSQLDropIndex( const char *pszSQLCommand )

{
    char **papszTokens = CSLTokenizeString( pszSQLCommand );

/* -------------------------------------------------------------------- */
/*      Do some general syntax checking.                                */
/* -------------------------------------------------------------------- */
    if( (CSLCount(papszTokens) != 4 && CSLCount(papszTokens) != 6)
        || !EQUAL(papszTokens[0],"DROP")
        || !EQUAL(papszTokens[1],"INDEX")
        || !EQUAL(papszTokens[2],"ON") 
        || (CSLCount(papszTokens) == 6 && !EQUAL(papszTokens[4],"USING")) )
    {
        CSLDestroy( papszTokens );
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Syntax error in DROP INDEX command.\n"
                  "Was '%s'\n"
                  "Should be of form 'DROP INDEX ON <table> [USING <field>]'",
                  pszSQLCommand );
        return OGRERR_FAILURE;
    }

/* -------------------------------------------------------------------- */
/*      Find the named layer.                                           */
/* -------------------------------------------------------------------- */
    int  i;
    OGRLayer *poLayer=NULL;

    {
        CPLMutexHolderD( &m_hMutex );

        for( i = 0; i < GetLayerCount(); i++ )
        {
            poLayer = GetLayer(i);
        
            if( EQUAL(poLayer->GetName(),papszTokens[3]) )
                break;
        }

        if( i >= GetLayerCount() )
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "CREATE INDEX ON failed, no such layer as `%s'.",
                      papszTokens[3] );
            CSLDestroy( papszTokens );
            return OGRERR_FAILURE;
        }
    }

/* -------------------------------------------------------------------- */
/*      Does this layer even support attribute indexes?                 */
/* -------------------------------------------------------------------- */
    if( poLayer->GetIndex() == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Indexes not supported by this driver." );
        CSLDestroy( papszTokens );
        return OGRERR_FAILURE;
    }

/* -------------------------------------------------------------------- */
/*      If we weren't given a field name, drop all indexes.             */
/* -------------------------------------------------------------------- */
    OGRErr eErr;

    if( CSLCount(papszTokens) == 4 )
    {
        for( i = 0; i < poLayer->GetLayerDefn()->GetFieldCount(); i++ )
        {
            OGRAttrIndex *poAttrIndex;

            poAttrIndex = poLayer->GetIndex()->GetFieldIndex(i);
            if( poAttrIndex != NULL )
            {
                eErr = poLayer->GetIndex()->DropIndex( i );
                if( eErr != OGRERR_NONE )
                {
                    CSLDestroy(papszTokens);
                    return eErr;
                }
            }
        }

        CSLDestroy(papszTokens);
        return OGRERR_NONE;
    }

/* -------------------------------------------------------------------- */
/*      Find the named field.                                           */
/* -------------------------------------------------------------------- */
    for( i = 0; i < poLayer->GetLayerDefn()->GetFieldCount(); i++ )
    {
        if( EQUAL(papszTokens[5],
                  poLayer->GetLayerDefn()->GetFieldDefn(i)->GetNameRef()) )
            break;
    }

    CSLDestroy( papszTokens );

    if( i >= poLayer->GetLayerDefn()->GetFieldCount() )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "`%s' failed, field not found.",
                  pszSQLCommand );
        return OGRERR_FAILURE;
    }

/* -------------------------------------------------------------------- */
/*      Attempt to drop the index.                                      */
/* -------------------------------------------------------------------- */
    eErr = poLayer->GetIndex()->DropIndex( i );

    return eErr;
}

/************************************************************************/
/*                        ProcessSQLDropTable()                         */
/*                                                                      */
/*      The correct syntax for dropping a table (layer) in the OGR SQL  */
/*      dialect is:                                                     */
/*                                                                      */
/*          DROP TABLE <layername>                                      */
/************************************************************************/

OGRErr GDALDataset::ProcessSQLDropTable( const char *pszSQLCommand )

{
    char **papszTokens = CSLTokenizeString( pszSQLCommand );

/* -------------------------------------------------------------------- */
/*      Do some general syntax checking.                                */
/* -------------------------------------------------------------------- */
    if( CSLCount(papszTokens) != 3
        || !EQUAL(papszTokens[0],"DROP")
        || !EQUAL(papszTokens[1],"TABLE") )
    {
        CSLDestroy( papszTokens );
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Syntax error in DROP TABLE command.\n"
                  "Was '%s'\n"
                  "Should be of form 'DROP TABLE <table>'",
                  pszSQLCommand );
        return OGRERR_FAILURE;
    }

/* -------------------------------------------------------------------- */
/*      Find the named layer.                                           */
/* -------------------------------------------------------------------- */
    int  i;
    OGRLayer *poLayer=NULL;

    for( i = 0; i < GetLayerCount(); i++ )
    {
        poLayer = GetLayer(i);
        
        if( EQUAL(poLayer->GetName(),papszTokens[2]) )
            break;
    }
    
    if( i >= GetLayerCount() )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "DROP TABLE failed, no such layer as `%s'.",
                  papszTokens[2] );
        CSLDestroy( papszTokens );
        return OGRERR_FAILURE;
    }

    CSLDestroy( papszTokens );

/* -------------------------------------------------------------------- */
/*      Delete it.                                                      */
/* -------------------------------------------------------------------- */

    return DeleteLayer( i );
}

/************************************************************************/
/*                    GDALDatasetParseSQLType()                       */
/************************************************************************/

/* All arguments will be altered */
static OGRFieldType GDALDatasetParseSQLType(char* pszType, int& nWidth, int &nPrecision)
{
    char* pszParenthesis = strchr(pszType, '(');
    if (pszParenthesis)
    {
        nWidth = atoi(pszParenthesis + 1);
        *pszParenthesis = '\0';
        char* pszComma = strchr(pszParenthesis + 1, ',');
        if (pszComma)
            nPrecision = atoi(pszComma + 1);
    }

    OGRFieldType eType = OFTString;
    if (EQUAL(pszType, "INTEGER"))
        eType = OFTInteger;
    else if (EQUAL(pszType, "INTEGER[]"))
        eType = OFTIntegerList;
    else if (EQUAL(pszType, "FLOAT") ||
             EQUAL(pszType, "NUMERIC") ||
             EQUAL(pszType, "DOUBLE") /* unofficial alias */ ||
             EQUAL(pszType, "REAL") /* unofficial alias */)
        eType = OFTReal;
    else if (EQUAL(pszType, "FLOAT[]") ||
             EQUAL(pszType, "NUMERIC[]") ||
             EQUAL(pszType, "DOUBLE[]") /* unofficial alias */ ||
             EQUAL(pszType, "REAL[]") /* unofficial alias */)
        eType = OFTRealList;
    else if (EQUAL(pszType, "CHARACTER") ||
             EQUAL(pszType, "TEXT") /* unofficial alias */ ||
             EQUAL(pszType, "STRING") /* unofficial alias */ ||
             EQUAL(pszType, "VARCHAR") /* unofficial alias */)
        eType = OFTString;
    else if (EQUAL(pszType, "TEXT[]") ||
             EQUAL(pszType, "STRING[]") /* unofficial alias */||
             EQUAL(pszType, "VARCHAR[]") /* unofficial alias */)
        eType = OFTStringList;
    else if (EQUAL(pszType, "DATE"))
        eType = OFTDate;
    else if (EQUAL(pszType, "TIME"))
        eType = OFTTime;
    else if (EQUAL(pszType, "TIMESTAMP") ||
             EQUAL(pszType, "DATETIME") /* unofficial alias */ )
        eType = OFTDateTime;
    else
    {
        CPLError(CE_Warning, CPLE_NotSupported,
                 "Unsupported column type '%s'. Defaulting to VARCHAR",
                 pszType);
    }
    return eType;
}

/************************************************************************/
/*                    ProcessSQLAlterTableAddColumn()                   */
/*                                                                      */
/*      The correct syntax for adding a column in the OGR SQL           */
/*      dialect is:                                                     */
/*                                                                      */
/*          ALTER TABLE <layername> ADD [COLUMN] <columnname> <columntype>*/
/************************************************************************/

OGRErr GDALDataset::ProcessSQLAlterTableAddColumn( const char *pszSQLCommand )

{
    char **papszTokens = CSLTokenizeString( pszSQLCommand );

/* -------------------------------------------------------------------- */
/*      Do some general syntax checking.                                */
/* -------------------------------------------------------------------- */
    const char* pszLayerName = NULL;
    const char* pszColumnName = NULL;
    char* pszType = NULL;
    int iTypeIndex = 0;
    int nTokens = CSLCount(papszTokens);

    if( nTokens >= 7
        && EQUAL(papszTokens[0],"ALTER")
        && EQUAL(papszTokens[1],"TABLE")
        && EQUAL(papszTokens[3],"ADD")
        && EQUAL(papszTokens[4],"COLUMN"))
    {
        pszLayerName = papszTokens[2];
        pszColumnName = papszTokens[5];
        iTypeIndex = 6;
    }
    else if( nTokens >= 6
             && EQUAL(papszTokens[0],"ALTER")
             && EQUAL(papszTokens[1],"TABLE")
             && EQUAL(papszTokens[3],"ADD"))
    {
        pszLayerName = papszTokens[2];
        pszColumnName = papszTokens[4];
        iTypeIndex = 5;
    }
    else
    {
        CSLDestroy( papszTokens );
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Syntax error in ALTER TABLE ADD COLUMN command.\n"
                  "Was '%s'\n"
                  "Should be of form 'ALTER TABLE <layername> ADD [COLUMN] <columnname> <columntype>'",
                  pszSQLCommand );
        return OGRERR_FAILURE;
    }

/* -------------------------------------------------------------------- */
/*      Merge type components into a single string if there were split  */
/*      with spaces                                                     */
/* -------------------------------------------------------------------- */
    CPLString osType;
    for(int i=iTypeIndex;i<nTokens;i++)
    {
        osType += papszTokens[i];
        CPLFree(papszTokens[i]);
    }
    pszType = papszTokens[iTypeIndex] = CPLStrdup(osType);
    papszTokens[iTypeIndex + 1] = NULL;

/* -------------------------------------------------------------------- */
/*      Find the named layer.                                           */
/* -------------------------------------------------------------------- */
    OGRLayer *poLayer = GetLayerByName(pszLayerName);
    if( poLayer == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "%s failed, no such layer as `%s'.",
                  pszSQLCommand,
                  pszLayerName );
        CSLDestroy( papszTokens );
        return OGRERR_FAILURE;
    }

/* -------------------------------------------------------------------- */
/*      Add column.                                                     */
/* -------------------------------------------------------------------- */

    int nWidth = 0, nPrecision = 0;
    OGRFieldType eType = GDALDatasetParseSQLType(pszType, nWidth, nPrecision);
    OGRFieldDefn oFieldDefn(pszColumnName, eType);
    oFieldDefn.SetWidth(nWidth);
    oFieldDefn.SetPrecision(nPrecision);

    CSLDestroy( papszTokens );

    return poLayer->CreateField( &oFieldDefn );
}

/************************************************************************/
/*                    ProcessSQLAlterTableDropColumn()                  */
/*                                                                      */
/*      The correct syntax for droping a column in the OGR SQL          */
/*      dialect is:                                                     */
/*                                                                      */
/*          ALTER TABLE <layername> DROP [COLUMN] <columnname>          */
/************************************************************************/

OGRErr GDALDataset::ProcessSQLAlterTableDropColumn( const char *pszSQLCommand )

{
    char **papszTokens = CSLTokenizeString( pszSQLCommand );

/* -------------------------------------------------------------------- */
/*      Do some general syntax checking.                                */
/* -------------------------------------------------------------------- */
    const char* pszLayerName = NULL;
    const char* pszColumnName = NULL;
    if( CSLCount(papszTokens) == 6
        && EQUAL(papszTokens[0],"ALTER")
        && EQUAL(papszTokens[1],"TABLE")
        && EQUAL(papszTokens[3],"DROP")
        && EQUAL(papszTokens[4],"COLUMN"))
    {
        pszLayerName = papszTokens[2];
        pszColumnName = papszTokens[5];
    }
    else if( CSLCount(papszTokens) == 5
             && EQUAL(papszTokens[0],"ALTER")
             && EQUAL(papszTokens[1],"TABLE")
             && EQUAL(papszTokens[3],"DROP"))
    {
        pszLayerName = papszTokens[2];
        pszColumnName = papszTokens[4];
    }
    else
    {
        CSLDestroy( papszTokens );
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Syntax error in ALTER TABLE DROP COLUMN command.\n"
                  "Was '%s'\n"
                  "Should be of form 'ALTER TABLE <layername> DROP [COLUMN] <columnname>'",
                  pszSQLCommand );
        return OGRERR_FAILURE;
    }

/* -------------------------------------------------------------------- */
/*      Find the named layer.                                           */
/* -------------------------------------------------------------------- */
    OGRLayer *poLayer = GetLayerByName(pszLayerName);
    if( poLayer == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "%s failed, no such layer as `%s'.",
                  pszSQLCommand,
                  pszLayerName );
        CSLDestroy( papszTokens );
        return OGRERR_FAILURE;
    }

/* -------------------------------------------------------------------- */
/*      Find the field.                                                 */
/* -------------------------------------------------------------------- */

    int nFieldIndex = poLayer->GetLayerDefn()->GetFieldIndex(pszColumnName);
    if( nFieldIndex < 0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "%s failed, no such field as `%s'.",
                  pszSQLCommand,
                  pszColumnName );
        CSLDestroy( papszTokens );
        return OGRERR_FAILURE;
    }


/* -------------------------------------------------------------------- */
/*      Remove it.                                                      */
/* -------------------------------------------------------------------- */

    CSLDestroy( papszTokens );

    return poLayer->DeleteField( nFieldIndex );
}

/************************************************************************/
/*                 ProcessSQLAlterTableRenameColumn()                   */
/*                                                                      */
/*      The correct syntax for renaming a column in the OGR SQL         */
/*      dialect is:                                                     */
/*                                                                      */
/*       ALTER TABLE <layername> RENAME [COLUMN] <oldname> TO <newname> */
/************************************************************************/

OGRErr GDALDataset::ProcessSQLAlterTableRenameColumn( const char *pszSQLCommand )

{
    char **papszTokens = CSLTokenizeString( pszSQLCommand );

/* -------------------------------------------------------------------- */
/*      Do some general syntax checking.                                */
/* -------------------------------------------------------------------- */
    const char* pszLayerName = NULL;
    const char* pszOldColName = NULL;
    const char* pszNewColName = NULL;
    if( CSLCount(papszTokens) == 8
        && EQUAL(papszTokens[0],"ALTER")
        && EQUAL(papszTokens[1],"TABLE")
        && EQUAL(papszTokens[3],"RENAME")
        && EQUAL(papszTokens[4],"COLUMN")
        && EQUAL(papszTokens[6],"TO"))
    {
        pszLayerName = papszTokens[2];
        pszOldColName = papszTokens[5];
        pszNewColName = papszTokens[7];
    }
    else if( CSLCount(papszTokens) == 7
             && EQUAL(papszTokens[0],"ALTER")
             && EQUAL(papszTokens[1],"TABLE")
             && EQUAL(papszTokens[3],"RENAME")
             && EQUAL(papszTokens[5],"TO"))
    {
        pszLayerName = papszTokens[2];
        pszOldColName = papszTokens[4];
        pszNewColName = papszTokens[6];
    }
    else
    {
        CSLDestroy( papszTokens );
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Syntax error in ALTER TABLE RENAME COLUMN command.\n"
                  "Was '%s'\n"
                  "Should be of form 'ALTER TABLE <layername> RENAME [COLUMN] <columnname> TO <newname>'",
                  pszSQLCommand );
        return OGRERR_FAILURE;
    }

/* -------------------------------------------------------------------- */
/*      Find the named layer.                                           */
/* -------------------------------------------------------------------- */
    OGRLayer *poLayer = GetLayerByName(pszLayerName);
    if( poLayer == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "%s failed, no such layer as `%s'.",
                  pszSQLCommand,
                  pszLayerName );
        CSLDestroy( papszTokens );
        return OGRERR_FAILURE;
    }

/* -------------------------------------------------------------------- */
/*      Find the field.                                                 */
/* -------------------------------------------------------------------- */

    int nFieldIndex = poLayer->GetLayerDefn()->GetFieldIndex(pszOldColName);
    if( nFieldIndex < 0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "%s failed, no such field as `%s'.",
                  pszSQLCommand,
                  pszOldColName );
        CSLDestroy( papszTokens );
        return OGRERR_FAILURE;
    }

/* -------------------------------------------------------------------- */
/*      Rename column.                                                  */
/* -------------------------------------------------------------------- */
    OGRFieldDefn* poOldFieldDefn = poLayer->GetLayerDefn()->GetFieldDefn(nFieldIndex);
    OGRFieldDefn oNewFieldDefn(poOldFieldDefn);
    oNewFieldDefn.SetName(pszNewColName);

    CSLDestroy( papszTokens );

    return poLayer->AlterFieldDefn( nFieldIndex, &oNewFieldDefn, ALTER_NAME_FLAG );
}

/************************************************************************/
/*                 ProcessSQLAlterTableAlterColumn()                    */
/*                                                                      */
/*      The correct syntax for altering the type of a column in the     */
/*      OGR SQL dialect is:                                             */
/*                                                                      */
/*   ALTER TABLE <layername> ALTER [COLUMN] <columnname> TYPE <newtype> */
/************************************************************************/

OGRErr GDALDataset::ProcessSQLAlterTableAlterColumn( const char *pszSQLCommand )

{
    char **papszTokens = CSLTokenizeString( pszSQLCommand );

/* -------------------------------------------------------------------- */
/*      Do some general syntax checking.                                */
/* -------------------------------------------------------------------- */
    const char* pszLayerName = NULL;
    const char* pszColumnName = NULL;
    char* pszType = NULL;
    int iTypeIndex = 0;
    int nTokens = CSLCount(papszTokens);

    if( nTokens >= 8
        && EQUAL(papszTokens[0],"ALTER")
        && EQUAL(papszTokens[1],"TABLE")
        && EQUAL(papszTokens[3],"ALTER")
        && EQUAL(papszTokens[4],"COLUMN")
        && EQUAL(papszTokens[6],"TYPE"))
    {
        pszLayerName = papszTokens[2];
        pszColumnName = papszTokens[5];
        iTypeIndex = 7;
    }
    else if( nTokens >= 7
             && EQUAL(papszTokens[0],"ALTER")
             && EQUAL(papszTokens[1],"TABLE")
             && EQUAL(papszTokens[3],"ALTER")
             && EQUAL(papszTokens[5],"TYPE"))
    {
        pszLayerName = papszTokens[2];
        pszColumnName = papszTokens[4];
        iTypeIndex = 6;
    }
    else
    {
        CSLDestroy( papszTokens );
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Syntax error in ALTER TABLE ALTER COLUMN command.\n"
                  "Was '%s'\n"
                  "Should be of form 'ALTER TABLE <layername> ALTER [COLUMN] <columnname> TYPE <columntype>'",
                  pszSQLCommand );
        return OGRERR_FAILURE;
    }

/* -------------------------------------------------------------------- */
/*      Merge type components into a single string if there were split  */
/*      with spaces                                                     */
/* -------------------------------------------------------------------- */
    CPLString osType;
    for(int i=iTypeIndex;i<nTokens;i++)
    {
        osType += papszTokens[i];
        CPLFree(papszTokens[i]);
    }
    pszType = papszTokens[iTypeIndex] = CPLStrdup(osType);
    papszTokens[iTypeIndex + 1] = NULL;

/* -------------------------------------------------------------------- */
/*      Find the named layer.                                           */
/* -------------------------------------------------------------------- */
    OGRLayer *poLayer = GetLayerByName(pszLayerName);
    if( poLayer == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "%s failed, no such layer as `%s'.",
                  pszSQLCommand,
                  pszLayerName );
        CSLDestroy( papszTokens );
        return OGRERR_FAILURE;
    }

/* -------------------------------------------------------------------- */
/*      Find the field.                                                 */
/* -------------------------------------------------------------------- */

    int nFieldIndex = poLayer->GetLayerDefn()->GetFieldIndex(pszColumnName);
    if( nFieldIndex < 0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "%s failed, no such field as `%s'.",
                  pszSQLCommand,
                  pszColumnName );
        CSLDestroy( papszTokens );
        return OGRERR_FAILURE;
    }

/* -------------------------------------------------------------------- */
/*      Alter column.                                                   */
/* -------------------------------------------------------------------- */

    OGRFieldDefn* poOldFieldDefn = poLayer->GetLayerDefn()->GetFieldDefn(nFieldIndex);
    OGRFieldDefn oNewFieldDefn(poOldFieldDefn);

    int nWidth = 0, nPrecision = 0;
    OGRFieldType eType = GDALDatasetParseSQLType(pszType, nWidth, nPrecision);
    oNewFieldDefn.SetType(eType);
    oNewFieldDefn.SetWidth(nWidth);
    oNewFieldDefn.SetPrecision(nPrecision);

    int nFlags = 0;
    if (poOldFieldDefn->GetType() != oNewFieldDefn.GetType())
        nFlags |= ALTER_TYPE_FLAG;
    if (poOldFieldDefn->GetWidth() != oNewFieldDefn.GetWidth() ||
        poOldFieldDefn->GetPrecision() != oNewFieldDefn.GetPrecision())
        nFlags |= ALTER_WIDTH_PRECISION_FLAG;

    CSLDestroy( papszTokens );

    if (nFlags == 0)
        return OGRERR_NONE;
    else
        return poLayer->AlterFieldDefn( nFieldIndex, &oNewFieldDefn, nFlags );
}

/************************************************************************/
/*                             ExecuteSQL()                             */
/************************************************************************/

/**
 \brief Execute an SQL statement against the data store. 

 The result of an SQL query is either NULL for statements that are in error,
 or that have no results set, or an OGRLayer pointer representing a results
 set from the query.  Note that this OGRLayer is in addition to the layers
 in the data store and must be destroyed with 
 ReleaseResultSet() before the dataset is closed
 (destroyed).  

 This method is the same as the C function GDALDatasetExecuteSQL() and the
 deprecated OGR_DS_ExecuteSQL().

 For more information on the SQL dialect supported internally by OGR
 review the <a href="ogr_sql.html">OGR SQL</a> document.  Some drivers (ie.
 Oracle and PostGIS) pass the SQL directly through to the underlying RDBMS.

 Starting with OGR 1.10, the <a href="ogr_sql_sqlite.html">SQLITE dialect</a>
 can also be used.

 In GDAL 1.X, this method used to be in the OGRDataSource class.
 
 @param pszStatement the SQL statement to execute. 
 @param poSpatialFilter geometry which represents a spatial filter. Can be NULL.
 @param pszDialect allows control of the statement dialect. If set to NULL, the
OGR SQL engine will be used, except for RDBMS drivers that will use their dedicated SQL engine,
unless OGRSQL is explicitely passed as the dialect. Starting with OGR 1.10, the SQLITE dialect
can also be used.

 @return an OGRLayer containing the results of the query.  Deallocate with
 ReleaseResultSet().

*/

OGRLayer * GDALDataset::ExecuteSQL( const char *pszStatement,
                                      OGRGeometry *poSpatialFilter,
                                      const char *pszDialect )

{
    return ExecuteSQL(pszStatement, poSpatialFilter, pszDialect, NULL);
}

OGRLayer * GDALDataset::ExecuteSQL( const char *pszStatement,
                                    OGRGeometry *poSpatialFilter,
                                    const char *pszDialect,
                                    swq_select_parse_options* poSelectParseOptions)

{
    swq_select *psSelectInfo = NULL;

    if( pszDialect != NULL && EQUAL(pszDialect, "SQLite") )
    {
#ifdef SQLITE_ENABLED
        return OGRSQLiteExecuteSQL( this, pszStatement, poSpatialFilter, pszDialect );
#else
        CPLError(CE_Failure, CPLE_NotSupported,
                 "The SQLite driver needs to be compiled to support the SQLite SQL dialect");
        return NULL;
#endif
    }

/* -------------------------------------------------------------------- */
/*      Handle CREATE INDEX statements specially.                       */
/* -------------------------------------------------------------------- */
    if( EQUALN(pszStatement,"CREATE INDEX",12) )
    {
        ProcessSQLCreateIndex( pszStatement );
        return NULL;
    }
    
/* -------------------------------------------------------------------- */
/*      Handle DROP INDEX statements specially.                         */
/* -------------------------------------------------------------------- */
    if( EQUALN(pszStatement,"DROP INDEX",10) )
    {
        ProcessSQLDropIndex( pszStatement );
        return NULL;
    }
    
/* -------------------------------------------------------------------- */
/*      Handle DROP TABLE statements specially.                         */
/* -------------------------------------------------------------------- */
    if( EQUALN(pszStatement,"DROP TABLE",10) )
    {
        ProcessSQLDropTable( pszStatement );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Handle ALTER TABLE statements specially.                        */
/* -------------------------------------------------------------------- */
    if( EQUALN(pszStatement,"ALTER TABLE",11) )
    {
        char **papszTokens = CSLTokenizeString( pszStatement );
        if( CSLCount(papszTokens) >= 4 &&
            EQUAL(papszTokens[3],"ADD") )
        {
            ProcessSQLAlterTableAddColumn( pszStatement );
            CSLDestroy(papszTokens);
            return NULL;
        }
        else if( CSLCount(papszTokens) >= 4 &&
                 EQUAL(papszTokens[3],"DROP") )
        {
            ProcessSQLAlterTableDropColumn( pszStatement );
            CSLDestroy(papszTokens);
            return NULL;
        }
        else if( CSLCount(papszTokens) >= 4 &&
                 EQUAL(papszTokens[3],"RENAME") )
        {
            ProcessSQLAlterTableRenameColumn( pszStatement );
            CSLDestroy(papszTokens);
            return NULL;
        }
        else if( CSLCount(papszTokens) >= 4 &&
                 EQUAL(papszTokens[3],"ALTER") )
        {
            ProcessSQLAlterTableAlterColumn( pszStatement );
            CSLDestroy(papszTokens);
            return NULL;
        }
        else
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Unsupported ALTER TABLE command : %s",
                      pszStatement );
            CSLDestroy(papszTokens);
            return NULL;
        }
    }
    
/* -------------------------------------------------------------------- */
/*      Preparse the SQL statement.                                     */
/* -------------------------------------------------------------------- */
    psSelectInfo = new swq_select();
    swq_custom_func_registrar* poCustomFuncRegistrar = NULL;
    if( poSelectParseOptions != NULL )
        poCustomFuncRegistrar = poSelectParseOptions->poCustomFuncRegistrar;
    if( psSelectInfo->preparse( pszStatement,
                                poCustomFuncRegistrar != NULL ) != CPLE_None )
    {
        delete psSelectInfo;
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      If there is no UNION ALL, build result layer.                   */
/* -------------------------------------------------------------------- */
    if( psSelectInfo->poOtherSelect == NULL )
    {
        return BuildLayerFromSelectInfo(psSelectInfo,
                                        poSpatialFilter,
                                        pszDialect,
                                        poSelectParseOptions);
    }

/* -------------------------------------------------------------------- */
/*      Build result union layer.                                       */
/* -------------------------------------------------------------------- */
    int nSrcLayers = 0;
    OGRLayer** papoSrcLayers = NULL;

    do
    {
        swq_select* psNextSelectInfo = psSelectInfo->poOtherSelect;
        psSelectInfo->poOtherSelect = NULL;

        OGRLayer* poLayer = BuildLayerFromSelectInfo(psSelectInfo,
                                                     poSpatialFilter,
                                                     pszDialect,
                                                     poSelectParseOptions);
        if( poLayer == NULL )
        {
            /* Each source layer owns an independant select info */
            for(int i=0;i<nSrcLayers;i++)
                delete papoSrcLayers[i];
            CPLFree(papoSrcLayers);

            /* So we just have to destroy the remaining select info */
            delete psNextSelectInfo;

            return NULL;
        }
        else
        {
            papoSrcLayers = (OGRLayer**) CPLRealloc(papoSrcLayers,
                                sizeof(OGRLayer*) * (nSrcLayers + 1));
            papoSrcLayers[nSrcLayers] = poLayer;
            nSrcLayers ++;

            psSelectInfo = psNextSelectInfo;
        }
    }
    while( psSelectInfo != NULL );

    return new OGRUnionLayer("SELECT",
                                nSrcLayers,
                                papoSrcLayers,
                                TRUE);
}

/************************************************************************/
/*                        BuildLayerFromSelectInfo()                    */
/************************************************************************/

struct GDALSQLParseInfo
{
    swq_field_list sFieldList;
    int            nExtraDSCount;
    GDALDataset**  papoExtraDS;
    char          *pszWHERE;
};

OGRLayer* GDALDataset::BuildLayerFromSelectInfo(swq_select* psSelectInfo,
                                                OGRGeometry *poSpatialFilter,
                                                const char *pszDialect,
                                                swq_select_parse_options* poSelectParseOptions)
{
    OGRGenSQLResultsLayer *poResults = NULL;
    GDALSQLParseInfo* psParseInfo = BuildParseInfo(psSelectInfo,
                                                   poSelectParseOptions);

    if( psParseInfo )
    {
        poResults = new OGRGenSQLResultsLayer( this, psSelectInfo,
                                               poSpatialFilter,
                                               psParseInfo->pszWHERE,
                                               pszDialect );
    }
    else
    {
        delete psSelectInfo;
    }
    DestroyParseInfo(psParseInfo);

    return poResults;
}

/************************************************************************/
/*                             DestroyParseInfo()                       */
/************************************************************************/

void GDALDataset::DestroyParseInfo(GDALSQLParseInfo* psParseInfo )
{
    if( psParseInfo != NULL )
    {
        CPLFree( psParseInfo->sFieldList.names );
        CPLFree( psParseInfo->sFieldList.types );
        CPLFree( psParseInfo->sFieldList.table_ids );
        CPLFree( psParseInfo->sFieldList.ids );

        /* Release the datasets we have opened with OGROpenShared() */
        /* It is safe to do that as the 'new OGRGenSQLResultsLayer' itself */
        /* has taken a reference on them, which it will release in its */
        /* destructor */
        for(int iEDS = 0; iEDS < psParseInfo->nExtraDSCount; iEDS++)
            GDALClose( (GDALDatasetH)psParseInfo->papoExtraDS[iEDS] );
        CPLFree(psParseInfo->papoExtraDS);

        CPLFree(psParseInfo->pszWHERE);

        CPLFree(psParseInfo);
    }
}

/************************************************************************/
/*                            BuildParseInfo()                          */
/************************************************************************/

GDALSQLParseInfo* GDALDataset::BuildParseInfo(swq_select* psSelectInfo,
                                              swq_select_parse_options* poSelectParseOptions)
{
    int            nFIDIndex = 0;

    GDALSQLParseInfo* psParseInfo = (GDALSQLParseInfo*)CPLCalloc(1, sizeof(GDALSQLParseInfo));

/* -------------------------------------------------------------------- */
/*      Validate that all the source tables are recognised, count       */
/*      fields.                                                         */
/* -------------------------------------------------------------------- */
    int  nFieldCount = 0, iTable, iField;

    for( iTable = 0; iTable < psSelectInfo->table_count; iTable++ )
    {
        swq_table_def *psTableDef = psSelectInfo->table_defs + iTable;
        OGRLayer *poSrcLayer;
        GDALDataset *poTableDS = this;

        if( psTableDef->data_source != NULL )
        {
            poTableDS = (GDALDataset *) 
                OGROpenShared( psTableDef->data_source, FALSE, NULL );
            if( poTableDS == NULL )
            {
                if( strlen(CPLGetLastErrorMsg()) == 0 )
                    CPLError( CE_Failure, CPLE_AppDefined, 
                              "Unable to open secondary datasource\n"
                              "`%s' required by JOIN.",
                              psTableDef->data_source );

                DestroyParseInfo(psParseInfo);
                return NULL;
            }

            /* Keep in an array to release at the end of this function */
            psParseInfo->papoExtraDS = (GDALDataset** )CPLRealloc(
                               psParseInfo->papoExtraDS,
                               sizeof(GDALDataset*) * (psParseInfo->nExtraDSCount + 1));
            psParseInfo->papoExtraDS[psParseInfo->nExtraDSCount++] = poTableDS;
        }

        poSrcLayer = poTableDS->GetLayerByName( psTableDef->table_name );

        if( poSrcLayer == NULL )
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "SELECT from table %s failed, no such table/featureclass.",
                      psTableDef->table_name );

            DestroyParseInfo(psParseInfo);
            return NULL;
        }

        nFieldCount += poSrcLayer->GetLayerDefn()->GetFieldCount();
        if( iTable == 0 || (poSelectParseOptions &&
                            poSelectParseOptions->bAddSecondaryTablesGeometryFields) )
            nFieldCount += poSrcLayer->GetLayerDefn()->GetGeomFieldCount();
    }
    
/* -------------------------------------------------------------------- */
/*      Build the field list for all indicated tables.                  */
/* -------------------------------------------------------------------- */

    psParseInfo->sFieldList.table_count = psSelectInfo->table_count;
    psParseInfo->sFieldList.table_defs = psSelectInfo->table_defs;

    psParseInfo->sFieldList.count = 0;
    psParseInfo->sFieldList.names = (char **) CPLMalloc( sizeof(char *) * (nFieldCount+SPECIAL_FIELD_COUNT) );
    psParseInfo->sFieldList.types = (swq_field_type *)  
        CPLMalloc( sizeof(swq_field_type) * (nFieldCount+SPECIAL_FIELD_COUNT) );
    psParseInfo->sFieldList.table_ids = (int *) 
        CPLMalloc( sizeof(int) * (nFieldCount+SPECIAL_FIELD_COUNT) );
    psParseInfo->sFieldList.ids = (int *) 
        CPLMalloc( sizeof(int) * (nFieldCount+SPECIAL_FIELD_COUNT) );
    
    for( iTable = 0; iTable < psSelectInfo->table_count; iTable++ )
    {
        swq_table_def *psTableDef = psSelectInfo->table_defs + iTable;
        GDALDataset *poTableDS = this;
        OGRLayer *poSrcLayer;
        
        if( psTableDef->data_source != NULL )
        {
            poTableDS = (GDALDataset *) 
                OGROpenShared( psTableDef->data_source, FALSE, NULL );
            CPLAssert( poTableDS != NULL );
            poTableDS->Dereference();
        }

        poSrcLayer = poTableDS->GetLayerByName( psTableDef->table_name );

        for( iField = 0; 
             iField < poSrcLayer->GetLayerDefn()->GetFieldCount();
             iField++ )
        {
            OGRFieldDefn *poFDefn=poSrcLayer->GetLayerDefn()->GetFieldDefn(iField);
            int iOutField = psParseInfo->sFieldList.count++;
            psParseInfo->sFieldList.names[iOutField] = (char *) poFDefn->GetNameRef();
            if( poFDefn->GetType() == OFTInteger )
            {
                if( poFDefn->GetSubType() == OFSTBoolean )
                    psParseInfo->sFieldList.types[iOutField] = SWQ_BOOLEAN;
                else
                    psParseInfo->sFieldList.types[iOutField] = SWQ_INTEGER;
            }
            else if( poFDefn->GetType() == OFTInteger64 )
            {
                if( poFDefn->GetSubType() == OFSTBoolean )
                    psParseInfo->sFieldList.types[iOutField] = SWQ_BOOLEAN;
                else
                    psParseInfo->sFieldList.types[iOutField] = SWQ_INTEGER64;
            }
            else if( poFDefn->GetType() == OFTReal )
                psParseInfo->sFieldList.types[iOutField] = SWQ_FLOAT;
            else if( poFDefn->GetType() == OFTString )
                psParseInfo->sFieldList.types[iOutField] = SWQ_STRING;
            else if( poFDefn->GetType() == OFTTime )
                psParseInfo->sFieldList.types[iOutField] = SWQ_TIME;
            else if( poFDefn->GetType() == OFTDate )
                psParseInfo->sFieldList.types[iOutField] = SWQ_DATE;
            else if( poFDefn->GetType() == OFTDateTime )
                psParseInfo->sFieldList.types[iOutField] = SWQ_TIMESTAMP;
            else
                psParseInfo->sFieldList.types[iOutField] = SWQ_OTHER;

            psParseInfo->sFieldList.table_ids[iOutField] = iTable;
            psParseInfo->sFieldList.ids[iOutField] = iField;
        }

        if( iTable == 0 || (poSelectParseOptions &&
                            poSelectParseOptions->bAddSecondaryTablesGeometryFields) )
        {
            nFIDIndex = psParseInfo->sFieldList.count;

            for( iField = 0; 
                 iField < poSrcLayer->GetLayerDefn()->GetGeomFieldCount();
                 iField++ )
            {
                OGRGeomFieldDefn *poFDefn=poSrcLayer->GetLayerDefn()->GetGeomFieldDefn(iField);
                int iOutField = psParseInfo->sFieldList.count++;
                psParseInfo->sFieldList.names[iOutField] = (char *) poFDefn->GetNameRef();
                if( *psParseInfo->sFieldList.names[iOutField] == '\0' )
                    psParseInfo->sFieldList.names[iOutField] = (char*) OGR_GEOMETRY_DEFAULT_NON_EMPTY_NAME;
                psParseInfo->sFieldList.types[iOutField] = SWQ_GEOMETRY;

                psParseInfo->sFieldList.table_ids[iOutField] = iTable;
                psParseInfo->sFieldList.ids[iOutField] =
                    GEOM_FIELD_INDEX_TO_ALL_FIELD_INDEX(poSrcLayer->GetLayerDefn(), iField);
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Expand '*' in 'SELECT *' now before we add the pseudo fields    */
/* -------------------------------------------------------------------- */
    int bAlwaysPrefixWithTableName = poSelectParseOptions &&
                                     poSelectParseOptions->bAlwaysPrefixWithTableName;
    if( psSelectInfo->expand_wildcard( &psParseInfo->sFieldList,
                                       bAlwaysPrefixWithTableName)  != CE_None )
    {
        DestroyParseInfo(psParseInfo);
        return NULL;
    }

    for (iField = 0; iField < SPECIAL_FIELD_COUNT; iField++)
    {
        psParseInfo->sFieldList.names[psParseInfo->sFieldList.count] = (char*) SpecialFieldNames[iField];
        psParseInfo->sFieldList.types[psParseInfo->sFieldList.count] = SpecialFieldTypes[iField];
        psParseInfo->sFieldList.table_ids[psParseInfo->sFieldList.count] = 0;
        psParseInfo->sFieldList.ids[psParseInfo->sFieldList.count] = nFIDIndex + iField;
        psParseInfo->sFieldList.count++;
    }
    
/* -------------------------------------------------------------------- */
/*      Finish the parse operation.                                     */
/* -------------------------------------------------------------------- */
    if( psSelectInfo->parse( &psParseInfo->sFieldList, poSelectParseOptions ) != CE_None )
    {
        DestroyParseInfo(psParseInfo);
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Extract the WHERE expression to use separately.                 */
/* -------------------------------------------------------------------- */
    if( psSelectInfo->where_expr != NULL )
    {
        psParseInfo->pszWHERE = psSelectInfo->where_expr->Unparse( &psParseInfo->sFieldList, '"' );
        //CPLDebug( "OGR", "Unparse() -> %s", pszWHERE );
    }

    return psParseInfo;
}

/************************************************************************/
/*                          ReleaseResultSet()                          */
/************************************************************************/

/**
 \brief Release results of ExecuteSQL().

 This method should only be used to deallocate OGRLayers resulting from
 an ExecuteSQL() call on the same GDALDataset.  Failure to deallocate a
 results set before destroying the GDALDataset may cause errors. 

 This method is the same as the C function GDALDatasetReleaseResultSet() and the
 deprecated OGR_DS_ReleaseResultSet().
 
 In GDAL 1.X, this method used to be in the OGRDataSource class.
 
 @param poResultsSet the result of a previous ExecuteSQL() call.

*/ 
void GDALDataset::ReleaseResultSet( OGRLayer * poResultsSet )

{
    delete poResultsSet;
}

/************************************************************************/
/*                            GetStyleTable()                           */
/************************************************************************/

/**
 \brief Returns dataset style table.
 
 This method is the same as the C function GDALDatasetGetStyleTable() and the
 deprecated OGR_DS_GetStyleTable().
 
 In GDAL 1.X, this method used to be in the OGRDataSource class.
 
 @return pointer to a style table which should not be modified or freed by the
 caller.
*/

OGRStyleTable *GDALDataset::GetStyleTable()
{
    return m_poStyleTable;
}

/************************************************************************/
/*                         SetStyleTableDirectly()                      */
/************************************************************************/

/**
 \brief Set dataset style table.
 
 This method operate exactly as SetStyleTable() except that it
 assumes ownership of the passed table.
 
 This method is the same as the C function GDALDatasetSetStyleTableDirectly() and
 the deprecated OGR_DS_SetStyleTableDirectly().
 
 In GDAL 1.X, this method used to be in the OGRDataSource class.
 
 @param poStyleTable pointer to style table to set

*/
void GDALDataset::SetStyleTableDirectly( OGRStyleTable *poStyleTable )
{
    if ( m_poStyleTable )
        delete m_poStyleTable;
    m_poStyleTable = poStyleTable;
}

/************************************************************************/
/*                            SetStyleTable()                           */
/************************************************************************/

/**
 \brief Set dataset style table.
 
 This method operate exactly as SetStyleTableDirectly() except
 that it does not assume ownership of the passed table.
 
 This method is the same as the C function GDALDatasetSetStyleTable() and the
 deprecated OGR_DS_SetStyleTable().
 
 In GDAL 1.X, this method used to be in the OGRDataSource class.
 
 @param poStyleTable pointer to style table to set

*/

void GDALDataset::SetStyleTable(OGRStyleTable *poStyleTable)
{
    if ( m_poStyleTable )
        delete m_poStyleTable;
    if ( poStyleTable )
        m_poStyleTable = poStyleTable->Clone();
}

/************************************************************************/
/*                         IsGenericSQLDialect()                        */
/************************************************************************/

int GDALDataset::IsGenericSQLDialect(const char* pszDialect)
{
    return ( pszDialect != NULL && (EQUAL(pszDialect,"OGRSQL") ||
                                    EQUAL(pszDialect,"SQLITE")) );

}

/************************************************************************/
/*                            GetLayerCount()                           */
/************************************************************************/

/**
 \brief Get the number of layers in this dataset.

 This method is the same as the C function GDALDatasetGetLayerCount(),
 and the deprecated OGR_DS_GetLayerCount().
 
 In GDAL 1.X, this method used to be in the OGRDataSource class.

 @return layer count.
*/

int GDALDataset::GetLayerCount()
{
    return 0;
}

/************************************************************************/
/*                                GetLayer()                            */
/************************************************************************/

/**
 \brief Fetch a layer by index.

 The returned layer remains owned by the 
 GDALDataset and should not be deleted by the application.

 This method is the same as the C function GDALDatasetGetLayer() and the
 deprecated OGR_DS_GetLayer().

 In GDAL 1.X, this method used to be in the OGRDataSource class.

 @param iLayer a layer number between 0 and GetLayerCount()-1.

 @return the layer, or NULL if iLayer is out of range or an error occurs.
*/

OGRLayer* GDALDataset::GetLayer(CPL_UNUSED int iLayer)
{
    return NULL;
}

/************************************************************************/
/*                            TestCapability()                          */
/************************************************************************/

/**
 \brief Test if capability is available.

 One of the following dataset capability names can be passed into this
 method, and a TRUE or FALSE value will be returned indicating whether or not
 the capability is available for this object.

 <ul>
  <li> <b>ODsCCreateLayer</b>: True if this datasource can create new layers.<p>
  <li> <b>ODsCDeleteLayer</b>: True if this datasource can delete existing layers.<p>
  <li> <b>ODsCCreateGeomFieldAfterCreateLayer</b>: True if the layers of this
        datasource support CreateGeomField() just after layer creation.<p>
  <li> <b>ODsCCurveGeometries</b>: True if this datasource supports curve geometries.<p>
  <li> <b>ODsCTransactions</b>: True if this datasource supports (efficient) transactions.<p>
  <li> <b>ODsCEmulatedTransactions</b>: True if this datasource supports transactions through emulation.<p>
 </ul>

 The \#define macro forms of the capability names should be used in preference
 to the strings themselves to avoid mispelling.

 This method is the same as the C function GDALDatasetTestCapability() and the
 deprecated OGR_DS_TestCapability().

 In GDAL 1.X, this method used to be in the OGRDataSource class.

 @param pszCapability the capability to test.

 @return TRUE if capability available otherwise FALSE.

*/

int GDALDataset::TestCapability( CPL_UNUSED const char * pszCap )
{
    return FALSE;
}

/************************************************************************/
/*                           StartTransaction()                         */
/************************************************************************/

/**
 \brief For datasources which support transactions, StartTransaction creates a transaction.

 If starting the transaction fails, will return 
 OGRERR_FAILURE. Datasources which do not support transactions will 
 always return OGRERR_UNSUPPORTED_OPERATION.

 Nested transactions are not supported.
 
 All changes done after the start of the transaction are definitely applied in the
 datasource if CommitTransaction() is called. They may be cancelled by calling
 RollbackTransaction() instead.
 
 At the time of writing, transactions only apply on vector layers.
 
 Datasets that support transactions will advertize the ODsCTransactions capability.
 Use of transactions at dataset level is generally prefered to transactions at
 layer level, whose scope is rarely limited to the layer from which it was started.
 
 In case StartTransaction() fails, neither CommitTransaction() or RollbackTransaction()
 should be called.
 
 If an error occurs after a successful StartTransaction(), the whole
 transaction may or may not be implicitely cancelled, depending on drivers. (e.g.
 the PG driver will cancel it, SQLite/GPKG not). In any case, in the event of an
 error, an explicit call to RollbackTransaction() should be done to keep things balanced.
 
 By default, when bForce is set to FALSE, only "efficient" transactions will be
 attempted. Some drivers may offer an emulation of transactions, but sometimes
 with significant overhead, in which case the user must explicitely allow for such
 an emulation by setting bForce to TRUE. Drivers that offer emulated transactions
 should advertize the ODsCEmulatedTransactions capability (and not ODsCTransactions).
 
 This function is the same as the C function GDALDatasetStartTransaction().

 @param bForce can be set to TRUE if an emulation, possibly slow, of a transaction
               mechanism is acceptable.

 @return OGRERR_NONE on success.
 @since GDAL 2.0
*/
OGRErr GDALDataset::StartTransaction(CPL_UNUSED int bForce)
{
    return OGRERR_UNSUPPORTED_OPERATION;
}

/************************************************************************/
/*                      GDALDatasetStartTransaction()                   */
/************************************************************************/

/**
 \brief For datasources which support transactions, StartTransaction creates a transaction.

 If starting the transaction fails, will return 
 OGRERR_FAILURE. Datasources which do not support transactions will 
 always return OGRERR_UNSUPPORTED_OPERATION.

 Nested transactions are not supported.
 
 All changes done after the start of the transaction are definitely applied in the
 datasource if CommitTransaction() is called. They may be cancelled by calling
 RollbackTransaction() instead.
 
 At the time of writing, transactions only apply on vector layers.
 
 Datasets that support transactions will advertize the ODsCTransactions capability.
 Use of transactions at dataset level is generally prefered to transactions at
 layer level, whose scope is rarely limited to the layer from which it was started.
 
 In case StartTransaction() fails, neither CommitTransaction() or RollbackTransaction()
 should be called.

 If an error occurs after a successful StartTransaction(), the whole
 transaction may or may not be implicitely cancelled, depending on drivers. (e.g.
 the PG driver will cancel it, SQLite/GPKG not). In any case, in the event of an
 error, an explicit call to RollbackTransaction() should be done to keep things balanced.

 By default, when bForce is set to FALSE, only "efficient" transactions will be
 attempted. Some drivers may offer an emulation of transactions, but sometimes
 with significant overhead, in which case the user must explicitely allow for such
 an emulation by setting bForce to TRUE. Drivers that offer emulated transactions
 should advertize the ODsCEmulatedTransactions capability (and not ODsCTransactions).

 This function is the same as the C++ method GDALDataset::StartTransaction()

 @param hDS the dataset handle.
 @param bForce can be set to TRUE if an emulation, possibly slow, of a transaction
               mechanism is acceptable.

 @return OGRERR_NONE on success.
 @since GDAL 2.0
*/
OGRErr GDALDatasetStartTransaction(GDALDatasetH hDS, int bForce)
{
    VALIDATE_POINTER1( hDS, "GDALDatasetStartTransaction", OGRERR_INVALID_HANDLE );

#ifdef OGRAPISPY_ENABLED
    if( bOGRAPISpyEnabled )
        OGRAPISpy_Dataset_StartTransaction(hDS, bForce);
#endif

    return ((GDALDataset*) hDS)->StartTransaction(bForce);
}

/************************************************************************/
/*                           CommitTransaction()                        */
/************************************************************************/

/**
 \brief For datasources which support transactions, CommitTransaction commits a transaction.

 If no transaction is active, or the commit fails, will return 
 OGRERR_FAILURE. Datasources which do not support transactions will 
 always return OGRERR_UNSUPPORTED_OPERATION. 
 
 Depending on drivers, this may or may not abort layer sequential readings that
 are active.

 This function is the same as the C function GDALDatasetCommitTransaction().

 @return OGRERR_NONE on success.
 @since GDAL 2.0
*/
OGRErr GDALDataset::CommitTransaction()
{
    return OGRERR_UNSUPPORTED_OPERATION;
}

/************************************************************************/
/*                        GDALDatasetCommitTransaction()                */
/************************************************************************/

/**
 \brief For datasources which support transactions, CommitTransaction commits a transaction.

 If no transaction is active, or the commit fails, will return 
 OGRERR_FAILURE. Datasources which do not support transactions will 
 always return OGRERR_UNSUPPORTED_OPERATION. 
 
 Depending on drivers, this may or may not abort layer sequential readings that
 are active.

 This function is the same as the C++ method GDALDataset::CommitTransaction()

 @return OGRERR_NONE on success.
 @since GDAL 2.0
*/
OGRErr GDALDatasetCommitTransaction(GDALDatasetH hDS)
{
    VALIDATE_POINTER1( hDS, "GDALDatasetCommitTransaction", OGRERR_INVALID_HANDLE );

#ifdef OGRAPISPY_ENABLED
    if( bOGRAPISpyEnabled )
        OGRAPISpy_Dataset_CommitTransaction(hDS);
#endif

    return ((GDALDataset*) hDS)->CommitTransaction();
}

/************************************************************************/
/*                           RollbackTransaction()                      */
/************************************************************************/

/**
 \brief For datasources which support transactions, RollbackTransaction will roll back a datasource to its state before the start of the current transaction. 
 If no transaction is active, or the rollback fails, will return  
 OGRERR_FAILURE. Datasources which do not support transactions will
 always return OGRERR_UNSUPPORTED_OPERATION. 

 This function is the same as the C function GDALDatasetRollbackTransaction().

 @return OGRERR_NONE on success.
 @since GDAL 2.0
*/
OGRErr GDALDataset::RollbackTransaction()
{
    return OGRERR_UNSUPPORTED_OPERATION;
}

/************************************************************************/
/*                     GDALDatasetRollbackTransaction()                 */
/************************************************************************/

/**
 \brief For datasources which support transactions, RollbackTransaction will roll back a datasource to its state before the start of the current transaction. 
 If no transaction is active, or the rollback fails, will return  
 OGRERR_FAILURE. Datasources which do not support transactions will
 always return OGRERR_UNSUPPORTED_OPERATION. 

 This function is the same as the C++ method GDALDataset::RollbackTransaction().

 @return OGRERR_NONE on success.
 @since GDAL 2.0
*/
OGRErr GDALDatasetRollbackTransaction(GDALDatasetH hDS)
{
    VALIDATE_POINTER1( hDS, "GDALDatasetRollbackTransaction", OGRERR_INVALID_HANDLE );

#ifdef OGRAPISPY_ENABLED
    if( bOGRAPISpyEnabled )
        OGRAPISpy_Dataset_RollbackTransaction(hDS);
#endif

    return ((GDALDataset*) hDS)->RollbackTransaction();
}
