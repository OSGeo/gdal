/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Declaration of the CSFSession
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
 * Revision 1.5  1999/06/15 03:03:16  kshih
 * Provider Type schema Rowset
 *
 * Revision 1.4  1999/06/13 17:49:46  warmerda
 * Added copyright header, and some comments.
 *
 */

#ifndef __CSFSession_H_
#define __CSFSession_H_
#include "resource.h"       // main symbols
#include "SFRS.h"
#include "oledbgis.h"

class CSFSessionTRSchemaRowset;
class CSFSessionColSchemaRowset;
class CSFSessionPTSchemaRowset;
class CSFSessionSchemaOGISTables;
class CSFSessionSchemaOGISGeoColumns;
class CSFSessionSchemaSpatRef;

/////////////////////////////////////////////////////////////////////////////
// CSFSession
class ATL_NO_VTABLE CSFSession : 
	public CComObjectRootEx<CComSingleThreadModel>,
	public IGetDataSourceImpl<CSFSession>,
	public IOpenRowsetImpl<CSFSession>,
	public ISessionPropertiesImpl<CSFSession>,
	public IObjectWithSiteSessionImpl<CSFSession>,
	public IDBSchemaRowsetImpl<CSFSession>,
	public IDBCreateCommandImpl<CSFSession, CSFCommand>
{
public:
	CSFSession()
	{
	}
	HRESULT FinalConstruct()
	{
		return FInit();
	}
	STDMETHOD(OpenRowset)(IUnknown *pUnk, DBID *pTID, DBID *pInID, REFIID riid,
					   ULONG cSets, DBPROPSET rgSets[], IUnknown **ppRowset)
	{
		CSFRowset* pRowset;
		return CreateRowset(pUnk, pTID, pInID, riid, cSets, rgSets, ppRowset, pRowset);
	}
BEGIN_PROPSET_MAP(CSFSession)
	BEGIN_PROPERTY_SET(DBPROPSET_SESSION)
		PROPERTY_INFO_ENTRY(SESS_AUTOCOMMITISOLEVELS)
	END_PROPERTY_SET(DBPROPSET_SESSION)
END_PROPSET_MAP()
BEGIN_COM_MAP(CSFSession)
	COM_INTERFACE_ENTRY(IGetDataSource)
	COM_INTERFACE_ENTRY(IOpenRowset)
	COM_INTERFACE_ENTRY(ISessionProperties)
	COM_INTERFACE_ENTRY(IObjectWithSite)
	COM_INTERFACE_ENTRY(IDBCreateCommand)
	COM_INTERFACE_ENTRY(IDBSchemaRowset)
END_COM_MAP()
BEGIN_SCHEMA_MAP(CSFSession)
	SCHEMA_ENTRY(DBSCHEMA_TABLES, CSFSessionTRSchemaRowset)
	SCHEMA_ENTRY(DBSCHEMA_COLUMNS, CSFSessionColSchemaRowset)
	SCHEMA_ENTRY(DBSCHEMA_PROVIDER_TYPES, CSFSessionPTSchemaRowset)
	SCHEMA_ENTRY(DBSCHEMA_OGIS_FEATURE_TABLES, CSFSessionSchemaOGISTables)
	SCHEMA_ENTRY(DBSCHEMA_OGIS_GEOMETRY_COLUMNS,CSFSessionSchemaOGISGeoColumns)
	SCHEMA_ENTRY(DBSCHEMA_OGIS_SPATIAL_REF_SYSTEMS,CSFSessionSchemaSpatRef);
END_SCHEMA_MAP()
};

