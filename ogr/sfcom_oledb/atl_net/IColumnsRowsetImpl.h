/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  IColumnsRowsetImpl template class.
 * Author:   Len Holgate, len.holgate@jetbyte.com
 *
 * This code was cribbed from web articles by Len.  More information can
 * be found at:  http://www.jetbyte.com/Source/COM/OLEDB/oledb.htm
 *
 ******************************************************************************
 * Copyright (c) 2001, JetByte Limited (www.jetbyte.com)
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
 * Revision 1.5  2002/09/04 14:14:06  warmerda
 * added debug in PopulateRowset()
 *
 * Revision 1.4  2002/08/29 19:02:21  warmerda
 * modified mechanism for processing the SRS substantially
 *
 * Revision 1.3  2002/08/28 18:51:20  warmerda
 * fixed bug2, iLayer was -1 for command rowsets
 *
 * Revision 1.2  2002/08/28 16:38:26  warmerda
 * dont check m_rgRowData.Add() result as true/false for CAtlArray
 *
 * Revision 1.1  2002/08/09 21:36:39  warmerda
 * New
 *
 * Revision 1.4  2001/10/15 15:21:07  warmerda
 * pass raw data points to GetRCDBStatus
 *
 * Revision 1.3  2001/05/31 02:55:30  warmerda
 * return NULL values for OGC IColumnRowset fields on non-spatial fields
 *
 * Revision 1.2  2001/05/30 20:27:51  warmerda
 * strip LFs
 *
 * Revision 1.1  2001/05/28 19:34:13  warmerda
 * New
 *
 */

#ifndef __I_COLUMNS_ROWSET_IMPL__INCLUDED__
#define __I_COLUMNS_ROWSET_IMPL__INCLUDED__

#include <atlcom.h>
#include <atldb.h>
#include "ICRRowsetImpl.h"
#include "cpl_string.h"

template <class T, class DeAllocator = CRunTimeFree < T > >
class CAutoMemRelease
{
public:
	CAutoMemRelease()
	{
		m_pData = NULL;
	}

	CAutoMemRelease(T* pData)
	{
		m_pData = pData;
	}

	~CAutoMemRelease()
	{
		Attach(NULL);
	}

	void Attach(T* pData)
	{
		DeAllocator::Free(m_pData);
		m_pData = pData;
	}

	T* Detach()
	{
		T* pTemp = m_pData;
		m_pData = NULL;
		return pTemp;
	}

	T* m_pData;
};

template <class T>
class CRunTimeFree
{
public:

	static void Free(T* pData)
	{
		delete [] pData;
	}
};

#define PROVIDER_COLUMN_ENTRY_DBID(name, dbid, ordinal, member) \
{ \
	(LPOLESTR)OLESTR(name), \
	(ITypeInfo*)NULL, \
	(ULONG)ordinal, \
	DBCOLUMNFLAGS_ISFIXEDLENGTH, \
   (ULONG)sizeof(((_Class*)0)->member), \
	_GetOleDBType(((_Class*)0)->member), \
	(BYTE)0, \
	(BYTE)0, \
	{ \
		EXPANDGUID(dbid.uGuid.guid), \
		(DWORD)dbid.eKind, \
      (LPOLESTR)dbid.uName.ulPropid\
	}, \
   offsetof(_Class, member) \
},


class CColumnsRowsetRow
{
  public:

    WCHAR    m_DBCOLUMN_IDNAME[129];
    GUID     m_DBCOLUMN_GUID;
    ULONG    m_DBCOLUMN_PROPID;
    WCHAR    m_DBCOLUMN_NAME[129];
    ULONG    m_DBCOLUMN_NUMBER;
    USHORT   m_DBCOLUMN_TYPE;
    IUnknown *m_DBCOLUMN_TYPEINFO;
    ULONG    m_DBCOLUMN_COLUMNSIZE;
    USHORT   m_DBCOLUMN_PRECISION;
    USHORT   m_DBCOLUMN_SCALE;
    ULONG    m_DBCOLUMN_FLAGS;
    WCHAR    m_DBCOLUMN_BASECOLUMNNAME[129];
    WCHAR    m_DBCOLUMN_BASETABLENAME[129];   
    BOOL     m_DBCOLUMN_KEYCOLUMN;

    // special to our provider.
    unsigned int m_nGeomType;
    int	    m_nSpatialRefId;
    WCHAR    m_pszSpatialRefSystem[10240];
   
