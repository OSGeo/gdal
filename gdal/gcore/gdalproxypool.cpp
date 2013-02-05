/******************************************************************************
 * $Id$
 *
 * Project:  GDAL Core
 * Purpose:  A dataset and raster band classes that differ the opening of the
 *           underlying dataset in a limited pool of opened datasets.
 * Author:   Even Rouault <even dot rouault at mines dash paris dot org>
 *
 ******************************************************************************
 * Copyright (c) 2008, Even Rouault
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

#include "gdal_proxy.h"
#include "cpl_multiproc.h"

CPL_CVSID("$Id$");

/* Functions shared between gdalproxypool.cpp and gdaldataset.cpp */
void** GDALGetphDLMutex();
void GDALSetResponsiblePIDForCurrentThread(GIntBig responsiblePID);
GIntBig GDALGetResponsiblePIDForCurrentThread();

/* We *must* share the same mutex as the gdaldataset.cpp file, as we are */
/* doing GDALOpen() calls that can indirectly call GDALOpenShared() on */
/* an auxiliary dataset ... */
/* Then we could get dead-locks in multi-threaded use case */

/* ******************************************************************** */
/*                         GDALDatasetPool                              */
/* ******************************************************************** */

/* This class is a singleton that maintains a pool of opened datasets */
/* The cache uses a LRU strategy */

class GDALDatasetPool;
static GDALDatasetPool* singleton = NULL;

struct _GDALProxyPoolCacheEntry
{
    GIntBig       responsiblePID;
    char         *pszFileName;
    GDALDataset  *poDS;

    /* Ref count of the cached dataset */
    int           refCount;

    GDALProxyPoolCacheEntry* prev;
    GDALProxyPoolCacheEntry* next;
};

class GDALDatasetPool
{
    private:
        /* Ref count of the pool singleton */
        /* Taken by "toplevel" GDALProxyPoolDataset in its constructor and released */
        /* in its destructor. See also refCountOfDisableRefCount for the difference */
        /* between toplevel and inner GDALProxyPoolDataset */
        int refCount;

        int maxSize;
        int currentSize;
        GDALProxyPoolCacheEntry* firstEntry;
        GDALProxyPoolCacheEntry* lastEntry;

        /* This variable prevents a dataset that is going to be opened in GDALDatasetPool::_RefDataset */
        /* from increasing refCount if, during its opening, it creates a GDALProxyPoolDataset */
        /* We increment it before opening or closing a cached dataset and decrement it afterwards */
        /* The typical use case is a VRT made of simple sources that are VRT */
        /* We don't want the "inner" VRT to take a reference on the pool, otherwise there is */
        /* a high chance that this reference will not be dropped and the pool remain ghost */
        int refCountOfDisableRefCount;

        /* Caution : to be sure that we don't run out of entries, size must be at */
        /* least greater or equal than the maximum number of threads */
        GDALDatasetPool(int maxSize);
        ~GDALDatasetPool();
        GDALProxyPoolCacheEntry* _RefDataset(const char* pszFileName, GDALAccess eAccess);

        void ShowContent();
        void CheckLinks();

    public:
        static void Ref();
        static void Unref();
        static GDALProxyPoolCacheEntry* RefDataset(const char* pszFileName, GDALAccess eAccess);
        static void UnrefDataset(GDALProxyPoolCacheEntry* cacheEntry);

        static void PreventDestroy();
        static void ForceDestroy();
};


/************************************************************************/
/*                         GDALDatasetPool()                            */
/************************************************************************/

GDALDatasetPool::GDALDatasetPool(int maxSize)
{
    this->maxSize = maxSize;
    currentSize = 0;
    firstEntry = NULL;
    lastEntry = NULL;
    refCount = 0;
    refCountOfDisableRefCount = 0;
}

/************************************************************************/
/*                        ~GDALDatasetPool()                            */
/************************************************************************/

GDALDatasetPool::~GDALDatasetPool()
{
    GDALProxyPoolCacheEntry* cur = firstEntry;
    GIntBig responsiblePID = GDALGetResponsiblePIDForCurrentThread();
    while(cur)
    {
        GDALProxyPoolCacheEntry* next = cur->next;
        CPLFree(cur->pszFileName);
        CPLAssert(cur->refCount == 0);
        if (cur->poDS)
        {
            GDALSetResponsiblePIDForCurrentThread(cur->responsiblePID);
            GDALClose(cur->poDS);
        }
        CPLFree(cur);
        cur = next;
    }
    GDALSetResponsiblePIDForCurrentThread(responsiblePID);
}

/************************************************************************/
/*                            ShowContent()                             */
/************************************************************************/

void GDALDatasetPool::ShowContent()
{
    GDALProxyPoolCacheEntry* cur = firstEntry;
    int i = 0;
    while(cur)
    {
        printf("[%d] pszFileName=%s, refCount=%d, responsiblePID=%d\n",
               i, cur->pszFileName, cur->refCount, (int)cur->responsiblePID);
        i++;
        cur = cur->next;
    }
}

/************************************************************************/
/*                             CheckLinks()                             */
/************************************************************************/

void GDALDatasetPool::CheckLinks()
{
    GDALProxyPoolCacheEntry* cur = firstEntry;
    int i = 0;
    while(cur)
    {
        CPLAssert(cur == firstEntry || cur->prev->next == cur);
        CPLAssert(cur == lastEntry || cur->next->prev == cur);
        i++;
	CPLAssert(cur->next != NULL || cur == lastEntry);
        cur = cur->next;
    }
    CPLAssert(i == currentSize);
}