/////////////////////////////////////////////////////////////////////////////
// CSFSessionTRSchemaRowset
class CSFSessionTRSchemaRowset : 
	public CRowsetImpl< CSFSessionTRSchemaRowset, CTABLESRow, CSFSession>
{
public:
	HRESULT Execute(LONG* pcRowsAffected, ULONG, const VARIANT*)
	{
		USES_CONVERSION;
		CTABLESRow trData;
		
		lstrcpyW(trData.m_szType, OLESTR("TABLE"));
		lstrcpyW(trData.m_szDesc, OLESTR("The Directory Table"));
		lstrcpyW(trData.m_szTable,A2OLE("DBF"));
		if (!m_rgRowData.Add(trData))
			return E_OUTOFMEMORY;
		*pcRowsAffected = 1;

		return S_OK;
	}
};
 
	static DBFHandle GetDBFHandle(IUnknown *pIUnknown)
	{
        DBFHandle       hDBF = NULL;
        HRESULT         hr;
        IRowsetInfo     *pRInfo;
        
        hr = pIUnknown->QueryInterface(IID_IRowsetInfo,(void **) &pRInfo);
        
        
        if (SUCCEEDED(hr))
        {
            IGetDataSource  *pIGetDataSource;
            hr = pRInfo->GetSpecification(IID_IGetDataSource, (IUnknown **) &pIGetDataSource);
            pRInfo->Release();
            
            if (SUCCEEDED(hr))
            {
                IDBProperties *pIDBProp;
                
                hr = pIGetDataSource->GetDataSource(IID_IDBProperties, (IUnknown **) &pIDBProp);
                pIGetDataSource->Release();
                
                if (SUCCEEDED(hr))
                {
                    DBPROPIDSET sPropIdSets[1];
                    DBPROPID    rgPropIds[1];
                    
                    ULONG       nPropSets;
                    DBPROPSET   *rgPropSets;
                    
                    rgPropIds[0] = DBPROP_INIT_DATASOURCE;
                    
                    sPropIdSets[0].cPropertyIDs = 1;
                    sPropIdSets[0].guidPropertySet = DBPROPSET_DBINIT;
                    sPropIdSets[0].rgPropertyIDs = rgPropIds;
                    
                    pIDBProp->GetProperties(1,sPropIdSets,&nPropSets,&rgPropSets);
                    
                    if (rgPropSets)
                    {
                        USES_CONVERSION;
                        char *pszDataSource;
                        char *pszSource = (char *)  OLE2A(rgPropSets[0].rgProperties[0].vValue.bstrVal);
                        
                        hDBF = DBFOpen(pszSource,"r");
                        pszDataSource = (char *) malloc(5+strlen(pszSource));
                        strcpy(pszDataSource,pszSource);
                        strcat(pszDataSource,".dbf");
                        
                        hDBF = DBFOpen(pszDataSource,"r");
                        free(pszDataSource);
                    }
                    
                    if (rgPropSets)
                    {
                        int i;
                        for (i=0; i < (int) nPropSets; i++)
                        {
                            CoTaskMemFree(rgPropSets[i].rgProperties);
                        }
                        CoTaskMemFree(rgPropSets);
                    }
                    
                    pIDBProp->Release();
                }
            }
        }
        
        return hDBF;
        
    }

/////////////////////////////////////////////////////////////////////////////
// CSFSessionColSchemaRowset
class CSFSessionColSchemaRowset : 
	public CRowsetImpl< CSFSessionColSchemaRowset, CCOLUMNSRow, CSFSession>
{
public:
    

	HRESULT Execute(LONG* pcRowsAffected, ULONG, const VARIANT*)
	{
		USES_CONVERSION;
		DBFHandle	hDBF;
		int			i;
		IUnknown    *pIU;



		QueryInterface(IID_IUnknown,(void **) &pIU);
		hDBF = GetDBFHandle((IUnknown *) pIU);
		pIU->Release();

		if (!hDBF)
			return E_FAIL;

		for (i=0; i < DBFGetFieldCount(hDBF); i++)
		{
			CCOLUMNSRow trData;
			char szFieldName[12];
			int	 nWidth;
			int  nDecimals;


			switch(DBFGetFieldInfo(hDBF,i,szFieldName,&nWidth,&nDecimals))
			{
			case FTString:
				trData.m_nDataType = DBTYPE_STR;
				trData.m_ulCharMaxLength = nWidth;
				trData.m_ulCharOctetLength = nWidth;
				break;
			case FTDouble:
				trData.m_nDataType = DBTYPE_R8;
				trData.m_nNumericPrecision = nDecimals;
				break;
			case FTInteger:
				trData.m_nDataType = DBTYPE_I4;
				break;
			};

			lstrcpyW(trData.m_szTableName,A2OLE("DBF"));
			lstrcpyW(trData.m_szColumnName,A2OLE(szFieldName));
			trData.m_ulOrdinalPosition = i+1;


			if (!m_rgRowData.Add(trData))
			{
				DBFClose(hDBF);
				return E_OUTOFMEMORY;
			}
		}
		
		CCOLUMNSRow trData;
		
		lstrcpyW(trData.m_szTableName,A2OLE("DBF"));
		lstrcpyW(trData.m_szColumnName,A2OLE("OGIS_GEOMETRY"));
		trData.m_ulOrdinalPosition = i+1;
		trData.m_nDataType = DBTYPE_IUNKNOWN;
		if (!m_rgRowData.Add(trData))
		{
			DBFClose(hDBF);
			return E_OUTOFMEMORY;
		}
		
		
		DBFClose(hDBF);
		return S_OK;
	}
};

