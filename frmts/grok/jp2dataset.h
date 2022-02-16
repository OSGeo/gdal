/******************************************************************************
 *
 * Project:  JPEG2000 base driver
 * Purpose:  JPEG2000 base driver
 * Author:   Aaron Boxer, <boxerab at protonmail dot com>
 *
 ******************************************************************************
 * Copyright (c) 2010-2014, Even Rouault <even dot rouault at spatialys dot com>
 * Copyright (c) 2015, European Union (European Environment Agency)
 * Copyright (c) 2022, Grok Image Compression
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

#pragma once

#include <cstdint>
#include <cstddef>
#include "cpl_atomic_ops.h"
#include "cpl_multiproc.h"
#include "cpl_string.h"
#include "cpl_worker_thread_pool.h"
#include "gdal_frmts.h"
#include "gdaljp2abstractdataset.h"
#include "gdaljp2metadata.h"
#include "vrt/vrtdataset.h"

enum JP2_COLOR_SPACE {
	JP2_CLRSPC_UNKNOWN,
	JP2_CLRSPC_SRGB,
	JP2_CLRSPC_GRAY,
	JP2_CLRSPC_SYCC
};

enum JP2_CODEC_FORMAT {
	JP2_CODEC_JP2,
	JP2_CODEC_J2K
};


/************************************************************************/
/* ==================================================================== */
/*                           JP2Dataset                         */
/* ==================================================================== */
/************************************************************************/

class JP2Dataset;

typedef struct
{
    VSILFILE*    fp;
    vsi_l_offset nBaseOffset;
} JP2File;

class JobStruct
{
public:

    JP2Dataset* poGDS;
    int                 nBand;
    std::vector< std::pair<int, int> > oPairs;
    volatile int        nCurPair;
    int                 nBandCount;
    int                *panBandMap;
    volatile bool       bSuccess;
};

class JP2RasterBand;

class JP2Dataset : public GDALJP2AbstractDataset {
    friend class JP2RasterBand;
public:
    virtual ~JP2Dataset() = default;
    GDALColorInterp GetColorInterpretation(int nBand);

    static int Identify( GDALOpenInfo * poOpenInfo );
	static GDALDriver* CreateDriver(const char* driverVersion,
							const char* driverName ,
							const char* driverLongName,
							const char* driverHelp);
    virtual CPLErr SetGeoTransform( double* ) override;
    CPLErr SetGCPs( int nGCPCountIn, const GDAL_GCP *pasGCPListIn,
                    const OGRSpatialReference* poSRS ) override ;

    virtual CPLErr      SetMetadata( char ** papszMetadata,
                             const char * pszDomain = "" ) override;
    virtual CPLErr      SetMetadataItem( const char * pszName,
                                 const char * pszValue,
                                 const char * pszDomain = "" ) override;
    CPLErr SetSpatialRef(const OGRSpatialReference* poSRS) override;
protected:
    CPLErr IBuildOverviews( const char *pszResampling,
                                       int nOverviews, int *panOverviewList,
                                       int nListBands, int *panBandList,
                                       GDALProgressFunc pfnProgress,
                                       void *pProgressData ) override;
    virtual int         CloseDependentDatasets() override;
	void getBandInfo(int nBand,
						int &nBlockXSize,
						int &nBlockYSize,
						GDALDataType &eDataType,
						int &nBlocksPerRow);
    virtual CPLErr      ReadBlock( int nBand, VSILFILE* fp,
                           int nBlockXOff, int nBlockYOff, void * pImage,
                           int nBandCount, int *panBandMap )=0;
    virtual CPLErr  IRasterIO( GDALRWFlag eRWFlag,
                               int nXOff, int nYOff, int nXSize, int nYSize,
                               void * pData, int nBufXSize, int nBufYSize,
                               GDALDataType eBufType,
                               int nBandCount, int *panBandMap,
                               GSpacing nPixelSpace, GSpacing nLineSpace,
                               GSpacing nBandSpace,
                               GDALRasterIOExtraArg* psExtraArg) override;

    int         PreloadBlocks( JP2RasterBand* poBand,
                               int nXOff, int nYOff, int nXSize, int nYSize,
                               int nBandCount, int *panBandMap );

    static void ReadBlockInThread(void* userdata);
	int GetNumThreads();

    static void         WriteBox(VSILFILE* fp, GDALJP2Box* poBox);
    static void         WriteGDALMetadataBox( VSILFILE* fp, GDALDataset* poSrcDS,
                                       char** papszOptions );
    static void         WriteXMLBoxes( VSILFILE* fp, GDALDataset* poSrcDS,
                                       char** papszOptions );
    static void         WriteXMPBox( VSILFILE* fp, GDALDataset* poSrcDS,
                                     char** papszOptions );
    static void         WriteIPRBox( VSILFILE* fp, GDALDataset* poSrcDS,
                                     char** papszOptions );
	static int FloorPowerOfTwo(int nVal);
	static vsi_l_offset FindCodeStream( VSILFILE* fp,
	                                               vsi_l_offset* pnLength );
	std::string  m_osFilename;
    VSILFILE   *fp = nullptr; /* Large FILE API */
    vsi_l_offset nCodeStreamStart = 0;
    vsi_l_offset nCodeStreamLength = 0;

    int         nRedIndex = 0;
    int         nGreenIndex = 1;
    int         nBlueIndex = 2;
    int         nAlphaIndex = -1;

    int         bIs420 = FALSE;

    int         nParentXSize = 0;
    int         nParentYSize = 0;
    int         iLevel = 0;
    int         nOverviewCount = 0;
    bool        bUseSetDecodeArea = false;
    bool        bSingleTiled = false;
    int*             m_pnLastLevel = nullptr;
    int         m_nX0 = 0;
    int         m_nY0 = 0;

    int         nThreads = -1;
    int         m_nBlocksToLoad = 0;
    int         bEnoughMemoryToLoadOtherBands = TRUE;
    int         bRewrite = FALSE;
    int         bHasGeoreferencingAtOpening = FALSE;
    JP2Dataset** papoOverviewDS = nullptr;
    JP2_COLOR_SPACE eColorSpace = JP2_CLRSPC_UNKNOWN;
};


/************************************************************************/
/* ==================================================================== */
/*                         JP2GrokRasterBand                            */
/* ==================================================================== */
/************************************************************************/

class JP2RasterBand final: public GDALPamRasterBand
{
  friend class JP2Dataset;
  public:
	JP2RasterBand( JP2Dataset * poDS, int nBand,
						   GDALDataType eDataType, int nBits,
						   int bPromoteTo8Bit,
						   int nBlockXSize, int nBlockYSize );
    virtual ~JP2RasterBand();

    virtual CPLErr          IReadBlock( int, int, void * ) override;
    virtual CPLErr          IRasterIO( GDALRWFlag eRWFlag,
                                  int nXOff, int nYOff, int nXSize, int nYSize,
                                  void * pData, int nBufXSize, int nBufYSize,
                                  GDALDataType eBufType,
                                  GSpacing nPixelSpace, GSpacing nLineSpace,
                                  GDALRasterIOExtraArg* psExtraArg) override;

    virtual GDALColorInterp GetColorInterpretation() override;
    virtual GDALColorTable* GetColorTable() override { return poCT; }

    virtual int             GetOverviewCount() override;
    virtual GDALRasterBand* GetOverview(int iOvrLevel) override;

    virtual int HasArbitraryOverviews() override { return poCT == nullptr; }
    int             bPromoteTo8Bit;
    GDALColorTable* poCT;
};
