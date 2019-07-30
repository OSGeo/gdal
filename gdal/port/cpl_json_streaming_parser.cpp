/******************************************************************************
 *
 * Project:  CPL - Common Portability Library
 * Purpose:  JSon streaming parser
 * Author:   Even Rouault, even.rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2017, Even Rouault <even.rouault at spatialys.com>
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

/*! @cond Doxygen_Suppress */

#include <assert.h>
#include <ctype.h> // isdigit...
#include <stdio.h> // snprintf
#include <string.h> // strlen
#include <vector>
#include <string>

#include "cpl_conv.h"
#include "cpl_string.h"
#include "cpl_json_streaming_parser.h"

/************************************************************************/
/*                       CPLJSonStreamingParser()                       */
/************************************************************************/

CPLJSonStreamingParser::CPLJSonStreamingParser()
{
    m_aState.push_back(INIT);
}

/************************************************************************/
/*                      ~CPLJSonStreamingParser()                       */
/************************************************************************/

CPLJSonStreamingParser::~CPLJSonStreamingParser()
{
}

/************************************************************************/
/*                           SetMaxDepth()                              */
/************************************************************************/

void CPLJSonStreamingParser::SetMaxDepth(size_t nVal)
{
    m_nMaxDepth = nVal;
}
/************************************************************************/
/*                         SetMaxStringSize()                           */
/************************************************************************/

void CPLJSonStreamingParser::SetMaxStringSize(size_t nVal)
{
    m_nMaxStringSize = nVal;
}

/************************************************************************/
/*                                Reset()                               */
/************************************************************************/

void CPLJSonStreamingParser::Reset()
{
    m_bExceptionOccurred = false;
    m_bElementFound = false;
    m_nLastChar = 0;
    m_nLineCounter = 1;
    m_nCharCounter = 1;
    m_aState.clear();
    m_aState.push_back(INIT);
    m_osToken.clear();
    m_abArrayState.clear();
    m_aeObjectState.clear();
    m_bInStringEscape = false;
    m_bInUnicode = false;
    m_osUnicodeHex.clear();
}

/************************************************************************/
/*                              AdvanceChar()                           */
/************************************************************************/

void CPLJSonStreamingParser::AdvanceChar(const char*& pStr, size_t& nLength)
{
    if( *pStr == 13 && m_nLastChar != 10 )
    {
        m_nLineCounter ++;
        m_nCharCounter = 0;
    }
    else if( *pStr == 10 && m_nLastChar != 13 )
    {
        m_nLineCounter ++;
        m_nCharCounter = 0;
    }
    m_nLastChar = *pStr;

    pStr ++;
    nLength --;
    m_nCharCounter ++;
}

/************************************************************************/
/*                               SkipSpace()                            */
/************************************************************************/

void CPLJSonStreamingParser::SkipSpace(const char*& pStr, size_t& nLength)
{
    while( nLength > 0 && isspace(*pStr) )
    {
        AdvanceChar(pStr, nLength);
    }
}

/************************************************************************/
/*                             EmitException()                          */
/************************************************************************/

bool CPLJSonStreamingParser::EmitException(const char* pszMessage)
{
    m_bExceptionOccurred = true;
    char szMessage[108];
    snprintf(szMessage, sizeof(szMessage),
             "At line %d, character %d: %s",
             m_nLineCounter, m_nCharCounter, pszMessage);
    Exception(szMessage);
    return false;
}

/************************************************************************/
/*                          EmitUnexpectedChar()                        */
/************************************************************************/

bool CPLJSonStreamingParser::EmitUnexpectedChar(char ch,
                                                const char* pszExpecting)
{
    char szMessage[64];
    if( pszExpecting )
    {
        snprintf(szMessage, sizeof(szMessage),
             "Unexpected character (%c). Expecting %s", ch, pszExpecting);
    }
    else
    {
        snprintf(szMessage, sizeof(szMessage),
                "Unexpected character (%c)", ch);
    }
    return EmitException(szMessage);
}

/************************************************************************/
/*                            IsValidNewToken()                         */
/************************************************************************/

