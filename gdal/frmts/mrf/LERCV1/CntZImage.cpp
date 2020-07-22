/*
Copyright 2015 - 2020 Esri

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
               Lucian Plesea
*/

#include "CntZImage.h"
#include <cmath>
#include <cfloat>
#include <string>
#include <cassert>
#include <algorithm>

using namespace std;

NAMESPACE_LERC_START

/** BitMaskV1 - Convenient and fast access to binary mask bits
* includes RLE compression and decompression, in BitMaskV1.cpp
*
*/

class BitMaskV1
{
public:
    BitMaskV1(int nCols, int nRows) : m_nRows(nRows), m_nCols(nCols)
    {
        bits.resize(Size(), 0);
    }

    Byte  IsValid(int k) const { return (bits[k >> 3] & Bit(k)) != 0; }
    int   Size() const { return (m_nCols * m_nRows - 1) / 8 + 1; }
    // max RLE compressed size is n + 4 + 2 * (n - 1) / 32767
    // Returns encoded size
    void Set(int k, bool v) { if (v) SetValid(k); else SetInvalid(k); }
    int RLEcompress(Byte* aRLE) const;
    // current encoded size
    int RLEsize() const;
    // Decompress a RLE bitmask, bitmask size should be already set
    // Returns false if input seems wrong
    bool RLEdecompress(const Byte* src, size_t n);

private:
    int m_nRows, m_nCols;
    std::vector<Byte> bits;
    static Byte  Bit(int k) { return static_cast<Byte>(0x80 >> (k & 7)); }
    void  SetValid(int k) { bits[k >> 3] |= Bit(k); }
    void  SetInvalid(int k) { bits[k >> 3] &= ~Bit(k); }

    // Disable assignment op, default and copy constructor
    BitMaskV1();
    BitMaskV1(const BitMaskV1& copy);
    BitMaskV1& operator=(const BitMaskV1& m);
};

struct BitStufferV1
{
    // these 2 functions do not allocate memory. Byte ptr is moved like a file pointer
    static bool write(Byte** ppByte, const std::vector<unsigned int>& dataVec);
    // dataVec is sized to max expected values. It is resized on return, based on read data
    static bool read(Byte** ppByte, size_t& sz, std::vector<unsigned int>& dataVec);
    static int numBytesUInt(unsigned int k) { return (k <= 0xff) ? 1 : (k <= 0xffff) ? 2 : 4; }
    static unsigned int numTailBytesNotNeeded(unsigned int numElem, int numBits) {
        numBits = (numElem * numBits) & 31;
        return (numBits == 0 || numBits > 24) ? 0 : (numBits > 16) ? 1 : (numBits > 8) ? 2 : 3;
    }
};

#define MAX_RUN 32767
#define MIN_RUN 5
// End of Transmission
#define EOT -(MAX_RUN + 1)

// Decode a RLE bitmask, size should be already set
// Returns false if input seems wrong
// Not safe if fed garbage !!!
// Zero size mask is fine, only checks the end marker
bool BitMaskV1::RLEdecompress(const Byte* src, size_t n) {
    Byte* dst = bits.data();
    int sz = Size();
    short int count;
    assert(src);

    // Read a low endian short int
#define READ_COUNT if (true) {if (n < 2) return false; count = *src++; count += (*src++ << 8);}
    while (sz > 0) { // One sequence per loop
        READ_COUNT;
        n -= 2;
        if (count < 0) { // negative count for repeats
            if (0 == n--)
                return false;
            Byte b = *src++;
            sz += count;
            if (sz < 0)
                return false;
            while (0 != count++)
                *dst++ = b;
        }
        else { // No repeats, count is positive
            if (sz < count || n < static_cast<size_t>(count))
                return false;
            sz -= count;
            n -= count;
            while (0 != count--)
                *dst++ = *src++;
        }
    }
    READ_COUNT;
    return (count == EOT);
}

