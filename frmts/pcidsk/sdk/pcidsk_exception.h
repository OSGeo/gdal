/******************************************************************************
 *
 * Purpose:  Declaration of the PCIDSKException class. All exceptions thrown
 *           by the PCIDSK library will be of this type.
 *
 ******************************************************************************
 * Copyright (c) 2009
 * PCI Geomatics, 90 Allstate Parkway, Markham, Ontario, Canada.
 *
 * SPDX-License-Identifier: MIT
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

    class PCIDSK_DLL PCIDSKException : public std::exception
    {
        friend void PCIDSK_DLL ThrowPCIDSKException( const char *fmt, ... ) PCIDSK_PRINT_FUNC_FORMAT(1,2);
        friend int PCIDSK_DLL ThrowPCIDSKException( int ret_unused, const char *fmt, ... ) PCIDSK_PRINT_FUNC_FORMAT(2,3);
        friend void PCIDSK_DLL * ThrowPCIDSKExceptionPtr( const char *fmt, ... ) PCIDSK_PRINT_FUNC_FORMAT(1,2);
        PCIDSKException() {}
    public:
        PCIDSKException(const char *fmt, ... ) PCIDSK_PRINT_FUNC_FORMAT(2,3);
        virtual ~PCIDSKException() throw();

        void vPrintf( const char *fmt, std::va_list list );
        virtual const char *what() const throw() override { return message.c_str(); }
    private:
        std::string   message;
    };

    void PCIDSK_DLL ThrowPCIDSKException( const char *fmt, ... ) PCIDSK_PRINT_FUNC_FORMAT(1,2);
    int PCIDSK_DLL ThrowPCIDSKException( int ret_unused, const char *fmt, ... ) PCIDSK_PRINT_FUNC_FORMAT(2,3);
    void PCIDSK_DLL * ThrowPCIDSKExceptionPtr( const char *fmt, ... ) PCIDSK_PRINT_FUNC_FORMAT(1,2);

} // end namespace PCIDSK

#endif // INCLUDE_PCIDSK_EXCEPTION_H
