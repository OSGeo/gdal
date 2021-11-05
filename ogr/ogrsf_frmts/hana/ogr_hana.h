/******************************************************************************
 *
 * Project:  SAP HANA Spatial Driver
 * Purpose:  SAP HANA Spatial OGR Driver Declarations.
 * Author:   Maxim Rylov
 *
 ******************************************************************************
 * Copyright (c) 2020, SAP SE
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

#ifndef OGR_HANA_H_INCLUDED
#define OGR_HANA_H_INCLUDED

#include "ogrsf_frmts.h"
#include "cpl_string.h"

#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "odbc/Forwards.h"

/************************************************************************/
/*                          Internal struct definitions                 */
/************************************************************************/

struct ColumnDefinition
{
    CPLString name;
    CPLString typeDef;
};

struct AttributeColumnDescription
{
    CPLString name;
    short type;
    CPLString typeName;
    int length; // the same type as in OGRFieldDefn.GetWidth
    unsigned short precision;
    unsigned short scale;
    bool isFeatureID;
    bool isArray;
    bool isAutoIncrement;
    bool isNullable;
    CPLString defaultValue;

    AttributeColumnDescription()
        : name("")
        , type(-1)
        , typeName("")
        , length(0)
        , precision(0)
        , scale(0)
        , isFeatureID(false)
        , isArray(false)
        , isAutoIncrement(false)
        , isNullable(false)
    {
    }
};

struct GeometryColumnDescription
{
    CPLString name;
    OGRwkbGeometryType type;
    int srid;
    bool isNullable;
};

struct ColumnDescription
{
    bool isGeometry;
    AttributeColumnDescription attributeDescription;
    GeometryColumnDescription geometryDescription;
};

struct FieldTypeInfo
{
    CPLString name;
    short type;
    int width;
    int precision;
};

struct Binary
{
    GByte* data;
    std::size_t size;
};

/************************************************************************/
/*                          OGRHanaDataSource                          */
/************************************************************************/

class OGRHanaDataSource : public GDALDataset
{
private:
    friend class OGRHanaLayer;
    friend class OGRHanaTableLayer;
    friend class OGRHanaResultLayer;

    using SrsCache = std::unordered_map<int, OGRSpatialReference*>;

    CPLString schemaName_;
    bool updateMode_;
    bool isTransactionStarted_;
    std::vector<std::unique_ptr<OGRLayer>> layers_;
    SrsCache srsCache_;
    odbc::EnvironmentRef connEnv_;
    odbc::ConnectionRef conn_;

private:
    void CreateTable(
        const CPLString& tableName,
        const CPLString& fidName,
        const CPLString& fidType,
        const CPLString& geomColumnName,
        const CPLString& geomColumnNullable,
        int geomSrid);

protected:
    std::pair<CPLString, CPLString> FindSchemaAndTableNames(const char* query);
    int FindLayerByName(const char* name);
    CPLString FindSchemaName(const char* objectName);

    odbc::StatementRef CreateStatement();
    odbc::PreparedStatementRef PrepareStatement(const char* sql);
    void Commit();
    void ExecuteSQL(const char* sql);

    OGRSpatialReference* GetSrsById(int srid);
    int GetSrsId(OGRSpatialReference* srs);
    bool IsSrsRoundEarth(int srid);
    bool HasSrsPlanarEquivalent(int srid);
    OGRErr GetQueryColumns(
        const CPLString& schemaName,
        const CPLString& query,
        std::vector<ColumnDescription>& columDescriptions);
    std::vector<CPLString> GetTablePrimaryKeys(
        const char* schemaName, const char* tableName);

    void InitializeLayers(const char* schemaName, const char* tableNames);
    void CreateSpatialReferenceSystem(
        const OGRSpatialReference& srs,
        const char* authorityName,
        int authorityCode,
        const char* wkt,
        const char* proj4);

    bool IsTransactionStarted() const { return isTransactionStarted_; }

    void CreateParseArrayFunctions(const char* schemaName);
    bool ParseArrayFunctionsExist(const char* schemaName);

public:
    static const char* GetPrefix();
    static const char* GetLayerCreationOptions();
    static const char* GetOpenOptions();
    static const char* GetSupportedDataTypes();

public:
    OGRHanaDataSource();
    ~OGRHanaDataSource() override;

    int Open(const char* newName, char** options, int update);

    OGRErr DeleteLayer(int index) override;
    int GetLayerCount() override { return static_cast<int>(layers_.size()); }
    OGRLayer* GetLayer(int index) override;
    OGRLayer* GetLayerByName(const char*) override;
    OGRLayer* ICreateLayer(
        const char* layerName,
        OGRSpatialReference* srs = nullptr,
        OGRwkbGeometryType geomType = wkbUnknown,
        char** options = nullptr) override;
    int TestCapability(const char* capabilities) override;

    OGRLayer* ExecuteSQL(
        const char* sqlCommand,
        OGRGeometry* spatialFilter,
        const char* dialect) override;

    OGRErr StartTransaction(int bForce = FALSE) override;
    OGRErr CommitTransaction() override;
    OGRErr RollbackTransaction() override;
};

/************************************************************************/
/*                             OGRHanaLayer                             */
/************************************************************************/

