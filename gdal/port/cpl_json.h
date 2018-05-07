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
    enum Type {
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
    enum PrettyFormat {
        Plain,  ///< No extra whitespace or formatting applied
        Spaced, ///< Minimal whitespace inserted
        Pretty  ///< Formatted output
    };

public:
/*! @cond Doxygen_Suppress */
    CPLJSONObject();
    explicit CPLJSONObject(const std::string &osName, const CPLJSONObject &oParent);
    ~CPLJSONObject();
    CPLJSONObject(const CPLJSONObject &other);
    CPLJSONObject &operator=(const CPLJSONObject &other);

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
    void Add(const std::string &osName, const CPLJSONArray &oValue);
    void Add(const std::string &osName, const CPLJSONObject &oValue);
    void Add(const std::string &osName, bool bValue);
    void AddNull(const std::string &osName);

    void Set(const std::string &osName, const std::string &osValue);
    void Set(const std::string &osName, const char *pszValue);
    void Set(const std::string &osName, double dfValue);
    void Set(const std::string &osName, int nValue);
    void Set(const std::string &osName, GInt64 nValue);
    void Set(const std::string &osName, bool bValue);
    void SetNull(const std::string &osName);

/*! @cond Doxygen_Suppress */
    JSONObjectH GetInternalHandle() const { return m_poJsonObject; }
/*! @endcond */

    // getters
    std::string GetString(const std::string &osName, const std::string &osDefault = "") const;
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
    std::string Format(enum PrettyFormat eFormat) const;

    //
    void Delete(const std::string &osName);
    CPLJSONArray GetArray(const std::string &osName) const;
    CPLJSONObject GetObj(const std::string &osName) const;
    CPLJSONObject operator[](const std::string &osName) const;
    enum Type GetType() const;
/*! @cond Doxygen_Suppress */
    std::string GetName() const { return m_osKey; }
/*! @endcond */

    std::vector<CPLJSONObject> GetChildren() const;
    bool IsValid() const;
    void Deinit();

protected:
/*! @cond Doxygen_Suppress */
    CPLJSONObject GetObjectByPath(const std::string &osPath, std::string &osName) const;
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
/*! @endcond */
public:
    int Size() const;
    void Add(const CPLJSONObject &oValue);
    void Add(const std::string &osValue);
    void Add(const char* pszValue);
    void Add(double dfValue);
    void Add(int nValue);
    void Add(GInt64 nValue);
    void Add(bool bValue);
    CPLJSONObject operator[](int nIndex);
    const CPLJSONObject operator[](int nIndex) const;
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
    CPLJSONDocument& operator=(const CPLJSONDocument &other);
/*! @endcond */

    bool Save(const std::string &osPath);
    std::string SaveAsString();

    CPLJSONObject GetRoot();
    bool Load(const std::string &osPath);
    bool LoadMemory(const std::string &osStr);
    bool LoadMemory(const GByte *pabyData, int nLength = -1);
    bool LoadChunks(const std::string &osPath, size_t nChunkSize = 16384,
                    GDALProgressFunc pfnProgress = nullptr,
                    void *pProgressArg = nullptr);
    bool LoadUrl(const std::string &osUrl, char **papszOptions,
                 GDALProgressFunc pfnProgress = nullptr,
                 void *pProgressArg = nullptr);

private:
    JSONObjectH m_poRootJsonObject;
};

CPL_C_END

#endif // CPL_JSON_H_INCLUDED
