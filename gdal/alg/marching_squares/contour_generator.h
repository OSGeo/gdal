/******************************************************************************
 *
 * Project:  Marching squares
 * Purpose:  Classes for contour generation
 * Author:   Hugo Mercier, <hugo dot mercier at oslandia dot com>
 *
 ******************************************************************************
 * Copyright (c) 2018, Oslandia <infos at oslandia dot com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#ifndef MARCHING_SQUARES_CONTOUR_GENERATOR_H
#define MARCHING_SQUARES_CONTOUR_GENERATOR_H

#include <vector>
#include <algorithm>

#include "gdal.h"

#include "utility.h"
#include "point.h"
#include "square.h"

namespace marching_squares
{

template <typename ContourWriter, typename LevelGenerator>
class ContourGenerator
{
public:
    ContourGenerator( size_t width, size_t height,
                      bool hasNoData, double noDataValue,
                      ContourWriter& writer, LevelGenerator& levelGenerator )
        : width_( width )
        , height_( height )
        , hasNoData_( hasNoData )
        , noDataValue_( noDataValue )
        , previousLine_()
        , writer_( writer )
        , levelGenerator_( levelGenerator )
    {
        previousLine_.resize( width_ );
        std::fill( previousLine_.begin(), previousLine_.end(), NaN );
    }
    CPLErr feedLine( const double* line )
    {
        if ( lineIdx_ <= height_ )
        {
            feedLine_( line );
            if ( lineIdx_ == height_ ) {
                // last line
                feedLine_( nullptr );
            }
        }
        return CE_None;
    }
private:
    size_t width_;
    size_t height_;
    bool hasNoData_;
    double noDataValue_;

    size_t lineIdx_ = 0;

    std::vector<double> previousLine_;

    ContourWriter& writer_;
    LevelGenerator& levelGenerator_;

    class ExtendedLine
    {
    public:
        ExtendedLine( const double* line, size_t size, bool hasNoData, double noDataValue )
            : line_( line )
            , size_( size )
            , hasNoData_( hasNoData )
            , noDataValue_( noDataValue )
        {}

        double value( int idx ) const
        {
            if ( line_ == nullptr )
                return NaN;
            if ( idx < 0 || idx >= int(size_) )
                return NaN;
            double v = line_[idx];
            if ( hasNoData_ && v == noDataValue_ )
                return NaN;
            return v;
        }
    private:
        const double* line_;
        size_t size_;
        bool hasNoData_;
        double noDataValue_;
    };
    void feedLine_( const double* line )
    {
        writer_.beginningOfLine();

        ExtendedLine previous( &previousLine_[0], width_, hasNoData_, noDataValue_ );
        ExtendedLine current( line, width_, hasNoData_, noDataValue_ );
        for ( int colIdx = -1; colIdx < int(width_); colIdx++ )
        {
            const ValuedPoint upperLeft(colIdx + 1 - .5, lineIdx_ - .5, previous.value( colIdx ));
            const ValuedPoint upperRight(colIdx + 1 + .5, lineIdx_ - .5, previous.value( colIdx+1 ));
            const ValuedPoint lowerLeft(colIdx + 1 - .5, lineIdx_ + .5, current.value( colIdx ));
            const ValuedPoint lowerRight(colIdx + 1 + .5, lineIdx_ + .5, current.value( colIdx+1 ));

            Square(upperLeft, upperRight, lowerLeft, lowerRight).process(levelGenerator_, writer_);
        }
        if ( line != nullptr )
            std::copy( line, line + width_, previousLine_.begin() );
        lineIdx_++;

        writer_.endOfLine();
    }
};

template <typename ContourWriter, typename LevelGenerator>
inline
ContourGenerator<ContourWriter, LevelGenerator>* newContourGenerator( size_t width, size_t height,
                                                                      bool hasNoData, double noDataValue,
                                                                      ContourWriter& writer, LevelGenerator& levelGenerator )
{
    return new ContourGenerator<ContourWriter, LevelGenerator>( width, height, hasNoData, noDataValue, writer, levelGenerator );
}

template <typename ContourWriter, typename LevelGenerator>
class ContourGeneratorFromRaster : public ContourGenerator<ContourWriter, LevelGenerator>
{
public:
    ContourGeneratorFromRaster( const GDALRasterBandH band,
                                bool hasNoData, double noDataValue,
                                ContourWriter& writer, LevelGenerator& levelGenerator )
        : ContourGenerator<ContourWriter, LevelGenerator>( GDALGetRasterBandXSize( band ),
                                                           GDALGetRasterBandYSize( band ),
                                                           hasNoData, noDataValue,
                                                           writer, levelGenerator )
        , band_( band )
    {
    }

    bool process( GDALProgressFunc progressFunc = nullptr, void* progressData = nullptr )
    {
        size_t width = GDALGetRasterBandXSize( band_ );
        size_t height = GDALGetRasterBandYSize( band_ );
        std::vector<double> line;
        line.resize( width );

        for ( size_t lineIdx = 0; lineIdx < height; lineIdx++ )
        {
            if ( progressFunc && progressFunc( double(lineIdx) / height, "Processing line", progressData ) == FALSE )
                return false;
            
            CPLErr error = GDALRasterIO(band_, GF_Read, 0, int(lineIdx), int(width),
                                        1, &line[0], int(width), 1, GDT_Float64, 0, 0);
            if (error != CE_None)
            {
                CPLDebug("CONTOUR", "failed fetch %d %d", int(lineIdx), int(width));
                return false;
            }
            this->feedLine( &line[0] );
        }
        if ( progressFunc)
            progressFunc( 1.0, "", progressData );
        return true;
    }
private:
    const GDALRasterBandH band_;

    ContourGeneratorFromRaster( const ContourGeneratorFromRaster& ) = delete;
    ContourGeneratorFromRaster& operator=( const ContourGeneratorFromRaster& ) = delete;
};

}

#endif
