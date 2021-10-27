/******************************************************************************
 *
 * Project:  SGI Image Driver
 * Purpose:  Implement SGI Image Support based on Paul Bourke's SGI Image code.
 *           http://astronomy.swin.edu.au/~pbourke/dataformats/sgirgb/
 *           ftp://ftp.sgi.com/graphics/SGIIMAGESPEC
 * Authors:  Mike Mazzella (GDAL driver)
 *           Paul Bourke (original SGI format code)
 *           Frank Warmerdam (write support)
 *
 ******************************************************************************
 * Copyright (c) 2005, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2008-2010, Even Rouault <even dot rouault at spatialys.com>
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
#include "cpl_string.h"
#include "gdal_frmts.h"
#include "gdal_pam.h"

#include <algorithm>

CPL_CVSID("$Id$")

struct ImageRec
{
    GUInt16 imagic;
    GByte type;
    GByte bpc;
    GUInt16 dim;
    GUInt16 xsize;
    GUInt16 ysize;
    GUInt16 zsize;
    GUInt32 min;
    GUInt32 max;
    char wasteBytes[4];
    char name[80];
    GUInt32 colorMap;

    VSILFILE* file;
    std::string fileName;
    int tmpSize;
    unsigned char* tmp;
    GUInt32 rleEnd;
    int     rleTableDirty;
    GUInt32* rowStart;
    GInt32* rowSize;

    ImageRec()
            : imagic(0),
              type(0),
              bpc(1),
              dim(0),
              xsize(0),
              ysize(0),
              zsize(0),
              min(0),
              max(0),
              colorMap(0),
              file(nullptr),
              fileName(""),
              tmpSize(0),
              tmp(nullptr),
              rleEnd(0),
              rleTableDirty(FALSE),
              rowStart(nullptr),
              rowSize(nullptr)
        {
            memset(wasteBytes, 0, 4);
            memset(name, 0, 80);
        }

    void Swap()
        {
#ifdef CPL_LSB
            CPL_SWAP16PTR(&imagic);
            CPL_SWAP16PTR(&dim);
            CPL_SWAP16PTR(&xsize);
            CPL_SWAP16PTR(&ysize);
            CPL_SWAP16PTR(&zsize);
            CPL_SWAP32PTR(&min);
            CPL_SWAP32PTR(&max);
#endif
        }
};

/************************************************************************/
/*                            ConvertLong()                             */
/************************************************************************/
#ifdef CPL_LSB
static void ConvertLong(GUInt32* array, GInt32 length)
{
   GUInt32* ptr = reinterpret_cast<GUInt32*>( array );
   while(length--)
   {
     CPL_SWAP32PTR(ptr);
     ptr ++;
   }
}
#else
static void ConvertLong(GUInt32* /*array*/, GInt32 /*length */)
{
}
#endif

/************************************************************************/
/*                            ImageGetRow()                             */
/************************************************************************/
static CPLErr ImageGetRow(ImageRec* image, unsigned char* buf, int y, int z)
{
    y = image->ysize - 1 - y;

    if( static_cast<int>( image->type ) != 1)
    {
        VSIFSeekL(image->file, 512+(y*static_cast<vsi_l_offset>(image->xsize))+(z*static_cast<vsi_l_offset>(image->xsize)*static_cast<vsi_l_offset>(image->ysize)), SEEK_SET);
        if(VSIFReadL(buf, 1, image->xsize, image->file) != image->xsize)
        {
            CPLError(CE_Failure, CPLE_OpenFailed, "file read error: row (%d) of (%s)\n", y, image->fileName.empty() ? "none" : image->fileName.c_str());
            return CE_Failure;
        }
        return CE_None;
    }

    // Image type 1.

    // reads row
    if( image->rowSize[y+z*image->ysize] < 0 ||
        image->rowSize[y+z*image->ysize] > image->tmpSize )
    {
        return CE_Failure;
    }
    VSIFSeekL( image->file,
               static_cast<long>( image->rowStart[y+z*image->ysize] ),
               SEEK_SET);
    if( VSIFReadL( image->tmp, 1, static_cast<GUInt32>(
           image->rowSize[y+z*image->ysize]), image->file )
        != static_cast<GUInt32>( image->rowSize[y+z*image->ysize] ) )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "file read error: row (%d) of (%s)\n",
                  y,
                  image->fileName.empty() ? "none" : image->fileName.c_str() );
        return CE_Failure;
    }

    // expands row
    unsigned char *iPtr = image->tmp;
    unsigned char *oPtr = buf;
    int xsizeCount = 0;
    for(;;)
    {
        unsigned char pixel = *iPtr++;
        int count = static_cast<int>( pixel & 0x7F );
        if(!count)
        {
            if(xsizeCount != image->xsize)
            {
                CPLError( CE_Failure, CPLE_OpenFailed,
                          "file read error: row (%d) of (%s)\n",
                          y,
                          image->fileName.empty() ? "none" : image->fileName.c_str() );
                return CE_Failure;
            }
            else
            {
                break;
            }
        }

        if( xsizeCount + count > image->xsize )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Wrong repetition number that would overflow data "
                      "at line %d", y );
            return CE_Failure;
        }

        if(pixel & 0x80)
        {
          memcpy(oPtr, iPtr, count);
          iPtr += count;
        }
        else
        {
            pixel = *iPtr++;
            memset(oPtr, pixel, count);
        }
        oPtr += count;
        xsizeCount += count;
    }

    return CE_None;
}

