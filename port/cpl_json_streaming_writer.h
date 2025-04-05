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

#ifndef CPL_JSON_STREAMING_WRITER_H
#define CPL_JSON_STREAMING_WRITER_H

/*! @cond Doxygen_Suppress */

#if defined(__cplusplus) && !defined(CPL_SUPRESS_CPLUSPLUS)

#include <cstdint>
#include <vector>
#include <string>

#include "cpl_float.h"
#include "cpl_port.h"

class CPL_DLL CPLJSonStreamingWriter
{
  public:
    typedef void (*SerializationFuncType)(const char *pszTxt, void *pUserData);

  private:
    CPLJSonStreamingWriter(const CPLJSonStreamingWriter &) = delete;
    CPLJSonStreamingWriter &operator=(const CPLJSonStreamingWriter &) = delete;

    std::string m_osStr{};
    SerializationFuncType m_pfnSerializationFunc = nullptr;
    void *m_pUserData = nullptr;
    bool m_bPretty = true;
    std::string m_osIndent = std::string("  ");
    std::string m_osIndentAcc{};
    int m_nLevel = 0;
    bool m_bNewLineEnabled = true;
    std::string m_osTmpForSerialize{};
    std::string m_osTmpForFormatString{};

    struct State
    {
        bool bIsObj = false;
        bool bFirstChild = true;

        explicit State(bool bIsObjIn) : bIsObj(bIsObjIn)
        {
        }
    };

    std::vector<State> m_states{};
    bool m_bWaitForValue = false;

    void IncIndent();
    void DecIndent();
    const std::string &FormatString(const std::string_view &str);
    void EmitCommaIfNeeded();

    void Serialize(const char *pszStr, size_t nLength);

  protected:
    virtual void Serialize(const std::string_view &str);

  public:
    CPLJSonStreamingWriter(SerializationFuncType pfnSerializationFunc,
                           void *pUserData);
    virtual ~CPLJSonStreamingWriter();

    void clear();

    void SetPrettyFormatting(bool bPretty)
    {
        m_bPretty = bPretty;
    }

    void SetIndentationSize(int nSpaces);

    // cppcheck-suppress functionStatic
    const std::string &GetString() const
    {
        return m_osStr;
    }

    void Add(const char *pszStr);
    void Add(const std::string &str);
    void Add(const std::string_view &str);
    void Add(bool bVal);

    void AddSerializedValue(const std::string_view &str);

    void Add(int nVal)
    {
        Add(static_cast<std::int64_t>(nVal));
    }

    void Add(unsigned int nVal)
    {
        Add(static_cast<std::int64_t>(nVal));
    }

    void Add(std::int64_t nVal);
    void Add(std::uint64_t nVal);
    void Add(GFloat16 hfVal, int nPrecision = 5);
    void Add(float fVal, int nPrecision = 9);
    void Add(double dfVal, int nPrecision = 17);
    void AddNull();

    void StartObj();
    void EndObj();
    void AddObjKey(const std::string_view &key);

    struct CPL_DLL ObjectContext
    {
        CPLJSonStreamingWriter &m_serializer;

        ObjectContext(const ObjectContext &) = delete;
        ObjectContext(ObjectContext &&) = default;

        explicit inline ObjectContext(CPLJSonStreamingWriter &serializer)
            : m_serializer(serializer)
        {
            m_serializer.StartObj();
        }

        ~ObjectContext()
        {
            m_serializer.EndObj();
        }
    };

    inline ObjectContext MakeObjectContext()
    {
        return ObjectContext(*this);
    }

    void StartArray();
    void EndArray();

    struct CPL_DLL ArrayContext
    {
        CPLJSonStreamingWriter &m_serializer;
        bool m_bForceSingleLine;
        bool m_bNewLineEnabledBackup;

        ArrayContext(const ArrayContext &) = delete;
        ArrayContext(ArrayContext &&) = default;

        inline explicit ArrayContext(CPLJSonStreamingWriter &serializer,
                                     bool bForceSingleLine = false)
            : m_serializer(serializer), m_bForceSingleLine(bForceSingleLine),
              m_bNewLineEnabledBackup(serializer.GetNewLine())
        {
            if (m_bForceSingleLine)
                serializer.SetNewline(false);
            m_serializer.StartArray();
        }

        ~ArrayContext()
        {
            m_serializer.EndArray();
            if (m_bForceSingleLine)
                m_serializer.SetNewline(m_bNewLineEnabledBackup);
        }
    };

    inline ArrayContext MakeArrayContext(bool bForceSingleLine = false)
    {
        return ArrayContext(*this, bForceSingleLine);
    }

    bool GetNewLine() const
    {
        return m_bNewLineEnabled;
    }

    void SetNewline(bool bEnabled)
    {
        m_bNewLineEnabled = bEnabled;
    }
};

#endif  // __cplusplus

/*! @endcond */

#endif  // CPL_JSON_STREAMING_WRITER_H
