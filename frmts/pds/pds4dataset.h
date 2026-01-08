/******************************************************************************
 *
 * Project:  PDS 4 Driver; Planetary Data System Format
 * Purpose:  Implementation of PDS4Dataset
 * Author:   Even Rouault, even.rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2019, Hobu Inc
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#pragma once

#include "cpl_string.h"
#include "gdal_priv.h"
#include "gdal_proxy.h"
#include "ogreditablelayer.h"
#include "rawdataset.h"
#include "ogr_spatialref.h"

#include <array>
#include <vector>

class PDS4Dataset;

/************************************************************************/
/* ==================================================================== */
/*                        PDS4TableBaseLayer                            */
/* ==================================================================== */
/************************************************************************/

class PDS4TableBaseLayer CPL_NON_FINAL : public OGRLayer
{
  protected:
    PDS4Dataset *m_poDS = nullptr;
    OGRFeatureDefn *m_poRawFeatureDefn = nullptr;
    OGRFeatureDefn *m_poFeatureDefn = nullptr;
    CPLString m_osFilename{};
    int m_iLatField = -1;
    int m_iLongField = -1;
    int m_iAltField = -1;
    int m_iWKT = -1;
    bool m_bKeepGeomColmuns = false;
    bool m_bDirtyHeader = false;
    VSILFILE *m_fp = nullptr;
    GIntBig m_nFeatureCount = -1;
    GIntBig m_nFID = 1;
    vsi_l_offset m_nOffset = 0;
    CPLStringList m_aosLCO{};
    std::string m_osLineEnding{};

    void SetupGeomField();
    OGRFeature *AddGeometryFromFields(OGRFeature *poFeature);
    OGRFeature *AddFieldsFromGeometry(OGRFeature *poFeature);
    void MarkHeaderDirty();
    CPLXMLNode *RefreshFileAreaObservationalBeginningCommon(
        CPLXMLNode *psFAO, const CPLString &osPrefix,
        const char *pszTableEltName, CPLString &osDescription);
    void ParseLineEndingOption(CSLConstList papszOptions);

    CPL_DISALLOW_COPY_ASSIGN(PDS4TableBaseLayer)

  public:
    PDS4TableBaseLayer(PDS4Dataset *poDS, const char *pszName,
                       const char *pszFilename);
    ~PDS4TableBaseLayer() override;

    using OGRLayer::GetLayerDefn;

    const OGRFeatureDefn *GetLayerDefn() const override
    {
        return m_poFeatureDefn;
    }

    GIntBig GetFeatureCount(int bForce) override;

    const char *GetFileName() const
    {
        return m_osFilename.c_str();
    }

    bool IsDirtyHeader() const
    {
        return m_bDirtyHeader;
    }

    int GetRawFieldCount() const
    {
        return m_poRawFeatureDefn->GetFieldCount();
    }

    bool RenameFileTo(const char *pszNewName);
    virtual char **GetFileList() const;

    virtual void RefreshFileAreaObservational(CPLXMLNode *psFAO) = 0;

    GDALDataset *GetDataset() override;
};

/************************************************************************/
/* ==================================================================== */
/*                        PDS4FixedWidthTable                           */
/* ==================================================================== */
/************************************************************************/

template <class T> class PDS4EditableSynchronizer;

class PDS4FixedWidthTable CPL_NON_FINAL : public PDS4TableBaseLayer
{
    friend class PDS4EditableSynchronizer<PDS4FixedWidthTable>;

  protected:
    int m_nRecordSize = 0;
    CPLString m_osBuffer{};

    struct Field
    {
        int m_nOffset = 0;  // in XML 1-based, here 0-based
        int m_nLength = 0;
        CPLString m_osDataType{};
        CPLString m_osUnit{};
        CPLString m_osDescription{};
        CPLString m_osSpecialConstantsXML{};
    };

    std::vector<Field> m_aoFields{};

    virtual CPLString GetSubType() const = 0;

    virtual bool CreateFieldInternal(OGRFieldType eType,
                                     OGRFieldSubType eSubType, int nWidth,
                                     Field &f) = 0;

