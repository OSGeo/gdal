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

#ifndef EFAL_H
#define EFAL_H
#if defined(_MSC_VER)
#pragma once
#endif

#include <EFALAPI.h>

#ifndef __EFALDLL__
namespace Ellis
{
	/*************************************************************************
	* Character set stuff
	*
	* These values define what character set the system is running.
	* They are used for the system_charset and os_charset variables.
	*
	* NOTE WELL: Begining in MapInfo v3.0, we store these values into index
	* files, among other things.  DO NOT CHANGE ANY OF THE EXISTING VALUES
	* UNDER ANY CIRCUMSTANCE!  To add new charsets, append them to the end.
	*
	* The value CHARSET_NEUTRAL is used to identify a character set that we
	* do not want to perform conversions on.  This is useful if we know we
	* have portable 7-bit ASCII characters (blank through tilde), or if we
	* encounter a (single byte) character set that we don't know what else to
	* do with.  Replaces previous concept of CHARSET_UNKNOWN - not knowing
	* what the character set is is only one reason to treat it NEUTRALly.
	*************************************************************************/
	enum MICHARSET
	{
		CHARSET_NONE        = -1,
		CHARSET_NEUTRAL     =  0,         // Treat as if ASCII-7: never convert, etc.

		// Unicode, vol.I, p. 467ff: Unicode Encoding to ISO 8859 Mappings
		CHARSET_ISO8859_1   =  1,         // ISO 8859-1 (Latin-1)
		CHARSET_ISO8859_2   =  2,         // ISO 8859-2 (Latin-2)
		CHARSET_ISO8859_3   =  3,         // ISO 8859-3 (Latin-3)
		CHARSET_ISO8859_4   =  4,         // ISO 8859-4 (Latin-4)
		CHARSET_ISO8859_5   =  5,         // ISO 8859-5 (English and Cyrillic-based)
		CHARSET_ISO8859_6   =  6,         // ISO 8859-6 (English and Arabic)
		CHARSET_ISO8859_7   =  7,         // ISO 8859-7 (English and Greek)
		CHARSET_ISO8859_8   =  8,         // ISO 8859-8 (English and Hebrew)
		CHARSET_ISO8859_9   =  9,         // ISO 8859-9 (Latin-5: Western Europe and Turkish)

		// Unicode, vol.I, p. 519ff: Microsoft Windows Character Sets
		CHARSET_WLATIN1     = 10,         // Windows Latin-1 (Code Page 1252, a.k.a. "ANSI")
		CHARSET_WLATIN2     = 11,         // Windows Latin-2 (CP 1250)
		CHARSET_WARABIC     = 12,         // Windows Arabic (CP 1256)
		CHARSET_WCYRILLIC   = 13,         // Windows Cyrillic (CP 1251)
		CHARSET_WGREEK      = 14,         // Windows Greek (CP 1253)
		CHARSET_WHEBREW     = 15,         // Windows Hebrew (CP 1255)
		CHARSET_WTURKISH    = 16,         // Windows Turkish (CP 1254)

		// Windows Far Eastern character sets
		CHARSET_WTCHINESE   = 17,         // Windows Big 5 ("Traditional": Taiwan, Hong Kong)
		CHARSET_WSCHINESE   = 18,         // Windows ?? ("Simplified": China)
		CHARSET_WJAPANESE   = 19,         // Windows Shift JIS X0208 (Japan)
		CHARSET_WKOREAN     = 20,         // Windows KS C5601 (Korea)

#if 0	// Not supported in EFAL
		// Unicode, vol.I, p. 509ff: Unicode Encoding to Macintosh
		CHARSET_MROMAN      = 21,         // Mac Standard Roman
		CHARSET_MARABIC     = 22,         // Mac Arabic
		CHARSET_MGREEK      = 23,         // Mac Greek: ISO 8859-7
		CHARSET_MHEBREW     = 24,         // Mac Hebrew: extension of ISO 8859-8

