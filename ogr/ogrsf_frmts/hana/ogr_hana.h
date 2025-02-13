/******************************************************************************
 *
 * Project:  SAP HANA Spatial Driver
 * Purpose:  SAP HANA Spatial OGR Driver Declarations.
 * Author:   Maxim Rylov
 *
 ******************************************************************************
 * Copyright (c) 2020, SAP SE
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef OGR_HANA_H_INCLUDED
#define OGR_HANA_H_INCLUDED

#include "hana/ogrhanautils.h"
#include "ogrsf_frmts.h"
#include "cpl_string.h"

#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "odbc/Forwards.h"
#include "odbc/Types.h"

class OGRHanaDataSource;

namespace OGRHANA
{

constexpr static int DEFAULT_BATCH_SIZE = 4 * 1024 * 1024;
constexpr static int DEFAULT_STRING_SIZE = 256;

constexpr static int UNDETERMINED_SRID = -1;

/************************************************************************/
/*                 Internal enum and struct definitions                 */
/************************************************************************/

class QGRHanaDataTypes
{
    QGRHanaDataTypes() = delete;

  public:
    /// Unknown data type.
    static constexpr int Unknown = odbc::SQLDataTypes::Unknown;
    /// 64-bit integer value.
    static constexpr int BigInt = odbc::SQLDataTypes::BigInt;
    /// Binary data of fixed length.
    static constexpr int Binary = odbc::SQLDataTypes::Binary;
    /// Single bit binary data.
    static constexpr int Bit = odbc::SQLDataTypes::Bit;
    /// Boolean value.
    static constexpr int Boolean = odbc::SQLDataTypes::Boolean;
    /// Character string of fixed string length.
    static constexpr int Char = odbc::SQLDataTypes::Char;
    /// Year, month, and day fields.
    static constexpr int Date = odbc::SQLDataTypes::Date;
    /// Year, month, and day fields.
    static constexpr int DateTime = odbc::SQLDataTypes::DateTime;
    /// Signed, exact, numeric value.
    static constexpr int Decimal = odbc::SQLDataTypes::Decimal;
    /// Double-precision floating point number.
    static constexpr int Double = odbc::SQLDataTypes::Double;
    /// Floating point number with driver-specific precision.
    static constexpr int Float = odbc::SQLDataTypes::Float;
    /// 32-bit integer value.
    static constexpr int Integer = odbc::SQLDataTypes::Integer;
    /// Variable length binary data.
    static constexpr int LongVarBinary = odbc::SQLDataTypes::LongVarBinary;
    /// Variable length character data.
    static constexpr int LongVarChar = odbc::SQLDataTypes::LongVarChar;
    /// Signed, exact, numeric value.
    static constexpr int Numeric = odbc::SQLDataTypes::Numeric;
    /// Single-precision floating point number.
    static constexpr int Real = odbc::SQLDataTypes::Real;
    /// 16-bit integer value.
    static constexpr int SmallInt = odbc::SQLDataTypes::SmallInt;
    /// Hour, minute, and second fields.
    static constexpr int Time = odbc::SQLDataTypes::Time;
    /// Year, month, day, hour, minute, and second fields.
    static constexpr int Timestamp = odbc::SQLDataTypes::Timestamp;
    /// 8-bit integer value.
    static constexpr int TinyInt = odbc::SQLDataTypes::TinyInt;
    /// Year, month, and day fields.
    static constexpr int TypeDate = odbc::SQLDataTypes::TypeDate;
    /// Hour, minute, and second fields.
    static constexpr int TypeTime = odbc::SQLDataTypes::TypeTime;
    /// Year, month, day, hour, minute, and second fields.
    static constexpr int TypeTimestamp = odbc::SQLDataTypes::TypeTimestamp;
    /// Variable length binary data.
    static constexpr int VarBinary = odbc::SQLDataTypes::VarBinary;
    /// Variable-length character string.
    static constexpr int VarChar = odbc::SQLDataTypes::VarChar;
    /// Unicode character string of fixed string length.
    static constexpr int WChar = odbc::SQLDataTypes::WChar;
    /// Unicode variable-length character data.
    static constexpr int WLongVarChar = odbc::SQLDataTypes::WLongVarChar;
    /// Unicode variable-length character string.
    static constexpr int WVarChar = odbc::SQLDataTypes::WVarChar;
    /// ST_GEOMETRY/ST_POINT value.
    static constexpr int Geometry = 29812;
    /// REAL_VECTOR value.
    static constexpr int RealVector = 29814;
};

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
    int length = 0;  // the same type as in OGRFieldDefn.GetWidth
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
    GByte *data;
    std::size_t size;
};

enum class BatchOperation
{
    NONE = 0,
    DELETE = 1,
    INSERT = 2,
    UPDATE = 4,
    ALL = 7
};

