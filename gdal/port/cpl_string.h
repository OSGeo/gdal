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

#ifndef _CPL_STRING_H_INCLUDED
#define _CPL_STRING_H_INCLUDED

#include "cpl_vsi.h"
#include "cpl_error.h"
#include "cpl_conv.h"

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

char CPL_DLL **CSLAddString(char **papszStrList, const char *pszNewString) CPL_WARN_UNUSED_RESULT;
int CPL_DLL CSLCount(char **papszStrList);
const char CPL_DLL *CSLGetField( char **, int );
void CPL_DLL CPL_STDCALL CSLDestroy(char **papszStrList);
char CPL_DLL **CSLDuplicate(char **papszStrList) CPL_WARN_UNUSED_RESULT;
char CPL_DLL **CSLMerge( char **papszOrig, char **papszOverride ) CPL_WARN_UNUSED_RESULT;

char CPL_DLL **CSLTokenizeString(const char *pszString ) CPL_WARN_UNUSED_RESULT;
char CPL_DLL **CSLTokenizeStringComplex(const char *pszString,
                                   const char *pszDelimiter,
                                   int bHonourStrings, int bAllowEmptyTokens ) CPL_WARN_UNUSED_RESULT;
char CPL_DLL **CSLTokenizeString2( const char *pszString, 
                                   const char *pszDelimeter, 
                                   int nCSLTFlags ) CPL_WARN_UNUSED_RESULT;

#define CSLT_HONOURSTRINGS      0x0001
#define CSLT_ALLOWEMPTYTOKENS   0x0002
#define CSLT_PRESERVEQUOTES     0x0004
#define CSLT_PRESERVEESCAPES    0x0008
#define CSLT_STRIPLEADSPACES    0x0010
#define CSLT_STRIPENDSPACES     0x0020

int CPL_DLL CSLPrint(char **papszStrList, FILE *fpOut);
char CPL_DLL **CSLLoad(const char *pszFname) CPL_WARN_UNUSED_RESULT;
char CPL_DLL **CSLLoad2(const char *pszFname, int nMaxLines, int nMaxCols, char** papszOptions) CPL_WARN_UNUSED_RESULT;
int CPL_DLL CSLSave(char **papszStrList, const char *pszFname);

char CPL_DLL **CSLInsertStrings(char **papszStrList, int nInsertAtLineNo, 
                         char **papszNewLines) CPL_WARN_UNUSED_RESULT;
char CPL_DLL **CSLInsertString(char **papszStrList, int nInsertAtLineNo, 
                               const char *pszNewLine) CPL_WARN_UNUSED_RESULT;
char CPL_DLL **CSLRemoveStrings(char **papszStrList, int nFirstLineToDelete,
                         int nNumToRemove, char ***ppapszRetStrings) CPL_WARN_UNUSED_RESULT;
int CPL_DLL CSLFindString( char **, const char * );
int CPL_DLL CSLPartialFindString( char **papszHaystack, 
	const char * pszNeedle );
int CPL_DLL CSLFindName(char **papszStrList, const char *pszName);
int CPL_DLL CSLTestBoolean( const char *pszValue );
int CPL_DLL CSLFetchBoolean( char **papszStrList, const char *pszKey, 
                             int bDefault );

const char CPL_DLL *CPLSPrintf(const char *fmt, ...) CPL_PRINT_FUNC_FORMAT(1, 2);
char CPL_DLL **CSLAppendPrintf(char **papszStrList, const char *fmt, ...) CPL_PRINT_FUNC_FORMAT(2, 3) CPL_WARN_UNUSED_RESULT;
int CPL_DLL CPLVASPrintf(char **buf, const char *fmt, va_list args );

const char CPL_DLL *
      CPLParseNameValue(const char *pszNameValue, char **ppszKey );
const char CPL_DLL *
      CSLFetchNameValue(char **papszStrList, const char *pszName);
const char CPL_DLL *
      CSLFetchNameValueDef(char **papszStrList, const char *pszName,
                           const char *pszDefault );
char CPL_DLL **
      CSLFetchNameValueMultiple(char **papszStrList, const char *pszName);
char CPL_DLL **
      CSLAddNameValue(char **papszStrList, 
                      const char *pszName, const char *pszValue) CPL_WARN_UNUSED_RESULT;
char CPL_DLL **
      CSLSetNameValue(char **papszStrList, 
                      const char *pszName, const char *pszValue) CPL_WARN_UNUSED_RESULT;
void CPL_DLL CSLSetNameValueSeparator( char ** papszStrList, 
                                       const char *pszSeparator );