static bool IsValidNewToken(char ch)
{
    return ch == '[' || ch == '{' || ch == '"' || ch == '-' ||
           ch == '.' || isdigit(ch) || ch == 't' || ch == 'f' || ch == 'n' ||
           ch == 'i' || ch == 'I' || ch == 'N';
}

/************************************************************************/
/*                             StartNewToken()                          */
/************************************************************************/

bool CPLJSonStreamingParser::StartNewToken(const char*& pStr, size_t& nLength)
{
    char ch = *pStr;
    if( ch == '{' )
    {
        if( m_aState.size() == m_nMaxDepth )
        {
            return EmitException("Too many nested objects and/or arrays");
        }
        StartObject();
        m_aeObjectState.push_back(WAITING_KEY);
        m_aState.push_back(OBJECT);
        AdvanceChar(pStr, nLength);
    }
    else if( ch == '"' )
    {
        m_aState.push_back(STRING);
        AdvanceChar(pStr, nLength);
    }
    else if( ch == '[' )
    {
        if( m_aState.size() == m_nMaxDepth )
        {
            return EmitException("Too many nested objects and/or arrays");
        }
        StartArray();
        m_abArrayState.push_back(ArrayState::INIT);
        m_aState.push_back(ARRAY);
        AdvanceChar(pStr, nLength);
    }
    else if( ch == '-' || ch == '.' || isdigit(ch) ||
             ch == 'i' || ch == 'I' || ch == 'N' )
    {
        m_aState.push_back(NUMBER);
    }
    else if( ch == 't' )
    {
        m_aState.push_back(STATE_TRUE);
    }
    else if( ch == 'f' )
    {
        m_aState.push_back(STATE_FALSE);
    }
    else if( ch == 'n' )
    {
        m_aState.push_back(STATE_NULL); /* might be nan */
    }
    else
    {
        assert( false );
    }
    return true;
}

/************************************************************************/
/*                       CheckAndEmitTrueFalseOrNull()                  */
/************************************************************************/

bool CPLJSonStreamingParser::CheckAndEmitTrueFalseOrNull(char ch)
{
    State eCurState = currentState();

    if( eCurState == STATE_TRUE )
    {
        if( m_osToken == "true" )
        {
            Boolean(true);
        }
        else
        {
            return EmitUnexpectedChar(ch);
        }
    }
    else if( eCurState == STATE_FALSE)
    {
        if( m_osToken == "false" )
        {
            Boolean(false);
        }
        else
        {
            return EmitUnexpectedChar(ch);
        }
    }
    else /* if( eCurState == STATE_NULL ) */
    {
        if( m_osToken == "null" )
        {
            Null();
        }
        else
        {
            return EmitUnexpectedChar(ch);
        }
    }
    m_aState.pop_back();
    m_osToken.clear();
    return true;
}

/************************************************************************/
/*                           CheckStackEmpty()                          */
/************************************************************************/

bool CPLJSonStreamingParser::CheckStackEmpty()
{
    if( !m_aeObjectState.empty() )
    {
        return EmitException("Unterminated object");
    }
    else if( !m_abArrayState.empty() )
    {
        return EmitException("Unterminated array");
    }
    return true;
}

/************************************************************************/
/*                           IsHighSurrogate()                          */
/************************************************************************/

static bool IsHighSurrogate(unsigned uc)
{
    return (uc & 0xFC00) == 0xD800;
}

/************************************************************************/
/*                           IsLowSurrogate()                           */
/************************************************************************/

static bool IsLowSurrogate(unsigned uc)
{
    return (uc & 0xFC00) == 0xDC00;
}

/************************************************************************/
/*                         GetSurrogatePair()                           */
/************************************************************************/

static unsigned GetSurrogatePair(unsigned hi, unsigned lo)
{
    return ((hi & 0x3FF) << 10) + (lo & 0x3FF) + 0x10000;
}

/************************************************************************/
/*                            IsHexDigit()                              */
/************************************************************************/

static bool IsHexDigit(char ch)
{
    return (ch >= '0' && ch <= '9') ||
           (ch >= 'a' && ch <= 'f') ||
           (ch >= 'A' && ch <= 'F');
}

/************************************************************************/
/*                           HexToDecimal()                             */
/************************************************************************/