		// Other Macintosh character sets, including Far Eastern
		CHARSET_MCENTEURO   = 25,         // Mac Central European
		CHARSET_MCROATIAN   = 26,         // Mac Croatian
		CHARSET_MCYRILLIC   = 27,         // Mac Cyrillic
		CHARSET_MICELANDIC  = 28,         // Mac Icelandic
		CHARSET_MTHAI       = 29,         // Mac Thai: TIS 620-2529
		CHARSET_MTURKISH    = 30,         // Mac Turkish
		CHARSET_MTCHINESE   = 31,         // Mac Big 5 ("Traditional": Taiwan, Hong Kong)
		CHARSET_MJAPANESE   = 32,         // Mac Shift JIS X0208 (Japan)
		CHARSET_MKOREAN     = 33,         // Mac KS C5601 (Korea)
#endif

		// Unicode, vol.I, p. 536ff: Unicode to PC Code Page Mappings (Latin)
		CHARSET_CP437       = 34,          // IBM Code Page 437 ("extended ASCII")
		CHARSET_CP850       = 35,          // IBM CP 850 (Multilingual)
		CHARSET_CP852       = 36,          // IBM CP 852 (Eastern Europe)
		CHARSET_CP857       = 37,          // IBM CP 857 (Turkish)
		CHARSET_CP860       = 38,          // IBM CP 860 (Portugeuse)
		CHARSET_CP861       = 39,          // IBM CP 861 (Icelandic)
		CHARSET_CP863       = 40,          // IBM CP 863 (French Canada)
		CHARSET_CP865       = 41,          // IBM CP 865 (Norway)

		// Unicode, vol.I, p. 546ff: Unicode to PC Code Page Mappings (Greek,Cyrillic,Arabic)
		CHARSET_CP855       = 42,          // IBM CP 855 (Cyrillic)
		CHARSET_CP864       = 43,          // IBM CP 864 (Arabic)
		CHARSET_CP869       = 44,          // IBM CP 869 (Modern Greek)

#if 0	// Not supported in EFAL
		// Lotus proprietary character sets
		CHARSET_LICS        = 45,          // Lotus International: for older Lotus files
		CHARSET_LMBCS       = 46,          // Lotus MultiByte: for newer Lotus files
		CHARSET_LMBCS1      = 47,          // Lotus MultiByte group 1: newer Lotus files
		CHARSET_LMBCS2      = 48,          // Lotus MultiByte group 2: newer Lotus files

		// Another Macintosh Far Eastern character set
		CHARSET_MSCHINESE   = 49,          // Macintosh ?? ("Simplified": China)

		// UNIX Far Eastern character sets
		CHARSET_UTCHINESE   = 50,          // UNIX ?? ("Traditional": Taiwan, Hong Kong)
		CHARSET_USCHINESE   = 51,          // UNIX ?? ("Simplified": China)
		CHARSET_UJAPANESE   = 52,          // UNIX Packed EUC/JIS (Japan)
		CHARSET_UKOREAN     = 53,          // UNIX ?? (Korea)
#endif

		// More Windows code pages (introduced by Windows 95)

		CHARSET_WTHAI       = 54,          // Windows Thai (CP 874)
		CHARSET_WBALTICRIM  = 55,          // Windows Baltic Rim (CP 1257)
		CHARSET_WVIETNAMESE = 56,          // Windows Vietnamese (CP 1258)

