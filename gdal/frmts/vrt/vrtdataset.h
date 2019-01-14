/******************************************************************************
 * $Id$
 *
 * Project:  Virtual GDAL Datasets
 * Purpose:  Declaration of virtual gdal dataset classes.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2001, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2007-2013, Even Rouault <even dot rouault at mines-paris dot org>
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

#ifndef VIRTUALDATASET_H_INCLUDED
#define VIRTUALDATASET_H_INCLUDED

#ifndef DOXYGEN_SKIP

#include "cpl_hash_set.h"
#include "gdal_pam.h"
#include "gdal_priv.h"
#include "gdal_rat.h"
#include "gdal_vrt.h"
#include "gdal_rat.h"

#include <map>
#include <memory>
#include <vector>

int VRTApplyMetadata( CPLXMLNode *, GDALMajorObject * );
CPLXMLNode *VRTSerializeMetadata( GDALMajorObject * );
CPLErr GDALRegisterDefaultPixelFunc();

#if 0
int VRTWarpedOverviewTransform( void *pTransformArg, int bDstToSrc,
                                int nPointCount,
                                double *padfX, double *padfY, double *padfZ,
                                int *panSuccess );
void* VRTDeserializeWarpedOverviewTransformer( CPLXMLNode *psTree );
#endif

/************************************************************************/
/*                          VRTOverviewInfo()                           */
/************************************************************************/
class VRTOverviewInfo
{
    CPL_DISALLOW_COPY_ASSIGN(VRTOverviewInfo)

public:
    CPLString       osFilename{};
    int             nBand = 0;
    GDALRasterBand *poBand = nullptr;
    int             bTriedToOpen = FALSE;

    VRTOverviewInfo() = default;
    VRTOverviewInfo(VRTOverviewInfo&& oOther) noexcept:
        osFilename(std::move(oOther.osFilename)),
        nBand(oOther.nBand),
        poBand(oOther.poBand),
        bTriedToOpen(oOther.bTriedToOpen)
    {
        oOther.poBand = nullptr;
    }

    ~VRTOverviewInfo() {
        if( poBand == nullptr )
            /* do nothing */;
        else if( poBand->GetDataset()->GetShared() )
            GDALClose( /* (GDALDatasetH) */ poBand->GetDataset() );
        else
            poBand->GetDataset()->Dereference();
    }
};

/************************************************************************/
/*                              VRTSource                               */
/************************************************************************/

class CPL_DLL VRTSource
{
public:
    virtual ~VRTSource();

    virtual CPLErr  RasterIO( GDALDataType eBandDataType,
                              int nXOff, int nYOff, int nXSize, int nYSize,
                              void *pData, int nBufXSize, int nBufYSize,
                              GDALDataType eBufType,
                              GSpacing nPixelSpace, GSpacing nLineSpace,
                              GDALRasterIOExtraArg* psExtraArg ) = 0;

    virtual double GetMinimum( int nXSize, int nYSize, int *pbSuccess ) = 0;
    virtual double GetMaximum( int nXSize, int nYSize, int *pbSuccess ) = 0;
    virtual CPLErr ComputeRasterMinMax( int nXSize, int nYSize, int bApproxOK,
                                        double* adfMinMax ) = 0;
    virtual CPLErr ComputeStatistics( int nXSize, int nYSize,
                                      int bApproxOK,
                                      double *pdfMin, double *pdfMax,
                                      double *pdfMean, double *pdfStdDev,
                                      GDALProgressFunc pfnProgress,
                                      void *pProgressData ) = 0;
    virtual CPLErr  GetHistogram( int nXSize, int nYSize,
                                  double dfMin, double dfMax,
                                  int nBuckets, GUIntBig * panHistogram,
                                  int bIncludeOutOfRange, int bApproxOK,
                                  GDALProgressFunc pfnProgress,
                                  void *pProgressData ) = 0;

    virtual CPLErr  XMLInit( CPLXMLNode *psTree, const char *, void* ) = 0;
    virtual CPLXMLNode *SerializeToXML( const char *pszVRTPath ) = 0;

    virtual void   GetFileList(char*** ppapszFileList, int *pnSize,
                               int *pnMaxSize, CPLHashSet* hSetFiles);

    virtual int    IsSimpleSource() { return FALSE; }
    virtual CPLErr FlushCache() { return CE_None; }
};

typedef VRTSource *(*VRTSourceParser)(CPLXMLNode *, const char *, void* pUniqueHandle);

VRTSource *VRTParseCoreSources( CPLXMLNode *psTree, const char *, void* pUniqueHandle );
VRTSource *VRTParseFilterSources( CPLXMLNode *psTree, const char *, void* pUniqueHandle );

/************************************************************************/
/*                              VRTDataset                              */
/************************************************************************/

class VRTRasterBand;

template<class T> struct VRTFlushCacheStruct
{
    static void FlushCache(T& obj);
};

class VRTWarpedDataset;
class VRTPansharpenedDataset;

class CPL_DLL VRTDataset : public GDALDataset
{
    friend class VRTRasterBand;
    friend struct VRTFlushCacheStruct<VRTDataset>;
    friend struct VRTFlushCacheStruct<VRTWarpedDataset>;
    friend struct VRTFlushCacheStruct<VRTPansharpenedDataset>;

    char           *m_pszProjection;

    int            m_bGeoTransformSet;
    double         m_adfGeoTransform[6];

    int            m_nGCPCount;
    GDAL_GCP      *m_pasGCPList;
    char          *m_pszGCPProjection;

    int            m_bNeedsFlush;
    int            m_bWritable;

    char          *m_pszVRTPath;

    VRTRasterBand *m_poMaskBand;

    int            m_bCompatibleForDatasetIO;
    int            CheckCompatibleForDatasetIO();
    void           ExpandProxyBands();

