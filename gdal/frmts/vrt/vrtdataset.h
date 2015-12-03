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

#include "cpl_hash_set.h"
#include "gdal_pam.h"
#include "gdal_priv.h"
#include "gdal_vrt.h"

#include <map>
#include <vector>

int VRTApplyMetadata( CPLXMLNode *, GDALMajorObject * );
CPLXMLNode *VRTSerializeMetadata( GDALMajorObject * );

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
public:
    CPLString       osFilename;
    int             nBand;
    GDALRasterBand *poBand;
    int             bTriedToOpen;

    VRTOverviewInfo() : nBand(0), poBand(NULL), bTriedToOpen(FALSE) {}
    ~VRTOverviewInfo() {
        if( poBand == NULL )
            /* do nothing */;
        else if( poBand->GetDataset()->GetShared() )
            GDALClose( (GDALDatasetH) poBand->GetDataset() );
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

    virtual CPLErr  RasterIO( int nXOff, int nYOff, int nXSize, int nYSize, 
                              void *pData, int nBufXSize, int nBufYSize, 
                              GDALDataType eBufType, 
                              GSpacing nPixelSpace, GSpacing nLineSpace,
                              GDALRasterIOExtraArg* psExtraArg ) = 0;

    virtual double GetMinimum( int nXSize, int nYSize, int *pbSuccess ) = 0;
    virtual double GetMaximum( int nXSize, int nYSize, int *pbSuccess ) = 0;
    virtual CPLErr ComputeRasterMinMax( int nXSize, int nYSize, int bApproxOK, double* adfMinMax ) = 0;
    virtual CPLErr ComputeStatistics( int nXSize, int nYSize,
                                      int bApproxOK, 
                                      double *pdfMin, double *pdfMax, 
                                      double *pdfMean, double *pdfStdDev,
                                      GDALProgressFunc pfnProgress, void *pProgressData ) = 0;
    virtual CPLErr  GetHistogram( int nXSize, int nYSize,
                                  double dfMin, double dfMax,
                                  int nBuckets, GUIntBig * panHistogram,
                                  int bIncludeOutOfRange, int bApproxOK,
                                  GDALProgressFunc pfnProgress, void *pProgressData ) = 0;

    virtual CPLErr  XMLInit( CPLXMLNode *psTree, const char * ) = 0;
    virtual CPLXMLNode *SerializeToXML( const char *pszVRTPath ) = 0;

    virtual void   GetFileList(char*** ppapszFileList, int *pnSize,
                               int *pnMaxSize, CPLHashSet* hSetFiles);

    virtual int    IsSimpleSource() { return FALSE; }
};

typedef VRTSource *(*VRTSourceParser)(CPLXMLNode *, const char *);

VRTSource *VRTParseCoreSources( CPLXMLNode *psTree, const char * );
VRTSource *VRTParseFilterSources( CPLXMLNode *psTree, const char * );

/************************************************************************/
/*                              VRTDataset                              */
/************************************************************************/

class VRTRasterBand;

class CPL_DLL VRTDataset : public GDALDataset
{
    friend class VRTRasterBand;

    char           *m_pszProjection;

    int            m_bGeoTransformSet;
    double         m_adfGeoTransform[6];

    int           m_nGCPCount;
    GDAL_GCP      *m_pasGCPList;
    char          *m_pszGCPProjection;

    int            m_bNeedsFlush;
    int            m_bWritable;

    char          *m_pszVRTPath;

    VRTRasterBand *m_poMaskBand;

    int            m_bCompatibleForDatasetIO;
    int            CheckCompatibleForDatasetIO();
    std::vector<GDALDataset*> m_apoOverviews;
    std::vector<GDALDataset*> m_apoOverviewsBak;

  protected:
    virtual int         CloseDependentDatasets();

  public:
                 VRTDataset(int nXSize, int nYSize);
                ~VRTDataset();

    void          SetNeedsFlush() { m_bNeedsFlush = TRUE; }
    virtual void  FlushCache();

    void SetWritable(int bWritableIn) { m_bWritable = bWritableIn; }

    virtual CPLErr          CreateMaskBand( int nFlags );
    void SetMaskBand(VRTRasterBand* poMaskBand);

    virtual const char *GetProjectionRef(void);
    virtual CPLErr SetProjection( const char * );
    virtual CPLErr GetGeoTransform( double * );
    virtual CPLErr SetGeoTransform( double * );

