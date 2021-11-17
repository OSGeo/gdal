/******************************************************************************
 * $Id$
 *
 * Project:  GDAL
 * Purpose:  ECW (ERDAS Wavelet Compression Format) Driver Definitions
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2001-2011, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2013, Even Rouault <even dot rouault at spatialys.com>
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

#ifndef GDAL_ECW_H_INCLUDED
#define GDAL_ECW_H_INCLUDED

#include "gdaljp2abstractdataset.h"
#include "gdal_frmts.h"
#include "cpl_string.h"
#include "cpl_conv.h"
#include "cpl_multiproc.h"
#include "cpl_vsi.h"

#undef NOISY_DEBUG

#ifdef FRMT_ecw

#include "ecwsdk_headers.h"

#if ECWSDK_VERSION >= 55
#include "NCSIOStreamOptions.h"
#endif


void ECWInitialize( void );
GDALDataset* ECWDatasetOpenJPEG2000(GDALOpenInfo* poOpenInfo);
const char* ECWGetColorInterpretationName(GDALColorInterp eColorInterpretation, int nBandNumber);
GDALColorInterp ECWGetColorInterpretationByName(const char *pszName);
const char* ECWGetColorSpaceName(NCSFileColorSpace colorSpace);
#ifdef HAVE_COMPRESS
GDALDataset *
ECWCreateCopyECW( const char * pszFilename, GDALDataset *poSrcDS,
                 int bStrict, char ** papszOptions,
                 GDALProgressFunc pfnProgress, void * pProgressData );
GDALDataset *
ECWCreateCopyJPEG2000( const char * pszFilename, GDALDataset *poSrcDS,
                 int bStrict, char ** papszOptions,
                 GDALProgressFunc pfnProgress, void * pProgressData );

GDALDataset *
ECWCreateECW( const char * pszFilename, int nXSize, int nYSize, int nBands,
              GDALDataType eType, char **papszOptions );
GDALDataset *
ECWCreateJPEG2000(const char *pszFilename, int nXSize, int nYSize, int nBands,
                  GDALDataType eType, char **papszOptions );
#endif

void ECWReportError(CNCSError& oErr, const char* pszMsg = "");

/************************************************************************/
/* ==================================================================== */
/*                             JP2Userbox                               */
/* ==================================================================== */
/************************************************************************/
#ifdef HAVE_COMPRESS
#if ECWSDK_VERSION>=50
class JP2UserBox final: public CNCSSDKBox {
#else
class JP2UserBox final: public CNCSJP2Box {
#endif
private:
    int           nDataLength;
    unsigned char *pabyData;

public:
    JP2UserBox();

    virtual ~JP2UserBox();

#if ECWSDK_VERSION >= 55
    CNCSError Parse(NCS::SDK::CFileBase &JP2File, const NCS::CIOStreamPtr &Stream) override;
    CNCSError UnParse(NCS::SDK::CFileBase &JP2File, const NCS::CIOStreamPtr &Stream) override;
#elif ECWSDK_VERSION >= 40
    virtual CNCSError Parse(NCS::SDK::CFileBase &JP2File,
                             NCS::CIOStream &Stream) override;
    virtual CNCSError UnParse(NCS::SDK::CFileBase &JP2File,
                                NCS::CIOStream &Stream) override;
#else
    virtual CNCSError Parse(class CNCSJP2File &JP2File,
                            CNCSJPCIOStream &Stream) override;
    virtual CNCSError UnParse(class CNCSJP2File &JP2File,
                              CNCSJPCIOStream &Stream) override;
#endif
    virtual void UpdateXLBox() override;

    void    SetData( int nDataLength, const unsigned char *pabyDataIn );

    int     GetDataLength() { return nDataLength; }
    unsigned char *GetData() { return pabyData; }
};
#endif /* def HAVE_COMPRESS */

/************************************************************************/
/* ==================================================================== */
/*                             VSIIOStream                              */
/* ==================================================================== */
/************************************************************************/

class VSIIOStream final: public CNCSJPCIOStream
{
    VSIIOStream(const VSIIOStream &) = delete;
    VSIIOStream& operator= (const VSIIOStream&) = delete;
    VSIIOStream(VSIIOStream &&) = delete;
    VSIIOStream& operator= (VSIIOStream &&) = delete;

