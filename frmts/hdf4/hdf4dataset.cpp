/******************************************************************************
 * $Id$
 *
 * Project:  Hierarchical Data Format Release 4 (HDF4)
 * Purpose:  HDF4 Datasets. Open HDF4 file, fetch metadata and list of
 *           subdatasets.
 *           This driver initially based on code supplied by Markus Neteler
 * Author:   Andrey Kiselev, dron@remotesensing.org
 *
 ******************************************************************************
 * Copyright (c) 2002, Andrey Kiselev <dron@remotesensing.org>
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
 * Revision 1.26  2003/11/07 15:48:01  dron
 * TranslateHDF4Attributes() improved, added GetDataTypeSize().
 *
 * Revision 1.25  2003/11/05 17:05:02  warmerda
 * Fixed type casting problem (bug 431).
 *
 * Revision 1.24  2003/06/26 20:42:31  dron
 * Support for Hyperion Level 1 data product.
 *
 * Revision 1.23  2003/06/25 08:26:18  dron
 * Support for Aster Level 1A/1B/2 products.
 *
 * Revision 1.22  2003/06/12 15:07:04  dron
 * Added support for SeaWiFS Level 3 Standard Mapped Image Products.
 *
 * Revision 1.21  2003/06/10 09:32:31  dron
 * Added support for MODIS Level 3 products.
 *
 * Revision 1.20  2003/05/21 14:11:43  dron
 * MODIS Level 1B earth-view (EV) product now supported.
 *
 * Revision 1.19  2003/03/01 15:54:43  dron
 * Significant improvements in HDF EOS metadata parsing.
 *
 * Revision 1.18  2003/02/27 14:28:55  dron
 * Fixes in HDF-EOS metadata parsing algorithm.
 *
 * Revision 1.17  2002/12/19 17:42:57  dron
 * Size of string buffer in TranslateHDF4EOSAttributes() fixed.
 *
 * Revision 1.16  2002/11/29 18:25:22  dron
 * MODIS determination improved.
 *
 * Revision 1.15  2002/11/13 06:43:15  warmerda
 * quote filename in case it contains a drive indicator
 *
 * Revision 1.14  2002/11/13 06:00:04  warmerda
 * avoid use of long long type as it doesn't exist perse on windows
 *
 * Revision 1.13  2002/11/08 18:29:04  dron
 * Added Create() method.
 *
 * Revision 1.12  2002/11/07 13:23:44  dron
 * Support for projection information writing.
 *
 * Revision 1.11  2002/11/06 15:47:14  dron
 * Added support for 3D datasets creation
 *
 * Revision 1.10  2002/10/25 14:28:54  dron
 * Initial support for HDF4 creation.
 *
 * Revision 1.9  2002/09/25 14:44:40  warmerda
 * Fixed iDataType initialization.
 *
 * Revision 1.8  2002/09/06 10:42:23  dron
 * Georeferencing for ASTER Level 1b datasets and ASTER DEMs.
 *
 * Revision 1.7  2002/09/04 06:50:37  warmerda
 * avoid static driver pointers
 *
 * Revision 1.6  2002/07/23 12:27:58  dron
 * General Raster Interface support added.
 *
 * Revision 1.5  2002/07/19 13:39:17  dron
 * Lists supported in HDF-EOS attributes.
 *
 * Revision 1.4  2002/07/17 16:24:31  dron
 * MODIS support improved a bit.
 *
 * Revision 1.3  2002/07/17 13:36:18  dron
 * <hdf.h> and <mfhdf.h> changed to "hdf.h" and "mfhdf.h".
 *
 * Revision 1.2  2002/07/16 17:51:10  warmerda
 * removed hdf/ from include statements
 *
 * Revision 1.1  2002/07/16 11:04:11  dron
 * New driver: HDF4 datasets. Initial version.
 *
 *
 */

#include "hdf.h"
#include "mfhdf.h"

#include "gdal_priv.h"
#include "cpl_string.h"
#include "hdf4dataset.h"

CPL_CVSID("$Id$");

CPL_C_START
void	GDALRegister_HDF4(void);
CPL_C_END

extern const char *pszGDALSignature;

/************************************************************************/
/* ==================================================================== */
/*				HDF4Dataset				*/
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                           HDF4Dataset()                      	*/
/************************************************************************/

HDF4Dataset::HDF4Dataset()

{
    fp = NULL;
    hHDF4 = 0;
    hSD = 0;
    hGR = 0;
    psDataField = NULL;
    psDimMap = NULL;
    papszGlobalMetadata = NULL;
    papszSubDatasets = NULL;
}

/************************************************************************/
/*                            ~HDF4Dataset()                            */
/************************************************************************/

HDF4Dataset::~HDF4Dataset()

{
    if ( hSD )
	SDend( hSD );
    if ( hGR )
	GRend( hGR );
    if ( papszSubDatasets )
	CSLDestroy( papszSubDatasets );
    if ( papszGlobalMetadata )
	CSLDestroy( papszGlobalMetadata );
    if( fp != NULL )
        VSIFClose( fp );
    if( hHDF4 > 0 )
	Hclose( hHDF4 );

    if ( psDimMap )
    {
        int     i, nCount;

        nCount = CPLListCount( psDimMap );
        for ( i = 0; i < nCount; i++ )
        {
            HDF4EOSDimensionMap *psTemp;

            psTemp = (HDF4EOSDimensionMap *)
                CPLListGetData( CPLListGet( psDimMap, i ) );
            CPLFree( psTemp->pszDataDimension );
            CPLFree( psTemp );

        }
        CPLListDestroy( psDimMap );
    }

    if ( psDataField )
    {
        int     i, nCount;

        nCount = CPLListCount( psDataField );
        for ( i = 0; i < nCount; i++ )
        {
            HDF4EOSDataField *psTemp;

            psTemp = (HDF4EOSDataField *)
                CPLListGetData( CPLListGet( psDataField, i ) );
            CPLFree( psTemp->pszDataFieldName );
            CPLListDestroy( psTemp->psDimList );
            CPLFree( psTemp );

        }
        CPLListDestroy( psDataField );
    }
}