/************************************************************************/
/*                            _RefDataset()                             */
/************************************************************************/

GDALProxyPoolCacheEntry* GDALDatasetPool::_RefDataset(const char* pszFileName, GDALAccess eAccess)
{
    GDALProxyPoolCacheEntry* cur = firstEntry;
    GIntBig responsiblePID = GDALGetResponsiblePIDForCurrentThread();
    GDALProxyPoolCacheEntry* lastEntryWithZeroRefCount = NULL;

    while(cur)
    {
        GDALProxyPoolCacheEntry* next = cur->next;

        if (strcmp(cur->pszFileName, pszFileName) == 0 &&
            cur->responsiblePID == responsiblePID)
        {
            if (cur != firstEntry)
            {
                /* Move to begin */
                if (cur->next)
                    cur->next->prev = cur->prev;
                else
                    lastEntry = cur->prev;
                cur->prev->next = cur->next;
                cur->prev = NULL;
                firstEntry->prev = cur;
                cur->next = firstEntry;
                firstEntry = cur;

#ifdef DEBUG_PROXY_POOL
                CheckLinks();
#endif
            }

            cur->refCount ++;
            return cur;
        }

        if (cur->refCount == 0)
            lastEntryWithZeroRefCount = cur;

        cur = next;
    }

    if (currentSize == maxSize)
    {
        if (lastEntryWithZeroRefCount == NULL)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Too many threads are running for the current value of the dataset pool size (%d).\n"
                     "or too many proxy datasets are opened in a cascaded way.\n"
                     "Try increasing GDAL_MAX_DATASET_POOL_SIZE.", maxSize);
            return NULL;
        }

        CPLFree(lastEntryWithZeroRefCount->pszFileName);
        lastEntryWithZeroRefCount->pszFileName = NULL;
        if (lastEntryWithZeroRefCount->poDS)
        {
            /* Close by pretending we are the thread that GDALOpen'ed this */
            /* dataset */
            GDALSetResponsiblePIDForCurrentThread(lastEntryWithZeroRefCount->responsiblePID);

            refCountOfDisableRefCount ++;
            GDALClose(lastEntryWithZeroRefCount->poDS);
            refCountOfDisableRefCount --;

            lastEntryWithZeroRefCount->poDS = NULL;
            GDALSetResponsiblePIDForCurrentThread(responsiblePID);
        }

        /* Recycle this entry for the to-be-openeded dataset and */
        /* moves it to the top of the list */
        if (lastEntryWithZeroRefCount->prev)
            lastEntryWithZeroRefCount->prev->next = lastEntryWithZeroRefCount->next;
        else
            CPLAssert(0);
        if (lastEntryWithZeroRefCount->next)
            lastEntryWithZeroRefCount->next->prev = lastEntryWithZeroRefCount->prev;
        else
        {
            CPLAssert(lastEntryWithZeroRefCount == lastEntry);
            lastEntry->prev->next = NULL;
            lastEntry = lastEntry->prev;
        }
        lastEntryWithZeroRefCount->prev = NULL;
        lastEntryWithZeroRefCount->next = firstEntry;
        firstEntry->prev = lastEntryWithZeroRefCount;
        cur = firstEntry = lastEntryWithZeroRefCount;
#ifdef DEBUG_PROXY_POOL
        CheckLinks();
#endif
    }
    else
    {
        /* Prepend */
        cur = (GDALProxyPoolCacheEntry*) CPLMalloc(sizeof(GDALProxyPoolCacheEntry));
        if (lastEntry == NULL)
            lastEntry = cur;
        cur->prev = NULL;
        cur->next = firstEntry;
        if (firstEntry)
            firstEntry->prev = cur;
        firstEntry = cur;
        currentSize ++;
#ifdef DEBUG_PROXY_POOL
        CheckLinks();
#endif
    }

    cur->pszFileName = CPLStrdup(pszFileName);
    cur->responsiblePID = responsiblePID;
    cur->refCount = 1;

    refCountOfDisableRefCount ++;
    const char* pszOldVal = CPLGetConfigOption("GDAL_RPC", NULL);
    char* pszOldValDup = (pszOldVal) ? CPLStrdup(pszOldVal) : NULL;
    CPLSetThreadLocalConfigOption("GDAL_RPC", "OFF");
    cur->poDS = (GDALDataset*) GDALOpen(pszFileName, eAccess);
    CPLSetThreadLocalConfigOption("GDAL_RPC", pszOldValDup);
    CPLFree(pszOldValDup);
    refCountOfDisableRefCount --;

    return cur;
}

/************************************************************************/
/*                                 Ref()                                */
/************************************************************************/

void GDALDatasetPool::Ref()
{
    CPLMutexHolderD( GDALGetphDLMutex() );
    if (singleton == NULL)
    {
        int maxSize = atoi(CPLGetConfigOption("GDAL_MAX_DATASET_POOL_SIZE", "100"));
        if (maxSize < 2 || maxSize > 1000)
            maxSize = 100;
        singleton = new GDALDatasetPool(maxSize);
    }
    if (singleton->refCountOfDisableRefCount == 0)
      singleton->refCount++;
}

/* keep that in sync with gdaldrivermanager.cpp */
void GDALDatasetPool::PreventDestroy()
{
    CPLMutexHolderD( GDALGetphDLMutex() );
    if (! singleton)
        return;
    singleton->refCountOfDisableRefCount ++;
}

/* keep that in sync with gdaldrivermanager.cpp */
void GDALDatasetPoolPreventDestroy()
{
    GDALDatasetPool::PreventDestroy();
}


/************************************************************************/
/*                               Unref()                                */
/************************************************************************/

