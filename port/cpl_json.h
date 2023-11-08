/******************************************************************************
 * Project:  Common Portability Library
 * Purpose:  Function wrapper for libjson-c access.
 * Author:   Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
 *
 ******************************************************************************
 * Copyright (c) 2017-2018 NextGIS, <info@nextgis.com>
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

#ifndef CPL_JSON_H_INCLUDED
#define CPL_JSON_H_INCLUDED

#include "cpl_progress.h"
#include "cpl_string.h"

#include <cstdint>
#include <string>
#include <vector>

/**
 * \file cpl_json.h
 *
 * Interface for read and write JSON documents
 */

/*! @cond Doxygen_Suppress */
typedef void *JSONObjectH;

CPL_C_START

class CPLJSONArray;
/*! @endcond */

/**
 * @brief The CPLJSONArray class holds JSON object from CPLJSONDocument
 */
class CPL_DLL CPLJSONObject
{
    friend class CPLJSONArray;
    friend class CPLJSONDocument;

  public:
    /**
     * Json object types
     */
    enum class Type
    {
        Unknown,
        Null,
        Object,
        Array,
        Boolean,
        String,
        Integer,
        Long,
        Double
    };

    /**
     * Json object format to string options
     */
    enum class PrettyFormat
    {
        Plain,   ///< No extra whitespace or formatting applied
        Spaced,  ///< Minimal whitespace inserted
        Pretty   ///< Formatted output
    };

  public:
    /*! @cond Doxygen_Suppress */
    CPLJSONObject();
    explicit CPLJSONObject(const std::string &osName,
                           const CPLJSONObject &oParent);
    explicit CPLJSONObject(std::nullptr_t);
    explicit CPLJSONObject(const std::string &osVal);
    explicit CPLJSONObject(const char *pszValue);
    explicit CPLJSONObject(bool bVal);
    explicit CPLJSONObject(int nVal);
    explicit CPLJSONObject(int64_t nVal);
    explicit CPLJSONObject(uint64_t nVal);
    explicit CPLJSONObject(double dfVal);
    ~CPLJSONObject();
    CPLJSONObject(const CPLJSONObject &other);
    CPLJSONObject(CPLJSONObject &&other);
    CPLJSONObject &operator=(const CPLJSONObject &other);
    CPLJSONObject &operator=(CPLJSONObject &&other);

    // This method is not thread-safe
    CPLJSONObject Clone() const;

  private:
    explicit CPLJSONObject(const std::string &osName, JSONObjectH poJsonObject);
    /*! @endcond */

  public:
    // setters
    void Add(const std::string &osName, const std::string &osValue);
    void Add(const std::string &osName, const char *pszValue);
    void Add(const std::string &osName, double dfValue);
    void Add(const std::string &osName, int nValue);
    void Add(const std::string &osName, GInt64 nValue);
    void Add(const std::string &osName, uint64_t nValue);
    void Add(const std::string &osName, const CPLJSONArray &oValue);
    void Add(const std::string &osName, const CPLJSONObject &oValue);
    void AddNoSplitName(const std::string &osName, const CPLJSONObject &oValue);
    void Add(const std::string &osName, bool bValue);
    void AddNull(const std::string &osName);

    void Set(const std::string &osName, const std::string &osValue);
    void Set(const std::string &osName, const char *pszValue);
    void Set(const std::string &osName, double dfValue);
    void Set(const std::string &osName, int nValue);
    void Set(const std::string &osName, GInt64 nValue);
    void Set(const std::string &osName, uint64_t nValue);
    void Set(const std::string &osName, bool bValue);
    void SetNull(const std::string &osName);

    /*! @cond Doxygen_Suppress */
    JSONObjectH GetInternalHandle() const
    {
        return m_poJsonObject;
    }
    /*! @endcond */

    // getters
    std::string GetString(const std::string &osName,
                          const std::string &osDefault = "") const;
    double GetDouble(const std::string &osName, double dfDefault = 0.0) const;
    int GetInteger(const std::string &osName, int nDefault = 0) const;
    GInt64 GetLong(const std::string &osName, GInt64 nDefault = 0) const;
    bool GetBool(const std::string &osName, bool bDefault = false) const;
    std::string ToString(const std::string &osDefault = "") const;
    double ToDouble(double dfDefault = 0.0) const;
    int ToInteger(int nDefault = 0) const;
    GInt64 ToLong(GInt64 nDefault = 0) const;
    bool ToBool(bool bDefault = false) const;
    CPLJSONArray ToArray() const;
    std::string Format(PrettyFormat eFormat) const;