/************************************************************************/
/*                            GetMetadata()                             */
/************************************************************************/

char **HDF4Dataset::GetMetadata( const char *pszDomain )

{
    if( pszDomain != NULL && EQUALN( pszDomain, "SUBDATASETS", 11 ) )
        return papszSubDatasets;
    else
        return GDALDataset::GetMetadata( pszDomain );
}

/************************************************************************/
/*	Prints numerical arrays in string buffer.			*/
/*	This function takes pfaDataArray as a pointer to printed array,	*/
/*	nValues as a number of values to print and pszDelimiter as a	*/
/*	field delimiting strings.					*/
/*	Pointer to filled buffer will be returned.			*/
/************************************************************************/

static char *SPrintArray( GDALDataType eDataType, void *paDataArray,
                          int nValues, char * pszDelimiter )
{
    char        *pszString, *pszField;
    int         i, iFieldSize, iStringSize;

    iFieldSize = 32 + strlen( pszDelimiter );
    pszField = (char *)CPLMalloc( iFieldSize + 1 );
    iStringSize = nValues * iFieldSize + 1;
    pszString = (char *)CPLMalloc( iStringSize );
    memset( pszString, 0, iStringSize );
    for ( i = 0; i < nValues; i++ )
    {
        switch ( eDataType )
        {
            case GDT_Byte:
                sprintf( pszField, "%d%s", ((GByte *)paDataArray)[i],
                     (i < nValues - 1)?pszDelimiter:"" );
                break;
            case GDT_UInt16:
                sprintf( pszField, "%d%s", ((GUInt16 *)paDataArray)[i],
                     (i < nValues - 1)?pszDelimiter:"" );
                break;
            case GDT_Int16:
            default:
                sprintf( pszField, "%d%s", ((GInt16 *)paDataArray)[i],
                     (i < nValues - 1)?pszDelimiter:"" );
                break;
            case GDT_UInt32:
                sprintf( pszField, "%d%s", ((GUInt32 *)paDataArray)[i],
                     (i < nValues - 1)?pszDelimiter:"" );
                break;
            case GDT_Int32:
                sprintf( pszField, "%d%s", ((GInt32 *)paDataArray)[i],
                     (i < nValues - 1)?pszDelimiter:"" );
                break;
            case GDT_Float32:
                sprintf( pszField, "%.7g%s", ((float *)paDataArray)[i],
                     (i < nValues - 1)?pszDelimiter:"" );
                break;
            case GDT_Float64:
                sprintf( pszField, "%.15g%s", ((double *)paDataArray)[i],
                     (i < nValues - 1)?pszDelimiter:"" );
                break;
        }
        strcat( pszString, pszField );
    }
    
    CPLFree( pszField );
    return pszString;
}

/************************************************************************/
/*              Translate HDF4 data type into GDAL data type            */
/************************************************************************/
GDALDataType HDF4Dataset::GetDataType( int32 iNumType )
{
    switch (iNumType)
    {
        case DFNT_CHAR8: // The same as DFNT_CHAR
        case DFNT_UCHAR8: // The same as DFNT_UCHAR
        case DFNT_INT8:
        case DFNT_UINT8:
            return GDT_Byte;
        case DFNT_INT16:
            return GDT_Int16;
        case DFNT_UINT16:
            return GDT_UInt16;
        case DFNT_INT32:
            return GDT_Int32;
        case DFNT_UINT32:
            return GDT_UInt32;
        case DFNT_INT64:
            return GDT_Unknown;
        case DFNT_UINT64:
            return GDT_Unknown;
        case DFNT_FLOAT32:
            return GDT_Float32;
        case DFNT_FLOAT64:
            return GDT_Float64;
        default:
            return GDT_Unknown;
    }
}

/************************************************************************/
/*		Return the human readable name of data type		*/
/************************************************************************/

const char *HDF4Dataset::GetDataTypeName( int32 iNumType )
{
    switch (iNumType)
    {
        case DFNT_CHAR8: // The same as DFNT_CHAR
	    return "8-bit character";
	case DFNT_UCHAR8: // The same as DFNT_UCHAR
	    return "8-bit unsigned character";
        case DFNT_INT8:
	    return "8-bit integer";
        case DFNT_UINT8:
	    return "8-bit unsigned integer";
        case DFNT_INT16:
	    return "16-bit integer";
        case DFNT_UINT16:
	    return "16-bit unsigned integer";
        case DFNT_INT32:
	    return "32-bit integer";
        case DFNT_UINT32:
	    return "32-bit unsigned integer";
        case DFNT_INT64:
	    return "64-bit integer";
        case DFNT_UINT64:
	    return "64-bit unsigned integer";
        case DFNT_FLOAT32:
	    return "32-bit floating-point";
        case DFNT_FLOAT64:
	    return "64-bit floating-point";
	default:
	    return "unknown type";
    }
}

/************************************************************************/
/*		Return the size of data type in bytes           	*/
/************************************************************************/

int HDF4Dataset::GetDataTypeSize( int32 iNumType )
{
    switch (iNumType)
    {
        case DFNT_CHAR8: // The same as DFNT_CHAR
	case DFNT_UCHAR8: // The same as DFNT_UCHAR
        case DFNT_INT8:
        case DFNT_UINT8:
	    return 1;
        case DFNT_INT16:
        case DFNT_UINT16:
	    return 2;
        case DFNT_INT32:
        case DFNT_UINT32:
        case DFNT_FLOAT32:
	    return 4;
        case DFNT_INT64:
        case DFNT_UINT64:
        case DFNT_FLOAT64:
	    return 8;
	default:
	    return 0;
    }
}

/************************************************************************/
/*         Tokenize HDF-EOS attributes.                                 */
/************************************************************************/

char **HDF4Dataset::HDF4EOSTokenizeAttrs( const char * pszString )

