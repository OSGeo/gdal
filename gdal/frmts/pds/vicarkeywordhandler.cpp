/******************************************************************************
*
* Project:  VICAR Driver; JPL/MIPL VICAR Format
* Purpose:  Implementation of VICARKeywordHandler - a class to read
*           keyword data from VICAR data products.
* Author:   Sebastian Walter <sebastian dot walter at fu-berlin dot de>
*
* NOTE: This driver code is loosely based on the ISIS and PDS drivers.
* It is not intended to diminish the contribution of the authors.
******************************************************************************
* Copyright (c) 2014, Sebastian Walter <sebastian dot walter at fu-berlin dot de>
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

#include "cpl_string.h"
#include "vicarkeywordhandler.h"
#include "vicardataset.h"

#include <algorithm>
#include <limits>

CPL_CVSID("$Id$")

/************************************************************************/
/* ==================================================================== */
/*                          VICARKeywordHandler                         */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                         VICARKeywordHandler()                        */
/************************************************************************/

VICARKeywordHandler::VICARKeywordHandler() :
    papszKeywordList(nullptr),
    pszHeaderNext(nullptr)
{
    oJSon.Deinit();
}

/************************************************************************/
/*                        ~VICARKeywordHandler()                        */
/************************************************************************/

VICARKeywordHandler::~VICARKeywordHandler()

{
    CSLDestroy( papszKeywordList );
}

/************************************************************************/
/*                               Ingest()                               */
/************************************************************************/

bool VICARKeywordHandler::Ingest( VSILFILE *fp, const GByte *pabyHeader )

{
/* -------------------------------------------------------------------- */
/*      Read in label at beginning of file.                             */
/* -------------------------------------------------------------------- */
    if( VSIFSeekL( fp, 0, SEEK_SET ) != 0 )
        return false;

    // Find LBLSIZE Entry
    const char* pszLBLSIZE = strstr(reinterpret_cast<const char *>( pabyHeader ), "LBLSIZE");
    if( !pszLBLSIZE )
        return false;

    const char *pch1 = strchr(pszLBLSIZE, '=');
    if( pch1 == nullptr )
        return false;
    ++pch1;
    while( isspace(static_cast<unsigned char>(*pch1)) )
        ++pch1;
    const char *pch2 = strchr(pch1, ' ');
    if( pch2 == nullptr )
        return false;

    std::string keyval;
    keyval.assign(pch1, static_cast<size_t>(pch2 - pch1));
    int LabelSize = atoi( keyval.c_str() );
    if( LabelSize <= 0 || LabelSize > 10 * 1024 * 124 )
        return false;

    char* pszChunk = reinterpret_cast<char *>(  VSIMalloc( LabelSize + 1 ) );
    if( pszChunk == nullptr )
        return false;
    int nBytesRead = static_cast<int>(VSIFReadL( pszChunk, 1, LabelSize, fp ));
    pszChunk[nBytesRead] = '\0';

    osHeaderText += pszChunk ;
    VSIFree( pszChunk );
    pszHeaderNext = osHeaderText.c_str();

/* -------------------------------------------------------------------- */
/*      Process name/value pairs                                        */
/* -------------------------------------------------------------------- */
    if( !Parse() )
        return false;

/* -------------------------------------------------------------------- */
/*      Now check for the Vicar End-of-Dataset Label...                 */
/* -------------------------------------------------------------------- */
    const char *pszResult = CSLFetchNameValueDef( papszKeywordList, "EOL", "0" );
    if( !EQUAL(pszResult,"1") )
        return true;

/* -------------------------------------------------------------------- */
/*      There is a EOL!   e.G.  h4231_0000.nd4.06                       */
/* -------------------------------------------------------------------- */

    GUInt64 nPixelOffset;
    GUInt64 nLineOffset;
    GUInt64 nBandOffset;
    GUInt64 nImageOffsetWithoutNBB;
    GUInt64 nNBB;
    GUInt64 nImageSize;
    if( !VICARDataset::GetSpacings(*this, nPixelOffset, nLineOffset, nBandOffset,
                                   nImageOffsetWithoutNBB, nNBB, nImageSize) )
        return false;

    // Position of EOL in case of compressed data
    const vsi_l_offset nEOCI1 = static_cast<vsi_l_offset>(
        CPLAtoGIntBig(CSLFetchNameValueDef(papszKeywordList, "EOCI1", "0")));
    const vsi_l_offset nEOCI2 = static_cast<vsi_l_offset>(
        CPLAtoGIntBig(CSLFetchNameValueDef(papszKeywordList, "EOCI2", "0")));
    const vsi_l_offset nEOCI = (nEOCI2 << 32) | nEOCI1;

    if( nImageOffsetWithoutNBB > std::numeric_limits<GUInt64>::max() - nImageSize )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid label values");
        return false;
    }

    const vsi_l_offset nStartEOL = nEOCI ? nEOCI :
                                        nImageOffsetWithoutNBB + nImageSize;

    if( VSIFSeekL( fp, nStartEOL, SEEK_SET ) != 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Error seeking to EOL");
        return false;
    }
    char* pszEOLHeader = static_cast<char*>(VSIMalloc(32));
    if( pszEOLHeader == nullptr )
        return false;
    nBytesRead = static_cast<int>(VSIFReadL( pszEOLHeader, 1, 31, fp ));
    pszEOLHeader[nBytesRead] = '\0';
    pszLBLSIZE=strstr(pszEOLHeader,"LBLSIZE");
    if( !pszLBLSIZE )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "END-OF-DATASET LABEL NOT FOUND!");
        VSIFree(pszEOLHeader);
        return false;
    }
    pch1 = strchr( pszLBLSIZE, '=' );
    if( pch1 == nullptr )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "END-OF-DATASET LABEL NOT FOUND!");
        VSIFree(pszEOLHeader);
        return false;
    }
    ++pch1;
    while( isspace(static_cast<unsigned char>(*pch1)) )
        ++pch1;
    pch2 = strchr( pch1, ' ' );
    if( pch2 == nullptr )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "END-OF-DATASET LABEL NOT FOUND!");
        VSIFree(pszEOLHeader);
        return false;
    }
    keyval.assign(pch1, static_cast<size_t>(pch2 - pch1));
    const auto nSkipEOLLBLSize = static_cast<size_t>(pch2 - pszEOLHeader);
    VSIFree(pszEOLHeader);

    int EOLabelSize = atoi( keyval.c_str() );
    if( EOLabelSize <= 0 ||
        static_cast<size_t>(EOLabelSize) <= nSkipEOLLBLSize ||
        EOLabelSize > 100 * 1024 * 1024 )
        return false;
    if( VSIFSeekL( fp, nStartEOL, SEEK_SET ) != 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Error seeking to EOL");
        return false;
    }
    char* pszChunkEOL = (char*) VSIMalloc(EOLabelSize+1);
    if( pszChunkEOL == nullptr )
        return false;
    nBytesRead = static_cast<int>(VSIFReadL( pszChunkEOL, 1, EOLabelSize, fp ));
    pszChunkEOL[nBytesRead] = '\0';
    osHeaderText += pszChunkEOL + nSkipEOLLBLSize;
    VSIFree(pszChunkEOL);
    CSLDestroy(papszKeywordList);
    papszKeywordList = nullptr;
    pszHeaderNext = osHeaderText.c_str();
    return Parse();
}

