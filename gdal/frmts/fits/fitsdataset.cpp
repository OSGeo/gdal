/******************************************************************************
 *
 * Project:  FITS Driver
 * Purpose:  Implement FITS raster read/write support
 * Author:   Simon Perkins, s.perkins@lanl.gov
 *
 ******************************************************************************
 * Copyright (c) 2001, Simon Perkins
 * Copyright (c) 2008-2018, Even Rouault <even dot rouault at mines-paris dot org>
 * Copyright (c) 2018, Chiara Marmo <chiara dot marmo at u-psud dot fr>
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

#include "cpl_string.h"
#include "gdal_frmts.h"
#include "gdal_pam.h"
#include "ogr_spatialref.h"

#include <string.h>
#include <fitsio.h>

#include <string>
#include <cstring>


CPL_CVSID("$Id$")

/************************************************************************/
/* ==================================================================== */
/*                              FITSDataset                             */
/* ==================================================================== */
/************************************************************************/

class FITSRasterBand;

class FITSDataset final : public GDALPamDataset {

  friend class FITSRasterBand;

  fitsfile* hFITS;

  GDALDataType gdalDataType;   // GDAL code for the image type
  int fitsDataType;   // FITS code for the image type

  bool isExistingFile;
  long highestOffsetWritten;  // How much of image has been written

  bool        bNoDataChanged;
  bool        bNoDataSet;
  double      dfNoDataValue;

  bool        bMetadataChanged;

  FITSDataset();     // Others should not call this constructor explicitly

  CPLErr Init(fitsfile* hFITS_, bool isExistingFile_);

  OGRSpatialReference oSRS{};

  double      adfGeoTransform[6];
  bool        bGeoTransformValid;

  void        WriteFITSInfo();
  bool        bFITSInfoChanged;

  void        LoadFITSInfo();

public:
  ~FITSDataset();

  static GDALDataset* Open( GDALOpenInfo* );
  static GDALDataset* Create( const char* pszFilename,
                              int nXSize, int nYSize, int nBands,
                              GDALDataType eType,
                              char** papszParmList );
  const OGRSpatialReference* GetSpatialRef() const override;
  CPLErr SetSpatialRef(const OGRSpatialReference* poSRS) override;
  virtual CPLErr GetGeoTransform( double * ) override;
  virtual CPLErr SetGeoTransform( double * ) override;

};

/************************************************************************/
/* ==================================================================== */
/*                            FITSRasterBand                           */
/* ==================================================================== */
/************************************************************************/

class FITSRasterBand : public GDALPamRasterBand {

  friend class  FITSDataset;

  bool               bHaveOffsetScale;
  double             dfOffset;
  double             dfScale;

 protected:
    FITSDataset       *poFDS;

    bool               bNoDataSet;
    double             dfNoDataValue;

 public:

  FITSRasterBand(FITSDataset*, int);
  virtual ~FITSRasterBand();

  virtual CPLErr IReadBlock( int, int, void * ) override;
  virtual CPLErr IWriteBlock( int, int, void * ) override;

  virtual double GetNoDataValue( int * ) override final;
  virtual CPLErr SetNoDataValue( double ) override final;
  virtual CPLErr DeleteNoDataValue() override final;

  virtual double GetOffset( int *pbSuccess = nullptr ) override final;
  virtual CPLErr SetOffset( double dfNewValue ) override final;
  virtual double GetScale( int *pbSuccess = nullptr ) override final;
  virtual CPLErr SetScale( double dfNewValue ) override final;

};

/************************************************************************/
/*                          FITSRasterBand()                           */
/************************************************************************/

FITSRasterBand::FITSRasterBand( FITSDataset *poDSIn, int nBandIn ) :
  bHaveOffsetScale(false),
  dfOffset(0.0),
  dfScale(1.0),
  poFDS(poDSIn),
  bNoDataSet(false),
  dfNoDataValue(-9999.0)
{
  poDS = poDSIn;
  nBand = nBandIn;
  eDataType = poDSIn->gdalDataType;
  nBlockXSize = poDSIn->nRasterXSize;
  nBlockYSize = 1;
}

/************************************************************************/
/*                          ~FITSRasterBand()                           */
/************************************************************************/

