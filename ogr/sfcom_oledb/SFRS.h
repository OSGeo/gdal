// SFRS.h : Declaration of the CSFRowset
#ifndef __CSFRowset_H_
#define __CSFRowset_H_
#include "resource.h"       // main symbols
extern "C" 
{
#include "shapefil.h"
}

class SchemaInfo
{
public:
	int		nOffset;
	DBFFieldType eType;
};

class CVirtualArray
{
public:
	CVirtualArray();
	~CVirtualArray();
	void	RemoveAll();
	void	Initialize(int nArraySize,DBFHandle,SHPHandle);
	BYTE    &operator[](int iIndex);
	int		GetSize() const {return m_nArraySize;}
private:
	BYTE		**m_ppasArray;
	int			m_nArraySize;
	DBFHandle	m_hDBFHandle;
	SHPHandle	m_hSHPHandle;
	CSimpleArray<SchemaInfo>	aSchemaInfo;
	int			m_nPackedRecordLength;
};


class CShapeFile
{
public:
	static ATLCOLUMNINFO colInfo;

	template <class T>
		static ATLCOLUMNINFO * GetColumnInfo(T* pT, ULONG* pcCols)
	{	
		USES_CONVERSION;
#ifdef ZERO
		*pcCols = 1;

		memset(&colInfo, 0, sizeof(ATLCOLUMNINFO));

		colInfo.pwszName = ::SysAllocString(T2OLE("Test Integer"));
		colInfo.iOrdinal = 1;
		colInfo.dwFlags = DBCOLUMNFLAGS_ISFIXEDLENGTH;
		colInfo.ulColumnSize = 4;
		colInfo.wType = DBTYPE_I4;
		colInfo.bPrecision = 1;
		colInfo.bScale = 1;
		colInfo.columnid.uName.pwszName = colInfo.pwszName;
		colInfo.cbOffset = 0;
		
		return &colInfo;
#endif


		CComQIPtr<ICommand> spCommand = pT->GetUnknown();
		if (spCommand == NULL)
		{
			if (pcCols != NULL)
				*pcCols = pT->m_paColInfo.GetSize();
			return pT->m_paColInfo.m_aT;
		}
		CComPtr<IRowset> pRowset;
		if (pT->m_paColInfo.m_aT == NULL)
		{
			LONG cRows;
			HRESULT hr = spCommand->Execute(NULL, IID_IRowset, NULL, &cRows, (IUnknown**)&pRowset);
		}
		if (pcCols != NULL)
			*pcCols = pT->m_paColInfo.GetSize();
		return pT->m_paColInfo.m_aT;

	}
};





// CSFCommand
class ATL_NO_VTABLE CSFCommand : 
	public CComObjectRootEx<CComSingleThreadModel>,
	public IAccessorImpl<CSFCommand>,
	public ICommandTextImpl<CSFCommand>,
	public ICommandPropertiesImpl<CSFCommand>,
	public IObjectWithSiteImpl<CSFCommand>,
	public IConvertTypeImpl<CSFCommand>,
	public IColumnsInfoImpl<CSFCommand>
{
public:
BEGIN_COM_MAP(CSFCommand)
	COM_INTERFACE_ENTRY(ICommand)
	COM_INTERFACE_ENTRY(IObjectWithSite)
	COM_INTERFACE_ENTRY(IAccessor)
	COM_INTERFACE_ENTRY(ICommandProperties)
	COM_INTERFACE_ENTRY2(ICommandText, ICommand)
	COM_INTERFACE_ENTRY(IColumnsInfo)
	COM_INTERFACE_ENTRY(IConvertType)
END_COM_MAP()
// ICommand
public:
	HRESULT FinalConstruct()
	{
		HRESULT hr = CConvertHelper::FinalConstruct();
		if (FAILED (hr))
			return hr;
		hr = IAccessorImpl<CSFCommand>::FinalConstruct();
		if (FAILED(hr))
			return hr;
		return CUtlProps<CSFCommand>::FInit();
	}
	void FinalRelease()
	{
		IAccessorImpl<CSFCommand>::FinalRelease();
	}
	HRESULT WINAPI Execute(IUnknown * pUnkOuter, REFIID riid, DBPARAMS * pParams, 
						  LONG * pcRowsAffected, IUnknown ** ppRowset);
	static ATLCOLUMNINFO* GetColumnInfo(CSFCommand* pv, ULONG* pcInfo)
	{
		return CShapeFile::GetColumnInfo(pv,pcInfo);
	}
BEGIN_PROPSET_MAP(CSFCommand)
	BEGIN_PROPERTY_SET(DBPROPSET_ROWSET)
		PROPERTY_INFO_ENTRY(IAccessor)
		PROPERTY_INFO_ENTRY(IColumnsInfo)
		PROPERTY_INFO_ENTRY(IConvertType)
		PROPERTY_INFO_ENTRY(IRowset)
		PROPERTY_INFO_ENTRY(IRowsetIdentity)
		PROPERTY_INFO_ENTRY(IRowsetInfo)
		PROPERTY_INFO_ENTRY(IRowsetLocate)
		PROPERTY_INFO_ENTRY(BOOKMARKS)
		PROPERTY_INFO_ENTRY(BOOKMARKSKIPPED)
		PROPERTY_INFO_ENTRY(BOOKMARKTYPE)
		PROPERTY_INFO_ENTRY(CANFETCHBACKWARDS)
		PROPERTY_INFO_ENTRY(CANHOLDROWS)
		PROPERTY_INFO_ENTRY(CANSCROLLBACKWARDS)
		PROPERTY_INFO_ENTRY(LITERALBOOKMARKS)
		PROPERTY_INFO_ENTRY(ORDEREDBOOKMARKS)
	END_PROPERTY_SET(DBPROPSET_ROWSET)
END_PROPSET_MAP()
		CSimpleArray<ATLCOLUMNINFO>		m_paColInfo;
};

class CSFRowset : public CRowsetImpl< CSFRowset, CShapeFile, CSFCommand,CVirtualArray>
{
public:
	HRESULT Execute(DBPARAMS * pParams, LONG* pcRowsAffected);

	CSimpleArray<ATLCOLUMNINFO>		m_paColInfo;
};
#endif //__CSFRowset_H_
