/******************************************************************************
 * $Id$
 *
 * Project:  Virtual GDAL Datasets
 * Purpose:  Declaration of virtual gdal dataset classes.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2001, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2007-2013, Even Rouault <even dot rouault at spatialys.com>
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
#include "cpl_minixml.h"
#include "gdal_pam.h"
#include "gdal_priv.h"
#include "gdal_rat.h"
#include "gdal_vrt.h"
#include "gdal_rat.h"

#include <functional>
#include <map>
#include <memory>
#include <vector>

int VRTApplyMetadata( CPLXMLNode *, GDALMajorObject * );
CPLXMLNode *VRTSerializeMetadata( GDALMajorObject * );
CPLErr GDALRegisterDefaultPixelFunc();
CPLString VRTSerializeNoData(double dfVal, GDALDataType eDataType, int nPrecision);
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
        CloseDataset();
    }

    bool CloseDataset()
    {
        if( poBand == nullptr )
            return false;

        GDALDataset* poDS = poBand->GetDataset();
        // Nullify now, to prevent recursion in some cases !
        poBand = nullptr;
        if( poDS->GetShared() )
            GDALClose( /* (GDALDatasetH) */ poDS );
        else
            poDS->Dereference();

        return true;
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

    virtual CPLErr  XMLInit( CPLXMLNode *psTree, const char *, void*,
                             std::map<CPLString, GDALDataset*>& ) = 0;
    virtual CPLXMLNode *SerializeToXML( const char *pszVRTPath ) = 0;

    virtual void   GetFileList(char*** ppapszFileList, int *pnSize,
                               int *pnMaxSize, CPLHashSet* hSetFiles);

    virtual int    IsSimpleSource() { return FALSE; }
    virtual CPLErr FlushCache() { return CE_None; }
};

typedef VRTSource *(*VRTSourceParser)(CPLXMLNode *, const char *, void* pUniqueHandle,
                                      std::map<CPLString, GDALDataset*>& oMapSharedSources);

VRTSource *VRTParseCoreSources( CPLXMLNode *psTree, const char *, void* pUniqueHandle,
                                std::map<CPLString, GDALDataset*>& oMapSharedSources);
VRTSource *VRTParseFilterSources( CPLXMLNode *psTree, const char *, void* pUniqueHandle,
                                  std::map<CPLString, GDALDataset*>& oMapSharedSources );

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
class VRTGroup;

class CPL_DLL VRTDataset CPL_NON_FINAL: public GDALDataset
{
    friend class VRTRasterBand;
    friend struct VRTFlushCacheStruct<VRTDataset>;
    friend struct VRTFlushCacheStruct<VRTWarpedDataset>;
    friend struct VRTFlushCacheStruct<VRTPansharpenedDataset>;
    friend class VRTSourcedRasterBand;
    friend VRTDatasetH CPL_STDCALL VRTCreate(int nXSize, int nYSize);

    OGRSpatialReference* m_poSRS = nullptr;

    int            m_bGeoTransformSet = false;
    double         m_adfGeoTransform[6];

    int            m_nGCPCount = 0;
    GDAL_GCP      *m_pasGCPList = nullptr;
    OGRSpatialReference *m_poGCP_SRS = nullptr;

    bool           m_bNeedsFlush = false;
    bool           m_bWritable = true;
    bool           m_bCanTakeRef = true;

    char          *m_pszVRTPath = nullptr;

    VRTRasterBand *m_poMaskBand = nullptr;

    int            m_bCompatibleForDatasetIO = -1;
    int            CheckCompatibleForDatasetIO();
    void           ExpandProxyBands();

    // Virtual (ie not materialized) overviews, created either implicitly
    // when it is cheap to do it, or explicitly.
    std::vector<GDALDataset*> m_apoOverviews{};
    std::vector<GDALDataset*> m_apoOverviewsBak{};
    CPLString           m_osOverviewResampling{};
    std::vector<int>    m_anOverviewFactors{};

    char         **m_papszXMLVRTMetadata = nullptr;

    std::map<CPLString, GDALDataset*> m_oMapSharedSources{};
    std::shared_ptr<VRTGroup> m_poRootGroup{};

    int            m_nRecursionCounter = 0;

    VRTRasterBand*      InitBand(const char* pszSubclass, int nBand,
                                 bool bAllowPansharpened);
    static GDALDataset *OpenVRTProtocol( const char* pszSpec );
    bool                AddVirtualOverview(int nOvFactor,
                                           const char* pszResampling);

    CPL_DISALLOW_COPY_ASSIGN(VRTDataset)

  protected:
    virtual int         CloseDependentDatasets() override;

