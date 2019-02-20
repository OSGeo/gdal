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

Contributors:  Thomas Maurer, Lucian Plesea
*/

#ifndef LERC_DEFINES_H
#define LERC_DEFINES_H

// This is useful when compiling within GDAL in DEBUG_BOOL mode, where a
// MSVCPedanticBool class is used as an alias for the bool type, so as
// to catch more easily int/bool misuses, even on Linux
// Also for NULL_AS_NULLPTR mode where NULL is aliased to C++11 nullptr
#if defined(DEBUG_BOOL) || defined(NULL_AS_NULLPTR)
#include "cpl_port.h"
#endif

#define NAMESPACE_LERC_START namespace GDAL_LercNS {
#define NAMESPACE_LERC_END }
#define USING_NAMESPACE_LERC using namespace GDAL_LercNS;

//#define HAVE_LERC1_DECODE
#ifdef GDAL_COMPILATION
#define CHECK_FOR_NAN
#endif

NAMESPACE_LERC_START

typedef unsigned char Byte;

//#ifndef max
//#define max(a,b)      (((a) > (b)) ? (a) : (b))
//#endif
//
//#ifndef min
//#define min(a,b)      (((a) < (b)) ? (a) : (b))
//#endif

#ifdef SWAPB    // define this on big endian system

// big endian systems no longer supported by Lerc

#else // SWAPB

#define SWAP_2(x)
#define SWAP_4(x)
#define SWAP_8(x)

#endif // SWAPB

NAMESPACE_LERC_END
#endif
