/******************************************************************************
 *
 * Project:  CSV Translator
 * Purpose:  Definition of classes for OGR .csv driver.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2004,  Frank Warmerdam
 * Copyright (c) 2008-2013, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef OGR_CSV_H_INCLUDED
#define OGR_CSV_H_INCLUDED

#include "ogrsf_frmts.h"

#include <set>

typedef enum
{
    OGR_CSV_GEOM_NONE,
    OGR_CSV_GEOM_AS_WKT,
    OGR_CSV_GEOM_AS_SOME_GEOM_FORMAT,
    OGR_CSV_GEOM_AS_XYZ,
    OGR_CSV_GEOM_AS_XY,
    OGR_CSV_GEOM_AS_YX,
} OGRCSVGeometryFormat;

class OGRCSVDataSource;

typedef enum
{
    CREATE_FIELD_DO_NOTHING,
    CREATE_FIELD_PROCEED,
    CREATE_FIELD_ERROR
} OGRCSVCreateFieldAction;

void OGRCSVDriverRemoveFromMap(const char *pszName, GDALDataset *poDS);

// Must be kept as a macro with the value fully resolved, as it is used
// by STRINGIFY(x) to generate open option description.
#define OGR_CSV_DEFAULT_MAX_LINE_SIZE 10000000

/************************************************************************/
/*                             OGRCSVLayer                              */
/************************************************************************/

class IOGRCSVLayer CPL_NON_FINAL
{
  public:
    IOGRCSVLayer() = default;
    virtual ~IOGRCSVLayer();

    virtual OGRLayer *GetLayer() = 0;

    virtual std::vector<std::string> GetFileList() = 0;
};

class OGRCSVLayer final : public IOGRCSVLayer, public OGRLayer
{
  public:
    enum class StringQuoting
    {
        IF_NEEDED,
        IF_AMBIGUOUS,
        ALWAYS
    };

  private:
    GDALDataset *m_poDS = nullptr;
    OGRFeatureDefn *poFeatureDefn = nullptr;
    std::set<CPLString> m_oSetFields{};

    VSILFILE *fpCSV = nullptr;
    const int m_nMaxLineSize = -1;

    static constexpr int64_t FID_INITIAL_VALUE = 1;
    int64_t m_nNextFID = FID_INITIAL_VALUE;

    bool bHasFieldNames = false;

    OGRFeature *GetNextUnfilteredFeature();

    bool bNew = false;
    bool bInWriteMode = false;
    bool bUseCRLF = false;
    bool bNeedRewindBeforeRead = false;
    OGRCSVGeometryFormat eGeometryFormat = OGR_CSV_GEOM_NONE;

    char *pszFilename = nullptr;
    std::string m_osCSVTFilename{};
    bool bCreateCSVT = false;
    bool bWriteBOM = false;
    char szDelimiter[2] = {0};

    int nCSVFieldCount = 0;
    int *panGeomFieldIndex = nullptr;
    bool bFirstFeatureAppendedDuringSession = true;
    bool bHiddenWKTColumn = false;

    // http://www.faa.gov/airports/airport_safety/airportdata_5010/menu/index.cfm
    // specific
    int iNfdcLongitudeS = -1;
    int iNfdcLatitudeS = 1;
    bool bHonourStrings = true;

    // https://www.usgs.gov/u.s.-board-on-geographic-names/download-gnis-data
    bool m_bIsGNIS = false;
    int iLongitudeField = -1;
    int iLatitudeField = -1;
    int iZField = -1;
    CPLString osXField{};
    CPLString osYField{};
    CPLString osZField{};

    bool bIsEurostatTSV = false;
    int nEurostatDims = 0;

    GIntBig nTotalFeatures = 0;

    char **AutodetectFieldTypes(CSLConstList papszOpenOptions, int nFieldCount);

    bool bWarningBadTypeOrWidth = false;
    bool bKeepSourceColumns = false;
    bool bKeepGeomColumns = true;

    bool bMergeDelimiter = false;

    bool bEmptyStringNull = false;

    bool m_bWriteHeader = true;

    StringQuoting m_eStringQuoting = StringQuoting::IF_AMBIGUOUS;

    char **GetNextLineTokens();

    static bool Matches(const char *pszFieldName, char **papszPossibleNames);

    CPL_DISALLOW_COPY_ASSIGN(OGRCSVLayer)

  public:
    OGRCSVLayer(GDALDataset *poDS, const char *pszName, VSILFILE *fp,
                int nMaxLineSize, const char *pszFilename, int bNew,
                int bInWriteMode, char chDelimiter);
    ~OGRCSVLayer() override;

    OGRLayer *GetLayer() override
    {
        return this;
    }

    const char *GetFilename() const
    {
        return pszFilename;
    }

    std::vector<std::string> GetFileList() override;