FITSRasterBand::~FITSRasterBand()
{
    FlushCache();
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr FITSRasterBand::IReadBlock(CPL_UNUSED int nBlockXOff, int nBlockYOff,
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

  // Calculate offsets and read in the data. Note that FITS array offsets
  // start at 1...
  LONGLONG offset = static_cast<LONGLONG>(nBand - 1) * nRasterXSize * nRasterYSize +
    static_cast<LONGLONG>(nBlockYOff) * nRasterXSize + 1;
  long nElements = nRasterXSize;

  // If we haven't written this block to the file yet, then attempting
  // to read causes an error, so in this case, just return zeros.
  if (!dataset->isExistingFile && offset > dataset->highestOffsetWritten) {
    memset(pImage, 0, nBlockXSize * nBlockYSize
           * GDALGetDataTypeSize(eDataType) / 8);
    return CE_None;
  }

  // Otherwise read in the image data
  fits_read_img(hFITS, dataset->fitsDataType, offset, nElements,
                nullptr, pImage, nullptr, &status);

  // Capture special case of non-zero status due to data range
  // overflow Standard GDAL policy is to silently truncate, which is
  // what CFITSIO does, in addition to returning NUM_OVERFLOW (412) as
  // the status.
  if (status == NUM_OVERFLOW)
    status = 0;

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

CPLErr FITSRasterBand::IWriteBlock( CPL_UNUSED int nBlockXOff, int nBlockYOff,
                                    void* pImage )
{
  FITSDataset* dataset = (FITSDataset*) poDS;
  fitsfile* hFITS = dataset->hFITS;
  int status = 0;

  // Since a FITS block is a whole row, nBlockXOff must be zero
  // and the row number equals the y block offset. Also, nBlockYOff
  // cannot be greater than the number of rows

  // Calculate offsets and read in the data. Note that FITS array offsets
  // start at 1 at the bottom left...
  LONGLONG offset = static_cast<LONGLONG>(nBand - 1) * nRasterXSize * nRasterYSize +
    static_cast<LONGLONG>(nBlockYOff) * nRasterXSize + 1;
  long nElements = nRasterXSize;
  fits_write_img(hFITS, dataset->fitsDataType, offset, nElements,
                 pImage, &status);

  // Capture special case of non-zero status due to data range
  // overflow Standard GDAL policy is to silently truncate, which is
  // what CFITSIO does, in addition to returning NUM_OVERFLOW (412) as
  // the status.
  if (status == NUM_OVERFLOW)
    status = 0;

  // Check for other errors
  if (status) {
    CPLError(CE_Failure, CPLE_AppDefined,
             "Error writing image data to FITS file (%d).", status);
    return CE_Failure;
  }

  // When we write a block, update the offset counter that we've written
  if (offset > dataset->highestOffsetWritten)
    dataset->highestOffsetWritten = offset;

  return CE_None;
}

/************************************************************************/
/* ==================================================================== */
/*                             FITSDataset                             */
/* ==================================================================== */
/************************************************************************/

// Some useful utility functions

// Simple static function to determine if FITS header keyword should
// be saved in meta data.
constexpr int ignorableHeaderCount = 18;
static const char* const ignorableFITSHeaders[ignorableHeaderCount] = {
  "SIMPLE", "BITPIX", "NAXIS", "NAXIS1", "NAXIS2", "NAXIS3", "END",
  "XTENSION", "PCOUNT", "GCOUNT", "EXTEND", "CONTINUE",
  "COMMENT", "", "LONGSTRN", "BZERO", "BSCALE", "BLANK"
};
static bool isIgnorableFITSHeader(const char* name) {
  for (int i = 0; i < ignorableHeaderCount; ++i) {
    if (strcmp(name, ignorableFITSHeaders[i]) == 0)
      return true;
  }
  return false;
}

/************************************************************************/
/*                            FITSDataset()                            */
/************************************************************************/

FITSDataset::FITSDataset():
    hFITS(nullptr),
    gdalDataType(GDT_Unknown),
    fitsDataType(0),
    isExistingFile(false),
    highestOffsetWritten(0),
    bNoDataChanged(false),
    bNoDataSet(false),
    dfNoDataValue(-9999.0),
    bMetadataChanged(false),
    bGeoTransformValid(false),
    bFITSInfoChanged(false)
{
    oSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
}

/************************************************************************/
/*                           ~FITSDataset()                            */
/************************************************************************/

FITSDataset::~FITSDataset() {

  int status;
  if( hFITS )
  {
    if(eAccess == GA_Update)
    {
      // Only do this if we've successfully opened the file and update
      // capability.  Write any meta data to the file that's compatible with
      // FITS.
      status = 0;
      fits_movabs_hdu(hFITS, 1, nullptr, &status);
      fits_write_key_longwarn(hFITS, &status);
      if (status) {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "Couldn't move to first HDU in FITS file %s (%d).\n",
                 GetDescription(), status);
      }
      char** metaData = GetMetadata();
      int count = CSLCount(metaData);
      for (int i = 0; i < count; ++i) {
        const char* field = CSLGetField(metaData, i);
        if (strlen(field) == 0)
            continue;
        else {
            char* key = nullptr;
            const char* value = CPLParseNameValue(field, &key);
            // FITS keys must be less than 8 chars
            if (key != nullptr && strlen(key) <= 8 && !isIgnorableFITSHeader(key))
            {
                // Although FITS provides support for different value
                // types, the GDAL Metadata mechanism works only with
                // string values. Prior to about 2003-05-02, this driver
                // would attempt to guess the value type from the metadata
                // value string amd then would use the appropriate
                // type-specific FITS keyword update routine. This was
                // found to be troublesome (e.g. a numeric version string
                // with leading zeros would be interpreted as a number
                // and might get those leading zeros stripped), and so now
                // the driver writes every value as a string. In practice
                // this is not a problem since most FITS reading routines
                // will convert from strings to numbers automatically, but
                // if you want finer control, use the underlying FITS
                // handle. Note: to avoid a compiler warning we copy the
                // const value string to a non const one.
                char* valueCpy = CPLStrdup(value);
                fits_update_key_longstr(hFITS, key, valueCpy, nullptr, &status);
                CPLFree(valueCpy);

                // Check for errors.
                if (status)
                {
                    // Throw a warning with CFITSIO error status, then ignore status 
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "Couldn't update key %s in FITS file %s (%d).",
                             key, GetDescription(), status);
                    status = 0;
                    return;
                }
            }
            // Must free up key
            CPLFree(key);
        }
      }

      // Writing nodata value
      if (gdalDataType != GDT_Float32 && gdalDataType != GDT_Float64) {
        fits_update_key( hFITS, TDOUBLE, "BLANK", &dfNoDataValue, nullptr, &status);
        if (status)
        {
            // Throw a warning with CFITSIO error status, then ignore status 
            CPLError(CE_Warning, CPLE_AppDefined,
                    "Couldn't update key BLANK in FITS file %s (%d).",
                    GetDescription(), status);
            status = 0;
            return;
        }
      }

      // Writing Scale and offset if defined
      int pbSuccess;
      GDALRasterBand* poSrcBand = GDALPamDataset::GetRasterBand(1);
      double dfScale = poSrcBand->GetScale(&pbSuccess);
      double dfOffset = poSrcBand->GetOffset(&pbSuccess);
      if (bMetadataChanged) {
        fits_update_key( hFITS, TDOUBLE, "BSCALE", &dfScale, nullptr, &status);
        if (status)
        {
            // Throw a warning with CFITSIO error status, then ignore status 
            CPLError(CE_Warning, CPLE_AppDefined,
                    "Couldn't update key BSCALE in FITS file %s (%d).",
                    GetDescription(), status);
            status = 0;
            return;
        }
        fits_update_key( hFITS, TDOUBLE, "BZERO", &dfOffset, nullptr, &status);
        if (status)
        {
            // Throw a warning with CFITSIO error status, then ignore status 
            CPLError(CE_Warning, CPLE_AppDefined,
                    "Couldn't update key BZERO in FITS file %s (%d).",
                    GetDescription(), status);
            status = 0;
            return;
        }
      }

      // Copy georeferencing info to PAM if the profile is not FITS
      GDALPamDataset::SetSpatialRef(GDALPamDataset::GetSpatialRef());

      // Write geographic info
      if (bFITSInfoChanged) {
        WriteFITSInfo();
      }

      // Make sure we flush the raster cache before we close the file!
      FlushCache();
    }

    // Close the FITS handle
    fits_close_file(hFITS, &status);
  }
}

