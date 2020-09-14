/******************************************************************************
 *
 * Project:  FITS Driver
 * Purpose:  Implement FITS raster read/write support
 * Author:   Simon Perkins, s.perkins@lanl.gov
 *
 ******************************************************************************
 * Copyright (c) 2001, Simon Perkins
 * Copyright (c) 2008-2020, Even Rouault <even dot rouault at spatialys.com>
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

  fitsfile* m_hFITS = nullptr;

  int       m_hduNum = 0;
  GDALDataType m_gdalDataType = GDT_Unknown;   // GDAL code for the image type
  int m_fitsDataType = 0;   // FITS code for the image type

  bool m_isExistingFile = false;
  long m_highestOffsetWritten = 0;  // How much of image has been written

  bool        m_bNoDataChanged = false;
  bool        m_bNoDataSet = false;
  double      m_dfNoDataValue = -9999.0;

  bool        m_bMetadataChanged = false;

  CPLStringList m_aosSubdatasets{};

  OGRSpatialReference m_oSRS{};

  double      m_adfGeoTransform[6];
  bool        m_bGeoTransformValid = false;

  bool        m_bFITSInfoChanged = false;


  FITSDataset();     // Others should not call this constructor explicitly

  CPLErr Init(fitsfile* hFITS, bool isExistingFile, int hduNum);

  void        LoadGeoreferencing();
  void        LoadFITSInfo();
  void        WriteFITSInfo();
  void        LoadMetadata();

public:
  ~FITSDataset();

  static GDALDataset* Open( GDALOpenInfo* );
  static int          Identify( GDALOpenInfo* );
  static GDALDataset* Create( const char* pszFilename,
                              int nXSize, int nYSize, int nBands,
                              GDALDataType eType,
                              char** papszParmList );
  const OGRSpatialReference* GetSpatialRef() const override;
  CPLErr SetSpatialRef(const OGRSpatialReference* poSRS) override;
  virtual CPLErr GetGeoTransform( double * ) override;
  virtual CPLErr SetGeoTransform( double * ) override;
  char** GetMetadata(const char* papszDomain = nullptr) override;

  bool GetRawBinaryLayout(GDALDataset::RawBinaryLayout&) override;

};

/************************************************************************/
/* ==================================================================== */
/*                            FITSRasterBand                           */
/* ==================================================================== */
/************************************************************************/

class FITSRasterBand final: public GDALPamRasterBand {

  friend class  FITSDataset;

  bool               m_bHaveOffsetScale = false;
  double             m_dfOffset = 0.0;
  double             m_dfScale = 1.0;

 protected:
    FITSDataset       *m_poFDS = nullptr;