  public:
                 VRTDataset(int nXSize, int nYSize);
    virtual ~VRTDataset();

    void          SetNeedsFlush() { m_bNeedsFlush = true; }
    virtual void  FlushCache() override;

    void SetWritable(int bWritableIn) { m_bWritable = CPL_TO_BOOL(bWritableIn); }

    virtual CPLErr          CreateMaskBand( int nFlags ) override;
    void SetMaskBand(VRTRasterBand* poMaskBand);

    const OGRSpatialReference* GetSpatialRef() const override { return m_poSRS; }
    CPLErr SetSpatialRef(const OGRSpatialReference* poSRS) override;

    virtual CPLErr GetGeoTransform( double * ) override;
    virtual CPLErr SetGeoTransform( double * ) override;

    virtual CPLErr SetMetadata( char **papszMetadata,
                                const char *pszDomain = "" ) override;
    virtual CPLErr SetMetadataItem( const char *pszName, const char *pszValue,
                                    const char *pszDomain = "" ) override;

    virtual char** GetMetadata( const char *pszDomain = "" ) override;

    virtual int    GetGCPCount() override;
    const OGRSpatialReference* GetGCPSpatialRef() const override { return m_poGCP_SRS; }
    virtual const GDAL_GCP *GetGCPs() override;
    using GDALDataset::SetGCPs;
    CPLErr SetGCPs( int nGCPCount, const GDAL_GCP *pasGCPList,
                    const OGRSpatialReference* poSRS ) override;

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

    std::shared_ptr<GDALGroup> GetRootGroup() const override;

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
    static GDALDataset *CreateMultiDimensional( const char * pszFilename,
                                                CSLConstList papszRootGroupOptions,
                                                CSLConstList papszOptions );
    static CPLErr       Delete( const char * pszFilename );
};

/************************************************************************/
/*                           VRTWarpedDataset                           */
/************************************************************************/

class GDALWarpOperation;
class VRTWarpedRasterBand;

class CPL_DLL VRTWarpedDataset final: public VRTDataset
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
        CPLString osVGrids{};
        int       bInverse = false;
        double    dfToMeterSrc = 0.0;
        double    dfToMeterDest = 0.0;
        CPLStringList aosOptions{};
    };
    std::vector<VerticalShiftGrid> m_aoVerticalShiftGrids{};

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

class VRTPansharpenedDataset final: public VRTDataset
{
    friend class      VRTPansharpenedRasterBand;

    int               m_nBlockXSize;
    int               m_nBlockYSize;
    GDALPansharpenOperation* m_poPansharpener;
    VRTPansharpenedDataset* m_poMainDataset;
    std::vector<VRTPansharpenedDataset*> m_apoOverviewDatasets{};
    // Map from absolute to relative.
    std::map<CPLString,CPLString> m_oMapToRelativeFilenames{};

    int               m_bLoadingOtherBands;

    GByte            *m_pabyLastBufferBandRasterIO;
    int               m_nLastBandRasterIOXOff;
    int               m_nLastBandRasterIOYOff;
    int               m_nLastBandRasterIOXSize;
    int               m_nLastBandRasterIOYSize;
    GDALDataType      m_eLastBandRasterIODataType;

    GTAdjustment      m_eGTAdjustment;
    int               m_bNoDataDisabled;

    std::vector<GDALDataset*> m_apoDatasetsToClose{};

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

class CPL_DLL VRTRasterBand CPL_NON_FINAL: public GDALRasterBand
{
  protected:
    friend class VRTDataset;

    int            m_bIsMaskBand;

    int            m_bNoDataValueSet;
    // If set to true, will not report the existence of nodata.
    int            m_bHideNoDataValue;
    double         m_dfNoDataValue;

    std::unique_ptr<GDALColorTable> m_poColorTable{};

    GDALColorInterp m_eColorInterp;

    char           *m_pszUnitType;
    char           **m_papszCategoryNames;

    double         m_dfOffset;
    double         m_dfScale;

    CPLXMLNode    *m_psSavedHistograms;

    void           Initialize( int nXSize, int nYSize );

    std::vector<VRTOverviewInfo> m_aoOverviewInfos{};

    VRTRasterBand *m_poMaskBand;

    std::unique_ptr<GDALRasterAttributeTable> m_poRAT{};

    CPL_DISALLOW_COPY_ASSIGN(VRTRasterBand)

  public:

                    VRTRasterBand();
    virtual        ~VRTRasterBand();

