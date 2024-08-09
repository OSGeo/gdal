/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Private definitions for OGR/PostgreSQL dump driver.
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2010-2013, Even Rouault <even dot rouault at spatialys.com>
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

#ifndef OGR_PGDUMP_H_INCLUDED
#define OGR_PGDUMP_H_INCLUDED

#include "ogrsf_frmts.h"
#include "cpl_string.h"

#include <vector>

// Cf https://www.postgresql.org/docs/current/sql-syntax-lexical.html#SQL-SYNTAX-IDENTIFIERS
constexpr int OGR_PG_NAMEDATALEN = 64;

CPLString OGRPGDumpEscapeColumnName(const char *pszColumnName);
CPLString OGRPGDumpEscapeString(const char *pszStrValue, int nMaxLength = -1,
                                const char *pszFieldName = "");
char CPL_DLL *OGRPGCommonGByteArrayToBYTEA(const GByte *pabyData, size_t nLen);
CPLString CPL_DLL OGRPGCommonLayerGetType(const OGRFieldDefn &oField,
                                          bool bPreservePrecision,
                                          bool bApproxOK);
bool CPL_DLL OGRPGCommonLayerSetType(OGRFieldDefn &oField, const char *pszType,
                                     const char *pszFormatType, int nWidth);
void CPL_DLL OGRPGCommonLayerNormalizeDefault(OGRFieldDefn *poFieldDefn,
                                              const char *pszDefault);
CPLString CPL_DLL OGRPGCommonLayerGetPGDefault(OGRFieldDefn *poFieldDefn);

void CPL_DLL OGRPGCommonAppendCopyFID(CPLString &osCommand,
                                      OGRFeature *poFeature);

typedef CPLString (*OGRPGCommonEscapeStringCbk)(void *userdata,
                                                const char *pszValue,
                                                int nWidth,
                                                const char *pszLayerName,
                                                const char *pszFieldRef);
void CPL_DLL OGRPGCommonAppendCopyRegularFields(
    CPLString &osCommand, OGRFeature *poFeature, const char *pszFIDColumn,
    const std::vector<bool> &abFieldsToInclude,
    OGRPGCommonEscapeStringCbk pfnEscapeString, void *userdata);

void CPL_DLL OGRPGCommonAppendFieldValue(
    CPLString &osCommand, OGRFeature *poFeature, int i,
    OGRPGCommonEscapeStringCbk pfnEscapeString, void *userdata);

char CPL_DLL *OGRPGCommonLaunderName(const char *pszSrcName,
                                     const char *pszDebugPrefix,
                                     bool bUTF8ToASCII);

std::string CPL_DLL
OGRPGCommonGenerateShortEnoughIdentifier(const char *pszIdentifier);

std::string CPL_DLL OGRPGCommonGenerateSpatialIndexName(
    const char *pszTableName, const char *pszGeomFieldName, int nGeomFieldIdx);

/************************************************************************/
/*                        OGRPGDumpGeomFieldDefn                        */
/************************************************************************/

class OGRPGDumpGeomFieldDefn final : public OGRGeomFieldDefn
{
    OGRPGDumpGeomFieldDefn(const OGRPGDumpGeomFieldDefn &) = delete;
    OGRPGDumpGeomFieldDefn &operator=(const OGRPGDumpGeomFieldDefn &) = delete;

  public:
    explicit OGRPGDumpGeomFieldDefn(OGRGeomFieldDefn *poGeomField)
        : OGRGeomFieldDefn(poGeomField), m_nSRSId(-1), m_nGeometryTypeFlags(0)
    {
    }

    int m_nSRSId;
    int m_nGeometryTypeFlags;
};

/************************************************************************/
/*                          OGRPGDumpLayer                              */
/************************************************************************/

class OGRPGDumpDataSource;

class OGRPGDumpLayer final : public OGRLayer
{
    OGRPGDumpLayer(const OGRPGDumpLayer &) = delete;
    OGRPGDumpLayer &operator=(const OGRPGDumpLayer &) = delete;

    static constexpr int USE_COPY_UNSET = -1;

    char *m_pszSchemaName = nullptr;
    char *m_pszSqlTableName = nullptr;
    CPLString m_osForcedDescription{};
    char *m_pszFIDColumn = nullptr;
    OGRFeatureDefn *m_poFeatureDefn = nullptr;
    OGRPGDumpDataSource *m_poDS = nullptr;
    bool m_bLaunderColumnNames = true;
    bool m_bUTF8ToASCII = false;
    bool m_bPreservePrecision = true;
    int m_bUseCopy = USE_COPY_UNSET;
    bool m_bWriteAsHex = false;
    bool m_bCopyActive = false;
    bool m_bFIDColumnInCopyFields = false;
    int m_bCreateTable = false;
    int m_nUnknownSRSId = -1;
    int m_nForcedSRSId = -1;
    int m_nForcedGeometryTypeFlags = -2;
    bool m_bCreateSpatialIndexFlag = false;
    CPLString m_osSpatialIndexType{};
    int m_nPostGISMajor = 0;
    int m_nPostGISMinor = 0;

    GIntBig m_iNextShapeId = 0;
    int m_iFIDAsRegularColumnIndex = -1;
    bool m_bAutoFIDOnCreateViaCopy = true;
    bool m_bCopyStatementWithFID = true;
    bool m_bNeedToUpdateSequence = false;
    bool m_bGeomColumnPositionImmediate = true;
    std::vector<std::string> m_aosDeferredGeomFieldCreationCommands{};
    std::vector<std::string> m_aosDeferrentNonGeomFieldCreationCommands{};
    std::vector<std::string> m_aosSpatialIndexCreationCommands{};

