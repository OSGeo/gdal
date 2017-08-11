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

#include "Huffman.h"
#include "BitStuffer2.h"
#include <queue>
#include <cstring>
#include <algorithm>

using namespace std;

NAMESPACE_LERC_START

// -------------------------------------------------------------------------- ;

bool Huffman::ComputeCodes(const vector<int>& histo)
{
  if (histo.empty() || histo.size() >= m_maxHistoSize)
    return false;

  priority_queue<Node, vector<Node>, less<Node> > pq;

  int numNodes = 0;

  int size = (int)histo.size();
  for (int i = 0; i < size; i++)    // add all leaf nodes
    if (histo[i] > 0)
      pq.push(Node((short)i, histo[i]));

  if (pq.size() < 2)    // histo has only 0 or 1 bin that is not empty; quit Huffman and give it to Lerc
    return false;

  while (pq.size() > 1)    // build the Huffman tree
  {
    Node* child0 = new Node(pq.top());
    numNodes++;
    pq.pop();
    Node* child1 = new Node(pq.top());
    numNodes++;
    pq.pop();
    pq.push(Node(child0, child1));
  }

  m_codeTable.resize(size);
  std::fill( m_codeTable.begin(), m_codeTable.end(),
             std::pair<short, unsigned int>(0, 0) );

  if (!pq.top().TreeToLUT(0, 0, m_codeTable))    // fill the LUT
    return false;

  Node nodeNonConst = pq.top();
  nodeNonConst.FreeTree(numNodes);

  if (numNodes != 0)    // check the ref count
    return false;

  if (!ConvertCodesToCanonical())
    return false;

  return true;
}

// -------------------------------------------------------------------------- ;

bool Huffman::ComputeCompressedSize(const std::vector<int>& histo, int& numBytes, double& avgBpp) const
{
  if (histo.empty() || histo.size() >= m_maxHistoSize)
    return false;

  numBytes = 0;
  if (!ComputeNumBytesCodeTable(numBytes))    // header and code table
    return false;

  int numBits = 0, numElem = 0;
  int size = (int)histo.size();
  for (int i = 0; i < size; i++)
    if (histo[i] > 0)
    {
      numBits += histo[i] * m_codeTable[i].first;
      numElem += histo[i];
    }

  /* to please Coverity about potential divide by zero below */
  if( numElem == 0 )
    return false;

  int numUInts = ((((numBits + 7) >> 3) + 3) >> 2) + 1;    // add one more as the decode LUT can read ahead
  numBytes += 4 * numUInts;    // data huffman coded
  avgBpp = 8 * numBytes / (double)numElem;

  return true;
}

// -------------------------------------------------------------------------- ;

bool Huffman::SetCodes(const vector<pair<short, unsigned int> >& codeTable)
{
  if (codeTable.empty() || codeTable.size() >= m_maxHistoSize)
    return false;

  m_codeTable = codeTable;
  return true;
}

// -------------------------------------------------------------------------- ;

bool Huffman::WriteCodeTable(Byte** ppByte) const
{
  if (!ppByte)
    return false;

  int i0, i1, maxLen;
  if (!GetRange(i0, i1, maxLen))
    return false;

  int size = (int)m_codeTable.size();
  vector<unsigned int> dataVec(i1 - i0, 0);

  for (int i = i0; i < i1; i++)
  {
    int k = GetIndexWrapAround(i, size);
    dataVec[i - i0] = (unsigned int)m_codeTable[k].first;
  }

  // header
  vector<int> intVec;
  intVec.push_back(3);    // huffman version, 3 uses canonical huffman codes
  intVec.push_back(size);
  intVec.push_back(i0);   // code range
  intVec.push_back(i1);

  Byte* ptr = *ppByte;

  for (size_t i = 0; i < intVec.size(); i++)
  {
    *((int*)ptr) = intVec[i];
    ptr += sizeof(int);
  }

  BitStuffer2 bitStuffer2;
  if (!bitStuffer2.EncodeSimple(&ptr, dataVec))    // code lengths, bit stuffed
    return false;

  if (!BitStuffCodes(&ptr, i0, i1))    // variable length codes, bit stuffed
    return false;

  *ppByte = ptr;
  return true;
}

// -------------------------------------------------------------------------- ;