    virtual CPLErr SetMetadata( char **papszMD, const char *pszDomain = "" );
    virtual CPLErr SetMetadataItem( const char *pszName, const char *pszValue,
                                    const char *pszDomain = "" );

    virtual char** GetMetadata( const char *pszDomain = "" );

    virtual int    GetGCPCount();
    virtual const char *GetGCPProjection();
    virtual const GDAL_GCP *GetGCPs();
    virtual CPLErr SetGCPs( int nGCPCount, const GDAL_GCP *pasGCPList,
                            const char *pszGCPProjection );

    virtual CPLErr AddBand( GDALDataType eType, 
                            char **papszOptions=NULL );

    virtual char      **GetFileList();

    virtual CPLErr  IRasterIO( GDALRWFlag eRWFlag,
                               int nXOff, int nYOff, int nXSize, int nYSize,
                               void * pData, int nBufXSize, int nBufYSize,
                               GDALDataType eBufType,
                               int nBandCount, int *panBandMap,
                               GSpacing nPixelSpace, GSpacing nLineSpace,
                               GSpacing nBandSpace,
                               GDALRasterIOExtraArg* psExtraArg);

    virtual CPLXMLNode *SerializeToXML( const char *pszVRTPath);
    virtual CPLErr      XMLInit( CPLXMLNode *, const char * );

    virtual CPLErr IBuildOverviews( const char *, int, int *,
                                    int, int *, GDALProgressFunc, void * );

    /* Used by PDF driver for example */
    GDALDataset*        GetSingleSimpleSource();
    void                BuildVirtualOverviews();

    void                UnsetPreservedRelativeFilenames();

    static int          Identify( GDALOpenInfo * );
    static GDALDataset *Open( GDALOpenInfo * );
    static GDALDataset *OpenXML( const char *, const char * = NULL, GDALAccess eAccess = GA_ReadOnly );
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

    friend class VRTWarpedRasterBand;

  protected:
    virtual int         CloseDependentDatasets();

public:
                      VRTWarpedDataset( int nXSize, int nYSize );
                     ~VRTWarpedDataset();

    CPLErr            Initialize( /* GDALWarpOptions */ void * );

    virtual CPLErr IBuildOverviews( const char *, int, int *,
                                    int, int *, GDALProgressFunc, void * );

    virtual CPLErr SetMetadataItem( const char *pszName, const char *pszValue,
                                    const char *pszDomain = "" );

    virtual CPLXMLNode *SerializeToXML( const char *pszVRTPath );
    virtual CPLErr    XMLInit( CPLXMLNode *, const char * );

    virtual CPLErr AddBand( GDALDataType eType, 
                            char **papszOptions=NULL );

    virtual char      **GetFileList();

    CPLErr            ProcessBlock( int iBlockX, int iBlockY );

    void              GetBlockSize( int *, int * );
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
    std::map<CPLString,CPLString> m_oMapToRelativeFilenames; // map from absolute to relative

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

  protected:
    virtual int         CloseDependentDatasets();

public:
                      VRTPansharpenedDataset( int nXSize, int nYSize );
                     ~VRTPansharpenedDataset();

    virtual CPLErr    XMLInit( CPLXMLNode *, const char * );
    virtual CPLXMLNode *   SerializeToXML( const char *pszVRTPath );

    CPLErr            XMLInit( CPLXMLNode *psTree, const char *pszVRTPath,
                                        GDALRasterBandH hPanchroBandIn,
                                        int nInputSpectralBandsIn,
                                        GDALRasterBandH* pahInputSpectralBandsIn );

    virtual CPLErr AddBand( GDALDataType eType, 
                            char **papszOptions=NULL );

    virtual char      **GetFileList();

    virtual CPLErr  IRasterIO( GDALRWFlag eRWFlag,
                               int nXOff, int nYOff, int nXSize, int nYSize,
                               void * pData, int nBufXSize, int nBufYSize,
                               GDALDataType eBufType,
                               int nBandCount, int *panBandMap,
                               GSpacing nPixelSpace, GSpacing nLineSpace,
                               GSpacing nBandSpace,
                               GDALRasterIOExtraArg* psExtraArg);

    void              GetBlockSize( int *, int * );

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
    int            m_bHideNoDataValue; // If set to true, will not report the existence of nodata
    double         m_dfNoDataValue;

