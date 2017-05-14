/******************************************************************************
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

#include "cpl_port.h"
#include "gdal.h"
#include "gdal_priv.h"

#include <climits>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <map>
#include <new>
#include <string>
#include <utility>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_hash_set.h"
#include "cpl_multiproc.h"
#include "cpl_progress.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "cpl_vsi_error.h"
#include "ogr_api.h"
#include "ogr_attrind.h"
#include "ogr_core.h"
#include "ogr_feature.h"
#include "ogr_featurestyle.h"
#include "ogr_gensql.h"
#include "ogr_geometry.h"
#include "ogr_p.h"
#include "ogr_spatialref.h"
#include "ogr_srs_api.h"
#include "ograpispy.h"
#include "ogrsf_frmts.h"
#include "ogrunionlayer.h"
#include "swq.h"

#include "../frmts/derived/derivedlist.h"

#ifdef SQLITE_ENABLED
#include "../sqlite/ogrsqliteexecutesql.h"
#endif

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
                           int nBandSpace, char **papszOptions );
CPL_C_END

typedef enum
{
    RW_MUTEX_STATE_UNKNOWN,
    RW_MUTEX_STATE_ALLOWED,
    RW_MUTEX_STATE_DISABLED
} GDALAllowReadWriteMutexState;

const GIntBig TOTAL_FEATURES_NOT_INIT = -2;
const GIntBig TOTAL_FEATURES_UNKNOWN = -1;

class GDALDatasetPrivate
{
  public:
    CPLMutex *hMutex;
    std::map<GIntBig, int> oMapThreadToMutexTakenCount;
#ifdef DEBUG_EXTRA
    std::map<GIntBig, int> oMapThreadToMutexTakenCountSaved;
#endif
    GDALAllowReadWriteMutexState eStateReadWriteMutex;
    int nCurrentLayerIdx;
    int nLayerCount;
    GIntBig nFeatureReadInLayer;
    GIntBig nFeatureReadInDataset;
    GIntBig nTotalFeaturesInLayer;
    GIntBig nTotalFeatures;
    OGRLayer *poCurrentLayer;

    GDALDatasetPrivate() :
        hMutex(NULL),
        eStateReadWriteMutex(RW_MUTEX_STATE_UNKNOWN),
        nCurrentLayerIdx(0),
        nLayerCount(-1),
        nFeatureReadInLayer(0),
        nFeatureReadInDataset(0),
        nTotalFeaturesInLayer(TOTAL_FEATURES_NOT_INIT),
        nTotalFeatures(TOTAL_FEATURES_NOT_INIT),
        poCurrentLayer(NULL)
        {}
};

typedef struct
{
    // PID of the thread that mark the dataset as shared
    // This may not be the actual PID, but the responsiblePID.
    GIntBig nPID;
    char *pszDescription;
    GDALAccess eAccess;

    GDALDataset *poDS;
} SharedDatasetCtxt;

// Set of datasets opened as shared datasets (with GDALOpenShared)
// The values in the set are of type SharedDatasetCtxt.
static CPLHashSet *phSharedDatasetSet = NULL;

// Set of all datasets created in the constructor of GDALDataset.
// In the case of a shared dataset, memorize the PID of the thread
// that marked the dataset as shared, so that we can remove it from
// the phSharedDatasetSet in the destructor of the dataset, even
// if GDALClose is called from a different thread.
static std::map<GDALDataset *, GIntBig> *poAllDatasetMap = NULL;

static CPLMutex *hDLMutex = NULL;

// Static array of all datasets. Used by GDALGetOpenDatasets.
// Not thread-safe. See GDALGetOpenDatasets.
static GDALDataset **ppDatasets = NULL;

static unsigned long GDALSharedDatasetHashFunc( const void *elt )
{
    const SharedDatasetCtxt *psStruct =
        static_cast<const SharedDatasetCtxt *>(elt);
    return static_cast<unsigned long>(
        CPLHashSetHashStr(psStruct->pszDescription) ^ psStruct->eAccess ^
        psStruct->nPID);
}

static int GDALSharedDatasetEqualFunc( const void* elt1, const void* elt2 )
{
    const SharedDatasetCtxt *psStruct1 =
        static_cast<const SharedDatasetCtxt *>(elt1);
    const SharedDatasetCtxt *psStruct2 =
        static_cast<const SharedDatasetCtxt *>(elt2);
    return strcmp(psStruct1->pszDescription, psStruct2->pszDescription) == 0 &&
           psStruct1->nPID == psStruct2->nPID &&
           psStruct1->eAccess == psStruct2->eAccess;
}

static void GDALSharedDatasetFreeFunc( void* elt )
{
    SharedDatasetCtxt *psStruct = static_cast<SharedDatasetCtxt *>(elt);
    CPLFree(psStruct->pszDescription);
    CPLFree(psStruct);
}

/************************************************************************/
/* Functions shared between gdalproxypool.cpp and gdaldataset.cpp */
/************************************************************************/

// The open-shared mutex must be used by the ProxyPool too.
CPLMutex **GDALGetphDLMutex() { return &hDLMutex; }

// The current thread will act in the behalf of the thread of PID
// responsiblePID.
void GDALSetResponsiblePIDForCurrentThread(GIntBig responsiblePID)
{
    GIntBig *pResponsiblePID =
        static_cast<GIntBig *>(CPLGetTLS(CTLS_RESPONSIBLEPID));
    if(pResponsiblePID == NULL)
    {
        pResponsiblePID = static_cast<GIntBig *>(CPLMalloc(sizeof(GIntBig)));
        CPLSetTLS(CTLS_RESPONSIBLEPID, pResponsiblePID, TRUE);
    }
    *pResponsiblePID = responsiblePID;
}

