/******************************************************************************
 * $Id$
 *
 * Project:  Hierarchical Data Format Release 4 (HDF4) Reader
 * Purpose:  HDF4 Datasets. Open HDF4 file, fetch metadata and list of
 *           subdatasets.
 *           This driver initially based on code supplied by Markus Neteler
 * Author:   Andrey Kiselev, dron@at1895.spb.edu
 *
 ******************************************************************************
 * Copyright (c) 2002, Andrey Kiselev <a_kissel@eudoramail.com>
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
 * Revision 1.1  2002/07/16 11:04:11  dron
 * New driver: HDF4 datasets. Initial version.
 *
 *
 */

#include <hdf/hdf.h>
#include <hdf/mfhdf.h>

#include "gdal_priv.h"
#include "cpl_string.h"
#include "hdf4dataset.h"

CPL_CVSID("$Id$");

static GDALDriver	*poHDF4Driver = NULL;

CPL_C_START
void	GDALRegister_HDF4(void);
CPL_C_END

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
    pszDataType = "UNKNOWN";
    papszSubDatasets = NULL;
}

/************************************************************************/
/*                            ~HDF4Dataset()                         */
/************************************************************************/

HDF4Dataset::~HDF4Dataset()

{
    CSLDestroy( papszSubDatasets );
    if( fp != NULL )
        VSIFClose( fp );
    if( hHDF4 > 0 )
	Hclose(hHDF4);
}

/************************************************************************/
/*                            GetMetadata()                             */
/************************************************************************/

char **HDF4Dataset::GetMetadata( const char *pszDomain )

