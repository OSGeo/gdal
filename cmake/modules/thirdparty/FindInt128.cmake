# ============================================================================
#  MCKL/cmake/FindInt128.cmake
# ----------------------------------------------------------------------------
#  MCKL: Monte Carlo Kernel Library
# ----------------------------------------------------------------------------
#  Copyright (c) 2013-2017, Yan Zhou
#  All rights reserved.
#
#  Redistribution and use in source and binary forms, with or without
#  modification, are permitted provided that the following conditions are met:
#
#    Redistributions of source code must retain the above copyright notice,
#    this list of conditions and the following disclaimer.
#
#    Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
#
#  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS AS IS
#  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
#  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
#  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
#  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
#  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
#  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
#  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
#  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
#  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
#  POSSIBILITY OF SUCH DAMAGE.
# ============================================================================

# Find 128-bits integer type
#
# The following variable is set
#
# INT128_FOUND - TRUE if 128-bits integer type is found and works correctly
# INT128_TYPE  - The type of the 128-bits integer

if(DEFINED INT128_FOUND)
    return()
endif(DEFINED INT128_FOUND)

set(SAFE_CMAKE_REQUIRED_DEFINITIONS ${CMAKE_REQUIRED_DEFINITIONS})

set(INT128_TEST_SOURCE "#include <cassert>\nint main(){\n
    unsigned long long a = 0x0123456789ABCDEF;unsigned long long b = 0xFEDCBA9876543210;
    unsigned INT128 c = static_cast<unsigned INT128>(a) * static_cast<unsigned INT128>(b);
    assert(static_cast<unsigned long long>(c >> 0x00) == 0x2236D88FE5618CF0);
    assert(static_cast<unsigned long long>(c >> 0x40) == 0x0121FA00AD77D742);return 0;}"
)
set(INT128_TRY_TYPE "__int128")
set(CMAKE_REQUIRED_DEFINITIONS ${SAFE_CMAKE_REQUIRED_DEFINITIONS}
    -DINT128=${INT128_TRY_TYPE})
include(CheckCXXSourceCompiles)
check_cxx_source_compiles("${INT128_TEST_SOURCE}" INT128_TEST)
if(INT128_TEST)
    set(INT128_TYPE ${INT128_TRY_TYPE} CACHE STRING "128-bit type")
    set(INT128_FOUND TRUE CACHE BOOL "Found 128-bits integer type")
else(INT128_TEST)
    set(INT128_FOUND FALSE CACHE BOOL "NOT Found 128-bit integer type")
endif(INT128_TEST)
mark_as_advanced(INT128_TYPE)
mark_as_advanced(INT128_FOUND)

set(CMAKE_REQUIRED_DEFINITIONS ${SAFE_CMAKE_REQUIRED_DEFINITIONS})