// Get the PID of the thread that the current thread will act in the behalf of
// By default : the current thread acts in the behalf of itself.
GIntBig GDALGetResponsiblePIDForCurrentThread()
{
    GIntBig *pResponsiblePID =
        static_cast<GIntBig *>(CPLGetTLS(CTLS_RESPONSIBLEPID));
    if( pResponsiblePID == NULL )
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

//! @cond Doxygen_Suppress
GDALDataset::GDALDataset()

{
    Init(CPLTestBool(CPLGetConfigOption("GDAL_FORCE_CACHING", "NO")));
}

GDALDataset::GDALDataset(int bForceCachedIOIn)

{
    Init(CPL_TO_BOOL(bForceCachedIOIn));
}

void GDALDataset::Init(bool bForceCachedIOIn)
{
    poDriver = NULL;
    eAccess = GA_ReadOnly;
    nRasterXSize = 512;
    nRasterYSize = 512;
    nBands = 0;
    papoBands = NULL;
    nRefCount = 1;
    nOpenFlags = 0;
    bShared = false;
    bIsInternal = true;
    bSuppressOnClose = false;
    papszOpenOptions = NULL;

/* -------------------------------------------------------------------- */
/*      Set forced caching flag.                                        */
/* -------------------------------------------------------------------- */
    bForceCachedIO = bForceCachedIOIn;

    m_poStyleTable = NULL;
    m_hPrivateData = new(std::nothrow) GDALDatasetPrivate;
}
//! @endcond

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
 * dataset object because of known issues when allocating and freeing memory
 * across module boundaries. Calling GDALClose() is then a better option.
 */

GDALDataset::~GDALDataset()

{
    // we don't want to report destruction of datasets that
    // were never really open or meant as internal
    if( !bIsInternal && (nBands != 0 || !EQUAL(GetDescription(), "")) )
    {
        if( CPLGetPID() != GDALGetResponsiblePIDForCurrentThread() )
            CPLDebug("GDAL",
                     "GDALClose(%s, this=%p) (pid=%d, responsiblePID=%d)",
                     GetDescription(), this, static_cast<int>(CPLGetPID()),
                     static_cast<int>(GDALGetResponsiblePIDForCurrentThread()));
        else
            CPLDebug("GDAL", "GDALClose(%s, this=%p)", GetDescription(), this);
    }

    if( bSuppressOnClose )
        VSIUnlink(GetDescription());

/* -------------------------------------------------------------------- */
/*      Remove dataset from the "open" dataset list.                    */
/* -------------------------------------------------------------------- */
    if( !bIsInternal )
    {
        CPLMutexHolderD(&hDLMutex);
        if( poAllDatasetMap )
        {
            std::map<GDALDataset *, GIntBig>::iterator oIter =
                poAllDatasetMap->find(this);
            CPLAssert(oIter != poAllDatasetMap->end());
            GIntBig nPIDCreatorForShared = oIter->second;
            poAllDatasetMap->erase(oIter);

            if( bShared && phSharedDatasetSet != NULL )
            {
                SharedDatasetCtxt sStruct;
                sStruct.nPID = nPIDCreatorForShared;
                sStruct.eAccess = eAccess;
                sStruct.pszDescription = const_cast<char *>(GetDescription());
                SharedDatasetCtxt *psStruct = static_cast<SharedDatasetCtxt *>(
                    CPLHashSetLookup(phSharedDatasetSet, &sStruct));
                if( psStruct && psStruct->poDS == this )
                {
                    CPLHashSetRemove(phSharedDatasetSet, psStruct);
                }
                else
                {
                    CPLDebug("GDAL", "Should not happen. Cannot find %s, "
                                     "this=%p in phSharedDatasetSet",
                             GetDescription(), this);
                }
            }

            if (poAllDatasetMap->empty())
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
    for( int i = 0; i < nBands && papoBands != NULL; ++i )
    {
        if( papoBands[i] != NULL )
            delete papoBands[i];
    }

    CPLFree(papoBands);

    if ( m_poStyleTable )
    {
        delete m_poStyleTable;
        m_poStyleTable = NULL;
    }

    GDALDatasetPrivate *psPrivate =
        static_cast<GDALDatasetPrivate *>(m_hPrivateData);
    if( psPrivate != NULL && psPrivate->hMutex != NULL )
        CPLDestroyMutex(psPrivate->hMutex);
    delete psPrivate;

    CSLDestroy(papszOpenOptions);
}

/************************************************************************/
/*                      AddToDatasetOpenList()                          */
/************************************************************************/

void GDALDataset::AddToDatasetOpenList()
{
/* -------------------------------------------------------------------- */
/*      Add this dataset to the open dataset list.                      */
/* -------------------------------------------------------------------- */
    bIsInternal = false;

    CPLMutexHolderD(&hDLMutex);

    if (poAllDatasetMap == NULL)
        poAllDatasetMap = new std::map<GDALDataset *, GIntBig>;
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
    // This sometimes happens if a dataset is destroyed before completely
    // built.

    if( papoBands != NULL )
    {
        for( int i = 0; i < nBands; ++i )
        {
            if( papoBands[i] != NULL )
                papoBands[i]->FlushCache();
        }
    }

    const int nLayers = GetLayerCount();
    // cppcheck-suppress knownConditionTrueFalse
    if( nLayers > 0 )
    {
        GDALDatasetPrivate *psPrivate =
            static_cast<GDALDatasetPrivate *>(m_hPrivateData);
        CPLMutexHolderD(psPrivate ? &(psPrivate->hMutex) : NULL);
        for( int i = 0; i < nLayers; ++i )
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
    VALIDATE_POINTER0(hDS, "GDALFlushCache");

    static_cast<GDALDataset *>(hDS)->FlushCache();
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

//! @cond Doxygen_Suppress
void GDALDataset::BlockBasedFlushCache()

{
    GDALRasterBand *poBand1 = GetRasterBand(1);
    if( poBand1 == NULL )
    {
        GDALDataset::FlushCache();
        return;
    }

    int nBlockXSize = 0;
    int nBlockYSize = 0;
    poBand1->GetBlockSize(&nBlockXSize, &nBlockYSize);

/* -------------------------------------------------------------------- */
/*      Verify that all bands match.                                    */
/* -------------------------------------------------------------------- */
    for( int iBand = 1; iBand < nBands; ++iBand )
    {
        GDALRasterBand *poBand = GetRasterBand(iBand + 1);

        int nThisBlockXSize, nThisBlockYSize;
        poBand->GetBlockSize(&nThisBlockXSize, &nThisBlockYSize);
        if( nThisBlockXSize != nBlockXSize && nThisBlockYSize != nBlockYSize )
        {
            GDALDataset::FlushCache();
            return;
        }
    }

/* -------------------------------------------------------------------- */
/*      Now flush writable data.                                        */
/* -------------------------------------------------------------------- */
    for( int iY = 0; iY < poBand1->nBlocksPerColumn; ++iY )
    {
        for( int iX = 0; iX < poBand1->nBlocksPerRow; ++iX )
        {
            for( int iBand = 0; iBand < nBands; ++iBand )
            {
                GDALRasterBand *poBand = GetRasterBand(iBand + 1);

                const CPLErr eErr = poBand->FlushBlock(iX, iY);

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
    CPLAssert(nXSize > 0 && nYSize > 0);

    nRasterXSize = nXSize;
    nRasterYSize = nYSize;
}
//! @endcond

/************************************************************************/
/*                              AddBand()                               */
/************************************************************************/

/**
 * \fn GDALDataset::AddBand(GDALDataType, char**)
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

CPLErr GDALDataset::AddBand( CPL_UNUSED GDALDataType eType,
                             CPL_UNUSED char **papszOptions )

{
    ReportError(CE_Failure, CPLE_NotSupported,
                "Dataset does not support the AddBand() method.");

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
    VALIDATE_POINTER1(hDataset, "GDALAddBand", CE_Failure);

    return static_cast<GDALDataset *>(hDataset)->AddBand(eType, papszOptions);
}

/************************************************************************/
/*                              SetBand()                               */
/************************************************************************/

//! @cond Doxygen_Suppress
/**  Set a band in the band array, updating the band count, and array size
 * appropriately.
 *
 * @param nNewBand new band number (indexing starts at 1)
 * @param poBand band object.
 */

void GDALDataset::SetBand( int nNewBand, GDALRasterBand * poBand )

{
/* -------------------------------------------------------------------- */
/*      Do we need to grow the bands list?                              */
/* -------------------------------------------------------------------- */
    if( nBands < nNewBand || papoBands == NULL )
    {
        GDALRasterBand **papoNewBands = NULL;

        if( papoBands == NULL )
            papoNewBands = static_cast<GDALRasterBand **>(VSICalloc(
                sizeof(GDALRasterBand *), std::max(nNewBand, nBands)));
        else
            papoNewBands = static_cast<GDALRasterBand **>(
                VSIRealloc(papoBands, sizeof(GDALRasterBand *) *
                                          std::max(nNewBand, nBands)));
        if (papoNewBands == NULL)
        {
            ReportError(CE_Failure, CPLE_OutOfMemory,
                        "Cannot allocate band array");
            return;
        }

        papoBands = papoNewBands;

        for( int i = nBands; i < nNewBand; ++i )
            papoBands[i] = NULL;

        nBands = std::max(nBands, nNewBand);
    }

/* -------------------------------------------------------------------- */
/*      Set the band.  Resetting the band is currently not permitted.   */
/* -------------------------------------------------------------------- */
    if( papoBands[nNewBand - 1] != NULL )
    {
        ReportError(CE_Failure, CPLE_NotSupported,
                    "Cannot set band %d as it is already set", nNewBand);
        return;
    }

    papoBands[nNewBand - 1] = poBand;

/* -------------------------------------------------------------------- */
/*      Set back reference information on the raster band.  Note        */
/*      that the GDALDataset is a friend of the GDALRasterBand          */
/*      specifically to allow this.                                     */
/* -------------------------------------------------------------------- */
    poBand->nBand = nNewBand;
    poBand->poDS = this;
    poBand->nRasterXSize = nRasterXSize;
    poBand->nRasterYSize = nRasterYSize;
    poBand->eAccess = eAccess;  // Default access to be same as dataset.
}
//! @endcond

/************************************************************************/
/*                           GetRasterXSize()                           */
/************************************************************************/

/**

 \brief Fetch raster width in pixels.

 Equivalent of the C function GDALGetRasterXSize().

 @return the width in pixels of raster bands in this GDALDataset.

*/

int GDALDataset::GetRasterXSize() { return nRasterXSize; }

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
    VALIDATE_POINTER1(hDataset, "GDALGetRasterXSize", 0);

    return static_cast<GDALDataset *>(hDataset)->GetRasterXSize();
}

/************************************************************************/
/*                           GetRasterYSize()                           */
/************************************************************************/

/**

 \brief Fetch raster height in pixels.

 Equivalent of the C function GDALGetRasterYSize().

 @return the height in pixels of raster bands in this GDALDataset.

*/

int GDALDataset::GetRasterYSize() { return nRasterYSize; }

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
    VALIDATE_POINTER1(hDataset, "GDALGetRasterYSize", 0);

    return static_cast<GDALDataset *>(hDataset)->GetRasterYSize();
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
            ReportError(CE_Failure, CPLE_IllegalArg,
                        "GDALDataset::GetRasterBand(%d) - Illegal band #\n",
                        nBandId);
            return NULL;
        }

        return papoBands[nBandId - 1];
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
    VALIDATE_POINTER1(hDS, "GDALGetRasterBand", NULL);

    return static_cast<GDALRasterBandH>(
        static_cast<GDALDataset *>(hDS)->GetRasterBand(nBandId));
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

int GDALDataset::GetRasterCount() { return papoBands ? nBands : 0; }

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
    VALIDATE_POINTER1(hDS, "GDALGetRasterCount", 0);

    return static_cast<GDALDataset *>(hDS)->GetRasterCount();
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
 * @see http://www.gdal.org/osr_tutorial.html
 */

const char *GDALDataset::GetProjectionRef() { return (""); }

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
    VALIDATE_POINTER1(hDS, "GDALGetProjectionRef", NULL);

    return static_cast<GDALDataset *>(hDS)->GetProjectionRef();
}

/************************************************************************/
/*                           SetProjection()                            */
/************************************************************************/

/**
 * \fn GDALDataset::SetProjection(const char*)
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

CPLErr GDALDataset::SetProjection( CPL_UNUSED const char *pszProjection )
{
    if( !(GetMOFlags() & GMO_IGNORE_UNIMPLEMENTED) )
        ReportError(CE_Failure, CPLE_NotSupported,
                    "Dataset does not support the SetProjection() method.");
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

CPLErr CPL_STDCALL GDALSetProjection( GDALDatasetH hDS,
                                      const char * pszProjection )

{
    VALIDATE_POINTER1(hDS, "GDALSetProjection", CE_Failure);

    return static_cast<GDALDataset *>(hDS)->SetProjection(pszProjection);
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
    CPLAssert(padfTransform != NULL);

    padfTransform[0] = 0.0;  // X Origin (top left corner)
    padfTransform[1] = 1.0;  // X Pixel size */
    padfTransform[2] = 0.0;

    padfTransform[3] = 0.0;  // Y Origin (top left corner)
    padfTransform[4] = 0.0;
    padfTransform[5] = 1.0;  // Y Pixel Size

    return CE_Failure;
}

/************************************************************************/
/*                        GDALGetGeoTransform()                         */
/************************************************************************/

/**
 * \brief Fetch the affine transformation coefficients.
 *
 * @see GDALDataset::GetGeoTransform()
 */

CPLErr CPL_STDCALL GDALGetGeoTransform( GDALDatasetH hDS,
                                        double * padfTransform )

{
    VALIDATE_POINTER1(hDS, "GDALGetGeoTransform", CE_Failure);

    return static_cast<GDALDataset *>(hDS)->GetGeoTransform(padfTransform);
}

/************************************************************************/
/*                          SetGeoTransform()                           */
/************************************************************************/

/**
 * \fn GDALDataset::SetGeoTransform(double*)
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

CPLErr GDALDataset::SetGeoTransform( CPL_UNUSED double *padfTransform )

{
    if( !(GetMOFlags() & GMO_IGNORE_UNIMPLEMENTED) )
        ReportError(CE_Failure, CPLE_NotSupported,
                    "SetGeoTransform() not supported for this dataset.");

    return CE_Failure;
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
GDALSetGeoTransform( GDALDatasetH hDS, double *padfTransform )

{
    VALIDATE_POINTER1(hDS, "GDALSetGeoTransform", CE_Failure);

    return static_cast<GDALDataset *>(hDS)->SetGeoTransform(padfTransform);
}

/************************************************************************/
/*                         GetInternalHandle()                          */
/************************************************************************/

/**
 * \fn GDALDataset::GetInternalHandle(const char*)
 * \brief Fetch a format specific internally meaningful handle.
 *
 * This method is the same as the C GDALGetInternalHandle() method.
 *
 * @param pszHandleName the handle name desired.  The meaningful names
 * will be specific to the file format.
 *
 * @return the desired handle value, or NULL if not recognized/supported.
 */

void *GDALDataset::GetInternalHandle( CPL_UNUSED const char *pszHandleName )

{
    return NULL;
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
    VALIDATE_POINTER1(hDS, "GDALGetInternalHandle", NULL);

    return static_cast<GDALDataset *>(hDS)->GetInternalHandle(pszRequest);
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

GDALDriver *GDALDataset::GetDriver() { return poDriver; }

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
    VALIDATE_POINTER1(hDataset, "GDALGetDatasetDriver", NULL);

    return static_cast<GDALDriverH>(
        static_cast<GDALDataset *>(hDataset)->GetDriver());
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

int GDALDataset::Reference() { return ++nRefCount; }

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
    VALIDATE_POINTER1(hDataset, "GDALReferenceDataset", 0);

    return static_cast<GDALDataset *>(hDataset)->Reference();
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

int GDALDataset::Dereference() { return --nRefCount; }

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
    VALIDATE_POINTER1(hDataset, "GDALDereferenceDataset", 0);

    return static_cast<GDALDataset *>(hDataset)->Dereference();
}


/************************************************************************/
/*                            ReleaseRef()                              */
/************************************************************************/

/**
 * \brief Drop a reference to this object, and destroy if no longer referenced.
 * @return TRUE if the object has been destroyed.
 * @since GDAL 2.2
 */

int GDALDataset::ReleaseRef()

{
    CPLAssert( NULL != this );

    if( Dereference() <= 0 )
    {
        nRefCount = 1;
        delete this;
        return TRUE;
    }
    return FALSE;
}

/************************************************************************/
/*                        GDALReleaseDataset()                          */
/************************************************************************/

/**
 * \brief Drop a reference to this object, and destroy if no longer referenced.
 *
 * @see GDALDataset::ReleaseRef()
 * @since GDAL 2.2
 */

int CPL_STDCALL GDALReleaseDataset( GDALDatasetH hDataset )

{
    VALIDATE_POINTER1(hDataset, "GDALReleaseDataset", 0);

    return static_cast<GDALDataset *>(hDataset)->ReleaseRef();
}

/************************************************************************/
/*                             GetShared()                              */
/************************************************************************/

/**
 * \brief Returns shared flag.
 *
 * @return TRUE if the GDALDataset is available for sharing, or FALSE if not.
 */

int GDALDataset::GetShared() const { return bShared; }

/************************************************************************/
/*                            MarkAsShared()                            */
/************************************************************************/

/**
 * \brief Mark this dataset as available for sharing.
 */

void GDALDataset::MarkAsShared()

{
    CPLAssert(!bShared);

    bShared = true;
    if( bIsInternal )
        return;

    GIntBig nPID = GDALGetResponsiblePIDForCurrentThread();

    // Insert the dataset in the set of shared opened datasets.
    CPLMutexHolderD(&hDLMutex);
    if (phSharedDatasetSet == NULL)
        phSharedDatasetSet =
            CPLHashSetNew(GDALSharedDatasetHashFunc, GDALSharedDatasetEqualFunc,
                          GDALSharedDatasetFreeFunc);

    SharedDatasetCtxt *psStruct =
        static_cast<SharedDatasetCtxt *>(CPLMalloc(sizeof(SharedDatasetCtxt)));
    psStruct->poDS = this;
    psStruct->nPID = nPID;
    psStruct->eAccess = eAccess;
    psStruct->pszDescription = CPLStrdup(GetDescription());
    if(CPLHashSetLookup(phSharedDatasetSet, psStruct) != NULL)
    {
        CPLFree(psStruct);
        ReportError(CE_Failure, CPLE_AppDefined,
                    "An existing shared dataset already has this description. "
                    "This should not happen.");
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

int GDALDataset::GetGCPCount() { return 0; }

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
    VALIDATE_POINTER1(hDS, "GDALGetGCPCount", 0);

    return static_cast<GDALDataset *>(hDS)->GetGCPCount();
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

const char *GDALDataset::GetGCPProjection() { return ""; }

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
    VALIDATE_POINTER1(hDS, "GDALGetGCPProjection", NULL);

    return static_cast<GDALDataset *>(hDS)->GetGCPProjection();
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

const GDAL_GCP *GDALDataset::GetGCPs() { return NULL; }

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
    VALIDATE_POINTER1(hDS, "GDALGetGCPs", NULL);

    return static_cast<GDALDataset *>(hDS)->GetGCPs();
}

/************************************************************************/
/*                              SetGCPs()                               */
/************************************************************************/

/**
 * \fn GDALDataset::SetGCPs(int, const GDAL_GCP*, const char*)
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

CPLErr GDALDataset::SetGCPs( CPL_UNUSED int nGCPCount,
                             CPL_UNUSED const GDAL_GCP *pasGCPList,
                             CPL_UNUSED const char *pszGCPProjection )

{
    if( !(GetMOFlags() & GMO_IGNORE_UNIMPLEMENTED) )
        ReportError(CE_Failure, CPLE_NotSupported,
                    "Dataset does not support the SetGCPs() method.");

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
    VALIDATE_POINTER1(hDS, "GDALSetGCPs", CE_Failure);

    return static_cast<GDALDataset *>(hDS)
        ->SetGCPs(nGCPCount, pasGCPList, pszGCPProjection);
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
 * Depending on the actual file format, all overviews level can be also
 * deleted by specifying nOverviews == 0. This works at least for external
 * overviews (.ovr), TIFF internal overviews, etc.
 *
 * This method is the same as the C function GDALBuildOverviews().
 *
 * @param pszResampling one of "NEAREST", "GAUSS", "CUBIC", "AVERAGE", "MODE",
 * "AVERAGE_MAGPHASE" or "NONE" controlling the downsampling method applied.
 * @param nOverviews number of overviews to build, or 0 to clean overviews.
 * @param panOverviewList the list of overview decimation factors to build, or
 *                        NULL if nOverviews == 0.
 * @param nListBands number of bands to build overviews for in panBandList.
 * Build for all bands if this is 0.
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
    int *panAllBandList = NULL;

    if( nListBands == 0 )
    {
        nListBands = GetRasterCount();
        panAllBandList =
            static_cast<int *>(CPLMalloc(sizeof(int) * nListBands));
        for( int i = 0; i < nListBands; ++i )
            panAllBandList[i] = i + 1;

        panBandList = panAllBandList;
    }

    if( pfnProgress == NULL )
        pfnProgress = GDALDummyProgress;

    const CPLErr eErr =
        IBuildOverviews(pszResampling, nOverviews, panOverviewList, nListBands,
                        panBandList, pfnProgress, pProgressData);

    if( panAllBandList != NULL )
        CPLFree(panAllBandList);

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
    VALIDATE_POINTER1(hDataset, "GDALBuildOverviews", CE_Failure);

    return static_cast<GDALDataset *>(hDataset)
        ->BuildOverviews(pszResampling, nOverviews, panOverviewList, nListBands,
                         panBandList, pfnProgress, pProgressData);
}

/************************************************************************/
/*                          IBuildOverviews()                           */
/*                                                                      */
/*      Default implementation.                                         */
/************************************************************************/

//! @cond Doxygen_Suppress
CPLErr GDALDataset::IBuildOverviews( const char *pszResampling,
                                     int nOverviews, int *panOverviewList,
                                     int nListBands, int *panBandList,
                                     GDALProgressFunc pfnProgress,
                                     void * pProgressData )

{
    if( oOvManager.IsInitialized() )
        return oOvManager.BuildOverviews(
            NULL, pszResampling, nOverviews, panOverviewList, nListBands,
            panBandList, pfnProgress, pProgressData);
    else
    {
        ReportError(CE_Failure, CPLE_NotSupported,
                    "BuildOverviews() not supported for this dataset.");

        return CE_Failure;
    }
}
//! @endcond

/************************************************************************/
/*                             IRasterIO()                              */
/*                                                                      */
/*      The default implementation of IRasterIO() is, in the general    */
/*      case to pass the request off to each band objects rasterio      */
/*      methods with appropriate arguments. In some cases, it might     */
/*      choose instead the BlockBasedRasterIO() implementation.         */
/************************************************************************/

//! @cond Doxygen_Suppress
CPLErr GDALDataset::IRasterIO( GDALRWFlag eRWFlag,
                               int nXOff, int nYOff, int nXSize, int nYSize,
                               void * pData, int nBufXSize, int nBufYSize,
                               GDALDataType eBufType,
                               int nBandCount, int *panBandMap,
                               GSpacing nPixelSpace, GSpacing nLineSpace,
                               GSpacing nBandSpace,
                               GDALRasterIOExtraArg* psExtraArg )

{

    const char *pszInterleave = NULL;

    CPLAssert(NULL != pData);

    if (nXSize == nBufXSize && nYSize == nBufYSize && nBandCount > 1 &&
        (pszInterleave = GetMetadataItem("INTERLEAVE", "IMAGE_STRUCTURE")) !=
            NULL &&
        EQUAL(pszInterleave, "PIXEL"))
    {
        return BlockBasedRasterIO(eRWFlag, nXOff, nYOff, nXSize, nYSize, pData,
                                  nBufXSize, nBufYSize, eBufType, nBandCount,
                                  panBandMap, nPixelSpace, nLineSpace,
                                  nBandSpace, psExtraArg);
    }

    if( eRWFlag == GF_Read &&
        (psExtraArg->eResampleAlg == GRIORA_Cubic ||
         psExtraArg->eResampleAlg == GRIORA_CubicSpline ||
         psExtraArg->eResampleAlg == GRIORA_Bilinear ||
         psExtraArg->eResampleAlg == GRIORA_Lanczos) &&
        !(nXSize == nBufXSize && nYSize == nBufYSize) && nBandCount > 1 )
    {
        GDALDataType eFirstBandDT = GDT_Unknown;
        int nFirstMaskFlags = 0;
        GDALRasterBand *poFirstMaskBand = NULL;
        int nOKBands = 0;
        for( int i = 0; i < nBandCount; ++i )
        {
            GDALRasterBand *poBand = GetRasterBand(panBandMap[i]);
            if( (nBufXSize < nXSize || nBufYSize < nYSize) &&
                poBand->GetOverviewCount() )
            {
                // Could be improved to select the appropriate overview.
                break;
            }
            if( poBand->GetColorTable() != NULL )
            {
                break;
            }
            const GDALDataType eDT = poBand->GetRasterDataType();
            if( GDALDataTypeIsComplex(eDT) )
            {
                break;
            }
            if( i == 0 )
            {
                eFirstBandDT = eDT;
                nFirstMaskFlags = poBand->GetMaskFlags();
                poFirstMaskBand = poBand->GetMaskBand();
            }
            else
            {
                if( eDT != eFirstBandDT )
                {
                    break;
                }
                int nMaskFlags = poBand->GetMaskFlags();
                GDALRasterBand *poMaskBand = poBand->GetMaskBand();
                if( nFirstMaskFlags == GMF_ALL_VALID &&
                    nMaskFlags == GMF_ALL_VALID )
                {
                    // Ok.
                }
                else if( poFirstMaskBand == poMaskBand )
                {
                    // Ok.
                }
                else
                {
                    break;
                }
            }

            ++nOKBands;
        }

        GDALProgressFunc pfnProgressGlobal = psExtraArg->pfnProgress;
        void *pProgressDataGlobal = psExtraArg->pProgressData;

        CPLErr eErr = CE_None;
        if( nOKBands > 0 )
        {
            if( nOKBands < nBandCount )
            {
                psExtraArg->pfnProgress = GDALScaledProgress;
                psExtraArg->pProgressData = GDALCreateScaledProgress(
                    0.0, static_cast<double>(nOKBands) / nBandCount,
                    pfnProgressGlobal,
                    pProgressDataGlobal);
                if( psExtraArg->pProgressData == NULL )
                    psExtraArg->pfnProgress = NULL;
            }

            eErr = RasterIOResampled(eRWFlag, nXOff, nYOff, nXSize, nYSize,
                                     pData, nBufXSize, nBufYSize, eBufType,
                                     nOKBands, panBandMap, nPixelSpace,
                                     nLineSpace, nBandSpace, psExtraArg);

            if( nOKBands < nBandCount )
            {
                GDALDestroyScaledProgress(psExtraArg->pProgressData);
            }
        }
        if( eErr == CE_None && nOKBands < nBandCount )
        {
            if( nOKBands > 0 )
            {
                psExtraArg->pfnProgress = GDALScaledProgress;
                psExtraArg->pProgressData = GDALCreateScaledProgress(
                    static_cast<double>(nOKBands) / nBandCount,
                    1.0, pfnProgressGlobal,
                    pProgressDataGlobal);
                if( psExtraArg->pProgressData == NULL )
                    psExtraArg->pfnProgress = NULL;
            }
            eErr = BandBasedRasterIO(
                eRWFlag, nXOff, nYOff, nXSize, nYSize,
                static_cast<GByte *>(pData) + nBandSpace * nOKBands,
                nBufXSize, nBufYSize,
                eBufType, nBandCount - nOKBands, panBandMap + nOKBands,
                nPixelSpace, nLineSpace, nBandSpace, psExtraArg);
            if( nOKBands > 0 )
            {
                GDALDestroyScaledProgress(psExtraArg->pProgressData);
            }
        }

        psExtraArg->pfnProgress = pfnProgressGlobal;
        psExtraArg->pProgressData = pProgressDataGlobal;

        return eErr;
    }

    return BandBasedRasterIO(eRWFlag, nXOff, nYOff, nXSize, nYSize, pData,
                             nBufXSize, nBufYSize, eBufType, nBandCount,
                             panBandMap, nPixelSpace, nLineSpace, nBandSpace,
                             psExtraArg);
}
//! @endcond

/************************************************************************/
/*                         BandBasedRasterIO()                          */
/*                                                                      */
/*      Pass the request off to each band objects rasterio methods with */
/*      appropriate arguments.                                          */
/************************************************************************/

//! @cond Doxygen_Suppress
CPLErr GDALDataset::BandBasedRasterIO( GDALRWFlag eRWFlag,
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

    GDALProgressFunc pfnProgressGlobal = psExtraArg->pfnProgress;
    void *pProgressDataGlobal = psExtraArg->pProgressData;

    for( iBandIndex = 0;
         iBandIndex < nBandCount && eErr == CE_None;
         ++iBandIndex )
    {
        GDALRasterBand *poBand = GetRasterBand(panBandMap[iBandIndex]);

        if (poBand == NULL)
        {
            eErr = CE_Failure;
            break;
        }

        GByte *pabyBandData =
            static_cast<GByte *>(pData) + iBandIndex * nBandSpace;

        if( nBandCount > 1 )
        {
            psExtraArg->pfnProgress = GDALScaledProgress;
            psExtraArg->pProgressData = GDALCreateScaledProgress(
                1.0 * iBandIndex / nBandCount,
                1.0 * (iBandIndex + 1) / nBandCount, pfnProgressGlobal,
                pProgressDataGlobal);
            if( psExtraArg->pProgressData == NULL )
                psExtraArg->pfnProgress = NULL;
        }

        eErr = poBand->IRasterIO(eRWFlag, nXOff, nYOff, nXSize, nYSize,
                                 pabyBandData, nBufXSize, nBufYSize,
                                 eBufType, nPixelSpace, nLineSpace, psExtraArg);

        if( nBandCount > 1 )
            GDALDestroyScaledProgress(psExtraArg->pProgressData);
    }

    psExtraArg->pfnProgress = pfnProgressGlobal;
    psExtraArg->pProgressData = pProgressDataGlobal;

    return eErr;
}
//! @endcond

/************************************************************************/
/*               ValidateRasterIOOrAdviseReadParameters()               */
/************************************************************************/

//! @cond Doxygen_Suppress
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
        CPLDebug("GDAL",
                 "%s skipped for odd window or buffer size.\n"
                 "  Window = (%d,%d)x%dx%d\n"
                 "  Buffer = %dx%d",
                 pszCallingFunc,
                 nXOff, nYOff, nXSize, nYSize,
                 nBufXSize, nBufYSize);

        *pbStopProcessingOnCENone = TRUE;
        return CE_None;
    }

    CPLErr eErr = CE_None;
    *pbStopProcessingOnCENone = FALSE;

    if( nXOff < 0 || nXOff > INT_MAX - nXSize || nXOff + nXSize > nRasterXSize
        || nYOff < 0 || nYOff > INT_MAX - nYSize || nYOff + nYSize > nRasterYSize )
    {
        ReportError(CE_Failure, CPLE_IllegalArg,
                    "Access window out of range in %s.  Requested "
                    "(%d,%d) of size %dx%d on raster of %dx%d.",
                    pszCallingFunc, nXOff, nYOff, nXSize, nYSize, nRasterXSize,
                    nRasterYSize);
        eErr = CE_Failure;
    }

    if( panBandMap == NULL && nBandCount > GetRasterCount() )
    {
        ReportError( CE_Failure, CPLE_IllegalArg,
                    "%s: nBandCount cannot be greater than %d",
                    pszCallingFunc, GetRasterCount() );
        eErr = CE_Failure;
    }

    for( int i = 0; i < nBandCount && eErr == CE_None; ++i )
    {
        int iBand = (panBandMap != NULL) ? panBandMap[i] : i + 1;
        if( iBand < 1 || iBand > GetRasterCount() )
        {
            ReportError(
                CE_Failure, CPLE_IllegalArg,
                "%s: panBandMap[%d] = %d, this band does not exist on dataset.",
                pszCallingFunc, i, iBand);
            eErr = CE_Failure;
        }

        if( eErr == CE_None && GetRasterBand( iBand ) == NULL )
        {
            ReportError(
                CE_Failure, CPLE_IllegalArg,
                "%s: panBandMap[%d]=%d, this band should exist but is NULL!",
                pszCallingFunc, i, iBand);
            eErr = CE_Failure;
        }
    }

    return eErr;
}
//! @endcond

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
 * @param psExtraArg (new in GDAL 2.0) pointer to a GDALRasterIOExtraArg
 * structure with additional arguments to specify resampling and progress
 * callback, or NULL for default behaviour. The GDAL_RASTERIO_RESAMPLING
 * configuration option can also be defined to override the default resampling
 * to one of BILINEAR, CUBIC, CUBICSPLINE, LANCZOS, AVERAGE or MODE.
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
    GDALRasterIOExtraArg sExtraArg;
    if( psExtraArg == NULL )
    {
        INIT_RASTERIO_EXTRA_ARG(sExtraArg);
        psExtraArg = &sExtraArg;
    }
    else if( psExtraArg->nVersion != RASTERIO_EXTRA_ARG_CURRENT_VERSION )
    {
        ReportError(CE_Failure, CPLE_AppDefined,
                    "Unhandled version of GDALRasterIOExtraArg");
        return CE_Failure;
    }

    GDALRasterIOExtraArgSetResampleAlg(psExtraArg, nXSize, nYSize, nBufXSize,
                                       nBufYSize);

    if( NULL == pData )
    {
        ReportError(CE_Failure, CPLE_AppDefined,
                    "The buffer into which the data should be read is null");
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Do some validation of parameters.                               */
/* -------------------------------------------------------------------- */

    if( eRWFlag != GF_Read && eRWFlag != GF_Write )
    {
        ReportError(
            CE_Failure, CPLE_IllegalArg,
            "eRWFlag = %d, only GF_Read (0) and GF_Write (1) are legal.",
            eRWFlag);
        return CE_Failure;
    }

    int bStopProcessing = FALSE;
    CPLErr eErr = ValidateRasterIOOrAdviseReadParameters(
        "RasterIO()", &bStopProcessing, nXOff, nYOff, nXSize, nYSize, nBufXSize,
        nBufYSize, nBandCount, panBandMap);
    if( eErr != CE_None || bStopProcessing )
        return eErr;

/* -------------------------------------------------------------------- */
/*      If pixel and line spacing are defaulted assign reasonable      */
/*      value assuming a packed buffer.                                 */
/* -------------------------------------------------------------------- */
    if( nPixelSpace == 0 )
        nPixelSpace = GDALGetDataTypeSizeBytes(eBufType);

    if( nLineSpace == 0 )
    {
        nLineSpace = nPixelSpace * nBufXSize;
    }

    if( nBandSpace == 0 && nBandCount > 1 )
    {
        nBandSpace = nLineSpace * nBufYSize;
    }

    bool bNeedToFreeBandMap = false;
    int anBandMap[] = { 1, 2, 3, 4 };
    if( panBandMap == NULL )
    {
        if( nBandCount > 4 )
        {
            panBandMap =
                static_cast<int *>(VSIMalloc2(sizeof(int), nBandCount));
            if (panBandMap == NULL)
            {
                ReportError(CE_Failure, CPLE_OutOfMemory,
                            "Out of memory while allocating band map array");
                return CE_Failure;
            }

            for( int i = 0; i < nBandCount; ++i )
                panBandMap[i] = i + 1;

            bNeedToFreeBandMap = true;
        }
        else
        {
            panBandMap = anBandMap;
        }
    }

    int bCallLeaveReadWrite = EnterReadWrite(eRWFlag);

/* -------------------------------------------------------------------- */
/*      We are being forced to use cached IO instead of a driver        */
/*      specific implementation.                                        */
/* -------------------------------------------------------------------- */
    if( bForceCachedIO )
    {
        eErr = BlockBasedRasterIO(eRWFlag, nXOff, nYOff, nXSize, nYSize, pData,
                                  nBufXSize, nBufYSize, eBufType, nBandCount,
                                  panBandMap, nPixelSpace, nLineSpace,
                                  nBandSpace, psExtraArg);
    }

/* -------------------------------------------------------------------- */
/*      Call the format specific function.                              */
/* -------------------------------------------------------------------- */
    else if( eErr == CE_None )
    {
        eErr = IRasterIO(eRWFlag, nXOff, nYOff, nXSize, nYSize, pData,
                         nBufXSize, nBufYSize, eBufType, nBandCount, panBandMap,
                         nPixelSpace, nLineSpace, nBandSpace, psExtraArg);
    }

    if( bCallLeaveReadWrite ) LeaveReadWrite();

/* -------------------------------------------------------------------- */
/*      Cleanup                                                         */
/* -------------------------------------------------------------------- */
    if( bNeedToFreeBandMap )
        CPLFree(panBandMap);

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
    VALIDATE_POINTER1(hDS, "GDALDatasetRasterIO", CE_Failure);

    GDALDataset *poDS = static_cast<GDALDataset *>(hDS);

    return poDS->RasterIO(eRWFlag, nXOff, nYOff, nXSize, nYSize, pData,
                          nBufXSize, nBufYSize, eBufType, nBandCount,
                          panBandMap, nPixelSpace, nLineSpace, nBandSpace,
                          NULL);
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

CPLErr CPL_STDCALL GDALDatasetRasterIOEx(
    GDALDatasetH hDS, GDALRWFlag eRWFlag, int nXOff, int nYOff, int nXSize,
    int nYSize, void *pData, int nBufXSize, int nBufYSize,
    GDALDataType eBufType, int nBandCount, int *panBandMap,
    GSpacing nPixelSpace, GSpacing nLineSpace, GSpacing nBandSpace,
    GDALRasterIOExtraArg *psExtraArg )

{
    VALIDATE_POINTER1(hDS, "GDALDatasetRasterIOEx", CE_Failure);

    GDALDataset *poDS = static_cast<GDALDataset *>(hDS);

    return poDS->RasterIO(eRWFlag, nXOff, nYOff, nXSize, nYSize, pData,
                          nBufXSize, nBufYSize, eBufType, nBandCount,
                          panBandMap, nPixelSpace, nLineSpace, nBandSpace,
                          psExtraArg);
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
    CPLMutexHolderD(&hDLMutex);

    if (poAllDatasetMap == NULL)
    {
        *pnCount = 0;
        return NULL;
    }

    *pnCount = static_cast<int>(poAllDatasetMap->size());
    ppDatasets = static_cast<GDALDataset **>(
        CPLRealloc(ppDatasets, (*pnCount) * sizeof(GDALDataset *)));
    std::map<GDALDataset *, GIntBig>::iterator oIter =
        poAllDatasetMap->begin();
    for( int i = 0; oIter != poAllDatasetMap->end(); ++oIter, ++i )
        ppDatasets[i] = oIter->first;
    return ppDatasets;
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
    VALIDATE_POINTER0(ppahDSList, "GDALGetOpenDatasets");
    VALIDATE_POINTER0(pnCount, "GDALGetOpenDatasets");

    *ppahDSList = (GDALDatasetH *)GDALDataset::GetOpenDatasets(pnCount);
}

/************************************************************************/
/*                        GDALCleanOpenDatasetsList()                   */
/************************************************************************/

// Useful when called from the child of a fork(), to avoid closing
// the datasets of the parent at the child termination.
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
    VALIDATE_POINTER1(hDS, "GDALGetAccess", 0);

    return static_cast<GDALDataset *>(hDS)->GetAccess();
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
/* -------------------------------------------------------------------- */
/*      Do some validation of parameters.                               */
/* -------------------------------------------------------------------- */
    int bStopProcessing = FALSE;
    CPLErr eErr = ValidateRasterIOOrAdviseReadParameters(
        "AdviseRead()", &bStopProcessing, nXOff, nYOff, nXSize, nYSize,
        nBufXSize, nBufYSize, nBandCount, panBandMap);
    if( eErr != CE_None || bStopProcessing )
        return eErr;

    for( int iBand = 0; iBand < nBandCount; ++iBand )
    {
        GDALRasterBand *poBand = NULL;

        if( panBandMap == NULL )
            poBand = GetRasterBand(iBand + 1);
        else
            poBand = GetRasterBand(panBandMap[iBand]);

        eErr = poBand->AdviseRead(nXOff, nYOff, nXSize, nYSize, nBufXSize,
                                  nBufYSize, eBufType, papszOptions);

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
    VALIDATE_POINTER1(hDS, "GDALDatasetAdviseRead", CE_Failure);

    return static_cast<GDALDataset *>(hDS)
        ->AdviseRead(nXOff, nYOff, nXSize, nYSize, nBufXSize, nBufYSize, eDT,
                     nBandCount, panBandMap, papszOptions);
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
    VSIStatBufL sStat;

/* -------------------------------------------------------------------- */
/*      Is the main filename even a real filesystem object?             */
/* -------------------------------------------------------------------- */
    int bMainFileReal =
        VSIStatExL(osMainFilename, &sStat, VSI_STAT_EXISTS_FLAG) == 0;

/* -------------------------------------------------------------------- */
/*      Form new list.                                                  */
/* -------------------------------------------------------------------- */
    char **papszList = NULL;

    if( bMainFileReal )
        papszList = CSLAddString(papszList, osMainFilename);

/* -------------------------------------------------------------------- */
/*      Do we have a known overview file?                               */
/* -------------------------------------------------------------------- */
    if(oOvManager.IsInitialized() && oOvManager.poODS != NULL)
    {
        char **papszOvrList = oOvManager.poODS->GetFileList();
        papszList = CSLInsertStrings(papszList, -1, papszOvrList);
        CSLDestroy(papszOvrList);
    }

/* -------------------------------------------------------------------- */
/*      Do we have a known mask file?                                   */
/* -------------------------------------------------------------------- */
    if( oOvManager.HaveMaskFile() )
    {
        char **papszMskList = oOvManager.poMaskDS->GetFileList();
        char **papszIter = papszMskList;
        while( papszIter && *papszIter )
        {
            if( CSLFindString( papszList, *papszIter ) < 0 )
                papszList = CSLAddString( papszList, *papszIter );
            ++papszIter;
        }
        CSLDestroy(papszMskList);
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
    VALIDATE_POINTER1(hDS, "GDALGetFileList", NULL);

    return static_cast<GDALDataset *>(hDS)->GetFileList();
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
 * It will have INTERNAL_MASK_FLAGS_xx metadata items set at the dataset
 * level, where xx matches the band number of a band of the main dataset. The
 * value of those items will be the one of the nFlagsIn parameter.
 *
 * Note that if you got a mask band with a previous call to GetMaskBand(), it
 * might be invalidated by CreateMaskBand(). So you have to call GetMaskBand()
 * again.
 *
 * @since GDAL 1.5.0
 *
 * @param nFlagsIn 0 or combination of GMF_PER_DATASET / GMF_ALPHA.
 *                 GMF_PER_DATASET will be always set, even if not explicitly
 *                 specified.
 * @return CE_None on success or CE_Failure on an error.
 *
 * @see http://trac.osgeo.org/gdal/wiki/rfc15_nodatabitmask
 * @see GDALRasterBand::CreateMaskBand()
 *
 */
CPLErr GDALDataset::CreateMaskBand( int nFlagsIn )

{
    if( oOvManager.IsInitialized() )
    {
        CPLErr eErr = oOvManager.CreateMaskBand(nFlagsIn, -1);
        if (eErr != CE_None)
            return eErr;

        // Invalidate existing raster band masks.
        for( int i = 0; i < nBands; ++i )
        {
            GDALRasterBand *poBand = papoBands[i];
            if (poBand->bOwnMask)
                delete poBand->poMask;
            poBand->bOwnMask = false;
            poBand->poMask = NULL;
        }

        return CE_None;
    }

    ReportError(CE_Failure, CPLE_NotSupported,
                "CreateMaskBand() not supported for this dataset.");

    return CE_Failure;
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
    VALIDATE_POINTER1(hDS, "GDALCreateDatasetMaskBand", CE_Failure);

    return static_cast<GDALDataset *>(hDS)->CreateMaskBand(nFlags);
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
 * <li>The returned dataset should only be accessed by one thread at a time. If
 * you want to use it from different threads, you must add all necessary code
 * (mutexes, etc.)  to avoid concurrent use of the object. (Some drivers, such
 * as GeoTIFF, maintain internal state variables that are updated each time a
 * new block is read, thus preventing concurrent use.) </li>
 * </ul>
 *
 * For drivers supporting the VSI virtual file API, it is possible to open a
 * file in a .zip archive (see VSIInstallZipFileHandler()), in a
 * .tar/.tar.gz/.tgz archive (see VSIInstallTarFileHandler()) or on a HTTP / FTP
 * server (see VSIInstallCurlFileHandler())
 *
 * In some situations (dealing with unverified data), the datasets can be opened
 * in another process through the \ref gdal_api_proxy mechanism.
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
    const int nUpdateFlag = eAccess == GA_Update ? GDAL_OF_UPDATE : 0;
    const int nOpenFlags = GDAL_OF_RASTER | nUpdateFlag | GDAL_OF_VERBOSE_ERROR;
    GDALDatasetH hDataset =
        GDALOpenEx(pszFilename, nOpenFlags, NULL, NULL, NULL);
    return hDataset;
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
 * <li>If you open a dataset object with GDAL_OF_UPDATE access, it is not
 * recommended to open a new dataset on the same underlying file.</li>
 * <li>The returned dataset should only be accessed by one thread at a time. If
 * you want to use it from different threads, you must add all necessary code
 * (mutexes, etc.)  to avoid concurrent use of the object. (Some drivers, such
 * as GeoTIFF, maintain internal state variables that are updated each time a
 * new block is read, thus preventing concurrent use.) </li>
 * </ul>
 *
 * For drivers supporting the VSI virtual file API, it is possible to open a
 * file in a .zip archive (see VSIInstallZipFileHandler()), in a
 * .tar/.tar.gz/.tgz archive (see VSIInstallTarFileHandler()) or on a HTTP / FTP
 * server (see VSIInstallCurlFileHandler())
 *
 * In some situations (dealing with unverified data), the datasets can be opened
 * in another process through the \ref gdal_api_proxy mechanism.
 *
 * In order to reduce the need for searches through the operating system
 * file system machinery, it is possible to give an optional list of files with
 * the papszSiblingFiles parameter.
 * This is the list of all files at the same level in the file system as the
 * target file, including the target file. The filenames must not include any
 * path components, are essentially just the output of VSIReadDir() on the
 * parent directory. If the target object does not have filesystem semantics
 * then the file list should be NULL.
 *
 * @param pszFilename the name of the file to access.  In the case of
 * exotic drivers this may not refer to a physical file, but instead contain
 * information for the driver on how to access a dataset.  It should be in UTF-8
 * encoding.
 *
 * @param nOpenFlags a combination of GDAL_OF_ flags that may be combined
 * through logical or operator.
 * <ul>
 * <li>Driver kind: GDAL_OF_RASTER for raster drivers, GDAL_OF_VECTOR for vector
 *     drivers.  If none of the value is specified, both kinds are implied.</li>
 * <li>Access mode: GDAL_OF_READONLY (exclusive)or GDAL_OF_UPDATE.</li>
 * <li>Shared mode: GDAL_OF_SHARED. If set, it allows the sharing of GDALDataset
 * handles for a dataset with other callers that have set GDAL_OF_SHARED.
 * In particular, GDALOpenEx() will first consult its list of currently
 * open and shared GDALDataset's, and if the GetDescription() name for one
 * exactly matches the pszFilename passed to GDALOpenEx() it will be
 * referenced and returned, if GDALOpenEx() is called from the same thread.</li>
 * <li>Verbose error: GDAL_OF_VERBOSE_ERROR. If set, a failed attempt to open
 * the file will lead to an error message to be reported.</li>
 * </ul>
 *
 * @param papszAllowedDrivers NULL to consider all candidate drivers, or a NULL
 * terminated list of strings with the driver short names that must be
 * considered.
 *
 * @param papszOpenOptions NULL, or a NULL terminated list of strings with open
 * options passed to candidate drivers. An option exists for all drivers,
 * OVERVIEW_LEVEL=level, to select a particular overview level of a dataset.
 * The level index starts at 0. The level number can be suffixed by "only" to
 * specify that only this overview level must be visible, and not sub-levels.
 * Open options are validated by default, and a warning is emitted in case the
 * option is not recognized. In some scenarios, it might be not desirable (e.g.
 * when not knowing which driver will open the file), so the special open option
 * VALIDATE_OPEN_OPTIONS can be set to NO to avoid such warnings. Alternatively,
 * since GDAL 2.1, an option name can be preceded by the @ character to indicate
 * that it may not cause a warning if the driver doesn't declare this option.
 *
 * @param papszSiblingFiles NULL, or a NULL terminated list of strings that are
 * filenames that are auxiliary to the main filename. If NULL is passed, a
 * probing of the file system will be done.
 *
 * @return A GDALDatasetH handle or NULL on failure.  For C++ applications
 * this handle can be cast to a GDALDataset *.
 *
 * @since GDAL 2.0
 */

GDALDatasetH CPL_STDCALL GDALOpenEx( const char *pszFilename,
                                     unsigned int nOpenFlags,
                                     const char *const *papszAllowedDrivers,
                                     const char *const *papszOpenOptions,
                                     const char *const *papszSiblingFiles )
{
    VALIDATE_POINTER1(pszFilename, "GDALOpen", NULL);

/* -------------------------------------------------------------------- */
/*      In case of shared dataset, first scan the existing list to see  */
/*      if it could already contain the requested dataset.              */
/* -------------------------------------------------------------------- */
    if( nOpenFlags & GDAL_OF_SHARED )
    {
        if( nOpenFlags & GDAL_OF_INTERNAL )
        {
            CPLError(CE_Failure, CPLE_IllegalArg,
                     "GDAL_OF_SHARED and GDAL_OF_INTERNAL are exclusive");
            return NULL;
        }

        CPLMutexHolderD(&hDLMutex);

        if (phSharedDatasetSet != NULL)
        {
            const GIntBig nThisPID = GDALGetResponsiblePIDForCurrentThread();
            SharedDatasetCtxt sStruct;

            sStruct.nPID = nThisPID;
            sStruct.pszDescription = const_cast<char *>(pszFilename);
            sStruct.eAccess =
                (nOpenFlags & GDAL_OF_UPDATE) ? GA_Update : GA_ReadOnly;
            SharedDatasetCtxt *psStruct = static_cast<SharedDatasetCtxt *>(
                CPLHashSetLookup(phSharedDatasetSet, &sStruct));
            if (psStruct == NULL && (nOpenFlags & GDAL_OF_UPDATE) == 0)
            {
                sStruct.eAccess = GA_Update;
                psStruct = static_cast<SharedDatasetCtxt *>(
                    CPLHashSetLookup(phSharedDatasetSet, &sStruct));
            }
            if (psStruct)
            {
                psStruct->poDS->Reference();
                return psStruct->poDS;
            }
        }
    }

    // If no driver kind is specified, assume all are to be probed.
    if( (nOpenFlags & GDAL_OF_KIND_MASK) == 0 )
        nOpenFlags |= GDAL_OF_KIND_MASK;

    GDALDriverManager *poDM = GetGDALDriverManager();
    // CPLLocaleC  oLocaleForcer;

    CPLErrorReset();
    VSIErrorReset();
    CPLAssert(NULL != poDM);

    // Build GDALOpenInfo just now to avoid useless file stat'ing if a
    // shared dataset was asked before.
    GDALOpenInfo oOpenInfo(pszFilename, nOpenFlags,
                           const_cast<char **>(papszSiblingFiles));
    oOpenInfo.papszAllowedDrivers = papszAllowedDrivers;

    // Prevent infinite recursion.
    {
        int *pnRecCount =
            static_cast<int *>(CPLGetTLS(CTLS_GDALDATASET_REC_PROTECT_MAP));
        if( pnRecCount == NULL )
        {
            pnRecCount = static_cast<int *>(CPLMalloc(sizeof(int)));
            *pnRecCount = 0;
            CPLSetTLS(CTLS_GDALDATASET_REC_PROTECT_MAP, pnRecCount, TRUE);
        }
        if( *pnRecCount == 100 )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "GDALOpen() called with too many recursion levels");
            return NULL;
        }
        (*pnRecCount)++;
    }

    // Remove leading @ if present.
    char **papszOpenOptionsCleaned =
        CSLDuplicate(const_cast<char **>(papszOpenOptions));
    for(char **papszIter = papszOpenOptionsCleaned; papszIter && *papszIter;
        ++papszIter)
    {
        char *pszOption = *papszIter;
        if( pszOption[0] == '@' )
            memmove(pszOption, pszOption + 1, strlen(pszOption + 1) + 1);
    }

    oOpenInfo.papszOpenOptions = papszOpenOptionsCleaned;

    for( int iDriver = -1; iDriver < poDM->GetDriverCount(); ++iDriver )
    {
        GDALDriver *poDriver = NULL;

        if( iDriver < 0 )
        {
            poDriver = GDALGetAPIPROXYDriver();
        }
        else
        {
            poDriver = poDM->GetDriver(iDriver);
            if (papszAllowedDrivers != NULL &&
                CSLFindString(papszAllowedDrivers,
                              GDALGetDriverShortName(poDriver)) == -1)
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

        // Remove general OVERVIEW_LEVEL open options from list before passing
        // it to the driver, if it isn't a driver specific option already.
        char **papszTmpOpenOptions = NULL;
        char **papszTmpOpenOptionsToValidate = NULL;
        char **papszOptionsToValidate = const_cast<char **>(papszOpenOptions);
        if( CSLFetchNameValue(papszOpenOptionsCleaned, "OVERVIEW_LEVEL") !=
               NULL &&
            (poDriver->GetMetadataItem(GDAL_DMD_OPENOPTIONLIST) == NULL ||
             CPLString(poDriver->GetMetadataItem(GDAL_DMD_OPENOPTIONLIST))
                    .ifind("OVERVIEW_LEVEL") == std::string::npos) )
        {
            papszTmpOpenOptions = CSLDuplicate(papszOpenOptionsCleaned);
            papszTmpOpenOptions =
                CSLSetNameValue(papszTmpOpenOptions, "OVERVIEW_LEVEL", NULL);
            oOpenInfo.papszOpenOptions = papszTmpOpenOptions;

            papszOptionsToValidate = CSLDuplicate(papszOptionsToValidate);
            papszOptionsToValidate =
                CSLSetNameValue(papszOptionsToValidate, "OVERVIEW_LEVEL", NULL);
            papszTmpOpenOptionsToValidate = papszOptionsToValidate;
        }

        const bool bIdentifyRes =
            poDriver->pfnIdentify && poDriver->pfnIdentify(&oOpenInfo) > 0;
        if( bIdentifyRes )
        {
            GDALValidateOpenOptions(poDriver, papszOptionsToValidate);
        }

        GDALDataset *poDS = NULL;
        if ( poDriver->pfnOpen != NULL )
        {
            poDS = poDriver->pfnOpen(&oOpenInfo);
            // If we couldn't determine for sure with Identify() (it returned
            // -1), but Open() managed to open the file, post validate options.
            if( poDS != NULL && poDriver->pfnIdentify && !bIdentifyRes )
                GDALValidateOpenOptions(poDriver, papszOptionsToValidate);
        }
        else if( poDriver->pfnOpenWithDriverArg != NULL )
        {
            poDS = poDriver->pfnOpenWithDriverArg(poDriver, &oOpenInfo);
        }
        else
        {
            CSLDestroy(papszTmpOpenOptions);
            CSLDestroy(papszTmpOpenOptionsToValidate);
            oOpenInfo.papszOpenOptions = papszOpenOptionsCleaned;
            continue;
        }

        CSLDestroy(papszTmpOpenOptions);
        CSLDestroy(papszTmpOpenOptionsToValidate);
        oOpenInfo.papszOpenOptions = papszOpenOptionsCleaned;

        if( poDS != NULL )
        {
            poDS->nOpenFlags = nOpenFlags;

            if( strlen(poDS->GetDescription()) == 0 )
                poDS->SetDescription(pszFilename);

            if( poDS->poDriver == NULL )
                poDS->poDriver = poDriver;

            if( poDS->papszOpenOptions == NULL )
            {
                poDS->papszOpenOptions = papszOpenOptionsCleaned;
                papszOpenOptionsCleaned = NULL;
            }

            if( !(nOpenFlags & GDAL_OF_INTERNAL) )
            {
                if( CPLGetPID() != GDALGetResponsiblePIDForCurrentThread() )
                    CPLDebug("GDAL",
                             "GDALOpen(%s, this=%p) succeeds as "
                             "%s (pid=%d, responsiblePID=%d).",
                             pszFilename, poDS, poDriver->GetDescription(),
                             static_cast<int>(CPLGetPID()),
                             static_cast<int>(
                                 GDALGetResponsiblePIDForCurrentThread()));
                else
                    CPLDebug("GDAL", "GDALOpen(%s, this=%p) succeeds as %s.",
                             pszFilename, poDS, poDriver->GetDescription());

                poDS->AddToDatasetOpenList();
            }

            int *pnRecCount =
                static_cast<int *>(CPLGetTLS(CTLS_GDALDATASET_REC_PROTECT_MAP));
            if( pnRecCount )
                (*pnRecCount)--;

            if( nOpenFlags & GDAL_OF_SHARED )
            {
                if (strcmp(pszFilename, poDS->GetDescription()) != 0)
                {
                    CPLError(CE_Warning, CPLE_NotSupported,
                             "A dataset opened by GDALOpenShared should have "
                             "the same filename (%s) "
                             "and description (%s)",
                             pszFilename, poDS->GetDescription());
                }
                else
                {
                    poDS->MarkAsShared();
                }
            }

            // Deal with generic OVERVIEW_LEVEL open option, unless it is
            // driver specific.
            if( CSLFetchNameValue(papszOpenOptions, "OVERVIEW_LEVEL") != NULL &&
                (poDriver->GetMetadataItem(GDAL_DMD_OPENOPTIONLIST) == NULL ||
                CPLString(poDriver->GetMetadataItem(GDAL_DMD_OPENOPTIONLIST))
                        .ifind("OVERVIEW_LEVEL") == std::string::npos) )
            {
                CPLString osVal(
                    CSLFetchNameValue(papszOpenOptions, "OVERVIEW_LEVEL"));
                const int nOvrLevel = atoi(osVal);
                const bool bThisLevelOnly =
                    osVal.ifind("only") != std::string::npos;
                GDALDataset *poOvrDS = GDALCreateOverviewDataset(
                    poDS, nOvrLevel, bThisLevelOnly);
                poDS->ReleaseRef();
                poDS = poOvrDS;
                if( poDS == NULL )
                {
                    if( nOpenFlags & GDAL_OF_VERBOSE_ERROR )
                    {
                        CPLError(CE_Failure, CPLE_OpenFailed,
                                 "Cannot open overview level %d of %s",
                                 nOvrLevel, pszFilename);
                    }
                }
            }
            VSIErrorReset();

            CSLDestroy(papszOpenOptionsCleaned);
            return poDS;
        }

        if( CPLGetLastErrorNo() != 0 && CPLGetLastErrorType() > CE_Warning)
        {
            int *pnRecCount =
                static_cast<int *>(CPLGetTLS(CTLS_GDALDATASET_REC_PROTECT_MAP));
            if( pnRecCount )
                (*pnRecCount)--;

            CSLDestroy(papszOpenOptionsCleaned);
            return NULL;
        }
    }

    CSLDestroy(papszOpenOptionsCleaned);

    if( nOpenFlags & GDAL_OF_VERBOSE_ERROR )
    {
        // Check to see if there was a filesystem error, and report it if so.
        // If not, return a more generic error.
        if(!VSIToCPLError(CE_Failure, CPLE_OpenFailed))
        {
            if( oOpenInfo.bStatOK )
            {
                CPLError(CE_Failure, CPLE_OpenFailed,
                         "`%s' not recognized as a supported file format.",
                         pszFilename);
            }
            else
            {
                // If Stat failed and no VSI error was set, assume it is because
                // the file did not exist on the filesystem.
                CPLError(CE_Failure, CPLE_OpenFailed,
                         "`%s' does not exist in the file system, "
                         "and is not recognized as a supported dataset name.",
                         pszFilename);
            }
        }
    }

    int *pnRecCount =
        static_cast<int *>(CPLGetTLS(CTLS_GDALDATASET_REC_PROTECT_MAP));
    if( pnRecCount )
        (*pnRecCount)--;

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
 * In particular, GDALOpenShared() will first consult its list of currently
 * open and shared GDALDataset's, and if the GetDescription() name for one
 * exactly matches the pszFilename passed to GDALOpenShared() it will be
 * referenced and returned.
 *
 * Starting with GDAL 1.6.0, if GDALOpenShared() is called on the same
 * pszFilename from two different threads, a different GDALDataset object will
 * be returned as it is not safe to use the same dataset from different threads,
 * unless the user does explicitly use mutexes in its code.
 *
 * For drivers supporting the VSI virtual file API, it is possible to open a
 * file in a .zip archive (see VSIInstallZipFileHandler()), in a
 * .tar/.tar.gz/.tgz archive (see VSIInstallTarFileHandler()) or on a HTTP / FTP
 * server (see VSIInstallCurlFileHandler())
 *
 * In some situations (dealing with unverified data), the datasets can be opened
 * in another process through the \ref gdal_api_proxy mechanism.
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
    VALIDATE_POINTER1(pszFilename, "GDALOpenShared", NULL);
    return GDALOpenEx(pszFilename,
                      GDAL_OF_RASTER |
                          (eAccess == GA_Update ? GDAL_OF_UPDATE : 0) |
                          GDAL_OF_SHARED | GDAL_OF_VERBOSE_ERROR,
                      NULL, NULL, NULL);
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

    GDALDataset *poDS = static_cast<GDALDataset *>(hDS);

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

static int GDALDumpOpenSharedDatasetsForeach(void *elt, void *user_data)
{
    SharedDatasetCtxt *psStruct = static_cast<SharedDatasetCtxt *>(elt);
    FILE *fp = static_cast<FILE *>(user_data);
    GDALDataset *poDS = psStruct->poDS;

    const char *pszDriverName = poDS->GetDriver() == NULL
                                    ? "DriverIsNULL"
                                    : poDS->GetDriver()->GetDescription();

    poDS->Reference();
    CPL_IGNORE_RET_VAL(
        VSIFPrintf(fp, "  %d %c %-6s %7d %dx%dx%d %s\n", poDS->Dereference(),
                   poDS->GetShared() ? 'S' : 'N', pszDriverName,
                   static_cast<int>(psStruct->nPID), poDS->GetRasterXSize(),
                   poDS->GetRasterYSize(), poDS->GetRasterCount(),
                   poDS->GetDescription()));

    return TRUE;
}

static int GDALDumpOpenDatasetsForeach(GDALDataset *poDS, FILE *fp)
{

    // Don't list shared datasets. They have already been listed by
    // GDALDumpOpenSharedDatasetsForeach.
    if (poDS->GetShared())
        return TRUE;

    const char *pszDriverName = poDS->GetDriver() == NULL
                                    ? "DriverIsNULL"
                                    : poDS->GetDriver()->GetDescription();

    poDS->Reference();
    CPL_IGNORE_RET_VAL(
        VSIFPrintf(fp, "  %d %c %-6s %7d %dx%dx%d %s\n", poDS->Dereference(),
                   poDS->GetShared() ? 'S' : 'N', pszDriverName, -1,
                   poDS->GetRasterXSize(), poDS->GetRasterYSize(),
                   poDS->GetRasterCount(), poDS->GetDescription()));

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
    VALIDATE_POINTER1(fp, "GDALDumpOpenDatasets", 0);

    CPLMutexHolderD(&hDLMutex);

    if (poAllDatasetMap == NULL)
        return 0;

    CPL_IGNORE_RET_VAL(VSIFPrintf(fp, "Open GDAL Datasets:\n"));

    for( std::map<GDALDataset *, GIntBig>::iterator oIter =
             poAllDatasetMap->begin();
         oIter != poAllDatasetMap->end(); ++oIter )
    {
        GDALDumpOpenDatasetsForeach(oIter->first, fp);
    }

    if (phSharedDatasetSet != NULL)
    {
        CPLHashSetForeach(phSharedDatasetSet,
                          GDALDumpOpenSharedDatasetsForeach, fp);
    }
    return static_cast<int>(poAllDatasetMap->size());
}

/************************************************************************/
/*                        BeginAsyncReader()                            */
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

GDALAsyncReader *GDALDataset::BeginAsyncReader(
    int nXOff, int nYOff, int nXSize, int nYSize, void *pBuf, int nBufXSize,
    int nBufYSize, GDALDataType eBufType, int nBandCount, int *panBandMap,
    int nPixelSpace, int nLineSpace, int nBandSpace, char **papszOptions )
{
    // See gdaldefaultasync.cpp

    return GDALGetDefaultAsyncReader(this, nXOff, nYOff, nXSize, nYSize, pBuf,
                                     nBufXSize, nBufYSize, eBufType, nBandCount,
                                     panBandMap, nPixelSpace, nLineSpace,
                                     nBandSpace, papszOptions);
}

/************************************************************************/
/*                        GDALBeginAsyncReader()                      */
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
 * This method is the same as the C++ GDALDataset::BeginAsyncReader() method.
 *
 * @param hDS handle to the dataset object.
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
 * @return handle representing the request.
 */

GDALAsyncReaderH CPL_STDCALL
GDALBeginAsyncReader( GDALDatasetH hDS, int nXOff, int nYOff,
                      int nXSize, int nYSize,
                      void *pBuf,
                      int nBufXSize, int nBufYSize,
                      GDALDataType eBufType,
                      int nBandCount, int* panBandMap,
                      int nPixelSpace, int nLineSpace,
                      int nBandSpace,
                      char **papszOptions )

{
    VALIDATE_POINTER1(hDS, "GDALDataset", NULL);
    return static_cast<GDALAsyncReaderH>(
        static_cast<GDALDataset *>(hDS)->BeginAsyncReader(
            nXOff, nYOff, nXSize, nYSize, pBuf, nBufXSize, nBufYSize, eBufType,
            nBandCount, panBandMap, nPixelSpace, nLineSpace, nBandSpace,
            papszOptions));
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

void GDALDataset::EndAsyncReader(GDALAsyncReader *poARIO) { delete poARIO; }

/************************************************************************/
/*                        GDALEndAsyncReader()                        */
/************************************************************************/

/**
 * End asynchronous request.
 *
 * This method destroys an asynchronous io request and recovers all
 * resources associated with it.
 *
 * This method is the same as the C++ method GDALDataset::EndAsyncReader().
 *
 * @param hDS handle to the dataset object.
 * @param hAsyncReaderH handle returned by GDALBeginAsyncReader()
 */

void CPL_STDCALL GDALEndAsyncReader( GDALDatasetH hDS,
                                     GDALAsyncReaderH hAsyncReaderH )
{
    VALIDATE_POINTER0(hDS, "GDALDataset");
    VALIDATE_POINTER0(hAsyncReaderH, "GDALAsyncReader");
    static_cast<GDALDataset *>(hDS)
        ->EndAsyncReader(static_cast<GDALAsyncReader *>(hAsyncReaderH));
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
 * Basically the only safe action you can do after calling
 * CloseDependentDatasets() is to call the destructor.
 *
 * Note: the only legitimate caller of CloseDependentDatasets() is
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

void GDALDataset::ReportError(CPLErr eErrClass, CPLErrorNum err_no,
                              const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);

    char szNewFmt[256] = {};
    const char *pszDSName = GetDescription();
    if (strlen(fmt) + strlen(pszDSName) + 3 >= sizeof(szNewFmt) - 1)
        pszDSName = CPLGetFilename(pszDSName);
    if (pszDSName[0] != '\0' && strchr(pszDSName, '%') == NULL &&
        strlen(fmt) + strlen(pszDSName) + 3 < sizeof(szNewFmt) - 1)
    {
        snprintf(szNewFmt, sizeof(szNewFmt), "%s: %s", pszDSName, fmt);
        CPLErrorV(eErrClass, err_no, szNewFmt, args);
    }
    else
    {
        CPLErrorV(eErrClass, err_no, fmt, args);
    }
    va_end(args);
}

/************************************************************************/
/*                            GetMetadata()                             */
/************************************************************************/
char **GDALDataset::GetMetadata(const char *pszDomain)
{
    if( pszDomain != NULL && EQUAL(pszDomain, "DERIVED_SUBDATASETS") )
    {
        oDerivedMetadataList.Clear();

        // First condition: at least one raster band.
        if(GetRasterCount() > 0)
        {
            // Check if there is at least one complex band.
            bool hasAComplexBand = false;

            for(int rasterId = 1; rasterId <= GetRasterCount(); ++rasterId)
            {
                if(GDALDataTypeIsComplex(
                       GetRasterBand(rasterId)->GetRasterDataType()))
                {
                    hasAComplexBand = true;
                    break;
                }
            }

            unsigned int nbSupportedDerivedDS = 0;
            const DerivedDatasetDescription *poDDSDesc =
                GDALGetDerivedDatasetDescriptions(&nbSupportedDerivedDS);

            int nNumDataset = 1;
            for(unsigned int derivedId = 0; derivedId < nbSupportedDerivedDS;
                ++derivedId)
            {
                if(hasAComplexBand ||
                   CPLString(poDDSDesc[derivedId].pszInputPixelType) !=
                       "complex")
                {
                    oDerivedMetadataList.SetNameValue(
                        CPLSPrintf("DERIVED_SUBDATASET_%d_NAME", nNumDataset),
                        CPLSPrintf("DERIVED_SUBDATASET:%s:%s",
                                   poDDSDesc[derivedId].pszDatasetName,
                                   GetDescription()));

                    CPLString osDesc(
                        CPLSPrintf("%s from %s",
                                   poDDSDesc[derivedId].pszDatasetDescription,
                                   GetDescription()));
                    oDerivedMetadataList.SetNameValue(
                        CPLSPrintf("DERIVED_SUBDATASET_%d_DESC", nNumDataset),
                        osDesc.c_str());

                    nNumDataset++;
                }
            }
        }
        return oDerivedMetadataList.List();
    }

    return GDALMajorObject::GetMetadata(pszDomain);
}

/**
 * \fn GDALDataset::SetMetadata( char ** papszMetadata, const char * pszDomain)
 * \brief Set metadata.
 *
 * CAUTION: depending on the format, older values of the updated information
 * might still be found in the file in a "ghost" state, even if no longer
 * accessible through the GDAL API. This is for example the case of the GTiff
 * format (this is not a exhaustive list)
 *
 * The C function GDALSetMetadata() does the same thing as this method.
 *
 * @param papszMetadata the metadata in name=value string list format to
 * apply.
 * @param pszDomain the domain of interest.  Use "" or NULL for the default
 * domain.
 * @return CE_None on success, CE_Failure on failure and CE_Warning if the
 * metadata has been accepted, but is likely not maintained persistently
 * by the underlying object between sessions.
 */

/**
 * \fn GDALDataset::SetMetadataItem( const char * pszName, const char *
 * pszValue, const char * pszDomain)
 * \brief Set single metadata item.
 *
 * CAUTION: depending on the format, older values of the updated information
 * might still be found in the file in a "ghost" state, even if no longer
 * accessible through the GDAL API. This is for example the case of the GTiff
 * format (this is not a exhaustive list)
 *
 * The C function GDALSetMetadataItem() does the same thing as this method.
 *
 * @param pszName the key for the metadata item to fetch.
 * @param pszValue the value to assign to the key.
 * @param pszDomain the domain to set within, use NULL for the default domain.
 *
 * @return CE_None on success, or an error code on failure.
 */

/************************************************************************/
/*                            GetMetadataDomainList()                   */
/************************************************************************/

char **GDALDataset::GetMetadataDomainList()
{
    char **currentDomainList = CSLDuplicate(oMDMD.GetDomainList());

    // Ensure that we do not duplicate DERIVED domain.
    if(GetRasterCount() > 0 &&
       CSLFindString(currentDomainList, "DERIVED_SUBDATASETS") == -1)
    {
        currentDomainList =
            CSLAddString(currentDomainList, "DERIVED_SUBDATASETS");
    }
    return currentDomainList;
}

/************************************************************************/
/*                            GetDriverName()                           */
/************************************************************************/

/** Return driver name.
 * @return driver name.
 */
const char *GDALDataset::GetDriverName()
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
 @param hLayer the result of a previous ExecuteSQL() call.

*/
void GDALDatasetReleaseResultSet( GDALDatasetH hDS, OGRLayerH hLayer )

{
    VALIDATE_POINTER0(hDS, "GDALDatasetReleaseResultSet");

    static_cast<GDALDataset *>(hDS)
        ->ReleaseResultSet(reinterpret_cast<OGRLayer *>(hLayer));
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
    VALIDATE_POINTER1(hDS, "GDALDatasetH", 0);

    return static_cast<GDALDataset *>(hDS)->GetLayerCount();
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
    VALIDATE_POINTER1(hDS, "GDALDatasetGetLayer", NULL);

    return reinterpret_cast<OGRLayerH>(
        static_cast<GDALDataset *>(hDS)->GetLayer(iLayer));
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
 @param pszName the layer name of the layer to fetch.

 @return the layer, or NULL if Layer is not found or an error occurs.
*/

OGRLayerH GDALDatasetGetLayerByName( GDALDatasetH hDS, const char *pszName )

{
    VALIDATE_POINTER1(hDS, "GDALDatasetGetLayerByName", NULL);

    return reinterpret_cast<OGRLayerH>(
        static_cast<GDALDataset *>(hDS)->GetLayerByName(pszName));
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
    VALIDATE_POINTER1(hDS, "GDALDatasetH", OGRERR_INVALID_HANDLE);

    return static_cast<GDALDataset *>(hDS)->DeleteLayer(iLayer);
}

/************************************************************************/
/*                            CreateLayer()                             */
/************************************************************************/

/**
\brief This method attempts to create a new layer on the dataset with the
indicated name, coordinate system, geometry type.

The papszOptions argument
can be used to control driver specific creation options.  These options are
normally documented in the format specific documentation.

In GDAL 2.0, drivers should extend the ICreateLayer() method and not
CreateLayer().  CreateLayer() adds validation of layer creation options, before
delegating the actual work to ICreateLayer().

This method is the same as the C function GDALDatasetCreateLayer() and the
deprecated OGR_DS_CreateLayer().

In GDAL 1.X, this method used to be in the OGRDataSource class.

@param pszName the name for the new layer.  This should ideally not
match any existing layer on the datasource.
@param poSpatialRef the coordinate system to use for the new layer, or NULL if
no coordinate system is available.  The driver might only increase
the reference counter of the object to take ownership, and not make a full copy,
so do not use OSRDestroySpatialReference(), but OSRRelease() instead when you
are done with the object.
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
    ValidateLayerCreationOptions(papszOptions);

    if( OGR_GT_IsNonLinear(eGType) && !TestCapability(ODsCCurveGeometries) )
    {
        eGType = OGR_GT_GetLinear(eGType);
    }

    OGRLayer *poLayer =
        ICreateLayer(pszName, poSpatialRef, eGType, papszOptions);
#ifdef DEBUG
    if( poLayer != NULL && OGR_GT_IsNonLinear(poLayer->GetGeomType()) &&
        !poLayer->TestCapability(OLCCurveGeometries) )
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "Inconsistent driver: Layer geometry type is non-linear, but "
                 "TestCapability(OLCCurveGeometries) returns FALSE.");
    }
#endif

    return poLayer;
}

/************************************************************************/
/*                         GDALDatasetCreateLayer()                     */
/************************************************************************/

/**
\brief This function attempts to create a new layer on the dataset with the
indicated name, coordinate system, geometry type.

The papszOptions argument can be used to control driver specific creation
options.  These options are normally documented in the format specific
documentation.

This method is the same as the C++ method GDALDataset::CreateLayer().

@since GDAL 2.0

@param hDS the dataset handle
@param pszName the name for the new layer.  This should ideally not
match any existing layer on the datasource.
@param hSpatialRef the coordinate system to use for the new layer, or NULL if
no coordinate system is available.  The driver might only increase
the reference counter of the object to take ownership, and not make a full copy,
so do not use OSRDestroySpatialReference(), but OSRRelease() instead when you
are done with the object.
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
                              OGRwkbGeometryType eGType,
                              char ** papszOptions )

{
    VALIDATE_POINTER1(hDS, "GDALDatasetCreateLayer", NULL);

    if (pszName == NULL)
    {
        CPLError(CE_Failure, CPLE_ObjectNull,
                 "Name was NULL in GDALDatasetCreateLayer");
        return NULL;
    }
    return reinterpret_cast<OGRLayerH>(
        static_cast<GDALDataset *>(hDS)->CreateLayer(
            pszName, reinterpret_cast<OGRSpatialReference *>(hSpatialRef),
            eGType, papszOptions));
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
    VALIDATE_POINTER1(hDS, "OGR_DS_CopyGDALDatasetCopyLayerLayer", NULL);
    VALIDATE_POINTER1(hSrcLayer, "GDALDatasetCopyLayer", NULL);
    VALIDATE_POINTER1(pszNewName, "GDALDatasetCopyLayer", NULL);

    return reinterpret_cast<OGRLayerH>(
        static_cast<GDALDataset *>(hDS)->CopyLayer(
            reinterpret_cast<OGRLayer *>(hSrcLayer), pszNewName, papszOptions));
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
 review the <a href="ogr_sql.html">OGR SQL</a> document.  Some drivers (i.e.
 Oracle and PostGIS) pass the SQL directly through to the underlying RDBMS.

 Starting with OGR 1.10, the <a href="ogr_sql_sqlite.html">SQLITE dialect</a>
 can also be used.

 @since GDAL 2.0

 @param hDS the dataset handle.
 @param pszStatement the SQL statement to execute.
 @param hSpatialFilter geometry which represents a spatial filter. Can be NULL.

 @param pszDialect allows control of the statement dialect. If set to NULL, the
 OGR SQL engine will be used, except for RDBMS drivers that will use their
 dedicated SQL engine, unless OGRSQL is explicitly passed as the
 dialect. Starting with OGR 1.10, the SQLITE dialect can also be used.

 @return an OGRLayer containing the results of the query.  Deallocate with
 ReleaseResultSet().

*/

OGRLayerH GDALDatasetExecuteSQL( GDALDatasetH hDS,
                             const char *pszStatement,
                             OGRGeometryH hSpatialFilter,
                             const char *pszDialect )

{
    VALIDATE_POINTER1(hDS, "GDALDatasetExecuteSQL", NULL);

    return reinterpret_cast<OGRLayerH>(
        static_cast<GDALDataset *>(hDS)->ExecuteSQL(
            pszStatement, reinterpret_cast<OGRGeometry *>(hSpatialFilter),
            pszDialect));
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
    VALIDATE_POINTER1(hDS, "OGR_DS_GetStyleTable", NULL);

    return reinterpret_cast<OGRStyleTableH>(
        static_cast<GDALDataset *>(hDS)->GetStyleTable());
}

/************************************************************************/
/*                    GDALDatasetSetStyleTableDirectly()                */
/************************************************************************/

/**
 \brief Set dataset style table.

 This function operate exactly as GDALDatasetSetStyleTable() except that it
 assumes ownership of the passed table.

 This function is the same as the C++ method
 GDALDataset::SetStyleTableDirectly()

 @since GDAL 2.0

 @param hDS the dataset handle
 @param hStyleTable style table handle to set

*/

void GDALDatasetSetStyleTableDirectly( GDALDatasetH hDS,
                                       OGRStyleTableH hStyleTable )

{
    VALIDATE_POINTER0(hDS, "OGR_DS_SetStyleTableDirectly");

    static_cast<GDALDataset *>(hDS)
        ->SetStyleTableDirectly(reinterpret_cast<OGRStyleTable *>(hStyleTable));
}

/************************************************************************/
/*                     GDALDatasetSetStyleTable()                       */
/************************************************************************/

/**
 \brief Set dataset style table.

 This function operate exactly as GDALDatasetSetStyleTableDirectly() except that
 it assumes ownership of the passed table.

 This function is the same as the C++ method GDALDataset::SetStyleTable()

 @since GDAL 2.0

 @param hDS the dataset handle
 @param hStyleTable style table handle to set

*/

void GDALDatasetSetStyleTable( GDALDatasetH hDS, OGRStyleTableH hStyleTable )

{
    VALIDATE_POINTER0(hDS, "OGR_DS_SetStyleTable");
    VALIDATE_POINTER0(hStyleTable, "OGR_DS_SetStyleTable");

    static_cast<GDALDataset *>(hDS)
        ->SetStyleTable(reinterpret_cast<OGRStyleTable *>(hStyleTable));
}

/************************************************************************/
/*                    ValidateLayerCreationOptions()                    */
/************************************************************************/

//! @cond Doxygen_Suppress
int GDALDataset::ValidateLayerCreationOptions( const char* const* papszLCO )
{
    const char *pszOptionList =
        GetMetadataItem(GDAL_DS_LAYER_CREATIONOPTIONLIST);
    if( pszOptionList == NULL && poDriver != NULL )
    {
        pszOptionList =
            poDriver->GetMetadataItem( GDAL_DS_LAYER_CREATIONOPTIONLIST );
    }
    CPLString osDataset;
    osDataset.Printf("dataset %s", GetDescription());
    return GDALValidateOptions(pszOptionList, papszLCO,
                               "layer creation option",
                               osDataset);
}
//! @endcond

/************************************************************************/
/*                              Release()                               */
/************************************************************************/

/**
\brief Drop a reference to this dataset, and if the reference count drops to one
close (destroy) the dataset.

This method is the same as the C function OGRReleaseDataSource().

@deprecated. In GDAL 2, use GDALClose() instead

@return OGRERR_NONE on success or an error code.
*/

OGRErr GDALDataset::Release()

{
    ReleaseRef();
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

int GDALDataset::GetRefCount() const { return nRefCount; }

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
    GDALDatasetPrivate *psPrivate =
        static_cast<GDALDatasetPrivate *>(m_hPrivateData);
    CPLMutexHolderD(psPrivate ? &(psPrivate->hMutex) : NULL);
    int nSummaryCount = nRefCount;
    GDALDataset *poUseThis = const_cast<GDALDataset *>(this);

    for( int iLayer = 0; iLayer < poUseThis->GetLayerCount(); ++iLayer )
        nSummaryCount += poUseThis->GetLayer(iLayer)->GetRefCount();

    return nSummaryCount;
}

/************************************************************************/
/*                           ICreateLayer()                             */
/************************************************************************/

/**
 \brief This method attempts to create a new layer on the dataset with the
 indicated name, coordinate system, geometry type.

 This method is reserved to implementation by drivers.

 The papszOptions argument can be used to control driver specific creation
 options.  These options are normally documented in the format specific
 documentation.

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

OGRLayer *GDALDataset::ICreateLayer( const char * /* pszName */,
                                     OGRSpatialReference * /* poSpatialRef */,
                                     OGRwkbGeometryType /* eGType */,
                                     char ** /* papszOptions */ )

{
    CPLError(CE_Failure, CPLE_NotSupported,
             "CreateLayer() not supported by this dataset.");

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
                     specific. There is a common option to set output layer
                     spatial reference: DST_SRSWKT. The option should be in
                     WKT format.

 @return an handle to the layer, or NULL if an error occurs.
*/

OGRLayer *GDALDataset::CopyLayer( OGRLayer *poSrcLayer,
                                  const char *pszNewName,
                                  char **papszOptions )

{
/* -------------------------------------------------------------------- */
/*      Create the layer.                                               */
/* -------------------------------------------------------------------- */
    if( !TestCapability( ODsCCreateLayer ) )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "This datasource does not support creation of layers.");
        return NULL;
    }

    const char *pszSRSWKT = CSLFetchNameValue(papszOptions, "DST_SRSWKT");
    OGRSpatialReference oDstSpaRef(pszSRSWKT);
    OGRFeatureDefn *poSrcDefn = poSrcLayer->GetLayerDefn();
    OGRLayer *poDstLayer = NULL;

    CPLErrorReset();
    if( poSrcDefn->GetGeomFieldCount() > 1 &&
        TestCapability(ODsCCreateGeomFieldAfterCreateLayer) )
    {
        poDstLayer = ICreateLayer(pszNewName, NULL, wkbNone, papszOptions);
    }
    else
    {
        if(NULL == pszSRSWKT)
        {
            poDstLayer = ICreateLayer(pszNewName, poSrcLayer->GetSpatialRef(),
                                      poSrcDefn->GetGeomType(), papszOptions);
        }
        else
        {
            // Remove DST_WKT from option list to prevent warning from driver.
            const int nSRSPos = CSLFindName(papszOptions, "DST_SRSWKT");
            papszOptions = CSLRemoveStrings(papszOptions, nSRSPos, 1, NULL);
            poDstLayer = ICreateLayer(pszNewName, &oDstSpaRef,
                                      poSrcDefn->GetGeomType(), papszOptions);
        }
    }

    if( poDstLayer == NULL )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Add fields.  Default to copy all fields, and make sure to       */
/*      establish a mapping between indices, rather than names, in      */
/*      case the target datasource has altered it (e.g. Shapefile       */
/*      limited to 10 char field names).                                */
/* -------------------------------------------------------------------- */
    const int nSrcFieldCount = poSrcDefn->GetFieldCount();

    // Initialize the index-to-index map to -1's.
    int *panMap = static_cast<int *>(CPLMalloc(sizeof(int) * nSrcFieldCount));
    for( int iField = 0; iField < nSrcFieldCount; ++iField )
        panMap[iField] = -1;

    // Caution: At the time of writing, the MapInfo driver
    // returns NULL until a field has been added.
    OGRFeatureDefn *poDstFDefn = poDstLayer->GetLayerDefn();
    int nDstFieldCount = poDstFDefn ? poDstFDefn->GetFieldCount() : 0;
    for( int iField = 0; iField < nSrcFieldCount; ++iField )
    {
        OGRFieldDefn *poSrcFieldDefn = poSrcDefn->GetFieldDefn(iField);
        OGRFieldDefn oFieldDefn(poSrcFieldDefn);

        // The field may have been already created at layer creation.
        int iDstField = -1;
        if (poDstFDefn)
            iDstField = poDstFDefn->GetFieldIndex(oFieldDefn.GetNameRef());
        if (iDstField >= 0)
        {
            panMap[iField] = iDstField;
        }
        else if (poDstLayer->CreateField( &oFieldDefn ) == OGRERR_NONE)
        {
            // Now that we've created a field, GetLayerDefn() won't return NULL.
            if (poDstFDefn == NULL)
                poDstFDefn = poDstLayer->GetLayerDefn();

            // Sanity check: if it fails, the driver is buggy.
            if (poDstFDefn != NULL &&
                poDstFDefn->GetFieldCount() != nDstFieldCount + 1)
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "The output driver has claimed to have added the %s "
                         "field, but it did not!",
                         oFieldDefn.GetNameRef());
            }
            else
            {
                panMap[iField] = nDstFieldCount;
                ++nDstFieldCount;
            }
        }
    }

/* -------------------------------------------------------------------- */
    OGRCoordinateTransformation *poCT = NULL;
    OGRSpatialReference *sourceSRS = poSrcLayer->GetSpatialRef();
    if (sourceSRS != NULL && pszSRSWKT != NULL &&
            sourceSRS->IsSame(&oDstSpaRef) == FALSE)
    {
        poCT = OGRCreateCoordinateTransformation(sourceSRS, &oDstSpaRef);
        if(NULL == poCT)
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "This input/output spatial reference is not supported.");
            CPLFree(panMap);
            return NULL;
        }
    }
/* -------------------------------------------------------------------- */
/*      Create geometry fields.                                         */
/* -------------------------------------------------------------------- */
    const int nSrcGeomFieldCount = poSrcDefn->GetGeomFieldCount();
    if( nSrcGeomFieldCount > 1 &&
        TestCapability(ODsCCreateGeomFieldAfterCreateLayer) )
    {

        for( int iField = 0; iField < nSrcGeomFieldCount; ++iField )
        {
            if(NULL == pszSRSWKT)
            {
                poDstLayer->CreateGeomField(
                    poSrcDefn->GetGeomFieldDefn(iField));
            }
            else
            {
                OGRGeomFieldDefn *pDstGeomFieldDefn =
                    poSrcDefn->GetGeomFieldDefn(iField);
                pDstGeomFieldDefn->SetSpatialRef(&oDstSpaRef);
                poDstLayer->CreateGeomField(pDstGeomFieldDefn);
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Check if the destination layer supports transactions and set a  */
/*      default number of features in a single transaction.             */
/* -------------------------------------------------------------------- */
    const int nGroupTransactions =
        poDstLayer->TestCapability(OLCTransactions) ? 128 : 0;

/* -------------------------------------------------------------------- */
/*      Transfer features.                                              */
/* -------------------------------------------------------------------- */
    OGRFeature *poFeature = NULL;

    poSrcLayer->ResetReading();

    if( nGroupTransactions <= 0 )
    {
        while( true )
        {
            poFeature = poSrcLayer->GetNextFeature();

            if( poFeature == NULL )
                break;

            CPLErrorReset();
            OGRFeature *poDstFeature =
                OGRFeature::CreateFeature(poDstLayer->GetLayerDefn());

            if( poDstFeature->SetFrom( poFeature, panMap, TRUE ) !=
                    OGRERR_NONE )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Unable to translate feature " CPL_FRMT_GIB
                         " from layer %s.",
                         poFeature->GetFID(), poSrcDefn->GetName());
                OGRFeature::DestroyFeature(poFeature);
                CPLFree(panMap);
                if(NULL != poCT)
                    OCTDestroyCoordinateTransformation(
                        reinterpret_cast<OGRCoordinateTransformationH>(poCT));
                return poDstLayer;
            }

            if(NULL != poCT)
            {
                for( int iField = 0; iField < nSrcGeomFieldCount; ++iField )
                {
                    OGRGeometry *pGeom = poDstFeature->GetGeomFieldRef(iField);
                    if(NULL == pGeom)
                        continue;

                    const OGRErr eErr = pGeom->transform(poCT);
                    if(eErr == OGRERR_NONE)
                        continue;

                    CPLError(
                        CE_Failure, CPLE_AppDefined,
                        "Unable to transform geometry " CPL_FRMT_GIB
                        " from layer %s.",
                        poFeature->GetFID(), poSrcDefn->GetName());
                    OGRFeature::DestroyFeature(poFeature);
                    CPLFree(panMap);
                    OCTDestroyCoordinateTransformation(
                        reinterpret_cast<OGRCoordinateTransformationH>(poCT));
                    return poDstLayer;
                }
            }

            poDstFeature->SetFID(poFeature->GetFID());

            OGRFeature::DestroyFeature(poFeature);

            CPLErrorReset();
            if( poDstLayer->CreateFeature( poDstFeature ) != OGRERR_NONE )
            {
                OGRFeature::DestroyFeature(poDstFeature);
                CPLFree(panMap);
                if(NULL != poCT)
                    OCTDestroyCoordinateTransformation(
                        reinterpret_cast<OGRCoordinateTransformationH>(poCT));
                return poDstLayer;
            }

            OGRFeature::DestroyFeature(poDstFeature);
        }
    }
    else
    {
        OGRFeature **papoDstFeature = static_cast<OGRFeature **>(
            VSI_CALLOC_VERBOSE(sizeof(OGRFeature *), nGroupTransactions));

        bool bStopTransfer = papoDstFeature == NULL;
        while( !bStopTransfer )
        {
/* -------------------------------------------------------------------- */
/*      Fill the array with features.                                   */
/* -------------------------------------------------------------------- */
            // Number of features in the temporary array.
            int nFeatCount = 0;  // Used after for.
            for( nFeatCount = 0; nFeatCount < nGroupTransactions; ++nFeatCount )
            {
                poFeature = poSrcLayer->GetNextFeature();

                if( poFeature == NULL )
                {
                    bStopTransfer = true;
                    break;
                }

                CPLErrorReset();
                papoDstFeature[nFeatCount] =
                    OGRFeature::CreateFeature(poDstLayer->GetLayerDefn());

                if( papoDstFeature[nFeatCount]->SetFrom(poFeature, panMap,
                                                       TRUE) != OGRERR_NONE )
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Unable to translate feature " CPL_FRMT_GIB
                             " from layer %s.",
                             poFeature->GetFID(), poSrcDefn->GetName());
                    OGRFeature::DestroyFeature( poFeature );
                    poFeature = NULL;
                    bStopTransfer = true;
                    break;
                }

                if(NULL != poCT)
                {
                    for( int iField = 0; iField < nSrcGeomFieldCount; ++iField )
                    {
                        OGRGeometry *pGeom =
                            papoDstFeature[nFeatCount]->GetGeomFieldRef(iField);
                        if(NULL == pGeom)
                            continue;

                        const OGRErr eErr = pGeom->transform(poCT);
                        if(eErr == OGRERR_NONE)
                            continue;

                        CPLError(
                            CE_Failure, CPLE_AppDefined,
                            "Unable to transform geometry " CPL_FRMT_GIB
                            " from layer %s.",
                            poFeature->GetFID(), poSrcDefn->GetName());
                        OGRFeature::DestroyFeature(poFeature);
                        bStopTransfer = true;
                        poFeature = NULL;
                        break;
                    }
                }

                if (poFeature)
                {
                    papoDstFeature[nFeatCount]->SetFID(poFeature->GetFID());
                    OGRFeature::DestroyFeature(poFeature);
                    poFeature = NULL;
                }
            }
            int nFeaturesToAdd = nFeatCount;

            CPLErrorReset();
            bool bStopTransaction = false;
            while( !bStopTransaction )
            {
                bStopTransaction = true;
                if( poDstLayer->StartTransaction() != OGRERR_NONE )
                    break;
                for( int i = 0; i < nFeaturesToAdd; ++i )
                {
                    if( poDstLayer->CreateFeature( papoDstFeature[i] ) !=
                       OGRERR_NONE )
                    {
                        nFeaturesToAdd = i;
                        bStopTransfer = true;
                        bStopTransaction = false;
                    }
                }
                if( bStopTransaction )
                {
                    if( poDstLayer->CommitTransaction() != OGRERR_NONE )
                        break;
                }
                else
                {
                    poDstLayer->RollbackTransaction();
                }
            }

            for( int i = 0; i < nFeatCount; ++i )
                OGRFeature::DestroyFeature(papoDstFeature[i]);
        }
        CPLFree(papoDstFeature);
    }

    if(NULL != poCT)
        OCTDestroyCoordinateTransformation(
            reinterpret_cast<OGRCoordinateTransformationH>(poCT));

    CPLFree(panMap);

    return poDstLayer;
}

/************************************************************************/
/*                            DeleteLayer()                             */
/************************************************************************/

/**
 \fn GDALDataset::DeleteLayer(int)
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

OGRErr GDALDataset::DeleteLayer( CPL_UNUSED int iLayer )

{
    CPLError(CE_Failure, CPLE_NotSupported,
             "DeleteLayer() not supported by this dataset.");

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

 @param pszName the layer name of the layer to fetch.

 @return the layer, or NULL if Layer is not found or an error occurs.
*/

OGRLayer *GDALDataset::GetLayerByName( const char *pszName )

{
    GDALDatasetPrivate *psPrivate =
        static_cast<GDALDatasetPrivate *>(m_hPrivateData);
    CPLMutexHolderD(psPrivate ? &(psPrivate->hMutex) : NULL);

    if ( ! pszName )
        return NULL;

    // First a case sensitive check.
    for( int i = 0; i < GetLayerCount(); ++i )
    {
        OGRLayer *poLayer = GetLayer(i);

        if( strcmp( pszName, poLayer->GetName() ) == 0 )
            return poLayer;
    }

    // Then case insensitive.
    for( int i = 0; i < GetLayerCount(); ++i )
    {
        OGRLayer *poLayer = GetLayer(i);

        if( EQUAL( pszName, poLayer->GetName() ) )
            return poLayer;
    }

    return NULL;
}

//! @cond Doxygen_Suppress
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
    char **papszTokens = CSLTokenizeString(pszSQLCommand);

/* -------------------------------------------------------------------- */
/*      Do some general syntax checking.                                */
/* -------------------------------------------------------------------- */
    if( CSLCount(papszTokens) != 6
        || !EQUAL(papszTokens[0], "CREATE")
        || !EQUAL(papszTokens[1], "INDEX")
        || !EQUAL(papszTokens[2], "ON")
        || !EQUAL(papszTokens[4], "USING") )
    {
        CSLDestroy(papszTokens);
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Syntax error in CREATE INDEX command.\n"
                 "Was '%s'\n"
                 "Should be of form 'CREATE INDEX ON <table> USING <field>'",
                 pszSQLCommand);
        return OGRERR_FAILURE;
    }

/* -------------------------------------------------------------------- */
/*      Find the named layer.                                           */
/* -------------------------------------------------------------------- */
    OGRLayer *poLayer = NULL;

    {
        GDALDatasetPrivate *psPrivate =
            static_cast<GDALDatasetPrivate *>(m_hPrivateData);
        CPLMutexHolderD(psPrivate ? &(psPrivate->hMutex) : NULL);

        for( int i = 0; i < GetLayerCount(); ++i )
        {
            poLayer = GetLayer(i);

            if( poLayer!= NULL && EQUAL(poLayer->GetName(),papszTokens[3]) )
                break;
            poLayer = NULL;
        }

        if( poLayer == NULL )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "CREATE INDEX ON failed, no such layer as `%s'.",
                     papszTokens[3]);
            CSLDestroy(papszTokens);
            return OGRERR_FAILURE;
        }
    }

