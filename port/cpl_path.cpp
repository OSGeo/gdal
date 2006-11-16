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
 * Revision 1.25  2006/11/16 15:23:33  mloskot
 * Fixed CPLGetCurrentDir which on Windows CE returns NULL.
 *
 * Revision 1.24  2006/11/13 18:45:05  fwarmerdam
 * added CPLCleanTrailingSlash() per bug 1311
 *
 * Revision 1.23  2006/09/22 12:21:12  dron
 * CPLIsFilenameRelative(): on Windows forward slashes can be used in file paths.
 *
 * Revision 1.22  2006/09/07 18:11:10  dron
 * Added CPLGetCurrentDir().
 *
 * Revision 1.21  2006/06/30 18:18:17  dron
 * Avoid warnings.
 *
 * Revision 1.20  2006/06/30 15:54:30  dron
 * Avoid warnings on win/64.
 *
 * Revision 1.19  2005/07/11 13:52:03  fwarmerdam
 * use new TLS support for static buffer
 *
 * Revision 1.18  2005/05/23 03:59:25  fwarmerdam
 * make working buffer threadlocal
 *
 * Revision 1.17  2004/08/13 15:59:39  warmerda
 * Fixed bug with CPLExtractRelativePath.
 *
 * Revision 1.16  2004/08/11 18:41:46  warmerda
 * added CPLExtractRelativePath
 *
 * Revision 1.15  2004/07/10 12:22:37  dron
 * Use locale aware character classification functions in CPLFormCIFilename().
 *
 * Revision 1.14  2003/05/28 19:22:38  warmerda
 * fixed docs
 *
 * Revision 1.13  2003/04/04 09:11:35  dron
 * strcpy() and strcat() replaced by strncpy() and strncat().
 * A lot of assertion on string sizes added.
 *
 * Revision 1.12  2002/12/13 06:14:17  warmerda
 * fixed bug with IsRelative function
 *
 * Revision 1.11  2002/12/13 06:00:54  warmerda
 * added CPLProjectRelativeFilename() and CPLIsFilenameRelative()
 *
 * Revision 1.10  2002/08/15 09:23:24  dron
 * Added CPLGetDirname() function
 *
 * Revision 1.9  2001/08/30 21:20:49  warmerda
 * expand tabs
 *
 * Revision 1.8  2001/07/18 04:00:49  warmerda
 * added CPL_CVSID
 *
 * Revision 1.7  2001/05/12 19:20:55  warmerda
 * Fixed documentation of CPLGetExtension().
 *
 * Revision 1.6  2001/03/16 22:15:08  warmerda
 * added CPLResetExtension
 *
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
#include "cpl_multiproc.h"

CPL_CVSID("$Id$");


/* should be size of larged possible filename */
#define CPL_PATH_BUF_SIZE 2048

#ifdef WIN32        
#define SEP_CHAR '\\'
#define SEP_STRING "\\"
#else
#define SEP_CHAR '/'
#define SEP_STRING "/"
#endif

/************************************************************************/
/*                         CPLGetStaticResult()                         */
/************************************************************************/

static char *CPLGetStaticResult()

{
    char *pszStaticResult = (char *) CPLGetTLS( CTLS_PATHBUF );
    if( pszStaticResult == NULL )
    {
        pszStaticResult = (char *) CPLMalloc(CPL_PATH_BUF_SIZE);
        CPLSetTLS( CTLS_PATHBUF, pszStaticResult, TRUE );
    }

    return pszStaticResult;
}


/************************************************************************/
/*                        CPLFindFilenameStart()                        */
/************************************************************************/

static int CPLFindFilenameStart( const char * pszFilename )

{
    size_t  iFileStart;

    for( iFileStart = strlen(pszFilename);
         iFileStart > 0
             && pszFilename[iFileStart-1] != '/'
             && pszFilename[iFileStart-1] != '\\';
         iFileStart-- ) {}

    return (int)iFileStart;
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
 * CPLGetPath( "/abc/def/" ) == "/abc/def"
 * CPLGetPath( "/" ) == "/"
 * CPLGetPath( "/abc/def" ) == "/abc"
 * CPLGetPath( "abc" ) == ""
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
    char       *pszStaticResult = CPLGetStaticResult();

    CPLAssert( iFileStart < CPL_PATH_BUF_SIZE );

    if( iFileStart == 0 )
    {
        strcpy( pszStaticResult, "" );
        return pszStaticResult;
    }

    strncpy( pszStaticResult, pszFilename, iFileStart );
    pszStaticResult[iFileStart] = '\0';

    if( iFileStart > 1
        && (pszStaticResult[iFileStart-1] == '/'
            || pszStaticResult[iFileStart-1] == '\\') )
        pszStaticResult[iFileStart-1] = '\0';

    return pszStaticResult;
}

