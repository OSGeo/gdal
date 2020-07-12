
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

#ifndef CNTZIMAGE_H
#define CNTZIMAGE_H

// ---- includes ------------------------------------------------------------ ;
#include "BitStufferV1.h"
#include <string>

NAMESPACE_LERC_START

template<typename T > class TImage
{
public:
    bool resize(int width, int height) {
        if (width <= 0 || height <= 0)
            return false;
        width_ = width;
        height_ = height;
        values.resize(size_t(width) * height);
        memset(values.data(), 0, values.size() * sizeof(T));
        return true;
    }

    int getWidth() const { return width_; }
    int getHeight() const { return height_; }
    int getSize() const { return width_ * height_; }

    bool isInside(int row, int col) const {
        return row >= 0 && row < height_&& col >= 0 && col < width_;
    }

    const T& operator() (int row, int col) const { return values[row * width_ + col]; }
    void setPixel(int row, int col, T value) { values[row * width_ + col] = value; }
    const T* data() const { return values.data(); }

protected:
    int width_, height_;
    std::vector<T> values;
};

// -------------------------------------------------------------------------- ;

/**     count / z image
 *
 *      count can also be a weight, therefore float;
 *      z can be elevation or intensity;
 */

struct CntZ
{
    float cnt, z;
};

class CntZImage : public TImage<CntZ>
{
public:
    /// binary file IO with optional compression
    /// (maxZError = 0  means no lossy compression for Z; the Cnt part is compressed lossless or not at all)
    /// read succeeds only if maxZError on file <= maxZError requested (!)

    unsigned int computeNumBytesNeededToWrite(double maxZError, bool onlyZPart = false) {
        return computeNumBytesNeededToWrite(maxZError, onlyZPart, m_infoFromComputeNumBytes);
    }

    static unsigned int computeNumBytesNeededToWriteVoidImage();

    /// these 2 do not allocate memory. Byte ptr is moved like a file pointer.
    bool write(Byte** ppByte,
        double maxZError = 0,
        bool useInfoFromPrevComputeNumBytes = false,
        bool onlyZPart = false) const;

    bool read(Byte** ppByte,
        size_t& nRemainingBytes,
        double maxZError,
        bool onlyZPart = false);

protected:

    struct InfoFromComputeNumBytes
    {
        double maxZError;
        int numTilesVertCnt;
        int numTilesHoriCnt;
        int numBytesCnt;
        float maxCntInImg;
        int numTilesVertZ;
        int numTilesHoriZ;
        int numBytesZ;
        float maxZInImg;
        struct InfoFromComputeNumBytes() {
            memset(this, 0, sizeof(*this));
        }
    };

    unsigned int computeNumBytesNeededToWrite(double maxZError, bool onlyZPart,
        InfoFromComputeNumBytes& info) const;

    bool findTiling(double maxZError, int& numTilesVert, int& numTilesHori,
        int& numBytesOpt, float& maxValInImg) const;

    bool writeTiles(double maxZError, int numTilesVert, int numTilesHori,
        Byte* bArr, int& numBytes, float& maxValInImg) const;

    bool readTiles(double maxZErrorInFile,
        int numTilesVert, int numTilesHori, float maxValInImg, Byte* bArr, size_t nRemainingBytes);

    bool computeCntStats(int i0, int i1, int j0, int j1, float& cntMin, float& cntMax) const;
    bool computeZStats(int i0, int i1, int j0, int j1, float& zMin, float& zMax, int& numValidPixel) const;

    int numBytesZTile(int numValidPixel, float zMin, float zMax, double maxZError) const;

    bool writeZTile(Byte** ppByte, int& numBytes, int i0, int i1, int j0, int j1,
        int numValidPixel, float zMin, float zMax, double maxZError) const;

    bool readZTile(Byte** ppByte, size_t& nRemainingBytes, int i0, int i1, int j0, int j1, double maxZErrorInFile, float maxZInImg);

    InfoFromComputeNumBytes m_infoFromComputeNumBytes;
    std::vector<unsigned int> m_tmpDataVec;    // used in read fcts
};

// -------------------------------------------------------------------------- ;

NAMESPACE_LERC_END
#endif