{
    if( pszDomain != NULL && EQUAL( pszDomain,"SUBDATASETS" ) )
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
char *SPrintArray(signed char *piaDataArray, int nValues, char * pszDelimiter)
{
    char *pszString, *pszField;
    int i, iFieldSize, iStringSize;
    
    iFieldSize = 4 + strlen( pszDelimiter );
    pszField = (char *)CPLMalloc( iFieldSize + 1 );
    iStringSize = nValues * iFieldSize + 1;
    pszString = (char *)CPLMalloc( iStringSize );
    memset( pszString, 0, iStringSize );
    for ( i = 0; i < nValues; i++ )
    {
        sprintf( pszField, "%d%s", piaDataArray[i], (i < nValues - 1)?pszDelimiter:"" );
        strcat( pszString, pszField );
    }
    
    return pszString;
}

char *SPrintArray(GByte *piaDataArray, int nValues, char * pszDelimiter)
{
    char *pszString, *pszField;
    int i, iFieldSize, iStringSize;
    
    iFieldSize = 4 + strlen( pszDelimiter );
    pszField = (char *)CPLMalloc( iFieldSize + 1 );
    iStringSize = nValues * iFieldSize + 1;
    pszString = (char *)CPLMalloc( iStringSize );
    memset( pszString, 0, iStringSize );
    for ( i = 0; i < nValues; i++ )
    {
        sprintf( pszField, "%d%s", piaDataArray[i], (i < nValues - 1)?pszDelimiter:"" );
        strcat( pszString, pszField );
    }
    
    return pszString;
}

char *SPrintArray(GInt16 *piaDataArray, int nValues, char * pszDelimiter)
{
    char *pszString, *pszField;
    int i, iFieldSize, iStringSize;
    
    iFieldSize = 6 + strlen( pszDelimiter );
    pszField = (char *)CPLMalloc( iFieldSize + 1 );
    iStringSize = nValues * iFieldSize + 1;
    pszString = (char *)CPLMalloc( iStringSize );
    memset( pszString, 0, iStringSize );
    for ( i = 0; i < nValues; i++ )
    {
        sprintf( pszField, "%d%s", piaDataArray[i], (i < nValues - 1)?pszDelimiter:"" );
        strcat( pszString, pszField );
    }
    
    return pszString;
}

char *SPrintArray(GUInt16 *piaDataArray, int nValues, char * pszDelimiter)
{
    char *pszString, *pszField;
    int i, iFieldSize, iStringSize;
    
    iFieldSize = 6 + strlen( pszDelimiter );
    pszField = (char *)CPLMalloc( iFieldSize + 1 );
    iStringSize = nValues * iFieldSize + 1;
    pszString = (char *)CPLMalloc( iStringSize );
    memset( pszString, 0, iStringSize );
    for ( i = 0; i < nValues; i++ )
    {
        sprintf( pszField, "%d%s", piaDataArray[i], (i < nValues - 1)?pszDelimiter:"" );
        strcat( pszString, pszField );
    }
    
    return pszString;
}

char *SPrintArray(GInt32 *piaDataArray, int nValues, char * pszDelimiter)
{
    char *pszString, *pszField;
    int i, iFieldSize, iStringSize;
    
    iFieldSize = 11 + strlen( pszDelimiter );
    pszField = (char *)CPLMalloc( iFieldSize + 1 );
    iStringSize = nValues * iFieldSize + 1;
    pszString = (char *)CPLMalloc( iStringSize );
    memset( pszString, 0, iStringSize );
    for ( i = 0; i < nValues; i++ )
    {
        sprintf( pszField, "%d%s", piaDataArray[i], (i < nValues - 1)?pszDelimiter:"" );
        strcat( pszString, pszField );
    }
    
    return pszString;
}

char *SPrintArray(GUInt32 *piaDataArray, int nValues, char * pszDelimiter)
{
    char *pszString, *pszField;
    int i, iFieldSize, iStringSize;
    
    iFieldSize = 11 + strlen( pszDelimiter );
    pszField = (char *)CPLMalloc( iFieldSize + 1 );
    iStringSize = nValues * iFieldSize + 1;
    pszString = (char *)CPLMalloc( iStringSize );
    memset( pszString, 0, iStringSize );
    for ( i = 0; i < nValues; i++ )
    {
        sprintf( pszField, "%d%s", piaDataArray[i], (i < nValues - 1)?pszDelimiter:"" );
        strcat( pszString, pszField );
    }
    
    return pszString;
}

char *SPrintArray(GIntBig *piaDataArray, int nValues, char * pszDelimiter)
{
    char *pszString, *pszField;
    int i, iFieldSize, iStringSize;
    
    iFieldSize = 11 + strlen( pszDelimiter ); //FIXME
    pszField = (char *)CPLMalloc( iFieldSize + 1 );
    iStringSize = nValues * iFieldSize + 1;
    pszString = (char *)CPLMalloc( iStringSize );
    memset( pszString, 0, iStringSize );
    for ( i = 0; i < nValues; i++ )
    {
        sprintf( pszField, "%Ld%s", piaDataArray[i], (i < nValues - 1)?pszDelimiter:"" );
        strcat( pszString, pszField );
    }
    
    return pszString;
}

char *SPrintArray(GUIntBig *piaDataArray, int nValues, char * pszDelimiter)
{
    char *pszString, *pszField;
    int i, iFieldSize, iStringSize;
    
    iFieldSize = 11 + strlen( pszDelimiter ); //FIXME
    pszField = (char *)CPLMalloc( iFieldSize + 1 );
    iStringSize = nValues * iFieldSize + 1;
    pszString = (char *)CPLMalloc( iStringSize );
    memset( pszString, 0, iStringSize );
    for ( i = 0; i < nValues; i++ )
    {
        sprintf( pszField, "%Ld%s", piaDataArray[i], (i < nValues - 1)?pszDelimiter:"" );
        strcat( pszString, pszField );
    }
    
    return pszString;
}

char *SPrintArray(float *pfaDataArray, int nValues, char * pszDelimiter)
{
    char *pszString, *pszField;
    int i, iFieldSize, iStringSize;
    
    iFieldSize = 13 + strlen( pszDelimiter );
    pszField = (char *)CPLMalloc( iFieldSize + 1 );
    iStringSize = nValues * iFieldSize + 1;
    pszString = (char *)CPLMalloc( iStringSize );
    memset( pszString, 0, iStringSize );
    for ( i = 0; i < nValues; i++ )
    {
        sprintf( pszField, "%.7g%s", pfaDataArray[i], (i < nValues - 1)?pszDelimiter:"" );
        strcat( pszString, pszField );
    }
    
    return pszString;
}

char *SPrintArray(double *pdfaDataArray, int nValues, char * pszDelimiter)
{
    char *pszString, *pszField;
    int i, iFieldSize, iStringSize;
    
    iFieldSize = 22 + strlen( pszDelimiter );
    pszField = (char *)CPLMalloc( iFieldSize + 1 );
    iStringSize = nValues * iFieldSize + 1;
    pszString = (char *)CPLMalloc( iStringSize );
    memset( pszString, 0, iStringSize );
    for ( i = 0; i < nValues; i++ )
    {
        sprintf( pszField, "%.15g%s", pdfaDataArray[i], (i < nValues - 1)?pszDelimiter:"" );
        strcat( pszString, pszField );
    }
    
    return pszString;
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
	break;
	case DFNT_UCHAR8: // The same as DFNT_UCHAR
	return "8-bit unsigned character";
	break;
        case DFNT_INT8:
	return "8-bit integer";
	break;
        case DFNT_UINT8:
	return "8-bit unsigned integer";
	break;
        case DFNT_INT16:
	return "16-bit integer";
	break;
        case DFNT_UINT16:
	return "16-bit unsigned integer";
	break;
        case DFNT_INT32:
	return "32-bit integer";
	break;
        case DFNT_UINT32:
	return "32-bit unsigned integer";
	break;
        case DFNT_INT64:
	return "64-bit integer";
	break;
        case DFNT_UINT64:
	return "64-bit unsigned integer";
	break;
        case DFNT_FLOAT32:
	return "32-bit floating-point";
	break;
        case DFNT_FLOAT64:
	return "64-bit floating-point";
	break;
	default:
	return "unknown type";
	break;
    }
}
	
