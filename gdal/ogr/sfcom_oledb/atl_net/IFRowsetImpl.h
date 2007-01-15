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
 * DEALINGS IN THE SOFTWARE.
 ******************************************************************************
 *
 * $Log$
 * Revision 1.1  2002/08/09 21:36:39  warmerda
 * New
 *
 * Revision 1.2  2002/02/05 20:42:46  warmerda
 * use CheckRows() in GetNextRows() to check availability
 *
 * Revision 1.1  2002/01/31 16:47:24  warmerda
 * New
 *
 */

#ifndef _IFRowsetImpl_INCLUDED
#define _IFRowsetImpl_INCLUDED

template <class T, class RowClass, class MapClass>
HRESULT SFTransferData(T* pT, bool bReading, void* pData, 
					 RowClass* pRow, MapClass* /*pMap*/, HACCESSOR hAccessor)
{
	ATLTRACE(atlTraceDBProvider, 2, _T("SFTransferData\n"));
	bool bFailed = false;
	bool bSucceeded = false;
	HRESULT hr = S_OK;

	__if_exists(T::Fire_OnFieldChange)
	{
		CAtlArray<DBORDINAL> rgColumns;
		HROW hNotifyRow = NULL;
		//HROW hNotifyRow = pT->m_rgRowHandles.ReverseLookup(pRow);
		{
			POSITION pos = pT->m_rgRowHandles.GetStartPosition();
			while( pos != NULL )
			{
				MapClass::CPair* pPair = pT->m_rgRowHandles.GetNext( pos );
				ATLASSERT( pPair != NULL );
				if( pPair->m_value == pRow )
				{
					hNotifyRow = pPair->m_key;
					break;
				}
			}
		}
	}

	__if_exists(T::Fire_OnRowChange)
	{
		// We need to send the DBREASON_ROW_FIRSTCHANGE notification's
		// SYNCHAFTER phase in this function.  IFF. we're deferred and
		// we have a newly changed row.

		CComVariant varDeferred;
		bool bDeferred;
		hr = pT->GetPropValue(&DBPROPSET_ROWSET, DBPROP_IRowsetUpdate, 
							&varDeferred);
		(FAILED(hr) || varDeferred.boolVal == ATL_VARIANT_FALSE) ? bDeferred = false : bDeferred = true;
	}

        int      cAvailableRows;
        cAvailableRows = pT->m_rgRowData.CheckRows(pRow->m_iRowset, 1 );
        
	// Check for a deleted row
	if( cAvailableRows < 1 )
	{
		__if_exists(T::Fire_OnFieldChange)
		{
			if( !bReading )
			{
				SendRowsFirstChangeFailureNotification( pT, pRow, &hNotifyRow, bDeferred );
			}
		}
		return DB_E_DELETEDROW;
	}

	// NOTE: This was checking against DBPENDINGSTATUS_DELETED.  Instead, it 
	// should check for DBPENDINGSTATUS_INVALIDROW (means a forced deleted
	// row).

	if (pRow->m_status == DBPENDINGSTATUS_INVALIDROW)
	{
		__if_exists(T::Fire_OnFieldChange)
		{
			if( !bReading )
			{
				SendRowsFirstChangeFailureNotification( pT, pRow, &hNotifyRow, bDeferred );
			}
		}
		return DB_E_DELETEDROW;
	}

	T::_BindType* pBinding;
	bool bFound = pT->m_rgBindings.Lookup((INT_PTR)hAccessor, pBinding);
	if (!bFound || pBinding == NULL)
	{
		__if_exists(T::Fire_OnFieldChange)
		{
			if( !bReading )
			{
				SendRowsFirstChangeFailureNotification( pT, pRow, &hNotifyRow, bDeferred );
			}
		}
		return DB_E_BADACCESSORHANDLE;
	}

	if (pData == NULL && pBinding->cBindings != 0)
	{
		__if_exists(T::Fire_OnFieldChange)
		{
			if( !bReading )
			{
				SendRowsFirstChangeFailureNotification( pT, pRow, &hNotifyRow, bDeferred );
			}
		}
		return E_INVALIDARG;
	}

	void* pDstData;
	void* pSrcData;
	if (bReading)
	{
		pDstData = pData;
		pSrcData = (void*) pT->m_rgRowData.GetRow( (int)pRow->m_iRowset,
                                                           hr );
                if( pSrcData == NULL || hr != S_OK )
                    return hr;
	}
	else
	{
		pSrcData = pData;
		pDstData = (void*) pT->m_rgRowData.GetRow( (int)pRow->m_iRowset,
                                                           hr );
                if( pDstData == NULL || hr != S_OK )
                    return hr;
	}

	if (!bReading)
	{
		// Send the OKTODO notification
		__if_exists(T::Fire_OnFieldChange)
		{
			if (/* pRow->m_status != DBPENDINGSTATUS_NEW && */
				pRow->m_status != (DBPENDINGSTATUS_NEW | DBPENDINGSTATUS_UNCHANGED))
			{
				HRESULT hrNotify;
				for (DBORDINAL l=0; l<pBinding->cBindings; l++)
				{
					_ATLTRY
					{
						rgColumns.Add(pBinding->pBindings[l].iOrdinal);
					}
					_ATLCATCH( e )
					{
						_ATLDELETEEXCEPTION( e );
						return E_FAIL;
					}
				}

				hrNotify = pT->Fire_OnFieldChange(pT, hNotifyRow, pBinding->cBindings, 
					rgColumns.GetData(), DBREASON_COLUMN_SET, 
					DBEVENTPHASE_OKTODO, FALSE);
				if ((hrNotify != S_OK) && (hrNotify != E_FAIL))
				{
					__if_exists(T::Fire_OnRowChange)
					{
						if (bDeferred)
							pT->Fire_OnRowChange(pT, 1, &hNotifyRow, 
								DBREASON_ROW_FIRSTCHANGE, DBEVENTPHASE_FAILEDTODO, TRUE);
						return DB_E_CANCELED;
					}
				}

				hrNotify = pT->Fire_OnFieldChange(pT, hNotifyRow, pBinding->cBindings,
					rgColumns.GetData(), DBREASON_COLUMN_SET, DBEVENTPHASE_ABOUTTODO, 
					FALSE);
				if ((hrNotify != S_OK) && (hrNotify != E_FAIL))
				{
					__if_exists(T::Fire_OnRowChange)
					{
						if (bDeferred)
							pT->Fire_OnRowChange(pT, 1, &hNotifyRow, 
								DBREASON_ROW_FIRSTCHANGE, DBEVENTPHASE_FAILEDTODO, TRUE);
						return DB_E_CANCELED;
					}
				}

				hrNotify = pT->Fire_OnFieldChange(pT, hNotifyRow, pBinding->cBindings, 
					rgColumns.GetData(), DBREASON_COLUMN_SET, 
					DBEVENTPHASE_SYNCHAFTER, FALSE);
				if ((hrNotify != S_OK) && (hrNotify != E_FAIL))
				{
					__if_exists(T::Fire_OnRowChange)
					{
						if (bDeferred)
							pT->Fire_OnRowChange(pT, 1, &hNotifyRow, 
								DBREASON_ROW_FIRSTCHANGE, DBEVENTPHASE_FAILEDTODO, TRUE);
						return DB_E_CANCELED;
					}
				}
			}
		}

		__if_exists(T::Fire_OnRowChange)
		{
			if(bDeferred && pRow->m_status != DBPENDINGSTATUS_CHANGED &&
				pRow->m_status != (DBPENDINGSTATUS_NEW | DBPENDINGSTATUS_UNCHANGED))
			{
				HRESULT hrNotify = pT->Fire_OnRowChange(pT, 1, &hNotifyRow, 
					DBREASON_ROW_FIRSTCHANGE, DBEVENTPHASE_SYNCHAFTER, FALSE);

				if ((hrNotify != S_OK) && (hrNotify != E_FAIL))
				{
					__if_exists(T::Fire_OnFieldChange)
					{
						pT->Fire_OnFieldChange(pT, hNotifyRow, pBinding->cBindings, 
							rgColumns.GetData(), DBREASON_COLUMN_SET, 
							DBEVENTPHASE_FAILEDTODO, TRUE);
					}
					return DB_E_CANCELED;
				}
			}
		}
	}


	DBORDINAL cCols;
	ATLCOLUMNINFO* pColInfo = T::GetColumnInfo(pT, &cCols);
	for (DBORDINAL iBind =0; iBind < pBinding->cBindings; iBind++)
	{
		DBBINDING* pBindCur = &(pBinding->pBindings[iBind]);
		DBORDINAL iColInfo;
		for (iColInfo = 0; 
			 iColInfo < cCols && pBindCur->iOrdinal != pColInfo[iColInfo].iOrdinal;
			 iColInfo++);
		if (iColInfo == cCols)
		{
			__if_exists(T::Fire_OnFieldChange)
			{
				if( !bReading )
				{
					SendColumnSetFailureNotification( pT, hNotifyRow, pBinding, rgColumns );
					SendRowsFirstChangeFailureNotification( pT, pRow, &hNotifyRow, bDeferred );
				}
			}
			return DB_E_BADORDINAL;
		}
		ATLCOLUMNINFO* pColCur = &(pColInfo[iColInfo]);
		// Ordinal found at iColInfo

		BYTE* pSrcTemp = (bReading) ? (BYTE*)pSrcData + pColCur->cbOffset : 
									  (BYTE*)pSrcData + pBindCur->obValue;

		BYTE* pDstTemp = NULL;
		if (pBindCur->dwPart & DBPART_VALUE)
			pDstTemp = (bReading) ? (BYTE*)pDstData + pBindCur->obValue : 
						 (BYTE*)pDstData + pColCur->cbOffset;


		if (!bReading)
		{
			// Check to see that the appropriate data parts are available
			if ((pBindCur->dwPart & DBPART_LENGTH) &&
				!(pBindCur->dwPart & DBPART_VALUE) &&
				!(pBindCur->dwPart & DBPART_STATUS))
			{
				__if_exists(T::Fire_OnFieldChange)
				{
					if (/* pRow->m_status != DBPENDINGSTATUS_NEW && */
						pRow->m_status != (DBPENDINGSTATUS_NEW | DBPENDINGSTATUS_UNCHANGED))
					{
						pT->Fire_OnFieldChange(pT, hNotifyRow, pBinding->cBindings,
							rgColumns.GetData(), DBREASON_COLUMN_SET, DBEVENTPHASE_FAILEDTODO, 
							TRUE);
					}
				}

				// Not sure why you would want to run SetData here!
				bFailed = true;
				continue;
			}
		}

		// Handle the the status for any consumer issues
		DBSTATUS dbStat = DBSTATUS_S_OK;
		if (bReading)
		{
			dbStat = pT->GetDBStatus(pRow, pColCur);

			if (dbStat == DBSTATUS_S_ISNULL)
			{
				if (pBindCur->dwPart & DBPART_STATUS)
					*((DBSTATUS*)((BYTE*)(pDstData) + pBindCur->obStatus)) = dbStat; 

				// Set the length to 0 as reqiured by the spec.
				if (pBindCur->dwPart & DBPART_LENGTH)
					*((DBLENGTH*)((BYTE*)(pDstData) + pBindCur->obLength)) = 0;

				// Set the destination value to NULL
				if (pBindCur->dwPart & DBPART_VALUE)
					*pDstTemp = NULL;

				continue;
			}
		}
		else
		{
			// Allow the provider to do checking for DBSTATUS_S_ISNULL
			if (pBindCur->dwPart & DBPART_STATUS)
			{
				dbStat = *((DBSTATUS*)((BYTE*)(pSrcData) + pBindCur->obStatus));

				// Return DBSTATUS_E_UNAVAILABLE if the status is DBSTATUS_S_OK
				//	and either the value part is not bound or the length part is
				//	bound and the type is DBTYPE_BYTES.

				// There was another entry of code here with LENGTH, NO VALUE,
				//	and status was not DBSTATUS_S_ISNULL.  May need to regenerate that
				if (dbStat == DBSTATUS_S_OK)
				{
					if (!(pBindCur->dwPart & DBPART_VALUE) ||
						((pBindCur->dwPart & DBPART_LENGTH) && (pBindCur->wType == DBTYPE_BYTES)))
					{
						// Can't set non-null columns w/o a value part
						__if_exists(T::Fire_OnFieldChange)
						{
							if (/* pRow->m_status != DBPENDINGSTATUS_NEW && */
								pRow->m_status != (DBPENDINGSTATUS_NEW | DBPENDINGSTATUS_UNCHANGED))
							{
								pT->Fire_OnFieldChange(pT, hNotifyRow, pBinding->cBindings,
									rgColumns.GetData(), DBREASON_COLUMN_SET, DBEVENTPHASE_FAILEDTODO, 
									TRUE);
							}
						}

						bFailed = true;
						*((DBSTATUS*)((BYTE*)(pSrcData) + pBindCur->obStatus)) = DBSTATUS_E_UNAVAILABLE;
						continue;
					}
				}

				switch (dbStat)
				{
				case DBSTATUS_S_ISNULL:
					if (!(pColCur->dwFlags & DBCOLUMNFLAGS_ISNULLABLE) ||
						FAILED(pT->SetDBStatus(&dbStat, pRow, pColCur)))
					{
						__if_exists(T::Fire_OnFieldChange)
						{
							if (/* pRow->m_status != DBPENDINGSTATUS_NEW && */
								pRow->m_status != (DBPENDINGSTATUS_NEW | DBPENDINGSTATUS_UNCHANGED))
							{
								pT->Fire_OnFieldChange(pT, hNotifyRow, 
									pBinding->cBindings, rgColumns.GetData(), 
									DBREASON_COLUMN_SET, DBEVENTPHASE_FAILEDTODO, 
									TRUE);
							}
						}

						// Special processing for attempting to write, read-only columns
						if (!(pColCur->dwFlags & DBCOLUMNFLAGS_ISNULLABLE))
							*((DBSTATUS*)((BYTE*)(pSrcData) + pBindCur->obStatus)) = DBSTATUS_E_INTEGRITYVIOLATION;

						bFailed = true;
					}
					else
					{
						__if_exists(T::Fire_OnFieldChange)
						{
							if (/* pRow->m_status != DBPENDINGSTATUS_NEW && */
								pRow->m_status != (DBPENDINGSTATUS_NEW | DBPENDINGSTATUS_UNCHANGED))
							{
								pT->Fire_OnFieldChange(pT, hNotifyRow, 1, &iBind, 
									DBREASON_COLUMN_SET, DBEVENTPHASE_DIDEVENT, TRUE);
							}
						}
						bSucceeded = true;
						dbStat = DBSTATUS_S_OK;
						if (pBindCur->dwPart & DBPART_VALUE)
							*pDstTemp = NULL;
					}
					continue;
					break;
				case DBSTATUS_S_DEFAULT:
				case DBSTATUS_S_IGNORE:
				{
					HRESULT hrStatus = pT->SetDBStatus(&dbStat, pRow, pColCur);
					*((DBSTATUS*)((BYTE*)(pSrcData) + pBindCur->obStatus)) = dbStat;

					if (FAILED(hrStatus))
					{
						__if_exists(T::Fire_OnFieldChange)
						{
							if (/* pRow->m_status != DBPENDINGSTATUS_NEW && */
								pRow->m_status != (DBPENDINGSTATUS_NEW | DBPENDINGSTATUS_UNCHANGED))
							{
								pT->Fire_OnFieldChange(pT, hNotifyRow, pBinding->cBindings, 
									rgColumns.GetData(), DBREASON_COLUMN_SET, 
									DBEVENTPHASE_FAILEDTODO, TRUE);
							}
						}

						// Note, status should be set by SetDBStatus
						bFailed = true;
					}
					else
					{
						__if_exists(T::Fire_OnFieldChange)
						{
							if (/* pRow->m_status != DBPENDINGSTATUS_NEW && */
								pRow->m_status != (DBPENDINGSTATUS_NEW | DBPENDINGSTATUS_UNCHANGED))
							{
								pT->Fire_OnFieldChange(pT, hNotifyRow, pBinding->cBindings, 
									rgColumns.GetData(), DBREASON_COLUMN_SET, 
									DBEVENTPHASE_DIDEVENT, TRUE);
							}
						}
						bSucceeded = true;
					}
					continue;
					break;
				}
				case DBSTATUS_S_OK:
					// Still call SetDBStatus here as they may have locks on 
					// integrity contstraints to observe
					if (FAILED(pT->SetDBStatus(&dbStat, pRow, pColCur)))
					{
						__if_exists(T::Fire_OnFieldChange)
						{
							if (/* pRow->m_status != DBPENDINGSTATUS_NEW && */
								pRow->m_status != (DBPENDINGSTATUS_NEW | DBPENDINGSTATUS_UNCHANGED))
							{
								pT->Fire_OnFieldChange(pT, hNotifyRow, pBinding->cBindings, 
									rgColumns.GetData(), DBREASON_COLUMN_SET, 
									DBEVENTPHASE_FAILEDTODO, TRUE);
							}
						}

						bFailed = true;
						*((DBSTATUS*)((BYTE*)(pSrcData) + pBindCur->obStatus)) = dbStat;
						continue;
					}
					break;
				default:
					*((DBSTATUS*)((BYTE*)(pSrcData) + pBindCur->obStatus)) = DBSTATUS_E_BADSTATUS;

					__if_exists(T::Fire_OnFieldChange)
					{
						if (/* pRow->m_status != DBPENDINGSTATUS_NEW && */
							pRow->m_status != (DBPENDINGSTATUS_NEW | DBPENDINGSTATUS_UNCHANGED))
						{
							pT->Fire_OnFieldChange(pT, hNotifyRow, pBinding->cBindings, 
								rgColumns.GetData(), DBREASON_COLUMN_SET, 
								DBEVENTPHASE_FAILEDTODO, TRUE);
						}
					}

					bFailed = true;
					continue;
					break;
				}
			}
		}


		// Determine sizes of input and output columns
		DBLENGTH cbCol = 0;
		DBLENGTH cbDst;
		if (bReading)
			cbDst = pBindCur->cbMaxLen;
		else
			cbDst = pColCur->ulColumnSize;

		switch (pColCur->wType)
		{
		case DBTYPE_STR:
			if (bReading)
				cbCol = lstrlenA((LPSTR)(((BYTE*)pSrcData) + pColCur->cbOffset));
			else
			{
				// Use the length field when setting data 
				if (pBindCur->dwPart & DBPART_LENGTH)
					cbCol = *((DBLENGTH*)((BYTE*)(pSrcData) + pBindCur->obLength));
				else
					cbCol = lstrlenA((LPSTR)(pSrcTemp));	// was cbDst

				if (cbCol >= cbDst)
				{
					if (cbCol > (cbDst + 1)) // over maximum case
					{
						__if_exists(T::Fire_OnFieldChange)
						{
							if (/* pRow->m_status != DBPENDINGSTATUS_NEW && */
								pRow->m_status != (DBPENDINGSTATUS_NEW | DBPENDINGSTATUS_UNCHANGED))
							{
								pT->Fire_OnFieldChange(pT, hNotifyRow, pBinding->cBindings, 
									rgColumns.GetData(), DBREASON_COLUMN_SET, 
									DBEVENTPHASE_FAILEDTODO, TRUE);
							}
						}

						bFailed = true;
						if (pBindCur->dwPart & DBPART_STATUS)
							*((DBSTATUS*)((BYTE*)(pSrcData) + pBindCur->obStatus)) = DBSTATUS_E_CANTCONVERTVALUE;
						continue;
					}
				}
				cbCol = cbDst;	// Leave room for NULL term. need to copy for WSTR
			}
			break;
		case DBTYPE_WSTR:
		case DBTYPE_BSTR:
			if (bReading)
				cbCol = lstrlenW((LPWSTR)(((BYTE*)pSrcData) + pColCur->cbOffset)) * sizeof(WCHAR);
			else
			{
				if (pBindCur->dwPart & DBPART_LENGTH)
					cbCol = *((DBLENGTH*)((BYTE*)(pSrcData) + pBindCur->obLength)); 
				else
					cbCol = lstrlenW((LPWSTR)(pSrcData)) * sizeof(WCHAR);

				if (cbCol >= cbDst)
				{
					if (cbCol > (cbDst + 1)) // over maximum case
					{
						__if_exists(T::Fire_OnFieldChange)
						{
							if (/* pRow->m_status != DBPENDINGSTATUS_NEW && */
								pRow->m_status != (DBPENDINGSTATUS_NEW | DBPENDINGSTATUS_UNCHANGED))
							{
								pT->Fire_OnFieldChange(pT, hNotifyRow, pBinding->cBindings, 
									rgColumns.GetData(), DBREASON_COLUMN_SET, 
									DBEVENTPHASE_FAILEDTODO, TRUE);
							}
						}

						bFailed = true;
						if (pBindCur->dwPart & DBPART_STATUS)
							*((DBSTATUS*)((BYTE*)(pSrcData) + pBindCur->obStatus)) = DBSTATUS_E_CANTCONVERTVALUE;
						continue;
					}
				}
				cbCol = cbDst;	// Leave room for NULL term. need to copy for WSTR
			}
			break;
		case DBTYPE_BYTES:
			if (bReading)
				cbCol = pColCur->ulColumnSize;
			else
			{
				if (pBindCur->dwPart & DBPART_LENGTH)
					cbCol = *((DBLENGTH *)((BYTE*)(pSrcData) + pBindCur->obLength)); 
				else
				{
					__if_exists(T::Fire_OnFieldChange)
					{
						if (/* pRow->m_status != DBPENDINGSTATUS_NEW && */
							pRow->m_status != (DBPENDINGSTATUS_NEW | DBPENDINGSTATUS_UNCHANGED))
						{
							pT->Fire_OnFieldChange(pT, hNotifyRow, pBinding->cBindings, 
								rgColumns.GetData(), DBREASON_COLUMN_SET, 
								DBEVENTPHASE_FAILEDTODO, TRUE);
						}
					}

					// If no length part is bound for DBTYPE_BYTES, it is an error
					bFailed = true;
					continue;
				}

				if (cbCol >= cbDst)
					cbCol = cbDst;	// Leave room for NULL term. need to copy for WSTR
			}
			break;
		default:
			if (bReading)
				cbCol = pColCur->ulColumnSize;
			else
				cbDst = pColCur->ulColumnSize;

			break;
		}


		// Handle cases where we have provider owned memory.  Note, these should be
		// with DBTYPE_BYREF (otherwise, it doesn't make sense).
		if (pBindCur->dwPart & DBPART_VALUE)
		{
			if (pBindCur->dwMemOwner == DBMEMOWNER_PROVIDEROWNED 
				&& pBindCur->wType & DBTYPE_BYREF)
			{
				*(BYTE**)pDstTemp = pSrcTemp;
			}
			else
			{
				ATLASSERT(pT->m_spConvert != NULL);
				hr = pT->m_spConvert->DataConvert(pColCur->wType, pBindCur->wType, 
						cbCol, &cbDst, pSrcTemp, pDstTemp, pBindCur->cbMaxLen, 
						dbStat, &dbStat, pBindCur->bPrecision, pBindCur->bScale,0);
			}
		}
		if (pBindCur->dwPart & DBPART_LENGTH)
		{
			if (bReading)
			{
				if (!(pBindCur->dwPart & DBPART_VALUE))
					cbDst = cbCol;	// We didn't have the data convert to correct this
				*((DBLENGTH*)((BYTE*)(pDstData) + pBindCur->obLength)) = (dbStat == DBSTATUS_S_ISNULL) ? 0 : cbDst;
			}
			else
				*((DBLENGTH*)((BYTE*)(pSrcData) + pBindCur->obLength)) = cbDst;
		}
		if (pBindCur->dwPart & DBPART_STATUS)
		{
			if (bReading)
				*((DBSTATUS*)((BYTE*)(pDstData) + pBindCur->obStatus)) = dbStat; 
			else
				*((DBSTATUS*)((BYTE*)(pSrcData) + pBindCur->obStatus)) = dbStat; 
		}

		if (FAILED(hr))
		{
			if (!bReading)
			{
				__if_exists(T::Fire_OnFieldChange)
				{
					if (/* pRow->m_status != DBPENDINGSTATUS_NEW && */
						pRow->m_status != (DBPENDINGSTATUS_NEW | DBPENDINGSTATUS_UNCHANGED))
					{
						pT->Fire_OnFieldChange(pT, hNotifyRow, pBinding->cBindings, 
							rgColumns.GetData(), DBREASON_COLUMN_SET, 
							DBEVENTPHASE_FAILEDTODO, TRUE);
					}
				}
			}

			bFailed = true;
		}
		else
		{
			bSucceeded = true;
		}
	}

	// Return error codes to the consumer
	if (bFailed)
	{
		__if_exists(T::Fire_OnFieldChange)
		{
			if( !bReading )
			{
//				SendColumnSetFailureNotification( pT, hNotifyRow, pBinding, rgColumns );
				SendRowsFirstChangeFailureNotification( pT, pRow, &hNotifyRow, bDeferred );
			}
		}
		return (bSucceeded != false) ? DB_S_ERRORSOCCURRED : DB_E_ERRORSOCCURRED;
	}
	else
	{
		if (!bReading)
		{
			__if_exists(T::Fire_OnFieldChange)
			{
				if (/* pRow->m_status != DBPENDINGSTATUS_NEW && */
					pRow->m_status != (DBPENDINGSTATUS_NEW | DBPENDINGSTATUS_UNCHANGED))
				{
					pT->Fire_OnFieldChange(pT, hNotifyRow, pBinding->cBindings, 
						rgColumns.GetData(), DBREASON_COLUMN_SET, 
						DBEVENTPHASE_DIDEVENT, TRUE);
				}
			}
		}

		return hr;
	}
}