    CPLStringList m_apszOverrideColumnTypes{};

    CPLString m_osFirstGeometryFieldName{};

    OGRErr StartCopy(int bSetFID);
    CPLString BuildCopyFields(int bSetFID);

    void UpdateSequenceIfNeeded();

    void LogDeferredFieldCreationIfNeeded();

  public:
    OGRPGDumpLayer(OGRPGDumpDataSource *poDS, const char *pszSchemaName,
                   const char *pszLayerName, const char *pszFIDColumn,
                   int bWriteAsHexIn, int bCreateTable);
    virtual ~OGRPGDumpLayer();

    virtual OGRFeatureDefn *GetLayerDefn() override
    {
        return m_poFeatureDefn;
    }

    virtual const char *GetFIDColumn() override
    {
        return m_pszFIDColumn ? m_pszFIDColumn : "";
    }

    virtual void ResetReading() override
    {
    }

    virtual int TestCapability(const char *) override;

    virtual OGRErr ICreateFeature(OGRFeature *poFeature) override;
    virtual OGRErr CreateFeatureViaInsert(OGRFeature *poFeature);
    virtual OGRErr CreateFeatureViaCopy(OGRFeature *poFeature);

    virtual OGRErr CreateField(const OGRFieldDefn *poField,
                               int bApproxOK = TRUE) override;
    virtual OGRErr CreateGeomField(const OGRGeomFieldDefn *poGeomField,
                                   int bApproxOK = TRUE) override;

    virtual OGRFeature *GetNextFeature() override;

    virtual CPLErr SetMetadata(char **papszMD,
                               const char *pszDomain = "") override;
    virtual CPLErr SetMetadataItem(const char *pszName, const char *pszValue,
                                   const char *pszDomain = "") override;

    GDALDataset *GetDataset() override;

    // follow methods are not base class overrides
    void SetLaunderFlag(bool bFlag)
    {
        m_bLaunderColumnNames = bFlag;
    }

    void SetUTF8ToASCIIFlag(bool bFlag)
    {
        m_bUTF8ToASCII = bFlag;
    }

    void SetPrecisionFlag(bool bFlag)
    {
        m_bPreservePrecision = bFlag;
    }

    void SetOverrideColumnTypes(const char *pszOverrideColumnTypes);

    void SetUnknownSRSId(int nUnknownSRSIdIn)
    {
        m_nUnknownSRSId = nUnknownSRSIdIn;
    }

    void SetForcedSRSId(int nForcedSRSIdIn)
    {
        m_nForcedSRSId = nForcedSRSIdIn;
    }

    void SetForcedGeometryTypeFlags(int GeometryTypeFlagsIn)
    {
        m_nForcedGeometryTypeFlags = GeometryTypeFlagsIn;
    }

    void SetCreateSpatialIndex(bool bFlag, const char *pszSpatialIndexType)
    {
        m_bCreateSpatialIndexFlag = bFlag;
        m_osSpatialIndexType = pszSpatialIndexType;
    }

    void SetPostGISVersion(int nPostGISMajorIn, int nPostGISMinorIn)
    {
        m_nPostGISMajor = nPostGISMajorIn;
        m_nPostGISMinor = nPostGISMinorIn;
    }

    void SetGeometryFieldName(const char *pszGeomFieldName)
    {
        m_osFirstGeometryFieldName = pszGeomFieldName;
    }

    void SetForcedDescription(const char *pszDescriptionIn);

    void SetGeomColumnPositionImmediate(bool bGeomColumnPositionImmediate)
    {
        m_bGeomColumnPositionImmediate = bGeomColumnPositionImmediate;
    }

    void SetDeferredGeomFieldCreationCommands(
        const std::vector<std::string> &aosDeferredGeomFieldCreationCommands)
    {
        m_aosDeferredGeomFieldCreationCommands =
            aosDeferredGeomFieldCreationCommands;
    }

    void SetSpatialIndexCreationCommands(
        const std::vector<std::string> &aosSpatialIndexCreationCommands)
    {
        m_aosSpatialIndexCreationCommands = aosSpatialIndexCreationCommands;
    }

    OGRErr EndCopy();
};

/************************************************************************/
/*                       OGRPGDumpDataSource                            */
/************************************************************************/
class OGRPGDumpDataSource final : public GDALDataset
{
    OGRPGDumpDataSource(const OGRPGDumpDataSource &) = delete;
    OGRPGDumpDataSource &operator=(const OGRPGDumpDataSource &) = delete;

    std::vector<std::unique_ptr<OGRPGDumpLayer>> m_apoLayers{};
    VSILFILE *m_fp = nullptr;
    bool m_bInTransaction = false;
    OGRPGDumpLayer *m_poLayerInCopyMode = nullptr;
    const char *m_pszEOL = "\n";

  public:
    OGRPGDumpDataSource(const char *pszName, char **papszOptions);
    virtual ~OGRPGDumpDataSource();

    bool Log(const char *pszStr, bool bAddSemiColumn = true);

    virtual int GetLayerCount() override
    {
        return static_cast<int>(m_apoLayers.size());
    }

    virtual OGRLayer *GetLayer(int) override;

    virtual OGRLayer *ICreateLayer(const char *pszName,
                                   const OGRGeomFieldDefn *poGeomFieldDefn,
                                   CSLConstList papszOptions) override;

    virtual int TestCapability(const char *) override;

    void LogStartTransaction();
    void LogCommit();

    void StartCopy(OGRPGDumpLayer *poPGLayer);
    OGRErr EndCopy();
};

#endif /* ndef OGR_PGDUMP_H_INCLUDED */