    CColumnsRowsetRow()
	{
            ClearMembers();
	}

    void ClearMembers()
	{
            m_DBCOLUMN_IDNAME[0] = NULL;
            m_DBCOLUMN_GUID = GUID_NULL;
            m_DBCOLUMN_PROPID = 0;
            m_DBCOLUMN_NAME[0] = 0;
            m_DBCOLUMN_NUMBER = 0;
            m_DBCOLUMN_TYPE = 0;
            m_DBCOLUMN_TYPEINFO = 0;
            m_DBCOLUMN_COLUMNSIZE = 0;
            m_DBCOLUMN_PRECISION = 0;
            m_DBCOLUMN_SCALE = 0;
            m_DBCOLUMN_FLAGS = 0;
            m_DBCOLUMN_BASECOLUMNNAME[0] = NULL;
            m_DBCOLUMN_BASETABLENAME[0] = NULL;
            m_DBCOLUMN_KEYCOLUMN = FALSE;
            m_nGeomType = 0;
            m_nSpatialRefId = 0;
            lstrcpyW(m_pszSpatialRefSystem,L"" );
        }


    BEGIN_PROVIDER_COLUMN_MAP(CColumnsRowsetRow)
	PROVIDER_COLUMN_ENTRY_DBID("DBCOLUMN_IDNAME", DBCOLUMN_IDNAME, 1, m_DBCOLUMN_IDNAME)
        PROVIDER_COLUMN_ENTRY_DBID("DBCOLUMN_GUID", DBCOLUMN_GUID, 2, m_DBCOLUMN_GUID)
        PROVIDER_COLUMN_ENTRY_DBID("DBCOLUMN_PROPID", DBCOLUMN_PROPID, 3, m_DBCOLUMN_PROPID)
        PROVIDER_COLUMN_ENTRY_DBID("DBCOLUMN_NAME", DBCOLUMN_NAME, 4, m_DBCOLUMN_NAME)
        PROVIDER_COLUMN_ENTRY_DBID("DBCOLUMN_NUMBER", DBCOLUMN_NUMBER, 5, m_DBCOLUMN_NUMBER)
        PROVIDER_COLUMN_ENTRY_DBID("DBCOLUMN_TYPE", DBCOLUMN_TYPE, 6, m_DBCOLUMN_TYPE)
        PROVIDER_COLUMN_ENTRY_DBID("DBCOLUMN_TYPEINFO", DBCOLUMN_TYPEINFO, 7, m_DBCOLUMN_TYPEINFO)
        PROVIDER_COLUMN_ENTRY_DBID("DBCOLUMN_COLUMNSIZE", DBCOLUMN_COLUMNSIZE, 8, m_DBCOLUMN_COLUMNSIZE)
        PROVIDER_COLUMN_ENTRY_DBID("DBCOLUMN_PRECISION", DBCOLUMN_PRECISION, 9, m_DBCOLUMN_PRECISION)
        PROVIDER_COLUMN_ENTRY_DBID("DBCOLUMN_SCALE", DBCOLUMN_SCALE, 10, m_DBCOLUMN_SCALE)
        PROVIDER_COLUMN_ENTRY_DBID("DBCOLUMN_FLAGS", DBCOLUMN_FLAGS, 11, m_DBCOLUMN_FLAGS)
        PROVIDER_COLUMN_ENTRY_DBID("DBCOLUMN_BASECOLUMNNAME", DBCOLUMN_BASECOLUMNNAME, 12, m_DBCOLUMN_BASECOLUMNNAME)
        PROVIDER_COLUMN_ENTRY_DBID("DBCOLUMN_BASETABLENAME", DBCOLUMN_BASETABLENAME, 13, m_DBCOLUMN_BASETABLENAME)
        PROVIDER_COLUMN_ENTRY_DBID("DBCOLUMN_KEYCOLUMN", DBCOLUMN_KEYCOLUMN, 14, m_DBCOLUMN_KEYCOLUMN)
        PROVIDER_COLUMN_ENTRY("GEOM_TYPE",15,m_nGeomType)
        PROVIDER_COLUMN_ENTRY("SPATIAL_REF_SYSTEM_ID",16,m_nSpatialRefId)
        PROVIDER_COLUMN_ENTRY("SPATIAL_REF_SYSTEM_WKT",17,m_pszSpatialRefSystem)
        END_PROVIDER_COLUMN_MAP()
        };