/************************************************************************/
/*                               Parse()                                */
/************************************************************************/

#define SYNTHETIC_END_MARKER    "__END__"

bool VICARKeywordHandler::Parse()
{
    CPLString osName, osValue, osGroupName;
    CPLJSONObject oProperties;
    CPLJSONObject oTasks;
    CPLJSONObject oCurObj;
    bool bHasProperties = false;
    bool bHasTasks = false;

    oJSon = CPLJSONObject();
    for( ; true; )
    {
        if( !ReadPair( osName, osValue, osGroupName.empty() ? oJSon : oCurObj ) )
            return false;

        if( EQUAL(osName, SYNTHETIC_END_MARKER) )
            break;

        if( EQUAL(osName,"PROPERTY") )
        {
            osGroupName = osValue;
            oCurObj = CPLJSONObject();
            bHasProperties = true;
            oProperties.Add(osValue, oCurObj);
        }
        else if( EQUAL(osName,"TASK") )
        {
            osGroupName = osValue;
            oCurObj = CPLJSONObject();
            bHasTasks = true;
            oTasks.Add(osValue, oCurObj);
        }
        else
        {
            if ( !osGroupName.empty() )
                osName = osGroupName + "." + osName;
            papszKeywordList = CSLSetNameValue( papszKeywordList, osName, osValue );
        }
    }
    if( bHasProperties )
        oJSon.Add("PROPERTY", oProperties);
    if( bHasTasks )
        oJSon.Add("TASK", oTasks);
    return true;
}

/************************************************************************/
/*                              ReadPair()                              */
/*                                                                      */
/*      Read a name/value pair from the input stream.  Strip off        */
/*      white space, ignore comments, split on '='.                     */
/*      Returns TRUE on success.                                        */
/************************************************************************/

