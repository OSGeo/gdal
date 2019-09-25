/*****************************************************************************
* Copyright 2016 Pitney Bowes Inc.
*
* Licensed under the MIT License (the “License”); you may not use this file
* except in the compliance with the License.
* You may obtain a copy of the License at https://opensource.org/licenses/MIT

* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an “AS IS” WITHOUT
* WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*****************************************************************************/

#ifndef EFALLIB_H
#define EFALLIB_H
#if defined(_MSC_VER)
#pragma once
#endif

#include <EFALAPI.h>
#include <EFAL.h>

#ifndef __EFALDLL__
class EFALLIB
{
private:
    typedef EFALHANDLE(__cdecl *InitializeSessionProc)(ResourceStringCallback resourceStringCallback);
    typedef void(__cdecl *DestroySessionProc)(EFALHANDLE hSession);
    typedef void(__cdecl *GetDataProc)(EFALHANDLE hSession, char bytes[], size_t nBytes);
    typedef bool(__cdecl *HaveErrorsProc)(EFALHANDLE hSession);
    typedef void(__cdecl *ClearErrorsProc)(EFALHANDLE hSession);
    typedef int(__cdecl *NumErrorsProc)(EFALHANDLE hSession);
    typedef const wchar_t *(__cdecl *GetErrorProc)(EFALHANDLE hSession, int ierror);
    typedef void(__cdecl *CloseAllProc)(EFALHANDLE hSession);
    typedef EFALHANDLE(__cdecl *OpenTableProc)(EFALHANDLE hSession, const wchar_t * path);
    typedef void(__cdecl *CloseTableProc)(EFALHANDLE hSession, EFALHANDLE hTable);
    typedef bool(__cdecl *BeginReadAccessProc)(EFALHANDLE hSession, EFALHANDLE hTable);
    typedef bool(__cdecl *BeginWriteAccessProc)(EFALHANDLE hSession, EFALHANDLE hTable);
    typedef void(__cdecl *EndAccessProc)(EFALHANDLE hSession, EFALHANDLE hTable);
    typedef MI_UINT32(__cdecl *GetTableCountProc)(EFALHANDLE hSession);
    typedef EFALHANDLE(__cdecl *GetTableHandleByIndexProc)(EFALHANDLE hSession, MI_UINT32 idx);
    typedef EFALHANDLE(__cdecl *GetTableHandleByAliasProc)(EFALHANDLE hSession, const wchar_t * alias);
    typedef bool(__cdecl *SupportsPackProc)(EFALHANDLE hSession, EFALHANDLE hTable, Ellis::ETablePackType ePackType);
    typedef bool(__cdecl *PackProc)(EFALHANDLE hSession, EFALHANDLE hTable, Ellis::ETablePackType ePackType);
    typedef const wchar_t *(__cdecl *CoordSys2PRJStringProc)(EFALHANDLE hSession, const wchar_t * csys);
    typedef const wchar_t *(__cdecl *CoordSys2MBStringProc)(EFALHANDLE hSession, const wchar_t * csys);
    typedef const wchar_t *(__cdecl *PRJ2CoordSysStringProc)(EFALHANDLE hSession, const wchar_t * csys);
    typedef const wchar_t *(__cdecl *MB2CoordSysStringProc)(EFALHANDLE hSession, const wchar_t * csys);
    typedef const wchar_t *(__cdecl *GetTableNameProc)(EFALHANDLE hSession, EFALHANDLE hTable);
    typedef const wchar_t *(__cdecl *GetTableDescriptionProc)(EFALHANDLE hSession, EFALHANDLE hTable);
    typedef const wchar_t *(__cdecl *GetTablePathProc)(EFALHANDLE hSession, EFALHANDLE hTable);
    typedef const wchar_t *(__cdecl *GetTableGUIDProc)(EFALHANDLE hSession, EFALHANDLE hTable);
    typedef Ellis::MICHARSET(__cdecl *GetTableCharsetProc)(EFALHANDLE hSession, EFALHANDLE hTable);
    typedef const wchar_t *(__cdecl *GetTableTypeProc)(EFALHANDLE hSession, EFALHANDLE hTable);
    typedef bool(__cdecl *HasRasterProc)(EFALHANDLE hSession, EFALHANDLE hTable);
    typedef bool(__cdecl *HasGridProc)(EFALHANDLE hSession, EFALHANDLE hTable);
    typedef bool(__cdecl *IsSeamlessProc)(EFALHANDLE hSession, EFALHANDLE hTable);
    typedef bool(__cdecl *IsVectorProc)(EFALHANDLE hSession, EFALHANDLE hTable);
    typedef bool(__cdecl *SupportsInsertProc)(EFALHANDLE hSession, EFALHANDLE hTable);
    typedef bool(__cdecl *SupportsUpdateProc)(EFALHANDLE hSession, EFALHANDLE hTable);
    typedef bool(__cdecl *SupportsDeleteProc)(EFALHANDLE hSession, EFALHANDLE hTable);
    typedef bool(__cdecl *SupportsBeginAccessProc)(EFALHANDLE hSession, EFALHANDLE hTable);
    typedef MI_INT32(__cdecl *GetReadVersionProc)(EFALHANDLE hSession, EFALHANDLE hTable);
    typedef MI_INT32(__cdecl *GetEditVersionProc)(EFALHANDLE hSession, EFALHANDLE hTable);
    typedef MI_UINT32(__cdecl *GetRowCountProc)(EFALHANDLE hSession, EFALHANDLE hTable);
    typedef MI_UINT32(__cdecl *GetColumnCountProc)(EFALHANDLE hSession, EFALHANDLE hTable);
    typedef const wchar_t *(__cdecl *GetColumnNameProc)(EFALHANDLE hSession, EFALHANDLE hTable, MI_UINT32 columnNbr);
    typedef Ellis::ALLTYPE_TYPE(__cdecl *GetColumnTypeProc)(EFALHANDLE hSession, EFALHANDLE hTable, MI_UINT32 columnNbr);
    typedef MI_UINT32(__cdecl *GetColumnWidthProc)(EFALHANDLE hSession, EFALHANDLE hTable, MI_UINT32 columnNbr);
    typedef MI_UINT32(__cdecl *GetColumnDecimalsProc)(EFALHANDLE hSession, EFALHANDLE hTable, MI_UINT32 columnNbr);
    typedef bool(__cdecl *IsColumnIndexedProc)(EFALHANDLE hSession, EFALHANDLE hTable, MI_UINT32 columnNbr);
    typedef bool(__cdecl *IsColumnReadOnlyProc)(EFALHANDLE hSession, EFALHANDLE hTable, MI_UINT32 columnNbr);
    typedef const wchar_t *(__cdecl *GetColumnCSysProc)(EFALHANDLE hSession, EFALHANDLE hTable, MI_UINT32 columnNbr);
    typedef Ellis::DRECT(__cdecl *GetEntireBoundsProc)(EFALHANDLE hSession, EFALHANDLE hTable, MI_UINT32 columnNbr);
    typedef Ellis::DRECT(__cdecl *GetDefaultViewProc)(EFALHANDLE hSession, EFALHANDLE hTable, MI_UINT32 columnNbr);
    typedef MI_UINT32(__cdecl *GetPointObjectCountProc)(EFALHANDLE hSession, EFALHANDLE hTable, MI_UINT32 columnNbr);
    typedef MI_UINT32(__cdecl *GetLineObjectCountProc)(EFALHANDLE hSession, EFALHANDLE hTable, MI_UINT32 columnNbr);
    typedef MI_UINT32(__cdecl *GetAreaObjectCountProc)(EFALHANDLE hSession, EFALHANDLE hTable, MI_UINT32 columnNbr);
    typedef MI_UINT32(__cdecl *GetMiscObjectCountProc)(EFALHANDLE hSession, EFALHANDLE hTable, MI_UINT32 columnNbr);
    typedef bool(__cdecl *HasZProc)(EFALHANDLE hSession, EFALHANDLE hTable, MI_UINT32 columnNbr);
    typedef bool(__cdecl *IsZRangeKnownProc)(EFALHANDLE hSession, EFALHANDLE hTable, MI_UINT32 columnNbr);
    typedef Ellis::DRANGE(__cdecl *GetZRangeProc)(EFALHANDLE hSession, EFALHANDLE hTable, MI_UINT32 columnNbr);
    typedef bool(__cdecl *HasMProc)(EFALHANDLE hSession, EFALHANDLE hTable, MI_UINT32 columnNbr);
    typedef bool(__cdecl *IsMRangeKnownProc)(EFALHANDLE hSession, EFALHANDLE hTable, MI_UINT32 columnNbr);
    typedef Ellis::DRANGE(__cdecl *GetMRangeProc)(EFALHANDLE hSession, EFALHANDLE hTable, MI_UINT32 columnNbr);
    typedef const wchar_t *(__cdecl *GetMetadataProc)(EFALHANDLE hSession, EFALHANDLE hTable, const wchar_t * key);
    typedef EFALHANDLE(__cdecl *EnumerateMetadataProc)(EFALHANDLE hSession, EFALHANDLE hTable);
    typedef void(__cdecl *DisposeMetadataEnumeratorProc)(EFALHANDLE hSession, EFALHANDLE hEnumerator);
    typedef bool(__cdecl *GetNextEntryProc)(EFALHANDLE hSession, EFALHANDLE hEnumerator);
    typedef const wchar_t *(__cdecl *GetCurrentMetadataKeyProc)(EFALHANDLE hSession, EFALHANDLE hEnumerator);
    typedef const wchar_t *(__cdecl *GetCurrentMetadataValueProc)(EFALHANDLE hSession, EFALHANDLE hEnumerator);
    typedef void(__cdecl *SetMetadataProc)(EFALHANDLE hSession, EFALHANDLE hTable, const wchar_t * key, const wchar_t * value);
    typedef void(__cdecl *DeleteMetadataProc)(EFALHANDLE hSession, EFALHANDLE hTable, const wchar_t * key);
    typedef bool(__cdecl *WriteMetadataProc)(EFALHANDLE hSession, EFALHANDLE hTable);
    typedef EFALHANDLE(__cdecl *CreateNativeTableMetadataProc)(EFALHANDLE hSession, const wchar_t * tableName, const wchar_t * tablePath, Ellis::MICHARSET charset);
    typedef EFALHANDLE(__cdecl *CreateNativeXTableMetadataProc)(EFALHANDLE hSession, const wchar_t * tableName, const wchar_t * tablePath, Ellis::MICHARSET charset);
    typedef EFALHANDLE(__cdecl *CreateGeopackageTableMetadataProc)(EFALHANDLE hSession, const wchar_t * tableName, const wchar_t * tablePath, const wchar_t * databasePath, Ellis::MICHARSET charset, bool convertUnsupportedObjects);
    typedef void(__cdecl *AddColumnProc)(EFALHANDLE hSession, EFALHANDLE hTableMetadata, const wchar_t * columnName, Ellis::ALLTYPE_TYPE dataType, bool indexed, MI_UINT32 width, MI_UINT32 decimals, const wchar_t * szCsys);
    typedef EFALHANDLE(__cdecl *CreateTableProc)(EFALHANDLE hSession, EFALHANDLE hTableMetadata);
    typedef void(__cdecl *DestroyTableMetadataProc)(EFALHANDLE hSession, EFALHANDLE hTableMetadata);
    typedef EFALHANDLE(__cdecl *CreateSeamlessTableProc)(EFALHANDLE hSession, const wchar_t * tablePath, const wchar_t * csys, Ellis::MICHARSET charset);
    typedef bool(__cdecl *AddSeamlessComponentTableProc)(EFALHANDLE hSession, EFALHANDLE hSeamlessTable, const wchar_t * componentTablePath, Ellis::DRECT mbr);
    typedef EFALHANDLE(__cdecl *SelectProc)(EFALHANDLE hSession, const wchar_t * txt);
    typedef bool(__cdecl *FetchNextProc)(EFALHANDLE hSession, EFALHANDLE hCursor);
    typedef void(__cdecl *DisposeCursorProc)(EFALHANDLE hSession, EFALHANDLE hCursor);
    typedef MI_INT32(__cdecl *InsertProc)(EFALHANDLE hSession, const wchar_t * txt);
    typedef MI_INT32(__cdecl *UpdateProc)(EFALHANDLE hSession, const wchar_t * txt);
    typedef MI_INT32(__cdecl *DeleteProc)(EFALHANDLE hSession, const wchar_t * txt);
    typedef EFALHANDLE(__cdecl *PrepareProc)(EFALHANDLE hSession, const wchar_t * txt);
    typedef void(__cdecl *DisposeStmtProc)(EFALHANDLE hSession, EFALHANDLE hStmt);
    typedef EFALHANDLE(__cdecl *ExecuteSelectProc)(EFALHANDLE hSession, EFALHANDLE hStmt);
    typedef long(__cdecl *ExecuteInsertProc)(EFALHANDLE hSession, EFALHANDLE hStmt);
    typedef long(__cdecl *ExecuteUpdateProc)(EFALHANDLE hSession, EFALHANDLE hStmt);
    typedef long(__cdecl *ExecuteDeleteProc)(EFALHANDLE hSession, EFALHANDLE hStmt);
    typedef MI_UINT32(__cdecl *GetCursorColumnCountProc)(EFALHANDLE hSession, EFALHANDLE hCursor);
    typedef const wchar_t *(__cdecl *GetCursorColumnNameProc)(EFALHANDLE hSession, EFALHANDLE hCursor, MI_UINT32 columnNbr);
    typedef Ellis::ALLTYPE_TYPE(__cdecl *GetCursorColumnTypeProc)(EFALHANDLE hSession, EFALHANDLE hCursor, MI_UINT32 columnNbr);
    typedef const wchar_t *(__cdecl *GetCursorColumnCSysProc)(EFALHANDLE hSession, EFALHANDLE hCursor, MI_UINT32 columnNbr);
    typedef const wchar_t *(__cdecl *GetCursorCurrentKeyProc)(EFALHANDLE hSession, EFALHANDLE hCursor);
    typedef bool(__cdecl *GetCursorIsNullProc)(EFALHANDLE hSession, EFALHANDLE hCursor, MI_UINT32 columnNbr);
    typedef const wchar_t * (__cdecl *GetCursorValueStringProc)(EFALHANDLE hSession, EFALHANDLE hCursor, MI_UINT32 columnNbr);
    typedef bool(__cdecl *GetCursorValueBooleanProc)(EFALHANDLE hSession, EFALHANDLE hCursor, MI_UINT32 columnNbr);
    typedef double(__cdecl *GetCursorValueDoubleProc)(EFALHANDLE hSession, EFALHANDLE hCursor, MI_UINT32 columnNbr);
    typedef MI_INT64(__cdecl *GetCursorValueInt64Proc)(EFALHANDLE hSession, EFALHANDLE hCursor, MI_UINT32 columnNbr);
    typedef MI_INT32(__cdecl *GetCursorValueInt32Proc)(EFALHANDLE hSession, EFALHANDLE hCursor, MI_UINT32 columnNbr);
    typedef MI_INT16(__cdecl *GetCursorValueInt16Proc)(EFALHANDLE hSession, EFALHANDLE hCursor, MI_UINT32 columnNbr);
    typedef const wchar_t * (__cdecl *GetCursorValueStyleProc)(EFALHANDLE hSession, EFALHANDLE hCursor, MI_UINT32 columnNbr);
    typedef MI_UINT32(__cdecl *PrepareCursorValueBinaryProc)(EFALHANDLE hSession, EFALHANDLE hCursor, MI_UINT32 columnNbr);
    typedef MI_UINT32(__cdecl *PrepareCursorValueGeometryProc)(EFALHANDLE hSession, EFALHANDLE hCursor, MI_UINT32 columnNbr);
    typedef double(__cdecl *GetCursorValueTimespanInMillisecondsProc)(EFALHANDLE hSession, EFALHANDLE hCursor, MI_UINT32 columnNbr);
    typedef EFALTIME(__cdecl *GetCursorValueTimeProc)(EFALHANDLE hSession, EFALHANDLE hCursor, MI_UINT32 columnNbr);
    typedef EFALDATE(__cdecl *GetCursorValueDateProc)(EFALHANDLE hSession, EFALHANDLE hCursor, MI_UINT32 columnNbr);
    typedef EFALDATETIME(__cdecl *GetCursorValueDateTimeProc)(EFALHANDLE hSession, EFALHANDLE hCursor, MI_UINT32 columnNbr);
    typedef bool(__cdecl *CreateVariableProc)(EFALHANDLE hSession, const wchar_t * name);
    typedef void(__cdecl *DropVariableProc)(EFALHANDLE hSession, const wchar_t * name);
    typedef MI_UINT32(__cdecl *GetVariableCountProc)(EFALHANDLE hSession);
    typedef const wchar_t *(__cdecl *GetVariableNameProc)(EFALHANDLE hSession, MI_UINT32 index);
    typedef Ellis::ALLTYPE_TYPE(__cdecl *GetVariableTypeProc)(EFALHANDLE hSession, const wchar_t * name);
    typedef Ellis::ALLTYPE_TYPE(__cdecl *SetVariableValueProc)(EFALHANDLE hSession, const wchar_t * name, const wchar_t * expression);
    typedef bool(__cdecl *GetVariableIsNullProc)(EFALHANDLE hSession, const wchar_t * name);
    typedef const wchar_t * (__cdecl *GetVariableValueStringProc)(EFALHANDLE hSession, const wchar_t * name);
    typedef bool(__cdecl *GetVariableValueBooleanProc)(EFALHANDLE hSession, const wchar_t * name);
    typedef double(__cdecl *GetVariableValueDoubleProc)(EFALHANDLE hSession, const wchar_t * name);
    typedef MI_INT64(__cdecl *GetVariableValueInt64Proc)(EFALHANDLE hSession, const wchar_t * name);
    typedef MI_INT32(__cdecl *GetVariableValueInt32Proc)(EFALHANDLE hSession, const wchar_t * name);
    typedef MI_INT16(__cdecl *GetVariableValueInt16Proc)(EFALHANDLE hSession, const wchar_t * name);
    typedef const wchar_t * (__cdecl *GetVariableValueStyleProc)(EFALHANDLE hSession, const wchar_t * name);
    typedef MI_UINT32(__cdecl *PrepareVariableValueBinaryProc)(EFALHANDLE hSession, const wchar_t * name);
    typedef MI_UINT32(__cdecl *PrepareVariableValueGeometryProc)(EFALHANDLE hSession, const wchar_t * name);
    typedef const wchar_t *(__cdecl *GetVariableColumnCSysProc)(EFALHANDLE hSession, const wchar_t * name);
    typedef double(__cdecl *GetVariableValueTimespanInMillisecondsProc)(EFALHANDLE hSession, const wchar_t * name);
    typedef EFALTIME(__cdecl *GetVariableValueTimeProc)(EFALHANDLE hSession, const wchar_t * name);
    typedef EFALDATE(__cdecl *GetVariableValueDateProc)(EFALHANDLE hSession, const wchar_t * name);
    typedef EFALDATETIME(__cdecl *GetVariableValueDateTimeProc)(EFALHANDLE hSession, const wchar_t * name);
    typedef bool(__cdecl *SetVariableIsNullProc)(EFALHANDLE hSession, const wchar_t * name);
    typedef bool(__cdecl *SetVariableValueStringProc)(EFALHANDLE hSession, const wchar_t * name, const wchar_t * value);
    typedef bool(__cdecl *SetVariableValueBooleanProc)(EFALHANDLE hSession, const wchar_t * name, bool value);
    typedef bool(__cdecl *SetVariableValueDoubleProc)(EFALHANDLE hSession, const wchar_t * name, double value);
    typedef bool(__cdecl *SetVariableValueInt64Proc)(EFALHANDLE hSession, const wchar_t * name, MI_INT64 value);
    typedef bool(__cdecl *SetVariableValueInt32Proc)(EFALHANDLE hSession, const wchar_t * name, MI_INT32 value);
    typedef bool(__cdecl *SetVariableValueInt16Proc)(EFALHANDLE hSession, const wchar_t * name, MI_INT16 value);
    typedef bool(__cdecl *SetVariableValueStyleProc)(EFALHANDLE hSession, const wchar_t * name, const wchar_t * value);
    typedef bool(__cdecl *SetVariableValueBinaryProc)(EFALHANDLE hSession, const wchar_t * name, MI_UINT32 nbytes, const char * value);
    typedef bool(__cdecl *SetVariableValueGeometryProc)(EFALHANDLE hSession, const wchar_t * name, MI_UINT32 nbytes, const char * value, const wchar_t * szcsys);
    typedef bool(__cdecl *SetVariableValueTimespanInMillisecondsProc)(EFALHANDLE hSession, const wchar_t * name, double value);
    typedef bool(__cdecl *SetVariableValueTimeProc)(EFALHANDLE hSession, const wchar_t * name, EFALTIME value);
    typedef bool(__cdecl *SetVariableValueDateProc)(EFALHANDLE hSession, const wchar_t * name, EFALDATE value);
    typedef bool(__cdecl *SetVariableValueDateTimeProc)(EFALHANDLE hSession, const wchar_t * name, EFALDATETIME value);

