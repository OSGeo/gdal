/******************************************************************************
 * $Id$
 *
 * Project:  GDAL Rasterlite driver
 * Purpose:  Implement GDAL Rasterlite support using OGR SQLite driver
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 **********************************************************************
 * Copyright (c) 2009-2013, Even Rouault <even dot rouault at spatialys.com>
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

#ifndef RASTERLITE_DATASET_INCLUDED
#define RASTERLITE_DATASET_INCLUDED

#include "gdal_pam.h"
#include "ogr_api.h"

char** RasterliteGetTileDriverOptions(char** papszOptions);

OGRDataSourceH RasterliteOpenSQLiteDB(const char* pszFilename,
                                      GDALAccess eAccess);
CPLString RasterliteGetPixelSizeCond(double dfPixelXSize,
                                     double dfPixelYSize,
                                     const char* pszTablePrefixWithDot = "");
CPLString RasterliteGetSpatialFilterCond(double minx, double miny,
                                         double maxx, double maxy);

class RasterliteBand;

/************************************************************************/
/* ==================================================================== */
/*                              RasterliteDataset                       */
/* ==================================================================== */
/************************************************************************/

class RasterliteDataset final: public GDALPamDataset
{
    friend class RasterliteBand;

  public:
                 RasterliteDataset();
                 RasterliteDataset(RasterliteDataset* poMainDS, int nLevel);

    virtual     ~RasterliteDataset();

    virtual char      **GetMetadataDomainList() override;
    virtual char **GetMetadata( const char *pszDomain ) override;
    virtual const char *GetMetadataItem( const char *pszName,
                                         const char *pszDomain ) override;
    virtual CPLErr GetGeoTransform( double* padfGeoTransform ) override;
    virtual const char* _GetProjectionRef() override;
    const OGRSpatialReference* GetSpatialRef() const override {
        return GetSpatialRefFromOldGetProjectionRef();
    }

    virtual char** GetFileList() override;

    virtual CPLErr IBuildOverviews( const char * pszResampling,
                                    int nOverviews, int * panOverviewList,
                                    int nBands, int * panBandList,
                                    GDALProgressFunc pfnProgress, void * pProgressData ) override;

    static GDALDataset *Open( GDALOpenInfo * );
    static int          Identify( GDALOpenInfo * );

  protected:
    virtual int         CloseDependentDatasets() override;

  private:
    int bMustFree;
    RasterliteDataset* poMainDS;
    int nLevel;

    char** papszMetadata;
    char** papszImageStructure;
    char** papszSubDatasets;

    int nResolutions;
    double *padfXResolutions;
    double *padfYResolutions;
    RasterliteDataset** papoOverviews;
    int nLimitOvrCount;

    int bValidGeoTransform;
    double adfGeoTransform[6];
    char* pszSRS;

    GDALColorTable* poCT;

    CPLString osTableName;
    CPLString osFileName;

    int bCheckForExistingOverview;
    CPLString osOvrFileName;

    OGRDataSourceH hDS;

    int m_nLastBadTileId = -1;

    void AddSubDataset( const char* pszDSName);
    int  GetBlockParams(OGRLayerH hRasterLyr, int nLevel, int* pnBands,
                        GDALDataType* peDataType,
                        int* pnBlockXSize, int* pnBlockYSize);
    CPLErr CleanOverviews();
    CPLErr CleanOverviewLevel(int nOvrFactor);
    CPLErr ReloadOverviews();
    CPLErr CreateOverviewLevel(const char * pszResampling,
                               int nOvrFactor,
                               char** papszOptions,
                               GDALProgressFunc pfnProgress,
                               void * pProgressData);
};

/************************************************************************/
/* ==================================================================== */
/*                              RasterliteBand                          */
/* ==================================================================== */
/************************************************************************/

class RasterliteBand final: public GDALPamRasterBand
{
    friend class RasterliteDataset;

  public:
                            RasterliteBand( RasterliteDataset* poDS, int nBand,
                                            GDALDataType eDataType,
                                            int nBlockXSize, int nBlockYSize );

    virtual GDALColorInterp GetColorInterpretation() override;
    virtual GDALColorTable* GetColorTable() override;

    virtual int             GetOverviewCount() override;
    virtual GDALRasterBand* GetOverview(int nLevel) override;

    virtual CPLErr          IReadBlock( int, int, void * ) override;
};

GDALDataset *
RasterliteCreateCopy( const char * pszFilename, GDALDataset *poSrcDS,
                       int bStrict, char ** papszOptions,
                       GDALProgressFunc pfnProgress, void * pProgressData );

CPLErr RasterliteDelete(const char* pszFilename);

#endif // RASTERLITE_DATASET_INCLUDED
