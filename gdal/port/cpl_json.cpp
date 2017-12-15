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

#include "cpl_json.h"


#include "cpl_error.h"
#include "cpl_json_header.h"
#include "cpl_vsi.h"

#include "cpl_http.h"
#include "cpl_multiproc.h"

#define TO_JSONOBJ(x) static_cast<json_object*>(x)

static const char *JSON_PATH_DELIMITER = "/";
static const unsigned short JSON_NAME_MAX_SIZE = 255;

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
    if( other.m_poRootJsonObject )
        m_poRootJsonObject = json_object_get( TO_JSONOBJ(other.m_poRootJsonObject) );
}

CPLJSONDocument& CPLJSONDocument::operator=(const CPLJSONDocument& other)
{
    if( other.m_poRootJsonObject )
        m_poRootJsonObject = json_object_get( TO_JSONOBJ(other.m_poRootJsonObject) );
    return *this;
}
/*! @endcond */

/**
 * Save json document at specified path
 * @param  pszPath Path to save json document
 * @return         true on success. If error occured it can be received using CPLGetLastErrorMsg method.
 *
 * @since GDAL 2.3
 */
bool CPLJSONDocument::Save(const char *pszPath)
{
    VSILFILE *fp = VSIFOpenL( pszPath, "wt" );
    if( nullptr == fp )
    {
        CPLError( CE_Failure, CPLE_NoWriteAccess, "Open file %s to write failed",
                 pszPath );
        return false;
    }

    const char *pabyData = json_object_to_json_string_ext(
                TO_JSONOBJ(m_poRootJsonObject), JSON_C_TO_STRING_PRETTY );
    VSIFWriteL(pabyData, 1, strlen(pabyData), fp);

    VSIFCloseL(fp);

    return true;
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
 * @param  pszPath Path to json file.
 * @return         true on success. If error occured it can be received using CPLGetLastErrorMsg method.
 *
 * @since GDAL 2.3
 */
bool CPLJSONDocument::Load(const char *pszPath)
{
    GByte *pabyOut = nullptr;
    vsi_l_offset nSize = 0;
    if( !VSIIngestFile( nullptr, pszPath, &pabyOut, &nSize, 4 * 1024 * 1024) ) // Maximum 4 Mb allowed
    {
        CPLError( CE_Failure, CPLE_FileIO, "Load json file %s failed", pszPath );
        return false;
    }

    bool bResult = Load(pabyOut, static_cast<int>(nSize));
    VSIFree(pabyOut);
    return bResult;
}

/**
 * Load json document from memory buffer.
 * @param  pabyData Buffer.data.
 * @param  nLength  Buffer size.
 * @return          true on success. If error occured it can be received using CPLGetLastErrorMsg method.
 *
 * @since GDAL 2.3
 */
bool CPLJSONDocument::Load(const GByte *pabyData, int nLength)
{
    if(nullptr == pabyData)
    {
        return false;
    }
    json_tokener *jstok = json_tokener_new();
    m_poRootJsonObject = json_tokener_parse_ex( jstok,
                                                reinterpret_cast<const char*>(pabyData),
                                                nLength );
    bool bParsed = jstok->err == json_tokener_success;
    if(!bParsed)
    {
        CPLError( CE_Failure, CPLE_AppDefined, "JSON parsing error: %s (at offset %d)",
                 json_tokener_error_desc( jstok->err ), jstok->char_offset );

        return false;
    }
    json_tokener_free( jstok );
    return bParsed;
}

/**
 * Load json document from file using small chunks of data.
 * @param  pszPath      Path to json document file.
 * @param  nChunkSize   Chunk size.
 * @param  pfnProgress  a function to report progress of the json data loading.
 * @param  pProgressArg application data passed into progress function.
 * @return              true on success. If error occured it can be received using CPLGetLastErrorMsg method.
 *
 * @since GDAL 2.3
 */
bool CPLJSONDocument::LoadChunks(const char *pszPath, size_t nChunkSize,
                                 GDALProgressFunc pfnProgress,
                                 void *pProgressArg)
{
    VSIStatBufL sStatBuf;
    if(VSIStatL(pszPath, &sStatBuf) != 0)
    {
        CPLError(CE_Failure, CPLE_FileIO, "Cannot open %s", pszPath);
        return false;
    }

    VSILFILE *fp = VSIFOpenL(pszPath, "rb");
    if( fp == nullptr )
    {
        CPLError(CE_Failure, CPLE_FileIO, "Cannot open %s", pszPath);
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
 * @param  pszUrl       Url to json document.
 * @param  papszOptions Option list as a NULL-terminated array of strings. May be NULL.
 * The available keys are same for CPLHTTPFetch method. Addtional key JSON_DEPTH
 * define json parse depth. Default is 10.
 * @param  pfnProgress  a function to report progress of the json data loading.
 * @param  pProgressArg application data passed into progress function.
 * @return              true on success. If error occured it can be received using CPLGetLastErrorMsg method.
 *
 * @since GDAL 2.3
 */

#ifdef HAVE_CURL
bool CPLJSONDocument::LoadUrl(const char *pszUrl, char **papszOptions,
                              GDALProgressFunc pfnProgress,
                              void *pProgressArg)
#else
bool CPLJSONDocument::LoadUrl(const char * /*pszUrl*/, char ** /*papszOptions*/,
                              GDALProgressFunc /*pfnProgress*/,
                              void * /*pProgressArg*/)
#endif // HAVE_CURL
{
#ifdef HAVE_CURL
    int nDepth = atoi( CSLFetchNameValueDef( papszOptions, "JSON_DEPTH", "10") );
    JsonContext ctx = { nullptr, json_tokener_new_ex(nDepth), 0 };
    CPLHTTPResult *psResult = CPLHTTPFetchEx( pszUrl, papszOptions,
                                              pfnProgress, pProgressArg,
                                              CPLJSONWriteFunction, &ctx );

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
CPLJSONObject::CPLJSONObject()
{
    m_poJsonObject = json_object_new_object();
}

CPLJSONObject::CPLJSONObject(const char *pszName, const CPLJSONObject &oParent) :
    m_soKey(pszName)
{
    m_poJsonObject = json_object_get(json_object_new_object());
    json_object_object_add( TO_JSONOBJ(oParent.m_poJsonObject), pszName,
                            TO_JSONOBJ(m_poJsonObject) );
}

CPLJSONObject::CPLJSONObject(const CPLString &soName, JSONObjectH poJsonObject) :
    m_poJsonObject( json_object_get( TO_JSONOBJ(poJsonObject) ) ),
    m_soKey(soName)
{

}

CPLJSONObject::~CPLJSONObject()
{
    // Should delete m_poJsonObject only if CPLJSONObject has no parent
    json_object_put( TO_JSONOBJ(m_poJsonObject) );
}

CPLJSONObject::CPLJSONObject(const CPLJSONObject &other)
{
    m_soKey = other.m_soKey;
    m_poJsonObject = json_object_get( TO_JSONOBJ(other.m_poJsonObject) );
}

CPLJSONObject &CPLJSONObject::operator=(const CPLJSONObject &other)
{
    m_soKey = other.m_soKey;
    m_poJsonObject = json_object_get( TO_JSONOBJ(other.m_poJsonObject) );
    return *this;
}
/*! @endcond */

/**
 * Add new key - value pair to json object.
 * @param pszName Key name.
 * @param soValue String value.
 *
 * @since GDAL 2.3
 */
void CPLJSONObject::Add(const char *pszName, const CPLString &soValue)
{
    char objectName[JSON_NAME_MAX_SIZE];
    CPLJSONObject object = GetObjectByPath( pszName, &objectName[0] );
    if( object.IsValid() )
    {
        json_object *poVal = json_object_new_string( soValue.c_str() );
        json_object_object_add( TO_JSONOBJ(object.GetInternalHandle()), objectName,
                                poVal );
    }
}

/**
 * Add new key - value pair to json object.
 * @param pszName  Key name.
 * @param pszValue String value.
 *
 * @since GDAL 2.3
 */
void CPLJSONObject::Add(const char *pszName, const char *pszValue)
{
    if( nullptr == pszName )
        return;
    char objectName[JSON_NAME_MAX_SIZE];
    CPLJSONObject object = GetObjectByPath( pszName, &objectName[0] );
    if( object.IsValid() )
    {
        json_object *poVal = json_object_new_string( pszValue );
        json_object_object_add( TO_JSONOBJ(object.GetInternalHandle()), objectName,
                                poVal );
    }
}

/**
 * Add new key - value pair to json object.
 * @param pszName  Key name.
 * @param dfValue Double value.
 *
 * @since GDAL 2.3
 */
void CPLJSONObject::Add(const char *pszName, double dfValue)
{
    if( nullptr == pszName )
        return;
    char objectName[JSON_NAME_MAX_SIZE];
    CPLJSONObject object = GetObjectByPath( pszName, &objectName[0] );
    if(object.IsValid())
    {
        json_object *poVal = json_object_new_double( dfValue );
        json_object_object_add( TO_JSONOBJ(object.GetInternalHandle()), objectName,
                                poVal );
    }
}

/**
 * Add new key - value pair to json object.
 * @param pszName  Key name.
 * @param nValue Integer value.
 *
 * @since GDAL 2.3
 */
void CPLJSONObject::Add(const char *pszName, int nValue)
{
    if( nullptr == pszName )
        return;
    char objectName[JSON_NAME_MAX_SIZE];
    CPLJSONObject object = GetObjectByPath( pszName, &objectName[0] );
    if( object.IsValid() )
    {
        json_object *poVal = json_object_new_int( nValue );
        json_object_object_add( TO_JSONOBJ(object.GetInternalHandle()), objectName,
                                poVal );
    }
}

/**
 * Add new key - value pair to json object.
 * @param pszName  Key name.
 * @param nValue Long value.
 *
 * @since GDAL 2.3
 */
void CPLJSONObject::Add(const char *pszName, int64_t nValue)
{
    if( nullptr == pszName )
        return;
    char objectName[JSON_NAME_MAX_SIZE];
    CPLJSONObject object = GetObjectByPath( pszName, &objectName[0] );
    if( object.IsValid() )
    {
        json_object *poVal = json_object_new_int64( nValue );
        json_object_object_add( TO_JSONOBJ(object.GetInternalHandle()), objectName,
                                poVal );
    }
}

/**
 * Add new key - value pair to json object.
 * @param pszName  Key name.
 * @param oValue   Array value.
 *
 * @since GDAL 2.3
 */
void CPLJSONObject::Add(const char *pszName, const CPLJSONArray &oValue)
{
    if( nullptr == pszName )
        return;
    char objectName[JSON_NAME_MAX_SIZE];
    CPLJSONObject object = GetObjectByPath(pszName, &objectName[0]);
    if( object.IsValid() )
    {
        json_object_object_add( TO_JSONOBJ(object.GetInternalHandle()), objectName,
                                json_object_get( TO_JSONOBJ(oValue.GetInternalHandle()) ) );
    }
}

/**
 * Add new key - value pair to json object.
 * @param pszName  Key name.
 * @param oValue   Json object value.
 *
 * @since GDAL 2.3
 */
void CPLJSONObject::Add(const char *pszName, const CPLJSONObject &oValue)
{
    if( nullptr == pszName )
        return;
    char objectName[JSON_NAME_MAX_SIZE];
    CPLJSONObject object = GetObjectByPath( pszName, &objectName[0] );
    if( object.IsValid() )
    {
        json_object_object_add( TO_JSONOBJ(object.GetInternalHandle()), objectName,
                                json_object_get( TO_JSONOBJ(oValue.GetInternalHandle()) ) );
    }
}

/**
 * Add new key - value pair to json object.
 * @param pszName  Key name.
 * @param bValue   Boolean value.
 *
 * @since GDAL 2.3
 */
void CPLJSONObject::Add(const char *pszName, bool bValue)
{
    if( nullptr == pszName )
        return;
    char objectName[JSON_NAME_MAX_SIZE];
    CPLJSONObject object = GetObjectByPath( pszName, &objectName[0] );
    if(object.IsValid())
    {
        json_object *poVal = json_object_new_boolean( bValue );
        json_object_object_add( TO_JSONOBJ(object.GetInternalHandle()), objectName,
                                poVal );
    }
}

/**
 * Change value by key.
 * @param pszName  Key name.
 * @param pszValue String value.
 *
 * @since GDAL 2.3
 */
void CPLJSONObject::Set(const char *pszName, const char *pszValue)
{
    Delete( pszName );
    Add( pszName, pszValue );
}

/**
 * Change value by key.
 * @param pszName  Key name.
 * @param dfValue  Double value.
 *
 * @since GDAL 2.3
 */
void CPLJSONObject::Set(const char *pszName, double dfValue)
{
    Delete( pszName );
    Add( pszName, dfValue );
}

/**
 * Change value by key.
 * @param pszName  Key name.
 * @param nValue   Integer value.
 *
 * @since GDAL 2.3
 */
void CPLJSONObject::Set(const char *pszName, int nValue)
{
    Delete( pszName );
    Add( pszName, nValue );
}

/**
 * Change value by key.
 * @param pszName  Key name.
 * @param nValue   Long value.
 *
 * @since GDAL 2.3
 */
void CPLJSONObject::Set(const char *pszName, int64_t nValue)
{
    Delete( pszName );
    Add( pszName, nValue );
}

/**
 * Change value by key.
 * @param pszName  Key name.
 * @param bValue   Boolean value.
 *
 * @since GDAL 2.3
 */
void CPLJSONObject::Set(const char *pszName, bool bValue)
{
    Delete( pszName );
    Add( pszName, bValue );
}

/**
 * Get value by key.
 * @param  pszName Key name.
 * @return         Json array object.
 *
 * @since GDAL 2.3
 */
CPLJSONArray CPLJSONObject::GetArray(const char *pszName) const
{
    if( nullptr == pszName )
        return CPLJSONArray( "", nullptr );
    char objectName[JSON_NAME_MAX_SIZE];
    CPLJSONObject object = GetObjectByPath( pszName, &objectName[0] );
    if( object.IsValid() )
    {
        json_object *poVal = nullptr;
        if( json_object_object_get_ex( TO_JSONOBJ(object.GetInternalHandle()),
                                       objectName, &poVal ) )
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
 * @param  pszName Key name.
 * @return         Json object.
 *
 * @since GDAL 2.3
 */
CPLJSONObject CPLJSONObject::GetObject(const char *pszName) const
{
    if( nullptr == pszName )
        return CPLJSONObject( "", nullptr );
    char objectName[JSON_NAME_MAX_SIZE];
    CPLJSONObject object = GetObjectByPath( pszName, &objectName[0] );
    if(object.IsValid())
    {
        json_object* poVal = nullptr;
        if(json_object_object_get_ex( TO_JSONOBJ(object.GetInternalHandle()),
                                      objectName, &poVal ) )
        {
            return CPLJSONObject( objectName, poVal );
        }
    }
    return CPLJSONObject( "", nullptr );
}

/**
 * Get value by key.
 * @param  pszName Key name.
 * @return         Json object.
 *
 * @since GDAL 2.3
 */
CPLJSONObject CPLJSONObject::operator[](const char *pszName) const
{
    return GetObject(pszName);
}

/**
 * Delete json object by key.
 * @param  pszName Key name.
 *
 * @since GDAL 2.3
 */
void CPLJSONObject::Delete(const char *pszName)
{
    if(nullptr == pszName)
        return;
    char objectName[JSON_NAME_MAX_SIZE];
    CPLJSONObject object = GetObjectByPath( pszName, &objectName[0] );
    if(object.IsValid())
    {
        json_object_object_del( TO_JSONOBJ(object.GetInternalHandle()), objectName );
    }
}

/**
 * Get value by key.
 * @param  pszName    Key name.
 * @param  pszDefault Default value.
 * @return            String value.
 *
 * @since GDAL 2.3
 */
const char *CPLJSONObject::GetString(const char *pszName, const char* pszDefault) const
{
    if( nullptr == pszName )
        return pszDefault;
    CPLJSONObject object = GetObject( pszName );
    return object.GetString( pszDefault );
}

/**
 * Get value by key.
 * @param  pszDefault Default value.
 * @return            String value.
 *
 * @since GDAL 2.3
 */
const char* CPLJSONObject::GetString(const char* pszDefault) const
{
    if( m_poJsonObject && json_object_get_type( TO_JSONOBJ(m_poJsonObject) ) ==
            json_type_string )
        return json_object_get_string( TO_JSONOBJ(m_poJsonObject) );
    return pszDefault;
}

/**
 * Get value by key.
 * @param  pszName    Key name.
 * @param  dfDefault  Default value.
 * @return            Double value.
 *
 * @since GDAL 2.3
 */
double CPLJSONObject::GetDouble(const char *pszName, double dfDefault) const
{
    if( nullptr == pszName )
        return dfDefault;
    CPLJSONObject object = GetObject( pszName );
    return object.GetDouble( dfDefault );
}

/**
 * Get value by key.
 * @param  dfDefault  Default value.
 * @return            Double value.
 *
 * @since GDAL 2.3
 */
double CPLJSONObject::GetDouble(double dfDefault) const
{
    if( m_poJsonObject /*&& json_object_get_type( TO_JSONOBJ(m_poJsonObject) ) ==
            json_type_double*/ )
        return json_object_get_double( TO_JSONOBJ(m_poJsonObject) );
    return dfDefault;
}

/**
 * Get value by key.
 * @param  pszName    Key name.
 * @param  nDefault   Default value.
 * @return            Integer value.
 *
 * @since GDAL 2.3
 */
int CPLJSONObject::GetInteger(const char *pszName, int nDefault) const
{
    if( nullptr == pszName )
        return nDefault;
    CPLJSONObject object = GetObject( pszName );
    return object.GetInteger( nDefault );
}

/**
 * Get value by key.
 * @param  nDefault   Default value.
 * @return            Integer value.
 *
 * @since GDAL 2.3
 */
int CPLJSONObject::GetInteger(int nDefault) const
{
    if( m_poJsonObject /*&& json_object_get_type( TO_JSONOBJ(m_poJsonObject) ) ==
            json_type_int*/ )
        return json_object_get_int( TO_JSONOBJ(m_poJsonObject) );
    return nDefault;
}

/**
 * Get value by key.
 * @param  pszName    Key name.
 * @param  nDefault   Default value.
 * @return            Long value.
 *
 * @since GDAL 2.3
 */
int64_t CPLJSONObject::GetLong(const char *pszName, int64_t nDefault) const
{
    if( nullptr == pszName )
        return nDefault;
    CPLJSONObject object = GetObject( pszName );
    return object.GetLong( nDefault );
}

/**
 * Get value by key.
 * @param  nDefault   Default value.
 * @return            Long value.
 *
 * @since GDAL 2.3
 */
int64_t CPLJSONObject::GetLong(int64_t nDefault) const
{
    if( m_poJsonObject /*&& json_object_get_type( TO_JSONOBJ(m_poJsonObject) ) ==
            json_type_int*/ )
        return json_object_get_int64( TO_JSONOBJ(m_poJsonObject) );
    return nDefault;
}

/**
 * Get value by key.
 * @param  pszName    Key name.
 * @param  bDefault   Default value.
 * @return            Boolean value.
 *
 * @since GDAL 2.3
 */
bool CPLJSONObject::GetBool(const char *pszName, bool bDefault) const
{
    if( nullptr == pszName )
        return bDefault;
    CPLJSONObject object = GetObject( pszName );
    return object.GetBool( bDefault );
}

/**
 * \brief Get json object children.
 *
 * This is not an array [], but list {}. Any modification of children will not store in json document. This function is useful when keys is not know and need to iterate over json object items and get keys and values.
 *
 * @return Array of CPLJSONObject pointers. The caller must free this array using CPLJSONObject::DestroyJSONObjectList static function.
 *
 * @since GDAL 2.3
 */
CPLJSONObject **CPLJSONObject::GetChildren() const
{
    if(nullptr == m_poJsonObject)
    {
        return nullptr;
    }
    CPLJSONObject **papoChildren = nullptr;
    size_t nChildrenCount = 0;
    json_object_iter it;
    it.key = nullptr;
    it.val = nullptr;
    it.entry = nullptr;
    json_object_object_foreachC( TO_JSONOBJ(m_poJsonObject), it ) {
        CPLJSONObject *child = new CPLJSONObject(it.key, it.val);
        papoChildren = reinterpret_cast<CPLJSONObject **>(
            CPLRealloc( papoChildren,  sizeof(CPLJSONObject *) *
                        (nChildrenCount + 1) ) );
        papoChildren[nChildrenCount++] = child;
    }

    papoChildren = reinterpret_cast<CPLJSONObject **>(
        CPLRealloc( papoChildren,  sizeof(CPLJSONObject *) *
                    (nChildrenCount + 1) ) );
    papoChildren[nChildrenCount] = nullptr;

    return papoChildren;
}

/**
 * Get value by key.
 * @param  bDefault   Default value.
 * @return            Boolean value.
 *
 * @since GDAL 2.3
 */
bool CPLJSONObject::GetBool(bool bDefault) const
{
    if( m_poJsonObject /*&& json_object_get_type( TO_JSONOBJ(m_poJsonObject) ) ==
            json_type_boolean*/ )
        return json_object_get_boolean( TO_JSONOBJ(m_poJsonObject) ) == 1;
    return bDefault;
}

/*! @cond Doxygen_Suppress */
CPLJSONObject CPLJSONObject::GetObjectByPath(const char *pszPath, char *pszName) const
{
    json_object *poVal = nullptr;
    CPLStringList pathPortions( CSLTokenizeString2( pszPath, JSON_PATH_DELIMITER,
                                                    0 ) );
    int portionsCount = pathPortions.size();
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
            object = CPLJSONObject( pathPortions[i], object );
        }
    }

//    // Check if such object already  exists
//    if(json_object_object_get_ex(object.m_jsonObject,
//                                 pathPortions[portionsCount - 1], &poVal))
//        return JSONObject(nullptr);

    CPLStrlcpy( pszName, pathPortions[portionsCount - 1], JSON_NAME_MAX_SIZE );
    return object;
}
/*! @endcond */

/**
 * Get json object type.
 * @return Json object type.
 */
CPLJSONObject::Type CPLJSONObject::GetType() const
{
    if(nullptr == m_poJsonObject)
        return CPLJSONObject::Null;
    switch ( json_object_get_type( TO_JSONOBJ(m_poJsonObject) ) ) {
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
    return CPLJSONObject::Null;
}

/**
 * Check if json object valid.
 * @return true if json object valid.
 */
bool CPLJSONObject::IsValid() const
{
    return nullptr != m_poJsonObject;
}

/**
 * Free memory allocated by GetChildren method.
 * @param papsoList Null terminated array of json object pointers.
 */
void CPLJSONObject::DestroyJSONObjectList(CPLJSONObject **papsoList)
{
    if( !papsoList )
        return;

    for( CPLJSONObject **papsoPtr = papsoList; *papsoPtr != nullptr; ++papsoPtr )
    {
        CPLFree(*papsoPtr);
    }

    CPLFree(papsoList);
}

//------------------------------------------------------------------------------
// JSONArray
//------------------------------------------------------------------------------
/*! @cond Doxygen_Suppress */
CPLJSONArray::CPLJSONArray()
{
    m_poJsonObject = json_object_new_array();
}

CPLJSONArray::CPLJSONArray(const CPLString &soName) :
    CPLJSONObject( soName, json_object_new_array() )
{

}

CPLJSONArray::CPLJSONArray(const CPLString &soName, JSONObjectH poJsonObject) :
    CPLJSONObject(soName, poJsonObject)
{

}
/*! @endcond */

/**
 * Get array size.
 * @return Array size.
 */
int CPLJSONArray::Size() const
{
    if( nullptr == m_poJsonObject )
        return 0;
    return json_object_array_length( TO_JSONOBJ(m_poJsonObject) );
}

/**
 * Add json object to array.
 * @param oValue Json array.
 */
void CPLJSONArray::Add(const CPLJSONObject &oValue)
{
    if( oValue.m_poJsonObject )
        json_object_array_add( TO_JSONOBJ(m_poJsonObject),
                               json_object_get( TO_JSONOBJ(oValue.m_poJsonObject) ) );
}

/**
 * Get array item by index.
 * @param  nIndex Item index.
 * @return        Json object.
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
 */
const CPLJSONObject CPLJSONArray::operator[](int nIndex) const
{
    return CPLJSONObject( CPLSPrintf("id:%d", nIndex),
                          json_object_array_get_idx( TO_JSONOBJ(m_poJsonObject),
                                                     nIndex ) );
}