/************************************************************************/
/*         Translate HDF4-EOS attributes in GDAL metadata items         */
/************************************************************************/
void HDF4Dataset::TranslateHDF4EOSAttributes( int32 iHandle, int32 iAttribute, int32 nValues )
{
    char	*pszData;
    
    pszData = (char *)CPLMalloc( nValues * sizeof(char) );
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
    // other objects, groups contains other groups and objects. Names of
    // groups and objects are not unique and may repeats.
    // Objects may contains other types of records.
    //
    // We are interested in OBJECTS structures only.

    char *pszDelimiters = " \t\n\r";
    char *pszAttrName, *pszAttrValue;
    char **papszAttrList;
    int iCount, i, j;
    
    papszAttrList = CSLTokenizeString2( pszData, pszDelimiters, CSLT_HONOURSTRINGS );
    iCount = CSLCount( papszAttrList );
    for ( i = 0; i < iCount - 2; i++ )
    {
	if ( EQUAL( papszAttrList[i], "OBJECT" ) )
	{
	    i += 2;
	    pszAttrName = papszAttrList[i];
	    for ( j = 1; i + j < iCount - 2; j++ )
	    {
	        if ( EQUAL( papszAttrList[i + j], "END_OBJECT" ) )
	            break;
	        else if ( EQUAL( papszAttrList[i + j], "VALUE" ) )
	        {
                    // TODO: Lists processing
		    i += j;
		    i += 2;
	            pszAttrValue = papszAttrList[i];
	            SetMetadataItem( pszAttrName, pszAttrValue );
		    break;
	        }
	    }
	}
    }
}

