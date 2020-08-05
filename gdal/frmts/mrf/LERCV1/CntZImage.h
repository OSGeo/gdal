
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

#ifndef CNTZIMAGE_H
#define CNTZIMAGE_H

#include <cstring>
#include <vector>

#ifndef NAMESPACE_LERC1_START
#define NAMESPACE_LERC1_START namespace GDAL_Lerc1NS {
#define NAMESPACE_LERC1_END }
#define USING_NAMESPACE_LERC1 using namespace GDAL_Lerc1NS;
#endif

NAMESPACE_LERC1_START

typedef unsigned char Byte;

/** BitMaskV1 - Convenient and fast access to binary mask bits
* includes RLE compression and decompression
*
*/

class BitMaskV1
{
public:
    BitMaskV1(int nCols, int nRows) : m_nRows(nRows), m_nCols(nCols) {
        bits.resize(Size(), 0);
    }

    Byte  IsValid(int k) const { return (bits[k >> 3] & Bit(k)) != 0; }
    int   Size() const { return 1 + (m_nCols * m_nRows - 1) / 8; }
    void Set(int k, bool v) { if (v) SetValid(k); else SetInvalid(k); }
    // max RLE compressed size is n + 4 + 2 * (n - 1) / 32767
    // Returns encoded size
    int RLEcompress(Byte* aRLE) const;
    // current encoded size
    int RLEsize() const;
    // Decompress a RLE bitmask, bitmask size should be already set
    // Returns false if input seems wrong
    bool RLEdecompress(const Byte* src, size_t sz);

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

template<typename T > class TImage
{
public:
    TImage() : width_(0), height_(0) {}
    ~TImage() {}

    bool resize(int width, int height) {
        if (width <= 0 || height <= 0)
            return false;
        width_ = width;
        height_ = height;
        values.resize(getSize());
        std::memset(values.data(), 0, values.size() * sizeof(T));
        return true;
    }

    int getWidth() const { return width_; }
    int getHeight() const { return height_; }
    int getSize() const { return width_ * height_; }

    const T& operator() (int row, int col) const { return values[row * width_ + col]; }
    void setPixel(int row, int col, T value) { values[row * width_ + col] = value; }
    const T* data() const { return values.data(); }

private:
    int width_, height_;
    std::vector<T> values;
};

 // cnt is a mask, > 0 if valid;
struct CntZ {
    float cnt, z;
};

class CntZImage : public TImage<CntZ>
{
public:
    /// binary file IO with optional compression
    /// (maxZError = 0  means no lossy compression for Z; the Cnt part is compressed lossless or not at all)
    /// read succeeds only if maxZError on file <= maxZError requested (!)

    CntZImage() {}
    ~CntZImage() {}

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
        InfoFromComputeNumBytes() {
            std::memset(this, 0, sizeof(*this));
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

    void computeCntStats(float& cntMin, float& cntMax) const; // Across the whole image, always works
    bool computeZStats(int i0, int i1, int j0, int j1, float& zMin, float& zMax, int& numValidPixel) const;

    static int numBytesZTile(int numValidPixel, float zMin, float zMax, double maxZError);

    bool writeZTile(Byte** ppByte, int& numBytes, int i0, int i1, int j0, int j1,
        int numValidPixel, float zMin, float zMax, double maxZError) const;

    bool readZTile(Byte** ppByte, size_t& nRemainingBytes, int i0, int i1, int j0, int j1, double maxZErrorInFile, float maxZInImg);

    InfoFromComputeNumBytes m_infoFromComputeNumBytes;
    std::vector<unsigned int> dataVec;    // temporary buffer, reused in readZTile
};

// -------------------------------------------------------------------------- ;

NAMESPACE_LERC1_END
#endif