/* -------------------------------------------------------------------- */
/*      Does this layer even support attribute indexes?                 */
/* -------------------------------------------------------------------- */
    if( poLayer->GetIndex() == NULL )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "CREATE INDEX ON not supported by this driver.");
        CSLDestroy(papszTokens);
        return OGRERR_FAILURE;
    }

/* -------------------------------------------------------------------- */
/*      Find the named field.                                           */
/* -------------------------------------------------------------------- */
    int i = 0;  // Used after for.
    for( ; i < poLayer->GetLayerDefn()->GetFieldCount(); ++i )
    {
        if( EQUAL(papszTokens[5],
                  poLayer->GetLayerDefn()->GetFieldDefn(i)->GetNameRef()) )
            break;
    }

    CSLDestroy(papszTokens);

    if( i >= poLayer->GetLayerDefn()->GetFieldCount() )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "`%s' failed, field not found.",
                 pszSQLCommand);
        return OGRERR_FAILURE;
    }

/* -------------------------------------------------------------------- */
/*      Attempt to create the index.                                    */
/* -------------------------------------------------------------------- */
    OGRErr eErr = poLayer->GetIndex()->CreateIndex(i);
    if( eErr == OGRERR_NONE )
    {
        eErr = poLayer->GetIndex()->IndexAllFeatures(i);
    }
    else
    {
        if( strlen(CPLGetLastErrorMsg()) == 0 )
            CPLError(CE_Failure, CPLE_AppDefined, "Cannot '%s'", pszSQLCommand);
    }

    return eErr;
}

