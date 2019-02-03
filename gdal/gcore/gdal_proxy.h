/******************************************************************************
 * $Id$
 *
 * Project:  GDAL Core
 * Purpose:  GDAL Core C++/Private declarations
 * Author:   Even Rouault <even dot rouault at mines dash paris dot org>
 *
 ******************************************************************************
 * Copyright (c) 2008-2014, Even Rouault <even dot rouault at mines-paris dot org>
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

#ifndef GDAL_PROXY_H_INCLUDED
#define GDAL_PROXY_H_INCLUDED

#ifndef DOXYGEN_SKIP

#include "gdal.h"

#ifdef __cplusplus

#include "gdal_priv.h"
#include "cpl_hash_set.h"

/* ******************************************************************** */
/*                        GDALProxyDataset                              */
/* ******************************************************************** */

class CPL_DLL GDALProxyDataset : public GDALDataset
{
  protected:
    GDALProxyDataset() {}

    virtual GDALDataset *RefUnderlyingDataset() const = 0;
    virtual void UnrefUnderlyingDataset(GDALDataset* poUnderlyingDataset) const;

    CPLErr IBuildOverviews( const char *, int, int *,
                            int, int *, GDALProgressFunc, void * ) override;
    CPLErr IRasterIO( GDALRWFlag, int, int, int, int,
                      void *, int, int, GDALDataType,
                      int, int *, GSpacing, GSpacing, GSpacing,
                      GDALRasterIOExtraArg* psExtraArg ) override;

  public:
    char **GetMetadataDomainList() override;
    char **GetMetadata( const char * pszDomain  ) override;
    CPLErr SetMetadata( char ** papszMetadata,
                            const char * pszDomain  ) override;
    const char *GetMetadataItem( const char * pszName,
                                const char * pszDomain  ) override;
    CPLErr SetMetadataItem( const char * pszName,
                            const char * pszValue,
                            const char * pszDomain ) override;

    void FlushCache() override;

    const OGRSpatialReference* GetSpatialRef() const override;
    CPLErr SetSpatialRef(const OGRSpatialReference* poSRS) override;

    CPLErr GetGeoTransform( double * ) override;
    CPLErr SetGeoTransform( double * ) override;

    void *GetInternalHandle( const char * ) override;
    GDALDriver *GetDriver() override;
    char **GetFileList() override;

    int GetGCPCount() override;
    const OGRSpatialReference* GetGCPSpatialRef() const override;
    const GDAL_GCP *GetGCPs() override;
    CPLErr SetGCPs( int nGCPCount, const GDAL_GCP *pasGCPList,
                    const OGRSpatialReference * poGCP_SRS ) override;

    CPLErr AdviseRead( int nXOff, int nYOff, int nXSize, int nYSize,
                       int nBufXSize, int nBufYSize,
                       GDALDataType eDT,
                       int nBandCount, int *panBandList,
                       char **papszOptions ) override;

    CPLErr          CreateMaskBand( int nFlags ) override;

  protected:
    const char *_GetProjectionRef(void) override;
    CPLErr _SetProjection( const char * ) override;
    const char *_GetGCPProjection() override;
    CPLErr _SetGCPs( int nGCPCount, const GDAL_GCP *pasGCPList,
                    const char *pszGCPProjection ) override;

  private:
    CPL_DISALLOW_COPY_ASSIGN(GDALProxyDataset)
};

/* ******************************************************************** */
/*                         GDALProxyRasterBand                          */
/* ******************************************************************** */

class CPL_DLL GDALProxyRasterBand : public GDALRasterBand
{
  protected:
    GDALProxyRasterBand() {}

    virtual GDALRasterBand* RefUnderlyingRasterBand() = 0;
    virtual void UnrefUnderlyingRasterBand(GDALRasterBand* poUnderlyingRasterBand);

    CPLErr IReadBlock( int, int, void * ) override;
    CPLErr IWriteBlock( int, int, void * ) override;
    CPLErr IRasterIO( GDALRWFlag, int, int, int, int,
                    void *, int, int, GDALDataType,
                    GSpacing, GSpacing, GDALRasterIOExtraArg* psExtraArg ) override;