    bool ReadFields(const CPLXMLNode *psParent, int nBaseOffset,
                    const CPLString &osSuffixFieldName);

  public:
    PDS4FixedWidthTable(PDS4Dataset *poDS, const char *pszName,
                        const char *pszFilename);

    void ResetReading() override;
    OGRFeature *GetFeature(GIntBig nFID) override;
    OGRFeature *GetNextFeature() override;
    int TestCapability(const char *) const override;
    OGRErr ISetFeature(OGRFeature *poFeature) override;
    OGRErr ICreateFeature(OGRFeature *poFeature) override;
    OGRErr CreateField(const OGRFieldDefn *poFieldIn, int) override;

    bool ReadTableDef(const CPLXMLNode *psTable);

    bool InitializeNewLayer(const OGRSpatialReference *poSRS,
                            bool bForceGeographic, OGRwkbGeometryType eGType,
                            const char *const *papszOptions);

    virtual PDS4FixedWidthTable *NewLayer(PDS4Dataset *poDS,
                                          const char *pszName,
                                          const char *pszFilename) = 0;

    void RefreshFileAreaObservational(CPLXMLNode *psFAO) override;
};

/************************************************************************/
/* ==================================================================== */
/*                        PDS4TableCharacter                            */
/* ==================================================================== */
/************************************************************************/

class PDS4TableCharacter final : public PDS4FixedWidthTable
{
    CPLString GetSubType() const override
    {
        return "Character";
    }

    bool CreateFieldInternal(OGRFieldType eType, OGRFieldSubType eSubType,
                             int nWidth, Field &f) override;

  public:
    PDS4TableCharacter(PDS4Dataset *poDS, const char *pszName,
                       const char *pszFilename);

    PDS4FixedWidthTable *NewLayer(PDS4Dataset *poDS, const char *pszName,
                                  const char *pszFilename) override
    {
        return new PDS4TableCharacter(poDS, pszName, pszFilename);
    }
};

/************************************************************************/
/* ==================================================================== */
/*                        PDS4TableBinary                               */
/* ==================================================================== */
/************************************************************************/

class PDS4TableBinary final : public PDS4FixedWidthTable
{
    CPLString GetSubType() const override
    {
        return "Binary";
    }

    bool CreateFieldInternal(OGRFieldType eType, OGRFieldSubType eSubType,
                             int nWidth, Field &f) override;

  public:
    PDS4TableBinary(PDS4Dataset *poDS, const char *pszName,
                    const char *pszFilename);

    PDS4FixedWidthTable *NewLayer(PDS4Dataset *poDS, const char *pszName,
                                  const char *pszFilename) override
    {
        return new PDS4TableBinary(poDS, pszName, pszFilename);
    }
};

/************************************************************************/
/* ==================================================================== */
/*                        PDS4DelimitedTable                            */
/* ==================================================================== */
/************************************************************************/

class PDS4DelimitedTable CPL_NON_FINAL : public PDS4TableBaseLayer
{
    friend class PDS4EditableSynchronizer<PDS4DelimitedTable>;

  protected:
    bool m_bCreation = false;
    char m_chFieldDelimiter = ',';
    bool m_bAddWKTColumnPending = false;

    struct Field
    {
        CPLString m_osDataType{};
        CPLString m_osUnit{};
        CPLString m_osDescription{};
        CPLString m_osSpecialConstantsXML{};
        CPLString m_osMissingConstant{};  // included in above potentially
    };

    std::vector<Field> m_aoFields{};

    OGRFeature *GetNextFeatureRaw();
    CPLString QuoteIfNeeded(const char *pszVal);
    void GenerateVRT();

    bool ReadFields(const CPLXMLNode *psParent,
                    const CPLString &osSuffixFieldName);

  public:
    PDS4DelimitedTable(PDS4Dataset *poDS, const char *pszName,
                       const char *pszFilename);
    ~PDS4DelimitedTable() override;

    void ResetReading() override;
    OGRFeature *GetNextFeature() override;
    int TestCapability(const char *) const override;
    OGRErr ICreateFeature(OGRFeature *poFeature) override;
    OGRErr CreateField(const OGRFieldDefn *poFieldIn, int) override;

