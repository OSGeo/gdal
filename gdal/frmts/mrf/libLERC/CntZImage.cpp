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
#include "BitMaskV1.h"
#include <cmath>
#include <cfloat>
#include <cstring>
#include <algorithm>

using namespace std;

NAMESPACE_LERC_START

static const int CNT_Z = 8;
static const int CNT_Z_VER = 11;
static const string sCntZImage("CntZImage "); // Includes a space

// -------------------------------------------------------------------------- ;

static int numBytesFlt(float z)
{
    short s = (short)z;
    signed char c = static_cast<signed char>(s);
    return ((float)c == z) ? 1 : ((float)s == z) ? 2 : 4;
}

// -------------------------------------------------------------------------- ;

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

// -------------------------------------------------------------------------- ;

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

// -------------------------------------------------------------------------- ;

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

// -------------------------------------------------------------------------- ;

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

        int n = numBytesFlt(zMin);
        int bits67 = (n == 4) ? 0 : 3 - n;
        flag |= bits67 << 6;
        *ptr++ = flag;

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
    int bits67 = comprFlag >> 6;
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
        int n = (bits67 == 0) ? 4 : 3 - bits67;
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
