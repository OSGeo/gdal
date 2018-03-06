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

#include "cpl_json.h"


#include "cpl_error.h"
#include "cpl_json_header.h"
#include "cpl_vsi.h"

#include "cpl_http.h"
#include "cpl_multiproc.h"

#define TO_JSONOBJ(x) static_cast<json_object*>(x)

static const char *JSON_PATH_DELIMITER = "/";

//------------------------------------------------------------------------------
// JSONDocument
//------------------------------------------------------------------------------
/*! @cond Doxygen_Suppress */
CPLJSONDocument::CPLJSONDocument() : m_poRootJsonObject(nullptr)
{

}

CPLJSONDocument::~CPLJSONDocument()
{
    if( m_poRootJsonObject )
        json_object_put( TO_JSONOBJ(m_poRootJsonObject) );
}

CPLJSONDocument::CPLJSONDocument(const CPLJSONDocument& other)
{
    m_poRootJsonObject = json_object_get( TO_JSONOBJ(other.m_poRootJsonObject) );
}

CPLJSONDocument& CPLJSONDocument::operator=(const CPLJSONDocument& other)
{
    if( this == &other )
        return *this;

    if( m_poRootJsonObject )
        json_object_put( TO_JSONOBJ(m_poRootJsonObject) );
    m_poRootJsonObject = json_object_get( TO_JSONOBJ(other.m_poRootJsonObject) );

    return *this;
}
/*! @endcond */

/**
 * Save json document at specified path
 * @param  osPath Path to save json document
 * @return         true on success. If error occurred it can be received using CPLGetLastErrorMsg method.
 *
 * @since GDAL 2.3
 */
bool CPLJSONDocument::Save(const std::string &osPath)
{
    VSILFILE *fp = VSIFOpenL( osPath.c_str(), "wt" );
    if( nullptr == fp )
    {
        CPLError( CE_Failure, CPLE_NoWriteAccess, "Open file %s to write failed",
                 osPath.c_str() );
        return false;
    }

    const char *pabyData = json_object_to_json_string_ext(
                TO_JSONOBJ(m_poRootJsonObject), JSON_C_TO_STRING_PRETTY );
    VSIFWriteL(pabyData, 1, strlen(pabyData), fp);

    VSIFCloseL(fp);

    return true;
}

/**
 * Return the json document as a serialized string.
 * @return         serialized document.
 *
 * @since GDAL 2.3
 */
std::string CPLJSONDocument::SaveAsString()
{
    return json_object_to_json_string_ext(
                TO_JSONOBJ(m_poRootJsonObject), JSON_C_TO_STRING_PRETTY );
}

/**
 * Get json document root object
 * @return CPLJSONObject class instance
 *
 * @since GDAL 2.3
 */
CPLJSONObject CPLJSONDocument::GetRoot()
{
    if( nullptr == m_poRootJsonObject )
    {
        m_poRootJsonObject = json_object_new_object();
    }

    if( json_object_get_type( TO_JSONOBJ(m_poRootJsonObject) ) == json_type_array )
    {
        return CPLJSONArray( "", m_poRootJsonObject );
    }
    else
    {
        return CPLJSONObject( "", m_poRootJsonObject );
    }
}

/**
 * Load json document from file by provided path
 * @param  osPath Path to json file.
 * @return         true on success. If error occurred it can be received using CPLGetLastErrorMsg method.
 *
 * @since GDAL 2.3
 */
bool CPLJSONDocument::Load(const std::string &osPath)
{
    GByte *pabyOut = nullptr;
    vsi_l_offset nSize = 0;
    if( !VSIIngestFile( nullptr, osPath.c_str(), &pabyOut, &nSize, 8 * 1024 * 1024) ) // Maximum 8 Mb allowed
    {
        CPLError( CE_Failure, CPLE_FileIO, "Load json file %s failed", osPath.c_str() );
        return false;
    }

    bool bResult = LoadMemory(pabyOut, static_cast<int>(nSize));
    VSIFree(pabyOut);
    return bResult;
}

/**
 * Load json document from memory buffer.
 * @param  pabyData Buffer.data.
 * @param  nLength  Buffer size.
 * @return          true on success. If error occurred it can be received using CPLGetLastErrorMsg method.
 *
 * @since GDAL 2.3
 */
bool CPLJSONDocument::LoadMemory(const GByte *pabyData, int nLength)
{
    if(nullptr == pabyData)
    {
        return false;
    }

    if( m_poRootJsonObject )
        json_object_put( TO_JSONOBJ(m_poRootJsonObject) );

    json_tokener *jstok = json_tokener_new();
    m_poRootJsonObject = json_tokener_parse_ex( jstok,
                                                reinterpret_cast<const char*>(pabyData),
                                                nLength );
    bool bParsed = jstok->err == json_tokener_success;
    if(!bParsed)
    {
        CPLError( CE_Failure, CPLE_AppDefined, "JSON parsing error: %s (at offset %d)",
                 json_tokener_error_desc( jstok->err ), jstok->char_offset );
        json_tokener_free( jstok );
        return false;
    }
    json_tokener_free( jstok );
    return bParsed;
}