    std::vector<GDALDataset*> m_apoOverviews;
    std::vector<GDALDataset*> m_apoOverviewsBak;
    char         **m_papszXMLVRTMetadata;

    VRTRasterBand*      InitBand(const char* pszSubclass, int nBand,
                                 bool bAllowPansharpened);

    CPL_DISALLOW_COPY_ASSIGN(VRTDataset)

  protected:
    virtual int         CloseDependentDatasets() override;

  public:
                 VRTDataset(int nXSize, int nYSize);
    virtual ~VRTDataset();

    void          SetNeedsFlush() { m_bNeedsFlush = TRUE; }
    virtual void  FlushCache() override;

    void SetWritable(int bWritableIn) { m_bWritable = bWritableIn; }

    virtual CPLErr          CreateMaskBand( int nFlags ) override;
    void SetMaskBand(VRTRasterBand* poMaskBand);

    virtual const char *GetProjectionRef() override;
    virtual CPLErr SetProjection( const char * ) override;
    virtual CPLErr GetGeoTransform( double * ) override;
    virtual CPLErr SetGeoTransform( double * ) override;

    virtual CPLErr SetMetadata( char **papszMetadata,
                                const char *pszDomain = "" ) override;
    virtual CPLErr SetMetadataItem( const char *pszName, const char *pszValue,
                                    const char *pszDomain = "" ) override;

    virtual char** GetMetadata( const char *pszDomain = "" ) override;

    virtual int    GetGCPCount() override;
    virtual const char *GetGCPProjection() override;
    virtual const GDAL_GCP *GetGCPs() override;
    virtual CPLErr SetGCPs( int nGCPCount, const GDAL_GCP *pasGCPList,
                            const char *pszGCPProjection ) override;

    virtual CPLErr AddBand( GDALDataType eType,
                            char **papszOptions=nullptr ) override;

    virtual char      **GetFileList() override;

    virtual CPLErr  IRasterIO( GDALRWFlag eRWFlag,
                               int nXOff, int nYOff, int nXSize, int nYSize,
                               void * pData, int nBufXSize, int nBufYSize,
                               GDALDataType eBufType,
                               int nBandCount, int *panBandMap,
                               GSpacing nPixelSpace, GSpacing nLineSpace,
                               GSpacing nBandSpace,
                               GDALRasterIOExtraArg* psExtraArg) override;

    virtual CPLErr AdviseRead( int nXOff, int nYOff, int nXSize, int nYSize,
                               int nBufXSize, int nBufYSize,
                               GDALDataType eDT,
                               int nBandCount, int *panBandList,
                               char **papszOptions ) override;

    virtual CPLXMLNode *SerializeToXML( const char *pszVRTPath);
    virtual CPLErr      XMLInit( CPLXMLNode *, const char * );

    virtual CPLErr IBuildOverviews( const char *, int, int *,
                                    int, int *, GDALProgressFunc, void * ) override;

    /* Used by PDF driver for example */
    GDALDataset*        GetSingleSimpleSource();
    void                BuildVirtualOverviews();

    void                UnsetPreservedRelativeFilenames();

    static int          Identify( GDALOpenInfo * );
    static GDALDataset *Open( GDALOpenInfo * );
    static GDALDataset *OpenXML( const char *, const char * = nullptr,
                                 GDALAccess eAccess = GA_ReadOnly );
    static GDALDataset *Create( const char * pszName,
                                int nXSize, int nYSize, int nBands,
                                GDALDataType eType, char ** papszOptions );
    static CPLErr       Delete( const char * pszFilename );
};

/************************************************************************/
/*                           VRTWarpedDataset                           */
/************************************************************************/

class GDALWarpOperation;
class VRTWarpedRasterBand;

class CPL_DLL VRTWarpedDataset : public VRTDataset
{
    int               m_nBlockXSize;
    int               m_nBlockYSize;
    GDALWarpOperation *m_poWarper;

    int               m_nOverviewCount;
    VRTWarpedDataset **m_papoOverviews;
    int               m_nSrcOvrLevel;

    void              CreateImplicitOverviews();

    struct VerticalShiftGrid
    {
        CPLString osVGrids;
        int       bInverse;
        double    dfToMeterSrc;
        double    dfToMeterDest;
        CPLStringList aosOptions;
    };
    std::vector<VerticalShiftGrid> m_aoVerticalShiftGrids;

    friend class VRTWarpedRasterBand;

    CPL_DISALLOW_COPY_ASSIGN(VRTWarpedDataset)

  protected:
    virtual int         CloseDependentDatasets() override;

public:
                      VRTWarpedDataset( int nXSize, int nYSize );
    virtual ~VRTWarpedDataset();

    virtual void  FlushCache() override;

    CPLErr            Initialize( /* GDALWarpOptions */ void * );

    virtual CPLErr IBuildOverviews( const char *, int, int *,
                                    int, int *, GDALProgressFunc, void * ) override;

    virtual CPLErr SetMetadataItem( const char *pszName, const char *pszValue,
                                    const char *pszDomain = "" ) override;

    virtual CPLXMLNode *SerializeToXML( const char *pszVRTPath ) override;
    virtual CPLErr    XMLInit( CPLXMLNode *, const char * ) override;

    virtual CPLErr AddBand( GDALDataType eType,
                            char **papszOptions=nullptr ) override;

    virtual char      **GetFileList() override;

    CPLErr            ProcessBlock( int iBlockX, int iBlockY );

    void              GetBlockSize( int *, int * ) const;

    void              SetApplyVerticalShiftGrid(const char* pszVGrids,
                                             int bInverse,
                                             double dfToMeterSrc,
                                             double dfToMeterDest,
                                             char** papszOptions );
};

/************************************************************************/
/*                        VRTPansharpenedDataset                        */
/************************************************************************/

class GDALPansharpenOperation;

