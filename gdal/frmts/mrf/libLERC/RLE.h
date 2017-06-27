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

Contributors:  Thomas Maurer
*/

#ifndef RLE_H
#define RLE_H

#include "Defines.h"

NAMESPACE_LERC_START

/** RLE:
 *  run length encode a byte array
 *
 *  best case resize factor (all bytes are the same):
 *    (n + 1) * 3 / 32767 + 2  ~= 0.00009
 *
 *  worst case resize factor (no stretch of same bytes):
 *    n + 4 + 2 * (n - 1) / 32767 ~= 1.00006
 */

class RLE
{
public:
  RLE() : m_minNumEven(5) {}
  virtual ~RLE() {}

  size_t computeNumBytesRLE(const Byte* arr, size_t numBytes) const;

  // when done, call
  // delete[] *arrRLE;
  bool compress(const Byte* arr, size_t numBytes,
                Byte** arrRLE, size_t& numBytesRLE, bool verify = false) const;

  // when done, call
  // delete[] *arr;
  // cppcheck-suppress functionStatic
  bool decompress(const Byte* arrRLE, size_t nRemainingSize, Byte** arr, size_t& numBytes) const;

  // arr already allocated, just fill
  static bool decompress(const Byte* arrRLE, size_t nRemainingSize, Byte* arr, size_t arrSize);

protected:
  int m_minNumEven;

  static void writeCount(short cnt, Byte** ppCnt, Byte** ppDst);
  static short readCount(const Byte** ppCnt);
};

// -------------------------------------------------------------------------- ;

NAMESPACE_LERC_END
#endif
