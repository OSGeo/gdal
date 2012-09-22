/******************************************************************************
 * $Id: ecwdataset.cpp 21486 2011-01-13 17:38:17Z warmerdam $
 *
 * Project:  GDAL 
 * Purpose:  ECW (ERDAS Wavelet Compression Format) Driver Definitions
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2001-2011, Frank Warmerdam <warmerdam@pobox.com>
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

#include "gdal_pam.h"
#include "gdal_frmts.h"
#include "gdaljp2metadata.h"
#include "cpl_string.h"
#include "cpl_conv.h"
#include "cpl_multiproc.h"
#include "cpl_vsi.h"

#undef NOISY_DEBUG

#ifdef FRMT_ecw

// The following is needed on 4.x+ to enable rw support.
#if defined(HAVE_COMPRESS)
#  define ECW_COMPRESS_RW_SDK_VERSION
#endif

#if defined(_MSC_VER)
#  pragma warning(disable:4800)
#endif

#include <NCSECWClient.h>
#include <NCSECWCompressClient.h>
#include <NCSErrors.h>
#include <NCSFile.h>
#include <NCSJP2FileView.h>

/* By default, assume 3.3 SDK Version. */
#if !defined(ECWSDK_VERSION)
#  define ECWSDK_VERSION 33
#endif

#if ECWSDK_VERSION < 40

#if !defined(NO_COMPRESS)
#  define HAVE_COMPRESS
#endif

#  include <NCSJP2File.h>
#else
#  include <ECWJP2BuildNumber.h>
#  include <HeaderEditor.h>
#  define NCS_FASTCALL
#endif

#ifndef NCSFILEBASE_H
#  include <NCSJP2FileView.h>
#else
#  undef  CNCSJP2FileView
#  define CNCSJP2FileView	  CNCSFile
#endif

void ECWInitialize( void );
GDALDataset* ECWDatasetOpenJPEG2000(GDALOpenInfo* poOpenInfo);

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

/************************************************************************/
/* ==================================================================== */
/*                             JP2Userbox                               */
/* ==================================================================== */
/************************************************************************/
#ifdef HAVE_COMPRESS
class JP2UserBox : public CNCSJP2Box {

private:
    int           nDataLength;
    unsigned char *pabyData;

public:
    JP2UserBox();

    virtual ~JP2UserBox();

#if ECWSDK_VERSION >= 40
    virtual CNCSError Parse( NCS::JP2::CFile &JP2File, 
                             NCS::CIOStream &Stream);
    virtual CNCSError UnParse( NCS::JP2::CFile &JP2File, 
								NCS::CIOStream &Stream);
#else        
    virtual CNCSError Parse(class CNCSJP2File &JP2File, 
                            CNCSJPCIOStream &Stream);
    virtual CNCSError UnParse(class CNCSJP2File &JP2File, 
                              CNCSJPCIOStream &Stream);
#endif
    virtual void UpdateXLBox(void);

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

class VSIIOStream : public CNCSJPCIOStream

{
  public:
    
    INT64    startOfJPData;
    INT64    lengthOfJPData;
    VSILFILE    *fpVSIL;
    int      bWritable;
	int      nFileViewCount;
    char     *pszFilename;

    VSIIOStream() {
        nFileViewCount = 0;
        startOfJPData = 0;
        lengthOfJPData = -1;
        fpVSIL = NULL;
    }
    virtual ~VSIIOStream() {
        Close();
    }

    virtual CNCSError Close() {
        CNCSError oErr = CNCSJPCIOStream::Close();
        if( fpVSIL != NULL )
        {
            VSIFCloseL( fpVSIL );
            fpVSIL = NULL;
        }
        return oErr;
    }        
        
#if ECWSDK_VERSION >= 40
    virtual NCS::CIOStream *Clone() { return NULL; }
#endif /* ECWSDK_VERSION >= 4 */

    virtual CNCSError Access( VSILFILE *fpVSILIn, BOOLEAN bWrite,
                              const char *pszFilename, 
                              INT64 start, INT64 size = -1) {

        fpVSIL = fpVSILIn;
        startOfJPData = start;
        lengthOfJPData = size;
        bWritable = bWrite;
        VSIFSeekL(fpVSIL, startOfJPData, SEEK_SET);

        // the filename is used to establish where to put temporary files.
        // if it does not have a path to a real directory, we will 
        // substitute something. 
        CPLString osFilenameUsed = pszFilename;
        CPLString osPath = CPLGetPath( pszFilename );
        struct stat sStatBuf;
        if( osPath != "" && stat( osPath, &sStatBuf ) != 0 )
        {
            osFilenameUsed = CPLGenerateTempFilename( NULL );
            // try to preserve the extension.
            if( strlen(CPLGetExtension(pszFilename)) > 0 )
            {
                osFilenameUsed += ".";
                osFilenameUsed += CPLGetExtension(pszFilename);
            }
            CPLDebug( "ECW", "Using filename '%s' for temporary directory determination purposes.", osFilenameUsed.c_str() );
        }
        return(CNCSJPCIOStream::Open((char *)osFilenameUsed.c_str(), 
                                     (bool) bWrite));
    }

    virtual bool NCS_FASTCALL Seek() {
        return(true);
    }
    