    virtual CPLErr         XMLInit( CPLXMLNode *, const char *, void*,
                                    std::map<CPLString, GDALDataset*>& );
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

class CPL_DLL VRTSourcedRasterBand CPL_NON_FINAL: public VRTRasterBand
{
  private:
    int            m_nRecursionCounter;
    CPLString      m_osLastLocationInfo{};
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
                   VRTSourcedRasterBand( GDALDataset *poDS, int nBand,
                                         GDALDataType eType,
                                         int nXSize, int nYSize,
                                         int nBlockXSizeIn, int nBlockYSizeIn );
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

    virtual CPLErr         XMLInit( CPLXMLNode *, const char *, void*,
                                    std::map<CPLString, GDALDataset*>& ) override;
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

class CPL_DLL VRTWarpedRasterBand final: public VRTRasterBand
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

class VRTPansharpenedRasterBand final: public VRTRasterBand
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

class CPL_DLL VRTDerivedRasterBand CPL_NON_FINAL: public VRTSourcedRasterBand
{
    VRTDerivedRasterBandPrivateData* m_poPrivate;
    bool InitializePython();

    CPL_DISALLOW_COPY_ASSIGN(VRTDerivedRasterBand)

 public:
    char *pszFuncName;
    GDALDataType eSourceTransferType;

    using PixelFunc = std::function<CPLErr(void**, int, void*, int, int, GDALDataType, GDALDataType, int, int, CSLConstList)>;

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
    static CPLErr AddPixelFunction( const char *pszFuncName,
                                    GDALDerivedPixelFuncWithArgs pfnPixelFunc,
                                    const char *pszMetadata);

    static PixelFunc* GetPixelFunction( const char *pszFuncName );

    void SetPixelFunctionName( const char *pszFuncName );
    void SetSourceTransferType( GDALDataType eDataType );
    void SetPixelFunctionLanguage( const char* pszLanguage );

    virtual CPLErr         XMLInit( CPLXMLNode *, const char *, void*,
                                    std::map<CPLString, GDALDataset*>& ) override;
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

class CPL_DLL VRTRawRasterBand CPL_NON_FINAL: public VRTRasterBand
{
    RawRasterBand  *m_poRawRaster;

    char           *m_pszSourceFilename;
    int            m_bRelativeToVRT;

    CPL_DISALLOW_COPY_ASSIGN(VRTRawRasterBand)

  public:
                   VRTRawRasterBand( GDALDataset *poDS, int nBand,
                                     GDALDataType eType = GDT_Unknown );
    virtual        ~VRTRawRasterBand();

    virtual CPLErr         XMLInit( CPLXMLNode *, const char *, void*,
                                    std::map<CPLString, GDALDataset*>& ) override;
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

    CPLVirtualMem *GetVirtualMemAuto( GDALRWFlag eRWFlag,
                                      int *pnPixelSpace,
                                      GIntBig *pnLineSpace,
                                      char **papszOptions ) override;

    virtual void   GetFileList( char*** ppapszFileList, int *pnSize,
                                int *pnMaxSize, CPLHashSet* hSetFiles ) override;
};

/************************************************************************/
/*                              VRTDriver                               */
/************************************************************************/

class VRTDriver final: public GDALDriver
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
                              void* pUniqueHandle,
                              std::map<CPLString, GDALDataset*>& oMapSharedSources );
    void         AddSourceParser( const char *pszElementName,
                                  VRTSourceParser pfnParser );
};

/************************************************************************/
/*                           VRTSimpleSource                            */
/************************************************************************/

class CPL_DLL VRTSimpleSource CPL_NON_FINAL: public VRTSource
{
    CPL_DISALLOW_COPY_ASSIGN(VRTSimpleSource)

protected:
    friend class VRTSourcedRasterBand;
    friend class VRTDataset;

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

    int                 m_bNoDataSet;       // should really be a member of VRTComplexSource as only taken into account by it
    double              m_dfNoDataValue;    // same as above
    CPLString           m_osResampling{};

    int                 m_nMaxValue;

    int                 m_bRelativeToVRTOri;
    CPLString           m_osSourceFileNameOri{};
    int                 m_nExplicitSharedStatus; // -1 unknown, 0 = unshared, 1 = shared

    bool                m_bDropRefOnSrcBand;

    int                 NeedMaxValAdjustment() const;

public:
            VRTSimpleSource();
            VRTSimpleSource( const VRTSimpleSource* poSrcSource,
                             double dfXDstRatio, double dfYDstRatio );
    virtual ~VRTSimpleSource();

    virtual CPLErr  XMLInit( CPLXMLNode *psTree, const char *, void*,
                             std::map<CPLString, GDALDataset*>& ) override;
    virtual CPLXMLNode *SerializeToXML( const char *pszVRTPath ) override;