template <class T, class CreatorClass>
class ATL_NO_VTABLE IColumnsRowsetImpl : public IColumnsRowset
{
  public:

    class CColumnsRowsetRowset : 
        public CRowsetImpl< CColumnsRowsetRowset , CColumnsRowsetRow, CreatorClass, CAtlArray<CColumnsRowsetRow>, CSimpleRow, ICRRowsetImpl< CColumnsRowsetRowset,IRowset > >
        {
          public:

            DBSTATUS GetRCDBStatus(CSimpleRow* poRC,
                                   ATLCOLUMNINFO*poColInfo,
                                   void *)
            {
                T* pT = (T*) this;
                ULONG      row_id = poRC->m_iRowset;
                CColumnsRowsetRow *psRow;

                if( lstrcmpW(poColInfo->pwszName,L"GEOM_TYPE") != 0
                    && lstrcmpW(poColInfo->pwszName,
                                L"SPATIAL_REF_SYSTEM_ID") != 0
                    && lstrcmpW(poColInfo->pwszName,
                                L"SPATIAL_REF_SYSTEM_WKT") != 0 )
                    return DBSTATUS_S_OK;

                psRow = &(m_rgRowData[row_id]);
                if( lstrcmpW(psRow->m_DBCOLUMN_NAME,L"OGIS_GEOMETRY") == 0 )
                    return DBSTATUS_S_OK;
                
                return DBSTATUS_S_ISNULL;
            }

            HRESULT PopulateRowset(ULONG numCols, DBCOLUMNINFO *pColInfo,
                                   T *poCSFRowset,
                                   OGRDataSource *poDS, int iLayer,
                                   OGRLayer *poLayer )
                {
                    USES_CONVERSION;

                    CPLDebug( "OGR_OLEDB",
                        "PopulateRowset() called for CColumnsRowsetRowset." );
                    
                    for (ULONG i = 0 ; i < numCols; i++)
                    {
                        // copy data out of the pColInfo struct and into the 
                        // rowset array 

                        CColumnsRowsetRow data;

                        // Should select the name correctly, rather than just assuming we'll use the name and
                        // not the guid and propid..

                        lstrcpynW(data.m_DBCOLUMN_IDNAME, pColInfo[i].pwszName , sizeof(data.m_DBCOLUMN_IDNAME));
                        data.m_DBCOLUMN_GUID = GUID_NULL;
                        data.m_DBCOLUMN_PROPID = 0;
                  
                        lstrcpynW(data.m_DBCOLUMN_NAME, pColInfo[i].pwszName , sizeof(data.m_DBCOLUMN_NAME));
                        data.m_DBCOLUMN_NUMBER = pColInfo[i].iOrdinal;
                        data.m_DBCOLUMN_TYPE = pColInfo[i].wType;
                        data.m_DBCOLUMN_TYPEINFO = pColInfo[i].pTypeInfo;
                        data.m_DBCOLUMN_COLUMNSIZE = pColInfo[i].ulColumnSize;
                        data.m_DBCOLUMN_PRECISION = pColInfo[i].bPrecision;
                        data.m_DBCOLUMN_SCALE = pColInfo[i].bScale;
                        data.m_DBCOLUMN_FLAGS = pColInfo[i].dwFlags;
                        lstrcpynW(data.m_DBCOLUMN_BASECOLUMNNAME, pColInfo[i].pwszName, sizeof(data.m_DBCOLUMN_BASECOLUMNNAME));
                        lstrcpynW(data.m_DBCOLUMN_BASETABLENAME, L"Table", sizeof(data.m_DBCOLUMN_BASETABLENAME));
                        data.m_DBCOLUMN_KEYCOLUMN = (pColInfo[i].iOrdinal == 0) || (pColInfo[i].dwFlags & DBCOLUMNFLAGS_ISROWID);

                        // Base the keycolumn decision on the flags too?
                        // Keycolumn is only used if the column has been displayed :( Pity as this renders the bookmarks
                        // useless..

                        // Set the OGIS related information, if this is the
                        // spatial column.
                        if( lstrcmpW(pColInfo[i].pwszName,L"OGIS_GEOMETRY") == 0)
                        {
                            IUnknown *pIU;
                            
                            data.m_nGeomType =
                                SFWkbGeomTypeToDBGEOM(poLayer->GetLayerDefn()->GetGeomType());
                            
                            poCSFRowset->QueryInterface(IID_IUnknown,(void **) &pIU);

                            char * pszWKT = SFGetLayerWKT( poLayer, pIU );

                            if( pszWKT == NULL )
                                pszWKT = CPLStrdup( "" );
                            
                            poCSFRowset->QueryInterface(IID_IUnknown,(void **) &pIU);
                            data.m_nSpatialRefId =
                                SFGetSRSIDFromWKT( pszWKT, pIU );
                            pIU->Release();
                                
                            lstrcpyW(data.m_pszSpatialRefSystem,
                                     A2OLE(pszWKT) );
                            OGRFree( pszWKT );

                        }
                        else
                        {
                            data.m_nGeomType = 0;
                            data.m_nSpatialRefId = 0;
                            lstrcpyW(data.m_pszSpatialRefSystem,L"" );
                        }

                        m_rgRowData.Add(data);
                    }

                    return S_OK;
                }
        };

