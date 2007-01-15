/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  IAccessor implementation that doesn't suffer IAccessorImpl's
 *           problem with confusion of accessor handle ids. 
 * Author:   Frank Warmerdam <warmerdam@pobox.com>
 *
 * This code is closely derived from the code in ATLDB.H for CRowsetImpl.
 * It is basically the same as CRowsetImpl, but is built on ICRRowsetImpl
 * instead of IRowsetImpl.
 *
 ******************************************************************************
 * Copyright (c) 2001, Frank Warmerdam
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
 ****************************************************************************/

#ifndef _CCRRowsetImpl_INCLUDED
#define _CCRRowsetImpl_INCLUDED

template <class T, class Storage, class CreatorClass,
	  class ArrayType = CSimpleArray<Storage>,
	  class RowClass = CSimpleRow,
	  class RowsetInterface = ICRRowsetImpl < T, IRowset, RowClass> >
class CCRRowsetImpl :
	public CComObjectRootEx<CreatorClass::_ThreadModel>,
	public IAccessorImpl<T>,
	public IRowsetIdentityImpl<T, RowClass>,
	public IRowsetCreatorImpl<T>,
	public IRowsetInfoImpl<T, CreatorClass::_PropClass>,
	public IColumnsInfoImpl<T>,
	public IConvertTypeImpl<T>,
	public RowsetInterface
{
public:

	typedef CreatorClass _RowsetCreatorClass;
	typedef ArrayType _RowsetArrayType;
	typedef CCRRowsetImpl< T, Storage, CreatorClass, ArrayType, RowClass, RowsetInterface> _RowsetBaseClass;

BEGIN_COM_MAP(CCRRowsetImpl)
	COM_INTERFACE_ENTRY(IAccessor)
	COM_INTERFACE_ENTRY(IObjectWithSite)
	COM_INTERFACE_ENTRY(IRowsetInfo)
	COM_INTERFACE_ENTRY(IColumnsInfo)
	COM_INTERFACE_ENTRY(IConvertType)
	COM_INTERFACE_ENTRY(IRowsetIdentity)
	COM_INTERFACE_ENTRY(IRowset)
END_COM_MAP()

	HRESULT FinalConstruct()
	{
		HRESULT hr = IAccessorImpl<T>::FinalConstruct();
		if (FAILED(hr))
			return hr;
		return CConvertHelper::FinalConstruct();
	}

	HRESULT NameFromDBID(DBID* pDBID, CComBSTR& bstr, bool bIndex)
	{

		if (pDBID->uName.pwszName != NULL)
		{
			bstr = pDBID->uName.pwszName;
			if (m_strCommandText == (BSTR)NULL)
				return E_OUTOFMEMORY;
			return S_OK;
		}

		return (bIndex) ? DB_E_NOINDEX : DB_E_NOTABLE;
	}

	HRESULT GetCommandFromID(DBID* pTableID, DBID* pIndexID)
	{
		USES_CONVERSION;
		HRESULT hr;

		if (pTableID == NULL && pIndexID == NULL)
			return E_INVALIDARG;

		if (pTableID != NULL && pTableID->eKind == DBKIND_NAME)
		{
			hr = NameFromDBID(pTableID, m_strCommandText, true);
			if (FAILED(hr))
				return hr;
			if (pIndexID != NULL)
			{
				if (pIndexID->eKind == DBKIND_NAME)
				{
					hr = NameFromDBID(pIndexID, m_strIndexText, false);
					if (FAILED(hr))
					{
						m_strCommandText.Empty();
						return hr;
					}
				}
				else
				{
					m_strCommandText.Empty();
					return DB_E_NOINDEX;
				}
			}
			return S_OK;
		}
		if (pIndexID != NULL && pIndexID->eKind == DBKIND_NAME)
			return NameFromDBID(pIndexID, m_strIndexText, false);

		return S_OK;
	}

	HRESULT ValidateCommandID(DBID* pTableID, DBID* pIndexID)
	{
		HRESULT hr = S_OK;

		if (pTableID != NULL)
		{
			hr = CUtlProps<T>::IsValidDBID(pTableID);

			if (hr != S_OK)
				return hr;

			// Check for a NULL TABLE ID (where its a valid pointer but NULL)
			if ((pTableID->eKind == DBKIND_GUID_NAME ||
				pTableID->eKind == DBKIND_NAME ||
				pTableID->eKind == DBKIND_PGUID_NAME)
				&& pTableID->uName.pwszName == NULL)
				return DB_E_NOTABLE;
		}

		if (pIndexID != NULL)
			hr = CUtlProps<T>::IsValidDBID(pIndexID);

		return hr;
	}

	HRESULT SetCommandText(DBID* pTableID, DBID* pIndexID)
	{
		T* pT = (T*)this;
		HRESULT hr = pT->ValidateCommandID(pTableID, pIndexID);
		if (FAILED(hr))
			return hr;
		hr = pT->GetCommandFromID(pTableID, pIndexID);
		return hr;
	}
	void FinalRelease()
	{
		m_rgRowData.RemoveAll();
	}

	static ATLCOLUMNINFO* GetColumnInfo(T* pv, ULONG* pcCols)
	{
		return Storage::GetColumnInfo(pv,pcCols);
	}


	CComBSTR m_strCommandText;
	CComBSTR m_strIndexText;
	ArrayType m_rgRowData;
};

#endif // ifndef _CCRRowsetImpl_INCLUDED
