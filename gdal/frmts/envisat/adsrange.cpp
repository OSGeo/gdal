/******************************************************************************
 * $Id$
 *
 * Project:  APP ENVISAT Support
 * Purpose:  Detect range of ADS records matching the MDS records.
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

#include "adsrange.hpp"
#include "timedelta.hpp"

#include "cpl_string.h"

CPL_C_START
#include "EnvisatFile.h"
#include "records.h"
CPL_C_END

#include <cmath>

/* -------------------------------------------------------------------- */
/* 
 * data-set descriptor (private helper class) 
 */

class DataSet
{ 
  public:

    EnvisatFile & envfile ;
    int index ;
    int nrec ; 

    DataSet( EnvisatFile & envfile , int index ) : 
        envfile(envfile), index(index), nrec(0)
    { 
        EnvisatFile_GetDatasetInfo( &envfile, index, NULL, NULL, NULL,
                NULL , NULL, &nrec, NULL ) ;
    } 

    TimeDelta getMJD( int ridx ) 
    { 
        GUInt32 mjd[3] ; 

        if ( ridx < 0 ) ridx += nrec ; 
    
        EnvisatFile_ReadDatasetRecordChunk(&envfile,index,ridx,mjd,0,12) ;

        #define INT32(x)    ((GInt32)CPL_MSBWORD32(x)) 

        return TimeDelta( INT32(mjd[0]), INT32(mjd[1]), INT32(mjd[2]) ) ;

        #undef INT32 
    } 

} ; 

/* -------------------------------------------------------------------- */
/* 
 * constructor of the ADSRangeLastAfter object 
 *
 */ 

ADSRangeLastAfter::ADSRangeLastAfter( EnvisatFile & envfile, 
    int  ads_idx , int mds_idx, const TimeDelta & line_interval )
{ 
    int idx ; 
    TimeDelta t_mds , t_ads , t_ads_prev ; 

    /* abs.time tolerance */ 
    TimeDelta atol = line_interval * 0.5 ; 

    /* MDS and ADS descriptor classes */
    DataSet mds( envfile, mds_idx ) ; 
    DataSet ads( envfile, ads_idx ) ; 

    /* read the times of the first and last measurements */ 
    mjd_m_first = mds.getMJD(0) ;  /* MDJ time of the first MDS record */
    mjd_m_last  = mds.getMJD(-1) ;   /* MDJ time of the last MDS record */

    /* look-up the the first applicable ADSR */ 

    idx   = 0 ; 
    t_mds = mjd_m_first + atol ; /*time of the first MDSR + tolerance */ 
    t_ads  = ads.getMJD(0) ;     /*time of the first ADSR */
    t_ads_prev = t_ads ;         /* holds previous ADSR */

    if ( t_ads < t_mds ) 
    { 
        for( idx = 1 ; idx < ads.nrec ; ++idx )
        { 
            t_ads = ads.getMJD(idx) ; 
        
            if ( t_ads >= t_mds ) break ; 

            t_ads_prev = t_ads ; 
        } 
    } 
    
    /* store the first applicable ASDR */
    idx_first = idx - 1 ; /* sets -1 if no match */
    mjd_first = t_ads_prev ; /* set time of the first rec. if no match */

    /* look-up the the last applicable ADSR */ 

    idx   = ads.nrec-2 ; 
    t_mds = mjd_m_last - atol ;  /* time of the last MDSR - tolerance */ 
    t_ads  = ads.getMJD(-1) ;    /* time of the first ADSR */
    t_ads_prev = t_ads ;         /* holds previous ADSR */

    if ( t_ads > t_mds ) 
    { 
        for( idx = ads.nrec-2 ; idx >= 0 ; --idx )
        { 
            t_ads = ads.getMJD(idx) ; 
        
            if ( t_ads <= t_mds ) break ; 

            t_ads_prev = t_ads ; 
        } 
    } 
   
    /* store the last applicable ASDR */
    idx_last = idx + 1 ; /* sets ads.nrec if no match */
    mjd_last = t_ads_prev ; /* set time of the last rec. if no match */

    /* valuate the line offsets */
    off_first = round( ( mjd_m_first - mjd_first ) / line_interval ) ; 
    off_last  = round( ( mjd_last  - mjd_m_last  ) / line_interval ) ; 

} ;

