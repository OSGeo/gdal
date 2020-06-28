/*
Copyright 2015 Esri
Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at
http://www.apache.org/licenses/LICENSE-2.0
Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
A local copy of the license and additional notices are located with the
source distribution at:
http://github.com/Esri/lerc/
Contributors:
    Thomas Maurer
    Lucian Plesea
*/

#ifndef LERC_DEFINESV1_H
#define LERC_DEFINESV1_H

#include <cstddef> // size_t

#if defined(GDAL_COMPILATION)
#include "cpl_port.h"
#endif

#if __clang_major__ >= 4 || (__clang_major__ == 3 && __clang_minor__ >= 8)
#define LERC_NOSANITIZE_UNSIGNED_INT_OVERFLOW __attribute__((no_sanitize("unsigned-integer-overflow")))
#else
#define LERC_NOSANITIZE_UNSIGNED_INT_OVERFLOW
#endif

#ifndef NAMESPACE_LERC_START
#define NAMESPACE_LERC_START namespace GDAL_LercNS {
#define NAMESPACE_LERC_END }
#define USING_NAMESPACE_LERC using namespace GDAL_LercNS;
#endif

NAMESPACE_LERC_START
typedef unsigned char Byte;
NAMESPACE_LERC_END

#endif
