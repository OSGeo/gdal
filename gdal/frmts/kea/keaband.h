/*
 * $Id$
 *  keaband.h
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

#ifndef KEABAND_H
#define KEABAND_H

#include "gdal_pam.h"
#include "keadataset.h"

class KEAOverview;
class KEAMaskBand;

// Provides the implementation of a GDAL raster band
class KEARasterBand CPL_NON_FINAL: public GDALPamRasterBand
{
private:
    LockedRefCount      *m_pRefCount = nullptr; // reference count of m_pImageIO

    int                  m_nOverviews = 0; // number of overviews
    KEAOverview        **m_panOverviewBands = nullptr; // array of overview objects
    GDALRasterBand      *m_pMaskBand = nullptr;   // pointer to mask band if one exists (and been requested)
    bool                 m_bMaskBandOwned = false; // do we delete it or not?

    GDALRasterAttributeTable  *m_pAttributeTable = nullptr; // pointer to the attribute table
                                                 // created on first call to GetDefaultRAT()
    GDALColorTable      *m_pColorTable = nullptr;     // pointer to the color table
                                            // created on first call to GetColorTable()

    int                  m_nAttributeChunkSize = 0; // for reporting via the metadata
public:
    // constructor/destructor
    KEARasterBand( KEADataset *pDataset, int nSrcBand, GDALAccess eAccess, kealib::KEAImageIO *pImageIO, LockedRefCount *pRefCount );
    ~KEARasterBand();

    // virtual methods for overview support
    int GetOverviewCount() override;
    GDALRasterBand* GetOverview(int nOverview) override;

    // virtual methods for band names (aka description)
    void SetDescription(const char *) override;

    // virtual methods for handling the metadata
    CPLErr SetMetadataItem (const char *pszName, const char *pszValue, const char *pszDomain="") override;
    const char *GetMetadataItem (const char *pszName, const char *pszDomain="") override;
    char **GetMetadata(const char *pszDomain="") override;
    CPLErr SetMetadata(char **papszMetadata, const char *pszDomain="") override;

    // virtual methods for the no data value
    double GetNoDataValue(int *pbSuccess=nullptr) override;
    CPLErr SetNoDataValue(double dfNoData) override;
    virtual CPLErr DeleteNoDataValue() override;

    // histogram methods
    CPLErr GetDefaultHistogram( double *pdfMin, double *pdfMax,
                                        int *pnBuckets, GUIntBig ** ppanHistogram,
                                        int bForce,
                                        GDALProgressFunc, void *pProgressData) override;
    CPLErr SetDefaultHistogram( double dfMin, double dfMax,
                                        int nBuckets, GUIntBig *panHistogram ) override;

    // virtual methods for RATs
    GDALRasterAttributeTable *GetDefaultRAT() override;
    CPLErr SetDefaultRAT(const GDALRasterAttributeTable *poRAT) override;

    // virtual methods for color tables
    GDALColorTable *GetColorTable() override;
    CPLErr SetColorTable(GDALColorTable *poCT) override;

    // virtual methods for color interpretation
    GDALColorInterp GetColorInterpretation() override;
    CPLErr SetColorInterpretation(GDALColorInterp gdalinterp) override;

    // Virtual methods for band masks.
    CPLErr CreateMaskBand(int nFlags) override;
    GDALRasterBand* GetMaskBand() override;
    int GetMaskFlags() override;

    // internal methods for overviews
    void readExistingOverviews();
    void deleteOverviewObjects();
    void CreateOverviews(int nOverviews, int *panOverviewList);
    KEAOverview** GetOverviewList() { return m_panOverviewBands; }

    kealib::KEALayerType getLayerType() const;
    void setLayerType(kealib::KEALayerType eLayerType);

protected:
    // methods for accessing data as blocks
    virtual CPLErr IReadBlock( int, int, void * ) override;
    virtual CPLErr IWriteBlock( int, int, void * ) override;

    // updates m_papszMetadataList
    void UpdateMetadataList();

    // sets/gets the histogram column from a string (for metadata)
    CPLErr SetHistogramFromString(const char *pszString);
    char *GetHistogramAsString();
    // So we can return the histogram as a string from GetMetadataItem
    char *m_pszHistoBinValues = nullptr;

    kealib::KEAImageIO  *m_pImageIO = nullptr; // our image access pointer - refcounted
    char               **m_papszMetadataList = nullptr; // CPLStringList of metadata
    kealib::KEADataType  m_eKEADataType; // data type as KEA enum
    CPLMutex            *m_hMutex;
};

#endif //KEABAND_H