    char GetDelimiter() const
    {
        return szDelimiter[0];
    }

    bool GetCRLF() const
    {
        return bUseCRLF;
    }

    bool GetCreateCSVT() const
    {
        return bCreateCSVT;
    }

    bool GetWriteBOM() const
    {
        return bWriteBOM;
    }

    OGRCSVGeometryFormat GetGeometryFormat() const
    {
        return eGeometryFormat;
    }

    bool HasHiddenWKTColumn() const
    {
        return bHiddenWKTColumn;
    }

    GIntBig GetTotalFeatureCount() const
    {
        return nTotalFeatures;
    }

    const CPLString &GetXField() const
    {
        return osXField;
    }

    const CPLString &GetYField() const
    {
        return osYField;
    }

    const CPLString &GetZField() const
    {
        return osZField;
    }

    void BuildFeatureDefn(const char *pszNfdcGeomField = nullptr,
                          const char *pszGeonamesGeomFieldPrefix = nullptr,
                          CSLConstList papszOpenOptions = nullptr);

    void ResetReading() override;
    OGRFeature *GetNextFeature() override;
    OGRFeature *GetFeature(GIntBig nFID) override;

    using OGRLayer::GetLayerDefn;

    const OGRFeatureDefn *GetLayerDefn() const override
    {
        return poFeatureDefn;
    }

    int TestCapability(const char *) const override;

    virtual OGRErr CreateField(const OGRFieldDefn *poField,
                               int bApproxOK = TRUE) override;

    static OGRCSVCreateFieldAction
    PreCreateField(OGRFeatureDefn *poFeatureDefn,
                   const std::set<CPLString> &oSetFields,
                   const OGRFieldDefn *poNewField, int bApproxOK);
    virtual OGRErr CreateGeomField(const OGRGeomFieldDefn *poGeomField,
                                   int bApproxOK = TRUE) override;

    OGRErr ICreateFeature(OGRFeature *poFeature) override;

    void SetCRLF(bool bNewValue);
    void SetWriteGeometry(OGRwkbGeometryType eGType,
                          OGRCSVGeometryFormat eGeometryFormat,
                          const char *pszGeomCol = nullptr);
    void SetCreateCSVT(bool bCreateCSVT);
    void SetWriteBOM(bool bWriteBOM);

    void SetWriteHeader(bool b)
    {
        m_bWriteHeader = b;
    }

    void SetStringQuoting(StringQuoting eVal)
    {
        m_eStringQuoting = eVal;
    }

    StringQuoting GetStringQuoting() const
    {
        return m_eStringQuoting;
    }

    GIntBig GetFeatureCount(int bForce = TRUE) override;
    OGRErr SyncToDisk() override;

    GDALDataset *GetDataset() override
    {
        return m_poDS;
    }

    OGRErr WriteHeader();
};

/************************************************************************/
/*                           OGRCSVDataSource                           */
/************************************************************************/

class OGRCSVDataSource final : public GDALDataset
{
    char *pszName = nullptr;

    std::vector<std::unique_ptr<IOGRCSVLayer>> m_apoLayers{};

    bool bUpdate = false;

    CPLString osDefaultCSVName{};

    bool bEnableGeometryFields = false;

    bool DealWithOgrSchemaOpenOption(CSLConstList papszOpenOptions);

    /* When OGR_SCHEMA and schemaType=Full, this will contain the list
     * of removed field (if any).
     */
    std::vector<int> m_oDeletedFieldIndexes{};

    CPL_DISALLOW_COPY_ASSIGN(OGRCSVDataSource)

  public:
    OGRCSVDataSource();
    ~OGRCSVDataSource() override;

    bool Open(const char *pszFilename, bool bUpdate, bool bForceOpen,
              CSLConstList papszOpenOptions, bool bSingleDriver);
    bool OpenTable(const char *pszFilename, CSLConstList papszOpenOptions,
                   const char *pszNfdcRunwaysGeomField = nullptr,
                   const char *pszGeonamesGeomFieldPrefix = nullptr);

    int GetLayerCount() const override
    {
        return static_cast<int>(m_apoLayers.size());
    }

    using GDALDataset::GetLayer;
    const OGRLayer *GetLayer(int) const override;

    char **GetFileList() override;

    virtual OGRLayer *ICreateLayer(const char *pszName,
                                   const OGRGeomFieldDefn *poGeomFieldDefn,
                                   CSLConstList papszOptions) override;

    OGRErr DeleteLayer(int) override;

    int TestCapability(const char *) const override;

    void CreateForSingleFile(const char *pszDirname, const char *pszFilename);

    void EnableGeometryFields()
    {
        bEnableGeometryFields = true;
    }

    static CPLString GetRealExtension(CPLString osFilename);
    const std::vector<int> &DeletedFieldIndexes() const;
};

#endif  // ndef OGR_CSV_H_INCLUDED
