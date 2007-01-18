/******************************************************************************
 * $Id$
 *
 * Project:  DGN Tag Read/Write Bindings for Pacific Gas and Electric
 * Purpose:  VB callable entry points for DGN Tag read/update functions.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2002, Pacific Gas and Electric Co, San Franciso, CA, USA.
 *
 * All rights reserved.  Not to be used, reproduced or disclosed without
 * permission.
 ****************************************************************************/

#include <windows.h>
#include "dgn_pge.h"
#include "cpl_string.h"

// This is just for the USES_CONVERSION & OLESTR stuff.
#include <atlbase.h>

// External entry points (from VB)
extern "C" {

int _declspec(dllexport) __stdcall
vbDGNReadTags( const BSTR *bstrFilename, int nTagScheme, int *pnTagCount,
               VARIANT *vTagSets, VARIANT *vTagNames, VARIANT *vTagValues,
               VARIANT *vErrorMsg );

int _declspec(dllexport) __stdcall
vbDGNWriteTags( const BSTR *bstrFilename, int nTagScheme, int nTagCount,
                VARIANT *vTagSets, VARIANT *vTagNames, VARIANT *vTagValues,
                VARIANT *vErrorMsg );
}

static char *pszErrorMessage = NULL;

/************************************************************************/
/*                        CPLPGEErrorCollector()                        */
/*                                                                      */
/*      This function will be called everytime a CPLError() call is     */
/*      made anywhere in the library.  It accumulates all error         */
/*      messages in a local buffer so they can be returned to the       */
/*      application as a group.                                         */
/************************************************************************/

static void CPLPGEErrorCollector( CPLErr eErrType, int nErrorCode, 
                                  const char *pszMessage )

{       
    if( pszErrorMessage == NULL )
    {
        pszErrorMessage = (char *) CPLMalloc(strlen(pszMessage)+1);
        pszErrorMessage[0] = '\0';
    }
    else
        pszErrorMessage = (char *) 
            CPLRealloc(pszErrorMessage, 
                       strlen(pszErrorMessage)+strlen(pszMessage)+3);

    strcat( pszErrorMessage, "\n" );
    strcat( pszErrorMessage, pszMessage );
}

/************************************************************************/
/*                           PGEErrorClear()                            */
/*                                                                      */
/*      Initialize error system (if not alrady initialized), and        */
/*      clear any errors if any are posted.                             */
/************************************************************************/

static void PGEErrorClear()

{
    static int bErrorHandlerInstalled = FALSE;

    if( !bErrorHandlerInstalled )
    {
        CPLSetErrorHandler( CPLPGEErrorCollector );
        bErrorHandlerInstalled = TRUE;
    }
        
    CPLErrorReset();

    if( pszErrorMessage != NULL )
        CPLFree( pszErrorMessage );
    pszErrorMessage = NULL;
}

/************************************************************************/
/*                          SetErrorMessage()                           */
/*                                                                      */
/*      Apply the current error message(s) if any to the VARIANT        */
/*      that can be returned to VB.                                     */
/************************************************************************/

static void SetErrorMessage( VARIANT *vErrorMsg )

{
    USES_CONVERSION;
    const char *pszMsg = pszErrorMessage;

    VariantClear( vErrorMsg );

    if( pszMsg == NULL || strlen(pszMsg) == 0 )
        pszMsg = "Unknown Failure";
    
    vErrorMsg->vt = VT_BSTR;
    vErrorMsg->bstrVal = SysAllocString( A2BSTR(pszMsg) );

    PGEErrorClear();
}

/************************************************************************/
/*                            CSLToVariant()                            */
/*                                                                      */
/*      Translate a list of C strings into a VARIANT array of           */
/*      VARIANT strings that can be returned to VB.                     */
/************************************************************************/

static void CSLToVariant( char **papszList, VARIANT *out_list )