    char     *m_Filename;
  public:

    INT64    startOfJPData;
    INT64    lengthOfJPData;
    VSILFILE    *fpVSIL;
    BOOLEAN      bWritable;
    BOOLEAN      bSeekable;
    int      nFileViewCount;

    int      nCOMState;
    int      nCOMLength;
    GByte    abyCOMType[2]{};

    /* To fix ‘virtual bool NCS::CIOStream::Read(INT64, void*, UINT32)’ was hidden' with SDK 5 */
    using CNCSJPCIOStream::Read;

    VSIIOStream() : m_Filename(nullptr)
    {
        nFileViewCount = 0;
        startOfJPData = 0;
        lengthOfJPData = -1;
        fpVSIL = nullptr;
        bWritable = false;
        bSeekable = false;
        if( CSLTestBoolean(CPLGetConfigOption("GDAL_ECW_WRITE_COMPRESSION_SOFTWARE", "YES")) )
            nCOMState = -1;
        else
            nCOMState = 0;
        nCOMLength = 0;
        abyCOMType[0] = 0;
        abyCOMType[1] = 0;
    }
    virtual ~VSIIOStream() {
        VSIIOStream::Close();
        if (m_Filename!=nullptr){
            CPLFree(m_Filename);
        }
    }

    CNCSError Close() override {
        CNCSError oErr = CNCSJPCIOStream::Close();
        if( fpVSIL != nullptr )
        {
            VSIFCloseL( fpVSIL );
            fpVSIL = nullptr;
        }
        return oErr;
    }

#if ECWSDK_VERSION >= 40
    VSIIOStream *Clone() override {
        CPLDebug( "ECW", "VSIIOStream::Clone()" );
        VSILFILE *fpNewVSIL = VSIFOpenL( m_Filename, "rb" );
        if (fpNewVSIL == nullptr)
        {
            return nullptr;
        }

        VSIIOStream *pDst = new VSIIOStream();
        pDst->Access(fpNewVSIL, bWritable, bSeekable, m_Filename, startOfJPData, lengthOfJPData);
        return pDst;
    }
#endif /* ECWSDK_VERSION >= 4 */

    CNCSError Access( VSILFILE *fpVSILIn, BOOLEAN bWrite, BOOLEAN bSeekableIn,
                              const char *pszFilename,
                              INT64 start, INT64 size = -1) {

        fpVSIL = fpVSILIn;
        startOfJPData = start;
        lengthOfJPData = size;
        bWritable = bWrite;
        bSeekable = bSeekableIn;
        VSIFSeekL(fpVSIL, startOfJPData, SEEK_SET);
        m_Filename = CPLStrdup(pszFilename);

#if ECWSDK_VERSION >= 55
        const std::string vsiStreamPrefix("STREAM=/vsi");
        const std::string vsiPrefix("/vsi");
        m_StreamOptions->SetIsRemoteStream(
            std::string(m_Filename).compare(0, vsiPrefix.length(), vsiPrefix) == 0 ||
            std::string(m_Filename).compare(0, vsiStreamPrefix.length(), vsiStreamPrefix) == 0
        );
#endif
        // the filename is used to establish where to put temporary files.
        // if it does not have a path to a real directory, we will
        // substitute something.
        CPLString osFilenameUsed = pszFilename;

#if ECWSDK_VERSION < 55
        CPLString osPath = CPLGetPath( pszFilename );
        struct stat sStatBuf;
        if( !osPath.empty() && stat( osPath, &sStatBuf ) != 0 )
        {
            osFilenameUsed = CPLGenerateTempFilename( nullptr );
            // try to preserve the extension.
            if( strlen(CPLGetExtension(pszFilename)) > 0 )
            {
                osFilenameUsed += ".";
                osFilenameUsed += CPLGetExtension(pszFilename);
            }
            CPLDebug( "ECW", "Using filename '%s' for temporary directory determination purposes.", osFilenameUsed.c_str() );
        }
#endif

#ifdef WIN32
        if( CSLTestBoolean( CPLGetConfigOption( "GDAL_FILENAME_IS_UTF8", "YES" ) ) )
        {
            wchar_t       *pwszFilename = CPLRecodeToWChar( osFilenameUsed.c_str(), CPL_ENC_UTF8, CPL_ENC_UCS2 );
            CNCSError oError;
            oError = CNCSJPCIOStream::Open( pwszFilename, (bool) bWrite );
            CPLFree( pwszFilename );
            return oError;
        }
        else
#endif
        {
            return(CNCSJPCIOStream::Open((char *)osFilenameUsed.c_str(),
                                        (bool) bWrite));
        }
    }

