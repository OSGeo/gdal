/******************************************************************************
 *
 * Project:  Hierarchical Data Format Release 4 (HDF4)
 * Purpose:  Header file for HDF4 datasets reader.
 * Author:   Andrey Kiselev, dron@ak4719.spb.edu
 *
 ******************************************************************************
 * Copyright (c) 2002, Andrey Kiselev <dron@ak4719.spb.edu>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef HDF4DATASET_H_INCLUDED_
#define HDF4DATASET_H_INCLUDED_

#include "hdf.h"
#include "mfhdf.h"

#include "cpl_list.h"
#include "gdal_pam.h"

constexpr int HDF4_SDS_MAXNAMELEN = 65;

constexpr int N_BUF_SIZE = 8192;

typedef enum  // Types of dataset:
{
    HDF4_SDS,  // Scientific Dataset
    HDF4_GR,   // General Raster Image
    HDF4_EOS,  // HDF EOS
    HDF4_UNKNOWN
} HDF4DatasetType;

typedef enum  // Types of data products:
{
    H4ST_GDAL,            // HDF written by GDAL
    H4ST_EOS_GRID,        // HDF-EOS Grid
    H4ST_EOS_SWATH,       // HDF-EOS Swath
    H4ST_EOS_SWATH_GEOL,  // HDF-EOS Swath Geolocation Array
    H4ST_SEAWIFS_L1A,     // SeaWiFS Level-1A Data
    H4ST_SEAWIFS_L2,      // SeaWiFS Level-2 Data
    H4ST_SEAWIFS_L3,      // SeaWiFS Level-3 Standard Mapped Image
    H4ST_HYPERION_L1,     // Hyperion L1 Data Product
    H4ST_UNKNOWN
} HDF4SubdatasetType;

/************************************************************************/
/* ==================================================================== */
/*                              HDF4Dataset                             */
/* ==================================================================== */
/************************************************************************/

class HDF4Dataset CPL_NON_FINAL : public GDALPamDataset
{

  private:
    bool bIsHDFEOS;
    std::shared_ptr<GDALGroup> m_poRootGroup{};

    static char **HDF4EOSTokenizeAttrs(const char *pszString);
    static char **HDF4EOSGetObject(char **papszAttrList, char **ppszAttrName,
                                   char **ppszAttrClass, char **ppszAttrValue);

    void OpenMultiDim(const char *pszFilename, CSLConstList papszOpenOptionsIn);

    CPL_DISALLOW_COPY_ASSIGN(HDF4Dataset)

  protected:
    int32 hGR;
    int32 hSD;
    int32 nImages;
    HDF4SubdatasetType iSubdatasetType;
    const char *pszSubdatasetType;

    char **papszGlobalMetadata;
    char **papszSubDatasets;

    CPLErr ReadGlobalAttributes(int32);

  public:
    static GDALDataType GetDataType(int32);
    static const char *GetDataTypeName(int32);
    static int GetDataTypeSize(int32);
    static double AnyTypeToDouble(int32, void *);
    static char **TranslateHDF4Attributes(int32, int32, char *, int32, int32,
                                          char **);
    static char **TranslateHDF4EOSAttributes(int32, int32, int32, char **);

  public:
    HDF4Dataset();
    ~HDF4Dataset() override;

    std::shared_ptr<GDALGroup> GetRootGroup() const override
    {
        return m_poRootGroup;
    }

    char **GetMetadataDomainList() override;
    CSLConstList GetMetadata(const char *pszDomain = "") override;
    static GDALDataset *Open(GDALOpenInfo *);
};

/************************************************************************/
/* ==================================================================== */
/*                              HDF4ImageDataset                        */
/* ==================================================================== */
/************************************************************************/

constexpr int N_COLOR_ENTRIES = 256;

class HDF4ImageDataset final : public HDF4Dataset
{
    friend class HDF4ImageRasterBand;

    char *pszFilename;
    int32 hHDF4;
    int32 iGR;
    int32 iPal;
    int32 iDataset;
    int32 iRank;
    int32 iNumType;
    int32 nAttrs;
    int32 iInterlaceMode;
    int32 iPalInterlaceMode;
    int32 iPalDataType;
    int32 nComps;
    int32 nPalEntries;
    int32 aiDimSizes[H4_MAX_VAR_DIMS];
    int iXDim;
    int iYDim;
    int iBandDim;
    int i4Dim;
    int nBandCount;
    char **papszLocalMetadata{};
    uint8 aiPaletteData[N_COLOR_ENTRIES][3];  // XXX: Static array for now
    char szName[HDF4_SDS_MAXNAMELEN];
    char *pszSubdatasetName;
    char *pszFieldName;

    GDALColorTable *poColorTable;

    OGRSpatialReference m_oSRS{};
    OGRSpatialReference m_oGCPSRS{};
    bool bHasGeoTransform;
    GDALGeoTransform m_gt{};
    std::vector<gdal::GCP> m_aoGCPs{};

    HDF4DatasetType iDatasetType;

    int32 iSDS;

    int nBlockPreferredXSize;
    int nBlockPreferredYSize;
    bool bReadTile;

    void ToGeoref(double *, double *);
    void GetImageDimensions(char *);
    void GetSwatAttrs(int32);
    void GetGridAttrs(int32 hGD);
    void CaptureNRLGeoTransform(void);
    void CaptureL1GMTLInfo(void);
    void CaptureCoastwatchGCTPInfo(void);
    void ProcessModisSDSGeolocation(void);
    int ProcessSwathGeolocation(int32, char **);

    static long USGSMnemonicToCode(const char *);
    static void ReadCoordinates(const char *, double *, double *);

    CPL_DISALLOW_COPY_ASSIGN(HDF4ImageDataset)

  public:
    HDF4ImageDataset();
    ~HDF4ImageDataset() override;

    static GDALDataset *Open(GDALOpenInfo *);
    static GDALDataset *Create(const char *pszFilename, int nXSize, int nYSize,
                               int nBandsIn, GDALDataType eType,
                               CSLConstList papszParamList);
    CPLErr FlushCache(bool bAtClosing) override;
    CPLErr GetGeoTransform(GDALGeoTransform &gt) const override;
    CPLErr SetGeoTransform(const GDALGeoTransform &gt) override;
    const OGRSpatialReference *GetSpatialRef() const override;
    CPLErr SetSpatialRef(const OGRSpatialReference *poSRS) override;
    int GetGCPCount() override;
    const OGRSpatialReference *GetGCPSpatialRef() const override;
    const GDAL_GCP *GetGCPs() override;
};

char *SPrintArray(GDALDataType eDataType, const void *paDataArray, int nValues,
                  const char *pszDelimiter);

#endif /* HDF4DATASET_H_INCLUDED_ */
