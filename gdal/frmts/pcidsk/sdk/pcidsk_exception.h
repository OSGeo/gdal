/******************************************************************************
 *
 * Purpose:  Declaration of the PCIDSKException class. All exceptions thrown
 *           by the PCIDSK library will be of this type.
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
#ifndef INCLUDE_PCIDSK_EXCEPTION_H
#define INCLUDE_PCIDSK_EXCEPTION_H

#include "pcidsk_config.h"

#include <string>
#include <cstdarg>
#include <stdexcept>

namespace PCIDSK
{
/************************************************************************/
/*                              Exception                               */
/************************************************************************/

    class PCIDSKException : public std::exception
    {
        friend void PCIDSK_DLL ThrowPCIDSKException( const char *fmt, ... ) PCIDSK_PRINT_FUNC_FORMAT(1,2);
        friend int PCIDSK_DLL ThrowPCIDSKException( int ret_unused, const char *fmt, ... ) PCIDSK_PRINT_FUNC_FORMAT(2,3);
        friend void* PCIDSK_DLL ThrowPCIDSKExceptionPtr( const char *fmt, ... ) PCIDSK_PRINT_FUNC_FORMAT(1,2);
        PCIDSKException() {}
    public:
        PCIDSKException(const char *fmt, ... ) PCIDSK_PRINT_FUNC_FORMAT(2,3);
        virtual ~PCIDSKException() throw();

        void vPrintf( const char *fmt, std::va_list list );
        virtual const char *what() const throw() { return message.c_str(); }
    private:
        std::string   message;
    };

    void PCIDSK_DLL ThrowPCIDSKException( const char *fmt, ... ) PCIDSK_PRINT_FUNC_FORMAT(1,2);
    int PCIDSK_DLL ThrowPCIDSKException( int ret_unused, const char *fmt, ... ) PCIDSK_PRINT_FUNC_FORMAT(2,3);
    void* PCIDSK_DLL ThrowPCIDSKExceptionPtr( const char *fmt, ... ) PCIDSK_PRINT_FUNC_FORMAT(1,2);

} // end namespace PCIDSK

#endif // INCLUDE_PCIDSK_EXCEPTION_H