/************************************************************************/
/*                             CPLGetDirname()                          */
/************************************************************************/

/**
 * Extract directory path portion of filename.
 *
 * Returns a string containing the directory path portion of the passed
 * filename.  If there is no path in the passed filename the dot will be
 * returned.  It is the only difference from CPLGetPath().
 *
 * <pre>
 * CPLGetDirname( "abc/def.xyz" ) == "abc"
 * CPLGetDirname( "/abc/def/" ) == "/abc/def"
 * CPLGetDirname( "/" ) == "/"
 * CPLGetDirname( "/abc/def" ) == "/abc"
 * CPLGetDirname( "abc" ) == "."
 * </pre>
 *
 * @param pszFilename the filename potentially including a path.
 *
 * @return Path in an internal string which must not be freed.  The string
 * may be destroyed by the next CPL filename handling call.  The returned
 * will generally not contain a trailing path separator.
 */

const char *CPLGetDirname( const char *pszFilename )

{
    int         iFileStart = CPLFindFilenameStart(pszFilename);
    char       *pszStaticResult = CPLGetStaticResult();

    CPLAssert( iFileStart < CPL_PATH_BUF_SIZE );

    if( iFileStart == 0 )
    {
        strcpy( pszStaticResult, "." );
        return pszStaticResult;
    }

    strncpy( pszStaticResult, pszFilename, iFileStart );
    pszStaticResult[iFileStart] = '\0';

    if( iFileStart > 1
        && (pszStaticResult[iFileStart-1] == '/'
            || pszStaticResult[iFileStart-1] == '\\') )
        pszStaticResult[iFileStart-1] = '\0';

    return pszStaticResult;
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
    char       *pszStaticResult = CPLGetStaticResult();

    strncpy( pszStaticResult, pszFullFilename + iFileStart, 
             CPL_PATH_BUF_SIZE );
    pszStaticResult[CPL_PATH_BUF_SIZE - 1] = '\0';

    return pszStaticResult;
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
    size_t  iFileStart = CPLFindFilenameStart( pszFullFilename );
    size_t  iExtStart, nLength;
    char    *pszStaticResult = CPLGetStaticResult();

    for( iExtStart = strlen(pszFullFilename);
         iExtStart > iFileStart && pszFullFilename[iExtStart] != '.';
         iExtStart-- ) {}

    if( iExtStart == iFileStart )
        iExtStart = strlen(pszFullFilename);

    nLength = iExtStart - iFileStart;

    CPLAssert( nLength < CPL_PATH_BUF_SIZE );

    strncpy( pszStaticResult, pszFullFilename + iFileStart, nLength );
    pszStaticResult[nLength] = '\0';

    return pszStaticResult;
}


/************************************************************************/
/*                           CPLGetExtension()                          */
/************************************************************************/

/**
 * Extract filename extension from full filename.
 *
 * Returns a string containing the extention portion of the passed
 * name.  If there is no extension (the filename has no dot) an empty string
 * is returned.  The returned extension will not include the period.
 *
 * <pre>
 * CPLGetExtension( "abc/def.xyz" ) == "xyz"
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
    size_t  iFileStart = CPLFindFilenameStart( pszFullFilename );
    size_t  iExtStart;
    char    *pszStaticResult = CPLGetStaticResult();

    for( iExtStart = strlen(pszFullFilename);
         iExtStart > iFileStart && pszFullFilename[iExtStart] != '.';
         iExtStart-- ) {}

    if( iExtStart == iFileStart )
        iExtStart = strlen(pszFullFilename)-1;

    strncpy( pszStaticResult, pszFullFilename+iExtStart+1, CPL_PATH_BUF_SIZE );
    pszStaticResult[CPL_PATH_BUF_SIZE - 1] = '\0';

    return pszStaticResult;
}

/************************************************************************/
/*                         CPLGetCurrentDir()                           */
/************************************************************************/

