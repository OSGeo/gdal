/******************************************************************************
 *
 * Project:  CPL - Common Portability Library
 * Purpose:  JSon streaming parser
 * Author:   Even Rouault, even.rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2017, Even Rouault <even.rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
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
  public:
    CPLJSonStreamingParser();
    virtual ~CPLJSonStreamingParser();

    void SetMaxDepth(size_t nVal);
    void SetMaxStringSize(size_t nVal);

    bool ExceptionOccurred() const
    {
        return m_bExceptionOccurred;
    }

    static std::string GetSerializedString(const char *pszStr);

    virtual void Reset();
    virtual bool Parse(const char *pStr, size_t nLength, bool bFinished);

  protected:
    bool EmitException(const char *pszMessage);
    void StopParsing();

    virtual void String(const char * /*pszValue*/, size_t /*nLength*/)
    {
    }

    virtual void Number(const char * /*pszValue*/, size_t /*nLength*/)
    {
    }

    virtual void Boolean(bool /*b*/)
    {
    }

    virtual void Null()
    {
    }

    virtual void StartObject()
    {
    }

    virtual void EndObject()
    {
    }

    virtual void StartObjectMember(const char * /*pszKey*/, size_t /*nLength*/)
    {
    }

    virtual void StartArray()
    {
    }

    virtual void EndArray()
    {
    }

    virtual void StartArrayMember()
    {
    }

    virtual void Exception(const char * /*pszMessage*/)
    {
    }

  private:
    CPL_DISALLOW_COPY_ASSIGN(CPLJSonStreamingParser)

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

    bool m_bExceptionOccurred = false;
    bool m_bElementFound = false;
    bool m_bStopParsing = false;
    int m_nLastChar = 0;
    int m_nLineCounter = 1;
    int m_nCharCounter = 1;
    std::vector<State> m_aState{};
    std::string m_osToken{};
    enum class ArrayState
    {
        INIT,
        AFTER_COMMA,
        AFTER_VALUE
    };
    std::vector<ArrayState> m_abArrayState{};
    bool m_bInStringEscape = false;
    bool m_bInUnicode = false;
    std::string m_osUnicodeHex{};
    size_t m_nMaxDepth = 1024;
    size_t m_nMaxStringSize = 10000000;

    enum MemberState
    {
        WAITING_KEY,
        IN_KEY,
        KEY_FINISHED,
        IN_VALUE
    };

    std::vector<MemberState> m_aeObjectState{};

    enum State currentState()
    {
        return m_aState.back();
    }

    void SkipSpace(const char *&pStr, size_t &nLength);
    void AdvanceChar(const char *&pStr, size_t &nLength);
    bool EmitUnexpectedChar(char ch, const char *pszExpecting = nullptr);
    bool StartNewToken(const char *&pStr, size_t &nLength);
    bool CheckAndEmitTrueFalseOrNull(char ch);
    bool CheckStackEmpty();
    void DecodeUnicode();
};

#endif  // __cplusplus

/*! @endcond */

#endif  // CPL_JSON_STREAMIN_PARSER_H
