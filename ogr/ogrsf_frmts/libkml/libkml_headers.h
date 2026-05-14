/******************************************************************************
 *
 * Project:  KML Translator
 * Purpose:  Implements OGRLIBKMLDriver
 * Author:   Even Rouault <even dot rouault at spatialys dot com>
 *
 ******************************************************************************
 * Copyright (c) 2015, Even Rouault <even dot rouault at spatialys dot com>
 *
 * SPDX-License-Identifier: MIT
 *****************************************************************************/

#ifndef LIBKML_HEADERS_H
#define LIBKML_HEADERS_H

#if ((__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 2)) &&               \
     !defined(_MSC_VER))
#pragma GCC system_header
#endif

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable                                                        \
                : 4512) /* assignment operator could not be generated  \
                                 */
#endif

#if defined(__clang_major__) && __clang_major__ >= 21 && __cplusplus >= 202302L
// Since Boost 1.90, in particular https://github.com/boostorg/smart_ptr/commit/d08d035bdf4d5d1b9d5c6b798e0b7da8fa9bb325
// methods of Boost intrusive_ptr class are prefixed with a BOOST_SP_CXX20_CONSTEXPR macro
// defined in a ‎include/boost/smart_ptr/detail/sp_cxx20_constexpr.hpp‎ header
// protected by BOOST_SMART_PTR_DETAIL_SP_CXX20_CONSTEXPR_HPP_INCLUDED
// The expansion of that BOOST_SP_CXX20_CONSTEXPR macro cause build issues in
// libkml header with clang 21 *and* c++23
// e.g: clang++ -std=c++23 -c /usr/include/kml/dom/xal.h
// outputs:
/*
 /usr/include/boost/smart_ptr/intrusive_ptr.hpp:76:23: error: use of undeclared identifier 'intrusive_ptr_release'
   76 |         if( px != 0 ) intrusive_ptr_release( px );
      |                       ^~~~~~~~~~~~~~~~~~~~~
/usr/include/kml/dom/xal.h:62:11: note: in instantiation of member function 'boost::intrusive_ptr<kmldom::XalCountry>::~intrusive_ptr' requested here
   62 |   virtual ~XalAddressDetails() {}
      |           ^
*/
// It is not clear to me if this is a defect of clang 21 in C++23 mode, or
// if some changes would be needed in the way libkml defines the intrusive_ptr_release()
// function in kml/base/referent.h
// Anyway the following defines have the consequence of the methods of intrusive_ptr
// not being marked as constexpr, which works around the issue.
#define BOOST_SMART_PTR_DETAIL_SP_CXX20_CONSTEXPR_HPP_INCLUDED
#define BOOST_SP_CXX20_CONSTEXPR
#define BOOST_SP_NO_CXX20_CONSTEXPR
#endif

#include <kml/engine.h>
#include <kml/dom.h>
#include <kml/base/color32.h>
#include <kml/base/file.h>

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#endif