static unsigned HexToDecimal(char ch)
{
    if( ch >= '0' && ch <= '9' )
        return ch - '0';
    if (ch >= 'a' && ch <= 'f' )
        return 10 + ch - 'a';
    //if (ch >= 'A' && ch <= 'F' )
    return 10 + ch - 'A';
}

/************************************************************************/
/*                            getUCSChar()                              */
/************************************************************************/

static unsigned getUCSChar(const std::string& unicode4HexChar)
{
    return (HexToDecimal(unicode4HexChar[0]) << 12) |
           (HexToDecimal(unicode4HexChar[1]) << 8) |
           (HexToDecimal(unicode4HexChar[2]) << 4) |
           (HexToDecimal(unicode4HexChar[3]));
}

/************************************************************************/
/*                           DecodeUnicode()                            */
/************************************************************************/

void CPLJSonStreamingParser::DecodeUnicode()
{
    constexpr char szReplacementUTF8[] = "\xEF\xBF\xBD";
    unsigned nUCSChar;
    if( m_osUnicodeHex.size() == 8 )
    {
        unsigned nUCSHigh = getUCSChar(m_osUnicodeHex);
        assert( IsHighSurrogate(nUCSHigh) );
        unsigned nUCSLow = getUCSChar(m_osUnicodeHex.substr(4));
        if( IsLowSurrogate(nUCSLow) )
        {
            nUCSChar = GetSurrogatePair(nUCSHigh, nUCSLow);
        }
        else
        {
            /* Invalid code point. Insert the replacement char */
            nUCSChar = 0xFFFFFFFFU;
        }
    }
    else
    {
        assert( m_osUnicodeHex.size() == 4 );
        nUCSChar = getUCSChar(m_osUnicodeHex);
    }

    if( nUCSChar < 0x80)
    {
        m_osToken += static_cast<char>(nUCSChar);
    }
    else if( nUCSChar < 0x800)
    {
        m_osToken += static_cast<char>(0xC0 | (nUCSChar >> 6));
        m_osToken += static_cast<char>(0x80 | (nUCSChar & 0x3F));
    }
    else if (IsLowSurrogate(nUCSChar) ||
             IsHighSurrogate(nUCSChar) )
    {
        /* Invalid code point. Insert the replacement char */
        m_osToken += szReplacementUTF8;
    }
    else if (nUCSChar < 0x10000)
    {
        m_osToken += static_cast<char>(0xE0 | (nUCSChar >> 12));
        m_osToken += static_cast<char>(0x80 | ((nUCSChar >> 6) & 0x3F));
        m_osToken += static_cast<char>(0x80 | (nUCSChar & 0x3F));
    }
    else if (nUCSChar < 0x110000)
    {
        m_osToken += static_cast<char>(0xF0 | ((nUCSChar >> 18) & 0x07));
        m_osToken += static_cast<char>(0x80 | ((nUCSChar >> 12) & 0x3F));
        m_osToken += static_cast<char>(0x80 | ((nUCSChar >> 6) & 0x3F));
        m_osToken += static_cast<char>(0x80 | (nUCSChar & 0x3F));
    }
    else
    {
        /* Invalid code point. Insert the replacement char */
        m_osToken += szReplacementUTF8;
    }

    m_bInUnicode = false;
    m_osUnicodeHex.clear();
}

/************************************************************************/
/*                              Parse()                                 */
/************************************************************************/