    bool               m_bNoDataSet = false;
    double             m_dfNoDataValue = -9999.0;

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
  m_poFDS(poDSIn)
{
  poDS = poDSIn;
  nBand = nBandIn;
  eDataType = poDSIn->m_gdalDataType;
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
  FITSDataset* dataset = m_poFDS;
  fitsfile* hFITS = dataset->m_hFITS;
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
  if (!dataset->m_isExistingFile && offset > dataset->m_highestOffsetWritten) {
    memset(pImage, 0, nBlockXSize * nBlockYSize
           * GDALGetDataTypeSize(eDataType) / 8);
    return CE_None;
  }

  // Otherwise read in the image data
  fits_read_img(hFITS, dataset->m_fitsDataType, offset, nElements,
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
  FITSDataset* dataset = m_poFDS;
  fitsfile* hFITS = dataset->m_hFITS;
  int status = 0;

  // Since a FITS block is a whole row, nBlockXOff must be zero
  // and the row number equals the y block offset. Also, nBlockYOff
  // cannot be greater than the number of rows

  // Calculate offsets and read in the data. Note that FITS array offsets
  // start at 1 at the bottom left...
  LONGLONG offset = static_cast<LONGLONG>(nBand - 1) * nRasterXSize * nRasterYSize +
    static_cast<LONGLONG>(nBlockYOff) * nRasterXSize + 1;
  long nElements = nRasterXSize;
  fits_write_img(hFITS, dataset->m_fitsDataType, offset, nElements,
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
  if (offset > dataset->m_highestOffsetWritten)
    dataset->m_highestOffsetWritten = offset;

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
static const char* const ignorableFITSHeaders[] = {
  "SIMPLE", "BITPIX", "NAXIS", "NAXIS1", "NAXIS2", "NAXIS3", "END",
  "XTENSION", "PCOUNT", "GCOUNT", "EXTEND", "CONTINUE",
  "COMMENT", "", "LONGSTRN", "BZERO", "BSCALE", "BLANK",
  "CHECKSUM", "DATASUM",
};
static bool isIgnorableFITSHeader(const char* name) {
  for (const char* keyword: ignorableFITSHeaders) {
    if (strcmp(name, keyword) == 0)
      return true;
  }
  return false;
}

/************************************************************************/
/*                            FITSDataset()                            */
/************************************************************************/

FITSDataset::FITSDataset()
{
    m_oSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    m_adfGeoTransform[0] = 0;
    m_adfGeoTransform[1] = 1;
    m_adfGeoTransform[2] = 0;
    m_adfGeoTransform[3] = 0;
    m_adfGeoTransform[4] = 0;
    m_adfGeoTransform[5] = 1;
}

/************************************************************************/
/*                           ~FITSDataset()                            */
/************************************************************************/

FITSDataset::~FITSDataset() {

  int status;
  if( m_hFITS )
  {
    if(m_hduNum > 0 && eAccess == GA_Update)
    {
      // Only do this if we've successfully opened the file and update
      // capability.  Write any meta data to the file that's compatible with
      // FITS.
      status = 0;
      fits_movabs_hdu(m_hFITS, m_hduNum, nullptr, &status);
      fits_write_key_longwarn(m_hFITS, &status);
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
                fits_update_key_longstr(m_hFITS, key, valueCpy, nullptr, &status);
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
      if (m_gdalDataType != GDT_Float32 && m_gdalDataType != GDT_Float64) {
        fits_update_key( m_hFITS, TDOUBLE, "BLANK", &m_dfNoDataValue, nullptr, &status);
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
      if (m_bMetadataChanged) {
        fits_update_key( m_hFITS, TDOUBLE, "BSCALE", &dfScale, nullptr, &status);
        if (status)
        {
            // Throw a warning with CFITSIO error status, then ignore status 
            CPLError(CE_Warning, CPLE_AppDefined,
                    "Couldn't update key BSCALE in FITS file %s (%d).",
                    GetDescription(), status);
            status = 0;
            return;
        }
        fits_update_key( m_hFITS, TDOUBLE, "BZERO", &dfOffset, nullptr, &status);
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
      if (m_bFITSInfoChanged) {
        WriteFITSInfo();
      }

      // Make sure we flush the raster cache before we close the file!
      FlushCache();
    }

    // Close the FITS handle
    fits_close_file(m_hFITS, &status);
  }
}


/************************************************************************/
/*                        GetRawBinaryLayout()                          */
/************************************************************************/

bool FITSDataset::GetRawBinaryLayout(GDALDataset::RawBinaryLayout& sLayout)
{
    if( m_hduNum == 0 )
        return false;
    int status = 0;
    if( fits_is_compressed_image( m_hFITS, &status) )
        return false;
    GDALDataType eDT = GetRasterBand(1)->GetRasterDataType();
    if( eDT == GDT_UInt16 || eDT == GDT_UInt32 )
        return false; // are supported as native signed with offset

    sLayout.osRawFilename = GetDescription();
    OFF_T headerstart = 0;
    OFF_T datastart = 0;
    OFF_T dataend = 0;
    fits_get_hduoff(m_hFITS, &headerstart, &datastart, &dataend, &status);
    if( nBands > 1 )
        sLayout.eInterleaving = RawBinaryLayout::Interleaving::BSQ;
    sLayout.eDataType = eDT;
    sLayout.bLittleEndianOrder = false;
    sLayout.nImageOffset = static_cast<GIntBig>(datastart);
    sLayout.nPixelOffset = GDALGetDataTypeSizeBytes(eDT);
    sLayout.nLineOffset = sLayout.nPixelOffset * nRasterXSize;
    sLayout.nBandOffset = sLayout.nLineOffset * nRasterYSize;
    return true;
}

/************************************************************************/
/*                           Init()                                     */
/************************************************************************/

CPLErr FITSDataset::Init(fitsfile* hFITS, bool isExistingFile, int hduNum) {

    m_hFITS = hFITS;
    m_isExistingFile = isExistingFile;

    int status = 0;
    double offset;

    int hduType = 0;
    fits_movabs_hdu(hFITS, hduNum, &hduType, &status);
    if (status)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                "Couldn't move to HDU %d in FITS file %s (%d).",
                hduNum, GetDescription(), status);
        return CE_Failure;
    }

    if( hduType != IMAGE_HDU )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                "HDU %d is not an image.", hduNum);
        return CE_Failure;
    }

    // Get the image info for this dataset (note that all bands in a FITS dataset
    // have the same type)
    int bitpix = 0;
    int naxis = 0;
    const int maxdim = 3;
    long naxes[maxdim] = {0,0,0};
    fits_get_img_param(hFITS, maxdim, &bitpix, &naxis, naxes, &status);
    if (status) {
        CPLError(CE_Failure, CPLE_AppDefined,
                "Couldn't determine image parameters of FITS file %s (%d)",
                GetDescription(), status);
        return CE_Failure;
    }

    m_hduNum = hduNum;

    fits_read_key(hFITS, TDOUBLE, "BZERO", &offset, nullptr, &status);
    if( status )
    {
        // BZERO is not mandatory offset defaulted to 0 if BZERO is missing
        status = 0;
        offset = 0.;
    }

    fits_read_key(hFITS, TDOUBLE, "BLANK", &m_dfNoDataValue, nullptr, &status);
    m_bNoDataSet = !status;
    status = 0;

    // Determine data type and nodata value if BLANK keyword is absent
    if (bitpix == BYTE_IMG) {
        m_gdalDataType = GDT_Byte;
        m_fitsDataType = TBYTE;
    }
    else if (bitpix == SHORT_IMG) {
        if (offset == 32768.)
        {
            m_gdalDataType = GDT_UInt16;
            m_fitsDataType = TUSHORT;
        }
        else {
            m_gdalDataType = GDT_Int16;
            m_fitsDataType = TSHORT;
        }
    }
    else if (bitpix == LONG_IMG) {
        if (offset == 2147483648.)
        {
            m_gdalDataType = GDT_UInt32;
            m_fitsDataType = TUINT;
        }
        else {
            m_gdalDataType = GDT_Int32;
            m_fitsDataType = TINT;
        }
    }
    else if (bitpix == FLOAT_IMG) {
        m_gdalDataType = GDT_Float32;
        m_fitsDataType = TFLOAT;
    }
    else if (bitpix == DOUBLE_IMG) {
        m_gdalDataType = GDT_Float64;
        m_fitsDataType = TDOUBLE;
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

    return CE_None;
}

/************************************************************************/
/*                         LoadMetadata()                               */
/************************************************************************/

void FITSDataset::LoadMetadata()
{
    // Read header information from file and use it to set metadata
    // This process understands the CONTINUE standard for long strings.
    // We don't bother to capture header names that duplicate information
    // already captured elsewhere (e.g. image dimensions and type)
    int keyNum;
    char key[100];
    char value[100];
    CPLStringList aosMD;

    int nKeys = 0;
    int nMoreKeys = 0;
    int status = 0;
    fits_get_hdrspace(m_hFITS, &nKeys, &nMoreKeys, &status);
    for(keyNum = 1; keyNum <= nKeys; keyNum++)
    {
        fits_read_keyn(m_hFITS, keyNum, key, value, nullptr, &status);
        if (status) {
            CPLError(CE_Failure, CPLE_AppDefined,
                    "Error while reading key %d from FITS file %s (%d)",
                    keyNum, GetDescription(), status);
            return;
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
                fits_read_key_longstr(m_hFITS, key, &longString, nullptr, &status);
                // Note that read_key_longstr already strips quotes
                if( status )
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                            "Error while reading long string for key %s from "
                            "FITS file %s (%d)", key, GetDescription(), status);
                    return;
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
}

/************************************************************************/
/*                           Identify()                                 */
/************************************************************************/

int FITSDataset::Identify(GDALOpenInfo* poOpenInfo)
{
    if( STARTS_WITH(poOpenInfo->pszFilename, "FITS:") )
        return true;

    const char* fitsID = "SIMPLE  =                    T";  // Spaces important!
    const size_t fitsIDLen = strlen(fitsID);  // Should be 30 chars long

    if (static_cast<size_t>(poOpenInfo->nHeaderBytes) < fitsIDLen)
        return false;
    if (memcmp(poOpenInfo->pabyHeader, fitsID, fitsIDLen) != 0)
        return false;
    return true;
}

/************************************************************************/
/*                            GetMetadata()                             */
/************************************************************************/

char **FITSDataset::GetMetadata( const char *pszDomain )

{
    if( pszDomain != nullptr && EQUAL(pszDomain,"SUBDATASETS") )
    {
        return m_aosSubdatasets.List();
    }

    return GDALPamDataset::GetMetadata(pszDomain);
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset* FITSDataset::Open(GDALOpenInfo* poOpenInfo) {

    if( !Identify(poOpenInfo) )
        return nullptr;

    CPLString osFilename(poOpenInfo->pszFilename);
    int iSelectedHDU = 0;
    if( STARTS_WITH(poOpenInfo->pszFilename, "FITS:") )
    {
        const CPLStringList aosTokens(
            CSLTokenizeString2(poOpenInfo->pszFilename, ":",
                            CSLT_HONOURSTRINGS | CSLT_PRESERVEESCAPES));
        if( aosTokens.size() != 3 )
        {
            return nullptr;
        }
        osFilename = aosTokens[1];
        iSelectedHDU = atoi(aosTokens[2]);
        if( iSelectedHDU <= 0 )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Invalid HDU number");
            return nullptr;
        }
    }

    // Get access mode and attempt to open the file
    int status = 0;
    fitsfile* hFITS = nullptr;
    if (poOpenInfo->eAccess == GA_ReadOnly)
        fits_open_file(&hFITS, osFilename.c_str(), READONLY, &status);
    else
        fits_open_file(&hFITS, osFilename.c_str(), READWRITE, &status);
    if (status) {
        CPLError(CE_Failure, CPLE_AppDefined,
                "Error while opening FITS file %s (%d).\n",
                osFilename.c_str(), status);
        fits_close_file(hFITS, &status);
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Iterate over HDUs                                               */
/* -------------------------------------------------------------------- */
    bool firstHDUIsDummy = false;
    int firstValidHDU = 0;
    CPLStringList aosSubdatasets;
    if( iSelectedHDU == 0 )
    {
        int numHDUs = 0;
        fits_get_num_hdus(hFITS, &numHDUs, &status);
        if( numHDUs <= 0 )
        {
            fits_close_file(hFITS, &status);
            return nullptr;
        }

        for( int iHDU = 1; iHDU <= numHDUs; iHDU++ )
        {
            int hduType = 0;
            fits_movabs_hdu(hFITS, iHDU, &hduType, &status);
            if (status)
            {
                continue;
            }

            if( hduType != IMAGE_HDU )
            {
                continue;
            }

            int bitpix = 0;
            int naxis = 0;
            const int maxdim = 3;
            long naxes[maxdim] = {0,0,0};
            fits_get_img_param(hFITS, maxdim, &bitpix, &naxis, naxes, &status);
            if (status)
            {
                continue;
            }

            if( naxis != 2 && naxis != 3 )
            {
                if( naxis == 0 && iHDU == 1 )
                {
                    firstHDUIsDummy = true;
                }
                continue;
            }

            char szExtname[81] = { 0 };
            fits_read_key(hFITS, TSTRING, "EXTNAME", szExtname, nullptr, &status);
            status = 0;

            const int nIdx = aosSubdatasets.size() / 2 + 1;
            aosSubdatasets.AddNameValue(
                CPLSPrintf("SUBDATASET_%d_NAME", nIdx),
                CPLSPrintf("FITS:\"%s\":%d", poOpenInfo->pszFilename, iHDU));
            CPLString osDesc(CPLSPrintf("HDU %d (%dx%d, %d band%s)", iHDU,
                        static_cast<int>(naxes[0]),
                        static_cast<int>(naxes[1]),
                        naxis == 3 ? static_cast<int>(naxes[2]) : 1,
                        (naxis == 3 && naxes[2] > 1) ? "s" : ""));
            if( szExtname[0] )
            {
                osDesc += ", ";
                osDesc += szExtname;
            }
            aosSubdatasets.AddNameValue(
                CPLSPrintf("SUBDATASET_%d_DESC", nIdx),
                osDesc);

            if( firstValidHDU == 0 )
            {
                firstValidHDU = iHDU;
            }
        }
        if( aosSubdatasets.size() == 2 )
        {
            aosSubdatasets.Clear();
        }
    }
    else
    {
        if( iSelectedHDU != 1 )
        {
            int hduType = 0;
            fits_movabs_hdu(hFITS, 1, &hduType, &status);
            if( status == 0 )
            {
                int bitpix = 0;
                int naxis = 0;
                const int maxdim = 3;
                long naxes[maxdim] = {0,0,0};
                fits_get_img_param(hFITS, maxdim, &bitpix, &naxis, naxes, &status);
                if( status == 0 && naxis == 0 )
                {
                    firstHDUIsDummy = true;
                }
            }
            status = 0;
        }
        firstValidHDU = iSelectedHDU;
    }

    if( firstValidHDU == 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find HDU of image type with 2 or 3 axes");
        fits_close_file(hFITS, &status);
        return nullptr;
    }

    // Create a FITSDataset object and initialize it from the FITS handle
    FITSDataset* dataset = new FITSDataset();
    dataset->eAccess = poOpenInfo->eAccess;
    dataset->m_aosSubdatasets = aosSubdatasets;

    // Set up the description and initialize the dataset
    dataset->SetDescription(poOpenInfo->pszFilename);
    if( aosSubdatasets.size() > 2 )
    {
        firstValidHDU = 0;
        dataset->m_hFITS = hFITS;
        dataset->m_isExistingFile = true;
        int hduType = 0;
        fits_movabs_hdu(hFITS, 1, &hduType, &status);
    }
    else
    {
        if (dataset->Init(hFITS, true, firstValidHDU) != CE_None) {
            delete dataset;
            return nullptr;
        }
    }

    // If the first HDU is a dummy one, load its metadata first, and then
    // add/override it by the one of the image HDU
    if( firstHDUIsDummy && firstValidHDU > 1 )
    {
        int hduType = 0;
        status = 0;
        fits_movabs_hdu(hFITS, 1, &hduType, &status);
        if( status == 0 )
        {
            dataset->LoadMetadata();
        }
        status = 0;
        fits_movabs_hdu(hFITS, firstValidHDU, &hduType, &status);
        if( status ) {
            delete dataset;
            return nullptr;
        }
    }
    dataset->LoadMetadata();

/* -------------------------------------------------------------------- */
/*      Initialize any information.                                     */
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
  if (dataset->Init(hFITS, false, 1) != CE_None) {
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
    const bool bHasProjection = !m_oSRS.IsEmpty();
    if( bHasProjection )
    {

        // Set according to coordinate system (thanks to Trent Hare - USGS)

        std::string object, ctype1, ctype2;
        
        const char* target = m_oSRS.GetAttrValue("DATUM",0);
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

            fits_update_key( m_hFITS, TSTRING, "OBJECT",
                             const_cast<void*>(static_cast<const void*>(object.c_str())),
                             nullptr, &status);
        }

        double aradius = m_oSRS.GetSemiMajor();
        double bradius = aradius;
        double cradius = m_oSRS.GetSemiMinor();

        cfactor = aradius * DEG2RAD;

        fits_update_key( m_hFITS, TDOUBLE, "A_RADIUS", &aradius, nullptr, &status);
        if (status)
        {
            // Throw a warning with CFITSIO error status, then ignore status 
            CPLError(CE_Warning, CPLE_AppDefined,
                    "Couldn't update key A_RADIUS in FITS file %s (%d).",
                    GetDescription(), status);
            status = 0;
            return;
        }
        fits_update_key( m_hFITS, TDOUBLE, "B_RADIUS", &bradius, nullptr, &status);
        if (status)
        {
            // Throw a warning with CFITSIO error status, then ignore status 
            CPLError(CE_Warning, CPLE_AppDefined,
                    "Couldn't update key B_RADIUS in FITS file %s (%d).",
                    GetDescription(), status);
            status = 0;
            return;
        }
        fits_update_key( m_hFITS, TDOUBLE, "C_RADIUS", &cradius, nullptr, &status);
        if (status)
        {
            // Throw a warning with CFITSIO error status, then ignore status 
            CPLError(CE_Warning, CPLE_AppDefined,
                    "Couldn't update key C_RADIUS in FITS file %s (%d).",
                    GetDescription(), status);
            status = 0;
            return;
        }

        const char* unit = m_oSRS.GetAttrValue("UNIT",0);

        ctype1.append("LN-");
        ctype2.append("LT-"); 

        // strcat(ctype1a, "PX-");
        // strcat(ctype2a, "PY-"); 

        std::string fitsproj;
        const char* projection = m_oSRS.GetAttrValue("PROJECTION",0);
        double centlon = 0, centlat = 0;

        if (projection) {
            if ( strstr(projection, "Sinusoidal") ) {
              fitsproj.assign("SFL");
              centlon = m_oSRS.GetProjParm("central_meridian", 0, nullptr);
            } else if ( strstr(projection, "Equirectangular") ) {
              fitsproj.assign("CAR");
              centlat = m_oSRS.GetProjParm("standard_parallel_1", 0, nullptr);
              centlon = m_oSRS.GetProjParm("central_meridian", 0, nullptr);
            } else if ( strstr(projection, "Orthographic") ) {
              fitsproj.assign("SIN");
              centlat = m_oSRS.GetProjParm("standard_parallel_1", 0, nullptr);
              centlon = m_oSRS.GetProjParm("central_meridian", 0, nullptr);
            } else if ( strstr(projection, "Mercator_1SP") || strstr(projection, "Mercator") ) {
              fitsproj.assign("MER");
              centlat = m_oSRS.GetProjParm("standard_parallel_1", 0, nullptr);
              centlon = m_oSRS.GetProjParm("central_meridian", 0, nullptr);
            } else if ( strstr(projection, "Polar_Stereographic") || strstr(projection, "Stereographic_South_Pole") || strstr(projection, "Stereographic_North_Pole") ) {
              fitsproj.assign("STG");
              centlat = m_oSRS.GetProjParm("latitude_of_origin", 0, nullptr);
              centlon = m_oSRS.GetProjParm("central_meridian", 0, nullptr);
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

            fits_update_key( m_hFITS, TSTRING, "CTYPE1",
                             const_cast<void*>(
                                 static_cast<const void*>(ctype1.c_str())),
                             nullptr, &status);
            if (status)
            {
                // Throw a warning with CFITSIO error status, then ignore status 
                CPLError(CE_Warning, CPLE_AppDefined,
                        "Couldn't update key CTYPE1 in FITS file %s (%d).",
                        GetDescription(), status);
                status = 0;
                return;
            }

            fits_update_key( m_hFITS, TSTRING, "CTYPE2",
                             const_cast<void*>(
                                 static_cast<const void*>(ctype2.c_str())),
                             nullptr, &status);
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


        UpperLeftCornerX = m_adfGeoTransform[0] - falseEast;
        UpperLeftCornerY = m_adfGeoTransform[3] - falseNorth;

        if ( centlon > 180. ) {
          centlon = centlon - 180.;
        }
        if ( strstr(unit, "metre") ) {
          // convert degrees/pixel to m/pixel 
          mapres = 1. / m_adfGeoTransform[1] ; // mapres is pixel/meters
          mres = m_adfGeoTransform[1] / cfactor ; // mres is deg/pixel
          crpix1 = - (UpperLeftCornerX * mapres) + centlon / mres + 0.5;
          // assuming that center latitude is also the origin of the coordinate
          // system: this is not always true.
          // More generic implementation coming soon
          crpix2 = (UpperLeftCornerY * mapres) + 0.5; // - (centlat / mres);
        } else if ( strstr(unit, "degree") ) {
          //convert m/pixel to pixel/degree
          mapres = 1. / m_adfGeoTransform[1] / cfactor; // mapres is pixel/deg
          mres = m_adfGeoTransform[1] ; // mres is meters/pixel
          crpix1 = - (UpperLeftCornerX * mres) + centlon / mapres + 0.5;
          // assuming that center latitude is also the origin of the coordinate
          // system: this is not always true.
          // More generic implementation coming soon
          crpix2 = (UpperLeftCornerY * mres) + 0.5; // - (centlat / mapres);
        }

        /// Write WCS CRPIXia CRVALia CTYPEia here

        fits_update_key( m_hFITS, TDOUBLE, "CRVAL1", &centlon, nullptr, &status);
        if (status)
        {
            // Throw a warning with CFITSIO error status, then ignore status 
            CPLError(CE_Warning, CPLE_AppDefined,
                    "Couldn't update key CRVAL1 in FITS file %s (%d).",
                    GetDescription(), status);
            status = 0;
            return;
        }
        fits_update_key( m_hFITS, TDOUBLE, "CRVAL2", &centlat, nullptr, &status);
        if (status)
        {
            // Throw a warning with CFITSIO error status, then ignore status 
            CPLError(CE_Warning, CPLE_AppDefined,
                    "Couldn't update key CRVAL2 in FITS file %s (%d).",
                    GetDescription(), status);
            status = 0;
            return;
        }
        fits_update_key( m_hFITS, TDOUBLE, "CRPIX1", &crpix1, nullptr, &status);
        if (status)
        {
            // Throw a warning with CFITSIO error status, then ignore status 
            CPLError(CE_Warning, CPLE_AppDefined,
                    "Couldn't update key CRPIX1 in FITS file %s (%d).",
                    GetDescription(), status);
            status = 0;
            return;
        }
        fits_update_key( m_hFITS, TDOUBLE, "CRPIX2", &crpix2, nullptr, &status);
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
        if( m_bGeoTransformValid )
        {

/* -------------------------------------------------------------------- */
/*      Write the transform.                                            */
/* -------------------------------------------------------------------- */

            /// Write WCS CDELTia and PCi_ja here

            double cd[4];
            cd[0] = m_adfGeoTransform[1] / cfactor;
            cd[1] = m_adfGeoTransform[2] / cfactor;
            cd[2] = m_adfGeoTransform[4] / cfactor;
            cd[3] = m_adfGeoTransform[5] / cfactor;

            double pc[4];
            pc[0] = 1.;
            pc[1] = cd[1] / cd[0];
            pc[2] = cd[2] / cd[3];
            pc[3] = - 1.;

            fits_update_key( m_hFITS, TDOUBLE, "CDELT1", &cd[0], nullptr, &status);
            if (status)
            {
                // Throw a warning with CFITSIO error status, then ignore status 
                CPLError(CE_Warning, CPLE_AppDefined,
                        "Couldn't update key CDELT1 in FITS file %s (%d).",
                        GetDescription(), status);
                status = 0;
                return;
            }

            fits_update_key( m_hFITS, TDOUBLE, "CDELT2", &cd[3], nullptr, &status);
            if (status)
            {
                // Throw a warning with CFITSIO error status, then ignore status 
                CPLError(CE_Warning, CPLE_AppDefined,
                        "Couldn't update key CDELT2 in FITS file %s (%d).",
                        GetDescription(), status);
                status = 0;
                return;
            }

            fits_update_key( m_hFITS, TDOUBLE, "PC1_1", &pc[0], nullptr, &status);
            if (status)
            {
                // Throw a warning with CFITSIO error status, then ignore status 
                CPLError(CE_Warning, CPLE_AppDefined,
                        "Couldn't update key PC1_1 in FITS file %s (%d).",
                        GetDescription(), status);
                status = 0;
                return;
            }

            fits_update_key( m_hFITS, TDOUBLE, "PC1_2", &pc[1], nullptr, &status);
            if (status)
            {
                // Throw a warning with CFITSIO error status, then ignore status 
                CPLError(CE_Warning, CPLE_AppDefined,
                        "Couldn't update key PC1_2 in FITS file %s (%d).",
                        GetDescription(), status);
                status = 0;
                return;
            }

            fits_update_key( m_hFITS, TDOUBLE, "PC2_1", &pc[2], nullptr, &status);
            if (status)
            {
                // Throw a warning with CFITSIO error status, then ignore status 
                CPLError(CE_Warning, CPLE_AppDefined,
                        "Couldn't update key PC2_1 in FITS file %s (%d).",
                        GetDescription(), status);
                status = 0;
                return;
            }

            fits_update_key( m_hFITS, TDOUBLE, "PC2_2", &pc[3], nullptr, &status);
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
    return m_oSRS.IsEmpty() ? nullptr : &m_oSRS;
}

/************************************************************************/
/*                           SetSpatialRef()                            */
/************************************************************************/

CPLErr FITSDataset::SetSpatialRef( const OGRSpatialReference * poSRS )

{
    if( poSRS == nullptr || poSRS->IsEmpty() )
    {
        m_oSRS.Clear();
    }
    else
    {
        m_oSRS = *poSRS;
        m_oSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    }

    m_bFITSInfoChanged = true;

    return CE_None;
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr FITSDataset::GetGeoTransform( double * padfTransform )

{
    memcpy( padfTransform, m_adfGeoTransform, sizeof(double) * 6 );

    if( !m_bGeoTransformValid )
        return CE_Failure;

    return CE_None;
}

/************************************************************************/
/*                          SetGeoTransform()                           */
/************************************************************************/

CPLErr FITSDataset::SetGeoTransform( double * padfTransform )

{
    memcpy( m_adfGeoTransform, padfTransform, sizeof(double)*6 );
    m_bGeoTransformValid = true;

    return CE_None;
}

/************************************************************************/
/*                             GetOffset()                              */
/************************************************************************/

double FITSRasterBand::GetOffset( int *pbSuccess )

{
    if( pbSuccess )
        *pbSuccess = m_bHaveOffsetScale;
    return m_dfOffset;
}

/************************************************************************/
/*                             SetOffset()                              */
/************************************************************************/

CPLErr FITSRasterBand::SetOffset( double dfNewValue )

{
    if( !m_bHaveOffsetScale || dfNewValue != m_dfOffset )
        m_poFDS->m_bMetadataChanged = true;

    m_bHaveOffsetScale = true;
    m_dfOffset = dfNewValue;
    return CE_None;
}

/************************************************************************/
/*                              GetScale()                              */
/************************************************************************/

double FITSRasterBand::GetScale( int *pbSuccess )

{
    if( pbSuccess )
        *pbSuccess = m_bHaveOffsetScale;
    return m_dfScale;
}

/************************************************************************/
/*                              SetScale()                              */
/************************************************************************/

CPLErr FITSRasterBand::SetScale( double dfNewValue )

{
    if( !m_bHaveOffsetScale || dfNewValue != m_dfScale )
        m_poFDS->m_bMetadataChanged = true;

    m_bHaveOffsetScale = true;
    m_dfScale = dfNewValue;
    return CE_None;
}

/************************************************************************/
/*                           GetNoDataValue()                           */
/************************************************************************/

double FITSRasterBand::GetNoDataValue( int * pbSuccess )

{
    if( m_bNoDataSet )
    {
        if( pbSuccess )
            *pbSuccess = TRUE;

        return m_dfNoDataValue;
    }

    if( m_poFDS->m_bNoDataSet )
    {
        if( pbSuccess )
            *pbSuccess = TRUE;

        return m_poFDS->m_dfNoDataValue;
    }

    return GDALPamRasterBand::GetNoDataValue( pbSuccess );
}

/************************************************************************/
/*                           SetNoDataValue()                           */
/************************************************************************/

CPLErr FITSRasterBand::SetNoDataValue( double dfNoData )

{
    if( m_poFDS->m_bNoDataSet && m_poFDS->m_dfNoDataValue == dfNoData )
    {
        m_bNoDataSet = true;
        m_dfNoDataValue = dfNoData;
        return CE_None;
    }

    m_poFDS->m_bNoDataSet = true;
    m_poFDS->m_dfNoDataValue = dfNoData;

    m_poFDS->m_bNoDataChanged = true;

    m_bNoDataSet = true;
    m_dfNoDataValue = dfNoData;
    return CE_None;
}

/************************************************************************/
/*                        DeleteNoDataValue()                           */
/************************************************************************/

CPLErr FITSRasterBand::DeleteNoDataValue()

{
    if( !m_poFDS->m_bNoDataSet )
        return CE_None;

    m_poFDS->m_bNoDataSet = false;
    m_poFDS->m_dfNoDataValue = -9999.0;

    m_poFDS->m_bNoDataChanged = true;

    m_bNoDataSet = false;
    m_dfNoDataValue = -9999.0;
    return CE_None;
}

/************************************************************************/
/*                         LoadGeoreferencing()                         */
/************************************************************************/

void FITSDataset::LoadGeoreferencing()
{
    int status = 0;
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

    fits_read_key(m_hFITS, TSTRING, "OBJECT", target, nullptr, &status);
    if( status )
    {
        strncpy(target, "Undefined", 10);
        CPLDebug("FITS", "OBJECT keyword is missing");
        status = 0;
    }

    GeogName.assign("GCS_");
    GeogName.append(target);
    DatumName.assign("D_");
    DatumName.append(target);

    fits_read_key(m_hFITS, TDOUBLE, "A_RADIUS", &aRadius, nullptr, &status);
    if( status )
    {
        CPLDebug("FITS",
            "No Radii keyword available, metadata will not contain DATUM information.");
        return;
    } else {
        fits_read_key(m_hFITS, TDOUBLE, "C_RADIUS", &cRadius, nullptr, &status);
        if( status )
        {
            CPLError(CE_Warning, CPLE_AppDefined,
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
    fits_read_key(m_hFITS, TSTRING, "CTYPE1", ctype, nullptr, &status);
    if ( !status ) {
        /* Check if angular WCS are there */
        if ( strstr(ctype, "LN") )
        {
            /* Reading reference points */
            fits_read_key(m_hFITS, TDOUBLE, "CRPIX1", &crpix1, nullptr, &status);
            fits_read_key(m_hFITS, TDOUBLE, "CRPIX2", &crpix2, nullptr, &status);
            fits_read_key(m_hFITS, TDOUBLE, "CRVAL1", &crval1, nullptr, &status);
            fits_read_key(m_hFITS, TDOUBLE, "CRVAL2", &crval2, nullptr, &status);
            if( status )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                     "No CRPIX / CRVAL keyword available, the raster cannot be georeferenced.");
                status = 0;
            } else {
                /* Check for CDELT and PC matrix representation */
                fits_read_key(m_hFITS, TDOUBLE, "CDELT1", &cdelt1, nullptr, &status);
                if ( ! status ) {
                    fits_read_key(m_hFITS, TDOUBLE, "CDELT2", &cdelt2, nullptr, &status);
                    fits_read_key(m_hFITS, TDOUBLE, "PC1_1", &pc[0], nullptr, &status);
                    fits_read_key(m_hFITS, TDOUBLE, "PC1_2", &pc[1], nullptr, &status);
                    fits_read_key(m_hFITS, TDOUBLE, "PC2_1", &pc[2], nullptr, &status);
                    fits_read_key(m_hFITS, TDOUBLE, "PC2_2", &pc[3], nullptr, &status);
                    cd[0] = cdelt1 * pc[0];
                    cd[1] = cdelt1 * pc[1];
                    cd[2] = cdelt2 * pc[2];
                    cd[3] = cdelt2 * pc[3];
                    status = 0;
                } else {
                    /* Look for CD matrix representation */
                    fits_read_key(m_hFITS, TDOUBLE, "CD1_1", &cd[0], nullptr, &status);
                    fits_read_key(m_hFITS, TDOUBLE, "CD1_2", &cd[1], nullptr, &status);
                    fits_read_key(m_hFITS, TDOUBLE, "CD2_1", &cd[2], nullptr, &status);
                    fits_read_key(m_hFITS, TDOUBLE, "CD2_2", &cd[3], nullptr, &status);
                }

                double radfac = DEG2RAD * aRadius;

                m_adfGeoTransform[1] = cd[0] * radfac;
                m_adfGeoTransform[2] = cd[1] * radfac;
                m_adfGeoTransform[4] = cd[2] * radfac;
                m_adfGeoTransform[5] = - cd[3] * radfac ;
                if ( crval1 > 180. ) {
                    crval1 = crval1 - 180.;
                }

                /* NOTA BENE: FITS standard define pixel integers at the center of the pixel,
                   0.5 must be subtract to have UpperLeft corner */
                m_adfGeoTransform[0] = crval1 * radfac - m_adfGeoTransform[1] * (crpix1-0.5);
                // assuming that center latitude is also the origin of the coordinate
                // system: this is not always true.
                // More generic implementation coming soon
                m_adfGeoTransform[3] = - m_adfGeoTransform[5] * (crpix2-0.5);
                                                         //+ crval2 * radfac;
                m_bGeoTransformValid = true;
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
                    m_oSRS.SetSinusoidal(crval1, falseEast, falseNorth);

                /* Mercator, Oblique (Hotine) Mercator, Transverse Mercator */
                /* Mercator / MER projection */
                } else if( strcmp(pstr,"MER" ) == 0 ) {
                    projName.assign("Mercator_");
                    m_oSRS.SetMercator(crval2, crval1, scale, falseEast, falseNorth);

                /* Equirectangular / CAR projection */
                } else if( strcmp(pstr,"CAR" ) == 0 ) {
                    projName.assign("Equirectangular_");
                /*
                The standard_parallel_1 defines where the local radius is calculated
                not the center of Y Cartesian system (which is latitude_of_origin)
                But FITS WCS only supports projections on the sphere
                we assume here that the local radius is the one computed at the projection center
                */
                    m_oSRS.SetEquirectangular2(crval2, crval1, crval2, falseEast, falseNorth);
                /* Lambert Azimuthal Equal Area / ZEA projection */
                } else if( strcmp(pstr,"ZEA" ) == 0 ) {
                    projName.assign("Lambert_Azimuthal_Equal_Area_");
                    m_oSRS.SetLAEA(crval2, crval1, falseEast, falseNorth);

                /* Lambert Conformal Conic 1SP / COO projection */
                } else if( strcmp(pstr,"COO" ) == 0 ) {
                    projName.assign("Lambert_Conformal_Conic_1SP_");
                    m_oSRS.SetLCC1SP (crval2, crval1, scale, falseEast, falseNorth);

                /* Orthographic / SIN projection */
                } else if( strcmp(pstr,"SIN" ) == 0 ) {
                    projName.assign("Orthographic_");
                    m_oSRS.SetOrthographic(crval2, crval1, falseEast, falseNorth);

                /* Point Perspective / AZP projection */
                } else if( strcmp(pstr,"AZP" ) == 0 ) {
                    projName.assign("perspective_point_height_");
                    m_oSRS.SetProjection(SRS_PP_PERSPECTIVE_POINT_HEIGHT);
                    /* # appears to need height... maybe center lon/lat */

                /* Polar Stereographic / STG projection */
                } else if( strcmp(pstr,"STG" ) == 0 ) {
                    projName.assign("Polar_Stereographic_");
                    m_oSRS.SetStereographic(crval2, crval1, scale, falseEast, falseNorth);
                } else {
                    CPLError(CE_Failure, CPLE_AppDefined, "Unknown projection.");
                }

                projName.append(target);
                m_oSRS.SetProjParm(SRS_PP_FALSE_EASTING,0.0);
                m_oSRS.SetProjParm(SRS_PP_FALSE_NORTHING,0.0);

                m_oSRS.SetNode("PROJCS",projName.c_str());

                m_oSRS.SetGeogCS(GeogName.c_str(), DatumName.c_str(), target, aRadius, invFlattening,
                    "Reference_Meridian", 0.0, "degree", 0.0174532925199433);
            }  else {
                CPLError(CE_Failure, CPLE_AppDefined, "Unknown projection.");
            }
        }
    } else {
        CPLError(CE_Warning, CPLE_AppDefined,
             "No CTYPE keywords: no geospatial information available.");
    }
}

/************************************************************************/
/*                     LoadFITSInfo()                                   */
/************************************************************************/

void FITSDataset::LoadFITSInfo()

{
    int status = 0;
    int bitpix;
    double dfScale, dfOffset;

    LoadGeoreferencing();

    CPLAssert(!m_bMetadataChanged);
    CPLAssert(!m_bNoDataChanged);

    m_bMetadataChanged = false;
    m_bNoDataChanged = false;

    bitpix = this->m_fitsDataType;
    FITSRasterBand *poBand = cpl::down_cast<FITSRasterBand*>(GetRasterBand(1));

    if (bitpix != TUSHORT && bitpix != TUINT)
    {
        fits_read_key(m_hFITS, TDOUBLE, "BSCALE", &dfScale, nullptr, &status);
        if( status )
        {
            status = 0;
            dfScale = 1.;
        }
        fits_read_key(m_hFITS, TDOUBLE, "BZERO", &dfOffset, nullptr, &status);
        if( status )
        {
            status = 0;
            dfOffset = 0.;
        }
        if ( dfScale != 1. || dfOffset != 0. )
        {
            poBand->m_bHaveOffsetScale = true;
            poBand->m_dfScale = dfScale;
            poBand->m_dfOffset = dfOffset;
        }
    }

    fits_read_key(m_hFITS, TDOUBLE, "BLANK", &m_dfNoDataValue, nullptr, &status);
    m_bNoDataSet = !status;
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
                               "drivers/raster/fits.html" );
    poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES,
                               "Byte UInt16 Int16 UInt32 Int32 Float32 Float64" );
    poDriver->SetMetadataItem( GDAL_DMD_EXTENSIONS, "fits" );

    poDriver->pfnOpen = FITSDataset::Open;
    poDriver->pfnIdentify = FITSDataset::Identify;
    poDriver->pfnCreate = FITSDataset::Create;
    poDriver->pfnCreateCopy = nullptr;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
