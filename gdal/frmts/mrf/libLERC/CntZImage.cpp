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

#include "CntZImage.h"
#include "BitMaskV1.h"
#include <cmath>
#include <cfloat>
#include <cstring>
#include <algorithm>

using namespace std;

NAMESPACE_LERC_START

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
    unsigned int cnt = 0;

    CntZImage zImg;
    cnt += (unsigned int)zImg.getTypeString().length();    // "CntZImage ", 10 bytes
    cnt += 2 * sizeof(int);
    cnt += 2 * sizeof(int);
    cnt += 1 * sizeof(double);

    // cnt part
    cnt += 3 * sizeof(int);
    cnt += sizeof(float);

    // z part
    cnt += 3 * sizeof(int);
    cnt += sizeof(float);
    cnt += 1;

    return cnt;
}

// -------------------------------------------------------------------------- ;

unsigned int CntZImage::computeNumBytesNeededToWrite(double maxZError,
    bool onlyZPart,
    InfoFromComputeNumBytes& info) const
{
    unsigned int cnt = 0;

    cnt += (unsigned int)getTypeString().length();
    cnt += 2 * sizeof(int);
    cnt += 2 * sizeof(int);
    cnt += 1 * sizeof(double);

    int numTilesVert, numTilesHori, numBytesOpt;
    float maxValInImg;

    // cnt part first
    if (!onlyZPart) {
        float cntMin, cntMax;
        if (!computeCntStats(0, static_cast<int>(height_), 0, static_cast<int>(width_), cntMin, cntMax))
            return false;

        bool bCntsNoInt = true;
        numTilesVert = 0;    // no tiling
        numTilesHori = 0;
        maxValInImg = cntMax;

        if (cntMin == cntMax) { // cnt part is const
            bCntsNoInt = fabsf(cntMax - (int)(cntMax + 0.5f)) > 0.0001;
            numBytesOpt = 0;    // nothing else to encode
        }
        else
        {
            bCntsNoInt = cntsNoInt();
            if (!bCntsNoInt && cntMin == 0 && cntMax == 1) { // cnt part is binary mask, use fast RLE class
                // convert to bit mask
                BitMaskV1 bitMask(width_, height_);
                // in case bitMask allocation fails
                if (!bitMask.Size())
                    return 0;
                const CntZ* srcPtr = data();
                for (int k = 0; k < width_ * height_; k++, srcPtr++) {
                    if (srcPtr->cnt <= 0)
                        bitMask.SetInvalid(k);
                    else
                        bitMask.SetValid(k);
                }

                // determine numBytes needed to encode
                numBytesOpt = static_cast<int>(bitMask.RLEsize());
            }
            else {
                if (!findTiling(false, 0, bCntsNoInt, numTilesVert, numTilesHori, numBytesOpt, maxValInImg))
                    return 0;
            }
        }

        info.cntsNoInt = bCntsNoInt;
        info.numTilesVertCnt = numTilesVert;
        info.numTilesHoriCnt = numTilesHori;
        info.numBytesCnt = numBytesOpt;
        info.maxCntInImg = maxValInImg;

        cnt += 3 * sizeof(int);
        cnt += sizeof(float);
        cnt += numBytesOpt;
    }

    // z part second
    if (!findTiling(true, maxZError, false, numTilesVert, numTilesHori, numBytesOpt, maxValInImg))
        return 0;

    info.maxZError = maxZError;
    info.numTilesVertZ = numTilesVert;
    info.numTilesHoriZ = numTilesHori;
    info.numBytesZ = numBytesOpt;
    info.maxZInImg = maxValInImg;

    cnt += 3 * sizeof(int);
    cnt += sizeof(float);
    cnt += numBytesOpt;

    return cnt;
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
    const int version = 11;

    if (getSize() == 0)
        return false;

    Byte* ptr = *ppByte;

    memcpy(ptr, getTypeString().c_str(), getTypeString().length());
    ptr += getTypeString().length();

    memcpy(ptr, &version, sizeof(int));  ptr += sizeof(int);
    memcpy(ptr, &type_, sizeof(int));  ptr += sizeof(int);
    memcpy(ptr, &height_, sizeof(int));  ptr += sizeof(int);
    memcpy(ptr, &width_, sizeof(int));  ptr += sizeof(int);
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
        bool bCntsNoInt = false;
        int numTilesVert, numTilesHori, numBytesOpt, numBytesWritten = 0;
        float maxValInImg;

        if (!zPart) {
            bCntsNoInt = info.cntsNoInt;
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
                BitMaskV1 bitMask(width_, height_);
                const CntZ* srcPtr = data();
                for (int k = 0; k < width_ * height_; k++, srcPtr++) {
                    if (srcPtr->cnt <= 0)
                        bitMask.SetInvalid(k);
                    else
                        bitMask.SetValid(k);
                }
                // RLE encoding, update numBytesWritten
                numBytesWritten = static_cast<int>(bitMask.RLEcompress(bArr));
            }
        }
        else { // encode tiles to buffer
            float maxVal;
            if (!writeTiles(zPart, maxZError, bCntsNoInt, numTilesVert, numTilesHori,
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
    bool onlyHeader,
    bool onlyZPart)
{
    size_t len = getTypeString().length();
    string typeStr(len, '0');

    if (nRemainingBytes < len)
        return false;
    memcpy(&typeStr[0], *ppByte, len);
    *ppByte += len;
    nRemainingBytes -= len;

    if (typeStr != getTypeString())
        return false;

    int version = 0, type = 0;
    int width = 0, height = 0;
    double maxZErrorInFile = 0;

    if (nRemainingBytes < 4 * sizeof(int) + sizeof(double))
        return false;
    Byte* ptr = *ppByte;

    memcpy(&version, ptr, sizeof(int));  ptr += sizeof(int);
    memcpy(&type, ptr, sizeof(int));  ptr += sizeof(int);
    memcpy(&height, ptr, sizeof(int));  ptr += sizeof(int);
    memcpy(&width, ptr, sizeof(int));  ptr += sizeof(int);
    memcpy(&maxZErrorInFile, ptr, sizeof(double));  ptr += sizeof(double);

    *ppByte = ptr;
    nRemainingBytes -= 4 * sizeof(int) + sizeof(double);

    if (version != 11 || type != type_)
        return false;

    if (width <= 0 || width > 20000 || height <= 0 || height > 20000)
        return false;
    // To avoid excessive memory allocation attempts
    if (width * height > 1800 * 1000 * 1000 / static_cast<int>(sizeof(CntZ)))
        return false;

    if (maxZErrorInFile > maxZError)
        return false;


    if (onlyHeader) {
        width_ = width;
        height_ = height;
        return true;
    }

    if (!onlyZPart && !resize(width, height))
        return false;

    bool zPart = onlyZPart;
    do {
        int numTilesVert = 0, numTilesHori = 0, numBytes = 0;
        float maxValInImg = 0;

        if (nRemainingBytes < 3 * sizeof(int) + sizeof(float))
            return false;
        ptr = *ppByte;
        memcpy(&numTilesVert, ptr, sizeof(int));  ptr += sizeof(int);
        memcpy(&numTilesHori, ptr, sizeof(int));  ptr += sizeof(int);
        memcpy(&numBytes, ptr, sizeof(int));  ptr += sizeof(int);
        memcpy(&maxValInImg, ptr, sizeof(float));  ptr += sizeof(float);

        *ppByte = ptr;
        nRemainingBytes -= 3 * sizeof(int) + sizeof(float);
        Byte* bArr = ptr;

        if (!zPart && numTilesVert == 0 && numTilesHori == 0) { // no tiling for this cnt part
            if (numBytes == 0) {   // cnt part is const
                for (int i = 0; i < height_; i++) {
                    for (int j = 0; j < width_; j++) {
                        CntZ val = (*this)(i, j);
                        val.cnt = maxValInImg;
                        setPixel(i, j, val);
                    }
                }
            }

            if (numBytes > 0) {  // cnt part is binary mask, RLE compressed
                // Read bit mask
                BitMaskV1 bitMask(width_, height_);
                if (!bitMask.RLEdecompress(bArr, nRemainingBytes))
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
        else if (!readTiles(zPart, maxZErrorInFile, numTilesVert, numTilesHori, maxValInImg, bArr, nRemainingBytes)) {
            return false;
        }

        if (nRemainingBytes < static_cast<size_t>(numBytes))
            return false;
        *ppByte += numBytes;
        nRemainingBytes -= numBytes;
        zPart = !zPart;
    } while (zPart);

    m_tmpDataVec.clear();
    return true;
}

// -------------------------------------------------------------------------- ;

bool CntZImage::findTiling(bool zPart, double maxZError, bool cntsNoIntIn,
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
    if (!writeTiles(zPart, maxZError, cntsNoIntIn, 1, 1, nullptr, numBytesOptA, maxValInImgA))
        return false;

    // if cnt part is constant 1 (all valid), or 0 or -1 (all invalid),
    // or if all is invalid so z part is empty, then we have to write the header only
    if (numBytesOptA == (zPart ? numBytesZTile(0, 0, 0, 0) : numBytesCntTile(0, 0, 0, false)))
        return true;

    int numBytesPrev = 0;
    for (int k = 0; k < numConfigs; k++) {
        int tileWidth = tileWidthArr[k];
        int numTilesVert = static_cast<int>(height_ / tileWidth);
        int numTilesHori = static_cast<int>(width_ / tileWidth);

        if (numTilesVert * numTilesHori < 2)
            return true;

        int numBytes = 0;
        float maxVal;
        if (!writeTiles(zPart, maxZError, cntsNoIntIn, numTilesVert, numTilesHori, nullptr, numBytes, maxVal))
            return false;

        if (numBytes < numBytesOptA) {
            numTilesVertA = numTilesVert;
            numTilesHoriA = numTilesHori;
            numBytesOptA = numBytes;
        }

        // we stop once things get worse by further increasing the block size
        if (k > 0 && numBytes > numBytesPrev)
            return true;

        numBytesPrev = numBytes;
    }
    return true;
}

// -------------------------------------------------------------------------- ;

bool CntZImage::writeTiles(bool zPart, double maxZError, bool cntsNoIntIn,
    int numTilesVert, int numTilesHori,
    Byte* bArr, int& numBytes, float& maxValInImg) const
{
    Byte* ptr = bArr;
    numBytes = 0;
    maxValInImg = -FLT_MAX;

    if (numTilesVert == 0)
        return false;
    for (int iTile = 0; iTile <= numTilesVert; iTile++) {
        int tileH = static_cast<int>(height_ / numTilesVert);
        int i0 = iTile * tileH;
        if (iTile == numTilesVert)
            tileH = height_ % numTilesVert;

        if (tileH == 0)
            continue;

        for (int jTile = 0; jTile <= numTilesHori; jTile++) {
            int tileW = static_cast<int>(width_ / numTilesHori);
            int j0 = jTile * tileW;
            if (jTile == numTilesHori)
                tileW = width_ % numTilesHori;

            if (tileW == 0)
                continue;

            float cntMin = 0, cntMax = 0, zMin = 0, zMax = 0;
            int numValidPixel = 0;

            bool rv = zPart ? computeZStats(i0, i0 + tileH, j0, j0 + tileW, zMin, zMax, numValidPixel) :
                computeCntStats(i0, i0 + tileH, j0, j0 + tileW, cntMin, cntMax);
            if (!rv)
                return false;

            maxValInImg = max(maxValInImg, zPart ? zMax : cntMax);

            int numBytesNeeded = zPart ? numBytesZTile(numValidPixel, zMin, zMax, maxZError) :
                numBytesCntTile(tileH * tileW, cntMin, cntMax, cntsNoIntIn);
            numBytes += numBytesNeeded;

            if (bArr) {
                int numBytesWritten = 0;
                rv = zPart ? writeZTile(&ptr, numBytesWritten, i0, i0 + tileH, j0, j0 + tileW, numValidPixel, zMin, zMax, maxZError) :
                    writeCntTile(&ptr, numBytesWritten, i0, i0 + tileH, j0, j0 + tileW, cntMin, cntMax, cntsNoIntIn);
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

bool CntZImage::readTiles(bool zPart, double maxZErrorInFile,
    int numTilesVert, int numTilesHori, float maxValInImg,
    Byte* bArr, size_t nRemainingBytes)
{
    Byte* ptr = bArr;

    if (numTilesVert == 0)
        return false;
    for (int iTile = 0; iTile <= numTilesVert; iTile++) {
        int tileH = static_cast<int>(height_ / numTilesVert);
        int i0 = iTile * tileH;
        if (iTile == numTilesVert)
            tileH = height_ % numTilesVert;

        if (tileH == 0)
            continue;

        for (int jTile = 0; jTile <= numTilesHori; jTile++) {
            int tileW = static_cast<int>(width_ / numTilesHori);
            int j0 = jTile * tileW;
            if (jTile == numTilesHori)
                tileW = width_ % numTilesHori;

            if (tileW == 0)
                continue;

            bool rv = zPart ? readZTile(&ptr, nRemainingBytes, i0, i0 + tileH, j0, j0 + tileW, maxZErrorInFile, maxValInImg) :
                readCntTile(&ptr, nRemainingBytes, i0, i0 + tileH, j0, j0 + tileW);

            if (!rv)
                return false;
        }
    }

    return true;
}

// -------------------------------------------------------------------------- ;

bool CntZImage::cntsNoInt() const
{
    float cntMaxErr = 0;
    for (int i = 0; i < height_; i++) {
        for (int j = 0; j < width_; j++) {
            float cnt = (*this)(i, j).cnt;
            float cntErr = fabsf(cnt - (int)(cnt + 0.5f));
            cntMaxErr = max(cntErr, cntMaxErr);
        }
    }
    return (cntMaxErr > 0.0001);
}

// -------------------------------------------------------------------------- ;

bool CntZImage::computeCntStats(int i0, int i1, int j0, int j1,
    float& cntMin, float& cntMax) const
{
    if (i0 < 0 || j0 < 0 || i1 > height_ || j1 > width_)
        return false;

    // determine cnt ranges
    cntMin = FLT_MAX;
    cntMax = -FLT_MAX;

    for (int i = i0; i < i1; i++) {
        for (int j = j0; j < j1; j++) {
            float cnt = (*this)(i, j).cnt;
            cntMin = min(cnt, cntMin);
            cntMax = max(cnt, cntMax);
        }
    }

    return true;
}

// -------------------------------------------------------------------------- ;

bool CntZImage::computeZStats(int i0, int i1, int j0, int j1,
    float& zMin, float& zMax, int& numValidPixel) const
{

    if (i0 < 0 || j0 < 0 || i1 > height_ || j1 > width_)
        return false;

    // determine z ranges
    zMin = FLT_MAX;
    zMax = -FLT_MAX;
    numValidPixel = 0;

    for (int i = i0; i < i1; i++) {
        for (int j = j0; j < j1; j++) {
            CntZ val = (*this)(i, j);
            if (val.cnt > 0) {  // cnt <= 0 means ignore z
                zMin = min(val.z, zMin);
                zMax = max(val.z, zMax);
                numValidPixel++;
            }
        }
    }

    if (zMin > zMax)
        zMin = zMax = 0;

    return true;
}

// -------------------------------------------------------------------------- ;

int CntZImage::numBytesCntTile(int numPixel, float cntMin, float cntMax, bool cntsNoIntIn) const
{
    if (cntMin == cntMax && (cntMin == 0 || cntMin == -1 || cntMin == 1))
        return 1;

    if (cntsNoIntIn || (cntMax - cntMin) > (1 << 28)) {
        return(int)(1 + numPixel * sizeof(float));
    }
    else {
        unsigned int maxElem = (unsigned int)(cntMax - cntMin + 0.5f);
        return 1 + numBytesFlt(floorf(cntMin + 0.5f)) + computeNumBytesNeededByStuffer(numPixel, maxElem);
    }
}

// -------------------------------------------------------------------------- ;

int CntZImage::numBytesZTile(int numValidPixel, float zMin, float zMax, double maxZError) const
{
    if (numValidPixel == 0 || (zMin == 0 && zMax == 0))
        return 1;

    if (maxZError == 0 || (double)(zMax - zMin) / (2 * maxZError) > (1 << 28)) {
        return(int)(1 + numValidPixel * sizeof(float));
    }
    else {
        unsigned int maxElem = (unsigned int)((double)(zMax - zMin) / (2 * maxZError) + 0.5);
        return 1 + numBytesFlt(zMin) + (maxElem ? computeNumBytesNeededByStuffer(numValidPixel, maxElem) : 0);
    }
}

// -------------------------------------------------------------------------- ;

bool CntZImage::writeCntTile(Byte** ppByte, int& numBytes,
    int i0, int i1, int j0, int j1,
    float cntMin, float cntMax, bool cntsNoIntIn) const
{
    Byte* ptr = *ppByte;
    int numPixel = (i1 - i0) * (j1 - j0);

    if (cntMin == cntMax && (cntMin == 0 || cntMin == -1 || cntMin == 1))    // special case all constant 0, -1, or 1
    {
        if (cntMin == 0)
            *ptr++ = 2;
        else if (cntMin == -1)
            *ptr++ = 3;
        else if (cntMin == 1)
            *ptr++ = 4;

        numBytes = 1;
        *ppByte = ptr;
        return true;
    }

    if (cntsNoIntIn || cntMax - cntMin > (1 << 28)) {
        // write cnt's as flt arr uncompressed
        *ptr++ = 0;
        float* dstPtr = (float*)ptr;

        for (int i = i0; i < i1; i++)
            for (int j = j0; j < j1; j++)
                *dstPtr++ = (*this)(i, j).cnt;

        ptr += numPixel * sizeof(float);
    }
    else {
        // write cnt's as int arr bit stuffed
        Byte flag = 1;
        float offset = floorf(cntMin + 0.5f);
        int n = numBytesFlt(offset);
        int bits67 = (n == 4) ? 0 : 3 - n;
        flag |= bits67 << 6;

        *ptr++ = flag;

        if (!writeFlt(&ptr, offset, n))
            return false;

        vector<unsigned int> dataVec(numPixel, 0);
        unsigned int* dstPtr = &dataVec[0];
        for (int i = i0; i < i1; i++)
            for (int j = j0; j < j1; j++)
                *dstPtr++ = (int)((*this)(i, j).cnt - offset + 0.5f);

        if (!BitStufferV1::write(&ptr, dataVec))
            return false;
    }

    numBytes = (int)(ptr - *ppByte);
    *ppByte = ptr;
    return true;
}

// -------------------------------------------------------------------------- ;

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
    else {
        // write z's as int arr bit stuffed
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
            vector<unsigned int> dataVec(numValidPixel, 0);
            unsigned int* dstPtr = &dataVec[0];
            double scale = 1 / (2 * maxZError);

            for (int i = i0; i < i1; i++) {
                for (int j = j0; j < j1; j++) {
                    CntZ val = (*this)(i, j);
                    if (val.cnt > 0) {
                        *dstPtr++ = (unsigned int)((val.z - zMin) * scale + 0.5);
                        cntPixel++;
                    }
                }
            }

            if (cntPixel != numValidPixel)
                return false;

            if (!BitStufferV1::write(&ptr, dataVec))
                return false;
        }
    }

    numBytes = (int)(ptr - *ppByte);
    *ppByte = ptr;
    return true;
}

// -------------------------------------------------------------------------- ;

bool CntZImage::readCntTile(Byte** ppByte, size_t& nRemainingBytesInOut, int i0, int i1, int j0, int j1)
{
    size_t nRemainingBytes = nRemainingBytesInOut;
    Byte* ptr = *ppByte;
    int numPixel = (i1 - i0) * (j1 - j0);

    if (nRemainingBytes < 1)
        return false;
    Byte comprFlag = *ptr++;
    nRemainingBytes -= 1;

    if (comprFlag == 2) {  // entire tile is constant 0 (invalid)
        // here we depend on resize()
        *ppByte = ptr;
        nRemainingBytesInOut = nRemainingBytes;
        return true;
    }

    if (comprFlag == 3 || comprFlag == 4) {
        // entire tile is constant -1 (invalid) or 1 (valid)
        CntZ cz1m = { -1, 0 };
        CntZ cz1p = { 1, 0 };
        CntZ cz1 = (comprFlag == 3) ? cz1m : cz1p;

        for (int i = i0; i < i1; i++)
            for (int j = j0; j < j1; j++)
                setPixel(i, j, cz1);

        *ppByte = ptr;
        nRemainingBytesInOut = nRemainingBytes;
        return true;
    }

    if ((comprFlag & 63) > 4)
        return false;

    if (comprFlag == 0) {
        // read cnt's as flt arr uncompressed
        const float* srcPtr = (const float*)ptr;
        for (int i = i0; i < i1; i++) {
            for (int j = j0; j < j1; j++) {
                if (nRemainingBytes < sizeof(float))
                    return false;
                CntZ val = (*this)(i, j);
                val.cnt = *srcPtr++;
                setPixel(i, j, val);
                nRemainingBytes -= sizeof(float);
            }
        }

        ptr += numPixel * sizeof(float);
    }
    else {
        // read cnt's as int arr bit stuffed
        int bits67 = comprFlag >> 6;
        // cppcheck-suppress knownConditionTrueFalse
        int n = (bits67 == 0) ? 4 : 3 - bits67;

        float offset = 0;
        if (!readFlt(&ptr, nRemainingBytes, offset, n))
            return false;

        vector<unsigned int>& dataVec = m_tmpDataVec;
        size_t nMaxElts =
            static_cast<size_t>(i1 - i0) * static_cast<size_t>(j1 - j0);
        dataVec.resize(nMaxElts);
        if (!BitStufferV1::read(&ptr, nRemainingBytes, dataVec))
            return false;

        size_t dataVecIdx = 0;
        for (int i = i0; i < i1; i++) {
            for (int j = j0; j < j1; j++) {
                if (dataVecIdx == dataVec.size())
                    return false;
                CntZ val = (*this)(i, j);
                val.cnt = offset + dataVec[dataVecIdx++];
                setPixel(i, j, val);
            }
        }
    }

    *ppByte = ptr;
    nRemainingBytesInOut = nRemainingBytes;
    return true;
}

// -------------------------------------------------------------------------- ;

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
            vector<unsigned int>& dataVec = m_tmpDataVec;
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

// -------------------------------------------------------------------------- ;

int CntZImage::numBytesFlt(float z)
{
    short s = (short)z;
    signed char c = static_cast<signed char>(s);
    return ((float)c == z) ? 1 : ((float)s == z) ? 2 : 4;
}

// -------------------------------------------------------------------------- ;

bool CntZImage::writeFlt(Byte** ppByte, float z, int numBytes)
{
    Byte* ptr = *ppByte;

    if (numBytes == 1)
    {
        char c = (char)z;
        *((char*)ptr) = c;
    }
    else if (numBytes == 2)
    {
        short s = (short)z;
        *((short*)ptr) = s;
    }
    else if (numBytes == 4)
    {
        *((float*)ptr) = z;
    }
    else
        return false;

    *ppByte = ptr + numBytes;
    return true;
}

// -------------------------------------------------------------------------- ;

bool CntZImage::readFlt(Byte** ppByte, size_t& nRemainingBytes, float& z, int numBytes)
{
    Byte* ptr = *ppByte;

    if (numBytes == 1)
    {
        if (nRemainingBytes < static_cast<size_t>(numBytes))
            return false;
        char c = *((char*)ptr);
        z = c;
    }
    else if (numBytes == 2)
    {
        if (nRemainingBytes < static_cast<size_t>(numBytes))
            return false;
        short s = *((short*)ptr);
        z = s;
    }
    else if (numBytes == 4)
    {
        if (nRemainingBytes < static_cast<size_t>(numBytes))
            return false;
        z = *((float*)ptr);
    }
    else
        return false;

    *ppByte = ptr + numBytes;
    nRemainingBytes -= numBytes;
    return true;
}

// ADJUST goes LSB to MSB in bytes, regardless of endianness
// POFFSET is the relative byte offset for a given type, for native endian
#if defined BIG_ENDIAN
#define ADJUST(ptr) (--(ptr))
#define POFFSET(T) sizeof(T)
#else
#define ADJUST(ptr) ((ptr)++)
#define POFFSET(T) 0
#endif

#define NEXTBYTE (*(*ppByte)++)

// endianness and alignment safe
// returns the number of bytes it wrote, adjusts *ppByte
int CntZImage::writeVal(Byte** ppByte, float z, int numBytes)
{

    short s = (short)z;
    // Calculate numBytes if needed
    if (0 == numBytes)
        numBytes = (z != (float)s) ? 4 : (s != (signed char)s) ? 2 : 1;

    if (4 == numBytes) {
        // Store as floating point, 4 bytes in LSB order
        Byte* fp = (Byte*)&z + POFFSET(float);
        NEXTBYTE = *ADJUST(fp);
        NEXTBYTE = *ADJUST(fp);
        NEXTBYTE = *ADJUST(fp);
        NEXTBYTE = *ADJUST(fp);
        return 4;
    }

    // Int types, 2 or 1 bytes
    NEXTBYTE = (Byte)s; // Lower byte first
    if (2 == numBytes)
        NEXTBYTE = (Byte)(s >> 8); // Second byte
    return numBytes;
}

// endianness and alignment safe, not alliasing safe
void CntZImage::readVal(Byte** ppByte, float& val, int numBytes)
{
    // Floating point, read the 4 bytes in LSB first order
    if (4 == numBytes) {
        // cppcheck-suppress unreadVariable
        Byte* vp = ((Byte*)&val) + POFFSET(float);
        *ADJUST(vp) = NEXTBYTE;
        *ADJUST(vp) = NEXTBYTE;
        *ADJUST(vp) = NEXTBYTE;
        *ADJUST(vp) = NEXTBYTE;
        return;
    }

    int v = (int)((signed char)NEXTBYTE); // Low byte, signed extended
    if (2 == numBytes)
        v = (256 * (signed char)NEXTBYTE) | (v & 0xff);
    val = static_cast<float>(v);
}

NAMESPACE_LERC_END
