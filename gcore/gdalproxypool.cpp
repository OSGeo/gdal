/******************************************************************************
 * $Id: gdalproxypool.cpp $
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


/* ******************************************************************** */
/*                         GDALDatasetPool                              */
/* ******************************************************************** */

/* This class is a singleton that maintains a pool of opened datasets */
/* The cache uses a LRU strategy */

class GDALDatasetPool;
static GDALDatasetPool* singleton = NULL;
static int refCount = 0;
static void* CacheMutex = NULL;

typedef struct _CacheEntry CacheEntry;
struct _CacheEntry
{
    char* fileName;
    GDALDataset* ds;

    CacheEntry* next;
};

class GDALDatasetPool
{
    private:
        int maxSize;
        int currentSize;
        CacheEntry* firstEntry;

        /* Caution : to be sure that we don't run out of entries, size must be at */
        /* least greater or equal than the maximum number of threads */
        GDALDatasetPool(int maxSize);
        ~GDALDatasetPool();
        GDALDataset* _GetDataset(const char* fileName, GDALAccess eAccess);

    public:
        static void Ref();
        static void Unref();
        static GDALDataset* GetDataset(const char* fileName, GDALAccess eAccess);
};


/************************************************************************/
/*                         GDALDatasetPool()                            */
/************************************************************************/

GDALDatasetPool::GDALDatasetPool(int maxSize)
{
    this->maxSize = maxSize;
    currentSize = 0;
    firstEntry = NULL;
}

/************************************************************************/
/*                        ~GDALDatasetPool()                            */
/************************************************************************/

GDALDatasetPool::~GDALDatasetPool()
{
    CacheEntry* cur = firstEntry;
    while(cur)
    {
        CacheEntry* next = cur->next;
        CPLFree(cur->fileName);
        if (cur->ds)
            GDALClose(cur->ds);
        CPLFree(cur);
        cur = next;
    }
}

/************************************************************************/
/*                            _GetDataset()                             */
/************************************************************************/

GDALDataset* GDALDatasetPool::_GetDataset(const char* fileName, GDALAccess eAccess)
{
    CacheEntry* cur = firstEntry;
    CacheEntry* lastEntry = cur;
    CacheEntry* prevEntry = NULL;
    while(cur)
    {
        CacheEntry* next = cur->next;

        if (strcmp(cur->fileName, fileName) == 0)
        {
            if (cur != firstEntry)
            {
                prevEntry->next = cur->next;
                cur->next = firstEntry;
                firstEntry = cur;
            }
            return cur->ds;
        }

        if (next)
        {
            prevEntry = cur;
            lastEntry = next;
        }
        cur = next;
    }

    if (currentSize == maxSize)
    {
        CPLAssert(lastEntry);
        CPLFree(lastEntry->fileName);
        if (lastEntry->ds)
            GDALClose(lastEntry->ds);

        prevEntry->next = NULL;
        cur = lastEntry;
        cur->next = firstEntry;
        firstEntry = cur;
    }
    else
    {
        cur = (CacheEntry*) CPLMalloc(sizeof(CacheEntry));
        cur->next = NULL;
        if (lastEntry)
            lastEntry->next = cur;
        else
        {
            CPLAssert(firstEntry == NULL);
            firstEntry = cur;
        }

        currentSize ++;
    }

    cur->fileName = CPLStrdup(fileName);
    cur->ds = (GDALDataset*) GDALOpen(fileName, eAccess);
    return cur->ds;
}

/************************************************************************/
/*                                 Ref()                                */
/************************************************************************/

void GDALDatasetPool::Ref()
{
    CPLMutexHolderD( &CacheMutex );
    if (singleton == NULL)
    {
        int maxSize = atoi(CPLGetConfigOption("GDAL_MAX_DATASET_POOL_SIZE", "100"));
        if (maxSize < 2 || maxSize > 1000)
            maxSize = 100;
        singleton = new GDALDatasetPool(maxSize);
    }
    refCount++;
}

/************************************************************************/
/*                               Unref()                                */
/************************************************************************/

void GDALDatasetPool::Unref()
{
    CPLMutexHolderD( &CacheMutex );
    refCount--;
    if (refCount == 0)
    {
        delete singleton;
        singleton = NULL;
    }
}

/************************************************************************/
/*                           GetDataset()                               */
/************************************************************************/

GDALDataset* GDALDatasetPool::GetDataset(const char* fileName, GDALAccess eAccess)
{
    CPLMutexHolderD( &CacheMutex );
    if (! singleton) return NULL;
    return singleton->_GetDataset(fileName, eAccess);
}

/* ******************************************************************** */
/*                     GDALProxyPoolDataset                             */
/* ******************************************************************** */

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
}

/************************************************************************/
/*                    ~GDALProxyPoolDataset()                           */
/************************************************************************/

GDALProxyPoolDataset::~GDALProxyPoolDataset()
{
    CPLFree(pszProjectionRef);

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
/*                    GetUnderlyingDataset()                            */
/************************************************************************/

GDALDataset* GDALProxyPoolDataset::GetUnderlyingDataset()
{
    return GDALDatasetPool::GetDataset(GetDescription(), eAccess);
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
/*                    GDALProxyPoolRasterBand                           */
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
}

/************************************************************************/
/*                  GetUnderlyingRasterBand()                           */
/************************************************************************/

GDALRasterBand* GDALProxyPoolRasterBand::GetUnderlyingRasterBand()
{
    GDALDataset* poUnderlyingDataset = ((GDALProxyPoolDataset*)poDS)->GetUnderlyingDataset();
    if (poUnderlyingDataset == NULL)
        return NULL;

    return poUnderlyingDataset->GetRasterBand(nBand);
}