void GDALDatasetPool::Unref()
{
    CPLMutexHolderD( GDALGetphDLMutex() );
    if (! singleton)
    {
        CPLAssert(0);
        return;
    }
    if (singleton->refCountOfDisableRefCount == 0)
    {
      singleton->refCount--;
      if (singleton->refCount == 0)
      {
          delete singleton;
          singleton = NULL;
      }
    }
}

/* keep that in sync with gdaldrivermanager.cpp */
void GDALDatasetPool::ForceDestroy()
{
    CPLMutexHolderD( GDALGetphDLMutex() );
    if (! singleton)
        return;
    singleton->refCountOfDisableRefCount --;
    CPLAssert(singleton->refCountOfDisableRefCount == 0);
    singleton->refCount = 0;
    delete singleton;
    singleton = NULL;
}

/* keep that in sync with gdaldrivermanager.cpp */
void GDALDatasetPoolForceDestroy()
{
    GDALDatasetPool::ForceDestroy();
}

/************************************************************************/
/*                           RefDataset()                               */
/************************************************************************/

GDALProxyPoolCacheEntry* GDALDatasetPool::RefDataset(const char* pszFileName, GDALAccess eAccess)
{
    CPLMutexHolderD( GDALGetphDLMutex() );
    return singleton->_RefDataset(pszFileName, eAccess);
}

/************************************************************************/
/*                       UnrefDataset()                                 */
/************************************************************************/

void GDALDatasetPool::UnrefDataset(GDALProxyPoolCacheEntry* cacheEntry)
{
    CPLMutexHolderD( GDALGetphDLMutex() );
    cacheEntry->refCount --;
}

CPL_C_START

typedef struct
{
    char* pszDomain;
    char** papszMetadata;
} GetMetadataElt;

static
unsigned long hash_func_get_metadata(const void* _elt)
{
    GetMetadataElt* elt = (GetMetadataElt*) _elt;
    return CPLHashSetHashStr(elt->pszDomain);
}

static
int equal_func_get_metadata(const void* _elt1, const void* _elt2)
{
    GetMetadataElt* elt1 = (GetMetadataElt*) _elt1;
    GetMetadataElt* elt2 = (GetMetadataElt*) _elt2;
    return CPLHashSetEqualStr(elt1->pszDomain, elt2->pszDomain);
}

static
void free_func_get_metadata(void* _elt)
{
    GetMetadataElt* elt = (GetMetadataElt*) _elt;
    CPLFree(elt->pszDomain);
    CSLDestroy(elt->papszMetadata);
}


typedef struct
{
    char* pszName;
    char* pszDomain;
    char* pszMetadataItem;
} GetMetadataItemElt;

static
unsigned long hash_func_get_metadata_item(const void* _elt)
{
    GetMetadataItemElt* elt = (GetMetadataItemElt*) _elt;
    return CPLHashSetHashStr(elt->pszName) ^ CPLHashSetHashStr(elt->pszDomain);
}

static
int equal_func_get_metadata_item(const void* _elt1, const void* _elt2)
{
    GetMetadataItemElt* elt1 = (GetMetadataItemElt*) _elt1;
    GetMetadataItemElt* elt2 = (GetMetadataItemElt*) _elt2;
    return CPLHashSetEqualStr(elt1->pszName, elt2->pszName) &&
           CPLHashSetEqualStr(elt1->pszDomain, elt2->pszDomain);
}

static
void free_func_get_metadata_item(void* _elt)
{
    GetMetadataItemElt* elt = (GetMetadataItemElt*) _elt;
    CPLFree(elt->pszName);
    CPLFree(elt->pszDomain);
    CPLFree(elt->pszMetadataItem);
}

CPL_C_END

/* ******************************************************************** */
/*                     GDALProxyPoolDataset                             */
/* ******************************************************************** */

/* Note : the bShared parameter must be used with caution. You can */
/* set it to TRUE  for being used as a VRT source : in that case, */
/* VRTSimpleSource will take care of destroying it when there are no */
/* reference to it (in VRTSimpleSource::~VRTSimpleSource()) */
/* However this will not be registered as a genuine shared dataset, like it */
/* would have been with MarkAsShared(). But MarkAsShared() is not usable for */
/* GDALProxyPoolDataset objects, as they share the same description as their */
/* underlying dataset. So *NEVER* call MarkAsShared() on a GDALProxyPoolDataset */
/* object */

GDALProxyPoolDataset::GDALProxyPoolDataset(const char* pszSourceDatasetDescription,
                                   int nRasterXSize, int nRasterYSize,
                                   GDALAccess eAccess, int bShared,
                                   const char * pszProjectionRef,
                                   double * padfGeoTransform)
{
    GDALDatasetPool::Ref();

    SetDescription(pszSourceDatasetDescription);

    this->nRasterXSize = nRasterXSize;
    this->nRasterYSize = nRasterYSize;
    this->eAccess = eAccess;

    this->bShared = bShared;

    this->responsiblePID = GDALGetResponsiblePIDForCurrentThread();

    if (pszProjectionRef)
    {
        this->pszProjectionRef = NULL;
        bHasSrcProjection = FALSE;
    }
    else
    {
        this->pszProjectionRef = CPLStrdup(pszProjectionRef);
        bHasSrcProjection = TRUE;
    }
    if (padfGeoTransform)
    {
        memcpy(adfGeoTransform, padfGeoTransform,6 * sizeof(double));
        bHasSrcGeoTransform = TRUE;
    }
    else
    {
        adfGeoTransform[0] = 0;
        adfGeoTransform[1] = 1;
        adfGeoTransform[2] = 0;
        adfGeoTransform[3] = 0;
        adfGeoTransform[4] = 0;
        adfGeoTransform[5] = 1;
        bHasSrcGeoTransform = FALSE;
    }

    pszGCPProjection = NULL;
    nGCPCount = 0;
    pasGCPList = NULL;
    metadataSet = NULL;
    metadataItemSet = NULL;
    cacheEntry = NULL;
}

