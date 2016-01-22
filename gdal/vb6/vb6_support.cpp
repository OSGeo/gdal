/******************************************************************************
 * $Id$
 *
 * Project:  GDAL VB6 Bindings
 * Purpose:  Support functions for GDAL VB6 bindings.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2005, Frank Warmerdam
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

#include <windows.h>
#include "cpl_string.h"
#include "gdal.h"

// This is just for the USES_CONVERSION & OLESTR stuff.
#include <atlbase.h>

CPL_CVSID("$Id$");

// External entry points (from VB)
extern "C" {

int __declspec(dllexport) __stdcall
vbCStringToVB6( VARIANT *vResult, char *pszInput );

void __declspec(dllexport) __stdcall
vbCSLToVariant( char **papszList, VARIANT *out_list );

char __declspec(dllexport) ** __stdcall 
vbVariantToCSL( VARIANT *vList );

int __declspec(dllexport) __stdcall 
vbSafeArrayToPtr( VARIANT *vArray, GDALDataType *peDataType, 
                  int *pnXSize, int *pnYSize );

}

static char *pszErrorMessage = NULL;

/************************************************************************/
/*                            CStringToVB6()                            */
/************************************************************************/

int __declspec(dllexport) __stdcall
vbCStringToVB6( VARIANT *vResult, char *pszInput )

{
    USES_CONVERSION;

    VariantClear( vResult );
    
    if( pszInput != NULL )
    {
        vResult->vt = VT_BSTR;
        vResult->bstrVal = SysAllocString( A2BSTR(pszInput) );
        return 0;
    }
    else
        return 1;
}

/************************************************************************/
/*                            CSLToVariant()                            */
/*                                                                      */
/*      Translate a list of C strings into a VARIANT array of           */
/*      VARIANT strings that can be returned to VB.                     */
/************************************************************************/

void __declspec(dllexport) __stdcall
vbCSLToVariant( char **papszList, VARIANT *out_list )

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
//        MessageBox( NULL, papszList[i-1], "Metadata Item", MB_OK );
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

char __declspec(dllexport) ** __stdcall 
vbVariantToCSL( VARIANT *vList )

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

    if( nUBound < nLBound )
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
    papszResult = (char **) CPLCalloc(sizeof(char *),(nUBound-nLBound+2));

    for( long iElement = nLBound; iElement <= nUBound; iElement++ )
    {
        BSTR bstrValue;
        char szValue[5000];

        if( FAILED(SafeArrayGetElement(psSA, &iElement, &bstrValue)) )
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "SafeArrayGetElement(%d) failed.", 
                      iElement );
            CSLDestroy( papszResult );
            return NULL;
        }

        sprintf( szValue, "%S", bstrValue );
        papszResult[iElement - nLBound] = CPLStrdup( szValue );
    }

    return papszResult;
}

/************************************************************************/
/*                          vbSafeArrayToPtr()                          */
/*                                                                      */
/*      Get the raw pointer (as LONG) and datatype and size from a      */
/*      SafeArray.                                                      */
/************************************************************************/

int __declspec(dllexport) __stdcall 
vbSafeArrayToPtr( VARIANT *vArray, GDALDataType *peDataType, 
                  int *pnXSize, int *pnYSize )

{
    char **papszResult = NULL;
    SAFEARRAY *psSA;
    long nLBound, nUBound;
    VARTYPE eVartype;

/* -------------------------------------------------------------------- */
/*      Get and verify info about safe array.                           */
/* -------------------------------------------------------------------- */
    if( vArray == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "VARIANT is NULL in SafeArrayToPtr()." );
        return NULL;
    }

    if( (vArray->vt & (VT_ARRAY | VT_BYREF)) == (VT_ARRAY | VT_BYREF) )
        psSA = *(vArray->pparray);
    else if( (vArray->vt & VT_ARRAY) == VT_ARRAY )
        psSA = vArray->parray;
    else
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "VARIANT is wrong type (%x).", 
                  vArray->vt );
        return NULL;
    }

    if( SafeArrayGetDim(psSA) < 1 || SafeArrayGetDim(psSA) > 2 )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Wrong dimension in array (%d)", 
                  SafeArrayGetDim(psSA) );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Get XSize                                                       */
/* -------------------------------------------------------------------- */
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

    *pnXSize = nUBound - nLBound + 1;

/* -------------------------------------------------------------------- */
/*      Get YSize                                                       */
/* -------------------------------------------------------------------- */
    if( SafeArrayGetDim(psSA) == 1 )
        *pnYSize = 1;
    else
    {
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
        
        *pnYSize = nUBound - nLBound + 1;
    }
        
/* -------------------------------------------------------------------- */
/*      Translate the type.                                             */
/* -------------------------------------------------------------------- */
    SafeArrayGetVartype(psSA, &eVartype );
    if( eVartype == VT_UI1 )
        *peDataType = GDT_Byte;
    else if( eVartype == VT_UI2 )
        *peDataType = GDT_UInt16;
    else if( eVartype == VT_I2 )
        *peDataType = GDT_Int16;
    else if( eVartype == VT_I4 || eVartype == VT_INT )
        *peDataType = GDT_Int32;
    else if( eVartype == VT_UI4 || eVartype == VT_UINT )
        *peDataType = GDT_UInt32;
    else if( eVartype == VT_R4 )
        *peDataType = GDT_Float32;
    else if( eVartype == VT_R8 )
        *peDataType = GDT_Float64;
    else
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "SafeArray contains type %d which is not supported.", 
                  eVartype );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Return the raw data pointer cast to an int.                     */
/* -------------------------------------------------------------------- */
    return (int) psSA->pvData;
}