// Encode helper function
// It returns how many times the byte at *s is repeated
// a value between 1 and min(max_count, MAX_RUN)
inline static int run_length(const Byte* s, int max_count)
{
    assert(max_count && s);
    if (max_count > MAX_RUN)
        max_count = MAX_RUN;
    const Byte c = *s++;
    for (int count = 1; count < max_count; count++)
        if (c != *s++)
            return count;
    return max_count;
}

//
// RLE compressed size is bound by n + 4 + 2 * (n - 1) / 32767
//
int BitMaskV1::RLEcompress(Byte* dst) const {
    assert(dst);
    const Byte* src = bits.data();  // Next input byte
    Byte* start = dst;
    int sz = Size(); // left to process
    Byte* pCnt = dst; // Pointer to current sequence count
    int oddrun = 0; // non-repeated byte count

// Store val as short low endian integer
#define WRITE_COUNT(val) if (true) { *pCnt++ = Byte(val & 0xff); *pCnt++ = Byte(val >> 8); }
// Flush an existing odd run
#define FLUSH if (oddrun) { WRITE_COUNT(oddrun); pCnt += oddrun; dst = pCnt + 2; oddrun = 0; }

    dst += 2; // Skip the space for the first count
    while (sz > 0) {
        int run = run_length(src, sz);
        if (run < MIN_RUN) { // Use one byte
            *dst++ = *src++;
            sz--;
            if (MAX_RUN == ++oddrun)
                FLUSH;
        }
        else { // Found a run
            FLUSH;
            WRITE_COUNT(-run);
            *pCnt++ = *src;
            src += run;
            sz -= run;
            // cppcheck-suppress redundantAssignment
            dst = pCnt + 2; // after the next marker
        }
    }
    // cppcheck-suppress uselessAssignmentPtrArg
    FLUSH;
    (void)oddrun;
    WRITE_COUNT(EOT); // End marker
    // return compressed output size
    return int(pCnt - start);
}

// calculate encoded size
int BitMaskV1::RLEsize() const {
    const Byte* src = bits.data(); // Next input byte
    int sz = Size(); // left to process
    int oddrun = 0; // current non-repeated byte count
    // Simulate an odd run flush
#define SIMFLUSH if (oddrun) { osz += oddrun + 2; oddrun = 0; }
    int osz = 2; // output size, start with size of end marker
    while (sz) {
        int run = run_length(src, sz);
        if (run < MIN_RUN) {
            src++;
            sz--;
            if (MAX_RUN == ++oddrun)
                SIMFLUSH;
        }
        else {
            SIMFLUSH;
            src += run;
            sz -= run;
            osz += 3; // Any run is 3 bytes
        }
    }
    return oddrun ? (osz + oddrun + 2) : osz;
}

// Lookup tables for number of bytes in float and int
static const Byte bits67[4] = { 0x80, 0x40, 0xc0, 0 };
static const Byte stib67[4] = { 4, 2, 1, 3 };

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
    // n is in {1, 2, 4}
    int n = numBytesUInt(numElements);
    // 0xc0 is invalid, will trigger an error
    **ppByte = static_cast<Byte>(numBits | bits67[n-1]);
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

static int numBytesFlt(float z)
{
    short s = (short)z;
    signed char c = static_cast<signed char>(s);
    return ((float)c == z) ? 1 : ((float)s == z) ? 2 : 4;
}

static bool writeFlt(Byte** ppByte, float z, int numBytes)
{
    Byte* ptr = *ppByte;
    switch (numBytes) {
    case 1:
        *ptr = static_cast<Byte>(z);
        break;
    case 2: {
        short s = static_cast<short>(z);
        memcpy(ptr, &s, 2);
    }
          break;
    case 4:
        memcpy(ptr, &z, 4);
        break;
    default:
        return false;
    }

    *ppByte = ptr + numBytes;
    return true;
}