		CHARSET_UTF8        = 57,          // standard UTF-8
		CHARSET_UTF16       = 58,          // standard UTF-16
	};
	/*************************************************************************
	* Data types
	*************************************************************************/
	enum ALLTYPE_TYPE
	{
		OT_NONE = 0,
		OT_CHAR = 1,
		OT_DECIMAL = 2,
		OT_INTEGER = 3,
		OT_SMALLINT = 4,
		OT_DATE = 5,
		OT_LOGICAL = 6,
		OT_FLOAT = 8,
		OT_OBJECT = 13,
		OT_NULL = 17,
		OT_BINARY = 27, // used as an index type
		OT_STYLE = 36, //for style column which is a type of ALLSTYLE
		OT_INTEGER64 = 40,
		OT_TIMESPAN = 41,
		OT_TIME = 42,
		OT_DATETIME = 43,
	};
	/*************************************************************************
	* Pack table operations
	*************************************************************************/
	enum ETablePackType {
		eTablePackTypePackGraphics = 0x01,
		eTablePackTypeRebuildGraphics = eTablePackTypePackGraphics << 1,
		eTablePackTypePackIndex = eTablePackTypePackGraphics << 2,
		eTablePackTypeRebuildIndex = eTablePackTypePackGraphics << 3,
		eTablePackTypeRemoveDeletedRecords = eTablePackTypePackGraphics << 4,
		eTablePackTypeCompactDB = eTablePackTypePackGraphics << 5,
		eTablePackTypeAll = eTablePackTypePackGraphics | eTablePackTypePackIndex | eTablePackTypeRemoveDeletedRecords, // NOTE: Does not include eTablePackTypeCompactDB
	};
	struct DRECT {
		double x1, y1, x2, y2;
	};
	struct DPNT {
		double x, y;
	};
	struct DRANGE {
		double min, max;
	};
	enum CalloutLineType {
		eNone = 0, //No callout line
		eSimple = 1, //Indicates that the text uses a simple callout line (no pointer to its reference geometry).
		eArrow = 2 //Indicates that the text uses an arrow that points to its reference geometry.
	};
}
#ifdef EFAL_IN_GDAL
	enum WKBGeometryType {
		EFAL_wkbPoint = 1,
		EFAL_wkbLineString = 2,
		EFAL_wkbPolygon = 3,
		EFAL_wkbTriangle = 17,
		EFAL_wkbMultiPoint = 4,
		EFAL_wkbMultiLineString = 5,
		EFAL_wkbMultiPolygon = 6,
		EFAL_wkbGeometryCollection = 7,
		EFAL_wkbPolyhedralSurface = 15,
		EFAL_wkbTIN = 16,
		EFAL_ewkbLegacyText = 206,
		EFAL_wkbPointZ = 1001,
		EFAL_wkbLineStringZ = 1002,
		EFAL_wkbPolygonZ = 1003,
		EFAL_wkbTrianglez = 1017,
		EFAL_wkbMultiPointZ = 1004,
		EFAL_wkbMultiLineStringZ = 1005,
		EFAL_wkbMultiPolygonZ = 1006,
		EFAL_wkbGeometryCollectionZ = 1007,
		EFAL_wkbPolyhedralSurfaceZ = 1015,
		EFAL_wkbTINZ = 1016,
		EFAL_wkbPointM = 2001,
		EFAL_wkbLineStringM = 2002,
		EFAL_wkbPolygonM = 2003,
		EFAL_wkbTriangleM = 2017,
		EFAL_wkbMultiPointM = 2004,
		EFAL_wkbMultiLineStringM = 2005,
		EFAL_wkbMultiPolygonM = 2006,
		EFAL_wkbGeometryCollectionM = 2007,
		EFAL_wkbPolyhedralSurfaceM = 2015,
		EFAL_wkbTINM = 2016,
		EFAL_wkbPointZM = 3001,
		EFAL_wkbLineStringZM = 3002,
		EFAL_wkbPolygonZM = 3003,
		EFAL_wkbTriangleZM = 3017,
		EFAL_wkbMultiPointZM = 3004,
		EFAL_wkbMultiLineStringZM = 3005,
		EFAL_wkbMultiPolygonZM = 3006,
		EFAL_wkbGeometryCollectionZM = 3007,
		EFAL_wkbPolyhedralSurfaceZM = 3015,
		EFAL_wkbTinZM = 3016,
	};
#else
	enum WKBGeometryType {
		wkbPoint = 1,
		wkbLineString = 2,
		wkbPolygon = 3,
		wkbTriangle = 17,
		wkbMultiPoint = 4,
		wkbMultiLineString = 5,
		wkbMultiPolygon = 6,
		wkbGeometryCollection = 7,
		wkbPolyhedralSurface = 15,
		wkbTIN = 16,
		ewkbLegacyText = 206,
		wkbPointZ = 1001,
		wkbLineStringZ = 1002,
		wkbPolygonZ = 1003,
		wkbTrianglez = 1017,
		wkbMultiPointZ = 1004,
		wkbMultiLineStringZ = 1005,
		wkbMultiPolygonZ = 1006,
		wkbGeometryCollectionZ = 1007,
		wkbPolyhedralSurfaceZ = 1015,
		wkbTINZ = 1016,
		wkbPointM = 2001,
		wkbLineStringM = 2002,
		wkbPolygonM = 2003,
		wkbTriangleM = 2017,
		wkbMultiPointM = 2004,
		wkbMultiLineStringM = 2005,
		wkbMultiPolygonM = 2006,
		wkbGeometryCollectionM = 2007,
		wkbPolyhedralSurfaceM = 2015,
		wkbTINM = 2016,
		wkbPointZM = 3001,
		wkbLineStringZM = 3002,
		wkbPolygonZM = 3003,
		wkbTriangleZM = 3017,
		wkbMultiPointZM = 3004,
		wkbMultiLineStringZM = 3005,
		wkbMultiPolygonZM = 3006,
		wkbGeometryCollectionZM = 3007,
		wkbPolyhedralSurfaceZM = 3015,
		wkbTinZM = 3016,
	};
