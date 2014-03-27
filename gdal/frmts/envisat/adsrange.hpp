/******************************************************************************
 * $Id$
 *
 * Project:  APP ENVISAT Support
 * Purpose:  Detect range of ADS records matching the MDS records
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

#ifndef adsrange_hpp
#define adsrange_hpp

#include "cpl_string.h"

CPL_C_START
#include "EnvisatFile.h"
#include "records.h"
CPL_C_END

#include "timedelta.hpp"

/* -------------------------------------------------------------------- */
/*
 * class ADSRange 
 * 
 * Range of ADS record matching the range of the MDS records. 
 *
 */

class ADSRange 
{ 

  protected: 

    int idx_first ; /* index of the first matched ADSR */
    int idx_last ;  /* index of the last matched ADSR */
    int off_first ; /* num. of lines from 1st matched ADSR to 1st MDSR */
    int off_last ;  /* num. of lines from last MDSR to last matched ADSR*/

    TimeDelta mjd_first ;  /* MDJ time of the first matched ADS record */
    TimeDelta mjd_last ;   /* MDJ time of the last matched ADS record */
    TimeDelta mjd_m_first ;  /* MDJ time of the first MDS record */
    TimeDelta mjd_m_last ;   /* MDJ time of the last MDS record */

  public: 

    /* CONSTRUCTOR */ 
    ADSRange() :
        idx_first(0), idx_last(0), off_first(0), off_last(0),
        mjd_first(0), mjd_last(0), mjd_m_first(0), mjd_m_last(0) 
    { 
    } 

    ADSRange( const int idx_first, const int idx_last, 
        const int off_first, const int off_last, 
        const TimeDelta &mjd_first, const TimeDelta &mjd_last, 
        const TimeDelta &mjd_m_first, const TimeDelta &mjd_m_last ) :
        idx_first(idx_first), idx_last(idx_last), off_first(off_first),
        off_last(off_last), mjd_first(mjd_first), mjd_last(mjd_last),
        mjd_m_first(mjd_m_first), mjd_m_last(mjd_m_last) 
    { 
    } 
    
    /* get count of matched records */
    inline int getDSRCount( void ) const 
    { 
        return ( idx_last - idx_first + 1 ) ; 
    } 

    /* GETTERS */ 

    /* get index of the first matched ADS record */
    inline int getFirstIndex( void ) 
    { 
        return this->idx_first ; 
    } 

    /* get index of the last matched ADS record */
    inline int getLastIndex( void ) 
    { 
        return this->idx_last ; 
    } 

    /* get offset of the first matched ADS record */
    inline int getFirstOffset( void ) 
    { 
        return this->off_first ; 
    } 

    /* get offset of the last matched ADS record */
    inline int getLastOffset( void ) 
    { 
        return this->off_last ; 
    } 

    /* get MJD time of the first matched ADS record */
    inline TimeDelta getFirstTime( void ) 
    { 
        return this->mjd_first ; 
    } 

    /* get MJD time of the last matched ADS record */
    inline TimeDelta getLastTime( void ) 
    { 
        return this->mjd_last ; 
    } 

    /* get MJD time of the first MDS record */
    inline TimeDelta getMDSRFirstTime( void ) 
    { 
        return this->mjd_m_first ; 
    } 

    /* get MJD time of the last MDS record */
    inline TimeDelta getMDSRLastTime( void ) 
    { 
        return this->mjd_m_last ; 
    } 

} ;  


/* -------------------------------------------------------------------- */
/* 
 * NOTE: There are two kinds of ADS records: 
 *
 *  1) One ADS record appliable to all consequent MDS records until replaced 
 *     by another ADS record, i.e., last MDS records does no need to be 
 *     followed by an ADS record.
 *
 *  2) Two ADS records applicable to all MDS records between them 
 *     (e.g., tiepoints ADS), i.e., last MDS record should be followed 
 *     by an ADS rescord having the same or later time-stamp.  
 *  
 *  The type of the ADS afects the way how the ADS records corresponding 
 *  to a set of MDS records should be selected. 
 */


class ADSRangeLastAfter: public ADSRange
{ 

  public: 

    /* CONSTRUCTOR */ 
    ADSRangeLastAfter( EnvisatFile & envfile, int  ads_idx , int mds_idx,
            const TimeDelta & line_interval ) ; 

} ;  


#endif /*tiepointrange_hpp*/

