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

class OGRHanaDataSource;

namespace OGRHANA {

constexpr static int DEFAULT_BATCH_SIZE = 4 * 1024 * 1024;
constexpr static int DEFAULT_STRING_SIZE = 256;

constexpr static int UNDETERMINED_SRID = -1;

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
    short type = -1;
    CPLString typeName;
    int length = 0; // the same type as in OGRFieldDefn.GetWidth
    unsigned short precision = 0;
    unsigned short scale = 0;
    bool isFeatureID = false;
    bool isArray = false;
    bool isAutoIncrement = false;
    bool isNullable = false;
    CPLString defaultValue;
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

struct ColumnTypeInfo
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
/*                             OGRHanaLayer                             */
/************************************************************************/

class OGRHanaLayer : public OGRLayer
{
protected:
    OGRHanaDataSource* dataSource_ = nullptr;
    OGRFeatureDefn* featureDefn_ = nullptr;
    GIntBig nextFeatureId_ = 0;
    std::vector<AttributeColumnDescription> attrColumns_;
    std::vector<GeometryColumnDescription> geomColumns_;
    int fidFieldIndex_ = OGRNullFID;
    CPLString fidFieldName_;
    CPLString rawQuery_;
    CPLString queryStatement_;
    CPLString whereClause_;
    CPLString attrFilter_;
    odbc::ResultSetRef resultSet_;
    std::vector<char> dataBuffer_;
    bool initialized_ = false;

    void EnsureInitialized();
    virtual OGRErr Initialize() = 0;

    void ClearQueryStatement();
    const CPLString& GetQueryStatement();
    void BuildWhereClause();
    void EnsureBufferCapacity(std::size_t size);
    virtual OGRFeature* GetNextFeatureInternal();
    int GetGeometryColumnSrid(int columnIndex) const;
    virtual OGRFeature* ReadFeature();
    OGRErr InitFeatureDefinition(
        const CPLString& schemaName,
        const CPLString& tableName,
        const CPLString& query,
        const CPLString& featureDefName);
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
    const char* GetFIDColumn() override;
    OGRFeatureDefn* GetLayerDefn() override;
    const char* GetName() override;

    OGRErr SetAttributeFilter( const char *pszQuery ) override;

    void SetSpatialFilter(OGRGeometry* poGeom) override
    {
        SetSpatialFilter(0, poGeom);
    }
    void SetSpatialFilter(int iGeomField, OGRGeometry* poGeom) override;
};

/************************************************************************/
/*                             OGRHanaTableLayer                        */
/************************************************************************/

class OGRHanaTableLayer final : public OGRHanaLayer
{
private:
    CPLString schemaName_;
    CPLString tableName_;
    bool updateMode_ = false;

    odbc::PreparedStatementRef currentIdentityValueStmt_;
    odbc::PreparedStatementRef insertFeatureStmtWithFID_;
    odbc::PreparedStatementRef insertFeatureStmtWithoutFID_;
    odbc::PreparedStatementRef deleteFeatureStmt_;
    odbc::PreparedStatementRef updateFeatureStmt_;

    std::size_t batchSize_ = DEFAULT_BATCH_SIZE;
    std::size_t defaultStringSize_ = DEFAULT_STRING_SIZE;
    bool launderColumnNames_ = true;
    bool preservePrecision_ = true;
    std::vector<ColumnDefinition> customColumnDefs_;
    bool parseFunctionsChecked_ = false;

    OGRErr Initialize() override;
    std::pair<OGRErr, std::size_t> ExecuteUpdate(
        odbc::PreparedStatement& statement, bool withBatch, const char* functionName);
    odbc::PreparedStatementRef CreateDeleteFeatureStatement();
    odbc::PreparedStatementRef CreateInsertFeatureStatement(bool withFID);
    odbc::PreparedStatementRef CreateUpdateFeatureStatement();
    void ResetPreparedStatements();
    OGRErr SetStatementParameters(
        odbc::PreparedStatement& statement,
        OGRFeature* feature,
        bool newFeature,
        bool withFID,
        const char* functionName);

    void FlushPendingFeatures();
    bool HasPendingFeatures() const;
    ColumnTypeInfo GetColumnTypeInfo(const OGRFieldDefn& field) const;
    OGRErr GetGeometryWkb(OGRFeature* feature, int fieldIndex, Binary& binary);
    void ClearBatches();

public:
    OGRHanaTableLayer(
        OGRHanaDataSource* datasource,
        const char* schemaName,
        const char* tableName,
        int update);
    ~OGRHanaTableLayer() override;

    OGRErr DropTable();

    void ResetReading() override;
    const char* GetName() override { return tableName_.c_str(); }
    int TestCapability(const char* capabilities) override;

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

class OGRHanaResultLayer final : public OGRHanaLayer
{
    OGRErr Initialize() override;

public:
    explicit OGRHanaResultLayer(OGRHanaDataSource* datasource, const char* query);

    int TestCapability(const char* capabilities) override;
};

} /* end of OGRHANA namespace */

/************************************************************************/
/*                          OGRHanaDataSource                          */
/************************************************************************/

class OGRHanaDataSource final : public GDALDataset
{
private:
    friend class OGRHANA::OGRHanaLayer;
    friend class OGRHANA::OGRHanaTableLayer;
    friend class OGRHANA::OGRHanaResultLayer;

    using SrsCache = std::unordered_map<int, OGRSpatialReference*>;

    CPLString schemaName_;
    bool updateMode_ = false;
    bool detectGeometryType_ = true;
    bool isTransactionStarted_ = false;
    std::vector<std::unique_ptr<OGRLayer>> layers_;
    SrsCache srsCache_;
    odbc::EnvironmentRef connEnv_;
    odbc::ConnectionRef conn_;
    int majorVersion_ = 0;

private:
    void CreateTable(
        const CPLString& tableName,
        const CPLString& fidName,
        const CPLString& fidType,
        const CPLString& geomColumnName,
        OGRwkbGeometryType geomType,
        bool geomColumnNullable,
        const CPLString& geomColumnIndexType,
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
        std::vector<OGRHANA::ColumnDescription>& columnDescriptions);
    std::vector<CPLString> GetTablePrimaryKeys(
        const char* schemaName, const char* tableName);

    void InitializeLayers(
        const char* schemaName,
        const char* tableNames);
    void CreateSpatialReferenceSystem(
        const OGRSpatialReference& srs,
        int srid,
        const char* authorityName,
        int authorityCode,
        const CPLString& wkt,
        const CPLString& proj4);

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

    uint GetMajorVersion() const { return majorVersion_; }
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

#endif /* ndef OGR_HANA_H_INCLUDED */
