# ******************************************************************************
# * Project:  CMake4GDAL
# * Purpose:  CMake build scripts
# * Author:   Hiroshi Miura <miurahr@linux.com>
# ******************************************************************************
# * Copyright (C) 2017 Hiroshi Miura
# * 
# * Permission is hereby granted, free of charge, to any person obtaining a
# * copy of this software and associated documentation files (the "Software"),
# * to deal in the Software without restriction, including without limitation
# * the rights to use, copy, modify, merge, publish, distribute, sublicense,
# * and/or sell copies of the Software, and to permit persons to whom the
# * Software is furnished to do so, subject to the following conditions:
# *
# * The above copyright notice and this permission notice shall be included
# * in all copies or substantial portions of the Software.
# *
# * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
# * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
# * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
# * DEALINGS IN THE SOFTWARE.
# ******************************************************************************

cmake_minimum_required (VERSION 2.8.12)

# MACRO_OPTIONAL_ADD_DRIVER(NAME desc dir OFF/ON) do followings:
# - add subdirectory 'dir'
# - define option "ENABLE_NAME" then set to default OFF/ON
# - when enabled, add definition"-DNAME_ENABLED"

macro(MACRO_OPTIONAL_ADD_DRIVER _name _desc _default)
    option(OGR_ENABLE_${_name} "Set ON to build ${_desc} driver" ${_default})
    if (OGR_ENABLE_${_name})
        add_definitions(-D${_name}_ENABLED)
    endif()
endmacro()


# MACRO_ADD_OGR_DEFAULT_DRIVER(NAME desc dir OFF/ON)
# - add subdirectory "dir"
# - if GDAL_ENABLE_OGR then add definition -DNAME_ENABLED
# - else add option for driver NAME as of MACRO_OPTIONAL_ADD_DRIVER
macro(MACRO_ADD_OGR_DEFAULT_DRIVER _name _desc)
    if (GDAL_ENABLE_OGR)
        add_definitions(-D${_name}_ENABLED)
        set(OGR_ENABLE_${_name} ON)
    else()
        option(OGR_ENABLE_${_name} "Set ON to build ${_desc} driver" ON)
        if (OGR_ENABLE_${_name})
            add_defiitions(-D${_name}_ENABLED)
        endif()
    endif()
endmacro()

