/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  RowsetInterface implementation specifically for columns rowset.
 * Author:   Frank Warmerdam <warmerdam@pobox.com>
 *
 * This code is closely derived from the code in ATLDB.H for IRowsetImpl.
 * It basically modifies the CRowsetImpl to call GetRCDBStatus() on the
 * derived class from the GetDBStatus() method, allowing a field to be marked
 * as DBSTATUS_S_ISNULL.  Also, there are some changes to handle null field
 * status properly.
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

#ifndef _ICRRowsetImpl_INCLUDED
#define _ICRRowsetImpl_INCLUDED

// ICRRowsetImpl
template <class T, class RowsetInterface, 
		  class RowClass = CSimpleRow, 
		  class MapClass = CAtlMap < RowClass::KeyType, RowClass* > >
class ATL_NO_VTABLE ICRRowsetImpl : public RowsetInterface
{
public:
	typedef RowClass _HRowClass;
	ICRRowsetImpl()
	{
		m_iRowset = 0;
		m_bCanScrollBack = false;
		m_bCanFetchBack = false;
		m_bRemoveDeleted = true;
		m_bIRowsetUpdate = false;
		m_bReset = true;
		m_bExternalFetch = false;
	}
	~ICRRowsetImpl()
	{
		//for (int i = 0; i < m_rgRowHandles.GetCount(); i++)
		//	delete (m_rgRowHandles.GetValueAt(i));
		POSITION pos = m_rgRowHandles.GetStartPosition();
		while( pos != NULL )
		{
			MapClass::CPair *pPair = m_rgRowHandles.GetNext(pos);
			ATLASSERT( pPair != NULL );
			delete pPair->m_value;
		}
	}
	HRESULT RefRows(DBCOUNTITEM cRows, const HROW rghRows[], DBREFCOUNT rgRefCounts[],
					DBROWSTATUS rgRowStatus[], BOOL bAdd)
	{
		ATLTRACE(atlTraceDBProvider, 2, _T("ICRRowsetImpl::AddRefRows\n"));
		if (cRows == 0)
			return S_OK;
		if (rghRows == NULL)
			return E_INVALIDARG;
		T::ObjectLock cab((T*)this);
		BOOL bSuccess1 = FALSE;
		BOOL bFailed1 = FALSE;
		DBROWSTATUS rs;
		DWORD dwRef;

		__if_exists(T::Fire_OnRowChange)
		{
			// Maintain an array of handles w/ zero ref counts for notification
			CAtlArray<HROW>  arrZeroHandles;
		}

		for (ULONG iRow = 0; iRow < cRows; iRow++)
		{
			HROW hRowCur = rghRows[iRow];
			RowClass* pRow;
			bool bFound = m_rgRowHandles.Lookup((RowClass::KeyType)hRowCur, pRow);
			if (!bFound || pRow == NULL)
			{
				ATLTRACE(atlTraceDBProvider, 0, _T("Could not find HANDLE %x in list\n"));
				rs = DBROWSTATUS_E_INVALID;
				dwRef = 0;
				bFailed1 = TRUE;
			}
			else
			{

				if (pRow->m_status != DBPENDINGSTATUS_UNCHANGED &&
					pRow->m_status != DBPENDINGSTATUS_INVALIDROW &&
					pRow->m_dwRef == 0 && !bAdd)
				{
					if (rgRefCounts)
						rgRefCounts[iRow] = 0;
					if (rgRowStatus != NULL)
						rgRowStatus[iRow] = DBROWSTATUS_E_INVALID;
					bFailed1 = TRUE;
					continue;
				}

				// Check if we're in immediate or deferred mode
				CComVariant varDeferred;
				bool bDeferred;
				T* pT = (T*)this;
				HRESULT hr = pT->GetPropValue(&DBPROPSET_ROWSET, 
					DBPROP_IRowsetUpdate, &varDeferred);
				(FAILED(hr) || varDeferred.boolVal == ATL_VARIANT_FALSE) ? 
					bDeferred = false : bDeferred = true;

				if (!bDeferred && bAdd &&
					pRow->m_status == DBPENDINGSTATUS_DELETED)
				{
					bFailed1 = TRUE;
					if (rgRowStatus != NULL)
						rgRowStatus[iRow] = DBROWSTATUS_E_DELETED;
					continue;
				}

				if (bAdd)
					dwRef = pRow->AddRefRow();
				else
				{
					dwRef = pRow->ReleaseRow();
					if ((pRow->m_status != DBPENDINGSTATUS_UNCHANGED &&
						pRow->m_status != 0 &&
						pRow->m_status != DBPENDINGSTATUS_INVALIDROW) &&
						bDeferred)
					{
						if (rgRefCounts)
							rgRefCounts[iRow] = dwRef;
						if (rgRowStatus != NULL)
							rgRowStatus[iRow] = DBROWSTATUS_S_PENDINGCHANGES;
						bSuccess1 = TRUE;
						continue;
					}

					if (dwRef == 0)
					{
						__if_exists(T::Fire_OnRowsetChange)
						{
							_ATLTRY
							{
								arrZeroHandles.Add(hRowCur);
							}
							_ATLCATCH( e )
							{
								_ATLDELETEEXCEPTION( e );
								return E_FAIL;
							}
						}

						// Now determine if the DBPROP_REMOVEDELETED property
						// is ATL_VARIANT_FALSE.  If so, then do NOT remove the
						// row.
						hr = pT->GetPropValue(&DBPROPSET_ROWSET, 
							DBPROP_REMOVEDELETED, &varDeferred);
						if (FAILED(hr) || varDeferred.boolVal != ATL_VARIANT_FALSE)
						{
							delete pRow;
							m_rgRowHandles.RemoveKey((RowClass::KeyType)hRowCur);
						}
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

		__if_exists(T::Fire_OnRowsetChange)
		{
			if (!bAdd && arrZeroHandles.GetCount() > 0)
			{
				T* pT = (T*)this;
				pT->Fire_OnRowChange(pT, (ULONG_PTR)arrZeroHandles.GetCount(), arrZeroHandles.GetData(), 
					DBREASON_ROW_RELEASE, DBEVENTPHASE_DIDEVENT, FALSE); 
			}
		}

		if (!bSuccess1 && !bFailed1)
		{
			ATLTRACE(atlTraceDBProvider, 0, _T("ICRRowsetImpl::RefRows Unexpected state\n"));
			return E_FAIL;
		}
		HRESULT hr = S_OK;
		if (bSuccess1 && bFailed1)
			hr = DB_S_ERRORSOCCURRED;
		if (!bSuccess1 && bFailed1)
			hr = DB_E_ERRORSOCCURRED;
		return hr;
	}

	STDMETHOD(AddRefRows)(DBCOUNTITEM cRows,
						  const HROW rghRows[],
						  DBREFCOUNT rgRefCounts[],
						  DBROWSTATUS rgRowStatus[])
	{
		ATLTRACE(atlTraceDBProvider, 2, _T("ICRRowsetImpl::AddRefRows\n"));
		if (cRows == 0)
			return S_OK;
		return RefRows(cRows, rghRows, rgRefCounts, rgRowStatus, TRUE);
	}
	virtual DBSTATUS GetDBStatus(RowClass* pRow, ATLCOLUMNINFO *poColInfo)
	{
            T* pT = (T*) this;
            void *pSrcData;
            
            pSrcData = (void*)&(pT->m_rgRowData[pRow->m_iRowset]);
            return pT->GetRCDBStatus(pRow, poColInfo, pSrcData);
	}

	virtual HRESULT SetDBStatus(DBSTATUS*, RowClass* , ATLCOLUMNINFO*)
	{
		// The provider overrides this function to handle special processing
		// for DBSTATUS_S_ISNULL and DBSTATUS_S_DEFAULT.  
		return S_OK;
	}

	OUT_OF_LINE HRESULT GetDataHelper(HACCESSOR hAccessor,
									  ATLCOLUMNINFO*& rpInfo,
									  void** ppBinding,
									  void*& rpSrcData,
									  DBORDINAL& rcCols, 
								  	  CComPtr<IDataConvert>& rspConvert,
									  RowClass* pRow)
	{
		ATLASSERT(ppBinding != NULL);
		T* pT = (T*) this;
//		*ppBinding = (void*)pT->m_rgBindings.Lookup((INT_PTR)hAccessor);
		T::_BindingVector::CPair* pPair = pT->m_rgBindings.Lookup( hAccessor );
		if (pPair == NULL || pPair->m_value == NULL)
			return DB_E_BADACCESSORHANDLE;
		*ppBinding = pPair->m_value;
		rpSrcData = (void*)&(pT->m_rgRowData[pRow->m_iRowset]);
		rpInfo = T::GetColumnInfo((T*)this, &rcCols);
		rspConvert = pT->m_spConvert;
		return S_OK;

	}
	STDMETHOD(GetData)(HROW hRow,
					   HACCESSOR hAccessor,
					   void *pDstData)
	{
		T* pT = (T*)this;
		RowClass* pRow;
		if (hRow == NULL )
			return DB_E_BADROWHANDLE;

		if( !pT->m_rgRowHandles.Lookup((INT_PTR)hRow, pRow))
			return DB_E_BADROWHANDLE;

		if (pRow == NULL)
			return DB_E_BADROWHANDLE;

		return TransferData<T, RowClass, MapClass>
						   (pT, true, pDstData, pRow, &(pT->m_rgRowHandles), hAccessor);
	}

	HRESULT CreateRow(DBROWOFFSET lRowsOffset, DBCOUNTITEM& cRowsObtained, HROW* rgRows)
	{
		RowClass* pRow = NULL;
		ATLASSERT(lRowsOffset >= 0);
		RowClass::KeyType key = lRowsOffset+1;
		ATLASSERT(key > 0);
		bool bFound = m_rgRowHandles.Lookup(key,pRow);
		if (!bFound || pRow == NULL)
		{
			ATLTRY(pRow = new RowClass(lRowsOffset))
			if (pRow == NULL)
				return E_OUTOFMEMORY;
			_ATLTRY
			{
				m_rgRowHandles.SetAt(key, pRow);
			}
			_ATLCATCH( e )
			{
				_ATLDELETEEXCEPTION( e );
				delete pRow;
				pRow = NULL;
				return E_OUTOFMEMORY;
			}
		}
		pRow->AddRefRow();
		m_bReset = false;
		rgRows[cRowsObtained++] = (HROW)key;
		return S_OK;
	}

	HRESULT GetNextRowsSkipDeleted(HCHAPTER /*hReserved*/,
									DBROWOFFSET lRowsOffset,
									DBROWCOUNT cRows,
									DBCOUNTITEM *pcRowsObtained,
									HROW **prghRows)
	{
		ATLTRACE(atlTraceDBProvider, 2, _T("ICRRowsetImpl::GetNextRows\n"));
		T* pT = (T*) this;

		__if_exists(T::Fire_OnRowChange)
		{
			// Check to see if someone is in an event handler.  If we do, then 
			// we should return DB_E_NOTREENTRANT.
			if (!pT->IncrementMutex())
			{
				// Note, we can't set this above this block because we may
				// inadvertantly reset somebody else's pcRowsObtained
				if (pcRowsObtained != NULL)
					*pcRowsObtained = 0;
				return DB_E_NOTREENTRANT;
			}
			else
				pT->DecrementMutex();
		}

		if (pcRowsObtained != NULL)
			*pcRowsObtained = 0;
		if (prghRows == NULL || pcRowsObtained == NULL)
			return E_INVALIDARG;
		if (cRows == 0)
			return S_OK;
		HRESULT hr = S_OK;
		T::ObjectLock cab(pT);
		if (lRowsOffset < 0 && !m_bCanScrollBack)
			return DB_E_CANTSCROLLBACKWARDS;
		if (cRows < 0  && !m_bCanFetchBack)
			return DB_E_CANTFETCHBACKWARDS;

		DBROWOFFSET cRowsInSet = (DBROWOFFSET)pT->m_rgRowData.GetCount();

		DBROWOFFSET iStepSize = cRows >= 0 ? 1 : -1;
		// If cRows == MINLONG_PTR, we can't use ABS on it.  Therefore, we reset it
		// to a value just greater than cRowsInSet
		if (cRows == MINLONG_PTR && cRowsInSet != MINLONG_PTR)
			cRows = cRowsInSet + 2;	// set the value to something we can deal with
		else
			cRows = AbsVal(cRows);

		// First, simulate the operation, skipping over any deleted rows, calculate the number of rows retrieved,
		// and return an error code if appropriate

		DBROWOFFSET nCurrentRow = m_iRowset;

		// Note, if m_bReset, m_iRowset must be 0
		if ( m_bReset && (lRowsOffset < 0 || ( lRowsOffset == 0 && iStepSize < 0 ) ) )
			nCurrentRow = cRowsInSet;

		// skip the rows according to the lRowsOffset value
		if( lRowsOffset > 0 )
		{
			DBROWOFFSET nRowsToSkip = lRowsOffset;

			while( nRowsToSkip > 0 && nCurrentRow <= cRowsInSet )
			{
				RowClass* pRow = NULL;
				RowClass::KeyType key = nCurrentRow + 1;
				bool bFound = m_rgRowHandles.Lookup(key,pRow);
				if( bFound && pRow != NULL )
				{
					if( pRow->m_status == DBPENDINGSTATUS_DELETED )
					{
						nCurrentRow++;
						continue;
					}
				}
				nCurrentRow++;
				nRowsToSkip--;
			}

			if( nCurrentRow > cRowsInSet )
				return DB_S_ENDOFROWSET;
		}
		else if( lRowsOffset < 0 )
		{
			DBROWOFFSET nRowsToSkip = lRowsOffset;
			if (nRowsToSkip == MINLONG_PTR && cRowsInSet != MINLONG_PTR)
				nRowsToSkip = cRowsInSet + 2;	// set the value to something we can deal with
			else
				nRowsToSkip = -nRowsToSkip;

			while( nRowsToSkip > 0 && nCurrentRow > 0 )
			{
				nCurrentRow--;

				RowClass* pRow = NULL;
				RowClass::KeyType key = nCurrentRow + 1;
				bool bFound = m_rgRowHandles.Lookup(key,pRow);
				if( bFound && pRow != NULL )
				{
					if( pRow->m_status == DBPENDINGSTATUS_DELETED )
					{
						continue;
					}
				}
				nRowsToSkip--;
			}

			if( nCurrentRow < 0 )
				return DB_S_ENDOFROWSET;
		}

		DBROWOFFSET nFetchStartPosition = nCurrentRow;

		// now fetch the rows
		DBROWOFFSET cRowsToFetch = cRows;
		DBROWOFFSET cRowsFetched = 0;
		if( iStepSize == 1 )
		{
			while( cRowsToFetch > 0 && nCurrentRow < cRowsInSet )
			{
				RowClass* pRow = NULL;
				RowClass::KeyType key = nCurrentRow + 1;
				bool bFound = m_rgRowHandles.Lookup(key,pRow);
				if( bFound && pRow != NULL )
				{
					if( pRow->m_status == DBPENDINGSTATUS_DELETED )
					{
						nCurrentRow++;
						continue;
					}
				}
				// now we would fetch the row
				cRowsFetched++;
				cRowsToFetch--;
				nCurrentRow++;
			}
		}
		else
		{
			while( cRowsToFetch > 0 && nCurrentRow > 0 )
			{
				nCurrentRow--;
				RowClass* pRow = NULL;
				RowClass::KeyType key = nCurrentRow + 1;
				bool bFound = m_rgRowHandles.Lookup(key,pRow);
				if( bFound && pRow != NULL )
				{
					if( pRow->m_status == DBPENDINGSTATUS_DELETED )
					{
						continue;
					}
				}
				// now we would fetch the row
				cRowsFetched++;
				cRowsToFetch--;
			}
		}

		//  we could not fetch any rows
		if( cRowsFetched == 0 )
			return DB_S_ENDOFROWSET;

		// Simulation completed... no problems detected... we can now perform the real fetching

		// Fire events for OKTODO and ABOUTTODO after all validation has taken
		// place but before any permanent changes to the rowset state take place
		__if_exists(T::Fire_OnRowsetChange)
		{
			// Only fire these events if we're not being called by a bookmark
			// operation (which is why m_bExternalFetch would be set to true)
			if(!m_bExternalFetch)	
			{
				HRESULT hrNotify = pT->Fire_OnRowsetChange(pT, 
					DBREASON_ROWSET_FETCHPOSITIONCHANGE, DBEVENTPHASE_OKTODO, FALSE); 
				if (hrNotify == S_FALSE)
					return DB_E_CANCELED;
				else
				{
					hrNotify = pT->Fire_OnRowsetChange(pT, DBREASON_ROWSET_FETCHPOSITIONCHANGE,
						DBEVENTPHASE_ABOUTTODO, FALSE);
					if (hrNotify == S_FALSE)
						return DB_E_CANCELED;
					else
					{
						hrNotify = pT->Fire_OnRowsetChange(pT, DBREASON_ROWSET_FETCHPOSITIONCHANGE,
							DBEVENTPHASE_SYNCHAFTER, FALSE);
						if (hrNotify == S_FALSE)
							return DB_E_CANCELED;
					}
				}
			}
		}

		nCurrentRow = nFetchStartPosition; // we already calculated the 'start fetch position' in the simulation stage
		ATLASSERT( nCurrentRow >= 0 && nCurrentRow <= cRowsInSet );

		*pcRowsObtained = 0;
		CComHeapPtr<HROW> rghRowsAllocated;
		if (*prghRows == NULL)
		{
			DBROWOFFSET cHandlesToAlloc = cRowsFetched;

			rghRowsAllocated.Allocate(cHandlesToAlloc);
			if(rghRowsAllocated == NULL)
				return E_OUTOFMEMORY;

			*prghRows = rghRowsAllocated;
		}

		// now fetch the rows
		cRowsToFetch = cRows;

		while( cRowsToFetch > 0 && nCurrentRow >= 0 && nCurrentRow <= cRowsInSet )
		{
			if( ( iStepSize == 1 && nCurrentRow == cRowsInSet ) ||
				( iStepSize == -1 && nCurrentRow == 0 ) )
				break;

			DBROWOFFSET lRow = nCurrentRow;

			if( iStepSize > 0 )
			{
				while(true)
				{
					RowClass* pRow = NULL;
					RowClass::KeyType key = lRow + 1;
					bool bFound = m_rgRowHandles.Lookup(key,pRow);
					if( bFound && pRow != NULL )
					{
						if( pRow->m_status == DBPENDINGSTATUS_DELETED )
						{
							lRow++;
							ATLASSERT( lRow < cRowsInSet );
							continue;
						}
					}
					break;
				}
			}
			else
			{
				while(true)
				{
					lRow--;
					RowClass* pRow = NULL;
					RowClass::KeyType key = lRow + 1;
					bool bFound = m_rgRowHandles.Lookup(key,pRow);
					if( bFound && pRow != NULL )
					{
						if( pRow->m_status == DBPENDINGSTATUS_DELETED )
						{
							ATLASSERT( lRow >= 0 );
							continue;
						}
					}
					break;
				}
			}

			ATLASSERT( lRow >= 0 && lRow < cRowsInSet );

			hr = pT->CreateRow(lRow, *pcRowsObtained, *prghRows);

			if (FAILED(hr))
			{
				RefRows(*pcRowsObtained, *prghRows, NULL, NULL, FALSE);
				for (ULONG iRowDel = 0; iRowDel < *pcRowsObtained; iRowDel++)
					*prghRows[iRowDel] = NULL;
				*pcRowsObtained = 0; 
				return hr;
			}

			__if_exists(T::Fire_OnRowsetChange)
			{
				if (!m_bExternalFetch)
					pT->Fire_OnRowsetChange(pT, DBREASON_ROWSET_FETCHPOSITIONCHANGE,
						DBEVENTPHASE_DIDEVENT, TRUE);
			}

			cRowsToFetch--;
			if( iStepSize > 0 )
				nCurrentRow = lRow + iStepSize;
			else
				nCurrentRow = lRow;
		} // while

		// If we have multiple rows fetched, return one event, per the specification
		// containing all rows activated.
		if (*pcRowsObtained >= 1)
		{
			__if_exists(T::Fire_OnRowsetChange)
			{
				CAtlArray<HROW> rgActivated;
				for (size_t ulActivated = 0; ulActivated < *pcRowsObtained; ulActivated++)
				{
					// This is a bit of an assumption that all newly activated
					// rows would have the ref count as 1.  Another way to solve this
					// problem would be to modify the signature of CreateRow to take
					// a CAtlArray<HROW> as a parameter and store the activated rows.
					RowClass* pActiveRow;
					if( m_rgRowHandles.Lookup((*prghRows)[ulActivated], pActiveRow ) &&
						(pActiveRow != NULL && pActiveRow->m_dwRef == 1) )
					{
						_ATLTRY
						{
							rgActivated.Add((*prghRows)[ulActivated]);
						}
						_ATLCATCH( e )
						{
							_ATLDELETEEXCEPTION( e );
							return E_OUTOFMEMORY;
						}
					}
				}
				if (rgActivated.GetCount() > 0)
				{
					pT->Fire_OnRowChange(pT, (DBCOUNTITEM)rgActivated.GetCount(), rgActivated.GetData(), 
						DBREASON_ROW_ACTIVATE, DBEVENTPHASE_DIDEVENT, FALSE); 
				}
			}
		}

		m_iRowset = nCurrentRow;
		if( *pcRowsObtained < (DBCOUNTITEM)cRows ) // we could not fetch the requested # of rows
			hr = DB_S_ENDOFROWSET;

		if (SUCCEEDED(hr))
			rghRowsAllocated.Detach();

		return hr;
	}

	STDMETHOD(GetNextRows)(HCHAPTER hReserved,
						   DBROWOFFSET lRowsOffset,
						   DBROWCOUNT cRows,
						   DBCOUNTITEM *pcRowsObtained,
						   HROW **prghRows)
	{

		if( m_bRemoveDeleted && m_bIRowsetUpdate )
			return GetNextRowsSkipDeleted( hReserved, lRowsOffset, cRows, pcRowsObtained, prghRows );

		DBROWOFFSET lTmpRows = lRowsOffset;
		ATLTRACE(atlTraceDBProvider, 2, _T("ICRRowsetImpl::GetNextRows\n"));
		T* pT = (T*) this;

		__if_exists(T::Fire_OnRowChange)
		{
			// Check to see if someone is in an event handler.  If we do, then 
			// we should return DB_E_NOTREENTRANT.
			if (!pT->IncrementMutex())
			{
				// Note, we can't set this above this block because we may
				// inadvertantly reset somebody else's pcRowsObtained
				if (pcRowsObtained != NULL)
					*pcRowsObtained = 0;
				return DB_E_NOTREENTRANT;
			}
			else
				pT->DecrementMutex();
		}

		if (pcRowsObtained != NULL)
			*pcRowsObtained = 0;
		if (prghRows == NULL || pcRowsObtained == NULL)
			return E_INVALIDARG;
		if (cRows == 0)
			return S_OK;
		HRESULT hr = S_OK;
		T::ObjectLock cab(pT);
		if (lRowsOffset < 0 && !m_bCanScrollBack)
			return DB_E_CANTSCROLLBACKWARDS;
		if (cRows < 0  && !m_bCanFetchBack)
			return DB_E_CANTFETCHBACKWARDS;

		// Calculate # of rows in set and the base fetch position.  If the rowset
		// is at its head position, then lRowOffset < 0 means moving from the BACK 
		// of the rowset and not the front.

		DBROWOFFSET cRowsInSet = (DBROWOFFSET)pT->m_rgRowData.GetCount();

		if (((lRowsOffset == MINLONG_PTR) && (cRowsInSet != MINLONG_PTR))
			|| AbsVal(lRowsOffset) > cRowsInSet ||
			( AbsVal(lRowsOffset) == cRowsInSet && lRowsOffset < 0 && cRows < 0 ) ||
			( AbsVal(lRowsOffset) == cRowsInSet && lRowsOffset > 0 && cRows > 0 ))
			return DB_S_ENDOFROWSET;

		// In the case where the user is moving backwards after moving forwards,
		// we do not wrap around to the end of the rowset.
		if ((m_iRowset == 0 && !m_bReset && cRows < 0) ||
			((m_iRowset + lRowsOffset) > cRowsInSet) ||
			(m_iRowset == cRowsInSet && lRowsOffset >= 0 && cRows > 0))
			return DB_S_ENDOFROWSET;

		// Fire events for OKTODO and ABOUTTODO after all validation has taken
		// place but before any permanent changes to the rowset state take place
		__if_exists(T::Fire_OnRowsetChange)
		{
			// Only fire these events if we're not being called by a bookmark
			// operation (which is why m_bExternalFetch would be set to true)
			if(!m_bExternalFetch)	
			{
				HRESULT hrNotify = pT->Fire_OnRowsetChange(pT, 
					DBREASON_ROWSET_FETCHPOSITIONCHANGE, DBEVENTPHASE_OKTODO, FALSE); 
				if (hrNotify == S_FALSE)
					return DB_E_CANCELED;
				else
				{
					hrNotify = pT->Fire_OnRowsetChange(pT, DBREASON_ROWSET_FETCHPOSITIONCHANGE,
						DBEVENTPHASE_ABOUTTODO, FALSE);
					if (hrNotify == S_FALSE)
						return DB_E_CANCELED;
					else
					{
						hrNotify = pT->Fire_OnRowsetChange(pT, DBREASON_ROWSET_FETCHPOSITIONCHANGE,
							DBEVENTPHASE_SYNCHAFTER, FALSE);
						if (hrNotify == S_FALSE)
							return DB_E_CANCELED;
					}
				}
			}
		}

		// Note, if m_bReset, m_iRowset must be 0
		if (lRowsOffset < 0 && m_bReset)
			m_iRowset = cRowsInSet;

		int iStepSize = cRows >= 0 ? 1 : -1;

		// If cRows == MINLONG_PTR, we can't use ABS on it.  Therefore, we reset it
		// to a value just greater than cRowsInSet
		if (cRows == MINLONG_PTR && cRowsInSet != MINLONG_PTR)
			cRows = cRowsInSet + 2;	// set the value to something we can deal with
		else
			cRows = AbsVal(cRows);

		if (iStepSize < 0 && m_iRowset == 0 && m_bReset && lRowsOffset <= 0)
			m_iRowset = cRowsInSet; 

		lRowsOffset += m_iRowset;

		*pcRowsObtained = 0;
		CComHeapPtr<HROW> rghRowsAllocated;
		if (*prghRows == NULL)
		{
			DBROWOFFSET cHandlesToAlloc = __min(cRowsInSet, cRows);
			if (iStepSize == 1 && (cRowsInSet - lRowsOffset) < cHandlesToAlloc)
				cHandlesToAlloc = cRowsInSet - lRowsOffset;
			if (iStepSize == -1 && lRowsOffset < cHandlesToAlloc)
				cHandlesToAlloc = lRowsOffset;

			rghRowsAllocated.Allocate(cHandlesToAlloc);
			if(rghRowsAllocated == NULL)
				return E_OUTOFMEMORY;			
			*prghRows = rghRowsAllocated;
		}

		while ((lRowsOffset >= 0 && cRows != 0) &&
			((lRowsOffset < cRowsInSet) || (lRowsOffset <= cRowsInSet && iStepSize < 0))) 
		{
			// cRows > cRowsInSet && iStepSize < 0
			if (lRowsOffset == 0 && cRows > 0 && iStepSize < 0)
				break;

			// in the case where we have iStepSize < 0, move the row back
			// further because we want the previous row
			DBROWOFFSET lRow = lRowsOffset;
			if ((lRowsOffset == 0) && (lTmpRows == 0) && (iStepSize < 0))
				lRow = cRowsInSet;

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

			__if_exists(T::Fire_OnRowsetChange)
			{
				if (!m_bExternalFetch)
					pT->Fire_OnRowsetChange(pT, DBREASON_ROWSET_FETCHPOSITIONCHANGE,
						DBEVENTPHASE_DIDEVENT, TRUE);
			}

			cRows--;
			lRowsOffset += iStepSize;
		}

		// If we have multiple rows fetched, return one event, per the specification
		// containing all rows activated.
		if (*pcRowsObtained >= 1)
		{
			__if_exists(T::Fire_OnRowsetChange)
			{
				CAtlArray<HROW> rgActivated;
				for (size_t ulActivated = 0; ulActivated < *pcRowsObtained; ulActivated++)
				{
					// This is a bit of an assumption that all newly activated
					// rows would have the ref count as 1.  Another way to solve this
					// problem would be to modify the signature of CreateRow to take
					// a CAtlArray<HROW> as a parameter and store the activated rows.
					RowClass* pActiveRow;
					if( m_rgRowHandles.Lookup((*prghRows)[ulActivated], pActiveRow ) &&
						(pActiveRow != NULL && pActiveRow->m_dwRef == 1) )
					{
						_ATLTRY
						{
							rgActivated.Add((*prghRows)[ulActivated]);
						}
						_ATLCATCH( e )
						{
							_ATLDELETEEXCEPTION( e );
							return E_OUTOFMEMORY;
						}
					}
				}
				if (rgActivated.GetCount() > 0)
				{
					pT->Fire_OnRowChange(pT, (DBCOUNTITEM)rgActivated.GetCount(), rgActivated.GetData(), 
						DBREASON_ROW_ACTIVATE, DBEVENTPHASE_DIDEVENT, FALSE); 
				}
			}
		}

		m_iRowset = lRowsOffset;
		if ((lRowsOffset >= cRowsInSet && cRows) || (lRowsOffset < 0 && cRows)  ||
			(lRowsOffset == 0 && cRows > 0 && iStepSize < 0))
			hr = DB_S_ENDOFROWSET;

		if (SUCCEEDED(hr))
			rghRowsAllocated.Detach();
		return hr;
	}

	STDMETHOD(ReleaseRows)(DBCOUNTITEM cRows,
						   const HROW rghRows[],
						   DBROWOPTIONS rgRowOptions[],
						   DBREFCOUNT rgRefCounts[],
						   DBROWSTATUS rgRowStatus[])
	{
		ATLTRACE(atlTraceDBProvider, 2, _T("ICRRowsetImpl::ReleaseRows\n"));

		__if_exists(T::Fire_OnRowChange)
		{
			T* pT = (T*) this;

			// Check to see if someone is in an event handler.  If we do, then 
			// we should return DB_E_NOTREENTRANT.
			if (!pT->IncrementMutex())
				return DB_E_NOTREENTRANT;
			else
				pT->DecrementMutex();
		}

		if (cRows == 0)
			return S_OK;
		rgRowOptions;
		return RefRows(cRows, rghRows, rgRefCounts, rgRowStatus, FALSE);
	}

	STDMETHOD(RestartPosition)(HCHAPTER /*hReserved*/)
	{
		ATLTRACE(atlTraceDBProvider, 2, _T("ICRRowsetImpl::RestartPosition\n"));

		T* pT = (T*) this;

		__if_exists(T::Fire_OnRowsetChange)
		{
			// Check to see if someone is in an event handler.  If we do, then 
			// we should return DB_E_NOTREENTRANT.
			if (!pT->IncrementMutex())
				return DB_E_NOTREENTRANT;
			else
				pT->DecrementMutex();


			bool bNeedEvents = ((m_iRowset != 0 || !m_bReset)) ? true : false;

			// Only fire the events iff. we are actually causing a reset
			if (bNeedEvents)
			{
				HRESULT hrNotify = pT->Fire_OnRowsetChange(pT, 
					DBREASON_ROWSET_FETCHPOSITIONCHANGE, DBEVENTPHASE_OKTODO, FALSE); 
				if (hrNotify == S_FALSE)
					return DB_E_CANCELED;
				else
				{
					hrNotify = pT->Fire_OnRowsetChange(pT, DBREASON_ROWSET_FETCHPOSITIONCHANGE,
						DBEVENTPHASE_ABOUTTODO, FALSE);
					if (hrNotify == S_FALSE)
						return DB_E_CANCELED;
					else
					{
						hrNotify = pT->Fire_OnRowsetChange(pT, DBREASON_ROWSET_FETCHPOSITIONCHANGE,
							DBEVENTPHASE_SYNCHAFTER, FALSE);
						if (hrNotify == S_FALSE)
							return DB_E_CANCELED;
					}
				}

			}
		}

		// Check to see if DBPROP_CANHOLDROWS is set to false.  In this case,
		// return a DB_E_ROWSNOTRELEASED.
		CComVariant varHoldRows;
		HRESULT hr = pT->GetPropValue(&DBPROPSET_ROWSET, DBPROP_CANHOLDROWS, 
			&varHoldRows);

		if (FAILED(hr) || varHoldRows.boolVal == ATL_VARIANT_FALSE)
		{
			if (m_rgRowHandles.GetCount() > 0)
			{
				RowClass* pRow = NULL;
				POSITION pos = pT->m_rgRowHandles.GetStartPosition();

				while (pos != NULL)
				{
					MapClass::CPair* pPair = pT->m_rgRowHandles.GetNext(pos);
					ATLASSERT( pPair != NULL );
					HROW hRow = pPair->m_key;
					bool bFound = pT->m_rgRowHandles.Lookup(hRow, pRow);

					if (bFound && pRow != NULL && 
						pRow->m_status != DBPENDINGSTATUS_UNCHANGED)
					{
						__if_exists(T::Fire_OnRowsetChange)
						{
							if (bNeedEvents)
							{
								pT->Fire_OnRowsetChange(pT, DBREASON_ROWSET_FETCHPOSITIONCHANGE, 
										DBEVENTPHASE_FAILEDTODO, TRUE); 
							}
						}

						return DB_E_ROWSNOTRELEASED;
					}
				}
			}
		}

		m_iRowset = 0;
		m_bReset = true;
		__if_exists(T::Fire_OnRowsetChange)
		{
			// listener must comply so blow off ret val.
			if (bNeedEvents)
				pT->Fire_OnRowsetChange(pT, DBREASON_ROWSET_FETCHPOSITIONCHANGE, 
						DBEVENTPHASE_DIDEVENT, TRUE); 
		}
		return S_OK;
	}

	MapClass  m_rgRowHandles;
	DBROWOFFSET m_iRowset; // cursor
	unsigned  m_bCanScrollBack:1;
	unsigned  m_bCanFetchBack:1;
	unsigned  m_bRemoveDeleted:1; // DBPROP_REMOVEDELETED
	unsigned  m_bIRowsetUpdate:1; // DBPROP_IRowsetUpdate
	unsigned  m_bReset:1;
	unsigned  m_bExternalFetch:1;
};

#endif // ifndef _ICRRowsetImpl_INCLUDED