/************************************************************************/
/*         Translate HDF4 attributes in GDAL metadata items             */
/************************************************************************/
void HDF4Dataset::TranslateHDF4Attributes( int32 iHandle, int32 iAttribute,
		        char *pszAttrName, int32 iNumType, int32 nValues )
{
    int8	*pbData;
    
    // Allocate a buffer to hold the attribute data.
    switch (iNumType)
    {
        case DFNT_CHAR8: // The same as DFNT_CHAR
	pbData = (int8 *)CPLMalloc( nValues * sizeof(char8) );
	break;
	case DFNT_UCHAR8: // The same as DFNT_UCHAR
	pbData = (int8 *)CPLMalloc( nValues * sizeof(uchar8) );
	break;
        case DFNT_INT8:
	pbData = (int8 *)CPLMalloc( nValues * sizeof(int8) );
	break;
        case DFNT_UINT8:
	pbData = (int8 *)CPLMalloc( nValues * sizeof(uint8) );
	break;
        case DFNT_INT16:
	pbData = (int8 *)CPLMalloc( nValues * sizeof(int16) );
	break;
        case DFNT_UINT16:
	pbData = (int8 *)CPLMalloc( nValues * sizeof(uint16) );
	break;
        case DFNT_INT32:
	pbData = (int8 *)CPLMalloc( nValues * sizeof(int32) );
	break;
        case DFNT_UINT32:
	pbData = (int8 *)CPLMalloc( nValues * sizeof(uint32) );
	break;
        case DFNT_INT64:
	pbData = (int8 *)CPLMalloc( nValues * sizeof(long long) );
	break;
        case DFNT_UINT64:
	pbData = (int8 *)CPLMalloc( nValues * sizeof(unsigned long long) );
	break;
        case DFNT_FLOAT32:
	pbData = (int8 *)CPLMalloc( nValues * sizeof(float) );
	break;
        case DFNT_FLOAT64:
	pbData = (int8 *)CPLMalloc( nValues * sizeof(float64) );
	break;
	default:
	break;
    }
    // Read the file attribute data.
    SDreadattr( iHandle, iAttribute, pbData );
    switch (iNumType)
    {
        case DFNT_CHAR8: // The same as DFNT_CHAR
        SetMetadataItem( pszAttrName, pbData );
	break;
	case DFNT_UCHAR8: // The same as DFNT_UCHAR
        SetMetadataItem( pszAttrName, pbData );
	break;
        case DFNT_INT8:
        SetMetadataItem( pszAttrName, SPrintArray((signed char *)pbData, nValues, ", ") );
	break;
        case DFNT_UINT8:
        SetMetadataItem( pszAttrName, SPrintArray((GByte *)pbData, nValues, ", ") );
	break;
        case DFNT_INT16:
        SetMetadataItem( pszAttrName, SPrintArray((GInt16 *)pbData, nValues, ", ") );
	break;
        case DFNT_UINT16:
        SetMetadataItem( pszAttrName, SPrintArray((GUInt16 *)pbData, nValues, ", ") );
	break;
        case DFNT_INT32:
        SetMetadataItem( pszAttrName, SPrintArray((GInt32 *)pbData, nValues, ", ") );
	break;
        case DFNT_UINT32:
        SetMetadataItem( pszAttrName, SPrintArray((GUInt32 *)pbData, nValues, ", ") );
	break;
        case DFNT_INT64:
        SetMetadataItem( pszAttrName, SPrintArray((GIntBig *)pbData, nValues, ", ") );
	break;
        case DFNT_UINT64:
        SetMetadataItem( pszAttrName, SPrintArray((GUIntBig *)pbData, nValues, ", ") );
	break;
        case DFNT_FLOAT32:
        SetMetadataItem( pszAttrName, SPrintArray((float *)pbData, nValues, ", ") );
	break;
        case DFNT_FLOAT64:
        SetMetadataItem( pszAttrName, SPrintArray((double *)pbData, nValues, ", ") );
	break;
	default:
	break;
    }
    
    CPLFree(pbData);
}
/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *HDF4Dataset::Open( GDALOpenInfo * poOpenInfo )

