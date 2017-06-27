/******************************************************************************
 *
 * Project:  Hierarchical Data Format Release 4 (HDF4)
 * Purpose:  HDF4 Datasets. Open HDF4 file, fetch metadata and list of
 *           subdatasets.
 *           This driver initially based on code supplied by Markus Neteler
 * Author:   Andrey Kiselev, dron@ak4719.spb.edu
 *
 ******************************************************************************
 * Copyright (c) 2002, Andrey Kiselev <dron@ak4719.spb.edu>
 * Copyright (c) 2007-2013, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "cpl_multiproc.h"
#include "cpl_string.h"
#include "gdal_frmts.h"
#include "gdal_priv.h"

#include "hdf.h"
#include "mfhdf.h"

#include "HdfEosDef.h"

#include "hdf4compat.h"
#include "hdf4dataset.h"

CPL_CVSID("$Id$")

extern const char * const pszGDALSignature;

CPLMutex *hHDF4Mutex = NULL;

/************************************************************************/
/* ==================================================================== */
/*                              HDF4Dataset                             */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                           HDF4Dataset()                              */
/************************************************************************/

HDF4Dataset::HDF4Dataset() :
    bIsHDFEOS(false),
    hGR(0),
    hSD(0),
    nImages(0),
    iSubdatasetType(H4ST_UNKNOWN),
    pszSubdatasetType(NULL),
    papszGlobalMetadata(NULL),
    papszSubDatasets(NULL)
{}

/************************************************************************/
/*                            ~HDF4Dataset()                            */
/************************************************************************/

HDF4Dataset::~HDF4Dataset()

{
    CPLMutexHolderD(&hHDF4Mutex);

    if ( hSD )
        SDend( hSD );
    if ( hGR )
        GRend( hGR );
    if ( papszSubDatasets )
        CSLDestroy( papszSubDatasets );
    if ( papszGlobalMetadata )
        CSLDestroy( papszGlobalMetadata );
}

/************************************************************************/
/*                      GetMetadataDomainList()                         */
/************************************************************************/

char **HDF4Dataset::GetMetadataDomainList()
{
    return BuildMetadataDomainList(GDALPamDataset::GetMetadataDomainList(),
                                   TRUE,
                                   "SUBDATASETS", NULL);
}

/************************************************************************/
/*                            GetMetadata()                             */
/************************************************************************/

char **HDF4Dataset::GetMetadata( const char *pszDomain )

{
    if( pszDomain != NULL && STARTS_WITH_CI(pszDomain, "SUBDATASETS") )
        return papszSubDatasets;

    return GDALDataset::GetMetadata( pszDomain );
}

/************************************************************************/
/*                           SPrintArray()                              */
/*      Prints numerical arrays in string buffer.                       */
/*      This function takes pfaDataArray as a pointer to printed array, */
/*      nValues as a number of values to print and pszDelimiter as a    */
/*      field delimiting strings.                                       */
/*      Pointer to filled buffer will be returned.                      */
/************************************************************************/

char *SPrintArray( GDALDataType eDataType, const void *paDataArray,
                   int nValues, const char *pszDelimiter )
{
    const int iFieldSize = 32 + static_cast<int>(strlen( pszDelimiter ) );
    char *pszField = static_cast<char *>( CPLMalloc( iFieldSize + 1 ) );
    const int iStringSize = nValues * iFieldSize + 1;
    char *pszString = static_cast<char *>( CPLMalloc( iStringSize ) );
    memset( pszString, 0, iStringSize );
    for( int i = 0; i < nValues; i++ )
    {
        switch ( eDataType )
        {
            case GDT_Byte:
                snprintf( pszField, iFieldSize + 1, "%d%s",
                         reinterpret_cast<GByte *>(
                             const_cast<void *>( paDataArray ) )[i],
                         (i < nValues - 1)?pszDelimiter:"" );
                break;
            case GDT_UInt16:
                snprintf( pszField, iFieldSize + 1, "%u%s",
                         reinterpret_cast<GUInt16 *>(
                             const_cast<void *>(  paDataArray ) )[i],
                         (i < nValues - 1)?pszDelimiter:"" );
                break;
            case GDT_Int16:
            default:
                snprintf( pszField, iFieldSize + 1, "%d%s",
                         reinterpret_cast<GInt16 *>(
                             const_cast<void *>(  paDataArray ) )[i],
                         (i < nValues - 1)?pszDelimiter:"" );
                break;
            case GDT_UInt32:
                snprintf( pszField, iFieldSize + 1, "%u%s",
                         reinterpret_cast<GUInt32 *>(
                             const_cast<void *>(  paDataArray ) )[i],
                     (i < nValues - 1)?pszDelimiter:"" );
                break;
            case GDT_Int32:
                snprintf( pszField, iFieldSize + 1, "%d%s",
                         reinterpret_cast<GInt32 *>(
                                const_cast<void *>(  paDataArray ) )[i],
                         (i < nValues - 1)?pszDelimiter:"" );
                break;
            case GDT_Float32:
                CPLsnprintf( pszField, iFieldSize + 1, "%.10g%s",
                             reinterpret_cast<float *>(
                                 const_cast<void *>(  paDataArray ) )[i],
                             (i < nValues - 1)?pszDelimiter:"" );
                break;
            case GDT_Float64:
                CPLsnprintf( pszField, iFieldSize + 1, "%.15g%s",
                             reinterpret_cast<double *>(
                                 const_cast<void *>(  paDataArray ) )[i],
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
/*              Return the human readable name of data type             */
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
        {
            CPLError( CE_Warning, CPLE_AppDefined,
                      "Unknown type %d", static_cast<int>(iNumType) );

            return "unknown type";
        }
    }
}

/************************************************************************/
/*  Return the size of data type in bytes                               */
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
        {
            CPLError( CE_Warning, CPLE_AppDefined,
                      "Unknown type %d", static_cast<int>(iNumType) );
            return 0;
        }
    }
}