/**
 * Load json document from memory buffer.
 * @param  osStr    String
 * @return          true on success. If error occurred it can be received using CPLGetLastErrorMsg method.
 *
 * @since GDAL 2.3
 */
bool CPLJSONDocument::LoadMemory(const std::string &osStr)
{
    if( osStr.empty() )
        return false;
    return LoadMemory( reinterpret_cast<const GByte*>(osStr.data()),
                       static_cast<int>(osStr.size()) );
}

/**
 * Load json document from file using small chunks of data.
 * @param  osPath      Path to json document file.
 * @param  nChunkSize   Chunk size.
 * @param  pfnProgress  a function to report progress of the json data loading.
 * @param  pProgressArg application data passed into progress function.
 * @return              true on success. If error occurred it can be received using CPLGetLastErrorMsg method.
 *
 * @since GDAL 2.3
 */
bool CPLJSONDocument::LoadChunks(const std::string &osPath, size_t nChunkSize,
                                 GDALProgressFunc pfnProgress,
                                 void *pProgressArg)
{
    VSIStatBufL sStatBuf;
    if(VSIStatL( osPath.c_str(), &sStatBuf ) != 0)
    {
        CPLError(CE_Failure, CPLE_FileIO, "Cannot open %s", osPath.c_str());
        return false;
    }

    VSILFILE *fp = VSIFOpenL( osPath.c_str(), "rb" );
    if( fp == nullptr )
    {
        CPLError(CE_Failure, CPLE_FileIO, "Cannot open %s", osPath.c_str());
        return false;
    }

    void *pBuffer = CPLMalloc( nChunkSize );
    json_tokener *tok = json_tokener_new();
    bool bSuccess = true;
    GUInt32 nFileSize = static_cast<GUInt32>(sStatBuf.st_size);
    double dfTotalRead = 0.0;

    while( true )
    {
        size_t nRead = VSIFReadL( pBuffer, 1, nChunkSize, fp );
        dfTotalRead += nRead;

        if( m_poRootJsonObject )
            json_object_put( TO_JSONOBJ(m_poRootJsonObject) );

        m_poRootJsonObject = json_tokener_parse_ex(tok,
                                                   static_cast<const char*>(pBuffer),
                                                   static_cast<int>(nRead));

        enum json_tokener_error jerr = json_tokener_get_error(tok);
        if(jerr != json_tokener_continue && jerr != json_tokener_success)
        {
            CPLError( CE_Failure, CPLE_AppDefined, "JSON error: %s",
                     json_tokener_error_desc(jerr) );
            bSuccess = false;
            break;
        }

        if( nRead < nChunkSize )
        {
            break;
        }

        if( nullptr != pfnProgress )
        {
            pfnProgress(dfTotalRead / nFileSize, "Loading ...", pProgressArg);
        }
    }

    json_tokener_free(tok);
    CPLFree(pBuffer);
    VSIFCloseL(fp);

    if( nullptr != pfnProgress )
    {
        pfnProgress(1.0, "Loading ...", pProgressArg);
    }

    return bSuccess;
}

/*! @cond Doxygen_Suppress */
#ifdef HAVE_CURL

typedef struct {
    json_object *pObject;
    json_tokener *pTokener;
    int nDataLen;
} JsonContext, *JsonContextL;

static size_t CPLJSONWriteFunction(void *pBuffer, size_t nSize, size_t nMemb,
                                           void *pUserData)
{
    size_t nLength = nSize * nMemb;
    JsonContextL ctx = static_cast<JsonContextL>(pUserData);
    ctx->pObject = json_tokener_parse_ex(ctx->pTokener,
                                         static_cast<const char*>(pBuffer),
                                         static_cast<int>(nLength));
    ctx->nDataLen = static_cast<int>(nLength);
    switch (json_tokener_get_error(ctx->pTokener)) {
    case json_tokener_continue:
    case json_tokener_success:
        return nLength;
    default:
        return 0; /* error: interrupt the transfer */
    }
}

#endif // HAVE_CURL
/*! @endcond */

/**
 * Load json document from web.
 * @param  osUrl       Url to json document.
 * @param  papszOptions Option list as a NULL-terminated array of strings. May be NULL.
 * The available keys are same for CPLHTTPFetch method. Additional key JSON_DEPTH
 * define json parse depth. Default is 10.
 * @param  pfnProgress  a function to report progress of the json data loading.
 * @param  pProgressArg application data passed into progress function.
 * @return              true on success. If error occurred it can be received using CPLGetLastErrorMsg method.
 *
 * @since GDAL 2.3
 */

