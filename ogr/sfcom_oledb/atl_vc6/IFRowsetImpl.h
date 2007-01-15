/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  RowsetInterface implementation used for main features rowset.
 *           Only used by CSFRowsetImpl in SFRS.h.
 * Author:   Frank Warmerdam <warmerdam@pobox.com>
 *
 * This code is closely derived from the code in ATLDB.H for IRowsetImpl.
 * It basically modifies the CRowsetImpl to check overall status from
 * the row fetching mechanism (which comes from CVirtualArray).  This is
 * all based on the fact that we don't know the rowset size in advance, and
 * so must avoid calling CVirtualArray.GetSize().
 *
 * Note: also contains some fixes mentioned in ICRRowsetImpl.h from which it
 * was directly derived.
 *
 ******************************************************************************
 * Copyright (c) 2002, Frank Warmerdam
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

#ifndef _IFRowsetImpl_INCLUDED
#define _IFRowsetImpl_INCLUDED

// IFRowsetImpl
template <class T, class RowsetInterface,
	  class RowClass = CSimpleRow,
	  class MapClass = CSimpleMap < RowClass::KeyType, RowClass* > >
class ATL_NO_VTABLE IFRowsetImpl : public RowsetInterface
{
public:
    typedef RowClass _HRowClass;
    IFRowsetImpl()
        {
            m_iRowset = 0;
            m_bCanScrollBack = false;
            m_bCanFetchBack = false;
            m_bReset = true;
            m_bRemoveDeleted = true;
            m_bIRowsetUpdate = false;
        }
    ~IFRowsetImpl()
        {
            for (int i = 0; i < m_rgRowHandles.GetSize(); i++)
                delete (m_rgRowHandles.GetValueAt(i));
        }
    HRESULT RefRows(ULONG cRows, const HROW rghRows[], ULONG rgRefCounts[],
                    DBROWSTATUS rgRowStatus[], BOOL bAdd)
        {
            ATLTRACE2(atlTraceDBProvider, 0, "IRowsetImpl::AddRefRows\n");
            if (cRows == 0)
                return S_OK;
            if (rghRows == NULL)
                return E_INVALIDARG;
            T::ObjectLock cab((T*)this);
            BOOL bSuccess1 = FALSE;
            BOOL bFailed1 = FALSE;
            DBROWSTATUS rs;
            DWORD dwRef;
            for (ULONG iRow = 0; iRow < cRows; iRow++)
            {
                HROW hRowCur = rghRows[iRow];
                RowClass* pRow = m_rgRowHandles.Lookup((RowClass::KeyType)hRowCur);
                if (pRow == NULL)
                {
                    ATLTRACE2(atlTraceDBProvider, 0, "Could not find HANDLE %x in list\n");
                    rs = DBROWSTATUS_E_INVALID;
                    dwRef = 0;
                    bFailed1 = TRUE;
                }
                else
                {
                    if (bAdd)
                        dwRef = pRow->AddRefRow();
                    else
                    {
                        dwRef = pRow->ReleaseRow();
                        if (dwRef == 0)
                        {
                            delete pRow;
                            m_rgRowHandles.Remove((RowClass::KeyType)hRowCur);
                        }
                    }
                    bSuccess1 = TRUE;
                    rs = DBROWSTATUS_S_OK;
                }
                if (rgRefCounts)
                    rgRefCounts[iRow] = dwRef;
                if (rgRowStatus != NULL)
                    rgRowStatus[iRow] = rs;
            }
            if (!bSuccess1 && !bFailed1)
            {
                ATLTRACE2(atlTraceDBProvider, 0, "IRowsetImpl::RefRows Unexpected state\n");
                return E_FAIL;
            }
            HRESULT hr = S_OK;
            if (bSuccess1 && bFailed1)
                hr = DB_S_ERRORSOCCURRED;
            if (!bSuccess1 && bFailed1)
                hr = DB_E_ERRORSOCCURRED;
            return hr;
        }