/************************************************************************/
/* ==================================================================== */
/*                              SGIDataset                              */
/* ==================================================================== */
/************************************************************************/

class SGIRasterBand;

class SGIDataset final: public GDALPamDataset
{
    friend class SGIRasterBand;

    VSILFILE*  fpImage;

    int    bGeoTransformValid;
    double adfGeoTransform[6];

    ImageRec image;

public:
    SGIDataset();
    virtual ~SGIDataset();

    virtual CPLErr GetGeoTransform(double*) override;
    static GDALDataset* Open(GDALOpenInfo*);
    static GDALDataset *Create( const char * pszFilename,
                                int nXSize, int nYSize, int nBands,
                                GDALDataType eType, char **papszOptions );
};

/************************************************************************/
/* ==================================================================== */
/*                            SGIRasterBand                             */
/* ==================================================================== */
/************************************************************************/

class SGIRasterBand final: public GDALPamRasterBand
{
    friend class SGIDataset;

public:
    SGIRasterBand(SGIDataset*, int);

    virtual CPLErr IReadBlock(int, int, void*) override;
    virtual CPLErr IWriteBlock(int, int, void*) override;
    virtual GDALColorInterp GetColorInterpretation() override;
};

/************************************************************************/
/*                           SGIRasterBand()                            */
/************************************************************************/

SGIRasterBand::SGIRasterBand( SGIDataset* poDSIn, int nBandIn )