    virtual bool NCS_FASTCALL Seek(INT64 offset, Origin origin = CURRENT) {
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

    virtual INT64 NCS_FASTCALL Tell() {
        return VSIFTellL( fpVSIL ) - startOfJPData;
    }

    virtual INT64 NCS_FASTCALL Size() {
        if( lengthOfJPData != -1 )
            return lengthOfJPData;
        else
        {
            INT64 curPos = Tell(), size;

            Seek( 0, END );
            size = Tell();
            Seek( curPos, START );

            return size;
        }
    }

    virtual bool NCS_FASTCALL Read(void* buffer, UINT32 count) {
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

    virtual bool NCS_FASTCALL Write(void* buffer, UINT32 count) {
        if( count == 0 )
            return true;
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

class ECWAsyncReader : public GDALAsyncReader
{
private:
    CNCSJP2FileView *poFileView;
    void            *hMutex;
    int              bUsingCustomStream;

    int              bUpdateReady;
    int              bComplete;

    static NCSEcwReadStatus RefreshCB( NCSFileView * );
    NCSEcwReadStatus ReadToBuffer();
    
public:
    ECWAsyncReader();
    virtual ~ECWAsyncReader();
    virtual GDALAsyncStatusType GetNextUpdatedRegion(double dfTimeout,
                                                     int* pnXBufOff,
                                                     int* pnYBufOff,
                                                     int* pnXBufSize,
                                                     int* pnYBufSize);

    friend class ECWDataset;
};
#endif /* ECWSDK_VERSION >= 40 */

/************************************************************************/
/* ==================================================================== */
/*				ECWDataset				*/
/* ==================================================================== */
/************************************************************************/

class ECWRasterBand;

class CPL_DLL ECWDataset : public GDALPamDataset
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

    int         bGeoTransformValid;
    double      adfGeoTransform[6];
    char        *pszProjection;
    int         nGCPCount;
    GDAL_GCP    *pasGCPList;

    char        **papszGMLMetadata;

    void        ECW2WKTProjection();

    void        CleanupWindow();
    int         TryWinRasterIO( GDALRWFlag, int, int, int, int,
                                GByte *, int, int, GDALDataType,
                                int, int *, int, int, int );
    CPLErr      LoadNextLine();

    static CNCSJP2FileView    *OpenFileView( const char *pszDatasetName,
                                             bool bProgressive,
                                             int &bUsingCustomStream );

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

    CPLStringList oECWMetadataList;

  public:
		ECWDataset(int bIsJPEG2000);
		~ECWDataset();
                
    static GDALDataset *Open( GDALOpenInfo *, int bIsJPEG2000 );
    static int          IdentifyJPEG2000( GDALOpenInfo * poOpenInfo );
    static GDALDataset *OpenJPEG2000( GDALOpenInfo * );
    static int          IdentifyECW( GDALOpenInfo * poOpenInfo );
    static GDALDataset *OpenECW( GDALOpenInfo * );

    virtual CPLErr IRasterIO( GDALRWFlag, int, int, int, int,
                              void *, int, int, GDALDataType,
                              int, int *, int, int, int );

    virtual CPLErr GetGeoTransform( double * );
    virtual const char *GetProjectionRef();

    virtual int    GetGCPCount();
    virtual const char *GetGCPProjection();
    virtual const GDAL_GCP *GetGCPs();

    virtual const char *GetMetadataItem( const char * pszName,
                                     const char * pszDomain = "" );
    virtual char      **GetMetadata( const char * pszDomain = "" );

    virtual CPLErr SetGeoTransform( double * padfGeoTransform );
    virtual CPLErr SetProjection( const char* pszProjection );
    virtual CPLErr SetMetadataItem( const char * pszName,
                                 const char * pszValue,
                                 const char * pszDomain = "" );
    virtual CPLErr SetMetadata( char ** papszMetadata,
                             const char * pszDomain = "" );

    virtual CPLErr AdviseRead( int nXOff, int nYOff, int nXSize, int nYSize,
                               int nBufXSize, int nBufYSize, 
                               GDALDataType eDT, 
                               int nBandCount, int *panBandList,
                               char **papszOptions );

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
                                               char **papszOptions);

    virtual void EndAsyncReader(GDALAsyncReader *);
#endif /* ECWSDK_VERSION > 40 */
};

/************************************************************************/
/* ==================================================================== */
/*                            ECWRasterBand                             */
/* ==================================================================== */
/************************************************************************/

class ECWRasterBand : public GDALPamRasterBand
{
    friend class ECWDataset;
    
    // NOTE: poDS may be altered for NITF/JPEG2000 files!
    ECWDataset     *poGDS;

    GDALColorInterp         eBandInterp;

    int                          iOverview; // -1 for base. 

    std::vector<ECWRasterBand*>  apoOverviews;

    virtual CPLErr IRasterIO( GDALRWFlag, int, int, int, int,
                              void *, int, int, GDALDataType,
                              int, int );

  public:

                   ECWRasterBand( ECWDataset *, int, int = -1 );
                   ~ECWRasterBand();

    virtual CPLErr IReadBlock( int, int, void * );
    virtual int    HasArbitraryOverviews() { return apoOverviews.size() == 0; }
    virtual int    GetOverviewCount() { return apoOverviews.size(); }
    virtual GDALRasterBand *GetOverview(int);

    virtual GDALColorInterp GetColorInterpretation();
    virtual CPLErr SetColorInterpretation( GDALColorInterp );

    virtual CPLErr AdviseRead( int nXOff, int nYOff, int nXSize, int nYSize,
                               int nBufXSize, int nBufYSize, 
                               GDALDataType eDT, char **papszOptions );
};

int ECWTranslateFromWKT( const char *pszWKT,
                         char *pszProjection,
                         int nProjectionLen,
                         char *pszDatum,
                         int nDatumLen,
                         char *pszUnits);

CellSizeUnits ECWTranslateToCellSizeUnits(const char* pszUnits);
const char* ECWTranslateFromCellSizeUnits(CellSizeUnits eUnits);

#endif /* def FRMT_ecw */

#endif /* ndef GDAL_ECW_H_INCLUDED */
