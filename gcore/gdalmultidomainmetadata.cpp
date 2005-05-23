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
 * Revision 1.2  2005/05/23 06:45:59  fwarmerdam
 * fixed flaw in walking papszMD
 *
 * Revision 1.1  2005/05/22 08:13:53  fwarmerdam
 * New
 *
 */

#include "gdal_pam.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                      GDALMultiDomainMetadata()                       */
/************************************************************************/

GDALMultiDomainMetadata::GDALMultiDomainMetadata()

{
    papszDomainList = NULL;
    papapszMetadataLists = NULL;
}

/************************************************************************/
/*                      ~GDALMultiDomainMetadata()                      */
/************************************************************************/

GDALMultiDomainMetadata::~GDALMultiDomainMetadata()

{
    int i;

    CSLDestroy( papszDomainList );

    for( i = 0; papapszMetadataLists != NULL 
                && papapszMetadataLists[i] != NULL; i++ )
    {
        CSLDestroy( papapszMetadataLists[i] );
    }
    CPLFree( papapszMetadataLists );
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
        return papapszMetadataLists[iDomain];
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

        papapszMetadataLists = (char ***) 
            CPLRealloc( papapszMetadataLists, sizeof(char*)*(nDomainCount+1) );
        papapszMetadataLists[nDomainCount] = NULL;
        papapszMetadataLists[nDomainCount-1] = CSLDuplicate( papszMetadata );
    }
    else
    {
        papapszMetadataLists[iDomain] = CSLDuplicate( papszMetadata );
    }

    return CE_None;
}

/************************************************************************/
/*                          GetMetadataItem()                           */
/************************************************************************/

const char *GDALMultiDomainMetadata::GetMetadataItem( const char *pszName, 
                                                      const char *pszDomain )

{
    char **papszMD = GetMetadata( pszDomain );
    if( papszMD != NULL )
        return CSLFetchNameValue( papszMD, pszName );
    else
        return NULL;
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

    int iDomain = CSLFindString( papszDomainList, pszDomain );

    if( iDomain == -1 )
    {
        int nDomainCount;

        papszDomainList = CSLAddString( papszDomainList, pszDomain );
        nDomainCount = CSLCount( papszDomainList );

        papapszMetadataLists = (char ***) 
            CPLRealloc( papapszMetadataLists, sizeof(char*)*(nDomainCount+1) );
        papapszMetadataLists[nDomainCount] = NULL;
        papapszMetadataLists[nDomainCount-1] = 
            CSLSetNameValue( NULL, pszName, pszValue );
    }
    else
    {
        papapszMetadataLists[iDomain] = 
            CSLSetNameValue( papapszMetadataLists[iDomain], 
                             pszName, pszValue );
    }

    return CE_None;
}

/************************************************************************/
/*                              XMLInit()                               */
/*                                                                      */
/*      This method should be invoked on the parent of the              */
/*      <Metadata> elements.                                            */
/************************************************************************/

int GDALMultiDomainMetadata::XMLInit( CPLXMLNode *psTree )

{
    CPLXMLNode *psMetadata;

    for( psMetadata = psTree->psChild; 
         psMetadata != NULL; psMetadata = psMetadata->psNext )
    {
        char **papszMD = NULL;
        CPLXMLNode *psMDI;
        const char *pszDomain = "";

        if( psMetadata->eType != CXT_Element
            || !EQUAL(psMetadata->pszValue,"Metadata") )
            continue;

        /* Format is <MDI key="...">value_Text</MDI> */

        for( psMDI = psMetadata->psChild; psMDI != NULL; psMDI = psMDI->psNext )
        {
            if( psMDI->eType == CXT_Attribute 
                && EQUAL(psMDI->pszValue,"domain") 
                && psMDI->psChild != NULL )
                pszDomain = psMDI->psChild->pszValue; 

            if( !EQUAL(psMDI->pszValue,"MDI") || psMDI->eType != CXT_Element 
                || psMDI->psChild == NULL || psMDI->psChild->psNext == NULL 
                || psMDI->psChild->eType != CXT_Attribute
                || psMDI->psChild->psChild == NULL )
                continue;

            papszMD = 
                CSLSetNameValue( papszMD, psMDI->psChild->psChild->pszValue, 
                                 psMDI->psChild->psNext->pszValue );
        }

        SetMetadata( papszMD, pszDomain );
        CSLDestroy( papszMD );
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
        char **papszMD = papapszMetadataLists[iDomain];
        CPLXMLNode *psMD;
        
        psMD = CPLCreateXMLNode( NULL, CXT_Element, "Metadata" );

        if( strlen( papszDomainList[iDomain] ) > 0 )
            CPLCreateXMLNode( 
                CPLCreateXMLNode( psMD, CXT_Attribute, "Domain" ), 
                CXT_Text, papszDomainList[iDomain] );
        
        for( int i = 0; papszMD != NULL && papszMD[i] != NULL; i++ )
        {
            const char *pszRawValue;
            char *pszKey;
            CPLXMLNode *psMDI;
        
            pszRawValue = CPLParseNameValue( papszMD[i], &pszKey );

            psMDI = CPLCreateXMLNode( psMD, CXT_Element, "MDI" );
            CPLSetXMLValue( psMDI, "#key", pszKey );
            CPLCreateXMLNode( psMDI, CXT_Text, pszRawValue );
            
            CPLFree( pszKey );
        }

        if( psFirst == NULL )
            psFirst = psMD;
        else
            CPLAddXMLSibling( psFirst, psMD );
    }

    return psFirst;
}