/************************************************************************/
/*                    ~GDALProxyPoolDataset()                           */
/************************************************************************/

GDALProxyPoolDataset::~GDALProxyPoolDataset()
{
    /* See comment in constructor */
    /* It is not really a genuine shared dataset, so we don't */
    /* want ~GDALDataset() to try to release it from its */
    /* shared dataset hashset. This will save a */
    /* "Should not happen. Cannot find %s, this=%p in phSharedDatasetSet" debug message */
    bShared = FALSE;

    CPLFree(pszProjectionRef);
    CPLFree(pszGCPProjection);
    if (nGCPCount)
    {
        GDALDeinitGCPs( nGCPCount, pasGCPList );
        CPLFree( pasGCPList );
    }
    if (metadataSet)
        CPLHashSetDestroy(metadataSet);
    if (metadataItemSet)
        CPLHashSetDestroy(metadataItemSet);

    GDALDatasetPool::Unref();
}

/************************************************************************/
/*                    AddSrcBandDescription()                           */
/************************************************************************/

void GDALProxyPoolDataset::AddSrcBandDescription( GDALDataType eDataType, int nBlockXSize, int nBlockYSize)
{
    SetBand(nBands + 1, new GDALProxyPoolRasterBand(this, nBands + 1, eDataType, nBlockXSize, nBlockYSize));
}

/************************************************************************/
/*                    RefUnderlyingDataset()                            */
/************************************************************************/

GDALDataset* GDALProxyPoolDataset::RefUnderlyingDataset()
{
    /* We pretend that the current thread is responsiblePID, that is */
    /* to say the thread that created that GDALProxyPoolDataset object. */
    /* This is for the case when a GDALProxyPoolDataset is created by a */
    /* thread and used by other threads. These other threads, when doing actual */
    /* IO, will come there and potentially open the underlying dataset. */
    /* By doing this, they can indirectly call GDALOpenShared() on .aux file */
    /* for example. So this call to GDALOpenShared() must occur as if it */
    /* was done by the creating thread, otherwise it will not be correctly closed afterwards... */
    /* To make a long story short : this is necessary when warping with ChunkAndWarpMulti */
    /* a VRT of GeoTIFFs that have associated .aux files */
    GIntBig curResponsiblePID = GDALGetResponsiblePIDForCurrentThread();
    GDALSetResponsiblePIDForCurrentThread(responsiblePID);
    cacheEntry = GDALDatasetPool::RefDataset(GetDescription(), eAccess);
    GDALSetResponsiblePIDForCurrentThread(curResponsiblePID);
    if (cacheEntry != NULL)
    {
        if (cacheEntry->poDS != NULL)
            return cacheEntry->poDS;
        else
            GDALDatasetPool::UnrefDataset(cacheEntry);
    }
    return NULL;
}

/************************************************************************/
/*                    UnrefUnderlyingDataset()                        */
/************************************************************************/

void GDALProxyPoolDataset::UnrefUnderlyingDataset(GDALDataset* poUnderlyingDataset)
{
    if (cacheEntry != NULL)
    {
        CPLAssert(cacheEntry->poDS == poUnderlyingDataset);
        if (cacheEntry->poDS != NULL)
            GDALDatasetPool::UnrefDataset(cacheEntry);
    }
}

/************************************************************************/
/*                        SetProjection()                               */
/************************************************************************/

CPLErr GDALProxyPoolDataset::SetProjection(const char* pszProjectionRef)
{
    bHasSrcProjection = FALSE;
    return GDALProxyDataset::SetProjection(pszProjectionRef);
}

/************************************************************************/
/*                        GetProjectionRef()                            */
/************************************************************************/

const char *GDALProxyPoolDataset::GetProjectionRef()
{
    if (bHasSrcProjection)
        return pszProjectionRef;
    else
        return GDALProxyDataset::GetProjectionRef();
}

/************************************************************************/
/*                        SetGeoTransform()                             */
/************************************************************************/

CPLErr GDALProxyPoolDataset::SetGeoTransform( double * padfGeoTransform )
{
    bHasSrcGeoTransform = FALSE;
    return GDALProxyDataset::SetGeoTransform(padfGeoTransform);
}

/************************************************************************/
/*                        GetGeoTransform()                             */
/************************************************************************/

CPLErr GDALProxyPoolDataset::GetGeoTransform( double * padfGeoTransform )
{
    if (bHasSrcGeoTransform)
    {
        memcpy(padfGeoTransform, adfGeoTransform, 6 * sizeof(double));
        return CE_None;
    }
    else
    {
        return GDALProxyDataset::GetGeoTransform(padfGeoTransform);
    }
}

/************************************************************************/
/*                            GetMetadata()                             */
/************************************************************************/