    bool ReadTableDef(const CPLXMLNode *psTable);

    bool InitializeNewLayer(const OGRSpatialReference *poSRS,
                            bool bForceGeographic, OGRwkbGeometryType eGType,
                            const char *const *papszOptions);

    void RefreshFileAreaObservational(CPLXMLNode *psFAO) override;
    char **GetFileList() const override;

    PDS4DelimitedTable *NewLayer(PDS4Dataset *poDS, const char *pszName,
                                 const char *pszFilename)
    {
        return new PDS4DelimitedTable(poDS, pszName, pszFilename);
    }
};

/************************************************************************/
/* ==================================================================== */
/*                         PDS4EditableLayer                            */
/* ==================================================================== */
/************************************************************************/

class PDS4EditableLayer final : public OGREditableLayer
{
    PDS4TableBaseLayer *GetBaseLayer() const;

  public:
    explicit PDS4EditableLayer(
        std::unique_ptr<PDS4FixedWidthTable> poBaseLayer);
    explicit PDS4EditableLayer(std::unique_ptr<PDS4DelimitedTable> poBaseLayer);
    ~PDS4EditableLayer() override;

    void RefreshFileAreaObservational(CPLXMLNode *psFAO)
    {
        GetBaseLayer()->RefreshFileAreaObservational(psFAO);
    }

    const char *GetFileName() const
    {
        return GetBaseLayer()->GetFileName();
    }

    bool IsDirtyHeader() const
    {
        return GetBaseLayer()->IsDirtyHeader();
    }

    int GetRawFieldCount() const
    {
        return GetBaseLayer()->GetRawFieldCount();
    }

    void SetSpatialRef(OGRSpatialReference *poSRS);

    char **GetFileList() const
    {
        return GetBaseLayer()->GetFileList();
    }
};

/************************************************************************/
/*                            PDS4Dataset                               */
/************************************************************************/

class PDS4Dataset final : public RawDataset
{
    friend class PDS4RawRasterBand;
    friend class PDS4WrapperRasterBand;

    VSILFILE *m_fpImage = nullptr;
    vsi_l_offset m_nBaseOffset = 0;
    GDALDataset *m_poExternalDS = nullptr;  // external dataset (GeoTIFF)
    OGRSpatialReference m_oSRS{};
    bool m_bGotTransform = false;
    GDALGeoTransform m_gt{};
    CPLString m_osXMLFilename{};
    CPLString m_osImageFilename{};
    CPLString m_osUnits{};
    bool m_bCreatedFromExistingBinaryFile = false;

    std::vector<std::unique_ptr<PDS4EditableLayer>> m_apoLayers{};

    // Write dedicated parameters
    bool m_bMustInitImageFile = false;
    bool m_bUseSrcLabel = true;
    bool m_bDirtyHeader = false;
    bool m_bCreateHeader = false;
    bool m_bStripFileAreaObservationalFromTemplate = false;
    bool m_bIsLSB = true;
    CPLString m_osHeaderParsingStandard{};
    CPLString m_osInterleave{};
    char **m_papszCreationOptions = nullptr;
    CPLString m_osXMLPDS4{};

    void CreateHeader(CPLXMLNode *psProduct, const char *pszCARTVersion);
    void WriteHeader();
    void WriteHeaderAppendCase();
    void WriteVectorLayers(CPLXMLNode *psProduct);
    void WriteArray(const CPLString &osPrefix, CPLXMLNode *psFAO,
                    const char *pszLocalIdentifier,
                    CPLXMLNode *psTemplateSpecialConstants);
    void WriteGeoreferencing(CPLXMLNode *psCart, const char *pszCARTVersion);
    void ReadGeoreferencing(CPLXMLNode *psProduct);
    bool InitImageFile();

    void SubstituteVariables(CPLXMLNode *psNode, char **papszDict);

    bool OpenTableCharacter(const char *pszFilename, const CPLXMLNode *psTable);

    bool OpenTableBinary(const char *pszFilename, const CPLXMLNode *psTable);

    bool OpenTableDelimited(const char *pszFilename, const CPLXMLNode *psTable);