    GDALColorTable *m_poColorTable;

    GDALColorInterp m_eColorInterp;

    char           *m_pszUnitType;
    char           **m_papszCategoryNames;

    double         m_dfOffset;
    double         m_dfScale;

    CPLXMLNode    *m_psSavedHistograms;

    void           Initialize( int nXSize, int nYSize );

    std::vector<VRTOverviewInfo> m_apoOverviews;

    VRTRasterBand *m_poMaskBand;

  public:

                    VRTRasterBand();
    virtual        ~VRTRasterBand();

    virtual CPLErr         XMLInit( CPLXMLNode *, const char * );
    virtual CPLXMLNode *   SerializeToXML( const char *pszVRTPath );

    virtual CPLErr SetNoDataValue( double );
    virtual double GetNoDataValue( int *pbSuccess = NULL );
    virtual CPLErr DeleteNoDataValue();

    virtual CPLErr SetColorTable( GDALColorTable * ); 
    virtual GDALColorTable *GetColorTable();

    virtual CPLErr SetColorInterpretation( GDALColorInterp );
    virtual GDALColorInterp GetColorInterpretation();

    virtual const char *GetUnitType();
    CPLErr SetUnitType( const char * ); 

    virtual char **GetCategoryNames();
    virtual CPLErr SetCategoryNames( char ** );

    virtual CPLErr SetMetadata( char **papszMD, const char *pszDomain = "" );
    virtual CPLErr SetMetadataItem( const char *pszName, const char *pszValue,
                                    const char *pszDomain = "" );

    virtual double GetOffset( int *pbSuccess = NULL );
    CPLErr SetOffset( double );
    virtual double GetScale( int *pbSuccess = NULL );
    CPLErr SetScale( double );

    virtual int GetOverviewCount();
    virtual GDALRasterBand *GetOverview(int);

    virtual CPLErr  GetHistogram( double dfMin, double dfMax,
                          int nBuckets, GUIntBig * panHistogram,
                          int bIncludeOutOfRange, int bApproxOK,
                          GDALProgressFunc, void *pProgressData );

    virtual CPLErr GetDefaultHistogram( double *pdfMin, double *pdfMax,
                                        int *pnBuckets, GUIntBig ** ppanHistogram,
                                        int bForce,
                                        GDALProgressFunc, void *pProgressData);

    virtual CPLErr SetDefaultHistogram( double dfMin, double dfMax,
                                        int nBuckets, GUIntBig *panHistogram );

    CPLErr         CopyCommonInfoFrom( GDALRasterBand * );

    virtual void   GetFileList(char*** ppapszFileList, int *pnSize,
                               int *pnMaxSize, CPLHashSet* hSetFiles);

    virtual void   SetDescription( const char * );

    virtual GDALRasterBand *GetMaskBand();
    virtual int             GetMaskFlags();

    virtual CPLErr          CreateMaskBand( int nFlags );

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

    void           Initialize( int nXSize, int nYSize );

    int            CanUseSourcesMinMaxImplementations();