typedef enum
{
    GTAdjust_Union,
    GTAdjust_Intersection,
    GTAdjust_None,
    GTAdjust_NoneWithoutWarning
} GTAdjustment;

class VRTPansharpenedDataset : public VRTDataset
{
    friend class      VRTPansharpenedRasterBand;

    int               m_nBlockXSize;
    int               m_nBlockYSize;
    GDALPansharpenOperation* m_poPansharpener;
    VRTPansharpenedDataset* m_poMainDataset;
    std::vector<VRTPansharpenedDataset*> m_apoOverviewDatasets;
    // Map from absolute to relative.
    std::map<CPLString,CPLString> m_oMapToRelativeFilenames;

    int               m_bLoadingOtherBands;

    GByte            *m_pabyLastBufferBandRasterIO;
    int               m_nLastBandRasterIOXOff;
    int               m_nLastBandRasterIOYOff;
    int               m_nLastBandRasterIOXSize;
    int               m_nLastBandRasterIOYSize;
    GDALDataType      m_eLastBandRasterIODataType;

    GTAdjustment      m_eGTAdjustment;
    int               m_bNoDataDisabled;

    std::vector<GDALDataset*> m_apoDatasetsToClose;

    CPL_DISALLOW_COPY_ASSIGN(VRTPansharpenedDataset)

  protected:
    virtual int         CloseDependentDatasets() override;

public:
                      VRTPansharpenedDataset( int nXSize, int nYSize );
    virtual ~VRTPansharpenedDataset();

    virtual void  FlushCache() override;

    virtual CPLErr    XMLInit( CPLXMLNode *, const char * ) override;
    virtual CPLXMLNode *   SerializeToXML( const char *pszVRTPath ) override;

    CPLErr            XMLInit( CPLXMLNode *psTree, const char *pszVRTPath,
                               GDALRasterBandH hPanchroBandIn,
                               int nInputSpectralBandsIn,
                               GDALRasterBandH* pahInputSpectralBandsIn );

    virtual CPLErr AddBand( GDALDataType eType,
                            char **papszOptions=nullptr ) override;

    virtual char      **GetFileList() override;

    virtual CPLErr  IRasterIO( GDALRWFlag eRWFlag,
                               int nXOff, int nYOff, int nXSize, int nYSize,
                               void * pData, int nBufXSize, int nBufYSize,
                               GDALDataType eBufType,
                               int nBandCount, int *panBandMap,
                               GSpacing nPixelSpace, GSpacing nLineSpace,
                               GSpacing nBandSpace,
                               GDALRasterIOExtraArg* psExtraArg) override;

    void              GetBlockSize( int *, int * ) const;

    GDALPansharpenOperation* GetPansharpener() { return m_poPansharpener; }
};

/************************************************************************/
/*                            VRTRasterBand                             */
/*                                                                      */
/*      Provides support for all the various kinds of metadata but      */
/*      no raster access.  That is handled by derived classes.          */
/************************************************************************/

class CPL_DLL VRTRasterBand : public GDALRasterBand
{
  protected:
    int            m_bIsMaskBand;

    int            m_bNoDataValueSet;
    // If set to true, will not report the existence of nodata.
    int            m_bHideNoDataValue;
    double         m_dfNoDataValue;

    std::unique_ptr<GDALColorTable> m_poColorTable;

    GDALColorInterp m_eColorInterp;

    char           *m_pszUnitType;
    char           **m_papszCategoryNames;

    double         m_dfOffset;
    double         m_dfScale;

    CPLXMLNode    *m_psSavedHistograms;

    void           Initialize( int nXSize, int nYSize );

    std::vector<VRTOverviewInfo> m_apoOverviews;

    VRTRasterBand *m_poMaskBand;

    std::unique_ptr<GDALRasterAttributeTable> m_poRAT;

    CPL_DISALLOW_COPY_ASSIGN(VRTRasterBand)

  public:

                    VRTRasterBand();
    virtual        ~VRTRasterBand();

    virtual CPLErr         XMLInit( CPLXMLNode *, const char *, void* );
    virtual CPLXMLNode *   SerializeToXML( const char *pszVRTPath );

    virtual CPLErr SetNoDataValue( double ) override;
    virtual double GetNoDataValue( int *pbSuccess = nullptr ) override;
    virtual CPLErr DeleteNoDataValue() override;

    virtual CPLErr SetColorTable( GDALColorTable * ) override;
    virtual GDALColorTable *GetColorTable() override;

    virtual GDALRasterAttributeTable *GetDefaultRAT() override;
    virtual CPLErr SetDefaultRAT( const GDALRasterAttributeTable * poRAT ) override;

    virtual CPLErr SetColorInterpretation( GDALColorInterp ) override;
    virtual GDALColorInterp GetColorInterpretation() override;

    virtual const char *GetUnitType() override;
    CPLErr SetUnitType( const char * ) override;

    virtual char **GetCategoryNames() override;
    virtual CPLErr SetCategoryNames( char ** ) override;

    virtual CPLErr SetMetadata( char **papszMD, const char *pszDomain = "" ) override;
    virtual CPLErr SetMetadataItem( const char *pszName, const char *pszValue,
                                    const char *pszDomain = "" ) override;

    virtual double GetOffset( int *pbSuccess = nullptr ) override;
    CPLErr SetOffset( double ) override;
    virtual double GetScale( int *pbSuccess = nullptr ) override;
    CPLErr SetScale( double ) override;

    virtual int GetOverviewCount() override;
    virtual GDALRasterBand *GetOverview(int) override;

    virtual CPLErr  GetHistogram( double dfMin, double dfMax,
                                  int nBuckets, GUIntBig * panHistogram,
                                  int bIncludeOutOfRange, int bApproxOK,
                                  GDALProgressFunc, void *pProgressData ) override;

