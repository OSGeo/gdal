/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implement methods of CSFSource, the core implementation object
 *           represending a database instances / OGRDataset.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
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

#include "stdafx.h"
#include <assert.h>
#include "SF.h"
#include "SFDS.h"

#include "cpl_string.h"

/************************************************************************/
/*                             CSFSource()                              */
/************************************************************************/
CSFSource::CSFSource()
{
    CPLDebug( "OGR_OLEDB", "CSFSource(): %p", this );
    m_poDS = NULL;

    m_bSRSListInitialized = FALSE;
    m_nSRSCount = 0;
    m_papszSRSList = NULL;

}

/************************************************************************/
/*                             ~CSFSource()                             */
/************************************************************************/

CSFSource::~CSFSource()
{
    CPLDebug( "OGR_OLEDB", "~CSFSource(): %p", this );
    if( m_poDS != NULL )
    {
        SFDSCacheReleaseDataSource( m_poDS );
        m_poDS = NULL;
    }

    CSLDestroy( m_papszSRSList );
    
//    DumpProperties();
}

/************************************************************************/
/*                           OpenDataSource()                           */
/************************************************************************/

HRESULT CSFSource::OpenDataSource()

{
    HRESULT hr = S_OK;
    char *pszDataSource;
    IUnknown *pIU;

    if( m_poDS != NULL )
    {
        CPLDebug( "OGR_OLEDB", 
                  "** m_poDS is not NULL in CSFSource::OpenDataSource() **" );
    }

/* -------------------------------------------------------------------- */
/*      Fetch the datasource name from the properties list.             */
/* -------------------------------------------------------------------- */
    QueryInterface(IID_IUnknown, (void **) &pIU);
    pszDataSource = SFGetInitDataSource(pIU);

/* -------------------------------------------------------------------- */
/*      Open (with possible caching) the data source.                   */
/* -------------------------------------------------------------------- */
    m_poDS = SFDSCacheOpenDataSource( pszDataSource );

    CPLDebug( "OGR_OLEDB", "CSFSource::OpenDataSource(%s) = %p",
              pszDataSource, m_poDS );

    if( m_poDS == NULL )
        hr = E_FAIL;
    
    free(pszDataSource);

    return hr;
}

/************************************************************************/
/*                            InitSRSList()                             */
/************************************************************************/

void CSFSource::InitSRSList()

{
    IUnknown *pIU = NULL;

    if( m_bSRSListInitialized )
        return;

    m_bSRSListInitialized = TRUE;

    for (int iLayer = 0; iLayer < m_poDS->GetLayerCount(); iLayer++)
    {
        char *pszWKT = NULL;
        OGRLayer *pLayer = m_poDS->GetLayer(iLayer);

        QueryInterface(IID_IUnknown,(void **) &pIU);
        pszWKT = SFGetLayerWKT( pLayer, pIU );

        if( pszWKT != NULL && strlen(pszWKT) > 0 )
        {
            if( CSLFindString( m_papszSRSList, pszWKT ) == -1 )
                m_papszSRSList = CSLAddString( m_papszSRSList, pszWKT );
        }

        OGRFree( pszWKT );
    }

    m_papszSRSList = CSLAddString( m_papszSRSList, "" );

    m_nSRSCount = CSLCount( m_papszSRSList );
}

/************************************************************************/
/*                            GetSRSCount()                             */
/************************************************************************/

int CSFSource::GetSRSCount()

{
    InitSRSList();

    return m_nSRSCount;
}

/************************************************************************/
/*                             GetSRSWKT()                              */
/************************************************************************/

const char *CSFSource::GetSRSWKT( int iSRS )

{
    InitSRSList();

    if( iSRS < 0 || iSRS >= m_nSRSCount )
        return "";
    else
        return m_papszSRSList[iSRS];
}

/************************************************************************/
/*                              GetSRSID()                              */
/************************************************************************/

int CSFSource::GetSRSID( const char *pszWKT )

{
    int  nSRSID;

    InitSRSList();

    nSRSID = CSLFindString( m_papszSRSList, pszWKT );
    if( nSRSID == -1 )
        nSRSID = CSLFindString( m_papszSRSList, "" );

    return nSRSID;
}

/************************************************************************/
/*                           FinalConstruct()                           */
/************************************************************************/
HRESULT CSFSource::FinalConstruct()
{
    HRESULT hr;

    CPLDebug( "OGR_OLEDB", "FinalConstruct() -> FInit()" );

    hr = FInit();

//    DumpProperties();

    return hr;
}

/************************************************************************/
/*                           DumpProperties()                           */
/************************************************************************/

void CSFSource::DumpProperties()

{
    CUtlProps<CSFSource> *pUtlProps = static_cast<CUtlProps<CSFSource> *>(this);

    CPLDebug( "OGR_OLEDB", "pUtlProps = %p", pUtlProps );

#ifdef SUPPORT_ATL_NET
    CPLDebug( "OGR_OLEDB", 
              "m_pUProp = %p,"
              "m_cUPropSet = %d,"
              "sizeof(UPROPVAL) = %d, sizeof(ATL::UPROPVAL) = %d"
              ,
              pUtlProps->m_pUProp,
              pUtlProps->m_cUPropSet,
              sizeof(UPROPVAL), sizeof(ATL::UPROPVAL) );
#endif

    for( unsigned int ulPropSet = 0; 
         ulPropSet < pUtlProps->m_cUPropSet; ulPropSet++ )
    {
        CPLDebug( "OGR_OLEDB", "Property Set %d", ulPropSet );
            
        UPROPVAL* pUPropVal = pUtlProps->m_pUProp[ulPropSet].pUPropVal;
        for(ULONG ulPropId=0; 
            ulPropId < pUtlProps->m_pUProp[ulPropSet].cPropIds; 
            ulPropId++)
        {
            UPROPVAL *pThisProp = pUPropVal + ulPropId;

            CPLDebug( "OGR_OLEDB", "[%d]pUPropVal[%d].pCColumnIds = %p", 
                      ulPropSet, ulPropId, pThisProp->pCColumnIds );
        }
    }
}
