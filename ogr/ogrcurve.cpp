/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  The OGRCurve geometry class. 
 * Author:   Frank Warmerdam, warmerda@home.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
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

#include "ogr_geometry.h"
#include "ogr_p.h"

CPL_CVSID("$Id$");

OGRCurve::OGRCurve()
{
}

OGRCurve::~OGRCurve()
{
}

/************************************************************************/
/*                            get_IsClosed()                            */
/************************************************************************/

/**
 * \brief Return TRUE if curve is closed.
 *
 * Tests if a curve is closed. A curve is closed if its start point is
 * equal to its end point.
 *
 * This method relates to the SFCOM ICurve::get_IsClosed() method.
 *
 * @return TRUE if closed, else FALSE.
 */

int OGRCurve::get_IsClosed() const

{
    OGRPoint            oStartPoint, oEndPoint;

    StartPoint( &oStartPoint );
    EndPoint( &oEndPoint );

    if( oStartPoint.getX() == oEndPoint.getX()
        && oStartPoint.getY() == oEndPoint.getY() )
    {
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

/**
 * \fn double OGRCurve::get_Length() const;
 *
 * \brief Returns the length of the curve.
 *
 * This method relates to the SFCOM ICurve::get_Length() method.
 *
 * @return the length of the curve, zero if the curve hasn't been
 * initialized.
 */

/**
 * \fn void OGRCurve::StartPoint( OGRPoint * poPoint ) const;
 *
 * \brief Return the curve start point.
 *
 * This method relates to the SF COM ICurve::get_StartPoint() method.
 *
 * @param poPoint the point to be assigned the start location.
 */

/**
 * \fn void OGRCurve::EndPoint( OGRPoint * poPoint ) const;
 *
 * \brief Return the curve end point.
 *
 * This method relates to the SF COM ICurve::get_EndPoint() method.
 *
 * @param poPoint the point to be assigned the end location.
 */

/**
 * \fn void OGRCurve::Value( double dfDistance, OGRPoint * poPoint ) const;
 *
 * \brief Fetch point at given distance along curve.
 *
 * This method relates to the SF COM ICurve::get_Value() method.
 *
 * @param dfDistance distance along the curve at which to sample position.
 *                   This distance should be between zero and get_Length()
 *                   for this curve.
 * @param poPoint the point to be assigned the curve position.
 */

