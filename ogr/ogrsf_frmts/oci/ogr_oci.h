/******************************************************************************
 *
 * Project:  Oracle Spatial Driver
 * Purpose:  Oracle Spatial OGR Driver Declarations.
 * Author:   Frank Warmerdam <warmerdam@pobox.com>
 *
 ******************************************************************************
 * Copyright (c) 2002, Frank Warmerdam <warmerdam@pobox.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef OGR_OCI_H_INCLUDED
#define OGR_OCI_H_INCLUDED

#include "ogrsf_frmts.h"
#include "oci.h"
#include "cpl_error.h"

#include <map>
#include <set>

/* -------------------------------------------------------------------- */
/*      Low level Oracle spatial declarations.                          */
/* -------------------------------------------------------------------- */
#define TYPE_OWNER "MDSYS"
#define SDO_GEOMETRY "MDSYS.SDO_GEOMETRY"

typedef struct
{
    OCINumber x;
    OCINumber y;
    OCINumber z;
} sdo_point_type;

typedef OCIArray sdo_elem_info_array;
typedef OCIArray sdo_ordinate_array;

typedef struct
{
    OCINumber sdo_gtype;
    OCINumber sdo_srid;
    sdo_point_type sdo_point;
    OCIArray *sdo_elem_info;
    OCIArray *sdo_ordinates;
} SDO_GEOMETRY_TYPE;

typedef struct
{
    OCIInd _atomic;
    OCIInd x;
    OCIInd y;
    OCIInd z;
} sdo_point_type_ind;

typedef struct
{
    OCIInd _atomic;
    OCIInd sdo_gtype;
    OCIInd sdo_srid;
    sdo_point_type_ind sdo_point;
    OCIInd sdo_elem_info;
    OCIInd sdo_ordinates;
} SDO_GEOMETRY_ind;

#define ORA_GTYPE_MATCH(a, b) (((a) % 100) == ((b) % 100))
#define ORA_GTYPE_UNKNOWN 0
#define ORA_GTYPE_POINT 1
#define ORA_GTYPE_LINESTRING 2  // or curve
#define ORA_GTYPE_POLYGON 3     // or surface
#define ORA_GTYPE_COLLECTION 4
#define ORA_GTYPE_MULTIPOINT 5
#define ORA_GTYPE_MULTILINESTRING 6  // or multicurve
#define ORA_GTYPE_MULTIPOLYGON 7     // or multisurface
#define ORA_GTYPE_SOLID 8
#define ORA_GTYPE_MULTISOLID 9

#define DEFAULT_STRING_SIZE 4000

/************************************************************************/
/*                            OGROCISession                             */
/************************************************************************/
class CPL_DLL OGROCISession final
{
  public:
    OCIEnv *hEnv;
    OCIError *hError;
    OCISvcCtx *hSvcCtx;
    OCIServer *hServer;
    OCISession *hSession;
    OCIDescribe *hDescribe;
    OCIType *hGeometryTDO;
    OCIType *hOrdinatesTDO;
    OCIType *hElemInfoTDO;

    char *pszUserid;
    char *pszPassword;
    char *pszDatabase;

  public:
    OGROCISession();
    ~OGROCISession();

    int EstablishSession(const char *pszUserid, const char *pszPassword,
                         const char *pszDatabase);

    int Failed(sword nStatus, const char *pszFunction = nullptr);

    CPLErr GetParamInfo(OCIParam *hParamDesc, OGRFieldDefn *poOGRDefn,
                        ub2 *pnOCIType, ub4 *pnOCILen);

    void CleanName(char *);

    OCIType *PinTDO(const char *);

  private:
    int nServerVersion;
    int nServerRelease;
    size_t nMaxNameLength;
};

OGROCISession CPL_DLL *OGRGetOCISession(const char *pszUserid,
                                        const char *pszPassword,
                                        const char *pszDatabase);

/************************************************************************/
/*                           OGROCIStatement                            */
/************************************************************************/
class CPL_DLL OGROCIStatement final
{
  public:
    explicit OGROCIStatement(OGROCISession *);
    ~OGROCIStatement();

    OCIStmt *GetStatement()
    {
        return hStatement;
    }