  public:
    char **GetMetadataDomainList() override;
    char **GetMetadata( const char * pszDomain  ) override;
    CPLErr SetMetadata( char ** papszMetadata,
                        const char * pszDomain  ) override;
    const char *GetMetadataItem( const char * pszName,
                                const char * pszDomain  ) override;
    CPLErr SetMetadataItem( const char * pszName,
                            const char * pszValue,
                            const char * pszDomain ) override;
    CPLErr FlushCache() override;
    char **GetCategoryNames() override;
    double GetNoDataValue( int *pbSuccess = nullptr ) override;
    double GetMinimum( int *pbSuccess = nullptr ) override;
    double GetMaximum(int *pbSuccess = nullptr ) override;
    double GetOffset( int *pbSuccess = nullptr ) override;
    double GetScale( int *pbSuccess = nullptr ) override;
    const char *GetUnitType() override;
    GDALColorInterp GetColorInterpretation() override;
    GDALColorTable *GetColorTable() override;
    CPLErr Fill(double dfRealValue, double dfImaginaryValue = 0) override;

    CPLErr SetCategoryNames( char ** ) override;
    CPLErr SetNoDataValue( double ) override;
    CPLErr DeleteNoDataValue() override;
    CPLErr SetColorTable( GDALColorTable * ) override;
    CPLErr SetColorInterpretation( GDALColorInterp ) override;
    CPLErr SetOffset( double ) override;
    CPLErr SetScale( double ) override;
    CPLErr SetUnitType( const char * ) override;

    CPLErr GetStatistics( int bApproxOK, int bForce,
                          double *pdfMin, double *pdfMax,
                          double *pdfMean, double *padfStdDev ) override;
    CPLErr ComputeStatistics( int bApproxOK,
                              double *pdfMin, double *pdfMax,
                              double *pdfMean, double *pdfStdDev,
                              GDALProgressFunc, void *pProgressData ) override;
    CPLErr SetStatistics( double dfMin, double dfMax,
                          double dfMean, double dfStdDev ) override;
    CPLErr ComputeRasterMinMax( int, double* ) override;

    int HasArbitraryOverviews() override;
    int GetOverviewCount() override;
    GDALRasterBand *GetOverview( int ) override;
    GDALRasterBand *GetRasterSampleOverview( GUIntBig ) override;
    CPLErr BuildOverviews( const char *, int, int *,
                           GDALProgressFunc, void * ) override;

    CPLErr AdviseRead( int nXOff, int nYOff, int nXSize, int nYSize,
                       int nBufXSize, int nBufYSize,
                       GDALDataType eDT, char **papszOptions ) override;

    CPLErr  GetHistogram( double dfMin, double dfMax,
                          int nBuckets, GUIntBig * panHistogram,
                          int bIncludeOutOfRange, int bApproxOK,
                          GDALProgressFunc, void *pProgressData ) override;

    CPLErr GetDefaultHistogram( double *pdfMin, double *pdfMax,
                                int *pnBuckets, GUIntBig ** ppanHistogram,
                                int bForce,
                                GDALProgressFunc, void *pProgressData) override;
    CPLErr SetDefaultHistogram( double dfMin, double dfMax,
                                int nBuckets, GUIntBig *panHistogram ) override;

    GDALRasterAttributeTable *GetDefaultRAT() override;
    CPLErr SetDefaultRAT( const GDALRasterAttributeTable * ) override;

    GDALRasterBand *GetMaskBand() override;
    int GetMaskFlags() override;
    CPLErr CreateMaskBand( int nFlags ) override;

    CPLVirtualMem  *GetVirtualMemAuto( GDALRWFlag eRWFlag,
                                       int *pnPixelSpace,
                                       GIntBig *pnLineSpace,
                                       char **papszOptions ) override;

  private:
    CPL_DISALLOW_COPY_ASSIGN(GDALProxyRasterBand)
};

/* ******************************************************************** */
/*                     GDALProxyPoolDataset                             */
/* ******************************************************************** */

typedef struct _GDALProxyPoolCacheEntry GDALProxyPoolCacheEntry;
class     GDALProxyPoolRasterBand;

class CPL_DLL GDALProxyPoolDataset : public GDALProxyDataset
{
  private:
        GIntBig          responsiblePID = -1;

