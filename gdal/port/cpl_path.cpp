/**********************************************************************
 * $Id$
 *
 * Project:  CPL - Common Portability Library
 * Purpose:  Portable filename/path parsing, and forming ala "Glob API".
 * Author:   Frank Warmerdam, warmerdam@pobox.com
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
 ****************************************************************************/

#include "cpl_conv.h"
#include "cpl_string.h"
#include "cpl_multiproc.h"

CPL_CVSID("$Id$");


/* should be size of larged possible filename */
#define CPL_PATH_BUF_SIZE 2048
#define CPL_PATH_BUF_COUNT  10

#if defined(WIN32) || defined(WIN32CE)
#define SEP_CHAR '\\'
#define SEP_STRING "\\"
#else
#define SEP_CHAR '/'
#define SEP_STRING "/"
#endif

static const char* CPLStaticBufferTooSmall(char *pszStaticResult)
{
    CPLError(CE_Failure, CPLE_AppDefined, "Destination buffer too small");
    strcpy( pszStaticResult, "" );
    return pszStaticResult;
}

/************************************************************************/
/*                         CPLGetStaticResult()                         */
/************************************************************************/

static char *CPLGetStaticResult()

{
    char *pachBufRingInfo = (char *) CPLGetTLS( CTLS_PATHBUF );
    if( pachBufRingInfo == NULL )
    {
        pachBufRingInfo = (char *) CPLCalloc(1, sizeof(int) + CPL_PATH_BUF_SIZE * CPL_PATH_BUF_COUNT);
        CPLSetTLS( CTLS_PATHBUF, pachBufRingInfo, TRUE );
    }

/* -------------------------------------------------------------------- */
/*      Work out which string in the "ring" we want to use this         */
/*      time.                                                           */
/* -------------------------------------------------------------------- */
    int *pnBufIndex = (int *) pachBufRingInfo;
    int nOffset = sizeof(int) + *pnBufIndex * CPL_PATH_BUF_SIZE;
    char *pachBuffer = pachBufRingInfo + nOffset;

    *pnBufIndex = (*pnBufIndex + 1) % CPL_PATH_BUF_COUNT;

    return pachBuffer;
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

    if( iFileStart >= CPL_PATH_BUF_SIZE )
        return CPLStaticBufferTooSmall(pszStaticResult);

    CPLAssert( ! (pszFilename >= pszStaticResult && pszFilename < pszStaticResult + CPL_PATH_BUF_SIZE) );

    if( iFileStart == 0 )
    {
        strcpy( pszStaticResult, "" );
        return pszStaticResult;
    }

    CPLStrlcpy( pszStaticResult, pszFilename, iFileStart+1 );

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

    if( iFileStart >= CPL_PATH_BUF_SIZE )
        return CPLStaticBufferTooSmall(pszStaticResult);

    CPLAssert( ! (pszFilename >= pszStaticResult && pszFilename < pszStaticResult + CPL_PATH_BUF_SIZE) );

    if( iFileStart == 0 )
    {
        strcpy( pszStaticResult, "." );
        return pszStaticResult;
    }

    CPLStrlcpy( pszStaticResult, pszFilename, iFileStart+1 );

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
 *  @return just the non-directory portion of the path (points back into original string).
 */

const char *CPLGetFilename( const char *pszFullFilename )

{
    int iFileStart = CPLFindFilenameStart( pszFullFilename );

    return pszFullFilename + iFileStart;
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

    CPLAssert( ! (pszFullFilename >= pszStaticResult && pszFullFilename < pszStaticResult + CPL_PATH_BUF_SIZE) );

    for( iExtStart = strlen(pszFullFilename);
         iExtStart > iFileStart && pszFullFilename[iExtStart] != '.';
         iExtStart-- ) {}

    if( iExtStart == iFileStart )
        iExtStart = strlen(pszFullFilename);

    nLength = iExtStart - iFileStart;

    if (nLength >= CPL_PATH_BUF_SIZE)
        return CPLStaticBufferTooSmall(pszStaticResult);

    CPLStrlcpy( pszStaticResult, pszFullFilename + iFileStart, nLength + 1 );

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

    CPLAssert( ! (pszFullFilename >= pszStaticResult && pszFullFilename < pszStaticResult + CPL_PATH_BUF_SIZE) );

    for( iExtStart = strlen(pszFullFilename);
         iExtStart > iFileStart && pszFullFilename[iExtStart] != '.';
         iExtStart-- ) {}

    if( iExtStart == iFileStart )
        iExtStart = strlen(pszFullFilename)-1;

    if (CPLStrlcpy( pszStaticResult, pszFullFilename+iExtStart+1, CPL_PATH_BUF_SIZE ) >= CPL_PATH_BUF_SIZE)
        return CPLStaticBufferTooSmall(pszStaticResult);

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

    CPLAssert( ! (pszPath >= pszStaticResult && pszPath < pszStaticResult + CPL_PATH_BUF_SIZE) );

/* -------------------------------------------------------------------- */
/*      First, try and strip off any existing extension.                */
/* -------------------------------------------------------------------- */
    if (CPLStrlcpy( pszStaticResult, pszPath, CPL_PATH_BUF_SIZE ) >= CPL_PATH_BUF_SIZE)
        return CPLStaticBufferTooSmall(pszStaticResult);

    if (*pszStaticResult)
    {
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
    }

/* -------------------------------------------------------------------- */
/*      Append the new extension.                                       */
/* -------------------------------------------------------------------- */
    if (CPLStrlcat( pszStaticResult, ".", CPL_PATH_BUF_SIZE) >= CPL_PATH_BUF_SIZE ||
        CPLStrlcat( pszStaticResult, pszExt, CPL_PATH_BUF_SIZE) >= CPL_PATH_BUF_SIZE)
        return CPLStaticBufferTooSmall(pszStaticResult);

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

    CPLAssert( ! (pszPath >= pszStaticResult && pszPath < pszStaticResult + CPL_PATH_BUF_SIZE) );
    CPLAssert( ! (pszBasename >= pszStaticResult && pszBasename < pszStaticResult + CPL_PATH_BUF_SIZE) );

    if( pszPath == NULL )
        pszPath = "";
    else if( strlen(pszPath) > 0
             && pszPath[strlen(pszPath)-1] != '/'
             && pszPath[strlen(pszPath)-1] != '\\' )
    {
        /* FIXME? would be better to ask the filesystems what they */
        /* prefer as directory separator */
        if (strncmp(pszPath, "/vsicurl/", 9) == 0)
            pszAddedPathSep = "/";
        else
            pszAddedPathSep = SEP_STRING;
    }

    if( pszExtension == NULL )
        pszExtension = "";
    else if( pszExtension[0] != '.' && strlen(pszExtension) > 0 )
        pszAddedExtSep = ".";

    if (CPLStrlcpy( pszStaticResult, pszPath, CPL_PATH_BUF_SIZE) >= CPL_PATH_BUF_SIZE ||
        CPLStrlcat( pszStaticResult, pszAddedPathSep, CPL_PATH_BUF_SIZE) >= CPL_PATH_BUF_SIZE ||
        CPLStrlcat( pszStaticResult, pszBasename, CPL_PATH_BUF_SIZE) >= CPL_PATH_BUF_SIZE ||
        CPLStrlcat( pszStaticResult, pszAddedExtSep, CPL_PATH_BUF_SIZE) >= CPL_PATH_BUF_SIZE ||
        CPLStrlcat( pszStaticResult, pszExtension, CPL_PATH_BUF_SIZE) >= CPL_PATH_BUF_SIZE)
        return CPLStaticBufferTooSmall(pszStaticResult);

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
    // On case insensitive filesystems, just default to
    // CPLFormFilename()
    if( !VSIIsCaseSensitiveFS(pszPath) )
        return CPLFormFilename( pszPath, pszBasename, pszExtension );

    const char  *pszAddedExtSep = "";
    char        *pszFilename;
    const char  *pszFullPath;
    int         nLen = strlen(pszBasename)+2, i;
    VSIStatBufL sStatBuf;
    int         nStatRet;

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
    nStatRet = VSIStatExL( pszFullPath, &sStatBuf, VSI_STAT_EXISTS_FLAG );
    if( nStatRet != 0 )
    {
        for( i = 0; pszFilename[i] != '\0'; i++ )
        {
            if( islower(pszFilename[i]) )
                pszFilename[i] = (char) toupper(pszFilename[i]);
        }

        pszFullPath = CPLFormFilename( pszPath, pszFilename, NULL );
        nStatRet = VSIStatExL( pszFullPath, &sStatBuf, VSI_STAT_EXISTS_FLAG );
    }

    if( nStatRet != 0 )
    {
        for( i = 0; pszFilename[i] != '\0'; i++ )
        {
            if( isupper(pszFilename[i]) )
                pszFilename[i] = (char) tolower(pszFilename[i]);
        }

        pszFullPath = CPLFormFilename( pszPath, pszFilename, NULL );
        nStatRet = VSIStatExL( pszFullPath, &sStatBuf, VSI_STAT_EXISTS_FLAG );
    }

    if( nStatRet != 0 )
        pszFullPath = CPLFormFilename( pszPath, pszBasename, pszExtension );

    CPLFree( pszFilename );

    return pszFullPath;
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

    CPLAssert( ! (pszProjectDir >= pszStaticResult && pszProjectDir < pszStaticResult + CPL_PATH_BUF_SIZE) );
    CPLAssert( ! (pszSecondaryFilename >= pszStaticResult && pszSecondaryFilename < pszStaticResult + CPL_PATH_BUF_SIZE) );

    if( !CPLIsFilenameRelative( pszSecondaryFilename ) )
        return pszSecondaryFilename;

    if( pszProjectDir == NULL || strlen(pszProjectDir) == 0 )
        return pszSecondaryFilename;

    if (CPLStrlcpy( pszStaticResult, pszProjectDir, CPL_PATH_BUF_SIZE ) >= CPL_PATH_BUF_SIZE)
        goto error;

    if( pszProjectDir[strlen(pszProjectDir)-1] != '/' 
        && pszProjectDir[strlen(pszProjectDir)-1] != '\\' )
    {
        /* FIXME? would be better to ask the filesystems what they */
        /* prefer as directory separator */
        const char* pszAddedPathSep;
        if (strncmp(pszStaticResult, "/vsicurl/", 9) == 0)
            pszAddedPathSep = "/";
        else
            pszAddedPathSep = SEP_STRING;
        if (CPLStrlcat( pszStaticResult, pszAddedPathSep, CPL_PATH_BUF_SIZE ) >= CPL_PATH_BUF_SIZE)
            goto error;
    }

    if (CPLStrlcat( pszStaticResult, pszSecondaryFilename, CPL_PATH_BUF_SIZE ) >= CPL_PATH_BUF_SIZE)
        goto error;

    return pszStaticResult;
error:
    return CPLStaticBufferTooSmall(pszStaticResult);
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
/*      One simple case is when the base dir is '.' and the target      */
/*      filename is relative.                                           */
/* -------------------------------------------------------------------- */
    if( (nBasePathLen == 0 || EQUAL(pszBaseDir,"."))
        && CPLIsFilenameRelative(pszTarget) )
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
 * may be destroyed by the next CPL filename handling call.
 */

const char *CPLCleanTrailingSlash( const char *pszPath )

{
    char       *pszStaticResult = CPLGetStaticResult();
    int        iPathLength = strlen(pszPath);

    CPLAssert( ! (pszPath >= pszStaticResult && pszPath < pszStaticResult + CPL_PATH_BUF_SIZE) );

    if (iPathLength >= CPL_PATH_BUF_SIZE)
        return CPLStaticBufferTooSmall(pszStaticResult);

    CPLStrlcpy( pszStaticResult, pszPath, iPathLength+1 );

    if( iPathLength > 0 
        && (pszStaticResult[iPathLength-1] == '\\' 
            || pszStaticResult[iPathLength-1] == '/'))
        pszStaticResult[iPathLength-1] = '\0';

    return pszStaticResult;
}

/************************************************************************/
/*                       CPLCorrespondingPaths()                        */
/************************************************************************/

/**
 * Identify corresponding paths.
 *
 * Given a prototype old and new filename this function will attempt
 * to determine corresponding names for a set of other old filenames that
 * will rename them in a similar manner.  This correspondance assumes there
 * are two possibly kinds of renaming going on.  A change of path, and a 
 * change of filename stem. 
 * 
 * If a consistent renaming cannot be established for all the files this
 * function will return indicating an error.  
 *
 * The returned file list becomes owned by the caller and should be destroyed
 * with CSLDestroy(). 
 *
 * @param pszOldFilename path to old prototype file. 
 * @param pszNewFilename path to new prototype file. 
 * @param papszFileList list of other files associated with pszOldFilename to 
 * rename similarly.
 * 
 * @return a list of files corresponding to papszFileList but renamed to 
 * correspond to pszNewFilename.
 */

char **CPLCorrespondingPaths( const char *pszOldFilename, 
                              const char *pszNewFilename, 
                              char **papszFileList )

{
    CPLString osOldPath = CPLGetPath( pszOldFilename );
    CPLString osNewPath = CPLGetPath( pszNewFilename );
    CPLString osOldBasename = CPLGetBasename( pszOldFilename );
    CPLString osNewBasename = CPLGetBasename( pszNewFilename );
    int i;

    if( CSLCount(papszFileList) == 0 )
        return NULL;

/* -------------------------------------------------------------------- */
/*      There is a special case for a one item list which exactly       */
/*      matches the old name, to rename to the new name.                */
/* -------------------------------------------------------------------- */
    if( CSLCount(papszFileList) == 1 
        && strcmp(pszOldFilename,papszFileList[0]) == 0 )
    {
        return CSLAddString( NULL, pszNewFilename );
    }

/* -------------------------------------------------------------------- */
/*      If the basename is changing, verify that all source files       */
/*      have the same starting basename.                                */
/* -------------------------------------------------------------------- */
    if( osOldBasename != osNewBasename )
    {
        for( i=0; papszFileList[i] != NULL; i++ )
        {
            if( osOldBasename == CPLGetBasename( papszFileList[i] ) )
                continue;

            CPLString osFilePath, osFileName;

            osFilePath = CPLGetPath( papszFileList[i] );
            osFileName = CPLGetFilename( papszFileList[i] );

            if( !EQUALN(osFileName,osOldBasename,osOldBasename.size())
                || !EQUAL(osFilePath,osOldPath)
                || osFileName[osOldBasename.size()] != '.' )
            {
                CPLError( CE_Failure, CPLE_AppDefined, 
                          "Unable to rename fileset due irregular basenames.");
                return NULL;
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      If the filename portions differs, ensure they only differ in    */
/*      basename.                                                       */
/* -------------------------------------------------------------------- */
    if( osOldBasename != osNewBasename )
    {
        CPLString osOldExtra = CPLGetFilename(pszOldFilename) 
            + strlen(osOldBasename);
        CPLString osNewExtra = CPLGetFilename(pszNewFilename) 
            + strlen(osNewBasename);

        if( osOldExtra != osNewExtra )
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "Unable to rename fileset due to irregular filename correspondence." );
            return NULL;
        }
    }

/* -------------------------------------------------------------------- */
/*      Generate the new filenames.                                     */
/* -------------------------------------------------------------------- */
    char **papszNewList = NULL;

    for( i=0; papszFileList[i] != NULL; i++ )
    {
        CPLString osNewFilename;
        CPLString osOldFilename = CPLGetFilename( papszFileList[i] );

        if( osOldBasename == osNewBasename )
            osNewFilename = 
                CPLFormFilename( osNewPath, osOldFilename, NULL );
        else
            osNewFilename = 
                CPLFormFilename( osNewPath, osNewBasename, 
                                 osOldFilename.c_str()+strlen(osOldBasename));

        papszNewList = CSLAddString( papszNewList, osNewFilename );
    }

    return papszNewList;
}

/************************************************************************/
/*                      CPLGenerateTempFilename()                       */
/************************************************************************/

/**
 * Generate temporary file name.
 * 
 * Returns a filename that may be used for a temporary file.  The location
 * of the file tries to follow operating system semantics but may be
 * forced via the CPL_TMPDIR configuration option.  
 *
 * @param pszStem if non-NULL this will be part of the filename.
 * 
 * @return a filename which is valid till the next CPL call in this thread.
 */

const char *CPLGenerateTempFilename( const char *pszStem )

{
    const char *pszDir = CPLGetConfigOption( "CPL_TMPDIR", NULL );
    static volatile int nTempFileCounter = 0;

    if( pszDir == NULL )
        pszDir = CPLGetConfigOption( "TMPDIR", NULL );

    if( pszDir == NULL )
        pszDir = CPLGetConfigOption( "TEMP", NULL );

    if( pszDir == NULL )
        pszDir = ".";

    CPLString osFilename;

    if( pszStem == NULL )
        pszStem = "";

    osFilename.Printf( "%s%u_%d", pszStem, 
                       (int) CPLGetPID(), nTempFileCounter++ );

    return CPLFormFilename( pszDir, osFilename, NULL );
}
