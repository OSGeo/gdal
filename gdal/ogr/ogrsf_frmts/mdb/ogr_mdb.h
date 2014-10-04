/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Private definitions for MDB driver.
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 ******************************************************************************
 * Copyright (c) 2011, Even Rouault <even dot rouault at mines-paris dot org>
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

#ifndef _OGR_MDB_H_INCLUDED
#define _OGR_MDB_H_INCLUDED

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
    public:
        OGRMDBJavaEnv();
        ~OGRMDBJavaEnv();

        int Init();


    JavaVM *jvm;
    JNIEnv *env;
    int bCalledFromJava;

    int ExceptionOccured();

    jclass byteArray_class;

    jclass file_class;
    jmethodID file_constructor;
    jclass database_class;
    jmethodID database_open;
    jmethodID database_close;
    jmethodID database_getTableNames;
    jmethodID database_getTable;

    jclass table_class;
    jmethodID table_getColumns;
    jmethodID table_iterator;
    jmethodID table_getRowCount;

    jclass column_class;
    jmethodID column_getName;
    jmethodID column_getType;
    jmethodID column_getLength;
    jmethodID column_isVariableLength;

    jclass datatype_class;
    jmethodID datatype_getValue;

    jclass list_class;
    jmethodID list_iterator;

    jclass set_class;
    jmethodID set_iterator;

    jclass map_class;
    jmethodID map_get;

    jclass iterator_class;
    jmethodID iterator_hasNext;
    jmethodID iterator_next;

    jclass object_class;
    jmethodID object_toString;
    jmethodID object_getClass;

    jclass boolean_class;
    jmethodID boolean_booleanValue;

    jclass byte_class;
    jmethodID byte_byteValue;

    jclass short_class;
    jmethodID short_shortValue;
    
    jclass integer_class;
    jmethodID integer_intValue;

    jclass float_class;
    jmethodID float_floatValue;

    jclass double_class;
    jmethodID double_doubleValue;
};

/************************************************************************/
/*                           OGRMDBDatabase                             */
/************************************************************************/

class OGRMDBTable;

class OGRMDBDatabase
{
    OGRMDBJavaEnv* env;
    jobject database;

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
    OGRMDBJavaEnv* env;
    OGRMDBDatabase* poDB;
    jobject table;

    jobject table_iterator_obj;
    jobject row;

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
    
class OGRMDBLayer : public OGRLayer
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

    virtual void        ResetReading();
    virtual int         GetFeatureCount( int bForce );
    virtual OGRFeature *GetNextRawFeature();
    virtual OGRFeature *GetNextFeature();

    virtual OGRFeature *GetFeature( long nFeatureId );
    
    OGRFeatureDefn *    GetLayerDefn() { return poFeatureDefn; }

    virtual int         TestCapability( const char * );

    virtual const char *GetFIDColumn();

    virtual OGRErr      GetExtent( OGREnvelope *psExtent, int bForce );
};

/************************************************************************/
/*                           OGRMDBDataSource                            */
/************************************************************************/

class OGRMDBDataSource : public OGRDataSource
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

    const char          *GetName() { return pszName; }
    int                 GetLayerCount() { return nLayers; }
    OGRLayer            *GetLayer( int );
    OGRLayer            *GetLayerByName( const char* pszLayerName );

    int                 TestCapability( const char * );
};

/************************************************************************/
/*                             OGRMDBDriver                             */
/************************************************************************/

class OGRMDBDriver : public OGRSFDriver
{
  public:
                ~OGRMDBDriver();

    const char  *GetName();
    OGRDataSource *Open( const char *, int );

    int          TestCapability( const char * );
};

#endif /* ndef _OGR_MDB_H_INCLUDED */
