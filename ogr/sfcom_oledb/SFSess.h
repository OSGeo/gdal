// Session.h : Declaration of the CSFSession
#ifndef __CSFSession_H_
#define __CSFSession_H_
#include "resource.h"       // main symbols
#include "SFRS.h"
class CSFSessionTRSchemaRowset;
class CSFSessionColSchemaRowset;
class CSFSessionPTSchemaRowset;
/////////////////////////////////////////////////////////////////////////////
// CSFSession
class ATL_NO_VTABLE CSFSession : 
	public CComObjectRootEx<CComSingleThreadModel>,
	public IGetDataSourceImpl<CSFSession>,
	public IOpenRowsetImpl<CSFSession>,
	public ISessionPropertiesImpl<CSFSession>,
	public IObjectWithSiteSessionImpl<CSFSession>,
	public IDBSchemaRowsetImpl<CSFSession>,
	public IDBCreateCommandImpl<CSFSession, CSFCommand>
{
public:
	CSFSession()
	{
	}
	HRESULT FinalConstruct()
	{
		return FInit();
	}
	STDMETHOD(OpenRowset)(IUnknown *pUnk, DBID *pTID, DBID *pInID, REFIID riid,
					   ULONG cSets, DBPROPSET rgSets[], IUnknown **ppRowset)
	{
		CSFRowset* pRowset;
		return CreateRowset(pUnk, pTID, pInID, riid, cSets, rgSets, ppRowset, pRowset);
	}
BEGIN_PROPSET_MAP(CSFSession)
	BEGIN_PROPERTY_SET(DBPROPSET_SESSION)
		PROPERTY_INFO_ENTRY(SESS_AUTOCOMMITISOLEVELS)
	END_PROPERTY_SET(DBPROPSET_SESSION)
END_PROPSET_MAP()
BEGIN_COM_MAP(CSFSession)
	COM_INTERFACE_ENTRY(IGetDataSource)
	COM_INTERFACE_ENTRY(IOpenRowset)
	COM_INTERFACE_ENTRY(ISessionProperties)
	COM_INTERFACE_ENTRY(IObjectWithSite)
	COM_INTERFACE_ENTRY(IDBCreateCommand)
	COM_INTERFACE_ENTRY(IDBSchemaRowset)
END_COM_MAP()
BEGIN_SCHEMA_MAP(CSFSession)
	SCHEMA_ENTRY(DBSCHEMA_TABLES, CSFSessionTRSchemaRowset)
	SCHEMA_ENTRY(DBSCHEMA_COLUMNS, CSFSessionColSchemaRowset)
	SCHEMA_ENTRY(DBSCHEMA_PROVIDER_TYPES, CSFSessionPTSchemaRowset)
END_SCHEMA_MAP()
};
class CSFSessionTRSchemaRowset : 
	public CRowsetImpl< CSFSessionTRSchemaRowset, CTABLESRow, CSFSession>
{
public:
	HRESULT Execute(LONG* pcRowsAffected, ULONG, const VARIANT*)
	{
		USES_CONVERSION;
		//CSFWindowsFile wf;
		CTABLESRow trData;
		lstrcpyW(trData.m_szType, OLESTR("TABLE"));
		lstrcpyW(trData.m_szDesc, OLESTR("The Directory Table"));
		HANDLE hFile = INVALID_HANDLE_VALUE;
		//TCHAR szDir[MAX_PATH + 1];
		//DWORD cbCurDir = GetCurrentDirectory(MAX_PATH, szDir);
		//lstrcat(szDir, _T("\\*.*"));
		//hFile = FindFirstFile(szDir, &wf);
		//if (hFile == INVALID_HANDLE_VALUE)
			return E_FAIL; // User doesn't have a c:\ drive
		//FindClose(hFile);
		lstrcpynW(trData.m_szTable, T2OLE("Testing"), SIZEOF_MEMBER(CTABLESRow, m_szTable));
		if (!m_rgRowData.Add(trData))
			return E_OUTOFMEMORY;
		*pcRowsAffected = 1;
		return S_OK;
	}
};
class CSFSessionColSchemaRowset : 
	public CRowsetImpl< CSFSessionColSchemaRowset, CCOLUMNSRow, CSFSession>
{
public:
	HRESULT Execute(LONG* pcRowsAffected, ULONG, const VARIANT*)
	{
		USES_CONVERSION;
		//CSFWindowsFile wf;
		HANDLE hFile = INVALID_HANDLE_VALUE;
		TCHAR szDir[MAX_PATH + 1];
		//DWORD cbCurDir = GetCurrentDirectory(MAX_PATH, szDir);
		strcpy(szDir,"Testing");
		lstrcat(szDir, _T("\\*.*"));
		//hFile = FindFirstFile(szDir, &wf);
		//if (hFile == INVALID_HANDLE_VALUE)
		//	return E_FAIL; // User doesn't have a c:\ drive
		//FindClose(hFile);// szDir has got the tablename
		DBID dbid;
		memset(&dbid, 0, sizeof(DBID));
		dbid.uName.pwszName = T2OLE(szDir);
		dbid.eKind = DBKIND_NAME;
		return InitFromRowset < _RowsetArrayType > (m_rgRowData, &dbid, NULL, m_spUnkSite, pcRowsAffected);
	}
};
class CSFSessionPTSchemaRowset : 
	public CRowsetImpl< CSFSessionPTSchemaRowset, CPROVIDER_TYPERow, CSFSession>
{
public:
	HRESULT Execute(LONG* pcRowsAffected, ULONG, const VARIANT*)
	{
		return S_OK;
	}
};
#endif //__CSFSession_H_
