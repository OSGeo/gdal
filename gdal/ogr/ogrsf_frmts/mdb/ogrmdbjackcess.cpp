/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRMDBJavaEnv class.
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

#include "ogr_mdb.h"

CPL_CVSID("$Id$");

static JavaVM *jvm_static = NULL;
static JNIEnv *env_static = NULL;

/************************************************************************/
/*                         OGRMDBJavaEnv()                              */
/************************************************************************/

OGRMDBJavaEnv::OGRMDBJavaEnv()
{
    jvm = NULL;
    env = NULL;
    bCalledFromJava = FALSE;

    byteArray_class = NULL;

    file_class = NULL;
    file_constructor = NULL;
    database_class = NULL;
    database_open = NULL;
    database_close = NULL;
    database_getTableNames = NULL;
    database_getTable = NULL;

    table_class = NULL;
    table_getColumns = NULL;
    table_iterator = NULL;
    table_getRowCount = NULL;

    column_class = NULL;
    column_getName = NULL;
    column_getType = NULL;
    column_getLength = NULL;
    column_isVariableLength = NULL;

    datatype_class = NULL;
    datatype_getValue = NULL;

    list_class = NULL;
    list_iterator = NULL;

    set_class = NULL;
    set_iterator = NULL;

    map_class = NULL;
    map_get = NULL;

    iterator_class = NULL;
    iterator_hasNext = NULL;
    iterator_next = NULL;

    object_class = NULL;
    object_toString = NULL;
    object_getClass = NULL;

    boolean_class = NULL;
    boolean_booleanValue = NULL;

    byte_class = NULL;
    byte_byteValue = NULL;

    short_class = NULL;
    short_shortValue = NULL;

    integer_class = NULL;
    integer_intValue = NULL;

    float_class = NULL;
    float_floatValue = NULL;

    double_class = NULL;
    double_doubleValue = NULL;
}

/************************************************************************/
/*                        ~OGRMDBJavaEnv()                              */
/************************************************************************/

OGRMDBJavaEnv::~OGRMDBJavaEnv()
{
    if (jvm)
    {
        env->DeleteLocalRef(byteArray_class);

        env->DeleteLocalRef(file_class);
        env->DeleteLocalRef(database_class);

        env->DeleteLocalRef(table_class);

        env->DeleteLocalRef(column_class);

        env->DeleteLocalRef(datatype_class);

        env->DeleteLocalRef(list_class);

        env->DeleteLocalRef(set_class);

        env->DeleteLocalRef(map_class);

        env->DeleteLocalRef(iterator_class);

        env->DeleteLocalRef(object_class);

        env->DeleteLocalRef(boolean_class);
        env->DeleteLocalRef(byte_class);
        env->DeleteLocalRef(short_class);
        env->DeleteLocalRef(integer_class);
        env->DeleteLocalRef(float_class);
        env->DeleteLocalRef(double_class);

        /*if (!bCalledFromJava)
        {
            CPLDebug("MDB", "Destroying JVM");
            int ret = jvm->DestroyJavaVM();
            CPLDebug("MDB", "ret=%d", ret);
        }*/
    }
}

#define CHECK(x, y) do {x = y; if (!x) { CPLError(CE_Failure, CPLE_AppDefined, #y " failed"); return FALSE;} } while(0)

/************************************************************************/
/*                              Init()                                  */
/************************************************************************/

