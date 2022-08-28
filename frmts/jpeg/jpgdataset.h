/******************************************************************************
 * $Id: jpgdataset.cpp 37340 2017-02-11 18:28:02Z goatbar
 *
 * Project:  JPEG JFIF Driver
 * Purpose:  Implement GDAL JPEG Support based on IJG libjpeg.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2000, Frank Warmerdam
 * Copyright (c) 2007-2014, Even Rouault <even dot rouault at spatialys.com>
 *
 * Portions Copyright (c) Her majesty the Queen in right of Canada as
 * represented by the Minister of National Defence, 2006.
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

#include "cpl_port.h"

// TODO(schwehr): Run IWYU.
#include <cerrno>
#include <climits>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#if HAVE_FCNTL_H
#  include <fcntl.h>
#endif
#include <setjmp.h>

#include <algorithm>
#include <string>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_progress.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "gdal.h"
#include "gdal_frmts.h"
#include "gdal_pam.h"
#include "gdal_priv.h"
CPL_C_START
#ifdef LIBJPEG_12_PATH
#  include LIBJPEG_12_PATH
#else
#  include "jpeglib.h"
#endif
CPL_C_END
#include "memdataset.h"
#include "vsidataio.h"

// TIFF header.
typedef struct
{
    GUInt16 tiff_magic;    // Magic number (defines byte order).
    GUInt16 tiff_version;  // TIFF version number.
    GUInt32 tiff_diroff;   // byte offset to first directory.
} TIFFHeader;

// Ok to use setjmp().
#ifdef _MSC_VER
#  pragma warning(disable:4611)
#endif

typedef struct
{
    const char *pszFilename;
    VSILFILE *fpLin;
    char **papszSiblingFiles;
    int nScaleFactor;
    bool bDoPAMInitialize;
    bool bUseInternalOverviews;
} JPGDatasetOpenArgs;

class JPGDatasetCommon;

#if defined(JPEG_DUAL_MODE_8_12) && !defined(JPGDataset)
JPGDatasetCommon *JPEGDataset12Open(JPGDatasetOpenArgs *psArgs);
GDALDataset *JPEGDataset12CreateCopy( const char *pszFilename,
                                      GDALDataset *poSrcDS,
                                      int bStrict, char **papszOptions,
                                      GDALProgressFunc pfnProgress,
                                      void *pProgressData );
#endif

// Do we want to do special processing suitable for when JSAMPLE is a
// 16bit value?
#if defined(JPEG_LIB_MK1)
#  define JPEG_LIB_MK1_OR_12BIT 1
#elif BITS_IN_JSAMPLE == 12
#  define JPEG_LIB_MK1_OR_12BIT 1
#endif

GDALRasterBand *JPGCreateBand(JPGDatasetCommon *poDS, int nBand);

typedef void (*my_jpeg_write_m_header)(void *cinfo, int marker,
                                       unsigned int datalen);
typedef void (*my_jpeg_write_m_byte)(void *cinfo, int val);

CPLErr JPGAppendMask( const char *pszJPGFilename, GDALRasterBand *poMask,
                      GDALProgressFunc pfnProgress, void *pProgressData );
void   JPGAddEXIF        ( GDALDataType eWorkDT,
                           GDALDataset *poSrcDS, char** papszOptions,
                           void *cinfo,
                           my_jpeg_write_m_header p_jpeg_write_m_header,
                           my_jpeg_write_m_byte p_jpeg_write_m_byte,
                           GDALDataset *(pCreateCopy)(
                               const char *, GDALDataset *,
                               int, char **,
                               GDALProgressFunc pfnProgress,
                               void *pProgressData ) );
void JPGAddICCProfile( void *pInfo,
                       const char *pszICCProfile,
                       my_jpeg_write_m_header p_jpeg_write_m_header,
                       my_jpeg_write_m_byte p_jpeg_write_m_byte);

class GDALJPEGUserData
{
public:
    jmp_buf     setjmp_buffer;
    bool        bNonFatalErrorEncountered = false;
    void      (*p_previous_emit_message)(j_common_ptr cinfo, int msg_level) = nullptr;
    int         nMaxScans;

    GDALJPEGUserData() :
        nMaxScans(atoi(
            CPLGetConfigOption("GDAL_JPEG_MAX_ALLOWED_SCAN_NUMBER", "100")))
    {
        memset(&setjmp_buffer, 0, sizeof(setjmp_buffer));
    }
};

/************************************************************************/
/* ==================================================================== */
/*                         JPGDatasetCommon                             */
/* ==================================================================== */
/************************************************************************/

