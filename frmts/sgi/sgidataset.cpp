/******************************************************************************
 * $Id$
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
 * Copyright (c) 2008-2010, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "gdal_pam.h"
#include "cpl_port.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

CPL_C_START
void	GDALRegister_SGI(void);
CPL_C_END

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
              file(NULL),
              fileName(""),
              tmp(NULL),
              rleEnd(0),
              rleTableDirty(FALSE),
              rowStart(NULL),
              rowSize(NULL)
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
static void ConvertLong(GUInt32* array, GInt32 length) 
{
#ifdef CPL_LSB
   GUInt32* ptr;
   ptr = (GUInt32*)array;
   while(length--)
     CPL_SWAP32PTR(ptr++);
#endif
}

/************************************************************************/
/*                            ImageGetRow()                             */
/************************************************************************/
static CPLErr ImageGetRow(ImageRec* image, unsigned char* buf, int y, int z) 
{
    unsigned char *iPtr, *oPtr, pixel;
    int count;

    y = image->ysize - 1 - y;

    if(int(image->type) == 1)
    {
        // reads row
        VSIFSeekL(image->file, (long)image->rowStart[y+z*image->ysize], SEEK_SET);
        if(VSIFReadL(image->tmp, 1, (GUInt32)image->rowSize[y+z*image->ysize], image->file) != (GUInt32)image->rowSize[y+z*image->ysize])
        {
            CPLError(CE_Failure, CPLE_OpenFailed, "file read error: row (%d) of (%s)\n", y, image->fileName.empty() ? "none" : image->fileName.c_str());
            return CE_Failure;
        }

        // expands row
        iPtr = image->tmp;
        oPtr = buf;
        int xsizeCount = 0;
        for(;;)
        {
            pixel = *iPtr++;
            count = (int)(pixel & 0x7F);
            if(!count)
            {
                if(xsizeCount != image->xsize)
                {
                    CPLError(CE_Failure, CPLE_OpenFailed, "file read error: row (%d) of (%s)\n", y, image->fileName.empty() ? "none" : image->fileName.c_str());
                    return CE_Failure;
                }
                else
                {
                    return CE_None;
                }
            }

            if( xsizeCount + count > image->xsize )
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Wrong repetition number that would overflow data at line %d", y);
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
    }
    else
    {
        VSIFSeekL(image->file, 512+(y*image->xsize)+(z*image->xsize*image->ysize), SEEK_SET);
        if(VSIFReadL(buf, 1, image->xsize, image->file) != image->xsize)
        {
            CPLError(CE_Failure, CPLE_OpenFailed, "file read error: row (%d) of (%s)\n", y, image->fileName.empty() ? "none" : image->fileName.c_str());
            return CE_Failure;
        }
    }

    return CE_None;
}

/************************************************************************/
/* ==================================================================== */
/*				SGIDataset				*/
/* ==================================================================== */
/************************************************************************/

class SGIRasterBand;

class SGIDataset : public GDALPamDataset
{
    friend class SGIRasterBand;

    VSILFILE*  fpImage;

    int	   bGeoTransformValid;
    double adfGeoTransform[6];

    ImageRec image;

public:
    SGIDataset();
    ~SGIDataset();

    virtual CPLErr GetGeoTransform(double*);
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

class SGIRasterBand : public GDALPamRasterBand
{
    friend class SGIDataset;

public:
    SGIRasterBand(SGIDataset*, int);

    virtual CPLErr IReadBlock(int, int, void*);
    virtual CPLErr IWriteBlock(int, int, void*);
    virtual GDALColorInterp GetColorInterpretation();
};


/************************************************************************/
/*                           SGIRasterBand()                            */
/************************************************************************/

SGIRasterBand::SGIRasterBand(SGIDataset* poDS, int nBand)

{
  this->poDS = poDS;
  this->nBand = nBand;
  if(poDS == NULL)
  {
    eDataType = GDT_Byte;
  }
  else
  {
    if(int(poDS->image.bpc) == 1)
      eDataType = GDT_Byte;
    else
      eDataType = GDT_Int16;
  }
  nBlockXSize = poDS->nRasterXSize;;
  nBlockYSize = 1;
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr SGIRasterBand::IReadBlock(int nBlockXOff, int nBlockYOff,
				 void*  pImage)

{
    SGIDataset* poGDS = (SGIDataset*) poDS;
    
    CPLAssert(nBlockXOff == 0);

/* -------------------------------------------------------------------- */
/*      Load the desired data into the working buffer.              */
/* -------------------------------------------------------------------- */
    return ImageGetRow(&(poGDS->image), (unsigned char*)pImage, nBlockYOff, nBand-1);
}

/************************************************************************/
/*                             IWritelock()                             */
/************************************************************************/

CPLErr SGIRasterBand::IWriteBlock(int nBlockXOff, int nBlockYOff,
                                  void*  pImage)

{
    SGIDataset* poGDS = (SGIDataset*) poDS;
    ImageRec *image = &(poGDS->image);
    
    CPLAssert(nBlockXOff == 0);

/* -------------------------------------------------------------------- */
/*      Handle the fairly trivial non-RLE case.                         */
/* -------------------------------------------------------------------- */
    if( image->type == 0 )
    {
        VSIFSeekL(image->file, 
                  512 + (nBlockYOff*image->xsize)
                  + ((nBand-1)*image->xsize*image->ysize ), 
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
    const GByte *pabyRawBuf = (const GByte *) pImage;
    GByte *pabyRLEBuf = (GByte *) CPLMalloc(image->xsize * 2 + 6);
    int iX = 0, nRLEBytes = 0;

    while( iX < image->xsize )
    {
        int nRepeatCount = 1;

        while( iX + nRepeatCount < image->xsize
               && nRepeatCount < 127
               && pabyRawBuf[iX + nRepeatCount] == pabyRawBuf[iX] )
            nRepeatCount++;

        if( nRepeatCount > 2 
            || iX + nRepeatCount == image->xsize
            || (iX + nRepeatCount < image->xsize - 2
                && pabyRawBuf[iX + nRepeatCount + 1] 
                == pabyRawBuf[iX + nRepeatCount + 2]
                && pabyRawBuf[iX + nRepeatCount + 1] 
                == pabyRawBuf[iX + nRepeatCount + 3]) )
        { // encode a constant run.
            pabyRLEBuf[nRLEBytes++] = (GByte) nRepeatCount; 
            pabyRLEBuf[nRLEBytes++] = pabyRawBuf[iX];
            iX += nRepeatCount;
        }
        else 
        { // copy over mixed data. 
            nRepeatCount = 1;

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

            pabyRLEBuf[nRLEBytes++] = (GByte) (0x80 | nRepeatCount); 
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
    int row = (image->ysize - nBlockYOff - 1) + (nBand-1) * image->ysize;

    VSIFSeekL(image->file, 0, SEEK_END );

    image->rowStart[row] = (GUInt32) VSIFTellL( image->file );
    image->rowSize[row] = nRLEBytes;
    image->rleTableDirty = TRUE;

    if( (int) VSIFWriteL(pabyRLEBuf, 1, nRLEBytes, image->file) != nRLEBytes )
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
    SGIDataset* poGDS = (SGIDataset*)poDS;

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

SGIDataset::SGIDataset()
  : fpImage(NULL),
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
    FlushCache();

    // Do we need to write out rle table?
    if( image.rleTableDirty )
    {
        CPLDebug( "SGI", "Flushing RLE offset table." );
        ConvertLong( image.rowStart, image.ysize * image.zsize );
        ConvertLong( (GUInt32 *) image.rowSize, image.ysize * image.zsize );

        VSIFSeekL( fpImage, 512, SEEK_SET );
        VSIFWriteL( image.rowStart, 4, image.ysize * image.zsize, fpImage );
        VSIFWriteL( image.rowSize, 4, image.ysize * image.zsize, fpImage );
        image.rleTableDirty = FALSE;
    }

    if(fpImage != NULL)
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
    else 
        return GDALPamDataset::GetGeoTransform(padfTransform);
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset* SGIDataset::Open(GDALOpenInfo* poOpenInfo)

{
/* -------------------------------------------------------------------- */
/*	First we check to see if the file has the expected header	*/
/*	bytes.								*/    
/* -------------------------------------------------------------------- */
    if(poOpenInfo->nHeaderBytes < 12)
        return NULL;

    ImageRec tmpImage;
    memcpy(&tmpImage, poOpenInfo->pabyHeader, 12);
    tmpImage.Swap();

    if(tmpImage.imagic != 474)
        return NULL;

    if (tmpImage.type != 0 && tmpImage.type != 1)
        return NULL;

    if (tmpImage.bpc != 1 && tmpImage.bpc != 2)
        return NULL;

    if (tmpImage.dim != 1 && tmpImage.dim != 2 && tmpImage.dim != 3)
        return NULL;

    if(tmpImage.bpc != 1)
    {
        CPLError(CE_Failure, CPLE_NotSupported, 
                 "The SGI driver only supports 1 byte channel values.\n");
        return NULL;
    }
  
/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    SGIDataset* poDS;

    poDS = new SGIDataset();
    poDS->eAccess = poOpenInfo->eAccess;

/* -------------------------------------------------------------------- */
/*      Open the file using the large file api.                         */
/* -------------------------------------------------------------------- */
    if( poDS->eAccess == GA_ReadOnly )
        poDS->fpImage = VSIFOpenL(poOpenInfo->pszFilename, "rb");
    else
        poDS->fpImage = VSIFOpenL(poOpenInfo->pszFilename, "rb+");
    if(poDS->fpImage == NULL)
    {
        CPLError(CE_Failure, CPLE_OpenFailed, 
                 "VSIFOpenL(%s) failed unexpectedly in sgidataset.cpp\n%s", 
                 poOpenInfo->pszFilename,
                 VSIStrerror( errno ) );
        delete poDS;
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*	Read pre-image data after ensuring the file is rewound.         */
/* -------------------------------------------------------------------- */
    VSIFSeekL(poDS->fpImage, 0, SEEK_SET);
    if(VSIFReadL((void*)(&(poDS->image)), 1, 12, poDS->fpImage) != 12)
    {
        CPLError(CE_Failure, CPLE_OpenFailed, "file read error while reading header in sgidataset.cpp");
        delete poDS;
        return NULL;
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
        return NULL;
    }
    poDS->nBands = MAX(1,poDS->image.zsize);
    if (poDS->nBands > 256)
    {
        CPLError(CE_Failure, CPLE_OpenFailed, 
                     "Too many bands : %d", poDS->nBands);
        delete poDS;
        return NULL;
    }

    int numItems = (int(poDS->image.bpc) == 1) ? 256 : 65536;
    poDS->image.tmp = (unsigned char*)VSICalloc(poDS->image.xsize,numItems);
    if (poDS->image.tmp == NULL)
    {
        CPLError(CE_Failure, CPLE_OutOfMemory, "Out of memory");
        delete poDS;
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Read RLE Pointer tables.                                        */
/* -------------------------------------------------------------------- */
    if(int(poDS->image.type) == 1) // RLE compressed
    {
        int x = poDS->image.ysize * poDS->nBands * sizeof(GUInt32);
        poDS->image.rowStart = (GUInt32*)VSIMalloc2(poDS->image.ysize, poDS->nBands * sizeof(GUInt32));
        poDS->image.rowSize = (GInt32*)VSIMalloc2(poDS->image.ysize, poDS->nBands * sizeof(GUInt32));
        if (poDS->image.rowStart == NULL || poDS->image.rowSize == NULL)
        {
            CPLError(CE_Failure, CPLE_OutOfMemory, "Out of memory");
            delete poDS;
            return NULL;
        }
        memset(poDS->image.rowStart, 0, x);
        memset(poDS->image.rowSize, 0, x);
        poDS->image.rleEnd = 512 + (2 * x);
        VSIFSeekL(poDS->fpImage, 512, SEEK_SET);
        if((int) VSIFReadL(poDS->image.rowStart, 1, x, poDS->image.file) != x)
        {
            delete poDS;
            CPLError(CE_Failure, CPLE_OpenFailed, 
                     "file read error while reading start positions in sgidataset.cpp");
            return NULL;
        }
        if((int) VSIFReadL(poDS->image.rowSize, 1, x, poDS->image.file) != x)
        {
            delete poDS;
            CPLError(CE_Failure, CPLE_OpenFailed, 
                     "file read error while reading row lengths in sgidataset.cpp");
            return NULL;
        }
        ConvertLong(poDS->image.rowStart, x/(int)sizeof(GUInt32));
        ConvertLong((GUInt32*)poDS->image.rowSize, x/(int)sizeof(GInt32));
    }
    else // uncompressed.
    {
        poDS->image.rowStart = NULL;
        poDS->image.rowSize = NULL;
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
                                 int nXSize, int nYSize, int nBands,
                                 GDALDataType eType, char **papszOptions )

{
    if( eType != GDT_Byte )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
              "Attempt to create SGI dataset with an illegal\n"
              "data type (%s), only Byte supported by the format.\n",
              GDALGetDataTypeName(eType) );

        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Open the file for output.                                       */
/* -------------------------------------------------------------------- */
    VSILFILE *fp = VSIFOpenL( pszFilename, "w" );
    if( fp == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed, 
                  "Failed to create file '%s': %s", 
                  pszFilename, VSIStrerror( errno ) );
        return NULL;
    }
    
/* -------------------------------------------------------------------- */
/*      Prepare and write 512 byte header.                              */
/* -------------------------------------------------------------------- */
    GByte abyHeader[512];
    GInt16 nShortValue;
    GInt32 nIntValue;

    memset( abyHeader, 0, 512 );

    abyHeader[0] = 1;
    abyHeader[1] = 218;
    abyHeader[2] = 1; // RLE
    abyHeader[3] = 1;  // 8bit

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

    nIntValue = CPL_MSBWORD32(0);
    memcpy( abyHeader + 12, &nIntValue, 4 );
    
    nIntValue = CPL_MSBWORD32(255);
    memcpy( abyHeader + 16, &nIntValue, 4 );
    
    VSIFWriteL( abyHeader, 1, 512, fp );

/* -------------------------------------------------------------------- */
/*      Create our RLE compressed zero-ed dummy line.                   */
/* -------------------------------------------------------------------- */
    GByte *pabyRLELine;
    GInt32 nRLEBytes = 0;
    int   nPixelsRemaining = nXSize;

    pabyRLELine = (GByte *) CPLMalloc((nXSize/127) * 2 + 4);
    
    while( nPixelsRemaining > 0 )
    {
        pabyRLELine[nRLEBytes] = (GByte) MIN(127,nPixelsRemaining);
        pabyRLELine[nRLEBytes+1] = 0;
        nPixelsRemaining -= pabyRLELine[nRLEBytes];

        nRLEBytes += 2;
    }

/* -------------------------------------------------------------------- */
/*      Prepare and write RLE offset/size tables with everything        */
/*      zeroed indicating dummy lines.                                  */
/* -------------------------------------------------------------------- */
    int i;
    int nTableLen = nYSize * nBands;
    GInt32 nDummyRLEOffset = 512 + 4 * nTableLen * 2;

    CPL_MSBPTR32( &nRLEBytes );
    CPL_MSBPTR32( &nDummyRLEOffset );
    
    for( i = 0; i < nTableLen; i++ )
        VSIFWriteL( &nDummyRLEOffset, 1, 4, fp );

    for( i = 0; i < nTableLen; i++ )
        VSIFWriteL( &nRLEBytes, 1, 4, fp );

/* -------------------------------------------------------------------- */
/*      write the dummy RLE blank line.                                 */
/* -------------------------------------------------------------------- */
    CPL_MSBPTR32( &nRLEBytes );
    if( (GInt32) VSIFWriteL( pabyRLELine, 1, nRLEBytes, fp ) != nRLEBytes )
    {
        CPLError( CE_Failure, CPLE_FileIO,  
                  "Failure writing SGI file '%s'.\n%s", 
                  pszFilename, 
                  VSIStrerror( errno ) );
        return NULL;
    }

    VSIFCloseL( fp );
    CPLFree( pabyRLELine );

    return (GDALDataset*) GDALOpen( pszFilename, GA_Update );
}

/************************************************************************/
/*                         GDALRegister_SGI()                          */
/************************************************************************/

void GDALRegister_SGI()

{
    GDALDriver*  poDriver;

    if(GDALGetDriverByName("SGI") == NULL)
    {
        poDriver = new GDALDriver();
        
        poDriver->SetDescription("SGI");
        poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, 
                                  "SGI Image File Format 1.0");
        poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "rgb");
        poDriver->SetMetadataItem(GDAL_DMD_MIMETYPE, "image/rgb");
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC,
                                   "frmt_various.html#SGI" );
        poDriver->pfnOpen = SGIDataset::Open;
        poDriver->pfnCreate = SGIDataset::Create;
        poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES, "Byte" );
        GetGDALDriverManager()->RegisterDriver(poDriver);
    }
}