    STDMETHOD(AddRefRows)(ULONG cRows,
                          const HROW rghRows[],
                          ULONG rgRefCounts[],
                          DBROWSTATUS rgRowStatus[])
        {
            ATLTRACE2(atlTraceDBProvider, 0, "IRowsetImpl::AddRefRows\n");
            if (cRows == 0)
                return S_OK;
            return RefRows(cRows, rghRows, rgRefCounts, rgRowStatus, TRUE);
        }
    virtual DBSTATUS GetDBStatus(RowClass* poRC, ATLCOLUMNINFO*poColInfo,
                                 void *pSrcData )
        {
            return DBSTATUS_S_OK;
        }
    OUT_OF_LINE HRESULT GetDataHelper(HACCESSOR hAccessor,
                                      ATLCOLUMNINFO*& rpInfo,
                                      void** ppBinding,
                                      void*& rpSrcData,
                                      ULONG& rcCols,
                                      CComPtr<IDataConvert>& rspConvert,
                                      RowClass* pRow)
        {
            HRESULT hr = S_OK;
        
            ATLASSERT(ppBinding != NULL);
            T* pT = (T*) this;
            *ppBinding = (void*)pT->m_rgBindings.Lookup((int)hAccessor);
            if (*ppBinding == NULL)
                return DB_E_BADACCESSORHANDLE;

            rpSrcData = (void*)pT->m_rgRowData.GetRow(pRow->m_iRowset, hr);
            if( rpSrcData == NULL )
                return hr;

            rpInfo = T::GetColumnInfo((T*)this, &rcCols);
            rspConvert = pT->m_spConvert;
            return S_OK;

        }
    STDMETHOD(GetData)(HROW hRow,
                       HACCESSOR hAccessor,
                       void *pDstData)
        {
            ATLTRACE2(atlTraceDBProvider, 0, "IRowsetImpl::GetData\n");
            if (pDstData == NULL)
                return E_INVALIDARG;
            HRESULT hr = S_OK;
            RowClass* pRow = (RowClass*)hRow;
            if (hRow == NULL || (pRow = m_rgRowHandles.Lookup((RowClass::KeyType)hRow)) == NULL)
                return DB_E_BADROWHANDLE;
            T::_BindType* pBinding;
            void* pSrcData;
            ULONG cCols;
            ATLCOLUMNINFO* pColInfo;
            CComPtr<IDataConvert> spConvert;
            hr = GetDataHelper(hAccessor, pColInfo, (void**)&pBinding, pSrcData, cCols, spConvert, pRow);
            if (FAILED(hr) || hr != S_OK )
                return hr;
            for (ULONG iBind =0; iBind < pBinding->cBindings; iBind++)
            {
                DBBINDING* pBindCur = &(pBinding->pBindings[iBind]);
                for (ULONG iColInfo = 0;
                     iColInfo < cCols && pBindCur->iOrdinal != pColInfo[iColInfo].iOrdinal;
                     iColInfo++);
                if (iColInfo == cCols)
                    return DB_E_BADORDINAL;
                ATLCOLUMNINFO* pColCur = &(pColInfo[iColInfo]);
                // Ordinal found at iColInfo
                BOOL bProvOwn = pBindCur->dwMemOwner == DBMEMOWNER_PROVIDEROWNED;
                bProvOwn;
                DBSTATUS dbStat = GetDBStatus(pRow, pColCur, pSrcData);

                // If the provider's field is NULL, we can optimize this situation,
                // set the fields to 0 and continue.
                if (dbStat == DBSTATUS_S_ISNULL)
                {
                    if (pBindCur->dwPart & DBPART_STATUS)
                        *((DBSTATUS*)((BYTE*)(pDstData) + pBindCur->obStatus)) = dbStat;

                    if (pBindCur->dwPart & DBPART_LENGTH)
                        *((ULONG*)((BYTE*)(pDstData) + pBindCur->obLength)) = 0;

                    if (pBindCur->dwPart & DBPART_VALUE)
                        *((BYTE*)(pDstData) + pBindCur->obValue) = NULL;
                    continue;
                }
                ULONG cbDst = pBindCur->cbMaxLen;
                ULONG cbCol;
                BYTE* pSrcTemp;

                if (bProvOwn && pColCur->wType == pBindCur->wType)
                {
                    pSrcTemp = ((BYTE*)(pSrcData) + pColCur->cbOffset);
                }
                else
                {
                    BYTE* pDstTemp = (BYTE*)pDstData + pBindCur->obValue;
                    switch (pColCur->wType)
                    {
                        case DBTYPE_STR:
                            cbCol = lstrlenA((LPSTR)(((BYTE*)pSrcData) + pColCur->cbOffset));
                            break;
                        case DBTYPE_WSTR:
                        case DBTYPE_BSTR:
                            cbCol = lstrlenW((LPWSTR)(((BYTE*)pSrcData) + pColCur->cbOffset)) * sizeof(WCHAR);
                            break;
                        default:
                            cbCol = pColCur->ulColumnSize;
                            break;
                    }
                    if (pBindCur->dwPart & DBPART_VALUE)
                    {
                        hr = spConvert->DataConvert(pColCur->wType, pBindCur->wType,
                                                    cbCol, &cbDst, (BYTE*)(pSrcData) + pColCur->cbOffset,
                                                    pDstTemp, pBindCur->cbMaxLen, dbStat, &dbStat,
                                                    pBindCur->bPrecision, pBindCur->bScale,0);
                    }
                }
                if (pBindCur->dwPart & DBPART_LENGTH)
                    *((ULONG*)((BYTE*)(pDstData) + pBindCur->obLength)) = cbDst;
                if (pBindCur->dwPart & DBPART_STATUS)
                    *((DBSTATUS*)((BYTE*)(pDstData) + pBindCur->obStatus)) = dbStat;
                if (FAILED(hr))
                    return hr;
            }
            return hr;
        }

