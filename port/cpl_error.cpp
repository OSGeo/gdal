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
 * Revision 1.1  1998/12/03 18:26:02  warmerda
 * New
 *
 **********************************************************************/

#include "cpl_error.h"

/* static buffer to store the last error message.  We'll assume that error
 * messages cannot be longer than 2000 chars... which is quite reasonable
 * (that's 25 lines of 80 chars!!!)
 */
static char gszCPLLastErrMsg[2000] = "";
static int  gnCPLLastErrNo = 0;

static void (*gpfnCPLErrorHandler)(CPLErr, int, const char *) = NULL;

/**********************************************************************
 *                          CPLError()
 *
 * This function records an error code and displays the error message
 * to stderr.
 *
 * The error code can be accessed later using CPLGetLastErrNo()
 **********************************************************************/
void    CPLError(CPLErr eErrClass, int errno, const char *fmt, ...)
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
    gnCPLLastErrNo = errno;

    if (gpfnCPLErrorHandler != NULL)
    {
        gpfnCPLErrorHandler(eErrClass, errno, gszCPLLastErrMsg);
    }
    else
    {
        fprintf(stderr, "ERROR %d: %s\n", gnCPLLastErrNo, gszCPLLastErrMsg);
    }

    if( eErrClass == CE_Fatal )
        abort();
}

/**********************************************************************
 *                          CPLErrorReset()
 *
 * Erase any traces of previous errors.
 **********************************************************************/
void    CPLErrorReset()
{
    gnCPLLastErrNo = 0;
    gszCPLLastErrMsg[0] = '\0';
}


/**********************************************************************
 *                          CPLGetLastErrorNo()
 *
 **********************************************************************/
int     CPLGetLastErrorNo()
{
    return gnCPLLastErrNo;
}

/**********************************************************************
 *                          CPLGetLastErrorMsg()
 *
 **********************************************************************/
const char* CPLGetLastErrorMsg()
{
    return gszCPLLastErrMsg;
}

/**********************************************************************
 *                          CPLSetErrorHandler()
 *
 * Allow the library's user to specify his own error handler function.
 *
 * A valid error handler is a C function with the following prototype:
 *
 *     void MyErrorHandler(int errno, const char *msg)
 *
 * Pass NULL to come back to the default behavior.
 **********************************************************************/

void     CPLSetErrorHandler(void (*pfnErrorHandler)(CPLErr, int, const char *))
{
    gpfnCPLErrorHandler = pfnErrorHandler;
}

