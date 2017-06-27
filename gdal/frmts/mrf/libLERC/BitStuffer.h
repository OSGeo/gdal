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

#ifndef BITSTUFFER_H
#define BITSTUFFER_H

// ---- includes ------------------------------------------------------------ ;

#include <vector>
#include <algorithm>
#include "Defines.h"

NAMESPACE_LERC_START
/** Bit stuffer, for writing unsigned int arrays compressed lossless
 *
 */

class BitStuffer
{
public:
  BitStuffer()           {}
  virtual ~BitStuffer()  {}

  // these 2 do not allocate memory. Byte ptr is moved like a file pointer.
  static bool write(Byte** ppByte, const std::vector<unsigned int>& dataVec);
  static bool read( Byte** ppByte, size_t& nRemainingBytes, std::vector<unsigned int>& dataVec, size_t nMaxBufferVecElts);

  static unsigned int computeNumBytesNeeded(unsigned int numElem, unsigned int maxElem);
  static unsigned int numExtraBytesToAllocate()  { return 3; }

protected:
  static unsigned int findMax(const std::vector<unsigned int>& dataVec);

  // numBytes = 1, 2, or 4
  static bool writeUInt(Byte** ppByte, unsigned int k, int numBytes);
  static bool readUInt( Byte** ppByte, size_t& nRemainingBytes, unsigned int& k, int numBytes);

  static int numBytesUInt(unsigned int k)  { return (k < 256) ? 1 : (k < (1 << 16)) ? 2 : 4; }
  static unsigned int numTailBytesNotNeeded(unsigned int numElem, int numBits);
};

// -------------------------------------------------------------------------- ;

NAMESPACE_LERC_END
#endif
