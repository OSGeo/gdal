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
#if defined(USE_GCC_VISIBILITY_FLAG) && !defined(DllExport)
#define DllExport CPL_DLL
#endif
#include "keadataset.h"

class KEAOverview;
class KEAMaskBand;

// Provides the implementation of a GDAL raster band
class KEARasterBand : public GDALPamRasterBand
{
private:
    int                 *m_pnRefCount; // reference count of m_pImageIO

    int                  m_nOverviews; // number of overviews
    KEAOverview        **m_panOverviewBands; // array of overview objects
    GDALRasterBand      *m_pMaskBand;   // pointer to mask band if one exists (and been requested)
    bool                 m_bMaskBandOwned; // do we delete it or not?

    GDALRasterAttributeTable  *m_pAttributeTable; // pointer to the attribute table
                                                 // created on first call to GetDefaultRAT()
    GDALColorTable      *m_pColorTable;     // pointer to the color table
                                            // created on first call to GetColorTable()

    int                  m_nAttributeChunkSize; // for reporting via the metadata
public:
    // constructor/destructor
    KEARasterBand( KEADataset *pDataset, int nSrcBand, GDALAccess eAccess, kealib::KEAImageIO *pImageIO, int *pRefCount );
    ~KEARasterBand();

    // virtual methods for overview support
    int GetOverviewCount();
    GDALRasterBand* GetOverview(int nOverview);

    // virtual methods for band names (aka description)
    void SetDescription(const char *);

    // virtual methods for handling the metadata
    CPLErr SetMetadataItem (const char *pszName, const char *pszValue, const char *pszDomain="");
    const char *GetMetadataItem (const char *pszName, const char *pszDomain="");
    char **GetMetadata(const char *pszDomain="");
    CPLErr SetMetadata(char **papszMetadata, const char *pszDomain="");

    // virtual methods for the no data value
    double GetNoDataValue(int *pbSuccess=NULL);
    CPLErr SetNoDataValue(double dfNoData);
    virtual CPLErr DeleteNoDataValue();

    // virtual methods for RATs
    GDALRasterAttributeTable *GetDefaultRAT();
    CPLErr SetDefaultRAT(const GDALRasterAttributeTable *poRAT);

    // virtual methods for color tables
    GDALColorTable *GetColorTable();
    CPLErr SetColorTable(GDALColorTable *poCT);

    // virtual methods for color interpretation
    GDALColorInterp GetColorInterpretation();
    CPLErr SetColorInterpretation(GDALColorInterp gdalinterp);

    // virtual mthods for band masks
    CPLErr CreateMaskBand(int nFlags);
    GDALRasterBand* GetMaskBand();
    int GetMaskFlags();

    // internal methods for overviews
    void readExistingOverviews();
    void deleteOverviewObjects();
    void CreateOverviews(int nOverviews, int *panOverviewList);
    KEAOverview** GetOverviewList() { return m_panOverviewBands; }

protected:
    // methods for accessing data as blocks
    virtual CPLErr IReadBlock( int, int, void * );
    virtual CPLErr IWriteBlock( int, int, void * );

    // updates m_papszMetadataList
    void UpdateMetadataList();

    kealib::KEAImageIO  *m_pImageIO; // our image access pointer - refcounted
    char               **m_papszMetadataList; // CPLStringList of metadata
    kealib::KEADataType  m_eKEADataType; // data type as KEA enum
};


#endif //KEABAND_H
