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

        virtual GDALDataset *RefUnderlyingDataset() = 0;
        virtual void UnrefUnderlyingDataset(GDALDataset* poUnderlyingDataset);

        virtual CPLErr IBuildOverviews( const char *, int, int *,
                                    int, int *, GDALProgressFunc, void * ) CPL_OVERRIDE;
        virtual CPLErr IRasterIO( GDALRWFlag, int, int, int, int,
                                void *, int, int, GDALDataType,
                                int, int *, GSpacing, GSpacing, GSpacing,
                                GDALRasterIOExtraArg* psExtraArg ) CPL_OVERRIDE;
    public:

        virtual char      **GetMetadataDomainList() CPL_OVERRIDE;
        virtual char      **GetMetadata( const char * pszDomain  ) CPL_OVERRIDE;
        virtual CPLErr      SetMetadata( char ** papszMetadata,
                                        const char * pszDomain  ) CPL_OVERRIDE;
        virtual const char *GetMetadataItem( const char * pszName,
                                            const char * pszDomain  ) CPL_OVERRIDE;
        virtual CPLErr      SetMetadataItem( const char * pszName,
                                            const char * pszValue,
                                            const char * pszDomain ) CPL_OVERRIDE;

        virtual void FlushCache(void) CPL_OVERRIDE;

        virtual const char *GetProjectionRef(void) CPL_OVERRIDE;
        virtual CPLErr SetProjection( const char * ) CPL_OVERRIDE;

        virtual CPLErr GetGeoTransform( double * ) CPL_OVERRIDE;
        virtual CPLErr SetGeoTransform( double * ) CPL_OVERRIDE;

        virtual void *GetInternalHandle( const char * ) CPL_OVERRIDE;
        virtual GDALDriver *GetDriver(void) CPL_OVERRIDE;
        virtual char      **GetFileList(void) CPL_OVERRIDE;

        virtual int    GetGCPCount() CPL_OVERRIDE;
        virtual const char *GetGCPProjection() CPL_OVERRIDE;
        virtual const GDAL_GCP *GetGCPs() CPL_OVERRIDE;
        virtual CPLErr SetGCPs( int nGCPCount, const GDAL_GCP *pasGCPList,
                                const char *pszGCPProjection ) CPL_OVERRIDE;

        virtual CPLErr AdviseRead( int nXOff, int nYOff, int nXSize, int nYSize,
                                int nBufXSize, int nBufYSize,
                                GDALDataType eDT,
                                int nBandCount, int *panBandList,
                                char **papszOptions ) CPL_OVERRIDE;

        virtual CPLErr          CreateMaskBand( int nFlags ) CPL_OVERRIDE;

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

        virtual CPLErr IReadBlock( int, int, void * ) CPL_OVERRIDE;
        virtual CPLErr IWriteBlock( int, int, void * ) CPL_OVERRIDE;
        virtual CPLErr IRasterIO( GDALRWFlag, int, int, int, int,
                                void *, int, int, GDALDataType,
                                GSpacing, GSpacing, GDALRasterIOExtraArg* psExtraArg ) CPL_OVERRIDE;

    public:

        virtual char      **GetMetadataDomainList() CPL_OVERRIDE;
        virtual char      **GetMetadata( const char * pszDomain  ) CPL_OVERRIDE;
        virtual CPLErr      SetMetadata( char ** papszMetadata,
                                        const char * pszDomain  ) CPL_OVERRIDE;
        virtual const char *GetMetadataItem( const char * pszName,
                                            const char * pszDomain  ) CPL_OVERRIDE;
        virtual CPLErr      SetMetadataItem( const char * pszName,
                                            const char * pszValue,
                                            const char * pszDomain ) CPL_OVERRIDE;
        virtual CPLErr FlushCache() CPL_OVERRIDE;
        virtual char **GetCategoryNames() CPL_OVERRIDE;
        virtual double GetNoDataValue( int *pbSuccess = NULL ) CPL_OVERRIDE;
        virtual double GetMinimum( int *pbSuccess = NULL ) CPL_OVERRIDE;
        virtual double GetMaximum(int *pbSuccess = NULL ) CPL_OVERRIDE;
        virtual double GetOffset( int *pbSuccess = NULL ) CPL_OVERRIDE;
        virtual double GetScale( int *pbSuccess = NULL ) CPL_OVERRIDE;
        virtual const char *GetUnitType() CPL_OVERRIDE;
        virtual GDALColorInterp GetColorInterpretation() CPL_OVERRIDE;
        virtual GDALColorTable *GetColorTable() CPL_OVERRIDE;
        virtual CPLErr Fill(double dfRealValue, double dfImaginaryValue = 0) CPL_OVERRIDE;

        virtual CPLErr SetCategoryNames( char ** ) CPL_OVERRIDE;
        virtual CPLErr SetNoDataValue( double ) CPL_OVERRIDE;
        virtual CPLErr DeleteNoDataValue() CPL_OVERRIDE;
        virtual CPLErr SetColorTable( GDALColorTable * ) CPL_OVERRIDE;
        virtual CPLErr SetColorInterpretation( GDALColorInterp ) CPL_OVERRIDE;
        virtual CPLErr SetOffset( double ) CPL_OVERRIDE;
        virtual CPLErr SetScale( double ) CPL_OVERRIDE;
        virtual CPLErr SetUnitType( const char * ) CPL_OVERRIDE;

        virtual CPLErr GetStatistics( int bApproxOK, int bForce,
                                    double *pdfMin, double *pdfMax,
                                    double *pdfMean, double *padfStdDev ) CPL_OVERRIDE;
        virtual CPLErr ComputeStatistics( int bApproxOK,
                                        double *pdfMin, double *pdfMax,
                                        double *pdfMean, double *pdfStdDev,
                                        GDALProgressFunc, void *pProgressData ) CPL_OVERRIDE;
        virtual CPLErr SetStatistics( double dfMin, double dfMax,
                                    double dfMean, double dfStdDev ) CPL_OVERRIDE;
        virtual CPLErr ComputeRasterMinMax( int, double* ) CPL_OVERRIDE;

        virtual int HasArbitraryOverviews() CPL_OVERRIDE;
        virtual int GetOverviewCount() CPL_OVERRIDE;
        virtual GDALRasterBand *GetOverview(int) CPL_OVERRIDE;
        virtual GDALRasterBand *GetRasterSampleOverview( GUIntBig ) CPL_OVERRIDE;
        virtual CPLErr BuildOverviews( const char *, int, int *,
                                    GDALProgressFunc, void * ) CPL_OVERRIDE;

        virtual CPLErr AdviseRead( int nXOff, int nYOff, int nXSize, int nYSize,
                                int nBufXSize, int nBufYSize,
                                GDALDataType eDT, char **papszOptions ) CPL_OVERRIDE;

        virtual CPLErr  GetHistogram( double dfMin, double dfMax,
                            int nBuckets, GUIntBig * panHistogram,
                            int bIncludeOutOfRange, int bApproxOK,
                            GDALProgressFunc, void *pProgressData ) CPL_OVERRIDE;

        virtual CPLErr GetDefaultHistogram( double *pdfMin, double *pdfMax,
                                            int *pnBuckets, GUIntBig ** ppanHistogram,
                                            int bForce,
                                            GDALProgressFunc, void *pProgressData) CPL_OVERRIDE;
        virtual CPLErr SetDefaultHistogram( double dfMin, double dfMax,
                                            int nBuckets, GUIntBig *panHistogram ) CPL_OVERRIDE;

        virtual GDALRasterAttributeTable *GetDefaultRAT() CPL_OVERRIDE;
        virtual CPLErr SetDefaultRAT( const GDALRasterAttributeTable * ) CPL_OVERRIDE;

        virtual GDALRasterBand *GetMaskBand() CPL_OVERRIDE;
        virtual int             GetMaskFlags() CPL_OVERRIDE;
        virtual CPLErr          CreateMaskBand( int nFlags ) CPL_OVERRIDE;

        virtual CPLVirtualMem  *GetVirtualMemAuto( GDALRWFlag eRWFlag,
                                                int *pnPixelSpace,
                                                GIntBig *pnLineSpace,
                                                char **papszOptions ) CPL_OVERRIDE;
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
        GIntBig          responsiblePID;

        char            *pszProjectionRef;
        double           adfGeoTransform[6];
        int              bHasSrcProjection;
        int              bHasSrcGeoTransform;
        char            *pszGCPProjection;
        int              nGCPCount;
        GDAL_GCP        *pasGCPList;
        CPLHashSet      *metadataSet;
        CPLHashSet      *metadataItemSet;

        GDALProxyPoolCacheEntry* cacheEntry;
        char            *m_pszOwner;

        GDALDataset *RefUnderlyingDataset(bool bForceOpen);

    protected:
        virtual GDALDataset *RefUnderlyingDataset() CPL_OVERRIDE;
        virtual void UnrefUnderlyingDataset(GDALDataset* poUnderlyingDataset) CPL_OVERRIDE;

        friend class     GDALProxyPoolRasterBand;

    public:
        GDALProxyPoolDataset(const char* pszSourceDatasetDescription,
                            int nRasterXSize, int nRasterYSize,
                            GDALAccess eAccess = GA_ReadOnly,
                            int bShared = FALSE,
                            const char * pszProjectionRef = NULL,
                            double * padfGeoTransform = NULL,
                            const char* pszOwner = NULL);
        virtual ~GDALProxyPoolDataset();

        void         SetOpenOptions(char** papszOpenOptions);
        void         AddSrcBandDescription( GDALDataType eDataType, int nBlockXSize, int nBlockYSize);

        virtual void FlushCache(void) CPL_OVERRIDE;

        virtual const char *GetProjectionRef(void) CPL_OVERRIDE;
        virtual CPLErr SetProjection( const char * ) CPL_OVERRIDE;

        virtual CPLErr GetGeoTransform( double * ) CPL_OVERRIDE;
        virtual CPLErr SetGeoTransform( double * ) CPL_OVERRIDE;

        /* Special behaviour for the following methods : they return a pointer */
        /* data type, that must be cached by the proxy, so it doesn't become invalid */
        /* when the underlying object get closed */
        virtual char      **GetMetadata( const char * pszDomain  ) CPL_OVERRIDE;
        virtual const char *GetMetadataItem( const char * pszName,
                                            const char * pszDomain  ) CPL_OVERRIDE;

        virtual void *GetInternalHandle( const char * pszRequest ) CPL_OVERRIDE;

        virtual const char *GetGCPProjection() CPL_OVERRIDE;
        virtual const GDAL_GCP *GetGCPs() CPL_OVERRIDE;
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
        CPLHashSet      *metadataSet;
        CPLHashSet      *metadataItemSet;
        char            *pszUnitType;
        char           **papszCategoryNames;
        GDALColorTable  *poColorTable;

        int                               nSizeProxyOverviewRasterBand;
        GDALProxyPoolOverviewRasterBand **papoProxyOverviewRasterBand;
        GDALProxyPoolMaskBand            *poProxyMaskBand;

        void Init();

        GDALRasterBand* RefUnderlyingRasterBand(bool bForceOpen);

    protected:
        virtual GDALRasterBand* RefUnderlyingRasterBand() CPL_OVERRIDE;
        virtual void UnrefUnderlyingRasterBand(GDALRasterBand* poUnderlyingRasterBand) CPL_OVERRIDE;

        friend class GDALProxyPoolOverviewRasterBand;
        friend class GDALProxyPoolMaskBand;

    public:
        GDALProxyPoolRasterBand(GDALProxyPoolDataset* poDS, int nBand,
                                GDALDataType eDataType,
                                int nBlockXSize, int nBlockYSize);
        GDALProxyPoolRasterBand(GDALProxyPoolDataset* poDS,
                                GDALRasterBand* poUnderlyingRasterBand);
        virtual ~GDALProxyPoolRasterBand();

        void AddSrcMaskBandDescription( GDALDataType eDataType, int nBlockXSize, int nBlockYSize);

        /* Special behaviour for the following methods : they return a pointer */
        /* data type, that must be cached by the proxy, so it doesn't become invalid */
        /* when the underlying object get closed */
        virtual char      **GetMetadata( const char * pszDomain  ) CPL_OVERRIDE;
        virtual const char *GetMetadataItem( const char * pszName,
                                            const char * pszDomain  ) CPL_OVERRIDE;
        virtual char **GetCategoryNames() CPL_OVERRIDE;
        virtual const char *GetUnitType() CPL_OVERRIDE;
        virtual GDALColorTable *GetColorTable() CPL_OVERRIDE;
        virtual GDALRasterBand *GetOverview(int) CPL_OVERRIDE;
        virtual GDALRasterBand *GetRasterSampleOverview( GUIntBig nDesiredSamples) CPL_OVERRIDE; // TODO
        virtual GDALRasterBand *GetMaskBand() CPL_OVERRIDE;

        virtual CPLErr FlushCache() CPL_OVERRIDE;
  private:
    CPL_DISALLOW_COPY_ASSIGN(GDALProxyPoolRasterBand)
};