{
    const char  *pszDelimiters = " \t\n\r";
    char        **papszRetList = NULL;
    char        *pszToken;
    int         nTokenMax, nTokenLen;

    pszToken = (char *) CPLCalloc( 10, 1 );
    nTokenMax = 10;
    
    while( pszString != NULL && *pszString != '\0' )
    {
        int     bInString = FALSE, bInBracket = FALSE;

        nTokenLen = 0;
        
        // Try to find the next delimeter, marking end of token
        for( ; *pszString != '\0'; pszString++ )
        {

            // End if this is a delimeter skip it and break.
            if ( !bInBracket && !bInString
                 && strchr(pszDelimiters, *pszString) != NULL )
            {
                pszString++;
                break;
            }

            // Sometimes in bracketed tokens we may found a sort of
            // paragraph formatting. We will remove unneeded spaces and new
            // lines. 
            if ( bInBracket )
                if ( strchr("\r\n", *pszString) != NULL
                     || ( *pszString == ' '
                          && strchr(" \r\n", *(pszString - 1)) != NULL ) )
                continue;
            
            if ( *pszString == '"' )
            {
                if ( bInString )
                {
                    bInString = FALSE;
                    continue;
                }
                else
                {
                    bInString = TRUE;
                    continue;
                }
            }
            else if ( *pszString == '(' )
	    {
                bInBracket = TRUE;
		continue;
	    }
	    else if ( *pszString == ')' )
	    {
		bInBracket = FALSE;
		continue;
	    }

	    if( nTokenLen >= nTokenMax - 2 )
            {
                nTokenMax = nTokenMax * 2 + 10;
                pszToken = (char *) CPLRealloc( pszToken, nTokenMax );
            }

            pszToken[nTokenLen] = *pszString;
            nTokenLen++;
        }

        pszToken[nTokenLen] = '\0';

        if( pszToken[0] != '\0' )
        {
            papszRetList = CSLAddString( papszRetList, pszToken );
        }

        // If the last token is an empty token, then we have to catch
        // it now, otherwise we won't reenter the loop and it will be lost. 
        if ( *pszString == '\0' && strchr(pszDelimiters, *(pszString-1)) )
        {
            papszRetList = CSLAddString( papszRetList, "" );
        }
    }

    if( papszRetList == NULL )
        papszRetList = (char **) CPLCalloc( sizeof(char *), 1 );

    CPLFree( pszToken );

    return papszRetList;
}

/************************************************************************/
/*     Find object name and its value in HDF-EOS attributes.            */
/*     Function returns pointer to the string in list next behind       */
/*     recognized object.                                               */
/************************************************************************/

char **HDF4Dataset::HDF4EOSGetObject( char **papszAttrList, char **ppszAttrName,
                                      char **ppszAttrValue )
{
    int	    iCount, i, j;
    *ppszAttrName = NULL;
    *ppszAttrValue = NULL;

    iCount = CSLCount( papszAttrList );
    for ( i = 0; i < iCount - 2; i++ )
    {
	if ( EQUAL( papszAttrList[i], "OBJECT" ) )
	{
	    i += 2;
	    for ( j = 1; i + j < iCount - 2; j++ )
	    {
	        if ( EQUAL( papszAttrList[i + j], "END_OBJECT" ) ||
		     EQUAL( papszAttrList[i + j], "OBJECT" ) )
	            return &papszAttrList[i + j];
	        else if ( EQUAL( papszAttrList[i + j], "VALUE" ) )
	        {
		    *ppszAttrName = papszAttrList[i];
	            *ppszAttrValue = papszAttrList[i + j + 2];

		    return &papszAttrList[i + j + 2];
	        }
	    }
	}
    }

    return NULL;
}

/************************************************************************/
/*         Translate HDF4-EOS attributes in GDAL metadata items         */
/************************************************************************/

char** HDF4Dataset::TranslateHDF4EOSAttributes( int32 iHandle,
    int32 iAttribute, int32 nValues, char **papszMetadata )
{
    char	*pszData;
    
    pszData = (char *)CPLMalloc( (nValues + 1) * sizeof(char) );
    pszData[nValues] = '\0';
    SDreadattr( iHandle, iAttribute, pszData );
    // HDF4-EOS attributes has followed structure:
    // 
    // GROUP = <name>
    //   GROUPTYPE = <name>
    //
    //   GROUP = <name>
    //   
    //     OBJECT = <name>
    //       CLASS = <string>
    //       NUM_VAL = <number>
    //       VALUE = <string> or <number>
    //     END_OBJECT = <name>
    //     
    //     .......
    //     .......
    //     .......
    //     
    //   END_GROUP = <name>
    //   
    // .......
    // .......
    // .......
    //
    // END_GROUP = <name>
    // END
    //
    // Used data types:
    // <name>   --- unquoted character strings
    // <string> --- quoted character strings
    // <number> --- numerical value
    // If NUM_VAL != 1 then values combined in lists:
    // (<string>,<string>,...)
    // or
    // (<number>,<number>,...)
    //
    // Records within objects may follows in any order, objects may contains
    // other objects (and lacks VALUE record), groups contains other groups
    // and objects. Names of groups and objects are not unique and may repeat.
    // Objects may contains other types of records.
    //
    // We are interested in OBJECTS structures only.

    char *pszAttrName, *pszAttrValue;
    char *pszAddAttrName = NULL;
    char **papszAttrList, **papszAttrs;
    
    papszAttrList = HDF4EOSTokenizeAttrs( pszData );
    papszAttrs = papszAttrList;
    while ( papszAttrs )
    {
	papszAttrs =
	    HDF4EOSGetObject( papszAttrs, &pszAttrName, &pszAttrValue );
	if ( pszAttrName && pszAttrValue )
	{
	    // Now we should recognize special type of HDF EOS metastructures:
	    // ADDITIONALATTRIBUTENAME = <name>
	    // PARAMETERVALUE = <value>
	    if ( EQUAL( pszAttrName, "ADDITIONALATTRIBUTENAME" ) )
		pszAddAttrName = pszAttrValue;
	    else if ( pszAddAttrName && EQUAL( pszAttrName, "PARAMETERVALUE" ) )
	    {
		papszMetadata =
		    CSLAddNameValue( papszMetadata, pszAddAttrName, pszAttrValue );
		pszAddAttrName = NULL;
	    }
	    else
	    {
		papszMetadata =
		    CSLAddNameValue( papszMetadata, pszAttrName, pszAttrValue );
	    }
	}
    }
    
    CSLDestroy( papszAttrList );
    CPLFree( pszData );

    return papszMetadata;
}