int OGRMDBJavaEnv::Init()
{
    if (jvm_static == NULL)
    {
        JavaVM* vmBuf[1];
        jsize nVMs;

        /* Are we already called from Java ? */
        if (JNI_GetCreatedJavaVMs(vmBuf, 1, &nVMs) == JNI_OK && nVMs == 1)
        {
            jvm = vmBuf[0];
            if (jvm->GetEnv((void **)&env, JNI_VERSION_1_2) == JNI_OK)
            {
                bCalledFromJava = TRUE;
            }
            else
            {
                jvm = NULL;
                env = NULL;
            }
        }
        else
        {
            JavaVMInitArgs args;
            JavaVMOption options[1];
            args.version = JNI_VERSION_1_2;
            const char* pszClassPath = CPLGetConfigOption("CLASSPATH", NULL);
            CPLString osClassPathOption;
            if (pszClassPath)
            {
                args.nOptions = 1;
                osClassPathOption.Printf("-Djava.class.path=%s", pszClassPath);
                options[0].optionString = (char*) osClassPathOption.c_str();
                args.options = options;
            }
            else
                args.nOptions = 0;
            args.ignoreUnrecognized = JNI_FALSE;

            int ret = JNI_CreateJavaVM(&jvm, (void **)&env, &args);
            if (ret != 0 || jvm == NULL || env == NULL)
            {
                CPLError(CE_Failure, CPLE_AppDefined, "JNI_CreateJavaVM failed (%d)", ret);
                return FALSE;
            }

            jvm_static = jvm;
            env_static = env;
        }
    }
    else
    {
        jvm = jvm_static;
        env = env_static;
    }

    CHECK(byteArray_class, env->FindClass("[B"));
    CHECK(file_class, env->FindClass("java/io/File"));
    CHECK(file_constructor, env->GetMethodID(file_class, "<init>", "(Ljava/lang/String;)V"));
    CHECK(database_class, env->FindClass("com/healthmarketscience/jackcess/Database"));

    CHECK(database_open, env->GetStaticMethodID(database_class, "open", "(Ljava/io/File;Z)Lcom/healthmarketscience/jackcess/Database;"));
    CHECK(database_close, env->GetMethodID(database_class, "close", "()V"));
    CHECK(database_getTableNames, env->GetMethodID(database_class, "getTableNames", "()Ljava/util/Set;"));
    CHECK(database_getTable, env->GetMethodID(database_class, "getTable", "(Ljava/lang/String;)Lcom/healthmarketscience/jackcess/Table;"));

    CHECK(table_class, env->FindClass("com/healthmarketscience/jackcess/Table"));
    CHECK(table_getColumns, env->GetMethodID(table_class, "getColumns", "()Ljava/util/List;"));
    CHECK(table_iterator, env->GetMethodID(table_class, "iterator", "()Ljava/util/Iterator;"));
    CHECK(table_getRowCount, env->GetMethodID(table_class, "getRowCount", "()I"));

    CHECK(column_class, env->FindClass("com/healthmarketscience/jackcess/Column"));
    CHECK(column_getName, env->GetMethodID(column_class, "getName", "()Ljava/lang/String;"));
    CHECK(column_getType, env->GetMethodID(column_class, "getType", "()Lcom/healthmarketscience/jackcess/DataType;"));
    CHECK(column_getLength, env->GetMethodID(column_class, "getLength", "()S"));
    CHECK(column_isVariableLength, env->GetMethodID(column_class, "isVariableLength", "()Z"));

    CHECK(datatype_class, env->FindClass("com/healthmarketscience/jackcess/DataType"));
    CHECK(datatype_getValue, env->GetMethodID(datatype_class, "getValue", "()B"));

    CHECK(list_class, env->FindClass("java/util/List"));
    CHECK(list_iterator, env->GetMethodID(list_class, "iterator", "()Ljava/util/Iterator;"));

    CHECK(set_class, env->FindClass("java/util/Set"));
    CHECK(set_iterator, env->GetMethodID(set_class, "iterator", "()Ljava/util/Iterator;"));

    CHECK(map_class, env->FindClass("java/util/Map"));
    CHECK(map_get, env->GetMethodID(map_class, "get", "(Ljava/lang/Object;)Ljava/lang/Object;"));

    CHECK(iterator_class,  env->FindClass("java/util/Iterator"));
    CHECK(iterator_hasNext, env->GetMethodID(iterator_class, "hasNext", "()Z"));
    CHECK(iterator_next, env->GetMethodID(iterator_class, "next", "()Ljava/lang/Object;"));

    CHECK(object_class,  env->FindClass("java/lang/Object"));
    CHECK(object_toString, env->GetMethodID(object_class, "toString", "()Ljava/lang/String;"));
    CHECK(object_getClass, env->GetMethodID(object_class, "getClass", "()Ljava/lang/Class;"));

    CHECK(boolean_class,  env->FindClass("java/lang/Boolean"));
    CHECK(boolean_booleanValue, env->GetMethodID(boolean_class, "booleanValue", "()Z"));

    CHECK(byte_class,  env->FindClass("java/lang/Byte"));
    CHECK(byte_byteValue, env->GetMethodID(byte_class, "byteValue", "()B"));

    CHECK(short_class,  env->FindClass("java/lang/Short"));
    CHECK(short_shortValue, env->GetMethodID(short_class, "shortValue", "()S"));

    CHECK(integer_class,  env->FindClass("java/lang/Integer"));
    CHECK(integer_intValue, env->GetMethodID(integer_class, "intValue", "()I"));

    CHECK(float_class,  env->FindClass("java/lang/Float"));
    CHECK(float_floatValue, env->GetMethodID(float_class, "floatValue", "()F"));

    CHECK(double_class,  env->FindClass("java/lang/Double"));
    CHECK(double_doubleValue, env->GetMethodID(integer_class, "doubleValue", "()D"));

    return TRUE;
}