class JPGRasterBand;
class JPGMaskBand;

class JPGDatasetCommon CPL_NON_FINAL: public GDALPamDataset
{
  protected:
    friend class JPGDataset;
    friend class JPGRasterBand;
    friend class JPGMaskBand;

    int           nScaleFactor;
    bool          bHasInitInternalOverviews;
    int           nInternalOverviewsCurrent;
    int           nInternalOverviewsToFree;
    GDALDataset **papoInternalOverviews;
    JPGDatasetCommon* poActiveDS = nullptr; /* only valid in parent DS */
    JPGDatasetCommon** ppoActiveDS = nullptr; /* &poActiveDS of poActiveDS from parentDS */
    void          InitInternalOverviews();
    GDALDataset  *InitEXIFOverview();

    char   *pszProjection;
    bool   bGeoTransformValid;
    double adfGeoTransform[6];
    int    nGCPCount;
    GDAL_GCP *pasGCPList;

    VSILFILE   *m_fpImage;
    GUIntBig nSubfileOffset;

    int    nLoadedScanline;
    GByte  *m_pabyScanline;

    bool   bHasReadEXIFMetadata;
    bool   bHasReadXMPMetadata;
    bool   bHasReadICCMetadata;
    bool   bHasReadFLIRMetadata = false;
    char   **papszMetadata;
    int    nExifOffset;
    int    nInterOffset;
    int    nGPSOffset;
    bool   bSwabflag;
    int    nTiffDirStart;
    int    nTIFFHEADER;
    bool   bHasDoneJpegCreateDecompress;
    bool   bHasDoneJpegStartDecompress;

    int    m_nSubdatasetCount = 0;

    // FLIR raw thermal image
    bool   m_bRawThermalLittleEndian = false;
    int    m_nRawThermalImageWidth = 0;
    int    m_nRawThermalImageHeight = 0;
    std::vector<GByte> m_abyRawThermalImage{};

    virtual CPLErr LoadScanline(int, GByte* outBuffer = nullptr) = 0;
    virtual void   StopDecompress() = 0;
    virtual CPLErr Restart() = 0;

    virtual int GetDataPrecision() = 0;
    virtual int GetOutColorSpace() = 0;

    bool   EXIFInit(VSILFILE *);
    void   ReadICCProfile();

    void   CheckForMask();
    void   DecompressMask();

    void   LoadForMetadataDomain( const char *pszDomain );

    void   ReadEXIFMetadata();
    void   ReadXMPMetadata();
    void   ReadFLIRMetadata();
    GDALDataset* OpenFLIRRawThermalImage();

    bool   bHasCheckedForMask;
    JPGMaskBand *poMaskBand;
    GByte  *pabyBitMask;
    bool   bMaskLSBOrder;

    GByte  *pabyCMask;
    int    nCMaskSize;

    // Color space exposed by GDAL.  Not necessarily the in_color_space nor
    // the out_color_space of JPEG library.
    /*J_COLOR_SPACE*/ int eGDALColorSpace;

    bool   bIsSubfile;
    bool   bHasTriedLoadWorldFileOrTab;
    void   LoadWorldFileOrTab();
    CPLString osWldFilename;

    virtual int         CloseDependentDatasets() override;

    virtual CPLErr IBuildOverviews( const char *, int, int *, int, int *,
                                    GDALProgressFunc, void * ) override;

  public:
                 JPGDatasetCommon();
    virtual ~JPGDatasetCommon();

    virtual CPLErr      IRasterIO( GDALRWFlag, int, int, int, int,
                                   void *, int, int, GDALDataType,
                                   int, int *,
                                   GSpacing nPixelSpace, GSpacing nLineSpace,
                                   GSpacing nBandSpace,
                                   GDALRasterIOExtraArg* psExtraArg ) override;

    virtual CPLErr GetGeoTransform( double * ) override;

    virtual int    GetGCPCount() override;
    virtual const char *_GetGCPProjection() override;
    const OGRSpatialReference* GetGCPSpatialRef() const override {
        return GetGCPSpatialRefFromOldGetGCPProjection();
    }
    virtual const GDAL_GCP *GetGCPs() override;

    virtual char  **GetMetadataDomainList() override;
    virtual char  **GetMetadata( const char *pszDomain = "" ) override;
    virtual const char *GetMetadataItem( const char *pszName,
                                         const char *pszDomain = "" ) override;

