/**********************************************************************
 * $Id$
 *
 * Project:  CPL - Common Portability Library
 * Purpose:  Portable filename/path parsing, and forming ala "Glob API".
 * Author:   Frank Warmerdam, warmerda@home.com
 *
 **********************************************************************
 * Copyright (c) 1999, Frank Warmerdam
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
 * Revision 1.5  2001/02/24 01:53:57  warmerda
 * Added CPLFormCIFilename()
 *
 * Revision 1.4  2001/01/19 21:18:25  warmerda
 * expanded tabs
 *
 * Revision 1.3  2000/01/26 17:53:36  warmerda
 * Fixed CPLGetExtension() for filenames with no extension.
 *
 * Revision 1.2  2000/01/24 19:32:59  warmerda
 * Fixed CPLGetExtension() to not include the dot.
 *
 * Revision 1.1  1999/10/14 19:23:39  warmerda
 * New
 *
 **********************************************************************/

#include "cpl_conv.h"
#include "cpl_string.h"


static char     szStaticResult[1024]; /* should be size of larged possible
                                         filename */

/************************************************************************/
/*                        CPLFindFilenameStart()                        */
/************************************************************************/

static int CPLFindFilenameStart( const char * pszFilename )

{
    int         iFileStart;

    for( iFileStart = strlen(pszFilename);
         iFileStart > 0
             && pszFilename[iFileStart-1] != '/'
             && pszFilename[iFileStart-1] != '\\';
         iFileStart-- ) {}

    return iFileStart;
}

/************************************************************************/
/*                             CPLGetPath()                             */
/************************************************************************/

/**
 * Extract directory path portion of filename.
 *
 * Returns a string containing the directory path portion of the passed
 * filename.  If there is no path in the passed filename an empty string
 * will be returned (not NULL).
 *
 * <pre>
 * CPLGetPath( "abc/def.xyz" ) == "abc"
 * CPLGetPath( "/abc/def/" ) == "abc/def"
 * CPLGetPath( "/" ) == "/"
 * CPLGetPath( "/abc/def" ) == "/abc"
 * </pre>
 *
 * @param pszFilename the filename potentially including a path.
 *
 *  @return Path in an internal string which must not be freed.  The string
 * may be destroyed by the next CPL filename handling call.  The returned
 * will generally not contain a trailing path separator.
 */

const char *CPLGetPath( const char *pszFilename )

{
    int         iFileStart = CPLFindFilenameStart(pszFilename);

    if( iFileStart == 0 )
    {
        strcpy( szStaticResult, "" );
        return szStaticResult;
    }

    strncpy( szStaticResult, pszFilename, iFileStart );
    szStaticResult[iFileStart] = '\0';

    if( iFileStart > 1
        && (szStaticResult[iFileStart-1] == '/'
            || szStaticResult[iFileStart-1] == '\\') )
        szStaticResult[iFileStart-1] = '\0';

    return szStaticResult;
}

/************************************************************************/
/*                           CPLGetFilename()                           */
/************************************************************************/

/**
 * Extract non-directory portion of filename.
 *
 * Returns a string containing the bare filename portion of the passed
 * filename.  If there is no filename (passed value ends in trailing directory
 * separator) an empty string is returned.
 *
 * <pre>
 * CPLGetFilename( "abc/def.xyz" ) == "def.xyz"
 * CPLGetFilename( "/abc/def/" ) == ""
 * CPLGetFilename( "abc/def" ) == "def"
 * </pre>
 *
 * @param pszFullFilename the full filename potentially including a path.
 *
 *  @return just the non-directory portion of the path in an internal string
 * which must not be freed.  The string
 * may be destroyed by the next CPL filename handling call.
 */

const char *CPLGetFilename( const char *pszFullFilename )

{
    int iFileStart = CPLFindFilenameStart( pszFullFilename );

    strcpy( szStaticResult, pszFullFilename + iFileStart );

    return szStaticResult;
}

