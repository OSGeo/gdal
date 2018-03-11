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

#ifndef CPL_JSON_STREAMIN_PARSER_H
#define CPL_JSON_STREAMIN_PARSER_H

/*! @cond Doxygen_Suppress */

#if defined(__cplusplus) && !defined(CPL_SUPRESS_CPLUSPLUS)

#include <vector>
#include <string>
#include "cpl_port.h"

class CPL_DLL CPLJSonStreamingParser
{
        enum State
        {
            INIT,
            OBJECT,
            ARRAY,
            STRING,
            NUMBER,
            STATE_TRUE,
            STATE_FALSE,
            STATE_NULL
        };

        bool m_bExceptionOccurred;
        bool m_bElementFound;
        int m_nLastChar;
        int m_nLineCounter;
        int m_nCharCounter;
        std::vector<State> m_aState;
        std::string m_osToken;
        std::vector<bool> m_abFirstElement;
        bool m_bInStringEscape;
        bool m_bInUnicode;
        std::string m_osUnicodeHex;
        size_t m_nMaxDepth;
        size_t m_nMaxStringSize;

        enum MemberState
        {
            WAITING_KEY,
            IN_KEY,
            KEY_FINISHED,
            IN_VALUE
        };
        std::vector<MemberState> m_aeObjectState;

        enum State currentState() { return m_aState.back(); }
        void SkipSpace(const char*& pStr, size_t& nLength);
        void AdvanceChar(const char*& pStr, size_t& nLength);
        bool EmitException(const char* pszMessage);
        bool EmitUnexpectedChar(char ch);
        bool StartNewToken(const char*& pStr, size_t& nLength);
        bool CheckAndEmitTrueFalseOrNull(char ch);
        bool CheckStackEmpty();
        void DecodeUnicode();

    public:
        CPLJSonStreamingParser();
        virtual ~CPLJSonStreamingParser();

        void SetMaxDepth(size_t nVal);
        void SetMaxStringSize(size_t nVal);
        bool ExceptionOccurred() const { return m_bExceptionOccurred; }

        static std::string GetSerializedString(const char* pszStr);

        virtual void Reset();
        virtual bool Parse(const char* pStr, size_t nLength, bool bFinished);

        virtual void String(const char* /*pszValue*/, size_t /*nLength*/) {}
        virtual void Number(const char* /*pszValue*/, size_t /*nLength*/) {}
        virtual void Boolean(bool /*b*/) {}
        virtual void Null() {}

        virtual void StartObject() {}
        virtual void EndObject() {}
        virtual void StartObjectMember(const char* /*pszKey*/, size_t /*nLength*/) {}

        virtual void StartArray() {}
        virtual void EndArray() {}
        virtual void StartArrayMember() {}

        virtual void Exception(const char* /*pszMessage*/) {}
};

#endif // __cplusplus

/*! @endcond */

#endif // CPL_JSON_STREAMIN_PARSER_H