    virtual bool NCS_FASTCALL Seek() override {
        return bSeekable;
    }

    virtual bool NCS_FASTCALL Seek(INT64 offset, Origin origin = CURRENT) override {
#ifdef DEBUG_VERBOSE
        CPLDebug( "ECW", "VSIIOStream::Seek(" CPL_FRMT_GIB ",%d)",
                  static_cast<GIntBig>(offset), (int) origin );
#endif
        bool success = false;
        switch(origin) {
            case START:
              success = (0 == VSIFSeekL(fpVSIL, offset+startOfJPData, SEEK_SET));
              break;

            case CURRENT:
              success = (0 == VSIFSeekL(fpVSIL, offset, SEEK_CUR));
              break;

            case END:
              success = (0 == VSIFSeekL(fpVSIL, offset, SEEK_END));
              break;
        }
        if( !success )
            CPLDebug( "ECW", "VSIIOStream::Seek(%d,%d) failed.",
                      (int) offset, (int) origin );
        return(success);
    }

    virtual INT64 NCS_FASTCALL Tell() override {
        return VSIFTellL( fpVSIL ) - startOfJPData;
    }

    virtual INT64 NCS_FASTCALL Size() override {
        if( lengthOfJPData != -1 )
            return lengthOfJPData;
        else
        {
            INT64 curPos = Tell(), size;

            Seek( 0, END );
            size = Tell();
            Seek( curPos, START );
#ifdef DEBUG_VERBOSE
            CPLDebug( "ECW", "VSIIOStream::Size()=" CPL_FRMT_GIB, static_cast<GIntBig>(size) );
#endif
            return size;
        }
    }

#if ECWSDK_VERSION >= 40
    /* New, and needed, in ECW SDK 4 */
    virtual bool Read(INT64 offset, void* buffer, UINT32 count) override
    {
#ifdef DEBUG_VERBOSE
      CPLDebug( "ECW", "VSIIOStream::Read(" CPL_FRMT_GIB ",%u)", static_cast<GIntBig>(offset), count );
#endif
      /* SDK 4.3 doc says it is not supposed to update the file pointer. */
      /* Later versions have no comment... */
      INT64 curPos = Tell();
      Seek( offset, START );
      bool ret = Read(buffer, count);
      Seek( curPos, START );
      return ret;
    }
#endif

    virtual bool NCS_FASTCALL Read(void* buffer, UINT32 count) override {
#ifdef DEBUG_VERBOSE
        CPLDebug( "ECW", "VSIIOStream::Read(%u)", count );
#endif
        if( count == 0 )
            return true;

//        return(1 == VSIFReadL( buffer, count, 1, fpVSIL ) );

        // The following is a hack
        if( VSIFReadL( buffer, count, 1, fpVSIL ) != 1 )
        {
            CPLDebug( "VSIIOSTREAM",
                      "Read(%d) failed @ " CPL_FRMT_GIB ", ignoring failure.",
                      count, (VSIFTellL( fpVSIL ) - startOfJPData) );
        }

        return true;
    }

