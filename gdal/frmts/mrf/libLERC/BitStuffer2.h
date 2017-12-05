
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
#ifndef BITSTUFFER2_H
#define BITSTUFFER2_H
// ---- includes ------------------------------------------------------------ ;

#include "Defines.h"
#include <vector>
#include <cstring>

NAMESPACE_LERC_START

// -------------------------------------------------------------------------- ;

// ---- related classes ----------------------------------------------------- ;

// -------------------------------------------------------------------------- ;

/** Bit stuffer, for writing unsigned int arrays compressed lossless
 *
 */

class BitStuffer2
{
public:
  BitStuffer2()           {}
  virtual ~BitStuffer2()  {}

  // these 3 do not allocate memory. Byte ptr is moved like a file pointer.
  static bool EncodeSimple(Byte** ppByte, const std::vector<unsigned int>& dataVec);
  // cppcheck-suppress functionStatic
  bool EncodeLut(Byte** ppByte, const std::vector<Quant>& sortedDataVec) const;
  bool Decode(const Byte** ppByte, size_t& nRemainingBytes, std::vector<unsigned int>& dataVec, size_t nMaxBufferVecElts) const;

  static unsigned int ComputeNumBytesNeededSimple(unsigned int numElem, unsigned int maxElem);
  static unsigned int ComputeNumBytesNeededLut(const std::vector<Quant>& sortedDataVec,
                                         bool& doLut);

  static unsigned int NumExtraBytesToAllocate()  { return 3; }

private:
  mutable std::vector<unsigned int>  m_tmpLutVec, m_tmpIndexVec;

  static void BitStuff(Byte** ppByte, const std::vector<unsigned int>& dataVec, int numBits);
  // cppcheck-suppress functionStatic
  bool BitUnStuff(const Byte** ppByte, size_t& nReminingBytes, std::vector<unsigned int>& dataVec, unsigned int numElements, int numBits) const;
  static bool EncodeUInt(Byte** ppByte, unsigned int k, int numBytes);     // numBytes = 1, 2, or 4
  static bool DecodeUInt(const Byte** ppByte, size_t& nReminingBytes, unsigned int& k, int numBytes);
  static int NumBytesUInt(unsigned int k) { return (k < 256) ? 1 : (k < (1 << 16)) ? 2 : 4; }
  static unsigned int NumTailBytesNotNeeded(unsigned int numElem, int numBits);
};

// -------------------------------------------------------------------------- ;
// -------------------------------------------------------------------------- ;

inline
unsigned int BitStuffer2::ComputeNumBytesNeededSimple(unsigned int numElem, unsigned int maxElem)
{
  int numBits = 0;
  while ((numBits < 32) && (maxElem >> numBits))
    numBits++;
  return 1 + NumBytesUInt(numElem) + ((numElem * numBits + 7) >> 3);
}

// -------------------------------------------------------------------------- ;

inline
bool BitStuffer2::EncodeUInt(Byte** ppByte, unsigned int k, int numBytes)
{
  Byte* ptr = *ppByte;

  if (numBytes == 1)
    *ptr = (Byte)k;
  else if (numBytes == 2)
  {
    const unsigned short kShort = (unsigned short)k;
    memcpy(ptr, &kShort, sizeof(unsigned short));
  }
  else if (numBytes == 4)
  {
    const unsigned int kInt = (unsigned int)k;
    memcpy(ptr, &kInt, sizeof(unsigned int));
  }
  else
    return false;

  *ppByte += numBytes;
  return true;
}

// -------------------------------------------------------------------------- ;

inline
bool BitStuffer2::DecodeUInt(const Byte** ppByte, size_t& nRemainingBytes, unsigned int& k, int numBytes)
{
  const Byte* ptr = *ppByte;
  if( nRemainingBytes < static_cast<size_t>(numBytes) )
  {
    LERC_BRKPNT();
    return false;
  }

  if (numBytes == 1)
    k = *ptr;
  else if (numBytes == 2)
  {
    unsigned short s;
    memcpy(&s, ptr, sizeof(unsigned short));
    k = s;
  }
  else if (numBytes == 4)
  {
    memcpy(&k, ptr, sizeof(unsigned int));
  }
  else
    return false;

  *ppByte += numBytes;
  nRemainingBytes -= numBytes;
  return true;
}

// -------------------------------------------------------------------------- ;

inline
unsigned int BitStuffer2::NumTailBytesNotNeeded(unsigned int numElem, int numBits)
{
  int numBitsTail = (numElem * numBits) & 31;
  int numBytesTail = (numBitsTail + 7) >> 3;
  return (numBytesTail > 0) ? 4 - numBytesTail : 0;
}

// -------------------------------------------------------------------------- ;

NAMESPACE_LERC_END
#endif