    virtual CPLErr GetDefaultHistogram( double *pdfMin, double *pdfMax,
                                        int *pnBuckets, GUIntBig ** ppanHistogram,
                                        int bForce,
                                        GDALProgressFunc, void *pProgressData) override;

    virtual CPLErr SetDefaultHistogram( double dfMin, double dfMax,
                                        int nBuckets, GUIntBig *panHistogram ) override;

    CPLErr         CopyCommonInfoFrom( GDALRasterBand * );

    virtual void   GetFileList(char*** ppapszFileList, int *pnSize,
                               int *pnMaxSize, CPLHashSet* hSetFiles);

    virtual void   SetDescription( const char * ) override;

    virtual GDALRasterBand *GetMaskBand() override;
    virtual int             GetMaskFlags() override;

    virtual CPLErr          CreateMaskBand( int nFlagsIn ) override;

    void SetMaskBand(VRTRasterBand* poMaskBand);

    void SetIsMaskBand();

    CPLErr UnsetNoDataValue();

    virtual int         CloseDependentDatasets();

    virtual int         IsSourcedRasterBand() { return FALSE; }
    virtual int         IsPansharpenRasterBand() { return FALSE; }
};

/************************************************************************/
/*                         VRTSourcedRasterBand                         */
/************************************************************************/

class VRTSimpleSource;

class CPL_DLL VRTSourcedRasterBand : public VRTRasterBand
{
  private:
    int            m_nRecursionCounter;
    CPLString      m_osLastLocationInfo;
    char         **m_papszSourceList;

    bool           CanUseSourcesMinMaxImplementations();
    void           CheckSource( VRTSimpleSource *poSS );

    CPL_DISALLOW_COPY_ASSIGN(VRTSourcedRasterBand)

  public:
    int            nSources;
    VRTSource    **papoSources;
    int            bSkipBufferInitialization;

                   VRTSourcedRasterBand( GDALDataset *poDS, int nBand );
                   VRTSourcedRasterBand( GDALDataType eType,
                                         int nXSize, int nYSize );
                   VRTSourcedRasterBand( GDALDataset *poDS, int nBand,
                                         GDALDataType eType,
                                         int nXSize, int nYSize );
    virtual        ~VRTSourcedRasterBand();

    virtual CPLErr IRasterIO( GDALRWFlag, int, int, int, int,
                              void *, int, int, GDALDataType,
                              GSpacing nPixelSpace, GSpacing nLineSpace,
                              GDALRasterIOExtraArg* psExtraArg) override;

    virtual int IGetDataCoverageStatus( int nXOff, int nYOff,
                                        int nXSize, int nYSize,
                                        int nMaskFlagStop,
                                        double* pdfDataPct) override;

    virtual char      **GetMetadataDomainList() override;
    virtual const char *GetMetadataItem( const char * pszName,
                                         const char * pszDomain = "" ) override;
    virtual char      **GetMetadata( const char * pszDomain = "" ) override;
    virtual CPLErr      SetMetadata( char ** papszMetadata,
                                     const char * pszDomain = "" ) override;
    virtual CPLErr      SetMetadataItem( const char * pszName,
                                         const char * pszValue,
                                         const char * pszDomain = "" ) override;

    virtual CPLErr         XMLInit( CPLXMLNode *, const char *, void* ) override;
    virtual CPLXMLNode *   SerializeToXML( const char *pszVRTPath ) override;

    virtual double GetMinimum( int *pbSuccess = nullptr ) override;
    virtual double GetMaximum(int *pbSuccess = nullptr ) override;
    virtual CPLErr ComputeRasterMinMax( int bApproxOK, double* adfMinMax ) override;
    virtual CPLErr ComputeStatistics( int bApproxOK,
                                      double *pdfMin, double *pdfMax,
                                      double *pdfMean, double *pdfStdDev,
                                      GDALProgressFunc pfnProgress,
                                      void *pProgressData ) override;
    virtual CPLErr  GetHistogram( double dfMin, double dfMax,
                                  int nBuckets, GUIntBig * panHistogram,
                                  int bIncludeOutOfRange, int bApproxOK,
                                  GDALProgressFunc pfnProgress,
                                  void *pProgressData ) override;

    CPLErr         AddSource( VRTSource * );
    CPLErr         AddSimpleSource( GDALRasterBand *poSrcBand,
                                    double dfSrcXOff=-1, double dfSrcYOff=-1,
                                    double dfSrcXSize=-1, double dfSrcYSize=-1,
                                    double dfDstXOff=-1, double dfDstYOff=-1,
                                    double dfDstXSize=-1, double dfDstYSize=-1,
                                    const char *pszResampling = "near",
                                    double dfNoDataValue = VRT_NODATA_UNSET);
    CPLErr         AddComplexSource( GDALRasterBand *poSrcBand,
                                     double dfSrcXOff=-1, double dfSrcYOff=-1,
                                     double dfSrcXSize=-1, double dfSrcYSize=-1,
                                     double dfDstXOff=-1, double dfDstYOff=-1,
                                     double dfDstXSize=-1, double dfDstYSize=-1,
                                     double dfScaleOff=0.0,
                                     double dfScaleRatio=1.0,
                                     double dfNoDataValue = VRT_NODATA_UNSET,
                                     int nColorTableComponent = 0);

    CPLErr         AddMaskBandSource( GDALRasterBand *poSrcBand,
                                      double dfSrcXOff=-1, double dfSrcYOff=-1,
                                      double dfSrcXSize=-1,
                                      double dfSrcYSize=-1,
                                      double dfDstXOff=-1, double dfDstYOff=-1,
                                      double dfDstXSize=-1,
                                      double dfDstYSize=-1 );

    CPLErr         AddFuncSource( VRTImageReadFunc pfnReadFunc, void *hCBData,
                                  double dfNoDataValue = VRT_NODATA_UNSET );