/**
 * Get the current working directory name.
 *
 * @return a pointer to buffer, containing current working directory path
 * or NULL in case of error.  User is responsible to free that buffer
 * after usage with CPLFree() function.
 * If HAVE_GETCWD macro is not defined, the function returns NULL.
 **/

char *CPLGetCurrentDir()

{
    size_t  nPathMax;
    char    *pszDirPath;

# ifdef _MAX_PATH
    nPathMax = _MAX_PATH;
# elif PATH_MAX
    nPathMax = PATH_MAX;
# else
    nPathMax = 8192;
# endif

    pszDirPath = (char*)CPLMalloc( nPathMax );
    if ( !pszDirPath )
        return NULL;

#ifdef HAVE_GETCWD
    return getcwd( pszDirPath, nPathMax );
#else
    return NULL;
#endif /* HAVE_GETCWD */
}

/************************************************************************/
/*                         CPLResetExtension()                          */
/************************************************************************/

/**
 * Replace the extension with the provided one.
 *
 * @param pszPath the input path, this string is not altered.
 * @param pszExt the new extension to apply to the given path.
 *
 * @return an altered filename with the new extension.    Do not
 * modify or free the returned string.  The string may be destroyed by the
 * next CPL call.
 */

const char *CPLResetExtension( const char *pszPath, const char *pszExt )

