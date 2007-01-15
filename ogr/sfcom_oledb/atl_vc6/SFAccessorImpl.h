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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.1  2002/08/09 21:36:17  warmerda
 * New
 *
 * Revision 1.2  2001/10/15 15:21:45  warmerda
 * switch to unix text mode
 *
 * Revision 1.1  2001/09/06 03:24:16  warmerda
 * New
 *
 */

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

#ifdef SUPPORT_ATL_NET
#  define SF_ATL_MAP CAtlMap
#else
#  define SF_ATL_MAP CSimpleMap
#endif

// SFAccessorImpl
template <class T, class BindType = ATLBINDINGS,
          class BindingVector = SF_ATL_MAP < int, BindType* > >
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
    OUT_OF_LINE HRESULT InternalFinalConstruct(IUnknown* pUnkThis)
    {
        CComQIPtr<ICommand> spCommand = pUnkThis;
        if (spCommand != NULL)
        {
            m_bIsCommand = TRUE;
            CComQIPtr<ICommandWithParameters> spCommandParams = pUnkThis;
            m_bHasParamaters =  spCommandParams != NULL;
        }
        else // its a Rowset
        {
            CComQIPtr<IRowsetChange> spRSChange = pUnkThis;
            m_bIsChangeable = spRSChange != NULL;
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
        if (m_rgBindings.GetSize())
        ATLTRACE2(atlTraceDBProvider, 0, "SFAccessorImpl::~SFAccessorImpl Bindings still in vector, removing\n");
#endif //_DEBUG
        while (m_rgBindings.GetSize())
        ReleaseAccessor((HACCESSOR)m_rgBindings.GetKeyAt(0), NULL);
    }
    STDMETHOD(AddRefAccessor)(HACCESSOR hAccessor,
                              ULONG *pcRefCount)
    {
        ATLTRACE2(atlTraceDBProvider, 0, "SFAccessorImpl::AddRefAccessor\n");
        if (hAccessor == NULL)
        {
            ATLTRACE2(atlTraceDBProvider, 0, _T("AddRefAccessor : Bad hAccessor\n"));
            return E_INVALIDARG;
        }
        if (pcRefCount == NULL)
        pcRefCount = (ULONG*)_alloca(sizeof(ULONG));

        BindType* pBind = m_rgBindings.Lookup((int)hAccessor);
        *pcRefCount = T::_ThreadModel::Increment((LONG*)&pBind->dwRef);
        return S_OK;
    }
    OUT_OF_LINE ATLCOLUMNINFO* ValidateHelper(ULONG* pcCols, CComPtr<IDataConvert> & rspConvert)
    {
        T* pT = (T*)this;
        rspConvert = pT->m_spConvert;
        return pT->GetColumnInfo(pT, pcCols);
    }
    OUT_OF_LINE HRESULT ValidateBindingsFromMetaData(ULONG cBindings, const DBBINDING rgBindings[],
                                                     DBBINDSTATUS rgStatus[], bool bHasBookmarks)
    {
        HRESULT hr = S_OK;
        ULONG cCols;
        CComPtr<IDataConvert> spConvert;
        ATLCOLUMNINFO* pColInfo = ValidateHelper(&cCols, spConvert);
        ATLASSERT(pColInfo != NULL);
        for (ULONG iBinding = 0; iBinding < cBindings; iBinding++)
        {
            const DBBINDING& rBindCur = rgBindings[iBinding];
            ULONG iOrdAdjusted;
            if (bHasBookmarks)
            iOrdAdjusted = rBindCur.iOrdinal;   // Bookmarks start with ordinal 0
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
                if ((rBindCur.wType & DBTYPE_BYREF) != 0)
                {
                    DBTYPE dbConsumerType = rBindCur.wType & 0xBFFF;
                    DBTYPE dbProviderType = pColInfo[iOrdAdjusted].wType & 0xBFFF;

                    if (dbConsumerType != dbProviderType)
                    {
                        hr = DB_E_ERRORSOCCURRED;
                        rgStatus[iBinding] = DBBINDSTATUS_BADBINDINFO;
                        continue;
                    }
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
                              ULONG cBindings,
                              const DBBINDING rgBindings[],
                              ULONG cbRowSize,
                              HACCESSOR *phAccessor,
                              DBBINDSTATUS rgStatus[])
    {
        ATLTRACE2(atlTraceDBProvider, 0, "SFAccessorImpl::CreateAccessor\n");
                          
        T* pT = (T*)this;
        T::ObjectLock cab(pT);

        if (!phAccessor)
        {
            ATLTRACE2(atlTraceDBProvider, 0, "SFAccessorImpl::CreateAccessor : Inavlid NULL Parameter for HACCESSOR*\n");
            return E_INVALIDARG;
        }
        *phAccessor = NULL;
        if (cBindings != 0 && rgBindings == NULL)
        {
            ATLTRACE2(atlTraceDBProvider, 0, "SFAccessorImpl::CreateAccessor  : Bad Binding array\n");
            return E_INVALIDARG;
        }
        if (dwAccessorFlags & DBACCESSOR_PASSBYREF)
        {
            CComVariant varByRef;
            HRESULT hr = pT->GetPropValue(&DBPROPSET_ROWSET, DBPROP_BYREFACCESSORS, &varByRef);
            if (FAILED(hr) || varByRef.boolVal == VARIANT_FALSE)
            return DB_E_BYREFACCESSORNOTSUPPORTED;
        }
        if (!m_bHasParamaters)
        {
            if (dwAccessorFlags & DBACCESSOR_PARAMETERDATA)
            return DB_E_BADACCESSORFLAGS;
        }
        if (m_bIsCommand || !m_bIsChangeable)
        {
            if (cBindings == 0) // No NULL Accessors on the command
            return DB_E_NULLACCESSORNOTSUPPORTED;
        }

        if (rgStatus == NULL && cBindings) // Create a fake status array
        rgStatus = (DBBINDSTATUS*)_alloca(cBindings*sizeof(DBBINDSTATUS));

        // Validate the Binding passed
        HRESULT hr;
        bool bHasBookmarks = false;
        CComVariant varBookmarks;
        HRESULT hrLocal = pT->GetPropValue(&DBPROPSET_ROWSET, DBPROP_BOOKMARKS, &varBookmarks);
        bHasBookmarks = (hrLocal == S_OK &&  varBookmarks.boolVal == VARIANT_TRUE);

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
            // NFW: Reset accessor handle!
            *phAccessor = g_nNextSFAccessorHandle++;
            hr = m_rgBindings.Add(*phAccessor, pBind) ? S_OK : E_OUTOFMEMORY;
        }
        return hr;

    }

    STDMETHOD(GetBindings)(HACCESSOR hAccessor,
                           DBACCESSORFLAGS *pdwAccessorFlags,
                           ULONG *pcBindings,
                           DBBINDING **prgBindings)
    {
        ATLTRACE2(atlTraceDBProvider, 0, "SFAccessorImpl::GetBindings");

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

        BindType* pBind = m_rgBindings.Lookup((int)hAccessor);
        HRESULT hr = DB_E_BADACCESSORHANDLE;
        if (pBind != NULL)
        {
            *pdwAccessorFlags = pBind->dwAccessorFlags;
            *pcBindings = pBind->cBindings;
            *prgBindings = (DBBINDING*)CoTaskMemAlloc(*pcBindings * sizeof(DBBINDING));
            if (*prgBindings == NULL)
            return E_OUTOFMEMORY;
            memcpy(*prgBindings, pBind->pBindings, sizeof(DBBINDING) * (*pcBindings));
            hr = S_OK;
        }
        return hr;
    }

    STDMETHOD(ReleaseAccessor)(HACCESSOR hAccessor,
                               ULONG *pcRefCount)
    {
        ATLTRACE2(atlTraceDBProvider, 0, _T("SFAccessorImpl::ReleaseAccessor\n"));
        BindType* pBind = m_rgBindings.Lookup((int)hAccessor);
        if (pBind == NULL)
        return DB_E_BADACCESSORHANDLE;

        if (pcRefCount == NULL)
        pcRefCount = (ULONG*)_alloca(sizeof(ULONG));
        *pcRefCount = T::_ThreadModel::Decrement((LONG*)&pBind->dwRef);
        if (!(*pcRefCount))
        {
            delete [] pBind->pBindings;
            delete pBind;
            return m_rgBindings.Remove((int)hAccessor) ? S_OK : DB_E_BADACCESSORHANDLE;
        }
        return S_OK;
    }

    BindingVector m_rgBindings;
};