    dynlib  __efalHandle = nullptr;
    InitializeSessionProc __InitializeSession = NULL;
    DestroySessionProc __DestroySession = NULL;
    GetDataProc __GetData = NULL;
    HaveErrorsProc __HaveErrors = NULL;
    ClearErrorsProc __ClearErrors = NULL;
    NumErrorsProc __NumErrors = NULL;
    GetErrorProc __GetError = NULL;
    CloseAllProc __CloseAll = NULL;
    OpenTableProc __OpenTable = NULL;
    CloseTableProc __CloseTable = NULL;
    BeginReadAccessProc __BeginReadAccess = NULL;
    BeginWriteAccessProc __BeginWriteAccess = NULL;
    EndAccessProc __EndAccess = NULL;
    GetTableCountProc __GetTableCount = NULL;
    GetTableHandleByIndexProc __GetTableHandleByIndex = NULL;
    GetTableHandleByAliasProc __GetTableHandleByAlias = NULL;
    GetTableHandleByAliasProc __GetTableHandleByPath = NULL;
    SupportsPackProc __SupportsPack = NULL;
    PackProc __Pack = NULL;
    CoordSys2PRJStringProc __CoordSys2PRJString = NULL;
    CoordSys2MBStringProc __CoordSys2MBString = NULL;
    PRJ2CoordSysStringProc __PRJ2CoordSysString = NULL;
    MB2CoordSysStringProc __MB2CoordSysString = NULL;
    GetTableNameProc __GetTableName = NULL;
    GetTableDescriptionProc __GetTableDescription = NULL;
    GetTablePathProc __GetTablePath = NULL;
    GetTableGUIDProc __GetTableGUID = NULL;
    GetTableCharsetProc __GetTableCharset = NULL;
    GetTableTypeProc __GetTableType = NULL;
    HasRasterProc __HasRaster = NULL;
    HasGridProc __HasGrid = NULL;
    IsSeamlessProc __IsSeamless = NULL;
    IsVectorProc __IsVector = NULL;
    SupportsInsertProc __SupportsInsert = NULL;
    SupportsUpdateProc __SupportsUpdate = NULL;
    SupportsDeleteProc __SupportsDelete = NULL;
    SupportsBeginAccessProc __SupportsBeginAccess = NULL;
    GetReadVersionProc __GetReadVersion = NULL;
    GetEditVersionProc __GetEditVersion = NULL;
    GetRowCountProc __GetRowCount = NULL;
    GetColumnCountProc __GetColumnCount = NULL;
    GetColumnNameProc __GetColumnName = NULL;
    GetColumnTypeProc __GetColumnType = NULL;
    GetColumnWidthProc __GetColumnWidth = NULL;
    GetColumnDecimalsProc __GetColumnDecimals = NULL;
    IsColumnIndexedProc __IsColumnIndexed = NULL;
    IsColumnReadOnlyProc __IsColumnReadOnly = NULL;
    GetColumnCSysProc __GetColumnCSys = NULL;
    GetEntireBoundsProc __GetEntireBounds = NULL;
    GetDefaultViewProc __GetDefaultView = NULL;
    GetPointObjectCountProc __GetPointObjectCount = NULL;
    GetLineObjectCountProc __GetLineObjectCount = NULL;
    GetAreaObjectCountProc __GetAreaObjectCount = NULL;
    GetMiscObjectCountProc __GetMiscObjectCount = NULL;
    HasZProc __HasZ = NULL;
    IsZRangeKnownProc __IsZRangeKnown = NULL;
    GetZRangeProc __GetZRange = NULL;
    HasMProc __HasM = NULL;
    IsMRangeKnownProc __IsMRangeKnown = NULL;
    GetMRangeProc __GetMRange = NULL;
    GetMetadataProc __GetMetadata = NULL;
    EnumerateMetadataProc __EnumerateMetadata = NULL;
    DisposeMetadataEnumeratorProc __DisposeMetadataEnumerator = NULL;
    GetNextEntryProc __GetNextEntry = NULL;
    GetCurrentMetadataKeyProc __GetCurrentMetadataKey = NULL;
    GetCurrentMetadataValueProc __GetCurrentMetadataValue = NULL;
    SetMetadataProc __SetMetadata = NULL;
    DeleteMetadataProc __DeleteMetadata = NULL;
    WriteMetadataProc __WriteMetadata = NULL;
    CreateNativeTableMetadataProc __CreateNativeTableMetadata = NULL;
    CreateNativeXTableMetadataProc __CreateNativeXTableMetadata = NULL;
    CreateGeopackageTableMetadataProc __CreateGeopackageTableMetadata = NULL;
    AddColumnProc __AddColumn = NULL;
    CreateTableProc __CreateTable = NULL;
    DestroyTableMetadataProc __DestroyTableMetadata = NULL;
    CreateSeamlessTableProc __CreateSeamlessTable = NULL;
    AddSeamlessComponentTableProc __AddSeamlessComponentTable = NULL;
    SelectProc __Select = NULL;
    FetchNextProc __FetchNext = NULL;
    DisposeCursorProc __DisposeCursor = NULL;
    InsertProc __Insert = NULL;
    UpdateProc __Update = NULL;
    DeleteProc __Delete = NULL;
    PrepareProc __Prepare = NULL;
    DisposeStmtProc __DisposeStmt = NULL;
    ExecuteSelectProc __ExecuteSelect = NULL;
    ExecuteInsertProc __ExecuteInsert = NULL;
    ExecuteUpdateProc __ExecuteUpdate = NULL;
    ExecuteDeleteProc __ExecuteDelete = NULL;
    GetCursorColumnCountProc __GetCursorColumnCount = NULL;
    GetCursorColumnNameProc __GetCursorColumnName = NULL;
    GetCursorColumnTypeProc __GetCursorColumnType = NULL;
    GetCursorColumnCSysProc __GetCursorColumnCSys = NULL;
    GetCursorCurrentKeyProc __GetCursorCurrentKey = NULL;
    GetCursorIsNullProc __GetCursorIsNull = NULL;
    GetCursorValueStringProc __GetCursorValueString = NULL;
    GetCursorValueBooleanProc __GetCursorValueBoolean = NULL;
    GetCursorValueDoubleProc __GetCursorValueDouble = NULL;
    GetCursorValueInt64Proc __GetCursorValueInt64 = NULL;
    GetCursorValueInt32Proc __GetCursorValueInt32 = NULL;
    GetCursorValueInt16Proc __GetCursorValueInt16 = NULL;
    GetCursorValueStyleProc __GetCursorValueStyle = NULL;
    PrepareCursorValueBinaryProc __PrepareCursorValueBinary = NULL;
    PrepareCursorValueGeometryProc __PrepareCursorValueGeometry = NULL;
    GetCursorValueTimespanInMillisecondsProc __GetCursorValueTimespanInMilliseconds = NULL;
    GetCursorValueTimeProc __GetCursorValueTime = NULL;
    GetCursorValueDateProc __GetCursorValueDate = NULL;
    GetCursorValueDateTimeProc __GetCursorValueDateTime = NULL;
    CreateVariableProc __CreateVariable = NULL;
    DropVariableProc __DropVariable = NULL;
    GetVariableCountProc __GetVariableCount = NULL;
    GetVariableNameProc __GetVariableName = NULL;
    GetVariableTypeProc __GetVariableType = NULL;
    SetVariableValueProc __SetVariableValue = NULL;
    GetVariableIsNullProc __GetVariableIsNull = NULL;
    GetVariableValueStringProc __GetVariableValueString = NULL;
    GetVariableValueBooleanProc __GetVariableValueBoolean = NULL;
    GetVariableValueDoubleProc __GetVariableValueDouble = NULL;
    GetVariableValueInt64Proc __GetVariableValueInt64 = NULL;
    GetVariableValueInt32Proc __GetVariableValueInt32 = NULL;
    GetVariableValueInt16Proc __GetVariableValueInt16 = NULL;
    GetVariableValueStyleProc __GetVariableValueStyle = NULL;
    PrepareVariableValueBinaryProc __PrepareVariableValueBinary = NULL;
    PrepareVariableValueGeometryProc __PrepareVariableValueGeometry = NULL;
    GetVariableColumnCSysProc __GetVariableColumnCSys = NULL;
    GetVariableValueTimespanInMillisecondsProc __GetVariableValueTimespanInMilliseconds = NULL;
    GetVariableValueTimeProc __GetVariableValueTime = NULL;
    GetVariableValueDateProc __GetVariableValueDate = NULL;
    GetVariableValueDateTimeProc __GetVariableValueDateTime = NULL;
    SetVariableIsNullProc __SetVariableIsNull = NULL;
    SetVariableValueStringProc __SetVariableValueString = NULL;
    SetVariableValueBooleanProc __SetVariableValueBoolean = NULL;
    SetVariableValueDoubleProc __SetVariableValueDouble = NULL;
    SetVariableValueInt64Proc __SetVariableValueInt64 = NULL;
    SetVariableValueInt32Proc __SetVariableValueInt32 = NULL;
    SetVariableValueInt16Proc __SetVariableValueInt16 = NULL;
    SetVariableValueStyleProc __SetVariableValueStyle = NULL;
    SetVariableValueBinaryProc __SetVariableValueBinary = NULL;
    SetVariableValueGeometryProc __SetVariableValueGeometry = NULL;
    SetVariableValueTimespanInMillisecondsProc __SetVariableValueTimespanInMilliseconds = NULL;
    SetVariableValueTimeProc __SetVariableValueTime = NULL;
    SetVariableValueDateProc __SetVariableValueDate = NULL;
    SetVariableValueDateTimeProc __SetVariableValueDateTime = NULL;

