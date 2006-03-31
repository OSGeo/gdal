/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  The OGRSurface class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
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
 * Revision 1.3  2006/03/31 17:44:20  fwarmerdam
 * header updates
 *
 * Revision 1.2  2005/04/06 20:43:00  fwarmerdam
 * fixed a variety of method signatures for documentation
 *
 * Revision 1.1  1999/05/31 14:59:26  warmerda
 * New
 *
 */

#include "ogr_geometry.h"
#include "ogr_p.h"

/**
 * \fn double OGRSurface::get_Area() const;
 *
 * Get the area of the surface object.
 *
 * For polygons the area is computed as the area of the outer ring less
 * the area of all internal rings. 
 *
 * This method relates to the SFCOM ISurface::get_Area() method.
 *
 * @return the area of the feature in square units of the spatial reference
 * system in use.
 */

/**
 * \fn OGRErr OGRSurface::Centroid( OGRPoint * poPoint ) const;
 *
 * Compute and return centroid of surface.  The centroid is not necessarily
 * within the geometry.  
 *
 * This method relates to the SFCOM ISurface::get_Centroid() method.
 *
 * NOTE: Only implemented when GEOS included in build.
 *
 * @param poPoint point to be set with the centroid location.
 *
 * @return OGRERR_NONE if it succeeds or OGRERR_FAILURE otherwise. 
 */

/**
 * \fn OGRErr OGRSurface::PointOnSurface( OGRPoint * poPoint ) const;
 *
 * This method relates to the SFCOM ISurface::get_PointOnSurface() method.
 *
 * NOTE: Only implemented when GEOS included in build.
 *
 * @param poPoint point to be set with an internal point. 
 *
 * @return OGRERR_NONE if it succeeds or OGRERR_FAILURE otherwise. 
 */