    void           SetSrcBand( GDALRasterBand * );
    void           SetSrcMaskBand( GDALRasterBand * );
    void           SetSrcWindow( double, double, double, double );
    void           SetDstWindow( double, double, double, double );
    void           SetNoDataValue( double dfNoDataValue );  // should really be a member of VRTComplexSource
    const CPLString& GetResampling() const { return m_osResampling; }
    void           SetResampling( const char* pszResampling );

    int            GetSrcDstWindow( double, double, double, double,
                                    int, int,
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
    GDALRasterBand* GetMaskBandMainBand() { return m_poMaskBandMainBand; }
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

class VRTAveragedSource final: public VRTSimpleSource
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

class CPL_DLL VRTComplexSource CPL_NON_FINAL: public VRTSimpleSource
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

    bool           m_bUseMaskBand = false;

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
    virtual CPLErr XMLInit( CPLXMLNode *, const char *, void*,
                            std::map<CPLString, GDALDataset*>& ) override;
    virtual const char* GetType() override { return "ComplexSource"; }

    double  LookupValue( double dfInput );

    void    SetUseMaskBand(bool bUseMaskBand) { m_bUseMaskBand = bUseMaskBand; }

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

class VRTFilteredSource CPL_NON_FINAL: public VRTComplexSource
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

class VRTKernelFilteredSource CPL_NON_FINAL: public VRTFilteredSource
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

    virtual CPLErr  XMLInit( CPLXMLNode *psTree, const char *, void*,
                             std::map<CPLString, GDALDataset*>& ) override;
    virtual CPLXMLNode *SerializeToXML( const char *pszVRTPath ) override;

    virtual CPLErr  FilterData( int nXSize, int nYSize, GDALDataType eType,
                                GByte *pabySrcData, GByte *pabyDstData ) override;

    CPLErr          SetKernel( int nKernelSize, bool bSeparable, double *padfCoefs );
    void            SetNormalized( int );
};

/************************************************************************/
/*                       VRTAverageFilteredSource                       */
/************************************************************************/

class VRTAverageFilteredSource final: public VRTKernelFilteredSource
{
    CPL_DISALLOW_COPY_ASSIGN(VRTAverageFilteredSource)

public:
            explicit VRTAverageFilteredSource( int nKernelSize );
    virtual ~VRTAverageFilteredSource();

    virtual CPLErr  XMLInit( CPLXMLNode *psTree, const char *, void*,
                             std::map<CPLString, GDALDataset*>& ) override;
    virtual CPLXMLNode *SerializeToXML( const char *pszVRTPath ) override;
};

/************************************************************************/
/*                            VRTFuncSource                             */
/************************************************************************/
class VRTFuncSource final: public VRTSource
{
    CPL_DISALLOW_COPY_ASSIGN(VRTFuncSource)

public:
            VRTFuncSource();
    virtual ~VRTFuncSource();

    virtual CPLErr  XMLInit( CPLXMLNode *, const char *, void*,
                             std::map<CPLString, GDALDataset*>& ) override { return CE_Failure; }
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

/************************************************************************/
/*                              VRTGroup                                */
/************************************************************************/

#ifdef TMPEXPORT
#define TMP_CPL_DLL CPL_DLL
#else
#define TMP_CPL_DLL
#endif

class VRTMDArray;
class VRTAttribute;
class VRTDimension;

class VRTGroup final: public GDALGroup
{
public:
    struct Ref
    {
        VRTGroup* m_ptr;
        explicit Ref(VRTGroup* ptr): m_ptr(ptr) {}
        Ref(const Ref&) = delete;
        Ref& operator=(const Ref&) = delete;
    };

private:
    std::shared_ptr<Ref> m_poSharedRefRootGroup{};
    std::weak_ptr<Ref> m_poWeakRefRootGroup{};
    std::shared_ptr<Ref> m_poRefSelf{};

    std::string m_osFilename{};
    mutable bool m_bDirty = false;
    std::string m_osVRTPath{};
    std::map<std::string, std::shared_ptr<VRTGroup>> m_oMapGroups{};
    std::map<std::string, std::shared_ptr<VRTMDArray>> m_oMapMDArrays{};
    std::map<std::string, std::shared_ptr<VRTAttribute>> m_oMapAttributes{};
    std::map<std::string, std::shared_ptr<VRTDimension>> m_oMapDimensions{};

    std::shared_ptr<VRTGroup> OpenGroupInternal(const std::string& osName) const;
    void SetRootGroupRef(const std::weak_ptr<Ref>& rgRef);
    std::weak_ptr<Ref> GetRootGroupRef() const;

public:

    VRTGroup(const std::string& osParentName, const std::string& osName);
    ~VRTGroup();

