/******************************************************************************
 * Project:  Common Portability Library
 * Purpose:  Function wrapper for libjson-c access.
 * Author:   Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
 *
 ******************************************************************************
 * Copyright (c) 2016-2017 NextGIS, <info@nextgis.com>
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

#include "cpl_string.h"
#include "cpl_progress.h"

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
        Null,
        Object,
        Array,
        Boolean,
        String,
        Integer,
        Long,
        Double
    };

public:
/*! @cond Doxygen_Suppress */
    CPLJSONObject();
    explicit CPLJSONObject(const char *pszName, const CPLJSONObject &oParent);
    ~CPLJSONObject();
    CPLJSONObject(const CPLJSONObject &other);
    CPLJSONObject &operator=(const CPLJSONObject &other);

#if !_MSC_VER
private:
#endif // ! _MSC_VER
    explicit CPLJSONObject(const CPLString &soName, JSONObjectH poJsonObject);
/*! @endcond */

public:
    // setters
    void Add(const char *pszName, const CPLString &soValue);
    void Add(const char *pszName, const char *pszValue);
    void Add(const char *pszName, double dfValue);
    void Add(const char *pszName, int nValue);
    void Add(const char *pszName, int64_t nValue);
    void Add(const char *pszName, const CPLJSONArray &oValue);
    void Add(const char *pszName, const CPLJSONObject &oValue);
    void Add(const char *pszName, bool bValue);

    void Set(const char *pszName, const char *pszValue);
    void Set(const char *pszName, double dfValue);
    void Set(const char *pszName, int nValue);
    void Set(const char *pszName, int64_t nValue);
    void Set(const char *pszName, bool bValue);

/*! @cond Doxygen_Suppress */
    JSONObjectH GetInternalHandle() const { return m_poJsonObject; }
/*! @endcond */

    // getters
    const char *GetString(const char *pszName, const char *pszDefault) const;
    double GetDouble(const char *pszName, double dfDefault) const;
    int GetInteger(const char *pszName, int nDefault) const;
    int64_t GetLong(const char *pszName, int64_t nDefault) const;
    bool GetBool(const char *pszName, bool bDefault) const;
    const char *GetString(const char *pszDefault) const;
    double GetDouble(double dfDefault) const;
    int GetInteger(int nDefault) const;
    int64_t GetLong(int64_t nDefault) const;
    bool GetBool(bool bDefault) const;

    //
    void Delete(const char* pszName);
    CPLJSONArray GetArray(const char *pszName) const;
    CPLJSONObject GetObject(const char *pszName) const;
    CPLJSONObject operator[](const char *pszName) const;
    enum Type GetType() const;
/*! @cond Doxygen_Suppress */
    const char *GetName() const { return m_soKey; }
/*! @endcond */

    CPLJSONObject **GetChildren() const;
    bool IsValid() const;

    static void DestroyJSONObjectList(CPLJSONObject **papsoList);

protected:
/*! @cond Doxygen_Suppress */
    CPLJSONObject GetObjectByPath(const char *pszPath, char *pszName) const;
/*! @endcond */

private:
    JSONObjectH m_poJsonObject;
    CPLString m_soKey;
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
    CPLJSONArray(const CPLString &soName);

#if !_MSC_VER
private:
#endif // ! _MSC_VER
    explicit CPLJSONArray(const CPLString &soName, JSONObjectH poJsonObject);
/*! @endcond */
public:
    int Size() const;
    void Add(const CPLJSONObject &oValue);
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

    bool Save(const char *pszPath);
    CPLJSONObject GetRoot();
    bool Load(const char *pszPath);
    bool Load(const GByte *pabyData, int nLength = -1);
    bool LoadChunks(const char *pszPath, size_t nChunkSize = 16384,
                    GDALProgressFunc pfnProgress = nullptr,
                    void *pProgressArg = nullptr);
    bool LoadUrl(const char *pszUrl, char **papszOptions,
                 GDALProgressFunc pfnProgress = nullptr,
                 void *pProgressArg = nullptr);

private:
    JSONObjectH m_poRootJsonObject;
};

CPL_C_END

#endif // CPL_JSON_H_INCLUDED