#ifdef HAVE_CURL
bool CPLJSONDocument::LoadUrl(const std::string &osUrl, char **papszOptions,
                              GDALProgressFunc pfnProgress,
                              void *pProgressArg)
#else
bool CPLJSONDocument::LoadUrl(const std::string & /*osUrl*/, char ** /*papszOptions*/,
                              GDALProgressFunc /*pfnProgress*/,
                              void * /*pProgressArg*/)
#endif // HAVE_CURL
{
#ifdef HAVE_CURL
    int nDepth = atoi( CSLFetchNameValueDef( papszOptions, "JSON_DEPTH", "10") );
    JsonContext ctx = { nullptr, json_tokener_new_ex(nDepth), 0 };

    CPLHTTPFetchWriteFunc pWriteFunc = CPLJSONWriteFunction;
    CPLHTTPResult *psResult = CPLHTTPFetchEx( osUrl.c_str(), papszOptions,
                                              pfnProgress, pProgressArg,
                                              pWriteFunc, &ctx );

    bool bResult = true;
    if( psResult->nStatus != 0 /*CURLE_OK*/ )
    {
        bResult = false;
    }

    CPLHTTPDestroyResult( psResult );

    enum json_tokener_error jerr;
    if ((jerr = json_tokener_get_error(ctx.pTokener)) != json_tokener_success) {
        CPLError(CE_Failure, CPLE_AppDefined, "JSON error: %s\n",
               json_tokener_error_desc(jerr));
        bResult = false;
    }
    else {
        if( m_poRootJsonObject )
            json_object_put( TO_JSONOBJ(m_poRootJsonObject) );

        m_poRootJsonObject = ctx.pObject;
    }
    json_tokener_free(ctx.pTokener);

    return bResult;
#else
    return false;
#endif
}

//------------------------------------------------------------------------------
// JSONObject
//------------------------------------------------------------------------------
/*! @cond Doxygen_Suppress */
CPLJSONObject::CPLJSONObject() : m_poJsonObject(json_object_new_object())
{

}

CPLJSONObject::CPLJSONObject(const std::string &osName, const CPLJSONObject &oParent) :
    m_poJsonObject(json_object_get(json_object_new_object())),
    m_osKey(osName)
{
    json_object_object_add( TO_JSONOBJ(oParent.m_poJsonObject), osName.c_str(),
                            TO_JSONOBJ(m_poJsonObject) );
}

CPLJSONObject::CPLJSONObject(const std::string &osName, JSONObjectH poJsonObject) :
    m_poJsonObject( json_object_get( TO_JSONOBJ(poJsonObject) ) ),
    m_osKey(osName)
{

}

CPLJSONObject::~CPLJSONObject()
{
    // Should delete m_poJsonObject only if CPLJSONObject has no parent
    if( m_poJsonObject )
    {
        json_object_put( TO_JSONOBJ(m_poJsonObject) );
        m_poJsonObject = nullptr;
    }
}

CPLJSONObject::CPLJSONObject(const CPLJSONObject &other) :
    m_poJsonObject(json_object_get( TO_JSONOBJ(other.m_poJsonObject) )),
    m_osKey(other.m_osKey)
{
}

CPLJSONObject &CPLJSONObject::operator=(const CPLJSONObject &other)
{
    if( this == &other )
        return *this;

    m_osKey = other.m_osKey;
    if( m_poJsonObject )
        json_object_put( TO_JSONOBJ(m_poJsonObject) );
    m_poJsonObject = json_object_get( TO_JSONOBJ(other.m_poJsonObject) );
    return *this;
}
/*! @endcond */

/**
 * Add new key - value pair to json object.
 * @param osName Key name.
 * @param osValue String value.
 *
 * @since GDAL 2.3
 */
void CPLJSONObject::Add(const std::string &osName, const std::string &osValue)
{
    std::string objectName;
    CPLJSONObject object = GetObjectByPath( osName, objectName );
    if( object.IsValid() &&
        json_object_get_type(TO_JSONOBJ(object.m_poJsonObject)) ==
            json_type_object )
    {
        json_object *poVal = json_object_new_string( osValue.c_str() );
        json_object_object_add( TO_JSONOBJ(object.GetInternalHandle()),
                                           objectName.c_str(), poVal );
    }
}

/**
 * Add new key - value pair to json object.
 * @param osName Key name.
 * @param pszValue String value.
 *
 * @since GDAL 2.3
 */
