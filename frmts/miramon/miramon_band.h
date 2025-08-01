/******************************************************************************
 *
 * Project:  MiraMonRaster driver
 * Purpose:  Implements MMRBand class: This class manages the metadata of each
 *           band to be processed. It is useful for maintaining a list of bands
 *           and for determining the number of subdatasets that need to be
 *           generated.
 * Author:   Abel Pau
 *
 ******************************************************************************
 * Copyright (c) 2025, Xavier Pons
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef MM_BAND_INCLUDED
#define MM_BAND_INCLUDED

#include <vector>
#include <array>

#include "miramon_rel.h"
class MMRRel;

/************************************************************************/
/*                               MMRBand                                */
/************************************************************************/
enum class MMDataType
{
    DATATYPE_AND_COMPR_UNDEFINED = -1,
    DATATYPE_AND_COMPR_MIN = 0,
    DATATYPE_AND_COMPR_STRING = 0,
    DATATYPE_AND_COMPR_BIT = 1,
    DATATYPE_AND_COMPR_BIT_VELL = 2,  // Not supported
    DATATYPE_AND_COMPR_BYTE = 3,
    DATATYPE_AND_COMPR_INTEGER = 4,
    DATATYPE_AND_COMPR_UINTEGER = 5,
    DATATYPE_AND_COMPR_LONG = 6,
    DATATYPE_AND_COMPR_INTEGER_ASCII = 7,
    DATATYPE_AND_COMPR_REAL = 8,
    DATATYPE_AND_COMPR_DOUBLE = 9,
    DATATYPE_AND_COMPR_REAL_ASCII = 10,
    DATATYPE_AND_COMPR_BYTE_RLE = 11,
    DATATYPE_AND_COMPR_INTEGER_RLE = 12,
    DATATYPE_AND_COMPR_UINTEGER_RLE = 13,
    DATATYPE_AND_COMPR_LONG_RLE = 14,
    DATATYPE_AND_COMPR_REAL_RLE = 15,
    DATATYPE_AND_COMPR_DOUBLE_RLE = 16,
    DATATYPE_AND_COMPR_MAX = 16
};

enum class MMBytesPerPixel
{
    TYPE_BYTES_PER_PIXEL_UNDEFINED = -1,
    TYPE_BYTES_PER_PIXEL_STRING = 0,
    TYPE_BYTES_PER_PIXEL_BIT = 0,
    TYPE_BYTES_PER_PIXEL_BYTE_I_RLE = 1,
    TYPE_BYTES_PER_PIXEL_INTEGER_I_RLE = 2,
    TYPE_BYTES_PER_PIXEL_LONG_REAL_I_RLE = 4,
    TYPE_BYTES_PER_PIXEL_DOUBLE_I_RLE = 8
};

class MMRBand
{
  public:
    MMRBand(MMRRel &pfRel, CPLString osSection);
    MMRBand(const MMRBand &) =
        delete;  // I don't want to construct a MMRBand from another MMRBand (effc++)
    MMRBand &operator=(const MMRBand &) =
        delete;  // I don't want to assing a MMRBand to another MMRBand (effc++)
    ~MMRBand();

    const CPLString GetRELFileName() const;
    CPLErr GetRasterBlock(int nXBlock, int nYBlock, void *pData, int nDataSize);

    int UpdateGeoTransform();

    int GetAssignedSubDataSet()
    {
        return nAssignedSDS;
    }

    void AssignSubDataSet(int nAssignedSDSIn)
    {
        nAssignedSDS = nAssignedSDSIn;
    }

    const CPLString &GetBandName() const
    {
        return osBandName;
    }

    const CPLString &GetBandSection() const
    {
        return osBandSection;
    }

    const CPLString &GetRawBandFileName() const
    {
        return osRawBandFileName;
    }

    const CPLString &GetFriendlyDescription() const
    {
        return osFriendlyDescription;
    }

    MMDataType GeteMMDataType() const
    {
        return eMMDataType;
    }

    MMBytesPerPixel GeteMMBytesPerPixel() const
    {
        return eMMBytesPerPixel;
    }

    bool GetMinSet() const
    {
        return bMinSet;
    }

    double GetMin() const
    {
        return dfMin;
    }

    bool GetMaxSet() const
    {
        return bMaxSet;
    }

    double GetMax() const
    {
        return dfMax;
    }

    bool GetVisuMinSet() const
    {
        return bMinVisuSet;
    }

    double GetVisuMin() const
    {
        return dfVisuMin;
    }

    bool GetVisuMaxSet() const
    {
        return bMaxVisuSet;
    }

    double GetVisuMax() const
    {
        return dfVisuMax;
    }

    double GetBoundingBoxMinX() const
    {
        return dfBBMinX;
    }