    CPLErr BindScalar(const char *pszPlaceName, void *pData, int nDataLen,
                      int nSQLType, sb2 *paeInd = nullptr);
    CPLErr BindString(const char *pszPlaceName, const char *pszData,
                      sb2 *paeInd = nullptr);
    CPLErr BindObject(const char *pszPlaceName, void *pahObject, OCIType *hTDO,
                      void **papIndicators);

    char *pszCommandText;

    CPLErr Prepare(const char *pszStatement);
    CPLErr Execute(const char *pszStatement, int nMode = -1);
    void Clean();

    OGRFeatureDefn *GetResultDefn()
    {
        return poDefn;
    }

    char **SimpleFetchRow();

    int GetAffectedRows() const
    {
        return nAffectedRows;
    }

  private:
    OGROCISession *poSession;
    OCIStmt *hStatement;

    OGRFeatureDefn *poDefn;

    char **papszCurColumn;
    char **papszCurImage;
    sb2 *panCurColumnInd;

    int nRawColumnCount;
    int *panFieldMap;
    int nAffectedRows;
};

/************************************************************************/
/*                           OGROCIStringBuf                            */
/************************************************************************/
class OGROCIStringBuf final
{
    char *pszString;
    int nLen;
    int nBufSize;

    void UpdateEnd();

  public:
    OGROCIStringBuf();
    ~OGROCIStringBuf();

    void MakeRoomFor(int);
    void Append(const char *);
    void Appendf(int nMax, const char *pszFormat, ...)
        CPL_PRINT_FUNC_FORMAT(3, 4);
    char *StealString();

    char GetLast();

    char *GetEnd()
    {
        UpdateEnd();
        return pszString + nLen;
    }

    char *GetString()
    {
        return pszString;
    }

    void Clear();
};

/************************************************************************/
/*                             OGROCILayer                              */
/************************************************************************/

class OGROCIDataSource;

class OGROCILayer CPL_NON_FINAL : public OGRLayer
{
  protected:
    OGROCIDataSource *poDS;

    OGRFeatureDefn *poFeatureDefn;

    int iNextShapeId;

    char *pszQueryStatement;

    int nResultOffset;

    OGROCIStatement *poStatement;

    int ExecuteQuery(const char *);

    SDO_GEOMETRY_TYPE *hLastGeom;
    SDO_GEOMETRY_ind *hLastGeomInd;

    char *pszGeomName;
    int iGeomColumn;

    char *pszFIDName;
    int iFIDColumn;

    std::set<int> setFieldIndexWithTimeStampWithTZ{};

    OGRGeometry *TranslateGeometry();
    OGRGeometry *TranslateGeometryElement(int *piElement, int nGType,
                                          int nDimension, int nEType,
                                          int nInterpretation,
                                          int nStartOrdinal, int nOrdCount);
    int LoadElementInfo(int iElement, int nElemCount, int nTotalOrdCount,
                        int *pnEType, int *pnInterpretation,
                        int *pnStartOrdinal, int *pnElemOrdCount);
    int GetOrdinalPoint(int iOrdinal, int nDimension, double *pdfX,
                        double *pdfY, double *pdfZ);

  public:
    explicit OGROCILayer(OGROCIDataSource *poDSIn);
    ~OGROCILayer() override;

    virtual int FindFieldIndex(const char *pszFieldName,
                               int bExactMatch) override
    {
        return OGRLayer::FindFieldIndex(pszFieldName, bExactMatch);
    }

    void ResetReading() override;
    virtual OGRFeature *GetNextRawFeature();
    OGRFeature *GetNextFeature() override;

    using OGRLayer::GetLayerDefn;

    const OGRFeatureDefn *GetLayerDefn() const override
    {
        return poFeatureDefn;
    }

    int TestCapability(const char *) const override;

    const char *GetFIDColumn() const override;
    const char *GetGeometryColumn() const override;

    GDALDataset *GetDataset() override;

    int LookupTableSRID();
};

/************************************************************************/
/*                         OGROCIWritableLayer                          */
/************************************************************************/

class OGROCIWritableLayer CPL_NON_FINAL : public OGROCILayer
{
  protected:
    int nDimension;
    int nSRID;

    int nOrdinalCount;
    int nOrdinalMax;
    double *padfOrdinals;

    int nElemInfoCount;
    int nElemInfoMax;
    int *panElemInfo;