static bool readFlt(Byte** ppByte, size_t& nRemainingBytes, float& z, int numBytes)
{
    if (nRemainingBytes < static_cast<size_t>(numBytes))
        return false;

    Byte* ptr = *ppByte;
    switch (numBytes) {
    case 1:
        z = static_cast<float>(*reinterpret_cast<char*>(ptr));
        break;
    case 2: {
        short s;
        memcpy(&s, ptr, 2);
        z = s;
    }
          break;
    case 4:
        memcpy(&z, ptr, 4);
        break;
    default:
        return false;
    }

    *ppByte = ptr + numBytes;
    nRemainingBytes -= numBytes;
    return true;
}

static unsigned int computeNumBytesNeededByStuffer(unsigned int numElem, unsigned int maxElem)
{
    int numBits = 0;
    while (maxElem >> numBits)
        numBits++;
    unsigned int numUInts = (numElem * numBits + 31) / 32;
    return 1 + BitStufferV1::numBytesUInt(numElem) + numUInts * sizeof(unsigned int) -
        BitStufferV1::numTailBytesNotNeeded(numElem, numBits);
}

static const int CNT_Z = 8;
static const int CNT_Z_VER = 11;
static const string sCntZImage("CntZImage "); // Includes a space

// -------------------------------------------------------------------------- ;
// computes the size of a CntZImage of any width and height, but all void / invalid,
// and then compressed

unsigned int CntZImage::computeNumBytesNeededToWriteVoidImage()
{
    unsigned int sz = (unsigned int)sCntZImage.size()
        + 4 * sizeof(int) + sizeof(double);

    // cnt part
    sz += 3 * sizeof(int) + sizeof(float);

    // z part, 1 is the empty Tile if all invalid
    sz += 3 * sizeof(int) + sizeof(float) + 1;
    return sz; // 67
}

unsigned int CntZImage::computeNumBytesNeededToWrite(double maxZError,
    bool onlyZPart,
    InfoFromComputeNumBytes& info) const
{
    unsigned int sz = (unsigned int)sCntZImage.size()
        + 4 * sizeof(int) + sizeof(double);

    int numBytesOpt;
    if (!onlyZPart) {
        float cntMin, cntMax;
        computeCntStats(cntMin, cntMax);

        if (cntMin == cntMax) { // cnt part is const
            numBytesOpt = 0;    // nothing else to encode
        }
        else {
            // binary mask, use fast RLE class
            BitMaskV1 bitMask(getWidth(), getHeight());
            // in case bitMask allocation fails
            if (!bitMask.Size())
                return 0;
            const CntZ* srcPtr = data();
            for (int k = 0; k < getSize(); k++, srcPtr++)
                bitMask.Set(k, srcPtr->cnt > 0);

            // determine numBytes needed to encode
            numBytesOpt = static_cast<int>(bitMask.RLEsize());
        }

        info.numTilesVertCnt = 0;
        info.numTilesHoriCnt = 0;
        info.numBytesCnt = numBytesOpt;
        info.maxCntInImg = cntMax;

        sz += 3 * sizeof(int) + sizeof(float) + numBytesOpt;
    }

    // z part
    int numTilesVert, numTilesHori;
    float maxValInImg;
    if (!findTiling(maxZError, numTilesVert, numTilesHori, numBytesOpt, maxValInImg))
        return 0;

    info.maxZError = maxZError;
    info.numTilesVertZ = numTilesVert;
    info.numTilesHoriZ = numTilesHori;
    info.numBytesZ = numBytesOpt;
    info.maxZInImg = maxValInImg;

    sz += 3 * sizeof(int) + sizeof(float) + numBytesOpt;
    return sz;
}

// -------------------------------------------------------------------------- ;
// if you change the file format, don't forget to update not only write and
// read functions, and the file version number, but also the computeNumBytes...
// and numBytes... functions