char      **GDALProxyPoolDataset::GetMetadata( const char * pszDomain  )
{
    if (metadataSet == NULL)
        metadataSet = CPLHashSetNew(hash_func_get_metadata,
                                    equal_func_get_metadata,
                                    free_func_get_metadata);

    GDALDataset* poUnderlyingDataset = RefUnderlyingDataset();
    if (poUnderlyingDataset == NULL)
        return NULL;

    char** papszUnderlyingMetadata = poUnderlyingDataset->GetMetadata(pszDomain);

    GetMetadataElt* pElt = (GetMetadataElt*) CPLMalloc(sizeof(GetMetadataElt));
    pElt->pszDomain = (pszDomain) ? CPLStrdup(pszDomain) : NULL;
    pElt->papszMetadata = CSLDuplicate(papszUnderlyingMetadata);
    CPLHashSetInsert(metadataSet, pElt);

    UnrefUnderlyingDataset(poUnderlyingDataset);

    return pElt->papszMetadata;
}

/************************************************************************/
/*                        GetMetadataItem()                             */
/************************************************************************/

const char *GDALProxyPoolDataset::GetMetadataItem( const char * pszName,
                                                   const char * pszDomain  )
{
    if (metadataItemSet == NULL)
        metadataItemSet = CPLHashSetNew(hash_func_get_metadata_item,
                                        equal_func_get_metadata_item,
                                        free_func_get_metadata_item);

    GDALDataset* poUnderlyingDataset = RefUnderlyingDataset();
    if (poUnderlyingDataset == NULL)
        return NULL;

    const char* pszUnderlyingMetadataItem =
            poUnderlyingDataset->GetMetadataItem(pszName, pszDomain);

    GetMetadataItemElt* pElt = (GetMetadataItemElt*) CPLMalloc(sizeof(GetMetadataItemElt));
    pElt->pszName = (pszName) ? CPLStrdup(pszName) : NULL;
    pElt->pszDomain = (pszDomain) ? CPLStrdup(pszDomain) : NULL;
    pElt->pszMetadataItem = (pszUnderlyingMetadataItem) ? CPLStrdup(pszUnderlyingMetadataItem) : NULL;
    CPLHashSetInsert(metadataItemSet, pElt);

    UnrefUnderlyingDataset(poUnderlyingDataset);

    return pElt->pszMetadataItem;
}

/************************************************************************/
/*                      GetInternalHandle()                             */
/************************************************************************/

void *GDALProxyPoolDataset::GetInternalHandle( const char * pszRequest)
{
    CPLError(CE_Warning, CPLE_AppDefined,
             "GetInternalHandle() cannot be safely called on a proxy pool dataset\n"
             "as the returned value may be invalidated at any time.\n");
    return GDALProxyDataset::GetInternalHandle(pszRequest);
}

/************************************************************************/
/*                     GetGCPProjection()                               */
/************************************************************************/

const char *GDALProxyPoolDataset::GetGCPProjection()
{
    GDALDataset* poUnderlyingDataset = RefUnderlyingDataset();
    if (poUnderlyingDataset == NULL)
        return NULL;

    CPLFree(pszGCPProjection);
    pszGCPProjection = NULL;

    const char* pszUnderlyingGCPProjection = poUnderlyingDataset->GetGCPProjection();
    if (pszUnderlyingGCPProjection)
        pszGCPProjection = CPLStrdup(pszUnderlyingGCPProjection);

    UnrefUnderlyingDataset(poUnderlyingDataset);

    return pszGCPProjection;
}

/************************************************************************/
/*                            GetGCPs()                                 */
/************************************************************************/

const GDAL_GCP *GDALProxyPoolDataset::GetGCPs()
{
    GDALDataset* poUnderlyingDataset = RefUnderlyingDataset();
    if (poUnderlyingDataset == NULL)
        return NULL;

    if (nGCPCount)
    {
        GDALDeinitGCPs( nGCPCount, pasGCPList );
        CPLFree( pasGCPList );
        pasGCPList = NULL;
    }

    const GDAL_GCP* pasUnderlyingGCPList = poUnderlyingDataset->GetGCPs();
    nGCPCount = poUnderlyingDataset->GetGCPCount();
    if (nGCPCount)
        pasGCPList = GDALDuplicateGCPs(nGCPCount, pasUnderlyingGCPList );

    UnrefUnderlyingDataset(poUnderlyingDataset);

    return pasGCPList;
}

/************************************************************************/
/*                     GDALProxyPoolDatasetCreate()                     */
/************************************************************************/

GDALProxyPoolDatasetH GDALProxyPoolDatasetCreate(const char* pszSourceDatasetDescription,
                                                 int nRasterXSize, int nRasterYSize,
                                                 GDALAccess eAccess, int bShared,
                                                 const char * pszProjectionRef,
                                                 double * padfGeoTransform)
{
    return (GDALProxyPoolDatasetH)
           new GDALProxyPoolDataset(pszSourceDatasetDescription,
                                    nRasterXSize, nRasterYSize,
                                    eAccess, bShared,
                                    pszProjectionRef, padfGeoTransform);
}

/************************************************************************/
/*                       GDALProxyPoolDatasetDelete()                   */
/************************************************************************/

void CPL_DLL GDALProxyPoolDatasetDelete(GDALProxyPoolDatasetH hProxyPoolDataset)
{
    delete (GDALProxyPoolDataset*)hProxyPoolDataset;
}

/************************************************************************/
/*              GDALProxyPoolDatasetAddSrcBandDescription()             */
/************************************************************************/

void GDALProxyPoolDatasetAddSrcBandDescription( GDALProxyPoolDatasetH hProxyPoolDataset,
                                                GDALDataType eDataType,
                                                int nBlockXSize, int nBlockYSize)
{
    ((GDALProxyPoolDataset*)hProxyPoolDataset)->
            AddSrcBandDescription(eDataType, nBlockXSize, nBlockYSize);
}

/* ******************************************************************** */
/*                    GDALProxyPoolRasterBand()                         */
/* ******************************************************************** */