    void PushOrdinal(double);
    void PushElemInfo(int, int, int);

    OGRErr TranslateToSDOGeometry(OGRGeometry *, int *pnGType);
    OGRErr TranslateElementGroup(OGRGeometry *poGeometry);

    int bLaunderColumnNames;
    int bPreservePrecision;
    int nDefaultStringSize;

    OGRSpatialReference *poSRS;

    char **papszOptions;

    int bTruncationReported;
    void ReportTruncation(OGRFieldDefn *);

    void ParseDIMINFO(const char *, double *, double *, double *);

    explicit OGROCIWritableLayer(OGROCIDataSource *poDSIn);
    ~OGROCIWritableLayer() override;

  public:
    const OGRSpatialReference *GetSpatialRef() const override
    {
        return poSRS;
    }

    virtual OGRErr CreateField(const OGRFieldDefn *poField,
                               int bApproxOK = TRUE) override;
    virtual int FindFieldIndex(const char *pszFieldName,
                               int bExactMatch) override;

    // following methods are not base class overrides
    void SetOptions(CSLConstList);

    void SetDimension(int);

    void SetLaunderFlag(int bFlag)
    {
        bLaunderColumnNames = bFlag;
    }

    void SetPrecisionFlag(int bFlag)
    {
        bPreservePrecision = bFlag;
    }

    void SetDefaultStringSize(int nSize)
    {
        nDefaultStringSize = nSize;
    }
};

/************************************************************************/
/*                          OGROCILoaderLayer                           */
/************************************************************************/

#define LDRM_UNKNOWN 0
#define LDRM_STREAM 1
#define LDRM_VARIABLE 2
#define LDRM_BINARY 3

class OGROCILoaderLayer final : public OGROCIWritableLayer
{
    OGREnvelope sExtent;
    int iNextFIDToWrite;

    char *pszLoaderFilename;

    FILE *fpLoader;
    int bHeaderWritten;

    FILE *fpData;

    int nLDRMode;

    void WriteLoaderHeader();
    void FinalizeNewLayer();

    OGRErr WriteFeatureStreamMode(OGRFeature *);
    OGRErr WriteFeatureVariableMode(OGRFeature *);

    static OGRErr WriteFeatureBinaryMode(OGRFeature *);

  public:
    OGROCILoaderLayer(OGROCIDataSource *, const char *pszName,
                      const char *pszGeomCol, int nSRID,
                      const char *pszLoaderFile);
    ~OGROCILoaderLayer() override;

    void ResetReading() override;
    GIntBig GetFeatureCount(int) override;

    OGRErr SetAttributeFilter(const char *) override
    {
        return OGRERR_UNSUPPORTED_OPERATION;
    }

    OGRFeature *GetNextFeature() override;

    OGRErr ICreateFeature(OGRFeature *poFeature) override;

    const OGRSpatialReference *GetSpatialRef() const override
    {
        return poSRS;
    }

    int TestCapability(const char *) const override;
};

/************************************************************************/
/*                           OGROCITableLayer                           */
/************************************************************************/

class OGROCITableLayer final : public OGROCIWritableLayer
{
    int bUpdateAccess;
    int bNewLayer;
    OGREnvelope sExtent;
    bool bExtentUpdated;

    int iNextFIDToWrite;
    int bHaveSpatialIndex;

    OGRFeatureDefn *ReadTableDefinition(const char *);

    void BuildWhere();
    char *BuildFields();
    void BuildFullQueryStatement();

    char *pszQuery;
    char *pszWHERE;

    int bValidTable;

    CPLString osTableName;
    CPLString osOwner;

    int nFirstId;
    int nMultiLoadCount;
    int bMultiLoad;

    OCIArray *hOrdVARRAY;
    OCIArray *hElemInfoVARRAY;

    void UpdateLayerExtents();
    void CreateSpatialIndex();

    void TestForSpatialIndex(const char *);

    OGROCIStatement *poBoundStatement;

    int nWriteCacheMax;
    int nWriteCacheUsed;

    SDO_GEOMETRY_TYPE *pasWriteGeoms;
    SDO_GEOMETRY_TYPE **papsWriteGeomMap;
    SDO_GEOMETRY_ind *pasWriteGeomInd;
    SDO_GEOMETRY_ind **papsWriteGeomIndMap;