    virtual bool NCS_FASTCALL Write(void* buffer, UINT32 count) override {
        if( count == 0 )
            return true;

        GByte* paby = (GByte*) buffer;
        if( nCOMState == 0 )
        {
            if( count == 2 && paby[0] == 0xff && paby[1] == 0x64 )
            {
                nCOMState ++;
                return true;
            }
        }
        else if( nCOMState == 1 )
        {
            if( count == 2 )
            {
                nCOMLength = (paby[0] << 8) | paby[1];
                nCOMState ++;
                return true;
            }
            else
            {
                GByte prevBuffer[] = { 0xff, 0x64 };
                VSIFWriteL(prevBuffer, 2, 1, fpVSIL);
                nCOMState = 0;
            }
        }
        else if( nCOMState == 2 )
        {
            if( count == 2 )
            {
                abyCOMType[0] = paby[0];
                abyCOMType[1] = paby[1];
                nCOMState ++;
                return true;
            }
            else
            {
                GByte prevBuffer[] =
                  { (GByte)(nCOMLength >> 8), (GByte) (nCOMLength & 0xff) };
                VSIFWriteL(prevBuffer, 2, 1, fpVSIL);
                nCOMState = 0;
            }
        }
        else if( nCOMState == 3 )
        {
            if( count == (UINT32)nCOMLength - 4 )
            {
                nCOMState = 0;
                return true;
            }
            else
            {
                VSIFWriteL(abyCOMType, 2, 1, fpVSIL);
                nCOMState = 0;
            }
        }

        if( 1 != VSIFWriteL(buffer, count, 1, fpVSIL) )
        {
            CPLDebug( "ECW", "VSIIOStream::Write(%d) failed.",
                      (int) count );
            return false;
        }
        else
            return true;
    }
};

/************************************************************************/
/* ==================================================================== */
/*                            ECWAsyncReader                            */
/* ==================================================================== */
/************************************************************************/
class ECWDataset;

#if ECWSDK_VERSION >= 40

class ECWAsyncReader final: public GDALAsyncReader
{
private:
    CNCSJP2FileView *poFileView = nullptr;
    CPLMutex        *hMutex = nullptr;
    int              bUsingCustomStream = false;

    int              bUpdateReady = false;
    int              bComplete = false;

    static NCSEcwReadStatus RefreshCB( NCSFileView * );
    NCSEcwReadStatus ReadToBuffer();

public:
    ECWAsyncReader();
    virtual ~ECWAsyncReader();
    virtual GDALAsyncStatusType GetNextUpdatedRegion(double dfTimeout,
                                                     int* pnXBufOff,
                                                     int* pnYBufOff,
                                                     int* pnXBufSize,
                                                     int* pnYBufSize) override;

    friend class ECWDataset;
};
#endif /* ECWSDK_VERSION >= 40 */

/************************************************************************/
/* ==================================================================== */
/*                              ECWDataset                              */
/* ==================================================================== */
/************************************************************************/

class ECWRasterBand;

typedef struct
{
    int bEnabled;
    int nBandsTried;

    int nXOff;
    int nYOff;
    int nXSize;
    int nYSize;
    int nBufXSize;
    int nBufYSize;
    GDALDataType eBufType;
    GByte* pabyData;
} ECWCachedMultiBandIO;

class CPL_DLL ECWDataset final: public GDALJP2AbstractDataset
{
    friend class ECWRasterBand;
    friend class ECWAsyncReader;

    int         bIsJPEG2000;

    CNCSJP2FileView *poFileView;
    NCSFileViewFileInfoEx *psFileInfo;

    GDALDataType eRasterDataType;
    NCSEcwCellType eNCSRequestDataType;

    int         bUsingCustomStream;

    // Current view window.
    int         bWinActive;
    int         nWinXOff, nWinYOff, nWinXSize, nWinYSize;
    int         nWinBufXSize, nWinBufYSize;
    int         nWinBandCount;
    int         *panWinBandList;
    int         nWinBufLoaded;
    void        **papCurLineBuf;

    // Deferred advise read parameters
    int         m_nAdviseReadXOff;
    int         m_nAdviseReadYOff;
    int         m_nAdviseReadXSize;
    int         m_nAdviseReadYSize;
    int         m_nAdviseReadBufXSize;
    int         m_nAdviseReadBufYSize;
    int         m_nAdviseReadBandCount;
    int        *m_panAdviseReadBandList;

    char        **papszGMLMetadata;

    ECWCachedMultiBandIO sCachedMultiBandIO;

    void        ECW2WKTProjection();