/************************************************************************/
/*                       ExceptionOccured()                             */
/************************************************************************/

int OGRMDBJavaEnv::ExceptionOccured()
{
    jthrowable exc = env->ExceptionOccurred();
    if (exc)
    {
         env->ExceptionDescribe();
         env->ExceptionClear();
         return TRUE;
    }
    return FALSE;
}


/************************************************************************/
/*                           OGRMDBDatabase()                           */
/************************************************************************/

OGRMDBDatabase::OGRMDBDatabase()
{
    env = NULL;
    database = NULL;
}

/************************************************************************/
/*                          ~OGRMDBDatabase()                           */
/************************************************************************/

OGRMDBDatabase::~OGRMDBDatabase()
{
    if (database)
    {
        CPLDebug("MDB", "Closing database");
        env->env->CallVoidMethod(database, env->database_close);

        env->env->DeleteGlobalRef(database);
    }
}

/************************************************************************/
/*                               Open()                                 */
/************************************************************************/

OGRMDBDatabase* OGRMDBDatabase::Open(OGRMDBJavaEnv* env, const char* pszName)
{
    jstring jstr = env->env->NewStringUTF(pszName);
    jobject file = env->env->NewObject(env->file_class, env->file_constructor, jstr);
    if (env->ExceptionOccured()) return NULL;
    env->env->ReleaseStringUTFChars(jstr, NULL);

    jobject database = env->env->CallStaticObjectMethod(env->database_class, env->database_open, file, JNI_TRUE);

    env->env->DeleteLocalRef(file);

    if (env->ExceptionOccured()) return NULL;
    if (database == NULL)
        return NULL;

    OGRMDBDatabase* poDB = new OGRMDBDatabase();
    poDB->env = env;
    poDB->database = env->env->NewGlobalRef(database);
    env->env->DeleteLocalRef(database);
    return poDB;
}

/************************************************************************/
/*                        FetchTableNames()                             */
/************************************************************************/

int OGRMDBDatabase::FetchTableNames()
{
    if (env->bCalledFromJava)
        env->Init();

    jobject table_set = env->env->CallObjectMethod(database, env->database_getTableNames);
    if (env->ExceptionOccured()) return FALSE;
    jobject iterator = env->env->CallObjectMethod(table_set, env->set_iterator);
    if (env->ExceptionOccured()) return FALSE;

    while( env->env->CallBooleanMethod(iterator, env->iterator_hasNext) )
    {
        if (env->ExceptionOccured()) return FALSE;
        jstring table_name_jstring = (jstring) env->env->CallObjectMethod(iterator, env->iterator_next);
        if (env->ExceptionOccured()) return FALSE;
        jboolean is_copy;
        const char* table_name_str = env->env->GetStringUTFChars(table_name_jstring, &is_copy);

        apoTableNames.push_back(table_name_str);
        //CPLDebug("MDB", "Table %s", table_name_str);

        env->env->ReleaseStringUTFChars(table_name_jstring, table_name_str);
        env->env->DeleteLocalRef(table_name_jstring);
    }
    env->env->DeleteLocalRef(iterator);
    env->env->DeleteLocalRef(table_set);
    return TRUE;
}