    explicit EFALLIB(dynlib efalHandle) :
        __efalHandle(efalHandle),
#if ELLIS_OS_IS_WINOS
        __InitializeSession((InitializeSessionProc)dynlib_sym(efalHandle, "?InitializeSession@EFAL@@YA_KP6APEB_WPEB_W@Z@Z")),
        __DestroySession((DestroySessionProc)dynlib_sym(efalHandle, "?DestroySession@EFAL@@YAX_K@Z")),
        __GetData((GetDataProc)dynlib_sym(efalHandle, "?GetData@EFAL@@YAX_KQEAD0@Z")),
        __HaveErrors((HaveErrorsProc)dynlib_sym(efalHandle, "?HaveErrors@EFAL@@YA_N_K@Z")),
        __ClearErrors((ClearErrorsProc)dynlib_sym(efalHandle, "?ClearErrors@EFAL@@YAX_K@Z")),
        __NumErrors((NumErrorsProc)dynlib_sym(efalHandle, "?NumErrors@EFAL@@YAH_K@Z")),
        __GetError((GetErrorProc)dynlib_sym(efalHandle, "?GetError@EFAL@@YAPEB_W_KH@Z")),
        __CloseAll((CloseAllProc)dynlib_sym(efalHandle, "?CloseAll@EFAL@@YAX_K@Z")),
        __OpenTable((OpenTableProc)dynlib_sym(efalHandle, "?OpenTable@EFAL@@YA_K_KPEB_W@Z")),
        __CloseTable((CloseTableProc)dynlib_sym(efalHandle, "?CloseTable@EFAL@@YAX_K0@Z")),
        __BeginReadAccess((BeginReadAccessProc)dynlib_sym(efalHandle, "?BeginReadAccess@EFAL@@YA_N_K0@Z")),
        __BeginWriteAccess((BeginWriteAccessProc)dynlib_sym(efalHandle, "?BeginWriteAccess@EFAL@@YA_N_K0@Z")),
        __EndAccess((EndAccessProc)dynlib_sym(efalHandle, "?EndAccess@EFAL@@YAX_K0@Z")),
        __GetTableCount((GetTableCountProc)dynlib_sym(efalHandle, "?GetTableCount@EFAL@@YAK_K@Z")),
        __GetTableHandleByIndex((GetTableHandleByIndexProc)dynlib_sym(efalHandle, "?GetTableHandle@EFAL@@YA_K_KK@Z")),
        __GetTableHandleByAlias((GetTableHandleByAliasProc)dynlib_sym(efalHandle, "?GetTableHandle@EFAL@@YA_K_KPEB_W@Z")),
        __GetTableHandleByPath((GetTableHandleByAliasProc)dynlib_sym(efalHandle, "?GetTableHandleFromTablePath@EFAL@@YA_K_KPEB_W@Z")),
        __SupportsPack((SupportsPackProc)dynlib_sym(efalHandle, "?SupportsPack@EFAL@@YA_N_K0W4ETablePackType@Ellis@@@Z")),
        __Pack((PackProc)dynlib_sym(efalHandle, "?Pack@EFAL@@YA_N_K0W4ETablePackType@Ellis@@@Z")),
        __CoordSys2PRJString((CoordSys2PRJStringProc)dynlib_sym(efalHandle, "?CoordSys2PRJString@EFAL@@YAPEB_W_KPEB_W@Z")),
        __CoordSys2MBString((CoordSys2MBStringProc)dynlib_sym(efalHandle, "?CoordSys2MBString@EFAL@@YAPEB_W_KPEB_W@Z")),
        __PRJ2CoordSysString((PRJ2CoordSysStringProc)dynlib_sym(efalHandle, "?PRJ2CoordSysString@EFAL@@YAPEB_W_KPEB_W@Z")),
        __MB2CoordSysString((MB2CoordSysStringProc)dynlib_sym(efalHandle, "?MB2CoordSysString@EFAL@@YAPEB_W_KPEB_W@Z")),
        __GetTableName((GetTableNameProc)dynlib_sym(efalHandle, "?GetTableName@EFAL@@YAPEB_W_K0@Z")),
        __GetTableDescription((GetTableDescriptionProc)dynlib_sym(efalHandle, "?GetTableDescription@EFAL@@YAPEB_W_K0@Z")),
        __GetTablePath((GetTablePathProc)dynlib_sym(efalHandle, "?GetTablePath@EFAL@@YAPEB_W_K0@Z")),
        __GetTableGUID((GetTableGUIDProc)dynlib_sym(efalHandle, "?GetTableGUID@EFAL@@YAPEB_W_K0@Z")),
        __GetTableCharset((GetTableCharsetProc)dynlib_sym(efalHandle, "?GetTableCharset@EFAL@@YA?AW4MICHARSET@Ellis@@_K0@Z")),
        __GetTableType((GetTableTypeProc)dynlib_sym(efalHandle, "?GetTableType@EFAL@@YAPEB_W_K0@Z")),
        __HasRaster((HasRasterProc)dynlib_sym(efalHandle, "?HasRaster@EFAL@@YA_N_K0@Z")),
        __HasGrid((HasGridProc)dynlib_sym(efalHandle, "?HasGrid@EFAL@@YA_N_K0@Z")),
        __IsSeamless((IsSeamlessProc)dynlib_sym(efalHandle, "?IsSeamless@EFAL@@YA_N_K0@Z")),
        __IsVector((IsVectorProc)dynlib_sym(efalHandle, "?IsVector@EFAL@@YA_N_K0@Z")),
        __SupportsInsert((SupportsInsertProc)dynlib_sym(efalHandle, "?SupportsInsert@EFAL@@YA_N_K0@Z")),
        __SupportsUpdate((SupportsUpdateProc)dynlib_sym(efalHandle, "?SupportsUpdate@EFAL@@YA_N_K0@Z")),
        __SupportsDelete((SupportsDeleteProc)dynlib_sym(efalHandle, "?SupportsDelete@EFAL@@YA_N_K0@Z")),
        __SupportsBeginAccess((SupportsBeginAccessProc)dynlib_sym(efalHandle, "?SupportsBeginAccess@EFAL@@YA_N_K0@Z")),
        __GetReadVersion((GetReadVersionProc)dynlib_sym(efalHandle, "?GetReadVersion@EFAL@@YAJ_K0@Z")),
        __GetEditVersion((GetEditVersionProc)dynlib_sym(efalHandle, "?GetEditVersion@EFAL@@YAJ_K0@Z")),
        __GetRowCount((GetRowCountProc)dynlib_sym(efalHandle, "?GetRowCount@EFAL@@YAK_K0@Z")),
        __GetColumnCount((GetColumnCountProc)dynlib_sym(efalHandle, "?GetColumnCount@EFAL@@YAK_K0@Z")),
        __GetColumnName((GetColumnNameProc)dynlib_sym(efalHandle, "?GetColumnName@EFAL@@YAPEB_W_K0K@Z")),
        __GetColumnType((GetColumnTypeProc)dynlib_sym(efalHandle, "?GetColumnType@EFAL@@YA?AW4ALLTYPE_TYPE@Ellis@@_K0K@Z")),
        __GetColumnWidth((GetColumnWidthProc)dynlib_sym(efalHandle, "?GetColumnWidth@EFAL@@YAK_K0K@Z")),
        __GetColumnDecimals((GetColumnDecimalsProc)dynlib_sym(efalHandle, "?GetColumnDecimals@EFAL@@YAK_K0K@Z")),
        __IsColumnIndexed((IsColumnIndexedProc)dynlib_sym(efalHandle, "?IsColumnIndexed@EFAL@@YA_N_K0K@Z")),
        __IsColumnReadOnly((IsColumnReadOnlyProc)dynlib_sym(efalHandle, "?IsColumnReadOnly@EFAL@@YA_N_K0K@Z")),
        __GetColumnCSys((GetColumnCSysProc)dynlib_sym(efalHandle, "?GetColumnCSys@EFAL@@YAPEB_W_K0K@Z")),
        __GetEntireBounds((GetEntireBoundsProc)dynlib_sym(efalHandle, "?GetEntireBounds@EFAL@@YA?AUDRECT@Ellis@@_K0K@Z")),
        __GetDefaultView((GetDefaultViewProc)dynlib_sym(efalHandle, "?GetDefaultView@EFAL@@YA?AUDRECT@Ellis@@_K0K@Z")),
        __GetPointObjectCount((GetPointObjectCountProc)dynlib_sym(efalHandle, "?GetPointObjectCount@EFAL@@YAK_K0K@Z")),
        __GetLineObjectCount((GetLineObjectCountProc)dynlib_sym(efalHandle, "?GetLineObjectCount@EFAL@@YAK_K0K@Z")),
        __GetAreaObjectCount((GetAreaObjectCountProc)dynlib_sym(efalHandle, "?GetAreaObjectCount@EFAL@@YAK_K0K@Z")),
        __GetMiscObjectCount((GetMiscObjectCountProc)dynlib_sym(efalHandle, "?GetMiscObjectCount@EFAL@@YAK_K0K@Z")),
        __HasZ((HasZProc)dynlib_sym(efalHandle, "?HasZ@EFAL@@YA_N_K0K@Z")),
        __IsZRangeKnown((IsZRangeKnownProc)dynlib_sym(efalHandle, "?IsZRangeKnown@EFAL@@YA_N_K0K@Z")),
        __GetZRange((GetZRangeProc)dynlib_sym(efalHandle, "?GetZRange@EFAL@@YA?AUDRANGE@Ellis@@_K0K@Z")),
        __HasM((HasMProc)dynlib_sym(efalHandle, "?HasM@EFAL@@YA_N_K0K@Z")),
        __IsMRangeKnown((IsMRangeKnownProc)dynlib_sym(efalHandle, "?IsMRangeKnown@EFAL@@YA_N_K0K@Z")),
        __GetMRange((GetMRangeProc)dynlib_sym(efalHandle, "?GetMRange@EFAL@@YA?AUDRANGE@Ellis@@_K0K@Z")),
        __GetMetadata((GetMetadataProc)dynlib_sym(efalHandle, "?GetMetadata@EFAL@@YAPEB_W_K0PEB_W@Z")),
        __EnumerateMetadata((EnumerateMetadataProc)dynlib_sym(efalHandle, "?EnumerateMetadata@EFAL@@YA_K_K0@Z")),
        __DisposeMetadataEnumerator((DisposeMetadataEnumeratorProc)dynlib_sym(efalHandle, "?DisposeMetadataEnumerator@EFAL@@YAX_K0@Z")),
        __GetNextEntry((GetNextEntryProc)dynlib_sym(efalHandle, "?GetNextEntry@EFAL@@YA_N_K0@Z")),
        __GetCurrentMetadataKey((GetCurrentMetadataKeyProc)dynlib_sym(efalHandle, "?GetCurrentMetadataKey@EFAL@@YAPEB_W_K0@Z")),
        __GetCurrentMetadataValue((GetCurrentMetadataValueProc)dynlib_sym(efalHandle, "?GetCurrentMetadataValue@EFAL@@YAPEB_W_K0@Z")),
        __SetMetadata((SetMetadataProc)dynlib_sym(efalHandle, "?SetMetadata@EFAL@@YAX_K0PEB_W1@Z")),
        __DeleteMetadata((DeleteMetadataProc)dynlib_sym(efalHandle, "?DeleteMetadata@EFAL@@YAX_K0PEB_W@Z")),
        __WriteMetadata((WriteMetadataProc)dynlib_sym(efalHandle, "?WriteMetadata@EFAL@@YA_N_K0@Z")),
        __CreateNativeTableMetadata((CreateNativeTableMetadataProc)dynlib_sym(efalHandle, "?CreateNativeTableMetadata@EFAL@@YA_K_KPEB_W1W4MICHARSET@Ellis@@@Z")),
        __CreateNativeXTableMetadata((CreateNativeXTableMetadataProc)dynlib_sym(efalHandle, "?CreateNativeXTableMetadata@EFAL@@YA_K_KPEB_W1W4MICHARSET@Ellis@@@Z")),
        __CreateGeopackageTableMetadata((CreateGeopackageTableMetadataProc)dynlib_sym(efalHandle, "?CreateGeopackageTableMetadata@EFAL@@YA_K_KPEB_W11W4MICHARSET@Ellis@@_N@Z")),
        __AddColumn((AddColumnProc)dynlib_sym(efalHandle, "?AddColumn@EFAL@@YAX_K0PEB_WW4ALLTYPE_TYPE@Ellis@@_NKK1@Z")),
        __CreateTable((CreateTableProc)dynlib_sym(efalHandle, "?CreateTable@EFAL@@YA_K_K0@Z")),
        __DestroyTableMetadata((DestroyTableMetadataProc)dynlib_sym(efalHandle, "?DestroyTableMetadata@EFAL@@YAX_K0@Z")),
        __CreateSeamlessTable((CreateSeamlessTableProc)dynlib_sym(efalHandle, "?CreateSeamlessTable@EFAL@@YA_K_KPEB_W1W4MICHARSET@Ellis@@@Z")),
        __AddSeamlessComponentTable((AddSeamlessComponentTableProc)dynlib_sym(efalHandle, "?AddSeamlessComponentTable@EFAL@@YA_N_K0PEB_WUDRECT@Ellis@@@Z")),
        __Select((SelectProc)dynlib_sym(efalHandle, "?Select@EFAL@@YA_K_KPEB_W@Z")),
        __FetchNext((FetchNextProc)dynlib_sym(efalHandle, "?FetchNext@EFAL@@YA_N_K0@Z")),
        __DisposeCursor((DisposeCursorProc)dynlib_sym(efalHandle, "?DisposeCursor@EFAL@@YAX_K0@Z")),
        __Insert((InsertProc)dynlib_sym(efalHandle, "?Insert@EFAL@@YAJ_KPEB_W@Z")),
        __Update((UpdateProc)dynlib_sym(efalHandle, "?Update@EFAL@@YAJ_KPEB_W@Z")),
        __Delete((DeleteProc)dynlib_sym(efalHandle, "?Delete@EFAL@@YAJ_KPEB_W@Z")),
        __Prepare((PrepareProc)dynlib_sym(efalHandle, "?Prepare@EFAL@@YA_K_KPEB_W@Z")),
        __DisposeStmt((DisposeStmtProc)dynlib_sym(efalHandle, "?DisposeStmt@EFAL@@YAX_K0@Z")),
        __ExecuteSelect((ExecuteSelectProc)dynlib_sym(efalHandle, "?ExecuteSelect@EFAL@@YA_K_K0@Z")),
        __ExecuteInsert((ExecuteInsertProc)dynlib_sym(efalHandle, "?ExecuteInsert@EFAL@@YAJ_K0@Z")),
        __ExecuteUpdate((ExecuteUpdateProc)dynlib_sym(efalHandle, "?ExecuteUpdate@EFAL@@YAJ_K0@Z")),
        __ExecuteDelete((ExecuteDeleteProc)dynlib_sym(efalHandle, "?ExecuteDelete@EFAL@@YAJ_K0@Z")),
        __GetCursorColumnCount((GetCursorColumnCountProc)dynlib_sym(efalHandle, "?GetCursorColumnCount@EFAL@@YAK_K0@Z")),
        __GetCursorColumnName((GetCursorColumnNameProc)dynlib_sym(efalHandle, "?GetCursorColumnName@EFAL@@YAPEB_W_K0K@Z")),
        __GetCursorColumnType((GetCursorColumnTypeProc)dynlib_sym(efalHandle, "?GetCursorColumnType@EFAL@@YA?AW4ALLTYPE_TYPE@Ellis@@_K0K@Z")),
        __GetCursorColumnCSys((GetCursorColumnCSysProc)dynlib_sym(efalHandle, "?GetCursorColumnCSys@EFAL@@YAPEB_W_K0K@Z")),
        __GetCursorCurrentKey((GetCursorCurrentKeyProc)dynlib_sym(efalHandle, "?GetCursorCurrentKey@EFAL@@YAPEB_W_K0@Z")),
        __GetCursorIsNull((GetCursorIsNullProc)dynlib_sym(efalHandle, "?GetCursorIsNull@EFAL@@YA_N_K0K@Z")),
        __GetCursorValueString((GetCursorValueStringProc)dynlib_sym(efalHandle, "?GetCursorValueString@EFAL@@YAPEB_W_K0K@Z")),
        __GetCursorValueBoolean((GetCursorValueBooleanProc)dynlib_sym(efalHandle, "?GetCursorValueBoolean@EFAL@@YA_N_K0K@Z")),
        __GetCursorValueDouble((GetCursorValueDoubleProc)dynlib_sym(efalHandle, "?GetCursorValueDouble@EFAL@@YAN_K0K@Z")),
        __GetCursorValueInt64((GetCursorValueInt64Proc)dynlib_sym(efalHandle, "?GetCursorValueInt64@EFAL@@YA_J_K0K@Z")),
        __GetCursorValueInt32((GetCursorValueInt32Proc)dynlib_sym(efalHandle, "?GetCursorValueInt32@EFAL@@YAJ_K0K@Z")),
        __GetCursorValueInt16((GetCursorValueInt16Proc)dynlib_sym(efalHandle, "?GetCursorValueInt16@EFAL@@YAF_K0K@Z")),
        __GetCursorValueStyle((GetCursorValueStyleProc)dynlib_sym(efalHandle, "?GetCursorValueStyle@EFAL@@YAPEB_W_K0K@Z")),
        __PrepareCursorValueBinary((PrepareCursorValueBinaryProc)dynlib_sym(efalHandle, "?PrepareCursorValueBinary@EFAL@@YAK_K0K@Z")),
        __PrepareCursorValueGeometry((PrepareCursorValueGeometryProc)dynlib_sym(efalHandle, "?PrepareCursorValueGeometry@EFAL@@YAK_K0K@Z")),
        __GetCursorValueTimespanInMilliseconds((GetCursorValueTimespanInMillisecondsProc)dynlib_sym(efalHandle, "?GetCursorValueTimespanInMilliseconds@EFAL@@YAN_K0K@Z")),
        __GetCursorValueTime((GetCursorValueTimeProc)dynlib_sym(efalHandle, "?GetCursorValueTime@EFAL@@YA?AUEFALTIME@@_K0K@Z")),
        __GetCursorValueDate((GetCursorValueDateProc)dynlib_sym(efalHandle, "?GetCursorValueDate@EFAL@@YA?AUEFALDATE@@_K0K@Z")),
        __GetCursorValueDateTime((GetCursorValueDateTimeProc)dynlib_sym(efalHandle, "?GetCursorValueDateTime@EFAL@@YA?AUEFALDATETIME@@_K0K@Z")),
        __CreateVariable((CreateVariableProc)dynlib_sym(efalHandle, "?CreateVariable@EFAL@@YA_N_KPEB_W@Z")),
        __DropVariable((DropVariableProc)dynlib_sym(efalHandle, "?DropVariable@EFAL@@YAX_KPEB_W@Z")),
        __GetVariableCount((GetVariableCountProc)dynlib_sym(efalHandle, "?GetVariableCount@EFAL@@YAK_K@Z")),
        __GetVariableName((GetVariableNameProc)dynlib_sym(efalHandle, "?GetVariableName@EFAL@@YAPEB_W_KK@Z")),
        __GetVariableType((GetVariableTypeProc)dynlib_sym(efalHandle, "?GetVariableType@EFAL@@YA?AW4ALLTYPE_TYPE@Ellis@@_KPEB_W@Z")),
        __SetVariableValue((SetVariableValueProc)dynlib_sym(efalHandle, "?SetVariableValue@EFAL@@YA?AW4ALLTYPE_TYPE@Ellis@@_KPEB_W1@Z")),
        __GetVariableIsNull((GetVariableIsNullProc)dynlib_sym(efalHandle, "?GetVariableIsNull@EFAL@@YA_N_KPEB_W@Z")),
        __GetVariableValueString((GetVariableValueStringProc)dynlib_sym(efalHandle, "?GetVariableValueString@EFAL@@YAPEB_W_KPEB_W@Z")),
        __GetVariableValueBoolean((GetVariableValueBooleanProc)dynlib_sym(efalHandle, "?GetVariableValueBoolean@EFAL@@YA_N_KPEB_W@Z")),
        __GetVariableValueDouble((GetVariableValueDoubleProc)dynlib_sym(efalHandle, "?GetVariableValueDouble@EFAL@@YAN_KPEB_W@Z")),
        __GetVariableValueInt64((GetVariableValueInt64Proc)dynlib_sym(efalHandle, "?GetVariableValueInt64@EFAL@@YA_J_KPEB_W@Z")),
        __GetVariableValueInt32((GetVariableValueInt32Proc)dynlib_sym(efalHandle, "?GetVariableValueInt32@EFAL@@YAJ_KPEB_W@Z")),
        __GetVariableValueInt16((GetVariableValueInt16Proc)dynlib_sym(efalHandle, "?GetVariableValueInt16@EFAL@@YAF_KPEB_W@Z")),
        __GetVariableValueStyle((GetVariableValueStyleProc)dynlib_sym(efalHandle, "?GetVariableValueStyle@EFAL@@YAPEB_W_KPEB_W@Z")),
        __PrepareVariableValueBinary((PrepareVariableValueBinaryProc)dynlib_sym(efalHandle, "?PrepareVariableValueBinary@EFAL@@YAK_KPEB_W@Z")),
        __PrepareVariableValueGeometry((PrepareVariableValueGeometryProc)dynlib_sym(efalHandle, "?PrepareVariableValueGeometry@EFAL@@YAK_KPEB_W@Z")),
        __GetVariableColumnCSys((GetVariableColumnCSysProc)dynlib_sym(efalHandle, "?GetVariableColumnCSys@EFAL@@YAPEB_W_KPEB_W@Z")),
        __GetVariableValueTimespanInMilliseconds((GetVariableValueTimespanInMillisecondsProc)dynlib_sym(efalHandle, "?GetVariableValueTimespanInMilliseconds@EFAL@@YAN_KPEB_W@Z")),
        __GetVariableValueTime((GetVariableValueTimeProc)dynlib_sym(efalHandle, "?GetVariableValueTime@EFAL@@YA?AUEFALTIME@@_KPEB_W@Z")),
        __GetVariableValueDate((GetVariableValueDateProc)dynlib_sym(efalHandle, "?GetVariableValueDate@EFAL@@YA?AUEFALDATE@@_KPEB_W@Z")),
        __GetVariableValueDateTime((GetVariableValueDateTimeProc)dynlib_sym(efalHandle, "?GetVariableValueDateTime@EFAL@@YA?AUEFALDATETIME@@_KPEB_W@Z")),
        __SetVariableIsNull((SetVariableIsNullProc)dynlib_sym(efalHandle, "?SetVariableIsNull@EFAL@@YA_N_KPEB_W@Z")),
        __SetVariableValueString((SetVariableValueStringProc)dynlib_sym(efalHandle, "?SetVariableValueString@EFAL@@YA_N_KPEB_W1@Z")),
        __SetVariableValueBoolean((SetVariableValueBooleanProc)dynlib_sym(efalHandle, "?SetVariableValueBoolean@EFAL@@YA_N_KPEB_W_N@Z")),
        __SetVariableValueDouble((SetVariableValueDoubleProc)dynlib_sym(efalHandle, "?SetVariableValueDouble@EFAL@@YA_N_KPEB_WN@Z")),
        __SetVariableValueInt64((SetVariableValueInt64Proc)dynlib_sym(efalHandle, "?SetVariableValueInt64@EFAL@@YA_N_KPEB_W_J@Z")),
        __SetVariableValueInt32((SetVariableValueInt32Proc)dynlib_sym(efalHandle, "?SetVariableValueInt32@EFAL@@YA_N_KPEB_WJ@Z")),
        __SetVariableValueInt16((SetVariableValueInt16Proc)dynlib_sym(efalHandle, "?SetVariableValueInt16@EFAL@@YA_N_KPEB_WF@Z")),
        __SetVariableValueStyle((SetVariableValueStyleProc)dynlib_sym(efalHandle, "?SetVariableValueStyle@EFAL@@YA_N_KPEB_W1@Z")),
        __SetVariableValueBinary((SetVariableValueBinaryProc)dynlib_sym(efalHandle, "?SetVariableValueBinary@EFAL@@YA_N_KPEB_WKPEBD@Z")),
        __SetVariableValueGeometry((SetVariableValueGeometryProc)dynlib_sym(efalHandle, "?SetVariableValueGeometry@EFAL@@YA_N_KPEB_WKPEBD1@Z")),
        __SetVariableValueTimespanInMilliseconds((SetVariableValueTimespanInMillisecondsProc)dynlib_sym(efalHandle, "?SetVariableValueTimespanInMilliseconds@EFAL@@YA_N_KPEB_WN@Z")),
        __SetVariableValueTime((SetVariableValueTimeProc)dynlib_sym(efalHandle, "?SetVariableValueTime@EFAL@@YA_N_KPEB_WUEFALTIME@@@Z")),
        __SetVariableValueDate((SetVariableValueDateProc)dynlib_sym(efalHandle, "?SetVariableValueDate@EFAL@@YA_N_KPEB_WUEFALDATE@@@Z")),
        __SetVariableValueDateTime((SetVariableValueDateTimeProc)dynlib_sym(efalHandle, "?SetVariableValueDateTime@EFAL@@YA_N_KPEB_WUEFALDATETIME@@@Z"))
#else
        __InitializeSession((InitializeSessionProc)dynlib_sym(efalHandle, "_ZN4EFAL17InitializeSessionEPFPKwS1_E")),
        __DestroySession((DestroySessionProc)dynlib_sym(efalHandle, "_ZN4EFAL14DestroySessionEy")),
        __GetData((GetDataProc)dynlib_sym(efalHandle, "_ZN4EFAL7GetDataEyPcm")),
        __HaveErrors((HaveErrorsProc)dynlib_sym(efalHandle, "_ZN4EFAL10HaveErrorsEy")),
        __ClearErrors((ClearErrorsProc)dynlib_sym(efalHandle, "_ZN4EFAL11ClearErrorsEy")),
        __NumErrors((NumErrorsProc)dynlib_sym(efalHandle, "_ZN4EFAL9NumErrorsEy")),
        __GetError((GetErrorProc)dynlib_sym(efalHandle, "_ZN4EFAL8GetErrorEyi")),
        __CloseAll((CloseAllProc)dynlib_sym(efalHandle, "_ZN4EFAL8CloseAllEy")),
        __OpenTable((OpenTableProc)dynlib_sym(efalHandle, "_ZN4EFAL9OpenTableEyPKw")),
        __CloseTable((CloseTableProc)dynlib_sym(efalHandle, "_ZN4EFAL10CloseTableEyy")),
        __BeginReadAccess((BeginReadAccessProc)dynlib_sym(efalHandle, "_ZN4EFAL15BeginReadAccessEyy")),
        __BeginWriteAccess((BeginWriteAccessProc)dynlib_sym(efalHandle, "_ZN4EFAL16BeginWriteAccessEyy")),
        __EndAccess((EndAccessProc)dynlib_sym(efalHandle, "_ZN4EFAL9EndAccessEyy")),
        __GetTableCount((GetTableCountProc)dynlib_sym(efalHandle, "_ZN4EFAL13GetTableCountEy")),
        __GetTableHandleByIndex((GetTableHandleByIndexProc)dynlib_sym(efalHandle, "_ZN4EFAL14GetTableHandleEyj")),
        __GetTableHandleByAlias((GetTableHandleByAliasProc)dynlib_sym(efalHandle, "_ZN4EFAL14GetTableHandleEyPKw")),
        __GetTableHandleByPath((GetTableHandleByAliasProc)dynlib_sym(efalHandle, "_ZN4EFAL14GetTableHandleFromTablePathEyPKw")),
        __SupportsPack((SupportsPackProc)dynlib_sym(efalHandle, "_ZN4EFAL12SupportsPackEyyN5Ellis14ETablePackTypeE")),
        __Pack((PackProc)dynlib_sym(efalHandle, "_ZN4EFAL4PackEyyN5Ellis14ETablePackTypeE")),
        __CoordSys2PRJString((CoordSys2PRJStringProc)dynlib_sym(efalHandle, "_ZN4EFAL18CoordSys2PRJStringEyPKw")),
        __CoordSys2MBString((CoordSys2MBStringProc)dynlib_sym(efalHandle, "_ZN4EFAL17CoordSys2MBStringEyPKw")),
        __PRJ2CoordSysString((PRJ2CoordSysStringProc)dynlib_sym(efalHandle, "_ZN4EFAL18PRJ2CoordSysStringEyPKw")),
        __MB2CoordSysString((MB2CoordSysStringProc)dynlib_sym(efalHandle, "_ZN4EFAL17MB2CoordSysStringEyPKw")),
        __GetTableName((GetTableNameProc)dynlib_sym(efalHandle, "_ZN4EFAL12GetTableNameEyy")),
        __GetTableDescription((GetTableDescriptionProc)dynlib_sym(efalHandle, "_ZN4EFAL19GetTableDescriptionEyy")),
        __GetTablePath((GetTablePathProc)dynlib_sym(efalHandle, "_ZN4EFAL12GetTablePathEyy")),
        __GetTableGUID((GetTableGUIDProc)dynlib_sym(efalHandle, "_ZN4EFAL12GetTableGUIDEyy")),
        __GetTableCharset((GetTableCharsetProc)dynlib_sym(efalHandle, "_ZN4EFAL15GetTableCharsetEyy")),
        __GetTableType((GetTableTypeProc)dynlib_sym(efalHandle, "_ZN4EFAL12GetTableTypeEyy")),
        __HasRaster((HasRasterProc)dynlib_sym(efalHandle, "_ZN4EFAL9HasRasterEyy")),
        __HasGrid((HasGridProc)dynlib_sym(efalHandle, "_ZN4EFAL7HasGridEyy")),
        __IsSeamless((IsSeamlessProc)dynlib_sym(efalHandle, "_ZN4EFAL10IsSeamlessEyy")),
        __IsVector((IsVectorProc)dynlib_sym(efalHandle, "_ZN4EFAL8IsVectorEyy")),
        __SupportsInsert((SupportsInsertProc)dynlib_sym(efalHandle, "_ZN4EFAL14SupportsInsertEyy")),
        __SupportsUpdate((SupportsUpdateProc)dynlib_sym(efalHandle, "_ZN4EFAL14SupportsUpdateEyy")),
        __SupportsDelete((SupportsDeleteProc)dynlib_sym(efalHandle, "_ZN4EFAL14SupportsDeleteEyy")),
        __SupportsBeginAccess((SupportsBeginAccessProc)dynlib_sym(efalHandle, "_ZN4EFAL19SupportsBeginAccessEyy")),
        __GetReadVersion((GetReadVersionProc)dynlib_sym(efalHandle, "_ZN4EFAL14GetReadVersionEyy")),
        __GetEditVersion((GetEditVersionProc)dynlib_sym(efalHandle, "_ZN4EFAL14GetEditVersionEyy")),
        __GetRowCount((GetRowCountProc)dynlib_sym(efalHandle, "_ZN4EFAL11GetRowCountEyy")),
        __GetColumnCount((GetColumnCountProc)dynlib_sym(efalHandle, "_ZN4EFAL14GetColumnCountEyy")),
        __GetColumnName((GetColumnNameProc)dynlib_sym(efalHandle, "_ZN4EFAL13GetColumnNameEyyj")),
        __GetColumnType((GetColumnTypeProc)dynlib_sym(efalHandle, "_ZN4EFAL13GetColumnTypeEyyj")),
        __GetColumnWidth((GetColumnWidthProc)dynlib_sym(efalHandle, "_ZN4EFAL14GetColumnWidthEyyj")),
        __GetColumnDecimals((GetColumnDecimalsProc)dynlib_sym(efalHandle, "_ZN4EFAL17GetColumnDecimalsEyyj")),
        __IsColumnIndexed((IsColumnIndexedProc)dynlib_sym(efalHandle, "_ZN4EFAL15IsColumnIndexedEyyj")),
        __IsColumnReadOnly((IsColumnReadOnlyProc)dynlib_sym(efalHandle, "_ZN4EFAL16IsColumnReadOnlyEyyj")),
        __GetColumnCSys((GetColumnCSysProc)dynlib_sym(efalHandle, "_ZN4EFAL13GetColumnCSysEyyj")),
        __GetEntireBounds((GetEntireBoundsProc)dynlib_sym(efalHandle, "_ZN4EFAL15GetEntireBoundsEyyj")),
        __GetDefaultView((GetDefaultViewProc)dynlib_sym(efalHandle, "_ZN4EFAL14GetDefaultViewEyyj")),
        __GetPointObjectCount((GetPointObjectCountProc)dynlib_sym(efalHandle, "_ZN4EFAL19GetPointObjectCountEyyj")),
        __GetLineObjectCount((GetLineObjectCountProc)dynlib_sym(efalHandle, "_ZN4EFAL18GetLineObjectCountEyyj")),
        __GetAreaObjectCount((GetAreaObjectCountProc)dynlib_sym(efalHandle, "_ZN4EFAL18GetAreaObjectCountEyyj")),
        __GetMiscObjectCount((GetMiscObjectCountProc)dynlib_sym(efalHandle, "_ZN4EFAL18GetMiscObjectCountEyyj")),
        __HasZ((HasZProc)dynlib_sym(efalHandle, "_ZN4EFAL4HasZEyyj")),
        __IsZRangeKnown((IsZRangeKnownProc)dynlib_sym(efalHandle, "_ZN4EFAL13IsZRangeKnownEyyj")),
        __GetZRange((GetZRangeProc)dynlib_sym(efalHandle, "_ZN4EFAL9GetZRangeEyyj")),
        __HasM((HasMProc)dynlib_sym(efalHandle, "_ZN4EFAL4HasMEyyj")),
        __IsMRangeKnown((IsMRangeKnownProc)dynlib_sym(efalHandle, "_ZN4EFAL13IsMRangeKnownEyyj")),
        __GetMRange((GetMRangeProc)dynlib_sym(efalHandle, "_ZN4EFAL9GetMRangeEyyj")),
        __GetMetadata((GetMetadataProc)dynlib_sym(efalHandle, "_ZN4EFAL11GetMetadataEyyPKw")),
        __EnumerateMetadata((EnumerateMetadataProc)dynlib_sym(efalHandle, "_ZN4EFAL17EnumerateMetadataEyy")),
        __DisposeMetadataEnumerator((DisposeMetadataEnumeratorProc)dynlib_sym(efalHandle, "_ZN4EFAL25DisposeMetadataEnumeratorEyy")),
        __GetNextEntry((GetNextEntryProc)dynlib_sym(efalHandle, "_ZN4EFAL12GetNextEntryEyy")),
        __GetCurrentMetadataKey((GetCurrentMetadataKeyProc)dynlib_sym(efalHandle, "_ZN4EFAL21GetCurrentMetadataKeyEyy")),
        __GetCurrentMetadataValue((GetCurrentMetadataValueProc)dynlib_sym(efalHandle, "_ZN4EFAL23GetCurrentMetadataValueEyy")),
        __SetMetadata((SetMetadataProc)dynlib_sym(efalHandle, "_ZN4EFAL11SetMetadataEyyPKwS1_")),
        __DeleteMetadata((DeleteMetadataProc)dynlib_sym(efalHandle, "_ZN4EFAL14DeleteMetadataEyyPKw")),
        __WriteMetadata((WriteMetadataProc)dynlib_sym(efalHandle, "_ZN4EFAL13WriteMetadataEyy")),
        __CreateNativeTableMetadata((CreateNativeTableMetadataProc)dynlib_sym(efalHandle, "_ZN4EFAL25CreateNativeTableMetadataEyPKwS1_N5Ellis9MICHARSETE")),
        __CreateNativeXTableMetadata((CreateNativeXTableMetadataProc)dynlib_sym(efalHandle, "_ZN4EFAL26CreateNativeXTableMetadataEyPKwS1_N5Ellis9MICHARSETE")),
        __CreateGeopackageTableMetadata((CreateGeopackageTableMetadataProc)dynlib_sym(efalHandle, "_ZN4EFAL29CreateGeopackageTableMetadataEyPKwS1_S1_N5Ellis9MICHARSETEb")),
        __AddColumn((AddColumnProc)dynlib_sym(efalHandle, "_ZN4EFAL9AddColumnEyyPKwN5Ellis12ALLTYPE_TYPEEbjjS1_")),
        __CreateTable((CreateTableProc)dynlib_sym(efalHandle, "_ZN4EFAL11CreateTableEyy")),
        __DestroyTableMetadata((DestroyTableMetadataProc)dynlib_sym(efalHandle, "_ZN4EFAL20DestroyTableMetadataEyy")),
        __CreateSeamlessTable((CreateSeamlessTableProc)dynlib_sym(efalHandle, "_ZN4EFAL19CreateSeamlessTableEyPKwS1_N5Ellis9MICHARSETE")),
        __AddSeamlessComponentTable((AddSeamlessComponentTableProc)dynlib_sym(efalHandle, "_ZN4EFAL25AddSeamlessComponentTableEyyPKwN5Ellis5DRECTE")),
        __Select((SelectProc)dynlib_sym(efalHandle, "_ZN4EFAL6SelectEyPKw")),
        __FetchNext((FetchNextProc)dynlib_sym(efalHandle, "_ZN4EFAL9FetchNextEyy")),
        __DisposeCursor((DisposeCursorProc)dynlib_sym(efalHandle, "_ZN4EFAL13DisposeCursorEyy")),
        __Insert((InsertProc)dynlib_sym(efalHandle, "_ZN4EFAL6InsertEyPKw")),
        __Update((UpdateProc)dynlib_sym(efalHandle, "_ZN4EFAL6UpdateEyPKw")),
        __Delete((DeleteProc)dynlib_sym(efalHandle, "_ZN4EFAL6DeleteEyPKw")),
        __Prepare((PrepareProc)dynlib_sym(efalHandle, "_ZN4EFAL7PrepareEyPKw")),
        __DisposeStmt((DisposeStmtProc)dynlib_sym(efalHandle, "_ZN4EFAL11DisposeStmtEyy")),
        __ExecuteSelect((ExecuteSelectProc)dynlib_sym(efalHandle, "_ZN4EFAL13ExecuteSelectEyy")),
        __ExecuteInsert((ExecuteInsertProc)dynlib_sym(efalHandle, "_ZN4EFAL13ExecuteInsertEyy")),
        __ExecuteUpdate((ExecuteUpdateProc)dynlib_sym(efalHandle, "_ZN4EFAL13ExecuteUpdateEyy")),
        __ExecuteDelete((ExecuteDeleteProc)dynlib_sym(efalHandle, "_ZN4EFAL13ExecuteDeleteEyy")),
        __GetCursorColumnCount((GetCursorColumnCountProc)dynlib_sym(efalHandle, "_ZN4EFAL20GetCursorColumnCountEyy")),
        __GetCursorColumnName((GetCursorColumnNameProc)dynlib_sym(efalHandle, "_ZN4EFAL19GetCursorColumnNameEyyj")),
        __GetCursorColumnType((GetCursorColumnTypeProc)dynlib_sym(efalHandle, "_ZN4EFAL19GetCursorColumnTypeEyyj")),
        __GetCursorColumnCSys((GetCursorColumnCSysProc)dynlib_sym(efalHandle, "_ZN4EFAL19GetCursorColumnCSysEyyj")),
        __GetCursorCurrentKey((GetCursorCurrentKeyProc)dynlib_sym(efalHandle, "_ZN4EFAL19GetCursorCurrentKeyEyy")),
        __GetCursorIsNull((GetCursorIsNullProc)dynlib_sym(efalHandle, "_ZN4EFAL15GetCursorIsNullEyyj")),
        __GetCursorValueString((GetCursorValueStringProc)dynlib_sym(efalHandle, "_ZN4EFAL20GetCursorValueStringEyyj")),
        __GetCursorValueBoolean((GetCursorValueBooleanProc)dynlib_sym(efalHandle, "_ZN4EFAL21GetCursorValueBooleanEyyj")),
        __GetCursorValueDouble((GetCursorValueDoubleProc)dynlib_sym(efalHandle, "_ZN4EFAL20GetCursorValueDoubleEyyj")),
        __GetCursorValueInt64((GetCursorValueInt64Proc)dynlib_sym(efalHandle, "_ZN4EFAL19GetCursorValueInt64Eyyj")),
        __GetCursorValueInt32((GetCursorValueInt32Proc)dynlib_sym(efalHandle, "_ZN4EFAL19GetCursorValueInt32Eyyj")),
        __GetCursorValueInt16((GetCursorValueInt16Proc)dynlib_sym(efalHandle, "_ZN4EFAL19GetCursorValueInt16Eyyj")),
        __GetCursorValueStyle((GetCursorValueStyleProc)dynlib_sym(efalHandle, "_ZN4EFAL19GetCursorValueStyleEyyj")),
        __PrepareCursorValueBinary((PrepareCursorValueBinaryProc)dynlib_sym(efalHandle, "_ZN4EFAL24PrepareCursorValueBinaryEyyj")),
        __PrepareCursorValueGeometry((PrepareCursorValueGeometryProc)dynlib_sym(efalHandle, "_ZN4EFAL26PrepareCursorValueGeometryEyyj")),
        __GetCursorValueTimespanInMilliseconds((GetCursorValueTimespanInMillisecondsProc)dynlib_sym(efalHandle, "_ZN4EFAL36GetCursorValueTimespanInMillisecondsEyyj")),
        __GetCursorValueTime((GetCursorValueTimeProc)dynlib_sym(efalHandle, "_ZN4EFAL18GetCursorValueTimeEyyj")),
        __GetCursorValueDate((GetCursorValueDateProc)dynlib_sym(efalHandle, "_ZN4EFAL18GetCursorValueDateEyyj")),
        __GetCursorValueDateTime((GetCursorValueDateTimeProc)dynlib_sym(efalHandle, "_ZN4EFAL22GetCursorValueDateTimeEyyj")),
        __CreateVariable((CreateVariableProc)dynlib_sym(efalHandle, "_ZN4EFAL14CreateVariableEyPKw")),
        __DropVariable((DropVariableProc)dynlib_sym(efalHandle, "_ZN4EFAL12DropVariableEyPKw")),
        __GetVariableCount((GetVariableCountProc)dynlib_sym(efalHandle, "_ZN4EFAL16GetVariableCountEy")),
        __GetVariableName((GetVariableNameProc)dynlib_sym(efalHandle, "_ZN4EFAL15GetVariableNameEyj")),
        __GetVariableType((GetVariableTypeProc)dynlib_sym(efalHandle, "_ZN4EFAL15GetVariableTypeEyPKw")),
        __SetVariableValue((SetVariableValueProc)dynlib_sym(efalHandle, "_ZN4EFAL16SetVariableValueEyPKwS1_")),
        __GetVariableIsNull((GetVariableIsNullProc)dynlib_sym(efalHandle, "_ZN4EFAL17GetVariableIsNullEyPKw")),
        __GetVariableValueString((GetVariableValueStringProc)dynlib_sym(efalHandle, "_ZN4EFAL22GetVariableValueStringEyPKw")),
        __GetVariableValueBoolean((GetVariableValueBooleanProc)dynlib_sym(efalHandle, "_ZN4EFAL23GetVariableValueBooleanEyPKw")),
        __GetVariableValueDouble((GetVariableValueDoubleProc)dynlib_sym(efalHandle, "_ZN4EFAL22GetVariableValueDoubleEyPKw")),
        __GetVariableValueInt64((GetVariableValueInt64Proc)dynlib_sym(efalHandle, "_ZN4EFAL21GetVariableValueInt64EyPKw")),
        __GetVariableValueInt32((GetVariableValueInt32Proc)dynlib_sym(efalHandle, "_ZN4EFAL21GetVariableValueInt32EyPKw")),
        __GetVariableValueInt16((GetVariableValueInt16Proc)dynlib_sym(efalHandle, "_ZN4EFAL21GetVariableValueInt16EyPKw")),
        __GetVariableValueStyle((GetVariableValueStyleProc)dynlib_sym(efalHandle, "_ZN4EFAL21GetVariableValueStyleEyPKw")),
        __PrepareVariableValueBinary((PrepareVariableValueBinaryProc)dynlib_sym(efalHandle, "_ZN4EFAL26PrepareVariableValueBinaryEyPKw")),
        __PrepareVariableValueGeometry((PrepareVariableValueGeometryProc)dynlib_sym(efalHandle, "_ZN4EFAL28PrepareVariableValueGeometryEyPKw")),
        __GetVariableColumnCSys((GetVariableColumnCSysProc)dynlib_sym(efalHandle, "_ZN4EFAL21GetVariableColumnCSysEyPKw")),
        __GetVariableValueTimespanInMilliseconds((GetVariableValueTimespanInMillisecondsProc)dynlib_sym(efalHandle, "_ZN4EFAL38GetVariableValueTimespanInMillisecondsEyPKw")),
        __GetVariableValueTime((GetVariableValueTimeProc)dynlib_sym(efalHandle, "_ZN4EFAL20GetVariableValueTimeEyPKw")),
        __GetVariableValueDate((GetVariableValueDateProc)dynlib_sym(efalHandle, "_ZN4EFAL20GetVariableValueDateEyPKw")),
        __GetVariableValueDateTime((GetVariableValueDateTimeProc)dynlib_sym(efalHandle, "_ZN4EFAL24GetVariableValueDateTimeEyPKw")),
        __SetVariableIsNull((SetVariableIsNullProc)dynlib_sym(efalHandle, "_ZN4EFAL17SetVariableIsNullEyPKw")),
        __SetVariableValueString((SetVariableValueStringProc)dynlib_sym(efalHandle, "_ZN4EFAL22SetVariableValueStringEyPKwS1_")),
        __SetVariableValueBoolean((SetVariableValueBooleanProc)dynlib_sym(efalHandle, "_ZN4EFAL23SetVariableValueBooleanEyPKwb")),
        __SetVariableValueDouble((SetVariableValueDoubleProc)dynlib_sym(efalHandle, "_ZN4EFAL22SetVariableValueDoubleEyPKwd")),
        __SetVariableValueInt64((SetVariableValueInt64Proc)dynlib_sym(efalHandle, "_ZN4EFAL21SetVariableValueInt64EyPKwx")),
        __SetVariableValueInt32((SetVariableValueInt32Proc)dynlib_sym(efalHandle, "_ZN4EFAL21SetVariableValueInt32EyPKwi")),
        __SetVariableValueInt16((SetVariableValueInt16Proc)dynlib_sym(efalHandle, "_ZN4EFAL21SetVariableValueInt16EyPKws")),
        __SetVariableValueStyle((SetVariableValueStyleProc)dynlib_sym(efalHandle, "_ZN4EFAL21SetVariableValueStyleEyPKwS1_")),
        __SetVariableValueBinary((SetVariableValueBinaryProc)dynlib_sym(efalHandle, "_ZN4EFAL22SetVariableValueBinaryEyPKwjPKc")),
        __SetVariableValueGeometry((SetVariableValueGeometryProc)dynlib_sym(efalHandle, "_ZN4EFAL24SetVariableValueGeometryEyPKwjPKcS1_")),
        __SetVariableValueTimespanInMilliseconds((SetVariableValueTimespanInMillisecondsProc)dynlib_sym(efalHandle, "_ZN4EFAL38SetVariableValueTimespanInMillisecondsEyPKwd")),
        __SetVariableValueTime((SetVariableValueTimeProc)dynlib_sym(efalHandle, "_ZN4EFAL20SetVariableValueTimeEyPKw8EFALTIME")),
        __SetVariableValueDate((SetVariableValueDateProc)dynlib_sym(efalHandle, "_ZN4EFAL20SetVariableValueDateEyPKw8EFALDATE")),
        __SetVariableValueDateTime((SetVariableValueDateTimeProc)dynlib_sym(efalHandle, "_ZN4EFAL24SetVariableValueDateTimeEyPKw12EFALDATETIME"))
#endif
    {
    }
public:
    static EFALLIB * Create(const char * path = nullptr)
    {
#if ELLIS_OS_ISUNIX
        if (path == nullptr) path = "libEFAL.so";
#else
        if (path == nullptr) path = "EFAL.dll";
#endif
        dynlib handle = dynlib_open(path);
        if (handle != nullptr)
        {
            return new EFALLIB(handle);
        }
        return NULL;
    }
    ~EFALLIB()
    {
        if (__efalHandle)
        {
            dynlib_close(__efalHandle);
        }
        __efalHandle = nullptr;
    }
    /* ***********************************************************
    * These functions were added after the initial release so we
    * have test functions to see if they are defined in the version
    * of EFAL we have loaded.
    * ***********************************************************
    */
    bool HasCoordSys2PRJStringProc()
    {
        return (__CoordSys2PRJString != nullptr);
    }
    bool HasCoordSys2MBStringProc()
    {
        return (__CoordSys2MBString != nullptr);
    }
    bool HasPRJ2CoordSysStringProc()
    {
        return (__PRJ2CoordSysString != nullptr);
    }
    bool HasMB2CoordSysStringProc()
    {
        return (__MB2CoordSysString != nullptr);
    }
    bool HasGetRowCountProc()
    {
        return (__GetRowCount != nullptr);
    }
    bool HasPrepareProc()
    {
        return (__Prepare != nullptr);
    }

