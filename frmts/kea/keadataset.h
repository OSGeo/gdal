/*
 * $Id$
 *  keadataset.h
 *
 *  Created by Pete Bunting on 01/08/2012.
 *  Copyright 2012 LibKEA. All rights reserved.
 *
 *  This file is part of LibKEA.
 *
 *  Permission is hereby granted, free of charge, to any person
 *  obtaining a copy of this software and associated documentation
 *  files (the "Software"), to deal in the Software without restriction,
 *  including without limitation the rights to use, copy, modify,
 *  merge, publish, distribute, sublicense, and/or sell copies of the
 *  Software, and to permit persons to whom the Software is furnished
 *  to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be
 *  included in all copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 *  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 *  OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 *  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR
 *  ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
 *  CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 *  WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#ifndef KEADATASET_H
#define KEADATASET_H

#include "gdal_pam.h"
#include "cpl_multiproc.h"
#include "libkea_headers.h"

class LockedRefCount;

// class that implements a GDAL dataset
class KEADataset final: public GDALPamDataset
{
    static H5::H5File *CreateLL( const char * pszFilename,
                                  int nXSize, int nYSize, int nBands,
                                  GDALDataType eType,
                                  char ** papszParamList  );

public:
    // constructor/destructor
    KEADataset( H5::H5File *keaImgH5File, GDALAccess eAccess );
    ~KEADataset();

    // static methods that handle open and creation
    // the driver class has pointers to these
    static GDALDataset *Open( GDALOpenInfo * );
    static int Identify( GDALOpenInfo * poOpenInfo );
    static GDALDataset *Create( const char * pszFilename,
                                  int nXSize, int nYSize, int nBands,
                                  GDALDataType eType,
                                  char **  papszParamList );
    static GDALDataset *CreateCopy( const char * pszFilename, GDALDataset *pSrcDs,
                                int bStrict, char **  papszParamList,
                                GDALProgressFunc pfnProgress, void *pProgressData );

    // virtual methods for dealing with transform and projection
    CPLErr      GetGeoTransform( double * padfTransform ) override;
    const char *_GetProjectionRef() override;
    const OGRSpatialReference* GetSpatialRef() const override {
        return GetSpatialRefFromOldGetProjectionRef();
    }

    CPLErr  SetGeoTransform (double *padfTransform ) override;
    CPLErr _SetProjection( const char *pszWKT ) override;
    CPLErr SetSpatialRef(const OGRSpatialReference* poSRS) override {
        return OldSetProjectionFromSetSpatialRef(poSRS);
    }

    // method to get a pointer to the imageio class
    void *GetInternalHandle (const char *) override;

    // virtual methods for dealing with metadata
    CPLErr SetMetadataItem (const char *pszName, const char *pszValue, const char *pszDomain="") override;
    const char *GetMetadataItem (const char *pszName, const char *pszDomain="") override;

    char **GetMetadata(const char *pszDomain="") override;
    CPLErr SetMetadata(char **papszMetadata, const char *pszDomain="") override;

    // virtual method for adding new image bands
    CPLErr AddBand(GDALDataType eType, char **papszOptions = nullptr) override;

    // GCPs
    int GetGCPCount() override;
    const char* _GetGCPProjection() override;
    const OGRSpatialReference* GetGCPSpatialRef() const override {
        return GetGCPSpatialRefFromOldGetGCPProjection();
    }
    const GDAL_GCP* GetGCPs() override;
    CPLErr _SetGCPs(int nGCPCount, const GDAL_GCP *pasGCPList, const char *pszGCPProjection) override;
    using GDALPamDataset::SetGCPs;
    CPLErr SetGCPs( int nGCPCount, const GDAL_GCP *pasGCPList,
                    const OGRSpatialReference* poSRS ) override {
        return OldSetGCPsFromNew(nGCPCount, pasGCPList, poSRS);
    }

protected:
    // this method builds overviews for the specified bands.
    virtual CPLErr IBuildOverviews(const char *pszResampling, int nOverviews, int *panOverviewList,
                                    int nListBands, int *panBandList, GDALProgressFunc pfnProgress,
                                    void *pProgressData) override;

    // internal method to update m_papszMetadataList
    void UpdateMetadataList();

    void DestroyGCPs();

private:
    // pointer to KEAImageIO class and the refcount for it
    kealib::KEAImageIO  *m_pImageIO;
    LockedRefCount      *m_pRefcount;
    char               **m_papszMetadataList; // CSLStringList for metadata
    GDAL_GCP            *m_pGCPs;
    char                *m_pszGCPProjection;
    CPLMutex            *m_hMutex;
};

// conversion functions
GDALDataType KEA_to_GDAL_Type( kealib::KEADataType ekeaType );
kealib::KEADataType GDAL_to_KEA_Type( GDALDataType egdalType );

// For unloading the VFL
void KEADatasetDriverUnload(GDALDriver*);

// A thresafe reference count. Used to manage shared pointer to
// the kealib::KEAImageIO instance between bands and dataset.
class LockedRefCount
{
private:
    int m_nRefCount;
    CPLMutex *m_hMutex;

    CPL_DISALLOW_COPY_ASSIGN(LockedRefCount)

public:
    explicit LockedRefCount(int initCount=1)
    {
        m_nRefCount = initCount;
        m_hMutex = CPLCreateMutex();
        CPLReleaseMutex( m_hMutex );
    }
    ~LockedRefCount()
    {
        CPLDestroyMutex( m_hMutex );
        m_hMutex = nullptr;
    }

    void IncRef()
    {
        CPLMutexHolderD( &m_hMutex );
        m_nRefCount++;
    }

    // returns true if reference count now 0
    bool DecRef()
    {
        CPLMutexHolderD( &m_hMutex );
        m_nRefCount--;
        return m_nRefCount <= 0;
    }
};

#endif //KEADATASET_H
