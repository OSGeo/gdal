/******************************************************************************
 * $Id$
 *
 * Project:  Spheroid classes
 * Purpose:  Provide spheroid lookup table base classes.
 * Author:   Gillian Walter
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

/**********************************************************************/
/* ================================================================== */
/*          Spheroid definitions                                      */
/* ================================================================== */
/**********************************************************************/


/* Maximum number of expected spheroids */
# define MAX_RECOGNIZED_SPHEROIDS 256

class SpheroidItem 
{

public:
   SpheroidItem();
   ~SpheroidItem();

   char *spheroid_name;
   double equitorial_radius;
   double polar_radius;
   double inverse_flattening;

   void SetValuesByRadii(const char *spheroidname, double eq_radius, double p_radius);
   void SetValuesByEqRadiusAndInvFlattening(const char *spheroidname, double eq_radius, double inverseflattening);

};

class SpheroidList 
{

public:
  int num_spheroids;
  // Acceptable errors for radii, inverse flattening
  double epsilonR;
  double epsilonI;


  SpheroidItem spheroids[MAX_RECOGNIZED_SPHEROIDS];

  SpheroidList();
  ~SpheroidList();

  char* GetSpheroidNameByRadii( double eq_radius, double polar_radius );
  char* GetSpheroidNameByEqRadiusAndInvFlattening( double eq_radius, double inverse_flatting );

  int SpheroidInList( const char *spheroid_name );
  double GetSpheroidEqRadius( const char *spheroid_name );
  double GetSpheroidPolarRadius( const char *spheroid_name ); 
  double GetSpheroidInverseFlattening( const char *spheroid_name );

};