    /* ***********************************************************
    * Session
    * ***********************************************************
    */

    /* ***********************************************************
    * InitializeSession : Initializes the EFAL session and returns
    * EFALHANDLE to use in other APIs. User can pass in optional
    * ResourceStringCallback to allow client application to return
    * custom EFAL string resources. If passed as nullptr, default
    * EFAL string resources will be used.
    * ***********************************************************
    */
    EFALHANDLE InitializeSession(ResourceStringCallback resourceStringCallback)
    {
        if (__InitializeSession != nullptr)
        {
            return (__InitializeSession)(resourceStringCallback);
        }
        return 0;
    }
    void DestroySession(EFALHANDLE hSession)
    {
        if (__DestroySession != nullptr)
        {
            (__DestroySession)(hSession);
        }
    }

    /* ***********************************************************
    * Variable length data retrieval (for use after calls to
    * PxrepareCursorValueBinary, PxrepareCursorValueGeometry,
    * PxrepareVariableValueBinary, and PxrepareVariableValueGeometry,
    * ***********************************************************
    */
    void GetData(EFALHANDLE hSession, char bytes[], size_t nBytes)
    {
        if (__GetData != nullptr)
        {
            (__GetData)(hSession, bytes, nBytes);
        }
    }

    /* ***********************************************************
    * Error Handling
    * ***********************************************************
    */
    bool HaveErrors(EFALHANDLE hSession)
    {
        if (__HaveErrors != nullptr)
        {
            return (__HaveErrors)(hSession);
        }
        return false;
    }
    void ClearErrors(EFALHANDLE hSession)
    {
        if (__ClearErrors != nullptr)
        {
            (__ClearErrors)(hSession);
        }
    }
    int NumErrors(EFALHANDLE hSession)
    {
        if (__NumErrors != nullptr)
        {
            return (__NumErrors)(hSession);
        }
        return 0;
    }
    const wchar_t * GetError(EFALHANDLE hSession, int ierror)
    {
        if (__GetError != nullptr)
        {
            return (__GetError)(hSession, ierror);
        }
        return nullptr;
    }