#define CPLES_BackslashQuotable 0
#define CPLES_XML               1
#define CPLES_URL               2
#define CPLES_SQL               3
#define CPLES_CSV               4
#define CPLES_XML_BUT_QUOTES    5

char CPL_DLL *CPLEscapeString( const char *pszString, int nLength, 
                               int nScheme ) CPL_WARN_UNUSED_RESULT;
char CPL_DLL *CPLUnescapeString( const char *pszString, int *pnLength,
                                 int nScheme ) CPL_WARN_UNUSED_RESULT;

char CPL_DLL *CPLBinaryToHex( int nBytes, const GByte *pabyData ) CPL_WARN_UNUSED_RESULT;
GByte CPL_DLL *CPLHexToBinary( const char *pszHex, int *pnBytes ) CPL_WARN_UNUSED_RESULT;

char CPL_DLL *CPLBase64Encode( int nBytes, const GByte *pabyData ) CPL_WARN_UNUSED_RESULT;
int CPL_DLL CPLBase64DecodeInPlace(GByte* pszBase64);

typedef enum
{
    CPL_VALUE_STRING,
    CPL_VALUE_REAL,
    CPL_VALUE_INTEGER
} CPLValueType;

CPLValueType CPL_DLL CPLGetValueType(const char* pszValue);

size_t CPL_DLL CPLStrlcpy(char* pszDest, const char* pszSrc, size_t nDestSize);
size_t CPL_DLL CPLStrlcat(char* pszDest, const char* pszSrc, size_t nDestSize);
size_t CPL_DLL CPLStrnlen (const char *pszStr, size_t nMaxLen);

/* -------------------------------------------------------------------- */
/*      RFC 23 character set conversion/recoding API (cpl_recode.cpp).  */
/* -------------------------------------------------------------------- */
#define CPL_ENC_LOCALE     ""
#define CPL_ENC_UTF8       "UTF-8"
#define CPL_ENC_UTF16      "UTF-16"
#define CPL_ENC_UCS2       "UCS-2"
#define CPL_ENC_UCS4       "UCS-4"
#define CPL_ENC_ASCII      "ASCII"
#define CPL_ENC_ISO8859_1  "ISO-8859-1"

int CPL_DLL  CPLEncodingCharSize( const char *pszEncoding );
void CPL_DLL  CPLClearRecodeWarningFlags( void );
char CPL_DLL *CPLRecode( const char *pszSource, 
                         const char *pszSrcEncoding, 
                         const char *pszDstEncoding ) CPL_WARN_UNUSED_RESULT;
char CPL_DLL *CPLRecodeFromWChar( const wchar_t *pwszSource, 
                                  const char *pszSrcEncoding, 
                                  const char *pszDstEncoding ) CPL_WARN_UNUSED_RESULT;
wchar_t CPL_DLL *CPLRecodeToWChar( const char *pszSource,
                                   const char *pszSrcEncoding, 
                                   const char *pszDstEncoding ) CPL_WARN_UNUSED_RESULT;
int CPL_DLL CPLIsUTF8(const char* pabyData, int nLen);
char CPL_DLL *CPLForceToASCII(const char* pabyData, int nLen, char chReplacementChar) CPL_WARN_UNUSED_RESULT;

CPL_C_END

/************************************************************************/
/*                              CPLString                               */
/************************************************************************/

#if defined(__cplusplus) && !defined(CPL_SUPRESS_CPLUSPLUS)

#include <string>

/*
 * Simple trick to avoid "using" declaration in header for new compilers
 * but make it still working with old compilers which throw C2614 errors.
 *
 * Define MSVC_OLD_STUPID_BEHAVIOUR
 * for old compilers: VC++ 5 and 6 as well as eVC++ 3 and 4.
 */

/*
 * Detect old MSVC++ compiler <= 6.0
 * 1200 - VC++ 6.0
 * 1200-1202 - eVC++ 4.0
 */
#if defined(_MSC_VER) 
# if (_MSC_VER <= 1202) 
#  define MSVC_OLD_STUPID_BEHAVIOUR 
# endif
#endif

/* Avoid C2614 errors */
#ifdef MSVC_OLD_STUPID_BEHAVIOUR
    using std::string;
# define gdal_std_string string
#else
# define gdal_std_string std::string
#endif 

/* Remove annoying warnings in Microsoft eVC++ and Microsoft Visual C++ */
#if defined(WIN32CE)
#  pragma warning(disable:4251 4275 4786)
#endif

