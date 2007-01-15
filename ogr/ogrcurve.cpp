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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.11  2005/04/06 20:43:00  fwarmerdam
 * fixed a variety of method signatures for documentation
 *
 * Revision 1.10  2004/02/21 15:36:14  warmerda
 * const correctness updates for geometry: bug 289
 *
 * Revision 1.9  2003/09/11 22:47:53  aamici
 * add class constructors and destructors where needed in order to
 * let the mingw/cygwin binutils produce sensible partially linked objet files
 * with 'ld -r'.
 *
 * Revision 1.8  2001/07/18 05:03:05  warmerda
 * added CPL_CVSID
 *
 * Revision 1.7  1999/11/18 19:02:19  warmerda
 * expanded tabs
 *
 * Revision 1.6  1999/05/31 15:01:59  warmerda
 * OGRCurve now an abstract base class with essentially no implementation.
 * Everything moved down to OGRLineString where it belongs.  Also documented
 * classes.
 *
 * Revision 1.5  1999/05/31 11:05:08  warmerda
 * added some documentation
 *
 * Revision 1.4  1999/05/20 14:35:44  warmerda
 * added support for well known text format
 *
 * Revision 1.3  1999/05/17 14:39:46  warmerda
 * Added various new methods for ICurve compatibility.
 *
 * Revision 1.2  1999/03/30 21:21:43  warmerda
 * added linearring/polygon support
 *
 * Revision 1.1  1999/03/29 21:21:10  warmerda
 * New
 *
 */

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
 * Return TRUE if curve is closed.
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
 * Returns the length of the curve.
 *
 * This method relates to the SFCOM ICurve::get_Length() method.
 *
 * @return the length of the curve, zero if the curve hasn't been
 * initialized.
 */

/**
 * \fn void OGRCurve::StartPoint( OGRPoint * poPoint ) const;
 *
 * Return the curve start point.
 *
 * This method relates to the SF COM ICurve::get_StartPoint() method.
 *
 * @param poPoint the point to be assigned the start location.
 */

/**
 * \fn void OGRCurve::EndPoint( OGRPoint * poPoint ) const;
 *
 * Return the curve end point.
 *
 * This method relates to the SF COM ICurve::get_EndPoint() method.
 *
 * @param poPoint the point to be assigned the end location.
 */

/**
 * \fn void OGRCurve::Value( double dfDistance, OGRPoint * poPoint ) const;
 *
 * Fetch point at given distance along curve.
 *
 * This method relates to the SF COM ICurve::get_Value() method.
 *
 * @param dfDistance distance along the curve at which to sample position.
 *                   This distance should be between zero and get_Length()
 *                   for this curve.
 * @param poPoint the point to be assigned the curve position.
 */

