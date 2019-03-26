/**********************************************************************
 * $Id$
 *
 * Name:     cpl_string.h
 * Project:  CPL - Common Portability Library
 * Purpose:  String and StringList functions.
 * Author:   Daniel Morissette, dmorissette@mapgears.com
 *
 **********************************************************************
 * Copyright (c) 1998, Daniel Morissette
 * Copyright (c) 2008-2014, Even Rouault <even dot rouault at mines-paris dot org>
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#ifndef CPL_STRING_H_INCLUDED
#define CPL_STRING_H_INCLUDED

#include "cpl_error.h"
#include "cpl_conv.h"
#include "cpl_vsi.h"

/**
 * \file cpl_string.h
 *
 * Various convenience functions for working with strings and string lists.
 *
 * A StringList is just an array of strings with the last pointer being
 * NULL.  An empty StringList may be either a NULL pointer, or a pointer to
 * a pointer memory location with a NULL value.
 *
 * A common convention for StringLists is to use them to store name/value
 * lists.  In this case the contents are treated like a dictionary of
 * name/value pairs.  The actual data is formatted with each string having
 * the format "<name>:<value>" (though "=" is also an acceptable separator).
 * A number of the functions in the file operate on name/value style
 * string lists (such as CSLSetNameValue(), and CSLFetchNameValue()).
 *
 * To some extent the CPLStringList C++ class can be used to abstract
 * managing string lists a bit but still be able to return them from C
 * functions.
 *
 */

CPL_C_START

char CPL_DLL **CSLAddString(char **papszStrList,
                            const char *pszNewString) CPL_WARN_UNUSED_RESULT;
char CPL_DLL **CSLAddStringMayFail(
    char **papszStrList, const char *pszNewString) CPL_WARN_UNUSED_RESULT;
int CPL_DLL CSLCount(CSLConstList papszStrList);
const char CPL_DLL *CSLGetField( CSLConstList, int );
void CPL_DLL CPL_STDCALL CSLDestroy(char **papszStrList);
char CPL_DLL **CSLDuplicate(CSLConstList papszStrList) CPL_WARN_UNUSED_RESULT;
char CPL_DLL **CSLMerge( char **papszOrig,
                         CSLConstList papszOverride ) CPL_WARN_UNUSED_RESULT;

char CPL_DLL **CSLTokenizeString(const char *pszString ) CPL_WARN_UNUSED_RESULT;
char CPL_DLL **CSLTokenizeStringComplex(
    const char *pszString, const char *pszDelimiter, int bHonourStrings,
    int bAllowEmptyTokens ) CPL_WARN_UNUSED_RESULT;
char CPL_DLL **CSLTokenizeString2( const char *pszString,
                                   const char *pszDelimiter,
                                   int nCSLTFlags ) CPL_WARN_UNUSED_RESULT;

/** Flag for CSLTokenizeString2() to honour strings */
#define CSLT_HONOURSTRINGS      0x0001
/** Flag for CSLTokenizeString2() to allow empty tokens */
#define CSLT_ALLOWEMPTYTOKENS   0x0002
/** Flag for CSLTokenizeString2() to preserve quotes */
#define CSLT_PRESERVEQUOTES     0x0004
/** Flag for CSLTokenizeString2() to preserve escape characters */
#define CSLT_PRESERVEESCAPES    0x0008
/** Flag for CSLTokenizeString2() to strip leading spaces */
#define CSLT_STRIPLEADSPACES    0x0010
/** Flag for CSLTokenizeString2() to strip trailaing spaces */
#define CSLT_STRIPENDSPACES     0x0020

int CPL_DLL CSLPrint(CSLConstList papszStrList, FILE *fpOut);
char CPL_DLL **CSLLoad(const char *pszFname) CPL_WARN_UNUSED_RESULT;
char CPL_DLL **CSLLoad2(
    const char *pszFname, int nMaxLines, int nMaxCols,
    CSLConstList papszOptions) CPL_WARN_UNUSED_RESULT;
int CPL_DLL CSLSave(CSLConstList papszStrList, const char *pszFname);

char CPL_DLL **CSLInsertStrings(char **papszStrList, int nInsertAtLineNo,
                        CSLConstList papszNewLines) CPL_WARN_UNUSED_RESULT;