  public:
    int            nSources;
    VRTSource    **papoSources;
    int            bEqualAreas;

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
                              GDALRasterIOExtraArg* psExtraArg);

    virtual char      **GetMetadataDomainList();
    virtual const char *GetMetadataItem( const char * pszName,
                                         const char * pszDomain = "" );
    virtual char      **GetMetadata( const char * pszDomain = "" );
    virtual CPLErr      SetMetadata( char ** papszMetadata,
                                     const char * pszDomain = "" );
    virtual CPLErr      SetMetadataItem( const char * pszName,
                                         const char * pszValue,
                                         const char * pszDomain = "" );

    virtual CPLErr         XMLInit( CPLXMLNode *, const char * );
    virtual CPLXMLNode *   SerializeToXML( const char *pszVRTPath );

    virtual double GetMinimum( int *pbSuccess = NULL );
    virtual double GetMaximum(int *pbSuccess = NULL );
    virtual CPLErr ComputeRasterMinMax( int bApproxOK, double* adfMinMax );
    virtual CPLErr ComputeStatistics( int bApproxOK, 
                                      double *pdfMin, double *pdfMax, 
                                      double *pdfMean, double *pdfStdDev,
                                      GDALProgressFunc pfnProgress, void *pProgressData );
    virtual CPLErr  GetHistogram( double dfMin, double dfMax,
                                  int nBuckets, GUIntBig * panHistogram,
                                  int bIncludeOutOfRange, int bApproxOK,
                                  GDALProgressFunc pfnProgress, void *pProgressData );

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
                                      double dfSrcXSize=-1, double dfSrcYSize=-1,
                                      double dfDstXOff=-1, double dfDstYOff=-1,
                                      double dfDstXSize=-1, double dfDstYSize=-1 );

    CPLErr         AddFuncSource( VRTImageReadFunc pfnReadFunc, void *hCBData,
                                  double dfNoDataValue = VRT_NODATA_UNSET );

    void           ConfigureSource(VRTSimpleSource *poSimpleSource,
                                           GDALRasterBand *poSrcBand,
                                           int bAddAsMaskBand,
                                           double dfSrcXOff, double dfSrcYOff,
                                           double dfSrcXSize, double dfSrcYSize,
                                           double dfDstXOff, double dfDstYOff,
                                           double dfDstXSize, double dfDstYSize);

    virtual CPLErr IReadBlock( int, int, void * );

    virtual void   GetFileList(char*** ppapszFileList, int *pnSize,
                               int *pnMaxSize, CPLHashSet* hSetFiles);

    virtual int         CloseDependentDatasets();

    virtual int         IsSourcedRasterBand() { return TRUE; }
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

    virtual CPLErr         XMLInit( CPLXMLNode *, const char * );
    virtual CPLXMLNode *   SerializeToXML( const char *pszVRTPath );

    virtual CPLErr IReadBlock( int, int, void * );
    virtual CPLErr IWriteBlock( int, int, void * );

    virtual int GetOverviewCount();
    virtual GDALRasterBand *GetOverview(int);
};
/************************************************************************/
/*                        VRTPansharpenedRasterBand                     */
/************************************************************************/

class VRTPansharpenedRasterBand : public VRTRasterBand
{
    int               m_nIndexAsPansharpenedBand;

  public:
                   VRTPansharpenedRasterBand( GDALDataset *poDS, int nBand,
                                              GDALDataType eDataType = GDT_Unknown );
    virtual        ~VRTPansharpenedRasterBand();

    virtual CPLErr         XMLInit( CPLXMLNode *, const char * );
    virtual CPLXMLNode *   SerializeToXML( const char *pszVRTPath );

    virtual CPLErr IReadBlock( int, int, void * );

    virtual CPLErr  IRasterIO( GDALRWFlag eRWFlag,
                               int nXOff, int nYOff, int nXSize, int nYSize,
                               void * pData, int nBufXSize, int nBufYSize,
                               GDALDataType eBufType,
                               GSpacing nPixelSpace, GSpacing nLineSpace,
                               GDALRasterIOExtraArg* psExtraArg);

    virtual int GetOverviewCount();
    virtual GDALRasterBand *GetOverview(int);

    virtual int         IsPansharpenRasterBand() { return TRUE; }

    void                SetIndexAsPansharpenedBand(int nIdx) { m_nIndexAsPansharpenedBand = nIdx; }
    int                 GetIndexAsPansharpenedBand() const { return m_nIndexAsPansharpenedBand; }
};

/************************************************************************/
/*                         VRTDerivedRasterBand                         */
/************************************************************************/

class CPL_DLL VRTDerivedRasterBand : public VRTSourcedRasterBand
{

 public:
    char *pszFuncName;
    GDALDataType eSourceTransferType;

    VRTDerivedRasterBand(GDALDataset *poDS, int nBand);
    VRTDerivedRasterBand(GDALDataset *poDS, int nBand, 
                         GDALDataType eType, int nXSize, int nYSize);
    virtual        ~VRTDerivedRasterBand();

    virtual CPLErr IRasterIO( GDALRWFlag, int, int, int, int,
                              void *, int, int, GDALDataType,
                              GSpacing nPixelSpace, GSpacing nLineSpace,
                              GDALRasterIOExtraArg* psExtraArg );

    static CPLErr AddPixelFunction
        (const char *pszFuncName, GDALDerivedPixelFunc pfnPixelFunc);
    static GDALDerivedPixelFunc GetPixelFunction(const char *pszFuncName);

    void SetPixelFunctionName(const char *pszFuncName);
    void SetSourceTransferType(GDALDataType eDataType);

    virtual CPLErr         XMLInit( CPLXMLNode *, const char * );
    virtual CPLXMLNode *   SerializeToXML( const char *pszVRTPath );

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