bool CntZImage::write(Byte** ppByte,
    double maxZError,
    bool useInfoFromPrevComputeNumBytes,
    bool onlyZPart) const
{
    if (getSize() == 0)
        return false;

    Byte* ptr = *ppByte;

    memcpy(ptr, sCntZImage.c_str(), sCntZImage.size());
    ptr += sCntZImage.length();

    int height = getHeight();
    int width = getWidth();
    memcpy(ptr, &CNT_Z_VER, sizeof(int));  ptr += sizeof(int);
    memcpy(ptr, &CNT_Z, sizeof(int));  ptr += sizeof(int);
    memcpy(ptr, &height, sizeof(int));  ptr += sizeof(int);
    memcpy(ptr, &width, sizeof(int));  ptr += sizeof(int);
    memcpy(ptr, &maxZError, sizeof(double));  ptr += sizeof(double);

    *ppByte = ptr;

    InfoFromComputeNumBytes info;
    memset(&info, 0, sizeof(InfoFromComputeNumBytes));

    if (useInfoFromPrevComputeNumBytes && (maxZError == m_infoFromComputeNumBytes.maxZError))
        info = m_infoFromComputeNumBytes;
    else if (0 == computeNumBytesNeededToWrite(maxZError, onlyZPart, info))
        return false;

    bool zPart = onlyZPart;
    do {
        int numTilesVert, numTilesHori, numBytesOpt, numBytesWritten = 0;
        float maxValInImg;

        if (!zPart) {
            numTilesVert = info.numTilesVertCnt;
            numTilesHori = info.numTilesHoriCnt;
            numBytesOpt = info.numBytesCnt;
            maxValInImg = info.maxCntInImg;
        }
        else {
            numTilesVert = info.numTilesVertZ;
            numTilesHori = info.numTilesHoriZ;
            numBytesOpt = info.numBytesZ;
            maxValInImg = info.maxZInImg;
        }

        ptr = *ppByte;
        memcpy(ptr, &numTilesVert, sizeof(int));  ptr += sizeof(int);
        memcpy(ptr, &numTilesHori, sizeof(int));  ptr += sizeof(int);
        memcpy(ptr, &numBytesOpt, sizeof(int));  ptr += sizeof(int);
        memcpy(ptr, &maxValInImg, sizeof(float));  ptr += sizeof(float);

        *ppByte = ptr;
        Byte* bArr = ptr;

        if (!zPart && numTilesVert == 0 && numTilesHori == 0) { // no tiling for cnt part
            if (numBytesOpt > 0) { // cnt part is binary mask, use fast RLE class
                // convert to bit mask
                BitMaskV1 bitMask(width, height);
                const CntZ* srcPtr = data();
                for (int k = 0; k < getSize(); k++, srcPtr++)
                    bitMask.Set(k, srcPtr->cnt > 0);
                // RLE encoding, update numBytesWritten
                numBytesWritten = static_cast<int>(bitMask.RLEcompress(bArr));
            }
        }
        else { // encode tiles to buffer, alwasy z part
            float maxVal;
            if (!writeTiles(maxZError, numTilesVert, numTilesHori,
                bArr, numBytesWritten, maxVal))
                return false;
        }

        if (numBytesWritten != numBytesOpt)
            return false;

        *ppByte += numBytesWritten;
        zPart = !zPart;
    } while (zPart);

    return true;
}