char CPL_DLL **CSLInsertString(char **papszStrList, int nInsertAtLineNo,
                               const char *pszNewLine) CPL_WARN_UNUSED_RESULT;
char CPL_DLL **CSLRemoveStrings(
    char **papszStrList, int nFirstLineToDelete,
    int nNumToRemove, char ***ppapszRetStrings) CPL_WARN_UNUSED_RESULT;
int CPL_DLL CSLFindString( CSLConstList papszList, const char *pszTarget );
int CPL_DLL CSLFindStringCaseSensitive( CSLConstList papszList,
                                        const char *pszTarget );
int CPL_DLL CSLPartialFindString( CSLConstList papszHaystack,
                                  const char *pszNeedle );
int CPL_DLL CSLFindName(CSLConstList papszStrList, const char *pszName);
int CPL_DLL CSLFetchBoolean( CSLConstList papszStrList, const char *pszKey,
                             int bDefault );

/* TODO: Deprecate CSLTestBoolean.  Remove in GDAL 3.x. */
int CPL_DLL CSLTestBoolean( const char *pszValue );
/* Do not use CPLTestBoolean in C++ code.  Use CPLTestBool. */
int CPL_DLL CPLTestBoolean( const char *pszValue );

#if defined(__cplusplus) && !defined(CPL_SUPRESS_CPLUSPLUS)
#ifdef DO_NOT_USE_DEBUG_BOOL
#define CPLTestBool(x) CPL_TO_BOOL(CPLTestBoolean(x))
#define CPLFetchBool(list,key,default) \
    CPL_TO_BOOL(CSLFetchBoolean(list,key,default))
#else /* DO_NOT_USE_DEBUG_BOOL */
/* Prefer these for C++ code. */
#ifdef DEBUG_BOOL
extern "C++" {
#endif
bool CPL_DLL CPLTestBool( const char *pszValue );
bool CPL_DLL CPLFetchBool( CSLConstList papszStrList, const char *pszKey,
                           bool bDefault );
#ifdef DEBUG_BOOL
}
#endif
#endif
#endif  /* __cplusplus */

const char CPL_DLL *
      CPLParseNameValue( const char *pszNameValue, char **ppszKey );

const char CPL_DLL *
      CSLFetchNameValue( CSLConstList papszStrList, const char *pszName);
const char CPL_DLL *
      CSLFetchNameValueDef( CSLConstList papszStrList,
                            const char *pszName,
                            const char *pszDefault );
char CPL_DLL **
      CSLFetchNameValueMultiple(CSLConstList papszStrList, const char *pszName);
char CPL_DLL **
      CSLAddNameValue(char **papszStrList,
                      const char *pszName,
                      const char *pszValue) CPL_WARN_UNUSED_RESULT;
char CPL_DLL **
      CSLSetNameValue(char **papszStrList,
                      const char *pszName,
                      const char *pszValue) CPL_WARN_UNUSED_RESULT;
void CPL_DLL CSLSetNameValueSeparator( char ** papszStrList,
                                       const char *pszSeparator );

char CPL_DLL ** CSLParseCommandLine(const char* pszCommandLine);

/** Scheme for CPLEscapeString()/CPLUnescapeString() for backlash quoting */
#define CPLES_BackslashQuotable 0
/** Scheme for CPLEscapeString()/CPLUnescapeString() for XML */
#define CPLES_XML               1
/** Scheme for CPLEscapeString()/CPLUnescapeString() for URL */
#define CPLES_URL               2
/** Scheme for CPLEscapeString()/CPLUnescapeString() for SQL */
#define CPLES_SQL               3
/** Scheme for CPLEscapeString()/CPLUnescapeString() for CSV */
#define CPLES_CSV               4
/** Scheme for CPLEscapeString()/CPLUnescapeString() for XML (preserves quotes) */
#define CPLES_XML_BUT_QUOTES    5
/** Scheme for CPLEscapeString()/CPLUnescapeString() for CSV (forced quoting) */
#define CPLES_CSV_FORCE_QUOTING 6
/** Scheme for CPLEscapeString()/CPLUnescapeString() for SQL identifiers */
#define CPLES_SQLI              7