/************************************************************************/
/*  Convert value stored in the input buffer to double value.           */
/************************************************************************/

double HDF4Dataset::AnyTypeToDouble( int32 iNumType, void *pData )
{
    switch ( iNumType )
    {
        case DFNT_INT8:
            return static_cast<double>(*reinterpret_cast<char *>(pData));
        case DFNT_UINT8:
            return static_cast<double>(*reinterpret_cast<GByte *>(pData));
        case DFNT_INT16:
            return static_cast<double>(*reinterpret_cast<GInt16 *>(pData));
        case DFNT_UINT16:
            return static_cast<double>(*reinterpret_cast<GUInt16 *>(pData));
        case DFNT_INT32:
            return static_cast<double>(*reinterpret_cast<GInt32 *>(pData));
        case DFNT_UINT32:
            return static_cast<double>(*reinterpret_cast<GUInt32 *>(pData));
#ifdef CPL_HAS_GINT64
        case DFNT_INT64:
            return static_cast<double>(*reinterpret_cast<GInt64 *>(pData));
        case DFNT_UINT64:
            return static_cast<double>(*reinterpret_cast<GUInt64 *>(pData));
#endif
        case DFNT_FLOAT32:
            return static_cast<double>(*reinterpret_cast<float *>(pData));
        case DFNT_FLOAT64:
            return static_cast<double>(*reinterpret_cast<double *>(pData));
        default:
        {
            CPLError( CE_Warning, CPLE_AppDefined,
                      "Unknown type %d", static_cast<int>(iNumType) );
            return 0.0;
        }
    }
}

/************************************************************************/
/*         Tokenize HDF-EOS attributes.                                 */
/************************************************************************/

char **HDF4Dataset::HDF4EOSTokenizeAttrs( const char * pszString )

