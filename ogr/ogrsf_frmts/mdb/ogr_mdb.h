/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Private definitions for MDB driver.
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2011, Even Rouault <even dot rouault at spatialys.com>
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

#ifndef OGR_MDB_H_INCLUDED
#define OGR_MDB_H_INCLUDED

#include <jni.h>
#include <vector>

#include "ogrsf_frmts.h"
#include "cpl_error.h"
#include "cpl_string.h"

/************************************************************************/
/*                            OGRMDBJavaEnv                             */
/************************************************************************/

class OGRMDBJavaEnv
{
    GIntBig nLastPID = 0;

        int Init();

    public:
        OGRMDBJavaEnv();
        ~OGRMDBJavaEnv();

    int InitIfNeeded();
    static void CleanupMutex();

    JavaVM *jvm = nullptr;
    JNIEnv *env = nullptr;
    bool bCalledFromJava = false;

    int ExceptionOccurred();

    jclass byteArray_class = nullptr;

    jclass file_class = nullptr;
    jmethodID file_constructor = nullptr;
    jclass database_class = nullptr;
    jmethodID database_open = nullptr;
    jmethodID database_close = nullptr;
    jmethodID database_getTableNames = nullptr;
    jmethodID database_getTable = nullptr;

    jclass table_class = nullptr;
    jmethodID table_getColumns = nullptr;
    jmethodID table_iterator = nullptr;
    jmethodID table_getRowCount = nullptr;

    jclass column_class = nullptr;
    jmethodID column_getName = nullptr;
    jmethodID column_getType = nullptr;
    jmethodID column_getLength = nullptr;
    jmethodID column_isVariableLength = nullptr;

    jclass datatype_class = nullptr;
    jmethodID datatype_getValue = nullptr;

    jclass list_class = nullptr;
    jmethodID list_iterator = nullptr;

    jclass set_class = nullptr;
    jmethodID set_iterator = nullptr;

    jclass map_class = nullptr;
    jmethodID map_get = nullptr;

    jclass iterator_class = nullptr;
    jmethodID iterator_hasNext = nullptr;
    jmethodID iterator_next = nullptr;

    jclass object_class = nullptr;
    jmethodID object_toString = nullptr;
    jmethodID object_getClass = nullptr;

    jclass boolean_class = nullptr;
    jmethodID boolean_booleanValue = nullptr;

    jclass byte_class = nullptr;
    jmethodID byte_byteValue = nullptr;

    jclass short_class = nullptr;
    jmethodID short_shortValue = nullptr;

    jclass integer_class = nullptr;
    jmethodID integer_intValue = nullptr;

    jclass float_class = nullptr;
    jmethodID float_floatValue = nullptr;

    jclass double_class = nullptr;
    jmethodID double_doubleValue = nullptr;
};

/************************************************************************/
/*                           OGRMDBDatabase                             */
/************************************************************************/

class OGRMDBTable;

class OGRMDBDatabase
{
    OGRMDBJavaEnv* env = nullptr;
    jobject database = nullptr;

    OGRMDBDatabase();
public:
    static OGRMDBDatabase* Open(OGRMDBJavaEnv* env, const char* pszName);
    ~OGRMDBDatabase();

    std::vector<CPLString>   apoTableNames;
    int                FetchTableNames();
    OGRMDBTable* GetTable(const char* pszTableName);
};

/************************************************************************/
/*                             OGRMDBTable                              */
/************************************************************************/

class OGRMDBTable
{
    OGRMDBJavaEnv* env = nullptr;
    OGRMDBDatabase* poDB = nullptr;
    jobject table = nullptr;

    jobject table_iterator_obj = nullptr;
    jobject row = nullptr;

    jobject GetColumnVal(int iCol);

    CPLString osTableName;

    std::vector<CPLString> apoColumnNames;
    std::vector<jstring>   apoColumnNameObjects;
    std::vector<int>       apoColumnTypes;
    std::vector<int>       apoColumnLengths;

public:
    OGRMDBTable(OGRMDBJavaEnv* env, OGRMDBDatabase* poDB, jobject table, const char* pszTableName);
    ~OGRMDBTable();

    OGRMDBDatabase* GetDB() { return poDB; }

    const char* GetName() { return osTableName.c_str(); }

    int GetColumnCount() { return (int)apoColumnNames.size(); }
    int GetColumnIndex(const char* pszColName, int bEmitErrorIfNotFound = FALSE);
    const char* GetColumnName(int iIndex) { return apoColumnNames[iIndex].c_str(); }
    int GetColumnType(int iIndex) { return apoColumnTypes[iIndex]; }
    int GetColumnLength(int iIndex) { return apoColumnLengths[iIndex]; }

    void DumpTable();

    int FetchColumns();

    int GetRowCount();
    int GetNextRow();
    void ResetReading();