#endif

#endif
namespace EFAL
{
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
	EFALFUNCTION EFALHANDLE InitializeSession(ResourceStringCallback resourceStringCallback);
	EFALFUNCTION void DestroySession(EFALHANDLE hSession);

	/* ***********************************************************
	* Variable length data retrieval (for use after calls to
	* PrepareCursorValueBinary, PrepareCursorValueGeometry,
	* PrepareVariableValueBinary, and PrepareVariableValueGeometry,
	* ***********************************************************
	*/
	EFALFUNCTION void GetData(EFALHANDLE hSession, char bytes[], size_t nBytes);

	/* ***********************************************************
	* Error Handling
	* ***********************************************************
	*/
	EFALFUNCTION bool HaveErrors(EFALHANDLE hSession);
	EFALFUNCTION void ClearErrors(EFALHANDLE hSession);
	EFALFUNCTION int NumErrors(EFALHANDLE hSession);
	EFALFUNCTION const wchar_t * GetError(EFALHANDLE hSession, int ierror);

	/* ***********************************************************
	* Table Catalog methods
	* ***********************************************************
	*/
	EFALFUNCTION void CloseAll(EFALHANDLE hSession);
	EFALFUNCTION EFALHANDLE OpenTable(EFALHANDLE hSession, const wchar_t * path);
	EFALFUNCTION void CloseTable(EFALHANDLE hSession, EFALHANDLE hTable);
	EFALFUNCTION bool BeginReadAccess(EFALHANDLE hSession, EFALHANDLE hTable);
	EFALFUNCTION bool BeginWriteAccess(EFALHANDLE hSession, EFALHANDLE hTable);
	EFALFUNCTION void EndAccess(EFALHANDLE hSession, EFALHANDLE hTable);
	EFALFUNCTION MI_UINT32 GetTableCount(EFALHANDLE hSession);
	EFALFUNCTION EFALHANDLE GetTableHandle(EFALHANDLE hSession, MI_UINT32 idx);
	EFALFUNCTION EFALHANDLE GetTableHandle(EFALHANDLE hSession, const wchar_t * alias);
	EFALFUNCTION EFALHANDLE GetTableHandleFromTablePath(EFALHANDLE hSession, const wchar_t * tablePath);
	EFALFUNCTION bool SupportsPack(EFALHANDLE hSession, EFALHANDLE hTable, Ellis::ETablePackType ePackType);
	EFALFUNCTION bool Pack(EFALHANDLE hSession, EFALHANDLE hTable, Ellis::ETablePackType ePackType);

	/* ***********************************************************
	* Utility Methods
	* ***********************************************************
	*/
	EFALFUNCTION const wchar_t * CoordSys2PRJString(EFALHANDLE hSession, const wchar_t * csys);
	EFALFUNCTION const wchar_t * CoordSys2MBString(EFALHANDLE hSession, const wchar_t * csys);
	EFALFUNCTION const wchar_t * PRJ2CoordSysString(EFALHANDLE hSession, const wchar_t * csys);
	EFALFUNCTION const wchar_t * MB2CoordSysString(EFALHANDLE hSession, const wchar_t * csys);