/************************************************************************/
/*         Translate HDF4 attributes in GDAL metadata items             */
/************************************************************************/

char** HDF4Dataset::TranslateHDF4Attributes( int32 iHandle,
    int32 iAttribute, char *pszAttrName, int32 iNumType, int32 nValues,
    char **papszMetadata )
{
    void	*pData = NULL;
    char	*pszTemp = NULL;
    
/* -------------------------------------------------------------------- */
/*     Allocate a buffer to hold the attribute data.                    */
/* -------------------------------------------------------------------- */
    if ( iNumType == DFNT_CHAR8 || iNumType == DFNT_UCHAR8 )
        pData = CPLMalloc( (nValues + 1) * GetDataTypeSize(iNumType) );
    else
        pData = CPLMalloc( nValues * GetDataTypeSize(iNumType) );

/* -------------------------------------------------------------------- */
/*     Read the attribute data.                                         */
/* -------------------------------------------------------------------- */
    SDreadattr( iHandle, iAttribute, pData );
    if ( iNumType == DFNT_CHAR8 || iNumType == DFNT_UCHAR8 )
    {
        ((char *)pData)[nValues] = '\0';
        papszMetadata = CSLAddNameValue( papszMetadata, pszAttrName, 
                                         (const char *) pData );
    }
    else
    {
        pszTemp = SPrintArray( GetDataType(iNumType), pData, nValues, ", " );
        papszMetadata = CSLAddNameValue( papszMetadata, pszAttrName, pszTemp );
        if ( pszTemp )
	    CPLFree( pszTemp );
    }
    
    if ( pData )
	CPLFree( pData );

    return papszMetadata;
}

/************************************************************************/
/*                    HDF4EOSParseStructMetadata()                      */
/************************************************************************/

void HDF4Dataset::HDF4EOSParseStructMetadata( int32 iHandle, int32 iAttribute,
                                              int32 nValues )
{
    char	*pszData;
    char        **papszAttrList;
    int	        iCount, i, j;
    CPLList     *psDM = NULL;
    
    pszData = (char *)CPLMalloc( (nValues + 1) * sizeof(char) );
    pszData[nValues] = '\0';
    SDreadattr( iHandle, iAttribute, pszData );
    
    papszAttrList =
        CSLTokenizeString2( pszData, "\r\n\t =", CSLT_HONOURSTRINGS );

    iCount = CSLCount( papszAttrList );
    for ( i = 0; i < iCount - 3; i++ )
    {

/* -------------------------------------------------------------------- */
/*     Extract DimensionMap table.                                      */
/* -------------------------------------------------------------------- */
        if ( EQUAL( papszAttrList[i], "GROUP" )
             && EQUALN( papszAttrList[i + 1], "DimensionMap", 12 ) )
        {
            i += 2;
            do
            {
                if ( EQUAL( papszAttrList[i], "OBJECT" ) &&
                     EQUALN( papszAttrList[i + 1], "DimensionMap", 12 ) )
                {
                    for ( j = 2; i + j < iCount - 1; j++ )
                    {
                        HDF4EOSDimensionMap *psTemp;

                        if ( EQUAL( papszAttrList[i + j], "END_OBJECT" ) ||
                             EQUAL( papszAttrList[i + j], "OBJECT" ) )
                        {
                            break;
                        }
                        else if ( EQUAL(papszAttrList[i + j], "DataDimension") )
                        {
                            psTemp = (HDF4EOSDimensionMap *)
                                CPLMalloc( sizeof(HDF4EOSDimensionMap) );
                            psTemp->pszDataDimension =
                                CPLStrdup( papszAttrList[i + j + 1] );
                            psDM = CPLListAppend( psDM, psTemp );

                            // Store head of the list for later freeing
                            if ( !psDimMap )
                                psDimMap = psDM;
                        }
                        else if ( EQUAL( papszAttrList[i + j], "Offset" ) )
                        {
                            psTemp = (HDF4EOSDimensionMap *)
                                CPLListGetData( CPLListGetLast(psDM) );
                            psTemp->dfOffset = atof( papszAttrList[i + j + 1] );
                        }
                        else if ( EQUAL( papszAttrList[i + j], "Increment" ) )
                        {
                            psTemp = (HDF4EOSDimensionMap *)
                                CPLListGetData( CPLListGetLast(psDM) );
                            psTemp->dfIncrement =
                                atof( papszAttrList[i + j + 1] );
                        }
                    }

                    i += j;
                }
                
                i++;
            }
            while ( i < iCount - 2 && !EQUAL( papszAttrList[i], "END_GROUP" ) );
            i++;
        }

/* -------------------------------------------------------------------- */
/*     Extract DataField table.                                         */
/* -------------------------------------------------------------------- */
        if ( i < iCount - 3
             && EQUAL( papszAttrList[i], "GROUP" )
             && EQUALN( papszAttrList[i + 1], "DataField", 9 ) )
        {
            i += 2;
            do
            {
                if ( EQUAL( papszAttrList[i], "OBJECT" ) &&
                     EQUALN( papszAttrList[i + 1], "DataField", 9 ) )
                {
                    for ( j = 2; i + j < iCount - 1; j++ )
                    {
                        HDF4EOSDataField *psTemp;

                        if ( EQUAL( papszAttrList[i + j], "END_OBJECT" ) ||
                             EQUAL( papszAttrList[i + j], "OBJECT" ) )
                        {
                            break;
                        }
                        else if ( EQUAL( papszAttrList[i + j], "DataFieldName" ) )
                        {
                            psTemp = (HDF4EOSDataField *)
                                CPLMalloc(sizeof(HDF4EOSDataField));
                            psTemp->pszDataFieldName =
                                CPLStrdup( papszAttrList[i + j + 1] );
                            psTemp->psDimList = NULL;
                            psDataField = CPLListAppend( psDataField, psTemp );
                        }
                        else if ( EQUAL( papszAttrList[i + j], "DimList" ) )
                        {
                            int k, l;
                            char **papszDimList =
                                CSLTokenizeString2( papszAttrList[i + j + 1],
                                                    "(), ",
                                                    CSLT_HONOURSTRINGS );
                            psTemp = (HDF4EOSDataField *)
                                CPLListGetData( CPLListGetLast(psDataField) );
                            for ( k = 0; k < CSLCount(papszDimList); k++ )
                            {
                                for ( l = 0; l < CPLListCount(psDM); l++ )
                                {
                                    CPLList *psElem = CPLListGet( psDM, l );
                                    if( EQUAL( papszDimList[k],
        ((HDF4EOSDimensionMap *)CPLListGetData( psElem ))->pszDataDimension ) )
                                    {
                                        psTemp->psDimList =
                                            CPLListAppend( psTemp->psDimList,
                                                    CPLListGetData( psElem ) );
                                        break;
                                    }
                                }
                            }

                            CSLDestroy( papszDimList );
                        }
                    }

                    i += j;
                }
                
                i++;
            }
            while ( i < iCount - 1 && !EQUAL( papszAttrList[i], "END_GROUP" ) );

            // Point psDM to the last value.
            psDM = CPLListGetLast( psDM );
        }
    }

    CSLDestroy( papszAttrList );
    CPLFree( pszData );
}