    STDMETHOD(GetAvailableColumns)(
        ULONG *pcOptColumns,
        DBID **prgOptColumns)
        {
            ATLTRACE2(atlTraceDBProvider, 0, "IColumnsRowsetImpl::GetAvailableColumns()\n");

            if (!pcOptColumns || !prgOptColumns)
            {
                return E_INVALIDARG;
            }

            const ULONG c_numOptColumns = 3;

            *pcOptColumns = c_numOptColumns;

            DBID *pOptCols = (DBID*)CoTaskMemAlloc(sizeof(DBID) * c_numOptColumns);

            memset(pOptCols, 0, sizeof(DBID) * c_numOptColumns);

            pOptCols[0] = DBCOLUMN_BASETABLENAME;
            pOptCols[1] =  DBCOLUMN_BASECOLUMNNAME;
            pOptCols[2] =  DBCOLUMN_KEYCOLUMN;
      
            *pcOptColumns = c_numOptColumns;
            *prgOptColumns = pOptCols;

            return S_OK;
        }

    STDMETHOD(GetColumnsRowset)(
        IUnknown *pUnkOuter,
        ULONG cOptColumns,
        const DBID rgOptColumns[],
        REFIID riid,
        ULONG cPropertySets,
        DBPROPSET rgPropertySets[],
        IUnknown **ppColRowset)
        {
            ATLTRACE2(atlTraceDBProvider, 0, "IColumnsRowsetImpl::GetColumnsRowset()\n");

            // need to create our columns rowset, 
            // then populate it from the actual rowset that we represent...
      
            // We can do that by using IColumnsInfo...

            CColumnsRowsetRowset *pColRowset = 0;

            HRESULT hr = CreateRowset(
                pUnkOuter, 
                riid, 
                cPropertySets, 
                rgPropertySets,
                pColRowset,
                ppColRowset);

            if (SUCCEEDED(hr))
            {
                if (pColRowset)
                {
                    T *pT = (T*)this;

                    CComQIPtr<IColumnsInfo> spColumnsInfo = pT->GetUnknown();

                    if (spColumnsInfo)
                    {
                        ULONG numCols = 0;
                        DBCOLUMNINFO *pColInfo = 0;
                        OLECHAR *pNotUsed = 0;

                        hr = spColumnsInfo->GetColumnInfo(&numCols, &pColInfo, &pNotUsed);

                        if (pNotUsed)
                        {
                            CoTaskMemFree(pNotUsed);
                        }

                        if (SUCCEEDED(hr))
                        {
                            hr = pColRowset->PopulateRowset(numCols, pColInfo,
                                                            pT, 
                                                            pT->m_poDS,
                                                            pT->m_iLayer,
                                                            pT->m_poLayer );
                        }

                        CoTaskMemFree(pColInfo);
                    }
                }
                else
                {
                    hr = E_UNEXPECTED;
                }
            }
      
            return hr;
        }

    private :