char CPL_DLL *CPLEscapeString( const char *pszString, int nLength,
                               int nScheme ) CPL_WARN_UNUSED_RESULT;
char CPL_DLL *CPLUnescapeString( const char *pszString, int *pnLength,
                                 int nScheme ) CPL_WARN_UNUSED_RESULT;

char CPL_DLL *CPLBinaryToHex( int nBytes,
                              const GByte *pabyData ) CPL_WARN_UNUSED_RESULT;
GByte CPL_DLL *CPLHexToBinary( const char *pszHex,
                               int *pnBytes ) CPL_WARN_UNUSED_RESULT;

char CPL_DLL *CPLBase64Encode( int nBytes,
                               const GByte *pabyData ) CPL_WARN_UNUSED_RESULT;
int CPL_DLL CPLBase64DecodeInPlace( GByte* pszBase64 ) CPL_WARN_UNUSED_RESULT;

/** Type of value */
typedef enum
{
    CPL_VALUE_STRING,  /**< String */
    CPL_VALUE_REAL,    /**< Real number */
    CPL_VALUE_INTEGER  /**< Integer */
} CPLValueType;

CPLValueType CPL_DLL CPLGetValueType(const char* pszValue);

size_t CPL_DLL CPLStrlcpy(char* pszDest, const char* pszSrc, size_t nDestSize);
size_t CPL_DLL CPLStrlcat(char* pszDest, const char* pszSrc, size_t nDestSize);
size_t CPL_DLL CPLStrnlen(const char *pszStr, size_t nMaxLen);

/* -------------------------------------------------------------------- */
/*      Locale independent formatting functions.                        */
/* -------------------------------------------------------------------- */
int CPL_DLL CPLvsnprintf( char *str, size_t size,
                          CPL_FORMAT_STRING(const char* fmt),
                          va_list args )
    CPL_PRINT_FUNC_FORMAT(3, 0 );

/* ALIAS_CPLSNPRINTF_AS_SNPRINTF might be defined to enable GCC 7 */
/* -Wformat-truncation= warnings, but shouldn't be set for normal use */
#if defined(ALIAS_CPLSNPRINTF_AS_SNPRINTF)
#define CPLsnprintf snprintf
#else
int CPL_DLL CPLsnprintf( char *str, size_t size,
                         CPL_FORMAT_STRING(const char* fmt), ... )
    CPL_PRINT_FUNC_FORMAT(3, 4);
#endif

/*! @cond Doxygen_Suppress */
#if defined(GDAL_COMPILATION) && !defined(DONT_DEPRECATE_SPRINTF)
int CPL_DLL CPLsprintf( char *str, CPL_FORMAT_STRING(const char* fmt), ... )
    CPL_PRINT_FUNC_FORMAT(2, 3)
    CPL_WARN_DEPRECATED("Use CPLsnprintf instead");
#else
int CPL_DLL CPLsprintf( char *str, CPL_FORMAT_STRING(const char* fmt), ... )
    CPL_PRINT_FUNC_FORMAT(2, 3);
#endif
/*! @endcond */
int CPL_DLL CPLprintf( CPL_FORMAT_STRING(const char* fmt), ... )
    CPL_PRINT_FUNC_FORMAT(1, 2);

/* For some reason Doxygen_Suppress is needed to avoid warning. Not sure why */
/*! @cond Doxygen_Suppress */
/* caution: only works with limited number of formats */
int CPL_DLL CPLsscanf( const char* str,
                       CPL_SCANF_FORMAT_STRING(const char* fmt), ... )
    CPL_SCAN_FUNC_FORMAT(2, 3);
/*! @endcond */

const char CPL_DLL *CPLSPrintf( CPL_FORMAT_STRING(const char *fmt), ... )
    CPL_PRINT_FUNC_FORMAT(1, 2) CPL_WARN_UNUSED_RESULT;
char CPL_DLL **CSLAppendPrintf( char **papszStrList,
                                CPL_FORMAT_STRING(const char *fmt), ... )
    CPL_PRINT_FUNC_FORMAT(2, 3) CPL_WARN_UNUSED_RESULT;