/************************************************************************/
/*                        ProcessSQLDropIndex()                         */
/*                                                                      */
/*      The correct syntax for dropping one or more indexes in          */
/*      the OGR SQL dialect is:                                         */
/*                                                                      */
/*          DROP INDEX ON <layername> [USING <columnname>]              */
/************************************************************************/

OGRErr GDALDataset::ProcessSQLDropIndex( const char *pszSQLCommand )

{
    char **papszTokens = CSLTokenizeString(pszSQLCommand);

/* -------------------------------------------------------------------- */
/*      Do some general syntax checking.                                */
/* -------------------------------------------------------------------- */
    if( (CSLCount(papszTokens) != 4 && CSLCount(papszTokens) != 6)
        || !EQUAL(papszTokens[0], "DROP")
        || !EQUAL(papszTokens[1], "INDEX")
        || !EQUAL(papszTokens[2], "ON")
        || (CSLCount(papszTokens) == 6 && !EQUAL(papszTokens[4], "USING")) )
    {
        CSLDestroy(papszTokens);
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Syntax error in DROP INDEX command.\n"
                 "Was '%s'\n"
                 "Should be of form 'DROP INDEX ON <table> [USING <field>]'",
                 pszSQLCommand);
        return OGRERR_FAILURE;
    }

/* -------------------------------------------------------------------- */
/*      Find the named layer.                                           */
/* -------------------------------------------------------------------- */
    OGRLayer *poLayer = NULL;

    {
        GDALDatasetPrivate *psPrivate =
            static_cast<GDALDatasetPrivate *>(m_hPrivateData);
        CPLMutexHolderD(psPrivate ? &(psPrivate->hMutex) : NULL);

        for( int i = 0; i < GetLayerCount(); ++i )
        {
            poLayer = GetLayer(i);

            if( poLayer!= NULL && EQUAL(poLayer->GetName(),papszTokens[3]) )
                break;
            poLayer = NULL;
        }

        if( poLayer == NULL )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "CREATE INDEX ON failed, no such layer as `%s'.",
                     papszTokens[3]);
            CSLDestroy(papszTokens);
            return OGRERR_FAILURE;
        }
    }

