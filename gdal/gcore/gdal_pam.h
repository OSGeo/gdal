/******************************************************************************
 * $Id$
 *
 * Project:  GDAL Core
 * Purpose:  Declaration for Peristable Auxiliary Metadata classes.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2005, Frank Warmerdam <warmerdam@pobox.com>
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

#ifndef GDAL_PAM_H_INCLUDED
#define GDAL_PAM_H_INCLUDED

//! @cond Doxygen_Suppress

#include "cpl_minixml.h"
#include "gdal_priv.h"
#include <map>
#include <vector>

class GDALPamRasterBand;

/* Clone Info Flags */

#define GCIF_GEOTRANSFORM       0x01
#define GCIF_PROJECTION         0x02
#define GCIF_METADATA           0x04
#define GCIF_GCPS               0x08

#define GCIF_NODATA             0x001000
#define GCIF_CATEGORYNAMES      0x002000
#define GCIF_MINMAX             0x004000
#define GCIF_SCALEOFFSET        0x008000
#define GCIF_UNITTYPE           0x010000
#define GCIF_COLORTABLE         0x020000
#define GCIF_COLORINTERP        0x020000
#define GCIF_BAND_METADATA      0x040000
#define GCIF_RAT                0x080000
#define GCIF_MASK               0x100000
#define GCIF_BAND_DESCRIPTION   0x200000

#define GCIF_ONLY_IF_MISSING    0x10000000
#define GCIF_PROCESS_BANDS      0x20000000

#define GCIF_PAM_DEFAULT        (GCIF_GEOTRANSFORM | GCIF_PROJECTION |     \
                                 GCIF_METADATA | GCIF_GCPS |               \
                                 GCIF_NODATA | GCIF_CATEGORYNAMES |        \
                                 GCIF_MINMAX | GCIF_SCALEOFFSET |          \
                                 GCIF_UNITTYPE | GCIF_COLORTABLE |         \
                                 GCIF_COLORINTERP | GCIF_BAND_METADATA |   \
                                 GCIF_RAT | GCIF_MASK |                    \
                                 GCIF_ONLY_IF_MISSING | GCIF_PROCESS_BANDS|\
                                 GCIF_BAND_DESCRIPTION)

/* GDAL PAM Flags */
/* ERO 2011/04/13 : GPF_AUXMODE seems to be unimplemented */
#define GPF_DIRTY               0x01  // .pam file needs to be written on close
#define GPF_TRIED_READ_FAILED   0x02  // no need to keep trying to read .pam.
#define GPF_DISABLED            0x04  // do not try any PAM stuff.
#define GPF_AUXMODE             0x08  // store info in .aux (HFA) file.
#define GPF_NOSAVE              0x10  // do not try to save pam info.

/* ==================================================================== */
/*      GDALDatasetPamInfo                                              */
/*                                                                      */
/*      We make these things a separate structure of information        */
/*      primarily so we can modify it without altering the size of      */
/*      the GDALPamDataset.  It is an effort to reduce ABI churn for    */
/*      driver plugins.                                                 */
/* ==================================================================== */
class GDALDatasetPamInfo
{
public:
    char        *pszPamFilename = nullptr;

    std::vector<CPLXMLTreeCloser> m_apoOtherNodes{};

    OGRSpatialReference* poSRS = nullptr;

    int         bHaveGeoTransform = false;
    double      adfGeoTransform[6]{0,0,0,0,0,0};

    int         nGCPCount = 0;
    GDAL_GCP   *pasGCPList = nullptr;
    OGRSpatialReference* poGCP_SRS = nullptr;

    CPLString   osPhysicalFilename{};
    CPLString   osSubdatasetName{};
    CPLString   osAuxFilename{};

    int         bHasMetadata = false;
};
//! @endcond

/* ******************************************************************** */
/*                           GDALPamDataset                             */
/* ******************************************************************** */

/** PAM dataset */
class CPL_DLL GDALPamDataset : public GDALDataset
{
    friend class GDALPamRasterBand;

  private:
    int IsPamFilenameAPotentialSiblingFile();

  protected:

                GDALPamDataset(void);
//! @cond Doxygen_Suppress
    int         nPamFlags = 0;
    GDALDatasetPamInfo *psPam = nullptr;

    virtual const char *_GetProjectionRef() override;
    virtual const char *_GetGCPProjection() override;
    virtual CPLErr _SetProjection( const char * pszProjection ) override;
    virtual CPLErr _SetGCPs( int nGCPCount, const GDAL_GCP *pasGCPList,
                    const char *pszGCPProjection ) override;

    virtual CPLXMLNode *SerializeToXML( const char *);
    virtual CPLErr      XMLInit( CPLXMLNode *, const char * );

    virtual CPLErr TryLoadXML(char **papszSiblingFiles = nullptr);
    virtual CPLErr TrySaveXML();

    CPLErr  TryLoadAux(char **papszSiblingFiles = nullptr);
    CPLErr  TrySaveAux();

    virtual const char *BuildPamFilename();

    void   PamInitialize();
    void   PamClear();