bool CntZImage::read(Byte** ppByte,
    size_t& nRemainingBytes,
    double maxZError,
    bool onlyZPart)
{
    size_t len = sCntZImage.length();
    if (nRemainingBytes < len)
        return false;

    string typeStr(reinterpret_cast<char *>(*ppByte), len);
    if (typeStr != sCntZImage)
        return false;
    *ppByte += len;
    nRemainingBytes -= len;

    int version = 0, type = 0;
    int width = 0, height = 0;
    double maxZErrorInFile = 0;

    static const size_t HDRSZ = 4 * sizeof(int) + sizeof(double);

    if (nRemainingBytes < HDRSZ)
        return false;
    {
        Byte* ptr = *ppByte;

        memcpy(&version, ptr, sizeof(int));  ptr += sizeof(int);
        memcpy(&type, ptr, sizeof(int));  ptr += sizeof(int);
        memcpy(&height, ptr, sizeof(int));  ptr += sizeof(int);
        memcpy(&width, ptr, sizeof(int));  ptr += sizeof(int);
        memcpy(&maxZErrorInFile, ptr, sizeof(double));  ptr += sizeof(double);

        *ppByte = ptr;
    }
    nRemainingBytes -= HDRSZ;

    if (version != CNT_Z_VER || type != CNT_Z)
        return false;

    if (width <= 0 || width > 20000 || height <= 0 || height > 20000)
        return false;
    // To avoid excessive memory allocation attempts, this is still 1.8GB!!
    if (width * height > 1800 * 1000 * 1000 / static_cast<int>(sizeof(CntZ)))
        return false;

    if (maxZErrorInFile > maxZError)
        return false;

    if (onlyZPart) { // Keep the buffer because it includes the mask
        // Check the matching size, otherwise this is an error
        if (width != getWidth() || height != getHeight())
            return false;
    }
    else {
        // Resize clears the buffer
        if (!resize(width, height))
            return false;
    }

    do {
        int numTilesVert = 0, numTilesHori = 0, numBytes = 0;
        float maxValInImg = 0;

        if (nRemainingBytes < 3 * sizeof(int) + sizeof(float))
            return false;
        {
            Byte* ptr = *ppByte;
            memcpy(&numTilesVert, ptr, sizeof(int));  ptr += sizeof(int);
            memcpy(&numTilesHori, ptr, sizeof(int));  ptr += sizeof(int);
            memcpy(&numBytes, ptr, sizeof(int));  ptr += sizeof(int);
            memcpy(&maxValInImg, ptr, sizeof(float));  ptr += sizeof(float);
            *ppByte = ptr;
        }
        nRemainingBytes -= 3 * sizeof(int) + sizeof(float);

        if (!onlyZPart) { // no tiling allowed for the cnt part
            if (numTilesVert != 0 && numTilesHori != 0)
                return false;
            if (numBytes == 0) {   // cnt part is const
                for (int i = 0; i < height; i++) {
                    for (int j = 0; j < width; j++) {
                        CntZ val = (*this)(i, j);
                        val.cnt = maxValInImg;
                        setPixel(i, j, val);
                    }
                }
            } else {// cnt part is binary mask, RLE compressed
                // Read bit mask
                BitMaskV1 bitMask(width, height);
                if (!bitMask.RLEdecompress(*ppByte, nRemainingBytes))
                    return false;

                for (int i = 0; i < height; i++) {
                    for (int j = 0; j < width; j++) {
                        CntZ val = (*this)(i, j);
                        val.cnt = bitMask.IsValid(i * width + j) ? 1.0f : 0.0f;
                        setPixel(i, j, val);
                    }
                }
            }
        }
        else {
            if (!readTiles(maxZErrorInFile, numTilesVert, numTilesHori, maxValInImg, *ppByte, nRemainingBytes))
                return false;
        }

        if (nRemainingBytes < static_cast<size_t>(numBytes))
            return false;
        *ppByte += numBytes;
        nRemainingBytes -= numBytes;
        onlyZPart = !onlyZPart;
    } while (onlyZPart);

    return true;
}

// -------------------------------------------------------------------------- ;

