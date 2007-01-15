/******************************************************************************
 * $Id$
 *
 * Project:  SGI Image Driver
 * Purpose:  Implement SGI Image Support based on Paul Bourke's SGI Image code.
 *           http://astronomy.swin.edu.au/~pbourke/dataformats/sgirgb/
 *           ftp://ftp.sgi.com/graphics/SGIIMAGESPEC
 * Authors:  Mike Mazzella (GDAL driver)
 *           Paul Bourke (original SGI format code)
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

    FILE* file;
    std::string fileName;
    unsigned char* tmp;
    unsigned char* tmpR;
    unsigned char* tmpG;
    unsigned char* tmpB;
    GUInt32 rleEnd;
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
              tmpR(NULL),
              tmpG(NULL),
              tmpB(NULL),
              rleEnd(0),
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
static void ImageGetRow(ImageRec* image, unsigned char* buf, int y, int z) 
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
            return;
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
                    CPLError(CE_Failure, CPLE_OpenFailed, "file read error: row (%d) of (%s)\n", y, image->fileName.empty() ? "none" : image->fileName.c_str());
                return;
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
            return;
        }
    }
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

    FILE*  fpImage;

    int	   bGeoTransformValid;
    double adfGeoTransform[6];

    char** papszMetadata;
    char** papszSubDatasets;

    ImageRec image;

public:
    SGIDataset();
    ~SGIDataset();

    virtual CPLErr GetGeoTransform(double*);
    static GDALDataset* Open(GDALOpenInfo*);
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
    if(nBlockXOff != 0)
    {
        printf("ERROR:  unhandled block value\n");
        exit(0);
    }

/* -------------------------------------------------------------------- */
/*      Load the desired data into the working buffer.              */
/* -------------------------------------------------------------------- */
    ImageGetRow(&(poGDS->image), (unsigned char*)pImage, nBlockYOff, nBand-1);

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
            return GCI_GrayIndex;
        else
            return GCI_BlueBand;
    }
    else if(poGDS->nBands == 4)
    {
        if(nBand == 1)
            return GCI_RedBand;
        else if(nBand == 2)
            return GCI_GrayIndex;
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
    bGeoTransformValid(FALSE),
    papszMetadata(NULL),
    papszSubDatasets(NULL)
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

    if(fpImage != NULL)
        VSIFCloseL(fpImage);

    if(papszMetadata != NULL) CSLDestroy(papszMetadata);

    if(image.tmp != NULL) CPLFree(image.tmp);
    if(image.tmpR != NULL) CPLFree(image.tmpR);
    if(image.tmpG != NULL) CPLFree(image.tmpG);
    if(image.tmpB != NULL) CPLFree(image.tmpB);
    if(image.rowSize != NULL) CPLFree(image.rowSize);
    if(image.rowStart != NULL) CPLFree(image.rowStart);
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

    if(tmpImage.bpc != 1)
    {
        CPLError(CE_Failure, CPLE_NotSupported, 
                 "The SGI driver only supports 1 byte channel values.\n");
        return NULL;
    }
  
    if(poOpenInfo->eAccess == GA_Update)
    {
        CPLError(CE_Failure, CPLE_NotSupported, 
                 "The SGI driver does not support update access to existing"
                 " datasets.\n");
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    SGIDataset* poDS;

    poDS = new SGIDataset();

/* -------------------------------------------------------------------- */
/*      Open the file using the large file api.                         */
/* -------------------------------------------------------------------- */
    poDS->fpImage = VSIFOpenL(poOpenInfo->pszFilename, "rb");
    if(poDS->fpImage == NULL)
    {
        CPLError(CE_Failure, CPLE_OpenFailed, "VSIFOpenL(%s) failed unexpectedly in sgidataset.cpp", poOpenInfo->pszFilename);
        return NULL;
    }
    // printf("----------- %s\n", poOpenInfo->pszFilename);

    poDS->eAccess = GA_ReadOnly;

/* -------------------------------------------------------------------- */
/*	Read pre-image data after ensuring the file is rewound.         */
/* -------------------------------------------------------------------- */
    VSIFSeekL(poDS->fpImage, 0, SEEK_SET);
    if(VSIFReadL((void*)(&(poDS->image)), 1, 12, poDS->fpImage) != 12)
    {
        CPLError(CE_Failure, CPLE_OpenFailed, "file read error while reading header in sgidataset.cpp");
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
    poDS->nBands = poDS->image.zsize;
    int numItems = (int(poDS->image.bpc) == 1) ? 256 : 65536;
    poDS->image.tmp = (unsigned char*)CPLMalloc(poDS->image.xsize*numItems);
    poDS->image.tmpR = (unsigned char*)CPLMalloc(poDS->image.xsize*numItems);
    poDS->image.tmpG = (unsigned char*)CPLMalloc(poDS->image.xsize*numItems);
    poDS->image.tmpB = (unsigned char*)CPLMalloc(poDS->image.xsize*numItems);
    if((poDS->image.tmp == NULL) || (poDS->image.tmpR == NULL) ||
       (poDS->image.tmpG == NULL) || (poDS->image.tmpB == NULL))
    {
      if(poDS->image.tmp != NULL) {CPLFree(poDS->image.tmp); poDS->image.tmp = NULL;}
      if(poDS->image.tmpR != NULL) {CPLFree(poDS->image.tmpR); poDS->image.tmpR = NULL;}
      if(poDS->image.tmpG != NULL) {CPLFree(poDS->image.tmpG); poDS->image.tmpG = NULL;}
      if(poDS->image.tmpB != NULL) {CPLFree(poDS->image.tmpB); poDS->image.tmpB = NULL;}
      CPLError(CE_Failure, CPLE_OpenFailed, "ran out of memory in sgidataset.cpp");
      return NULL;
    }
    memset(poDS->image.tmp, 0, poDS->image.xsize*numItems);
    memset(poDS->image.tmpR, 0, poDS->image.xsize*numItems);
    memset(poDS->image.tmpG, 0, poDS->image.xsize*numItems);
    memset(poDS->image.tmpB, 0, poDS->image.xsize*numItems);

    if(int(poDS->image.type) == 1)
    {
        int x = poDS->image.ysize * poDS->image.zsize * sizeof(GUInt32);
        poDS->image.rowStart = (GUInt32*)CPLMalloc(x);
        poDS->image.rowSize = (GInt32*)CPLMalloc(x);
        memset(poDS->image.rowStart, 0, x);
        memset(poDS->image.rowSize, 0, x);
        if(poDS->image.rowStart == NULL || poDS->image.rowSize == NULL)
        {
	  if(poDS->image.tmp != NULL) {CPLFree(poDS->image.tmp); poDS->image.tmp = NULL;}
	  if(poDS->image.tmpR != NULL) {CPLFree(poDS->image.tmpR); poDS->image.tmpR = NULL;}
	  if(poDS->image.tmpG != NULL) {CPLFree(poDS->image.tmpG); poDS->image.tmpG = NULL;}
	  if(poDS->image.tmpB != NULL) {CPLFree(poDS->image.tmpB); poDS->image.tmpB = NULL;}
	  CPLError(CE_Failure, CPLE_OpenFailed, "ran out of memory in sgidataset.cpp");
	  return NULL;
        }
        poDS->image.rleEnd = 512 + (2 * x);
        VSIFSeekL(poDS->fpImage, 512, SEEK_SET);
        if((int) VSIFReadL(poDS->image.rowStart, 1, x, poDS->image.file) != x)
        {
            CPLError(CE_Failure, CPLE_OpenFailed, "file read error while reading start positions in sgidataset.cpp");
            return NULL;
        }
        if((int) VSIFReadL(poDS->image.rowSize, 1, x, poDS->image.file) != x)
        {
            CPLError(CE_Failure, CPLE_OpenFailed, "file read error while reading row lengths in sgidataset.cpp");
            return NULL;
        }
        ConvertLong(poDS->image.rowStart, x/(int)sizeof(GUInt32));
        ConvertLong((GUInt32*)poDS->image.rowSize, x/(int)sizeof(GInt32));
    }
    else
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
/*      Check for overviews.                                            */
/* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename );

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    poDS->SetDescription(poOpenInfo->pszFilename);
    poDS->TryLoadXML();

    return poDS;
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
        poDriver->pfnOpen = SGIDataset::Open;
        GetGDALDriverManager()->RegisterDriver(poDriver);
    }
}

