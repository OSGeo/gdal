/**********************************************************************
 * $Id$
 *
 * Name:     cpl_error.cpp
 * Project:  CPL - Common Portability Library
 * Purpose:  Error handling functions.
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
 * Revision 1.10  1999/11/23 04:16:56  danmo
 * Fixed var. initialization that failed to compile as C
 *
 * Revision 1.9  1999/09/03 17:03:45  warmerda
 * Completed partial help line.
 *
 * Revision 1.8  1999/07/23 14:27:47  warmerda
 * CPLSetErrorHandler returns old handler
 *
 * Revision 1.7  1999/06/27 16:50:52  warmerda
 * added support for CPL_DEBUG and CPL_LOG variables
 *
 * Revision 1.6  1999/06/26 02:46:11  warmerda
 * Fixed initialization of debug messages.
 *
 * Revision 1.5  1999/05/20 14:59:05  warmerda
 * added CPLDebug()
 *
 * Revision 1.4  1999/05/20 02:54:38  warmerda
 * Added API documentation
 *
 * Revision 1.3  1998/12/15 19:02:27  warmerda
 * Avoid use of errno as a variable
 *
 * Revision 1.2  1998/12/06 02:52:52  warmerda
 * Implement assert support
 *
 * Revision 1.1  1998/12/03 18:26:02  warmerda
 * New
 *
 **********************************************************************/

#include "cpl_error.h"
#include "cpl_vsi.h"

/* static buffer to store the last error message.  We'll assume that error
 * messages cannot be longer than 2000 chars... which is quite reasonable
 * (that's 25 lines of 80 chars!!!)
 */
static char gszCPLLastErrMsg[2000] = "";
static int  gnCPLLastErrNo = 0;

static void CPLDefaultErrorHandler( CPLErr, int, const char *);
static CPLErrorHandler gpfnCPLErrorHandler = CPLDefaultErrorHandler;

/**********************************************************************
 *                          CPLError()
 **********************************************************************/

/**
 * Report an error.
 *
 * This function reports an error in a manner that can be hooked
 * and reported appropriate by different applications.
 *
 * The effect of this function can be altered by applications by installing
 * a custom error handling using CPLSetErrorHandler().
 *
 * The eErrClass argument can have the value CE_Warning indicating that the
 * message is an informational warning, CE_Failure indicating that the
 * action failed, but that normal recover mechanisms will be used or
 * CE_Fatal meaning that a fatal error has occured, and that CPLError()
 * should not return.  
 *
 * The default behaviour of CPLError() is to report errors to stderr,
 * and to abort() after reporting a CE_Fatal error.  It is expected that
 * some applications will want to supress error reporting, and will want to
 * install a C++ exception, or longjmp() approach to no local fatal error
 * recovery.
 *
 * Regardless of how application error handlers or the default error
 * handler choose to handle an error, the error number, and message will
 * be stored for recovery with CPLGetLastErrorNo() and CPLGetLastErrorMsg().
 *
 * @param eErrClass one of CE_Warning, CE_Failure or CE_Fatal.
 * @param err_no the error number (CPLE_*) from cpl_error.h.
 * @param fmt a printf() style format string.  Any additional arguments
 * will be treated as arguments to fill in this format in a manner
 * similar to printf().
 */

void    CPLError(CPLErr eErrClass, int err_no, const char *fmt, ...)
{
    va_list args;

    /* Expand the error message 
     */
    va_start(args, fmt);
    vsprintf(gszCPLLastErrMsg, fmt, args);
    va_end(args);

    /* If the user provided his own error handling function, then call
     * it, otherwise print the error to stderr and return.
     */
    gnCPLLastErrNo = err_no;

    if( gpfnCPLErrorHandler )
        gpfnCPLErrorHandler(eErrClass, err_no, gszCPLLastErrMsg);

    if( eErrClass == CE_Fatal )
        abort();
}

/************************************************************************/
/*                              CPLDebug()                              */
/************************************************************************/

/**
 * Display a debugging message.
 *
 * The category argument is used in conjunction with the CPL_DEBUG
 * environment variable to establish if the message should be displayed.
 * If the CPL_DEBUG environment variable is not set, no debug messages
 * are emitted (use CPLError(CE_Warning,...) to ensure messages are displayed).
 * If CPL_DEBUG is set, but is an empty string or the word "ON" then all
 * debug messages are shown.  Otherwise only messages whose category appears
 * somewhere within the CPL_DEBUG value are displayed (as determinted by
 * strstr()).
 *
 * Categories are usually an identifier for the subsystem producing the
 * error.  For instance "GDAL" might be used for the GDAL core, and "TIFF"
 * for messages from the TIFF translator.  
 *
 * @param pszCategory name of the debugging message category.
 * @param pszFormat printf() style format string for message to display.
 *        Remaining arguments are assumed to be for format.
 */ 

void CPLDebug( const char * pszCategory, const char * pszFormat, ... )