    void           ConfigureSource(VRTSimpleSource *poSimpleSource,
                                   GDALRasterBand *poSrcBand,
                                   int bAddAsMaskBand,
                                   double dfSrcXOff, double dfSrcYOff,
                                   double dfSrcXSize, double dfSrcYSize,
                                   double dfDstXOff, double dfDstYOff,
                                   double dfDstXSize, double dfDstYSize );

    virtual CPLErr IReadBlock( int, int, void * ) override;

    virtual void   GetFileList(char*** ppapszFileList, int *pnSize,
                               int *pnMaxSize, CPLHashSet* hSetFiles) override;

    virtual int         CloseDependentDatasets() override;

    virtual int         IsSourcedRasterBand() override { return TRUE; }

    virtual CPLErr      FlushCache() override;
};

/************************************************************************/
/*                         VRTWarpedRasterBand                          */
/************************************************************************/

class CPL_DLL VRTWarpedRasterBand : public VRTRasterBand
{
  public:
                   VRTWarpedRasterBand( GDALDataset *poDS, int nBand,
                                        GDALDataType eType = GDT_Unknown );
    virtual        ~VRTWarpedRasterBand();

    virtual CPLXMLNode *   SerializeToXML( const char *pszVRTPath ) override;

    virtual CPLErr IReadBlock( int, int, void * ) override;
    virtual CPLErr IWriteBlock( int, int, void * ) override;

    virtual int GetOverviewCount() override;
    virtual GDALRasterBand *GetOverview(int) override;
};
/************************************************************************/
/*                        VRTPansharpenedRasterBand                     */
/************************************************************************/

class VRTPansharpenedRasterBand : public VRTRasterBand
{
    int               m_nIndexAsPansharpenedBand;

  public:
                   VRTPansharpenedRasterBand(
                       GDALDataset *poDS, int nBand,
                       GDALDataType eDataType = GDT_Unknown );
    virtual        ~VRTPansharpenedRasterBand();

    virtual CPLXMLNode *   SerializeToXML( const char *pszVRTPath ) override;

    virtual CPLErr IReadBlock( int, int, void * ) override;

    virtual CPLErr  IRasterIO( GDALRWFlag eRWFlag,
                               int nXOff, int nYOff, int nXSize, int nYSize,
                               void * pData, int nBufXSize, int nBufYSize,
                               GDALDataType eBufType,
                               GSpacing nPixelSpace, GSpacing nLineSpace,
                               GDALRasterIOExtraArg* psExtraArg) override;

    virtual int GetOverviewCount() override;
    virtual GDALRasterBand *GetOverview(int) override;

    virtual int         IsPansharpenRasterBand() override { return TRUE; }

    void                SetIndexAsPansharpenedBand( int nIdx )
        { m_nIndexAsPansharpenedBand = nIdx; }
    int                 GetIndexAsPansharpenedBand() const
        { return m_nIndexAsPansharpenedBand; }
};

/************************************************************************/
/*                         VRTDerivedRasterBand                         */
/************************************************************************/

class VRTDerivedRasterBandPrivateData;

class CPL_DLL VRTDerivedRasterBand : public VRTSourcedRasterBand
{
    VRTDerivedRasterBandPrivateData* m_poPrivate;
    bool InitializePython();

    CPL_DISALLOW_COPY_ASSIGN(VRTDerivedRasterBand)

 public:
    char *pszFuncName;
    GDALDataType eSourceTransferType;

    VRTDerivedRasterBand( GDALDataset *poDS, int nBand );
    VRTDerivedRasterBand( GDALDataset *poDS, int nBand,
                          GDALDataType eType, int nXSize, int nYSize );
    virtual        ~VRTDerivedRasterBand();

    virtual CPLErr IRasterIO( GDALRWFlag, int, int, int, int,
                              void *, int, int, GDALDataType,
                              GSpacing nPixelSpace, GSpacing nLineSpace,
                              GDALRasterIOExtraArg* psExtraArg ) override;

    virtual int IGetDataCoverageStatus( int nXOff, int nYOff,
                                        int nXSize, int nYSize,
                                        int nMaskFlagStop,
                                        double* pdfDataPct) override;

    static CPLErr AddPixelFunction( const char *pszFuncName,
                                    GDALDerivedPixelFunc pfnPixelFunc );
    static GDALDerivedPixelFunc GetPixelFunction( const char *pszFuncName );

    void SetPixelFunctionName( const char *pszFuncName );
    void SetSourceTransferType( GDALDataType eDataType );
    void SetPixelFunctionLanguage( const char* pszLanguage );

    virtual CPLErr         XMLInit( CPLXMLNode *, const char *, void* ) override;
    virtual CPLXMLNode *   SerializeToXML( const char *pszVRTPath ) override;

    virtual double GetMinimum( int *pbSuccess = nullptr ) override;
    virtual double GetMaximum(int *pbSuccess = nullptr ) override;
    virtual CPLErr ComputeRasterMinMax( int bApproxOK, double* adfMinMax ) override;
    virtual CPLErr ComputeStatistics( int bApproxOK,
                                      double *pdfMin, double *pdfMax,
                                      double *pdfMean, double *pdfStdDev,
                                      GDALProgressFunc pfnProgress,
                                      void *pProgressData ) override;
    virtual CPLErr  GetHistogram( double dfMin, double dfMax,
                                  int nBuckets, GUIntBig * panHistogram,
                                  int bIncludeOutOfRange, int bApproxOK,
                                  GDALProgressFunc pfnProgress,
                                  void *pProgressData ) override;

    static void Cleanup();
};

/************************************************************************/
/*                           VRTRawRasterBand                           */
/************************************************************************/

class RawRasterBand;