void CPLJSONObject::Add(const std::string &osName, const char *pszValue)
{
    if(nullptr == pszValue)
    {
        return;
    }
    std::string objectName;
    CPLJSONObject object = GetObjectByPath( osName, objectName );
    if( object.IsValid() &&
        json_object_get_type(TO_JSONOBJ(object.m_poJsonObject)) ==
            json_type_object )
    {
        json_object *poVal = json_object_new_string( pszValue );
        json_object_object_add( TO_JSONOBJ(object.GetInternalHandle()),
                                           objectName.c_str(), poVal );
    }
}

// defined in ogr/ogrsf_frmts/geojson/ogrgeojsonwriter.cpp
CPL_C_START
/* %.XXXg formatting */
json_object CPL_DLL* json_object_new_double_with_significant_figures(double dfVal,
                                                                     int nSignificantFigures);
CPL_C_END

/**
 * Add new key - value pair to json object.
 * @param osName  Key name.
 * @param dfValue Double value.
 *
 * @since GDAL 2.3
 */
void CPLJSONObject::Add(const std::string &osName, double dfValue)
{
    std::string objectName;
    CPLJSONObject object = GetObjectByPath( osName, objectName );
    if( object.IsValid() &&
        json_object_get_type(TO_JSONOBJ(object.m_poJsonObject)) ==
            json_type_object )
    {
        json_object *poVal = json_object_new_double_with_significant_figures( dfValue, -1 );
        json_object_object_add( TO_JSONOBJ(object.GetInternalHandle()),
                                           objectName.c_str(), poVal );
    }
}

/**
 * Add new key - value pair to json object.
 * @param osName  Key name.
 * @param nValue Integer value.
 *
 * @since GDAL 2.3
 */
void CPLJSONObject::Add(const std::string &osName, int nValue)
{
    std::string objectName;
    CPLJSONObject object = GetObjectByPath( osName, objectName );
    if( object.IsValid() &&
        json_object_get_type(TO_JSONOBJ(object.m_poJsonObject)) ==
            json_type_object )
    {
        json_object *poVal = json_object_new_int( nValue );
        json_object_object_add( TO_JSONOBJ(object.GetInternalHandle()),
                                           objectName.c_str(), poVal );
    }
}

/**
 * Add new key - value pair to json object.
 * @param osName  Key name.
 * @param nValue Long value.
 *
 * @since GDAL 2.3
 */
void CPLJSONObject::Add(const std::string &osName, GInt64 nValue)
{
    std::string objectName;
    CPLJSONObject object = GetObjectByPath( osName, objectName );
    if( object.IsValid() &&
        json_object_get_type(TO_JSONOBJ(object.m_poJsonObject)) ==
            json_type_object )
    {
        json_object *poVal = json_object_new_int64( static_cast<int64_t>(nValue) );
        json_object_object_add( TO_JSONOBJ(object.GetInternalHandle()),
                                           objectName.c_str(), poVal );
    }
}

/**
 * Add new key - value pair to json object.
 * @param osName  Key name.
 * @param oValue   Array value.
 *
 * @since GDAL 2.3
 */
void CPLJSONObject::Add(const std::string &osName, const CPLJSONArray &oValue)
{
    std::string objectName;
    CPLJSONObject object = GetObjectByPath( osName, objectName );
    if( object.IsValid() &&
        json_object_get_type(TO_JSONOBJ(object.m_poJsonObject)) ==
            json_type_object )
    {
        json_object_object_add( TO_JSONOBJ(object.GetInternalHandle()),
                                objectName.c_str(),
                                json_object_get( TO_JSONOBJ(oValue.GetInternalHandle()) ) );
    }
}

/**
 * Add new key - value pair to json object.
 * @param osName  Key name.
 * @param oValue   Json object value.
 *
 * @since GDAL 2.3
 */
void CPLJSONObject::Add(const std::string &osName, const CPLJSONObject &oValue)
{
    std::string objectName;
    CPLJSONObject object = GetObjectByPath( osName, objectName );
    if( object.IsValid() &&
        json_object_get_type(TO_JSONOBJ(object.m_poJsonObject)) ==
            json_type_object )
    {
        json_object_object_add( TO_JSONOBJ(object.GetInternalHandle()),
                                objectName.c_str(),
                                json_object_get( TO_JSONOBJ(oValue.GetInternalHandle()) ) );
    }
}

/**
 * Add new key - value pair to json object.
 * @param osName  Key name.
 * @param bValue   Boolean value.
 *
 * @since GDAL 2.3
 */
void CPLJSONObject::Add(const std::string &osName, bool bValue)
{
    std::string objectName;
    CPLJSONObject object = GetObjectByPath( osName, objectName );
    if( object.IsValid() &&
        json_object_get_type(TO_JSONOBJ(object.m_poJsonObject)) ==
            json_type_object )
    {
        json_object *poVal = json_object_new_boolean( bValue );
        json_object_object_add( TO_JSONOBJ(object.GetInternalHandle()),
                                           objectName.c_str(), poVal );
    }
}

