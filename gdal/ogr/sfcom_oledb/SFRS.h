/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Core definitions for SF OLE DB provider.
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
 ****************************************************************************/

// SFRS.h : Declaration of the CSFRowset
#ifndef __CSFRowset_H_
#define __CSFRowset_H_
#include "resource.h"       // main symbols
#include "sfutil.h"
#include "IColumnsRowsetImpl.h"
#include "ICommandWithParametersImpl.h"
#include "SFAccessorImpl.h"
#include "IFRowsetImpl.h"

// Select one of BLOB_NONE, BLOB_IUNKNOWN, BLOB_BYTES, or BLOB_BYTES_BY_REF
// This will determine the type and handling of the geometry column.

#define BLOB_IUNKNOWN

/************************************************************************/
/*                           OGRVirtualArray                            */
/************************************************************************/

class CSFRowset;

class OGRVirtualArray
{
public:
	OGRVirtualArray();
	~OGRVirtualArray();
	void	RemoveAll();
	void	Initialize(OGRLayer *pOGRLayer,int, CSFRowset *);
	BYTE    *GetRow(int iIndex, HRESULT &hr );
        int     CheckRows( int iIndex, int nCount );

private:
        OGRFeature *GetFeature( int iIndex );
        void        ResetCache( int, int );
        
        int     FillGeometry( OGRGeometry *poGeometry, 
                              unsigned char *pabyBuffer,
                              ATLCOLUMNINFO *pColInfo );
        int     FillOGRField( OGRFeature *poFeature, int iField,
                              unsigned char *pabyBuffer,
                              ATLCOLUMNINFO *pColInfo );
        
	int	m_nPackedRecordLength;
	BYTE	*mBuffer;
        int     m_nBufferSize;
	OGRLayer *m_pOGRLayer;
	int	m_nLastRecordAccessed;
	OGRFeatureDefn	*m_pFeatureDefn;
        CSFRowset       *m_pRowset;

        // OGRFeature cache (filled by CheckRows()).
        int      m_nFeatureCacheBase;
        int      m_nFeatureCacheSize;
        OGRFeature **m_papoFeatureCache;
};

/************************************************************************/
/*                              CShapeFile                              */
/************************************************************************/