        HRESULT CreateRowset(
            IUnknown * pUnkOuter,	
            REFIID riid,				
            ULONG cPropertySets,
            DBPROPSET rgPropertySets[],
            CColumnsRowsetRowset *&pRowsetObj,
            IUnknown **ppRowset)
        {
            HRESULT hr;

            T* pT = (T*)this;

            if (ppRowset != NULL)
            {
                *ppRowset = NULL;
            }

            if ((pUnkOuter != NULL) && !InlineIsEqualUnknown(riid))
            {
                return DB_E_NOAGGREGATION;
            }

            CComPolyObject<CColumnsRowsetRowset>* pPolyObj;
	      
            if (FAILED(hr = CComPolyObject<CColumnsRowsetRowset>::CreateInstance(pUnkOuter, &pPolyObj)))
            {
                return hr;
            }
	      
            // Ref the created COM object and Auto release it on failure
	      
            CComPtr<IUnknown> spUnk;
	      
            hr = pPolyObj->QueryInterface(&spUnk);
	      
            if (FAILED(hr))
            {
                delete pPolyObj; // must hand delete as it is not ref'd
                return hr;
            }
	      
            // Get a pointer to the Rowset instance
            pRowsetObj = &(pPolyObj->m_contained);

            if (FAILED(hr = pRowsetObj->FInit(pT)))
            {
                return hr;
            }

            // Set Properties that were passed in.

            const GUID* ppGuid[1];
            ppGuid[0] = &DBPROPSET_ROWSET;

            // Call SetProperties.  The true in the last parameter indicates
            // the special behavior that takes place on rowset creation (i.e.
            // it succeeds as long as any of the properties were not marked
            // as DBPROPS_REQUIRED.

            hr = pRowsetObj->SetProperties(0, cPropertySets, rgPropertySets, 1, ppGuid, true);

            if (FAILED(hr))
            {
                return hr;
            }

            pRowsetObj->SetSite(pT->GetUnknown());

            if (InlineIsEqualGUID(riid, IID_NULL) || ppRowset == NULL)
            {
                if (ppRowset != NULL)
                    *ppRowset = NULL;
                return hr;
            }

            if (InlineIsEqualGUID(riid, IID_NULL) || ppRowset == NULL)
            {
                if (ppRowset != NULL)
                    *ppRowset = NULL;
                return hr;
            }
            hr = pPolyObj->QueryInterface(riid, (void**)ppRowset);
            if (FAILED(hr))
                return hr;

            
            for (POSITION hBindPos = pT->m_rgBindings.GetStartPosition();
                 hBindPos != NULL;
                 pT->m_rgBindings.GetNext( hBindPos ) )
            {
                T::_BindType* pBind = NULL;
                ATLTRY(pBind = new T::_BindType);
                T::_BindType* pBindSrc = NULL;

                if (pBind == NULL)
                {
                    ATLTRACE2(atlTraceDBProvider, 0, "Failed to allocate memory for new Binding\n");
                    CPLDebug( "OGR_OLEDB", "pBind == NULL in IColumnsRowsetImpl.h");
                    return E_OUTOFMEMORY;
                }
                // auto cleanup on failure
                CAutoMemRelease<T::_BindType> amr(pBind);
                pBindSrc = pT->m_rgBindings.GetValueAt( hBindPos );
                if (pBindSrc == NULL)
                {
                    ATLTRACE2(atlTraceDBProvider, 0, "The map appears to be corrupted, failing!!\n");
                    return E_FAIL;
                }
                if (!pRowsetObj->m_rgBindings.SetAt(pT->m_rgBindings.GetKeyAt(hBindPos), pBind))
                {
                    ATLTRACE2(atlTraceDBProvider, 0, "Failed to add hAccessor to Map\n");
                    CPLDebug( "OGR_OLEDB", "SetAt() failed in IColumnsRowsetImpl.h");
                    return E_OUTOFMEMORY;
                }
                if (pBindSrc->cBindings)
                {
                    ATLTRY(pBind->pBindings = new DBBINDING[pBindSrc->cBindings]);
                    if (pBind->pBindings == NULL)
                    {
                        CPLDebug( "OGR_OLEDB",
                                  "Failed to Allocate dbbinding Array");
                        // We added it, must now remove on failure
                        //pRowsetObj->m_rgBindings.Remove(pT->m_rgBindings.GetKeyAt(iBind));
                        return E_OUTOFMEMORY;
                    }
                }
                else
                {
                    pBind->pBindings = NULL; // NULL Accessor
                }

                pBind->dwAccessorFlags = pBindSrc->dwAccessorFlags;
                pBind->cBindings = pBindSrc->cBindings;
                pBind->dwRef = 1;
                memcpy (pBind->pBindings, pBindSrc->pBindings, (pBindSrc->cBindings)*sizeof(DBBINDING));
                pBind = amr.Detach();
            }

            return hr;
        }

};

#endif // __I_COLUMNS_ROWSET_IMPL__INCLUDED__