	/* ***********************************************************
	* Table Metadata methods
	* ***********************************************************
	*/
	EFALFUNCTION const wchar_t * GetTableName(EFALHANDLE hSession, EFALHANDLE hTable);
	EFALFUNCTION const wchar_t * GetTableDescription(EFALHANDLE hSession, EFALHANDLE hTable);
	EFALFUNCTION const wchar_t * GetTablePath(EFALHANDLE hSession, EFALHANDLE hTable);
	EFALFUNCTION const wchar_t * GetTableGUID(EFALHANDLE hSession, EFALHANDLE hTable);
	EFALFUNCTION Ellis::MICHARSET GetTableCharset(EFALHANDLE hSession, EFALHANDLE hTable);
	EFALFUNCTION const wchar_t * GetTableType(EFALHANDLE hSession, EFALHANDLE hTable);
	EFALFUNCTION bool HasRaster(EFALHANDLE hSession, EFALHANDLE hTable);
	EFALFUNCTION bool HasGrid(EFALHANDLE hSession, EFALHANDLE hTable);
	EFALFUNCTION bool IsSeamless(EFALHANDLE hSession, EFALHANDLE hTable);
	EFALFUNCTION bool IsVector(EFALHANDLE hSession, EFALHANDLE hTable);
	EFALFUNCTION bool SupportsInsert(EFALHANDLE hSession, EFALHANDLE hTable);
	EFALFUNCTION bool SupportsUpdate(EFALHANDLE hSession, EFALHANDLE hTable);
	EFALFUNCTION bool SupportsDelete(EFALHANDLE hSession, EFALHANDLE hTable);
	EFALFUNCTION bool SupportsBeginAccess(EFALHANDLE hSession, EFALHANDLE hTable);
	EFALFUNCTION MI_INT32 GetReadVersion(EFALHANDLE hSession, EFALHANDLE hTable);
	EFALFUNCTION MI_INT32 GetEditVersion(EFALHANDLE hSession, EFALHANDLE hTable);
	EFALFUNCTION MI_UINT32 GetRowCount(EFALHANDLE hSession, EFALHANDLE hTable);
	EFALFUNCTION MI_UINT32 GetColumnCount(EFALHANDLE hSession, EFALHANDLE hTable);
	EFALFUNCTION const wchar_t * GetColumnName(EFALHANDLE hSession, EFALHANDLE hTable, MI_UINT32 columnNbr);
	EFALFUNCTION Ellis::ALLTYPE_TYPE GetColumnType(EFALHANDLE hSession, EFALHANDLE hTable, MI_UINT32 columnNbr);
	EFALFUNCTION MI_UINT32 GetColumnWidth(EFALHANDLE hSession, EFALHANDLE hTable, MI_UINT32 columnNbr);
	EFALFUNCTION MI_UINT32 GetColumnDecimals(EFALHANDLE hSession, EFALHANDLE hTable, MI_UINT32 columnNbr);
	EFALFUNCTION bool IsColumnIndexed(EFALHANDLE hSession, EFALHANDLE hTable, MI_UINT32 columnNbr);
	EFALFUNCTION bool IsColumnReadOnly(EFALHANDLE hSession, EFALHANDLE hTable, MI_UINT32 columnNbr);
	EFALFUNCTION const wchar_t * GetColumnCSys(EFALHANDLE hSession, EFALHANDLE hTable, MI_UINT32 columnNbr);
	EFALFUNCTION Ellis::DRECT GetEntireBounds(EFALHANDLE hSession, EFALHANDLE hTable, MI_UINT32 columnNbr);
	EFALFUNCTION Ellis::DRECT GetDefaultView(EFALHANDLE hSession, EFALHANDLE hTable, MI_UINT32 columnNbr);
	EFALFUNCTION MI_UINT32 GetPointObjectCount(EFALHANDLE hSession, EFALHANDLE hTable, MI_UINT32 columnNbr);
	EFALFUNCTION MI_UINT32 GetLineObjectCount(EFALHANDLE hSession, EFALHANDLE hTable, MI_UINT32 columnNbr);
	EFALFUNCTION MI_UINT32 GetAreaObjectCount(EFALHANDLE hSession, EFALHANDLE hTable, MI_UINT32 columnNbr);
	EFALFUNCTION MI_UINT32 GetMiscObjectCount(EFALHANDLE hSession, EFALHANDLE hTable, MI_UINT32 columnNbr);
	EFALFUNCTION bool HasZ(EFALHANDLE hSession, EFALHANDLE hTable, MI_UINT32 columnNbr);
	EFALFUNCTION bool IsZRangeKnown(EFALHANDLE hSession, EFALHANDLE hTable, MI_UINT32 columnNbr);
	EFALFUNCTION Ellis::DRANGE GetZRange(EFALHANDLE hSession, EFALHANDLE hTable, MI_UINT32 columnNbr);
	EFALFUNCTION bool HasM(EFALHANDLE hSession, EFALHANDLE hTable, MI_UINT32 columnNbr);
	EFALFUNCTION bool IsMRangeKnown(EFALHANDLE hSession, EFALHANDLE hTable, MI_UINT32 columnNbr);
	EFALFUNCTION Ellis::DRANGE GetMRange(EFALHANDLE hSession, EFALHANDLE hTable, MI_UINT32 columnNbr);

