/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Utility functions.
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
 * Revision 1.12  2001/05/28 19:39:29  warmerda
 * added SFWkbGeomTypeToDBGEOM
 *
 * Revision 1.11  2001/04/30 18:57:22  warmerda
 * use CPLDebug
 *
 * Revision 1.10  2000/07/19 20:50:35  warmerda
 * better debug file location
 *
 * Revision 1.9  1999/11/25 04:09:57  kshih
 * Added code to delete OGR dataset when no longer in use.
 *
 * Revision 1.8  1999/11/22 17:17:05  warmerda
 * removed debug statements
 *
 * Revision 1.7  1999/11/22 15:21:37  warmerda
 * reformatted a bit
 *
 * Revision 1.6  1999/07/23 19:20:27  kshih
 * Modifications for errors etc...
 *
 * Revision 1.5  1999/07/20 17:09:57  kshih
 * Use OGR code.
 *
 * Revision 1.4  1999/06/25 18:17:25  kshih
 * Changes to get datasource from session/rowset/command
 *
 * Revision 1.3  1999/06/22 16:59:55  kshih
 * Modified GetFilenames to return previous filename if NULL is provided.
 *
 * Revision 1.2  1999/06/22 16:17:11  warmerda
 * added ogrcomdebug
 *
 * Revision 1.1  1999/06/22 15:53:54  kshih
 * Utility functions.
 */

#include "cpl_conv.h"
#include "sftraceback.h"
#include "sfutil.h"
#include "SF.h"
#include "cpl_error.h"
#include "oledbgis.h"

typedef struct _IUnknownOGRInfo
{
  IDBProperties        *pIDB;
  OGRDataSource        *pOGR;
  void		       *pKey;
  struct _IUnknownOGRInfo	*next;
}  IUnknownOGRInfo;

static IUnknownOGRInfo *pIUnkOGRInfo = NULL;

/************************************************************************/
/*                      SFGetOGRDataSource()                            */
/*                                                                      */
/*      Get a OGRData Source from a IUnknown Pointer of some sort	*/
/************************************************************************/

OGRDataSource *SFGetOGRDataSource(IUnknown *pUnk)

{
	IDBProperties	*pIDB  = NULL;
	IUnknownOGRInfo	*pInfo = pIUnkOGRInfo;
	OGRDataSource   *pOGR  = NULL;

	if (pInfo && pUnk)
	{
		pIDB = SFGetDataSourceProperties(pUnk);
	}

	while (pInfo && pIDB)
	{
		if (pInfo->pIDB == pIDB)
		{
			pOGR = pInfo->pOGR;
			break;
		}

		pInfo = pInfo->next;
	}
	
	if (pIDB)
		pIDB->Release();

	return pOGR;
}

/************************************************************************/
/*                          SFSetOGRDataSource()                        */
/*                                                                      */
/*     Set the OGRData Source from an IUnknown pointer.			*/
/************************************************************************/
void SFSetOGRDataSource(IUnknown *pUnk, OGRDataSource *pOGR, void *pKey)
{
	IUnknownOGRInfo	*pNew;
	IDBProperties	*pIDB;

	
	if (NULL == (pIDB = SFGetDataSourceProperties(pUnk)))
		return;

	if (pIDB)
	{
		pNew = new IUnknownOGRInfo;
		
		pNew->pIDB = pIDB;
		pNew->pOGR = pOGR;
		pNew->pKey = pKey;
		pNew->next = pIUnkOGRInfo;
		pIUnkOGRInfo = pNew;
		
		pIDB->Release();
	}
}

/************************************************************************/
/*                        SFClearOGRDataSource()                        */
/*                                                                      */
/*      Set the OGRData Source from an IUnknown pointer.                */
/************************************************************************/
void SFClearOGRDataSource(void *pKey)
{

	IUnknownOGRInfo *pInfo  =  pIUnkOGRInfo;
	IUnknownOGRInfo **pPrev = &pIUnkOGRInfo;

	while (pInfo && pKey)
	{
		if (pInfo->pKey == pKey)
		{
			*pPrev = pInfo->next;		
			delete pInfo->pOGR;
			delete pInfo;
			break;
		}
		pPrev = &(pInfo->next);
		pInfo = pInfo->next;
	}
}

/************************************************************************/
/*                        SFGetInitDataSource()                         */
/*                                                                      */
/*      Get the Data Source Filename from a session/rowset/command      */
/*      IUnknown pointer.  The interface passed in is freed             */
/*      automatically.  The returned name should be freed with free     */
/*      when done.                                                      */
/************************************************************************/