{
    const char  * const pszDelimiters = " \t\n\r";
    char        **papszRetList = NULL;

    char *pszToken = static_cast<char *>( CPLCalloc( 10, 1 ) );
    int nTokenMax = 10;

    while( pszString != NULL && *pszString != '\0' )
    {
        bool bInString = false;
        bool bInBracket = false;

        int nTokenLen = 0;

        // Try to find the next delimiter, marking end of token.
        for( ; *pszString != '\0'; pszString++ )
        {

            // End if this is a delimiter skip it and break.
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
                bInString = !bInString;
                continue;
            }
            else if ( *pszString == '(' )
            {
                bInBracket = true;
                continue;
            }
            else if ( *pszString == ')' )
            {
                bInBracket = false;
                continue;
            }

            if( nTokenLen >= nTokenMax - 2 )
            {
                nTokenMax = nTokenMax * 2 + 10;
                pszToken = static_cast<char *>(
                    CPLRealloc( pszToken, nTokenMax ) );
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
        papszRetList = static_cast<char **>( CPLCalloc( sizeof(char *), 1 ) );

    CPLFree( pszToken );

    return papszRetList;
}

/************************************************************************/
/*     Find object name, class value in HDF-EOS attributes.             */
/*     Function returns pointer to the string in list next behind       */
/*     recognized object.                                               */
/************************************************************************/

char **HDF4Dataset::HDF4EOSGetObject( char **papszAttrList,
                                      char **ppszAttrName,
                                      char **ppszAttrClass,
                                      char **ppszAttrValue )
{
    *ppszAttrName = NULL;
    *ppszAttrClass = NULL;
    *ppszAttrValue = NULL;

    const int iCount = CSLCount( papszAttrList );
    for( int i = 0; i < iCount - 2; i++ )
    {
        if ( EQUAL( papszAttrList[i], "OBJECT" ) )
        {
            i += 2;
            for ( int j = 1; i + j < iCount - 2; j++ )
            {
                if ( EQUAL( papszAttrList[i + j], "END_OBJECT" ) ||
                     EQUAL( papszAttrList[i + j], "OBJECT" ) )
                    return &papszAttrList[i + j];
                else if ( EQUAL( papszAttrList[i + j], "CLASS" ) )
                {
                    *ppszAttrClass = papszAttrList[i + j + 2];
                    continue;
                }
                else if ( EQUAL( papszAttrList[i + j], "VALUE" ) )
                {
                    *ppszAttrName = papszAttrList[i];
                    *ppszAttrValue = papszAttrList[i + j + 2];
                    continue;
                }
            }
        }
    }

    return NULL;
}

/************************************************************************/
/*         Translate HDF4-EOS attributes in GDAL metadata items         */
/************************************************************************/

char** HDF4Dataset::TranslateHDF4EOSAttributes(
    int32 iHandle, int32 iAttribute, int32 nValues, char **papszMetadata )
{
    char *pszData = static_cast<char *>(
        CPLMalloc( (nValues + 1) * sizeof(char) ) );
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
    // Records within objects could come in any order, objects could contain
    // other objects (and lack VALUE record), groups could contain other groups
    // and objects. Names of groups and objects are not unique and may repeat.
    // Objects may contains other types of records.
    //
    // We are interested in OBJECTS structures only. To avoid multiple items
    // with the same name, names will be suffixed with the class values, e.g.
    //
    //  OBJECT                 = PARAMETERNAME
    //    CLASS                = "9"
    //    NUM_VAL              = 1
    //    VALUE                = "Spectral IR Surf Bidirect Reflectivity"
    //  END_OBJECT             = PARAMETERNAME
    //
    //  will be translated into metadata record:
    //
    //  PARAMETERNAME.9 = "Spectral IR Surf Bidirect Reflectivity"

    char *pszAttrName = NULL;
    char *pszAttrClass = NULL;
    char *pszAttrValue = NULL;
    char *pszAddAttrName = NULL;

    char ** const papszAttrList = HDF4EOSTokenizeAttrs( pszData );
    char ** papszAttrs = papszAttrList;
    while ( papszAttrs )
    {
        papszAttrs = HDF4EOSGetObject( papszAttrs, &pszAttrName,
                                       &pszAttrClass, &pszAttrValue );
        if ( pszAttrName && pszAttrValue )
        {
            // Now we should recognize special type of HDF EOS metastructures:
            // ADDITIONALATTRIBUTENAME = <name>
            // PARAMETERVALUE = <value>
            if ( EQUAL( pszAttrName, "ADDITIONALATTRIBUTENAME" ) )
            {
                pszAddAttrName = pszAttrValue;
            }
            else if ( pszAddAttrName && EQUAL( pszAttrName, "PARAMETERVALUE" ) )
            {
                papszMetadata =
                    CSLAddNameValue( papszMetadata, pszAddAttrName,
                                     pszAttrValue );
                pszAddAttrName = NULL;
            }
            else
            {
                // Add class suffix to the key name if applicable.
                papszMetadata = CSLAddNameValue(
                    papszMetadata,
                    pszAttrClass
                    ? CPLSPrintf("%s.%s", pszAttrName, pszAttrClass)
                    : pszAttrName,
                    pszAttrValue );
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

char** HDF4Dataset::TranslateHDF4Attributes(
    int32 iHandle, int32 iAttribute, char *pszAttrName, int32 iNumType,
    int32 nValues, char **papszMetadata )
{

/* -------------------------------------------------------------------- */
/*     Allocate a buffer to hold the attribute data.                    */
/* -------------------------------------------------------------------- */
    void *pData = NULL;
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
        reinterpret_cast<char *>( pData )[nValues] = '\0';
        papszMetadata = CSLAddNameValue(
            papszMetadata, pszAttrName,
            const_cast<const char *>( reinterpret_cast<char *>( pData )) );
    }
    else
    {
        char *pszTemp = NULL;
        pszTemp = SPrintArray( GetDataType(iNumType), pData, nValues, ", " );
        papszMetadata = CSLAddNameValue( papszMetadata, pszAttrName, pszTemp );
        CPLFree( pszTemp );
    }

    CPLFree( pData );

    return papszMetadata;
}

/************************************************************************/
/*                       ReadGlobalAttributes()                         */
/************************************************************************/

CPLErr HDF4Dataset::ReadGlobalAttributes( int32 iHandler )
{
/* -------------------------------------------------------------------- */
/*     Obtain number of SDSs and global attributes in input file.       */
/* -------------------------------------------------------------------- */
    int32 nDatasets = 0;
    int32 nAttributes = 0;
    if ( SDfileinfo( iHandler, &nDatasets, &nAttributes ) != 0 )
        return CE_Failure;

    char szAttrName[H4_MAX_NC_NAME] = {};  // TODO: Get this off the stack.

    // Loop through the all attributes
    for( int32 iAttribute = 0; iAttribute < nAttributes; iAttribute++ )
    {
        int32 iNumType = 0;
        int32 nValues = 0;

        // Get information about the attribute. Note that the first
        // parameter is an SD interface identifier.
        SDattrinfo( iHandler, iAttribute, szAttrName, &iNumType, &nValues );

        if ( STARTS_WITH_CI(szAttrName, "coremetadata")    ||
             STARTS_WITH_CI(szAttrName, "archivemetadata.") ||
             STARTS_WITH_CI(szAttrName, "productmetadata.") ||
             STARTS_WITH_CI(szAttrName, "badpixelinformation") ||
             STARTS_WITH_CI(szAttrName, "product_summary") ||
             STARTS_WITH_CI(szAttrName, "dem_specific") ||
             STARTS_WITH_CI(szAttrName, "bts_specific") ||
             STARTS_WITH_CI(szAttrName, "etse_specific") ||
             STARTS_WITH_CI(szAttrName, "dst_specific") ||
             STARTS_WITH_CI(szAttrName, "acv_specific") ||
             STARTS_WITH_CI(szAttrName, "act_specific") ||
             STARTS_WITH_CI(szAttrName, "etst_specific") ||
             STARTS_WITH_CI(szAttrName, "level_1_carryover") )
        {
            bIsHDFEOS = true;
            papszGlobalMetadata
                = TranslateHDF4EOSAttributes(
                    iHandler, iAttribute, nValues, papszGlobalMetadata );
        }

        // Skip "StructMetadata.N" records. We will fetch information
        // from them using HDF-EOS API
        else if ( STARTS_WITH_CI(szAttrName, "structmetadata.") )
        {
            bIsHDFEOS = true;
            continue;
        }

        else
        {
            papszGlobalMetadata = TranslateHDF4Attributes(
                iHandler, iAttribute, szAttrName, iNumType, nValues,
                papszGlobalMetadata );
        }
    }

    return CE_None;
}

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/

int HDF4Dataset::Identify( GDALOpenInfo * poOpenInfo )

{
    if( poOpenInfo->nHeaderBytes < 4 )
        return FALSE;

    if( memcmp(poOpenInfo->pabyHeader, "\016\003\023\001", 4) != 0 )
        return FALSE;

    return TRUE;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *HDF4Dataset::Open( GDALOpenInfo * poOpenInfo )

{
#ifndef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
    // During fuzzing, do not use Identify to reject crazy content.
    if( !Identify( poOpenInfo ) )
        return NULL;
#endif

    CPLMutexHolderD(&hHDF4Mutex);

/* -------------------------------------------------------------------- */
/*      Try opening the dataset.                                        */
/* -------------------------------------------------------------------- */

    // Attempt to increase maximum number of opened HDF files.
#ifdef HDF4_HAS_MAXOPENFILES
    intn nCurrMax = 0;
    intn nSysLimit = 0;

    if ( SDget_maxopenfiles(&nCurrMax, &nSysLimit) >= 0
         && nCurrMax < nSysLimit )
    {
        /*intn res = */SDreset_maxopenfiles( nSysLimit );
    }
#endif /* HDF4_HAS_MAXOPENFILES */

    int32 hHDF4 = Hopen(poOpenInfo->pszFilename, DFACC_READ, 0);

    if( hHDF4 <= 0 )
        return NULL;

    Hclose( hHDF4 );

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    // Release mutex otherwise we will deadlock with GDALDataset own mutex.
    CPLReleaseMutex(hHDF4Mutex);
    HDF4Dataset *poDS = new HDF4Dataset();
    CPLAcquireMutex(hHDF4Mutex, 1000.0);

    if( poOpenInfo->fpL != NULL )
    {
        VSIFCloseL(poOpenInfo->fpL);
        poOpenInfo->fpL = NULL;
    }

/* -------------------------------------------------------------------- */
/*          Open HDF SDS Interface.                                     */
/* -------------------------------------------------------------------- */
    poDS->hSD = SDstart( poOpenInfo->pszFilename, DFACC_READ );

    if ( poDS->hSD == -1 )
    {
      // Release mutex otherwise we will deadlock with GDALDataset own mutex.
        CPLReleaseMutex(hHDF4Mutex);
        delete poDS;
        CPLAcquireMutex(hHDF4Mutex, 1000.0);
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Failed to open HDF4 file \"%s\" for SDS reading.",
                  poOpenInfo->pszFilename );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*              Now read Global Attributes.                             */
/* -------------------------------------------------------------------- */
    if ( poDS->ReadGlobalAttributes( poDS->hSD ) != CE_None )
    {
        // Release mutex otherwise we will deadlock with GDALDataset own mutex.
        CPLReleaseMutex(hHDF4Mutex);
        delete poDS;
        CPLAcquireMutex(hHDF4Mutex, 1000.0);
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Failed to read global attributes from HDF4 file \"%s\".",
                  poOpenInfo->pszFilename );
        return NULL;
    }

    poDS->SetMetadata( poDS->papszGlobalMetadata, "" );

/* -------------------------------------------------------------------- */
/*              Determine type of file we read.                         */
/* -------------------------------------------------------------------- */
    const char *pszValue = CSLFetchNameValue(poDS->papszGlobalMetadata,
                                             "Signature");

    if ( pszValue != NULL && EQUAL( pszValue, pszGDALSignature ) )
    {
        poDS->iSubdatasetType = H4ST_GDAL;
        poDS->pszSubdatasetType = "GDAL_HDF4";
    }

    else if ( (pszValue = CSLFetchNameValue(poDS->papszGlobalMetadata,
                                            "Title")) != NULL
         && EQUAL( pszValue, "SeaWiFS Level-1A Data" ) )
    {
        poDS->iSubdatasetType = H4ST_SEAWIFS_L1A;
        poDS->pszSubdatasetType = "SEAWIFS_L1A";
    }

    else if ( (pszValue = CSLFetchNameValue(poDS->papszGlobalMetadata,
                                            "Title")) != NULL
        && EQUAL( pszValue, "SeaWiFS Level-2 Data" ) )
    {
        poDS->iSubdatasetType = H4ST_SEAWIFS_L2;
        poDS->pszSubdatasetType = "SEAWIFS_L2";
    }

    else if ( (pszValue = CSLFetchNameValue(poDS->papszGlobalMetadata,
                                            "Title")) != NULL
        && EQUAL( pszValue, "SeaWiFS Level-3 Standard Mapped Image" ) )
    {
        poDS->iSubdatasetType = H4ST_SEAWIFS_L3;
        poDS->pszSubdatasetType = "SEAWIFS_L3";
    }

    else if ( (pszValue = CSLFetchNameValue(poDS->papszGlobalMetadata,
                                            "L1 File Generated By")) != NULL
        && STARTS_WITH_CI(pszValue, "HYP version ") )
    {
        poDS->iSubdatasetType = H4ST_HYPERION_L1;
        poDS->pszSubdatasetType = "HYPERION_L1";
    }

    else
    {
        poDS->iSubdatasetType = H4ST_UNKNOWN;
        poDS->pszSubdatasetType = "UNKNOWN";
    }

/* -------------------------------------------------------------------- */
/*  If we have HDF-EOS dataset, process it here.                        */
/* -------------------------------------------------------------------- */
    int32 aiDimSizes[H4_MAX_VAR_DIMS] = {};  // TODO: Get this off of the stack.
    int32 iRank = 0;
    int32 iNumType = 0;
    int32 nAttrs = 0;
    bool bIsHDF = true;

    // Sometimes "HDFEOSVersion" attribute is not defined and we will
    // determine HDF-EOS datasets using other records
    // (see ReadGlobalAttributes() method).
    if ( poDS->bIsHDFEOS
         || CSLFetchNameValue(poDS->papszGlobalMetadata, "HDFEOSVersion") )
    {
/* -------------------------------------------------------------------- */
/*  Process swath layers.                                               */
/* -------------------------------------------------------------------- */
        hHDF4 = SWopen( poOpenInfo->pszFilename, DFACC_READ );
        if( hHDF4 < 0)
        {
            // Release mutex otherwise we will deadlock with GDALDataset own
            // mutex.
            CPLReleaseMutex(hHDF4Mutex);
            delete poDS;
            CPLAcquireMutex(hHDF4Mutex, 1000.0);
            CPLError( CE_Failure, CPLE_OpenFailed,
                      "Failed to open HDF-EOS file \"%s\" for swath reading.",
                      poOpenInfo->pszFilename );
            return NULL;
        }
        int32 nStrBufSize = 0;
        int32 nSubDatasets =
            SWinqswath(poOpenInfo->pszFilename, NULL, &nStrBufSize);

#ifdef DEBUG
        CPLDebug( "HDF4", "Number of HDF-EOS swaths: %d",
                  static_cast<int>( nSubDatasets ) );
#endif

        if ( nSubDatasets > 0 && nStrBufSize > 0 )
        {
            char *pszSwathList =
                static_cast<char *>( CPLMalloc( nStrBufSize + 1 ) );
            SWinqswath( poOpenInfo->pszFilename, pszSwathList, &nStrBufSize );
            pszSwathList[nStrBufSize] = '\0';

#ifdef DEBUG
            CPLDebug( "HDF4", "List of HDF-EOS swaths: %s", pszSwathList );
#endif

            char **papszSwaths =
                CSLTokenizeString2( pszSwathList, ",", CSLT_HONOURSTRINGS );
            CPLFree( pszSwathList );

            if ( nSubDatasets != CSLCount(papszSwaths) )
            {
                CSLDestroy( papszSwaths );
                // Release mutex otherwise we will deadlock with GDALDataset own
                // mutex.
                CPLReleaseMutex(hHDF4Mutex);
                delete poDS;
                CPLAcquireMutex(hHDF4Mutex, 1000.0);
                CPLDebug( "HDF4", "Cannot parse list of HDF-EOS grids." );
                return NULL;
            }

            for( int32 i = 0; i < nSubDatasets; i++)
            {
                const int32 hSW = SWattach( hHDF4, papszSwaths[i] );

                const int32 nFields
                    = SWnentries( hSW, HDFE_NENTDFLD, &nStrBufSize );
                char *pszFieldList = static_cast<char *>(
                    CPLMalloc( nStrBufSize + 1 ) );
                int32 *paiRank = static_cast<int32 *>(
                    CPLMalloc( nFields * sizeof(int32) ) );
                int32 *paiNumType = static_cast<int32 *>(
                    CPLMalloc( nFields * sizeof(int32) ) );

                SWinqdatafields( hSW, pszFieldList, paiRank, paiNumType );

#ifdef DEBUG
                {
                    char * const pszTmp =
                        SPrintArray( GDT_UInt32, paiRank, nFields, "," );

                    CPLDebug( "HDF4", "Number of data fields in swath %d: %d",
                              static_cast<int>( i ),
                              static_cast<int>( nFields ) );
                    CPLDebug( "HDF4", "List of data fields in swath %d: %s",
                              static_cast<int>( i ), pszFieldList );
                    CPLDebug( "HDF4", "Data fields ranks: %s", pszTmp );

                    CPLFree( pszTmp );
                }
#endif

                char **papszFields = CSLTokenizeString2( pszFieldList, ",",
                                                         CSLT_HONOURSTRINGS );

                char szTemp[256] = {'\0'};  // TODO: Get this off the stack.
                for( int32 j = 0; j < nFields; j++ )
                {
                    SWfieldinfo( hSW, papszFields[j], &iRank, aiDimSizes,
                                 &iNumType, NULL );

                    if ( iRank < 2 )
                        continue;

                    // Add field to the list of GDAL subdatasets.
                    const int nCount = CSLCount( poDS->papszSubDatasets ) / 2;
                    snprintf( szTemp, sizeof(szTemp),
                              "SUBDATASET_%d_NAME", nCount + 1 );
                    // We will use the field index as an identificator.
                    poDS->papszSubDatasets =
                        CSLSetNameValue( poDS->papszSubDatasets, szTemp,
                                CPLSPrintf("HDF4_EOS:EOS_SWATH:\"%s\":%s:%s",
                                           poOpenInfo->pszFilename,
                                           papszSwaths[i], papszFields[j]) );

                    snprintf( szTemp, sizeof(szTemp),
                              "SUBDATASET_%d_DESC", nCount + 1 );
                    char *pszString = SPrintArray( GDT_UInt32, aiDimSizes,
                                                   iRank, "x" );
                    poDS->papszSubDatasets =
                        CSLSetNameValue( poDS->papszSubDatasets, szTemp,
                                         CPLSPrintf( "[%s] %s %s (%s)",
                                                     pszString,
                                         papszFields[j],
                                         papszSwaths[i],
                                         poDS->GetDataTypeName(iNumType) ) );
                    CPLFree( pszString );
                    szTemp[0] = '\0';
                }

                CSLDestroy( papszFields );
                CPLFree( paiNumType );
                CPLFree( paiRank );
                CPLFree( pszFieldList );
                SWdetach( hSW );
            }

            CSLDestroy( papszSwaths );
        }
        SWclose( hHDF4 );

/* -------------------------------------------------------------------- */
/*  Process grid layers.                                                */
/* -------------------------------------------------------------------- */
        hHDF4 = GDopen( poOpenInfo->pszFilename, DFACC_READ );
        nSubDatasets = GDinqgrid( poOpenInfo->pszFilename, NULL, &nStrBufSize );

#ifdef DEBUG
        CPLDebug( "HDF4", "Number of HDF-EOS grids: %d",
                  static_cast<int>( nSubDatasets ) );
#endif

        if ( nSubDatasets > 0 && nStrBufSize > 0 )
        {
            char *pszGridList
                = static_cast<char *>( CPLMalloc( nStrBufSize + 1 ) );
            GDinqgrid( poOpenInfo->pszFilename, pszGridList, &nStrBufSize );

#ifdef DEBUG
            CPLDebug( "HDF4", "List of HDF-EOS grids: %s", pszGridList );
#endif

            char **papszGrids =
                CSLTokenizeString2( pszGridList, ",", CSLT_HONOURSTRINGS );
            CPLFree( pszGridList );

            if ( nSubDatasets != CSLCount(papszGrids) )
            {
                CSLDestroy( papszGrids );
                GDclose( hHDF4 );
                // Release mutex otherwise we will deadlock with GDALDataset own
                // mutex.
                CPLReleaseMutex(hHDF4Mutex);
                delete poDS;
                CPLAcquireMutex(hHDF4Mutex, 1000.0);
                CPLDebug( "HDF4", "Cannot parse list of HDF-EOS grids." );
                return NULL;
            }

            for( int32 i = 0; i < nSubDatasets; i++)
            {
                const int32 hGD = GDattach( hHDF4, papszGrids[i] );

                const int32 nFields
                    = GDnentries( hGD, HDFE_NENTDFLD, &nStrBufSize );
                char *pszFieldList = static_cast<char *>(
                    CPLMalloc( nStrBufSize + 1 ) );
                int32 *paiRank = static_cast<int32 *>(
                    CPLMalloc( nFields * sizeof(int32) ) );
                int32 *paiNumType = static_cast<int32 *>(
                    CPLMalloc( nFields * sizeof(int32) ) );

                GDinqfields( hGD, pszFieldList, paiRank, paiNumType );

#ifdef DEBUG
                {
                    char* pszTmp =
                            SPrintArray( GDT_UInt32, paiRank, nFields, "," );
                    CPLDebug( "HDF4", "Number of fields in grid %d: %d",
                              static_cast<int>( i ),
                              static_cast<int>( nFields ) );
                    CPLDebug( "HDF4", "List of fields in grid %d: %s",
                              static_cast<int>( i ), pszFieldList );
                    CPLDebug( "HDF4", "Fields ranks: %s", pszTmp );
                    CPLFree( pszTmp );
                }
#endif

                char **papszFields = CSLTokenizeString2( pszFieldList, ",",
                                                  CSLT_HONOURSTRINGS );

                char szTemp[256];
                for( int32 j = 0; j < nFields; j++ )
                {
                    GDfieldinfo( hGD, papszFields[j], &iRank, aiDimSizes,
                                 &iNumType, NULL );

                    if ( iRank < 2 )
                        continue;

                    // Add field to the list of GDAL subdatasets
                    const int nCount = CSLCount( poDS->papszSubDatasets ) / 2;
                    snprintf( szTemp, sizeof(szTemp),
                              "SUBDATASET_%d_NAME", nCount + 1 );
                    // We will use the field index as an identificator.
                    poDS->papszSubDatasets =
                        CSLSetNameValue(poDS->papszSubDatasets, szTemp,
                                CPLSPrintf( "HDF4_EOS:EOS_GRID:\"%s\":%s:%s",
                                            poOpenInfo->pszFilename,
                                            papszGrids[i], papszFields[j]));

                    snprintf( szTemp, sizeof(szTemp),
                              "SUBDATASET_%d_DESC", nCount + 1 );
                    char *pszString = SPrintArray( GDT_UInt32, aiDimSizes,
                                                   iRank, "x" );
                    poDS->papszSubDatasets =
                        CSLSetNameValue( poDS->papszSubDatasets, szTemp,
                                         CPLSPrintf("[%s] %s %s (%s)",
                                                    pszString,
                                             papszFields[j],
                                             papszGrids[i],
                                             poDS->GetDataTypeName(iNumType)) );
                    CPLFree( pszString );
                }

                CSLDestroy( papszFields );
                CPLFree( paiNumType );
                CPLFree( paiRank );
                CPLFree( pszFieldList );
                GDdetach( hGD );
            }

            CSLDestroy( papszGrids );
        }
        GDclose( hHDF4 );

        bIsHDF = ( nSubDatasets == 0 ); // Try to read as HDF
    }

    char szName[VSNAMELENMAX + 1];

    if( bIsHDF )
    {

/* -------------------------------------------------------------------- */
/*  Make a list of subdatasets from SDSs contained in input HDF file.   */
/* -------------------------------------------------------------------- */
        int32 nDatasets = 0;

        if ( SDfileinfo( poDS->hSD, &nDatasets, &nAttrs ) != 0 )
            return NULL;

        char szTemp[256] = {'\0'};  // TODO: Get this off the stack.
        const char *pszName = NULL;

        for( int32 i = 0; i < nDatasets; i++ )
        {
            const int32 iSDS = SDselect( poDS->hSD, i );
            if ( SDgetinfo( iSDS, szName, &iRank, aiDimSizes, &iNumType,
                            &nAttrs) != 0 )
                return NULL;

            if ( iRank == 1 )  // Skip 1D datsets
                    continue;

            // Do sort of known datasets. We will display only image bands
            if ( (poDS->iSubdatasetType == H4ST_SEAWIFS_L1A ) &&
                      !STARTS_WITH_CI(szName, "l1a_data") )
                    continue;
            else
                pszName = szName;

            // Add datasets with multiple dimensions to the list of GDAL
            // subdatasets.
            const int nCount = CSLCount( poDS->papszSubDatasets ) / 2;
            snprintf( szTemp, sizeof(szTemp),
                      "SUBDATASET_%d_NAME", nCount + 1 );
            // We will use SDS index as an identificator, because SDS names
            // are not unique. Filename also needed for further file opening
            poDS->papszSubDatasets = CSLSetNameValue(
                  poDS->papszSubDatasets,
                  szTemp,
                  CPLSPrintf( "HDF4_SDS:%s:\"%s\":%ld", poDS->pszSubdatasetType,
                              poOpenInfo->pszFilename,
                              static_cast<long>( i ) ) );
            snprintf( szTemp, sizeof(szTemp),
                      "SUBDATASET_%d_DESC", nCount + 1 );
            char *pszString = SPrintArray( GDT_UInt32, aiDimSizes, iRank, "x" );
            poDS->papszSubDatasets = CSLSetNameValue(
                poDS->papszSubDatasets,
                szTemp,
                CPLSPrintf( "[%s] %s (%s)", pszString,
                            pszName, poDS->GetDataTypeName(iNumType)) );
            CPLFree( pszString );

            SDendaccess( iSDS );
            szTemp[0] = '\0';
        }

        SDend( poDS->hSD );
        poDS->hSD = 0;
    }

/* -------------------------------------------------------------------- */
/*      Build a list of raster images. Note, that HDF-EOS dataset may   */
/*      contain a raster image as well.                                 */
/* -------------------------------------------------------------------- */

    hHDF4 = Hopen(poOpenInfo->pszFilename, DFACC_READ, 0);
    poDS->hGR = GRstart( hHDF4 );

    if ( poDS->hGR != -1 )
    {
        if ( GRfileinfo( poDS->hGR, &poDS->nImages, &nAttrs ) == -1 )
        {
            // Release mutex otherwise we will deadlock with GDALDataset own
            // mutex.
            CPLReleaseMutex(hHDF4Mutex);
            GRend( poDS->hGR );
            poDS->hGR = 0;
            Hclose( hHDF4 );
            delete poDS;
            CPLAcquireMutex(hHDF4Mutex, 1000.0);
            return NULL;
        }

        char szTemp[256] = {'\0'};  // TODO: Get this off the stack.
        for( int32 i = 0; i < poDS->nImages; i++ )
        {
            const int32 iGR = GRselect( poDS->hGR, i );

            // iRank in GR interface has another meaning. It represents number
            // of samples per pixel. aiDimSizes has only two dimensions.
            int32 iInterlaceMode = 0;
            if ( GRgetiminfo( iGR, szName, &iRank, &iNumType, &iInterlaceMode,
                              aiDimSizes, &nAttrs ) != 0 )
            {
                // Release mutex otherwise we will deadlock with GDALDataset
                // own mutex.
                CPLReleaseMutex(hHDF4Mutex);
                GRend( poDS->hGR );
                poDS->hGR = 0;
                Hclose( hHDF4 );
                delete poDS;
                CPLAcquireMutex(hHDF4Mutex, 1000.0);
                return NULL;
            }
            const int nCount = CSLCount( poDS->papszSubDatasets ) / 2;
            snprintf( szTemp, sizeof(szTemp),
                      "SUBDATASET_%d_NAME", nCount + 1 );
            poDS->papszSubDatasets = CSLSetNameValue(poDS->papszSubDatasets,
                szTemp,CPLSPrintf( "HDF4_GR:UNKNOWN:\"%s\":%ld",
                                   poOpenInfo->pszFilename,
                                   static_cast<long>( i ) ) );
            snprintf( szTemp, sizeof(szTemp),
                      "SUBDATASET_%d_DESC", nCount + 1 );
            char *pszString = SPrintArray( GDT_UInt32, aiDimSizes, 2, "x" );
            poDS->papszSubDatasets = CSLSetNameValue(poDS->papszSubDatasets,
                szTemp, CPLSPrintf( "[%sx%ld] %s (%s)", pszString,
                                    static_cast<long>( iRank ),
                                    szName, poDS->GetDataTypeName(iNumType)) );
            CPLFree( pszString );

            GRendaccess( iGR );
            szTemp[0] = '\0';
        }

        GRend( poDS->hGR );
        poDS->hGR = 0;
    }

    Hclose( hHDF4 );

    poDS->nRasterXSize = 512; // XXX: bogus value
    poDS->nRasterYSize = 512; // XXX: bogus value

    // Make sure we don't try to do any pam stuff with this dataset.
    poDS->nPamFlags |= GPF_NOSAVE;

/* -------------------------------------------------------------------- */
/*      If we have single subdataset only, open it immediately          */
/* -------------------------------------------------------------------- */
    if ( CSLCount( poDS->papszSubDatasets ) / 2 == 1 )
    {
        char *pszSDSName = CPLStrdup( CSLFetchNameValue( poDS->papszSubDatasets,
                                                         "SUBDATASET_1_NAME" ));
        // Release mutex otherwise we will deadlock with GDALDataset own mutex.
        CPLReleaseMutex(hHDF4Mutex);
        delete poDS;
        poDS = NULL;

        GDALDataset* poRetDS = reinterpret_cast<GDALDataset*>(
            GDALOpen( pszSDSName, poOpenInfo->eAccess ) );
        CPLFree( pszSDSName );

        CPLAcquireMutex(hHDF4Mutex, 1000.0);

        if (poRetDS)
        {
            poRetDS->SetDescription(poOpenInfo->pszFilename);
        }

        return poRetDS;
    }
    else
    {
/* -------------------------------------------------------------------- */
/*      Confirm the requested access is supported.                      */
/* -------------------------------------------------------------------- */
        if( poOpenInfo->eAccess == GA_Update )
        {
            // Release mutex otherwise we will deadlock with GDALDataset own
            // mutex.
            CPLReleaseMutex(hHDF4Mutex);
            delete poDS;
            CPLAcquireMutex(hHDF4Mutex, 1000.0);

            CPLError( CE_Failure, CPLE_NotSupported,
                      "The HDF4 driver does not support update access to "
                      "existing datasets." );
            return NULL;
        }
    }

    return poDS;
}

/************************************************************************/
/*                           HDF4UnloadDriver()                         */
/************************************************************************/

static void HDF4UnloadDriver( GDALDriver * /* poDriver */ )
{
    if( hHDF4Mutex != NULL )
        CPLDestroyMutex(hHDF4Mutex);
    hHDF4Mutex = NULL;
}

/************************************************************************/
/*                        GDALRegister_HDF4()                           */
/************************************************************************/

void GDALRegister_HDF4()

{
    if( !GDAL_CHECK_VERSION( "HDF4 driver" ) )
        return;

    if( GDALGetDriverByName( "HDF4" ) != NULL )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "HDF4" );
    poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                               "Hierarchical Data Format Release 4" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "frmt_hdf4.html" );
    poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "hdf" );
    poDriver->SetMetadataItem( GDAL_DMD_SUBDATASETS, "YES" );

    poDriver->pfnOpen = HDF4Dataset::Open;
    poDriver->pfnIdentify = HDF4Dataset::Identify;
    poDriver->pfnUnloadDriver = HDF4UnloadDriver;

    GetGDALDriverManager()->RegisterDriver( poDriver );

#ifdef HDF4_PLUGIN
    GDALRegister_HDF4Image();
#endif
}
