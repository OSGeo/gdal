/******************************************************************************
 * $Id$
 *
 * Project:  APP ENVISAT Support
 * Purpose:  time difference class for handling of Envisat MJD time
 * Author:   Martin Paces, martin.paces@eox.at
 *
 ******************************************************************************
 * Copyright (c) 2013, EOX IT Services, GmbH
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
 *****************************************************************************/

#ifndef timedelta_hpp
#define timedelta_hpp
/*
 * TimeDelta class represents the time difference. It is used to
 * hold the Envisat MJD (Modified Julian Date - which is time
 * since 2000-01-01T00:00:00.000000Z)
 */

class TimeDelta
{

  private:

    int days ;     /* number of days */
    int secs ;     /* number of seconds since day start */
    int usecs ;    /* number of micro sec. since second start */

    /* SETTERS */

    /* set object using number of days, seconds and micro-seconds */
    inline void set( int daysIn , int secsIn , int usecsIn )
    {
        int tmp0 , tmp1 ;
        /* overflow check with proper handling of negative values */
        /* note that division and modulo for negative values is impl.dependent */

        secsIn += ( tmp0 = usecsIn>=0 ? usecsIn/1000000 : -1-((-usecsIn)/1000000) ) ;
        daysIn += ( tmp1 = secsIn>=0 ? secsIn/86400 : -1-((-secsIn)/86400) ) ;

        this->usecs = usecsIn - 1000000*tmp0 ;
        this->secs  = secsIn - 86400*tmp1 ;
        this->days  = daysIn ;
    }

    /* set object from floating point number of seconds */
    inline void fromSeconds( double secsIn )
    {
        int _days = (int)( secsIn / 86400 ) ;
        int _secs = (int)( secsIn - 86400*_days ) ;
        int _uscs = (int)(( secsIn - ((int)secsIn) )*1e6) ;

        this->set( _days , _secs , _uscs ) ;
    }

  public:

    /* CONSTRUCTORS */
    TimeDelta( void ) : days(0), secs(0), usecs(0) {}

    /* construct object using number of days, seconds and micro-seconds */
    TimeDelta( int daysIn , int secsIn , int usecsIn )
    {
        this->set( daysIn, secsIn, usecsIn ) ;
    }

    /* construct object from floating point number of seconds */
    explicit TimeDelta( double secsIn )
    {
        this->fromSeconds( secsIn ) ;
    }

    /* GETTERS */

    inline int getDays( void ) const
    {
        return this->days ;
    }

    inline int getSeconds( void ) const
    {
        return this->secs ;
    }

    inline int getMicroseconds( void ) const
    {
        return this->usecs ;
    }

    /* convert to seconds - can handle safely at least 250 years dif. */
    /*  ... before losing the microsecond precision */
    inline operator double( void ) const
    {
        return (this->days*86400.0) + this->secs + (this->usecs*1e-6) ;
    }

    /* OPERATORS */

    /* difference */
    inline TimeDelta operator -( const TimeDelta & that ) const
    {
        return TimeDelta( this->days - that.days, this->secs - that.secs,
                                this->usecs - that.usecs ) ;
    }

    /* addition */
    inline TimeDelta operator +( const TimeDelta & that ) const
    {
        return TimeDelta( this->days + that.days, this->secs + that.secs,
                                this->usecs + that.usecs ) ;
    }

    /* division */
    inline double operator /( const TimeDelta & that ) const
    {
        return ( (double)*this / (double)that ) ;
    }

    /* integer multiplication */
    inline TimeDelta operator *( const int i ) const
    {
        return TimeDelta( i*this->days, i*this->secs, i*this->usecs ) ;
    }

    /* float multiplication */
    inline TimeDelta operator *( const double f ) const
    {
        return TimeDelta( f * (double)*this ) ;
    }

    /* comparisons operators */

    inline bool operator ==( const TimeDelta & that ) const
    {
        return ( (this->usecs == that.usecs)&&(this->secs == that.secs)&&
                 (this->days == that.days) )  ;
    }


    inline bool operator >( const TimeDelta & that ) const
    {
        return  (this->days > that.days)
                ||(
                    (this->days == that.days)
                    &&(
                        (this->secs > that.secs)
                        ||(
                            (this->secs == that.secs)
                            &&(this->usecs > that.usecs)
                        )
                    )
                ) ;
    }

    inline bool operator <( const TimeDelta & that ) const
    {
        return  (this->days < that.days)
                ||(
                    (this->days == that.days)
                    &&(
                        (this->secs < that.secs)
                        ||(
                            (this->secs == that.secs)
                            &&(this->usecs < that.usecs)
                        )
                    )
                ) ;
    }

    inline bool operator !=( const TimeDelta & that ) const
    {
        return !( *this == that ) ;
    }

    inline bool operator >=( const TimeDelta & that ) const
    {
        return !( *this < that ) ;
    }

    inline bool operator <=( const TimeDelta & that ) const
    {
        return !( *this > that ) ;
    }

};

/*
#include <iostream>

std::ostream & operator<<( std::ostream & out, const TimeDelta & td )
{

    out << "TimeDelta(" << td.getDays()
        << "," << td.getSeconds()
        << "," << td.getMicroseconds() << ")" ;

    return out ;
}
*/

#endif /*timedelta_hpp*/
