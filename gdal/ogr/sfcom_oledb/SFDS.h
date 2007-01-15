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
 * Revision 1.22  2002/08/30 15:24:29  warmerda
 * moved FinalConstruct into SFDS.cpp
 *
 * Revision 1.21  2002/08/29 18:55:35  warmerda
 * restructured to put SRS and OGRDataSource in CSFSource
 *
 * Revision 1.20  2002/08/13 13:59:42  warmerda
 * removed MEDC related property page stuff
 *
 * Revision 1.19  2002/08/13 13:44:10  warmerda
 * removed custom MEDC property pages
 *
 * Revision 1.18  2002/08/12 18:06:32  warmerda
 * added IDataSourceKey support from Autodesk
 *
 * Revision 1.17  2002/05/08 20:27:48  warmerda
 * added support for caching OGRDataSources
 *
 * Revision 1.16  2002/05/06 15:12:39  warmerda
 * improve IErrorInfo support
 *
 * Revision 1.15  2002/04/25 17:38:28  warmerda
 * added custom connection tabs
 *
 * Revision 1.14  2001/11/09 19:06:07  warmerda
 * disable various DS properties, added debugging
 *
 * Revision 1.13  2001/10/24 16:02:02  warmerda
 * added debug call
 *
 * Revision 1.12  2001/10/02 14:25:45  warmerda
 * Added MAXTABLESINSELECT and SQLSUPPORT properties for MapGuide
 *
 * Revision 1.11  2001/06/07 15:50:14  warmerda
 * implement DBMSNAME property for ESRI compatibility
 *
 * Revision 1.10  2001/05/28 19:35:31  warmerda
 * added ROSETCONVERSIONSONCOMMAND property
 *
 * Revision 1.9  1999/11/22 17:17:18  warmerda
 * reformat
 *
 * Revision 1.8  1999/07/23 19:20:27  kshih
 * Modifications for errors etc...
 *
 * Revision 1.7  1999/07/21 13:25:50  warmerda
 * Remoted extra include of sfutil.h.
 *
 * Revision 1.6  1999/07/20 17:11:11  kshih
 * Use OGR code
 *
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
#include "SFSess.h"

class CSFSource;

/////////////////////////////////////////////////////////////////////////////
// IDBInitializeImpl

template <class T>
class ATL_NO_VTABLE MyIDBInitializeImpl : public IDBInitializeImpl<T>
{
  public:
             MyIDBInitializeImpl()
        {
            CPLDebug( "OGR_OLEDB", "MyIDBInitializeImpl() constructor" );
        }
    virtual ~MyIDBInitializeImpl()
	{
            CPLDebug( "OGR_OLEDB", "~MyIDBInitializeImpl()" );
	}

    STDMETHOD(Initialize)(void)
	{
            T* poCSFSource = static_cast<T*>(this);
            HRESULT hr;
            CPLDebug( "OGR_OLEDB", "MyIDBInitializeImpl::Initialize()" );
            hr =IDBInitializeImpl<T>::Initialize();

            if (SUCCEEDED(hr))
                hr = poCSFSource->OpenDataSource();
	
            return hr;
	}

  private:
};

/////////////////////////////////////////////////////////////////////////////
// CDataSourceISupportErrorInfoImpl

class ATL_NO_VTABLE CDataSourceISupportErrorInfoImpl : public ISupportErrorInfo
{
public:
	STDMETHOD(InterfaceSupportsErrorInfo)(REFIID riid)
	{
		if (IID_IDBInitialize == riid)
			return S_OK;

		return S_FALSE;
	}
};

/////////////////////////////////////////////////////////////////////////////
// IDataSourceKeyImpl

template <class T>
class ATL_NO_VTABLE IDataSourceKeyImpl : public IDataSourceKey
{
public:
	
	STDMETHOD(GetKey)(ULONG* nKey)
	{
		T* pT = static_cast<T*>(this);
		*nKey = (ULONG) pT;
		return S_OK;
	}
};

/////////////////////////////////////////////////////////////////////////////
// CSFSource