bool CPLJSonStreamingParser::Parse(const char* pStr, size_t nLength,
                                   bool bFinished)
{
    if( m_bExceptionOccurred )
        return false;

    while( true )
    {
        State eCurState = currentState();
        if( eCurState == INIT )
        {
            SkipSpace(pStr, nLength);
            if( nLength == 0 )
                return true;
            if( m_bElementFound || !IsValidNewToken(*pStr) )
            {
                return EmitUnexpectedChar(*pStr);
            }
            if( !StartNewToken(pStr, nLength) )
            {
                return false;
            }
            m_bElementFound = true;
        }
        else if( eCurState == NUMBER )
        {
            while(nLength)
            {
                char ch = *pStr;
                if( ch == '+' || ch == '-' || isdigit(ch) ||
                    ch == '.' || ch == 'e' || ch == 'E' )
                {
                    if( m_osToken.size() == 1024 )
                    {
                        return EmitException("Too many characters in number");
                    }
                    m_osToken += ch;
                }
                else if( isspace(ch) || ch == ',' || ch == '}' || ch == ']' )
                {
                    SkipSpace(pStr, nLength);
                    break;
                }
                else
                {
                    CPLString extendedToken(m_osToken + ch);
                    if( (STARTS_WITH_CI("Infinity", extendedToken) &&
                          m_osToken.size() + 1 <= strlen("Infinity")) ||
                         (STARTS_WITH_CI("-Infinity", extendedToken) &&
                          m_osToken.size() + 1 <= strlen("-Infinity")) ||
                         (STARTS_WITH_CI("NaN", extendedToken) &&
                          m_osToken.size() + 1 <= strlen("NaN")) )
                    {
                        m_osToken += ch;
                    }
                    else
                    {
                        return EmitUnexpectedChar(ch);
                    }
                }
                AdvanceChar(pStr, nLength);
            }

            if( nLength != 0 || bFinished )
            {
                const char firstCh = m_osToken[0];
                if( firstCh == 'i' || firstCh == 'I' )
                {
                    if( !EQUAL(m_osToken.c_str(), "Infinity") )
                    {
                        return EmitException("Invalid number");
                    }
                }
                else if( firstCh == '-' )
                {
                    if( m_osToken[1] == 'i' || m_osToken[1] == 'I' )
                    {
                        if( !EQUAL(m_osToken.c_str(), "-Infinity") )
                        {
                            return EmitException("Invalid number");
                        }
                    }
                }
                else if( firstCh == 'n' || firstCh == 'N' )
                {
                    if( m_osToken[1] == 'a' || m_osToken[1] == 'A' )
                    {
                        if( !EQUAL(m_osToken.c_str(), "NaN") )
                        {
                            return EmitException("Invalid number");
                        }
                    }
                }

                Number(m_osToken.c_str(), m_osToken.size());
                m_osToken.clear();
                m_aState.pop_back();
            }

            if( nLength == 0 )
            {
                if( bFinished )
                {
                    return CheckStackEmpty();
                }
                return true;
            }
        }
        else if( eCurState == STRING )
        {
            bool bEOS = false;
            while( nLength )
            {
                if( m_osToken.size() == m_nMaxStringSize )
                {
                    return EmitException("Too many characters in number");
                }

                char ch = *pStr;
                if( m_bInUnicode)
                {
                    if( m_osUnicodeHex.size() == 8 )
                    {
                        DecodeUnicode();
                    }
                    else if( m_osUnicodeHex.size() == 4 )
                    {
                        /* Start of next surrogate pair ? */
                        if( m_nLastChar == '\\' )
                        {
                            if( ch == 'u' )
                            {
                                AdvanceChar(pStr, nLength);
                                continue;
                            }
                            else
                            {
                                /* will be replacement character */
                                DecodeUnicode();
                                m_bInStringEscape = true;
                            }
                        }
                        else if( m_nLastChar == 'u' )
                        {
                            if( IsHexDigit(ch) )
                            {
                                m_osUnicodeHex += ch;
                            }
                            else
                            {
                                char szMessage[64];
                                snprintf(szMessage, sizeof(szMessage),
                                    "Illegal character in unicode "
                                    "sequence (\\%c)", ch);
                                return EmitException(szMessage);
                            }
                            AdvanceChar(pStr, nLength);
                            continue;
                        }
                        else if( ch == '\\' )
                        {
                            AdvanceChar(pStr, nLength);
                            continue;
                        }
                        else
                        {
                            /* will be replacement character */
                            DecodeUnicode();
                        }
                    }
                    else
                    {
                        if( IsHexDigit(ch) )
                        {
                            m_osUnicodeHex += ch;
                            if( m_osUnicodeHex.size() == 4 &&
                                !IsHighSurrogate(getUCSChar(m_osUnicodeHex)) )
                            {
                                DecodeUnicode();
                            }
                        }
                        else
                        {
                            char szMessage[64];
                            snprintf(szMessage, sizeof(szMessage),
                                "Illegal character in unicode "
                                "sequence (\\%c)", ch);
                            return EmitException(szMessage);
                        }
                        AdvanceChar(pStr, nLength);
                        continue;
                    }
                }

                if( m_bInStringEscape )
                {
                    if( ch == '"' || ch == '\\' || ch == '/' )
                        m_osToken += ch;
                    else if( ch == 'b' )
                        m_osToken += '\b';
                    else if( ch == 'f' )
                        m_osToken += '\f';
                    else if( ch == 'n' )
                        m_osToken += '\n';
                    else if( ch == 'r' )
                        m_osToken += '\r';
                    else if( ch == 't' )
                        m_osToken += '\t';
                    else if( ch == 'u' )
                    {
                        m_bInUnicode = true;
                    }
                    else
                    {
                        char szMessage[32];
                        snprintf(szMessage, sizeof(szMessage),
                                 "Illegal escape sequence (\\%c)", ch);
                        return EmitException(szMessage);
                    }
                    m_bInStringEscape = false;
                    AdvanceChar(pStr, nLength);
                    continue;
                }
                else if( ch == '\\' )
                {
                    m_bInStringEscape = true;
                    AdvanceChar(pStr, nLength);
                    continue;
                }
                else if( ch == '"' )
                {
                    bEOS = true;
                    AdvanceChar(pStr, nLength);
                    SkipSpace(pStr, nLength);

                    if( !m_aeObjectState.empty() &&
                        m_aeObjectState.back() == IN_KEY )
                    {
                        StartObjectMember(m_osToken.c_str(), m_osToken.size());
                    }
                    else
                    {
                        String(m_osToken.c_str(), m_osToken.size());
                    }
                    m_osToken.clear();
                    m_aState.pop_back();

                    break;
                }

                m_osToken += ch;
                AdvanceChar(pStr, nLength);
            }

            if( nLength == 0 )
            {
                if( bFinished )
                {
                    if( !bEOS )
                    {
                        return EmitException("Unterminated string");
                    }
                    return CheckStackEmpty();
                }
                return true;
            }
        }
        else if( eCurState == ARRAY )
        {
            SkipSpace(pStr, nLength);
            if( nLength == 0 )
            {
                if( bFinished )
                {
                    return EmitException("Unterminated array");
                }
                return true;
            }

            char ch = *pStr;
            if( ch == ',' )
            {
                if( m_abArrayState.back() != ArrayState::AFTER_VALUE )
                {
                    return EmitUnexpectedChar(ch, "','");
                }
                m_abArrayState.back() = ArrayState::AFTER_COMMA;
                AdvanceChar(pStr, nLength);
            }
            else if( ch == ']' )
            {
                if( m_abArrayState.back() == ArrayState::AFTER_COMMA)
                {
                    return EmitException("Missing value");
                }

                EndArray();
                AdvanceChar(pStr, nLength);
                m_abArrayState.pop_back();
                m_aState.pop_back();
            }
            else if( IsValidNewToken(ch) )
            {
                if( m_abArrayState.back() == ArrayState::AFTER_VALUE )
                {
                    return EmitException("Unexpected state: ',' or ']' expected");
                }
                m_abArrayState.back() = ArrayState::AFTER_VALUE;

                StartArrayMember();
                if( !StartNewToken(pStr, nLength) )
                {
                    return false;
                }
            }
            else
            {
                return EmitUnexpectedChar(ch);
            }
        }
        else if( eCurState == OBJECT )
        {
            SkipSpace(pStr, nLength);
            if( nLength == 0 )
            {
                if( bFinished )
                {
                    return EmitException("Unterminated object");
                }
                return true;
            }

            char ch = *pStr;
            if( ch == ',' )
            {
                if( m_aeObjectState.back() != IN_VALUE )
                {
                    return EmitUnexpectedChar(ch, "','");
                }

                m_aeObjectState.back() = WAITING_KEY;
                AdvanceChar(pStr, nLength);
            }
            else if( ch == ':' )
            {
                if( m_aeObjectState.back() != IN_KEY )
                {
                    return EmitUnexpectedChar(ch, "':'");
                }
                m_aeObjectState.back() = KEY_FINISHED;
                AdvanceChar(pStr, nLength);
            }
            else if( ch == '}' )
            {
                if( m_aeObjectState.back() == WAITING_KEY ||
                    m_aeObjectState.back() == IN_VALUE )
                {
                    // nothing
                }
                else
                {
                    return EmitException("Missing value");
                }

                EndObject();
                AdvanceChar(pStr, nLength);
                m_aeObjectState.pop_back();
                m_aState.pop_back();
            }
            else if( IsValidNewToken(ch) )
            {
                if( m_aeObjectState.back() == WAITING_KEY )
                {
                    if( ch != '"' )
                    {
                        return EmitUnexpectedChar(ch, "'\"'");
                    }
                     m_aeObjectState.back() = IN_KEY;
                }
                else if( m_aeObjectState.back() == KEY_FINISHED )
                {
                    m_aeObjectState.back() = IN_VALUE;
                }
                else
                {
                    return EmitException("Unexpected state");
                }
                if( !StartNewToken(pStr, nLength) )
                {
                    return false;
                }
            }
            else
            {
                return EmitUnexpectedChar(ch);
            }
        }
        else /* if( eCurState == STATE_TRUE || eCurState == STATE_FALSE ||
                    eCurState == STATE_NULL ) */
        {
            while(nLength)
            {
                char ch = *pStr;
                if( eCurState == STATE_NULL && (ch == 'a' || ch == 'A') &&
                    m_osToken.size() == 1 )
                {
                    m_aState.back() = NUMBER;
                    break;
                }
                if( isalpha(ch) )
                {
                    m_osToken += ch;
                    if( eCurState == STATE_TRUE &&
                        (m_osToken.size() > strlen("true") ||
                        memcmp(m_osToken.c_str(), "true",
                               m_osToken.size()) != 0) )
                    {
                        return EmitUnexpectedChar(*pStr);
                    }
                    else if( eCurState == STATE_FALSE &&
                        (m_osToken.size() > strlen("false") ||
                        memcmp(m_osToken.c_str(), "false",
                               m_osToken.size()) != 0) )
                    {
                        return EmitUnexpectedChar(*pStr);
                    }
                    else if( eCurState == STATE_NULL &&
                        (m_osToken.size() > strlen("null") ||
                        memcmp(m_osToken.c_str(), "null",
                               m_osToken.size()) != 0) )
                    {
                        return EmitUnexpectedChar(*pStr);
                    }
                }
                else if( isspace(ch) || ch == ',' || ch == '}' || ch == ']' )
                {
                    SkipSpace(pStr, nLength);
                    break;
                }
                else
                {
                    return EmitUnexpectedChar(ch);
                }
                AdvanceChar(pStr, nLength);
            }
            if( m_aState.back() == NUMBER )
            {
                continue;
            }
            if( nLength == 0 )
            {
                if( bFinished )
                {
                    if( !CheckAndEmitTrueFalseOrNull(0) )
                        return false;
                    return CheckStackEmpty();
                }
                return true;
            }

            if( !CheckAndEmitTrueFalseOrNull(*pStr) )
                return false;
        }
    }
}


/************************************************************************/
/*                       GetSerializedString()                          */
/************************************************************************/

std::string CPLJSonStreamingParser::GetSerializedString(const char* pszStr)
{
    std::string osStr("\"");
    for( int i = 0; pszStr[i]; i++ )
    {
        char ch = pszStr[i];
        if( ch == '\b' )
            osStr += "\\b";
        else if( ch == '\f' )
            osStr += "\\f";
        else if( ch == '\n' )
            osStr += "\\n";
        else if( ch == '\r' )
            osStr += "\\r";
        else if( ch == '\t' )
            osStr += "\\t";
        else if( ch == '"' )
            osStr += "\\\"";
        else if( ch == '\\' )
            osStr += "\\\\";
        else if( static_cast<unsigned char>(ch) < ' ' )
            osStr += CPLSPrintf("\\u%04X", ch);
        else
            osStr += ch;
    }
    osStr += "\"";
    return osStr;
}

/*! @endcond */