class CPL_DLL VRTRawRasterBand : public VRTRasterBand
{
    RawRasterBand  *m_poRawRaster;

    char           *m_pszSourceFilename;
    int            m_bRelativeToVRT;

    CPL_DISALLOW_COPY_ASSIGN(VRTRawRasterBand)

  public:
                   VRTRawRasterBand( GDALDataset *poDS, int nBand,
                                     GDALDataType eType = GDT_Unknown );
    virtual        ~VRTRawRasterBand();

    virtual CPLErr         XMLInit( CPLXMLNode *, const char *, void* ) override;
    virtual CPLXMLNode *   SerializeToXML( const char *pszVRTPath ) override;

    virtual CPLErr IRasterIO( GDALRWFlag, int, int, int, int,
                              void *, int, int, GDALDataType,
                              GSpacing nPixelSpace, GSpacing nLineSpace,
                              GDALRasterIOExtraArg* psExtraArg ) override;

    virtual CPLErr IReadBlock( int, int, void * ) override;
    virtual CPLErr IWriteBlock( int, int, void * ) override;

    CPLErr         SetRawLink( const char *pszFilename,
                               const char *pszVRTPath,
                               int bRelativeToVRT,
                               vsi_l_offset nImageOffset,
                               int nPixelOffset, int nLineOffset,
                               const char *pszByteOrder );

    void           ClearRawLink();

    virtual void   GetFileList( char*** ppapszFileList, int *pnSize,
                                int *pnMaxSize, CPLHashSet* hSetFiles ) override;
};

/************************************************************************/
/*                              VRTDriver                               */
/************************************************************************/

class VRTDriver : public GDALDriver
{
    CPL_DISALLOW_COPY_ASSIGN(VRTDriver)

  public:
                 VRTDriver();
    virtual ~VRTDriver();

    char         **papszSourceParsers;

    virtual char      **GetMetadataDomainList() override;
    virtual char      **GetMetadata( const char * pszDomain = "" ) override;
    virtual CPLErr      SetMetadata( char ** papszMetadata,
                                     const char * pszDomain = "" ) override;

    VRTSource   *ParseSource( CPLXMLNode *psSrc, const char *pszVRTPath,
                              void* pUniqueHandle );
    void         AddSourceParser( const char *pszElementName,
                                  VRTSourceParser pfnParser );
};

/************************************************************************/
/*                           VRTSimpleSource                            */
/************************************************************************/

class CPL_DLL VRTSimpleSource : public VRTSource
{
    CPL_DISALLOW_COPY_ASSIGN(VRTSimpleSource)

protected:
    friend class VRTSourcedRasterBand;

    GDALRasterBand      *m_poRasterBand;

    // When poRasterBand is a mask band, poMaskBandMainBand is the band
    // from which the mask band is taken.
    GDALRasterBand      *m_poMaskBandMainBand;

    double              m_dfSrcXOff;
    double              m_dfSrcYOff;
    double              m_dfSrcXSize;
    double              m_dfSrcYSize;

    double              m_dfDstXOff;
    double              m_dfDstYOff;
    double              m_dfDstXSize;
    double              m_dfDstYSize;

    int                 m_bNoDataSet;
    double              m_dfNoDataValue;
    CPLString           m_osResampling;

    int                 m_nMaxValue;

    int                 m_bRelativeToVRTOri;
    CPLString           m_osSourceFileNameOri;
    int                 m_nExplicitSharedStatus; // -1 unknown, 0 = unshared, 1 = shared

    int                 NeedMaxValAdjustment() const;

public:
            VRTSimpleSource();
            VRTSimpleSource( const VRTSimpleSource* poSrcSource,
                             double dfXDstRatio, double dfYDstRatio );
    virtual ~VRTSimpleSource();

    virtual CPLErr  XMLInit( CPLXMLNode *psTree, const char *, void* ) override;
    virtual CPLXMLNode *SerializeToXML( const char *pszVRTPath ) override;

    void           SetSrcBand( GDALRasterBand * );
    void           SetSrcMaskBand( GDALRasterBand * );
    void           SetSrcWindow( double, double, double, double );
    void           SetDstWindow( double, double, double, double );
    void           SetNoDataValue( double dfNoDataValue );
    const CPLString& GetResampling() const { return m_osResampling; }
    void           SetResampling( const char* pszResampling );

    int            GetSrcDstWindow( int, int, int, int, int, int,
                                    double *pdfReqXOff, double *pdfReqYOff,
                                    double *pdfReqXSize, double *pdfReqYSize,
                                    int *, int *, int *, int *,
                                    int *, int *, int *, int * );

    virtual CPLErr  RasterIO( GDALDataType eBandDataType,
                              int nXOff, int nYOff, int nXSize, int nYSize,
                              void *pData, int nBufXSize, int nBufYSize,
                              GDALDataType eBufType,
                              GSpacing nPixelSpace, GSpacing nLineSpace,
                              GDALRasterIOExtraArg* psExtraArgIn ) override;

    virtual double GetMinimum( int nXSize, int nYSize, int *pbSuccess ) override;
    virtual double GetMaximum( int nXSize, int nYSize, int *pbSuccess ) override;
    virtual CPLErr ComputeRasterMinMax( int nXSize, int nYSize, int bApproxOK,
                                        double* adfMinMax ) override;
    virtual CPLErr ComputeStatistics( int nXSize, int nYSize,
                                      int bApproxOK,
                                      double *pdfMin, double *pdfMax,
                                      double *pdfMean, double *pdfStdDev,
                                      GDALProgressFunc pfnProgress,
                                      void *pProgressData ) override;
    virtual CPLErr  GetHistogram( int nXSize, int nYSize,
                                  double dfMin, double dfMax,
                                  int nBuckets, GUIntBig * panHistogram,
                                  int bIncludeOutOfRange, int bApproxOK,
                                  GDALProgressFunc pfnProgress,
                                  void *pProgressData ) override;