bool Huffman::ReadCodeTable(const Byte** ppByte, size_t& nRemainingBytesInOut)
{
  if (!ppByte || !(*ppByte))
    return false;

  const Byte* ptr = *ppByte;
  size_t nRemainingBytes = nRemainingBytesInOut;

  if( nRemainingBytes < sizeof(int) )
  {
      LERC_BRKPNT();
      return false;
  }
  int version;
  // FIXME endianness handling
  memcpy(&version, ptr, sizeof(int));    // version
  ptr += sizeof(int);
  nRemainingBytes -= sizeof(int);

  if (version < 2) // allow forward compatibility
    return false;

  vector<int> intVec(4, 0);
  if( nRemainingBytes < sizeof(int) * ( intVec.size() - 1 ) )
  {
      LERC_BRKPNT();
      return false;
  }
  for (size_t i = 1; i < intVec.size(); i++)
  {
    memcpy(&intVec[i], ptr, sizeof(int)); // FIXME endianness handling
    ptr += sizeof(int);
  }
  nRemainingBytes -= sizeof(int) * ( intVec.size() - 1 );

  int size = intVec[1];
  int i0 = intVec[2];
  int i1 = intVec[3];

  if (i0 >= i1 || i0 < 0 || size < 0 || size > (int)m_maxHistoSize ||
      i1 - i0 >= size)
  {
    LERC_BRKPNT();
    return false;
  }

  try
  {
    vector<unsigned int> dataVec(i1 - i0, 0);
    BitStuffer2 bitStuffer2;
    if (!bitStuffer2.Decode(&ptr, nRemainingBytes, dataVec, i1 - i0))    // unstuff the code lengths
    {
        LERC_BRKPNT();
        return false;
    }
    if( dataVec.size() != static_cast<size_t>(i1 - i0) )
    {
        LERC_BRKPNT();
        return false;
    }

    m_codeTable.resize(size);
    std::fill( m_codeTable.begin(), m_codeTable.end(),
                std::pair<short, unsigned int>(0, 0) );

    if( GetIndexWrapAround(i0, size) >= size ||
        GetIndexWrapAround(i1 - 1, size) >= size )
    {
        LERC_BRKPNT();
        return false;
    }

    for (int i = i0; i < i1; i++)
    {
        int k = GetIndexWrapAround(i, size);
        m_codeTable[k].first = (short)dataVec[i - i0];
    }

    if (!BitUnStuffCodes(&ptr, nRemainingBytes, i0, i1))    // unstuff the codes
    {
        LERC_BRKPNT();
        return false;
    }

    *ppByte = ptr;
    nRemainingBytesInOut = nRemainingBytes;
    return true;
  }
  catch( std::bad_alloc& )
  {
    return false;
  }
}

// -------------------------------------------------------------------------- ;

bool Huffman::BuildTreeFromCodes(int& numBitsLUT)
{
  int i0, i1, maxLen;
  if (!GetRange(i0, i1, maxLen))
    return false;

  // build decode LUT using max of 12 bits
  int size = (int)m_codeTable.size();

  bool bNeedTree = maxLen > m_maxNumBitsLUT;
  numBitsLUT = min(maxLen, m_maxNumBitsLUT);

  m_decodeLUT.clear();
  const int lutMaxSize = 1 << numBitsLUT;
  m_decodeLUT.assign(lutMaxSize, pair<short, short>((short)-1, (short)-1));

  for (int i = i0; i < i1; i++)
  {
    int k = GetIndexWrapAround(i, size);
    int len = m_codeTable[k].first;
    if (len > 0 && len <= numBitsLUT)
    {
      int code = m_codeTable[k].second << (numBitsLUT - len);
      pair<short, short> entry((short)len, (short)k);
      int numEntries = 1 << (numBitsLUT - len);
      for (int j = 0; j < numEntries; j++)
        m_decodeLUT[code | j] = entry;    // add the duplicates
    }
  }

  if (!bNeedTree)    // decode LUT covers it all, no tree needed
    return true;

  // go over the codes too long for the LUT and count how many leading bits are 0 for all of them
  m_numBitsToSkipInTree = 32;
  for (int i = i0; i < i1; i++)
  {
    int k = GetIndexWrapAround(i, size);
    int len = m_codeTable[k].first;

    if (len > 0 && len > numBitsLUT)    // only codes not in the decode LUT
    {
      unsigned int code = m_codeTable[k].second;
      int shift = 1;
      while (true)
      {
          code = code >> 1;
          if( code == 0 )
              break;
          shift++;
      }
      m_numBitsToSkipInTree = min(m_numBitsToSkipInTree, len - shift);
    }
  }

  /* int numNodesCreated = 1; */
  Node emptyNode((short)-1, 0);

  if (!m_root)
    m_root = new Node(emptyNode);

  for (int i = i0; i < i1; i++)
  {
    int k = GetIndexWrapAround(i, size);
    int len = m_codeTable[k].first;

    if (len > 0 && len > numBitsLUT)    // add only codes not in the decode LUT
    {
      unsigned int code = m_codeTable[k].second;
      Node* node = m_root;
      int j = len - m_numBitsToSkipInTree; // reduce len by number of leading bits from above

      while (--j >= 0)    // go over the bits
      {
        if (code & (1 << j))
        {
          if (!node->child1)
          {
            node->child1 = new Node(emptyNode);
            /* numNodesCreated++; */
          }
          node = node->child1;
        }
        else
        {
          if (!node->child0)
          {
            node->child0 = new Node(emptyNode);
            /* numNodesCreated++; */
          }
          node = node->child0;
        }

        if (j == 0)    // last bit, leaf node
        {
          node->value = (short)k;    // set the value
        }
      }
    }
  }

  return true;
}

