/******************************************************************************
 * $Id$
 *
 * Project:  FITS Driver
 * Purpose:  Implement FITS raster read/write support
 * Author:   Simon Perkins, s.perkins@lanl.gov
 *
 ******************************************************************************
 * Copyright (c) 2001, Simon Perkins
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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.1  2001/03/06 03:53:44  sperkins
 * Added FITS format support.
 *
 */



#include "gdal_priv.h"
#include <string.h>

static GDALDriver* poFITSDriver = NULL;

CPL_C_START
#include <fitsio.h>
void	GDALRegister_FITS(void);
CPL_C_END

/************************************************************************/
/* ==================================================================== */
/*				FITSDataset				*/
/* ==================================================================== */
/************************************************************************/

class FITSRasterBand;

class FITSDataset : public GDALDataset {

  friend FITSRasterBand;
  
  fitsfile* hFITS;
  GDALDataType gdalDataType;   // GDAL code for the image type
  int fitsDataType;   // FITS code for the image type
  bool fileExists;

  FITSDataset();     // Others shouldn't call this constructor explicitly

  CPLErr Init(fitsfile* hFITS_, bool fileExists_);

public:
  ~FITSDataset();

  static GDALDataset* Open(GDALOpenInfo* );
  static GDALDataset* Create(const char* pszFilename,
			     int nXSize, int nYSize, int nBands,
			     GDALDataType eType,
			     char** papszParmList);
  
};

/************************************************************************/
/* ==================================================================== */
/*                            FITSRasterBand                           */
/* ==================================================================== */
/************************************************************************/

class FITSRasterBand : public GDALRasterBand {

  friend	FITSDataset;
  
public:

  FITSRasterBand(FITSDataset*, int);
  ~FITSRasterBand();

  virtual CPLErr IReadBlock( int, int, void * );
  virtual CPLErr IWriteBlock( int, int, void * ); 
};


/************************************************************************/
/*                          FITSRasterBand()                           */
/************************************************************************/

FITSRasterBand::FITSRasterBand(FITSDataset *poDS, int nBand) {

  this->poDS = poDS;
  this->nBand = nBand;
  eDataType = poDS->gdalDataType;
  nBlockXSize = poDS->nRasterXSize;;
  nBlockYSize = 1;
}

/************************************************************************/
/*                          ~FITSRasterBand()                           */
/************************************************************************/

FITSRasterBand::~FITSRasterBand() {
  // Nothing here yet
}


/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr FITSRasterBand::IReadBlock(int nBlockXOff, int nBlockYOff,
                                  void* pImage ) {
 
  // A FITS block is one row (we assume BSQ formatted data)
  FITSDataset* dataset = (FITSDataset*) poDS;
  fitsfile* hFITS = dataset->hFITS;
  int status = 0;

  // Since a FITS block is a whole row, nBlockXOff must be zero
  // and the row number equals the y block offset. Also, nBlockYOff
  // cannot be greater than the number of rows
  CPLAssert(nBlockXOff == 0);
  CPLAssert(nBlockYOff < nRasterYSize);

  // If we haven't written anything to the file yet, then attempting
  // to read causes an error, so in this case, just return zeros.
  if (!dataset->fileExists) {
    memset(pImage, 0, nBlockXSize * nBlockYSize
	   * GDALGetDataTypeSize(eDataType) / 8);
    return CE_None;
  }

  // Calculate offsets and read in the data. Note that FITS array offsets
  // start at 1...
  long offset = (nBand - 1) * nRasterXSize * nRasterYSize +
    nBlockYOff * nRasterXSize + 1;
  long nElements = nRasterXSize;
  fits_read_img(hFITS, dataset->fitsDataType, offset, nElements,
		0, pImage, 0, &status);
  if (status) {
    CPLError(CE_Failure, CPLE_AppDefined,
	     "Couldn't read image data from FITS file (%d).", status);
    return CE_Failure;
  }
  
  return CE_None;
}

/************************************************************************/
/*                            IWriteBlock()                             */
/*                                                                      */
/************************************************************************/