{
    int		i;
    
    if( poOpenInfo->fp == NULL /*|| poOpenInfo->nHeaderBytes < 200*/ )
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

    poDS = new HDF4Dataset( );

    poDS->poDriver = poHDF4Driver;
    poDS->fp = poOpenInfo->fp;
    poOpenInfo->fp = NULL;
    
/* -------------------------------------------------------------------- */
/*          Open HDF SDS Interface.                                     */
/* -------------------------------------------------------------------- */
    poDS->hSD = SDstart( poOpenInfo->pszFilename, DFACC_READ );
    if ( poDS->hSD == -1 )
        return NULL;
   
/* -------------------------------------------------------------------- */
/*     Obtain number of SDSsand global attributes in input file.        */
/* -------------------------------------------------------------------- */
    if ( SDfileinfo( poDS->hSD, &poDS->nDatasets, &poDS->nFileAttrs ) != 0 )
	return NULL;

/* -------------------------------------------------------------------- */
/*		Now read Global Attributes.				*/
/* -------------------------------------------------------------------- */
    int32	iAttribute, nValues, iNumType, nAttrs;
    char	szAttrName[MAX_NC_NAME];

    // Loop trough the all global file attributes
    for ( iAttribute = 0; iAttribute < poDS->nFileAttrs; iAttribute++ )
    {
        // Get information about the file attribute. Note that the first
        // parameter is an SD interface identifier.
        SDattrinfo( poDS->hSD, iAttribute, szAttrName, &iNumType, &nValues );
        if ( EQUAL( szAttrName, "coremetadata.0" )    ||
	     EQUAL( szAttrName, "archivemetadata.0" ) ||
	     EQUALN( szAttrName, "productmetadata.", 16 ) )
            poDS->TranslateHDF4EOSAttributes( poDS->hSD, iAttribute, nValues );
	else if ( !EQUAL( szAttrName, "structmetadata.0" ) ) // Not interesting
	    poDS->TranslateHDF4Attributes( poDS->hSD, iAttribute,
			                   szAttrName, iNumType, nValues );
     }

/* -------------------------------------------------------------------- */
/*		Determine type of file we read.				*/
/* -------------------------------------------------------------------- */
    const char	*pszValue;
    
    pszValue = poDS->GetMetadataItem( "Title", 0 );
    if ( pszValue && EQUAL( pszValue, "SeaWiFS Level-1A Data" ) )
    {
	poDS->iDataType = SEAWIFS_L1A;
	poDS->pszDataType = "SEAWIFS_L1A";
    }
    pszValue = poDS->GetMetadataItem( "Title", 0 );
    if ( pszValue && EQUAL( pszValue, "SeaWiFS Level-2 Data" ) )
    {
	poDS->iDataType = SEAWIFS_L2;
	poDS->pszDataType = "SEAWIFS_L2";
    }
    pszValue = poDS->GetMetadataItem( "Title", 0 );
    if ( pszValue && EQUAL( pszValue, "SeaWiFS Level-3 Standard Mapped Image" ) )
    {
	poDS->iDataType = SEAWIFS_L3;
	poDS->pszDataType = "SEAWIFS_L3";
    }
    pszValue = poDS->GetMetadataItem( "ALGORITHMPACKAGENAME", 0 ); // FIXME: is it right?
    if ( pszValue && EQUAL( pszValue, "MODIS Level 1B channel subset" ) )
    {
        poDS->iDataType = MODIS_L1B;
	poDS->pszDataType = "MODIS_L1B";
    }
    pszValue = poDS->GetMetadataItem( "LONGNAME", 0 );
    if ( pszValue && EQUAL( pszValue, "MODIS/Terra Calibrated Radiances 5-Min L1B Swath 250m" ) )
    {
        poDS->iDataType = MOD02QKM_L1B;
	poDS->pszDataType = "MOD02QKM_L1B";
    }
    if( poDS->iDataType == UNKNOWN )
	poDS->pszDataType = "UNKNOWN";

/* -------------------------------------------------------------------- */
/*  Make a list of subdatasets from SDSs contained in input HDF file.	*/
/* -------------------------------------------------------------------- */
    char	szName[65], szTemp[1024];
    int32	iSDS;
    int32	iRank; // Number of dimensions in the SDS
    int32	aiDimSizes[MAX_VAR_DIMS];
    int		nCount;

    for ( i = 0; i < poDS->nDatasets; i++ )
    {
	iSDS = SDselect( poDS->hSD, i );
	if ( SDgetinfo( iSDS, szName, &iRank, aiDimSizes, &iNumType, &nAttrs) != 0 )
	    return NULL;
	
	if ( iRank == 1 )	// Skip 1D datsets
		continue;
	
	// Add datasets with multiple dimensions to list of GDAL subdatasets
        nCount = CSLCount( poDS->papszSubDatasets ) / 2;
        sprintf( szTemp, "SUBDATASET_%d_NAME", nCount+1 );
	// We will use SDS index as an identificator, because SDS names
	// are not unique. Filename also needed for further file opening
        poDS->papszSubDatasets = CSLSetNameValue( poDS->papszSubDatasets, szTemp, 
              CPLSPrintf( "HDF4_SDS:%s:%s:%d", poDS->pszDataType, poOpenInfo->pszFilename, i ) );
        sprintf( szTemp, "SUBDATASET_%d_DESC", nCount+1 );
        poDS->papszSubDatasets = CSLSetNameValue( poDS->papszSubDatasets, szTemp, 
              CPLSPrintf( "[%s] %s (%s)", SPrintArray((GInt32 *)aiDimSizes, iRank, "x"),
		          szName, poDS->GetDataTypeName(iNumType) ) );

	SDendaccess( iSDS );
    }
    SDend( poDS->hSD );

/* -------------------------------------------------------------------- */
/*              The same list builded for raster images.                */
/* -------------------------------------------------------------------- */
/*  FIXME: it won't works. Why?

    int32	iGR;
    int32	iInterlaceMode; 

    poDS->hGR = GRstart( poDS->hHDF4 );
    if ( poDS->hGR == -1 )
    {fprintf(stderr,"GRstart failed\n");return NULL;}

    if ( GRfileinfo( poDS->hGR, &poDS->nImages, &poDS->nFileAttrs ) != 0 )
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
        sprintf( szTemp, "SUBDATASET_%d_NAME", nCount+1 );
        poDS->papszSubDatasets = CSLSetNameValue( poDS->papszSubDatasets, szTemp,
              CPLSPrintf( "HDF4_GR:%s:%d", poOpenInfo->pszFilename, i ) );
        sprintf( szTemp, "SUBDATASET_%d_DESC", nCount+1 );
        poDS->papszSubDatasets = CSLSetNameValue( poDS->papszSubDatasets, szTemp,
              CPLSPrintf( "[%sx%d] %s (%s)", SPrintArray((GInt32 *)aiDimSizes, 2, "x"),
		          iRank, szName, poDS->GetDataTypeName(iNumType) ) );

	GRendaccess( iGR );
    }
    GRend( poDS->hGR );*/
    
    poDS->nRasterXSize = poDS->nRasterYSize = 512; // XXX: bogus values
    
    Hclose( hHDF4 );

    return( poDS );
}

/************************************************************************/
/*                        GDALRegister_HDF4()				*/
/************************************************************************/

void GDALRegister_HDF4()

{
    GDALDriver	*poDriver;

    if( poHDF4Driver == NULL )
    {
        poHDF4Driver = poDriver = new GDALDriver();
        
        poDriver->SetDescription( "HDF4" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, 
                                   "Hierarchical Data Format Release 4" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, 
                                   "frmt_hdf4.html" );

        poDriver->pfnOpen = HDF4Dataset::Open;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