// -------------------------------------------------------------------------- ;

void Huffman::Clear()
{
  m_codeTable.clear();
  m_decodeLUT.clear();
  if (m_root)
  {
    int n = 0;
    m_root->FreeTree(n);
    delete m_root;
    m_root = NULL;
  }
}

// -------------------------------------------------------------------------- ;
// -------------------------------------------------------------------------- ;

bool Huffman::ComputeNumBytesCodeTable(int& numBytes) const
{
  int i0, i1, maxLen;
  if (!GetRange(i0, i1, maxLen))
    return false;

  int size = (int)m_codeTable.size();
  int sum = 0;
  for (int i = i0; i < i1; i++)
  {
    int k = GetIndexWrapAround(i, size);
    sum += m_codeTable[k].first;
  }

  numBytes = 4 * sizeof(int);    // version, size, first bin, (last + 1) bin

  BitStuffer2 bitStuffer2;
  numBytes += bitStuffer2.ComputeNumBytesNeededSimple((unsigned int)(i1 - i0), (unsigned int)maxLen);    // code lengths
  int numUInts = (((sum + 7) >> 3) + 3) >> 2;
  numBytes += 4 * numUInts;    // byte array with the codes bit stuffed

  return true;
}

// -------------------------------------------------------------------------- ;

bool Huffman::GetRange(int& i0, int& i1, int& maxCodeLength) const
{
  if (m_codeTable.empty() || m_codeTable.size() >= m_maxHistoSize)
    return false;

  // first, check for peak somewhere in the middle with 0 stretches left and right
  int size = (int)m_codeTable.size();
  int i = 0;
  while (i < size && m_codeTable[i].first == 0) i++;
  i0 = i;
  i = size - 1;
  while (i >= 0 && m_codeTable[i].first == 0) i--;
  i1 = i + 1;    // exclusive

  if (i1 <= i0)
    return false;

  // second, cover the common case that the peak is close to 0
  pair<int, int> segm(0, 0);
  int j = 0;
  while (j < size)    // find the largest stretch of 0's, if any
  {
    // FIXME? is the type of first (short) appropriate ? Or shouldn't that
    // check be moved elsewhere
    if( m_codeTable[j].first < 0 ) // avoids infinite loop
      return false;
    while (j < size && m_codeTable[j].first > 0) j++;
    int k0 = j;
    while (j < size && m_codeTable[j].first == 0) j++;
    int k1 = j;

    if (k1 - k0 > segm.second)
      segm = pair<int, int>(k0, k1 - k0);
  }

  if (size - segm.second < i1 - i0)
  {
    i0 = segm.first + segm.second;
    i1 = segm.first + size;    // do wrap around
  }

  if (i1 <= i0)
    return false;

  int maxLen = 0;
  for (i = i0; i < i1; i++)
  {
    int k = GetIndexWrapAround(i, size);
    int len = m_codeTable[k].first;
    maxLen = max(maxLen, len);
  }

  if (maxLen <= 0 || maxLen > 32)
    return false;

  maxCodeLength = maxLen;
  return true;
}

// -------------------------------------------------------------------------- ;