bool CntZImage::findTiling(double maxZError,
    int& numTilesVertA,
    int& numTilesHoriA,
    int& numBytesOptA,
    float& maxValInImgA) const
{
    const int tileWidthArr[] = { 8, 11, 15, 20, 32, 64 };
    const int numConfigs = 6;

    // first, do the entire image as 1 block
    numTilesVertA = 1;
    numTilesHoriA = 1;
    if (!writeTiles(maxZError, 1, 1, nullptr, numBytesOptA, maxValInImgA))
        return false;

    // if all is invalid so z part is empty, then we have to write the header only
    if (numBytesOptA == numBytesZTile(0, 0, 0, 0))
        return true;

    int numBytesPrev = 0;
    for (int k = 0; k < numConfigs; k++) {
        int tileWidth = tileWidthArr[k];
        int numTilesVert = static_cast<int>(getHeight() / tileWidth);
        int numTilesHori = static_cast<int>(getWidth() / tileWidth);

        if (numTilesVert * numTilesHori < 2)
            return true;

        int numBytes = 0;
        float maxVal;
        if (!writeTiles(maxZError, numTilesVert, numTilesHori, nullptr, numBytes, maxVal))
            return false;

        if (numBytes < numBytesOptA) {
            numTilesVertA = numTilesVert;
            numTilesHoriA = numTilesHori;
            numBytesOptA = numBytes;
        }

        // we stop once things get worse
        if (k > 0 && numBytes > numBytesPrev)
            return true;

        numBytesPrev = numBytes;
    }
    return true;
}

// -------------------------------------------------------------------------- ;

// if bArr is nullptr, it doesn't actually do the writing, only computes output values
bool CntZImage::writeTiles(double maxZError, int numTilesVert, int numTilesHori,
    Byte* bArr, int& numBytes, float& maxValInImg) const
{
    numBytes = 0;
    maxValInImg = -FLT_MAX;

    if (numTilesVert == 0)
        return false;
    for (int iTile = 0; iTile <= numTilesVert; iTile++) {
        int tileH = static_cast<int>(getHeight() / numTilesVert);
        int i0 = iTile * tileH;
        if (iTile == numTilesVert)
            tileH = getHeight() % numTilesVert;

        if (tileH == 0)
            continue;

        for (int jTile = 0; jTile <= numTilesHori; jTile++) {
            int tileW = static_cast<int>(getWidth() / numTilesHori);
            int j0 = jTile * tileW;
            if (jTile == numTilesHori)
                tileW = getWidth() % numTilesHori;

            if (tileW == 0)
                continue;

            float zMin = 0, zMax = 0;
            int numValidPixel = 0;

            bool rv = computeZStats(i0, i0 + tileH, j0, j0 + tileW, zMin, zMax, numValidPixel);
            if (!rv)
                return false;

            maxValInImg = max(maxValInImg, zMax);

            int numBytesNeeded = numBytesZTile(numValidPixel, zMin, zMax, maxZError);
            numBytes += numBytesNeeded;

            if (bArr) { // Skip the actual write
                int numBytesWritten = 0;
                rv = writeZTile(&bArr, numBytesWritten, i0, i0 + tileH, j0, j0 + tileW, numValidPixel, zMin, zMax, maxZError);
                if (!rv)
                    return false;
                if (numBytesWritten != numBytesNeeded)
                    return false;
            }
        }
    }

    return true;
}

// -------------------------------------------------------------------------- ;

bool CntZImage::readTiles(double maxZErrorInFile,
    int numTilesVert, int numTilesHori, float maxValInImg,
    Byte* bArr, size_t nRemainingBytes)
{
    if (numTilesVert == 0)
        return false;
    for (int iTile = 0; iTile <= numTilesVert; iTile++) {
        int tileH = static_cast<int>(getHeight() / numTilesVert);
        int i0 = iTile * tileH;
        if (iTile == numTilesVert)
            tileH = getHeight() % numTilesVert;

        if (tileH == 0)
            continue;

        for (int jTile = 0; jTile <= numTilesHori; jTile++) {
            int tileW = static_cast<int>(getWidth() / numTilesHori);
            int j0 = jTile * tileW;
            if (jTile == numTilesHori)
                tileW = getWidth() % numTilesHori;

            if (tileW == 0)
                continue;

            if (!readZTile(&bArr, nRemainingBytes, i0, i0 + tileH, j0, j0 + tileW, maxZErrorInFile, maxValInImg))
                return false;
        }
    }

    return true;
}