  public:
                   VRTRawRasterBand( GDALDataset *poDS, int nBand,
                                     GDALDataType eType = GDT_Unknown );
    virtual        ~VRTRawRasterBand();

    virtual CPLErr         XMLInit( CPLXMLNode *, const char * );
    virtual CPLXMLNode *   SerializeToXML( const char *pszVRTPath );

    virtual CPLErr IRasterIO( GDALRWFlag, int, int, int, int,
                              void *, int, int, GDALDataType,
                              GSpacing nPixelSpace, GSpacing nLineSpace,
                              GDALRasterIOExtraArg* psExtraArg );

    virtual CPLErr IReadBlock( int, int, void * );
    virtual CPLErr IWriteBlock( int, int, void * );

    CPLErr         SetRawLink( const char *pszFilename, 
                               const char *pszVRTPath,
                               int bRelativeToVRT, 
                               vsi_l_offset nImageOffset, 
                               int nPixelOffset, int nLineOffset, 
                               const char *pszByteOrder );

    void           ClearRawLink();

    virtual void   GetFileList(char*** ppapszFileList, int *pnSize,
                               int *pnMaxSize, CPLHashSet* hSetFiles);
};

/************************************************************************/
/*                              VRTDriver                               */
/************************************************************************/

class VRTDriver : public GDALDriver
{
    void        *m_pDeserializerData;

  public:
                 VRTDriver();
                 ~VRTDriver();

    char         **papszSourceParsers;

    virtual char      **GetMetadataDomainList();
    virtual char      **GetMetadata( const char * pszDomain = "" );
    virtual CPLErr      SetMetadata( char ** papszMetadata,
                                     const char * pszDomain = "" );

    VRTSource   *ParseSource( CPLXMLNode *psSrc, const char *pszVRTPath );
    void         AddSourceParser( const char *pszElementName, 
                                  VRTSourceParser pfnParser );
};

/************************************************************************/
/*                           VRTSimpleSource                            */
/************************************************************************/

class CPL_DLL VRTSimpleSource : public VRTSource
{
protected:
    GDALRasterBand      *m_poRasterBand;

    /* when poRasterBand is a mask band, poMaskBandMainBand is the band */
    /* from which the mask band is taken */
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

    int                 NeedMaxValAdjustment() const;

public:
            VRTSimpleSource();
            VRTSimpleSource(const VRTSimpleSource* poSrcSource,
                                 double dfXDstRatio, double dfYDstRatio);
    virtual ~VRTSimpleSource();

    virtual CPLErr  XMLInit( CPLXMLNode *psTree, const char * );
    virtual CPLXMLNode *SerializeToXML( const char *pszVRTPath );

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

    virtual CPLErr  RasterIO( int nXOff, int nYOff, int nXSize, int nYSize, 
                              void *pData, int nBufXSize, int nBufYSize, 
                              GDALDataType eBufType, 
                              GSpacing nPixelSpace, GSpacing nLineSpace,
                              GDALRasterIOExtraArg* psExtraArg );

    virtual double GetMinimum( int nXSize, int nYSize, int *pbSuccess );
    virtual double GetMaximum( int nXSize, int nYSize, int *pbSuccess );
    virtual CPLErr ComputeRasterMinMax( int nXSize, int nYSize, int bApproxOK, double* adfMinMax );
    virtual CPLErr ComputeStatistics( int nXSize, int nYSize,
                                      int bApproxOK, 
                                      double *pdfMin, double *pdfMax, 
                                      double *pdfMean, double *pdfStdDev,
                                      GDALProgressFunc pfnProgress, void *pProgressData );
    virtual CPLErr  GetHistogram( int nXSize, int nYSize,
                                  double dfMin, double dfMax,
                                  int nBuckets, GUIntBig * panHistogram,
                                  int bIncludeOutOfRange, int bApproxOK,
                                  GDALProgressFunc pfnProgress, void *pProgressData );

    void            DstToSrc( double dfX, double dfY,
                              double &dfXOut, double &dfYOut );
    void            SrcToDst( double dfX, double dfY,
                              double &dfXOut, double &dfYOut );

    virtual void   GetFileList(char*** ppapszFileList, int *pnSize,
                               int *pnMaxSize, CPLHashSet* hSetFiles);