/* ******************************************************************** */
/*                  GDALProxyPoolOverviewRasterBand                     */
/* ******************************************************************** */

class GDALProxyPoolOverviewRasterBand : public GDALProxyPoolRasterBand
{
    private:
        GDALProxyPoolRasterBand *poMainBand;
        int                      nOverviewBand;

        GDALRasterBand          *poUnderlyingMainRasterBand;
        int                      nRefCountUnderlyingMainRasterBand;

    protected:
        virtual GDALRasterBand* RefUnderlyingRasterBand() CPL_OVERRIDE;
        virtual void UnrefUnderlyingRasterBand(GDALRasterBand* poUnderlyingRasterBand) CPL_OVERRIDE;

    public:
        GDALProxyPoolOverviewRasterBand(GDALProxyPoolDataset* poDS,
                                        GDALRasterBand* poUnderlyingOverviewBand,
                                        GDALProxyPoolRasterBand* poMainBand,
                                        int nOverviewBand);
        virtual ~GDALProxyPoolOverviewRasterBand();
};

/* ******************************************************************** */
/*                      GDALProxyPoolMaskBand                           */
/* ******************************************************************** */

class GDALProxyPoolMaskBand : public GDALProxyPoolRasterBand
{
    private:
        GDALProxyPoolRasterBand *poMainBand;

        GDALRasterBand          *poUnderlyingMainRasterBand;
        int                      nRefCountUnderlyingMainRasterBand;

    protected:
        virtual GDALRasterBand* RefUnderlyingRasterBand() CPL_OVERRIDE;
        virtual void UnrefUnderlyingRasterBand(GDALRasterBand* poUnderlyingRasterBand) CPL_OVERRIDE;

    public:
        GDALProxyPoolMaskBand(GDALProxyPoolDataset* poDS,
                              GDALRasterBand* poUnderlyingMaskBand,
                              GDALProxyPoolRasterBand* poMainBand);
        GDALProxyPoolMaskBand(GDALProxyPoolDataset* poDS,
                              GDALProxyPoolRasterBand* poMainBand,
                              GDALDataType eDataType,
                              int nBlockXSize, int nBlockYSize);
        virtual ~GDALProxyPoolMaskBand();
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