char *SFGetInitDataSource(IUnknown *pIUnknownIn)
{
    IDBProperties	*pIDBProp;
    char			*pszDataSource = NULL;

    if (pIUnknownIn == NULL)
        return NULL;

    pIDBProp = SFGetDataSourceProperties(pIUnknownIn);
	
    if (pIDBProp)
    {
        DBPROPIDSET sPropIdSets[1];
        DBPROPID	rgPropIds[1];
		
        ULONG		nPropSets;
        DBPROPSET	*rgPropSets;
		
        rgPropIds[0] = DBPROP_INIT_DATASOURCE;
		
        sPropIdSets[0].cPropertyIDs = 1;
        sPropIdSets[0].guidPropertySet = DBPROPSET_DBINIT;
        sPropIdSets[0].rgPropertyIDs = rgPropIds;
		
        pIDBProp->GetProperties(1,sPropIdSets,&nPropSets,&rgPropSets);
		
        if (rgPropSets)
        {
            USES_CONVERSION;
            char *pszSource = (char *) 
                OLE2A(rgPropSets[0].rgProperties[0].vValue.bstrVal);
            pszDataSource = (char *) malloc(1+strlen(pszSource));
            strcpy(pszDataSource,pszSource);
        }
		
        if (rgPropSets)
        {
            int i;
            for (i=0; i < nPropSets; i++)
            {
                CoTaskMemFree(rgPropSets[i].rgProperties);
            }
            CoTaskMemFree(rgPropSets);
        }
        pIDBProp->Release();	
    }

    return pszDataSource;
}
/************************************************************************/
/*                            OGRComDebug()                             */
/************************************************************************/

void OGRComDebug( const char * pszDebugClass, const char * pszFormat, ... )

{
    va_list args;
    static FILE      *fpDebug = NULL;

/* -------------------------------------------------------------------- */
/*      Write message to stdout.                                        */
/* -------------------------------------------------------------------- */
    fprintf( stdout, "%s:", pszDebugClass );

    va_start(args, pszFormat);
    vfprintf( stdout, pszFormat, args );
    va_end(args);

    fflush( stdout );

/* -------------------------------------------------------------------- */
/*      Also route through CPL                                          */
/* -------------------------------------------------------------------- */
    char      szMessage[10000];

    va_start(args, pszFormat);
    vsprintf( szMessage, pszFormat, args );
    va_end(args);

    CPLDebug( pszDebugClass, "%s", szMessage );
}

/************************************************************************/
/*                            SFReportError()                           */
/************************************************************************/


HRESULT	SFReportError(HRESULT passed_hr, IID iid, DWORD providerCode,
                      char *pszText)
{
    static	IClassFactory *m_pErrorObjectFactory;

    if (FAILED(passed_hr))
    {
        ERRORINFO		ErrorInfo;
        IErrorInfo		*pErrorInfo;
        IErrorRecords	*pErrorRecords;
        HRESULT			hr;

        CPLDebug( "OGR_OLEDB", "SFReportError(%d,%d,%s)\n", 
                  passed_hr, providerCode, pszText );

        SetErrorInfo(0, NULL);
		
        GetErrorInfo(0,&pErrorInfo);
		
        if (!pErrorInfo)
        {
            if (!m_pErrorObjectFactory)
            {
                CoGetClassObject(CLSID_EXTENDEDERRORINFO,
                                 CLSCTX_INPROC_SERVER,
                                 NULL	,
                                 IID_IClassFactory,
                                 (LPVOID *) &m_pErrorObjectFactory);
            }
			
            hr = m_pErrorObjectFactory->CreateInstance(NULL, IID_IErrorInfo,
                                                       (void**) &pErrorInfo);
        }

        hr = pErrorInfo->QueryInterface(IID_IErrorRecords, 
                                        (void **) &pErrorRecords);
		
        VARIANTARG  varg;
        VariantInit (&varg); 
        DISPPARAMS  dispparams = {&varg, NULL, 1, 0};
        varg.vt = VT_BSTR; 
        varg.bstrVal = SysAllocString(A2BSTR(pszText));
        // Fill in the ERRORINFO structure and add the error record.
		
        ErrorInfo.hrError = passed_hr; 
        ErrorInfo.dwMinor = providerCode;
        ErrorInfo.clsid   = CLSID_SF;
        ErrorInfo.iid     = iid;

        hr = pErrorRecords->AddErrorRecord(&ErrorInfo,ErrorInfo.dwMinor,
                                           &dispparams,NULL,0);
        VariantClear(&varg);
        // Call SetErrorInfo to pass the error object to the Automation DLL.
        hr = SetErrorInfo(0, pErrorInfo);
        // Release the interface pointers on the object to finish transferring ownership of
        // the object to the Automation DLL. pErrorRecords->Release();
        pErrorInfo->Release();

    }
    return passed_hr;
}

/************************************************************************/
/*                       SFWkbGeomTypeToDBGEOM()                        */
/************************************************************************/

int             SFWkbGeomTypeToDBGEOM( OGRwkbGeometryType in )

{
    switch( in )
    {
        case wkbPoint:
        case wkbPoint25D:
            return  DBGEOM_POINT;
                        
        case wkbLineString:
        case wkbLineString25D:
            return DBGEOM_LINESTRING;
            break;
                        
        case wkbPolygon:
        case wkbPolygon25D:
            return DBGEOM_POLYGON;
                        
        case wkbMultiPoint:
            return DBGEOM_MULTIPOINT;
                        
        case wkbMultiLineString:
            return DBGEOM_MULTILINESTRING;
                        
        case wkbMultiPolygon:
            return DBGEOM_MULTIPOLYGON;
                        
        case wkbGeometryCollection:
            return DBGEOM_COLLECTION;
            break;
                        
        case wkbUnknown:
        case wkbNone:
        default:
            return DBGEOM_GEOMETRY;
    }

    return DBGEOM_GEOMETRY;
}

