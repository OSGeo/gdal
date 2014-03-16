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
        virtual GDALDataset *RefUnderlyingDataset() = 0;
        virtual void UnrefUnderlyingDataset(GDALDataset* poUnderlyingDataset);

        virtual CPLErr IBuildOverviews( const char *, int, int *,
                                    int, int *, GDALProgressFunc, void * );
        virtual CPLErr IRasterIO( GDALRWFlag, int, int, int, int,
                                void *, int, int, GDALDataType,
                                int, int *, int, int, int );
    public:

        virtual char      **GetMetadataDomainList();
        virtual char      **GetMetadata( const char * pszDomain  );
        virtual CPLErr      SetMetadata( char ** papszMetadata,
                                        const char * pszDomain  );
        virtual const char *GetMetadataItem( const char * pszName,
                                            const char * pszDomain  );
        virtual CPLErr      SetMetadataItem( const char * pszName,
                                            const char * pszValue,
                                            const char * pszDomain );

        virtual void FlushCache(void);

        virtual const char *GetProjectionRef(void);
        virtual CPLErr SetProjection( const char * );

        virtual CPLErr GetGeoTransform( double * );
        virtual CPLErr SetGeoTransform( double * );

        virtual void *GetInternalHandle( const char * );
        virtual GDALDriver *GetDriver(void);
        virtual char      **GetFileList(void);

        virtual int    GetGCPCount();
        virtual const char *GetGCPProjection();
        virtual const GDAL_GCP *GetGCPs();
        virtual CPLErr SetGCPs( int nGCPCount, const GDAL_GCP *pasGCPList,
                                const char *pszGCPProjection );

        virtual CPLErr AdviseRead( int nXOff, int nYOff, int nXSize, int nYSize,
                                int nBufXSize, int nBufYSize, 
                                GDALDataType eDT, 
                                int nBandCount, int *panBandList,
                                char **papszOptions );

        virtual CPLErr          CreateMaskBand( int nFlags );

};

/* ******************************************************************** */
/*                         GDALProxyRasterBand                          */
/* ******************************************************************** */

class CPL_DLL GDALProxyRasterBand : public GDALRasterBand
{
    protected:
        virtual GDALRasterBand* RefUnderlyingRasterBand() = 0;
        virtual void UnrefUnderlyingRasterBand(GDALRasterBand* poUnderlyingRasterBand);

        virtual CPLErr IReadBlock( int, int, void * );
        virtual CPLErr IWriteBlock( int, int, void * );
        virtual CPLErr IRasterIO( GDALRWFlag, int, int, int, int,
                                void *, int, int, GDALDataType,
                                int, int );

    public:

        virtual char      **GetMetadataDomainList();
        virtual char      **GetMetadata( const char * pszDomain  );
        virtual CPLErr      SetMetadata( char ** papszMetadata,
                                        const char * pszDomain  );
        virtual const char *GetMetadataItem( const char * pszName,
                                            const char * pszDomain  );
        virtual CPLErr      SetMetadataItem( const char * pszName,
                                            const char * pszValue,
                                            const char * pszDomain );
        virtual CPLErr FlushCache();
        virtual char **GetCategoryNames();
        virtual double GetNoDataValue( int *pbSuccess = NULL );
        virtual double GetMinimum( int *pbSuccess = NULL );
        virtual double GetMaximum(int *pbSuccess = NULL );
        virtual double GetOffset( int *pbSuccess = NULL );
        virtual double GetScale( int *pbSuccess = NULL );
        virtual const char *GetUnitType();
        virtual GDALColorInterp GetColorInterpretation();
        virtual GDALColorTable *GetColorTable();
        virtual CPLErr Fill(double dfRealValue, double dfImaginaryValue = 0);

        virtual CPLErr SetCategoryNames( char ** );
        virtual CPLErr SetNoDataValue( double );
        virtual CPLErr SetColorTable( GDALColorTable * ); 
        virtual CPLErr SetColorInterpretation( GDALColorInterp );
        virtual CPLErr SetOffset( double );
        virtual CPLErr SetScale( double );
        virtual CPLErr SetUnitType( const char * );

        virtual CPLErr GetStatistics( int bApproxOK, int bForce,
                                    double *pdfMin, double *pdfMax, 
                                    double *pdfMean, double *padfStdDev );
        virtual CPLErr ComputeStatistics( int bApproxOK, 
                                        double *pdfMin, double *pdfMax, 
                                        double *pdfMean, double *pdfStdDev,
                                        GDALProgressFunc, void *pProgressData );
        virtual CPLErr SetStatistics( double dfMin, double dfMax, 
                                    double dfMean, double dfStdDev );
        virtual CPLErr ComputeRasterMinMax( int, double* );

        virtual int HasArbitraryOverviews();
        virtual int GetOverviewCount();
        virtual GDALRasterBand *GetOverview(int);
        virtual GDALRasterBand *GetRasterSampleOverview( int );
        virtual CPLErr BuildOverviews( const char *, int, int *,
                                    GDALProgressFunc, void * );

        virtual CPLErr AdviseRead( int nXOff, int nYOff, int nXSize, int nYSize,
                                int nBufXSize, int nBufYSize, 
                                GDALDataType eDT, char **papszOptions );

