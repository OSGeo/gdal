/******************************************************************************
 * $Id$
 *
 * Project:  Raster Matrix Format
 * Purpose:  Private class declarations for the RMF classes used to read/write
 *           GIS "Integratsia" raster files (also known as "Panorama" GIS).
 * Author:   Andrey Kiselev, dron@ak4719.spb.edu
 *
 ******************************************************************************
 * Copyright (c) 2007, Andrey Kiselev <dron@ak4719.spb.edu>
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

#include "gdal_priv.h"

#define RMF_HEADER_SIZE         320
#define RMF_EXT_HEADER_SIZE     320

#define RMF_COMPRESSION_NONE    0
#define RMF_COMPRESSION_LZW     1
#define RMF_COMPRESSION_DEM     32

enum RMFType
{
    RMFT_RSW,       // Raster map
    RMFT_MTW        // Digital elevation model
};

/************************************************************************/
/*                            RMFHeader                                 */
/************************************************************************/

typedef struct
{
#define RMF_SIGNATURE_SIZE 4
    char        bySignature[RMF_SIGNATURE_SIZE];// "RSW" for raster
                                                // map or "MTW" for DEM
    GUInt32     iVersion;
    GUInt32     nSize;                          // File size in bytes
    GUInt32     nOvrOffset;                     // Offset to overview
    GUInt32     iUserID;
#define RMF_NAME_SIZE 32
    GByte       byName[RMF_NAME_SIZE];
    GUInt32     nBitDepth;                      // Number of bits per pixel
    GUInt32     nHeight;                        // Image length
    GUInt32     nWidth;                         // Image width
    GUInt32     nXTiles;                        // Number of tiles in line
    GUInt32     nYTiles;                        // Number of tiles in column
    GUInt32     nTileHeight;
    GUInt32     nTileWidth;
    GUInt32     nLastTileHeight;
    GUInt32     nLastTileWidth;
    GUInt32     nROIOffset;
    GUInt32     nROISize;
    GUInt32     nClrTblOffset;                  // Position and size
    GUInt32     nClrTblSize;                    // of the colour table
    GUInt32     nTileTblOffset;                 // Position and size of the
    GUInt32     nTileTblSize;                   // tile offsets/sizes table 
    GInt32      iMapType;
    GInt32      iProjection;
    double      dfScale;
    double      dfResolution;
    double      dfPixelSize;
    double      dfLLX;
    double      dfLLY;
    double      dfStdP1;
    double      dfStdP2;
    double      dfCenterLong;
    double      dfCenterLat;
    GByte       iCompression;
    GByte       iMaskType;
    GByte       iMaskStep;
    GByte       iFrameFlag;
    GUInt32     nFlagsTblOffset;
    GUInt32     nFlagsTblSize;
    GUInt32     nFileSize0;
    GUInt32     nFileSize1;
    GByte       iUnknown;
    GByte       iGeorefFlag;
    GByte       iInverse;
#define RMF_INVISIBLE_COLORS_SIZE 32
    GByte       abyInvisibleColors[RMF_INVISIBLE_COLORS_SIZE];
    double      adfElevMinMax[2];
    double      dfNoData;
    GUInt32     iElevationUnit;
    GByte       iElevationType;
    GUInt32     nExtHdrOffset;
    GUInt32     nExtHdrSize;
} RMFHeader;

/************************************************************************/
/*                            RMFExtHeader                              */
/************************************************************************/

typedef struct
{
    GInt32      nEllipsoid;
    GInt32      nDatum;
    GInt32      nZone;
} RMFExtHeader;

/************************************************************************/
/*                              RMFDataset                              */
/************************************************************************/

class RMFDataset : public GDALDataset
{
    friend class RMFRasterBand;

    RMFHeader       sHeader;
    RMFExtHeader    sExtHeader;
    RMFType         eRMFType;
    GUInt32         nXTiles;
    GUInt32         nYTiles;
    GUInt32         *paiTiles;
    
    GUInt32         nColorTableSize;
    GByte           *pabyColorTable;
    GDALColorTable  *poColorTable;
    double          adfGeoTransform[6];
    char            *pszProjection;

    char            *pszUnitType;

    int             bBigEndian;
    int             bHeaderDirty;

    const char      *pszFilename;
    VSILFILE        *fp;

    CPLErr          WriteHeader();
    static int      LZWDecompress( const GByte*, GUInt32, GByte*, GUInt32 );
    static int      DEMDecompress( const GByte*, GUInt32, GByte*, GUInt32 );
   int             (*Decompress)( const GByte*, GUInt32, GByte*, GUInt32 );

  public:
                RMFDataset();
                ~RMFDataset();

    static int          Identify( GDALOpenInfo * poOpenInfo );
    static GDALDataset  *Open( GDALOpenInfo * );
    static GDALDataset  *Create( const char *, int, int, int,
                                 GDALDataType, char ** );
    virtual void        FlushCache( void );

    virtual CPLErr      GetGeoTransform( double * padfTransform );
    virtual CPLErr      SetGeoTransform( double * );
    virtual const char  *GetProjectionRef();
    virtual CPLErr      SetProjection( const char * );
};

/************************************************************************/
/*                            RMFRasterBand                             */
/************************************************************************/

class RMFRasterBand : public GDALRasterBand
{
    friend class RMFDataset;

  private:

    GUInt32     nBytesPerPixel;
    GUInt32     nBlockSize, nBlockBytes;
    GUInt32     nLastTileXBytes, nLastTileHeight;
    GUInt32     nDataSize;

    CPLErr   ReadBuffer( GByte *, GUInt32 ) const;

  public:

                RMFRasterBand( RMFDataset *, int, GDALDataType );
                ~RMFRasterBand();

    virtual CPLErr          IReadBlock( int, int, void * );
    virtual CPLErr          IWriteBlock( int, int, void * );
    virtual const char      *GetUnitType();
    virtual GDALColorInterp GetColorInterpretation();
    virtual GDALColorTable  *GetColorTable();
    virtual CPLErr          SetUnitType(const char *);
    virtual CPLErr          SetColorTable( GDALColorTable * );
};