    bool XMLInit(const std::shared_ptr<VRTGroup>& poRoot,
                 const std::shared_ptr<VRTGroup>& poThisGroup,
                 const CPLXMLNode* psNode,
                 const char* pszVRTPath);

    std::vector<std::string> GetMDArrayNames(CSLConstList papszOptions) const override;
    std::shared_ptr<GDALMDArray> OpenMDArray(const std::string& osName,
                                             CSLConstList papszOptions = nullptr) const override;

    std::vector<std::string> GetGroupNames(CSLConstList papszOptions) const override;
    std::shared_ptr<GDALGroup> OpenGroup(const std::string& osName,
                                         CSLConstList) const override
    {
        return OpenGroupInternal(osName);
    }

    std::vector<std::shared_ptr<GDALDimension>> GetDimensions(CSLConstList) const override;

    std::vector<std::shared_ptr<GDALAttribute>> GetAttributes(CSLConstList) const override;

    std::shared_ptr<VRTDimension> GetDimension(const std::string& name) const {
        auto oIter = m_oMapDimensions.find(name);
        return oIter == m_oMapDimensions.end() ? nullptr : oIter->second;
    }
    std::shared_ptr<VRTDimension> GetDimensionFromFullName(const std::string& name,
                                                           bool bEmitError) const;

    std::shared_ptr<GDALGroup> CreateGroup(const std::string& osName,
                                           CSLConstList papszOptions = nullptr) override;

    std::shared_ptr<GDALDimension> CreateDimension(const std::string& osName,
                                                           const std::string& osType,
                                                           const std::string& osDirection,
                                                           GUInt64 nSize,
                                                           CSLConstList papszOptions = nullptr) override;

    std::shared_ptr<GDALAttribute> CreateAttribute(
        const std::string& osName,
        const std::vector<GUInt64>& anDimensions,
        const GDALExtendedDataType& oDataType,
        CSLConstList papszOptions = nullptr) override;

    std::shared_ptr<GDALMDArray> CreateMDArray(const std::string& osName,
                                                       const std::vector<std::shared_ptr<GDALDimension>>& aoDimensions,
                                                       const GDALExtendedDataType& oDataType,
                                                       CSLConstList papszOptions) override;

    void SetIsRootGroup();

    const std::shared_ptr<Ref>& GetRef() const { return m_poRefSelf; }
    VRTGroup* GetRootGroup() const;

    const std::string& GetVRTPath() const { return m_osVRTPath; }
    void SetDirty();
    void SetFilename(const std::string& osFilename) { m_osFilename = osFilename; }
    const std::string& GetFilename() const { return m_osFilename; }
    void Serialize() const;
    CPLXMLNode* SerializeToXML( const char *pszVRTPathIn ) const;
    void Serialize(CPLXMLNode* psParent, const char *pszVRTPathIn) const;
};

/************************************************************************/
/*                            VRTDimension                              */
/************************************************************************/

class VRTDimension final: public GDALDimension
{
    std::weak_ptr<VRTGroup::Ref> m_poGroupRef;
    std::string m_osIndexingVariableName;

public:
    VRTDimension(const std::shared_ptr<VRTGroup::Ref>& poGroupRef,
                  const std::string& osParentName,
                  const std::string& osName,
                  const std::string& osType,
                  const std::string& osDirection,
                  GUInt64 nSize,
                  const std::string& osIndexingVariableName):
        GDALDimension(osParentName, osName, osType, osDirection, nSize),
        m_poGroupRef(poGroupRef),
        m_osIndexingVariableName(osIndexingVariableName)
    {}

    VRTGroup* GetGroup() const;

    static std::shared_ptr<VRTDimension> Create(const std::shared_ptr<VRTGroup>& poThisGroup,
                                                const std::string& osParentName,
                                                const CPLXMLNode* psNode);

    std::shared_ptr<GDALMDArray> GetIndexingVariable() const override;

    bool SetIndexingVariable(std::shared_ptr<GDALMDArray> poIndexingVariable) override;

    void Serialize(CPLXMLNode* psParent) const;
};

/************************************************************************/
/*                            VRTAttribute                              */
/************************************************************************/

class VRTAttribute final: public GDALAttribute
{
    GDALExtendedDataType m_dt;
    std::vector<std::string> m_aosList{};
    std::vector<std::shared_ptr<GDALDimension>> m_dims{};

protected:

    bool IRead(const GUInt64* arrayStartIdx,
                      const size_t* count,
                      const GInt64* arrayStep,
                      const GPtrDiff_t* bufferStride,
                      const GDALExtendedDataType& bufferDataType,
                      void* pDstBuffer) const override;