    static std::unique_ptr<PDS4Dataset>
    CreateInternal(const char *pszFilename, GDALDataset *poSrcDS, int nXSize,
                   int nYSize, int nBands, GDALDataType eType,
                   const char *const *papszOptions);

    CPLErr Close(GDALProgressFunc = nullptr, void * = nullptr) override;

    CPL_DISALLOW_COPY_ASSIGN(PDS4Dataset)

  public:
    PDS4Dataset();
    ~PDS4Dataset() override;

    int CloseDependentDatasets() override;

    const OGRSpatialReference *GetSpatialRef() const override;
    CPLErr SetSpatialRef(const OGRSpatialReference *poSRS) override;
    CPLErr GetGeoTransform(GDALGeoTransform &gt) const override;
    CPLErr SetGeoTransform(const GDALGeoTransform &gt) override;
    char **GetFileList() override;
    CPLErr SetMetadata(CSLConstList papszMD,
                       const char *pszDomain = "") override;

    int GetLayerCount() const override
    {
        return static_cast<int>(m_apoLayers.size());
    }

    using GDALDataset::GetLayer;
    const OGRLayer *GetLayer(int) const override;

    OGRLayer *ICreateLayer(const char *pszName,
                           const OGRGeomFieldDefn *poGeomFieldDefn,
                           CSLConstList papszOptions) override;

    int TestCapability(const char *pszCap) const override;

    bool GetRawBinaryLayout(GDALDataset::RawBinaryLayout &) override;

    static std::unique_ptr<PDS4Dataset> OpenInternal(GDALOpenInfo *);

    static GDALDataset *Open(GDALOpenInfo *poOpenInfo)
    {
        return OpenInternal(poOpenInfo).release();
    }

    static GDALDataset *Create(const char *pszFilename, int nXSize, int nYSize,
                               int nBands, GDALDataType eType,
                               char **papszOptions);
    static GDALDataset *CreateCopy(const char *pszFilename,
                                   GDALDataset *poSrcDS, int bStrict,
                                   char **papszOptions,
                                   GDALProgressFunc pfnProgress,
                                   void *pProgressData);
    static CPLErr Delete(const char *pszName);

    const char *const *GetOpenOptions() const
    {
        return papszOpenOptions;
    }

    void MarkHeaderDirty()
    {
        m_bDirtyHeader = true;
    }
};

/************************************************************************/
/* ==================================================================== */
/*                        PDS4RawRasterBand                            */
/* ==================================================================== */
/************************************************************************/

class PDS4RawRasterBand final : public RawRasterBand
{
    friend class PDS4Dataset;

    bool m_bHasOffset{};
    bool m_bHasScale{};
    bool m_bHasNoData{};
    bool m_bHasNoDataInt64{};
    bool m_bHasNoDataUInt64{};
    double m_dfOffset{};
    double m_dfScale{};
    double m_dfNoData{};
    int64_t m_nNoDataInt64{};
    uint64_t m_nNoDataUInt64{};

  public:
    PDS4RawRasterBand(GDALDataset *l_poDS, int l_nBand, VSILFILE *l_fpRaw,
                      vsi_l_offset l_nImgOffset, int l_nPixelOffset,
                      int l_nLineOffset, GDALDataType l_eDataType,
                      RawRasterBand::ByteOrder eByteOrderIn);

    CPLErr IWriteBlock(int, int, void *) override;

    CPLErr IRasterIO(GDALRWFlag, int, int, int, int, void *, int, int,
                     GDALDataType, GSpacing nPixelSpace, GSpacing nLineSpace,
                     GDALRasterIOExtraArg *psExtraArg) override;

    double GetOffset(int *pbSuccess = nullptr) override;
    double GetScale(int *pbSuccess = nullptr) override;
    CPLErr SetOffset(double dfNewOffset) override;
    CPLErr SetScale(double dfNewScale) override;
    double GetNoDataValue(int *pbSuccess = nullptr) override;
    CPLErr SetNoDataValue(double dfNewNoData) override;
    int64_t GetNoDataValueAsInt64(int *pbSuccess = nullptr) override;
    uint64_t GetNoDataValueAsUInt64(int *pbSuccess = nullptr) override;
    CPLErr SetNoDataValueAsInt64(int64_t nNoData) override;
    CPLErr SetNoDataValueAsUInt64(uint64_t nNoData) override;