CPLErr FITSRasterBand::IWriteBlock(int nBlockXOff, int nBlockYOff,
				   void* pImage) {

  FITSDataset* dataset = (FITSDataset*) poDS;
  fitsfile* hFITS = dataset->hFITS;
  int status = 0;

  // Since a FITS block is a whole row, nBlockXOff must be zero
  // and the row number equals the y block offset. Also, nBlockYOff
  // cannot be greater than the number of rows

  // Calculate offsets and read in the data. Note that FITS array offsets
  // start at 1...
  long offset = (nBand - 1) * nRasterXSize * nRasterYSize +
    nBlockYOff * nRasterXSize + 1;
  long nElements = nRasterXSize;
  fits_write_img(hFITS, dataset->fitsDataType, offset, nElements,
		 pImage, &status);
  if (status) {
    CPLError(CE_Failure, CPLE_AppDefined,
	     "Couldn't write image data to FITS file (%d).", status);
    return CE_Failure;
  }

  // When we write a block, assume that the file now exists
  dataset->fileExists = true;

  return CE_None;
}

/************************************************************************/
/* ==================================================================== */
/*                             FITSDataset                             */
/* ==================================================================== */
/************************************************************************/


/************************************************************************/
/*                            FITSDataset()                            */
/************************************************************************/

FITSDataset::FITSDataset() {
  // Nothing needed here
}

/************************************************************************/
/*                           ~FITSDataset()                            */
/************************************************************************/

FITSDataset::~FITSDataset() {
  // Make sure we flush the raster cache before we close the file!
  FlushCache();
  // Close the FITS handle - ignore the error status
  int status = 0;
  fits_close_file(hFITS, &status);
}

/************************************************************************/
/*                           FITSDataset::Init()                        */
/************************************************************************/

CPLErr FITSDataset::Init(fitsfile* hFITS_, bool fileExists_) {

  hFITS = hFITS_;
  fileExists = fileExists_;
  int status = 0;

  // Move to the primary HDU
  fits_movabs_hdu(hFITS, 1, 0, &status);
  if (status) {
    CPLError(CE_Failure, CPLE_AppDefined,
	     "Couldn't move to first HDU in FITS file %s (%d).\n", 
	     GetDescription(), status);
    return CE_Failure;
  }

  // Get the image info for this dataset (note that all bands in a FITS dataset
  // have the same type)
  int bitpix;
  int naxis;
  const int maxdim = 3;
  long naxes[maxdim];
  fits_get_img_param(hFITS, maxdim, &bitpix, &naxis, naxes, &status);
  if (status) {
    CPLError(CE_Failure, CPLE_AppDefined,
	     "Couldn't determine image parameters of FITS file %s (%d).", 
	     GetDescription(), status);
    return CE_Failure;
  }
  // Determine data type
  if (bitpix == BYTE_IMG) {
     gdalDataType = GDT_Byte;
     fitsDataType = TBYTE;
  }
  else if (bitpix == SHORT_IMG) {
    gdalDataType = GDT_Int16;
    fitsDataType = TSHORT;
  }
  else if (bitpix == LONG_IMG) {
    gdalDataType = GDT_Int32;
    fitsDataType = TLONG;
  }
  else if (bitpix == FLOAT_IMG) {
    gdalDataType = GDT_Float32;
    fitsDataType = TFLOAT;
  }
  else if (bitpix == DOUBLE_IMG) {
    gdalDataType = GDT_Float64;
    fitsDataType = TDOUBLE;
  }
  else {
    CPLError(CE_Failure, CPLE_AppDefined,
	     "FITS file %s has unknown data type: %d.", GetDescription(), 
	     bitpix);
    return CE_Failure;
  }

  // Determine image dimensions - we assume BSQ ordering
  if (naxis == 2) {
    nRasterXSize = naxes[0];
    nRasterYSize = naxes[1];
    nBands = 1;
  }
  else if (naxis == 3) {
    nRasterXSize = naxes[0];
    nRasterYSize = naxes[1];
    nBands = naxes[2];
  }
  else {
    CPLError(CE_Failure, CPLE_AppDefined,
	     "FITS file %s does not have 2 or 3 dimensions.", 
	     GetDescription());
    return CE_Failure;
  }

  // Create the bands
  for (int i = 0; i < nBands; ++i)
    SetBand(i+1, new FITSRasterBand(this, i+1));
  
  return CE_None;
}