/************************************************************************/
/*                            GetTable()                                */
/************************************************************************/

OGRMDBTable* OGRMDBDatabase::GetTable(const char* pszTableName)
{
    if (env->bCalledFromJava)
        env->Init();

    jstring table_name_jstring = env->env->NewStringUTF(pszTableName);
    jobject table = env->env->CallObjectMethod(database, env->database_getTable, table_name_jstring);
    if (env->ExceptionOccured()) return NULL;
    env->env->DeleteLocalRef(table_name_jstring);

    if (!table)
        return NULL;

    jobject global_table = env->env->NewGlobalRef(table);
    env->env->DeleteLocalRef(table);
    table = global_table;

    OGRMDBTable* poTable = new OGRMDBTable(env, this, table, pszTableName);
    if (!poTable->FetchColumns())
    {
        delete poTable;
        return NULL;
    }
    return poTable;
}

/************************************************************************/
/*                           OGRMDBTable()                              */
/************************************************************************/

OGRMDBTable::OGRMDBTable(OGRMDBJavaEnv* env, OGRMDBDatabase* poDB, jobject table, const char* pszTableName )
{
    this->env = env;
    this->poDB = poDB;
    this->table = table;
    osTableName = pszTableName;
    table_iterator_obj = NULL;
    row = NULL;
}

/************************************************************************/
/*                          ~OGRMDBTable()                              */
/************************************************************************/

OGRMDBTable::~OGRMDBTable()
{
    if (env)
    {
        //CPLDebug("MDB", "Freeing table %s", osTableName.c_str());
        if (env->bCalledFromJava)
            env->Init();

        int i;
        for(i=0;i<(int)apoColumnNameObjects.size();i++)
            env->env->DeleteGlobalRef(apoColumnNameObjects[i]);

        env->env->DeleteGlobalRef(table_iterator_obj);
        env->env->DeleteGlobalRef(row);
        env->env->DeleteGlobalRef(table);
    }
}

/************************************************************************/
/*                          FetchColumns()                              */
/************************************************************************/

int OGRMDBTable::FetchColumns()
{
    if (env->bCalledFromJava)
        env->Init();

    jobject column_lists = env->env->CallObjectMethod(table, env->table_getColumns);
    if (env->ExceptionOccured()) return FALSE;

    jobject iterator_cols = env->env->CallObjectMethod(column_lists, env->list_iterator);
    if (env->ExceptionOccured()) return FALSE;

    while( env->env->CallBooleanMethod(iterator_cols, env->iterator_hasNext) )
    {
        if (env->ExceptionOccured()) return FALSE;

        jobject column = env->env->CallObjectMethod(iterator_cols, env->iterator_next);
        if (env->ExceptionOccured()) return FALSE;

        jstring column_name_jstring = (jstring) env->env->CallObjectMethod(column, env->column_getName);
        if (env->ExceptionOccured()) return FALSE;
        jboolean is_copy;
        const char* column_name_str = env->env->GetStringUTFChars(column_name_jstring, &is_copy);
        apoColumnNames.push_back(column_name_str);
        env->env->ReleaseStringUTFChars(column_name_jstring, column_name_str);

        apoColumnNameObjects.push_back((jstring) env->env->NewGlobalRef(column_name_jstring));
        env->env->DeleteLocalRef(column_name_jstring);

        jobject column_type = env->env->CallObjectMethod(column, env->column_getType);
        if (env->ExceptionOccured()) return FALSE;
        int type = env->env->CallByteMethod(column_type, env->datatype_getValue);
        if (env->ExceptionOccured()) return FALSE;
        apoColumnTypes.push_back(type);

        int isvariablelength = env->env->CallBooleanMethod(column, env->column_isVariableLength);
        if (env->ExceptionOccured()) return FALSE;
        if (!isvariablelength)
        {
            int length = env->env->CallShortMethod(column, env->column_getLength);
            if (env->ExceptionOccured()) return FALSE;
            apoColumnLengths.push_back(length);
        }
        else
            apoColumnLengths.push_back(0);

        //CPLDebug("MDB", "Column %s, type = %d", apoColumnNames[apoColumnNames.size()-1].c_str(), type);

        env->env->DeleteLocalRef(column_type);

        env->env->DeleteLocalRef(column);
    }
    env->env->DeleteLocalRef(iterator_cols);
    env->env->DeleteLocalRef(column_lists);

    return TRUE;
}