	/* ***********************************************************
	* TAB file Metadata methods
	* ***********************************************************
	*/
	EFALFUNCTION const wchar_t * GetMetadata(EFALHANDLE hSession, EFALHANDLE hTable, const wchar_t * key);
	EFALFUNCTION EFALHANDLE EnumerateMetadata(EFALHANDLE hSession, EFALHANDLE hTable);
	EFALFUNCTION void DisposeMetadataEnumerator(EFALHANDLE hSession, EFALHANDLE hEnumerator);
	EFALFUNCTION bool GetNextEntry(EFALHANDLE hSession, EFALHANDLE hEnumerator);
	EFALFUNCTION const wchar_t * GetCurrentMetadataKey(EFALHANDLE hSession, EFALHANDLE hEnumerator);
	EFALFUNCTION const wchar_t * GetCurrentMetadataValue(EFALHANDLE hSession, EFALHANDLE hEnumerator);
	EFALFUNCTION void SetMetadata(EFALHANDLE hSession, EFALHANDLE hTable, const wchar_t * key, const wchar_t * value);
	EFALFUNCTION void DeleteMetadata(EFALHANDLE hSession, EFALHANDLE hTable, const wchar_t * key);
	EFALFUNCTION bool WriteMetadata(EFALHANDLE hSession, EFALHANDLE hTable);

	/* ***********************************************************
	* Create Table methods
	* ***********************************************************
	*/
	// Should data source be an ENUM?
	EFALFUNCTION EFALHANDLE CreateNativeTableMetadata(EFALHANDLE hSession, const wchar_t * tableName, const wchar_t * tablePath, Ellis::MICHARSET charset);
	EFALFUNCTION EFALHANDLE CreateNativeXTableMetadata(EFALHANDLE hSession, const wchar_t * tableName, const wchar_t * tablePath, Ellis::MICHARSET charset);
	EFALFUNCTION EFALHANDLE CreateGeopackageTableMetadata(EFALHANDLE hSession, const wchar_t * tableName, const wchar_t * tablePath, const wchar_t * databasePath, Ellis::MICHARSET charset, bool convertUnsupportedObjects);
	EFALFUNCTION void AddColumn(EFALHANDLE hSession, EFALHANDLE hTableMetadata, const wchar_t * columnName, Ellis::ALLTYPE_TYPE dataType, bool indexed, MI_UINT32 width, MI_UINT32 decimals, const wchar_t * szCsys);
	EFALFUNCTION EFALHANDLE CreateTable(EFALHANDLE hSession, EFALHANDLE hTableMetadata);
	EFALFUNCTION void DestroyTableMetadata(EFALHANDLE hSession, EFALHANDLE hTableMetadata);

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
	EFALFUNCTION EFALHANDLE CreateSeamlessTable(EFALHANDLE hSession, const wchar_t * tablePath, const wchar_t * csys, Ellis::MICHARSET charset);
	EFALFUNCTION bool AddSeamlessComponentTable(EFALHANDLE hSession, EFALHANDLE hSeamlessTable, const wchar_t * componentTablePath, Ellis::DRECT mbr);

	/* ***********************************************************
	* SQL and Expression methods
	* ***********************************************************
	*/
	EFALFUNCTION EFALHANDLE Select(EFALHANDLE hSession, const wchar_t * txt);
	EFALFUNCTION bool FetchNext(EFALHANDLE hSession, EFALHANDLE hCursor);
	EFALFUNCTION void DisposeCursor(EFALHANDLE hSession, EFALHANDLE hCursor);
	EFALFUNCTION MI_INT32 Insert(EFALHANDLE hSession, const wchar_t * txt);
	EFALFUNCTION MI_INT32 Update(EFALHANDLE hSession, const wchar_t * txt);
	EFALFUNCTION MI_INT32 Delete(EFALHANDLE hSession, const wchar_t * txt);