/////////////////////////////////////////////////////////////////////////////
// CSFSessionPTSchemaRowset
class CSFSessionPTSchemaRowset : 
	public CRowsetImpl< CSFSessionPTSchemaRowset, CPROVIDER_TYPERow, CSFSession>
{
public:
	HRESULT Execute(LONG* pcRowsAffected, ULONG, const VARIANT*)
	{
		USES_CONVERSION;

		CPROVIDER_TYPERow trDataI,trDataR,trDataS,trDataBlob;
		
		lstrcpyW(trDataI.m_szName,A2OLE("Integer"));
		trDataI.m_nType  = DBTYPE_I4;
		m_rgRowData.Add(trDataI);

		lstrcpyW(trDataR.m_szName,A2OLE("Real"));
		trDataR.m_nType = DBTYPE_R8;
		m_rgRowData.Add(trDataR);

		lstrcpyW(trDataS.m_szName,A2OLE("String"));
		trDataS.m_nType = DBTYPE_STR;
		trDataS.m_ulSize = 256;
		trDataS.m_bUnsignedAttribute = NULL;
		m_rgRowData.Add(trDataS);

		lstrcpyW(trDataBlob.m_szName,A2OLE("Geometry"));
		trDataS.m_nType = DBTYPE_BYTES;
		m_rgRowData.Add(trDataBlob);

		return S_OK;
	}
};

/////////////////////////////////////////////////////////////////////////////
// CSFSessionSchemaOGISTables 
class OGISTables_Row
{
public:
	WCHAR	m_szAlias[4];
	WCHAR	m_szCatalog[4];
	WCHAR	m_szSchema[4];
	WCHAR	m_szTableName[129];
	WCHAR	m_szColumnName[129];
	WCHAR	m_szDGName[129];

	OGISTables_Row()
	{
		m_szAlias[0] = NULL;
		m_szCatalog[0] = NULL;
		m_szSchema[0] = NULL;
		m_szTableName[0] = NULL;
		m_szColumnName[0] = NULL;
		m_szDGName[0] = NULL;
	}

BEGIN_PROVIDER_COLUMN_MAP(OGISTables_Row)
	PROVIDER_COLUMN_ENTRY("FEATURE_TABLE_ALIAS",1,m_szAlias)
	PROVIDER_COLUMN_ENTRY("TABLE_CATALOG",2,m_szCatalog)
	PROVIDER_COLUMN_ENTRY("TABLE_SCHEMA",3,m_szSchema)
	PROVIDER_COLUMN_ENTRY("TABLE_NAME",4,m_szTableName)
	PROVIDER_COLUMN_ENTRY("ID_COLUMN_NAME",5,m_szColumnName)
	PROVIDER_COLUMN_ENTRY("DG_COLUMN_NAME",6,m_szDGName)
END_PROVIDER_COLUMN_MAP()
};

class CSFSessionSchemaOGISTables:
	public CRowsetImpl <CSFSessionSchemaOGISTables,OGISTables_Row, CSFSession>
{
public:
	HRESULT Execute(LONG* pcRowsAffected, ULONG, const VARIANT*)
	{
		USES_CONVERSION;
		DBFHandle		hDBF;
		OGISTables_Row	trData;
		char			szFieldName[12];
		IUnknown		*pIU;

		QueryInterface(IID_IUnknown,(void **) &pIU);
		hDBF = GetDBFHandle(pIU);
		pIU->Release();

		if (!hDBF)
			return E_FAIL;
		lstrcpyW(trData.m_szTableName,A2OLE("DBF"));
		lstrcpyW(trData.m_szDGName,A2OLE("OGIS_GEOMETRY"));
		DBFGetFieldInfo(hDBF,0,szFieldName,NULL,NULL);
		lstrcpyW(trData.m_szColumnName,A2OLE(szFieldName));

		m_rgRowData.Add(trData);

		DBFClose(hDBF);

		return S_OK;
	}
};