/************************************************************************/
/*                          ResetReading()                              */
/************************************************************************/

void OGRMDBTable::ResetReading()
{
    if (env->bCalledFromJava)
        env->Init();

    env->env->DeleteGlobalRef(table_iterator_obj);
    table_iterator_obj = NULL;
    env->env->DeleteGlobalRef(row);
    row = NULL;
}

/************************************************************************/
/*                           GetNextRow()                               */
/************************************************************************/

int OGRMDBTable::GetNextRow()
{
    if (env->bCalledFromJava)
        env->Init();

    if (table_iterator_obj == NULL)
    {
        table_iterator_obj = env->env->CallObjectMethod(table, env->table_iterator);
        if (env->ExceptionOccured()) return FALSE;
        if (table_iterator_obj)
        {
            jobject global_table_iterator_obj = env->env->NewGlobalRef(table_iterator_obj);
            env->env->DeleteLocalRef(table_iterator_obj);
            table_iterator_obj = global_table_iterator_obj;
        }
    }
    if (table_iterator_obj == NULL)
        return FALSE;

    if (!env->env->CallBooleanMethod(table_iterator_obj, env->iterator_hasNext))
        return FALSE;
    if (env->ExceptionOccured()) return FALSE;

    if (row)
    {
        env->env->DeleteGlobalRef(row);
        row = NULL;
    }

    row = env->env->CallObjectMethod(table_iterator_obj, env->iterator_next);
    if (env->ExceptionOccured()) return FALSE;
    if (row == NULL)
        return FALSE;

    jobject global_row = env->env->NewGlobalRef(row);
    env->env->DeleteLocalRef(row);
    row = global_row;

    return TRUE;
}

/************************************************************************/
/*                          GetColumnVal()                              */
/************************************************************************/

jobject OGRMDBTable::GetColumnVal(int iCol)
{
    if (row == NULL)
        return NULL;

    jobject val = env->env->CallObjectMethod(row, env->map_get, apoColumnNameObjects[iCol]);
    if (env->ExceptionOccured()) return NULL;
    return val;
}

/************************************************************************/
/*                        GetColumnAsString()                           */
/************************************************************************/

char* OGRMDBTable::GetColumnAsString(int iCol)
{
    jobject val = GetColumnVal(iCol);
    if (!val) return NULL;

    jstring val_jstring = (jstring) env->env->CallObjectMethod(val, env->object_toString);
    if (env->ExceptionOccured()) return NULL;
    jboolean is_copy;
    const char* val_str = env->env->GetStringUTFChars(val_jstring, &is_copy);
    char* dup_str = (val_str) ? CPLStrdup(val_str) : NULL;
    env->env->ReleaseStringUTFChars(val_jstring, val_str);
    env->env->DeleteLocalRef(val_jstring);

    env->env->DeleteLocalRef(val);

    return dup_str;
}

/************************************************************************/
/*                          GetColumnAsInt()                            */
/************************************************************************/

int OGRMDBTable::GetColumnAsInt(int iCol)
{
    jobject val = GetColumnVal(iCol);
    if (!val) return 0;

    int int_val = 0;
    if (apoColumnTypes[iCol] == MDB_Boolean)
        int_val = env->env->CallBooleanMethod(val, env->boolean_booleanValue);
    else if (apoColumnTypes[iCol] == MDB_Byte)
        int_val = env->env->CallByteMethod(val, env->byte_byteValue);
    else if (apoColumnTypes[iCol] == MDB_Short)
        int_val = env->env->CallShortMethod(val, env->short_shortValue);
    else if (apoColumnTypes[iCol] == MDB_Int)
        int_val = env->env->CallIntMethod(val, env->integer_intValue);
    if (env->ExceptionOccured()) return 0;

    env->env->DeleteLocalRef(val);

    return int_val;
}

