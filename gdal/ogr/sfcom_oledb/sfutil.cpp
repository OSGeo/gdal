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
 ****************************************************************************/

#include "cpl_conv.h"
#include "sftraceback.h"
#include "sfutil.h"
#include "SF.h"
#include "SFSess.h"
#include "SFDS.h"


#include "cpl_error.h"
#include "cpl_string.h"
#include "oledbgis.h"

/* our custom error info seems to work ok, and the old method doesn't
   so for now it is always on.
*/
#ifndef SUPPORT_CUSTOM_IERRORINFO
#define SUPPORT_CUSTOM_IERRORINFO
#endif

/************************************************************************/
/* ==================================================================== */
/*                              SFIError                                */
/*                                                                      */
/*     Simple implementation of the IError interface.                   */
/* ==================================================================== */
/************************************************************************/
#ifdef SUPPORT_CUSTOM_IERRORINFO
class SFIError : public IErrorInfo
{
public:
    // CTOR/DTOR
    SFIError( const char *pszErrorIn )
    {
            CPLDebug( "OGR_OLEDB", "SFIError(%s)", pszErrorIn );
            m_cRef   = 1;
            m_pszError = CPLStrdup(pszErrorIn);
    }

    ~SFIError()
    {
            CPLDebug( "OGR_OLEDB", "~SFIError(%s)", m_pszError );
            if( m_pszError != NULL )
                CPLFree( m_pszError );
    }

    // IUNKOWN
    HRESULT STDMETHODCALLTYPE    QueryInterface (REFIID riid, void **ppv) 
        {
            if (riid == IID_IUnknown||
                riid == IID_IErrorInfo ||
				riid == IID_IErrorRecords)
            {
                *ppv = (IErrorInfo *) this;        
                AddRef();
                return NOERROR;
            }
            else
            {
                *ppv = 0;
                return E_NOINTERFACE;
            }
    };

    ULONG STDMETHODCALLTYPE        AddRef (void) 
        {
            return ++m_cRef;
        };
    ULONG STDMETHODCALLTYPE        Release (void )
    {
            if (--m_cRef ==0)
            {
                delete this;
                return 0;
            }
            return m_cRef;
    };

    HRESULT STDMETHODCALLTYPE GetGUID( 
            /* [out] */ GUID *pGUID )
        {
            //*pGUID = DB_NULLGUID;                   
            return S_OK;
        };

    HRESULT STDMETHODCALLTYPE GetSource( 
            /* [out] */ BSTR __RPC_FAR *pBstrSource)
        {
            *pBstrSource = SysAllocString(A2BSTR("OLE DB Provider"));
            return S_OK;
        };
        
    HRESULT STDMETHODCALLTYPE GetDescription( 
            /* [out] */ BSTR __RPC_FAR *pBstrDescription)
        {
            *pBstrDescription = SysAllocString(A2BSTR(m_pszError));
            return S_OK;
        };
        
    HRESULT STDMETHODCALLTYPE GetHelpFile( 
            /* [out] */ BSTR __RPC_FAR *pBstrHelpFile)
        {
            *pBstrHelpFile = NULL;
            return S_OK;
        };
        
    HRESULT STDMETHODCALLTYPE GetHelpContext( 
        /* [out] */ DWORD __RPC_FAR *pdwHelpContext)
        {
            *pdwHelpContext = 0;
            return S_OK;
        };

    // Data Members

    int        m_cRef;
    char       *m_pszError;
};
#endif /* def SUPPORT_CUSTOM_IERRORINFO */


/************************************************************************/
/*                      SFGetOGRDataSource()                            */
/*                                                                      */
/*      Get a OGRData Source from a IUnknown Pointer of some sort       */
/************************************************************************/

OGRDataSource *SFGetOGRDataSource(IUnknown *pUnk)