/**
 * Add new key - null pair to json object.
 * @param osName  Key name.
 *
 * @since GDAL 2.3
 */
void CPLJSONObject::AddNull(const std::string &osName)
{
    std::string objectName;
    CPLJSONObject object = GetObjectByPath( osName, objectName );
    if( object.IsValid() &&
        json_object_get_type(TO_JSONOBJ(object.m_poJsonObject)) ==
            json_type_object )
    {
        json_object_object_add( TO_JSONOBJ(object.GetInternalHandle()),
                                           objectName.c_str(), nullptr );
    }
}

/**
 * Change value by key.
 * @param osName  Key name.
 * @param osValue String value.
 *
 * @since GDAL 2.3
 */
void CPLJSONObject::Set(const std::string &osName, const std::string &osValue)
{
    Delete( osName );
    Add( osName, osValue );
}

/**
 * Change value by key.
 * @param osName  Key name.
 * @param pszValue String value.
 *
 * @since GDAL 2.3
 */
void CPLJSONObject::Set(const std::string &osName, const char * pszValue)
{
    if(nullptr == pszValue)
        return;
    Delete( osName );
    Add( osName, pszValue );
}

/**
 * Change value by key.
 * @param osName  Key name.
 * @param dfValue  Double value.
 *
 * @since GDAL 2.3
 */
void CPLJSONObject::Set(const std::string &osName, double dfValue)
{
    Delete( osName );
    Add( osName, dfValue );
}

/**
 * Change value by key.
 * @param osName  Key name.
 * @param nValue   Integer value.
 *
 * @since GDAL 2.3
 */
void CPLJSONObject::Set(const std::string &osName, int nValue)
{
    Delete( osName );
    Add( osName, nValue );
}

/**
 * Change value by key.
 * @param osName  Key name.
 * @param nValue   Long value.
 *
 * @since GDAL 2.3
 */
void CPLJSONObject::Set(const std::string &osName, GInt64 nValue)
{
    Delete( osName );
    Add( osName, nValue );
}

/**
 * Change value by key.
 * @param osName  Key name.
 * @param bValue   Boolean value.
 *
 * @since GDAL 2.3
 */
void CPLJSONObject::Set(const std::string &osName, bool bValue)
{
    Delete( osName );
    Add( osName, bValue );
}

/**
 * Change value by key.
 * @param osName  Key name.
 *
 * @since GDAL 2.3
 */
void CPLJSONObject::SetNull(const std::string &osName)
{
    Delete( osName );
    AddNull( osName );
}

/**
 * Get value by key.
 * @param  osName Key name.
 * @return         Json array object.
 *
 * @since GDAL 2.3
 */
CPLJSONArray CPLJSONObject::GetArray(const std::string &osName) const
{
    std::string objectName;
    CPLJSONObject object = GetObjectByPath( osName, objectName );
    if( object.IsValid() )
    {
        json_object *poVal = nullptr;
        if( json_object_object_get_ex( TO_JSONOBJ(object.GetInternalHandle()),
                                       objectName.c_str(), &poVal ) )
        {
            if( poVal && json_object_get_type( poVal ) == json_type_array )
            {
                return CPLJSONArray( objectName, poVal );
            }
        }
    }
    return CPLJSONArray( "", nullptr );
}

/**
 * Get value by key.
 * @param  osName Key name.
 * @return         Json object.
 *
 * @since GDAL 2.3
 */
CPLJSONObject CPLJSONObject::GetObj(const std::string &osName) const
{
    std::string objectName;
    CPLJSONObject object = GetObjectByPath( osName, objectName );
    if( object.IsValid() )
    {
        json_object* poVal = nullptr;
        if(json_object_object_get_ex( TO_JSONOBJ(object.GetInternalHandle()),
                                      objectName.c_str(), &poVal ) )
        {
            return CPLJSONObject( objectName, poVal );
        }
    }
    return CPLJSONObject( "", nullptr );
}

/**
 * Get value by key.
 * @param  osName Key name.
 * @return         Json object.
 *
 * @since GDAL 2.3
 */
CPLJSONObject CPLJSONObject::operator[](const std::string &osName) const
{
    return GetObj(osName);
}

/**
 * Delete json object by key.
 * @param  osName Key name.
 *
 * @since GDAL 2.3
 */
void CPLJSONObject::Delete(const std::string &osName)
{
    std::string objectName;
    CPLJSONObject object = GetObjectByPath( osName, objectName );
    if( object.IsValid() )
    {
        json_object_object_del( TO_JSONOBJ(object.GetInternalHandle()),
                                objectName.c_str() );
    }
}