bool Huffman::BitStuffCodes(Byte** ppByte, int i0, int i1) const
{
  if (!ppByte)
    return false;

  unsigned int* arr = (unsigned int*)(*ppByte);
  unsigned int* dstPtr = arr;
  int size = (int)m_codeTable.size();
  int bitPos = 0;

  for (int i = i0; i < i1; i++)
  {
    int k = GetIndexWrapAround(i, size);
    int len = m_codeTable[k].first;
    if (len > 0)
    {
      unsigned int val = m_codeTable[k].second;

      if (32 - bitPos >= len)
      {
        if (bitPos == 0)
          Store(dstPtr, 0);

        Store(dstPtr, Load(dstPtr) | (val << (32 - bitPos - len)));
        bitPos += len;
        if (bitPos == 32)
        {
          bitPos = 0;
          dstPtr++;
        }
      }
      else
      {
        bitPos += len - 32;
        Store(dstPtr, Load(dstPtr) | (val >> bitPos));
        dstPtr ++;
        Store(dstPtr, val << (32 - bitPos));
      }
    }
  }

  size_t numUInts = dstPtr - arr + (bitPos > 0 ? 1 : 0);
  *ppByte += numUInts * sizeof(unsigned int);
  return true;
}

// -------------------------------------------------------------------------- ;

bool Huffman::BitUnStuffCodes(const Byte** ppByte, size_t& nRemainingBytesInOut, int i0, int i1)
{
  if (!ppByte || !(*ppByte))
    return false;

  size_t nRemainingBytes = nRemainingBytesInOut;
  const unsigned int* arr = (const unsigned int*)(*ppByte);
  const unsigned int* srcPtr = arr;
  int size = (int)m_codeTable.size();
  int bitPos = 0;

  for (int i = i0; i < i1; i++)
  {
    int k = GetIndexWrapAround(i, size);
    int len = m_codeTable[k].first;
    if (len > 0)
    {
      if( nRemainingBytes < sizeof(unsigned) )
      {
        LERC_BRKPNT();
        return false;
      }
      if( len > 32 )
      {
        LERC_BRKPNT();
        return false;
      }
      m_codeTable[k].second = (Load(srcPtr) << bitPos) >> (32 - len);

      if (32 - bitPos >= len)
      {
        bitPos += len;
        // cppcheck-suppress shiftTooManyBits
        if (bitPos == 32)
        {
          bitPos = 0;
          if( nRemainingBytes < sizeof(unsigned) )
          {
            LERC_BRKPNT();
            return false;
          }
          srcPtr++;
          nRemainingBytes -= sizeof(unsigned);
        }
      }
      else
      {
        bitPos += len - 32;
        if( nRemainingBytes < sizeof(unsigned) )
        {
           LERC_BRKPNT();
           return false;
        }
        srcPtr++;
        nRemainingBytes -= sizeof(unsigned);
        if( nRemainingBytes < sizeof(unsigned) )
        {
           LERC_BRKPNT();
           return false;
        }
        m_codeTable[k].second |= Load(srcPtr) >> (32 - bitPos);
      }
    }
  }

  size_t numUInts = srcPtr - arr + (bitPos > 0 ? 1 : 0);
  if( nRemainingBytesInOut < sizeof(unsigned) * numUInts )
  {
    LERC_BRKPNT();
    return false;
  }
  *ppByte += numUInts * sizeof(unsigned int);
  nRemainingBytesInOut -= numUInts * sizeof(unsigned int);
  return true;
}

// -------------------------------------------------------------------------- ;

struct MyLargerThanOp
{
  inline bool operator() (const pair<int, int>& p0, const pair<int, int>& p1)  { return p0.first > p1.first; }
};

// -------------------------------------------------------------------------- ;

bool Huffman::ConvertCodesToCanonical()
{
  // from the non canonical code book, create an array to be sorted in descending order:
  //   codeLength * tableSize - index

  int tableSize = (int)m_codeTable.size();
  vector<pair<int, int> > sortVec(tableSize, pair<int, int>(0,0));

  for (int i = 0; i < tableSize; i++)
    if (m_codeTable[i].first > 0)
      sortVec[i] = pair<int, int>(m_codeTable[i].first * tableSize - i, i);

  // sort descending
  std::sort(sortVec.begin(), sortVec.end(), MyLargerThanOp());

  // create canonical codes and assign to orig code table
  unsigned int codeCanonical = 0;
  int index = sortVec[0].second;
  short codeLen = m_codeTable[index].first;
  int i = 0;
  while (i < tableSize && sortVec[i].first > 0)
  {
    index = sortVec[i++].second;
    short delta = codeLen - m_codeTable[index].first;
    codeCanonical >>= delta;
    codeLen -= delta;
    m_codeTable[index].second = codeCanonical++;
  }

  return true;
}

// -------------------------------------------------------------------------- ;
NAMESPACE_LERC_END