/* -------------------------------------------------------------------- */
/*      Does this layer even support attribute indexes?                 */
/* -------------------------------------------------------------------- */
    if( poLayer->GetIndex() == NULL )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Indexes not supported by this driver.");
        CSLDestroy(papszTokens);
        return OGRERR_FAILURE;
    }

/* -------------------------------------------------------------------- */
/*      If we were not given a field name, drop all indexes.            */
/* -------------------------------------------------------------------- */
    if( CSLCount(papszTokens) == 4 )
    {
        for( int i = 0; i < poLayer->GetLayerDefn()->GetFieldCount(); ++i )
        {
            OGRAttrIndex *poAttrIndex;

            poAttrIndex = poLayer->GetIndex()->GetFieldIndex(i);
            if( poAttrIndex != NULL )
            {
                const OGRErr eErr = poLayer->GetIndex()->DropIndex(i);
                if(eErr != OGRERR_NONE)
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
    int i = 0;  // Used after for.
    for( ; i < poLayer->GetLayerDefn()->GetFieldCount(); ++i )
    {
        if( EQUAL(papszTokens[5],
                  poLayer->GetLayerDefn()->GetFieldDefn(i)->GetNameRef()) )
            break;
    }

    CSLDestroy(papszTokens);

    if( i >= poLayer->GetLayerDefn()->GetFieldCount() )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "`%s' failed, field not found.",
                 pszSQLCommand);
        return OGRERR_FAILURE;
    }

/* -------------------------------------------------------------------- */
/*      Attempt to drop the index.                                      */
/* -------------------------------------------------------------------- */
    const OGRErr eErr = poLayer->GetIndex()->DropIndex(i);

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
    char **papszTokens = CSLTokenizeString(pszSQLCommand);

/* -------------------------------------------------------------------- */
/*      Do some general syntax checking.                                */
/* -------------------------------------------------------------------- */
    if( CSLCount(papszTokens) != 3
        || !EQUAL(papszTokens[0], "DROP")
        || !EQUAL(papszTokens[1], "TABLE") )
    {
        CSLDestroy(papszTokens);
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Syntax error in DROP TABLE command.\n"
                 "Was '%s'\n"
                 "Should be of form 'DROP TABLE <table>'",
                 pszSQLCommand);
        return OGRERR_FAILURE;
    }