    void        CleanupWindow();
    CPLErr      RunDeferredAdviseRead();
    int         TryWinRasterIO( GDALRWFlag, int, int, int, int,
                                GByte *, int, int, GDALDataType,
                                int, int *,
                                GSpacing nPixelSpace, GSpacing nLineSpace,
                                GSpacing nBandSpace,
                                GDALRasterIOExtraArg* psExtraArg );
    CPLErr      LoadNextLine();

#if ECWSDK_VERSION>=50

    NCSFileStatistics* pStatistics;
    int bStatisticsDirty;
    int bStatisticsInitialized;
    NCS::CError StatisticsEnsureInitialized();
    NCS::CError StatisticsWrite();
    void CleanupStatistics();
    void ReadFileMetaDataFromFile();

    int bFileMetaDataDirty;
    void WriteFileMetaData(NCSFileMetaData* pFileMetaDataCopy);

#endif

    static CNCSJP2FileView    *OpenFileView( const char *pszDatasetName,
                                             bool bProgressive,
                                             int &bUsingCustomStream,
                                             bool bWrite=false);

    int         bHdrDirty;
    CPLString   m_osDatumCode;
    CPLString   m_osProjCode;
    CPLString   m_osUnitsCode;
    int         bGeoTransformChanged;
    int         bProjectionChanged;
    int         bProjCodeChanged;
    int         bDatumCodeChanged;
    int         bUnitsCodeChanged;
    void        WriteHeader();

    int         bUseOldBandRasterIOImplementation;

    int         bPreventCopyingSomeMetadata;

    int         nBandIndexToPromoteTo8Bit;

    CPLStringList oECWMetadataList;
    CPLErr ReadBands(void * pData, int nBufXSize, int nBufYSize,
                    GDALDataType eBufType,
                    int nBandCount,
                    GSpacing nPixelSpace, GSpacing nLineSpace, GSpacing nBandSpace,
                    GDALRasterIOExtraArg* psExtraArg);
    CPLErr ReadBandsDirectly(void * pData, int nBufXSize, int nBufYSize,
                    GDALDataType eBufType,
                    int nBandCount,
                    GSpacing nPixelSpace, GSpacing nLineSpace, GSpacing nBandSpace,
                    GDALRasterIOExtraArg* psExtraArg);
  public:
        ECWDataset(int bIsJPEG2000);
        ~ECWDataset();

    static GDALDataset *Open( GDALOpenInfo *, int bIsJPEG2000 );
    static int          IdentifyJPEG2000( GDALOpenInfo * poOpenInfo );
    static GDALDataset *OpenJPEG2000( GDALOpenInfo * );
    static int          IdentifyECW( GDALOpenInfo * poOpenInfo );
    static GDALDataset *OpenECW( GDALOpenInfo * );

    void        SetPreventCopyingSomeMetadata(int b) { bPreventCopyingSomeMetadata = b; }

    virtual CPLErr IRasterIO( GDALRWFlag, int, int, int, int,
                              void *, int, int, GDALDataType,
                              int, int *,
                              GSpacing nPixelSpace, GSpacing nLineSpace,
                              GSpacing nBandSpace,
                              GDALRasterIOExtraArg* psExtraArg) override;

    virtual char      **GetMetadataDomainList() override;
    virtual const char *GetMetadataItem( const char * pszName,
                                     const char * pszDomain = "" ) override;
    virtual char      **GetMetadata( const char * pszDomain = "" ) override;

    virtual CPLErr SetGeoTransform( double * padfGeoTransform ) override;
    CPLErr SetSpatialRef(const OGRSpatialReference* poSRS) override;

    virtual CPLErr SetMetadataItem( const char * pszName,
                                 const char * pszValue,
                                 const char * pszDomain = "" ) override;
    virtual CPLErr SetMetadata( char ** papszMetadata,
                             const char * pszDomain = "" ) override;

    virtual CPLErr AdviseRead( int nXOff, int nYOff, int nXSize, int nYSize,
                               int nBufXSize, int nBufYSize,
                               GDALDataType eDT,
                               int nBandCount, int *panBandList,
                               char **papszOptions ) override;

    // progressive methods
#if ECWSDK_VERSION >= 40
    virtual GDALAsyncReader* BeginAsyncReader( int nXOff, int nYOff,
                                               int nXSize, int nYSize,
                                               void *pBuf,
                                               int nBufXSize, int nBufYSize,
                                               GDALDataType eBufType,
                                               int nBandCount, int* panBandMap,
                                               int nPixelSpace, int nLineSpace,
                                               int nBandSpace,
                                               char **papszOptions) override;