    const char *GetUnitType() override
    {
        return static_cast<PDS4Dataset *>(poDS)->m_osUnits.c_str();
    }

    CPLErr SetUnitType(const char *pszUnits) override
    {
        static_cast<PDS4Dataset *>(poDS)->m_osUnits = pszUnits;
        return CE_None;
    }

    void SetMaskBand(std::unique_ptr<GDALRasterBand> poMaskBand);
};

/************************************************************************/
/* ==================================================================== */
/*                         PDS4WrapperRasterBand                       */
/*                                                                      */
/*      proxy for bands stored in other formats.                        */
/* ==================================================================== */
/************************************************************************/
class PDS4WrapperRasterBand final : public GDALProxyRasterBand
{
    friend class PDS4Dataset;

    GDALRasterBand *m_poBaseBand{};
    bool m_bHasOffset{};
    bool m_bHasScale{};
    bool m_bHasNoData{};
    bool m_bHasNoDataInt64{};
    bool m_bHasNoDataUInt64{};
    double m_dfOffset{};
    double m_dfScale{};
    double m_dfNoData{};
    int64_t m_nNoDataInt64{};
    uint64_t m_nNoDataUInt64{};

    CPL_DISALLOW_COPY_ASSIGN(PDS4WrapperRasterBand)

  protected:
    virtual GDALRasterBand *
    RefUnderlyingRasterBand(bool /*bForceOpen*/) const override
    {
        return m_poBaseBand;
    }

  public:
    explicit PDS4WrapperRasterBand(GDALRasterBand *poBaseBandIn);

    virtual CPLErr Fill(double dfRealValue,
                        double dfImaginaryValue = 0) override;
    CPLErr IWriteBlock(int, int, void *) override;

    CPLErr IRasterIO(GDALRWFlag, int, int, int, int, void *, int, int,
                     GDALDataType, GSpacing nPixelSpace, GSpacing nLineSpace,
                     GDALRasterIOExtraArg *psExtraArg) override;

    double GetOffset(int *pbSuccess = nullptr) override;
    double GetScale(int *pbSuccess = nullptr) override;
    CPLErr SetOffset(double dfNewOffset) override;
    CPLErr SetScale(double dfNewScale) override;
    double GetNoDataValue(int *pbSuccess = nullptr) override;
    CPLErr SetNoDataValue(double dfNewNoData) override;
    int64_t GetNoDataValueAsInt64(int *pbSuccess = nullptr) override;
    uint64_t GetNoDataValueAsUInt64(int *pbSuccess = nullptr) override;
    CPLErr SetNoDataValueAsInt64(int64_t nNoData) override;
    CPLErr SetNoDataValueAsUInt64(uint64_t nNoData) override;

    const char *GetUnitType() override
    {
        return static_cast<PDS4Dataset *>(poDS)->m_osUnits.c_str();
    }

    CPLErr SetUnitType(const char *pszUnits) override
    {
        static_cast<PDS4Dataset *>(poDS)->m_osUnits = pszUnits;
        return CE_None;
    }

    int GetMaskFlags() override
    {
        return nMaskFlags;
    }

    GDALRasterBand *GetMaskBand() override
    {
        return poMask;
    }

    void SetMaskBand(std::unique_ptr<GDALRasterBand> poMaskBand);
};

/************************************************************************/
/* ==================================================================== */
/*                             PDS4MaskBand                             */
/* ==================================================================== */

class PDS4MaskBand final : public GDALRasterBand
{
    GDALRasterBand *m_poBaseBand{};
    void *m_pBuffer{};
    std::vector<double> m_adfConstants{};

    CPL_DISALLOW_COPY_ASSIGN(PDS4MaskBand)

  public:
    PDS4MaskBand(GDALRasterBand *poBaseBand,
                 const std::vector<double> &adfConstants);
    ~PDS4MaskBand() override;

    CPLErr IReadBlock(int, int, void *) override;
};