        mutable char            *pszProjectionRef = nullptr;
        mutable OGRSpatialReference* m_poSRS = nullptr;
        mutable OGRSpatialReference* m_poGCPSRS = nullptr;
        double           adfGeoTransform[6]{0,1,0,0,0,1};
        bool             bHasSrcProjection = false;
        bool             m_bHasSrcSRS = false;
        bool             bHasSrcGeoTransform = false;
        char            *pszGCPProjection = nullptr;
        int              nGCPCount = 0;
        GDAL_GCP        *pasGCPList = nullptr;
        CPLHashSet      *metadataSet = nullptr;
        CPLHashSet      *metadataItemSet = nullptr;

        mutable GDALProxyPoolCacheEntry* cacheEntry = nullptr;
        char            *m_pszOwner = nullptr;

        GDALDataset *RefUnderlyingDataset(bool bForceOpen) const;

  protected:
    GDALDataset *RefUnderlyingDataset() const override;
    void UnrefUnderlyingDataset(GDALDataset* poUnderlyingDataset) const override;

    friend class     GDALProxyPoolRasterBand;

  public:
    GDALProxyPoolDataset( const char* pszSourceDatasetDescription,
                          int nRasterXSize, int nRasterYSize,
                          GDALAccess eAccess = GA_ReadOnly,
                          int bShared = FALSE,
                          const char * pszProjectionRef = nullptr,
                          double * padfGeoTransform = nullptr,
                          const char* pszOwner = nullptr );
    ~GDALProxyPoolDataset() override;

    void SetOpenOptions( char** papszOpenOptions );
    void AddSrcBandDescription( GDALDataType eDataType, int nBlockXSize,
                                int nBlockYSize );

    // Used by VRT SimpleSource to add a single GDALProxyPoolRasterBand while
    // keeping all other bands initialized to a nullptr. This is under the assumption,
    // VRT SimpleSource will not have to access any other bands than the one added.
    void AddSrcBand(int nBand, GDALDataType eDataType, int nBlockXSize,
                                int nBlockYSize );
    void FlushCache() override;

    const OGRSpatialReference* GetSpatialRef() const override;
    CPLErr SetSpatialRef(const OGRSpatialReference* poSRS) override;

    const char *_GetProjectionRef() override;
    CPLErr _SetProjection( const char * ) override;

    CPLErr GetGeoTransform( double * ) override;
    CPLErr SetGeoTransform( double * ) override;

    // Special behaviour for the following methods : they return a pointer
    // data type, that must be cached by the proxy, so it doesn't become invalid
    // when the underlying object get closed.
    char **GetMetadata( const char * pszDomain  ) override;
    const char *GetMetadataItem( const char * pszName,
                                 const char * pszDomain  ) override;

    void *GetInternalHandle( const char * pszRequest ) override;

    const char *_GetGCPProjection() override;
    const OGRSpatialReference* GetGCPSpatialRef() const override;
    const GDAL_GCP *GetGCPs() override;

  private:
    CPL_DISALLOW_COPY_ASSIGN(GDALProxyPoolDataset)
};

/* ******************************************************************** */
/*                  GDALProxyPoolRasterBand                             */
/* ******************************************************************** */

class GDALProxyPoolOverviewRasterBand;
class GDALProxyPoolMaskBand;

class CPL_DLL GDALProxyPoolRasterBand : public GDALProxyRasterBand
{
  private:
    CPLHashSet      *metadataSet = nullptr;
    CPLHashSet      *metadataItemSet = nullptr;
    char            *pszUnitType = nullptr;
    char           **papszCategoryNames = nullptr;
    GDALColorTable  *poColorTable = nullptr;

    int                               nSizeProxyOverviewRasterBand = 0;
    GDALProxyPoolOverviewRasterBand **papoProxyOverviewRasterBand = nullptr;
    GDALProxyPoolMaskBand            *poProxyMaskBand = nullptr;

    GDALRasterBand* RefUnderlyingRasterBand( bool bForceOpen );

  protected:
    GDALRasterBand* RefUnderlyingRasterBand() override;
    void UnrefUnderlyingRasterBand( GDALRasterBand* poUnderlyingRasterBand )
        override;

    friend class GDALProxyPoolOverviewRasterBand;
    friend class GDALProxyPoolMaskBand;

  public:
    GDALProxyPoolRasterBand( GDALProxyPoolDataset* poDS, int nBand,
                             GDALDataType eDataType,
                             int nBlockXSize, int nBlockYSize );
    GDALProxyPoolRasterBand( GDALProxyPoolDataset* poDS,
                             GDALRasterBand* poUnderlyingRasterBand );
    ~GDALProxyPoolRasterBand() override;