    void **papWriteFields;
    OCIInd **papaeWriteFieldInd;
    int *panWriteFIDs;

    int AllocAndBindForWrite();
    OGRErr FlushPendingFeatures();

    OGRErr UnboundCreateFeature(OGRFeature *poFeature);
    OGRErr BoundCreateFeature(OGRFeature *poFeature);

  public:
    OGROCITableLayer(OGROCIDataSource *, const char *pszName,
                     OGRwkbGeometryType eGType, int nSRID, int bUpdate,
                     int bNew);
    ~OGROCITableLayer() override;

    void ResetReading() override;
    GIntBig GetFeatureCount(int) override;

    OGRErr ISetSpatialFilter(int iGeomField,
                             const OGRGeometry *poGeom) override;

    OGRErr SetAttributeFilter(const char *) override;

    OGRFeature *GetNextFeature() override;
    OGRFeature *GetFeature(GIntBig nFeatureId) override;

    OGRErr ISetFeature(OGRFeature *poFeature) override;
    OGRErr ICreateFeature(OGRFeature *poFeature) override;
    OGRErr DeleteFeature(GIntBig nFID) override;

    OGRErr IGetExtent(int iGeomField, OGREnvelope *psExtent,
                      bool bForce) override;

    int TestCapability(const char *) const override;

    OGRErr SyncToDisk() override;

    // following methods are not base class overrides
    int IsValid()
    {
        return bValidTable;
    }

    int GetMaxFID();
};

/************************************************************************/
/*                          OGROCISelectLayer                           */
/************************************************************************/

class OGROCISelectLayer final : public OGROCILayer
{
    OGRFeatureDefn *ReadTableDefinition(OGROCIStatement *poStatement);

  public:
    OGROCISelectLayer(OGROCIDataSource *, const char *pszName,
                      OGROCIStatement *poStatement);
    ~OGROCISelectLayer() override;
};

/************************************************************************/
/*                           OGROCIDataSource                           */
/************************************************************************/

class OGROCIDataSource final : public GDALDataset
{
    OGROCILayer **papoLayers;
    int nLayers;

    char *pszDBName;

    int bDSUpdate;
    int bNoLogging;

    OGROCISession *poSession;

    // We maintain a list of known SRID to reduce the number of trips to
    // the database to get SRSes.
    std::map<int,
             std::unique_ptr<OGRSpatialReference, OGRSpatialReferenceReleaser>>
        m_oSRSCache{};

  public:
    OGROCIDataSource();
    ~OGROCIDataSource() override;

    OGROCISession *GetSession()
    {
        return poSession;
    }

    int Open(const char *, char **papszOpenOptionsIn, int bUpdate,
             int bTestOpen);
    int OpenTable(const char *pszTableName, int nSRID, int bUpdate,
                  int bTestOpen, char **papszOpenOptionsIn);

    int GetLayerCount() const override
    {
        return nLayers;
    }

    const OGRLayer *GetLayer(int) const override;
    OGRLayer *GetLayerByName(const char *pszName) override;

    OGRErr DeleteLayer(int) override;

    OGRLayer *ICreateLayer(const char *pszName,
                           const OGRGeomFieldDefn *poGeomFieldDefn,
                           CSLConstList papszOptions) override;

    int TestCapability(const char *) const override;

    void DeleteLayer(const char *);

    void TruncateLayer(const char *);
    void ValidateLayer(const char *);

    OGRLayer *ExecuteSQL(const char *pszSQLCommand,
                         OGRGeometry *poSpatialFilter,
                         const char *pszDialect) override;
    void ReleaseResultSet(OGRLayer *poLayer) override;

    int FetchSRSId(const OGRSpatialReference *poSRS);
    OGRSpatialReference *FetchSRS(int nSRID);
};

/* -------------------------------------------------------------------- */
/*      Helper functions.                                               */
/* -------------------------------------------------------------------- */
int OGROCIStrokeArcToOGRGeometry_Points(double dfStartX, double dfStartY,
                                        double dfAlongX, double dfAlongY,
                                        double dfEndX, double dfEndY,
                                        double dfMaxAngleStepSizeDegrees,
                                        int bForceWholeCircle,
                                        OGRLineString *poLine);

#endif /* ndef OGR_OCI_H_INCLUDED */
