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

#include "BitStufferV1.h"
#include <cstring>
#include <algorithm>

using namespace std;
NAMESPACE_LERC_START

// see the old stream IO functions below on how to call.
// if you change write(...) / read(...), don't forget to update computeNumBytesNeeded(...).

bool BitStufferV1::write(Byte** ppByte, const vector<unsigned int>& dataVec)
{
    if (!ppByte || dataVec.empty())
        return false;

    unsigned int maxElem = *max_element(dataVec.begin(), dataVec.end());
    int numBits = 0; // 0 to 23
    while (maxElem >> numBits)
        numBits++;
    unsigned int numElements = (unsigned int)dataVec.size();

    // use the upper 2 bits to encode the type used for numElements: Byte, ushort, or uint
    // n is 1, 2  or 4
    int n = numBytesUInt(numElements);
    const Byte bits67[5] = { 0xff, 0x80, 0x40, 0xff, 0 };
    **ppByte = static_cast<Byte>(numBits | bits67[n]);
    (*ppByte)++;
    memcpy(*ppByte, &numElements, n);
    *ppByte += n;
    if (numBits == 0)
        return true;

    int bits = 32; // Available
    unsigned int acc = 0;   // Accumulator
    for (unsigned int val : dataVec) {
        if (bits >= numBits) { // no accumulator overflow
            acc |= val << (bits - numBits);
            bits -= numBits;
        }
        else { // accum overflowing
            acc |= val >> (numBits - bits);
            memcpy(*ppByte, &acc, sizeof(acc));
            *ppByte += sizeof(acc);
            bits += 32 - numBits; // under 32
            acc = val << bits;
        }
    }

    // There are between 1 and 31 bits left to write
    int nbytes = 4;
    while (bits >= 8) {
        acc >>= 8;
        bits -= 8;
        nbytes--;
    }
    memcpy(*ppByte, &acc, nbytes);
    *ppByte += nbytes;
    return true;
}

// -------------------------------------------------------------------------- ;

bool BitStufferV1::read(Byte** ppByte, size_t& size, vector<unsigned int>& dataVec)
{
    if (!ppByte || !size)
        return false;

    Byte numBits = **ppByte;
    *ppByte += 1;
    size -= 1;

    int vbytes[4] = { 4, 2, 1, 0 };
    int n = vbytes[numBits >> 6];
    numBits &= 63;  // bits 0-5;
    if (numBits >= 32 || n == 0 || size < static_cast<size_t>(n))
        return false;

    unsigned int numElements = 0;
    memcpy(&numElements, *ppByte, n);
    *ppByte += n;
    size -= n;
    if (static_cast<size_t>(numElements) > dataVec.size())
        return false;
    dataVec.resize(numElements);
    if (numBits == 0) { // Nothing to read, all zeros
        dataVec.resize(0);
        dataVec.resize(numElements, 0);
        return true;
    }

    unsigned int numBytes = (numElements * numBits + 7) / 8;
    if (size < numBytes)
        return false;
    size -= numBytes;

    int bits = 0; // Available in accumulator, at the high end
    unsigned int acc = 0;
    for (unsigned int& val : dataVec) {
        if (bits >= numBits) { // Enough bits in accumulator
            val = acc >> (32 - numBits);
            acc <<= numBits;
            bits -= numBits;
            continue;
        }

        // Need to reload the accumulator
        val = 0;
        if (bits) {
            val = acc >> (32 - bits);
            val <<= (numBits - bits);
        }
        unsigned int nb = std::min(numBytes, 4u);
        switch (nb) {
        case 4:
            memcpy(&acc, *ppByte, 4);
            break;
        case 0:
            return false; // Need at least one byte
        default: // read just a few bytes in the high bytes of an int
            memcpy(reinterpret_cast<Byte*>(&acc) + (4 - nb), *ppByte, nb);
        }
        *ppByte += nb;
        numBytes -= nb;
        bits += 32 - numBits;
        val |= acc >> bits;
        acc <<= 32 - bits;
    }
    return numBytes == 0;
}

NAMESPACE_LERC_END