    virtual int    IsSimpleSource() { return TRUE; }
    virtual const char* GetType() { return "SimpleSource"; }

    GDALRasterBand* GetBand();
    int             IsSameExceptBandNumber(VRTSimpleSource* poOtherSource);
    CPLErr          DatasetRasterIO(
                               int nXOff, int nYOff, int nXSize, int nYSize,
                               void * pData, int nBufXSize, int nBufYSize,
                               GDALDataType eBufType,
                               int nBandCount, int *panBandMap,
                               GSpacing nPixelSpace, GSpacing nLineSpace,
                               GSpacing nBandSpace,
                               GDALRasterIOExtraArg* psExtraArg);

    void             UnsetPreservedRelativeFilenames();

    void                SetMaxValue(int nVal) { m_nMaxValue = nVal; }
};

/************************************************************************/
/*                          VRTAveragedSource                           */
/************************************************************************/

class VRTAveragedSource : public VRTSimpleSource
{
public:
                    VRTAveragedSource();
    virtual CPLErr  RasterIO( int nXOff, int nYOff, int nXSize, int nYSize, 
                              void *pData, int nBufXSize, int nBufYSize, 
                              GDALDataType eBufType, 
                              GSpacing nPixelSpace, GSpacing nLineSpace,
                              GDALRasterIOExtraArg* psExtraArg );

    virtual double GetMinimum( int nXSize, int nYSize, int *pbSuccess );
    virtual double GetMaximum( int nXSize, int nYSize, int *pbSuccess );
    virtual CPLErr ComputeRasterMinMax( int nXSize, int nYSize, int bApproxOK, double* adfMinMax );
    virtual CPLErr ComputeStatistics( int nXSize, int nYSize,
                                      int bApproxOK, 
                                      double *pdfMin, double *pdfMax, 
                                      double *pdfMean, double *pdfStdDev,
                                      GDALProgressFunc pfnProgress, void *pProgressData );
    virtual CPLErr  GetHistogram( int nXSize, int nYSize,
                                  double dfMin, double dfMax,
                                  int nBuckets, GUIntBig * panHistogram,
                                  int bIncludeOutOfRange, int bApproxOK,
                                  GDALProgressFunc pfnProgress, void *pProgressData );

    virtual CPLXMLNode *SerializeToXML( const char *pszVRTPath );
    virtual const char* GetType() { return "AveragedSource"; }
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
protected:
    VRTComplexSourceScaling m_eScalingType;
    double         m_dfScaleOff; /* for linear scaling */
    double         m_dfScaleRatio; /* for linear scaling */

    /* For non-linear scaling with a power function. */
    int            m_bSrcMinMaxDefined;
    double         m_dfSrcMin;
    double         m_dfSrcMax;
    double         m_dfDstMin;
    double         m_dfDstMax;
    double         m_dfExponent;

    int            m_nColorTableComponent;

    CPLErr          RasterIOInternal( int nReqXOff, int nReqYOff,
                                      int nReqXSize, int nReqYSize,
                                      void *pData, int nOutXSize, int nOutYSize,
                                      GDALDataType eBufType,
                                      GSpacing nPixelSpace, GSpacing nLineSpace,
                                      GDALRasterIOExtraArg* psExtraArg );

public:
                   VRTComplexSource();
                   VRTComplexSource(const VRTComplexSource* poSrcSource,
                                    double dfXDstRatio, double dfYDstRatio);
    virtual        ~VRTComplexSource();

    virtual CPLErr RasterIO( int nXOff, int nYOff, int nXSize, int nYSize, 
                             void *pData, int nBufXSize, int nBufYSize, 
                             GDALDataType eBufType, 
                             GSpacing nPixelSpace, GSpacing nLineSpace,
                             GDALRasterIOExtraArg* psExtraArg );

    virtual double GetMinimum( int nXSize, int nYSize, int *pbSuccess );
    virtual double GetMaximum( int nXSize, int nYSize, int *pbSuccess );
    virtual CPLErr ComputeRasterMinMax( int nXSize, int nYSize, int bApproxOK, double* adfMinMax );
    virtual CPLErr ComputeStatistics( int nXSize, int nYSize,
                                      int bApproxOK, 
                                      double *pdfMin, double *pdfMax, 
                                      double *pdfMean, double *pdfStdDev,
                                      GDALProgressFunc pfnProgress, void *pProgressData );
    virtual CPLErr  GetHistogram( int nXSize, int nYSize,
                                  double dfMin, double dfMax,
                                  int nBuckets, GUIntBig * panHistogram,
                                  int bIncludeOutOfRange, int bApproxOK,
                                  GDALProgressFunc pfnProgress, void *pProgressData );