	EFALFUNCTION EFALHANDLE Prepare(EFALHANDLE hSession, const wchar_t * txt);
	EFALFUNCTION void DisposeStmt(EFALHANDLE hSession, EFALHANDLE hStmt);
	EFALFUNCTION EFALHANDLE ExecuteSelect(EFALHANDLE hSession, EFALHANDLE hStmt);
	EFALFUNCTION long ExecuteInsert(EFALHANDLE hSession, EFALHANDLE hStmt);
	EFALFUNCTION long ExecuteUpdate(EFALHANDLE hSession, EFALHANDLE hStmt);
	EFALFUNCTION long ExecuteDelete(EFALHANDLE hSession, EFALHANDLE hStmt);


	/* ***********************************************************
	* Cursor Record Methods
	* ***********************************************************
	*/
	EFALFUNCTION MI_UINT32 GetCursorColumnCount(EFALHANDLE hSession, EFALHANDLE hCursor);
	EFALFUNCTION const wchar_t * GetCursorColumnName(EFALHANDLE hSession, EFALHANDLE hCursor, MI_UINT32 columnNbr);
	EFALFUNCTION Ellis::ALLTYPE_TYPE GetCursorColumnType(EFALHANDLE hSession, EFALHANDLE hCursor, MI_UINT32 columnNbr);
	EFALFUNCTION const wchar_t * GetCursorColumnCSys(EFALHANDLE hSession, EFALHANDLE hCursor, MI_UINT32 columnNbr);
	EFALFUNCTION const wchar_t * GetCursorCurrentKey(EFALHANDLE hSession, EFALHANDLE hCursor);
	EFALFUNCTION bool GetCursorIsNull(EFALHANDLE hSession, EFALHANDLE hCursor, MI_UINT32 columnNbr);
	EFALFUNCTION const wchar_t *  GetCursorValueString(EFALHANDLE hSession, EFALHANDLE hCursor, MI_UINT32 columnNbr);
	EFALFUNCTION bool GetCursorValueBoolean(EFALHANDLE hSession, EFALHANDLE hCursor, MI_UINT32 columnNbr);
	EFALFUNCTION double GetCursorValueDouble(EFALHANDLE hSession, EFALHANDLE hCursor, MI_UINT32 columnNbr);
	EFALFUNCTION MI_INT64 GetCursorValueInt64(EFALHANDLE hSession, EFALHANDLE hCursor, MI_UINT32 columnNbr);
	EFALFUNCTION MI_INT32 GetCursorValueInt32(EFALHANDLE hSession, EFALHANDLE hCursor, MI_UINT32 columnNbr);
	EFALFUNCTION MI_INT16 GetCursorValueInt16(EFALHANDLE hSession, EFALHANDLE hCursor, MI_UINT32 columnNbr);
	EFALFUNCTION const wchar_t *  GetCursorValueStyle(EFALHANDLE hSession, EFALHANDLE hCursor, MI_UINT32 columnNbr);
	EFALFUNCTION MI_UINT32 PrepareCursorValueBinary(EFALHANDLE hSession, EFALHANDLE hCursor, MI_UINT32 columnNbr);
	EFALFUNCTION MI_UINT32 PrepareCursorValueGeometry(EFALHANDLE hSession, EFALHANDLE hCursor, MI_UINT32 columnNbr);
	EFALFUNCTION double GetCursorValueTimespanInMilliseconds(EFALHANDLE hSession, EFALHANDLE hCursor, MI_UINT32 columnNbr);
	EFALFUNCTION EFALTIME GetCursorValueTime(EFALHANDLE hSession, EFALHANDLE hCursor, MI_UINT32 columnNbr);
	EFALFUNCTION EFALDATE GetCursorValueDate(EFALHANDLE hSession, EFALHANDLE hCursor, MI_UINT32 columnNbr);
	EFALFUNCTION EFALDATETIME GetCursorValueDateTime(EFALHANDLE hSession, EFALHANDLE hCursor, MI_UINT32 columnNbr);
	/* ***********************************************************
	* Variable Methods
	* ***********************************************************
	*/
	EFALFUNCTION bool CreateVariable(EFALHANDLE hSession, const wchar_t * name);
	EFALFUNCTION void DropVariable(EFALHANDLE hSession, const wchar_t * name);
	EFALFUNCTION MI_UINT32 GetVariableCount(EFALHANDLE hSession);
	EFALFUNCTION const wchar_t * GetVariableName(EFALHANDLE hSession, MI_UINT32 index);
	EFALFUNCTION Ellis::ALLTYPE_TYPE GetVariableType(EFALHANDLE hSession, const wchar_t * name);
	EFALFUNCTION Ellis::ALLTYPE_TYPE SetVariableValue(EFALHANDLE hSession, const wchar_t * name, const wchar_t * expression);