// -------------------------------------------------------------------------- ;

void CntZImage::computeCntStats(float& cntMin, float& cntMax) const
{
    auto v = data();
    cntMin = cntMax = v[0].cnt;
    for (int k = 0; k < getSize(); k++) {
        auto cnt = v[k].cnt;
        cntMin = min(cnt, cntMin);
        cntMax = max(cnt, cntMax);
        if (cntMin != cntMax)
            return;
    }
}

// -------------------------------------------------------------------------- ;

bool CntZImage::computeZStats(int i0, int i1, int j0, int j1,
    float& zMin, float& zMax, int& numValidPixel) const
{

    if (i0 < 0 || j0 < 0 || i1 > getHeight() || j1 > getWidth())
        return false;

    // determine z ranges
    zMin = FLT_MAX;
    zMax = -FLT_MAX;
    numValidPixel = 0;

    for (int i = i0; i < i1; i++) {
        for (int j = j0; j < j1; j++) {
            const CntZ &val = (*this)(i, j);
            if (val.cnt > 0) {  // cnt <= 0 means ignore z
                zMin = min(val.z, zMin);
                zMax = max(val.z, zMax);
                numValidPixel++;
            }
        }
    }

    if (!numValidPixel)
        zMin = zMax = 0;

    return true;
}


int CntZImage::numBytesZTile(int numValidPixel, float zMin, float zMax, double maxZError) const
{
    if (numValidPixel == 0 || (zMin == 0 && zMax == 0))
        return 1;
    if (maxZError == 0 || (double)(zMax - zMin) / (2 * maxZError) > (1 << 28))
        return(int)(1 + numValidPixel * sizeof(float));
    unsigned int maxElem = (unsigned int)((double)(zMax - zMin) / (2 * maxZError) + 0.5);
    return 1 + numBytesFlt(zMin) + (maxElem ? computeNumBytesNeededByStuffer(numValidPixel, maxElem) : 0);
}


bool CntZImage::writeZTile(Byte** ppByte, int& numBytes,
    int i0, int i1, int j0, int j1,
    int numValidPixel,
    float zMin, float zMax, double maxZError) const
{
    Byte* ptr = *ppByte;
    int cntPixel = 0;

    if (numValidPixel == 0 || (zMin == 0 && zMax == 0)) { // special cases
        *ptr++ = 2;    // set compression flag to 2 to mark tile as constant 0
        numBytes = 1;
        *ppByte = ptr;
        return true;
    }

    if (maxZError == 0 ||                                       // user asks lossless OR
        (double)(zMax - zMin) / (2 * maxZError) > (1 << 28)) {  // we'd need > 28 bit
        // write z's as flt arr uncompressed
        *ptr++ = 0;
        float* dstPtr = (float*)ptr;

        for (int i = i0; i < i1; i++) {
            for (int j = j0; j < j1; j++) {
                CntZ val = (*this)(i, j);
                if (val.cnt > 0) {
                    *dstPtr++ = val.z;
                    cntPixel++;
                }
            }
        }

        if (cntPixel != numValidPixel)
            return false;

        ptr += numValidPixel * sizeof(float);
    }
    else { // write z's as int arr bit stuffed
        Byte flag = 1;
        unsigned int maxElem = (unsigned int)((double)(zMax - zMin) / (2 * maxZError) + 0.5);
        if (maxElem == 0)
            flag = 3;    // set compression flag to 3 to mark tile as constant zMin

        int n = numBytesFlt(zMin); // n in { 1, 2, 4 }
        *ptr++ = (flag | bits67[n-1]);

        if (!writeFlt(&ptr, zMin, n))
            return false;

        if (maxElem > 0) {
            vector<unsigned int> odataVec;
            double scale = 1 / (2 * maxZError);

            for (int i = i0; i < i1; i++) {
                for (int j = j0; j < j1; j++) {
                    const CntZ &val = (*this)(i, j);
                    if (val.cnt > 0)
                        odataVec.push_back((unsigned int)((val.z - zMin) * scale + 0.5));
                }
            }

            if (odataVec.size() != static_cast<size_t>(numValidPixel))
                return false;

            if (!BitStufferV1::write(&ptr, odataVec))
                return false;
        }
    }

    numBytes = (int)(ptr - *ppByte);
    *ppByte = ptr;
    return true;
}


