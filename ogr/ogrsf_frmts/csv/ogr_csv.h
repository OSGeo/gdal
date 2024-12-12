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
    virtual ~IOGRCSVLayer() = default;

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
    OGRFeatureDefn *poFeatureDefn;
    std::set<CPLString> m_oSetFields;

    VSILFILE *fpCSV;
    const int m_nMaxLineSize = -1;

    int nNextFID;

    bool bHasFieldNames;

    OGRFeature *GetNextUnfilteredFeature();

    bool bNew;
    bool bInWriteMode;
    bool bUseCRLF;
    bool bNeedRewindBeforeRead;
    OGRCSVGeometryFormat eGeometryFormat;

    char *pszFilename;
    std::string m_osCSVTFilename{};
    bool bCreateCSVT;
    bool bWriteBOM;
    char szDelimiter[2] = {0};

    int nCSVFieldCount;
    int *panGeomFieldIndex;
    bool bFirstFeatureAppendedDuringSession;
    bool bHiddenWKTColumn;

    // http://www.faa.gov/airports/airport_safety/airportdata_5010/menu/index.cfm
    // specific
    int iNfdcLongitudeS;
    int iNfdcLatitudeS;
    bool bHonourStrings;

    bool m_bIsGNIS =
        false;  // https://www.usgs.gov/u.s.-board-on-geographic-names/download-gnis-data
    int iLongitudeField;
    int iLatitudeField;
    int iZField;
    CPLString osXField;
    CPLString osYField;
    CPLString osZField;

    bool bIsEurostatTSV;
    int nEurostatDims;

    GIntBig nTotalFeatures;

    char **AutodetectFieldTypes(CSLConstList papszOpenOptions, int nFieldCount);

    bool bWarningBadTypeOrWidth;
    bool bKeepSourceColumns;
    bool bKeepGeomColumns;

    bool bMergeDelimiter;

    bool bEmptyStringNull;

    StringQuoting m_eStringQuoting = StringQuoting::IF_AMBIGUOUS;

    char **GetNextLineTokens();

    static bool Matches(const char *pszFieldName, char **papszPossibleNames);

  public:
    OGRCSVLayer(GDALDataset *poDS, const char *pszName, VSILFILE *fp,
                int nMaxLineSize, const char *pszFilename, int bNew,
                int bInWriteMode, char chDelimiter);
    virtual ~OGRCSVLayer() override;

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
    virtual OGRFeature *GetFeature(GIntBig nFID) override;

    OGRFeatureDefn *GetLayerDefn() override
    {
        return poFeatureDefn;
    }

    int TestCapability(const char *) override;

    virtual OGRErr CreateField(const OGRFieldDefn *poField,
                               int bApproxOK = TRUE) override;

    static OGRCSVCreateFieldAction
    PreCreateField(OGRFeatureDefn *poFeatureDefn,
                   const std::set<CPLString> &oSetFields,
                   const OGRFieldDefn *poNewField, int bApproxOK);
    virtual OGRErr CreateGeomField(const OGRGeomFieldDefn *poGeomField,
                                   int bApproxOK = TRUE) override;

    virtual OGRErr ICreateFeature(OGRFeature *poFeature) override;

    void SetCRLF(bool bNewValue);
    void SetWriteGeometry(OGRwkbGeometryType eGType,
                          OGRCSVGeometryFormat eGeometryFormat,
                          const char *pszGeomCol = nullptr);
    void SetCreateCSVT(bool bCreateCSVT);
    void SetWriteBOM(bool bWriteBOM);

    void SetStringQuoting(StringQuoting eVal)
    {
        m_eStringQuoting = eVal;
    }

    StringQuoting GetStringQuoting() const
    {
        return m_eStringQuoting;
    }

    virtual GIntBig GetFeatureCount(int bForce = TRUE) override;
    virtual OGRErr SyncToDisk() override;

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
    std::vector<int> m_oDeletedFieldIndexes;

  public:
    OGRCSVDataSource();
    virtual ~OGRCSVDataSource() override;

    bool Open(const char *pszFilename, bool bUpdate, bool bForceOpen,
              CSLConstList papszOpenOptions, bool bSingleDriver);
    bool OpenTable(const char *pszFilename, CSLConstList papszOpenOptions,
                   const char *pszNfdcRunwaysGeomField = nullptr,
                   const char *pszGeonamesGeomFieldPrefix = nullptr);

    int GetLayerCount() override
    {
        return static_cast<int>(m_apoLayers.size());
    }

    OGRLayer *GetLayer(int) override;

    char **GetFileList() override;

    virtual OGRLayer *ICreateLayer(const char *pszName,
                                   const OGRGeomFieldDefn *poGeomFieldDefn,
                                   CSLConstList papszOptions) override;

    virtual OGRErr DeleteLayer(int) override;

    int TestCapability(const char *) override;

    void CreateForSingleFile(const char *pszDirname, const char *pszFilename);

    void EnableGeometryFields()
    {
        bEnableGeometryFields = true;
    }

    static CPLString GetRealExtension(CPLString osFilename);
    const std::vector<int> &DeletedFieldIndexes() const;
};

#endif  // ndef OGR_CSV_H_INCLUDED
