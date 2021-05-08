/******************************************************************************
 * $Id$
 *
 * Project:  jpip read driver
 * Purpose:  GDAL bindings for JPIP.
 * Author:   Norman Barker, ITT VIS, norman.barker@gmail.com
 *
 ******************************************************************************
 * ITT Visual Information Systems grants you use of this code, under the following license:
 *
 * Copyright (c) 2000-2007, ITT Visual Information Solutions
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
**/

#include "gdal_pam.h"
#include "gdaljp2metadata.h"
#include "cpl_conv.h"
#include "cpl_string.h"
#include "cpl_http.h"
#include "cpl_vsi.h"
#include "cpl_multiproc.h"

#include "jpipkak_headers.h"

#include <time.h>

#if KDU_MAJOR_VERSION > 7 || (KDU_MAJOR_VERSION == 7 && KDU_MINOR_VERSION >= 5)
    using namespace kdu_core;
    using namespace kdu_supp;
#endif

static void JPIPWorkerFunc(void *);

/************************************************************************/
/* ==================================================================== */
/*                           JPIPDataSegment                            */
/* ==================================================================== */
/************************************************************************/
class JPIPDataSegment
{
private:
    long nId;
    long nAux;
    long nClassId;
    long nCodestream;
    long nOffset;
    long nLen;
    GByte* pabyData;
    int bIsFinal;
    int bIsEOR;
public:
    long GetId() const {return nId;}
    long GetAux() const {return nAux;}
    long GetClassId() const {return nClassId;}
    long GetCodestreamIdx() const {return nCodestream;}
    long GetOffset() const {return nOffset;}
    long GetLen() const {return nLen;}
    GByte* GetData(){return pabyData;}
    int IsFinal() const {return bIsFinal;}
    int IsEOR() const {return bIsEOR;}

    void SetId(long nIdIn){this->nId = nIdIn;}
    void SetAux(long nAuxIn){this->nAux = nAuxIn;}
    void SetClassId(long nClassIdIn){this->nClassId = nClassIdIn;}
    void SetCodestreamIdx(long nCodestreamIn){this->nCodestream = nCodestreamIn;}
    void SetOffset(long nOffsetIn){this->nOffset = nOffsetIn;}
    void SetLen(long nLenIn){this->nLen = nLenIn;}
    void SetData(GByte* pabyDataIn){this->pabyData = pabyDataIn;}
    void SetFinal(int bIsFinalIn){this->bIsFinal = bIsFinalIn;}
    void SetEOR(int bIsEORIn){this->bIsEOR = bIsEORIn;}
    JPIPDataSegment();
    ~JPIPDataSegment();
};

/************************************************************************/
/* ==================================================================== */
/*                           JPIPKAKDataset                             */
/* ==================================================================== */
/************************************************************************/

class JPIPKAKDataset final: public GDALPamDataset
{
private:
    int       bNeedReinitialize = FALSE;
    CPLString osRequestUrl;
    char* pszPath = nullptr;
    char* pszCid = nullptr;
    OGRSpatialReference m_oSRS{};

    int nPos = 0;
    int nVBASLen = 0;
    int nVBASFirstByte = 0;
    int nClassId = 0;
    int nQualityLayers = 0;
    int nResLevels = 0;
    int nComps = 0;
    int nBitDepth = 0;
    int bYCC = 0;
    GDALDataType eDT = GDT_Unknown;

    int nCodestream = 0;
    long nDatabins = 0;

    double adfGeoTransform[6];

    int bWindowDone = FALSE;
    int bGeoTransformValid = FALSE;

    int nGCPCount = 0;
    GDAL_GCP *pasGCPList = nullptr;

    // kakadu
    kdu_codestream *poCodestream = nullptr;
    kdu_region_decompressor *poDecompressor = nullptr;
    kdu_cache *poCache = nullptr;

    long ReadVBAS(GByte* pabyData, int nLen);
    JPIPDataSegment* ReadSegment(GByte* pabyData, int nLen, int& bError);
    int Initialize(const char* url, int bReinitializing );
    void Deinitialize();
    static int KakaduClassId(int nClassId);

    CPLMutex *pGlobalMutex = nullptr;

    // support two communication threads to the server, a main and an overview thread
    volatile int bHighThreadRunning = 0;
    volatile int bLowThreadRunning = 0;
    volatile int bHighThreadFinished = 0;
    volatile int bLowThreadFinished = 0;

    // transmission counts
    volatile long nHighThreadByteCount = 0;
    volatile long nLowThreadByteCount = 0;

    static void KakaduInitialize();

public:
    JPIPKAKDataset();
    virtual ~JPIPKAKDataset();

    // progressive methods
    virtual GDALAsyncReader* BeginAsyncReader(int xOff, int yOff,
                                              int xSize, int ySize,
                                              void *pBuf,
                                              int bufXSize, int bufYSize,
                                              GDALDataType bufType,
                                              int nBandCount, int* bandMap,
                                              int nPixelSpace, int nLineSpace,
                                              int nBandSpace,
                                              char **papszOptions) override;

