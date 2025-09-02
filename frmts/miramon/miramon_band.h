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
    MMRBand(MMRRel &pfRel, const CPLString &osSection);
    MMRBand(const MMRBand &) =
        delete;  // I don't want to construct a MMRBand from another MMRBand (effc++)
    MMRBand &operator=(const MMRBand &) =
        delete;  // I don't want to assign a MMRBand to another MMRBand (effc++)
    ~MMRBand();

    const CPLString GetRELFileName() const;
    CPLErr GetRasterBlock(int nXBlock, int nYBlock, void *pData, int nDataSize);

    void UpdateGeoTransform();

    int GetAssignedSubDataSet() const
    {
        return m_nAssignedSDS;
    }

    void AssignSubDataSet(int nAssignedSDSIn)
    {
        m_nAssignedSDS = nAssignedSDSIn;
    }

    const CPLString &GetBandName() const
    {
        return m_osBandName;
    }

    const CPLString &GetBandSection() const
    {
        return m_osBandSection;
    }

    const CPLString &GetRawBandFileName() const
    {
        return m_osRawBandFileName;
    }

    const CPLString &GetFriendlyDescription() const
    {
        return m_osFriendlyDescription;
    }

    MMDataType GeteMMDataType() const
    {
        return m_eMMDataType;
    }

    MMBytesPerPixel GeteMMBytesPerPixel() const
    {
        return m_eMMBytesPerPixel;
    }

    bool GetMinSet() const
    {
        return m_bMinSet;
    }

    double GetMin() const
    {
        return m_dfMin;
    }

    bool GetMaxSet() const
    {
        return m_bMaxSet;
    }

    double GetMax() const
    {
        return m_dfMax;
    }

    bool GetVisuMinSet() const
    {
        return m_bMinVisuSet;
    }

    double GetVisuMin() const
    {
        return m_dfVisuMin;
    }

    bool GetVisuMaxSet() const
    {
        return m_bMaxVisuSet;
    }

    double GetVisuMax() const
    {
        return m_dfVisuMax;
    }

    double GetBoundingBoxMinX() const
    {
        return m_dfBBMinX;
    }

    double GetBoundingBoxMaxX() const
    {
        return m_dfBBMaxX;
    }

    double GetBoundingBoxMinY() const
    {
        return m_dfBBMinY;
    }

    double GetBoundingBoxMaxY() const
    {
        return m_dfBBMaxY;
    }

    bool BandHasNoData() const
    {
        return m_bNoDataSet;
    }

    double GetNoDataValue() const
    {
        return m_dfNoData;
    }

    int GetWidth() const
    {
        return m_nWidth;
    }

    int GetHeight() const
    {
        return m_nHeight;
    }

    int GetBlockXSize() const
    {
        return m_nBlockXSize;
    }

    int GetBlockYSize() const
    {
        return m_nBlockYSize;
    }

    bool IsValid() const
    {
        return m_bIsValid;
    }

    GDALGeoTransform m_gt{};  // Bounding box for this band

  private:
    bool Get_ATTRIBUTE_DATA_or_OVERVIEW_ASPECTES_TECNICS_int(
        const CPLString &osSection, const char *pszKey, int *nValue,
        const char *pszErrorMessage);
    static bool GetDataTypeAndBytesPerPixel(const char *pszCompType,
                                            MMDataType *nCompressionType,
                                            MMBytesPerPixel *nBytesPerPixel);
    bool UpdateDataTypeFromREL(const CPLString osSection);
    bool UpdateColumnsNumberFromREL(const CPLString &osSection);
    bool UpdateRowsNumberFromREL(const CPLString &osSection);
    void UpdateNoDataValue(const CPLString &osSection);
    void UpdateBoundingBoxFromREL(const CPLString &osSection);
    void UpdateReferenceSystemFromREL();
    void UpdateMinMaxValuesFromREL(const CPLString &osSection);
    void UpdateMinMaxVisuValuesFromREL(const CPLString &osSection);
    void UpdateFriendlyDescriptionFromREL(const CPLString &osSection);

    template <typename TYPE>
    CPLErr UncompressRow(void *rowBuffer, size_t nCompressedRawSize);
    CPLErr GetBlockData(void *rowBuffer, size_t nCompressedRawSize);
    int PositionAtStartOfRowOffsetsInFile();
    bool FillRowOffsets();

    bool m_bIsValid =
        false;  // Determines if the created object is valid or not.

    VSILFILE *m_pfIMG = nullptr;  // Point to IMG file (RAW data)
    MMRRel *m_pfRel = nullptr;    // Rel where metadata is read from

    int m_nBlockXSize = 1;
    int m_nBlockYSize = 1;

    int m_nWidth = 0;   // Number of columns
    int m_nHeight = 0;  // Number of rows

    int m_nNRowsPerBlock = 1;

    // indexed-RLE format
    std::vector<vsi_l_offset> m_aFileOffsets{};

    // Assigned Subdataset for this band.
    int m_nAssignedSDS = 0;

    // Section in REL file that give information about the band
    CPLString m_osBandSection;
    // File name relative to REL file with banda data
    CPLString m_osRawBandFileName = "";
    // Friendly osRawBandFileName
    CPLString m_osBandFileName = "";
    // Name of the band documented in REL metadata file.
    CPLString m_osBandName = "";
    // Descripcion of the band, not the name
    CPLString m_osFriendlyDescription = "";

    MMDataType m_eMMDataType =
        static_cast<MMDataType>(MMDataType::DATATYPE_AND_COMPR_UNDEFINED);
    MMBytesPerPixel m_eMMBytesPerPixel = static_cast<MMBytesPerPixel>(
        MMBytesPerPixel::TYPE_BYTES_PER_PIXEL_UNDEFINED);
    int m_nDataTypeSizeBytes = 0;

    bool m_bIsCompressed = false;

    // Min and Max values from metadata:  This value should correspond
    // to the actual minimum and maximum, not to an approximation.
    // However, MiraMon is proof to approximate values. The minimum
    // and maximum values are useful, for example, to properly scale
    // colors, etc.
    bool m_bMinSet = false;
    double m_dfMin = 0.0;
    bool m_bMaxSet = false;
    double m_dfMax = 0.0;
    // These values will be dfMin/dfMax if they don't exist in REL file
    bool m_bMinVisuSet = false;
    double m_dfVisuMin = 0.0;  // Key Color_ValorColor_0 in COLOR_TEXT
    bool m_bMaxVisuSet = false;
    double m_dfVisuMax = 0.0;  // Key Color_ValorColor_n_1 COLOR_TEXT

    CPLString m_osRefSystem = "";

    // Extent values of the band:
    // They always refer to extreme outer coordinates,
    // not to cell centers.

    double m_dfBBMinX = 0.0;
    double m_dfBBMinY = 0.0;
    double m_dfBBMaxX = 0.0;
    double m_dfBBMaxY = 0.0;

    // Nodata stuff
    bool m_bNoDataSet = false;  // There is nodata?
    double m_dfNoData = 0.0;    // Value of nodata
};

#endif /* ndef MM_BAND_INCLUDED */
