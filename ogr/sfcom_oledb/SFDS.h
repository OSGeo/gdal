/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  CSFSource declaration.
 * Author:   Ken Shih, kshih@home.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Les Technologies SoftMap Inc.
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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.5  1999/06/22 16:17:31  warmerda
 * added debug statement
 *
 * Revision 1.4  1999/06/22 15:52:56  kshih
 * Added Initialize error return for invalid data set.
 *
 * Revision 1.3  1999/06/12 17:15:42  kshih
 * Make use of datasource property
 * Add schema rowsets
 *
 * Revision 1.2  1999/06/04 15:18:32  warmerda
 * Added copyright header.
 *
 */

#ifndef __CSFSource_H_
#define __CSFSource_H_
#include "resource.h"       // main symbols
#include "SFRS.h"
#include "sfutil.h"

// IDBInitializeImpl
template <class T>
class ATL_NO_VTABLE MyIDBInitializeImpl : public IDBInitialize
{
public:
	MyIDBInitializeImpl()
	{
		m_dwStatus = 0;
		m_pCUtlPropInfo = NULL;
		m_cSessionsOpen = 0;
	}
	~MyIDBInitializeImpl()
	{
		delete m_pCUtlPropInfo;
	}

	STDMETHOD(Uninitialize)(void)
	{
		ATLTRACE2(atlTraceDBProvider, 0, "IDBInitializeImpl::Uninitialize\n");
		T* pT = (T*)this;
		pT->Lock();
		if (pT->m_cSessionsOpen != 0)
		{  
			ATLTRACE2(atlTraceDBProvider, 0, "Uninitialized called with Open Sessions\n");
			return DB_E_OBJECTOPEN;
		}
		delete m_pCUtlPropInfo;
		m_pCUtlPropInfo = NULL;
		pT->m_dwStatus |= DSF_PERSIST_DIRTY;
		pT->m_dwStatus &= DSF_MASK_INIT;    // Clear all non-init flags.
		pT->Unlock();
		return S_OK;

	}

	LONG m_cSessionsOpen;
	DWORD m_dwStatus;
	CUtlPropInfo<T>* m_pCUtlPropInfo;

	STDMETHOD(Initialize)(void)
	{

		ATLTRACE2(atlTraceDBProvider, 0, "IDBInitializeImpl::Initialize\n");
		T *pT = (T*)(this);
		T::ObjectLock lock(pT);
		HRESULT hr;
		if (pT->m_dwStatus & DSF_INITIALIZED)
		{
			ATLTRACE2(atlTraceDBProvider, 0, "IDBInitializeImpl::Initialize Error : Already Initialized\n");
			return DB_E_ALREADYINITIALIZED;
		}
		delete m_pCUtlPropInfo;
		m_pCUtlPropInfo = NULL;
		ATLTRY(m_pCUtlPropInfo = new CUtlPropInfo<T>())
		if (m_pCUtlPropInfo == NULL)
		{
			ATLTRACE2(atlTraceDBProvider, 0, "IDBInitializeImpl::Initialize Error : OOM\n");
			return E_OUTOFMEMORY;
		}
		hr = m_pCUtlPropInfo->FInit();
		if (hr == S_OK)
		{
			pT->m_dwStatus |= DSF_INITIALIZED;
			
// Inserted code!!!!!
			IDBProperties *pIDBProp;
			hr = QueryInterface(IID_IDBProperties, (void **) &pIDBProp);		
			
			if (SUCCEEDED(hr))
			{
	
				char		*pszDataSource;
				DBPROPIDSET sPropIdSets[1];
				DBPROPID	rgPropIds[1];
				
				ULONG		nPropSets;
				DBPROPSET	*rgPropSets;
				
				rgPropIds[0] = DBPROP_INIT_DATASOURCE;
				
				sPropIdSets[0].cPropertyIDs = 1;
				sPropIdSets[0].guidPropertySet = DBPROPSET_DBINIT;
				sPropIdSets[0].rgPropertyIDs = rgPropIds;
				
				pIDBProp->GetProperties(1,sPropIdSets,&nPropSets,&rgPropSets);
				pIDBProp->Release();			
				if (rgPropSets)
				{
					USES_CONVERSION;
					char *pszSource = (char *)  OLE2A(rgPropSets[0].rgProperties[0].vValue.bstrVal);
					pszDataSource = (char *) malloc(1+strlen(pszSource));
					strcpy(pszDataSource,pszSource);
				}
				
				if (rgPropSets)
				{
					int i;
					for (i=0; i < nPropSets; i++)
					{
						CoTaskMemFree(rgPropSets[i].rgProperties);
					}
					CoTaskMemFree(rgPropSets);
				}
				
				
				SHPHandle hSHP;
				DBFHandle hDBF;
				
				hr = E_FAIL;

				OGRComDebug( "info", "data source = %s\n",
                                             pszDataSource );
				if (NULL != (hSHP = SFGetSHPHandle(pszDataSource)))
				{
					SHPClose(hSHP);
					
					if (NULL != (hDBF = SFGetDBFHandle(pszDataSource)))
					{
						DBFClose(hDBF);
						
						hr = S_OK;
					}
				}
				
				free(pszDataSource);
			}
		}
		else
		{
			delete m_pCUtlPropInfo;
			m_pCUtlPropInfo = NULL;
		}
		return hr;
	}

};


