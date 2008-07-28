/******************************************************************************
 * $Id$
 *
 * Purpose:  Implementation of ReflectanceCalculator class. Calculate
 *           reflectance values from radiance, for visual bands.
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

#include "reflectancecalculator.h"
#include <cmath>
#include <cstdlib>
using namespace std;

#define M_PI        3.14159265358979323846

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

ReflectanceCalculator::ReflectanceCalculator(std::string sTimeStamp, double rRTOA)
: m_rRTOA(rRTOA)
{
  std::string sYear (sTimeStamp.substr(0, 4));
  std::string sMonth (sTimeStamp.substr(4, 2));
  std::string sDay (sTimeStamp.substr(6, 2));
  std::string sHours (sTimeStamp.substr(8, 2));
  std::string sMins (sTimeStamp.substr(10, 2));

  m_iYear = atoi(sYear.c_str());
  int iMonth = atoi(sMonth.c_str());
  m_iDay = atoi(sDay.c_str());
	for (int i = 1; i < iMonth; ++i)
		m_iDay += iDaysInMonth(i, m_iYear);
  int iHours = atoi(sHours.c_str());
  int iMins = atoi(sMins.c_str());

	m_rHours = iHours + iMins / 60.0;
}

ReflectanceCalculator::~ReflectanceCalculator()
{

}

double ReflectanceCalculator::rGetReflectance(double rRadiance, double rLat, double rLon) const
{
  double phi = rLat * M_PI / 180;
  double lam = rLon * M_PI / 180;
  double rSunDist = rSunDistance();
  double ReflectanceNumerator = rRadiance*rSunDist*rSunDist;
  double zenithAngle = rZenithAngle(phi, rDeclination(), rHourAngle(rLon));
  double ReflectanceDenominator = m_rRTOA*cos(zenithAngle*M_PI/180);
  double Reflectance = ReflectanceNumerator / ReflectanceDenominator;
  return Reflectance;
}

double ReflectanceCalculator::rZenithAngle(double phi, double rDeclin, double rHourAngle) const
{
  double rCosZen = (sin(phi) * sin(rDeclin) + cos(phi)
          * cos(rDeclin) * cos(rHourAngle));
  double zenithAngle = acos(rCosZen) * 180 / M_PI;
  return zenithAngle;
}

const double ReflectanceCalculator::rDeclination() const
{
  double rJulianDay = m_iDay - 1;
  double yearFraction = (rJulianDay + m_rHours / 24) / iDaysInYear(m_iYear);
  double T = 2 * M_PI * yearFraction;
  
  double declin = 0.006918 - 0.399912 * cos(T) + 0.070257 * sin(T)
          - 0.006758 * cos(2 * T) + 0.000907 * sin(2 * T)
          - 0.002697 * cos(3 * T) + 0.00148 * sin(3 * T);
  return declin;
}

double ReflectanceCalculator::rHourAngle(double rLon) const
{
	// In: rLon (in degrees)
	// Out: hourAngle (in radians)
  double rJulianDay = m_iDay - 1;
  double yearFraction = (rJulianDay + m_rHours / 24) / iDaysInYear(m_iYear);
  double T = 2 * M_PI * yearFraction;

  double EOT2 = 229.18 * (0.000075 + 0.001868 * cos(T)- 0.032077 * sin(T));
  double EOT3 = 229.18 * (- 0.014615 * cos(2 * T) - 0.040849 * sin(2 * T));
  double EOT = EOT2 + EOT3;
  double TimeOffset = EOT + (4. * rLon);
  // True solar time in minutes:
  double TrueSolarTime = m_rHours * 60 + TimeOffset;
  // Solar hour angle in degrees and in radians:
  double HaDegr = (TrueSolarTime / 4. - 180.);
  double hourAngle = HaDegr * M_PI / 180;
  return hourAngle;
}

const double ReflectanceCalculator::rSunDistance() const
{
  int iJulianDay = m_iDay - 1;
  double theta = 2*M_PI *(iJulianDay - 3) / 365.25;
	// rE0 is the inverse of the square of the sun-distance ratio
	double rE0 = 1.000110 + 0.034221*cos(theta)+0.00128*sin(theta) + 0.000719*cos(2*theta)+0.000077*sin(2*theta);
  // The calculated distance is expressed as a factor of the "average sun-distance" (on 1 Jan approx. 0.98, on 1 Jul approx. 1.01)
  return 1 / sqrt(rE0);
}

int ReflectanceCalculator::iDaysInYear(int iYear) const
{
  bool fLeapYear = iDaysInMonth(2, iYear) == 29;
  
  if (fLeapYear)
      return 366;
  else
      return 365;
}

int ReflectanceCalculator::iDaysInMonth(int iMonth, int iYear) const
{
  int iDays;

  if ((iMonth == 4) || (iMonth == 6) || (iMonth == 9) || (iMonth == 11))
    iDays = 30;
  else if (iMonth == 2)
  {
    iDays = 28;
    if (iYear % 100 == 0) // century year
    {
      if (iYear % 400 == 0) // century leap year
        ++iDays;
    }
    else
    {
      if (iYear % 4 == 0) // normal leap year
        ++iDays;
    }
  }
  else
    iDays = 31;

  return iDays;
}