{
    char    *pszStaticResult = CPLGetStaticResult();
    size_t  i;

/* -------------------------------------------------------------------- */
/*      First, try and strip off any existing extension.                */
/* -------------------------------------------------------------------- */
    strncpy( pszStaticResult, pszPath, CPL_PATH_BUF_SIZE );
    pszStaticResult[CPL_PATH_BUF_SIZE - 1] = '\0';
    for( i = strlen(pszStaticResult) - 1; i > 0; i-- )
    {
        if( pszStaticResult[i] == '.' )
        {
            pszStaticResult[i] = '\0';
            break;
        }

        if( pszStaticResult[i] == '/' || pszStaticResult[i] == '\\' 
            || pszStaticResult[i] == ':' )
            break;
    }

/* -------------------------------------------------------------------- */
/*      Append the new extension.                                       */
/* -------------------------------------------------------------------- */
    CPLAssert( strlen(pszExt) + 2 < CPL_PATH_BUF_SIZE );
    
    strcat( pszStaticResult, "." );
    strcat( pszStaticResult, pszExt );

    return pszStaticResult;
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
    char *pszStaticResult = CPLGetStaticResult();
    const char  *pszAddedPathSep = "";
    const char  *pszAddedExtSep = "";

    if( pszPath == NULL )
        pszPath = "";
    else if( strlen(pszPath) > 0
             && pszPath[strlen(pszPath)-1] != '/'
             && pszPath[strlen(pszPath)-1] != '\\' )
        pszAddedPathSep = SEP_STRING;

    if( pszExtension == NULL )
        pszExtension = "";
    else if( pszExtension[0] != '.' && strlen(pszExtension) > 0 )
        pszAddedExtSep = ".";

    CPLAssert( strlen(pszPath) + strlen(pszAddedPathSep) +
               strlen(pszBasename) + strlen(pszAddedExtSep) +
               strlen(pszExtension) + 1 < CPL_PATH_BUF_SIZE );

    strncpy( pszStaticResult, pszPath, CPL_PATH_BUF_SIZE );
    strncat( pszStaticResult, pszAddedPathSep, CPL_PATH_BUF_SIZE);
    strncat( pszStaticResult, pszBasename, CPL_PATH_BUF_SIZE);
    strncat( pszStaticResult, pszAddedExtSep, CPL_PATH_BUF_SIZE);
    strncat( pszStaticResult, pszExtension, CPL_PATH_BUF_SIZE);
    pszStaticResult[CPL_PATH_BUF_SIZE - 1] = '\0';

    return pszStaticResult;
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
    char        *pszFilename;
    const char  *pszFullPath;
    int         nLen = strlen(pszBasename)+2, i;
    FILE        *fp;

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
            if( islower(pszFilename[i]) )
                pszFilename[i] = toupper(pszFilename[i]);
        }

        pszFullPath = CPLFormFilename( pszPath, pszFilename, NULL );
        fp = VSIFOpen( pszFullPath, "r" );
    }

    if( fp == NULL )
    {
        for( i = 0; pszFilename[i] != '\0'; i++ )
        {
            if( isupper(pszFilename[i]) )
                pszFilename[i] = tolower(pszFilename[i]);
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

/************************************************************************/
/*                     CPLProjectRelativeFilename()                     */
/************************************************************************/

/**
 * Find a file relative to a project file. 
 *
 * Given the path to a "project" directory, and a path to a secondary file
 * referenced from that project, build a path to the secondary file
 * that the current application can use.  If the secondary path is already
 * absolute, rather than relative, then it will be returned unaltered. 
 * 
 * Examples:
 * <pre>
 * CPLProjectRelativeFilename("abc/def","tmp/abc.gif") == "abc/def/tmp/abc.gif"
 * CPLProjectRelativeFilename("abc/def","/tmp/abc.gif") == "/tmp/abc.gif"
 * CPLProjectRelativeFilename("/xy", "abc.gif") == "/xy/abc.gif"
 * CPLProjectRelativeFilename("/abc/def","../abc.gif") == "/abc/def/../abc.gif"
 * CPLProjectRelativeFilename("C:\WIN","abc.gif") == "C:\WIN\abc.gif"
 * </pre>
 *
 * @param pszProjectDir the directory relative to which the secondary files 
 * path should be interpreted.
 * @param pszSecondaryFilename the filename (potentially with path) that
 * is to be interpreted relative to the project directory.
 *
 * @return a composed path to the secondary file.  The returned string is
 * internal and should not be altered, freed, or depending on past the next
 * CPL call. 
 */

const char *CPLProjectRelativeFilename( const char *pszProjectDir, 
                                        const char *pszSecondaryFilename )

{
    char *pszStaticResult = CPLGetStaticResult();

    if( !CPLIsFilenameRelative( pszSecondaryFilename ) )
        return pszSecondaryFilename;

    if( pszProjectDir == NULL || strlen(pszProjectDir) == 0 )
        return pszSecondaryFilename;

    strncpy( pszStaticResult, pszProjectDir, CPL_PATH_BUF_SIZE );
    pszStaticResult[CPL_PATH_BUF_SIZE - 1] = '\0';

    if( pszProjectDir[strlen(pszProjectDir)-1] != '/' 
        && pszProjectDir[strlen(pszProjectDir)-1] != '\\' )
    {
        CPLAssert( strlen(SEP_STRING) + 1 < CPL_PATH_BUF_SIZE );

        strcat( pszStaticResult, SEP_STRING );
    }

    CPLAssert( strlen(pszSecondaryFilename) + 1 < CPL_PATH_BUF_SIZE );

    strcat( pszStaticResult, pszSecondaryFilename );
        
    return pszStaticResult;
}

/************************************************************************/
/*                       CPLIsFilenameRelative()                        */
/************************************************************************/

/**
 * Is filename relative or absolute?
 *
 * The test is filesystem convention agnostic.  That is it will test for
 * Unix style and windows style path conventions regardless of the actual
 * system in use.  
 *
 * @param pszFilename the filename with path to test.
 *
 * @return TRUE if the filename is relative or FALSE if it is absolute. 
 */

int CPLIsFilenameRelative( const char *pszFilename )

{
    if( (strlen(pszFilename) > 2
         && (strncmp(pszFilename+1,":\\",2) == 0
             || strncmp(pszFilename+1,":/",2) == 0))
        || pszFilename[0] == '\\'
        || pszFilename[0] == '/' )
        return FALSE;
    else
        return TRUE;
}

/************************************************************************/
/*                       CPLExtractRelativePath()                       */
/************************************************************************/

/**
 * Get relative path from directory to target file.
 *
 * Computes a relative path for pszTarget relative to pszBaseDir. 
 * Currently this only works if they share a common base path.  The returned
 * path is normally into the pszTarget string.  It should only be considered
 * valid as long as pszTarget is valid or till the next call to 
 * this function, whichever comes first. 
 *
 * @param pszBaseDir the name of the directory relative to which the path 
 * should be computed.  pszBaseDir may be NULL in which case the original
 * target is returned without relitivizing.
 * 
 * @param pszTarget the filename to be changed to be relative to pszBaseDir.
 *
 * @param pbGotRelative Pointer to location in which a flag is placed 
 * indicating that the returned path is relative to the basename (TRUE) or
 * not (FALSE).  This pointer may be NULL if flag is not desired.
 *
 * @return an adjusted path or the original if it could not be made relative
 * to the pszBaseFile's path. 
 **/

const char *CPLExtractRelativePath( const char *pszBaseDir, 
                                    const char *pszTarget,
                                    int *pbGotRelative )

{
    size_t nBasePathLen;

/* -------------------------------------------------------------------- */
/*      If we don't have a basedir, then we can't relativize the path.  */
/* -------------------------------------------------------------------- */
    if( pszBaseDir == NULL )
    {
        if( pbGotRelative != NULL )
            *pbGotRelative = FALSE;

        return pszTarget;
    }

    nBasePathLen = strlen(pszBaseDir);

/* -------------------------------------------------------------------- */
/*      One simple case is where neither file has a path.  We return    */
/*      the original target filename and it is relative.                */
/* -------------------------------------------------------------------- */
    const char *pszTargetPath = CPLGetPath(pszTarget);

    if( (nBasePathLen == 0 || EQUAL(pszBaseDir,"."))
        && (strlen(pszTargetPath) == 0 || EQUAL(pszTargetPath,".")) )
    {
        if( pbGotRelative != NULL )
            *pbGotRelative = TRUE;

        return pszTarget;
    }

/* -------------------------------------------------------------------- */
/*      By this point, if we don't have a base path, we can't have a    */
/*      meaningful common prefix.                                       */
/* -------------------------------------------------------------------- */
    if( nBasePathLen == 0 )
    {
        if( pbGotRelative != NULL )
            *pbGotRelative = FALSE;

        return pszTarget;
    }

/* -------------------------------------------------------------------- */
/*      If we don't have a common path prefix, then we can't get a      */
/*      relative path.                                                  */
/* -------------------------------------------------------------------- */
    if( !EQUALN(pszBaseDir,pszTarget,nBasePathLen) 
        || (pszTarget[nBasePathLen] != '\\' 
            && pszTarget[nBasePathLen] != '/') )
    {
        if( pbGotRelative != NULL )
            *pbGotRelative = FALSE;

        return pszTarget;
    }

/* -------------------------------------------------------------------- */
/*      We have a relative path.  Strip it off to get a string to       */
/*      return.                                                         */
/* -------------------------------------------------------------------- */
    if( pbGotRelative != NULL )
        *pbGotRelative = TRUE;

    return pszTarget + nBasePathLen + 1;
}

/************************************************************************/
/*                            CPLCleanTrailingSlash()                   */
/************************************************************************/

/**
 * Remove trailing forward/backward slash from the path for unix/windows resp.
 *
 * Returns a string containing the portion of the passed path string with 
 * trailing slash removed. If there is no path in the passed filename 
 * an empty string will be returned (not NULL).
 *
 * <pre>
 * CPLCleanTrailingSlash( "abc/def/" ) == "abc/def"
 * CPLCleanTrailingSlash( "abc/def" ) == "abc/def"
 * CPLCleanTrailingSlash( "c:\abc\def\" ) == "c:\abc\def"
 * CPLCleanTrailingSlash( "c:\abc\def" ) == "c:\abc\def"
 * CPLCleanTrailingSlash( "abc" ) == "abc"
 * </pre>
 *
 * @param pszPath the path to be cleaned up
 *
 *  @return Path in an internal string which must not be freed.  The string
 * may be destroyed by the next CPL filename handling call.  The returned
 * will generally not contain a trailing path separator.
 */

const char *CPLCleanTrailingSlash( const char *pszFilename )

{
    char       *pszStaticResult = CPLGetStaticResult();
    int        iPathLength = strlen(pszFilename);

    strncpy( pszStaticResult, pszFilename, iPathLength );
    pszStaticResult[iPathLength] = '\0';

    if( iPathLength > 0 
        && (pszStaticResult[iPathLength-1] == '\\' 
            || pszStaticResult[iPathLength-1] == '/'))
        pszStaticResult[iPathLength-1] = '\0';

    return pszStaticResult;
}