    virtual char **GetFileList(void) override;

    virtual void FlushCache(bool bAtClosing) override;

    static int          Identify( GDALOpenInfo * );
    static GDALDataset *Open( GDALOpenInfo * );
};

/************************************************************************/
/* ==================================================================== */
/*                              JPGDataset                              */
/* ==================================================================== */
/************************************************************************/

class JPGDataset final: public JPGDatasetCommon
{
    GDALJPEGUserData sUserData;

    bool ErrorOutOnNonFatalError();

    static void EmitMessage(j_common_ptr cinfo, int msg_level);
    static void ProgressMonitor (j_common_ptr cinfo );

    struct jpeg_decompress_struct sDInfo;
    struct jpeg_error_mgr sJErr;
    struct jpeg_progress_mgr sJProgress;

    virtual CPLErr LoadScanline(int, GByte* outBuffer) override;
    CPLErr         StartDecompress();
    virtual void   StopDecompress() override;
    virtual CPLErr Restart() override;
    virtual int GetDataPrecision() override { return sDInfo.data_precision; }
    virtual int GetOutColorSpace() override { return sDInfo.out_color_space; }

    int    nQLevel;
#if !defined(JPGDataset)
    void   LoadDefaultTables(int);
#endif
    void   SetScaleNumAndDenom();

    static JPGDatasetCommon *OpenStage2( JPGDatasetOpenArgs *psArgs,
                                    JPGDataset *&poDS );

  public:
                 JPGDataset();
    virtual ~JPGDataset();

    static JPGDatasetCommon *Open( JPGDatasetOpenArgs *psArgs );
    static GDALDataset *CreateCopy( const char *pszFilename,
                                    GDALDataset *poSrcDS,
                                    int bStrict, char ** papszOptions,
                                    GDALProgressFunc pfnProgress,
                                    void *pProgressData );
    static GDALDataset *CreateCopyStage2(
        const char *pszFilename, GDALDataset *poSrcDS, char **papszOptions,
        GDALProgressFunc pfnProgress, void *pProgressData, VSILFILE *fpImage,
        GDALDataType eDT, int nQuality, bool bAppendMask,
        GDALJPEGUserData &sUserData, struct jpeg_compress_struct &sCInfo,
        struct jpeg_error_mgr &sJErr, GByte *&pabyScanline);
    static void ErrorExit(j_common_ptr cinfo);
};

/************************************************************************/
/* ==================================================================== */
/*                            JPGRasterBand                             */
/* ==================================================================== */
/************************************************************************/

class JPGRasterBand final: public GDALPamRasterBand
{
    friend class JPGDatasetCommon;

    // We have to keep a pointer to the JPGDataset that this JPGRasterBand
    // belongs to. In some case, we may have this->poGDS != this->poDS
    // For example for a JPGRasterBand that is set to a NITFDataset.
    // In other words, this->poDS doesn't necessary point to a JPGDataset
    // See ticket #1807.
    JPGDatasetCommon *poGDS;

  public:
    JPGRasterBand(JPGDatasetCommon *, int);
    virtual ~JPGRasterBand() {}

    virtual CPLErr IReadBlock( int, int, void * ) override;
    virtual GDALColorInterp GetColorInterpretation() override;

    virtual GDALSuggestedBlockAccessPattern GetSuggestedBlockAccessPattern()
        const override { return GSBAP_TOP_TO_BOTTOM; }

    virtual GDALRasterBand *GetMaskBand() override;
    virtual int             GetMaskFlags() override;

    virtual GDALRasterBand *GetOverview(int i) override;
    virtual int             GetOverviewCount() override;
};

#if !defined(JPGDataset)

/************************************************************************/
/* ==================================================================== */
/*                             JPGMaskBand                              */
/* ==================================================================== */
/************************************************************************/

class JPGMaskBand final: public GDALRasterBand
{
  protected:
    virtual CPLErr IReadBlock( int, int, void * ) override;

  public:
    explicit JPGMaskBand( JPGDatasetCommon *poDS );
    virtual ~JPGMaskBand() {}
};

/************************************************************************/
/*                         GDALRegister_JPEG()                          */
/************************************************************************/

class GDALJPGDriver final: public GDALDriver
{
  public:
    GDALJPGDriver() {}

    char      **GetMetadata( const char *pszDomain = "" ) override;
    const char *GetMetadataItem( const char *pszName,
                                 const char *pszDomain = "" ) override;
};

#endif  // !defined(JPGDataset)