    virtual CPLXMLNode *SerializeToXML( const char *pszVRTPath );
    virtual CPLErr XMLInit( CPLXMLNode *, const char * );
    virtual const char* GetType() { return "ComplexSource"; }

    double  LookupValue( double dfInput );

    void    SetLinearScaling(double dfOffset, double dfScale);
    void    SetPowerScaling(double dfExponent,
                            double dfSrcMin,
                            double dfSrcMax,
                            double dfDstMin,
                            double dfDstMax);
    void    SetColorTableComponent(int nComponent);

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
    int          IsTypeSupported( GDALDataType eType );

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

    virtual CPLErr  RasterIO( int nXOff, int nYOff, int nXSize, int nYSize, 
                              void *pData, int nBufXSize, int nBufYSize, 
                              GDALDataType eBufType, 
                              GSpacing nPixelSpace, GSpacing nLineSpace,
                              GDALRasterIOExtraArg* psExtraArg );
};

/************************************************************************/
/*                       VRTKernelFilteredSource                        */
/************************************************************************/

class VRTKernelFilteredSource : public VRTFilteredSource
{
protected:
    int     m_nKernelSize;

    double  *m_padfKernelCoefs;

    int     m_bNormalized;

public:
            VRTKernelFilteredSource();
    virtual ~VRTKernelFilteredSource();

    virtual CPLErr  XMLInit( CPLXMLNode *psTree, const char * );
    virtual CPLXMLNode *SerializeToXML( const char *pszVRTPath );

    virtual CPLErr  FilterData( int nXSize, int nYSize, GDALDataType eType, 
                                GByte *pabySrcData, GByte *pabyDstData );

    CPLErr          SetKernel( int nKernelSize, double *padfCoefs );
    void            SetNormalized( int );
};

/************************************************************************/
/*                       VRTAverageFilteredSource                       */
/************************************************************************/

class VRTAverageFilteredSource : public VRTKernelFilteredSource
{
public:
            explicit VRTAverageFilteredSource( int nKernelSize );
    virtual ~VRTAverageFilteredSource();

    virtual CPLErr  XMLInit( CPLXMLNode *psTree, const char * );
    virtual CPLXMLNode *SerializeToXML( const char *pszVRTPath );
};

/************************************************************************/
/*                            VRTFuncSource                             */
/************************************************************************/
class VRTFuncSource : public VRTSource
{
public:
            VRTFuncSource();
    virtual ~VRTFuncSource();

    virtual CPLErr  XMLInit( CPLXMLNode *, const char *) { return CE_Failure; }
    virtual CPLXMLNode *SerializeToXML( const char *pszVRTPath );

    virtual CPLErr  RasterIO( int nXOff, int nYOff, int nXSize, int nYSize, 
                              void *pData, int nBufXSize, int nBufYSize, 
                              GDALDataType eBufType, 
                              GSpacing nPixelSpace, GSpacing nLineSpace,
                              GDALRasterIOExtraArg* psExtraArg );

    virtual double GetMinimum( int nXSize, int nYSize, int *pbSuccess );
    virtual double GetMaximum( int nXSize, int nYSize, int *pbSuccess );
    virtual CPLErr ComputeRasterMinMax( int nXSize, int nYSize, int bApproxOK, double* adfMinMax );
    virtual CPLErr ComputeStatistics( int nXSize, int nYSize,
                                      int bApproxOK, 
                                      double *pdfMin, double *pdfMax, 
                                      double *pdfMean, double *pdfStdDev,
                                      GDALProgressFunc pfnProgress, void *pProgressData );
    virtual CPLErr  GetHistogram( int nXSize, int nYSize,
                                  double dfMin, double dfMax,
                                  int nBuckets, GUIntBig * panHistogram,
                                  int bIncludeOutOfRange, int bApproxOK,
                                  GDALProgressFunc pfnProgress, void *pProgressData );

    VRTImageReadFunc    pfnReadFunc;
    void               *pCBData;
    GDALDataType        eType;

    float               fNoDataValue;
};

#endif /* ndef VIRTUALDATASET_H_INCLUDED */
