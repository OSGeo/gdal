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
/////////////////////////////////////////////////////////////////////////////
// CDataSource
class ATL_NO_VTABLE CSFSource : 
	public CComObjectRootEx<CComSingleThreadModel>,
	public CComCoClass<CSFSource, &CLSID_SF>,
	public IDBCreateSessionImpl<CSFSource, CSFSession>,
	public IDBInitializeImpl<CSFSource>,
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