//! Convenient string class based on std::string.
class CPL_DLL CPLString : public gdal_std_string
{
public:

    
    CPLString(void) {}
    CPLString( const std::string &oStr ) : gdal_std_string( oStr ) {}
    CPLString( const char *pszStr ) : gdal_std_string( pszStr ) {}
    
    operator const char* (void) const { return c_str(); }

    char& operator[](std::string::size_type i)
    {
        return gdal_std_string::operator[](i);
    }
    
    const char& operator[](std::string::size_type i) const
    {
        return gdal_std_string::operator[](i);
    }

    char& operator[](int i)
    {
        return gdal_std_string::operator[](static_cast<std::string::size_type>(i));
    }

    const char& operator[](int i) const
    {
        return gdal_std_string::operator[](static_cast<std::string::size_type>(i));
    }

    void Clear() { resize(0); }

    // NULL safe assign and free.
    void Seize(char *pszValue) 
    {
        if (pszValue == NULL )
            Clear();
        else
        {
            *this = pszValue;
            CPLFree(pszValue);
        }
    }

    /* There seems to be a bug in the way the compiler count indices... Should be CPL_PRINT_FUNC_FORMAT (1, 2) */
    CPLString &Printf( const char *pszFormat, ... ) CPL_PRINT_FUNC_FORMAT (2, 3);
    CPLString &vPrintf( const char *pszFormat, va_list args );
    CPLString &FormatC( double dfValue, const char *pszFormat = NULL );
    CPLString &Trim();
    CPLString &Recode( const char *pszSrcEncoding, const char *pszDstEncoding );

    /* case insensitive find alternates */
    size_t    ifind( const std::string & str, size_t pos = 0 ) const;
    size_t    ifind( const char * s, size_t pos = 0 ) const;
    CPLString &toupper( void );
    CPLString &tolower( void );
};

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
    char **papszList;
    mutable int nCount;
    mutable int nAllocation;
    int    bOwnList;
    int    bIsSorted;

    void   Initialize();
    void   MakeOurOwnCopy();
    void   EnsureAllocation( int nMaxLength );
    int    FindSortedInsertionPoint( const char *pszLine );
    
  public:
    CPLStringList();
    CPLStringList( char **papszList, int bTakeOwnership=TRUE );
    CPLStringList( const CPLStringList& oOther );
    ~CPLStringList();

    CPLStringList &Clear();

    int    size() const { return Count(); }
    int    Count() const;

    CPLStringList &AddString( const char *pszNewString );
    CPLStringList &AddStringDirectly( char *pszNewString );

    CPLStringList &InsertString( int nInsertAtLineNo, const char *pszNewLine )
    { return InsertStringDirectly( nInsertAtLineNo, CPLStrdup(pszNewLine) ); }
    CPLStringList &InsertStringDirectly( int nInsertAtLineNo, char *pszNewLine);
    
//    CPLStringList &InsertStrings( int nInsertAtLineNo, char **papszNewLines );
//    CPLStringList &RemoveStrings( int nFirstLineToDelete, int nNumToRemove=1 );
    
    int    FindString( const char *pszTarget ) const
    { return CSLFindString( papszList, pszTarget ); }
    int    PartialFindString( const char *pszNeedle ) const
    { return CSLPartialFindString( papszList, pszNeedle ); }

    int    FindName( const char *pszName ) const;
    int    FetchBoolean( const char *pszKey, int bDefault ) const;
    const char *FetchNameValue( const char *pszKey ) const;
    const char *FetchNameValueDef( const char *pszKey, const char *pszDefault ) const;
    CPLStringList &AddNameValue( const char *pszKey, const char *pszValue );
    CPLStringList &SetNameValue( const char *pszKey, const char *pszValue );

    CPLStringList &Assign( char **papszList, int bTakeOwnership=TRUE );
    CPLStringList &operator=(char **papszListIn) { return Assign( papszListIn, TRUE ); }
    CPLStringList &operator=(const CPLStringList& oOther);

    char * operator[](int i);
    char * operator[](size_t i) { return (*this)[(int)i]; }
    const char * operator[](int i) const;
    const char * operator[](size_t i) const { return (*this)[(int)i]; }

    char **List() { return papszList; }
    char **StealList();

    CPLStringList &Sort();
    int    IsSorted() const { return bIsSorted; }

    operator char**(void) { return List(); }
};

#endif /* def __cplusplus && !CPL_SUPRESS_CPLUSPLUS */

#endif /* _CPL_STRING_H_INCLUDED */