    bool IWrite(const GUInt64* arrayStartIdx,
                      const size_t* count,
                      const GInt64* arrayStep,
                      const GPtrDiff_t* bufferStride,
                      const GDALExtendedDataType& bufferDataType,
                      const void* pSrcBuffer) override;


public:
    VRTAttribute(const std::string& osParentName,
                 const std::string& osName,
                 const GDALExtendedDataType& dt,
                 std::vector<std::string>&& aosList):
        GDALAbstractMDArray(osParentName, osName),
        GDALAttribute(osParentName, osName),
        m_dt(dt),
        m_aosList(std::move(aosList))
    {
        if( m_aosList.size() > 1 )
        {
            m_dims.emplace_back(std::make_shared<GDALDimension>(
                std::string(), "dim",
                std::string(), std::string(), m_aosList.size()));
        }
    }

    VRTAttribute(const std::string& osParentName,
                 const std::string& osName,
                 GUInt64 nDim,
                 const GDALExtendedDataType& dt):
        GDALAbstractMDArray(osParentName, osName),
        GDALAttribute(osParentName, osName),
        m_dt(dt)
    {
        if( nDim != 0 )
        {
            m_dims.emplace_back(std::make_shared<GDALDimension>(
                std::string(), "dim",
                std::string(), std::string(), nDim));
        }
    }

    static bool CreationCommonChecks(const std::string& osName,
                                     const std::vector<GUInt64>& anDimensions,
                                     const std::map<std::string, std::shared_ptr<VRTAttribute>>& oMapAttributes);

    static std::shared_ptr<VRTAttribute> Create(const std::string& osParentName,
                                                const CPLXMLNode* psNode);

    const std::vector<std::shared_ptr<GDALDimension>>& GetDimensions() const override { return m_dims; }

    const GDALExtendedDataType &GetDataType() const override { return m_dt; }

    void Serialize(CPLXMLNode* psParent) const;
};

/************************************************************************/
/*                          VRTMDArraySource                            */
/************************************************************************/

class VRTMDArraySource
{
public:
    virtual ~VRTMDArraySource() = default;

    virtual bool Read(const GUInt64* arrayStartIdx,
                      const size_t* count,
                      const GInt64* arrayStep,
                      const GPtrDiff_t* bufferStride,
                      const GDALExtendedDataType& bufferDataType,
                      void* pDstBuffer) const = 0;

    virtual void Serialize(CPLXMLNode* psParent, const char* pszVRTPath) const = 0;
};

/************************************************************************/
/*                            VRTMDArray                                */
/************************************************************************/

class VRTMDArray final: public GDALMDArray
{
protected:
    friend class VRTGroup; // for access to SetSelf()

    std::weak_ptr<VRTGroup::Ref> m_poGroupRef;
    std::string m_osVRTPath{};

    GDALExtendedDataType m_dt;
    std::vector<std::shared_ptr<GDALDimension>> m_dims;
    std::map<std::string, std::shared_ptr<VRTAttribute>> m_oMapAttributes{};
    std::vector<std::unique_ptr<VRTMDArraySource>> m_sources{};
    std::shared_ptr<OGRSpatialReference> m_poSRS{};
    std::vector<GByte> m_abyNoData{};
    std::string m_osUnit{};
    double m_dfScale = 1.0;
    double m_dfOffset = 0.0;
    bool m_bHasScale = false;
    bool m_bHasOffset = false;
    std::string m_osFilename{};

    bool IRead(const GUInt64* arrayStartIdx,
                      const size_t* count,
                      const GInt64* arrayStep,
                      const GPtrDiff_t* bufferStride,
                      const GDALExtendedDataType& bufferDataType,
                      void* pDstBuffer) const override;

    void SetDirty();

public:
    VRTMDArray(const std::shared_ptr<VRTGroup::Ref>& poGroupRef,
               const std::string& osParentName,
               const std::string& osName,
               const GDALExtendedDataType& dt,
               std::vector<std::shared_ptr<GDALDimension>>&& dims,
               std::map<std::string, std::shared_ptr<VRTAttribute>>&& oMapAttributes) :
        GDALAbstractMDArray(osParentName, osName),
        GDALMDArray(osParentName, osName),
        m_poGroupRef(poGroupRef),
        m_osVRTPath(poGroupRef->m_ptr->GetVRTPath()),
        m_dt(dt),
        m_dims(std::move(dims)),
        m_oMapAttributes(std::move(oMapAttributes)),
        m_osFilename(poGroupRef->m_ptr->GetFilename())
    {
    }