    //
    void Delete(const std::string &osName);
    void DeleteNoSplitName(const std::string &osName);
    CPLJSONArray GetArray(const std::string &osName) const;
    CPLJSONObject GetObj(const std::string &osName) const;
    CPLJSONObject operator[](const std::string &osName) const;
    Type GetType() const;
    /*! @cond Doxygen_Suppress */
    std::string GetName() const
    {
        return m_osKey;
    }
    /*! @endcond */

    std::vector<CPLJSONObject> GetChildren() const;
    bool IsValid() const;
    void Deinit();

  protected:
    /*! @cond Doxygen_Suppress */
    CPLJSONObject GetObjectByPath(const std::string &osPath,
                                  std::string &osName) const;
    /*! @endcond */

  private:
    JSONObjectH m_poJsonObject = nullptr;
    std::string m_osKey{};
};

/**
 * @brief The JSONArray class JSON array from JSONDocument
 */
class CPL_DLL CPLJSONArray : public CPLJSONObject
{
    friend class CPLJSONObject;
    friend class CPLJSONDocument;

  public:
    /*! @cond Doxygen_Suppress */
    CPLJSONArray();
    explicit CPLJSONArray(const std::string &osName);
    explicit CPLJSONArray(const CPLJSONObject &other);

  private:
    explicit CPLJSONArray(const std::string &osName, JSONObjectH poJsonObject);

    class CPL_DLL ConstIterator
    {
        const CPLJSONArray &m_oSelf;
        int m_nIdx;
        mutable CPLJSONObject m_oObj{};

      public:
        ConstIterator(const CPLJSONArray &oSelf, bool bStart)
            : m_oSelf(oSelf), m_nIdx(bStart ? 0 : oSelf.Size())
        {
        }
        ~ConstIterator() = default;
        CPLJSONObject &operator*() const
        {
            m_oObj = m_oSelf[m_nIdx];
            return m_oObj;
        }
        ConstIterator &operator++()
        {
            m_nIdx++;
            return *this;
        }
        bool operator==(const ConstIterator &it) const
        {
            return m_nIdx == it.m_nIdx;
        }
        bool operator!=(const ConstIterator &it) const
        {
            return m_nIdx != it.m_nIdx;
        }
    };

    /*! @endcond */
  public:
    int Size() const;
    void AddNull();
    void Add(const CPLJSONObject &oValue);
    void Add(const std::string &osValue);
    void Add(const char *pszValue);
    void Add(double dfValue);
    void Add(int nValue);
    void Add(GInt64 nValue);
    void Add(uint64_t nValue);
    void Add(bool bValue);
    CPLJSONObject operator[](int nIndex);
    const CPLJSONObject operator[](int nIndex) const;

    /** Iterator to first element */
    ConstIterator begin() const
    {
        return ConstIterator(*this, true);
    }
    /** Iterator to after last element */
    ConstIterator end() const
    {
        return ConstIterator(*this, false);
    }
};

/**
 * @brief The CPLJSONDocument class Wrapper class around json-c library
 */
class CPL_DLL CPLJSONDocument
{
  public:
    /*! @cond Doxygen_Suppress */
    CPLJSONDocument();
    ~CPLJSONDocument();
    CPLJSONDocument(const CPLJSONDocument &other);
    CPLJSONDocument &operator=(const CPLJSONDocument &other);
    CPLJSONDocument(CPLJSONDocument &&other);
    CPLJSONDocument &operator=(CPLJSONDocument &&other);
    /*! @endcond */

    bool Save(const std::string &osPath) const;
    std::string SaveAsString() const;

    CPLJSONObject GetRoot();
    const CPLJSONObject GetRoot() const;
    void SetRoot(const CPLJSONObject &oRoot);
    bool Load(const std::string &osPath);
    bool LoadMemory(const std::string &osStr);
    bool LoadMemory(const GByte *pabyData, int nLength = -1);
    bool LoadChunks(const std::string &osPath, size_t nChunkSize = 16384,
                    GDALProgressFunc pfnProgress = nullptr,
                    void *pProgressArg = nullptr);
    bool LoadUrl(const std::string &osUrl, const char *const *papszOptions,
                 GDALProgressFunc pfnProgress = nullptr,
                 void *pProgressArg = nullptr);

  private:
    mutable JSONObjectH m_poRootJsonObject;
};

CPL_C_END

#if defined(__cplusplus) && !defined(CPL_SUPRESS_CPLUSPLUS)
extern "C++"
{
    CPLStringList CPLParseKeyValueJson(const char *pszJson);
}
#endif

#endif  // CPL_JSON_H_INCLUDED