    /* ***********************************************************
    * Table Catalog methods
    * ***********************************************************
    */
    void CloseAll(EFALHANDLE hSession)
    {
        if (__CloseAll != nullptr)
        {
            (__CloseAll)(hSession);
        }
    }
    EFALHANDLE OpenTable(EFALHANDLE hSession, const wchar_t * path)
    {
        if (__OpenTable != nullptr)
        {
            return (__OpenTable)(hSession, path);
        }
        return 0;
    }
    void CloseTable(EFALHANDLE hSession, EFALHANDLE hTable)
    {
        if (__CloseTable != nullptr)
        {
            (__CloseTable)(hSession, hTable);
        }
    }
    bool BeginReadAccess(EFALHANDLE hSession, EFALHANDLE hTable)
    {
        if (__BeginReadAccess != nullptr)
        {
            return (__BeginReadAccess)(hSession, hTable);
        }
        return false;
    }
    bool BeginWriteAccess(EFALHANDLE hSession, EFALHANDLE hTable)
    {
        if (__BeginWriteAccess != nullptr)
        {
            return (__BeginWriteAccess)(hSession, hTable);
        }
        return false;
    }
    void EndAccess(EFALHANDLE hSession, EFALHANDLE hTable)
    {
        if (__EndAccess != nullptr)
        {
            (__EndAccess)(hSession, hTable);
        }
    }
    MI_UINT32 GetTableCount(EFALHANDLE hSession)
    {
        if (__GetTableCount != nullptr)
        {
            return (__GetTableCount)(hSession);
        }
        return 0;
    }
    EFALHANDLE GetTableHandle(EFALHANDLE hSession, MI_UINT32 idx)
    {
        if (__GetTableHandleByIndex != nullptr)
        {
            return (__GetTableHandleByIndex)(hSession, idx);
        }
        return 0;
    }
    EFALHANDLE GetTableHandle(EFALHANDLE hSession, const wchar_t * alias)
    {
        if (__GetTableHandleByAlias != nullptr)
        {
            return (__GetTableHandleByAlias)(hSession, alias);
        }
        return 0;
    }
    EFALHANDLE GetTableHandleFromTablePath(EFALHANDLE hSession, const wchar_t * tablePath)
    {
        if (__GetTableHandleByPath != nullptr)
        {
            return (__GetTableHandleByPath)(hSession, tablePath);
        }
        return 0;
    }
    bool SupportsPack(EFALHANDLE hSession, EFALHANDLE hTable, Ellis::ETablePackType ePackType)
    {
        if (__SupportsPack != nullptr)
        {
            return (__SupportsPack)(hSession, hTable, ePackType);
        }
        return false;
    }
    bool Pack(EFALHANDLE hSession, EFALHANDLE hTable, Ellis::ETablePackType ePackType)
    {
        if (__Pack != nullptr)
        {
            return (__Pack)(hSession, hTable, ePackType);
        }
        return false;
    }