    HRESULT CreateRow(LONG lRowsOffset, ULONG& cRowsObtained, HROW* rgRows)
        {
            RowClass* pRow = NULL;
            ATLASSERT(lRowsOffset >= 0);
            RowClass::KeyType key = lRowsOffset+1;
            ATLASSERT(key > 0);
            pRow = m_rgRowHandles.Lookup(key);
            if (pRow == NULL)
            {
                ATLTRY(pRow = new RowClass(lRowsOffset))
                    if (pRow == NULL)
                        return E_OUTOFMEMORY;
                if (!m_rgRowHandles.Add(key, pRow))
                    return E_OUTOFMEMORY;
            }
            pRow->AddRefRow();
            m_bReset = false;
            rgRows[cRowsObtained++] = (HROW)key;
            return S_OK;
        }

    STDMETHOD(GetNextRows)(HCHAPTER /*hReserved*/,
                           LONG lRowsOffset,
                           LONG cRows,
                           ULONG *pcRowsObtained,
                           HROW **prghRows)
        {
            LONG lTmpRows = lRowsOffset;
            ATLTRACE2(atlTraceDBProvider, 0, "IRowsetImpl::GetNextRows\n");
            if (pcRowsObtained != NULL)
                *pcRowsObtained = 0;
            if (prghRows == NULL || pcRowsObtained == NULL)
                return E_INVALIDARG;
            if (cRows == 0)
                return S_OK;
            HRESULT hr = S_OK;
            T* pT = (T*) this;
            T::ObjectLock cab(pT);
            if (lRowsOffset < 0 && !m_bCanScrollBack)
                return DB_E_CANTSCROLLBACKWARDS;
            if (cRows < 0  && !m_bCanFetchBack)
                return DB_E_CANTFETCHBACKWARDS;

            // Calculate # of rows in set and the base fetch position.  If the
            // rowset is at its head position, then lRowOffset < 0 means moving
            // from the BACK of the rowset and not the front.
            if (lRowsOffset == LONG_MIN)
                return DB_S_ENDOFROWSET;

            // In the case where the user is moving backwards after moving
            // forwards, we do not wrap around to the end of the rowset.
            if (m_iRowset == 0 && !m_bReset && cRows < 0)
                return DB_S_ENDOFROWSET;

            int iStepSize = cRows >= 0 ? 1 : -1;

            cRows = abs(cRows);

            lRowsOffset += m_iRowset;

            *pcRowsObtained = 0;
            CAutoMemRelease<HROW, CComFree< HROW > > amr;
            if (*prghRows == NULL)
            {
                int cHandlesToAlloc = cRows;
                if (iStepSize == -1 && lRowsOffset < cHandlesToAlloc)
                    cHandlesToAlloc = lRowsOffset;
                *prghRows = (HROW*)CoTaskMemAlloc((cHandlesToAlloc) * sizeof(HROW*));
                amr.Attach(*prghRows);
            }
            if (*prghRows == NULL)
                return E_OUTOFMEMORY;

            // Check to see if we have the desired number of rows available
            // from the data source. 
            int      cAvailableRows;
            cAvailableRows = pT->m_rgRowData.CheckRows(lRowsOffset, cRows);
            if( cAvailableRows < cRows )
            {
                cRows = cAvailableRows;
                hr = DB_S_ENDOFROWSET;
            }
            
            while (lRowsOffset >= 0 && cRows != 0) 
            {
                // cRows > cRowsInSet && iStepSize < 0
                if (lRowsOffset == 0 && cRows > 0 && iStepSize < 0)
                    break;

                // in the case where we have iStepSize < 0, move the row back
                // further because we want the previous row
                LONG lRow = lRowsOffset;

                if (iStepSize < 0)
                    lRow += iStepSize;

                hr = pT->CreateRow(lRow, *pcRowsObtained, *prghRows);
                if (FAILED(hr))
                {
                    RefRows(*pcRowsObtained, *prghRows, NULL, NULL, FALSE);
                    for (ULONG iRowDel = 0; iRowDel < *pcRowsObtained; iRowDel++)
                        *prghRows[iRowDel] = NULL;
                    *pcRowsObtained = 0;
                    return hr;
                }
                cRows--;
                lRowsOffset += iStepSize;
            }

            if ((lRowsOffset < 0 && cRows)  ||
                (lRowsOffset == 0 && cRows > 0 && iStepSize < 0))
                hr = DB_S_ENDOFROWSET;
        
            m_iRowset = lRowsOffset;
            if (SUCCEEDED(hr))
                amr.Detach();
            return hr;
        }

