/******************************************************************************
 *
 * Project:  WCS Client Driver
 * Purpose:  Implementation of Dataset class for WCS.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2006, Frank Warmerdam
 * Copyright (c) 2008-2013, Even Rouault <even dot rouault at spatialys.com>
 * Copyright (c) 2017, Ari Jolma
 * Copyright (c) 2017, Finnish Environment Institute
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

/************************************************************************/
/* ==================================================================== */
/*                              WCSDataset                              */
/* ==================================================================== */
/************************************************************************/

#include "cpl_string.h"
#include "gdal_pam.h"

class WCSRasterBand;

class WCSDataset CPL_NON_FINAL: public GDALPamDataset
{
  friend class WCSRasterBand;
  friend class WCSDataset100;
  friend class WCSDataset110;
  friend class WCSDataset201;

    CPLString   m_cache_dir;
    bool        bServiceDirty;
    CPLXMLNode *psService;

    char       *apszCoverageOfferingMD[2];

    char      **papszSDSModifiers;

    int         m_Version; // eg 100 for 1.0.0, 110 for 1.1.0
    const char *Version() const;

    CPLString   osCRS; // name of the CRS
    char        *pszProjection; // (usually the) WKT of the CRS, from OGRSpatialReference.exportToWkt
    bool        native_crs; // the CRS is the native CRS of the server
    bool        axis_order_swap; // the CRS requires x and y coordinates to be swapped for requests
    double      adfGeoTransform[6];
    bool        SetCRS(const CPLString &crs, bool native);
    void        SetGeometry(const std::vector<int> &size,
                            const std::vector<double> &origin,
                            const std::vector<std::vector<double> > &offsets);

    CPLString   osBandIdentifier;

    CPLString   osDefaultTime;
    std::vector<CPLString> aosTimePositions;

    int         TestUseBlockIO( int, int, int, int, int, int ) const;
    CPLErr      DirectRasterIO( GDALRWFlag, int, int, int, int,
                                void *, int, int, GDALDataType,
                                int, int *,
                                GSpacing nPixelSpace, GSpacing nLineSpace,
                                GSpacing nBandSpace,
                                GDALRasterIOExtraArg* psExtraArg);

    virtual CPLErr IRasterIO( GDALRWFlag, int, int, int, int,
                              void *, int, int, GDALDataType,
                              int, int *,
                              GSpacing nPixelSpace, GSpacing nLineSpace,
                              GSpacing nBandSpace,
                              GDALRasterIOExtraArg* psExtraArg) override;

    virtual std::vector<double> GetExtent(int nXOff, int nYOff,
                                          int nXSize, int nYSize,
                                          int nBufXSize, int nBufYSize) = 0;

    virtual CPLString   GetCoverageRequest( bool scaled,
                                            int nBufXSize, int nBufYSize,
                                            const std::vector<double> &extent,
                                            CPLString osBandList) = 0;

    CPLErr      GetCoverage( int nXOff, int nYOff,
                             int nXSize, int nYSize,
                             int nBufXSize, int nBufYSize,
                             int nBandCount, int *panBandList,
                             GDALRasterIOExtraArg *psExtraArg,
                             CPLHTTPResult **ppsResult );

    virtual CPLString   DescribeCoverageRequest() {return "";}
    virtual CPLXMLNode *CoverageOffering(CPLXMLNode *psDC) = 0;

    int         DescribeCoverage();

    virtual bool        ExtractGridInfo() = 0;

    int         EstablishRasterDetails();

    virtual CPLErr      ParseCapabilities( CPLXMLNode *, CPLString ) = 0;
    virtual void        ParseCoverageCapabilities(CPLXMLNode *, const CPLString&, CPLXMLNode *) = 0;

    GDALDataset *GDALOpenResult( CPLHTTPResult *psResult );

    void        FlushMemoryResult();

    CPLString   osResultFilename;

    GByte      *pabySavedDataBuffer;

    char      **papszHttpOptions;

    int         nMaxCols;
    int         nMaxRows;

  public:
    WCSDataset(int version, const char *cache_dir);
    virtual ~WCSDataset();

    static WCSDataset *CreateFromMetadata( const CPLString&, CPLString );
    static WCSDataset *CreateFromCapabilities(CPLString, CPLString, CPLString );

    static GDALDataset *Open( GDALOpenInfo * );
    static int Identify( GDALOpenInfo * );

