/******************************************************************************
 *
 * Purpose:  Implementation of the PCIDSKException class.
 * 
 ******************************************************************************
 * Copyright (c) 2009
 * PCI Geomatics, 50 West Wilmot Street, Richmond Hill, Ont, Canada
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

#include "pcidsk_config.h"
#include "pcidsk_types.h"
#include "pcidsk_exception.h"
#include <cstdlib>
#include <cstring>
#include <cstdio>

using PCIDSK::PCIDSKException;

#if defined(_MSC_VER) && (_MSC_VER < 1500)
#  define vsnprintf _vsnprintf
#endif

/**

\class PCIDSK::PCIDSKException

\brief Generic SDK Exception

The PCIDSKException class is used for all errors thrown by the PCIDSK
library.  It includes a formatted message and is derived from std::exception.
The PCIDSK library throws all exceptions as pointers, and library exceptions 
should be caught like this:

@code
    try 
    {
         PCIDSKFile *file = PCIDSK::Open( "irvine.pix, "r", NULL );
    }
    catch( PCIDSK::PCIDSKException &ex )
    {
        fprintf( stderr, "PCIDSKException:\n%s\n", ex.what() );
        exit( 1 );
    }
@endcode

*/

/************************************************************************/
/*                          PCIDSKException()                           */
/************************************************************************/

/**
 * Create exception with formatted message.
 *
 * This constructor supports formatting of an exception message
 * using printf style format and additional arguments.
 *
 * @param fmt the printf style format (eg. "Illegal value:%d")
 * @param ... additional arguments as required by the format string.
 */

PCIDSKException::PCIDSKException( const char *fmt, ... )

{
    va_list args;

    va_start( args, fmt );
    vPrintf( fmt, args );
    va_end( args );
}

/************************************************************************/
/*                          ~PCIDSKException()                          */
/************************************************************************/

/**
 * Destructor.
 */

PCIDSKException::~PCIDSKException() throw()

{
}

/************************************************************************/
/*                              vPrintf()                               */
/************************************************************************/

/**
 * Format a message.
 *
 * Assigns a message to an exception using printf style formatting
 * and va_list arguments (similar to vfprintf(). 
 *
 * @param fmt printf style format string.
 * @param args additional arguments as required.
 */


void PCIDSKException::vPrintf( const char *fmt, va_list args )

{
/* -------------------------------------------------------------------- */
/*      This implementation for platforms without vsnprintf() will      */
/*      just plain fail if the formatted contents are too large.        */
/* -------------------------------------------------------------------- */

#if defined(MISSING_VSNPRINTF)
    char *pszBuffer = (char *) malloc(30000);
    if( vsprintf( pszBuffer, fmt, args) > 29998 )
    {
        message = "PCIDSKException::vPrintf() ... buffer overrun.";
    }
    else
        message = pszBuffer;

    free( pszBuffer );

/* -------------------------------------------------------------------- */
/*      This should grow a big enough buffer to hold any formatted      */
/*      result.                                                         */
/* -------------------------------------------------------------------- */
#else
    char szModestBuffer[500];
    int nPR;
    va_list wrk_args;

#ifdef va_copy
    va_copy( wrk_args, args );
#else
    wrk_args = args;
#endif
    
    nPR = vsnprintf( szModestBuffer, sizeof(szModestBuffer), fmt, 
                     wrk_args );
    if( nPR == -1 || nPR >= (int) sizeof(szModestBuffer)-1 )
    {
        int nWorkBufferSize = 2000;
        char *pszWorkBuffer = (char *) malloc(nWorkBufferSize);

#ifdef va_copy
        va_end( wrk_args );
        va_copy( wrk_args, args );
#else
        wrk_args = args;
#endif
        while( (nPR=vsnprintf( pszWorkBuffer, nWorkBufferSize, fmt, wrk_args))
               >= nWorkBufferSize-1 
               || nPR == -1 )
        {
            nWorkBufferSize *= 4;
            pszWorkBuffer = (char *) realloc(pszWorkBuffer, 
                                             nWorkBufferSize );
#ifdef va_copy
            va_end( wrk_args );
            va_copy( wrk_args, args );
#else
            wrk_args = args;
#endif
        }
        message = pszWorkBuffer;
        free( pszWorkBuffer );
    }
    else
    {
        message = szModestBuffer;
    }
    va_end( wrk_args );
#endif
}

/**
 * \fn const char *PCIDSKException::what() const throw();
 *
 * \brief fetch exception message.
 *
 * @return a pointer to the internal message associated with the exception.
 */ 

/**
 * \brief throw a formatted exception.
 *
 * This function throws a PCIDSK Exception by reference after formatting
 * the message using the given printf style format and arguments.  This
 * function exists primarily so that throwing an exception can be done in
 * one line of code, instead of declaring an exception and then throwing it.
 *
 * @param fmt the printf style format (eg. "Illegal value:%d")
 * @param ... additional arguments as required by the format string.
 */
void PCIDSK::ThrowPCIDSKException( const char *fmt, ... )

{
    va_list args;
    PCIDSKException ex("");

    va_start( args, fmt );
    ex.vPrintf( fmt, args );
    va_end( args );

    throw ex;
}