bool CntZImage::readZTile(Byte** ppByte, size_t& nRemainingBytesInOut,
    int i0, int i1, int j0, int j1,
    double maxZErrorInFile, float maxZInImg)
{
    size_t nRemainingBytes = nRemainingBytesInOut;
    Byte* ptr = *ppByte;
    int numPixel = 0;

    if (nRemainingBytes < 1)
        return false;
    Byte comprFlag = *ptr++;
    nRemainingBytes -= 1;
    // Used if bit-stuffed
    int n = stib67[comprFlag >> 6];
    comprFlag &= 63;

    if (comprFlag == 2) {
        // entire zTile is constant 0 (if valid or invalid doesn't matter)
        for (int i = i0; i < i1; i++) {
            for (int j = j0; j < j1; j++) {
                CntZ val = (*this)(i, j);
                val.z = 0;
                setPixel(i, j, val);
            }
        }

        *ppByte = ptr;
        nRemainingBytesInOut = nRemainingBytes;
        return true;
    }

    if (comprFlag > 3)
        return false;

    if (comprFlag == 0) {
        // read z's as flt arr uncompressed
        const float* srcPtr = (const float*)ptr;
        for (int i = i0; i < i1; i++) {
            for (int j = j0; j < j1; j++) {
                CntZ val = (*this)(i, j);
                if (val.cnt > 0) {
                    if (nRemainingBytes < sizeof(float))
                        return false;
                    val.z = *srcPtr++;
                    setPixel(i, j, val);
                    nRemainingBytes -= sizeof(float);
                    numPixel++;
                }
            }
        }
        ptr += numPixel * sizeof(float);
    }
    else {
        // read z's as int arr bit stuffed
        float offset = 0;
        if (!readFlt(&ptr, nRemainingBytes, offset, n))
            return false;

        if (comprFlag == 3) {
            for (int i = i0; i < i1; i++) {
                for (int j = j0; j < j1; j++) {
                    CntZ val = (*this)(i, j);
                    if (val.cnt > 0) {
                        val.z = offset;
                        setPixel(i, j, val);
                    }
                }
            }
        }
        else {
            size_t nMaxElts =
                static_cast<size_t>(i1 - i0) * static_cast<size_t>(j1 - j0);
            dataVec.resize(nMaxElts);
            if (!BitStufferV1::read(&ptr, nRemainingBytes, dataVec))
                return false;

            double invScale = 2 * maxZErrorInFile;
            size_t nDataVecIdx = 0;

            for (int i = i0; i < i1; i++) {
                for (int j = j0; j < j1; j++) {
                    CntZ val = (*this)(i, j);
                    if (val.cnt > 0) {
                        if (nDataVecIdx == dataVec.size())
                            return false;
                        val.z = (float)(offset + dataVec[nDataVecIdx++] * invScale);
                        if (val.z > maxZInImg)
                            val.z = maxZInImg;
                        setPixel(i, j, val);
                    }
                }
            }
        }
    }

    *ppByte = ptr;
    nRemainingBytesInOut = nRemainingBytes;
    return true;
}

NAMESPACE_LERC_END