class CShapeFile
{
  public:
    template <class T>
        static ATLCOLUMNINFO * GetColumnInfo(T* pT, ULONG* pcCols)
	{	
            USES_CONVERSION;

            CComQIPtr<ICommand> spCommand = pT->GetUnknown();
            if (spCommand == NULL)
            {
                if (pcCols != NULL)
                    *pcCols = pT->m_paColInfo.GetSize();
                return pT->m_paColInfo.m_aT;
            }
            
            CPLDebug( "OGR_OLEDB",
                      "CShapeFile::GetColumnInfo() - spCommand != NULL!" );
            
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

/************************************************************************/
/*                   CSFCommandSupportsErrorInfoImpl                    */
/************************************************************************/
class ATL_NO_VTABLE CSFCommandSupportsErrorInfoImpl : public ISupportErrorInfo
{
public:
	STDMETHOD(InterfaceSupportsErrorInfo)(REFIID riid)
	{
		if (IID_ICommand == riid)
			return S_OK;

		return S_FALSE;
	}
};

/************************************************************************/
/*                              CSFCommand                              */
/************************************************************************/
class ATL_NO_VTABLE CSFCommand : 
	public CComObjectRootEx<CComMultiThreadModel>,
	public SFAccessorImpl<CSFCommand>,
	public ICommandTextImpl<CSFCommand>,
	public ICommandPropertiesImpl<CSFCommand>,
	public IObjectWithSiteImpl<CSFCommand>,
	public IConvertTypeImpl<CSFCommand>,
	public IColumnsInfoImpl<CSFCommand>,
        public ICommandWithParametersImpl<CSFCommand>,
        public ICommandPrepare, //20020411 - ryan
	public CSFCommandSupportsErrorInfoImpl
{
public:
BEGIN_COM_MAP(CSFCommand)
	COM_INTERFACE_ENTRY(ICommand)
	COM_INTERFACE_ENTRY(IObjectWithSite)
	COM_INTERFACE_ENTRY(IAccessor)
	COM_INTERFACE_ENTRY(ICommandProperties)
	COM_INTERFACE_ENTRY(ICommandWithParameters)
	COM_INTERFACE_ENTRY2(ICommandText, ICommand)
	COM_INTERFACE_ENTRY(IColumnsInfo)
	COM_INTERFACE_ENTRY(IConvertType)
	COM_INTERFACE_ENTRY(ISupportErrorInfo)
        COM_INTERFACE_ENTRY(ICommandPrepare) //20020411 - ryan
END_COM_MAP()
// ICommand
public:
	HRESULT FinalConstruct()
	{
		HRESULT hr = CConvertHelper::FinalConstruct();
		if (FAILED (hr))
			return hr;
		hr = SFAccessorImpl<CSFCommand>::FinalConstruct();
		if (FAILED(hr))
			return hr;
                m_bHasParamaters = TRUE;
		return CUtlProps<CSFCommand>::FInit();
	}
 
	void FinalRelease();
        
        HRESULT ExtractSpatialQuery( DBPARAMS * );

	HRESULT WINAPI Execute(IUnknown * pUnkOuter, REFIID riid, DBPARAMS * pParams, 
						  LONG * pcRowsAffected, IUnknown ** ppRowset);

	static ATLCOLUMNINFO* GetColumnInfo(CSFCommand* pv, ULONG* pcInfo)
	{
		return CShapeFile::GetColumnInfo(pv,pcInfo);
	}
        
        //ICommandPrepare - 20020111 - ryan
        STDMETHOD(Prepare)(ULONG cExpectedRuns) 
        { 
            return S_OK; 
        }
        STDMETHOD(Unprepare)() 
        { 
            return S_OK; 
        }
        
BEGIN_PROPSET_MAP(CSFCommand)
    BEGIN_PROPERTY_SET(DBPROPSET_ROWSET)
        PROPERTY_INFO_ENTRY(IAccessor)
        PROPERTY_INFO_ENTRY(IColumnsInfo)
        PROPERTY_INFO_ENTRY_VALUE(IColumnsRowset,VARIANT_TRUE)
        PROPERTY_INFO_ENTRY(IConvertType)
	PROPERTY_INFO_ENTRY(IRowset)
	PROPERTY_INFO_ENTRY(IRowsetIdentity)
	PROPERTY_INFO_ENTRY(IRowsetInfo)
	PROPERTY_INFO_ENTRY(IRowsetLocate)
	PROPERTY_INFO_ENTRY(BOOKMARKS)
	PROPERTY_INFO_ENTRY(BOOKMARKSKIPPED)
	PROPERTY_INFO_ENTRY(BOOKMARKTYPE)
	PROPERTY_INFO_ENTRY_VALUE(CANFETCHBACKWARDS,VARIANT_FALSE) 
	PROPERTY_INFO_ENTRY(CANHOLDROWS)
	PROPERTY_INFO_ENTRY_VALUE(CANSCROLLBACKWARDS,VARIANT_FALSE)
	PROPERTY_INFO_ENTRY(LITERALBOOKMARKS)
	PROPERTY_INFO_ENTRY(ORDEREDBOOKMARKS)
	PROPERTY_INFO_ENTRY_VALUE(MAXOPENROWS,1000)
	PROPERTY_INFO_ENTRY_VALUE(ROWTHREADMODEL,DBPROPVAL_RT_APTMTTHREAD|DBPROPVAL_RT_FREETHREAD)
    END_PROPERTY_SET(DBPROPSET_ROWSET)
END_PROPSET_MAP()
		CSimpleArray<ATLCOLUMNINFO>		m_paColInfo;
};

/************************************************************************/
/*                            CSFRowsetImpl                             */
/*                                                                      */
/*      Template closely based on CRowsetImpl from ATLDB.H with a       */
/*      view variations.  It is instanatiated into a real class as      */
/*      CSFRowset below.                                                */
/************************************************************************/

template <class T, class Storage, class CreatorClass,
    class ArrayType = SF_SIMPLE_ARRAY<Storage>,
    class RowClass = CSimpleRow,
    class RowsetInterface = IFRowsetImpl < T, IRowset, RowClass> >
class CSFRowsetImpl :
	public CComObjectRootEx<CreatorClass::_ThreadModel>,
	public SFAccessorImpl<T>,
	public IRowsetIdentityImpl<T, RowClass>,
	public IRowsetCreatorImpl<T>,
	public IRowsetInfoImpl<T, CreatorClass::_PropClass>,
	public IColumnsInfoImpl<T>,
	public IConvertTypeImpl<T>,
        public IColumnsRowsetImpl<T,CreatorClass>,
        public IRowsetChange, //20020411 - ryan
	public RowsetInterface
{
public:

	typedef CreatorClass _RowsetCreatorClass;
	typedef ArrayType _RowsetArrayType;
	typedef CSFRowsetImpl< T, Storage, CreatorClass, ArrayType, RowClass, RowsetInterface> _RowsetBaseClass;

    //IRowsetChange - 20020411 - ryan
    STDMETHOD(DeleteRows) (HCHAPTER hChapter, ULONG cRows, 
                           const HROW rghRows[], DBROWSTATUS rgRowStatus[]) 
    { 
        ATLTRACENOTIMPL("CSFRowsetImpl::DeleteRows"); 
    }
    STDMETHOD(InsertRow) (HCHAPTER hChapter, HACCESSOR hAccessor, 
                          void* pData, HROW* phRow)     
    { 
        ATLTRACENOTIMPL("CSFRowsetImpl::InsertRow"); 
    }
    STDMETHOD(SetData) (HROW hRow, HACCESSOR hAccessor, void* pData)
    { 
        ATLTRACENOTIMPL("CSFRowsetImpl::SetData"); 
    }

BEGIN_COM_MAP(CSFRowsetImpl)
	COM_INTERFACE_ENTRY(IAccessor)
	COM_INTERFACE_ENTRY(IObjectWithSite)
	COM_INTERFACE_ENTRY(IRowsetInfo)
	COM_INTERFACE_ENTRY(IColumnsInfo)
	COM_INTERFACE_ENTRY(IColumnsRowset)
	COM_INTERFACE_ENTRY(IConvertType)
	COM_INTERFACE_ENTRY(IRowsetIdentity)
        COM_INTERFACE_ENTRY_IID(IID_IRowsetLocate, IRowset) //20020411 - ryan
	COM_INTERFACE_ENTRY(IRowset)
        COM_INTERFACE_ENTRY(IRowsetChange) //20020411 - ryan
END_COM_MAP()

        virtual ~CSFRowsetImpl()
        {
            CPLDebug( "OGR_OLEDB", "~CSFRowsetImpl()" );
        }

	HRESULT FinalConstruct()
	{
		HRESULT hr = SFAccessorImpl<T>::FinalConstruct();
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

        OUT_OF_LINE
        HRESULT GetDataHelper(HACCESSOR hAccessor,
                              ATLCOLUMNINFO*& rpInfo,
                              void** ppBinding,
                              void*& rpSrcData,
                              ULONG& rcCols,
                              CComPtr<IDataConvert>& rspConvert,
                              RowClass* pRow)
        {
            ATLASSERT(ppBinding != NULL);
            HRESULT hr;
            
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


	CComBSTR m_strCommandText;
	CComBSTR m_strIndexText;
	ArrayType m_rgRowData;
};

/************************************************************************/
/*                              CSFRowset                               */
/************************************************************************/

class CSFRowset :
public CSFRowsetImpl< CSFRowset, CShapeFile, CSFCommand, OGRVirtualArray>

{
    char          *ProcessSpecialFields( const char *, int *, int * );
public:

                  CSFRowset();
    virtual       ~CSFRowset();

    HRESULT Execute(DBPARAMS * pParams, LONG* pcRowsAffected);

    CSimpleArray<ATLCOLUMNINFO>		m_paColInfo;
    CSimpleArray<int>                   m_panOGRIndex;
    OGRDataSource                      *m_poDS;
    int                                 m_iLayer;
    OGRLayer                           *m_poLayer;
};

#endif //__CSFRowset_H_