/************************************************************************/
/*                           Init()                                     */
/************************************************************************/

CPLErr FITSDataset::Init(fitsfile* hFITS_, bool isExistingFile_) {

  hFITS = hFITS_;
  isExistingFile = isExistingFile_;
  highestOffsetWritten = 0;
  int status = 0;

  // Move to the primary HDU
  fits_movabs_hdu(hFITS, 1, nullptr, &status);
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
  double offset;
  
  fits_get_img_param(hFITS, maxdim, &bitpix, &naxis, naxes, &status);
  if (status) {
    CPLError(CE_Failure, CPLE_AppDefined,
             "Couldn't determine image parameters of FITS file %s (%d).",
             GetDescription(), status);
    return CE_Failure;
  }
  fits_read_key(hFITS, TDOUBLE, "BZERO", &offset, nullptr, &status);
  if( status )
  {
    // BZERO is not mandatory offset defaulted to 0 if BZERO is missing
    status = 0;
    offset = 0.;
  }

  fits_read_key(hFITS, TDOUBLE, "BLANK", &dfNoDataValue, nullptr, &status);
  bNoDataSet = !status;
  status = 0;

  // Determine data type and nodata value if BLANK keyword is absent
  if (bitpix == BYTE_IMG) {
     gdalDataType = GDT_Byte;
     fitsDataType = TBYTE;
  }
  else if (bitpix == SHORT_IMG) {
    if (offset == 32768.)
    {
      gdalDataType = GDT_UInt16;
      fitsDataType = TUSHORT;
    }
    else {
      gdalDataType = GDT_Int16;
      fitsDataType = TSHORT;
    }
  }
  else if (bitpix == LONG_IMG) {
    if (offset == 2147483648.)
    {
      gdalDataType = GDT_UInt32;
      fitsDataType = TUINT;
    }
    else {
      gdalDataType = GDT_Int32;
      fitsDataType = TINT;
    }
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
    nRasterXSize = static_cast<int>(naxes[0]);
    nRasterYSize = static_cast<int>(naxes[1]);
    nBands = 1;
  }
  else if (naxis == 3) {
    nRasterXSize = static_cast<int>(naxes[0]);
    nRasterYSize = static_cast<int>(naxes[1]);
    nBands = static_cast<int>(naxes[2]);
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

  // Read header information from file and use it to set metadata
  // This process understands the CONTINUE standard for long strings.
  // We don't bother to capture header names that duplicate information
  // already captured elsewhere (e.g. image dimensions and type)
  int keyNum;
  char key[100];
  char value[100];

  int nKeys = 0;
  int nMoreKeys = 0;
  fits_get_hdrspace(hFITS, &nKeys, &nMoreKeys, &status);
  for(keyNum = 1; keyNum <= nKeys; keyNum++)
  {
    fits_read_keyn(hFITS, keyNum, key, value, nullptr, &status);
    if (status) {
      CPLError(CE_Failure, CPLE_AppDefined,
               "Error while reading key %d from FITS file %s (%d)",
               keyNum, GetDescription(), status);
      return CE_Failure;
    }
    if (strcmp(key, "END") == 0) {
        // We should not get here in principle since the END
        // keyword shouldn't be counted in nKeys, but who knows.
        break;
    }
    else if (isIgnorableFITSHeader(key)) {
      // Ignore it
    }
    else {   // Going to store something, but check for long strings etc
      // Strip off leading and trailing quote if present
      char* newValue = value;
      if (value[0] == '\'' && value[strlen(value) - 1] == '\'')
      {
          newValue = value + 1;
          value[strlen(value) - 1] = '\0';
      }
      // Check for long string
      if (strrchr(newValue, '&') == newValue + strlen(newValue) - 1)
      {
          // Value string ends in "&", so use long string conventions
          char* longString = nullptr;
          fits_read_key_longstr(hFITS, key, &longString, nullptr, &status);
          // Note that read_key_longstr already strips quotes
          if( status )
          {
              CPLError(CE_Failure, CPLE_AppDefined,
                       "Error while reading long string for key %s from "
                       "FITS file %s (%d)", key, GetDescription(), status);
              return CE_Failure;
          }
          SetMetadataItem(key, longString);
          free(longString);
      }
      else
      {  // Normal keyword
          SetMetadataItem(key, newValue);
      }
    }
  }

  return CE_None;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset* FITSDataset::Open(GDALOpenInfo* poOpenInfo) {

  const char* fitsID = "SIMPLE  =                    T";  // Spaces important!
  size_t fitsIDLen = strlen(fitsID);  // Should be 30 chars long

  if ((size_t)poOpenInfo->nHeaderBytes < fitsIDLen)
    return nullptr;
  if (memcmp(poOpenInfo->pabyHeader, fitsID, fitsIDLen))
    return nullptr;

  // Get access mode and attempt to open the file
  int status = 0;
  fitsfile* hFITS = nullptr;
  if (poOpenInfo->eAccess == GA_ReadOnly)
    fits_open_file(&hFITS, poOpenInfo->pszFilename, READONLY, &status);
  else
    fits_open_file(&hFITS, poOpenInfo->pszFilename, READWRITE, &status);
  if (status) {
    CPLError(CE_Failure, CPLE_AppDefined,
             "Error while opening FITS file %s (%d).\n",
             poOpenInfo->pszFilename, status);
    fits_close_file(hFITS, &status);
    return nullptr;
  }

  // Create a FITSDataset object and initialize it from the FITS handle
  FITSDataset* dataset = new FITSDataset();
  dataset->eAccess = poOpenInfo->eAccess;

  dataset->bMetadataChanged = false;
  dataset->bNoDataChanged = false;

  // Set up the description and initialize the dataset
  dataset->SetDescription(poOpenInfo->pszFilename);
  if (dataset->Init(hFITS, true) != CE_None) {
    delete dataset;
    return nullptr;
  }
  else
  {
/* -------------------------------------------------------------------- */
/*      Initialize any information.                                 */
/* -------------------------------------------------------------------- */
      dataset->SetDescription( poOpenInfo->pszFilename );
      dataset->LoadFITSInfo();
      dataset->TryLoadXML();

/* -------------------------------------------------------------------- */
/*      Check for external overviews.                                   */
/* -------------------------------------------------------------------- */
      dataset->oOvManager.Initialize( dataset, poOpenInfo->pszFilename, poOpenInfo->GetSiblingFiles() );

      return dataset;
  }
}

/************************************************************************/
/*                               Create()                               */
/*                                                                      */
/*      Create a new FITS file.                                         */
/************************************************************************/

GDALDataset *FITSDataset::Create( const char* pszFilename,
                                  int nXSize, int nYSize,
                                  int nBands, GDALDataType eType,
                                  CPL_UNUSED char** papszParmList )
{
  int status = 0;

  // No creation options are defined. The BSCALE/BZERO options were
  // removed on 2002-07-02 by Simon Perkins because they introduced
  // excessive complications and didn't really fit into the GDAL
  // paradigm.
  // 2018 - BZERO BSCALE keywords are now set using SetScale() and
  // SetOffset() functions 

  if( nXSize < 1 || nYSize < 1 || nBands < 1 )  {
        CPLError(
            CE_Failure, CPLE_AppDefined,
            "Attempt to create %dx%dx%d raster FITS file, but width, height and bands"
            " must be positive.",
            nXSize, nYSize, nBands );

        return nullptr;
  }

  // Determine FITS type of image
  int bitpix;
  if (eType == GDT_Byte) {
    bitpix = BYTE_IMG;
  } else if (eType == GDT_UInt16) {
    bitpix = USHORT_IMG;
  } else if (eType == GDT_Int16) {
    bitpix = SHORT_IMG;
  } else if (eType == GDT_UInt32) {
    bitpix = ULONG_IMG;
  } else if (eType == GDT_Int32) {
    bitpix = LONG_IMG;
  } else if (eType == GDT_Float32)
    bitpix = FLOAT_IMG;
  else if (eType == GDT_Float64)
    bitpix = DOUBLE_IMG;
  else {
    CPLError(CE_Failure, CPLE_AppDefined,
             "GDALDataType (%d) unsupported for FITS", eType);
    return nullptr;
  }

  // Create the file - to force creation, we prepend the name with '!'
  CPLString extFilename("!");
  extFilename += pszFilename;
  fitsfile* hFITS = nullptr;
  fits_create_file(&hFITS, extFilename, &status);
  if (status) {
    CPLError(CE_Failure, CPLE_AppDefined,
             "Couldn't create FITS file %s (%d).\n", pszFilename, status);
    return nullptr;
  }

  // Now create an image of appropriate size and type
  long naxes[3] = {nXSize, nYSize, nBands};
  int naxis = (nBands == 1) ? 2 : 3;
  fits_create_img(hFITS, bitpix, naxis, naxes, &status);

  // Check the status
  if (status) {
    CPLError(CE_Failure, CPLE_AppDefined,
             "Couldn't create image within FITS file %s (%d).",
             pszFilename, status);
    fits_close_file(hFITS, &status);
    return nullptr;
  }

  FITSDataset* dataset = new FITSDataset();
  dataset->nRasterXSize = nXSize;
  dataset->nRasterYSize = nYSize;
  dataset->eAccess = GA_Update;
  dataset->SetDescription(pszFilename);

  // Init recalculates a lot of stuff we already know, but...
  if (dataset->Init(hFITS, false) != CE_None) {
    delete dataset;
    return nullptr;
  }
  else {
    return dataset;
  }
}

/************************************************************************/
/*                          WriteFITSInfo()                          */
/************************************************************************/

void FITSDataset::WriteFITSInfo()

{
  int status = 0;

  const double PI = std::atan(1.0)*4;
  const double DEG2RAD = PI / 180.;

  double falseEast = 0;
  double falseNorth = 0;

  double cfactor, mres, mapres, UpperLeftCornerX, UpperLeftCornerY;
  double crpix1, crpix2;

/* -------------------------------------------------------------------- */
/*      Write out projection definition.                                */
/* -------------------------------------------------------------------- */
    const bool bHasProjection = !oSRS.IsEmpty();
    if( bHasProjection )
    {

        // Set according to coordinate system (thanks to Trent Hare - USGS)

        std::string object, ctype1, ctype2;
        
        const char* target = oSRS.GetAttrValue("DATUM",0);
        if ( target ) {
            if ( strstr(target, "Moon") ) {
              object.assign("Moon");
              ctype1.assign("SE");
              ctype2.assign("SE");              
            } else if ( strstr(target, "Mercury") ) {
              object.assign("Mercury");
              ctype1.assign("ME");
              ctype2.assign("ME");
            } else if ( strstr(target, "Venus") ) {
              object.assign("Venus");
              ctype1.assign("VE");
              ctype2.assign("VE");
            } else if ( strstr(target, "Mars") ) {
              object.assign("Mars");
              ctype1.assign("MA");
              ctype2.assign("MA");
            } else if ( strstr(target, "Jupiter") ) {
              object.assign("Jupiter");
              ctype1.assign("JU");
              ctype2.assign("JU");
            } else if ( strstr(target, "Saturn") ) {
              object.assign("Saturn");
              ctype1.assign("SA");
              ctype2.assign("SA");
            } else if ( strstr(target, "Uranus") ) {
              object.assign("Uranus");
              ctype1.assign("UR");
              ctype2.assign("UR");
            } else if ( strstr(target, "Neptune") ) {
              object.assign("Neptune");
              ctype1.assign("NE");
              ctype2.assign("NE");
            } else {
              object.assign("Earth");
              ctype1.assign("EA");
              ctype2.assign("EA");
            }

            char * cstrobj = new char [object.length()+1];
            std::strcpy (cstrobj, object.c_str());

            fits_update_key( hFITS, TSTRING, "OBJECT", cstrobj, nullptr, &status);
        }

        double aradius = oSRS.GetSemiMajor();
        double bradius = aradius;
        double cradius = oSRS.GetSemiMinor();

        cfactor = aradius * DEG2RAD;

        fits_update_key( hFITS, TDOUBLE, "A_RADIUS", &aradius, nullptr, &status);
        if (status)
        {
            // Throw a warning with CFITSIO error status, then ignore status 
            CPLError(CE_Warning, CPLE_AppDefined,
                    "Couldn't update key A_RADIUS in FITS file %s (%d).",
                    GetDescription(), status);
            status = 0;
            return;
        }
        fits_update_key( hFITS, TDOUBLE, "B_RADIUS", &bradius, nullptr, &status);
        if (status)
        {
            // Throw a warning with CFITSIO error status, then ignore status 
            CPLError(CE_Warning, CPLE_AppDefined,
                    "Couldn't update key B_RADIUS in FITS file %s (%d).",
                    GetDescription(), status);
            status = 0;
            return;
        }
        fits_update_key( hFITS, TDOUBLE, "C_RADIUS", &cradius, nullptr, &status);
        if (status)
        {
            // Throw a warning with CFITSIO error status, then ignore status 
            CPLError(CE_Warning, CPLE_AppDefined,
                    "Couldn't update key C_RADIUS in FITS file %s (%d).",
                    GetDescription(), status);
            status = 0;
            return;
        }

        const char* unit = oSRS.GetAttrValue("UNIT",0);

        ctype1.append("LN-");
        ctype2.append("LT-"); 

        // strcat(ctype1a, "PX-");
        // strcat(ctype2a, "PY-"); 

        std::string fitsproj;
        const char* projection = oSRS.GetAttrValue("PROJECTION",0);
        double centlon = 0, centlat = 0;

        if (projection) {
            if ( strstr(projection, "Sinusoidal") ) {
              fitsproj.assign("SFL");
              centlon = oSRS.GetProjParm("central_meridian", 0, nullptr);
            } else if ( strstr(projection, "Equirectangular") ) {
              fitsproj.assign("CAR");
              centlat = oSRS.GetProjParm("standard_parallel_1", 0, nullptr);
              centlon = oSRS.GetProjParm("central_meridian", 0, nullptr);
            } else if ( strstr(projection, "Orthographic") ) {
              fitsproj.assign("SIN");
              centlat = oSRS.GetProjParm("standard_parallel_1", 0, nullptr);
              centlon = oSRS.GetProjParm("central_meridian", 0, nullptr);
            } else if ( strstr(projection, "Mercator_1SP") || strstr(projection, "Mercator") ) {
              fitsproj.assign("MER");
              centlat = oSRS.GetProjParm("standard_parallel_1", 0, nullptr);
              centlon = oSRS.GetProjParm("central_meridian", 0, nullptr);
            } else if ( strstr(projection, "Polar_Stereographic") || strstr(projection, "Stereographic_South_Pole") || strstr(projection, "Stereographic_North_Pole") ) {
              fitsproj.assign("STG");
              centlat = oSRS.GetProjParm("latitude_of_origin", 0, nullptr);
              centlon = oSRS.GetProjParm("central_meridian", 0, nullptr);
            }

/*
                #Transverse Mercator is supported in FITS via specific MER parameters.
                # need some more testing...
                #if EQUAL(mapProjection,"Transverse_Mercator"):
                #    mapProjection = "MER"
                #    centLat = hSRS.GetProjParm('standard_parallel_1')
                #    centLon = hSRS.GetProjParm('central_meridian')
                #    TMscale = hSRS.GetProjParm('scale_factor')
                #    #Need to research when TM actually applies false values
                #    #but planetary is almost always 0.0
                #    falseEast =  hSRS.GetProjParm('false_easting')
                #    falseNorth =  hSRS.GetProjParm('false_northing')
*/ 

            ctype1.append(fitsproj);
            ctype2.append(fitsproj);

            char * cstr1 = new char [ctype1.length()+1];
            std::strcpy (cstr1, ctype1.c_str());
            fits_update_key( hFITS, TSTRING, "CTYPE1", cstr1, nullptr, &status);
            if (status)
            {
                // Throw a warning with CFITSIO error status, then ignore status 
                CPLError(CE_Warning, CPLE_AppDefined,
                        "Couldn't update key CTYPE1 in FITS file %s (%d).",
                        GetDescription(), status);
                status = 0;
                return;
            }

            char * cstr2 = new char [ctype2.length()+1];
            std::strcpy (cstr2, ctype2.c_str());
            fits_update_key( hFITS, TSTRING, "CTYPE2", cstr2, nullptr, &status);
            if (status)
            {
                // Throw a warning with CFITSIO error status, then ignore status 
                CPLError(CE_Warning, CPLE_AppDefined,
                        "Couldn't update key CTYPE2 in FITS file %s (%d).",
                        GetDescription(), status);
                status = 0;
                return;
            }
        }


        UpperLeftCornerX = adfGeoTransform[0] - falseEast;
        UpperLeftCornerY = adfGeoTransform[3] - falseNorth;

        if ( centlon > 180. ) {
          centlon = centlon - 180.;
        }
        if ( strstr(unit, "metre") ) {
          // convert degrees/pixel to m/pixel 
          mapres = 1. / adfGeoTransform[1] ; // mapres is pixel/meters
          mres = adfGeoTransform[1] / cfactor ; // mres is deg/pixel
          crpix1 = - (UpperLeftCornerX * mapres) + centlon / mres + 0.5;
          // assuming that center latitude is also the origin of the coordinate
          // system: this is not always true.
          // More generic implementation coming soon
          crpix2 = (UpperLeftCornerY * mapres) + 0.5; // - (centlat / mres);
        } else if ( strstr(unit, "degree") ) {
          //convert m/pixel to pixel/degree
          mapres = 1. / adfGeoTransform[1] / cfactor; // mapres is pixel/deg
          mres = adfGeoTransform[1] ; // mres is meters/pixel
          crpix1 = - (UpperLeftCornerX * mres) + centlon / mapres + 0.5;
          // assuming that center latitude is also the origin of the coordinate
          // system: this is not always true.
          // More generic implementation coming soon
          crpix2 = (UpperLeftCornerY * mres) + 0.5; // - (centlat / mapres);
        }

        /// Write WCS CRPIXia CRVALia CTYPEia here

        fits_update_key( hFITS, TDOUBLE, "CRVAL1", &centlon, nullptr, &status);
        if (status)
        {
            // Throw a warning with CFITSIO error status, then ignore status 
            CPLError(CE_Warning, CPLE_AppDefined,
                    "Couldn't update key CRVAL1 in FITS file %s (%d).",
                    GetDescription(), status);
            status = 0;
            return;
        }
        fits_update_key( hFITS, TDOUBLE, "CRVAL2", &centlat, nullptr, &status);
        if (status)
        {
            // Throw a warning with CFITSIO error status, then ignore status 
            CPLError(CE_Warning, CPLE_AppDefined,
                    "Couldn't update key CRVAL2 in FITS file %s (%d).",
                    GetDescription(), status);
            status = 0;
            return;
        }
        fits_update_key( hFITS, TDOUBLE, "CRPIX1", &crpix1, nullptr, &status);
        if (status)
        {
            // Throw a warning with CFITSIO error status, then ignore status 
            CPLError(CE_Warning, CPLE_AppDefined,
                    "Couldn't update key CRPIX1 in FITS file %s (%d).",
                    GetDescription(), status);
            status = 0;
            return;
        }
        fits_update_key( hFITS, TDOUBLE, "CRPIX2", &crpix2, nullptr, &status);
        if (status)
        {
            // Throw a warning with CFITSIO error status, then ignore status 
            CPLError(CE_Warning, CPLE_AppDefined,
                    "Couldn't update key CRPIX2 in FITS file %s (%d).",
                    GetDescription(), status);
            status = 0;
            return;
        }

/* -------------------------------------------------------------------- */
/*      Write geotransform if valid.                                    */
/* -------------------------------------------------------------------- */
        if( bGeoTransformValid )
        {

/* -------------------------------------------------------------------- */
/*      Write the transform.                                            */
/* -------------------------------------------------------------------- */

            /// Write WCS CDELTia and PCi_ja here

            double cd[4];
            cd[0] = adfGeoTransform[1] / cfactor;
            cd[1] = adfGeoTransform[2] / cfactor;
            cd[2] = adfGeoTransform[4] / cfactor;
            cd[3] = adfGeoTransform[5] / cfactor;

            double pc[4];
            pc[0] = 1.;
            pc[1] = cd[1] / cd[0];
            pc[2] = cd[2] / cd[3];
            pc[3] = - 1.;

            fits_update_key( hFITS, TDOUBLE, "CDELT1", &cd[0], nullptr, &status);
            if (status)
            {
                // Throw a warning with CFITSIO error status, then ignore status 
                CPLError(CE_Warning, CPLE_AppDefined,
                        "Couldn't update key CDELT1 in FITS file %s (%d).",
                        GetDescription(), status);
                status = 0;
                return;
            }

            fits_update_key( hFITS, TDOUBLE, "CDELT2", &cd[3], nullptr, &status);
            if (status)
            {
                // Throw a warning with CFITSIO error status, then ignore status 
                CPLError(CE_Warning, CPLE_AppDefined,
                        "Couldn't update key CDELT2 in FITS file %s (%d).",
                        GetDescription(), status);
                status = 0;
                return;
            }

            fits_update_key( hFITS, TDOUBLE, "PC1_1", &pc[0], nullptr, &status);
            if (status)
            {
                // Throw a warning with CFITSIO error status, then ignore status 
                CPLError(CE_Warning, CPLE_AppDefined,
                        "Couldn't update key PC1_1 in FITS file %s (%d).",
                        GetDescription(), status);
                status = 0;
                return;
            }

            fits_update_key( hFITS, TDOUBLE, "PC1_2", &pc[1], nullptr, &status);
            if (status)
            {
                // Throw a warning with CFITSIO error status, then ignore status 
                CPLError(CE_Warning, CPLE_AppDefined,
                        "Couldn't update key PC1_2 in FITS file %s (%d).",
                        GetDescription(), status);
                status = 0;
                return;
            }

            fits_update_key( hFITS, TDOUBLE, "PC2_1", &pc[2], nullptr, &status);
            if (status)
            {
                // Throw a warning with CFITSIO error status, then ignore status 
                CPLError(CE_Warning, CPLE_AppDefined,
                        "Couldn't update key PC2_1 in FITS file %s (%d).",
                        GetDescription(), status);
                status = 0;
                return;
            }

            fits_update_key( hFITS, TDOUBLE, "PC2_2", &pc[3], nullptr, &status);
            if (status)
            {
                // Throw a warning with CFITSIO error status, then ignore status 
                CPLError(CE_Warning, CPLE_AppDefined,
                        "Couldn't update key PC2_2 in FITS file %s (%d).",
                        GetDescription(), status);
                status = 0;
                return;
            }
        }
    }
}


/************************************************************************/
/*                          GetSpatialRef()                             */
/************************************************************************/

const OGRSpatialReference* FITSDataset::GetSpatialRef() const

{
    return oSRS.IsEmpty() ? nullptr : &oSRS;
}

/************************************************************************/
/*                           SetSpatialRef()                            */
/************************************************************************/

CPLErr FITSDataset::SetSpatialRef( const OGRSpatialReference * poSRS )

{
    if( poSRS == nullptr || poSRS->IsEmpty() )
    {
        oSRS.Clear();
    }
    else
    {
        oSRS = *poSRS;
        oSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    }

    bFITSInfoChanged = true;

    return CE_None;
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr FITSDataset::GetGeoTransform( double * padfTransform )

{
    memcpy( padfTransform, adfGeoTransform, sizeof(double) * 6 );

    if( !bGeoTransformValid )
        return CE_Failure;

    return CE_None;
}

/************************************************************************/
/*                          SetGeoTransform()                           */
/************************************************************************/

CPLErr FITSDataset::SetGeoTransform( double * padfTransform )

{
    bGeoTransformValid = false;

    memcpy( adfGeoTransform, padfTransform, sizeof(double)*6 );
    bGeoTransformValid = true;

    return CE_None;
}

/************************************************************************/
/*                             GetOffset()                              */
/************************************************************************/

double FITSRasterBand::GetOffset( int *pbSuccess )

{
    if( pbSuccess )
        *pbSuccess = bHaveOffsetScale;
    return dfOffset;
}

/************************************************************************/
/*                             SetOffset()                              */
/************************************************************************/

CPLErr FITSRasterBand::SetOffset( double dfNewValue )

{
    if( !bHaveOffsetScale || dfNewValue != dfOffset )
        poFDS->bMetadataChanged = true;

    bHaveOffsetScale = true;
    dfOffset = dfNewValue;
    return CE_None;
}

/************************************************************************/
/*                              GetScale()                              */
/************************************************************************/

double FITSRasterBand::GetScale( int *pbSuccess )

{
    if( pbSuccess )
        *pbSuccess = bHaveOffsetScale;
    return dfScale;
}

/************************************************************************/
/*                              SetScale()                              */
/************************************************************************/

CPLErr FITSRasterBand::SetScale( double dfNewValue )

{
    if( !bHaveOffsetScale || dfNewValue != dfScale )
        poFDS->bMetadataChanged = true;

    bHaveOffsetScale = true;
    dfScale = dfNewValue;
    return CE_None;
}

/************************************************************************/
/*                           GetNoDataValue()                           */
/************************************************************************/

double FITSRasterBand::GetNoDataValue( int * pbSuccess )

{
    if( bNoDataSet )
    {
        if( pbSuccess )
            *pbSuccess = TRUE;

        return dfNoDataValue;
    }

    if( poFDS->bNoDataSet )
    {
        if( pbSuccess )
            *pbSuccess = TRUE;

        return poFDS->dfNoDataValue;
    }

    return GDALPamRasterBand::GetNoDataValue( pbSuccess );
}

/************************************************************************/
/*                           SetNoDataValue()                           */
/************************************************************************/

CPLErr FITSRasterBand::SetNoDataValue( double dfNoData )

{
    if( poFDS->bNoDataSet && poFDS->dfNoDataValue == dfNoData )
    {
        bNoDataSet = true;
        dfNoDataValue = dfNoData;
        return CE_None;
    }

    poFDS->bNoDataSet = true;
    poFDS->dfNoDataValue = dfNoData;

    poFDS->bNoDataChanged = true;

    bNoDataSet = true;
    dfNoDataValue = dfNoData;
    return CE_None;
}

/************************************************************************/
/*                        DeleteNoDataValue()                           */
/************************************************************************/

CPLErr FITSRasterBand::DeleteNoDataValue()

{
    if( !poFDS->bNoDataSet )
        return CE_None;

    poFDS->bNoDataSet = false;
    poFDS->dfNoDataValue = -9999.0;

    poFDS->bNoDataChanged = true;

    bNoDataSet = false;
    dfNoDataValue = -9999.0;
    return CE_None;
}

/************************************************************************/
/*                     LoadFITSInfo()                                   */
/************************************************************************/

void FITSDataset::LoadFITSInfo()

{
    int status = 0;
    int bitpix;
    double dfScale, dfOffset;
    double crpix1, crpix2, crval1, crval2, cdelt1, cdelt2, pc[4], cd[4];
    double aRadius, cRadius, invFlattening = 0.0;
    double falseEast = 0.0, falseNorth = 0.0, scale = 1.0;
    char target[81], ctype[81];
    std::string GeogName, DatumName, projName;

    const double PI = std::atan(1.0)*4;
    const double DEG2RAD = PI / 180.;

/* -------------------------------------------------------------------- */
/*      Get the transform from the FITS file.                           */
/* -------------------------------------------------------------------- */

    fits_read_key(hFITS, TSTRING, "OBJECT", target, nullptr, &status);
    if( status )
    {
        strncpy(target, "Undefined", 10);
        CPLError(CE_Failure, CPLE_AppDefined,
             "OBJECT keyword is missing");
        status = 0;
    }

    GeogName.assign("GCS_");
    GeogName.append(target);
    DatumName.assign("D_");
    DatumName.append(target);

    fits_read_key(hFITS, TDOUBLE, "A_RADIUS", &aRadius, nullptr, &status);
    if( status )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
            "No Radii keyword available, metadata will not contain DATUM information.");
        status = 0;
    } else {
        fits_read_key(hFITS, TDOUBLE, "C_RADIUS", &cRadius, nullptr, &status);
        if( status )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                "No polar radius keyword available, setting C_RADIUS = A_RADIUS");
            cRadius = aRadius;
            status = 0;
        }
        if( aRadius != cRadius )
        {
            invFlattening = aRadius / ( aRadius - cRadius );
        }
    }

    /* Waiting for linear keywords standardization only deg ctype are used */
    /* Check if WCS are there */
    fits_read_key(hFITS, TSTRING, "CTYPE1", ctype, nullptr, &status);
    if ( !status ) {
        /* Check if angular WCS are there */
        if ( strstr(ctype, "LN") )
        {
            /* Reading reference points */
            fits_read_key(hFITS, TDOUBLE, "CRPIX1", &crpix1, nullptr, &status);
            fits_read_key(hFITS, TDOUBLE, "CRPIX2", &crpix2, nullptr, &status);
            fits_read_key(hFITS, TDOUBLE, "CRVAL1", &crval1, nullptr, &status);
            fits_read_key(hFITS, TDOUBLE, "CRVAL2", &crval2, nullptr, &status);
            if( status )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                     "No CRPIX / CRVAL keyword available, the raster cannot be georeferenced.");
                status = 0;
            } else {
                /* Check for CDELT and PC matrix representation */
                fits_read_key(hFITS, TDOUBLE, "CDELT1", &cdelt1, nullptr, &status);
                if ( ! status ) {
                    fits_read_key(hFITS, TDOUBLE, "CDELT2", &cdelt2, nullptr, &status);
                    fits_read_key(hFITS, TDOUBLE, "PC1_1", &pc[0], nullptr, &status);
                    fits_read_key(hFITS, TDOUBLE, "PC1_2", &pc[1], nullptr, &status);
                    fits_read_key(hFITS, TDOUBLE, "PC2_1", &pc[2], nullptr, &status);
                    fits_read_key(hFITS, TDOUBLE, "PC2_2", &pc[3], nullptr, &status);
                    cd[0] = cdelt1 * pc[0];
                    cd[1] = cdelt1 * pc[1];
                    cd[2] = cdelt2 * pc[2];
                    cd[3] = cdelt2 * pc[3];
                    status = 0;
                } else {
                    /* Look for CD matrix representation */
                    fits_read_key(hFITS, TDOUBLE, "CD1_1", &cd[0], nullptr, &status);
                    fits_read_key(hFITS, TDOUBLE, "CD1_2", &cd[1], nullptr, &status);
                    fits_read_key(hFITS, TDOUBLE, "CD2_1", &cd[2], nullptr, &status);
                    fits_read_key(hFITS, TDOUBLE, "CD2_2", &cd[3], nullptr, &status);
                }

                double radfac = DEG2RAD * aRadius;

                adfGeoTransform[1] = cd[0] * radfac;
                adfGeoTransform[2] = cd[1] * radfac;
                adfGeoTransform[4] = cd[2] * radfac;
                adfGeoTransform[5] = - cd[3] * radfac ;
                if ( crval1 > 180. ) {
                    crval1 = crval1 - 180.;
                }

                /* NOTA BENE: FITS standard define pixel integers at the center of the pixel,
                   0.5 must be subtract to have UpperLeft corner */
                adfGeoTransform[0] = crval1 * radfac - adfGeoTransform[1] * (crpix1-0.5);
                // assuming that center latitude is also the origin of the coordinate
                // system: this is not always true.
                // More generic implementation coming soon
                adfGeoTransform[3] = - adfGeoTransform[5] * (crpix2-0.5);
                                                         //+ crval2 * radfac;
                bGeoTransformValid = true;
            }

            char* pstr = strrchr(ctype, '-');
            if( pstr ) {
                pstr += 1;

            /* Defining projection type
               Following http://www.gdal.org/ogr__srs__api_8h.html (GDAL)
               and http://www.aanda.org/component/article?access=bibcode&bibcode=&bibcode=2002A%2526A...395.1077CFUL (FITS)
            */

                /* Sinusoidal / SFL projection */
                if( strcmp(pstr,"SFL" ) == 0 ) {
                    projName.assign("Sinusoidal_");
                    oSRS.SetSinusoidal(crval1, falseEast, falseNorth);

                /* Mercator, Oblique (Hotine) Mercator, Transverse Mercator */
                /* Mercator / MER projection */
                } else if( strcmp(pstr,"MER" ) == 0 ) {
                    projName.assign("Mercator_");
                    oSRS.SetMercator(crval2, crval1, scale, falseEast, falseNorth);

                /* Equirectangular / CAR projection */
                } else if( strcmp(pstr,"CAR" ) == 0 ) {
                    projName.assign("Equirectangular_");
                /*
                The standard_parallel_1 defines where the local radius is calculated
                not the center of Y Cartesian system (which is latitude_of_origin)
                But FITS WCS only supports projections on the sphere
                we assume here that the local radius is the one computed at the projection center
                */
                    oSRS.SetEquirectangular2(crval2, crval1, crval2, falseEast, falseNorth);
                /* Lambert Azimuthal Equal Area / ZEA projection */
                } else if( strcmp(pstr,"ZEA" ) == 0 ) {
                    projName.assign("Lambert_Azimuthal_Equal_Area_");
                    oSRS.SetLAEA(crval2, crval1, falseEast, falseNorth);

                /* Lambert Conformal Conic 1SP / COO projection */
                } else if( strcmp(pstr,"COO" ) == 0 ) {
                    projName.assign("Lambert_Conformal_Conic_1SP_");
                    oSRS.SetLCC1SP (crval2, crval1, scale, falseEast, falseNorth);

                /* Orthographic / SIN projection */
                } else if( strcmp(pstr,"SIN" ) == 0 ) {
                    projName.assign("Orthographic_");
                    oSRS.SetOrthographic(crval2, crval1, falseEast, falseNorth);

                /* Point Perspective / AZP projection */
                } else if( strcmp(pstr,"AZP" ) == 0 ) {
                    projName.assign("perspective_point_height_");
                    oSRS.SetProjection(SRS_PP_PERSPECTIVE_POINT_HEIGHT);
                    /* # appears to need height... maybe center lon/lat */

                /* Polar Stereographic / STG projection */
                } else if( strcmp(pstr,"STG" ) == 0 ) {
                    projName.assign("Polar_Stereographic_");
                    oSRS.SetStereographic(crval2, crval1, scale, falseEast, falseNorth);
                } else {
                    CPLError(CE_Failure, CPLE_AppDefined, "Unknown projection.");
                }

                projName.append(target);
                oSRS.SetProjParm(SRS_PP_FALSE_EASTING,0.0);
                oSRS.SetProjParm(SRS_PP_FALSE_NORTHING,0.0);

                oSRS.SetNode("PROJCS",projName.c_str());

                oSRS.SetGeogCS(GeogName.c_str(), DatumName.c_str(), target, aRadius, invFlattening,
                    "Reference_Meridian", 0.0, "degree", 0.0174532925199433);
            }  else {
                CPLError(CE_Failure, CPLE_AppDefined, "Unknown projection.");
            }
        }
    } else {
        CPLError(CE_Failure, CPLE_AppDefined,
             "No CTYPE keywords: no geospatial information available.");
        status = 0;
    }

    CPLAssert(!bMetadataChanged);
    CPLAssert(!bNoDataChanged);

    bMetadataChanged = false;
    bNoDataChanged = false;

    bitpix = this->fitsDataType;
    FITSRasterBand *poBand = cpl::down_cast<FITSRasterBand*>(GetRasterBand(1));

    if (bitpix != TUSHORT && bitpix != TUINT)
    {
        fits_read_key(hFITS, TDOUBLE, "BSCALE", &dfScale, nullptr, &status);
        if( status )
        {
            status = 0;
            dfScale = 1.;
        }
        fits_read_key(hFITS, TDOUBLE, "BZERO", &dfOffset, nullptr, &status);
        if( status )
        {
            status = 0;
            dfOffset = 0.;
        }
        if ( dfScale != 1. || dfOffset != 0. )
        {
            poBand->bHaveOffsetScale = true;
            poBand->dfScale = dfScale;
            poBand->dfOffset = dfOffset;
        }
    }

    fits_read_key(hFITS, TDOUBLE, "BLANK", &dfNoDataValue, nullptr, &status);
    bNoDataSet = !status;
}

/************************************************************************/
/*                          GDALRegister_FITS()                         */
/************************************************************************/

void GDALRegister_FITS()

{
    if( GDALGetDriverByName( "FITS" ) != nullptr )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "FITS" );
    poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                               "Flexible Image Transport System" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC,
                               "frmt_various.html#FITS" );
    poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES,
                               "Byte UInt16 Int16 UInt32 Int32 Float32 Float64" );

    poDriver->pfnOpen = FITSDataset::Open;
    poDriver->pfnCreate = FITSDataset::Create;
    poDriver->pfnCreateCopy = nullptr;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
