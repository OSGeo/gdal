/******************************************************************************
 * $Id$
 *
 * Project:  GDAL Core
 * Purpose:  Implementation of GDALMultiDomainMetadata class.  This class
 *           manages metadata items for a variable list of domains. 
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2005, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2009-2011, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "gdal_pam.h"
#include "cpl_string.h"
#include <map>

CPL_CVSID("$Id$");

/************************************************************************/
/*                      GDALMultiDomainMetadata()                       */
/************************************************************************/

GDALMultiDomainMetadata::GDALMultiDomainMetadata()

{
    papszDomainList = NULL;
    papoMetadataLists = NULL;
}

/************************************************************************/
/*                      ~GDALMultiDomainMetadata()                      */
/************************************************************************/

GDALMultiDomainMetadata::~GDALMultiDomainMetadata()

{
    Clear();
}

/************************************************************************/
/*                               Clear()                                */
/************************************************************************/

void GDALMultiDomainMetadata::Clear()

{
    int i, nDomainCount;

    nDomainCount = CSLCount( papszDomainList );
    CSLDestroy( papszDomainList );
    papszDomainList = NULL;

    for( i = 0; i < nDomainCount; i++ )
    {
        delete papoMetadataLists[i];
    }
    CPLFree( papoMetadataLists );
    papoMetadataLists = NULL;
}


/************************************************************************/
/*                            GetMetadata()                             */
/************************************************************************/

char **GDALMultiDomainMetadata::GetMetadata( const char *pszDomain )

{
    if( pszDomain == NULL )
        pszDomain = "";

    int iDomain = CSLFindString( papszDomainList, pszDomain );

    if( iDomain == -1 )
        return NULL;
    else
        return papoMetadataLists[iDomain]->List();
}

/************************************************************************/
/*                            SetMetadata()                             */
/************************************************************************/

CPLErr GDALMultiDomainMetadata::SetMetadata( char **papszMetadata, 
                                             const char *pszDomain )

{
    if( pszDomain == NULL )
        pszDomain = "";

    int iDomain = CSLFindString( papszDomainList, pszDomain );

    if( iDomain == -1 )
    {
        int nDomainCount;

        papszDomainList = CSLAddString( papszDomainList, pszDomain );
        nDomainCount = CSLCount( papszDomainList );

        papoMetadataLists = (CPLStringList **) 
            CPLRealloc( papoMetadataLists, sizeof(void*)*(nDomainCount+1) );
        papoMetadataLists[nDomainCount] = NULL;
        papoMetadataLists[nDomainCount-1] = new CPLStringList();
        iDomain = nDomainCount-1;
    }

    papoMetadataLists[iDomain]->Assign( CSLDuplicate( papszMetadata ) );

    // we want to mark name/value pair domains as being sorted for fast
    // access.
    if( !EQUALN(pszDomain,"xml:",4) && !EQUAL(pszDomain, "SUBDATASETS") )
        papoMetadataLists[iDomain]->Sort();

    return CE_None;
}

/************************************************************************/
/*                          GetMetadataItem()                           */
/************************************************************************/

const char *GDALMultiDomainMetadata::GetMetadataItem( const char *pszName, 
                                                      const char *pszDomain )

{
    if( pszDomain == NULL )
        pszDomain = "";

    int iDomain = CSLFindString( papszDomainList, pszDomain );

    if( iDomain == -1 )
        return NULL;
    else
        return papoMetadataLists[iDomain]->FetchNameValue( pszName );
}

/************************************************************************/
/*                          SetMetadataItem()                           */
/************************************************************************/

CPLErr GDALMultiDomainMetadata::SetMetadataItem( const char *pszName,
                                                 const char *pszValue,
                                                 const char *pszDomain )

{
    if( pszDomain == NULL )
        pszDomain = "";

/* -------------------------------------------------------------------- */
/*      Create the domain if it does not already exist.                 */
/* -------------------------------------------------------------------- */
    int iDomain = CSLFindString( papszDomainList, pszDomain );

    if( iDomain == -1 )
    {
        SetMetadata( NULL, pszDomain );
        iDomain = CSLFindString( papszDomainList, pszDomain );
    }

/* -------------------------------------------------------------------- */
/*      Set the value in the domain list.                               */
/* -------------------------------------------------------------------- */
    papoMetadataLists[iDomain]->SetNameValue( pszName, pszValue );

    return CE_None;
}

/************************************************************************/
/*                              XMLInit()                               */
/*                                                                      */
/*      This method should be invoked on the parent of the              */
/*      <Metadata> elements.                                            */
/************************************************************************/