    VRTMDArray(const std::shared_ptr<VRTGroup::Ref>& poGroupRef,
               const std::string& osParentName,
               const std::string& osName,
               const std::vector<std::shared_ptr<GDALDimension>>& dims,
               const GDALExtendedDataType& dt) :
        GDALAbstractMDArray(osParentName, osName),
        GDALMDArray(osParentName, osName),
        m_poGroupRef(poGroupRef),
        m_osVRTPath(poGroupRef->m_ptr->GetVRTPath()),
        m_dt(dt),
        m_dims(dims),
        m_osFilename(poGroupRef->m_ptr->GetFilename())
    {
    }

    bool IsWritable() const override { return false; }

    const std::string& GetFilename() const override { return m_osFilename; }

    static std::shared_ptr<VRTMDArray> Create(const std::shared_ptr<VRTGroup>& poThisGroup,
                                              const std::string& osParentName,
                                              const CPLXMLNode* psNode);

    const std::vector<std::shared_ptr<GDALDimension>>& GetDimensions() const override { return m_dims; }

    std::vector<std::shared_ptr<GDALAttribute>> GetAttributes(CSLConstList) const override;

    const GDALExtendedDataType &GetDataType() const override { return m_dt; }

    bool SetSpatialRef(const OGRSpatialReference* poSRS) override;

    std::shared_ptr<OGRSpatialReference> GetSpatialRef() const override { return m_poSRS; }

    const void* GetRawNoDataValue() const override;

    bool SetRawNoDataValue(const void* pRawNoData) override;

    const std::string& GetUnit() const override { return m_osUnit; }

    bool SetUnit(const std::string& osUnit) override {
        m_osUnit = osUnit; return true; }

    double GetOffset(bool* pbHasOffset, GDALDataType* peStorageType) const override
    {
        if( pbHasOffset) *pbHasOffset = m_bHasOffset;
        if( peStorageType ) *peStorageType = GDT_Unknown;
        return m_dfOffset;
    }

    double GetScale(bool* pbHasScale, GDALDataType* peStorageType) const override
    {
        if( pbHasScale) *pbHasScale = m_bHasScale;
        if( peStorageType ) *peStorageType = GDT_Unknown;
        return m_dfScale;
    }

    bool SetOffset(double dfOffset, GDALDataType /* eStorageType */ = GDT_Unknown) override
    { SetDirty(); m_bHasOffset = true; m_dfOffset = dfOffset; return true; }

    bool SetScale(double dfScale, GDALDataType /* eStorageType */ = GDT_Unknown) override
    { SetDirty(); m_bHasScale = true; m_dfScale = dfScale; return true; }

    void AddSource(std::unique_ptr<VRTMDArraySource>&& poSource);

    std::shared_ptr<GDALAttribute> CreateAttribute(
        const std::string& osName,
        const std::vector<GUInt64>& anDimensions,
        const GDALExtendedDataType& oDataType,
        CSLConstList papszOptions = nullptr) override;

    bool CopyFrom(GDALDataset* poSrcDS,
                          const GDALMDArray* poSrcArray,
                          bool bStrict,
                          GUInt64& nCurCost,
                          const GUInt64 nTotalCost,
                          GDALProgressFunc pfnProgress,
                          void * pProgressData) override;

    void Serialize(CPLXMLNode* psParent, const char *pszVRTPathIn ) const;

    VRTGroup* GetGroup() const;

    const std::string& GetVRTPath() const { return m_osVRTPath; }
};

/************************************************************************/
/*                       VRTMDArraySourceInlinedValues                  */
/************************************************************************/

class VRTMDArraySourceInlinedValues final: public VRTMDArraySource
{
    const VRTMDArray* m_poDstArray = nullptr;
    bool m_bIsConstantValue;
    std::vector<GUInt64> m_anOffset{};
    std::vector<size_t> m_anCount{};
    std::vector<GByte> m_abyValues{};
    std::vector<size_t> m_anInlinedArrayStrideInBytes{};
    GDALExtendedDataType m_dt;

    VRTMDArraySourceInlinedValues(const VRTMDArraySourceInlinedValues&) = delete;
    VRTMDArraySourceInlinedValues& operator=(const VRTMDArraySourceInlinedValues&) = delete;

public:
    VRTMDArraySourceInlinedValues(const VRTMDArray* poDstArray,
                                  bool bIsConstantValue,
                                  std::vector<GUInt64>&& anOffset,
                                  std::vector<size_t>&& anCount,
                                  std::vector<GByte>&& abyValues):
        m_poDstArray(poDstArray),
        m_bIsConstantValue(bIsConstantValue),
        m_anOffset(std::move(anOffset)),
        m_anCount(std::move(anCount)),
        m_abyValues(std::move(abyValues)),
        m_dt(poDstArray->GetDataType())
    {
        const auto nDims(poDstArray->GetDimensionCount());
        m_anInlinedArrayStrideInBytes.resize(nDims);
        if( !bIsConstantValue && nDims > 0 )
        {
            m_anInlinedArrayStrideInBytes.back() = poDstArray->GetDataType().GetSize();
            for(size_t i = nDims - 1; i > 0; )
            {
                --i;
                m_anInlinedArrayStrideInBytes[i] =
                    m_anInlinedArrayStrideInBytes[i+1] * m_anCount[i+1];
            }
        }
    }