{
  poDS = poDSIn;
  nBand = nBandIn;

  if(static_cast<int>( poDSIn->image.bpc ) == 1)
      eDataType = GDT_Byte;
  else
      eDataType = GDT_Int16;

  nBlockXSize = poDSIn->nRasterXSize;
  nBlockYSize = 1;
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr SGIRasterBand::IReadBlock( CPL_UNUSED int nBlockXOff,
                                  int nBlockYOff,
                                  void* pImage )
{
    SGIDataset* poGDS = reinterpret_cast<SGIDataset *>( poDS );

    CPLAssert(nBlockXOff == 0);

/* -------------------------------------------------------------------- */
/*      Load the desired data into the working buffer.              */
/* -------------------------------------------------------------------- */
    return ImageGetRow( &(poGDS->image),
                        reinterpret_cast<unsigned char*>( pImage ),
                        nBlockYOff, nBand - 1 );
}

/************************************************************************/
/*                             IWritelock()                             */
/************************************************************************/

CPLErr SGIRasterBand::IWriteBlock(CPL_UNUSED int nBlockXOff,
                                  int nBlockYOff,
                                  void*  pImage)
{
    CPLAssert(nBlockXOff == 0);

    SGIDataset* poGDS = reinterpret_cast<SGIDataset *>( poDS);
    ImageRec *image = &(poGDS->image);

/* -------------------------------------------------------------------- */
/*      Handle the fairly trivial non-RLE case.                         */
/* -------------------------------------------------------------------- */
    if( image->type == 0 )
    {
        VSIFSeekL(image->file,
                  512 + (nBlockYOff*static_cast<vsi_l_offset>(image->xsize))
                  + ((nBand-1)*static_cast<vsi_l_offset>(image->xsize)*static_cast<vsi_l_offset>(image->ysize) ),
                  SEEK_SET);
        if(VSIFWriteL(pImage, 1, image->xsize, image->file) != image->xsize)
        {
            CPLError(CE_Failure, CPLE_OpenFailed,
                     "file write error: row (%d)\n", nBlockYOff );
            return CE_Failure;
        }
        return CE_None;
    }

/* -------------------------------------------------------------------- */
/*      Handle RLE case.                                                */
/* -------------------------------------------------------------------- */
    const GByte *pabyRawBuf = reinterpret_cast<const GByte *>( pImage );
    GByte *pabyRLEBuf = reinterpret_cast<GByte *>(
        CPLMalloc( image->xsize * 2 + 6 ) );

    int iX = 0;
    int nRLEBytes = 0;

    while( iX < image->xsize )
    {
        int nRepeatCount = 1;

        while( iX + nRepeatCount < image->xsize
               && nRepeatCount < 127
               && pabyRawBuf[iX + nRepeatCount] == pabyRawBuf[iX] )
            nRepeatCount++;

        if( nRepeatCount > 2
            || iX + nRepeatCount == image->xsize
            || (iX + nRepeatCount < image->xsize - 3
                && pabyRawBuf[iX + nRepeatCount + 1]
                == pabyRawBuf[iX + nRepeatCount + 2]
                && pabyRawBuf[iX + nRepeatCount + 1]
                == pabyRawBuf[iX + nRepeatCount + 3]) )
        { // encode a constant run.
            pabyRLEBuf[nRLEBytes++] = static_cast<GByte>( nRepeatCount );
            pabyRLEBuf[nRLEBytes++] = pabyRawBuf[iX];
            iX += nRepeatCount;
        }
        else
        { // copy over mixed data.
            for( nRepeatCount = 1;
                 iX + nRepeatCount < image->xsize && nRepeatCount < 127;
                 nRepeatCount++ )
            {
                if( iX + nRepeatCount + 3 >= image->xsize )
                    continue;

                // quit if the next 3 pixels match
                if( pabyRawBuf[iX + nRepeatCount]
                    == pabyRawBuf[iX + nRepeatCount+1]
                    && pabyRawBuf[iX + nRepeatCount]
                    == pabyRawBuf[iX + nRepeatCount+2] )
                    break;
            }

            pabyRLEBuf[nRLEBytes++] = static_cast<GByte>( 0x80 | nRepeatCount );
            memcpy( pabyRLEBuf + nRLEBytes,
                    pabyRawBuf + iX,
                    nRepeatCount );

            nRLEBytes += nRepeatCount;
            iX += nRepeatCount;
        }
    }

    // EOL marker.
    pabyRLEBuf[nRLEBytes++] = 0;

/* -------------------------------------------------------------------- */
/*      Write RLE Buffer at end of file.                                */
/* -------------------------------------------------------------------- */
    const int row = (image->ysize - nBlockYOff - 1) + (nBand-1) * image->ysize;

    VSIFSeekL(image->file, 0, SEEK_END );

    image->rowStart[row] = static_cast<GUInt32>( VSIFTellL( image->file ) );
    image->rowSize[row] = nRLEBytes;
    image->rleTableDirty = TRUE;

    if( static_cast<int>( VSIFWriteL(pabyRLEBuf, 1, nRLEBytes, image->file) )
        != nRLEBytes )
    {
        CPLFree( pabyRLEBuf );
        CPLError(CE_Failure, CPLE_OpenFailed,
                 "file write error: row (%d)\n", nBlockYOff );
        return CE_Failure;
    }

    CPLFree( pabyRLEBuf );

    return CE_None;
}

/************************************************************************/
/*                       GetColorInterpretation()                       */
/************************************************************************/

GDALColorInterp SGIRasterBand::GetColorInterpretation()

{
    SGIDataset* poGDS = reinterpret_cast<SGIDataset *>( poDS );

    if(poGDS->nBands == 1)
        return GCI_GrayIndex;
    else if(poGDS->nBands == 2)
    {
        if(nBand == 1)
            return GCI_GrayIndex;
        else
            return GCI_AlphaBand;
    }
    else if(poGDS->nBands == 3)
    {
        if(nBand == 1)
            return GCI_RedBand;
        else if(nBand == 2)
            return GCI_GreenBand;
        else
            return GCI_BlueBand;
    }
    else if(poGDS->nBands == 4)
    {
        if(nBand == 1)
            return GCI_RedBand;
        else if(nBand == 2)
            return GCI_GreenBand;
        else if(nBand == 3)
            return GCI_BlueBand;
        else
            return GCI_AlphaBand;
    }
    return GCI_Undefined;
}

/************************************************************************/
/* ==================================================================== */
/*                             SGIDataset                               */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                            SGIDataset()                              */
/************************************************************************/

SGIDataset::SGIDataset() :
    fpImage(nullptr),
    bGeoTransformValid(FALSE)
{
    adfGeoTransform[0] = 0.0;
    adfGeoTransform[1] = 1.0;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = 0.0;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = 1.0;
}

/************************************************************************/
/*                           ~SGIDataset()                            */
/************************************************************************/

SGIDataset::~SGIDataset()

{
    FlushCache(true);

    // Do we need to write out rle table?
    if( image.rleTableDirty )
    {
        CPLDebug( "SGI", "Flushing RLE offset table." );
        ConvertLong( image.rowStart, image.ysize * image.zsize );
        ConvertLong( reinterpret_cast<GUInt32 *>( image.rowSize ),
                     image.ysize * image.zsize );

        VSIFSeekL( fpImage, 512, SEEK_SET );
        size_t nSize = static_cast<size_t>(image.ysize) * static_cast<size_t>(image.zsize);
        VSIFWriteL( image.rowStart, 4, nSize, fpImage );
        VSIFWriteL( image.rowSize, 4, nSize, fpImage );
        image.rleTableDirty = FALSE;
    }

    if(fpImage != nullptr)
        VSIFCloseL(fpImage);

    CPLFree(image.tmp);
    CPLFree(image.rowSize);
    CPLFree(image.rowStart);
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr SGIDataset::GetGeoTransform(double * padfTransform)

{
    if(bGeoTransformValid)
    {
        memcpy(padfTransform, adfGeoTransform, sizeof(double)*6);
        return CE_None;
    }

    return GDALPamDataset::GetGeoTransform(padfTransform);
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset* SGIDataset::Open(GDALOpenInfo* poOpenInfo)

{
/* -------------------------------------------------------------------- */
/*      First we check to see if the file has the expected header       */
/*      bytes.                                                          */
/* -------------------------------------------------------------------- */
    if(poOpenInfo->nHeaderBytes < 12 || poOpenInfo->fpL == nullptr )
        return nullptr;

    ImageRec tmpImage;
    memcpy(&tmpImage.imagic, poOpenInfo->pabyHeader + 0, 2);
    memcpy(&tmpImage.type,   poOpenInfo->pabyHeader + 2, 1);
    memcpy(&tmpImage.bpc,    poOpenInfo->pabyHeader + 3, 1);
    memcpy(&tmpImage.dim,    poOpenInfo->pabyHeader + 4, 2);
    memcpy(&tmpImage.xsize,  poOpenInfo->pabyHeader + 6, 2);
    memcpy(&tmpImage.ysize,  poOpenInfo->pabyHeader + 8, 2);
    memcpy(&tmpImage.zsize,  poOpenInfo->pabyHeader + 10, 2);
    tmpImage.Swap();

    if(tmpImage.imagic != 474)
        return nullptr;

    if (tmpImage.type != 0 && tmpImage.type != 1)
        return nullptr;

    if (tmpImage.bpc != 1 && tmpImage.bpc != 2)
        return nullptr;

    if (tmpImage.dim != 1 && tmpImage.dim != 2 && tmpImage.dim != 3)
        return nullptr;

    if(tmpImage.bpc != 1)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "The SGI driver only supports 1 byte channel values.\n");
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    SGIDataset* poDS = new SGIDataset();
    poDS->eAccess = poOpenInfo->eAccess;
    poDS->fpImage = poOpenInfo->fpL;
    poOpenInfo->fpL = nullptr;

/* -------------------------------------------------------------------- */
/*      Read pre-image data after ensuring the file is rewound.         */
/* -------------------------------------------------------------------- */
    VSIFSeekL(poDS->fpImage, 0, SEEK_SET);
    if(VSIFReadL(reinterpret_cast<void*>( &(poDS->image) ),
                 1, 12, poDS->fpImage)
       != 12)
    {
        CPLError(CE_Failure, CPLE_OpenFailed, "file read error while reading header in sgidataset.cpp");
        delete poDS;
        return nullptr;
    }
    poDS->image.Swap();
    poDS->image.file = poDS->fpImage;
    poDS->image.fileName = poOpenInfo->pszFilename;

/* -------------------------------------------------------------------- */
/*      Capture some information from the file that is of interest.     */
/* -------------------------------------------------------------------- */
    poDS->nRasterXSize = poDS->image.xsize;
    poDS->nRasterYSize = poDS->image.ysize;
    if (poDS->nRasterXSize <= 0 || poDS->nRasterYSize <= 0)
    {
        CPLError(CE_Failure, CPLE_OpenFailed,
                     "Invalid image dimensions : %d x %d", poDS->nRasterXSize, poDS->nRasterYSize);
        delete poDS;
        return nullptr;
    }
    poDS->nBands = std::max(static_cast<GUInt16>(1), poDS->image.zsize);
    if (poDS->nBands > 256)
    {
        CPLError(CE_Failure, CPLE_OpenFailed,
                     "Too many bands : %d", poDS->nBands);
        delete poDS;
        return nullptr;
    }

    const int numItems
        = (static_cast<int>( poDS->image.bpc ) == 1) ? 256 : 65536;
    if( poDS->image.xsize > INT_MAX / numItems )
    {
        delete poDS;
        return nullptr;
    }
    poDS->image.tmpSize = poDS->image.xsize * numItems;
    poDS->image.tmp = (unsigned char*)VSI_CALLOC_VERBOSE(poDS->image.xsize,numItems);
    if (poDS->image.tmp == nullptr)
    {
        delete poDS;
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Read RLE Pointer tables.                                        */
/* -------------------------------------------------------------------- */
    if( static_cast<int>( poDS->image.type ) == 1 ) // RLE compressed
    {
        const size_t x = static_cast<size_t>(poDS->image.ysize) * poDS->nBands * sizeof(GUInt32);
        poDS->image.rowStart = reinterpret_cast<GUInt32*>(
            VSI_MALLOC2_VERBOSE(poDS->image.ysize, poDS->nBands * sizeof(GUInt32) ) );
        poDS->image.rowSize = reinterpret_cast<GInt32 *>(
            VSI_MALLOC2_VERBOSE(poDS->image.ysize, poDS->nBands * sizeof(GUInt32) ) );
        if (poDS->image.rowStart == nullptr || poDS->image.rowSize == nullptr)
        {
            delete poDS;
            return nullptr;
        }
        memset(poDS->image.rowStart, 0, x);
        memset(poDS->image.rowSize, 0, x);
        poDS->image.rleEnd = static_cast<GUInt32>(512 + (2 * x));
        VSIFSeekL(poDS->fpImage, 512, SEEK_SET);
        if( VSIFReadL(poDS->image.rowStart, 1, x, poDS->image.file ) != x )
        {
            delete poDS;
            CPLError(CE_Failure, CPLE_OpenFailed,
                     "file read error while reading start positions in sgidataset.cpp");
            return nullptr;
        }
        if( VSIFReadL(poDS->image.rowSize, 1, x, poDS->image.file) != x)
        {
            delete poDS;
            CPLError(CE_Failure, CPLE_OpenFailed,
                     "file read error while reading row lengths in sgidataset.cpp");
            return nullptr;
        }
        ConvertLong(poDS->image.rowStart,
                    static_cast<int>(x / static_cast<int>( sizeof(GUInt32))) );
        ConvertLong(reinterpret_cast<GUInt32 *>( poDS->image.rowSize ),
                    static_cast<int>(x / static_cast<int>( sizeof(GInt32) )) );
    }
    else // uncompressed.
    {
        poDS->image.rowStart = nullptr;
        poDS->image.rowSize = nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    for(int iBand = 0; iBand < poDS->nBands; iBand++)
        poDS->SetBand(iBand+1, new SGIRasterBand(poDS, iBand+1));

/* -------------------------------------------------------------------- */
/*      Check for world file.                                           */
/* -------------------------------------------------------------------- */
    poDS->bGeoTransformValid =
        GDALReadWorldFile(poOpenInfo->pszFilename, ".wld",
                          poDS->adfGeoTransform);

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    poDS->SetDescription(poOpenInfo->pszFilename);
    poDS->TryLoadXML();

/* -------------------------------------------------------------------- */
/*      Check for overviews.                                            */
/* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename );

    return poDS;
}

/************************************************************************/
/*                               Create()                               */
/************************************************************************/

GDALDataset *SGIDataset::Create( const char * pszFilename,
                                 int nXSize,
                                 int nYSize,
                                 int nBands,
                                 GDALDataType eType,
                                 CPL_UNUSED char **papszOptions )
{
    if( eType != GDT_Byte )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
              "Attempt to create SGI dataset with an illegal\n"
              "data type (%s), only Byte supported by the format.\n",
              GDALGetDataTypeName(eType) );

        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Open the file for output.                                       */
/* -------------------------------------------------------------------- */
    VSILFILE *fp = VSIFOpenL( pszFilename, "w" );
    if( fp == nullptr )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Failed to create file '%s': %s",
                  pszFilename, VSIStrerror( errno ) );
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Prepare and write 512 byte header.                              */
/* -------------------------------------------------------------------- */
    GByte abyHeader[512];

    memset( abyHeader, 0, 512 );

    abyHeader[0] = 1;
    abyHeader[1] = 218;
    abyHeader[2] = 1; // RLE
    abyHeader[3] = 1;  // 8bit

    GInt16 nShortValue;
    if( nBands == 1 )
        nShortValue = CPL_MSBWORD16(2);
    else
        nShortValue = CPL_MSBWORD16(3);
    memcpy( abyHeader + 4, &nShortValue, 2 );

    nShortValue = CPL_MSBWORD16(nXSize);
    memcpy( abyHeader + 6, &nShortValue, 2 );

    nShortValue = CPL_MSBWORD16(nYSize);
    memcpy( abyHeader + 8, &nShortValue, 2 );

    nShortValue = CPL_MSBWORD16(nBands);
    memcpy( abyHeader + 10, &nShortValue, 2 );

    GInt32 nIntValue = CPL_MSBWORD32(0);
    memcpy( abyHeader + 12, &nIntValue, 4 );

    GUInt32 nUIntValue = CPL_MSBWORD32(255);
    memcpy( abyHeader + 16, &nUIntValue, 4 );

    VSIFWriteL( abyHeader, 1, 512, fp );

/* -------------------------------------------------------------------- */
/*      Create our RLE compressed zero-ed dummy line.                   */
/* -------------------------------------------------------------------- */
    GByte *pabyRLELine = reinterpret_cast<GByte *>(
        CPLMalloc( ( nXSize / 127 ) * 2 + 4 ) );

    int nPixelsRemaining = nXSize;
    GInt32 nRLEBytes = 0;
    while( nPixelsRemaining > 0 )
    {
        pabyRLELine[nRLEBytes] = static_cast<GByte>(
            std::min( 127, nPixelsRemaining  ) );
        pabyRLELine[nRLEBytes+1] = 0;
        nPixelsRemaining -= pabyRLELine[nRLEBytes];

        nRLEBytes += 2;
    }

/* -------------------------------------------------------------------- */
/*      Prepare and write RLE offset/size tables with everything        */
/*      zeroed indicating dummy lines.                                  */
/* -------------------------------------------------------------------- */
    const int nTableLen = nYSize * nBands;
    GInt32 nDummyRLEOffset = 512 + 4 * nTableLen * 2;

    CPL_MSBPTR32( &nRLEBytes );
    CPL_MSBPTR32( &nDummyRLEOffset );

    for( int i = 0; i < nTableLen; i++ )
        VSIFWriteL( &nDummyRLEOffset, 1, 4, fp );

    for( int i = 0; i < nTableLen; i++ )
        VSIFWriteL( &nRLEBytes, 1, 4, fp );

/* -------------------------------------------------------------------- */
/*      write the dummy RLE blank line.                                 */
/* -------------------------------------------------------------------- */
    CPL_MSBPTR32( &nRLEBytes );
    if( static_cast<GInt32>( VSIFWriteL( pabyRLELine, 1, nRLEBytes, fp ) )
        != nRLEBytes )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Failure writing SGI file '%s'.\n%s",
                  pszFilename,
                  VSIStrerror( errno ) );
        VSIFCloseL( fp );
        CPLFree( pabyRLELine );
        return nullptr;
    }

    VSIFCloseL( fp );
    CPLFree( pabyRLELine );

    return reinterpret_cast<GDALDataset *>(
        GDALOpen( pszFilename, GA_Update ) );
}

/************************************************************************/
/*                         GDALRegister_SGI()                           */
/************************************************************************/

void GDALRegister_SGI()

{
    if( GDALGetDriverByName( "SGI" ) != nullptr )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription("SGI");
    poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, "SGI Image File Format 1.0" );
    poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "rgb" );
    poDriver->SetMetadataItem( GDAL_DMD_MIMETYPE, "image/rgb" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "drivers/raster/sgi.html" );
    poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES, "Byte" );
    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

    poDriver->pfnOpen = SGIDataset::Open;
    poDriver->pfnCreate = SGIDataset::Create;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