/////////////////////////////////////////////////////////////////////////////
// CDataSource
class ATL_NO_VTABLE CSFSource : 
	public CComObjectRootEx<CComSingleThreadModel>,
	public CComCoClass<CSFSource, &CLSID_SF>,
	public IDBCreateSessionImpl<CSFSource, CSFSession>,
	public MyIDBInitializeImpl<CSFSource>,
	public IDBPropertiesImpl<CSFSource>,
	public IPersistImpl<CSFSource>,
	public IInternalConnectionImpl<CSFSource>
{
public:

	

	HRESULT FinalConstruct()
	{
		// Do all my initialization here.

		// verify the 
		return FInit();
	}
DECLARE_REGISTRY_RESOURCEID(IDR_SF)
BEGIN_PROPSET_MAP(CSFSource)
	BEGIN_PROPERTY_SET(DBPROPSET_DATASOURCEINFO)
		PROPERTY_INFO_ENTRY(ACTIVESESSIONS)
		PROPERTY_INFO_ENTRY(DATASOURCEREADONLY)
		PROPERTY_INFO_ENTRY(BYREFACCESSORS)
		PROPERTY_INFO_ENTRY(OUTPUTPARAMETERAVAILABILITY)
		PROPERTY_INFO_ENTRY(PROVIDEROLEDBVER)
		PROPERTY_INFO_ENTRY(DSOTHREADMODEL)
		PROPERTY_INFO_ENTRY(SUPPORTEDTXNISOLEVELS)
		PROPERTY_INFO_ENTRY(USERNAME)
	END_PROPERTY_SET(DBPROPSET_DATASOURCEINFO)
	BEGIN_PROPERTY_SET(DBPROPSET_DBINIT)
		PROPERTY_INFO_ENTRY(AUTH_PASSWORD)
		PROPERTY_INFO_ENTRY(AUTH_PERSIST_SENSITIVE_AUTHINFO)
		PROPERTY_INFO_ENTRY(AUTH_USERID)
		PROPERTY_INFO_ENTRY(INIT_DATASOURCE)
		PROPERTY_INFO_ENTRY(INIT_HWND)
		PROPERTY_INFO_ENTRY(INIT_LCID)
		PROPERTY_INFO_ENTRY(INIT_LOCATION)
		PROPERTY_INFO_ENTRY(INIT_MODE)
		PROPERTY_INFO_ENTRY(INIT_PROMPT)
		PROPERTY_INFO_ENTRY(INIT_PROVIDERSTRING)
		PROPERTY_INFO_ENTRY(INIT_TIMEOUT)
	END_PROPERTY_SET(DBPROPSET_DBINIT)
	CHAIN_PROPERTY_SET(CSFCommand)
END_PROPSET_MAP()
BEGIN_COM_MAP(CSFSource)
	COM_INTERFACE_ENTRY(IDBCreateSession)
	COM_INTERFACE_ENTRY(IDBInitialize)
	COM_INTERFACE_ENTRY(IDBProperties)
	COM_INTERFACE_ENTRY(IPersist)
	COM_INTERFACE_ENTRY(IInternalConnection)
END_COM_MAP()
public:
};
#endif //__CSFSource_H_