int GDALMultiDomainMetadata::XMLInit( CPLXMLNode *psTree, CPL_UNUSED int bMerge )
{
    CPLXMLNode *psMetadata;

/* ==================================================================== */
/*      Process all <Metadata> elements, each for one domain.           */
/* ==================================================================== */
    for( psMetadata = psTree->psChild; 
         psMetadata != NULL; psMetadata = psMetadata->psNext )
    {
        CPLXMLNode *psMDI;
        const char *pszDomain, *pszFormat;

        if( psMetadata->eType != CXT_Element
            || !EQUAL(psMetadata->pszValue,"Metadata") )
            continue;

        pszDomain = CPLGetXMLValue( psMetadata, "domain", "" );
        pszFormat = CPLGetXMLValue( psMetadata, "format", "" );

        // Make sure we have a CPLStringList for this domain, 
        // without wiping out an existing one.
        if( GetMetadata( pszDomain ) == NULL )  
            SetMetadata( NULL, pszDomain );

        int iDomain = CSLFindString( papszDomainList, pszDomain );
        CPLAssert( iDomain != -1 );
        
        CPLStringList *poMDList = papoMetadataLists[iDomain];

/* -------------------------------------------------------------------- */
/*      XML format subdocuments.                                        */
/* -------------------------------------------------------------------- */
        if( EQUAL(pszFormat,"xml") )
        {
            CPLXMLNode *psSubDoc;

            /* find first non-attribute child of current element */
            psSubDoc = psMetadata->psChild;
            while( psSubDoc != NULL && psSubDoc->eType == CXT_Attribute )
                psSubDoc = psSubDoc->psNext;
            
            char *pszDoc = CPLSerializeXMLTree( psSubDoc );

            poMDList->Clear();
            poMDList->AddStringDirectly( pszDoc );
        }

/* -------------------------------------------------------------------- */
/*      Name value format.                                              */
/*      <MDI key="...">value_Text</MDI>                                 */
/* -------------------------------------------------------------------- */
        else
        {
            for( psMDI = psMetadata->psChild; psMDI != NULL;
                 psMDI = psMDI->psNext )
            {
                if( !EQUAL(psMDI->pszValue,"MDI")
                    || psMDI->eType != CXT_Element
                    || psMDI->psChild == NULL
                    || psMDI->psChild->psNext == NULL
                    || psMDI->psChild->eType != CXT_Attribute
                    || psMDI->psChild->psChild == NULL )
                    continue;

                char* pszName = psMDI->psChild->psChild->pszValue;
                char* pszValue = psMDI->psChild->psNext->pszValue;
                if( pszName != NULL && pszValue != NULL )
                    poMDList->SetNameValue( pszName, pszValue );
            }
        }
    }

    return CSLCount(papszDomainList) != 0;
}

/************************************************************************/
/*                             Serialize()                              */
/************************************************************************/

CPLXMLNode *GDALMultiDomainMetadata::Serialize()

{
    CPLXMLNode *psFirst = NULL;

    for( int iDomain = 0; 
         papszDomainList != NULL && papszDomainList[iDomain] != NULL; 
         iDomain++)
    {
        char **papszMD = papoMetadataLists[iDomain]->List();
        // Do not serialize empty domains
        if( papszMD == NULL || papszMD[0] == NULL )
            continue;

        CPLXMLNode *psMD;
        int bFormatXML = FALSE;
        
        psMD = CPLCreateXMLNode( NULL, CXT_Element, "Metadata" );

        if( strlen( papszDomainList[iDomain] ) > 0 )
            CPLCreateXMLNode( 
                CPLCreateXMLNode( psMD, CXT_Attribute, "domain" ), 
                CXT_Text, papszDomainList[iDomain] );

        if( EQUALN(papszDomainList[iDomain],"xml:",4) 
            && CSLCount(papszMD) == 1 )
        {
            CPLXMLNode *psValueAsXML = CPLParseXMLString( papszMD[0] );
            if( psValueAsXML != NULL )
            {
                bFormatXML = TRUE;

                CPLCreateXMLNode( 
                    CPLCreateXMLNode( psMD, CXT_Attribute, "format" ), 
                    CXT_Text, "xml" );
                
                CPLAddXMLChild( psMD, psValueAsXML );
            }
        }

        if( !bFormatXML )
        {
            CPLXMLNode* psLastChild = NULL;
            // To go after domain attribute
            if( psMD->psChild != NULL )
            {
                psLastChild = psMD->psChild;
                while( psLastChild->psNext != NULL )
                    psLastChild = psLastChild->psNext; 
            }
            for( int i = 0; papszMD != NULL && papszMD[i] != NULL; i++ )
            {
                const char *pszRawValue;
                char *pszKey = NULL;
                CPLXMLNode *psMDI;
                
                pszRawValue = CPLParseNameValue( papszMD[i], &pszKey );
                
                psMDI = CPLCreateXMLNode( NULL, CXT_Element, "MDI" );
                if( psLastChild == NULL )
                    psMD->psChild = psMDI;
                else
                    psLastChild->psNext = psMDI;
                psLastChild = psMDI;

                CPLSetXMLValue( psMDI, "#key", pszKey );
                CPLCreateXMLNode( psMDI, CXT_Text, pszRawValue );
                
                CPLFree( pszKey );
            }
        }
            
        if( psFirst == NULL )
            psFirst = psMD;
        else
            CPLAddXMLSibling( psFirst, psMD );
    }

    return psFirst;
}