/************************************************************************/
/*                       ReadGlobalAttributes()                         */
/************************************************************************/

CPLErr HDF4Dataset::ReadGlobalAttributes( int32 iHandler )
{
    int32	iAttribute, nValues, iNumType, nAttributes;
    char	szAttrName[MAX_NC_NAME];

/* -------------------------------------------------------------------- */
/*     Obtain number of SDSsand global attributes in input file.        */
/* -------------------------------------------------------------------- */
    if ( SDfileinfo( hSD, &nDatasets, &nAttributes ) != 0 )
	return CE_Failure;

    // Loop trough the all attributes
    for ( iAttribute = 0; iAttribute < nAttributes; iAttribute++ )
    {
        // Get information about the attribute. Note that the first
        // parameter is an SD interface identifier.
        SDattrinfo( iHandler, iAttribute, szAttrName, &iNumType, &nValues );
        if ( EQUALN( szAttrName, "coremetadata.", 13 )    ||
	     EQUALN( szAttrName, "archivemetadata.", 16 ) ||
	     EQUALN( szAttrName, "productmetadata.", 16 ) ||
             EQUALN( szAttrName, "badpixelinformation", 19 ) ||
	     EQUALN( szAttrName, "product_summary", 15 ) ||
	     EQUALN( szAttrName, "dem_specific", 12 ) ||
	     EQUALN( szAttrName, "bts_specific", 12 ) ||
	     EQUALN( szAttrName, "etse_specific", 13 ) ||
	     EQUALN( szAttrName, "dst_specific", 12 ) ||
	     EQUALN( szAttrName, "acv_specific", 12 ) ||
	     EQUALN( szAttrName, "act_specific", 12 ) ||
	     EQUALN( szAttrName, "etst_specific", 13 ) ||
	     EQUALN( szAttrName, "level_1_carryover", 17 ) )
        {
            papszGlobalMetadata = TranslateHDF4EOSAttributes( iHandler,
		iAttribute, nValues, papszGlobalMetadata );
        }
	else if ( EQUALN( szAttrName, "structmetadata.", 15 ) )
        {
            HDF4EOSParseStructMetadata( iHandler, iAttribute, nValues );
        }
        else
        {
	    papszGlobalMetadata = TranslateHDF4Attributes( iHandler,
		iAttribute, szAttrName,	iNumType, nValues, papszGlobalMetadata );
        }
    }
    return CE_None;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *HDF4Dataset::Open( GDALOpenInfo * poOpenInfo )

{
    int		i;
    
    if( poOpenInfo->fp == NULL )
        return NULL;

    // We have special routine in the HDF library for format checking!
    if ( !Hishdf(poOpenInfo->pszFilename) )
	return NULL;

/* -------------------------------------------------------------------- */
/*      Try opening the dataset.                                        */
/* -------------------------------------------------------------------- */
    int32	hHDF4;
    
    hHDF4 = Hopen(poOpenInfo->pszFilename, DFACC_READ, 0);
    
    if( hHDF4 <= 0 )
        return( NULL );

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    HDF4Dataset 	*poDS;

    poDS = new HDF4Dataset();

    poDS->fp = poOpenInfo->fp;
    poOpenInfo->fp = NULL;
    
/* -------------------------------------------------------------------- */
/*          Open HDF SDS Interface.                                     */
/* -------------------------------------------------------------------- */
    poDS->hSD = SDstart( poOpenInfo->pszFilename, DFACC_READ );
    if ( poDS->hSD == -1 )
    {
	delete poDS;
        return NULL;
    }
   
/* -------------------------------------------------------------------- */
/*		Now read Global Attributes.				*/
/* -------------------------------------------------------------------- */
    if ( poDS->ReadGlobalAttributes( poDS->hSD ) != CE_None )
    {
	delete poDS;
        return NULL;
    }

    poDS->SetMetadata( poDS->papszGlobalMetadata, "" );

/* -------------------------------------------------------------------- */
/*		Determine type of file we read.				*/
/* -------------------------------------------------------------------- */
    const char	*pszValue;
    
    if ( (pszValue = CSLFetchNameValue(poDS->papszGlobalMetadata, "Signature"))
	 && EQUAL( pszValue, pszGDALSignature ) )
    {
	poDS->iDataType = GDAL_HDF4;
	poDS->pszDataType = "GDAL_HDF4";
    }
    else if ( (pszValue = CSLFetchNameValue(poDS->papszGlobalMetadata, "SHORTNAME")) )
    {
	if ( EQUAL( pszValue, "ASTL1A" ) )
        {
            poDS->iDataType = ASTER_L1A;
            poDS->pszDataType = "ASTER_L1A";
        }
        else if ( EQUAL( pszValue, "ASTL1B" ) )
        {
            poDS->iDataType = ASTER_L1B;
	    poDS->pszDataType = "ASTER_L1B";
        }
        else if ( EQUAL( pszValue, "AST_04" )
                  || EQUAL( pszValue, "AST_05" )
                  || EQUAL( pszValue, "AST_06VD" )
                  || EQUAL( pszValue, "AST_06SD" )
                  || EQUAL( pszValue, "AST_06TD" )
                  || EQUAL( pszValue, "AST_07" )
                  || EQUAL( pszValue, "AST_08" )
                  || EQUAL( pszValue, "AST_09" )
                  || EQUAL( pszValue, "AST_09T" ) )
        {
            poDS->iDataType = ASTER_L2;
            poDS->pszDataType = "ASTER_L2";
        }
        else if ( EQUAL( pszValue, "AST14DEM" ) )
        {
            poDS->iDataType = AST14DEM;
            poDS->pszDataType = "AST14DEM";
        }
        else if ( EQUAL( pszValue, "GSUB1" )
            // L1B EV 1km
            || EQUAL( pszValue, "MOD021KM" ) || EQUAL( pszValue, "MYD021KM" )
            // L1B EV 500m
            || EQUAL( pszValue, "MOD02HKM" ) || EQUAL( pszValue, "MYD02HKM" )
            // L1B EV 250m
            || EQUAL( pszValue, "MOD02QKM" ) || EQUAL( pszValue, "MYD02QKM" ) ) 
        {
            poDS->iDataType = MODIS_L1B;
            poDS->pszDataType = "MODIS_L1B";
        }
        else if ( strlen( pszValue ) == 8
                  && (EQUALN( pszValue, "MO", 2 ) || EQUALN( pszValue, "MY", 2 ))
                  && (EQUALN( pszValue + 2, "04", 2 )
                      || EQUALN( pszValue + 2, "36", 2 )
                      || EQUALN( pszValue + 2, "1D", 2 ))
                  && (*(pszValue + 4) == 'M' || *(pszValue + 4) == 'S'
                      || *(pszValue + 4) == 'N' || *(pszValue + 4) == 'Q'
                      || *(pszValue + 4) == 'F' || *(pszValue + 4) == '1'
                      || *(pszValue + 4) == '2' || *(pszValue + 4) == '3')
                  && (*(pszValue + 5) == 'D' || *(pszValue + 5) == 'W'
                      || *(pszValue + 5) == 'M' || *(pszValue + 5) == 'N') )

        {
            poDS->iDataType = MODIS_L3;
            poDS->pszDataType = "MODIS_L3";
        }
        else if ( EQUAL( pszValue, "MODOCL2" )
                  || EQUAL( pszValue, "MYDOCL2" )
                  || EQUAL( pszValue, "MODOCL2A" )
                  || EQUAL( pszValue, "MYDOCL2A" )
                  || EQUAL( pszValue, "MODOCL2B" )
                  || EQUAL( pszValue, "MYDOCL2B" )
                  || EQUAL( pszValue, "MODOCQC" )
                  || EQUAL( pszValue, "MYDOCQC" )
                  || EQUAL( pszValue, "MOD28L2" )
                  || EQUAL( pszValue, "MYD28L2" )
                  || EQUAL( pszValue, "MOD28QC" )
                  || EQUAL( pszValue, "MYD28QC" ) )

        {
            poDS->iDataType = MODIS_L2;
            poDS->pszDataType = "MODIS_L2";
        }
        else
        {
            poDS->iDataType = UNKNOWN;
            poDS->pszDataType = "UNKNOWN";
        }
    }
    else if ( (pszValue = CSLFetchNameValue(poDS->papszGlobalMetadata, "Title"))
	 && EQUAL( pszValue, "SeaWiFS Level-1A Data" ) )
    {
	poDS->iDataType = SEAWIFS_L1A;
	poDS->pszDataType = "SEAWIFS_L1A";
    }
    else if ( (pszValue = CSLFetchNameValue(poDS->papszGlobalMetadata, "Title"))
	&& EQUAL( pszValue, "SeaWiFS Level-2 Data" ) )
    {
	poDS->iDataType = SEAWIFS_L2;
	poDS->pszDataType = "SEAWIFS_L2";
    }
    else if ( (pszValue = CSLFetchNameValue(poDS->papszGlobalMetadata, "Title"))
	&& EQUAL( pszValue, "SeaWiFS Level-3 Standard Mapped Image" ) )
    {
	poDS->iDataType = SEAWIFS_L3;
	poDS->pszDataType = "SEAWIFS_L3";
    }
    else if ( (pszValue = CSLFetchNameValue(poDS->papszGlobalMetadata,
                                            "L1 File Generated By"))
	&& EQUALN( pszValue, "HYP version ", 12 ) )
    {
	poDS->iDataType = HYPERION_L1;
	poDS->pszDataType = "HYPERION_L1";
    }
    else
    {
	poDS->iDataType = UNKNOWN;
	poDS->pszDataType = "UNKNOWN";
    }

/* -------------------------------------------------------------------- */
/*  Make a list of subdatasets from SDSs contained in input HDF file.	*/
/* -------------------------------------------------------------------- */
    char	szName[VSNAMELENMAX + 1], szTemp[1024];
    const char  *pszName;
    char	*pszString;
    int32	iSDS;
    int32	iRank; 			// Number of dimensions in the SDS
    int32	iNumType, nAttrs;
    int32	aiDimSizes[MAX_VAR_DIMS];
    int		nCount;

    for ( i = 0; i < poDS->nDatasets; i++ )
    {
	iSDS = SDselect( poDS->hSD, i );
	if ( SDgetinfo( iSDS, szName, &iRank, aiDimSizes, &iNumType, &nAttrs) != 0 )
	    return NULL;
	
	if ( iRank == 1 )		// Skip 1D datsets
		continue;

	// Sort known datasets. We will display only image bands
	if ( (poDS->iDataType == ASTER_L1A || poDS->iDataType == ASTER_L1B )
             && !EQUALN( szName, "ImageData", 9 ) )
		continue;
	else if ( (poDS->iDataType == AST14DEM || poDS->iDataType == ASTER_L2 )
                  && !EQUALN( szName, "Band", 4 )
                  && !EQUALN( szName, "QA_DataPlane", 12 )
                  && !EQUALN( szName, "KineticTemperature", 18 ) )
		continue;
	else if ( (poDS->iDataType == MODIS_L1B ) && !EQUALN( szName, "EV_", 3 ) )
        {
		continue;
        }
	else if ( poDS->iDataType == MODIS_L2 || poDS->iDataType == MODIS_L3 )
        {
            // MODIS Ocean Parameters
            if ( EQUALN( szName, "nLw_412", 7 ) )
                pszName =
                    CPLSPrintf( "%s: Normalized water-leaving radiance at 412 nm",
                                szName );
            else if ( EQUALN( szName, "nLw_443", 7 ) )
                pszName =
                    CPLSPrintf( "%s: Normalized water-leaving radiance at 443 nm",
                                szName );
            else if ( EQUALN( szName, "nLw_488", 7 ) )
                pszName =
                    CPLSPrintf( "%s: Normalized water-leaving radiance at 488 nm",
                                szName );
            else if ( EQUALN( szName, "nLw_531", 7 ) )
                pszName =
                    CPLSPrintf( "%s: Normalized water-leaving radiance at 531 nm",
                                szName );
            else if ( EQUALN( szName, "nLw_551", 7 ) )
                pszName =
                    CPLSPrintf( "%s: Normalized water-leaving radiance at 551 nm",
                                szName );
            else if ( EQUALN( szName, "nLw_667", 7 ) )
                pszName =
                    CPLSPrintf( "%s: Normalized water-leaving radiance at 667 nm",
                                szName );
            else if ( EQUALN( szName, "nLw_678", 7 ) )
                pszName =
                    CPLSPrintf( "%s: Normalized water-leaving radiance at 678 nm",
                                szName );
            else if ( EQUALN( szName, "Tau_865", 7 ) )
                pszName =
                    CPLSPrintf( "%s: Aerosol optical thickness, 865 nm",
                                szName );
            else if ( EQUALN( szName, "Eps_78", 6 ) )
                pszName =
                    CPLSPrintf( "%s: Epsilon of aerosol correction, 765 & 865 nm",
                                szName );
            else if ( EQUALN( szName, "aer_model1", 10 ) )
                pszName =
                    CPLSPrintf( "%s: Aerosol model identification number 1",
                                szName );
            else if ( EQUALN( szName, "aer_model2", 10 ) )
                pszName =
                    CPLSPrintf( "%s: Aerosol model identification number 2",
                                szName );
            else if ( EQUALN( szName, "eps_clr_water", 13 ) )
                pszName =
                    CPLSPrintf( "%s: Epsilon of clear water aerosol correction, 531 & 667 nm",
                                szName );
            else if ( EQUALN( szName, "CZCS_pigment", 12 ) )
                pszName =
                    CPLSPrintf( "%s: Chlorophyll-a + phaeopigment, fluorometric, empirical",
                                szName );
            else if ( EQUALN( szName, "chlor_MODIS", 11 ) )
                pszName =
                    CPLSPrintf( "%s: Chlorophyll-a concentration, HPLC, empirical",
                                szName );
            else if ( EQUALN( szName, "pigment_c1_total", 16 ) )
                pszName =
                    CPLSPrintf( "%s: Total pigment concentration, HPLC, empirical",
                                szName );
            else if ( EQUALN( szName, "chlor_fluor_ht", 14 ) )
                pszName =
                    CPLSPrintf( "%s: Chlorophyll fluorescence line height",
                                szName );
            else if ( EQUALN( szName, "chlor_fluor_base", 16 ) )
                pszName =
                    CPLSPrintf( "%s: Chlorophyll fluorescence baseline",
                                szName );
            else if ( EQUALN( szName, "chlor_fluor_effic", 17 ) )
                pszName =
                    CPLSPrintf( "%s: Chlorophyll fluorescence efficiency",
                                szName );
            else if ( EQUALN( szName, "susp_solids_conc", 16 ) )
                pszName =
                    CPLSPrintf( "%s: Total suspended matter concentration in ocean",
                                szName );
            else if ( EQUALN( szName, "cocco_pigmnt_conc", 17 ) )
                pszName =
                    CPLSPrintf( "%s: Pigment concentration in voccolithophore blooms",
                                szName );
            else if ( EQUALN( szName, "cocco_conc_detach", 17 ) )
                pszName =
                    CPLSPrintf( "%s: Detached coccolithophore concentration",
                                szName );
            else if ( EQUALN( szName, "calcite_conc", 12 ) )
                pszName = CPLSPrintf( "%s: Calcite concentration", szName );
            else if ( EQUALN( szName, "K_490", 5 ) )
                pszName =
                    CPLSPrintf( "%s: Diffuse attenuation coefficient at 490 nm",
                                szName );
            else if ( EQUALN( szName, "phycoeryth_conc", 15 ) )
                pszName =
                    CPLSPrintf( "%s: Phycoerythrobilin concentration", szName );
            else if ( EQUALN( szName, "phycou_conc", 11 ) )
                pszName =
                    CPLSPrintf( "%s: Phycourobilin concentration", szName );
            else if ( EQUALN( szName, "chlor_a_2", 9 ) )
                pszName =
                    CPLSPrintf( "%s: Chlorophyll-a concentration, SeaWiFS analog - OC3M",
                                szName );
            else if ( EQUALN( szName, "chlor_a_3", 9 ) )
                pszName =
                    CPLSPrintf( "%s: Chlorophyll-a concentration, semianalytic",
                                szName );
            else if ( EQUALN( szName, "ipar", 4 ) )
                pszName =
                    CPLSPrintf( "%s: Instantaneous photosynthetically available radiation",
                                szName );
            else if ( EQUALN( szName, "arp", 3 ) )
                pszName =
                    CPLSPrintf( "%s: Instantaneous absorbed radiation by phytoplankton for fluorescence",
                                szName );
            else if ( EQUALN( szName, "absorp_coef_gelb", 16 ) )
                pszName =
                    CPLSPrintf( "%s: Gelbstoff absorption coefficient at 400 nm",
                                szName );
            else if ( EQUALN( szName, "chlor_absorb", 12 ) )
                pszName =
                    CPLSPrintf( "%s: Phytoplankton absorption coefficient at 675 nm",
                                szName );
            else if ( EQUALN( szName, "tot_absorb_412", 14 ) )
                pszName =
                    CPLSPrintf( "%s: Total absorption coefficient, 412 nm",
                                szName );
            else if ( EQUALN( szName, "tot_absorb_443", 14 ) )
                pszName =
                    CPLSPrintf( "%s: Total absorption coefficient, 443 nm",
                                szName );
            else if ( EQUALN( szName, "tot_absorb_488", 14 ) )
                pszName =
                    CPLSPrintf( "%s: Total absorption coefficient, 488 nm",
                                szName );
            else if ( EQUALN( szName, "tot_absorb_531", 14 ) )
                pszName =
                    CPLSPrintf( "%s: Total absorption coefficient, 531 nm",
                                szName );
            else if ( EQUALN( szName, "tot_absorb_551", 14 ) )
                pszName =
                    CPLSPrintf( "%s: Total absorption coefficient, 551 nm",
                                szName );
            /* XXX: 'sst4' should go before 'sst' case */
            else if ( EQUALN( szName, "sst4", 4 ) )
                pszName =
                    CPLSPrintf( "%s: Sea surface temperature, daytime, 4 micron",
                                szName );
            else if ( EQUALN( szName, "sst", 3 ) )
                pszName =
                    CPLSPrintf( "%s: Sea surface temperature, daytime, 11 micron",
                                szName );
            else
		pszName = szName;
        }
	else if ( (poDS->iDataType == SEAWIFS_L1A ) &&
		  !EQUALN( szName, "l1a_data", 8 ) )
		continue;
        else
            pszName = szName;
	
	// Add datasets with multiple dimensions to the list of GDAL subdatasets
        nCount = CSLCount( poDS->papszSubDatasets ) / 2;
        sprintf( szTemp, "SUBDATASET_%d_NAME", nCount + 1 );
	// We will use SDS index as an identificator, because SDS names
	// are not unique. Filename also needed for further file opening
        poDS->papszSubDatasets = CSLSetNameValue(poDS->papszSubDatasets, szTemp, 
              CPLSPrintf( "HDF4_SDS:%s:\"%s\":%d", poDS->pszDataType,
			  poOpenInfo->pszFilename, i) );
        sprintf( szTemp, "SUBDATASET_%d_DESC", nCount + 1 );
	pszString = SPrintArray( GDT_UInt32, aiDimSizes, iRank, "x" );
        poDS->papszSubDatasets = CSLSetNameValue(poDS->papszSubDatasets, szTemp,
	    CPLSPrintf( "[%s] %s (%s)", pszString,
		        pszName, poDS->GetDataTypeName(iNumType)) );
	CPLFree( pszString );

	SDendaccess( iSDS );
    }
    SDend( poDS->hSD );

/* -------------------------------------------------------------------- */
/*              The same list builds for raster images.                 */
/* -------------------------------------------------------------------- */
    int32	iGR;
    int32	iInterlaceMode; 

    poDS->hGR = GRstart( hHDF4 );
    if ( poDS->hGR == -1 )
    {fprintf(stderr,"GRstart failed\n");}

    if ( GRfileinfo( poDS->hGR, &poDS->nImages, &nAttrs ) != 0 )
	return NULL;
   
    for ( i = 0; i < poDS->nImages; i++ )
    {
	iGR = GRselect( poDS->hGR, i );
	// iRank in GR interface has another meaning. It represents number
	// of samples per pixel. aiDimSizes has only two dimensions.
	if ( GRgetiminfo( iGR, szName, &iRank, &iNumType, &iInterlaceMode,
			  aiDimSizes, &nAttrs ) != 0 )
	    return NULL;
        nCount = CSLCount( poDS->papszSubDatasets ) / 2;
        sprintf( szTemp, "SUBDATASET_%d_NAME", nCount + 1 );
        poDS->papszSubDatasets = CSLSetNameValue(poDS->papszSubDatasets, szTemp,
              CPLSPrintf( "HDF4_GR:UNKNOWN:\"%s\":%d", poOpenInfo->pszFilename, i));
        sprintf( szTemp, "SUBDATASET_%d_DESC", nCount + 1 );
	pszString = SPrintArray( GDT_UInt32, aiDimSizes, 2, "x" );
        poDS->papszSubDatasets = CSLSetNameValue(poDS->papszSubDatasets, szTemp,
              CPLSPrintf( "[%sx%d] %s (%s)", pszString,
		          iRank, szName, poDS->GetDataTypeName(iNumType)) );
	CPLFree( pszString );

	GRendaccess( iGR );
    }
    GRend( poDS->hGR );
    
    poDS->nRasterXSize = poDS->nRasterYSize = 512; // XXX: bogus values
    
    Hclose( hHDF4 );

/* -------------------------------------------------------------------- */
/*      If we have single subdataset only, open it immediately          */
/* -------------------------------------------------------------------- */
    if ( CSLCount( poDS->papszSubDatasets ) / 2 == 1 )
    {
	char *pszSDSName;
	pszSDSName = CPLStrdup( CSLFetchNameValue( poDS->papszSubDatasets,
				        "SUBDATASET_1_NAME" ));
	delete poDS;
	poDS = (HDF4Dataset *) GDALOpen( pszSDSName, GA_ReadOnly );
	CPLFree( pszSDSName );
    }

    return( poDS );
}

/************************************************************************/
/*                        GDALRegister_HDF4()				*/
/************************************************************************/

void GDALRegister_HDF4()

{
    GDALDriver	*poDriver;

    if( GDALGetDriverByName( "HDF4" ) == NULL )
    {
        poDriver = new GDALDriver();
        
        poDriver->SetDescription( "HDF4" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, 
                                   "Hierarchical Data Format Release 4" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, 
                                   "frmt_hdf4.html" );

        poDriver->pfnOpen = HDF4Dataset::Open;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

