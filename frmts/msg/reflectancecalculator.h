/******************************************************************************
 *
 * Purpose:  Interface of ReflectanceCalculator class. Calculate reflectance
 *           values from radiance, for visual bands.
 * Author:   Bas Retsios, retsios@itc.nl
 *
 ******************************************************************************
 * Copyright (c) 2004, ITC
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#if !defined(                                                                  \
    AFX_REFLECTANCECALCULATOR_H__C9960E01_2A1B_41F0_B903_7957F11618D2__INCLUDED_)
#define AFX_REFLECTANCECALCULATOR_H__C9960E01_2A1B_41F0_B903_7957F11618D2__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif  // _MSC_VER > 1000

#include <string>

class ReflectanceCalculator
{
  public:
    ReflectanceCalculator(const std::string &sTimeStamp, double rRTOA);
    virtual ~ReflectanceCalculator();
    double rGetReflectance(double rRadiance, double rLat, double rLon) const;

  private:
    static double rZenithAngle(double phi, double rDeclin, double rHourAngle);
    double rDeclination() const;
    double rHourAngle(double lam) const;
    double rSunDistance() const;
    static int iDaysInYear(int iYear);
    static int iDaysInMonth(int iMonth, int iYear);

    const double m_rRTOA;  // solar irradiance on Top of Atmosphere
    int m_iYear;           // e.g. 2005
    int m_iDay;            // 1-365/366
    double m_rHours;       // 0-24
};

#endif  // !defined(AFX_REFLECTANCECALCULATOR_H__C9960E01_2A1B_41F0_B903_7957F11618D2__INCLUDED_)