{
    USES_CONVERSION;
    SAFEARRAYBOUND sBounds;
    SAFEARRAY *result;
    long i, nLength = CSLCount( papszList );

/* -------------------------------------------------------------------- */
/*      Create safe array result.                                       */
/* -------------------------------------------------------------------- */
    sBounds.lLbound = 1;
    sBounds.cElements = nLength;

    result = SafeArrayCreate( VT_BSTR, 1, &sBounds );     

    for( i = 1; i <= nLength; i++ )
    {
        SafeArrayPutElement( result, &i, 
                             SysAllocString( A2BSTR(papszList[i-1]) ) );
    }

/* -------------------------------------------------------------------- */
/*      Assign to variant.                                              */
/* -------------------------------------------------------------------- */
    VariantClear( out_list );

    out_list->vt = VT_BSTR | VT_ARRAY;
    out_list->parray = result;
}

/************************************************************************/
/*                            VariantToCSL()                            */
/*                                                                      */
/*      Extract a list of strings from a variant as a stringlist.       */
/************************************************************************/

static char **VariantToCSL( VARIANT *vList )

{
    char **papszResult = NULL;
    SAFEARRAY *psSA;
    long nLBound, nUBound;
    VARTYPE eVartype;

/* -------------------------------------------------------------------- */
/*      Get and verify info about safe array.                           */
/* -------------------------------------------------------------------- */
    if( vList == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "VARIANT is NULL in VariantToCSL()." );
        return NULL;
    }

    if( vList->vt == (VT_BSTR | VT_ARRAY | VT_BYREF) )
        psSA = *(vList->pparray);
    else if( vList->vt == (VT_BSTR | VT_ARRAY) )
        psSA = vList->parray;
    else
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "VARIANT is wrong type (%x).", 
                  vList->vt );
        return NULL;
    }

    if( SafeArrayGetDim(psSA) != 1 )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Wrong dimension in array (%d)", 
                  SafeArrayGetDim(psSA) );
        return NULL;
    }

    if( FAILED(SafeArrayGetLBound( psSA, 1, &nLBound ))
        || FAILED(SafeArrayGetUBound( psSA, 1, &nUBound)) )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "SafeARrayGet{L,U}Bound() failed." );
        return NULL;
    }

    if( nUBound <= nLBound )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Crazy L/U Bound (L=%d, U=%d)",
                  nLBound, nUBound );
        return NULL;
    }

    SafeArrayGetVartype(psSA, &eVartype );
    if( eVartype != VT_BSTR )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "SafeArray contains type %d instead of VT_BSTR.", 
                  eVartype );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Create string list from safe array BSTRings.                    */
/* -------------------------------------------------------------------- */
    for( long iElement = nLBound; iElement < nUBound; iElement++ )
    {
        BSTR bstrValue;
        char szValue[1000];

        if( FAILED(SafeArrayGetElement(psSA, &iElement, &bstrValue)) )
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "SafeArrayGetElement(%d) failed.", 
                      iElement );
            CSLDestroy( papszResult );
            return NULL;
        }

        sprintf( szValue, "%S", bstrValue );
        papszResult = CSLAddString( papszResult, szValue );
    }

    return papszResult;
}

/************************************************************************/
/*                           vbDGNWriteTags()                           */
/*                                                                      */
/*      VB callable function for writing tags to a DGN file.            */
/************************************************************************/

int  _declspec(dllexport) __stdcall
vbDGNWriteTags( const BSTR *bstrFilename, int nTagScheme, int nTagCount,
                VARIANT *vTagSets, VARIANT *vTagNames, VARIANT *vTagValues,
                VARIANT *vErrorMsg )

