/******************************************************************************
 *
 * Project:  CPL - Common Portability Library
 * Purpose:  JSon streaming writer
 * Author:   Even Rouault, even.rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2019, Even Rouault <even.rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

/*! @cond Doxygen_Suppress */

#include <cmath>
#include <vector>
#include <string>

#include "cpl_conv.h"
#include "cpl_float.h"
#include "cpl_string.h"
#include "cpl_json_streaming_writer.h"

CPLJSonStreamingWriter::CPLJSonStreamingWriter(
    SerializationFuncType pfnSerializationFunc, void *pUserData)
    : m_pfnSerializationFunc(pfnSerializationFunc), m_pUserData(pUserData)
{
}

CPLJSonStreamingWriter::~CPLJSonStreamingWriter()
{
    CPLAssert(m_nLevel == 0);
    CPLAssert(m_states.empty());
}

void CPLJSonStreamingWriter::clear()
{
    m_nLevel = 0;
    m_osStr.clear();
    m_osIndentAcc.clear();
    m_states.clear();
    m_bWaitForValue = false;
}

void CPLJSonStreamingWriter::Serialize(const std::string_view &str)
{
    if (m_pfnSerializationFunc)
    {
        m_osTmpForSerialize = str;
        m_pfnSerializationFunc(m_osTmpForSerialize.c_str(), m_pUserData);
    }
    else
    {
        m_osStr.append(str);
    }
}

void CPLJSonStreamingWriter::Serialize(const char *pszStr, size_t nLength)
{
    Serialize(std::string_view(pszStr, nLength));
}

void CPLJSonStreamingWriter::SetIndentationSize(int nSpaces)
{
    CPLAssert(m_nLevel == 0);
    m_osIndent.clear();
    m_osIndent.resize(nSpaces, ' ');
}

void CPLJSonStreamingWriter::IncIndent()
{
    m_nLevel++;
    if (m_bPretty)
        m_osIndentAcc += m_osIndent;
}

void CPLJSonStreamingWriter::DecIndent()
{
    CPLAssert(m_nLevel > 0);
    m_nLevel--;
    if (m_bPretty)
        m_osIndentAcc.resize(m_osIndentAcc.size() - m_osIndent.size());
}

const std::string &
CPLJSonStreamingWriter::FormatString(const std::string_view &str)
{
    m_osTmpForFormatString.clear();
    m_osTmpForFormatString += '"';
    for (char ch : str)
    {
        switch (ch)
        {
            case '"':
                m_osTmpForFormatString += "\\\"";
                break;
            case '\\':
                m_osTmpForFormatString += "\\\\";
                break;
            case '\b':
                m_osTmpForFormatString += "\\b";
                break;
            case '\f':
                m_osTmpForFormatString += "\\f";
                break;
            case '\n':
                m_osTmpForFormatString += "\\n";
                break;
            case '\r':
                m_osTmpForFormatString += "\\r";
                break;
            case '\t':
                m_osTmpForFormatString += "\\t";
                break;
            default:
                if (static_cast<unsigned char>(ch) < ' ')
                    m_osTmpForFormatString += CPLSPrintf("\\u%04X", ch);
                else
                    m_osTmpForFormatString += ch;
                break;
        }
    }
    m_osTmpForFormatString += '"';
    return m_osTmpForFormatString;
}

void CPLJSonStreamingWriter::EmitCommaIfNeeded()
{
    if (m_bWaitForValue)
    {
        m_bWaitForValue = false;
    }
    else if (!m_states.empty())
    {
        if (!m_states.back().bFirstChild)
        {
            Serialize(",", 1);
            if (m_bPretty && !m_bNewLineEnabled)
                Serialize(" ", 1);
        }
        if (m_bPretty && m_bNewLineEnabled)
        {
            Serialize("\n", 1);
            Serialize(m_osIndentAcc.c_str(), m_osIndentAcc.size());
        }
        m_states.back().bFirstChild = false;
    }
}

void CPLJSonStreamingWriter::StartObj()
{
    EmitCommaIfNeeded();
    Serialize("{", 1);
    IncIndent();
    m_states.emplace_back(State(true));
}

void CPLJSonStreamingWriter::EndObj()
{
    CPLAssert(!m_bWaitForValue);
    CPLAssert(!m_states.empty());
    CPLAssert(m_states.back().bIsObj);
    DecIndent();
    if (!m_states.back().bFirstChild)
    {
        if (m_bPretty && m_bNewLineEnabled)
        {
            Serialize("\n", 1);
            Serialize(m_osIndentAcc.c_str(), m_osIndentAcc.size());
        }
    }
    m_states.pop_back();
    Serialize("}", 1);
}