{
    IDBProperties    *pIDB  = NULL;
    OGRDataSource   *pOGR  = NULL;

    if (pUnk)
        pIDB = SFGetDataSourceProperties(pUnk);
    else
        CPLDebug( "OLEDB", "SFGetOGRDataSource, pUnk == NULL." );

    if (pIDB)
    {
        IDataSourceKey* pIKey = NULL;
        HRESULT hr = pIDB->QueryInterface(IID_IDataSourceKey, (void**) &pIKey);
        if (SUCCEEDED(hr) && pIKey != NULL)
        {
            ULONG ulVal = 0;
            if (SUCCEEDED(pIKey->GetKey(&ulVal)) && ulVal != 0)
            {
                CSFSource *poCSFSource = (CSFSource *) (ulVal);
                pOGR = poCSFSource->GetDataSource();
            }
            else
            {
                CPLDebug( "OLEDB", 
                   "SFGetOGRDatasource(), GetKey failed, or returned NULL." );
            }

            pIKey->Release();
        }

        pIDB->Release();
    }
    else
        CPLDebug( "OLEDB", "SFGetOGRDataSource, pIDB == NULL." );

    return pOGR;
}

/************************************************************************/
/*                           SFGetCSFSource()                           */
/*                                                                      */
/*      Get the CSFSource from an IUknown pointer that is somehow       */
/*      related.  Much like SFGetOGRDataSource(), the IUnknown will     */
/*      be dereferenced by this call.                                   */
/************************************************************************/

CSFSource *SFGetCSFSource(IUnknown *pUnk)