/* -------------------------------------------------------------------- */
/*      Find the named layer.                                           */
/* -------------------------------------------------------------------- */
    OGRLayer *poLayer = NULL;

    int i = 0;  // Used after for.
    for( ; i < GetLayerCount(); ++i )
    {
        poLayer = GetLayer(i);

        if( poLayer!= NULL && EQUAL(poLayer->GetName(),papszTokens[2]) )
            break;
        poLayer = NULL;
    }

    if( poLayer == NULL )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "DROP TABLE failed, no such layer as `%s'.", papszTokens[2]);
        CSLDestroy(papszTokens);
        return OGRERR_FAILURE;
    }

    CSLDestroy(papszTokens);

/* -------------------------------------------------------------------- */
/*      Delete it.                                                      */
/* -------------------------------------------------------------------- */

    return DeleteLayer(i);
}
//! @endcond

/************************************************************************/
/*                    GDALDatasetParseSQLType()                       */
/************************************************************************/

/* All arguments will be altered */
static OGRFieldType GDALDatasetParseSQLType(char *pszType, int &nWidth,
                                            int &nPrecision)
{
    char *pszParenthesis = strchr(pszType, '(');
    if (pszParenthesis)
    {
        nWidth = atoi(pszParenthesis + 1);
        *pszParenthesis = '\0';
        char *pszComma = strchr(pszParenthesis + 1, ',');
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
        CPLError(CE_Warning, CPLE_NotSupported,
                 "Unsupported column type '%s'. Defaulting to VARCHAR",
                 pszType);

    return eType;
}

/************************************************************************/
/*                    ProcessSQLAlterTableAddColumn()                   */
/*                                                                      */
/*      The correct syntax for adding a column in the OGR SQL           */
/*      dialect is:                                                     */
/*                                                                      */
/*       ALTER TABLE <layername> ADD [COLUMN] <columnname> <columntype> */
/************************************************************************/

//! @cond Doxygen_Suppress
OGRErr GDALDataset::ProcessSQLAlterTableAddColumn( const char *pszSQLCommand )

{
    char **papszTokens = CSLTokenizeString(pszSQLCommand);

/* -------------------------------------------------------------------- */
/*      Do some general syntax checking.                                */
/* -------------------------------------------------------------------- */
    const char *pszLayerName = NULL;
    const char *pszColumnName = NULL;
    int iTypeIndex = 0;
    const int nTokens = CSLCount(papszTokens);

    if( nTokens >= 7
        && EQUAL(papszTokens[0], "ALTER")
        && EQUAL(papszTokens[1], "TABLE")
        && EQUAL(papszTokens[3], "ADD")
        && EQUAL(papszTokens[4], "COLUMN"))
    {
        pszLayerName = papszTokens[2];
        pszColumnName = papszTokens[5];
        iTypeIndex = 6;
    }
    else if( nTokens >= 6
             && EQUAL(papszTokens[0], "ALTER")
             && EQUAL(papszTokens[1], "TABLE")
             && EQUAL(papszTokens[3], "ADD"))
    {
        pszLayerName = papszTokens[2];
        pszColumnName = papszTokens[4];
        iTypeIndex = 5;
    }
    else
    {
        CSLDestroy(papszTokens);
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Syntax error in ALTER TABLE ADD COLUMN command.\n"
                 "Was '%s'\n"
                 "Should be of form 'ALTER TABLE <layername> ADD [COLUMN] "
                 "<columnname> <columntype>'",
                 pszSQLCommand);
        return OGRERR_FAILURE;
    }

/* -------------------------------------------------------------------- */
/*      Merge type components into a single string if there were split  */
/*      with spaces                                                     */
/* -------------------------------------------------------------------- */
    CPLString osType;
    for( int i = iTypeIndex; i < nTokens; ++i )
    {
        osType += papszTokens[i];
        CPLFree(papszTokens[i]);
    }
    char *pszType = papszTokens[iTypeIndex] = CPLStrdup(osType);
    papszTokens[iTypeIndex + 1] = NULL;

/* -------------------------------------------------------------------- */
/*      Find the named layer.                                           */
/* -------------------------------------------------------------------- */
    OGRLayer *poLayer = GetLayerByName(pszLayerName);
    if( poLayer == NULL )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "%s failed, no such layer as `%s'.", pszSQLCommand,
                 pszLayerName);
        CSLDestroy(papszTokens);
        return OGRERR_FAILURE;
    }

/* -------------------------------------------------------------------- */
/*      Add column.                                                     */
/* -------------------------------------------------------------------- */

    int nWidth = 0;
    int nPrecision = 0;
    OGRFieldType eType = GDALDatasetParseSQLType(pszType, nWidth, nPrecision);
    OGRFieldDefn oFieldDefn(pszColumnName, eType);
    oFieldDefn.SetWidth(nWidth);
    oFieldDefn.SetPrecision(nPrecision);

    CSLDestroy(papszTokens);

    return poLayer->CreateField(&oFieldDefn);
}

/************************************************************************/
/*                    ProcessSQLAlterTableDropColumn()                  */
/*                                                                      */
/*      The correct syntax for dropping a column in the OGR SQL         */
/*      dialect is:                                                     */
/*                                                                      */
/*          ALTER TABLE <layername> DROP [COLUMN] <columnname>          */
/************************************************************************/

OGRErr GDALDataset::ProcessSQLAlterTableDropColumn( const char *pszSQLCommand )

{
    char **papszTokens = CSLTokenizeString(pszSQLCommand);

/* -------------------------------------------------------------------- */
/*      Do some general syntax checking.                                */
/* -------------------------------------------------------------------- */
    const char *pszLayerName = NULL;
    const char *pszColumnName = NULL;
    if( CSLCount(papszTokens) == 6
        && EQUAL(papszTokens[0], "ALTER")
        && EQUAL(papszTokens[1], "TABLE")
        && EQUAL(papszTokens[3], "DROP")
        && EQUAL(papszTokens[4], "COLUMN"))
    {
        pszLayerName = papszTokens[2];
        pszColumnName = papszTokens[5];
    }
    else if( CSLCount(papszTokens) == 5
             && EQUAL(papszTokens[0], "ALTER")
             && EQUAL(papszTokens[1], "TABLE")
             && EQUAL(papszTokens[3], "DROP"))
    {
        pszLayerName = papszTokens[2];
        pszColumnName = papszTokens[4];
    }
    else
    {
        CSLDestroy(papszTokens);
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Syntax error in ALTER TABLE DROP COLUMN command.\n"
                 "Was '%s'\n"
                 "Should be of form 'ALTER TABLE <layername> DROP [COLUMN] "
                 "<columnname>'",
                 pszSQLCommand);
        return OGRERR_FAILURE;
    }

/* -------------------------------------------------------------------- */
/*      Find the named layer.                                           */
/* -------------------------------------------------------------------- */
    OGRLayer *poLayer = GetLayerByName(pszLayerName);
    if( poLayer == NULL )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "%s failed, no such layer as `%s'.", pszSQLCommand,
                 pszLayerName);
        CSLDestroy(papszTokens);
        return OGRERR_FAILURE;
    }

/* -------------------------------------------------------------------- */
/*      Find the field.                                                 */
/* -------------------------------------------------------------------- */

    int nFieldIndex = poLayer->GetLayerDefn()->GetFieldIndex(pszColumnName);
    if( nFieldIndex < 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "%s failed, no such field as `%s'.", pszSQLCommand,
                 pszColumnName);
        CSLDestroy(papszTokens);
        return OGRERR_FAILURE;
    }

/* -------------------------------------------------------------------- */
/*      Remove it.                                                      */
/* -------------------------------------------------------------------- */

    CSLDestroy(papszTokens);

    return poLayer->DeleteField(nFieldIndex);
}

/************************************************************************/
/*                 ProcessSQLAlterTableRenameColumn()                   */
/*                                                                      */
/*      The correct syntax for renaming a column in the OGR SQL         */
/*      dialect is:                                                     */
/*                                                                      */
/*       ALTER TABLE <layername> RENAME [COLUMN] <oldname> TO <newname> */
/************************************************************************/

OGRErr
GDALDataset::ProcessSQLAlterTableRenameColumn( const char *pszSQLCommand )

{
    char **papszTokens = CSLTokenizeString(pszSQLCommand);

/* -------------------------------------------------------------------- */
/*      Do some general syntax checking.                                */
/* -------------------------------------------------------------------- */
    const char *pszLayerName = NULL;
    const char *pszOldColName = NULL;
    const char *pszNewColName = NULL;
    if( CSLCount(papszTokens) == 8
        && EQUAL(papszTokens[0], "ALTER")
        && EQUAL(papszTokens[1], "TABLE")
        && EQUAL(papszTokens[3], "RENAME")
        && EQUAL(papszTokens[4], "COLUMN")
        && EQUAL(papszTokens[6], "TO"))
    {
        pszLayerName = papszTokens[2];
        pszOldColName = papszTokens[5];
        pszNewColName = papszTokens[7];
    }
    else if( CSLCount(papszTokens) == 7
             && EQUAL(papszTokens[0], "ALTER")
             && EQUAL(papszTokens[1], "TABLE")
             && EQUAL(papszTokens[3], "RENAME")
             && EQUAL(papszTokens[5], "TO"))
    {
        pszLayerName = papszTokens[2];
        pszOldColName = papszTokens[4];
        pszNewColName = papszTokens[6];
    }
    else
    {
        CSLDestroy(papszTokens);
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Syntax error in ALTER TABLE RENAME COLUMN command.\n"
                 "Was '%s'\n"
                 "Should be of form 'ALTER TABLE <layername> RENAME [COLUMN] "
                 "<columnname> TO <newname>'",
                 pszSQLCommand);
        return OGRERR_FAILURE;
    }

/* -------------------------------------------------------------------- */
/*      Find the named layer.                                           */
/* -------------------------------------------------------------------- */
    OGRLayer *poLayer = GetLayerByName(pszLayerName);
    if( poLayer == NULL )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "%s failed, no such layer as `%s'.", pszSQLCommand,
                 pszLayerName);
        CSLDestroy(papszTokens);
        return OGRERR_FAILURE;
    }

/* -------------------------------------------------------------------- */
/*      Find the field.                                                 */
/* -------------------------------------------------------------------- */

    const int nFieldIndex =
        poLayer->GetLayerDefn()->GetFieldIndex(pszOldColName);
    if( nFieldIndex < 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "%s failed, no such field as `%s'.", pszSQLCommand,
                 pszOldColName);
        CSLDestroy(papszTokens);
        return OGRERR_FAILURE;
    }

/* -------------------------------------------------------------------- */
/*      Rename column.                                                  */
/* -------------------------------------------------------------------- */
    OGRFieldDefn *poOldFieldDefn =
        poLayer->GetLayerDefn()->GetFieldDefn(nFieldIndex);
    OGRFieldDefn oNewFieldDefn(poOldFieldDefn);
    oNewFieldDefn.SetName(pszNewColName);

    CSLDestroy(papszTokens);

    return poLayer->AlterFieldDefn(nFieldIndex, &oNewFieldDefn,
                                   ALTER_NAME_FLAG);
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
    char **papszTokens = CSLTokenizeString(pszSQLCommand);

/* -------------------------------------------------------------------- */
/*      Do some general syntax checking.                                */
/* -------------------------------------------------------------------- */
    const char *pszLayerName = NULL;
    const char *pszColumnName = NULL;
    int iTypeIndex = 0;
    const int nTokens = CSLCount(papszTokens);

    if( nTokens >= 8
        && EQUAL(papszTokens[0], "ALTER")
        && EQUAL(papszTokens[1], "TABLE")
        && EQUAL(papszTokens[3], "ALTER")
        && EQUAL(papszTokens[4], "COLUMN")
        && EQUAL(papszTokens[6], "TYPE"))
    {
        pszLayerName = papszTokens[2];
        pszColumnName = papszTokens[5];
        iTypeIndex = 7;
    }
    else if( nTokens >= 7
             && EQUAL(papszTokens[0], "ALTER")
             && EQUAL(papszTokens[1], "TABLE")
             && EQUAL(papszTokens[3], "ALTER")
             && EQUAL(papszTokens[5], "TYPE"))
    {
        pszLayerName = papszTokens[2];
        pszColumnName = papszTokens[4];
        iTypeIndex = 6;
    }
    else
    {
        CSLDestroy(papszTokens);
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Syntax error in ALTER TABLE ALTER COLUMN command.\n"
                 "Was '%s'\n"
                 "Should be of form 'ALTER TABLE <layername> ALTER [COLUMN] "
                 "<columnname> TYPE <columntype>'",
                 pszSQLCommand);
        return OGRERR_FAILURE;
    }

/* -------------------------------------------------------------------- */
/*      Merge type components into a single string if there were split  */
/*      with spaces                                                     */
/* -------------------------------------------------------------------- */
    CPLString osType;
    for( int i = iTypeIndex; i < nTokens; ++i )
    {
        osType += papszTokens[i];
        CPLFree(papszTokens[i]);
    }
    char *pszType = papszTokens[iTypeIndex] = CPLStrdup(osType);
    papszTokens[iTypeIndex + 1] = NULL;

/* -------------------------------------------------------------------- */
/*      Find the named layer.                                           */
/* -------------------------------------------------------------------- */
    OGRLayer *poLayer = GetLayerByName(pszLayerName);
    if( poLayer == NULL )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "%s failed, no such layer as `%s'.", pszSQLCommand,
                 pszLayerName);
        CSLDestroy(papszTokens);
        return OGRERR_FAILURE;
    }

/* -------------------------------------------------------------------- */
/*      Find the field.                                                 */
/* -------------------------------------------------------------------- */

    const int nFieldIndex =
        poLayer->GetLayerDefn()->GetFieldIndex(pszColumnName);
    if( nFieldIndex < 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "%s failed, no such field as `%s'.", pszSQLCommand,
                 pszColumnName);
        CSLDestroy(papszTokens);
        return OGRERR_FAILURE;
    }

/* -------------------------------------------------------------------- */
/*      Alter column.                                                   */
/* -------------------------------------------------------------------- */

    OGRFieldDefn *poOldFieldDefn =
        poLayer->GetLayerDefn()->GetFieldDefn(nFieldIndex);
    OGRFieldDefn oNewFieldDefn(poOldFieldDefn);

    int nWidth = 0;
    int nPrecision = 0;
    OGRFieldType eType = GDALDatasetParseSQLType(pszType, nWidth, nPrecision);
    oNewFieldDefn.SetType(eType);
    oNewFieldDefn.SetWidth(nWidth);
    oNewFieldDefn.SetPrecision(nPrecision);

    int l_nFlags = 0;
    if (poOldFieldDefn->GetType() != oNewFieldDefn.GetType())
        l_nFlags |= ALTER_TYPE_FLAG;
    if (poOldFieldDefn->GetWidth() != oNewFieldDefn.GetWidth() ||
        poOldFieldDefn->GetPrecision() != oNewFieldDefn.GetPrecision())
        l_nFlags |= ALTER_WIDTH_PRECISION_FLAG;

    CSLDestroy(papszTokens);

    if (l_nFlags == 0)
        return OGRERR_NONE;

    return poLayer->AlterFieldDefn(nFieldIndex, &oNewFieldDefn, l_nFlags);
}
//! @endcond

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
 review the <a href="ogr_sql.html">OGR SQL</a> document.  Some drivers (i.e.
 Oracle and PostGIS) pass the SQL directly through to the underlying RDBMS.

 Starting with OGR 1.10, the <a href="ogr_sql_sqlite.html">SQLITE dialect</a>
 can also be used.

 In GDAL 1.X, this method used to be in the OGRDataSource class.

 @param pszStatement the SQL statement to execute.
 @param poSpatialFilter geometry which represents a spatial filter. Can be NULL.
 @param pszDialect allows control of the statement dialect. If set to NULL, the
 OGR SQL engine will be used, except for RDBMS drivers that will use their
 dedicated SQL engine, unless OGRSQL is explicitly passed as the
 dialect. Starting with OGR 1.10, the SQLITE dialect can also be used.

 @return an OGRLayer containing the results of the query.  Deallocate with
 ReleaseResultSet().

*/

OGRLayer *GDALDataset::ExecuteSQL( const char *pszStatement,
                                   OGRGeometry *poSpatialFilter,
                                   const char *pszDialect )

{
    return ExecuteSQL(pszStatement, poSpatialFilter, pszDialect, NULL);
}

//! @cond Doxygen_Suppress
OGRLayer *
GDALDataset::ExecuteSQL( const char *pszStatement,
                         OGRGeometry *poSpatialFilter,
                         const char *pszDialect,
                         swq_select_parse_options *poSelectParseOptions )