    STDMETHOD(ReleaseRows)(ULONG cRows,
                           const HROW rghRows[],
                           DBROWOPTIONS rgRowOptions[],
                           ULONG rgRefCounts[],
                           DBROWSTATUS rgRowStatus[])
        {
            ATLTRACE2(atlTraceDBProvider, 0, "IRowsetImpl::ReleaseRows\n");
            if (cRows == 0)
                return S_OK;
            rgRowOptions;
            return RefRows(cRows, rghRows, rgRefCounts, rgRowStatus, FALSE);
        }

    STDMETHOD(RestartPosition)(HCHAPTER /*hReserved*/)
        {
            ATLTRACE2(atlTraceDBProvider, 0, "IRowsetImpl::RestartPosition\n");
            m_iRowset = 0;
            m_bReset = true;
            return S_OK;
        }

    MapClass  m_rgRowHandles;
    DWORD     m_iRowset; // cursor
    unsigned  m_bCanScrollBack:1;
    unsigned  m_bCanFetchBack:1;
    unsigned  m_bIRowsetUpdate:1; // DBPROP_IRowsetUpdate
    unsigned  m_bRemoveDeleted:1; // DBPROP_REMOVEDELETED
    unsigned  m_bReset:1;
};

#endif // ifndef _IFRowsetImpl_INCLUDED