/**
 * Get value by key.
 * @param  osName    Key name.
 * @param  osDefault Default value.
 * @return            String value.
 *
 * @since GDAL 2.3
 */
std::string CPLJSONObject::GetString(const std::string &osName,
                                     const std::string &osDefault) const
{
    CPLJSONObject object = GetObj( osName );
    return object.ToString( osDefault );
}

/**
 * Get value.
 * @param  osDefault Default value.
 * @return            String value.
 *
 * @since GDAL 2.3
 */
std::string CPLJSONObject::ToString(const std::string &osDefault) const
{
    if( m_poJsonObject /*&& json_object_get_type( TO_JSONOBJ(m_poJsonObject) ) ==
            json_type_string*/ )
    {
        const char *pszString = json_object_get_string( TO_JSONOBJ(m_poJsonObject) );
        if(nullptr != pszString)
        {
            return pszString;
        }
    }
    return osDefault;
}

/**
 * Get value by key.
 * @param  osName    Key name.
 * @param  dfDefault  Default value.
 * @return            Double value.
 *
 * @since GDAL 2.3
 */
double CPLJSONObject::GetDouble(const std::string &osName, double dfDefault) const
{
    CPLJSONObject object = GetObj( osName );
    return object.ToDouble( dfDefault );
}

/**
 * Get value
 * @param  dfDefault  Default value.
 * @return            Double value.
 *
 * @since GDAL 2.3
 */
double CPLJSONObject::ToDouble(double dfDefault) const
{
    if( m_poJsonObject /*&& json_object_get_type( TO_JSONOBJ(m_poJsonObject) ) ==
            json_type_double*/ )
        return json_object_get_double( TO_JSONOBJ(m_poJsonObject) );
    return dfDefault;
}

/**
 * Get value by key.
 * @param  osName    Key name.
 * @param  nDefault   Default value.
 * @return            Integer value.
 *
 * @since GDAL 2.3
 */
int CPLJSONObject::GetInteger(const std::string &osName, int nDefault) const
{
    CPLJSONObject object = GetObj( osName );
    return object.ToInteger( nDefault );
}

/**
 * Get value.
 * @param  nDefault   Default value.
 * @return            Integer value.
 *
 * @since GDAL 2.3
 */
int CPLJSONObject::ToInteger(int nDefault) const
{
    if( m_poJsonObject /*&& json_object_get_type( TO_JSONOBJ(m_poJsonObject) ) ==
            json_type_int*/ )
        return json_object_get_int( TO_JSONOBJ(m_poJsonObject) );
    return nDefault;
}

/**
 * Get value by key.
 * @param  osName    Key name.
 * @param  nDefault   Default value.
 * @return            Long value.
 *
 * @since GDAL 2.3
 */
GInt64 CPLJSONObject::GetLong(const std::string &osName, GInt64 nDefault) const
{
    CPLJSONObject object = GetObj( osName );
    return object.ToLong( nDefault );
}

/**
 * Get value.
 * @param  nDefault   Default value.
 * @return            Long value.
 *
 * @since GDAL 2.3
 */
GInt64 CPLJSONObject::ToLong(GInt64 nDefault) const
{
    if( m_poJsonObject /*&& json_object_get_type( TO_JSONOBJ(m_poJsonObject) ) ==
            json_type_int*/ )
        return static_cast<GInt64>( json_object_get_int64( TO_JSONOBJ(m_poJsonObject) ) );
    return nDefault;
}

/**
 * Get value by key.
 * @param  osName    Key name.
 * @param  bDefault   Default value.
 * @return            Boolean value.
 *
 * @since GDAL 2.3
 */
bool CPLJSONObject::GetBool(const std::string &osName, bool bDefault) const
{
    CPLJSONObject object = GetObj( osName );
    return object.ToBool( bDefault );
}

/**
 * \brief Get json object children.
 *
 * This function is useful when keys is not know and need to
 * iterate over json object items and get keys and values.
 *
 * @return Array of CPLJSONObject class instance.
 *
 * @since GDAL 2.3
 */
std::vector<CPLJSONObject> CPLJSONObject::GetChildren() const
{
    std::vector<CPLJSONObject> aoChildren;
    if(nullptr == m_poJsonObject || json_object_get_type(
                    TO_JSONOBJ(m_poJsonObject) ) != json_type_object )
    {
        return aoChildren;
    }

    json_object_iter it;
    it.key = nullptr;
    it.val = nullptr;
    it.entry = nullptr;
    json_object_object_foreachC( TO_JSONOBJ(m_poJsonObject), it ) {
        aoChildren.push_back(CPLJSONObject(it.key, it.val));
    }

    return aoChildren;
}

/**
 * Get value.
 * @param  bDefault   Default value.
 * @return            Boolean value.
 *
 * @since GDAL 2.3
 */
