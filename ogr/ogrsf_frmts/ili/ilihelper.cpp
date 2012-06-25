/******************************************************************************
 * $Id$
 *
 * Project:  Interlis 1/2 Translator
 * Purpose:  Helper functions for Interlis reader
 * Author:   Pirmin Kalberer, Sourcepole AG
 *
 ******************************************************************************
 * Copyright (c) 2004, Pirmin Kalberer, Sourcepole AG
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


#include "ilihelper.h"


CPL_CVSID("$Id$");


OGRPoint *getARCCenter(OGRPoint *ptStart, OGRPoint *ptArc, OGRPoint *ptEnd) {
  // FIXME precision
  double bx = ptStart->getX(); double by = ptStart->getY();
  double cx = ptArc->getX(); double cy = ptArc->getY();
  double dx = ptEnd->getX(); double dy = ptEnd->getY();
  double temp, bc, cd, det, x, y;

  temp = cx*cx+cy*cy;
  bc = (bx*bx + by*by - temp)/2.0;
  cd = (temp - dx*dx - dy*dy)/2.0;
  det = (bx-cx)*(cy-dy)-(cx-dx)*(by-cy);

  OGRPoint *center = new OGRPoint();

  if (fabs(det) < 1.0e-6) { // could not determin the determinante: too small
    return center;
  }
  det = 1/det;
  x = (bc*(cy-dy)-cd*(by-cy))*det;
  y = ((bx-cx)*cd-(cx-dx)*bc)*det;

  center->setX(x);
  center->setY(y);

  return center;
}

void interpolateArc(OGRLineString* line, OGRPoint *ptStart, OGRPoint *ptOnArc, OGRPoint *ptEnd, double arcIncr) {
  OGRPoint *center = getARCCenter(ptStart, ptOnArc, ptEnd);

  double cx = center->getX(); double cy = center->getY();
  double px = ptOnArc->getX(); double py = ptOnArc->getY();
  double r = sqrt((cx-px)*(cx-px)+(cy-py)*(cy-py));

  //assure minimal chord length (0.002m???)
  double myAlpha = 2.0*acos(1.0-0.002/r);      
  if (myAlpha < arcIncr)  {
      arcIncr = myAlpha;
  }

  double a1 = atan2(ptStart->getY() - cy, ptStart->getX() - cx);
  double a2 = atan2(py - cy, px - cx);
  double a3 = atan2(ptEnd->getY() - cy, ptEnd->getX() - cx);

  double sweep;

  // Clockwise
  if(a1 > a2 && a2 > a3) {
    sweep = a3 - a1;
  }
  // Counter-clockwise
  else if(a1 < a2 && a2 < a3) {
    sweep = a3 - a1;
  }
  // Clockwise, wrap
  else if((a1 < a2 && a1 > a3) || (a2 < a3 && a1 > a3)) {
    sweep = a3 - a1 + 2*PI;
  }
  // Counter-clockwise, wrap
  else if((a1 > a2 && a1 < a3) || (a2 > a3 && a1 < a3)) {
    sweep = a3 - a1 - 2*PI;
  }
  else {
    sweep = 0.0;
  }

  int ptcount = ceil(fabs(sweep/arcIncr));

  if(sweep < 0) arcIncr *= -1.0;

  double angle = a1;

  for(int i = 0; i < ptcount - 1; i++) {
    angle += arcIncr;

    if(arcIncr > 0.0 && angle > PI) angle -= 2*PI;
    if(arcIncr < 0.0 && angle < -1*PI) angle -= 2*PI;

    double x = cx + r*cos(angle);
    double y = cy + r*sin(angle);
   
    line->addPoint(x, y, 0);

   
    if((angle < a2) && ((angle + arcIncr) > a2)) {
       line->addPoint(ptOnArc);
    }

    if((angle > a2) && ((angle + arcIncr) < a2)) {
       line->addPoint(ptOnArc);
    }
   
  }
  line->addPoint(ptEnd);
  delete center;
}

