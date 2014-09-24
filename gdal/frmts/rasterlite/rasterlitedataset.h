/******************************************************************************
 * $Id$
 *
 * Project:  GDAL Rasterlite driver
 * Purpose:  Implement GDAL Rasterlite support using OGR SQLite driver
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 **********************************************************************
 * Copyright (c) 2009-2013, Even Rouault <even dot rouault at mines-paris dot org>
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

char** RasterliteGetTileDriverOptions(char** papszOptions);

class RasterliteBand;

/************************************************************************/
/* ==================================================================== */
/*                              RasterliteDataset                       */
/* ==================================================================== */
/************************************************************************/

class RasterliteDataset : public GDALPamDataset
{
    friend class RasterliteBand;
    
  public:
                 RasterliteDataset();
                 RasterliteDataset(RasterliteDataset* poMainDS, int nLevel);
                 
    virtual     ~RasterliteDataset();
    
    virtual char      **GetMetadataDomainList();
    virtual char **GetMetadata( const char *pszDomain );
    virtual const char *GetMetadataItem( const char *pszName, 
                                         const char *pszDomain );
    virtual CPLErr GetGeoTransform( double* padfGeoTransform );
    virtual const char* GetProjectionRef();
    
    virtual char** GetFileList();
    
    virtual CPLErr IBuildOverviews( const char * pszResampling, 
                                    int nOverviews, int * panOverviewList,
                                    int nBands, int * panBandList,
                                    GDALProgressFunc pfnProgress, void * pProgressData );

    static GDALDataset *Open( GDALOpenInfo * );
    static int          Identify( GDALOpenInfo * );

  protected:
    virtual int         CloseDependentDatasets();

  private:
  
    int bMustFree;
    RasterliteDataset* poMainDS;
    int nLevel;
    
    char** papszMetadata;
    char** papszImageStructure;
    char** papszSubDatasets;
    
    int nResolutions;
    double* padfXResolutions, *padfYResolutions;
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

class RasterliteBand: public GDALPamRasterBand
{
    friend class RasterliteDataset;

  public:
                            RasterliteBand( RasterliteDataset* poDS, int nBand,
                                            GDALDataType eDataType,
                                            int nBlockXSize, int nBlockYSize);

    virtual GDALColorInterp GetColorInterpretation();
    virtual GDALColorTable* GetColorTable();

    virtual int             GetOverviewCount();
    virtual GDALRasterBand* GetOverview(int nLevel);

    virtual CPLErr          IReadBlock( int, int, void * );
};

GDALDataset *
RasterliteCreateCopy( const char * pszFilename, GDALDataset *poSrcDS, 
                       int bStrict, char ** papszOptions, 
                       GDALProgressFunc pfnProgress, void * pProgressData );

CPLErr RasterliteDelete(const char* pszFilename);

#endif // RASTERLITE_DATASET_INCLUDED