    virtual CPLErr GetGeoTransform( double * ) override;
    virtual const char *_GetProjectionRef(void) override;
    const OGRSpatialReference* GetSpatialRef() const override {
        return GetSpatialRefFromOldGetProjectionRef();
    }
    virtual char **GetFileList(void) override;

    virtual char      **GetMetadataDomainList() override;
    virtual char **GetMetadata( const char *pszDomain ) override;
};

class WCSDataset100 final: public WCSDataset
{
    std::vector<double> GetExtent(int nXOff, int nYOff,
                                  int nXSize, int nYSize,
                                  int nBufXSize, int nBufYSize) override;
    CPLString   GetCoverageRequest( bool scaled,
                                    int nBufXSize, int nBufYSize,
                                    const std::vector<double> &extent,
                                    CPLString osBandList) override;
    CPLString   DescribeCoverageRequest() override;
    CPLXMLNode *CoverageOffering(CPLXMLNode *psDC) override;
    bool        ExtractGridInfo() override;
    CPLErr      ParseCapabilities( CPLXMLNode *, CPLString ) override;
    void        ParseCoverageCapabilities(CPLXMLNode *, const CPLString&, CPLXMLNode *) override;

  public:

    explicit WCSDataset100(const char *cache_dir) : WCSDataset(100, cache_dir) {}
};

class WCSDataset110 CPL_NON_FINAL: public WCSDataset
{
    std::vector<double> GetExtent(int nXOff, int nYOff,
                                  int nXSize, int nYSize,
                                  int nBufXSize, int nBufYSize) override;
    CPLString   GetCoverageRequest( bool scaled,
                                    int nBufXSize, int nBufYSize,
                                    const std::vector<double> &extent,
                                    CPLString osBandList) override;
    CPLString   DescribeCoverageRequest() override;
    CPLXMLNode *CoverageOffering(CPLXMLNode *psDC) override;
    bool        ExtractGridInfo() override;
    CPLErr      ParseCapabilities( CPLXMLNode *, CPLString ) override;
    void        ParseCoverageCapabilities(CPLXMLNode *, const CPLString&, CPLXMLNode *) override;

  public:

    WCSDataset110(int version, const char *cache_dir) : WCSDataset(version, cache_dir) {}
};

class WCSDataset201 final: public WCSDataset110
{
    std::vector<double> GetExtent(int nXOff, int nYOff,
                                  int nXSize, int nYSize,
                                  int nBufXSize, int nBufYSize) override;
    CPLString   GetSubdataset(const CPLString &coverage);
    bool        SetFormat(CPLXMLNode *coverage);
    static bool ParseGridFunction(CPLXMLNode *coverage, std::vector<int> &axisOrder);
    int         ParseRange(CPLXMLNode *coverage, const CPLString &range_subset, char ***metadata);
    CPLString   GetCoverageRequest( bool scaled,
                                    int nBufXSize, int nBufYSize,
                                    const std::vector<double> &extent,
                                    CPLString osBandList) override;
    CPLString   DescribeCoverageRequest() override;
    bool        GridOffsets(CPLXMLNode *grid,
                            CPLString subtype,
                            bool swap_grid_axis,
                            std::vector<double> &origin,
                            std::vector<std::vector<double> > &offset,
                            std::vector<CPLString> labels,
                            char ***metadata);
    bool        Offset2GeoTransform(std::vector<double> origin,
                                    std::vector<std::vector<double> > offset);
    bool        ExtractGridInfo() override;

 public:

    explicit WCSDataset201(const char *cache_dir) : WCSDataset110(201, cache_dir) {}

};

#define DIGIT_ZERO '0'

// The WCS URL parameters that can be set 
// - through options to the service file
// - to the URL
// These are also inherited from template service file.
// Fundamental URL parameters (service, version, request, coverage)
// and parameters that require more work from the driver's part, such
// as subsetting parameters (subset, rangesubset) are not in this
// list.

#define WCS_URL_PARAMETERS "Format", "Interpolation", "MediaType",      \
        "UpdateSequence", "GEOTIFF:COMPRESSION", "GEOTIFF:JPEG_QUALITY", \
        "GEOTIFF:PREDICTOR", "GEOTIFF:INTERLEAVE", "GEOTIFF:TILING",    \
        "GEOTIFF:TILEWIDTH"