/************************************************************************/
/*                        GetColumnAsDouble()                           */
/************************************************************************/

double OGRMDBTable::GetColumnAsDouble(int iCol)
{
    jobject val = GetColumnVal(iCol);
    if (!val) return 0;

    double double_val = 0;
    if (apoColumnTypes[iCol] == MDB_Double)
        double_val = env->env->CallDoubleMethod(val, env->double_doubleValue);
    else if (apoColumnTypes[iCol] == MDB_Float)
        double_val = env->env->CallFloatMethod(val, env->float_floatValue);
    if (env->ExceptionOccured()) return 0;

    env->env->DeleteLocalRef(val);

    return double_val;
}

/************************************************************************/
/*                        GetColumnAsBinary()                           */
/************************************************************************/

GByte* OGRMDBTable::GetColumnAsBinary(int iCol, int* pnBytes)
{
    *pnBytes = 0;

    jobject val = GetColumnVal(iCol);
    if (!val) return NULL;

    if (!env->env->IsInstanceOf(val, env->byteArray_class))
        return NULL;

    jbyteArray byteArray = (jbyteArray) val;
    *pnBytes = env->env->GetArrayLength(byteArray);
    if (env->ExceptionOccured()) return NULL;
    jboolean is_copy;
    jbyte* elts = env->env->GetByteArrayElements(byteArray, &is_copy);
    if (env->ExceptionOccured()) return NULL;

    GByte* pData = (GByte*)CPLMalloc(*pnBytes);
    memcpy(pData, elts, *pnBytes);

    env->env->ReleaseByteArrayElements(byteArray, elts, JNI_ABORT);

    env->env->DeleteLocalRef(val);

    return pData;
}

/************************************************************************/
/*                              DumpTable()                             */
/************************************************************************/

void OGRMDBTable::DumpTable()
{
    ResetReading();
    int iRow = 0;
    int nCols = apoColumnNames.size();
    while(GetNextRow())
    {
        printf("Row = %d\n", iRow ++);
        for(int i=0;i<nCols;i++)
        {
            printf("%s = ", apoColumnNames[i].c_str());
            if (apoColumnTypes[i] == MDB_Float ||
                apoColumnTypes[i] == MDB_Double)
            {
                printf("%.15f\n", GetColumnAsDouble(i));
            }
            else if (apoColumnTypes[i] == MDB_Boolean ||
                     apoColumnTypes[i] == MDB_Byte ||
                     apoColumnTypes[i] == MDB_Short ||
                     apoColumnTypes[i] == MDB_Int)
            {
                printf("%d\n", GetColumnAsInt(i));
            }
            else if (apoColumnTypes[i] == MDB_Binary ||
                     apoColumnTypes[i] == MDB_OLE)
            {
                int nBytes;
                GByte* pData = GetColumnAsBinary(i, &nBytes);
                printf("(%d bytes)\n", nBytes);
                CPLFree(pData);
            }
            else
            {
                char* val = GetColumnAsString(i);
                printf("'%s'\n", val);
                CPLFree(val);
            }
        }
    }
}

/************************************************************************/
/*                            GetColumnIndex()                          */
/************************************************************************/

int OGRMDBTable::GetColumnIndex(const char* pszColName, int bEmitErrorIfNotFound)
{
    int nCols = apoColumnNames.size();
    CPLString osColName(pszColName);
    for(int i=0;i<nCols;i++)
    {
        if (apoColumnNames[i] == osColName)
            return i;
    }
    if (bEmitErrorIfNotFound)
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot find column %s", pszColName);
    return -1;
}

/************************************************************************/
/*                             GetRowCount()                            */
/************************************************************************/

int OGRMDBTable::GetRowCount()
{
    if (env->bCalledFromJava)
        env->Init();
    int nRowCount = env->env->CallIntMethod(table, env->table_getRowCount);
    if (env->ExceptionOccured()) return 0;
    return nRowCount;
}