    void   SetPhysicalFilename( const char * );
    const char *GetPhysicalFilename();
    void   SetSubdatasetName( const char *);
    const char *GetSubdatasetName();
//! @endcond

  public:
    ~GDALPamDataset() override;

    void FlushCache(bool bAtClosing) override;

    const OGRSpatialReference* GetSpatialRef() const override;
    CPLErr SetSpatialRef(const OGRSpatialReference* poSRS) override;

    CPLErr GetGeoTransform( double * ) override;
    CPLErr SetGeoTransform( double * ) override;

    int GetGCPCount() override;
    const OGRSpatialReference* GetGCPSpatialRef() const override;
    const GDAL_GCP *GetGCPs() override;
    using GDALDataset::SetGCPs;
    CPLErr SetGCPs( int nGCPCount, const GDAL_GCP *pasGCPList,
                    const OGRSpatialReference* poSRS ) override;

    CPLErr SetMetadata( char ** papszMetadata,
                        const char * pszDomain = "" ) override;
    CPLErr SetMetadataItem( const char * pszName,
                            const char * pszValue,
                            const char * pszDomain = "" ) override;
    char **GetMetadata( const char * pszDomain = "" ) override;
    const char *GetMetadataItem( const char * pszName,
                                 const char * pszDomain = "" ) override;

    char **GetFileList(void) override;

    void ClearStatistics() override;

//! @cond Doxygen_Suppress
    virtual CPLErr CloneInfo( GDALDataset *poSrcDS, int nCloneInfoFlags );

    CPLErr IBuildOverviews( const char *pszResampling,
                            int nOverviews, int *panOverviewList,
                            int nListBands, int *panBandList,
                            GDALProgressFunc pfnProgress,
                            void * pProgressData ) override;

    // "semi private" methods.
    void   MarkPamDirty() { nPamFlags |= GPF_DIRTY; }
    GDALDatasetPamInfo *GetPamInfo() { return psPam; }
    int    GetPamFlags() { return nPamFlags; }
    void   SetPamFlags(int nValue ) { nPamFlags = nValue; }
//! @endcond

  private:
    CPL_DISALLOW_COPY_ASSIGN(GDALPamDataset)
};

//! @cond Doxygen_Suppress
/* ==================================================================== */
/*      GDALRasterBandPamInfo                                           */
/*                                                                      */
/*      We make these things a separate structure of information        */
/*      primarily so we can modify it without altering the size of      */
/*      the GDALPamDataset.  It is an effort to reduce ABI churn for    */
/*      driver plugins.                                                 */
/* ==================================================================== */
typedef struct {
    GDALPamDataset *poParentDS;

    int            bNoDataValueSet;
    double         dfNoDataValue;

    GDALColorTable *poColorTable;

    GDALColorInterp eColorInterp;

    char           *pszUnitType;
    char           **papszCategoryNames;

    double         dfOffset;
    double         dfScale;

    int            bHaveMinMax;
    double         dfMin;
    double         dfMax;

    int            bHaveStats;
    double         dfMean;
    double         dfStdDev;

    CPLXMLNode     *psSavedHistograms;

    GDALRasterAttributeTable *poDefaultRAT;

    bool           bOffsetSet;
    bool           bScaleSet;
} GDALRasterBandPamInfo;
//! @endcond
/* ******************************************************************** */
/*                          GDALPamRasterBand                           */
/* ******************************************************************** */

/** PAM raster band */
class CPL_DLL GDALPamRasterBand : public GDALRasterBand
{
    friend class GDALPamDataset;

  protected:
//! @cond Doxygen_Suppress
    virtual CPLXMLNode *SerializeToXML( const char *pszVRTPath );
    virtual CPLErr      XMLInit( CPLXMLNode *, const char * );

    void   PamInitialize();
    void   PamClear();

    GDALRasterBandPamInfo *psPam = nullptr;
//! @endcond

  public:
                GDALPamRasterBand();
//! @cond Doxygen_Suppress
    explicit    GDALPamRasterBand(int bForceCachedIO);
//! @endcond
    ~GDALPamRasterBand() override;

    void SetDescription( const char * ) override;

    CPLErr SetNoDataValue( double ) override;
    double GetNoDataValue( int *pbSuccess = nullptr ) override;
    CPLErr DeleteNoDataValue() override;

    CPLErr SetColorTable( GDALColorTable * ) override;
    GDALColorTable *GetColorTable() override;

    CPLErr SetColorInterpretation( GDALColorInterp ) override;
    GDALColorInterp GetColorInterpretation() override;

    const char *GetUnitType() override;
    CPLErr SetUnitType( const char * ) override;

    char **GetCategoryNames() override;
    CPLErr SetCategoryNames( char ** ) override;

    double GetOffset( int *pbSuccess = nullptr ) override;
    CPLErr SetOffset( double ) override;
    double GetScale( int *pbSuccess = nullptr ) override;
    CPLErr SetScale( double ) override;