/************************************************************************/
/*                           CPLGetBasename()                           */
/************************************************************************/

/**
 * Extract basename (non-directory, non-extension) portion of filename.
 *
 * Returns a string containing the file basename portion of the passed
 * name.  If there is no basename (passed value ends in trailing directory
 * separator, or filename starts with a dot) an empty string is returned.
 *
 * <pre>
 * CPLGetBasename( "abc/def.xyz" ) == "def"
 * CPLGetBasename( "abc/def" ) == "def"
 * CPLGetBasename( "abc/def/" ) == ""
 * </pre>
 *
 * @param pszFullFilename the full filename potentially including a path.
 *
 * @return just the non-directory, non-extension portion of the path in
 * an internal string which must not be freed.  The string
 * may be destroyed by the next CPL filename handling call.
 */

const char *CPLGetBasename( const char *pszFullFilename )

{
    int iFileStart = CPLFindFilenameStart( pszFullFilename );
    int iExtStart, nLength;

    for( iExtStart = strlen(pszFullFilename);
         iExtStart > iFileStart && pszFullFilename[iExtStart] != '.';
         iExtStart-- ) {}

    if( iExtStart == iFileStart )
        iExtStart = strlen(pszFullFilename);

    nLength = iExtStart - iFileStart;

    strncpy( szStaticResult, pszFullFilename + iFileStart, nLength );
    szStaticResult[nLength] = '\0';

    return szStaticResult;
}


/************************************************************************/
/*                           CPLGetExtension()                          */
/************************************************************************/

/**
 * Extract filename extension from full filename.
 *
 * Returns a string containing the extention portion of the passed
 * name.  If there is no extension (the filename has no dot) an empty string
 * is returned.  The returned extension will always include the period.
 *
 * <pre>
 * CPLGetExtension( "abc/def.xyz" ) == ".xyz"
 * CPLGetExtension( "abc/def" ) == ""
 * </pre>
 *
 * @param pszFullFilename the full filename potentially including a path.
 *
 * @return just the extension portion of the path in
 * an internal string which must not be freed.  The string
 * may be destroyed by the next CPL filename handling call.
 */

const char *CPLGetExtension( const char *pszFullFilename )

{
    int iFileStart = CPLFindFilenameStart( pszFullFilename );
    int iExtStart;

    for( iExtStart = strlen(pszFullFilename);
         iExtStart > iFileStart && pszFullFilename[iExtStart] != '.';
         iExtStart-- ) {}

    if( iExtStart == iFileStart )
        iExtStart = strlen(pszFullFilename)-1;

    strcpy( szStaticResult, pszFullFilename+iExtStart+1 );

    return szStaticResult;
}

/************************************************************************/
/*                          CPLFormFilename()                           */
/************************************************************************/

/**
 * Build a full file path from a passed path, file basename and extension.
 *
 * The path, and extension are optional.  The basename may in fact contain
 * an extension if desired.
 *
 * <pre>
 * CPLFormFilename("abc/xyz","def", ".dat" ) == "abc/xyz/def.dat"
 * CPLFormFilename(NULL,"def", NULL ) == "def"
 * CPLFormFilename(NULL,"abc/def.dat", NULL ) == "abc/def.dat"
 * CPLFormFilename("/abc/xyz/","def.dat", NULL ) == "/abc/xyz/def.dat"
 * </pre>
 *
 * @param pszPath directory path to the directory containing the file.  This
 * may be relative or absolute, and may have a trailing path separator or
 * not.  May be NULL.
 *
 * @param pszBasename file basename.  May optionally have path and/or
 * extension.  May not be NULL. 
 *
 * @param pszExtension file extension, optionally including the period.  May
 * be NULL.
 *
 * @return a fully formed filename in an internal static string.  Do not
 * modify or free the returned string.  The string may be destroyed by the
 * next CPL call.
 */