GDALProxyPoolRasterBand::GDALProxyPoolRasterBand(GDALProxyPoolDataset* poDS, int nBand,
                                                 GDALDataType eDataType,
                                                 int nBlockXSize, int nBlockYSize)
{
    this->poDS         = poDS;
    this->nBand        = nBand;
    this->eDataType    = eDataType;
    this->nRasterXSize = poDS->GetRasterXSize();
    this->nRasterYSize = poDS->GetRasterYSize();
    this->nBlockXSize  = nBlockXSize;
    this->nBlockYSize  = nBlockYSize;

    Init();
}

/* ******************************************************************** */
/*                    GDALProxyPoolRasterBand()                         */
/* ******************************************************************** */

GDALProxyPoolRasterBand::GDALProxyPoolRasterBand(GDALProxyPoolDataset* poDS,
                                                 GDALRasterBand* poUnderlyingRasterBand)
{
    this->poDS         = poDS;
    this->nBand        = poUnderlyingRasterBand->GetBand();
    this->eDataType    = poUnderlyingRasterBand->GetRasterDataType();
    this->nRasterXSize = poUnderlyingRasterBand->GetXSize();
    this->nRasterYSize = poUnderlyingRasterBand->GetYSize();
    poUnderlyingRasterBand->GetBlockSize(&nBlockXSize, &nBlockYSize);

    Init();
}

/* ******************************************************************** */
 /*                                  Init()                             */
/* ******************************************************************** */

void GDALProxyPoolRasterBand::Init()
{
    metadataSet = NULL;
    metadataItemSet = NULL;
    pszUnitType = NULL;
    papszCategoryNames = NULL;
    poColorTable = NULL;

    nSizeProxyOverviewRasterBand = 0;
    papoProxyOverviewRasterBand = NULL;
    poProxyMaskBand = NULL;
}

/* ******************************************************************** */
/*                   ~GDALProxyPoolRasterBand()                         */
/* ******************************************************************** */
GDALProxyPoolRasterBand::~GDALProxyPoolRasterBand()
{
    if (metadataSet)
        CPLHashSetDestroy(metadataSet);
    if (metadataItemSet)
        CPLHashSetDestroy(metadataItemSet);
    CPLFree(pszUnitType);
    CSLDestroy(papszCategoryNames);
    if (poColorTable)
        delete poColorTable;

    int i;
    for(i=0;i<nSizeProxyOverviewRasterBand;i++)
    {
        if (papoProxyOverviewRasterBand[i])
            delete papoProxyOverviewRasterBand[i];
    }
    CPLFree(papoProxyOverviewRasterBand);
    if (poProxyMaskBand)
        delete poProxyMaskBand;
}


/************************************************************************/
/*                 AddSrcMaskBandDescription()                          */
/************************************************************************/

void GDALProxyPoolRasterBand::AddSrcMaskBandDescription( GDALDataType eDataType,
                                                         int nBlockXSize,
                                                         int nBlockYSize)
{
    CPLAssert(poProxyMaskBand == NULL);
    poProxyMaskBand = new GDALProxyPoolMaskBand((GDALProxyPoolDataset*)poDS,
                                                this, eDataType,
                                                nBlockXSize, nBlockYSize);
}

/************************************************************************/
/*                  RefUnderlyingRasterBand()                           */
/************************************************************************/

GDALRasterBand* GDALProxyPoolRasterBand::RefUnderlyingRasterBand()
{
    GDALDataset* poUnderlyingDataset = ((GDALProxyPoolDataset*)poDS)->RefUnderlyingDataset();
    if (poUnderlyingDataset == NULL)
        return NULL;

    GDALRasterBand* poBand = poUnderlyingDataset->GetRasterBand(nBand);
    if (poBand == NULL)
    {
        ((GDALProxyPoolDataset*)poDS)->UnrefUnderlyingDataset(poUnderlyingDataset);
    }

    return poBand;
}

/************************************************************************/
/*                  UnrefUnderlyingRasterBand()                       */
/************************************************************************/

void GDALProxyPoolRasterBand::UnrefUnderlyingRasterBand(GDALRasterBand* poUnderlyingRasterBand)
{
    if (poUnderlyingRasterBand)
        ((GDALProxyPoolDataset*)poDS)->UnrefUnderlyingDataset(poUnderlyingRasterBand->GetDataset());
}

/************************************************************************/
/*                            GetMetadata()                             */
/************************************************************************/

char      **GDALProxyPoolRasterBand::GetMetadata( const char * pszDomain  )
{
    if (metadataSet == NULL)
        metadataSet = CPLHashSetNew(hash_func_get_metadata,
                                    equal_func_get_metadata,
                                    free_func_get_metadata);

    GDALRasterBand* poUnderlyingRasterBand = RefUnderlyingRasterBand();
    if (poUnderlyingRasterBand == NULL)
        return NULL;

    char** papszUnderlyingMetadata = poUnderlyingRasterBand->GetMetadata(pszDomain);

    GetMetadataElt* pElt = (GetMetadataElt*) CPLMalloc(sizeof(GetMetadataElt));
    pElt->pszDomain = (pszDomain) ? CPLStrdup(pszDomain) : NULL;
    pElt->papszMetadata = CSLDuplicate(papszUnderlyingMetadata);
    CPLHashSetInsert(metadataSet, pElt);

    UnrefUnderlyingRasterBand(poUnderlyingRasterBand);

    return pElt->papszMetadata;
}

/************************************************************************/
/*                        GetMetadataItem()                             */
/************************************************************************/

