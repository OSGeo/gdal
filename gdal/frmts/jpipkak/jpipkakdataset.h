/******************************************************************************
 * $Id: jpipkakdataset.cpp 2008-10-01 nbarker $
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

#include "kdu_cache.h"
#include "kdu_region_decompressor.h"
#include "kdu_file_io.h"

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
    int GetId(){return nId;}
    int GetAux(){return nAux;}
    int GetClassId(){return nClassId;}
    int GetCodestreamIdx(){return nCodestream;}
    int GetOffset(){return nOffset;}
    int GetLen(){return nLen;}
    GByte* GetData(){return pabyData;}
    int IsFinal(){return bIsFinal;}
    int IsEOR(){return bIsEOR;}

    void SetId(long nId){this->nId = nId;}
    void SetAux(long nAux){this->nAux = nAux;}
    void SetClassId(long nClassId){this->nClassId = nClassId;}
    void SetCodestreamIdx(long nCodestream){this->nCodestream = nCodestream;}
    void SetOffset(long nOffset){this->nOffset = nOffset;}
    void SetLen(long nLen){this->nLen = nLen;}
    void SetData(GByte* pabyData){this->pabyData = pabyData;}
    void SetFinal(int bIsFinal){this->bIsFinal = bIsFinal;}
    void SetEOR(int bIsEOR){this->bIsEOR = bIsEOR;}
    JPIPDataSegment();
    ~JPIPDataSegment();
};

/************************************************************************/
/* ==================================================================== */
/*                           JPIPKAKDataset                             */
/* ==================================================================== */
/************************************************************************/
class JPIPKAKDataset: public GDALPamDataset
{
private:
    int       bNeedReinitialize;
    CPLString osRequestUrl;
    char* pszTid;
    char* pszPath;
    char* pszCid;
    char* pszProjection;

    int nPos;
    int nVBASLen;
    int nVBASFirstByte;
    int nClassId;
    int nQualityLayers;
    int nResLevels;
    int nComps;
    int nBitDepth;
    int bYCC;
    GDALDataType eDT;

    int nCodestream;
    long nDatabins;

    double adfGeoTransform[6];

    int bWindowDone;
    int bGeoTransformValid;

    int nGCPCount;
    GDAL_GCP *pasGCPList;

    // kakadu
    kdu_codestream *poCodestream;
    kdu_region_decompressor *poDecompressor;
    kdu_cache *poCache;

    long ReadVBAS(GByte* pabyData, int nLen);
    JPIPDataSegment* ReadSegment(GByte* pabyData, int nLen, int& bError);
    int Initialize(const char* url, int bReinitializing );
    void Deinitialize();
    int KakaduClassId(int nClassId);

    void *pGlobalMutex;
 
    // support two communication threads to the server, a main and an overview thread
    volatile int bHighThreadRunning;
    volatile int bLowThreadRunning;
    volatile int bHighThreadFinished;
    volatile int bLowThreadFinished;
    
    // transmission counts
    volatile long nHighThreadByteCount;
    volatile long nLowThreadByteCount;

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
                                              char **papszOptions);

    virtual void EndAsyncReader(GDALAsyncReader *);
    int GetNQualityLayers(){return nQualityLayers;}
    int GetNResolutionLevels(){return nResLevels;}
    int GetNComponents(){return nComps;}

    int ReadFromInput(GByte* pabyData, int nLen, int& bError );

    int TestUseBlockIO( int nXOff, int nYOff, int nXSize, int nYSize,
                        int nBufXSize, int nBufYSize, GDALDataType eDataType, 
                        int nBandCount, int *panBandList );

    //gdaldataset methods
    virtual CPLErr GetGeoTransform( double * );
    virtual const char *GetProjectionRef(void);
    virtual int    GetGCPCount();
    virtual const char *GetGCPProjection();
    virtual const GDAL_GCP *GetGCPs();
    virtual CPLErr IRasterIO( GDALRWFlag eRWFlag,
                              int nXOff, int nYOff, int nXSize, int nYSize,
                              void * pData, int nBufXSize, int nBufYSize,
                              GDALDataType eBufType, 
                              int nBandCount, int *panBandMap,
                              GSpacing nPixelSpace, GSpacing nLineSpace,
                              GSpacing nBandSpace,
                              GDALRasterIOExtraArg* psExtraArg);

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

class JPIPKAKRasterBand : public GDALPamRasterBand
{
    friend class JPIPKAKDataset;

    JPIPKAKDataset *poBaseDS;

    int         nDiscardLevels; 

    kdu_dims 	band_dims; 

    int		nOverviewCount;
    JPIPKAKRasterBand **papoOverviewBand;

    kdu_codestream *oCodeStream;

    GDALColorTable oCT;
    GDALColorInterp eInterp;

public:

    JPIPKAKRasterBand( int, int, kdu_codestream *, int,
                       JPIPKAKDataset * );
    ~JPIPKAKRasterBand();
    
    virtual CPLErr IReadBlock( int, int, void * );
    virtual CPLErr IRasterIO( GDALRWFlag, int, int, int, int,
                              void *, int, int, GDALDataType,
                              GSpacing nPixelSpace, GSpacing nLineSpace,
                              GDALRasterIOExtraArg* psExtraArg );

    virtual int    GetOverviewCount();
    virtual GDALRasterBand *GetOverview( int );
};

/************************************************************************/
/* ==================================================================== */
/*                           JPIPKAKAsyncReader                         */
/* ==================================================================== */
/************************************************************************/

class JPIPKAKAsyncReader : public GDALAsyncReader
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
                                                     int* pnybufsize);
    void SetComplete(int bFinished){this->bComplete = bFinished;};

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