    /* ***********************************************************
    * Utility Methods
    * ***********************************************************
    */
    const wchar_t * CoordSys2PRJString(EFALHANDLE hSession, const wchar_t * csys)
    {
        if (__CoordSys2PRJString != nullptr)
        {
            return (__CoordSys2PRJString)(hSession, csys);
        }
        return nullptr;
    }
    const wchar_t * CoordSys2MBString(EFALHANDLE hSession, const wchar_t * csys)
    {
        if (__CoordSys2MBString != nullptr)
        {
            return (__CoordSys2MBString)(hSession, csys);
        }
        return nullptr;
    }
    const wchar_t * PRJ2CoordSysString(EFALHANDLE hSession, const wchar_t * csys)
    {
        if (__PRJ2CoordSysString != nullptr)
        {
            return (__PRJ2CoordSysString)(hSession, csys);
        }
        return nullptr;
    }
    const wchar_t * MB2CoordSysString(EFALHANDLE hSession, const wchar_t * csys)
    {
        if (__MB2CoordSysString != nullptr)
        {
            return (__MB2CoordSysString)(hSession, csys);
        }
        return nullptr;
    }

    /* ***********************************************************
    * Table Metadata methods
    * ***********************************************************
    */
    const wchar_t * GetTableName(EFALHANDLE hSession, EFALHANDLE hTable)
    {
        if (__GetTableName != nullptr)
        {
            return (__GetTableName)(hSession, hTable);
        }
        return nullptr;
    }
    const wchar_t * GetTableDescription(EFALHANDLE hSession, EFALHANDLE hTable)
    {
        if (__GetTableDescription != nullptr)
        {
            return (__GetTableDescription)(hSession, hTable);
        }
        return nullptr;
    }
    const wchar_t * GetTablePath(EFALHANDLE hSession, EFALHANDLE hTable)
    {
        if (__GetTablePath != nullptr)
        {
            return (__GetTablePath)(hSession, hTable);
        }
        return nullptr;
    }
    const wchar_t * GetTableGUID(EFALHANDLE hSession, EFALHANDLE hTable)
    {
        if (__GetTableGUID != nullptr)
        {
            return (__GetTableGUID)(hSession, hTable);
        }
        return nullptr;
    }
    Ellis::MICHARSET GetTableCharset(EFALHANDLE hSession, EFALHANDLE hTable)
    {
        if (__GetTableCharset != nullptr)
        {
            return (__GetTableCharset)(hSession, hTable);
        }
        return Ellis::MICHARSET::CHARSET_NONE;
    }
    const wchar_t * GetTableType(EFALHANDLE hSession, EFALHANDLE hTable)
    {
        if (__GetTableType != nullptr)
        {
            return (__GetTableType)(hSession, hTable);
        }
        return nullptr;
    }
    bool HasRaster(EFALHANDLE hSession, EFALHANDLE hTable)
    {
        if (__HasRaster != nullptr)
        {
            return (__HasRaster)(hSession, hTable);
        }
        return false;
    }
    bool HasGrid(EFALHANDLE hSession, EFALHANDLE hTable)
    {
        if (__HasGrid != nullptr)
        {
            return (__HasGrid)(hSession, hTable);
        }
        return false;
    }
    bool IsSeamless(EFALHANDLE hSession, EFALHANDLE hTable)
    {
        if (__IsSeamless != nullptr)
        {
            return (__IsSeamless)(hSession, hTable);
        }
        return false;
    }
    bool IsVector(EFALHANDLE hSession, EFALHANDLE hTable)
    {
        if (__IsVector != nullptr)
        {
            return (__IsVector)(hSession, hTable);
        }
        return false;
    }
    bool SupportsInsert(EFALHANDLE hSession, EFALHANDLE hTable)
    {
        if (__SupportsInsert != nullptr)
        {
            return (__SupportsInsert)(hSession, hTable);
        }
        return false;
    }
    bool SupportsUpdate(EFALHANDLE hSession, EFALHANDLE hTable)
    {
        if (__SupportsUpdate != nullptr)
        {
            return (__SupportsUpdate)(hSession, hTable);
        }
        return false;
    }
    bool SupportsDelete(EFALHANDLE hSession, EFALHANDLE hTable)
    {
        if (__SupportsDelete != nullptr)
        {
            return (__SupportsDelete)(hSession, hTable);
        }
        return false;
    }
    bool SupportsBeginAccess(EFALHANDLE hSession, EFALHANDLE hTable)
    {
        if (__SupportsBeginAccess != nullptr)
        {
            return (__SupportsBeginAccess)(hSession, hTable);
        }
        return false;
    }
    MI_INT32 GetReadVersion(EFALHANDLE hSession, EFALHANDLE hTable)
    {
        if (__GetReadVersion != nullptr)
        {
            return (__GetReadVersion)(hSession, hTable);
        }
        return 0;
    }
    MI_INT32 GetEditVersion(EFALHANDLE hSession, EFALHANDLE hTable)
    {
        if (__GetEditVersion != nullptr)
        {
            return (__GetEditVersion)(hSession, hTable);
        }
        return 0;
    }
    MI_UINT32 GetRowCount(EFALHANDLE hSession, EFALHANDLE hTable)
    {
        if (__GetRowCount != nullptr)
        {
            return (__GetRowCount)(hSession, hTable);
        }
        return 0;
    }
    MI_UINT32 GetColumnCount(EFALHANDLE hSession, EFALHANDLE hTable)
    {
        if (__GetColumnCount != nullptr)
        {
            return (__GetColumnCount)(hSession, hTable);
        }
        return 0;
    }
    const wchar_t * GetColumnName(EFALHANDLE hSession, EFALHANDLE hTable, MI_UINT32 columnNbr)
    {
        if (__GetColumnName != nullptr)
        {
            return (__GetColumnName)(hSession, hTable, columnNbr);
        }
        return nullptr;
    }
    Ellis::ALLTYPE_TYPE GetColumnType(EFALHANDLE hSession, EFALHANDLE hTable, MI_UINT32 columnNbr)
    {
        if (__GetColumnType != nullptr)
        {
            return (__GetColumnType)(hSession, hTable, columnNbr);
        }
        return Ellis::ALLTYPE_TYPE::OT_NONE;
    }
    MI_UINT32 GetColumnWidth(EFALHANDLE hSession, EFALHANDLE hTable, MI_UINT32 columnNbr)
    {
        if (__GetColumnWidth != nullptr)
        {
            return (__GetColumnWidth)(hSession, hTable, columnNbr);
        }
        return 0;
    }
    MI_UINT32 GetColumnDecimals(EFALHANDLE hSession, EFALHANDLE hTable, MI_UINT32 columnNbr)
    {
        if (__GetColumnDecimals != nullptr)
        {
            return (__GetColumnDecimals)(hSession, hTable, columnNbr);
        }
        return 0;
    }
    bool IsColumnIndexed(EFALHANDLE hSession, EFALHANDLE hTable, MI_UINT32 columnNbr)
    {
        if (__IsColumnIndexed != nullptr)
        {
            return (__IsColumnIndexed)(hSession, hTable, columnNbr);
        }
        return false;
    }
    bool IsColumnReadOnly(EFALHANDLE hSession, EFALHANDLE hTable, MI_UINT32 columnNbr)
    {
        if (__IsColumnReadOnly != nullptr)
        {
            return (__IsColumnReadOnly)(hSession, hTable, columnNbr);
        }
        return false;
    }
    const wchar_t * GetColumnCSys(EFALHANDLE hSession, EFALHANDLE hTable, MI_UINT32 columnNbr)
    {
        if (__GetColumnCSys != nullptr)
        {
            return (__GetColumnCSys)(hSession, hTable, columnNbr);
        }
        return nullptr;
    }
    Ellis::DRECT GetEntireBounds(EFALHANDLE hSession, EFALHANDLE hTable, MI_UINT32 columnNbr)
    {
        if (__GetEntireBounds != nullptr)
        {
            return (__GetEntireBounds)(hSession, hTable, columnNbr);
        }
        Ellis::DRECT drect; drect.x1 = drect.x2 = drect.y1 = drect.y2 = 0.0;
        return drect;
    }
    Ellis::DRECT GetDefaultView(EFALHANDLE hSession, EFALHANDLE hTable, MI_UINT32 columnNbr)
    {
        if (__GetDefaultView != nullptr)
        {
            return (__GetDefaultView)(hSession, hTable, columnNbr);
        }
        Ellis::DRECT drect; drect.x1 = drect.x2 = drect.y1 = drect.y2 = 0.0;
        return drect;
    }
    MI_UINT32 GetPointObjectCount(EFALHANDLE hSession, EFALHANDLE hTable, MI_UINT32 columnNbr)
    {
        if (__GetPointObjectCount != nullptr)
        {
            return (__GetPointObjectCount)(hSession, hTable, columnNbr);
        }
        return 0;
    }
    MI_UINT32 GetLineObjectCount(EFALHANDLE hSession, EFALHANDLE hTable, MI_UINT32 columnNbr)
    {
        if (__GetLineObjectCount != nullptr)
        {
            return (__GetLineObjectCount)(hSession, hTable, columnNbr);
        }
        return 0;
    }
    MI_UINT32 GetAreaObjectCount(EFALHANDLE hSession, EFALHANDLE hTable, MI_UINT32 columnNbr)
    {
        if (__GetAreaObjectCount != nullptr)
        {
            return (__GetAreaObjectCount)(hSession, hTable, columnNbr);
        }
        return 0;
    }
    MI_UINT32 GetMiscObjectCount(EFALHANDLE hSession, EFALHANDLE hTable, MI_UINT32 columnNbr)
    {
        if (__GetMiscObjectCount != nullptr)
        {
            return (__GetMiscObjectCount)(hSession, hTable, columnNbr);
        }
        return 0;
    }
    bool HasZ(EFALHANDLE hSession, EFALHANDLE hTable, MI_UINT32 columnNbr)
    {
        if (__HasZ != nullptr)
        {
            return (__HasZ)(hSession, hTable, columnNbr);
        }
        return false;
    }
    bool IsZRangeKnown(EFALHANDLE hSession, EFALHANDLE hTable, MI_UINT32 columnNbr)
    {
        if (__IsZRangeKnown != nullptr)
        {
            return (__IsZRangeKnown)(hSession, hTable, columnNbr);
        }
        return false;
    }
    Ellis::DRANGE GetZRange(EFALHANDLE hSession, EFALHANDLE hTable, MI_UINT32 columnNbr)
    {
        if (__GetZRange != nullptr)
        {
            return (__GetZRange)(hSession, hTable, columnNbr);
        }
        Ellis::DRANGE dr = { 0.0d, 0.0d };
        return dr;
    }
    bool HasM(EFALHANDLE hSession, EFALHANDLE hTable, MI_UINT32 columnNbr)
    {
        if (__HasM != nullptr)
        {
            return (__HasM)(hSession, hTable, columnNbr);
        }
        return false;
    }
    bool IsMRangeKnown(EFALHANDLE hSession, EFALHANDLE hTable, MI_UINT32 columnNbr)
    {
        if (__IsMRangeKnown != nullptr)
        {
            return (__IsMRangeKnown)(hSession, hTable, columnNbr);
        }
        return false;
    }
    Ellis::DRANGE GetMRange(EFALHANDLE hSession, EFALHANDLE hTable, MI_UINT32 columnNbr)
    {
        if (__GetMRange != nullptr)
        {
            return (__GetMRange)(hSession, hTable, columnNbr);
        }
        Ellis::DRANGE dr = { 0.0d, 0.0d };
        return dr;
    }

