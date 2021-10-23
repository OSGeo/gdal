/******************************************************************************
 * Project:  OGR
 * Purpose:  OGRGMLASDriver implementation
 * Author:   Even Rouault, <even dot rouault at spatialys dot com>
 *
 * Initial development funded by the European Earth observation programme
 * Copernicus
 *
 ******************************************************************************
 * Copyright (c) 2016, Even Rouault, <even dot rouault at spatialys dot com>
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

#include "ogr_gmlas.h"

#include <map>
#include <set>

/************************************************************************/
/*                  OGRGMLASTruncateIdentifier()                        */
/************************************************************************/

CPLString OGRGMLASTruncateIdentifier(const CPLString& osName,
                                     int nIdentMaxLength)
{
    int nExtra = static_cast<int>(osName.size()) - nIdentMaxLength;
    CPLAssert(nExtra > 0);

    // Decompose in tokens
    char** papszTokens = CSLTokenizeString2(osName, "_",
                                            CSLT_ALLOWEMPTYTOKENS );
    std::vector< char > achDelimiters;
    std::vector< CPLString > aosTokens;
    for( int j=0; papszTokens[j] != nullptr; ++j )
    {
        const char* pszToken = papszTokens[j];
        bool bIsCamelCase = false;
        // Split parts like camelCase or CamelCase into several tokens
        if( pszToken[0] != '\0' && islower(pszToken[1]) )
        {
            bIsCamelCase = true;
            bool bLastIsLower = true;
            std::vector<CPLString> aoParts;
            CPLString osCurrentPart;
            osCurrentPart += pszToken[0];
            osCurrentPart += pszToken[1];
            for( int k=2; pszToken[k]; ++k)
            {
                if( isupper(pszToken[k]) )
                {
                    if( !bLastIsLower )
                    {
                        bIsCamelCase = false;
                        break;
                    }
                    aoParts.push_back(osCurrentPart);
                    osCurrentPart.clear();
                    bLastIsLower = false;
                }
                else
                {
                    bLastIsLower = true;
                }
                osCurrentPart += pszToken[k];
            }
            if( bIsCamelCase )
            {
                if( !osCurrentPart.empty() )
                    aoParts.push_back(osCurrentPart);
                for( size_t k=0; k<aoParts.size(); ++k )
                {
                    achDelimiters.push_back( (j > 0 && k == 0) ? '_' : '\0' );
                    aosTokens.push_back( aoParts[k] );
                }
            }
        }
        if( !bIsCamelCase )
        {
            achDelimiters.push_back( (j > 0) ? '_' : '\0' );
            aosTokens.push_back( pszToken );
        }
    }
    CSLDestroy(papszTokens);

    // Truncate identifier by removing last character of longest part
    std::map<int, std::set<size_t>> oMapLengthToIdx;
    // Ignore last token in map creation
    for( size_t j=0; j + 1 < aosTokens.size(); ++j )
    {
        const int nTokenLen = static_cast<int>(aosTokens[j].size());
        oMapLengthToIdx[nTokenLen].insert(j);
    }
    int nLastTokenSize = static_cast<int>(aosTokens.back().size());
    if( oMapLengthToIdx.empty() )
    {
        if( nLastTokenSize > nExtra )
        {
            aosTokens[aosTokens.size() - 1].resize( nLastTokenSize - nExtra );
            nExtra = 0;
        }
    }
    else
    {
        bool bHasDoneSomething = true;
        while( nExtra > 0 && bHasDoneSomething )
        {
            bHasDoneSomething = false;
            auto iter = oMapLengthToIdx.end();
            --iter;
            // Avoid truncating last token unless it is excessively longer
            // than previous ones.
            if( nLastTokenSize > 2 * iter->first )
            {
                aosTokens[aosTokens.size() - 1].resize( nLastTokenSize - 1 );
                nLastTokenSize --;
                bHasDoneSomething = true;
                nExtra --;
            }
            else if( iter->first > 1 )
            {
                // Reduce one token by one character
                const size_t j = *iter->second.begin();
                aosTokens[j].resize( iter->first - 1 );

                // Move it to a new bucket
                iter->second.erase(iter->second.begin());
                oMapLengthToIdx[iter->first-1].insert(j);

                // Remove this bucket if is empty
                if( iter->second.empty() )
                {
                    oMapLengthToIdx.erase(iter);
                }

                nExtra --;
                bHasDoneSomething = true;
            }
        }
    }

    // Reassemble truncated parts
    CPLString osNewName;
    for( size_t j=0; j < aosTokens.size(); ++j )
    {
        if( achDelimiters[j] )
            osNewName += achDelimiters[j];
        osNewName += aosTokens[j];
    }

    // If we are still longer than max allowed, truncate beginning of name
    if( nExtra > 0 )
    {
        osNewName = osNewName.substr(nExtra);
    }
    CPLAssert( static_cast<int>(osNewName.size()) == nIdentMaxLength );
    return osNewName;
}


/************************************************************************/
/*                      OGRGMLASAddSerialNumber()                       */
/************************************************************************/

CPLString OGRGMLASAddSerialNumber(const CPLString& osNameIn,
                                  int iOccurrence,
                                  size_t nOccurrences,
                                  int nIdentMaxLength)
{
    CPLString osName(osNameIn);
    const int nDigitsSize = (nOccurrences < 10) ? 1:
                            (nOccurrences < 100) ? 2 : 3;
    char szDigits[4];
    snprintf(szDigits, sizeof(szDigits), "%0*d",
                nDigitsSize, iOccurrence);
    if( nIdentMaxLength >= MIN_VALUE_OF_MAX_IDENTIFIER_LENGTH )
    {
        if( static_cast<int>(osName.size()) < nIdentMaxLength )
        {
            if( static_cast<int>(osName.size()) + nDigitsSize <
                                            nIdentMaxLength )
            {
                osName += szDigits;
            }
            else
            {
                osName.resize(nIdentMaxLength - nDigitsSize);
                osName += szDigits;
            }
        }
        else
        {
            osName.resize(osName.size() - nDigitsSize);
            osName += szDigits;
        }
    }
    else
    {
        osName += szDigits;
    }
    return osName;
}
