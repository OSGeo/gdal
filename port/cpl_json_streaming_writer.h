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

#ifndef CPL_JSON_STREAMING_WRITER_H
#define CPL_JSON_STREAMING_WRITER_H

/*! @cond Doxygen_Suppress */

#if defined(__cplusplus) && !defined(CPL_SUPRESS_CPLUSPLUS)

#include <cstdint>
#include <vector>
#include <string>
#include "cpl_port.h"

class CPL_DLL CPLJSonStreamingWriter
{
public:
    typedef void (*SerializationFuncType)(const char* pszTxt, void* pUserData);

private:
    CPLJSonStreamingWriter(const CPLJSonStreamingWriter&) = delete;
    CPLJSonStreamingWriter& operator=(const CPLJSonStreamingWriter&) = delete;

    std::string m_osStr{};
    SerializationFuncType m_pfnSerializationFunc = nullptr;
    void* m_pUserData = nullptr;
    bool m_bPretty = true;
    std::string m_osIndent = std::string("  ");
    std::string m_osIndentAcc{};
    int m_nLevel = 0;
    bool m_bNewLineEnabled = true;
    struct State
    {
        bool bIsObj = false;
        bool bFirstChild = true;
        explicit State(bool bIsObjIn): bIsObj(bIsObjIn) {}
    };
    std::vector<State> m_states{};
    bool m_bWaitForValue = false;

    void Print(const std::string& text);
    void IncIndent();
    void DecIndent();
    static std::string FormatString(const std::string& str);
    void EmitCommaIfNeeded();

public:
    CPLJSonStreamingWriter(SerializationFuncType pfnSerializationFunc,
                           void* pUserData);
    ~CPLJSonStreamingWriter();

    void SetPrettyFormatting(bool bPretty) { m_bPretty = bPretty; }
    void SetIndentationSize(int nSpaces);

    // cppcheck-suppress functionStatic
    const std::string& GetString() const { return m_osStr; }

    void Add(const std::string& str);
    void Add(const char* pszStr);
    void Add(bool bVal);
    void Add(int nVal) { Add(static_cast<std::int64_t>(nVal)); }
    void Add(unsigned int nVal) { Add(static_cast<std::int64_t>(nVal)); }
    void Add(std::int64_t nVal);
    void Add(std::uint64_t nVal);
    void Add(float fVal, int nPrecision = 9);
    void Add(double dfVal, int nPrecision = 18);
    void AddNull();

    void StartObj();
    void EndObj();
    void AddObjKey(const std::string& key);
    struct CPL_DLL ObjectContext
    {
        CPLJSonStreamingWriter& m_serializer;

        ObjectContext(const ObjectContext &) = delete;
        ObjectContext(ObjectContext&&) = default;

        explicit inline ObjectContext(CPLJSonStreamingWriter& serializer):
            m_serializer(serializer) { m_serializer.StartObj(); }
        ~ObjectContext() { m_serializer.EndObj(); }
    };
    inline ObjectContext MakeObjectContext() { return ObjectContext(*this); }

    void StartArray();
    void EndArray();
    struct CPL_DLL ArrayContext
    {
        CPLJSonStreamingWriter& m_serializer;
        bool m_bForceSingleLine;
        bool m_bNewLineEnabledBackup;

        ArrayContext(const ArrayContext &) = delete;
        ArrayContext(ArrayContext&&) = default;

        inline explicit ArrayContext(CPLJSonStreamingWriter& serializer,
                     bool bForceSingleLine = false):
            m_serializer(serializer),
            m_bForceSingleLine(bForceSingleLine),
            m_bNewLineEnabledBackup(serializer.GetNewLine())
        {
            if( m_bForceSingleLine )
                serializer.SetNewline(false);
            m_serializer.StartArray();

        }
        ~ArrayContext()
        {
            m_serializer.EndArray();
            if( m_bForceSingleLine )
                m_serializer.SetNewline(m_bNewLineEnabledBackup);
        }
    };
    inline ArrayContext MakeArrayContext(bool bForceSingleLine = false)
        { return ArrayContext(*this, bForceSingleLine); }

    bool GetNewLine() const { return m_bNewLineEnabled; }
    void SetNewline(bool bEnabled) { m_bNewLineEnabled = bEnabled; }
};

#endif // __cplusplus

/*! @endcond */

#endif // CPL_JSON_STREAMING_WRITER_H