const char *GDALProxyPoolRasterBand::GetMetadataItem( const char * pszName,
                                                   const char * pszDomain  )
{
    if (metadataItemSet == NULL)
        metadataItemSet = CPLHashSetNew(hash_func_get_metadata_item,
                                        equal_func_get_metadata_item,
                                        free_func_get_metadata_item);

    GDALRasterBand* poUnderlyingRasterBand = RefUnderlyingRasterBand();
    if (poUnderlyingRasterBand == NULL)
        return NULL;

    const char* pszUnderlyingMetadataItem =
            poUnderlyingRasterBand->GetMetadataItem(pszName, pszDomain);

    GetMetadataItemElt* pElt = (GetMetadataItemElt*) CPLMalloc(sizeof(GetMetadataItemElt));
    pElt->pszName = (pszName) ? CPLStrdup(pszName) : NULL;
    pElt->pszDomain = (pszDomain) ? CPLStrdup(pszDomain) : NULL;
    pElt->pszMetadataItem = (pszUnderlyingMetadataItem) ? CPLStrdup(pszUnderlyingMetadataItem) : NULL;
    CPLHashSetInsert(metadataItemSet, pElt);

    UnrefUnderlyingRasterBand(poUnderlyingRasterBand);

    return pElt->pszMetadataItem;
}

/* ******************************************************************** */
/*                       GetCategoryNames()                             */
/* ******************************************************************** */

char **GDALProxyPoolRasterBand::GetCategoryNames()
{
    GDALRasterBand* poUnderlyingRasterBand = RefUnderlyingRasterBand();
    if (poUnderlyingRasterBand == NULL)
        return NULL;

    CSLDestroy(papszCategoryNames);
    papszCategoryNames = NULL;

    char** papszUnderlyingCategoryNames = poUnderlyingRasterBand->GetCategoryNames();
    if (papszUnderlyingCategoryNames)
        papszCategoryNames = CSLDuplicate(papszUnderlyingCategoryNames);

    UnrefUnderlyingRasterBand(poUnderlyingRasterBand);

    return papszCategoryNames;
}

/* ******************************************************************** */
/*                           GetUnitType()                              */
/* ******************************************************************** */

const char *GDALProxyPoolRasterBand::GetUnitType()
{
    GDALRasterBand* poUnderlyingRasterBand = RefUnderlyingRasterBand();
    if (poUnderlyingRasterBand == NULL)
        return NULL;

    CPLFree(pszUnitType);
    pszUnitType = NULL;

    const char* pszUnderlyingUnitType = poUnderlyingRasterBand->GetUnitType();
    if (pszUnderlyingUnitType)
        pszUnitType = CPLStrdup(pszUnderlyingUnitType);

    UnrefUnderlyingRasterBand(poUnderlyingRasterBand);

    return pszUnitType;
}

/* ******************************************************************** */
/*                          GetColorTable()                             */
/* ******************************************************************** */

GDALColorTable *GDALProxyPoolRasterBand::GetColorTable()
{
    GDALRasterBand* poUnderlyingRasterBand = RefUnderlyingRasterBand();
    if (poUnderlyingRasterBand == NULL)
        return NULL;

    if (poColorTable)
        delete poColorTable;
    poColorTable = NULL;

    GDALColorTable* poUnderlyingColorTable = poUnderlyingRasterBand->GetColorTable();
    if (poUnderlyingColorTable)
        poColorTable = poUnderlyingColorTable->Clone();

    UnrefUnderlyingRasterBand(poUnderlyingRasterBand);

    return poColorTable;
}

/* ******************************************************************** */
/*                           GetOverview()                              */
/* ******************************************************************** */

GDALRasterBand *GDALProxyPoolRasterBand::GetOverview(int nOverviewBand)
{
    if (nOverviewBand >= 0 && nOverviewBand < nSizeProxyOverviewRasterBand)
    {
        if (papoProxyOverviewRasterBand[nOverviewBand])
            return papoProxyOverviewRasterBand[nOverviewBand];
    }

    GDALRasterBand* poUnderlyingRasterBand = RefUnderlyingRasterBand();
    if (poUnderlyingRasterBand == NULL)
        return NULL;

    GDALRasterBand* poOverviewRasterBand = poUnderlyingRasterBand->GetOverview(nOverviewBand);
    if (poOverviewRasterBand == NULL)
    {
        UnrefUnderlyingRasterBand(poUnderlyingRasterBand);
        return NULL;
    }

    if (nOverviewBand >= nSizeProxyOverviewRasterBand)
    {
        int i;
        papoProxyOverviewRasterBand = (GDALProxyPoolOverviewRasterBand**)
                CPLRealloc(papoProxyOverviewRasterBand,
                        sizeof(GDALProxyPoolOverviewRasterBand*) * (nOverviewBand + 1));
        for(i=nSizeProxyOverviewRasterBand; i<nOverviewBand + 1;i++)
            papoProxyOverviewRasterBand[i] = NULL;
        nSizeProxyOverviewRasterBand = nOverviewBand + 1;
    }

    papoProxyOverviewRasterBand[nOverviewBand] =
            new GDALProxyPoolOverviewRasterBand((GDALProxyPoolDataset*)poDS,
                                                poOverviewRasterBand,
                                                this, nOverviewBand);

    UnrefUnderlyingRasterBand(poUnderlyingRasterBand);

    return papoProxyOverviewRasterBand[nOverviewBand];
}

/* ******************************************************************** */
/*                     GetRasterSampleOverview()                        */
/* ******************************************************************** */

