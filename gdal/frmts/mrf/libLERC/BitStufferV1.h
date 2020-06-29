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

#ifndef BITSTUFFERV1_H
#define BITSTUFFERV1_H

// ---- includes ------------------------------------------------------------ ;

#include <vector>
#include "DefinesV1.h"

NAMESPACE_LERC_START
/**
 * Bit stuffer, for writing unsigned int arrays compressed lossless
 */

struct BitStufferV1
{
  // these 2 do not allocate memory. Byte ptr is moved like a file pointer.
  static bool write(Byte** ppByte, const std::vector<unsigned int>& dataVec);
  static bool read(Byte** ppByte, size_t& nRemainingBytes, std::vector<unsigned int>& dataVec, size_t nMaxBufferVecElts);
  static int numBytesUInt(unsigned int k) { return (k <= 0xff) ? 1 : (k <= 0xffff) ? 2 : 4; }
  static unsigned int numTailBytesNotNeeded(unsigned int numElem, int numBits) {
      numBits = (numElem * numBits) & 31;
      return (numBits == 0 || numBits > 24) ? 0 : (numBits > 16) ? 1 : (numBits > 8) ? 2 : 3;
  }

};

// -------------------------------------------------------------------------- ;

NAMESPACE_LERC_END
#endif