    virtual void EndAsyncReader(GDALAsyncReader *) override;
    int GetNQualityLayers() const {return nQualityLayers;}
    int GetNResolutionLevels() const {return nResLevels;}
    int GetNComponents() const {return nComps;}

    int ReadFromInput(GByte* pabyData, int nLen, int& bError );

    int TestUseBlockIO( int nXOff, int nYOff, int nXSize, int nYSize,
                        int nBufXSize, int nBufYSize, GDALDataType eDataType,
                        int nBandCount, int *panBandList ) const;

    //gdaldataset methods
    virtual CPLErr GetGeoTransform( double * ) override;
    const OGRSpatialReference* GetSpatialRef() const override;
    virtual int    GetGCPCount() override;
    const OGRSpatialReference* GetGCPSpatialRef() const override;
    virtual const GDAL_GCP *GetGCPs() override;
    virtual CPLErr IRasterIO( GDALRWFlag eRWFlag,
                              int nXOff, int nYOff, int nXSize, int nYSize,
                              void * pData, int nBufXSize, int nBufYSize,
                              GDALDataType eBufType,
                              int nBandCount, int *panBandMap,
                              GSpacing nPixelSpace, GSpacing nLineSpace,
                              GSpacing nBandSpace,
                              GDALRasterIOExtraArg* psExtraArg) override;

    static GDALDataset *Open(GDALOpenInfo *);
    static const GByte JPIP_EOR_IMAGE_DONE = 1;
    static const GByte JPIP_EOR_WINDOW_DONE = 2;
    static const GByte MAIN_HEADER_DATA_BIN_CLASS = 6;
    static const GByte META_DATA_BIN_CLASS = 8;
    static const GByte PRECINCT_DATA_BIN_CLASS = 0;
    static const GByte TILE_HEADER_DATA_BIN_CLASS = 2;
    static const GByte TILE_DATA_BIN_CLASS = 4;

    friend class JPIPKAKAsyncReader;
    friend class JPIPKAKRasterBand;
    friend void JPIPWorkerFunc(void*);
};

/************************************************************************/
/* ==================================================================== */
/*                            JPIPKAKRasterBand                          */
/* ==================================================================== */
/************************************************************************/

class JPIPKAKRasterBand final: public GDALPamRasterBand
{
    friend class JPIPKAKDataset;

    JPIPKAKDataset *poBaseDS;

    int         nDiscardLevels;

    kdu_dims    band_dims;

    int         nOverviewCount;
    JPIPKAKRasterBand **papoOverviewBand;

    kdu_codestream *oCodeStream;

    GDALColorTable oCT;
    GDALColorInterp eInterp;

public:

    JPIPKAKRasterBand( int, int, kdu_codestream *, int,
                       JPIPKAKDataset * );
    virtual ~JPIPKAKRasterBand();

    virtual CPLErr IReadBlock( int, int, void * ) override;
    virtual CPLErr IRasterIO( GDALRWFlag, int, int, int, int,
                              void *, int, int, GDALDataType,
                              GSpacing nPixelSpace, GSpacing nLineSpace,
                              GDALRasterIOExtraArg* psExtraArg ) override;

    virtual int    GetOverviewCount() override;
    virtual GDALRasterBand *GetOverview( int ) override;
};

/************************************************************************/
/* ==================================================================== */
/*                           JPIPKAKAsyncReader                         */
/* ==================================================================== */
/************************************************************************/

class JPIPKAKAsyncReader final: public GDALAsyncReader
{
private:
    void *pAppBuf;
    int  nAppPixelSpace, nAppLineSpace, nAppBandSpace;

    int nDataRead;
    int nLevel;
    int nQualityLayers;
    int bHighPriority;
    int bComplete;
    kdu_channel_mapping channels;
    kdu_coords exp_numerator, exp_denominator;

    kdu_dims rr_win; // user requested window expressed on reduced res level

    void Start();
    void Stop();

public:
    JPIPKAKAsyncReader();
    virtual ~JPIPKAKAsyncReader();

    virtual GDALAsyncStatusType GetNextUpdatedRegion(double timeout,
                                                     int* pnxbufoff,
                                                     int* pnybufoff,
                                                     int* pnxbufsize,
                                                     int* pnybufsize) override;
    void SetComplete(int bFinished){this->bComplete = bFinished;}

    friend class JPIPKAKDataset;

    CPLString  osErrorMsg;
};

/************************************************************************/
/* ==================================================================== */
/*                           JPIPRequest                                */
/* ==================================================================== */
/************************************************************************/
struct JPIPRequest
{
    int bPriority;
    CPLString osRequest;
    JPIPKAKAsyncReader* poARIO;
};