// IFRowsetImpl
template <class T, class RowsetInterface, 
		  class RowClass = CSimpleRow, 
		  class MapClass = CAtlMap < RowClass::KeyType, RowClass* > >
class ATL_NO_VTABLE IFRowsetImpl : public RowsetInterface
{
public:
	typedef RowClass _HRowClass;
	IFRowsetImpl()
	{
		m_iRowset = 0;
		m_bCanScrollBack = false;
		m_bCanFetchBack = false;
		m_bRemoveDeleted = true;
		m_bIRowsetUpdate = false;
		m_bReset = true;
		m_bExternalFetch = false;
	}
	~IFRowsetImpl()
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
		ATLTRACE(atlTraceDBProvider, 2, _T("IFRowsetImpl::AddRefRows\n"));
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
			ATLTRACE(atlTraceDBProvider, 0, _T("IFRowsetImpl::RefRows Unexpected state\n"));
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
		ATLTRACE(atlTraceDBProvider, 2, _T("IFRowsetImpl::AddRefRows\n"));
		if (cRows == 0)
			return S_OK;
		return RefRows(cRows, rghRows, rgRefCounts, rgRowStatus, TRUE);
	}
	virtual DBSTATUS GetDBStatus(RowClass* , ATLCOLUMNINFO*)
	{
		return DBSTATUS_S_OK;
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
                HRESULT hr = S_OK;
                
		ATLASSERT(ppBinding != NULL);
		T* pT = (T*) this;
//		*ppBinding = (void*)pT->m_rgBindings.Lookup((INT_PTR)hAccessor);
		T::_BindingVector::CPair* pPair = pT->m_rgBindings.Lookup( hAccessor );
		if (pPair == NULL || pPair->m_value == NULL)
			return DB_E_BADACCESSORHANDLE;
		*ppBinding = pPair->m_value;

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
		T* pT = (T*)this;
		RowClass* pRow;
		if (hRow == NULL )
			return DB_E_BADROWHANDLE;

		if( !pT->m_rgRowHandles.Lookup((INT_PTR)hRow, pRow))
			return DB_E_BADROWHANDLE;

		if (pRow == NULL)
			return DB_E_BADROWHANDLE;

		return SFTransferData<T, RowClass, MapClass>
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

	STDMETHOD(GetNextRows)(HCHAPTER hReserved,
                               DBROWOFFSET lRowsOffset,
                               DBROWCOUNT cRows,
                               DBCOUNTITEM *pcRowsObtained,
                               HROW **prghRows)
	{
		DBROWOFFSET lTmpRows = lRowsOffset;
		ATLTRACE(atlTraceDBProvider, 2, _T("IFRowsetImpl::GetNextRows\n"));
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
		if (cRows < 0 )
			return DB_E_CANTFETCHBACKWARDS;

		// Calculate # of rows in set and the base fetch position.  If the rowset
		// is at its head position, then lRowOffset < 0 means moving from the BACK 
		// of the rowset and not the front.

                if( lRowsOffset == MINLONG_PTR )
                    return DB_S_ENDOFROWSET;
                
		// In the case where the user is moving backwards after moving forwards,
		// we do not wrap around to the end of the rowset.
		if ((m_iRowset == 0 && !m_bReset && cRows < 0) )
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
                {
                    CPLDebug( "OGR_ATL", "Backup not supported.");
                    return DB_E_CANTFETCHBACKWARDS;
                }


		int iStepSize = cRows >= 0 ? 1 : -1;

                cRows = AbsVal(cRows);
		lRowsOffset += m_iRowset;

		*pcRowsObtained = 0;
		CComHeapPtr<HROW> rghRowsAllocated;
		if (*prghRows == NULL)
		{
			DBROWOFFSET cHandlesToAlloc = cRows;
			if (iStepSize == -1 && lRowsOffset < cHandlesToAlloc)
				cHandlesToAlloc = lRowsOffset;

			rghRowsAllocated.Allocate(cHandlesToAlloc);
			if(rghRowsAllocated == NULL)
				return E_OUTOFMEMORY;			
			*prghRows = rghRowsAllocated;
		}

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
			DBROWOFFSET lRow = lRowsOffset;

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

                if ((lRowsOffset < 0 && cRows)  ||
                    (lRowsOffset == 0 && cRows > 0 && iStepSize < 0))
                    hr = DB_S_ENDOFROWSET;
        
		m_iRowset = lRowsOffset;

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
		ATLTRACE(atlTraceDBProvider, 2, _T("IFRowsetImpl::ReleaseRows\n"));

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
		ATLTRACE(atlTraceDBProvider, 2, _T("IFRowsetImpl::RestartPosition\n"));

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

#endif // ifndef _IFRowsetImpl_INCLUDED