    /* ***********************************************************
    * TAB file Metadata methods
    * ***********************************************************
    */
    const wchar_t * GetMetadata(EFALHANDLE hSession, EFALHANDLE hTable, const wchar_t * key)
    {
        if (__GetMetadata != nullptr)
        {
            return (__GetMetadata)(hSession, hTable, key);
        }
        return nullptr;
    }
    EFALHANDLE EnumerateMetadata(EFALHANDLE hSession, EFALHANDLE hTable)
    {
        if (__EnumerateMetadata != nullptr)
        {
            return (__EnumerateMetadata)(hSession, hTable);
        }
        return 0;
    }
    void DisposeMetadataEnumerator(EFALHANDLE hSession, EFALHANDLE hEnumerator)
    {
        if (__DisposeMetadataEnumerator != nullptr)
        {
            (__DisposeMetadataEnumerator)(hSession, hEnumerator);
        }
    }
    bool GetNextEntry(EFALHANDLE hSession, EFALHANDLE hEnumerator)
    {
        if (__GetNextEntry != nullptr)
        {
            return (__GetNextEntry)(hSession, hEnumerator);
        }
        return false;
    }
    const wchar_t * GetCurrentMetadataKey(EFALHANDLE hSession, EFALHANDLE hEnumerator)
    {
        if (__GetCurrentMetadataKey != nullptr)
        {
            return (__GetCurrentMetadataKey)(hSession, hEnumerator);
        }
        return nullptr;
    }
    const wchar_t * GetCurrentMetadataValue(EFALHANDLE hSession, EFALHANDLE hEnumerator)
    {
        if (__GetCurrentMetadataValue != nullptr)
        {
            return (__GetCurrentMetadataValue)(hSession, hEnumerator);
        }
        return nullptr;
    }
    void SetMetadata(EFALHANDLE hSession, EFALHANDLE hTable, const wchar_t * key, const wchar_t * value)
    {
        if (__SetMetadata != nullptr)
        {
            (__SetMetadata)(hSession, hTable, key, value);
        }
    }
    void DeleteMetadata(EFALHANDLE hSession, EFALHANDLE hTable, const wchar_t * key)
    {
        if (__DeleteMetadata != nullptr)
        {
            (__DeleteMetadata)(hSession, hTable, key);
        }
    }
    bool WriteMetadata(EFALHANDLE hSession, EFALHANDLE hTable)
    {
        if (__WriteMetadata != nullptr)
        {
            return (__WriteMetadata)(hSession, hTable);
        }
        return false;
    }

    /* ***********************************************************
    * Create Table methods
    * ***********************************************************
    */
    // Should data source be an ENUM?
    EFALHANDLE CreateNativeTableMetadata(EFALHANDLE hSession, const wchar_t * tableName, const wchar_t * tablePath, Ellis::MICHARSET charset)
    {
        if (__CreateNativeTableMetadata != nullptr)
        {
            return (__CreateNativeTableMetadata)(hSession, tableName, tablePath, charset);
        }
        return 0;
    }
    EFALHANDLE CreateNativeXTableMetadata(EFALHANDLE hSession, const wchar_t * tableName, const wchar_t * tablePath, Ellis::MICHARSET charset)
    {
        if (__CreateNativeXTableMetadata != nullptr)
        {
            return (__CreateNativeXTableMetadata)(hSession, tableName, tablePath, charset);
        }
        return 0;
    }
    EFALHANDLE CreateGeopackageTableMetadata(EFALHANDLE hSession, const wchar_t * tableName, const wchar_t * tablePath, const wchar_t * databasePath, Ellis::MICHARSET charset, bool convertUnsupportedObjects)
    {
        if (__CreateGeopackageTableMetadata != nullptr)
        {
            return (__CreateGeopackageTableMetadata)(hSession, tableName, tablePath, databasePath, charset, convertUnsupportedObjects);
        }
        return 0;
    }
    void AddColumn(EFALHANDLE hSession, EFALHANDLE hTableMetadata, const wchar_t * columnName, Ellis::ALLTYPE_TYPE dataType, bool indexed, MI_UINT32 width, MI_UINT32 decimals, const wchar_t * szCsys)
    {
        if (__AddColumn != nullptr)
        {
            (__AddColumn)(hSession, hTableMetadata, columnName, dataType, indexed, width, decimals, szCsys);
        }
    }
    EFALHANDLE CreateTable(EFALHANDLE hSession, EFALHANDLE hTableMetadata)
    {
        if (__CreateTable != nullptr)
        {
            return (__CreateTable)(hSession, hTableMetadata);
        }
        return 0;
    }
    void DestroyTableMetadata(EFALHANDLE hSession, EFALHANDLE hTableMetadata)
    {
        if (__DestroyTableMetadata != nullptr)
        {
            (__DestroyTableMetadata)(hSession, hTableMetadata);
        }
    }

    /* ***********************************************************
    * Create Seamless Table methods
    * ***********************************************************
    * A seamless table is a MapInfo TAB file that represents a spatial partitioning of feature
    * records across multiple component TAB file tables. Each component table must have the same
    * schema and same coordinate system. This API exposes two functions for creating a seamless
    * table. CreateSeamlessTable will create an empty seamless TAB file located in the supplied
    * tablePath. AddSeamlessComponentTable will register the specified component TAB file into
    * the seamless table. The registration entry will use the supplied bounds (mbr) unless the
    * mbr values are all zero in which case the component table will be opened and the MBR of the
    * component table data will be used.
    */
    EFALHANDLE CreateSeamlessTable(EFALHANDLE hSession, const wchar_t * tablePath, const wchar_t * csys, Ellis::MICHARSET charset)
    {
        if (__CreateSeamlessTable != nullptr)
        {
            return (__CreateSeamlessTable)(hSession, tablePath, csys, charset);
        }
        return 0;
    }
    bool AddSeamlessComponentTable(EFALHANDLE hSession, EFALHANDLE hSeamlessTable, const wchar_t * componentTablePath, Ellis::DRECT mbr)
    {
        if (__AddSeamlessComponentTable != nullptr)
        {
            return (__AddSeamlessComponentTable)(hSession, hSeamlessTable, componentTablePath, mbr);
        }
        return false;
    }

    /* ***********************************************************
    * SQL and Expression methods
    * ***********************************************************
    */
    EFALHANDLE Select(EFALHANDLE hSession, const wchar_t * txt)
    {
        if (__Select != nullptr)
        {
            return (__Select)(hSession, txt);
        }
        return 0;
    }
    bool FetchNext(EFALHANDLE hSession, EFALHANDLE hCursor)
    {
        if (__FetchNext != nullptr)
        {
            return (__FetchNext)(hSession, hCursor);
        }
        return false;
    }
    void DisposeCursor(EFALHANDLE hSession, EFALHANDLE hCursor)
    {
        if (__DisposeCursor != nullptr)
        {
            (__DisposeCursor)(hSession, hCursor);
        }
    }
    MI_INT32 Insert(EFALHANDLE hSession, const wchar_t * txt)
    {
        if (__Insert != nullptr)
        {
            return (__Insert)(hSession, txt);
        }
        return 0;
    }
    MI_INT32 Update(EFALHANDLE hSession, const wchar_t * txt)
    {
        if (__Update != nullptr)
        {
            return (__Update)(hSession, txt);
        }
        return 0;
    }
    MI_INT32 Delete(EFALHANDLE hSession, const wchar_t * txt)
    {
        if (__Delete != nullptr)
        {
            return (__Delete)(hSession, txt);
        }
        return 0;
    }

    EFALHANDLE Prepare(EFALHANDLE hSession, const wchar_t * txt)
    {
        if (__Prepare != nullptr)
        {
            return (__Prepare)(hSession, txt);
        }
        return 0;
    }
    void DisposeStmt(EFALHANDLE hSession, EFALHANDLE hStmt)
    {
        if (__DisposeStmt != nullptr)
        {
            (__DisposeStmt)(hSession, hStmt);
        }
    }
    EFALHANDLE ExecuteSelect(EFALHANDLE hSession, EFALHANDLE hStmt)
    {
        if (__ExecuteSelect != nullptr)
        {
            return (__ExecuteSelect)(hSession, hStmt);
        }
        return 0;
    }
    long ExecuteInsert(EFALHANDLE hSession, EFALHANDLE hStmt)
    {
        if (__ExecuteInsert != nullptr)
        {
            return (__ExecuteInsert)(hSession, hStmt);
        }
        return 0;
    }
    long ExecuteUpdate(EFALHANDLE hSession, EFALHANDLE hStmt)
    {
        if (__ExecuteUpdate != nullptr)
        {
            return (__ExecuteUpdate)(hSession, hStmt);
        }
        return 0;
    }
    long ExecuteDelete(EFALHANDLE hSession, EFALHANDLE hStmt)
    {
        if (__ExecuteDelete != nullptr)
        {
            return (__ExecuteDelete)(hSession, hStmt);
        }
        return 0;
    }