class OGRHanaLayer : public OGRLayer
{
protected:
    OGRHanaDataSource* dataSource_;
    OGRFeatureDefn* featureDefn_;
    GIntBig nextFeatureId_;
    std::vector<AttributeColumnDescription> attrColumns_;
    std::vector<GeometryColumnDescription> geomColumns_;
    int fidFieldIndex_;
    CPLString rawQuery_;
    CPLString queryStatement_;
    CPLString whereClause_;
    CPLString attrFilter_;
    bool rebuildQueryStatement_;
    odbc::ResultSetRef resultSet_;
    std::vector<char> dataBuffer_;
    int srid_;
    OGRSpatialReference* srs_;

    void BuildQueryStatement();
    void BuildWhereClause();
    void EnsureBufferCapacity(std::size_t size);
    virtual OGRFeature* GetNextFeatureInternal();
    int GetGeometryColumnSrid(int columnIndex) const;
    virtual OGRFeature* ReadFeature();
    OGRErr ReadFeatureDefinition(
        const CPLString& schemaName,
        const CPLString& tableName,
        const CPLString& query,
        const char* featureDefName);
    void ReadGeometryExtent(int geomField, OGREnvelope* extent);

public:
    explicit OGRHanaLayer(OGRHanaDataSource* datasource);
    ~OGRHanaLayer() override;

    void ResetReading() override;

    OGRErr GetExtent(OGREnvelope* extent, int force = TRUE) override
    {
        return GetExtent(0, extent, force);
    }
    OGRErr GetExtent(int geomField, OGREnvelope* extent, int force) override;
    GIntBig GetFeatureCount(int force) override;
    OGRFeature* GetNextFeature() override;
    OGRFeatureDefn* GetLayerDefn() override { return featureDefn_; }
    OGRSpatialReference* GetSpatialRef() override;
    const char* GetFIDColumn() override;

    void SetSpatialFilter(OGRGeometry* poGeom) override
    {
        SetSpatialFilter(0, poGeom);
    }
    void SetSpatialFilter(int iGeomField, OGRGeometry* poGeom) override;
};

/************************************************************************/
/*                             OGRHanaTableLayer                        */
/************************************************************************/

class OGRHanaTableLayer : public OGRHanaLayer
{
private:
    friend class OGRHanaDataSource;

    CPLString schemaName_;
    CPLString tableName_;
    bool updateMode_;

    odbc::PreparedStatementRef insertFeatureStmt_;
    odbc::PreparedStatementRef deleteFeatureStmt_;
    odbc::PreparedStatementRef updateFeatureStmt_;
    bool insertFeatureStmtHasFID;

    std::size_t batchSize_;
    std::size_t defaultStringSize_;
    bool launderColumnNames_;
    bool preservePrecision_;
    std::vector<ColumnDefinition> customColumnDefs_;
    bool parseFunctionsChecked_;

    OGRErr ReadTableDefinition();

    std::pair<OGRErr, std::size_t> ExecuteUpdate(
        odbc::PreparedStatement& statement, const char* functionName);
    odbc::PreparedStatementRef CreateDeleteFeatureStatement();
    odbc::PreparedStatementRef CreateInsertFeatureStatement(
        GIntBig fidColumnID);
    odbc::PreparedStatementRef CreateUpdateFeatureStatement();
    void ResetPreparedStatements();
    OGRErr SetStatementParameters(
        odbc::PreparedStatement& statement,
        OGRFeature* feature,
        bool skipFidColumn,
        bool newFeature,
        const char* functionName);

    OGRErr DropTable();
    void FlushPendingFeatures();
    bool HasPendingFeatures() const;
    FieldTypeInfo GetFieldTypeInfo(OGRFieldDefn& field) const;
    OGRErr GetGeometryWkb(OGRFeature* feature, int fieldIndex, Binary& binary);
    void ClearBatches();

public:
    OGRHanaTableLayer(OGRHanaDataSource* datasource, int update);
    ~OGRHanaTableLayer() override;

    OGRErr Initialize(const char* schemaName, const char* tableName);

    void ResetReading() override;
    const char* GetName() override { return tableName_.c_str(); }
    int TestCapability(const char* capabilities) override;

    OGRErr SetAttributeFilter(const char* attrFilter) override;

    OGRErr ICreateFeature(OGRFeature* feature) override;
    OGRErr DeleteFeature(GIntBig nFID) override;
    OGRErr ISetFeature(OGRFeature* feature) override;

    OGRErr CreateField(OGRFieldDefn* field, int approxOK = TRUE) override;
    OGRErr CreateGeomField(
        OGRGeomFieldDefn* geomField, int approxOK = TRUE) override;
    OGRErr DeleteField(int field) override;
    OGRErr AlterFieldDefn(
        int field, OGRFieldDefn* newFieldDefn, int flagsIn) override;

    void SetBatchSize(std::size_t size) { batchSize_ = size; }
    void SetDefaultStringSize(std::size_t size) { defaultStringSize_ = size; }
    void SetLaunderFlag(bool flag) { launderColumnNames_ = flag; }
    void SetCustomColumnTypes(const char* columnTypes);
    void SetPrecisionFlag(bool flag) { preservePrecision_ = flag; }

    OGRErr StartTransaction() override;
    OGRErr CommitTransaction() override;
    OGRErr RollbackTransaction() override;
};

/************************************************************************/
/*                              OGRHanaResultLayer                      */
/************************************************************************/

class OGRHanaResultLayer : public OGRHanaLayer
{
public:
    explicit OGRHanaResultLayer(OGRHanaDataSource* datasource);

    OGRErr Initialize(const char* query, OGRGeometry* spatialFilter);

    int TestCapability(const char* capabilities) override;
};

#endif /* ndef OGR_HANA_H_INCLUDED */
