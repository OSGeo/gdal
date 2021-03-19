/******************************************************************************
 *
 * Project:  Marching square algorithm
 * Purpose:  Core algorithm implementation for contour line generation.
 * Author:   Oslandia <infos at oslandia dot com>
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
#ifndef MARCHING_SQUARE_LEVEL_GENERATOR_H
#define MARCHING_SQUARE_LEVEL_GENERATOR_H

#include <climits>
#include <vector>
#include <limits>
#include <cmath>
#include <cstdlib>
#include "utility.h"

namespace marching_squares {

template <class Iterator>
class Range
{
public:
    Range( Iterator b, Iterator e ) : begin_( b ), end_( e ) {}
    Iterator begin() const { return begin_; }
    Iterator end() const { return end_; }
private:
    Iterator begin_;
    Iterator end_;
};

template <typename LevelIterator>
class RangeIterator
{
public:
    RangeIterator( const LevelIterator& parent, int idx ) : parent_( parent ), idx_( idx ) {}
    // Warning: this is a "pseudo" iterator, since operator* returns a value, not
    // a reference. This means we cannot have operator->
    std::pair<int, double> operator*() const
    {
        return std::make_pair( idx_, parent_.level( idx_ ) );
    }
    bool operator!=( const RangeIterator& other ) const
    {
        return idx_ != other.idx_;
    }
    const RangeIterator& operator++()
    {
        idx_++;
        return *this;
    }
private:
    const LevelIterator& parent_;
    int idx_;
};

class FixedLevelRangeIterator
{
public:
    typedef RangeIterator<FixedLevelRangeIterator> Iterator;
    FixedLevelRangeIterator( const double* levels, size_t count, double maxLevel = Inf ) : levels_( levels ), count_( count ), maxLevel_( maxLevel )
    {
    }

    Range<Iterator> range( double min, double max ) const
    {
        if ( min > max )
            std::swap( min, max );
        size_t b = 0;
        for (; b != count_ && levels_[b] < fudge(levels_[b], min); b++);
        if ( min == max )
            return Range<Iterator>( Iterator( *this, int(b) ), Iterator( *this, int(b) ) );
        size_t e = b;
        for (; e != count_ && levels_[e] <= fudge(levels_[e], max); e++);
        return Range<Iterator>( Iterator( *this, int(b) ), Iterator( *this, int(e) ) );
    }

    double level( int idx ) const
    {
        if ( idx >= int(count_) )
            return maxLevel_;
        return levels_[size_t(idx)];
    }

private:
    const double* levels_;
    size_t count_;
    double maxLevel_;
};

struct TooManyLevelsException : public std::exception
{
    const char * what () const throw () override
    {
        return "Input values and/or interval settings would lead to too many levels";
    }
};

// Arbitrary threshold to avoid too much computation time and memory
// consumption
constexpr int knMAX_NUMBER_LEVELS = 100*1000;

struct IntervalLevelRangeIterator
{
    typedef RangeIterator<IntervalLevelRangeIterator> Iterator;

    // Construction by a offset and an interval
    IntervalLevelRangeIterator( double offset, double interval ): offset_( offset ), interval_( interval ) {}

    Range<Iterator> range( double min, double max ) const
    {
        if ( min > max )
            std::swap(min, max);

        // compute the min index, adjusted to the fudged value if needed
        double df_i1 = ceil((min - offset_) / interval_);
        if( !(df_i1 >= INT_MIN && df_i1 < INT_MAX) )
            throw TooManyLevelsException();
        int i1 = static_cast<int>(df_i1);
        double l1 = fudge( level( i1 ), min );
        if ( l1 > min )
        {
            df_i1 = ceil((l1 - offset_) / interval_);
            if( !(df_i1 >= INT_MIN && df_i1 < INT_MAX) )
                throw TooManyLevelsException();
            i1 = static_cast<int>(df_i1);
        }
        Iterator b( *this, i1 );

        if ( min == max )
            return Range<Iterator>( b, b );

        // compute the max index, adjusted to the fudged value if needed
        double df_i2 = floor((max - offset_) / interval_)+1;
        if( !(df_i2 >= INT_MIN && df_i2 < INT_MAX) )
            throw TooManyLevelsException();
        int i2 = static_cast<int>(df_i2);
        double l2 = fudge( level( i2 ), max );
        if ( l2 > max )
        {
            df_i2 = floor((l2 - offset_) / interval_)+1;
            if( !(df_i2 >= INT_MIN && df_i2 < INT_MAX) )
                throw TooManyLevelsException();
            i2 = static_cast<int>(df_i2);
        }
        Iterator e( *this, i2 );

        // Arbitrary threshold to avoid too much computation time and memory
        // consumption
        if( i2 > i1 + static_cast<double>(knMAX_NUMBER_LEVELS) )
            throw TooManyLevelsException();

        return Range<Iterator>( b, e );
    }

    double level( int idx ) const
    {
        return idx * interval_ + offset_;
    }

private:
    const double offset_;
    const double interval_;
};

class ExponentialLevelRangeIterator
{
public:
    typedef RangeIterator<ExponentialLevelRangeIterator> Iterator;
    ExponentialLevelRangeIterator( double base ) : base_( base ), base_ln_( std::log( base_ ) ) {}

    double level( int idx ) const
    {
        if ( idx <= 0 )
            return 0.0;
        return std::pow( base_, idx - 1);
    }

    Range<Iterator> range( double min, double max ) const
    {
        if ( min > max )
            std::swap(min, max);

        int i1 = index1( min );
        double l1 = fudge( level( i1 ), min );
        if ( l1 > min )
            i1 = index1(l1 );
        Iterator b( *this, i1 );

        if ( min == max )
            return Range<Iterator>( b, b );

        int i2 = index2( max );
        double l2 = fudge( level( i2 ), max );
        if ( l2 > max )
            i2 = index2( l2 );
        Iterator e( *this, i2 );

        // Arbitrary threshold to avoid too much computation time and memory
        // consumption
        if( i2 > i1 + static_cast<double>(knMAX_NUMBER_LEVELS) )
            throw TooManyLevelsException();

        return Range<Iterator>( b, e );
    }

private:
    int index1( double plevel ) const
    {
        if ( plevel < 1.0 )
            return 1;
        const double dfVal = ceil(std::log( plevel ) / base_ln_)+1;
        if( !(dfVal >= INT_MIN && dfVal < INT_MAX) )
            throw TooManyLevelsException();
        return static_cast<int>(dfVal);
    }
    int index2( double plevel ) const
    {
        if ( plevel < 1.0 )
            return 0;
        const double dfVal = floor(std::log( plevel ) / base_ln_)+1+1;
        if( !(dfVal >= INT_MIN && dfVal < INT_MAX) )
            throw TooManyLevelsException();
        return static_cast<int>(dfVal);
    }

    // exponentiation base
    const double base_;
    const double base_ln_;
};


}
#endif
