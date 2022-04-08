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

#include <vector>
#include <cstring>
#include <utility>
#include "Defines.h"

NAMESPACE_LERC_START

/** Bit stuffer, for writing unsigned int arrays compressed lossless
 *
 */

class BitStuffer2
{
public:
  BitStuffer2()           {}
  virtual ~BitStuffer2()  {}

  // dst buffer is already allocated. byte ptr is moved like a file pointer.
  bool EncodeSimple(Byte** ppByte, const std::vector<unsigned int>& dataVec, int lerc2Version) const;
  bool EncodeLut(Byte** ppByte, const std::vector<std::pair<unsigned int, unsigned int> >& sortedDataVec, int lerc2Version) const;
  bool Decode(const Byte** ppByte, size_t& nBytesRemaining, std::vector<unsigned int>& dataVec, size_t maxElementCount, int lerc2Version) const;

  static unsigned int ComputeNumBytesNeededSimple(unsigned int numElem, unsigned int maxElem);
  static unsigned int ComputeNumBytesNeededLut(const std::vector<std::pair<unsigned int, unsigned int> >& sortedDataVec, bool& doLut);

private:
  mutable std::vector<unsigned int>  m_tmpLutVec, m_tmpIndexVec, m_tmpBitStuffVec;

  static void BitStuff_Before_Lerc2v3(Byte** ppByte, const std::vector<unsigned int>& dataVec, int numBits);
  bool BitUnStuff_Before_Lerc2v3(const Byte** ppByte, size_t& nBytesRemaining, std::vector<unsigned int>& dataVec, unsigned int numElements, int numBits) const;
  void BitStuff(Byte** ppByte, const std::vector<unsigned int>& dataVec, int numBits) const;
  bool BitUnStuff(const Byte** ppByte, size_t& nBytesRemaining, std::vector<unsigned int>& dataVec, unsigned int numElements, int numBits) const;

  static bool EncodeUInt(Byte** ppByte, unsigned int k, int numBytes);     // numBytes = 1, 2, or 4
  static bool DecodeUInt(const Byte** ppByte, size_t& nBytesRemaining, unsigned int& k, int numBytes);
  static int NumBytesUInt(unsigned int k)  { return (k < 256) ? 1 : (k < (1 << 16)) ? 2 : 4; }
  static unsigned int NumTailBytesNotNeeded(unsigned int numElem, int numBits);
};

// -------------------------------------------------------------------------- ;

inline unsigned int BitStuffer2::ComputeNumBytesNeededSimple(unsigned int numElem, unsigned int maxElem)
{
  int numBits = 0;
  while ((numBits < 32) && (maxElem >> numBits))
    numBits++;
  return 1 + NumBytesUInt(numElem) + ((numElem * numBits + 7) >> 3);
}

// -------------------------------------------------------------------------- ;

inline bool BitStuffer2::EncodeUInt(Byte** ppByte, unsigned int k, int numBytes)
{
  Byte* ptr = *ppByte;

  if (numBytes == 1)
    *ptr = (Byte)k;
  else if (numBytes == 2)
  {
    unsigned short kShort = (unsigned short)k;
    memcpy(ptr, &kShort, sizeof(unsigned short));
  }
  else if (numBytes == 4)
    memcpy(ptr, &k, sizeof(unsigned int));
  else
    return false;

  *ppByte += numBytes;
  return true;
}

// -------------------------------------------------------------------------- ;

inline bool BitStuffer2::DecodeUInt(const Byte** ppByte, size_t& nBytesRemaining, unsigned int& k, int numBytes)
{
  if (nBytesRemaining < (size_t)numBytes)
    return false;

  const Byte* ptr = *ppByte;

  if (numBytes == 1)
    k = *ptr;
  else if (numBytes == 2)
  {
    unsigned short s;
    memcpy(&s, ptr, sizeof(unsigned short));
    k = s;
  }
  else if (numBytes == 4)
    memcpy(&k, ptr, sizeof(unsigned int));
  else
    return false;

  *ppByte += numBytes;
  nBytesRemaining -= numBytes;
  return true;
}

// -------------------------------------------------------------------------- ;

inline unsigned int BitStuffer2::NumTailBytesNotNeeded(unsigned int numElem, int numBits)
{
  int numBitsTail = ((unsigned long long)numElem * numBits) & 31;
  int numBytesTail = (numBitsTail + 7) >> 3;
  return (numBytesTail > 0) ? 4 - numBytesTail : 0;
}

// -------------------------------------------------------------------------- ;

NAMESPACE_LERC_END
#endif