inline BatchOperation operator&(BatchOperation a, BatchOperation b)
{
    return static_cast<BatchOperation>(
        static_cast<std::underlying_type<BatchOperation>::type>(a) &
        static_cast<std::underlying_type<BatchOperation>::type>(b));
}

inline BatchOperation operator|(BatchOperation a, BatchOperation b)
{
    return static_cast<BatchOperation>(
        static_cast<std::underlying_type<BatchOperation>::type>(a) |
        static_cast<std::underlying_type<BatchOperation>::type>(b));
}

/************************************************************************/
/*                             OGRHanaLayer                             */
/************************************************************************/

class OGRHanaLayer : public OGRLayer
{
  protected:
    OGRHanaDataSource *dataSource_ = nullptr;
    OGRFeatureDefn *featureDefn_ = nullptr;
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
    const CPLString &GetQueryStatement();
    void BuildWhereClause();
    void EnsureBufferCapacity(std::size_t size);
    virtual OGRFeature *GetNextFeatureInternal();
    int GetGeometryColumnSrid(int columnIndex) const;
    virtual OGRFeature *ReadFeature();
    OGRErr InitFeatureDefinition(const CPLString &schemaName,
                                 const CPLString &tableName,
                                 const CPLString &query,
                                 const CPLString &featureDefName);
    void ReadGeometryExtent(int geomField, OGREnvelope *extent, int force);
    bool IsFastExtentAvailable();

  public:
    explicit OGRHanaLayer(OGRHanaDataSource *datasource);
    ~OGRHanaLayer() override;

    virtual bool IsTableLayer() const
    {
        return false;
    }

    void ResetReading() override;

    OGRErr IGetExtent(int geomField, OGREnvelope *extent, bool force) override;
    GIntBig GetFeatureCount(int force) override;
    OGRFeature *GetNextFeature() override;
    const char *GetFIDColumn() override;
    OGRFeatureDefn *GetLayerDefn() override;
    const char *GetName() override;

    OGRErr SetAttributeFilter(const char *pszQuery) override;

    OGRErr ISetSpatialFilter(int iGeomField,
                             const OGRGeometry *poGeom) override;
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

    bool allowAutoFIDOnCreateFeature_ = false;
    std::size_t batchSize_ = DEFAULT_BATCH_SIZE;
    std::size_t defaultStringSize_ = DEFAULT_STRING_SIZE;
    bool launderColumnNames_ = true;
    bool preservePrecision_ = true;
    std::vector<ColumnDefinition> customColumnDefs_;
    bool parseFunctionsChecked_ = false;

    OGRErr Initialize() override;
    std::pair<OGRErr, std::size_t>
    ExecuteUpdate(odbc::PreparedStatement &statement, bool withBatch,
                  const char *functionName);
    odbc::PreparedStatementRef CreateDeleteFeatureStatement();
    odbc::PreparedStatementRef CreateInsertFeatureStatement(bool withFID);
    odbc::PreparedStatementRef CreateUpdateFeatureStatement();
    void ResetPreparedStatements();
    OGRErr SetStatementParameters(odbc::PreparedStatement &statement,
                                  OGRFeature *feature, bool newFeature,
                                  bool withFID, const char *functionName);

    OGRErr ExecutePendingBatches(BatchOperation op);
    bool HasPendingBatches() const;
    ColumnTypeInfo GetColumnTypeInfo(const OGRFieldDefn &field) const;
    OGRErr GetGeometryWkb(OGRFeature *feature, int fieldIndex, Binary &binary);
    void ClearBatches();
    void ColumnsChanged();

  public:
    OGRHanaTableLayer(OGRHanaDataSource *datasource, const char *schemaName,
                      const char *tableName, int update);
    ~OGRHanaTableLayer() override;

    bool IsTableLayer() const override
    {
        return true;
    }

    OGRErr DropTable();

    void ResetReading() override;

    OGRErr IGetExtent(int iGeomField, OGREnvelope *extent, bool force) override;

    GIntBig GetFeatureCount(int force) override;

    const char *GetName() override
    {
        return tableName_.c_str();
    }

    int TestCapability(const char *capabilities) override;

    OGRErr ICreateFeature(OGRFeature *feature) override;
    OGRErr DeleteFeature(GIntBig nFID) override;
    OGRErr ISetFeature(OGRFeature *feature) override;

    OGRErr CreateField(const OGRFieldDefn *field, int approxOK = TRUE) override;
    OGRErr CreateGeomField(const OGRGeomFieldDefn *geomField,
                           int approxOK = TRUE) override;
    OGRErr DeleteField(int field) override;
    OGRErr AlterFieldDefn(int field, OGRFieldDefn *newFieldDefn,
                          int flagsIn) override;

    void SetBatchSize(std::size_t size)
    {
        batchSize_ = size;
    }