const char *CPLFormFilename( const char * pszPath,
                             const char * pszBasename,
                             const char * pszExtension )

{
    const char  *pszAddedPathSep = "";
    const char  *pszAddedExtSep = "";

    if( pszPath == NULL )
        pszPath = "";
    else if( strlen(pszPath) > 0
             && pszPath[strlen(pszPath)-1] != '/'
             && pszPath[strlen(pszPath)-1] != '\\' )
#ifdef WIN32        
        pszAddedPathSep = "\\";
#else    
        pszAddedPathSep = "/";
#endif        

    if( pszExtension == NULL )
        pszExtension = "";
    else if( pszExtension[0] != '.' && strlen(pszExtension) > 0 )
        pszAddedExtSep = ".";

    sprintf( szStaticResult, "%s%s%s%s%s",
             pszPath, pszAddedPathSep,
             pszBasename,
             pszAddedExtSep, pszExtension );

    return szStaticResult;
}

/************************************************************************/
/*                          CPLFormCIFilename()                         */
/************************************************************************/

/**
 * Case insensitive file searching, returing full path.
 *
 * This function tries to return the path to a file regardless of
 * whether the file exactly matches the basename, and extension case, or
 * is all upper case, or all lower case.  The path is treated as case 
 * sensitive.  This function is equivelent to CPLFormFilename() on 
 * case insensitive file systems (like Windows).
 *
 * @param pszPath directory path to the directory containing the file.  This
 * may be relative or absolute, and may have a trailing path separator or
 * not.  May be NULL.
 *
 * @param pszBasename file basename.  May optionally have path and/or
 * extension.  May not be NULL. 
 *
 * @param pszExtension file extension, optionally including the period.  May
 * be NULL.
 *
 * @return a fully formed filename in an internal static string.  Do not
 * modify or free the returned string.  The string may be destroyed by the
 * next CPL call.
 */

const char *CPLFormCIFilename( const char * pszPath,
                               const char * pszBasename,
                               const char * pszExtension )

{
#ifdef WIN32
    return CPLFormFilename( pszPath, pszBasename, pszExtension );
#else
    const char  *pszAddedExtSep = "";
    char	*pszFilename;
    const char  *pszFullPath;
    int		nLen = strlen(pszBasename)+2, i;
    FILE	*fp;

    if( pszExtension != NULL )
        nLen += strlen(pszExtension);

    pszFilename = (char *) CPLMalloc(nLen);

    if( pszExtension == NULL )
        pszExtension = "";
    else if( pszExtension[0] != '.' && strlen(pszExtension) > 0 )
        pszAddedExtSep = ".";

    sprintf( pszFilename, "%s%s%s", 
             pszBasename, pszAddedExtSep, pszExtension );

    pszFullPath = CPLFormFilename( pszPath, pszFilename, NULL );
    fp = VSIFOpen( pszFullPath, "r" );
    if( fp == NULL )
    {
        for( i = 0; pszFilename[i] != '\0'; i++ )
        {
            if( pszFilename[i] >= 'a' && pszFilename[i] <= 'z' )
                pszFilename[i] = pszFilename[i] + 'A' - 'a';
        }

        pszFullPath = CPLFormFilename( pszPath, pszFilename, NULL );
        fp = VSIFOpen( pszFullPath, "r" );
    }

    if( fp == NULL )
    {
        for( i = 0; pszFilename[i] != '\0'; i++ )
        {
            if( pszFilename[i] >= 'A' && pszFilename[i] <= 'Z' )
                pszFilename[i] = pszFilename[i] + 'a' - 'A';
        }

        pszFullPath = CPLFormFilename( pszPath, pszFilename, NULL );
        fp = VSIFOpen( pszFullPath, "r" );
    }

    if( fp != NULL )
        VSIFClose( fp );
    else
        pszFullPath = CPLFormFilename( pszPath, pszBasename, pszExtension );

    CPLFree( pszFilename );

    return pszFullPath;
#endif
}
