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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.1  2006/02/13 18:18:53  pka
 * Interlis 2: Support for nested attributes
 * Interlis 2: Arc interpolation
 *
 * Revision 1.2  2005/08/06 22:21:53  pka
 * Area polygonizer added
 *
 * Revision 1.1  2005/07/08 22:10:57  pka
 * Initial import of OGR Interlis driver
 *
 */


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
  //r = sqrt((x-bx)*(x-bx)+(y-by)*(y-by));
}

double getPhi(OGRPoint *center, OGRPoint *pt) {
  double cx = center->getX(); double cy = center->getY();
  double px = pt->getX(); double py = pt->getY();
  double r = sqrt((cx-px)*(cx-px)+(cy-py)*(cy-py));
  double phi = acos((px-cx)/r);
  return (py>cy) ? phi : -phi;
}

void interpolateArc(OGRLineString* line, OGRPoint *ptStart, OGRPoint *ptOnArc, OGRPoint *ptEnd, double arcIncr) {
  OGRPoint *center = getARCCenter(ptStart, ptOnArc, ptEnd);

  double cx = center->getX(); double cy = center->getY();
  double px = ptOnArc->getX(); double py = ptOnArc->getY();
  double r = sqrt((cx-px)*(cx-px)+(cy-py)*(cy-py));

  double phiPtStart = getPhi(center, ptStart);
  double phiPtOnArc = getPhi(center, ptOnArc);
  double phiPtEnd = getPhi(center, ptEnd);

  int pointCnt = 0;
  double deltaPhi = phiPtOnArc - phiPtStart;
  if (deltaPhi < 0) deltaPhi += 2*PI;
  if (deltaPhi < PI) {
    if (phiPtEnd < phiPtStart) phiPtEnd += 2*PI;
    for (double angle = phiPtStart+arcIncr; angle<phiPtEnd; angle += arcIncr) {
      line->addPoint(center->getX()+r*cos(angle), center->getY()+r*sin(angle), 0);
      ++pointCnt;
    }
  } else {
    if (phiPtEnd > phiPtStart) phiPtStart += 2*PI;
    for (double angle = phiPtStart-arcIncr; angle>phiPtEnd; angle -= arcIncr) {
      line->addPoint(center->getX()+r*cos(angle), center->getY()+r*sin(angle), 0);
      ++pointCnt;
    }
  }
  if (pointCnt == 0) line->addPoint(ptOnArc);
  delete center;
}