    virtual void EndAsyncReader(GDALAsyncReader *) override;
#endif /* ECWSDK_VERSION > 40 */
#if ECWSDK_VERSION >=50
    int GetFormatVersion() const {
        return psFileInfo->nFormatVersion;
    }
#endif
};

/************************************************************************/
/* ==================================================================== */
/*                            ECWRasterBand                             */
/* ==================================================================== */
/************************************************************************/

class ECWRasterBand final: public GDALPamRasterBand
{
    friend class ECWDataset;

    // NOTE: poDS may be altered for NITF/JPEG2000 files!
    ECWDataset     *poGDS;

    GDALColorInterp         eBandInterp;

    int                          iOverview; // -1 for base.

    std::vector<ECWRasterBand*>  apoOverviews;

#if ECWSDK_VERSION>=50

    int nStatsBandIndex = 0;
    int nStatsBandCount = 0;

#endif

    int         bPromoteTo8Bit;

//#if !defined(SDK_CAN_DO_SUPERSAMPLING)
    CPLErr OldIRasterIO( GDALRWFlag, int, int, int, int,
                              void *, int, int, GDALDataType,
                              GSpacing nPixelSpace, GSpacing nLineSpace,
                              GDALRasterIOExtraArg* psExtraArg );
//#endif

    virtual CPLErr IRasterIO( GDALRWFlag, int, int, int, int,
                              void *, int, int, GDALDataType,
                              GSpacing nPixelSpace, GSpacing nLineSpace,
                              GDALRasterIOExtraArg* psExtraArg) override;

  public:

                   ECWRasterBand( ECWDataset *, int, int iOverview, char** papszOpenOptions );
                   ~ECWRasterBand();

    virtual CPLErr IReadBlock( int, int, void * ) override;
    virtual int    HasArbitraryOverviews() override { return apoOverviews.empty(); }
    virtual int    GetOverviewCount() override { return (int)apoOverviews.size(); }
    virtual GDALRasterBand *GetOverview(int) override;

    virtual GDALColorInterp GetColorInterpretation() override;
    virtual CPLErr SetColorInterpretation( GDALColorInterp ) override;

    virtual CPLErr AdviseRead( int nXOff, int nYOff, int nXSize, int nYSize,
                               int nBufXSize, int nBufYSize,
                               GDALDataType eDT, char **papszOptions ) override;
#if ECWSDK_VERSION >= 50
    void GetBandIndexAndCountForStatistics(int &bandIndex, int &bandCount) const;
    virtual CPLErr GetDefaultHistogram( double *pdfMin, double *pdfMax,
                                    int *pnBuckets, GUIntBig ** ppanHistogram,
                                    int bForce,
                                    GDALProgressFunc, void *pProgressData) override;
    virtual CPLErr SetDefaultHistogram( double dfMin, double dfMax,
                                        int nBuckets, GUIntBig *panHistogram ) override;
    virtual double GetMinimum( int* pbSuccess ) override;
    virtual double GetMaximum( int* pbSuccess ) override;
    virtual CPLErr GetStatistics( int bApproxOK, int bForce,
                                  double *pdfMin, double *pdfMax,
                                  double *pdfMean, double *padfStdDev ) override;
    virtual CPLErr SetStatistics( double dfMin, double dfMax,
                                  double dfMean, double dfStdDev ) override;

    virtual CPLErr SetMetadataItem( const char * pszName,
                                 const char * pszValue,
                                 const char * pszDomain = "" ) override;
#endif

};

int ECWTranslateFromWKT( const OGRSpatialReference *poSRS,
                         char *pszProjection,
                         int nProjectionLen,
                         char *pszDatum,
                         int nDatumLen,
                         char *pszUnits);

CellSizeUnits ECWTranslateToCellSizeUnits(const char* pszUnits);
const char* ECWTranslateFromCellSizeUnits(CellSizeUnits eUnits);

#endif /* def FRMT_ecw */

#endif /* ndef GDAL_ECW_H_INCLUDED */
