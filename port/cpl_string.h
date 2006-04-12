/**********************************************************************
 * $Id$
 *
 * Name:     cpl_string.h
 * Project:  CPL - Common Portability Library
 * Purpose:  String and StringList functions.
 * Author:   Daniel Morissette, danmo@videotron.ca
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
 **********************************************************************
 *
 * $Log$
 * Revision 1.25  2006/04/12 15:10:40  fwarmerdam
 * argument to InsertString should be const
 *
 * Revision 1.24  2006/03/21 20:11:54  fwarmerdam
 * fixup headers a bit
 *
 * Revision 1.23  2006/02/19 21:54:34  mloskot
 * [WINCE] Changes related to Windows CE port of CPL. Most changes are #ifdef wrappers.
 *
 * Revision 1.22  2005/10/13 01:20:16  fwarmerdam
 * added CSLMerge()
 *
 * Revision 1.21  2005/09/14 19:21:17  fwarmerdam
 * binary pointer is const in binarytohex
 *
 * Revision 1.20  2005/08/31 05:08:01  fwarmerdam
 * fixed up std::string use for vc6 compatability
 *
 * Revision 1.19  2005/08/31 03:30:51  fwarmerdam
 * added binarytohex/hextobinary, CPLString
 *
 * Revision 1.18  2005/04/04 15:23:31  fwarmerdam
 * some functions now CPL_STDCALL
 *
 * Revision 1.17  2004/08/16 20:23:46  warmerda
 * added .csv escaping
 *
 * Revision 1.16  2004/07/12 21:50:38  warmerda
 * Added SQL escaping style
 *
 * Revision 1.15  2003/07/17 10:15:40  dron
 * CSLTestBoolean() added.
 *
 * Revision 1.14  2003/03/11 21:33:03  warmerda
 * added URL encode/decode support, untested
 *
 * Revision 1.13  2003/01/30 19:15:55  warmerda
 * added some docs
 **********************************************************************/

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
 */

CPL_C_START

char CPL_DLL **CSLAddString(char **papszStrList, const char *pszNewString);
int CPL_DLL CSLCount(char **papszStrList);
const char CPL_DLL *CSLGetField( char **, int );
void CPL_DLL CPL_STDCALL CSLDestroy(char **papszStrList);
char CPL_DLL **CSLDuplicate(char **papszStrList);
char CPL_DLL **CSLMerge( char **papszOrig, char **papszOverride );

char CPL_DLL **CSLTokenizeString(const char *pszString );
char CPL_DLL **CSLTokenizeStringComplex(const char *pszString,
                                   const char *pszDelimiter,
                                   int bHonourStrings, int bAllowEmptyTokens );
char CPL_DLL **CSLTokenizeString2( const char *pszString, 
                                   const char *pszDelimeter, 
                                   int nCSLTFlags );

#define CSLT_HONOURSTRINGS      0x0001
#define CSLT_ALLOWEMPTYTOKENS   0x0002
#define CSLT_PRESERVEQUOTES     0x0004
#define CSLT_PRESERVEESCAPES    0x0008

int CPL_DLL CSLPrint(char **papszStrList, FILE *fpOut);
char CPL_DLL **CSLLoad(const char *pszFname);
int CPL_DLL CSLSave(char **papszStrList, const char *pszFname);

char CPL_DLL **CSLInsertStrings(char **papszStrList, int nInsertAtLineNo, 
                         char **papszNewLines);
char CPL_DLL **CSLInsertString(char **papszStrList, int nInsertAtLineNo, 
                               const char *pszNewLine);
char CPL_DLL **CSLRemoveStrings(char **papszStrList, int nFirstLineToDelete,
                         int nNumToRemove, char ***ppapszRetStrings);
int CPL_DLL CSLFindString( char **, const char * );
int CPL_DLL CSLTestBoolean( const char *pszValue );
int CPL_DLL CSLFetchBoolean( char **papszStrList, const char *pszKey, 
                             int bDefault );

const char CPL_DLL *CPLSPrintf(char *fmt, ...);
char CPL_DLL **CSLAppendPrintf(char **papszStrList, char *fmt, ...);

const char CPL_DLL *
      CPLParseNameValue(const char *pszNameValue, char **ppszKey );
const char CPL_DLL *
      CSLFetchNameValue(char **papszStrList, const char *pszName);
char CPL_DLL **
      CSLFetchNameValueMultiple(char **papszStrList, const char *pszName);
char CPL_DLL **
      CSLAddNameValue(char **papszStrList, 
                      const char *pszName, const char *pszValue);
char CPL_DLL **
      CSLSetNameValue(char **papszStrList, 
                      const char *pszName, const char *pszValue);
void CPL_DLL CSLSetNameValueSeparator( char ** papszStrList, 
                                       const char *pszSeparator );

#define CPLES_BackslashQuotable 0
#define CPLES_XML               1
#define CPLES_URL               2   /* unescape only for now */
#define CPLES_SQL               3
#define CPLES_CSV               4

char CPL_DLL *CPLEscapeString( const char *pszString, int nLength, 
                               int nScheme );
char CPL_DLL *CPLUnescapeString( const char *pszString, int *pnLength,
                                 int nScheme );

char CPL_DLL *CPLBinaryToHex( int nBytes, const GByte *pabyData );
GByte CPL_DLL *CPLHexToBinary( const char *pszHex, int *pnBytes );

CPL_C_END

/************************************************************************/
/*                              CPLString                               */
/************************************************************************/

#ifdef __cplusplus

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
#if (_MSC_VER <= 1202)
#  define MSVC_OLD_STUPID_BEHAVIOUR
#endif
 

/* Avoid C2614 errors */
#ifdef MSVC_OLD_STUPID_BEHAVIOUR
    using std::string;
# define std_string string
#else
# define std_string std::string
#endif 

/* Remove annoying warnings in eVC++ and VC++ 6.0 */
#if defined(WIN32CE)
#  pragma warning(disable:4786)
#endif




class CPL_DLL CPLString : public std_string
{
public:
    CPLString(void) {}
    CPLString( const std::string &oStr ) : std_string( oStr ) {}
    CPLString( const char *pszStr ) : std_string( pszStr ) {}
    
    operator const char* (void) const { return c_str(); }

    CPLString &Printf( const char *pszFormat, ... );
    CPLString &vPrintf( const char *pszFormat, va_list args );
};

#endif /* def __cplusplus */

#endif /* _CPL_STRING_H_INCLUDED */
