/*
 *  keadataset.h
 *
 *  Created by Pete Bunting on 01/08/2012.
 *  Copyright 2012 LibKEA. All rights reserved.
 *
 *  This file is part of LibKEA.
 *
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef KEADATASET_H
#define KEADATASET_H

#include "gdal_priv.h"
#include "cpl_multiproc.h"
#include "libkea_headers.h"

class LockedRefCount;

// class that implements a GDAL dataset
class KEADataset final : public GDALDataset
{
    static H5::H5File *CreateLL(const char *pszFilename, int nXSize, int nYSize,
                                int nBands, GDALDataType eType,
                                char **papszParamList);

  public:
    // constructor/destructor
    KEADataset(H5::H5File *keaImgH5File, GDALAccess eAccess);
    ~KEADataset();

    // static methods that handle open and creation
    // the driver class has pointers to these
    static GDALDataset *Open(GDALOpenInfo *);
    static int Identify(GDALOpenInfo *poOpenInfo);
    static GDALDataset *Create(const char *pszFilename, int nXSize, int nYSize,
                               int nBands, GDALDataType eType,
                               char **papszParamList);
    static GDALDataset *CreateCopy(const char *pszFilename, GDALDataset *pSrcDs,
                                   int bStrict, char **papszParamList,
                                   GDALProgressFunc pfnProgress,
                                   void *pProgressData);

    // virtual methods for dealing with transform and projection
    CPLErr GetGeoTransform(GDALGeoTransform &gt) const override;
    const OGRSpatialReference *GetSpatialRef() const override;

    CPLErr SetGeoTransform(const GDALGeoTransform &gt) override;
    CPLErr SetSpatialRef(const OGRSpatialReference *poSRS) override;

    // method to get a pointer to the imageio class
    void *GetInternalHandle(const char *) override;

    // virtual methods for dealing with metadata
    CPLErr SetMetadataItem(const char *pszName, const char *pszValue,
                           const char *pszDomain = "") override;
    const char *GetMetadataItem(const char *pszName,
                                const char *pszDomain = "") override;

    char **GetMetadata(const char *pszDomain = "") override;
    CPLErr SetMetadata(char **papszMetadata,
                       const char *pszDomain = "") override;

    // virtual method for adding new image bands
    CPLErr AddBand(GDALDataType eType, char **papszOptions = nullptr) override;

    // GCPs
    int GetGCPCount() override;
    const OGRSpatialReference *GetGCPSpatialRef() const override;
    const GDAL_GCP *GetGCPs() override;
    CPLErr SetGCPs(int nGCPCount, const GDAL_GCP *pasGCPList,
                   const OGRSpatialReference *poSRS) override;

  protected:
    // this method builds overviews for the specified bands.
    virtual CPLErr IBuildOverviews(const char *pszResampling, int nOverviews,
                                   const int *panOverviewList, int nListBands,
                                   const int *panBandList,
                                   GDALProgressFunc pfnProgress,
                                   void *pProgressData,
                                   CSLConstList papszOptions) override;

    // internal method to update m_papszMetadataList
    void UpdateMetadataList();

    void DestroyGCPs();

  private:
    // pointer to KEAImageIO class and the refcount for it
    kealib::KEAImageIO *m_pImageIO;
    LockedRefCount *m_pRefcount;
    char **m_papszMetadataList;  // CSLStringList for metadata
    GDAL_GCP *m_pGCPs;
    mutable OGRSpatialReference m_oGCPSRS{};
    mutable CPLMutex *m_hMutex;
    mutable OGRSpatialReference m_oSRS{};
};

// conversion functions
GDALDataType KEA_to_GDAL_Type(kealib::KEADataType ekeaType);
kealib::KEADataType GDAL_to_KEA_Type(GDALDataType egdalType);

// For unloading the VFL
void KEADatasetDriverUnload(GDALDriver *);

// A thresafe reference count. Used to manage shared pointer to
// the kealib::KEAImageIO instance between bands and dataset.
class LockedRefCount
{
  private:
    int m_nRefCount;
    CPLMutex *m_hMutex;

    CPL_DISALLOW_COPY_ASSIGN(LockedRefCount)

  public:
    explicit LockedRefCount(int initCount = 1)
    {
        m_nRefCount = initCount;
        m_hMutex = CPLCreateMutex();
        CPLReleaseMutex(m_hMutex);
    }

    ~LockedRefCount()
    {
        CPLDestroyMutex(m_hMutex);
        m_hMutex = nullptr;
    }

    void IncRef()
    {
        CPLMutexHolderD(&m_hMutex);
        m_nRefCount++;
    }

    // returns true if reference count now 0
    bool DecRef()
    {
        CPLMutexHolderD(&m_hMutex);
        m_nRefCount--;
        return m_nRefCount <= 0;
    }
};

#endif  // KEADATASET_H
