/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  IAccessor implementation that doesn't suffer IAccessorImpl's
 *           problem with confusion of accessor handle ids. 
 * Author:   Frank Warmerdam <warmerdam@pobox.com>
 *
 * This code is closely derived from the code in ATLDB.H for IAccessor.
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
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

// It is assumed that ATLDB.H has already been included.

// The following is intended to be exactly the same as the ATLDB.H IAccessor
// except that IAccessor handles are strictly incrementing 32bit integers
// instead of trying to use the pointers as the handle.  Using the pointer
// causes problems when the accessor is "copied" into other rowsets (ie.
// from the ICommand to the IRowset result) causing the handle and pointer
// to get out of sync (since the handle has to be preserved but the pointer
// changes as a copy of the structure is made).  This can be bad if the
// old pointer (on the ICommand accessor) is deallocated and then gets reused
// as an accessor on the IRowset resulting in two accessors with the same
// handle on the Rowset. 

extern int g_nNextSFAccessorHandle;

// SFAccessorImpl
template <class T, class BindType = ATLBINDINGS, 
			class BindingVector = CAtlMap < HACCESSOR, BindType* > >
class ATL_NO_VTABLE SFAccessorImpl : public IAccessorImplBase<BindType>
{
public:
	typedef BindType _BindType;
	typedef BindingVector _BindingVector;
	SFAccessorImpl()
	{
		m_bIsCommand = FALSE;
		m_bHasParamaters = FALSE;
		m_bIsChangeable = FALSE;
	}
	OUT_OF_LINE HRESULT InternalFinalConstruct(IUnknown* /*pUnkThis*/)
	{
		CComPtr<ICommand> spCommand;
		CComPtr<ICommandWithParameters> spCommandWithParameters;
		T* pT = (T*)this;

		pT->_InternalQueryInterface(IID_ICommand,(void **) &spCommand);

		if (spCommand !=NULL)  // It's a command
		{
			m_bIsCommand = TRUE;
			pT->_InternalQueryInterface(IID_ICommandWithParameters, (void **) &spCommandWithParameters);
			m_bHasParamaters =  spCommandWithParameters != NULL;

		}
		return S_OK;
	}
	HRESULT FinalConstruct()
	{
		T* pT = (T*)this;
		return InternalFinalConstruct(pT->GetUnknown());
	}
	void FinalRelease()
	{
#ifdef _DEBUG
		if (m_rgBindings.GetCount())
			ATLTRACE(atlTraceDBProvider, 0, _T("SFAccessorImpl::~SFAccessorImpl Bindings still in vector, removing\n"));
#endif //_DEBUG
		while (m_rgBindings.GetCount()) 
			ReleaseAccessor((HACCESSOR)m_rgBindings.GetKeyAt(m_rgBindings.GetStartPosition()), NULL);
	}
	STDMETHOD(AddRefAccessor)(HACCESSOR hAccessor,
							  DBREFCOUNT *pcRefCount)
	{
		ATLTRACE(atlTraceDBProvider, 2, _T("SFAccessorImpl::AddRefAccessor\n"));
		if (hAccessor == NULL)
		{
			ATLTRACE(atlTraceDBProvider, 0, _T("AddRefAccessor : Bad hAccessor\n"));
			return DB_E_BADACCESSORHANDLE;
		}
		BindType* pBind;
		if( ! m_rgBindings.Lookup(hAccessor, pBind ) )
			return DB_E_BADACCESSORHANDLE;

		ATLASSERT( pBind );
		ULONG cRefCount = T::_ThreadModel::Increment((LONG*)&pBind->dwRef);

		if (pcRefCount != NULL)
			*pcRefCount = cRefCount;

		return S_OK;
	}
	OUT_OF_LINE ATLCOLUMNINFO* ValidateHelper(DBORDINAL* pcCols, CComPtr<IDataConvert> & rspConvert)
	{
		T* pT = (T*)this;
		rspConvert = pT->m_spConvert;
		return T::GetColumnInfo(pT, pcCols);
	}
	OUT_OF_LINE HRESULT ValidateBindingsFromMetaData(DBCOUNTITEM cBindings, const DBBINDING rgBindings[], 
				DBBINDSTATUS rgStatus[], bool bHasBookmarks)
	{
		HRESULT hr = S_OK;
		DBORDINAL cCols;
		CComPtr<IDataConvert> spConvert;
		ATLCOLUMNINFO* pColInfo = ValidateHelper(&cCols, spConvert);
		ATLASSERT(pColInfo != NULL);
		for (DBCOUNTITEM iBinding = 0; iBinding < cBindings; iBinding++)
		{
			const DBBINDING& rBindCur = rgBindings[iBinding];
			DBORDINAL iOrdAdjusted;
			if (bHasBookmarks)
				iOrdAdjusted = rBindCur.iOrdinal;	// Bookmarks start with ordinal 0
			else
				iOrdAdjusted = rBindCur.iOrdinal - 1; // Non-bookmarks start w/ ordinal 1
			if (rBindCur.iOrdinal > cCols)
			{
				hr = DB_E_ERRORSOCCURRED;
				rgStatus[iBinding] = DBBINDSTATUS_BADORDINAL;
				continue;
			}

			// If a binding specifies provider owned memory, and specifies type 
			// X | BYREF, and the provider's copy is not X or X | BYREF, return 
			// DBBINDSTATUS_BADBINDINFO

			if (rBindCur.dwMemOwner == DBMEMOWNER_PROVIDEROWNED)
			{
				if ((rBindCur.wType & DBTYPE_BYREF) != 0 &&
					((rBindCur.wType & (~DBTYPE_BYREF)) != 
						(pColInfo[iOrdAdjusted].wType & (~DBTYPE_BYREF))))
				{
					hr = DB_E_ERRORSOCCURRED;
					rgStatus[iBinding] = DBBINDSTATUS_BADBINDINFO;
					continue;
				}
			}

			ATLASSERT(spConvert != NULL);
			HRESULT hrConvert = spConvert->CanConvert(pColInfo[iOrdAdjusted].wType, rBindCur.wType);
			if (FAILED(hrConvert) || hrConvert == S_FALSE)
			{
				hr = DB_E_ERRORSOCCURRED;
				rgStatus[iBinding] = DBBINDSTATUS_UNSUPPORTEDCONVERSION;
				continue;
			}
		}
		return hr;
	}
	STDMETHOD(CreateAccessor)(DBACCESSORFLAGS dwAccessorFlags,
							  DBCOUNTITEM cBindings,
							  const DBBINDING rgBindings[],
							  DBLENGTH cbRowSize,
							  HACCESSOR *phAccessor,
							  DBBINDSTATUS rgStatus[])
	{
		ATLTRACE(atlTraceDBProvider, 2, _T("SFAccessorImpl::CreateAccessor\n"));
		T* pT = (T*)this;
		T::ObjectLock cab(pT);

		if (!phAccessor)
		{
			ATLTRACE(atlTraceDBProvider, 0, _T("SFAccessorImpl::CreateAccessor : Inavlid NULL Parameter for HACCESSOR*\n"));
			return E_INVALIDARG;
		}
		*phAccessor = NULL;
		if (cBindings != 0 && rgBindings == NULL)
		{
			ATLTRACE(atlTraceDBProvider, 0, _T("SFAccessorImpl::CreateAccessor  : Bad Binding array\n"));
			return E_INVALIDARG;
		}
		if (dwAccessorFlags & DBACCESSOR_PASSBYREF)
		{
			CComVariant varByRef;
			HRESULT hr = pT->GetPropValue(&DBPROPSET_ROWSET, 
				DBPROP_BYREFACCESSORS, &varByRef);
			if (FAILED(hr) || varByRef.boolVal == ATL_VARIANT_FALSE)
				return DB_E_BYREFACCESSORNOTSUPPORTED;
		}
		if (!m_bHasParamaters)
		{
			if (dwAccessorFlags & DBACCESSOR_PARAMETERDATA)
				return DB_E_BADACCESSORFLAGS;
		}

		// since our accessor does not provide any further restrictions or optimizations based
		// on the DBACCESSOR_OPTIMIZED flag, the flag will be ignored.  In particular we will 
		// not be returning this flag in the call to IAccessor::GetBindings.  This way we will
		// be complient with the OLEDB specifications and we will not have to prevent the 
		// client from creating additional accessors after the first row is fetched.
		DBACCESSORFLAGS dwMask = DBACCESSOR_OPTIMIZED;
		dwAccessorFlags &= ~dwMask;

		CComVariant varUpdate;
		HRESULT hr = pT->GetPropValue(&DBPROPSET_ROWSET, DBPROP_UPDATABILITY, &varUpdate);
		m_bIsChangeable = (SUCCEEDED(hr) && (varUpdate.iVal & DBPROPVAL_UP_INSERT));

		if (m_bIsCommand || !m_bIsChangeable)
		{
			if (cBindings == 0) // No NULL Accessors on the command
				return DB_E_NULLACCESSORNOTSUPPORTED;
		}

		CTempBuffer<DBBINDSTATUS> tmpBuffer;
		if (rgStatus == NULL && cBindings) // Create a fake status array 
			rgStatus = tmpBuffer.Allocate(cBindings);

		// Validate the Binding passed
		bool bHasBookmarks = false;
		CComVariant varBookmarks;
		HRESULT hrLocal = pT->GetPropValue(&DBPROPSET_ROWSET, DBPROP_BOOKMARKS, &varBookmarks);
		bHasBookmarks = (hrLocal == S_OK &&  varBookmarks.boolVal != ATL_VARIANT_FALSE);

		hr = ValidateBindings(cBindings, rgBindings, rgStatus, bHasBookmarks);
		if (FAILED(hr))
			return hr;
		if (!m_bIsCommand)
		{
			hr = ValidateBindingsFromMetaData(cBindings, rgBindings, rgStatus, 
					bHasBookmarks);
			if (FAILED(hr))
				return hr;
		}
		hr = IAccessorImplBase<BindType>::CreateAccessor(dwAccessorFlags, cBindings,
			rgBindings, cbRowSize, phAccessor,rgStatus);
		if (SUCCEEDED(hr))
		{
			ATLASSERT(*phAccessor != NULL);
			BindType* pBind = (BindType*)*phAccessor;
			//hr = m_rgBindings.SetAt((HACCESSOR)pBind, pBind) ? S_OK : E_OUTOFMEMORY;
			_ATLTRY
			{
				m_rgBindings.SetAt((HACCESSOR)pBind, pBind);
				hr = S_OK;
			}
			_ATLCATCH( e )
			{
				_ATLDELETEEXCEPTION( e );
				hr = E_OUTOFMEMORY;
			}
		}
		return hr;

	}