GDALRasterBand *GDALProxyPoolRasterBand::GetRasterSampleOverview( int nDesiredSamples)
{
    CPLError(CE_Failure, CPLE_AppDefined,
             "GDALProxyPoolRasterBand::GetRasterSampleOverview : not implemented yet");
    return NULL;
}

/* ******************************************************************** */
/*                           GetMaskBand()                              */
/* ******************************************************************** */

GDALRasterBand *GDALProxyPoolRasterBand::GetMaskBand()
{
    if (poProxyMaskBand)
        return poProxyMaskBand;

    GDALRasterBand* poUnderlyingRasterBand = RefUnderlyingRasterBand();
    if (poUnderlyingRasterBand == NULL)
        return NULL;

    GDALRasterBand* poMaskBand = poUnderlyingRasterBand->GetMaskBand();

    poProxyMaskBand =
            new GDALProxyPoolMaskBand((GDALProxyPoolDataset*)poDS,
                                       poMaskBand,
                                       this);

    UnrefUnderlyingRasterBand(poUnderlyingRasterBand);

    return poProxyMaskBand;
}

/* ******************************************************************** */
/*             GDALProxyPoolOverviewRasterBand()                        */
/* ******************************************************************** */

GDALProxyPoolOverviewRasterBand::GDALProxyPoolOverviewRasterBand(GDALProxyPoolDataset* poDS,
                                                                 GDALRasterBand* poUnderlyingOverviewBand,
                                                                 GDALProxyPoolRasterBand* poMainBand,
                                                                 int nOverviewBand) :
        GDALProxyPoolRasterBand(poDS, poUnderlyingOverviewBand)
{
    this->poMainBand = poMainBand;
    this->nOverviewBand = nOverviewBand;

    poUnderlyingMainRasterBand = NULL;
    nRefCountUnderlyingMainRasterBand = 0;
}

/* ******************************************************************** */
/*                  ~GDALProxyPoolOverviewRasterBand()                  */
/* ******************************************************************** */

GDALProxyPoolOverviewRasterBand::~GDALProxyPoolOverviewRasterBand()
{
    CPLAssert(nRefCountUnderlyingMainRasterBand == 0);
}

/* ******************************************************************** */
/*                    RefUnderlyingRasterBand()                         */
/* ******************************************************************** */

GDALRasterBand* GDALProxyPoolOverviewRasterBand::RefUnderlyingRasterBand()
{
    poUnderlyingMainRasterBand = poMainBand->RefUnderlyingRasterBand();
    if (poUnderlyingMainRasterBand == NULL)
        return NULL;

    nRefCountUnderlyingMainRasterBand ++;
    return poUnderlyingMainRasterBand->GetOverview(nOverviewBand);
}

/* ******************************************************************** */
/*                  UnrefUnderlyingRasterBand()                         */
/* ******************************************************************** */

void GDALProxyPoolOverviewRasterBand::UnrefUnderlyingRasterBand(GDALRasterBand* poUnderlyingRasterBand)
{
    poMainBand->UnrefUnderlyingRasterBand(poUnderlyingMainRasterBand);
    nRefCountUnderlyingMainRasterBand --;
}


/* ******************************************************************** */
/*                     GDALProxyPoolMaskBand()                          */
/* ******************************************************************** */

GDALProxyPoolMaskBand::GDALProxyPoolMaskBand(GDALProxyPoolDataset* poDS,
                                             GDALRasterBand* poUnderlyingMaskBand,
                                             GDALProxyPoolRasterBand* poMainBand) :
        GDALProxyPoolRasterBand(poDS, poUnderlyingMaskBand)
{
    this->poMainBand = poMainBand;

    poUnderlyingMainRasterBand = NULL;
    nRefCountUnderlyingMainRasterBand = 0;
}

/* ******************************************************************** */
/*                     GDALProxyPoolMaskBand()                          */
/* ******************************************************************** */

GDALProxyPoolMaskBand::GDALProxyPoolMaskBand(GDALProxyPoolDataset* poDS,
                                             GDALProxyPoolRasterBand* poMainBand,
                                             GDALDataType eDataType,
                                             int nBlockXSize, int nBlockYSize) :
        GDALProxyPoolRasterBand(poDS, 1, eDataType, nBlockXSize, nBlockYSize)
{
    this->poMainBand = poMainBand;

    poUnderlyingMainRasterBand = NULL;
    nRefCountUnderlyingMainRasterBand = 0;
}

/* ******************************************************************** */
/*                          ~GDALProxyPoolMaskBand()                    */
/* ******************************************************************** */

GDALProxyPoolMaskBand::~GDALProxyPoolMaskBand()
{
    CPLAssert(nRefCountUnderlyingMainRasterBand == 0);
}

/* ******************************************************************** */
/*                    RefUnderlyingRasterBand()                         */
/* ******************************************************************** */

GDALRasterBand* GDALProxyPoolMaskBand::RefUnderlyingRasterBand()
{
    poUnderlyingMainRasterBand = poMainBand->RefUnderlyingRasterBand();
    if (poUnderlyingMainRasterBand == NULL)
        return NULL;

    nRefCountUnderlyingMainRasterBand ++;
    return poUnderlyingMainRasterBand->GetMaskBand();
}

/* ******************************************************************** */
/*                  UnrefUnderlyingRasterBand()                         */
/* ******************************************************************** */

void GDALProxyPoolMaskBand::UnrefUnderlyingRasterBand(GDALRasterBand* poUnderlyingRasterBand)
{
    poMainBand->UnrefUnderlyingRasterBand(poUnderlyingMainRasterBand);
    nRefCountUnderlyingMainRasterBand --;
}