{
    char	*pszMessage;
    va_list args;
    const char      *pszDebug = getenv("CPL_DEBUG");

/* -------------------------------------------------------------------- */
/*      Does this message pass our current criteria?                    */
/* -------------------------------------------------------------------- */
    if( pszDebug == NULL )
        return;

    if( !EQUAL(pszDebug,"ON") && !EQUAL(pszDebug,"") )
    {
        int            i, nLen = strlen(pszCategory);

        for( i = 0; pszDebug[i] != '\0'; i++ )
        {
            if( EQUALN(pszCategory,pszDebug+i,nLen) )
                break;
        }

        if( pszDebug[i] == '\0' )
            return;
    }

/* -------------------------------------------------------------------- */
/*      Format the error message                                        */
/* -------------------------------------------------------------------- */
    pszMessage = (char *) VSIMalloc(25000);
    if( pszMessage == NULL )
        return;
        
    strcpy( pszMessage, pszCategory );
    strcat( pszMessage, ": " );
    
    va_start(args, pszFormat);
    vsprintf(pszMessage+strlen(pszMessage), pszFormat, args);
    va_end(args);

/* -------------------------------------------------------------------- */
/*      If the user provided his own error handling function, then call */
/*      it, otherwise print the error to stderr and return.             */
/* -------------------------------------------------------------------- */
    if( gpfnCPLErrorHandler )
        gpfnCPLErrorHandler(CE_Debug, CPLE_None, pszMessage);

    VSIFree( pszMessage );
}

/**********************************************************************
 *                          CPLErrorReset()
 **********************************************************************/

/**
 * Erase any traces of previous errors.
 *
 * This is normally used to ensure that an error which has been recovered
 * from does not appear to be still in play with high level functions.
 */

void    CPLErrorReset()
{
    gnCPLLastErrNo = CPLE_None;
    gszCPLLastErrMsg[0] = '\0';
}


/**********************************************************************
 *                          CPLGetLastErrorNo()
 **********************************************************************/

/**
 * Fetch the last error number.
 *
 * This is the error number, not the error class.
 *
 * @return the error number of the last error to occur, or CPLE_None (0)
 * if there are no posted errors.
 */

int     CPLGetLastErrorNo()
{
    return gnCPLLastErrNo;
}

/**********************************************************************
 *                          CPLGetLastErrorMsg()
 **********************************************************************/

/**
 * Get the last error message.
 *
 * Fetches the last error message posted with CPLError(), that hasn't
 * been cleared by CPLErrorReset().  The returned pointer is to an internal
 * string that should not be altered or freed.
 *
 * @return the last error message, or NULL if there is no posted error
 * message.
 */

const char* CPLGetLastErrorMsg()
{
    return gszCPLLastErrMsg;
}

/************************************************************************/
/*                       CPLDefaultErrorHandler()                       */
/************************************************************************/

static void CPLDefaultErrorHandler( CPLErr eErrClass, int nError, 
                                    const char * pszErrorMsg )

{
    static int       bLogInit = FALSE;
    static FILE *    fpLog;
    fpLog = stderr;

    if( !bLogInit )
    {
        bLogInit = TRUE;

        if( getenv( "CPL_LOG" ) != NULL )
        {
            fpLog = fopen( getenv("CPL_LOg"), "wt" );
            if( fpLog == NULL )
                fpLog = stderr;
        }
    }

    if( eErrClass == CE_Debug )
        fprintf( fpLog, "%s\n", pszErrorMsg );
    else if( eErrClass == CE_Warning )
        fprintf( fpLog, "Warning %d: %s\n", nError, pszErrorMsg );
    else
        fprintf( fpLog, "ERROR %d: %s\n", nError, pszErrorMsg );

    fflush( fpLog );
}


/**********************************************************************
 *                          CPLSetErrorHandler()
 **********************************************************************/

/**
 * Install custom error handler.
 *
 * Allow the library's user to specify his own error handler function.
 * A valid error handler is a C function with the following prototype:
 *
 * <pre>
 *     void MyErrorHandler(CPLErr eErrClass, int errno, const char *msg)
 * </pre>
 *
 * Pass NULL to come back to the default behavior.  The default behaviour
 * is to write the message to stderr. 
 *
 * The msg will be a partially formatted error message not containing the
 * "ERROR %d:" portion emitted by the default handler.  Message formatting
 * is handled by CPLError() before calling the handler.  If the error
 * handler function is passed a CE_Fatal class error and returns, then
 * CPLError() will call abort(). Applications wanting to interrupt this
 * fatal behaviour will have to use longjmp(), or a C++ exception to
 * indirectly exit the function.
 *
 * @param pfnErrorHandler new error handler function.
 * @return returns the previously installed error handler.
 */ 

CPLErrorHandler CPLSetErrorHandler( CPLErrorHandler pfnErrorHandler )
{
    CPLErrorHandler	pfnOldHandler = gpfnCPLErrorHandler;
    
    gpfnCPLErrorHandler = pfnErrorHandler;

    return pfnOldHandler;
}

/************************************************************************/
/*                             _CPLAssert()                             */
/*                                                                      */
/*      This function is called only when an assertion fails.           */
/************************************************************************/

/**
 * Report failure of a logical assertion.
 *
 * Applications would normally use the CPLAssert() macro which expands
 * into code calling _CPLAssert() only if the condition fails.  _CPLAssert()
 * will generate a CE_Fatal error call to CPLError(), indicating the file
 * name, and line number of the failed assertion, as well as containing
 * the assertion itself.
 *
 * There is no reason for application code to call _CPLAssert() directly.
 */

void _CPLAssert( const char * pszExpression, const char * pszFile,
                 int iLine )

{
    CPLError( CE_Fatal, CPLE_AssertionFailed,
              "Assertion `%s' failed\n"
              "in file `%s', line %d\n",
              pszExpression, pszFile, iLine );
}