    ~VRTMDArraySourceInlinedValues();

    static std::unique_ptr<VRTMDArraySourceInlinedValues> Create(
                                                const VRTMDArray* poDstArray,
                                                const CPLXMLNode* psNode);

    bool Read(const GUInt64* arrayStartIdx,
                      const size_t* count,
                      const GInt64* arrayStep,
                      const GPtrDiff_t* bufferStride,
                      const GDALExtendedDataType& bufferDataType,
                      void* pDstBuffer) const override;

    void Serialize(CPLXMLNode* psParent, const char* pszVRTPath) const override;
};

/************************************************************************/
/*                     VRTMDArraySourceRegularlySpaced                  */
/************************************************************************/

class VRTMDArraySourceRegularlySpaced final: public VRTMDArraySource
{
    double m_dfStart;
    double m_dfIncrement;

public:
    VRTMDArraySourceRegularlySpaced(
                 double dfStart, double dfIncrement):
        m_dfStart(dfStart),
        m_dfIncrement(dfIncrement)
    {
    }

    bool Read(const GUInt64* arrayStartIdx,
                      const size_t* count,
                      const GInt64* arrayStep,
                      const GPtrDiff_t* bufferStride,
                      const GDALExtendedDataType& bufferDataType,
                      void* pDstBuffer) const override;

    void Serialize(CPLXMLNode* psParent, const char* pszVRTPath) const override;
};

/************************************************************************/
/*                       VRTMDArraySourceFromArray                      */
/************************************************************************/

class VRTMDArraySourceFromArray final: public VRTMDArraySource
{
    const VRTMDArray* m_poDstArray = nullptr;
    bool m_bRelativeToVRTSet = false;
    bool m_bRelativeToVRT = false;
    std::string m_osFilename{};
    std::string m_osArray{};
    std::string m_osBand{};
    std::vector<int> m_anTransposedAxis{};
    std::string m_osViewExpr{};
    std::vector<GUInt64> m_anSrcOffset{};
    mutable std::vector<GUInt64> m_anCount{};
    std::vector<GUInt64> m_anStep{};
    std::vector<GUInt64> m_anDstOffset{};

    VRTMDArraySourceFromArray(const VRTMDArraySourceFromArray&) = delete;
    VRTMDArraySourceFromArray& operator=(const VRTMDArraySourceFromArray&) = delete;

public:
    VRTMDArraySourceFromArray(const VRTMDArray* poDstArray,
                              bool bRelativeToVRTSet,
                              bool bRelativeToVRT,
                              const std::string& osFilename,
                              const std::string& osArray,
                              const std::string& osBand,
                              std::vector<int>&& anTransposedAxis,
                              const std::string& osViewExpr,
                              std::vector<GUInt64>&& anSrcOffset,
                              std::vector<GUInt64>&& anCount,
                              std::vector<GUInt64>&& anStep,
                              std::vector<GUInt64>&& anDstOffset):
        m_poDstArray(poDstArray),
        m_bRelativeToVRTSet(bRelativeToVRTSet),
        m_bRelativeToVRT(bRelativeToVRT),
        m_osFilename(osFilename),
        m_osArray(osArray),
        m_osBand(osBand),
        m_anTransposedAxis(std::move(anTransposedAxis)),
        m_osViewExpr(osViewExpr),
        m_anSrcOffset(std::move(anSrcOffset)),
        m_anCount(std::move(anCount)),
        m_anStep(std::move(anStep)),
        m_anDstOffset(std::move(anDstOffset))
    {
    }

    ~VRTMDArraySourceFromArray() override;

    static std::unique_ptr<VRTMDArraySourceFromArray> Create(
                                                const VRTMDArray* poDstArray,
                                                const CPLXMLNode* psNode);

    bool Read(const GUInt64* arrayStartIdx,
                      const size_t* count,
                      const GInt64* arrayStep,
                      const GPtrDiff_t* bufferStride,
                      const GDALExtendedDataType& bufferDataType,
                      void* pDstBuffer) const override;

    void Serialize(CPLXMLNode* psParent, const char* pszVRTPath) const override;
};

#endif /* #ifndef DOXYGEN_SKIP */

#endif /* ndef VIRTUALDATASET_H_INCLUDED */