bool VICARKeywordHandler::ReadPair( CPLString &osName, CPLString &osValue, CPLJSONObject& oCur )
{
    osName.clear();
    osValue.clear();

    if( !ReadName( osName ) )
    {
        // VICAR has no NULL string termination
        if( *pszHeaderNext == '\0') {
            osName = SYNTHETIC_END_MARKER;
            return true;
        }
        return false;
    }

    bool bIsString = false;
    if( *pszHeaderNext == '(' )
    {
        CPLString osWord;
        pszHeaderNext ++;
        CPLJSONArray oArray;
        oCur.Add( osName, oArray );
        while( ReadValue( osWord, true, bIsString ) )
        {
            if( !osValue.empty() )
                osValue += ',';
            osValue += osWord;
            if( bIsString )
            {
                oArray.Add( osWord );
            }
            else if( CPLGetValueType(osWord) == CPL_VALUE_INTEGER )
            {
                oArray.Add( atoi(osWord) );
            }
            else
            {
                oArray.Add( CPLAtof(osWord) );
            }
            if( *pszHeaderNext == ')' )
            {
                pszHeaderNext ++;
                break;
            }
            pszHeaderNext ++;
        }
    }
    else
    {
        if( !ReadValue( osValue, false, bIsString ) )
            return false;
        if( !EQUAL(osName, "PROPERTY") && !EQUAL(osName, "TASK") )
        {
            if( bIsString )
            {
                oCur.Add( osName, osValue );
            }
            else if( CPLGetValueType(osValue) == CPL_VALUE_INTEGER )
            {
                oCur.Add( osName, atoi(osValue) );
            }
            else
            {
                oCur.Add( osName, CPLAtof(osValue) );
            }
        }
    }

    return true;
}

/************************************************************************/
/*                              ReadName()                              */
/************************************************************************/

bool VICARKeywordHandler::ReadName( CPLString &osWord )

{
    osWord.clear();

    SkipWhite();

    if( *pszHeaderNext == '\0')
        return false;

    while( *pszHeaderNext != '=' && !isspace((unsigned char)*pszHeaderNext) )
    {
        if( *pszHeaderNext == '\0' )
            return false;
        osWord += *pszHeaderNext;
        pszHeaderNext++;
    }

    SkipWhite();

    if( *pszHeaderNext != '=' )
        return false;
    pszHeaderNext++;

    SkipWhite();

    return true;
}

/************************************************************************/
/*                              ReadWord()                              */
/************************************************************************/

bool VICARKeywordHandler::ReadValue( CPLString &osWord, bool bInList, bool& bIsString )

{
    osWord.clear();

    SkipWhite();

    if( *pszHeaderNext == '\0')
        return false;

    if( *pszHeaderNext == '\'' )
    {
        bIsString = true;
        pszHeaderNext++;
        while( true )
        {
            if( *pszHeaderNext == '\0' )
                return false;
            if( *(pszHeaderNext) == '\'' )
            {
                if( *(pszHeaderNext+1) == '\'' )
                {
                    //Skip Double Quotes
                    pszHeaderNext++;
                }
                else
                    break;
            }
            osWord += *pszHeaderNext;
            pszHeaderNext++;
        }
        pszHeaderNext++;
    }
    else
    {
        while( !isspace((unsigned char)*pszHeaderNext) )
        {
            if( *pszHeaderNext == '\0' )
                return !bInList;
            if( bInList && (*pszHeaderNext == ',' || *pszHeaderNext == ')') )
            {
                return true;
            }
            osWord += *pszHeaderNext;
            pszHeaderNext++;
        }
        bIsString = CPLGetValueType(osWord) == CPL_VALUE_STRING;
    }

    SkipWhite();
    if( bInList && *pszHeaderNext != ',' && *pszHeaderNext != ')' )
        return false;

    return true;
}

/************************************************************************/
/*                             SkipWhite()                              */
/*  Skip white spaces                                                   */
/************************************************************************/

void VICARKeywordHandler::SkipWhite()

{
    for( ; true; )
    {
        if( isspace( (unsigned char)*pszHeaderNext ) )
        {
            pszHeaderNext++;
            continue;
        }

        // not white space, return.
        return;
    }
}

/************************************************************************/
/*                             GetKeyword()                             */
/************************************************************************/

const char *VICARKeywordHandler::GetKeyword( const char *pszPath, const char *pszDefault ) const

{
    const char *pszResult = CSLFetchNameValue( papszKeywordList, pszPath );

    if( pszResult == nullptr )
        return pszDefault;

    return pszResult;
}