    char* GetColumnAsString(int iCol);
    int GetColumnAsInt(int iCol);
    double GetColumnAsDouble(int iCol);
    GByte* GetColumnAsBinary(int iCol, int* pnBytes);
};

typedef enum
{
    MDB_Boolean = 0x01,
    MDB_Byte = 0x02,
    MDB_Short = 0x03,
    MDB_Int = 0x04,
    MDB_Money = 0x05,
    MDB_Float = 0x06,
    MDB_Double = 0x07,
    MDB_ShortDateTime = 0x08,
    MDB_Binary = 0x09,
    MDB_Text = 0x0A,
    MDB_OLE = 0x0B,
    MDB_Memo = 0x0C,
    MDB_Unknown = 0x0D,
    MDB_GUID = 0x0F,
    MDB_Numeric = 0x10
} MDBType;

typedef enum
{
    MDB_GEOM_NONE,
    MDB_GEOM_PGEO,
    MDB_GEOM_GEOMEDIA
} MDBGeometryType;

/************************************************************************/
/*                            OGRMDBLayer                              */
/************************************************************************/

class OGRMDBDataSource;

class OGRMDBLayer final: public OGRLayer
{
  protected:
    OGRMDBTable* poMDBTable;

    MDBGeometryType     eGeometryType;

    OGRFeatureDefn     *poFeatureDefn;

    // Layer spatial reference system, and srid.
    OGRSpatialReference *poSRS;
    int                 nSRSId;

    int                 iNextShapeId;

    OGRMDBDataSource    *poDS;

    int                 iGeomColumn;
    char                *pszGeomColumn;
    char                *pszFIDColumn;

    int                *panFieldOrdinals;

    int                 bHasExtent;
    OGREnvelope         sExtent;

    void                LookupSRID( int );

  public:
                        OGRMDBLayer(OGRMDBDataSource* poDS, OGRMDBTable* poMDBTable);
    virtual             ~OGRMDBLayer();

    CPLErr              BuildFeatureDefn();

    CPLErr              Initialize( const char *pszTableName,
                                    const char *pszGeomCol,
                                    int nShapeType,
                                    double dfExtentLeft,
                                    double dfExtentRight,
                                    double dfExtentBottom,
                                    double dfExtentTop,
                                    int nSRID,
                                    int bHasZ );

    CPLErr              Initialize( const char *pszTableName,
                                    const char *pszGeomCol,
                                    OGRSpatialReference* poSRS );

    virtual void        ResetReading() override;
    virtual GIntBig     GetFeatureCount( int bForce ) override;
    virtual OGRFeature *GetNextRawFeature();
    virtual OGRFeature *GetNextFeature() override;

    virtual OGRFeature *GetFeature( GIntBig nFeatureId ) override;

    OGRFeatureDefn *    GetLayerDefn() override { return poFeatureDefn; }

    virtual int         TestCapability( const char * ) override;

    virtual const char *GetFIDColumn() override;

    virtual OGRErr      GetExtent( OGREnvelope *psExtent, int bForce ) override;
    virtual OGRErr      GetExtent(int iGeomField, OGREnvelope *psExtent, int bForce) override
                { return OGRLayer::GetExtent(iGeomField, psExtent, bForce); }
};

/************************************************************************/
/*                           OGRMDBDataSource                            */
/************************************************************************/

class OGRMDBDataSource final: public OGRDataSource
{
    OGRMDBLayer        **papoLayers;
    int                 nLayers;

    OGRMDBLayer        **papoLayersInvisible;
    int                 nLayersWithInvisible;

    char               *pszName;

    OGRMDBJavaEnv       env;

    OGRMDBDatabase*     poDB;

    int                 OpenGDB(OGRMDBTable* poGDB_GeomColumns);
    int                 OpenGeomediaWarehouse(OGRMDBTable* poGAliasTable);
    OGRSpatialReference* GetGeomediaSRS(const char* pszGCoordSystemTable,
                                        const char* pszGCoordSystemGUID);

  public:
                        OGRMDBDataSource();
                        ~OGRMDBDataSource();

    int                 Open( const char * );
    int                 OpenTable( const char *pszTableName,
                                   const char *pszGeomCol,
                                   int bUpdate );

    const char          *GetName() override { return pszName; }
    int                 GetLayerCount() override { return nLayers; }
    OGRLayer            *GetLayer( int ) override;
    OGRLayer            *GetLayerByName( const char* pszLayerName ) override;

    int                 TestCapability( const char * ) override;
};

/************************************************************************/
/*                             OGRMDBDriver                             */
/************************************************************************/

class OGRMDBDriver final: public OGRSFDriver
{
  public:
                ~OGRMDBDriver();

    const char  *GetName() override;
    OGRDataSource *Open( const char *, int ) override;

    int          TestCapability( const char * ) override;
};

#endif /* ndef OGR_MDB_H_INCLUDED */
