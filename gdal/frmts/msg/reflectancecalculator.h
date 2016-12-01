/******************************************************************************
 * $Id$
 *
 * Purpose:  Interface of ReflectanceCalculator class. Calculate reflectance
 *           values from radiance, for visual bands.
 * Author:   Bas Retsios, retsios@itc.nl
 *
 ******************************************************************************
 * Copyright (c) 2004, ITC
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
 ******************************************************************************/

#if !defined(AFX_REFLECTANCECALCULATOR_H__C9960E01_2A1B_41F0_B903_7957F11618D2__INCLUDED_)
#define AFX_REFLECTANCECALCULATOR_H__C9960E01_2A1B_41F0_B903_7957F11618D2__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include <string>

class ReflectanceCalculator
{
public:
    ReflectanceCalculator(std::string sTimeStamp, double rRTOA);
    virtual ~ReflectanceCalculator();
    double rGetReflectance(double rRadiance, double rLat, double rLon) const;
private:
  static double rZenithAngle(double phi, double rDeclin, double rHourAngle);
  double rDeclination() const;
  double rHourAngle(double lam) const;
  double rSunDistance() const;
  static int iDaysInYear(int iYear);
  static int iDaysInMonth(int iMonth, int iYear);

    const double m_rRTOA; // solar irradiance on Top of Atmosphere
    int m_iYear; // e.g. 2005
    int m_iDay; // 1-365/366
    double m_rHours; // 0-24
};

#endif // !defined(AFX_REFLECTANCECALCULATOR_H__C9960E01_2A1B_41F0_B903_7957F11618D2__INCLUDED_)