int CPL_DLL CPLVASPrintf( char **buf,
                          CPL_FORMAT_STRING(const char *fmt), va_list args )
    CPL_PRINT_FUNC_FORMAT(2, 0);

/* -------------------------------------------------------------------- */
/*      RFC 23 character set conversion/recoding API (cpl_recode.cpp).  */
/* -------------------------------------------------------------------- */
/** Encoding of the current locale */
#define CPL_ENC_LOCALE     ""
/** UTF-8 encoding */
#define CPL_ENC_UTF8       "UTF-8"
/** UTF-16 encoding */
#define CPL_ENC_UTF16      "UTF-16"
/** UCS-2 encoding */
#define CPL_ENC_UCS2       "UCS-2"
/** UCS-4 encoding */
#define CPL_ENC_UCS4       "UCS-4"
/** ASCII encoding */
#define CPL_ENC_ASCII      "ASCII"
/** ISO-8859-1 (LATIN1) encoding */
#define CPL_ENC_ISO8859_1  "ISO-8859-1"

int CPL_DLL  CPLEncodingCharSize( const char *pszEncoding );
/*! @cond Doxygen_Suppress */
void CPL_DLL  CPLClearRecodeWarningFlags( void );
/*! @endcond */
char CPL_DLL *CPLRecode(
    const char *pszSource, const char *pszSrcEncoding,
    const char *pszDstEncoding ) CPL_WARN_UNUSED_RESULT CPL_RETURNS_NONNULL;
char CPL_DLL *CPLRecodeFromWChar(
    const wchar_t *pwszSource, const char *pszSrcEncoding,
    const char *pszDstEncoding ) CPL_WARN_UNUSED_RESULT;
wchar_t CPL_DLL *CPLRecodeToWChar(
    const char *pszSource, const char *pszSrcEncoding,
    const char *pszDstEncoding ) CPL_WARN_UNUSED_RESULT;
int CPL_DLL CPLIsUTF8( const char* pabyData, int nLen );
char CPL_DLL *CPLForceToASCII(
    const char* pabyData, int nLen,
    char chReplacementChar ) CPL_WARN_UNUSED_RESULT;
int CPL_DLL CPLStrlenUTF8( const char *pszUTF8Str );
CPL_C_END

/************************************************************************/
/*                              CPLString                               */
/************************************************************************/

#if defined(__cplusplus) && !defined(CPL_SUPRESS_CPLUSPLUS)