    double GetBoundingBoxMaxX() const
    {
        return dfBBMaxX;
    }

    double GetBoundingBoxMinY() const
    {
        return dfBBMinY;
    }

    double GetBoundingBoxMaxY() const
    {
        return dfBBMaxY;
    }

    bool BandHasNoData() const
    {
        return bNoDataSet;
    }

    double GetNoDataValue() const
    {
        return dfNoData;
    }

    int GetWidth() const
    {
        return nWidth;
    }

    int GetHeight() const
    {
        return nHeight;
    }

    int GetBlockXSize() const
    {
        return nBlockXSize;
    }

    int GetBlockYSize() const
    {
        return nBlockYSize;
    }

    bool IsValid() const
    {
        return bIsValid;
    }

    GDALGeoTransform m_gt{};  // Bounding box for this band

  private:
    int Get_ATTRIBUTE_DATA_or_OVERVIEW_ASPECTES_TECNICS_int(
        const CPLString osSection, const char *pszKey, int *nValue,
        const char *pszErrorMessage);
    static int GetDataTypeAndBytesPerPixel(const char *pszCompType,
                                           MMDataType *nCompressionType,
                                           MMBytesPerPixel *nBytesPerPixel);
    int UpdateDataTypeFromREL(const CPLString osSection);
    int UpdateColumnsNumberFromREL(const CPLString osSection);
    int UpdateRowsNumberFromREL(const CPLString osSection);
    void UpdateNoDataValue(const CPLString osSection);
    void UpdateBoundingBoxFromREL(const CPLString osSection);
    void UpdateReferenceSystemFromREL();
    void UpdateMinMaxValuesFromREL(const CPLString osSection);
    void UpdateMinMaxVisuValuesFromREL(const CPLString osSection);
    void UpdateFriendlyDescriptionFromREL(const CPLString osSection);

    template <typename TYPE>
    CPLErr UncompressRow(void *rowBuffer, size_t nCompressedRawSize);
    CPLErr GetBlockData(void *rowBuffer, size_t nCompressedRawSize);
    int PositionAtStartOfRowOffsetsInFile();
    bool FillRowOffsets();

    bool bIsValid = false;  // Determines if the created object is valid or not.

    VSILFILE *pfIMG = nullptr;  // Point to IMG file (RAW data)
    MMRRel *pfRel = nullptr;    // Rel where metadata is readed from

    int nBlockXSize = 1;
    int nBlockYSize = 1;

    int nWidth = 0;   // Number of columns
    int nHeight = 0;  // Number of rows

    int nNRowsPerBlock = 1;

    // indexed-RLE format
    std::vector<vsi_l_offset> aFileOffsets{};

    // Assigned Subdataset for this band.
    int nAssignedSDS = 0;

    // Section in REL file that give information about the band
    CPLString osBandSection;
    // File name relative to REL file with banda data
    CPLString osRawBandFileName = "";
    // Friendly osRawBandFileName
    CPLString osBandFileName = "";
    // Name of the band documented in REL metadata file.
    CPLString osBandName = "";
    // Descripcion of the band, not the name
    CPLString osFriendlyDescription = "";

    MMDataType eMMDataType =
        static_cast<MMDataType>(MMDataType::DATATYPE_AND_COMPR_UNDEFINED);
    MMBytesPerPixel eMMBytesPerPixel = static_cast<MMBytesPerPixel>(
        MMBytesPerPixel::TYPE_BYTES_PER_PIXEL_UNDEFINED);
    int nDataTypeSizeBytes = 0;

    bool bIsCompressed = false;

    // Min and Max values from metadata:  This value should correspond
    // to the actual minimum and maximum, not to an approximation.
    // However, MiraMon is proof to approximate values. The minimum
    // and maximum values are useful, for example, to properly scale
    // colors, etc.
    bool bMinSet = false;
    double dfMin = 0.0;
    bool bMaxSet = false;
    double dfMax = 0.0;
    // These values will be dfMin/dfMax if they don't exist in REL file
    bool bMinVisuSet = false;
    double dfVisuMin = 0.0;  // Key Color_ValorColor_0 in COLOR_TEXT
    bool bMaxVisuSet = false;
    double dfVisuMax = 0.0;  // Key Color_ValorColor_n_1 COLOR_TEXT

    CPLString osRefSystem = "";

    // Extent values of the band:
    // They always refer to extreme outer coordinates,
    // not to cell centers.

    double dfBBMinX = 0.0;
    double dfBBMinY = 0.0;
    double dfBBMaxX = 0.0;
    double dfBBMaxY = 0.0;

    // Nodata stuff
    bool bNoDataSet = false;  // There is nodata?
    double dfNoData = 0.0;    // Value of nodata
};

#endif /* ndef MM_BAND_INCLUDED */