void CPLJSonStreamingWriter::StartArray()
{
    EmitCommaIfNeeded();
    Serialize("[", 1);
    IncIndent();
    m_states.emplace_back(State(false));
}

void CPLJSonStreamingWriter::EndArray()
{
    CPLAssert(!m_states.empty());
    CPLAssert(!m_states.back().bIsObj);
    DecIndent();
    if (!m_states.back().bFirstChild)
    {
        if (m_bPretty && m_bNewLineEnabled)
        {
            Serialize("\n", 1);
            Serialize(m_osIndentAcc.c_str(), m_osIndentAcc.size());
        }
    }
    m_states.pop_back();
    Serialize("]", 1);
}

void CPLJSonStreamingWriter::AddObjKey(const std::string_view &key)
{
    CPLAssert(!m_states.empty());
    CPLAssert(m_states.back().bIsObj);
    CPLAssert(!m_bWaitForValue);
    EmitCommaIfNeeded();
    Serialize(FormatString(key));
    if (m_bPretty)
        Serialize(": ", 2);
    else
        Serialize(":", 1);
    m_bWaitForValue = true;
}

void CPLJSonStreamingWriter::Add(bool bVal)
{
    EmitCommaIfNeeded();
    Serialize(bVal ? "true" : "false", bVal ? 4 : 5);
}

void CPLJSonStreamingWriter::Add(const char *pszStr)
{
    EmitCommaIfNeeded();
    Serialize(FormatString(std::string_view(pszStr)));
}

void CPLJSonStreamingWriter::Add(const std::string_view &str)
{
    EmitCommaIfNeeded();
    Serialize(FormatString(str));
}

void CPLJSonStreamingWriter::Add(const std::string &str)
{
    EmitCommaIfNeeded();
    Serialize(FormatString(str));
}

void CPLJSonStreamingWriter::AddSerializedValue(const std::string_view &str)
{
    EmitCommaIfNeeded();
    Serialize(str);
}

void CPLJSonStreamingWriter::Add(std::int64_t nVal)
{
    EmitCommaIfNeeded();
    Serialize(CPLSPrintf(CPL_FRMT_GIB, static_cast<GIntBig>(nVal)));
}

void CPLJSonStreamingWriter::Add(std::uint64_t nVal)
{
    EmitCommaIfNeeded();
    Serialize(CPLSPrintf(CPL_FRMT_GUIB, static_cast<GUIntBig>(nVal)));
}

void CPLJSonStreamingWriter::Add(GFloat16 hfVal, int nPrecision)
{
    EmitCommaIfNeeded();
    if (CPLIsNan(hfVal))
    {
        Serialize("\"NaN\"", 5);
    }
    else if (CPLIsInf(hfVal))
    {
        Serialize(hfVal > 0 ? "\"Infinity\"" : "\"-Infinity\"",
                  hfVal > 0 ? 10 : 11);
    }
    else
    {
        char szFormatting[10];
        snprintf(szFormatting, sizeof(szFormatting), "%%.%dg", nPrecision);
        Serialize(CPLSPrintf(szFormatting, float(hfVal)));
    }
}

void CPLJSonStreamingWriter::Add(float fVal, int nPrecision)
{
    EmitCommaIfNeeded();
    if (std::isnan(fVal))
    {
        Serialize("\"NaN\"", 5);
    }
    else if (std::isinf(fVal))
    {
        Serialize(fVal > 0 ? "\"Infinity\"" : "\"-Infinity\"",
                  fVal > 0 ? 10 : 11);
    }
    else
    {
        char szFormatting[10];
        snprintf(szFormatting, sizeof(szFormatting), "%%.%dg", nPrecision);
        Serialize(CPLSPrintf(szFormatting, fVal));
    }
}

void CPLJSonStreamingWriter::Add(double dfVal, int nPrecision)
{
    EmitCommaIfNeeded();
    if (std::isnan(dfVal))
    {
        Serialize("\"NaN\"", 5);
    }
    else if (std::isinf(dfVal))
    {
        Serialize(dfVal > 0 ? "\"Infinity\"" : "\"-Infinity\"");
    }
    else
    {
        char szFormatting[10];
        snprintf(szFormatting, sizeof(szFormatting), "%%.%dg", nPrecision);
        Serialize(CPLSPrintf(szFormatting, dfVal));
    }
}

void CPLJSonStreamingWriter::AddNull()
{
    EmitCommaIfNeeded();
    Serialize("null", 4);
}

/*! @endcond */