/////////////////////////////////////////////////////////////////////////////
// CSFSessionSchemaOGISGeoColumns

class OGISGeometry_Row
{
public:
	WCHAR	m_szCatalog[4];
	WCHAR	m_szSchema[4];
	WCHAR	m_szTableName[129];
	WCHAR	m_szColumnName[129];
	unsigned int m_nGeomType;
	int		m_nSpatialRefId;
	
	OGISGeometry_Row()
	{
		m_szCatalog[0] = NULL;
		m_szSchema[0] = NULL;
		m_szTableName[0] = NULL;
		m_szColumnName[0] = NULL;
		m_nGeomType = 0;
		m_nSpatialRefId = 0;
	}

BEGIN_PROVIDER_COLUMN_MAP(OGISGeometry_Row)
	PROVIDER_COLUMN_ENTRY("TABLE_CATALOG",1,m_szCatalog)
	PROVIDER_COLUMN_ENTRY("TABLE_SCHEMA",2,m_szSchema)
	PROVIDER_COLUMN_ENTRY("TABLE_NAME",3,m_szTableName)
	PROVIDER_COLUMN_ENTRY("COLUMN_NAME",4,m_szColumnName)
	PROVIDER_COLUMN_ENTRY("GEOM_TYPE",5,m_nGeomType)
	PROVIDER_COLUMN_ENTRY("SPATIAL_REF_SYSTEM_ID",6,m_nSpatialRefId)
END_PROVIDER_COLUMN_MAP()
};

class CSFSessionSchemaOGISGeoColumns:
public CRowsetImpl<CSFSessionSchemaOGISGeoColumns,OGISGeometry_Row,CSFSession>
{
public:
	HRESULT Execute(LONG* pcRowsAffected, ULONG, const VARIANT*)
	{
		USES_CONVERSION;
		DBFHandle		hDBF;
		OGISGeometry_Row trData;

		lstrcpyW(trData.m_szTableName,A2OLE("DBF"));
		lstrcpyW(trData.m_szColumnName,A2OLE("OGIS_GEOMETRY"));

		m_rgRowData.Add(trData);

		return S_OK;
	}
};


/////////////////////////////////////////////////////////////////////////////
// CSFSessionSchemaSpatRef

class OGISSpat_Row
{
public:
	int		m_nSpatialRefId;
	WCHAR	m_szAuthorityName[129];
	WCHAR	m_nAuthorityId;
	WCHAR	m_pszSpatialRefSystem[512];

	OGISSpat_Row()
	{
		m_nSpatialRefId = 0;
		m_szAuthorityName[0] = NULL;
		m_nAuthorityId = 0;
		m_pszSpatialRefSystem[0] = NULL;
	}

BEGIN_PROVIDER_COLUMN_MAP(OGISSpat_Row)
	PROVIDER_COLUMN_ENTRY("SPATIAL_REF_SYSTEM_ID",1,m_nSpatialRefId)
	PROVIDER_COLUMN_ENTRY("AUTHORITY_NAME",2,m_szAuthorityName)
	PROVIDER_COLUMN_ENTRY("AUTHORITY_ID",3,m_nAuthorityId)
	PROVIDER_COLUMN_ENTRY_WSTR("SPATIAL_REF_SYSTEM_WKT",4,m_pszSpatialRefSystem)
END_PROVIDER_COLUMN_MAP()
};


class CSFSessionSchemaSpatRef:
public CRowsetImpl<CSFSessionSchemaSpatRef,OGISSpat_Row,CSFSession>
{
public:
	HRESULT Execute(LONG* pcRowsAffected, ULONG, const VARIANT*)
	{
		USES_CONVERSION;
		DBFHandle		hDBF;
		OGISSpat_Row trData;

		trData.m_nAuthorityId = 1;
		trData.m_nSpatialRefId = 2;
		lstrcpyW(trData.m_szAuthorityName,A2OLE("USGS"));
	
		m_rgRowData.Add(trData);
		return S_OK;
	}
};

#endif //__CSFSession_H_