        virtual CPLErr  GetHistogram( double dfMin, double dfMax,
                            int nBuckets, int * panHistogram,
                            int bIncludeOutOfRange, int bApproxOK,
                            GDALProgressFunc, void *pProgressData );

        virtual CPLErr GetDefaultHistogram( double *pdfMin, double *pdfMax,
                                            int *pnBuckets, int ** ppanHistogram,
                                            int bForce,
                                            GDALProgressFunc, void *pProgressData);
        virtual CPLErr SetDefaultHistogram( double dfMin, double dfMax,
                                            int nBuckets, int *panHistogram );

        virtual GDALRasterAttributeTable *GetDefaultRAT();
        virtual CPLErr SetDefaultRAT( const GDALRasterAttributeTable * );

        virtual GDALRasterBand *GetMaskBand();
        virtual int             GetMaskFlags();
        virtual CPLErr          CreateMaskBand( int nFlags );

        virtual CPLVirtualMem  *GetVirtualMemAuto( GDALRWFlag eRWFlag,
                                                int *pnPixelSpace,
                                                GIntBig *pnLineSpace,
                                                char **papszOptions );
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

    protected:
        virtual GDALDataset *RefUnderlyingDataset();
        virtual void UnrefUnderlyingDataset(GDALDataset* poUnderlyingDataset);

        friend class     GDALProxyPoolRasterBand;

    public:
        GDALProxyPoolDataset(const char* pszSourceDatasetDescription,
                            int nRasterXSize, int nRasterYSize,
                            GDALAccess eAccess = GA_ReadOnly,
                            int bShared = FALSE,
                            const char * pszProjectionRef = NULL,
                            double * padfGeoTransform = NULL);
        ~GDALProxyPoolDataset();

        void         AddSrcBandDescription( GDALDataType eDataType, int nBlockXSize, int nBlockYSize);

        virtual const char *GetProjectionRef(void);
        virtual CPLErr SetProjection( const char * );

        virtual CPLErr GetGeoTransform( double * );
        virtual CPLErr SetGeoTransform( double * );

        /* Special behaviour for the following methods : they return a pointer */
        /* data type, that must be cached by the proxy, so it doesn't become invalid */
        /* when the underlying object get closed */
        virtual char      **GetMetadata( const char * pszDomain  );
        virtual const char *GetMetadataItem( const char * pszName,
                                            const char * pszDomain  );

        virtual void *GetInternalHandle( const char * pszRequest );

        virtual const char *GetGCPProjection();
        virtual const GDAL_GCP *GetGCPs();
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

    protected:
        virtual GDALRasterBand* RefUnderlyingRasterBand();
        virtual void UnrefUnderlyingRasterBand(GDALRasterBand* poUnderlyingRasterBand);

        friend class GDALProxyPoolOverviewRasterBand;
        friend class GDALProxyPoolMaskBand;

    public:
        GDALProxyPoolRasterBand(GDALProxyPoolDataset* poDS, int nBand,
                                GDALDataType eDataType,
                                int nBlockXSize, int nBlockYSize);
        GDALProxyPoolRasterBand(GDALProxyPoolDataset* poDS,
                                GDALRasterBand* poUnderlyingRasterBand);
        ~GDALProxyPoolRasterBand();

        void AddSrcMaskBandDescription( GDALDataType eDataType, int nBlockXSize, int nBlockYSize);

        /* Special behaviour for the following methods : they return a pointer */
        /* data type, that must be cached by the proxy, so it doesn't become invalid */
        /* when the underlying object get closed */
        virtual char      **GetMetadata( const char * pszDomain  );
        virtual const char *GetMetadataItem( const char * pszName,
                                            const char * pszDomain  );
        virtual char **GetCategoryNames();
        virtual const char *GetUnitType();
        virtual GDALColorTable *GetColorTable();
        virtual GDALRasterBand *GetOverview(int);
        virtual GDALRasterBand *GetRasterSampleOverview( int nDesiredSamples); // TODO
        virtual GDALRasterBand *GetMaskBand();

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
        virtual GDALRasterBand* RefUnderlyingRasterBand();
        virtual void UnrefUnderlyingRasterBand(GDALRasterBand* poUnderlyingRasterBand);

    public:
        GDALProxyPoolOverviewRasterBand(GDALProxyPoolDataset* poDS,
                                        GDALRasterBand* poUnderlyingOverviewBand,
                                        GDALProxyPoolRasterBand* poMainBand,
                                        int nOverviewBand);
        ~GDALProxyPoolOverviewRasterBand();
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
        virtual GDALRasterBand* RefUnderlyingRasterBand();
        virtual void UnrefUnderlyingRasterBand(GDALRasterBand* poUnderlyingRasterBand);

    public:
        GDALProxyPoolMaskBand(GDALProxyPoolDataset* poDS,
                              GDALRasterBand* poUnderlyingMaskBand,
                              GDALProxyPoolRasterBand* poMainBand);
        GDALProxyPoolMaskBand(GDALProxyPoolDataset* poDS,
                              GDALProxyPoolRasterBand* poMainBand,
                              GDALDataType eDataType,
                              int nBlockXSize, int nBlockYSize);
        ~GDALProxyPoolMaskBand();
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

#endif /* GDAL_PROXY_H_INCLUDED */