{
    if( pszDialect != NULL && EQUAL(pszDialect, "SQLite") )
    {
#ifdef SQLITE_ENABLED
        return OGRSQLiteExecuteSQL(this, pszStatement, poSpatialFilter,
                                   pszDialect);
#else
        CPLError(CE_Failure, CPLE_NotSupported,
                 "The SQLite driver needs to be compiled to support the "
                 "SQLite SQL dialect");
        return NULL;
#endif
    }

/* -------------------------------------------------------------------- */
/*      Handle CREATE INDEX statements specially.                       */
/* -------------------------------------------------------------------- */
    if( STARTS_WITH_CI(pszStatement, "CREATE INDEX") )
    {
        ProcessSQLCreateIndex(pszStatement);
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Handle DROP INDEX statements specially.                         */
/* -------------------------------------------------------------------- */
    if( STARTS_WITH_CI(pszStatement, "DROP INDEX") )
    {
        ProcessSQLDropIndex(pszStatement);
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Handle DROP TABLE statements specially.                         */
/* -------------------------------------------------------------------- */
    if( STARTS_WITH_CI(pszStatement, "DROP TABLE") )
    {
        ProcessSQLDropTable(pszStatement);
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Handle ALTER TABLE statements specially.                        */
/* -------------------------------------------------------------------- */
    if( STARTS_WITH_CI(pszStatement, "ALTER TABLE") )
    {
        char **papszTokens = CSLTokenizeString(pszStatement);
        if( CSLCount(papszTokens) >= 4 &&
            EQUAL(papszTokens[3], "ADD") )
        {
            ProcessSQLAlterTableAddColumn(pszStatement);
            CSLDestroy(papszTokens);
            return NULL;
        }
        else if( CSLCount(papszTokens) >= 4 &&
                 EQUAL(papszTokens[3], "DROP") )
        {
            ProcessSQLAlterTableDropColumn( pszStatement );
            CSLDestroy(papszTokens);
            return NULL;
        }
        else if( CSLCount(papszTokens) >= 4 &&
                 EQUAL(papszTokens[3], "RENAME") )
        {
            ProcessSQLAlterTableRenameColumn(pszStatement);
            CSLDestroy(papszTokens);
            return NULL;
        }
        else if( CSLCount(papszTokens) >= 4 &&
                 EQUAL(papszTokens[3], "ALTER") )
        {
            ProcessSQLAlterTableAlterColumn(pszStatement);
            CSLDestroy(papszTokens);
            return NULL;
        }
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Unsupported ALTER TABLE command : %s", pszStatement);
            CSLDestroy(papszTokens);
            return NULL;
        }
    }

/* -------------------------------------------------------------------- */
/*      Preparse the SQL statement.                                     */
/* -------------------------------------------------------------------- */
    swq_select *psSelectInfo = new swq_select();
    swq_custom_func_registrar *poCustomFuncRegistrar = NULL;
    if( poSelectParseOptions != NULL )
        poCustomFuncRegistrar = poSelectParseOptions->poCustomFuncRegistrar;
    if( psSelectInfo->preparse( pszStatement,
                                poCustomFuncRegistrar != NULL ) != CE_None )
    {
        delete psSelectInfo;
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      If there is no UNION ALL, build result layer.                   */
/* -------------------------------------------------------------------- */
    if( psSelectInfo->poOtherSelect == NULL )
    {
        return BuildLayerFromSelectInfo(psSelectInfo, poSpatialFilter,
                                        pszDialect, poSelectParseOptions);
    }

/* -------------------------------------------------------------------- */
/*      Build result union layer.                                       */
/* -------------------------------------------------------------------- */
    int nSrcLayers = 0;
    OGRLayer **papoSrcLayers = NULL;

    do
    {
        swq_select *psNextSelectInfo = psSelectInfo->poOtherSelect;
        psSelectInfo->poOtherSelect = NULL;

        OGRLayer *poLayer = BuildLayerFromSelectInfo(
            psSelectInfo, poSpatialFilter, pszDialect, poSelectParseOptions);
        if( poLayer == NULL )
        {
            // Each source layer owns an independent select info.
            for( int i = 0; i < nSrcLayers; ++i )
                delete papoSrcLayers[i];
            CPLFree(papoSrcLayers);

            // So we just have to destroy the remaining select info.
            delete psNextSelectInfo;

            return NULL;
        }
        else
        {
            papoSrcLayers = static_cast<OGRLayer **>(CPLRealloc(
                papoSrcLayers, sizeof(OGRLayer *) * (nSrcLayers + 1)));
            papoSrcLayers[nSrcLayers] = poLayer;
            ++nSrcLayers;

            psSelectInfo = psNextSelectInfo;
        }
    }
    while( psSelectInfo != NULL );

    return new OGRUnionLayer("SELECT", nSrcLayers, papoSrcLayers, TRUE);
}
//! @endcond

/************************************************************************/
/*                        BuildLayerFromSelectInfo()                    */
/************************************************************************/

struct GDALSQLParseInfo
{
    swq_field_list sFieldList;
    int nExtraDSCount;
    GDALDataset **papoExtraDS;
    char *pszWHERE;
};

OGRLayer *GDALDataset::BuildLayerFromSelectInfo(
    swq_select *psSelectInfo, OGRGeometry *poSpatialFilter,
    const char *pszDialect, swq_select_parse_options *poSelectParseOptions)
{
    OGRGenSQLResultsLayer *poResults = NULL;
    GDALSQLParseInfo *psParseInfo =
        BuildParseInfo(psSelectInfo, poSelectParseOptions);

    if( psParseInfo )
    {
        poResults =
            new OGRGenSQLResultsLayer(this, psSelectInfo, poSpatialFilter,
                                      psParseInfo->pszWHERE, pszDialect);
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

//! @cond Doxygen_Suppress
void GDALDataset::DestroyParseInfo(GDALSQLParseInfo *psParseInfo)
{
    if( psParseInfo == NULL )
        return;

    CPLFree(psParseInfo->sFieldList.names);
    CPLFree(psParseInfo->sFieldList.types);
    CPLFree(psParseInfo->sFieldList.table_ids);
    CPLFree(psParseInfo->sFieldList.ids);

    // Release the datasets we have opened with OGROpenShared()
    // It is safe to do that as the 'new OGRGenSQLResultsLayer' itself
    // has taken a reference on them, which it will release in its
    // destructor.
    for( int iEDS = 0; iEDS < psParseInfo->nExtraDSCount; ++iEDS )
        GDALClose(psParseInfo->papoExtraDS[iEDS]);

    CPLFree(psParseInfo->papoExtraDS);
    CPLFree(psParseInfo->pszWHERE);
    CPLFree(psParseInfo);
}

/************************************************************************/
/*                            BuildParseInfo()                          */
/************************************************************************/

GDALSQLParseInfo *
GDALDataset::BuildParseInfo(swq_select *psSelectInfo,
                            swq_select_parse_options *poSelectParseOptions)
{
    int nFIDIndex = 0;

    GDALSQLParseInfo *psParseInfo =
        static_cast<GDALSQLParseInfo *>(CPLCalloc(1, sizeof(GDALSQLParseInfo)));

/* -------------------------------------------------------------------- */
/*      Validate that all the source tables are recognized, count       */
/*      fields.                                                         */
/* -------------------------------------------------------------------- */
    int nFieldCount = 0;

    for( int iTable = 0; iTable < psSelectInfo->table_count; iTable++ )
    {
        swq_table_def *psTableDef = psSelectInfo->table_defs + iTable;
        GDALDataset *poTableDS = this;

        if( psTableDef->data_source != NULL )
        {
            poTableDS = reinterpret_cast<GDALDataset *>(
                OGROpenShared(psTableDef->data_source, FALSE, NULL));
            if( poTableDS == NULL )
            {
                if( strlen(CPLGetLastErrorMsg()) == 0 )
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Unable to open secondary datasource "
                             "`%s' required by JOIN.",
                             psTableDef->data_source);

                DestroyParseInfo(psParseInfo);
                return NULL;
            }

            // Keep in an array to release at the end of this function.
            psParseInfo->papoExtraDS = static_cast<GDALDataset **>(CPLRealloc(
                psParseInfo->papoExtraDS,
                sizeof(GDALDataset *) * (psParseInfo->nExtraDSCount + 1)));
            psParseInfo->papoExtraDS[psParseInfo->nExtraDSCount++] = poTableDS;
        }

        OGRLayer *poSrcLayer =
            poTableDS->GetLayerByName(psTableDef->table_name);

        if( poSrcLayer == NULL )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "SELECT from table %s failed, no such table/featureclass.",
                     psTableDef->table_name);

            DestroyParseInfo(psParseInfo);
            return NULL;
        }

        nFieldCount += poSrcLayer->GetLayerDefn()->GetFieldCount();
        if( iTable == 0 ||
            (poSelectParseOptions &&
            poSelectParseOptions->bAddSecondaryTablesGeometryFields) )
            nFieldCount += poSrcLayer->GetLayerDefn()->GetGeomFieldCount();
    }

/* -------------------------------------------------------------------- */
/*      Build the field list for all indicated tables.                  */
/* -------------------------------------------------------------------- */

    psParseInfo->sFieldList.table_count = psSelectInfo->table_count;
    psParseInfo->sFieldList.table_defs = psSelectInfo->table_defs;

    psParseInfo->sFieldList.count = 0;
    psParseInfo->sFieldList.names = static_cast<char **>(
        CPLMalloc(sizeof(char *) * (nFieldCount + SPECIAL_FIELD_COUNT)));
    psParseInfo->sFieldList.types = static_cast<swq_field_type *>(CPLMalloc(
        sizeof(swq_field_type) * (nFieldCount + SPECIAL_FIELD_COUNT)));
    psParseInfo->sFieldList.table_ids = static_cast<int *>(
        CPLMalloc(sizeof(int) * (nFieldCount + SPECIAL_FIELD_COUNT)));
    psParseInfo->sFieldList.ids = static_cast<int *>(
        CPLMalloc(sizeof(int) * (nFieldCount + SPECIAL_FIELD_COUNT)));

    bool bIsFID64 = false;
    for( int iTable = 0; iTable < psSelectInfo->table_count; iTable++ )
    {
        swq_table_def *psTableDef = psSelectInfo->table_defs + iTable;
        GDALDataset *poTableDS = this;

        if( psTableDef->data_source != NULL )
        {
            poTableDS = reinterpret_cast<GDALDataset *>(
                OGROpenShared(psTableDef->data_source, FALSE, NULL));
            CPLAssert(poTableDS != NULL);
            poTableDS->Dereference();
        }

        OGRLayer *poSrcLayer =
            poTableDS->GetLayerByName(psTableDef->table_name);

        for( int iField = 0;
             iField < poSrcLayer->GetLayerDefn()->GetFieldCount();
             iField++ )
        {
            OGRFieldDefn *poFDefn =
                poSrcLayer->GetLayerDefn()->GetFieldDefn(iField);
            const int iOutField = psParseInfo->sFieldList.count++;
            psParseInfo->sFieldList.names[iOutField] =
                const_cast<char *>(poFDefn->GetNameRef());
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

        if( iTable == 0 ||
            (poSelectParseOptions &&
             poSelectParseOptions->bAddSecondaryTablesGeometryFields) )
        {
            nFIDIndex = psParseInfo->sFieldList.count;

            for( int iField = 0;
                 iField < poSrcLayer->GetLayerDefn()->GetGeomFieldCount();
                 iField++ )
            {
                OGRGeomFieldDefn *poFDefn =
                    poSrcLayer->GetLayerDefn()->GetGeomFieldDefn(iField);
                const int iOutField = psParseInfo->sFieldList.count++;
                psParseInfo->sFieldList.names[iOutField] =
                    const_cast<char *>(poFDefn->GetNameRef());
                if( *psParseInfo->sFieldList.names[iOutField] == '\0' )
                    psParseInfo->sFieldList.names[iOutField] =
                        const_cast<char *>(OGR_GEOMETRY_DEFAULT_NON_EMPTY_NAME);
                psParseInfo->sFieldList.types[iOutField] = SWQ_GEOMETRY;

                psParseInfo->sFieldList.table_ids[iOutField] = iTable;
                psParseInfo->sFieldList.ids[iOutField] =
                    GEOM_FIELD_INDEX_TO_ALL_FIELD_INDEX(
                        poSrcLayer->GetLayerDefn(), iField);
            }
        }

        if( iTable == 0 && poSrcLayer->GetMetadataItem(OLMD_FID64) != NULL &&
            EQUAL(poSrcLayer->GetMetadataItem(OLMD_FID64), "YES") )
        {
            bIsFID64 = true;
        }
    }

/* -------------------------------------------------------------------- */
/*      Expand '*' in 'SELECT *' now before we add the pseudo fields    */
/* -------------------------------------------------------------------- */
    const bool bAlwaysPrefixWithTableName =
        poSelectParseOptions &&
        poSelectParseOptions->bAlwaysPrefixWithTableName;
    if( psSelectInfo->expand_wildcard(&psParseInfo->sFieldList,
                                      bAlwaysPrefixWithTableName) != CE_None )
    {
        DestroyParseInfo(psParseInfo);
        return NULL;
    }

    for (int iField = 0; iField < SPECIAL_FIELD_COUNT; iField++)
    {
        psParseInfo->sFieldList.names[psParseInfo->sFieldList.count] =
            const_cast<char *>(SpecialFieldNames[iField]);
        psParseInfo->sFieldList.types[psParseInfo->sFieldList.count] =
            (iField == SPF_FID && bIsFID64) ? SWQ_INTEGER64
                                            : SpecialFieldTypes[iField];
        psParseInfo->sFieldList.table_ids[psParseInfo->sFieldList.count] = 0;
        psParseInfo->sFieldList.ids[psParseInfo->sFieldList.count] =
            nFIDIndex + iField;
        psParseInfo->sFieldList.count++;
    }

/* -------------------------------------------------------------------- */
/*      Finish the parse operation.                                     */
/* -------------------------------------------------------------------- */
    if( psSelectInfo->parse(&psParseInfo->sFieldList, poSelectParseOptions) !=
        CE_None )
    {
        DestroyParseInfo(psParseInfo);
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Extract the WHERE expression to use separately.                 */
/* -------------------------------------------------------------------- */
    if( psSelectInfo->where_expr != NULL )
    {
        psParseInfo->pszWHERE =
            psSelectInfo->where_expr->Unparse(&psParseInfo->sFieldList, '"');
        // CPLDebug( "OGR", "Unparse() -> %s", pszWHERE );
    }

    return psParseInfo;
}
//! @endcond

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

void GDALDataset::ReleaseResultSet( OGRLayer *poResultsSet )

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

OGRStyleTable *GDALDataset::GetStyleTable() { return m_poStyleTable; }

/************************************************************************/
/*                         SetStyleTableDirectly()                      */
/************************************************************************/

/**
 \brief Set dataset style table.

 This method operate exactly as SetStyleTable() except that it
 assumes ownership of the passed table.

 This method is the same as the C function GDALDatasetSetStyleTableDirectly()
 and the deprecated OGR_DS_SetStyleTableDirectly().

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

//! @cond Doxygen_Suppress
int GDALDataset::IsGenericSQLDialect(const char *pszDialect)
{
    return pszDialect != NULL &&
           (EQUAL(pszDialect, "OGRSQL") || EQUAL(pszDialect, "SQLITE"));
}
//! @endcond

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

int GDALDataset::GetLayerCount() { return 0; }

/************************************************************************/
/*                                GetLayer()                            */
/************************************************************************/

/**
 \fn GDALDataset::GetLayer(int)
 \brief Fetch a layer by index.

 The returned layer remains owned by the
 GDALDataset and should not be deleted by the application.

 This method is the same as the C function GDALDatasetGetLayer() and the
 deprecated OGR_DS_GetLayer().

 In GDAL 1.X, this method used to be in the OGRDataSource class.

 @param iLayer a layer number between 0 and GetLayerCount()-1.

 @return the layer, or NULL if iLayer is out of range or an error occurs.
*/

OGRLayer *GDALDataset::GetLayer(CPL_UNUSED int iLayer) { return NULL; }

/************************************************************************/
/*                           ResetReading()                             */
/************************************************************************/

/**
 \brief Reset feature reading to start on the first feature.

 This affects GetNextFeature().

 Depending on drivers, this may also have the side effect of calling
 OGRLayer::ResetReading() on the layers of this dataset.

 This method is the same as the C function GDALDatasetResetReading().

 @since GDAL 2.2
*/
void GDALDataset::ResetReading()
{
    GDALDatasetPrivate *psPrivate =
        static_cast<GDALDatasetPrivate *>(m_hPrivateData);
    if( !psPrivate )
        return;
    psPrivate->nCurrentLayerIdx = 0;
    psPrivate->nLayerCount = -1;
    psPrivate->poCurrentLayer = NULL;
    psPrivate->nFeatureReadInLayer = 0;
    psPrivate->nFeatureReadInDataset = 0;
    psPrivate->nTotalFeaturesInLayer = TOTAL_FEATURES_NOT_INIT;
    psPrivate->nTotalFeatures = TOTAL_FEATURES_NOT_INIT;
}

/************************************************************************/
/*                         GDALDatasetResetReading()                    */
/************************************************************************/

/**
 \brief Reset feature reading to start on the first feature.

 This affects GDALDatasetGetNextFeature().

 Depending on drivers, this may also have the side effect of calling
 OGR_L_ResetReading() on the layers of this dataset.

 This method is the same as the C++ method GDALDataset::ResetReading()

 @param hDS dataset handle
 @since GDAL 2.2
*/
void CPL_DLL GDALDatasetResetReading( GDALDatasetH hDS )
{
    VALIDATE_POINTER0(hDS, "GDALDatasetResetReading");

    return static_cast<GDALDataset *>(hDS)->ResetReading();
}

/************************************************************************/
/*                          GetNextFeature()                            */
/************************************************************************/

/**
 \brief Fetch the next available feature from this dataset.

 This method is intended for the few drivers where OGRLayer::GetNextFeature()
 is not efficient, but in general OGRLayer::GetNextFeature() is a more
 natural API.

 The returned feature becomes the responsibility of the caller to
 delete with OGRFeature::DestroyFeature().

 Depending on the driver, this method may return features from layers in a
 non sequential way. This is what may happen when the
 ODsCRandomLayerRead capability is declared (for example for the
 OSM and GMLAS drivers). When datasets declare this capability, it is strongly
 advised to use GDALDataset::GetNextFeature() instead of
 OGRLayer::GetNextFeature(), as the later might have a slow, incomplete or stub
 implementation.

 The default implementation, used by most drivers, will
 however iterate over each layer, and then over each feature within this
 layer.

 This method takes into account spatial and attribute filters set on layers that
 will be iterated upon.

 The ResetReading() method can be used to start at the beginning again.

 Depending on drivers, this may also have the side effect of calling
 OGRLayer::GetNextFeature() on the layers of this dataset.

 This method is the same as the C function GDALDatasetGetNextFeature().

 @param ppoBelongingLayer a pointer to a OGRLayer* variable to receive the
                          layer to which the object belongs to, or NULL.
                          It is possible that the output of *ppoBelongingLayer
                          to be NULL despite the feature not being NULL.
 @param pdfProgressPct    a pointer to a double variable to receive the
                          percentage progress (in [0,1] range), or NULL.
                          On return, the pointed value might be negative if
                          determining the progress is not possible.
 @param pfnProgress       a progress callback to report progress (for
                          GetNextFeature() calls that might have a long
                          duration) and offer cancellation possibility, or NULL.
 @param pProgressData     user data provided to pfnProgress, or NULL
 @return a feature, or NULL if no more features are available.
 @since GDAL 2.2
*/

OGRFeature *GDALDataset::GetNextFeature( OGRLayer **ppoBelongingLayer,
                                         double *pdfProgressPct,
                                         GDALProgressFunc pfnProgress,
                                         void *pProgressData )
{
    GDALDatasetPrivate *psPrivate =
        static_cast<GDALDatasetPrivate *>(m_hPrivateData);
    if( !psPrivate || psPrivate->nCurrentLayerIdx < 0 )
    {
        if( ppoBelongingLayer != NULL )
            *ppoBelongingLayer = NULL;
        if( pdfProgressPct != NULL )
            *pdfProgressPct = 1.0;
        if( pfnProgress != NULL )
            pfnProgress(1.0, "", pProgressData);
        return NULL;
    }

    if ( psPrivate->poCurrentLayer == NULL &&
         (pdfProgressPct != NULL || pfnProgress != NULL) )
    {
        if( psPrivate->nLayerCount < 0 )
        {
            psPrivate->nLayerCount = GetLayerCount();
        }

        if( psPrivate->nTotalFeatures == TOTAL_FEATURES_NOT_INIT )
        {
            psPrivate->nTotalFeatures = 0;
            for(int i = 0; i < psPrivate->nLayerCount; i++)
            {
                OGRLayer *poLayer = GetLayer(i);
                if( poLayer == NULL ||
                    !poLayer->TestCapability(OLCFastFeatureCount) )
                {
                    psPrivate->nTotalFeatures = TOTAL_FEATURES_UNKNOWN;
                    break;
                }
                GIntBig nCount = poLayer->GetFeatureCount(FALSE);
                if( nCount < 0 )
                {
                    psPrivate->nTotalFeatures = TOTAL_FEATURES_UNKNOWN;
                    break;
                }
                psPrivate->nTotalFeatures += nCount;
            }
        }
    }

    while( true )
    {
        if( psPrivate->poCurrentLayer == NULL )
        {
            psPrivate->poCurrentLayer = GetLayer(psPrivate->nCurrentLayerIdx);
            if( psPrivate->poCurrentLayer == NULL )
            {
                psPrivate->nCurrentLayerIdx = -1;
                if( ppoBelongingLayer != NULL )
                    *ppoBelongingLayer = NULL;
                if( pdfProgressPct != NULL )
                    *pdfProgressPct = 1.0;
                return NULL;
            }
            psPrivate->poCurrentLayer->ResetReading();
            psPrivate->nFeatureReadInLayer = 0;
            if( psPrivate->nTotalFeatures < 0 && pdfProgressPct != NULL )
            {
                if( psPrivate->poCurrentLayer->TestCapability(
                        OLCFastFeatureCount) )
                    psPrivate->nTotalFeaturesInLayer =
                        psPrivate->poCurrentLayer->GetFeatureCount(FALSE);
                else
                    psPrivate->nTotalFeaturesInLayer = 0;
            }
        }
        OGRFeature *poFeature = psPrivate->poCurrentLayer->GetNextFeature();
        if( poFeature == NULL )
        {
            psPrivate->nCurrentLayerIdx++;
            psPrivate->poCurrentLayer = NULL;
            continue;
        }

        psPrivate->nFeatureReadInLayer++;
        psPrivate->nFeatureReadInDataset++;
        if( pdfProgressPct != NULL || pfnProgress != NULL )
        {
            double dfPct = 0.0;
            if( psPrivate->nTotalFeatures > 0 )
            {
                dfPct = 1.0 * psPrivate->nFeatureReadInDataset /
                        psPrivate->nTotalFeatures;
            }
            else
            {
                dfPct =
                    1.0 * psPrivate->nCurrentLayerIdx / psPrivate->nLayerCount;
                if( psPrivate->nTotalFeaturesInLayer > 0 )
                {
                    dfPct += 1.0 * psPrivate->nFeatureReadInLayer /
                             psPrivate->nTotalFeaturesInLayer /
                             psPrivate->nLayerCount;
                }
            }
            if( pdfProgressPct )
                *pdfProgressPct = dfPct;
            if( pfnProgress )
                pfnProgress(dfPct, "", NULL);
        }

        if( ppoBelongingLayer != NULL )
            *ppoBelongingLayer = psPrivate->poCurrentLayer;
        return poFeature;
    }
}

/************************************************************************/
/*                     GDALDatasetGetNextFeature()                      */
/************************************************************************/
/**
 \brief Fetch the next available feature from this dataset.

 This method is intended for the few drivers where OGR_L_GetNextFeature()
 is not efficient, but in general OGR_L_GetNextFeature() is a more
 natural API.

 The returned feature becomes the responsibility of the caller to
 delete with OGRFeature::DestroyFeature().

 Depending on the driver, this method may return features from layers in a
 non sequential way. This is what may happen when the
 ODsCRandomLayerRead capability is declared (for example for the
 OSM and GMLAS drivers). When datasets declare this capability, it is strongly
 advised to use GDALDataset::GetNextFeature() instead of
 OGRLayer::GetNextFeature(), as the later might have a slow, incomplete or stub
 implementation.

 The default implementation, used by most drivers, will
 however iterate over each layer, and then over each feature within this
 layer.

 This method takes into account spatial and attribute filters set on layers that
 will be iterated upon.

 The ResetReading() method can be used to start at the beginning again.

 Depending on drivers, this may also have the side effect of calling
 OGRLayer::GetNextFeature() on the layers of this dataset.

 This method is the same as the C++ method GDALDataset::GetNextFeature()

 @param hDS               dataset handle.
 @param phBelongingLayer  a pointer to a OGRLayer* variable to receive the
                          layer to which the object belongs to, or NULL.
                          It is possible that the output of *ppoBelongingLayer
                          to be NULL despite the feature not being NULL.
 @param pdfProgressPct    a pointer to a double variable to receive the
                          percentage progress (in [0,1] range), or NULL.
                          On return, the pointed value might be negative if
                          determining the progress is not possible.
 @param pfnProgress       a progress callback to report progress (for
                          GetNextFeature() calls that might have a long
                          duration) and offer cancellation possibility, or NULL
 @param pProgressData     user data provided to pfnProgress, or NULL
 @return a feature, or NULL if no more features are available.
 @since GDAL 2.2
*/
OGRFeatureH CPL_DLL GDALDatasetGetNextFeature( GDALDatasetH hDS,
                                               OGRLayerH *phBelongingLayer,
                                               double *pdfProgressPct,
                                               GDALProgressFunc pfnProgress,
                                               void *pProgressData )
{
    VALIDATE_POINTER1(hDS, "GDALDatasetGetNextFeature", NULL);

    return reinterpret_cast<OGRFeatureH>(
        reinterpret_cast<GDALDataset *>(hDS)
            ->GetNextFeature((OGRLayer **)phBelongingLayer, pdfProgressPct,
                             pfnProgress, pProgressData));
}

/************************************************************************/
/*                            TestCapability()                          */
/************************************************************************/

/**
 \fn GDALDataset::TestCapability( const char * pszCap )
 \brief Test if capability is available.

 One of the following dataset capability names can be passed into this
 method, and a TRUE or FALSE value will be returned indicating whether or not
 the capability is available for this object.

 <ul>
  <li> <b>ODsCCreateLayer</b>: True if this datasource can create new layers.<p>
  <li> <b>ODsCDeleteLayer</b>: True if this datasource can delete existing
          layers.<p>
  <li> <b>ODsCCreateGeomFieldAfterCreateLayer</b>: True if the layers of this
          datasource support CreateGeomField() just after layer creation.<p>
  <li> <b>ODsCCurveGeometries</b>: True if this datasource supports curve
          geometries.<p>
  <li> <b>ODsCTransactions</b>: True if this datasource supports (efficient)
          transactions.<p>
  <li> <b>ODsCEmulatedTransactions</b>: True if this datasource supports
          transactions through emulation.<p>
  <li> <b>ODsCRandomLayerRead</b>: True if this datasource has a dedicated
          GetNextFeature() implementation, potentially returning features from
          layers in a non sequential way.<p>
  <li> <b>ODsCRandomLayerWrite</b>: True if this datasource supports calling
         CreateFeature() on layers in a non sequential way.<p>
 </ul>

 The \#define macro forms of the capability names should be used in preference
 to the strings themselves to avoid misspelling.

 This method is the same as the C function GDALDatasetTestCapability() and the
 deprecated OGR_DS_TestCapability().

 In GDAL 1.X, this method used to be in the OGRDataSource class.

 @param pszCap the capability to test.

 @return TRUE if capability available otherwise FALSE.
*/

int GDALDataset::TestCapability(CPL_UNUSED const char *pszCap) { return FALSE; }

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
  <li> <b>ODsCDeleteLayer</b>: True if this datasource can delete existing
          layers.<p>
  <li> <b>ODsCCreateGeomFieldAfterCreateLayer</b>: True if the layers of this
          datasource support CreateGeomField() just after layer creation.<p>
  <li> <b>ODsCCurveGeometries</b>: True if this datasource supports curve
          geometries.<p>
  <li> <b>ODsCTransactions</b>: True if this datasource supports (efficient)
          transactions.<p>
  <li> <b>ODsCEmulatedTransactions</b>: True if this datasource supports
          transactions through emulation.<p>
  <li> <b>ODsCRandomLayerRead</b>: True if this datasource has a dedicated
          GetNextFeature() implementation, potentially returning features from
          layers in a non sequential way.<p>
  <li> <b>ODsCRandomLayerWrite</b>: True if this datasource supports calling
          CreateFeature() on layers in a non sequential way.<p>
 </ul>

 The \#define macro forms of the capability names should be used in preference
 to the strings themselves to avoid misspelling.

 This function is the same as the C++ method GDALDataset::TestCapability()

 @since GDAL 2.0

 @param hDS the dataset handle.
 @param pszCap the capability to test.

 @return TRUE if capability available otherwise FALSE.
*/
int GDALDatasetTestCapability( GDALDatasetH hDS, const char *pszCap )

{
    VALIDATE_POINTER1(hDS, "GDALDatasetTestCapability", 0);
    VALIDATE_POINTER1(pszCap, "GDALDatasetTestCapability", 0);

    return static_cast<GDALDataset *>(hDS)->TestCapability(pszCap);
}

/************************************************************************/
/*                           StartTransaction()                         */
/************************************************************************/

/**
 \fn GDALDataset::StartTransaction(int)
 \brief For datasources which support transactions, StartTransaction creates a
`transaction.

 If starting the transaction fails, will return
 OGRERR_FAILURE. Datasources which do not support transactions will
 always return OGRERR_UNSUPPORTED_OPERATION.

 Nested transactions are not supported.

 All changes done after the start of the transaction are definitely applied in
 the datasource if CommitTransaction() is called. They may be canceled by
 calling RollbackTransaction() instead.

 At the time of writing, transactions only apply on vector layers.

 Datasets that support transactions will advertise the ODsCTransactions
 capability.  Use of transactions at dataset level is generally preferred to
 transactions at layer level, whose scope is rarely limited to the layer from
 which it was started.

 In case StartTransaction() fails, neither CommitTransaction() or
 RollbackTransaction() should be called.

 If an error occurs after a successful StartTransaction(), the whole transaction
 may or may not be implicitly canceled, depending on drivers. (e.g.  the PG
 driver will cancel it, SQLite/GPKG not). In any case, in the event of an error,
 an explicit call to RollbackTransaction() should be done to keep things
 balanced.

 By default, when bForce is set to FALSE, only "efficient" transactions will be
 attempted. Some drivers may offer an emulation of transactions, but sometimes
 with significant overhead, in which case the user must explicitly allow for
 such an emulation by setting bForce to TRUE. Drivers that offer emulated
 transactions should advertise the ODsCEmulatedTransactions capability (and not
 ODsCTransactions).

 This function is the same as the C function GDALDatasetStartTransaction().

 @param bForce can be set to TRUE if an emulation, possibly slow, of a
 transaction
               mechanism is acceptable.

 @return OGRERR_NONE on success.
 @since GDAL 2.0
*/

OGRErr GDALDataset::StartTransaction( CPL_UNUSED int bForce )
{
    return OGRERR_UNSUPPORTED_OPERATION;
}

/************************************************************************/
/*                      GDALDatasetStartTransaction()                   */
/************************************************************************/

/**
 \brief For datasources which support transactions, StartTransaction creates a
 transaction.

 If starting the transaction fails, will return
 OGRERR_FAILURE. Datasources which do not support transactions will
 always return OGRERR_UNSUPPORTED_OPERATION.

 Nested transactions are not supported.

 All changes done after the start of the transaction are definitely applied in
 the datasource if CommitTransaction() is called. They may be canceled by
 calling RollbackTransaction() instead.

 At the time of writing, transactions only apply on vector layers.

 Datasets that support transactions will advertise the ODsCTransactions
 capability.
 Use of transactions at dataset level is generally preferred to transactions at
 layer level, whose scope is rarely limited to the layer from which it was
 started.

 In case StartTransaction() fails, neither CommitTransaction() or
 RollbackTransaction() should be called.

 If an error occurs after a successful StartTransaction(), the whole
 transaction may or may not be implicitly canceled, depending on drivers. (e.g.
 the PG driver will cancel it, SQLite/GPKG not). In any case, in the event of an
 error, an explicit call to RollbackTransaction() should be done to keep things
 balanced.

 By default, when bForce is set to FALSE, only "efficient" transactions will be
 attempted. Some drivers may offer an emulation of transactions, but sometimes
 with significant overhead, in which case the user must explicitly allow for
 such an emulation by setting bForce to TRUE. Drivers that offer emulated
 transactions should advertise the ODsCEmulatedTransactions capability (and not
 ODsCTransactions).

 This function is the same as the C++ method GDALDataset::StartTransaction()

 @param hDS the dataset handle.
 @param bForce can be set to TRUE if an emulation, possibly slow, of a
 transaction
               mechanism is acceptable.

 @return OGRERR_NONE on success.
 @since GDAL 2.0
*/
OGRErr GDALDatasetStartTransaction(GDALDatasetH hDS, int bForce)
{
    VALIDATE_POINTER1(hDS, "GDALDatasetStartTransaction",
                      OGRERR_INVALID_HANDLE);

#ifdef OGRAPISPY_ENABLED
    if( bOGRAPISpyEnabled )
        OGRAPISpy_Dataset_StartTransaction(hDS, bForce);
#endif

    return static_cast<GDALDataset *>(hDS)->StartTransaction(bForce);
}

/************************************************************************/
/*                           CommitTransaction()                        */
/************************************************************************/

/**
 \brief For datasources which support transactions, CommitTransaction commits a
 transaction.

 If no transaction is active, or the commit fails, will return
 OGRERR_FAILURE. Datasources which do not support transactions will
 always return OGRERR_UNSUPPORTED_OPERATION.

 Depending on drivers, this may or may not abort layer sequential readings that
 are active.

 This function is the same as the C function GDALDatasetCommitTransaction().

 @return OGRERR_NONE on success.
 @since GDAL 2.0
*/
OGRErr GDALDataset::CommitTransaction() { return OGRERR_UNSUPPORTED_OPERATION; }

/************************************************************************/
/*                        GDALDatasetCommitTransaction()                */
/************************************************************************/

/**
 \brief For datasources which support transactions, CommitTransaction commits a
 transaction.

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
    VALIDATE_POINTER1(hDS, "GDALDatasetCommitTransaction",
                      OGRERR_INVALID_HANDLE);

#ifdef OGRAPISPY_ENABLED
    if( bOGRAPISpyEnabled )
        OGRAPISpy_Dataset_CommitTransaction(hDS);
#endif

    return static_cast<GDALDataset *>(hDS)->CommitTransaction();
}

/************************************************************************/
/*                           RollbackTransaction()                      */
/************************************************************************/

/**
 \brief For datasources which support transactions, RollbackTransaction will
 roll back a datasource to its state before the start of the current
 transaction.
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
 \brief For datasources which support transactions, RollbackTransaction will
 roll back a datasource to its state before the start of the current
 transaction.
 If no transaction is active, or the rollback fails, will return
 OGRERR_FAILURE. Datasources which do not support transactions will
 always return OGRERR_UNSUPPORTED_OPERATION.

 This function is the same as the C++ method GDALDataset::RollbackTransaction().

 @return OGRERR_NONE on success.
 @since GDAL 2.0
*/
OGRErr GDALDatasetRollbackTransaction( GDALDatasetH hDS )
{
    VALIDATE_POINTER1(hDS, "GDALDatasetRollbackTransaction",
                      OGRERR_INVALID_HANDLE);

#ifdef OGRAPISPY_ENABLED
    if( bOGRAPISpyEnabled )
        OGRAPISpy_Dataset_RollbackTransaction(hDS);
#endif

    return static_cast<GDALDataset *>(hDS)->RollbackTransaction();
}

/************************************************************************/
/*                          EnterReadWrite()                            */
/************************************************************************/

//! @cond Doxygen_Suppress
int GDALDataset::EnterReadWrite(GDALRWFlag eRWFlag)
{
    GDALDatasetPrivate *psPrivate =
        static_cast<GDALDatasetPrivate *>(m_hPrivateData);
    if( psPrivate != NULL && eAccess == GA_Update )
    {
        if( psPrivate->eStateReadWriteMutex == RW_MUTEX_STATE_UNKNOWN )
        {
            // In case dead-lock would occur, which is not impossible,
            // this can be used to prevent it, but at the risk of other
            // issues.
            if(CPLTestBool(
                   CPLGetConfigOption("GDAL_ENABLE_READ_WRITE_MUTEX", "YES")))
            {
                psPrivate->eStateReadWriteMutex = RW_MUTEX_STATE_ALLOWED;
            }
            else
            {
                psPrivate->eStateReadWriteMutex = RW_MUTEX_STATE_DISABLED;
            }
        }
        if( psPrivate->eStateReadWriteMutex == RW_MUTEX_STATE_ALLOWED &&
            (eRWFlag == GF_Write || psPrivate->hMutex != NULL) )
        {
            // There should be no race related to creating this mutex since
            // it should be first created through IWriteBlock() / IRasterIO()
            // and then GDALRasterBlock might call it from another thread.
#ifdef DEBUG_VERBOSE
            CPLDebug("GDAL",
                     "[Thread " CPL_FRMT_GIB "] Acquiring RW mutex for %s",
                     CPLGetPID(), GetDescription());
#endif
            CPLCreateOrAcquireMutex(&(psPrivate->hMutex), 1000.0);
            // Not sure if we can have recursive calls, so...
            psPrivate->oMapThreadToMutexTakenCount[CPLGetPID()]++;
            return TRUE;
        }
    }
    return FALSE;
}

/************************************************************************/
/*                         LeaveReadWrite()                             */
/************************************************************************/

void GDALDataset::LeaveReadWrite()
{
    GDALDatasetPrivate *psPrivate =
        static_cast<GDALDatasetPrivate *>(m_hPrivateData);
    if( psPrivate )
    {
        psPrivate->oMapThreadToMutexTakenCount[CPLGetPID()]--;
        CPLReleaseMutex(psPrivate->hMutex);
#ifdef DEBUG_VERBOSE
        CPLDebug("GDAL", "[Thread " CPL_FRMT_GIB "] Releasing RW mutex for %s",
                 CPLGetPID(), GetDescription());
#endif
    }
}

/************************************************************************/
/*                           InitRWLock()                               */
/************************************************************************/

void GDALDataset::InitRWLock()
{
    GDALDatasetPrivate *psPrivate =
        static_cast<GDALDatasetPrivate *>(m_hPrivateData);
    if( psPrivate )
    {
        if( psPrivate->eStateReadWriteMutex == RW_MUTEX_STATE_UNKNOWN )
        {
            if( EnterReadWrite(GF_Write) )
                LeaveReadWrite();
        }
    }
}

/************************************************************************/
/*                       DisableReadWriteMutex()                        */
/************************************************************************/

// The mutex logic is broken in multi-threaded situations, for example
// with 2 WarpedVRT datasets being read at the same time. In that
// particular case, the mutex is not needed, so allow the VRTWarpedDataset code
// to disable it.
void GDALDataset::DisableReadWriteMutex()
{
    GDALDatasetPrivate *psPrivate =
        static_cast<GDALDatasetPrivate *>(m_hPrivateData);
    if( psPrivate )
    {
        psPrivate->eStateReadWriteMutex = RW_MUTEX_STATE_DISABLED;
    }
}

/************************************************************************/
/*                      TemporarilyDropReadWriteLock()                  */
/************************************************************************/

void GDALDataset::TemporarilyDropReadWriteLock()
{
    GDALDatasetPrivate *psPrivate =
        static_cast<GDALDatasetPrivate *>(m_hPrivateData);
    if( psPrivate && psPrivate->hMutex )
    {
#ifdef DEBUG_VERBOSE
        CPLDebug("GDAL", "[Thread " CPL_FRMT_GIB "] "
                 "Temporarily drop RW mutex for %s",
                 CPLGetPID(), GetDescription());
#endif
        CPLAcquireMutex(psPrivate->hMutex, 1000.0);
        const int nCount = psPrivate->oMapThreadToMutexTakenCount[CPLGetPID()];
#ifdef DEBUG_EXTRA
        psPrivate->oMapThreadToMutexTakenCountSaved[CPLGetPID()] = nCount;
#endif
        for(int i = 0; i < nCount + 1; i++)
        {
            CPLReleaseMutex(psPrivate->hMutex);
        }
    }
}

/************************************************************************/
/*                       ReacquireReadWriteLock()                       */
/************************************************************************/

void GDALDataset::ReacquireReadWriteLock()
{
    GDALDatasetPrivate *psPrivate =
        static_cast<GDALDatasetPrivate *>(m_hPrivateData);
    if( psPrivate && psPrivate->hMutex )
    {
#ifdef DEBUG_VERBOSE
        CPLDebug("GDAL", "[Thread " CPL_FRMT_GIB "] "
                 "Reacquire temporarily dropped RW mutex for %s",
                 CPLGetPID(), GetDescription());
#endif
        CPLAcquireMutex(psPrivate->hMutex, 1000.0);
        const int nCount = psPrivate->oMapThreadToMutexTakenCount[CPLGetPID()];
#ifdef DEBUG_EXTRA
        CPLAssert(nCount ==
                  psPrivate->oMapThreadToMutexTakenCountSaved[CPLGetPID()]);
#endif
        if( nCount == 0 )
            CPLReleaseMutex(psPrivate->hMutex);
        for(int i = 0; i < nCount - 1; i++)
        {
            CPLAcquireMutex(psPrivate->hMutex, 1000.0);
        }
    }
}

/************************************************************************/
/*                           AcquireMutex()                             */
/************************************************************************/

int GDALDataset::AcquireMutex()
{
    GDALDatasetPrivate *psPrivate =
        static_cast<GDALDatasetPrivate *>(m_hPrivateData);
    if( psPrivate == NULL )
        return 0;
    return CPLCreateOrAcquireMutex(&(psPrivate->hMutex), 1000.0);
}

/************************************************************************/
/*                          ReleaseMutex()                              */
/************************************************************************/

void GDALDataset::ReleaseMutex()
{
    GDALDatasetPrivate *psPrivate =
        static_cast<GDALDatasetPrivate *>(m_hPrivateData);
    if( psPrivate )
        CPLReleaseMutex(psPrivate->hMutex);
}
//! @endcond