{
    int       nResultCode = TRUE;

/* -------------------------------------------------------------------- */
/*      Translate inputs into C string lists.                           */
/* -------------------------------------------------------------------- */
    char      **papszTagSets = VariantToCSL(vTagSets);
    char      **papszTagNames = VariantToCSL(vTagNames);
    char      **papszTagValues = VariantToCSL(vTagValues);
  
/* -------------------------------------------------------------------- */
/*      Initialize error handling.                                      */
/* -------------------------------------------------------------------- */
    PGEErrorClear();

    VariantClear( vErrorMsg );

/* -------------------------------------------------------------------- */
/*      Verify that arguments ... especially the counts.                */
/* -------------------------------------------------------------------- */
    if( papszTagSets == NULL 
        || papszTagNames == NULL 
        || papszTagValues == NULL )
    {
        SetErrorMessage( vErrorMsg );
        nResultCode = FALSE;
    }
    else if( CSLCount(papszTagSets) != nTagCount
             || CSLCount(papszTagNames) != nTagCount
             || CSLCount(papszTagValues) != nTagCount )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Some of array count(s) (%d,%d,%d) don't match passed tag count (%d)", 
                  CSLCount(papszTagSets), 
                  CSLCount(papszTagNames), 
                  CSLCount(papszTagValues), 
                  nTagCount );
                  
        SetErrorMessage( vErrorMsg );
        nResultCode = FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Write the tags.                                                 */
/* -------------------------------------------------------------------- */
    else
    {
        nResultCode = 
            DGNWriteTags( (const char *) bstrFilename, nTagScheme, 
                          papszTagSets, papszTagNames, papszTagValues);

/* -------------------------------------------------------------------- */
/*      Report errors.                                                  */
/* -------------------------------------------------------------------- */
        if( !nResultCode )
        {
            if( pszErrorMessage == NULL )
                CPLError( CE_Failure, CPLE_AppDefined, 
                          "General error in DGNWriteTags" );
            SetErrorMessage( vErrorMsg );
        }
    }

/* -------------------------------------------------------------------- */
/*      Cleanup temporary variables.                                    */
/* -------------------------------------------------------------------- */
    CSLDestroy( papszTagSets );
    CSLDestroy( papszTagNames );
    CSLDestroy( papszTagValues );

    return nResultCode;
}

/************************************************************************/
/*                           vbDGNReadTags()                            */
/*                                                                      */
/*      VB callable function for reading all the tags from a DGN        */
/*      file.                                                           */
/************************************************************************/

int  _declspec(dllexport) __stdcall
vbDGNReadTags( const BSTR *bstrFilename, int nTagScheme, int *pnTagCount,
               VARIANT *vTagSets, VARIANT *vTagNames, VARIANT *vTagValues,
               VARIANT *vErrorMsg )

{
    char      **papszTagSets = NULL;
    char      **papszTagNames = NULL;
    char      **papszTagValues = NULL;
    int       nResultCode;

/* -------------------------------------------------------------------- */
/*      Initialize error information.                                   */
/* -------------------------------------------------------------------- */
    PGEErrorClear();

    VariantClear( vErrorMsg );

/* -------------------------------------------------------------------- */
/*      Read the tag values.                                            */
/* -------------------------------------------------------------------- */
    nResultCode = DGNReadTags( (const char *) bstrFilename, nTagScheme, 
                               &papszTagSets, &papszTagNames, &papszTagValues);

/* -------------------------------------------------------------------- */
/*      Translate tag list into VB compatible variables.                */
/* -------------------------------------------------------------------- */
    *pnTagCount = CSLCount( papszTagSets );

    CSLToVariant( papszTagSets, vTagSets );
    CSLToVariant( papszTagNames, vTagNames );
    CSLToVariant( papszTagValues, vTagValues );

    CSLDestroy( papszTagSets );
    CSLDestroy( papszTagNames );
    CSLDestroy( papszTagValues );
    
/* -------------------------------------------------------------------- */
/*      Return error if there is one.                                   */
/* -------------------------------------------------------------------- */
    if( !nResultCode )
        SetErrorMessage( vErrorMsg );

    return nResultCode;
}