	EFALFUNCTION bool GetVariableIsNull(EFALHANDLE hSession, const wchar_t * name);
	EFALFUNCTION const wchar_t *  GetVariableValueString(EFALHANDLE hSession, const wchar_t * name);
	EFALFUNCTION bool GetVariableValueBoolean(EFALHANDLE hSession, const wchar_t * name);
	EFALFUNCTION double GetVariableValueDouble(EFALHANDLE hSession, const wchar_t * name);
	EFALFUNCTION MI_INT64 GetVariableValueInt64(EFALHANDLE hSession, const wchar_t * name);
	EFALFUNCTION MI_INT32 GetVariableValueInt32(EFALHANDLE hSession, const wchar_t * name);
	EFALFUNCTION MI_INT16 GetVariableValueInt16(EFALHANDLE hSession, const wchar_t * name);
	EFALFUNCTION const wchar_t *  GetVariableValueStyle(EFALHANDLE hSession, const wchar_t * name);
	EFALFUNCTION MI_UINT32 PrepareVariableValueBinary(EFALHANDLE hSession, const wchar_t * name);
	EFALFUNCTION MI_UINT32 PrepareVariableValueGeometry(EFALHANDLE hSession, const wchar_t * name);
	EFALFUNCTION const wchar_t * GetVariableColumnCSys(EFALHANDLE hSession, const wchar_t * name);
	EFALFUNCTION double GetVariableValueTimespanInMilliseconds(EFALHANDLE hSession, const wchar_t * name);
	EFALFUNCTION EFALTIME GetVariableValueTime(EFALHANDLE hSession, const wchar_t * name);
	EFALFUNCTION EFALDATE GetVariableValueDate(EFALHANDLE hSession, const wchar_t * name);
	EFALFUNCTION EFALDATETIME GetVariableValueDateTime(EFALHANDLE hSession, const wchar_t * name);

	EFALFUNCTION bool SetVariableIsNull(EFALHANDLE hSession, const wchar_t * name);
	EFALFUNCTION bool SetVariableValueString(EFALHANDLE hSession, const wchar_t * name, const wchar_t * value);
	EFALFUNCTION bool SetVariableValueBoolean(EFALHANDLE hSession, const wchar_t * name, bool value);
	EFALFUNCTION bool SetVariableValueDouble(EFALHANDLE hSession, const wchar_t * name, double value);
	EFALFUNCTION bool SetVariableValueInt64(EFALHANDLE hSession, const wchar_t * name, MI_INT64 value);
	EFALFUNCTION bool SetVariableValueInt32(EFALHANDLE hSession, const wchar_t * name, MI_INT32 value);
	EFALFUNCTION bool SetVariableValueInt16(EFALHANDLE hSession, const wchar_t * name, MI_INT16 value);
	EFALFUNCTION bool SetVariableValueStyle(EFALHANDLE hSession, const wchar_t * name, const wchar_t * value);
	EFALFUNCTION bool SetVariableValueBinary(EFALHANDLE hSession, const wchar_t * name, MI_UINT32 nbytes, const char * value);
	EFALFUNCTION bool SetVariableValueGeometry(EFALHANDLE hSession, const wchar_t * name, MI_UINT32 nbytes, const char * value, const wchar_t * szcsys);
	EFALFUNCTION bool SetVariableValueTimespanInMilliseconds(EFALHANDLE hSession, const wchar_t * name, double value);
	EFALFUNCTION bool SetVariableValueTime(EFALHANDLE hSession, const wchar_t * name, EFALTIME value);
	EFALFUNCTION bool SetVariableValueDate(EFALHANDLE hSession, const wchar_t * name, EFALDATE value);
	EFALFUNCTION bool SetVariableValueDateTime(EFALHANDLE hSession, const wchar_t * name, EFALDATETIME value);
}

#endif 