    /* ***********************************************************
    * Cursor Record Methods
    * ***********************************************************
    */
    MI_UINT32 GetCursorColumnCount(EFALHANDLE hSession, EFALHANDLE hCursor)
    {
        if (__GetCursorColumnCount != nullptr)
        {
            return (__GetCursorColumnCount)(hSession, hCursor);
        }
        return 0;
    }
    const wchar_t * GetCursorColumnName(EFALHANDLE hSession, EFALHANDLE hCursor, MI_UINT32 columnNbr)
    {
        if (__GetCursorColumnName != nullptr)
        {
            return (__GetCursorColumnName)(hSession, hCursor, columnNbr);
        }
        return nullptr;
    }
    Ellis::ALLTYPE_TYPE GetCursorColumnType(EFALHANDLE hSession, EFALHANDLE hCursor, MI_UINT32 columnNbr)
    {
        if (__GetCursorColumnType != nullptr)
        {
            return (__GetCursorColumnType)(hSession, hCursor, columnNbr);
        }
        return Ellis::ALLTYPE_TYPE::OT_NONE;
    }
    const wchar_t * GetCursorColumnCSys(EFALHANDLE hSession, EFALHANDLE hCursor, MI_UINT32 columnNbr)
    {
        if (__GetCursorColumnCSys != nullptr)
        {
            return (__GetCursorColumnCSys)(hSession, hCursor, columnNbr);
        }
        return nullptr;
    }
    const wchar_t * GetCursorCurrentKey(EFALHANDLE hSession, EFALHANDLE hCursor)
    {
        if (__GetCursorCurrentKey != nullptr)
        {
            return (__GetCursorCurrentKey)(hSession, hCursor);
        }
        return nullptr;
    }
    bool GetCursorIsNull(EFALHANDLE hSession, EFALHANDLE hCursor, MI_UINT32 columnNbr)
    {
        if (__GetCursorIsNull != nullptr)
        {
            return (__GetCursorIsNull)(hSession, hCursor, columnNbr);
        }
        return false;
    }
    const wchar_t *  GetCursorValueString(EFALHANDLE hSession, EFALHANDLE hCursor, MI_UINT32 columnNbr)
    {
        if (__GetCursorValueString != nullptr)
        {
            return (__GetCursorValueString)(hSession, hCursor, columnNbr);
        }
        return nullptr;
    }
    bool GetCursorValueBoolean(EFALHANDLE hSession, EFALHANDLE hCursor, MI_UINT32 columnNbr)
    {
        if (__GetCursorValueBoolean != nullptr)
        {
            return (__GetCursorValueBoolean)(hSession, hCursor, columnNbr);
        }
        return 0;
    }
    double GetCursorValueDouble(EFALHANDLE hSession, EFALHANDLE hCursor, MI_UINT32 columnNbr)
    {
        if (__GetCursorValueDouble != nullptr)
        {
            return (__GetCursorValueDouble)(hSession, hCursor, columnNbr);
        }
        return 0;
    }
    MI_INT64 GetCursorValueInt64(EFALHANDLE hSession, EFALHANDLE hCursor, MI_UINT32 columnNbr)
    {
        if (__GetCursorValueInt64 != nullptr)
        {
            return (__GetCursorValueInt64)(hSession, hCursor, columnNbr);
        }
        return 0;
    }
    MI_INT32 GetCursorValueInt32(EFALHANDLE hSession, EFALHANDLE hCursor, MI_UINT32 columnNbr)
    {
        if (__GetCursorValueInt32 != nullptr)
        {
            return (__GetCursorValueInt32)(hSession, hCursor, columnNbr);
        }
        return 0;
    }
    MI_INT16 GetCursorValueInt16(EFALHANDLE hSession, EFALHANDLE hCursor, MI_UINT32 columnNbr)
    {
        if (__GetCursorValueInt16 != nullptr)
        {
            return (__GetCursorValueInt16)(hSession, hCursor, columnNbr);
        }
        return 0;
    }
    const wchar_t *  GetCursorValueStyle(EFALHANDLE hSession, EFALHANDLE hCursor, MI_UINT32 columnNbr)
    {
        if (__GetCursorValueStyle != nullptr)
        {
            return (__GetCursorValueStyle)(hSession, hCursor, columnNbr);
        }
        return nullptr;
    }
    MI_UINT32 PrepareCursorValueBinary(EFALHANDLE hSession, EFALHANDLE hCursor, MI_UINT32 columnNbr)
    {
        if (__PrepareCursorValueBinary != nullptr)
        {
            return (__PrepareCursorValueBinary)(hSession, hCursor, columnNbr);
        }
        return 0;
    }
    MI_UINT32 PrepareCursorValueGeometry(EFALHANDLE hSession, EFALHANDLE hCursor, MI_UINT32 columnNbr)
    {
        if (__PrepareCursorValueGeometry != nullptr)
        {
            return (__PrepareCursorValueGeometry)(hSession, hCursor, columnNbr);
        }
        return 0;
    }
    double GetCursorValueTimespanInMilliseconds(EFALHANDLE hSession, EFALHANDLE hCursor, MI_UINT32 columnNbr)
    {
        if (__GetCursorValueTimespanInMilliseconds != nullptr)
        {
            return (__GetCursorValueTimespanInMilliseconds)(hSession, hCursor, columnNbr);
        }
        return 0;
    }
    EFALTIME GetCursorValueTime(EFALHANDLE hSession, EFALHANDLE hCursor, MI_UINT32 columnNbr)
    {
        if (__GetCursorValueTime != nullptr)
        {
            return (__GetCursorValueTime)(hSession, hCursor, columnNbr);
        }
        EFALTIME t; t.hour = t.millisecond = t.minute = t.second = 0;
        return t;
    }
    EFALDATE GetCursorValueDate(EFALHANDLE hSession, EFALHANDLE hCursor, MI_UINT32 columnNbr)
    {
        if (__GetCursorValueDate != nullptr)
        {
            return (__GetCursorValueDate)(hSession, hCursor, columnNbr);
        }
        EFALDATE d; d.day = d.month = d.year = 0;
        return d;
    }
    EFALDATETIME GetCursorValueDateTime(EFALHANDLE hSession, EFALHANDLE hCursor, MI_UINT32 columnNbr)
    {
        if (__GetCursorValueDateTime != nullptr)
        {
            return (__GetCursorValueDateTime)(hSession, hCursor, columnNbr);
        }
        EFALDATETIME dt; dt.day = dt.hour = dt.millisecond = dt.minute = dt.month = dt.second = dt.year = 0;
        return dt;
    }
    /* ***********************************************************
    * Variable Methods
    * ***********************************************************
    */
    bool CreateVariable(EFALHANDLE hSession, const wchar_t * name)
    {
        if (__CreateVariable != nullptr)
        {
            return (__CreateVariable)(hSession, name);
        }
        return false;
    }
    void DropVariable(EFALHANDLE hSession, const wchar_t * name)
    {
        if (__DropVariable != nullptr)
        {
            (__DropVariable)(hSession, name);
        }
    }
    MI_UINT32 GetVariableCount(EFALHANDLE hSession)
    {
        if (__GetVariableCount != nullptr)
        {
            return (__GetVariableCount)(hSession);
        }
        return 0;
    }
    const wchar_t * GetVariableName(EFALHANDLE hSession, MI_UINT32 index)
    {
        if (__GetVariableName != nullptr)
        {
            return (__GetVariableName)(hSession, index);
        }
        return nullptr;
    }
    Ellis::ALLTYPE_TYPE GetVariableType(EFALHANDLE hSession, const wchar_t * name)
    {
        if (__GetVariableType != nullptr)
        {
            return (__GetVariableType)(hSession, name);
        }
        return Ellis::ALLTYPE_TYPE::OT_NONE;
    }
    Ellis::ALLTYPE_TYPE SetVariableValue(EFALHANDLE hSession, const wchar_t * name, const wchar_t * expression)
    {
        if (__SetVariableValue != nullptr)
        {
            return (__SetVariableValue)(hSession, name, expression);
        }
        return Ellis::ALLTYPE_TYPE::OT_NONE;
    }

    bool GetVariableIsNull(EFALHANDLE hSession, const wchar_t * name)
    {
        if (__GetVariableIsNull != nullptr)
        {
            return (__GetVariableIsNull)(hSession, name);
        }
        return false;
    }
    const wchar_t *  GetVariableValueString(EFALHANDLE hSession, const wchar_t * name)
    {
        if (__GetVariableValueString != nullptr)
        {
            return (__GetVariableValueString)(hSession, name);
        }
        return nullptr;
    }
    bool GetVariableValueBoolean(EFALHANDLE hSession, const wchar_t * name)
    {
        if (__GetVariableValueBoolean != nullptr)
        {
            return (__GetVariableValueBoolean)(hSession, name);
        }
        return false;
    }
    double GetVariableValueDouble(EFALHANDLE hSession, const wchar_t * name)
    {
        if (__GetVariableValueDouble != nullptr)
        {
            return (__GetVariableValueDouble)(hSession, name);
        }
        return 0;
    }
    MI_INT64 GetVariableValueInt64(EFALHANDLE hSession, const wchar_t * name)
    {
        if (__GetVariableValueInt64 != nullptr)
        {
            return (__GetVariableValueInt64)(hSession, name);
        }
        return 0;
    }
    MI_INT32 GetVariableValueInt32(EFALHANDLE hSession, const wchar_t * name)
    {
        if (__GetVariableValueInt32 != nullptr)
        {
            return (__GetVariableValueInt32)(hSession, name);
        }
        return 0;
    }
    MI_INT16 GetVariableValueInt16(EFALHANDLE hSession, const wchar_t * name)
    {
        if (__GetVariableValueInt16 != nullptr)
        {
            return (__GetVariableValueInt16)(hSession, name);
        }
        return 0;
    }
    const wchar_t *  GetVariableValueStyle(EFALHANDLE hSession, const wchar_t * name)
    {
        if (__GetVariableValueStyle != nullptr)
        {
            return (__GetVariableValueStyle)(hSession, name);
        }
        return nullptr;
    }
    MI_UINT32 PrepareVariableValueBinary(EFALHANDLE hSession, const wchar_t * name)
    {
        if (__PrepareVariableValueBinary != nullptr)
        {
            return (__PrepareVariableValueBinary)(hSession, name);
        }
        return 0;
    }
    MI_UINT32 PrepareVariableValueGeometry(EFALHANDLE hSession, const wchar_t * name)
    {
        if (__PrepareVariableValueGeometry != nullptr)
        {
            return (__PrepareVariableValueGeometry)(hSession, name);
        }
        return 0;
    }
    const wchar_t * GetVariableColumnCSys(EFALHANDLE hSession, const wchar_t * name)
    {
        if (__GetVariableColumnCSys != nullptr)
        {
            return (__GetVariableColumnCSys)(hSession, name);
        }
        return nullptr;
    }
    double GetVariableValueTimespanInMilliseconds(EFALHANDLE hSession, const wchar_t * name)
    {
        if (__GetVariableValueTimespanInMilliseconds != nullptr)
        {
            return (__GetVariableValueTimespanInMilliseconds)(hSession, name);
        }
        return 0;
    }
    EFALTIME GetVariableValueTime(EFALHANDLE hSession, const wchar_t * name)
    {
        if (__GetVariableValueTime != nullptr)
        {
            return (__GetVariableValueTime)(hSession, name);
        }
        EFALTIME t; t.hour = t.millisecond = t.minute = t.second = 0;
        return t;
    }
    EFALDATE GetVariableValueDate(EFALHANDLE hSession, const wchar_t * name)
    {
        if (__GetVariableValueDate != nullptr)
        {
            return (__GetVariableValueDate)(hSession, name);
        }
        EFALDATE d; d.day = d.month = d.year = 0;
        return d;
    }
    EFALDATETIME GetVariableValueDateTime(EFALHANDLE hSession, const wchar_t * name)
    {
        if (__GetVariableValueDateTime != nullptr)
        {
            return (__GetVariableValueDateTime)(hSession, name);
        }
        EFALDATETIME dt; dt.day = dt.hour = dt.millisecond = dt.minute = dt.month = dt.second = dt.year = 0;
        return dt;
    }
    bool SetVariableIsNull(EFALHANDLE hSession, const wchar_t * name)
    {
        if (__SetVariableIsNull != nullptr)
        {
            return (__SetVariableIsNull)(hSession, name);
        }
        return false;
    }
    bool SetVariableValueString(EFALHANDLE hSession, const wchar_t * name, const wchar_t * value)
    {
        if (__SetVariableValueString != nullptr)
        {
            return (__SetVariableValueString)(hSession, name, value);
        }
        return false;
    }
    bool SetVariableValueBoolean(EFALHANDLE hSession, const wchar_t * name, bool value)
    {
        if (__SetVariableValueBoolean != nullptr)
        {
            return (__SetVariableValueBoolean)(hSession, name, value);
        }
        return false;
    }
    bool SetVariableValueDouble(EFALHANDLE hSession, const wchar_t * name, double value)
    {
        if (__SetVariableValueDouble != nullptr)
        {
            return (__SetVariableValueDouble)(hSession, name, value);
        }
        return false;
    }
    bool SetVariableValueInt64(EFALHANDLE hSession, const wchar_t * name, MI_INT64 value)
    {
        if (__SetVariableValueInt64 != nullptr)
        {
            return (__SetVariableValueInt64)(hSession, name, value);
        }
        return false;
    }
    bool SetVariableValueInt32(EFALHANDLE hSession, const wchar_t * name, MI_INT32 value)
    {
        if (__SetVariableValueInt32 != nullptr)
        {
            return (__SetVariableValueInt32)(hSession, name, value);
        }
        return false;
    }
    bool SetVariableValueInt16(EFALHANDLE hSession, const wchar_t * name, MI_INT16 value)
    {
        if (__SetVariableValueInt16 != nullptr)
        {
            return (__SetVariableValueInt16)(hSession, name, value);
        }
        return false;
    }
    bool SetVariableValueStyle(EFALHANDLE hSession, const wchar_t * name, const wchar_t * value)
    {
        if (__SetVariableValueStyle != nullptr)
        {
            return (__SetVariableValueStyle)(hSession, name, value);
        }
        return false;
    }
    bool SetVariableValueBinary(EFALHANDLE hSession, const wchar_t * name, MI_UINT32 nbytes, const char * value)
    {
        if (__SetVariableValueBinary != nullptr)
        {
            return (__SetVariableValueBinary)(hSession, name, nbytes, value);
        }
        return false;
    }
    bool SetVariableValueGeometry(EFALHANDLE hSession, const wchar_t * name, MI_UINT32 nbytes, const char * value, const wchar_t * szcsys)
    {
        if (__SetVariableValueGeometry != nullptr)
        {
            return (__SetVariableValueGeometry)(hSession, name, nbytes, value, szcsys);
        }
        return false;
    }
    bool SetVariableValueTimespanInMilliseconds(EFALHANDLE hSession, const wchar_t * name, double value)
    {
        if (__SetVariableValueTimespanInMilliseconds != nullptr)
        {
            return (__SetVariableValueTimespanInMilliseconds)(hSession, name, value);
        }
        return false;
    }
    bool SetVariableValueTime(EFALHANDLE hSession, const wchar_t * name, EFALTIME value)
    {
        if (__SetVariableValueTime != nullptr)
        {
            return (__SetVariableValueTime)(hSession, name, value);
        }
        return false;
    }
    bool SetVariableValueDate(EFALHANDLE hSession, const wchar_t * name, EFALDATE value)
    {
        if (__SetVariableValueDate != nullptr)
        {
            return (__SetVariableValueDate)(hSession, name, value);
        }
        return false;
    }
    bool SetVariableValueDateTime(EFALHANDLE hSession, const wchar_t * name, EFALDATETIME value)
    {
        if (__SetVariableValueDateTime != nullptr)
        {
            return (__SetVariableValueDateTime)(hSession, name, value);
        }
        return false;
    }

};

#endif

#endif