    CPLErr GetHistogram( double dfMin, double dfMax,
                         int nBuckets, GUIntBig * panHistogram,
                         int bIncludeOutOfRange, int bApproxOK,
                         GDALProgressFunc, void *pProgressData ) override;

    CPLErr GetDefaultHistogram( double *pdfMin, double *pdfMax,
                                int *pnBuckets, GUIntBig ** ppanHistogram,
                                int bForce,
                                GDALProgressFunc, void *pProgressData) override;

    CPLErr SetDefaultHistogram( double dfMin, double dfMax,
                                int nBuckets, GUIntBig *panHistogram ) override;

    CPLErr SetMetadata( char ** papszMetadata,
                        const char * pszDomain = "" ) override;
    CPLErr SetMetadataItem( const char * pszName,
                            const char * pszValue,
                            const char * pszDomain = "" ) override;

    GDALRasterAttributeTable *GetDefaultRAT() override;
    CPLErr SetDefaultRAT( const GDALRasterAttributeTable * ) override;

//! @cond Doxygen_Suppress
    // new in GDALPamRasterBand.
    virtual CPLErr CloneInfo( GDALRasterBand *poSrcBand, int nCloneInfoFlags );

    // "semi private" methods.
    GDALRasterBandPamInfo *GetPamInfo() { return psPam; }
//! @endcond
  private:
    CPL_DISALLOW_COPY_ASSIGN(GDALPamRasterBand)
};

//! @cond Doxygen_Suppress

/* ******************************************************************** */
/*                          GDALPamMultiDim                             */
/* ******************************************************************** */

/** Class that serializes/deserializes metadata on multidimensional objects.
 * Currently SRS on GDALMDArray.
 */
class CPL_DLL GDALPamMultiDim
{
    struct Private;
    std::unique_ptr<Private> d;

    void Load();
    void Save();

public:
    explicit GDALPamMultiDim(const std::string& osFilename);
    virtual ~GDALPamMultiDim();

    std::shared_ptr<OGRSpatialReference> GetSpatialRef(const std::string& osArrayFullName);

    void SetSpatialRef(const std::string& osArrayFullName,
                       const OGRSpatialReference* poSRS);

    CPLErr GetStatistics( const std::string& osArrayFullName,
                          bool bApproxOK,
                          double *pdfMin, double *pdfMax,
                          double *pdfMean, double *pdfStdDev,
                          GUInt64* pnValidCount);

    void SetStatistics( const std::string& osArrayFullName,
                        bool bApproxStats,
                        double dfMin, double dfMax,
                        double dfMean, double dfStdDev,
                        GUInt64 nValidCount );

    void ClearStatistics();

    void ClearStatistics( const std::string& osArrayFullName );
};

/* ******************************************************************** */
/*                          GDALPamMDArray                              */
/* ******************************************************************** */

/** Class that relies on GDALPamMultiDim to serializes/deserializes metadata. */
class CPL_DLL GDALPamMDArray: public GDALMDArray
{
    std::shared_ptr<GDALPamMultiDim> m_poPam;

protected:
    GDALPamMDArray(const std::string& osParentName,
                   const std::string& osName,
                   const std::shared_ptr<GDALPamMultiDim>& poPam);

    bool SetStatistics( bool bApproxStats,
                                double dfMin, double dfMax,
                                double dfMean, double dfStdDev,
                                GUInt64 nValidCount ) override;

public:
    const std::shared_ptr<GDALPamMultiDim>& GetPAM() const { return m_poPam; }

    CPLErr GetStatistics( bool bApproxOK, bool bForce,
                                  double *pdfMin, double *pdfMax,
                                  double *pdfMean, double *padfStdDev,
                                  GUInt64* pnValidCount,
                                  GDALProgressFunc pfnProgress, void *pProgressData ) override;

    void ClearStatistics() override;

    bool SetSpatialRef(const OGRSpatialReference* poSRS) override;

    std::shared_ptr<OGRSpatialReference> GetSpatialRef() const override;
};

// These are mainly helper functions for internal use.
int CPL_DLL PamParseHistogram( CPLXMLNode *psHistItem,
                               double *pdfMin, double *pdfMax,
                               int *pnBuckets, GUIntBig **ppanHistogram,
                               int *pbIncludeOutOfRange, int *pbApproxOK );
CPLXMLNode CPL_DLL *
PamFindMatchingHistogram( CPLXMLNode *psSavedHistograms,
                          double dfMin, double dfMax, int nBuckets,
                          int bIncludeOutOfRange, int bApproxOK );
CPLXMLNode CPL_DLL *
PamHistogramToXMLTree( double dfMin, double dfMax,
                       int nBuckets, GUIntBig * panHistogram,
                       int bIncludeOutOfRange, int bApprox );

// For managing the proxy file database.
const char CPL_DLL * PamGetProxy( const char * );
const char CPL_DLL * PamAllocateProxy( const char * );
const char CPL_DLL * PamDeallocateProxy( const char * );
void CPL_DLL PamCleanProxyDB( void );

//! @endcond

#endif /* ndef GDAL_PAM_H_INCLUDED */