    void SetDefaultStringSize(std::size_t size)
    {
        defaultStringSize_ = size;
    }

    void SetLaunderFlag(bool flag)
    {
        launderColumnNames_ = flag;
    }

    void SetCustomColumnTypes(const char *columnTypes);

    void SetPrecisionFlag(bool flag)
    {
        preservePrecision_ = flag;
    }

    OGRErr StartTransaction() override;
    OGRErr CommitTransaction() override;
    OGRErr RollbackTransaction() override;

    void FlushPendingBatches(bool commit);
};

/************************************************************************/
/*                              OGRHanaResultLayer                      */
/************************************************************************/

class OGRHanaResultLayer final : public OGRHanaLayer
{
    OGRErr Initialize() override;

  public:
    explicit OGRHanaResultLayer(OGRHanaDataSource *datasource,
                                const char *query);

    int TestCapability(const char *capabilities) override;
};

}  // namespace OGRHANA

/************************************************************************/
/*                          OGRHanaDataSource                          */
/************************************************************************/

class OGRHanaDataSource final : public GDALDataset
{
  private:
    friend class OGRHANA::OGRHanaLayer;
    friend class OGRHANA::OGRHanaTableLayer;
    friend class OGRHANA::OGRHanaResultLayer;

    using SrsCache = std::unordered_map<int, OGRSpatialReference *>;

    CPLString schemaName_;
    bool updateMode_ = false;
    bool detectGeometryType_ = true;
    bool isTransactionStarted_ = false;
    std::vector<std::unique_ptr<OGRLayer>> layers_;
    SrsCache srsCache_;
    odbc::EnvironmentRef connEnv_;
    odbc::ConnectionRef conn_;
    OGRHANA::HanaVersion hanaVersion_;
    OGRHANA::HanaVersion cloudVersion_;

  private:
    void CreateTable(const CPLString &tableName, const CPLString &fidName,
                     const CPLString &fidType, const CPLString &geomColumnName,
                     OGRwkbGeometryType geomType, bool geomColumnNullable,
                     const CPLString &geomColumnIndexType, int geomSrid);
    void DetermineVersions();

  protected:
    std::pair<CPLString, CPLString> FindSchemaAndTableNames(const char *query);
    int FindLayerByName(const char *name);
    CPLString FindSchemaName(const char *objectName);

    odbc::StatementRef CreateStatement();
    odbc::PreparedStatementRef PrepareStatement(const char *sql);
    void Commit();
    void ExecuteSQL(const CPLString &sql);

    OGRSpatialReference *GetSrsById(int srid);
    int GetSrsId(const OGRSpatialReference *srs);
    bool IsSrsRoundEarth(int srid);
    bool HasSrsPlanarEquivalent(int srid);
    OGRErr GetQueryColumns(
        const CPLString &schemaName, const CPLString &query,
        std::vector<OGRHANA::ColumnDescription> &columnDescriptions);
    std::vector<CPLString> GetTablePrimaryKeys(const char *schemaName,
                                               const char *tableName);

    void InitializeLayers(const char *schemaName, const char *tableNames);
    void CreateSpatialReferenceSystem(const OGRSpatialReference &srs, int srid,
                                      const char *authorityName,
                                      int authorityCode, const CPLString &wkt,
                                      const CPLString &proj4);

    std::pair<OGRErr, CPLString> LaunderName(const char *name);

    bool IsTransactionStarted() const
    {
        return isTransactionStarted_;
    }

    void CreateParseArrayFunctions(const char *schemaName);
    bool ParseArrayFunctionsExist(const char *schemaName);

  public:
    static const char *GetPrefix();

  public:
    OGRHanaDataSource();
    ~OGRHanaDataSource() override;

    int Open(const char *newName, char **options, int update);

    OGRHANA::HanaVersion GetHanaVersion() const
    {
        return hanaVersion_;
    }

    OGRHANA::HanaVersion GetHanaCloudVersion() const
    {
        return cloudVersion_;
    }

    OGRErr DeleteLayer(int index) override;

    int GetLayerCount() override
    {
        return static_cast<int>(layers_.size());
    }

    OGRLayer *GetLayer(int index) override;
    OGRLayer *GetLayerByName(const char *) override;
    OGRLayer *ICreateLayer(const char *pszName,
                           const OGRGeomFieldDefn *poGeomFieldDefn,
                           CSLConstList papszOptions) override;
    int TestCapability(const char *capabilities) override;

    OGRLayer *ExecuteSQL(const char *sqlCommand, OGRGeometry *spatialFilter,
                         const char *dialect) override;

    OGRErr StartTransaction(int bForce = FALSE) override;
    OGRErr CommitTransaction() override;
    OGRErr RollbackTransaction() override;
};

#endif /* ndef OGR_HANA_H_INCLUDED */
