/******************************************************************************
 *
 * Project:  CPL - Common Portability Library
 * Purpose:  JSon streaming writer
 * Author:   Even Rouault, even.rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2019, Even Rouault <even.rouault at spatialys.com>
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

#include <vector>
#include <string>

#include "cpl_conv.h"
#include "cpl_string.h"
#include "cpl_json_streaming_writer.h"

CPLJSonStreamingWriter::CPLJSonStreamingWriter(
                    SerializationFuncType pfnSerializationFunc,
                    void* pUserData):
    m_pfnSerializationFunc(pfnSerializationFunc),
    m_pUserData(pUserData)
{}

CPLJSonStreamingWriter::~CPLJSonStreamingWriter()
{
    CPLAssert( m_nLevel == 0 );
    CPLAssert( m_states.empty() );
}

void CPLJSonStreamingWriter::Print(const std::string& text)
{
    if( m_pfnSerializationFunc )
    {
        m_pfnSerializationFunc(text.c_str(), m_pUserData);
    }
    else
    {
        m_osStr += text;
    }
}

void CPLJSonStreamingWriter::SetIndentationSize(int nSpaces)
{
    CPLAssert( m_nLevel == 0 );
    m_osIndent.clear();
    m_osIndent.resize(nSpaces, ' ');
}

void CPLJSonStreamingWriter::IncIndent()
{
    m_nLevel ++;
    if( m_bPretty )
        m_osIndentAcc += m_osIndent;
}

void CPLJSonStreamingWriter::DecIndent()
{
    CPLAssert(m_nLevel > 0);
    m_nLevel --;
    if( m_bPretty )
        m_osIndentAcc.resize(m_osIndentAcc.size() - m_osIndent.size());
}

std::string CPLJSonStreamingWriter::FormatString(const std::string& str)
{
    std::string ret;
    ret += '"';
    for( char ch: str )
    {
        switch(ch)
        {
            case '"' : ret += "\\\""; break;
            case '\\': ret += "\\\\"; break;
            case '\b': ret += "\\b";  break;
            case '\f': ret += "\\f";  break;
            case '\n': ret += "\\n";  break;
            case '\r': ret += "\\r";  break;
            case '\t': ret += "\\t";  break;
            default: 
                if( static_cast<unsigned char>(ch) < ' ' )
                    ret += CPLSPrintf("\\u%04X", ch);
                else
                    ret += ch;
                break;
        }
    }
    ret += '"';
    return ret;
}

void CPLJSonStreamingWriter::EmitCommaIfNeeded()
{
    if( m_bWaitForValue )
    {
        m_bWaitForValue = false;
    }
    else if( !m_states.empty() )
    {
        if( !m_states.back().bFirstChild )
        {
            Print(",");
            if( m_bPretty && !m_bNewLineEnabled )
                Print(" ");
        }
        if( m_bPretty && m_bNewLineEnabled )
        {
            Print("\n");
            Print(m_osIndentAcc);
        }
        m_states.back().bFirstChild = false;
    }
}

void CPLJSonStreamingWriter::StartObj()
{
    EmitCommaIfNeeded();
    Print("{");
    IncIndent();
    m_states.emplace_back(State(true));
}

void CPLJSonStreamingWriter::EndObj()
{
    CPLAssert(!m_bWaitForValue);
    CPLAssert( !m_states.empty() );
    CPLAssert( m_states.back().bIsObj );
    DecIndent();
    if( !m_states.back().bFirstChild )
    {
        if( m_bPretty && m_bNewLineEnabled )
        {
            Print("\n");
            Print(m_osIndentAcc);
        }
    }
    m_states.pop_back();
    Print("}");
}

void CPLJSonStreamingWriter::StartArray()
{
    EmitCommaIfNeeded();
    Print("[");
    IncIndent();
    m_states.emplace_back(State(false));
}

void CPLJSonStreamingWriter::EndArray()
{
    CPLAssert( !m_states.empty() );
    CPLAssert( !m_states.back().bIsObj );
    DecIndent();
    if( !m_states.back().bFirstChild )
    {
        if( m_bPretty && m_bNewLineEnabled )
        {
            Print("\n");
            Print(m_osIndentAcc);
        }
    }
    m_states.pop_back();
    Print("]");
}

void CPLJSonStreamingWriter::AddObjKey(const std::string& key)
{
    CPLAssert( !m_states.empty() );
    CPLAssert( m_states.back().bIsObj );
    CPLAssert(!m_bWaitForValue);
    EmitCommaIfNeeded();
    Print(FormatString(key));
    Print(m_bPretty ? ": " : ":");
    m_bWaitForValue = true;
}

void CPLJSonStreamingWriter::Add(bool bVal)
{
    EmitCommaIfNeeded();
    Print(bVal ? "true" : "false");
}

void CPLJSonStreamingWriter::Add(const std::string& str)
{
    EmitCommaIfNeeded();
    Print(FormatString(str));
}

void CPLJSonStreamingWriter::Add(const char* pszStr)
{
    EmitCommaIfNeeded();
    Print(FormatString(pszStr));
}

void CPLJSonStreamingWriter::Add(GIntBig nVal)
{
    EmitCommaIfNeeded();
    Print(CPLSPrintf(CPL_FRMT_GIB, nVal));
}

void CPLJSonStreamingWriter::Add(GUInt64 nVal)
{
    EmitCommaIfNeeded();
    Print(CPLSPrintf(CPL_FRMT_GUIB, static_cast<GUIntBig>(nVal)));
}

void CPLJSonStreamingWriter::Add(float fVal, int nPrecision)
{
    EmitCommaIfNeeded();
    if( CPLIsNan(fVal) )
    {
        Print("\"NaN\"");
    }
    else if( CPLIsInf(fVal) )
    {
        Print( fVal > 0 ? "\"Infinity\"" : "\"-Infinity\"" );
    }
    else
    {
        char szFormatting[10];
        snprintf(szFormatting, sizeof(szFormatting), "%%.%dg", nPrecision);
        Print(CPLSPrintf(szFormatting, fVal));
    }
}

void CPLJSonStreamingWriter::Add(double dfVal, int nPrecision)
{
    EmitCommaIfNeeded();
    if( CPLIsNan(dfVal) )
    {
       Print("\"NaN\"");
    }
    else if( CPLIsInf(dfVal) )
    {
        Print( dfVal > 0 ? "\"Infinity\"" : "\"-Infinity\"" );
    }
    else
    {
        char szFormatting[10];
        snprintf(szFormatting, sizeof(szFormatting), "%%.%dg", nPrecision);
        Print(CPLSPrintf(szFormatting, dfVal));
    }
}

void CPLJSonStreamingWriter::AddNull()
{
    EmitCommaIfNeeded();
    Print("null");
}

/*! @endcond */