class ATL_NO_VTABLE CSFSource : 
	public CComObjectRootEx<CComSingleThreadModel>,
	public CComCoClass<CSFSource, &CLSID_SF>,
	public IDBCreateSessionImpl<CSFSource, CSFSession>,
	public MyIDBInitializeImpl<CSFSource>,
	public IDBPropertiesImpl<CSFSource>,
	public IPersistImpl<CSFSource>,
	public IInternalConnectionImpl<CSFSource>,
	public CDataSourceISupportErrorInfoImpl,
	public IDataSourceKeyImpl<CSFSource>
{

private:
    OGRDataSource *m_poDS;

    int m_bSRSListInitialized;
    int m_nSRSCount;
    char **m_papszSRSList;
    
public:
    CSFSource();
    virtual ~CSFSource();

    HRESULT OpenDataSource();
    
    OGRDataSource* GetDataSource( void ) { return m_poDS; }

    int         GetSRSCount();
    const char *GetSRSWKT( int );
    int         GetSRSID( const char * );

    void        InitSRSList();
    
    HRESULT     FinalConstruct();

    void        DumpProperties();

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
		PROPERTY_INFO_ENTRY(ROWSETCONVERSIONSONCOMMAND)
		PROPERTY_INFO_ENTRY_VALUE(DBMSNAME,OLESTR("OGR"))
		PROPERTY_INFO_ENTRY_VALUE(MAXTABLESINSELECT,1)
		PROPERTY_INFO_ENTRY_VALUE(SQLSUPPORT,DBPROPVAL_SQL_SUBMINIMUM)
	END_PROPERTY_SET(DBPROPSET_DATASOURCEINFO)
	BEGIN_PROPERTY_SET(DBPROPSET_DBINIT)
//		PROPERTY_INFO_ENTRY(AUTH_PASSWORD)
//		PROPERTY_INFO_ENTRY(AUTH_PERSIST_SENSITIVE_AUTHINFO)
//		PROPERTY_INFO_ENTRY(AUTH_USERID)
		PROPERTY_INFO_ENTRY(INIT_DATASOURCE)
		PROPERTY_INFO_ENTRY(INIT_HWND)
//		PROPERTY_INFO_ENTRY(INIT_LCID)
//		PROPERTY_INFO_ENTRY(INIT_LOCATION)
//		PROPERTY_INFO_ENTRY(INIT_MODE)
//		PROPERTY_INFO_ENTRY(INIT_PROMPT)
		PROPERTY_INFO_ENTRY(INIT_PROVIDERSTRING)
//		PROPERTY_INFO_ENTRY(INIT_TIMEOUT)
	END_PROPERTY_SET(DBPROPSET_DBINIT)
	BEGIN_PROPERTY_SET(DBPROPSET_OGIS_SPATIAL_OPS)
		PROPERTY_INFO_ENTRY_EX(OGIS_TOUCHES, VT_BOOL, DBPROPFLAGS_READ, VARIANT_FALSE,0)
		PROPERTY_INFO_ENTRY_EX(OGIS_WITHIN, VT_BOOL, DBPROPFLAGS_READ, VARIANT_FALSE,0)
		PROPERTY_INFO_ENTRY_EX(OGIS_CONTAINS, VT_BOOL, DBPROPFLAGS_READ, VARIANT_FALSE,0)
		PROPERTY_INFO_ENTRY_EX(OGIS_CROSSES, VT_BOOL, DBPROPFLAGS_READ, VARIANT_FALSE,0)
		PROPERTY_INFO_ENTRY_EX(OGIS_OVERLAPS, VT_BOOL, DBPROPFLAGS_READ, VARIANT_FALSE,0)
		PROPERTY_INFO_ENTRY_EX(OGIS_DISJOINT, VT_BOOL, DBPROPFLAGS_READ, VARIANT_FALSE,0)
		PROPERTY_INFO_ENTRY_EX(OGIS_INTERSECT, VT_BOOL, DBPROPFLAGS_READ, VARIANT_FALSE,0)
		PROPERTY_INFO_ENTRY_EX(OGIS_ENVELOPE_INTERSECTS, VT_BOOL, DBPROPFLAGS_READ, VARIANT_TRUE,0)
		PROPERTY_INFO_ENTRY_EX(OGIS_INDEX_INTERSECTS, VT_BOOL, DBPROPFLAGS_READ, VARIANT_FALSE,0)
	END_PROPERTY_SET(DBPROPSET_OGIS_SPATIAL_OPS)
	CHAIN_PROPERTY_SET(CSFCommand)
END_PROPSET_MAP()
    
BEGIN_COM_MAP(CSFSource)
    COM_INTERFACE_ENTRY(IDBCreateSession)
    COM_INTERFACE_ENTRY(IDBInitialize)
    COM_INTERFACE_ENTRY(IDBProperties)
    COM_INTERFACE_ENTRY(IPersist)
    COM_INTERFACE_ENTRY(IInternalConnection)
    COM_INTERFACE_ENTRY(ISupportErrorInfo)
    COM_INTERFACE_ENTRY(IDataSourceKey)
END_COM_MAP()

};
#endif //__CSFSource_H_