    void AddSrcMaskBandDescription( GDALDataType eDataType, int nBlockXSize,
                                    int nBlockYSize );

    // Special behaviour for the following methods : they return a pointer
    // data type, that must be cached by the proxy, so it doesn't become invalid
    // when the underlying object get closed.
    char **GetMetadata( const char * pszDomain ) override;
    const char *GetMetadataItem( const char * pszName,
                                 const char * pszDomain ) override;
    char **GetCategoryNames() override;
    const char *GetUnitType() override;
    GDALColorTable *GetColorTable() override;
    GDALRasterBand *GetOverview( int ) override;
    GDALRasterBand *GetRasterSampleOverview( GUIntBig nDesiredSamples ) override; // TODO
    GDALRasterBand *GetMaskBand() override;

    CPLErr FlushCache() override;

  private:
    CPL_DISALLOW_COPY_ASSIGN(GDALProxyPoolRasterBand)
};

/* ******************************************************************** */
/*                  GDALProxyPoolOverviewRasterBand                     */
/* ******************************************************************** */

class GDALProxyPoolOverviewRasterBand : public GDALProxyPoolRasterBand
{
  private:
    GDALProxyPoolRasterBand *poMainBand = nullptr;
    int                      nOverviewBand = 0;

    GDALRasterBand          *poUnderlyingMainRasterBand = nullptr;
    int                      nRefCountUnderlyingMainRasterBand = 0;

    CPL_DISALLOW_COPY_ASSIGN(GDALProxyPoolOverviewRasterBand)

  protected:
    GDALRasterBand* RefUnderlyingRasterBand() override;
    void UnrefUnderlyingRasterBand( GDALRasterBand* poUnderlyingRasterBand )
        override;

  public:
    GDALProxyPoolOverviewRasterBand( GDALProxyPoolDataset* poDS,
                                     GDALRasterBand* poUnderlyingOverviewBand,
                                     GDALProxyPoolRasterBand* poMainBand,
                                     int nOverviewBand );
    ~GDALProxyPoolOverviewRasterBand() override;
};

/* ******************************************************************** */
/*                      GDALProxyPoolMaskBand                           */
/* ******************************************************************** */

class GDALProxyPoolMaskBand : public GDALProxyPoolRasterBand
{
  private:
    GDALProxyPoolRasterBand *poMainBand = nullptr;

    GDALRasterBand          *poUnderlyingMainRasterBand = nullptr;
    int                      nRefCountUnderlyingMainRasterBand = 0;

    CPL_DISALLOW_COPY_ASSIGN(GDALProxyPoolMaskBand)

  protected:
    GDALRasterBand* RefUnderlyingRasterBand() override;
    void UnrefUnderlyingRasterBand( GDALRasterBand* poUnderlyingRasterBand )
        override;

  public:
    GDALProxyPoolMaskBand( GDALProxyPoolDataset* poDS,
                           GDALRasterBand* poUnderlyingMaskBand,
                           GDALProxyPoolRasterBand* poMainBand );
    GDALProxyPoolMaskBand( GDALProxyPoolDataset* poDS,
                           GDALProxyPoolRasterBand* poMainBand,
                           GDALDataType eDataType,
                           int nBlockXSize, int nBlockYSize );
    ~GDALProxyPoolMaskBand() override;
};

#endif

/* ******************************************************************** */
/*            C types and methods declarations                          */
/* ******************************************************************** */

CPL_C_START

typedef struct GDALProxyPoolDatasetHS *GDALProxyPoolDatasetH;

GDALProxyPoolDatasetH CPL_DLL GDALProxyPoolDatasetCreate(const char* pszSourceDatasetDescription,
                                                         int nRasterXSize, int nRasterYSize,
                                                         GDALAccess eAccess, int bShared,
                                                         const char * pszProjectionRef,
                                                         double * padfGeoTransform);

void CPL_DLL GDALProxyPoolDatasetDelete(GDALProxyPoolDatasetH hProxyPoolDataset);

void CPL_DLL GDALProxyPoolDatasetAddSrcBandDescription( GDALProxyPoolDatasetH hProxyPoolDataset,
                                                        GDALDataType eDataType,
                                                        int nBlockXSize, int nBlockYSize);

CPL_C_END

#endif /* #ifndef DOXYGEN_SKIP */

#endif /* GDAL_PROXY_H_INCLUDED */