{
    IDBProperties    *pIDB  = NULL;
    CSFSource        *poCSFSource = NULL;

    if (pUnk)
        pIDB = SFGetDataSourceProperties(pUnk);
    else
        CPLDebug( "OLEDB", "SFGetCSFSource, pUnk == NULL." );

    if (pIDB)
    {
        IDataSourceKey* pIKey = NULL;
        HRESULT hr = pIDB->QueryInterface(IID_IDataSourceKey, 
                                          (void**) &pIKey);
        if (SUCCEEDED(hr) && pIKey != NULL)
        {
            if( !SUCCEEDED(pIKey->GetKey((ULONG *) &poCSFSource) )
                           || poCSFSource == NULL )
            {
                CPLDebug( "OLEDB", 
                   "SFGetCSFSource(), GetKey failed, or returned NULL." );
            }

            pIKey->Release();
        }

        pIDB->Release();
    }
    else
        CPLDebug( "OLEDB", "SFGetCSFSource, pIDB == NULL." );

    return poCSFSource;
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
    IDBProperties    *pIDBProp;
    char            *pszDataSource = NULL;

    if (pIUnknownIn == NULL)
        return NULL;

    pIDBProp = SFGetDataSourceProperties(pIUnknownIn);
    
    if (pIDBProp)
    {
        DBPROPIDSET sPropIdSets[1];
        DBPROPID    rgPropIds[1];
        
        ULONG        nPropSets;
        DBPROPSET    *rgPropSets;
        
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
            for (i=0; i < (int) nPropSets; i++)
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
/*                        SFGetProviderOptions()                        */
/*                                                                      */
/*      Get the set of provider options in effect from the provider     */
/*      string.  Returned as a CPL name/value string list.              */
/************************************************************************/

char **SFGetProviderOptions(IUnknown *pIUnknownIn)
{
    IDBProperties    *pIDBProp;
    char                **papszResult = NULL;

    if (pIUnknownIn == NULL)
        return NULL;

    pIDBProp = SFGetDataSourceProperties(pIUnknownIn);
    
    if (pIDBProp == NULL)
    {
        CPLDebug( "OGR_OLEDB", "SFGetProviderOptions(%p) - pIDBProp == NULL",
                  pIUnknownIn );
        return NULL;
    }

    DBPROPIDSET sPropIdSets[1];
    DBPROPID    rgPropIds[1];
        
    ULONG        nPropSets;
    DBPROPSET    *rgPropSets;
        
    rgPropIds[0] = DBPROP_INIT_PROVIDERSTRING;
        
    sPropIdSets[0].cPropertyIDs = 1;
    sPropIdSets[0].guidPropertySet = DBPROPSET_DBINIT;
    sPropIdSets[0].rgPropertyIDs = rgPropIds;
        
    pIDBProp->GetProperties(1,sPropIdSets,&nPropSets,&rgPropSets);
        
    if (rgPropSets)
    {
        USES_CONVERSION;
        char *pszProviderString = (char *) 
            OLE2A(rgPropSets[0].rgProperties[0].vValue.bstrVal);

        CPLDebug( "OLEDB", "ProviderString[%s]", pszProviderString );

        papszResult = CSLTokenizeStringComplex( pszProviderString, 
                                                ";", TRUE, FALSE );
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

    return papszResult;
}

/************************************************************************/
/*                           SFGetLayerWKT()                            */
/*                                                                      */
/*      Fetch the WKT coordinate system associated with a layer,        */
/*      after passing through the appropriate SRS_PROFILE for the       */
/*      provider instance.  The returned string should be freed by      */
/*      the caller.  The passed in IUnknown reference is released       */
/*      internally.                                                     */
/************************************************************************/

char *SFGetLayerWKT( OGRLayer *poLayer, IUnknown *pIUnknown )

{
    char      **papszOptions;
    char      *pszWKT = NULL;
    OGRSpatialReference *poSRS;
    const char  *pszSrsProfile;

    if( poLayer->GetSpatialRef() == NULL )
    {
        pIUnknown->Release();
        return NULL;
    }

    papszOptions = SFGetProviderOptions(pIUnknown);

    poSRS = poLayer->GetSpatialRef()->Clone();

    pszSrsProfile = CSLFetchNameValue( papszOptions, "SRS_PROFILE" );
    if( pszSrsProfile != NULL && EQUAL(pszSrsProfile,"ESRI") )
    {
        poSRS->morphToESRI();
    }
    else if( pszSrsProfile != NULL
             && EQUAL(pszSrsProfile,"SF1") )
    {
        poSRS->StripCTParms();
    }
    
    poSRS->exportToWkt( &pszWKT );
    OSRDestroySpatialReference( poSRS );

    CSLDestroy( papszOptions );

    return pszWKT;
}

/************************************************************************/
/*                         SFGetSRSIDFromWKT()                          */
/*                                                                      */
/*      This method is really just masqarading access to the            */
/*      CSFSource, so that it doesn't have to be #included in places    */
/*      like IColumnsRowsetImpl.h.                                      */
/*                                                                      */
/*      Unlike some other methods, this call does not dereference       */
/*      the passed in IUnknown.                                         */
/************************************************************************/

int SFGetSRSIDFromWKT( const char *pszWKT, IUnknown *pIUnknownIn )

{
    CSFSource *poCSFSource = NULL;
    IUnknown  *pIUnknown;

    pIUnknownIn->QueryInterface(IID_IUnknown,(void **) &pIUnknown);
    poCSFSource = SFGetCSFSource( pIUnknown );
    if( poCSFSource == NULL )
    {
        CPLDebug( "OGR_OLEDB", "failed to get CSFSource from %p.",
                  pIUnknownIn );
        return -1;
    }

    return poCSFSource->GetSRSID( pszWKT );
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
/*                           CPL_ATLTrace2()                            */
/************************************************************************/

void CPL_ATLTrace2( DWORD category, UINT level, const char * format, ... )

{
    va_list args;

/* -------------------------------------------------------------------- */
/*      Also route through CPL                                          */
/* -------------------------------------------------------------------- */
    char      szMessage[10000];

    va_start(args, format);
    vsprintf( szMessage, format, args );
    va_end(args);

    CPLDebug( "ATLTrace2", "%s", szMessage );
}

#ifdef SUPPORT_CUSTOM_IERRORINFO
/************************************************************************/
/*                            SFReportError()                           */
/************************************************************************/

HRESULT    SFReportError(HRESULT passed_hr, IID iid, DWORD providerCode,
                      char *pszFmt, ...)
{
    va_list args;

    if (!FAILED(passed_hr))
        return passed_hr;

    IErrorInfo        *pErrorInfo = NULL;
    char                szErrorMsg[20000];

    /* Expand the error message 
     */
    va_start(args, pszFmt);
    vsprintf( szErrorMsg, pszFmt, args );
    va_end(args);

    CPLDebug( "OGR_OLEDB", "SFReportError(%d,%d,%s)\n", 
              passed_hr, providerCode, szErrorMsg );
/*
    SetErrorInfo(0, NULL);

    pErrorInfo = new SFIError( szErrorMsg );

    // Call SetErrorInfo to pass the error object to the Automation DLL.
    SetErrorInfo(0, pErrorInfo);

    pErrorInfo->Release();
*/
    return passed_hr;
}
#else /* notdef SUPPORT_CUSTOM_IERRORINFO */
/************************************************************************/
/*                            SFReportError()                           */
/************************************************************************/

HRESULT    SFReportError(HRESULT passed_hr, IID iid, DWORD providerCode,
                      char *pszText)
{
    static      IClassFactory *m_pErrorObjectFactory = NULL;

    if (FAILED(passed_hr))
    {
        ERRORINFO        ErrorInfo;
        IErrorInfo        *pErrorInfo;
        IErrorRecords    *pErrorRecords;
        HRESULT            hr;

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
                                 NULL    ,
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
        ErrorInfo.dispid  = 0;

        hr = pErrorRecords->AddErrorRecord(&ErrorInfo,ErrorInfo.dwMinor,
                                           &dispparams,NULL,0);
        VariantClear(&varg);
        // Call SetErrorInfo to pass the error object to the Automation DLL.
        hr = SetErrorInfo(0, pErrorInfo);
        // Release the interface pointers on the object to finish transferring ownership of
        // the object to the Automation DLL. pErrorRecords->Release();
        pErrorInfo->Release();

/* -------------------------------------------------------------------- */
/*      For debugging purposes, lets try and verify that we can get     */
/*      the error information back out now in a manner similar to       */
/*      what RowsetViewer does.                                         */
/* -------------------------------------------------------------------- */
#ifdef notdef
        pErrorInfo = NULL;
        pErrorRecords = NULL;
        ULONG cErrorRecords = 0;
        
        CComBSTR cstrDescription;
        CComBSTR cstrSource;
        CComBSTR cstrSQLInfo;
        INT iResult = 0;
        static LCID lcid = GetSystemDefaultLCID(); 
        
        if((hr = GetErrorInfo(0, &pErrorInfo))==S_OK && pErrorInfo)
        {
            //The Error Object may support multiple Errors (IErrorRecords)
            if(SUCCEEDED(hr = pErrorInfo->QueryInterface(&pErrorRecords)))
            {
                //Multiple Errors
                hr = pErrorRecords->GetRecordCount(&cErrorRecords);
            }
            else
            {
                //Only a single Error Object
                cErrorRecords = 1;
            }

            //Get the Description
            hr = pErrorInfo->GetDescription(&cstrDescription);
            
            //Get the Source - this will be the window title...
            hr = pErrorInfo->GetSource(&cstrSource);
            
            ERRORINFO ErrorInfo = { passed_hr, 0 };

            //Loop through the records
            for(ULONG i=0; i<cErrorRecords; i++)
            {
                //ErrorRecords
                if(pErrorRecords)
                {
                    pErrorInfo->Release();
                    hr = pErrorRecords->GetErrorInfo(i, lcid, &pErrorInfo);

                    //Get the Basic ErrorInfo
                    hr = pErrorRecords->GetBasicErrorInfo(i, &ErrorInfo);
                }
                else
                {
                    //ErrorInfo is only available...
                    hr = pErrorInfo->GetGUID(&ErrorInfo.iid);
                }

                //Get the Description
                hr = pErrorInfo->GetDescription(&cstrDescription);
                                
                //Get the Source - this will be the window title...
                hr = pErrorInfo->GetSource(&cstrSource);
            }
        }
#endif
    }
    return passed_hr;
}
#endif

/************************************************************************/
/*                       SFWkbGeomTypeToDBGEOM()                        */
/************************************************************************/

int             SFWkbGeomTypeToDBGEOM( OGRwkbGeometryType in )

{
    switch( wkbFlatten(in) )
    {
        case wkbPoint:
            return  DBGEOM_POINT;
                        
        case wkbLineString:
            return DBGEOM_LINESTRING;
                        
        case wkbPolygon:
            return DBGEOM_POLYGON;
                        
        case wkbMultiPoint:
            return DBGEOM_MULTIPOINT;
                        
        case wkbMultiLineString:
            return DBGEOM_MULTILINESTRING;
                        
        case wkbMultiPolygon:
            return DBGEOM_MULTIPOLYGON;
                        
        case wkbGeometryCollection:
            return DBGEOM_COLLECTION;
                        
        case wkbUnknown:
        case wkbNone:
        default:
            return DBGEOM_GEOMETRY;
    }

    return DBGEOM_GEOMETRY;
}