bool CPLJSONObject::ToBool(bool bDefault) const
{
    if( m_poJsonObject /*&& json_object_get_type( TO_JSONOBJ(m_poJsonObject) ) ==
            json_type_boolean*/ )
        return json_object_get_boolean( TO_JSONOBJ(m_poJsonObject) ) == 1;
    return bDefault;
}

/**
 * Get value.
 * @return            Array
 *
 * @since GDAL 2.3
 */
CPLJSONArray CPLJSONObject::ToArray() const
{
    if( m_poJsonObject && json_object_get_type( TO_JSONOBJ(m_poJsonObject) ) ==
            json_type_array )
        return CPLJSONArray("", TO_JSONOBJ(m_poJsonObject) );
    return CPLJSONArray("", nullptr);
}

/**
 * Stringify object to json format.
 * @param  eFormat Format type,
 * @return         A string in JSON format.
 *
 * @since GDAL 2.3
 */
std::string CPLJSONObject::Format(enum PrettyFormat eFormat) const
{
    if( m_poJsonObject )
    {
        const char *pszFormatString = nullptr;
        switch ( eFormat ) {
            case Spaced:
                pszFormatString = json_object_to_json_string_ext(
                    TO_JSONOBJ(m_poJsonObject), JSON_C_TO_STRING_SPACED );
                break;
            case Pretty:
                pszFormatString = json_object_to_json_string_ext(
                    TO_JSONOBJ(m_poJsonObject), JSON_C_TO_STRING_PRETTY );
                break;
            default:
                pszFormatString = json_object_to_json_string_ext(
                    TO_JSONOBJ(m_poJsonObject), JSON_C_TO_STRING_PLAIN);
        }
        if(nullptr != pszFormatString)
        {
            return pszFormatString;
        }
    }
    return "";
}

/*! @cond Doxygen_Suppress */
CPLJSONObject CPLJSONObject::GetObjectByPath(const std::string &osPath,
                                             std::string &osName) const
{
    json_object *poVal = nullptr;

    // Typically for keys that contain / character
    if( json_object_object_get_ex( TO_JSONOBJ(GetInternalHandle()),
                                   osPath.c_str(), &poVal ) )
    {
        osName = osPath;
        return *this;
    }

    CPLStringList pathPortions( CSLTokenizeString2( osPath.c_str(),
                                                    JSON_PATH_DELIMITER, 0 ) );
    int portionsCount = pathPortions.size();
    if( portionsCount > 100 )
    {
        CPLError(CE_Failure, CPLE_NotSupported, "Too many components in path");
        return CPLJSONObject( "", nullptr );
    }
    if( 0 == portionsCount )
        return CPLJSONObject( "", nullptr );
    CPLJSONObject object = *this;
    for( int i = 0; i < portionsCount - 1; ++i ) {
        // TODO: check array index in path - i.e. settings/catalog/root/id:1/name
        // if EQUALN(pathPortions[i+1], "id:", 3) -> getArray
        if( json_object_object_get_ex( TO_JSONOBJ(object.GetInternalHandle()),
                                       pathPortions[i], &poVal ) )
        {
            object = CPLJSONObject( pathPortions[i], poVal );
        }
        else
        {
            if( json_object_get_type(TO_JSONOBJ(object.m_poJsonObject)) !=
                                                            json_type_object )
            {
                return CPLJSONObject( "", nullptr );
            }
            object = CPLJSONObject( pathPortions[i], object );
        }
    }

//    // Check if such object already  exists
//    if(json_object_object_get_ex(object.m_jsonObject,
//                                 pathPortions[portionsCount - 1], &poVal))
//        return JSONObject(nullptr);
//
    osName = pathPortions[portionsCount - 1];
    return object;
}
/*! @endcond */

/**
 * Get json object type.
 * @return Json object type.
 *
 * @since GDAL 2.3
 */
CPLJSONObject::Type CPLJSONObject::GetType() const
{
    if(nullptr == m_poJsonObject)
        return CPLJSONObject::Unknown;
    switch ( json_object_get_type( TO_JSONOBJ(m_poJsonObject) ) )
    {
    case  json_type_null:
        return CPLJSONObject::Null;
    case json_type_boolean:
        return CPLJSONObject::Boolean;
    case json_type_double:
        return CPLJSONObject::Double;
    case json_type_int:
        return CPLJSONObject::Integer;
    case json_type_object:
        return CPLJSONObject::Object;
    case json_type_array:
        return CPLJSONObject::Array;
    case json_type_string:
        return CPLJSONObject::String;
    }
    return CPLJSONObject::Unknown;
}

/**
 * Check if json object valid.
 * @return true if json object valid.
 *
 * @since GDAL 2.3
 */
bool CPLJSONObject::IsValid() const
{
    return nullptr != m_poJsonObject;
}