    void            DstToSrc( double dfX, double dfY,
                              double &dfXOut, double &dfYOut ) const;
    void            SrcToDst( double dfX, double dfY,
                              double &dfXOut, double &dfYOut ) const;

    virtual void   GetFileList( char*** ppapszFileList, int *pnSize,
                                int *pnMaxSize, CPLHashSet* hSetFiles ) override;

    virtual int    IsSimpleSource() override { return TRUE; }
    virtual const char* GetType() { return "SimpleSource"; }
    virtual CPLErr FlushCache() override;

    GDALRasterBand* GetBand();
    int             IsSameExceptBandNumber( VRTSimpleSource* poOtherSource );
    CPLErr          DatasetRasterIO(
                               GDALDataType eBandDataType,
                               int nXOff, int nYOff, int nXSize, int nYSize,
                               void * pData, int nBufXSize, int nBufYSize,
                               GDALDataType eBufType,
                               int nBandCount, int *panBandMap,
                               GSpacing nPixelSpace, GSpacing nLineSpace,
                               GSpacing nBandSpace,
                               GDALRasterIOExtraArg* psExtraArg );

    void             UnsetPreservedRelativeFilenames();

    void             SetMaxValue( int nVal ) { m_nMaxValue = nVal; }
};

/************************************************************************/
/*                          VRTAveragedSource                           */
/************************************************************************/

class VRTAveragedSource : public VRTSimpleSource
{
    CPL_DISALLOW_COPY_ASSIGN(VRTAveragedSource)

public:
                    VRTAveragedSource();
    virtual CPLErr  RasterIO( GDALDataType eBandDataType,
                              int nXOff, int nYOff, int nXSize, int nYSize,
                              void *pData, int nBufXSize, int nBufYSize,
                              GDALDataType eBufType,
                              GSpacing nPixelSpace, GSpacing nLineSpace,
                              GDALRasterIOExtraArg* psExtraArgIn ) override;

    virtual double GetMinimum( int nXSize, int nYSize, int *pbSuccess ) override;
    virtual double GetMaximum( int nXSize, int nYSize, int *pbSuccess ) override;
    virtual CPLErr ComputeRasterMinMax( int nXSize, int nYSize, int bApproxOK,
                                        double* adfMinMax ) override;
    virtual CPLErr ComputeStatistics( int nXSize, int nYSize,
                                      int bApproxOK,
                                      double *pdfMin, double *pdfMax,
                                      double *pdfMean, double *pdfStdDev,
                                      GDALProgressFunc pfnProgress,
                                      void *pProgressData ) override;
    virtual CPLErr  GetHistogram( int nXSize, int nYSize,
                                  double dfMin, double dfMax,
                                  int nBuckets, GUIntBig * panHistogram,
                                  int bIncludeOutOfRange, int bApproxOK,
                                  GDALProgressFunc pfnProgress,
                                  void *pProgressData ) override;

    virtual CPLXMLNode *SerializeToXML( const char *pszVRTPath ) override;
    virtual const char* GetType() override { return "AveragedSource"; }
};

/************************************************************************/
/*                           VRTComplexSource                           */
/************************************************************************/

typedef enum
{
    VRT_SCALING_NONE,
    VRT_SCALING_LINEAR,
    VRT_SCALING_EXPONENTIAL,
} VRTComplexSourceScaling;

class CPL_DLL VRTComplexSource : public VRTSimpleSource
{
    CPL_DISALLOW_COPY_ASSIGN(VRTComplexSource)
    bool           AreValuesUnchanged() const;

protected:
    VRTComplexSourceScaling m_eScalingType;
    double         m_dfScaleOff;  // For linear scaling.
    double         m_dfScaleRatio;  // For linear scaling.

    // For non-linear scaling with a power function.
    int            m_bSrcMinMaxDefined;
    double         m_dfSrcMin;
    double         m_dfSrcMax;
    double         m_dfDstMin;
    double         m_dfDstMax;
    double         m_dfExponent;

    int            m_nColorTableComponent;

    template <class WorkingDT>
    CPLErr          RasterIOInternal( int nReqXOff, int nReqYOff,
                                      int nReqXSize, int nReqYSize,
                                      void *pData, int nOutXSize, int nOutYSize,
                                      GDALDataType eBufType,
                                      GSpacing nPixelSpace, GSpacing nLineSpace,
                                      GDALRasterIOExtraArg* psExtraArg,
                                      GDALDataType eWrkDataType );

public:
                   VRTComplexSource();
                   VRTComplexSource(const VRTComplexSource* poSrcSource,
                                    double dfXDstRatio, double dfYDstRatio);
    virtual        ~VRTComplexSource();

    virtual CPLErr RasterIO( GDALDataType eBandDataType,
                             int nXOff, int nYOff, int nXSize, int nYSize,
                             void *pData, int nBufXSize, int nBufYSize,
                             GDALDataType eBufType,
                             GSpacing nPixelSpace, GSpacing nLineSpace,
                             GDALRasterIOExtraArg* psExtraArgIn ) override;

    virtual double GetMinimum( int nXSize, int nYSize, int *pbSuccess ) override;
    virtual double GetMaximum( int nXSize, int nYSize, int *pbSuccess ) override;
    virtual CPLErr ComputeRasterMinMax( int nXSize, int nYSize, int bApproxOK,
                                        double* adfMinMax ) override;
    virtual CPLErr ComputeStatistics( int nXSize, int nYSize,
                                      int bApproxOK,
                                      double *pdfMin, double *pdfMax,
                                      double *pdfMean, double *pdfStdDev,
                                      GDALProgressFunc pfnProgress,
                                      void *pProgressData ) override;
    virtual CPLErr  GetHistogram( int nXSize, int nYSize,
                                  double dfMin, double dfMax,
                                  int nBuckets, GUIntBig * panHistogram,
                                  int bIncludeOutOfRange, int bApproxOK,
                                  GDALProgressFunc pfnProgress,
                                  void *pProgressData ) override;