	STDMETHOD(GetBindings)(HACCESSOR hAccessor,
						   DBACCESSORFLAGS *pdwAccessorFlags,
						   DBCOUNTITEM *pcBindings,
						   DBBINDING **prgBindings)
	{
		ATLTRACE(atlTraceDBProvider, 2, _T("SFAccessorImpl::GetBindings\n"));

		// Zero output parameters in case of failure
		if (pdwAccessorFlags != NULL)
			*pdwAccessorFlags = NULL;

		if (pcBindings != NULL)
			*pcBindings = NULL;

		if (prgBindings != NULL)
			*prgBindings = NULL;

		// Check if any of the out params are NULL pointers
		if ((pdwAccessorFlags && pcBindings && prgBindings) == NULL)
			return E_INVALIDARG;

		BindType* pBind;
		bool bFound = m_rgBindings.Lookup((INT_PTR)hAccessor, pBind);
		HRESULT hr = DB_E_BADACCESSORHANDLE; 
		if (bFound && pBind != NULL)
		{
			*pdwAccessorFlags = pBind->dwAccessorFlags;
			*pcBindings = pBind->cBindings;
			// Get NULL for NULL Accessor
			*prgBindings = (pBind->cBindings) ? (DBBINDING*)CoTaskMemAlloc(*pcBindings * sizeof(DBBINDING)) : NULL;
			if (*prgBindings == NULL && pBind->cBindings) // No Error if NULL Accessor
				return E_OUTOFMEMORY;
			memcpy(*prgBindings, pBind->pBindings, sizeof(DBBINDING) * (*pcBindings));
			hr = S_OK;
		}
		return hr;
	}

	STDMETHOD(ReleaseAccessor)(HACCESSOR hAccessor,
							   DBREFCOUNT *pcRefCount)
	{
		ATLTRACE(atlTraceDBProvider, 2, _T("SFAccessorImpl::ReleaseAccessor\n"));
		T::ObjectLock cab((T*)this);
		BindType* pBind;
		bool bFound = m_rgBindings.Lookup((INT_PTR)hAccessor, pBind);
		if (!bFound || pBind == NULL)
			return DB_E_BADACCESSORHANDLE;
		DBREFCOUNT cRefCount = T::_ThreadModel::Decrement((LONG*)&pBind->dwRef);
		if (pcRefCount != NULL)
			*pcRefCount = cRefCount;
		if (cRefCount == 0)
		{
			delete [] pBind->pBindings;
			delete pBind;
			return m_rgBindings.RemoveKey(hAccessor) ? S_OK : DB_E_BADACCESSORHANDLE;
		}
		return S_OK;
	}

	BindingVector m_rgBindings;
};