/**
 * Decrement reference counter and make pointer NULL.
 * A json object will become invalid.
 *
 * @since GDAL 2.3
 */
void CPLJSONObject::Deinit()
{
    if( m_poJsonObject )
    {
        json_object_put( TO_JSONOBJ(m_poJsonObject) );
        m_poJsonObject = nullptr;
    }
}

//------------------------------------------------------------------------------
// JSONArray
//------------------------------------------------------------------------------
/*! @cond Doxygen_Suppress */
CPLJSONArray::CPLJSONArray()
{
    json_object_put( TO_JSONOBJ(m_poJsonObject) );
    m_poJsonObject = json_object_new_array();
}

CPLJSONArray::CPLJSONArray(const std::string &osName) :
    CPLJSONObject( osName, json_object_new_array() )
{
    json_object_put( TO_JSONOBJ(m_poJsonObject) );
}

CPLJSONArray::CPLJSONArray(const std::string &osName, JSONObjectH poJsonObject) :
    CPLJSONObject(osName, poJsonObject)
{

}

CPLJSONArray::CPLJSONArray(const CPLJSONObject &other) : CPLJSONObject(other)
{

}
/*! @endcond */

/**
 * Get array size.
 * @return Array size.
 *
 * @since GDAL 2.3
 */
int CPLJSONArray::Size() const
{
    if( m_poJsonObject )
        return json_object_array_length( TO_JSONOBJ(m_poJsonObject) );
    return 0;
}

/**
 * Add json object to array.
 * @param oValue Json array.
 *
 * @since GDAL 2.3
 */
void CPLJSONArray::Add(const CPLJSONObject &oValue)
{
    if( m_poJsonObject && oValue.m_poJsonObject )
        json_object_array_add( TO_JSONOBJ(m_poJsonObject),
                               json_object_get( TO_JSONOBJ(oValue.m_poJsonObject) ) );
}

/**
 * Add value to array
 * @param osValue Value to add.
 *
 * @since GDAL 2.3
 */
void CPLJSONArray::Add(const std::string &osValue)
{
    if( m_poJsonObject )
        json_object_array_add( TO_JSONOBJ(m_poJsonObject),
            json_object_new_string( osValue.c_str() ) );
}

/**
 * Add value to array
 * @param pszValue Value to add.
 *
 * @since GDAL 2.3
 */
void CPLJSONArray::Add(const char *pszValue)
{
    if(nullptr == pszValue)
        return;
    if( m_poJsonObject )
        json_object_array_add( TO_JSONOBJ(m_poJsonObject),
            json_object_new_string( pszValue ) );
}

/**
 * Add value to array
 * @param dfValue Value to add.
 *
 * @since GDAL 2.3
 */
void CPLJSONArray::Add(double dfValue)
{
    if( m_poJsonObject )
        json_object_array_add( TO_JSONOBJ(m_poJsonObject),
            json_object_new_double( dfValue ) );
}

/**
 * Add value to array
 * @param nValue Value to add.
 *
 * @since GDAL 2.3
 */
void CPLJSONArray::Add(int nValue)
{
    if( m_poJsonObject )
        json_object_array_add( TO_JSONOBJ(m_poJsonObject),
            json_object_new_int( nValue ) );
}

/**
 * Add value to array
 * @param nValue Value to add.
 *
 * @since GDAL 2.3
 */
void CPLJSONArray::Add(GInt64 nValue)
{
    if( m_poJsonObject )
        json_object_array_add( TO_JSONOBJ(m_poJsonObject),
            json_object_new_int64( nValue ) );
}

/**
 * Add value to array
 * @param bValue Value to add.
 *
 * @since GDAL 2.3
 */
void CPLJSONArray::Add(bool bValue)
{
    if( m_poJsonObject )
        json_object_array_add( TO_JSONOBJ(m_poJsonObject),
            json_object_new_boolean( bValue ) );
}

/**
 * Get array item by index.
 * @param  nIndex Item index.
 * @return        Json object.
 *
 * @since GDAL 2.3
 */
CPLJSONObject CPLJSONArray::operator[](int nIndex)
{
    return CPLJSONObject( CPLSPrintf("id:%d", nIndex),
                          json_object_array_get_idx( TO_JSONOBJ(m_poJsonObject),
                                                     nIndex ) );
}

/**
 * Get array const item by index.
 * @param  nIndex Item index.
 * @return        Json object.
 *
 * @since GDAL 2.3
 */
const CPLJSONObject CPLJSONArray::operator[](int nIndex) const
{
    return CPLJSONObject( CPLSPrintf("id:%d", nIndex),
                          json_object_array_get_idx( TO_JSONOBJ(m_poJsonObject),
                                                     nIndex ) );
}