/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset* FITSDataset::Open(GDALOpenInfo* poOpenInfo) {

  const char* fitsID = "SIMPLE  =                    T";  // Spaces important!
  size_t fitsIDLen = strlen(fitsID);  // Should be 30 chars long

  if ((size_t)poOpenInfo->nHeaderBytes < fitsIDLen)
    return NULL;
  if (memcmp(poOpenInfo->pabyHeader, fitsID, fitsIDLen))
    return NULL;
  
  // Get access mode and attempt to open the file
  int status = 0;
  fitsfile* hFITS = 0;
  if (poOpenInfo->eAccess == GA_ReadOnly)
    fits_open_file(&hFITS, poOpenInfo->pszFilename, READONLY, &status);
  else
    fits_open_file(&hFITS, poOpenInfo->pszFilename, READWRITE, &status);
  if (status) {
    CPLError(CE_Failure, CPLE_AppDefined,
	     "Error while opening FITS file %s (%d).\n", 
	     poOpenInfo->pszFilename, status);
    fits_close_file(hFITS, &status);
    return NULL;
  }

  // Create a FITSDataset object and initialize it from the FITS handle
  FITSDataset* dataset = new FITSDataset();
  dataset->SetDescription(poOpenInfo->pszFilename);
  if (dataset->Init(hFITS, true) != CE_None) {
    delete dataset;
    return NULL;
  }
  else
    return dataset;
}


/************************************************************************/
/*                               Create()                               */
/*                                                                      */
/*      Create a new FITS file.                                         */
/************************************************************************/

GDALDataset *FITSDataset::Create(const char* pszFilename,
				 int nXSize, int nYSize, 
				 int nBands, GDALDataType eType,
				 char** papszParmList) {

  FITSDataset* dataset;
  fitsfile* hFITS;
  int status = 0;
  
  // Currently we don't have any creation options for FITS

  // Create the file - to force creation, we prepend the name with '!'
  char* extFilename = new char[strlen(pszFilename) + 10];  // 10 for margin!
  sprintf(extFilename, "!%s", pszFilename);
  fits_create_file(&hFITS, extFilename, &status);
  delete extFilename;
  if (status) {
    CPLError(CE_Failure, CPLE_AppDefined,
	     "Couldn't create FITS file %s (%d).\n", pszFilename, status);
    return NULL;
  }

  // Determine FITS type of image
  int bitpix;
  if (eType == GDT_Byte)
    bitpix = BYTE_IMG;
  else if (eType == GDT_Int16)
    bitpix = SHORT_IMG;
  else if (eType == GDT_Int32)
    bitpix = LONG_IMG;
  else if (eType == GDT_Float32)
    bitpix = FLOAT_IMG;
  else if (eType == GDT_Float64)
    bitpix = DOUBLE_IMG;
  else {
    CPLError(CE_Failure, CPLE_AppDefined,
	     "GDALDataType (%d) unsupported for FITS", eType);
    return NULL;
  }

  // Now create an image of appropriate size and type
  long naxes[3] = {nXSize, nYSize, nBands};
  int naxis = (nBands == 1) ? 2 : 3;
  fits_create_img(hFITS, bitpix, naxis, naxes, &status);
  if (status) {
    CPLError(CE_Failure, CPLE_AppDefined,
	     "Couldn't create image within FITS file %s (%d).", 
	     pszFilename, status);
    return NULL;
  }

  dataset = new FITSDataset();
  dataset->SetDescription(pszFilename);
  // Init recalculates a lot of stuff we already know, but...
  if (dataset->Init(hFITS, false) != CE_None) { 
    delete dataset;
    return NULL;
  }
  else {
    return dataset;
  }
}


/************************************************************************/
/*                          GDALRegister_FITS()                        */
/************************************************************************/

void GDALRegister_FITS() {

  GDALDriver* poDriver;

  if(poFITSDriver == NULL) {
    poFITSDriver = poDriver = new GDALDriver();
        
    poDriver->pszShortName = "FITS";
    poDriver->pszLongName = "Flexible Image Transport System";
    poDriver->pszHelpTopic = "frmt_various.html#FITS";
        
    poDriver->pfnOpen = FITSDataset::Open;
    poDriver->pfnCreate = FITSDataset::Create;
    poDriver->pfnCreateCopy = NULL;

    GetGDALDriverManager()->RegisterDriver(poDriver);
  }
}