    virtual CPLXMLNode *SerializeToXML( const char *pszVRTPath ) override;
    virtual CPLErr XMLInit( CPLXMLNode *, const char *, void* ) override;
    virtual const char* GetType() override { return "ComplexSource"; }

    double  LookupValue( double dfInput );

    void    SetLinearScaling( double dfOffset, double dfScale );
    void    SetPowerScaling( double dfExponent,
                             double dfSrcMin,
                             double dfSrcMax,
                             double dfDstMin,
                             double dfDstMax );
    void    SetColorTableComponent( int nComponent );

    double         *m_padfLUTInputs;
    double         *m_padfLUTOutputs;
    int            m_nLUTItemCount;
};

/************************************************************************/
/*                           VRTFilteredSource                          */
/************************************************************************/

class VRTFilteredSource : public VRTComplexSource
{
private:
    int          IsTypeSupported( GDALDataType eTestType ) const;

    CPL_DISALLOW_COPY_ASSIGN(VRTFilteredSource)

protected:
    int          m_nSupportedTypesCount;
    GDALDataType m_aeSupportedTypes[20];

    int          m_nExtraEdgePixels;

public:
            VRTFilteredSource();
    virtual ~VRTFilteredSource();

    void    SetExtraEdgePixels( int );
    void    SetFilteringDataTypesSupported( int, GDALDataType * );

    virtual CPLErr  FilterData( int nXSize, int nYSize, GDALDataType eType,
                                GByte *pabySrcData, GByte *pabyDstData ) = 0;

    virtual CPLErr  RasterIO( GDALDataType eBandDataType,
                              int nXOff, int nYOff, int nXSize, int nYSize,
                              void *pData, int nBufXSize, int nBufYSize,
                              GDALDataType eBufType,
                              GSpacing nPixelSpace, GSpacing nLineSpace,
                              GDALRasterIOExtraArg* psExtraArg ) override;
};

/************************************************************************/
/*                       VRTKernelFilteredSource                        */
/************************************************************************/

class VRTKernelFilteredSource : public VRTFilteredSource
{
    CPL_DISALLOW_COPY_ASSIGN(VRTKernelFilteredSource)

protected:
    int     m_nKernelSize;

    bool    m_bSeparable;

    double  *m_padfKernelCoefs;

    int     m_bNormalized;

public:
            VRTKernelFilteredSource();
    virtual ~VRTKernelFilteredSource();

    virtual CPLErr  XMLInit( CPLXMLNode *psTree, const char *, void* ) override;
    virtual CPLXMLNode *SerializeToXML( const char *pszVRTPath ) override;

    virtual CPLErr  FilterData( int nXSize, int nYSize, GDALDataType eType,
                                GByte *pabySrcData, GByte *pabyDstData ) override;

    CPLErr          SetKernel( int nKernelSize, bool bSeparable, double *padfCoefs );
    void            SetNormalized( int );
};

/************************************************************************/
/*                       VRTAverageFilteredSource                       */
/************************************************************************/

class VRTAverageFilteredSource : public VRTKernelFilteredSource
{
    CPL_DISALLOW_COPY_ASSIGN(VRTAverageFilteredSource)

public:
            explicit VRTAverageFilteredSource( int nKernelSize );
    virtual ~VRTAverageFilteredSource();

    virtual CPLErr  XMLInit( CPLXMLNode *psTree, const char *, void* ) override;
    virtual CPLXMLNode *SerializeToXML( const char *pszVRTPath ) override;
};

/************************************************************************/
/*                            VRTFuncSource                             */
/************************************************************************/
class VRTFuncSource : public VRTSource
{
    CPL_DISALLOW_COPY_ASSIGN(VRTFuncSource)

public:
            VRTFuncSource();
    virtual ~VRTFuncSource();

    virtual CPLErr  XMLInit( CPLXMLNode *, const char *, void* ) override { return CE_Failure; }
    virtual CPLXMLNode *SerializeToXML( const char *pszVRTPath ) override;

    virtual CPLErr  RasterIO( GDALDataType eBandDataType,
                              int nXOff, int nYOff, int nXSize, int nYSize,
                              void *pData, int nBufXSize, int nBufYSize,
                              GDALDataType eBufType,
                              GSpacing nPixelSpace, GSpacing nLineSpace,
                              GDALRasterIOExtraArg* psExtraArg ) override;

    virtual double GetMinimum( int nXSize, int nYSize, int *pbSuccess ) override;
    virtual double GetMaximum( int nXSize, int nYSize, int *pbSuccess ) override;
    virtual CPLErr ComputeRasterMinMax( int nXSize, int nYSize, int bApproxOK,
                                        double* adfMinMax ) override;
    virtual CPLErr ComputeStatistics( int nXSize, int nYSize,
                                      int bApproxOK,
                                      double *pdfMin, double *pdfMax,
                                      double *pdfMean, double *pdfStdDev,
                                      GDALProgressFunc pfnProgress,
                                      void *pProgressData ) override;
    virtual CPLErr  GetHistogram( int nXSize, int nYSize,
                                  double dfMin, double dfMax,
                                  int nBuckets, GUIntBig * panHistogram,
                                  int bIncludeOutOfRange, int bApproxOK,
                                  GDALProgressFunc pfnProgress,
                                  void *pProgressData ) override;

    VRTImageReadFunc    pfnReadFunc;
    void               *pCBData;
    GDALDataType        eType;

    float               fNoDataValue;
};

#endif /* #ifndef DOXYGEN_SKIP */

#endif /* ndef VIRTUALDATASET_H_INCLUDED */