extern "C++"
{
#ifndef DOXYGEN_SKIP
#include <string>
#endif

// VC++ implicitly applies __declspec(dllexport) to template base classes
// of classes marked with __declspec(dllexport).
// Hence, if marked with CPL_DLL, VC++ would export symbols for the specialization
// of std::basic_string<char>, since it is a base class of CPLString.
// As a result, if an application linked both gdal.dll and a static library that
// (implicitly) instantiates std::string (almost all do!), then the linker would
// emit an error concerning duplicate symbols for std::string.
// The least intrusive solution is to not mark the whole class with
// __declspec(dllexport) for VC++, but only its non-inline methods.
#ifdef _MSC_VER
#  define CPLSTRING_CLASS_DLL
#  define CPLSTRING_METHOD_DLL CPL_DLL
#else
/*! @cond Doxygen_Suppress */
#  define CPLSTRING_CLASS_DLL CPL_DLL
#  define CPLSTRING_METHOD_DLL
/*! @endcond */
#endif

//! Convenient string class based on std::string.
class CPLSTRING_CLASS_DLL CPLString : public std::string
{
public:

    /** Constructor */
    CPLString(void) {}
    /** Constructor */
    // cppcheck-suppress noExplicitConstructor
    CPLString( const std::string &oStr ) : std::string( oStr ) {}
    /** Constructor */
    // cppcheck-suppress noExplicitConstructor
    CPLString( const char *pszStr ) : std::string( pszStr ) {}
    /** Constructor */
    CPLString( const char *pszStr, size_t n ) : std::string( pszStr, n ) {}

    /** Return string as zero terminated character array */
    operator const char* (void) const { return c_str(); }

    /** Return character at specified index */
    char& operator[](std::string::size_type i)
    {
        return std::string::operator[](i);
    }

    /** Return character at specified index */
    const char& operator[](std::string::size_type i) const
    {
        return std::string::operator[](i);
    }

    /** Return character at specified index */
    char& operator[](int i)
    {
        return std::string::operator[](
            static_cast<std::string::size_type>(i));
    }

    /** Return character at specified index */
    const char& operator[](int i) const
    {
        return std::string::operator[](
            static_cast<std::string::size_type>(i));
    }

    /** Clear the string */
    void Clear() { resize(0); }

    /** Assign specified string and take ownership of it (assumed to be
     * allocated with CPLMalloc()). NULL can be safely passed to clear the
     * string. */
    void Seize( char *pszValue )
    {
        if (pszValue == nullptr )
            Clear();
        else
        {
            *this = pszValue;
            CPLFree(pszValue);
        }
    }

    /* There seems to be a bug in the way the compiler count indices...
     * Should be CPL_PRINT_FUNC_FORMAT (1, 2) */
    CPLSTRING_METHOD_DLL CPLString &Printf(
        CPL_FORMAT_STRING(const char *pszFormat), ... )
        CPL_PRINT_FUNC_FORMAT (2, 3);
    CPLSTRING_METHOD_DLL CPLString &vPrintf(
        CPL_FORMAT_STRING(const char *pszFormat), va_list args )
        CPL_PRINT_FUNC_FORMAT(2, 0);
    CPLSTRING_METHOD_DLL CPLString &FormatC( double dfValue, const char *pszFormat = nullptr );
    CPLSTRING_METHOD_DLL CPLString &Trim();
    CPLSTRING_METHOD_DLL CPLString &Recode( const char *pszSrcEncoding, const char *pszDstEncoding );
    CPLSTRING_METHOD_DLL CPLString &replaceAll(
        const std::string &osBefore, const std::string& osAfter );
    CPLSTRING_METHOD_DLL CPLString &replaceAll( const std::string &osBefore, char chAfter );
    CPLSTRING_METHOD_DLL CPLString &replaceAll( char chBefore, const std::string &osAfter );
    CPLSTRING_METHOD_DLL CPLString &replaceAll( char chBefore, char chAfter );

    /* case insensitive find alternates */
    CPLSTRING_METHOD_DLL size_t    ifind( const std::string & str, size_t pos = 0 ) const;
    CPLSTRING_METHOD_DLL size_t    ifind( const char * s, size_t pos = 0 ) const;
    CPLSTRING_METHOD_DLL CPLString &toupper( void );
    CPLSTRING_METHOD_DLL CPLString &tolower( void );

    CPLSTRING_METHOD_DLL bool      endsWith( const std::string& osStr ) const;
};

#undef CPLSTRING_CLASS_DLL
#undef CPLSTRING_METHOD_DLL

CPLString CPL_DLL CPLOPrintf(CPL_FORMAT_STRING(const char *pszFormat), ... )
    CPL_PRINT_FUNC_FORMAT (1, 2);
CPLString CPL_DLL CPLOvPrintf(
    CPL_FORMAT_STRING(const char *pszFormat), va_list args)
    CPL_PRINT_FUNC_FORMAT (1, 0);
CPLString CPL_DLL CPLQuotedSQLIdentifier(const char *pszIdent);

/* -------------------------------------------------------------------- */
/*      URL processing functions, here since they depend on CPLString.  */
/* -------------------------------------------------------------------- */
CPLString CPL_DLL CPLURLGetValue(const char* pszURL, const char* pszKey);
CPLString CPL_DLL CPLURLAddKVP(const char* pszURL, const char* pszKey,
                               const char* pszValue);

/************************************************************************/
/*                            CPLStringList                             */
/************************************************************************/

//! String list class designed around our use of C "char**" string lists.
class CPL_DLL CPLStringList
{
    char **papszList = nullptr;
    mutable int nCount = 0;
    mutable int nAllocation = 0;
    bool   bOwnList = false;
    bool   bIsSorted = false;

    void   MakeOurOwnCopy();
    void   EnsureAllocation( int nMaxLength );
    int    FindSortedInsertionPoint( const char *pszLine );

  public:
    CPLStringList();
    explicit CPLStringList( char **papszList, int bTakeOwnership=TRUE );
    explicit CPLStringList( CSLConstList papszList );
    CPLStringList( const CPLStringList& oOther );
    ~CPLStringList();

    CPLStringList &Clear();

    /** Return size of list */
    int    size() const { return Count(); }
    int    Count() const;

    /** Return whether the list is empty. */
    bool   empty() const { return Count() == 0; }

    CPLStringList &AddString( const char *pszNewString );
    CPLStringList &AddStringDirectly( char *pszNewString );

    CPLStringList &InsertString( int nInsertAtLineNo, const char *pszNewLine )
    { return InsertStringDirectly( nInsertAtLineNo, CPLStrdup(pszNewLine) ); }
    CPLStringList &InsertStringDirectly( int nInsertAtLineNo, char *pszNewLine);

    // CPLStringList &InsertStrings( int nInsertAtLineNo, char **papszNewLines );
    // CPLStringList &RemoveStrings( int nFirstLineToDelete, int nNumToRemove=1 );

    /** Return index of pszTarget in the list, or -1 */
    int    FindString( const char *pszTarget ) const
    { return CSLFindString( papszList, pszTarget ); }
    /** Return index of pszTarget in the list (using partial search), or -1 */
    int    PartialFindString( const char *pszNeedle ) const
    { return CSLPartialFindString( papszList, pszNeedle ); }

    int    FindName( const char *pszName ) const;
    bool   FetchBool( const char *pszKey, bool bDefault ) const;
    // Deprecated.
    int    FetchBoolean( const char *pszKey, int bDefault ) const;
    const char *FetchNameValue( const char *pszKey ) const;
    const char *FetchNameValueDef(
        const char *pszKey, const char *pszDefault ) const;
    CPLStringList &AddNameValue( const char *pszKey, const char *pszValue );
    CPLStringList &SetNameValue( const char *pszKey, const char *pszValue );

    CPLStringList &Assign( char **papszListIn, int bTakeOwnership=TRUE );
    /** Assignment operator */
    CPLStringList &operator=(char **papszListIn) {
      return Assign( papszListIn, TRUE ); }
    /** Assignment operator */
    CPLStringList &operator=(const CPLStringList& oOther);
    /** Assignment operator */
    CPLStringList &operator=(CSLConstList papszListIn);

    /** Return string at specified index */
    char * operator[](int i);
    /** Return string at specified index */
    char * operator[](size_t i) { return (*this)[static_cast<int>(i)]; }
    /** Return string at specified index */
    const char * operator[](int i) const;
    /** Return string at specified index */
    const char * operator[](size_t i) const {
      return (*this)[static_cast<int>(i)]; }
    /** Return value corresponding to pszKey, or nullptr */
    const char * operator[](const char* pszKey) const {
        return FetchNameValue(pszKey); }

    /** Return list. Ownership remains to the object */
    char** List() { return papszList; }
    /** Return list. Ownership remains to the object */
    CSLConstList List() const { return papszList; }
    char **StealList();

    CPLStringList &Sort();
    /** Returns whether the list is sorted */
    int    IsSorted() const { return bIsSorted; }

    /** Return lists */
    operator char**(void) { return List(); }
    /** Return lists */
    operator CSLConstList(void) const { return List(); }
};

#ifdef GDAL_COMPILATION

#include <memory>

/*! @cond Doxygen_Suppress */
struct CPL_DLL CSLDestroyReleaser
{
    void operator()(char** papszStr) const { CSLDestroy(papszStr); }
};
/*! @endcond */

/** Unique pointer type to use with CSL functions returning a char** */
using CSLUniquePtr = std::unique_ptr< char*, CSLDestroyReleaser>;

/*! @cond Doxygen_Suppress */
struct CPL_DLL CPLFreeReleaser
{
    void operator()(void* p) const { CPLFree(p); }
};
/*! @endcond */

/** Unique pointer type to use with functions returning a char* to release with CPLFree */
using CPLCharUniquePtr = std::unique_ptr<char, CPLFreeReleaser>;

#endif

} // extern "C++"

#endif /* def __cplusplus && !CPL_SUPRESS_CPLUSPLUS */

#endif /* CPL_STRING_H_INCLUDED */
