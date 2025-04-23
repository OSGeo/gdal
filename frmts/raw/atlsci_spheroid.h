/******************************************************************************
 *
 * Project:  Spheroid classes
 * Purpose:  Provide spheroid lookup table base classes.
 * Author:   Gillian Walter
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_port.h"

/**********************************************************************/
/* ================================================================== */
/*          Spheroid definitions                                      */
/* ================================================================== */
/**********************************************************************/

class SpheroidItem
{
    CPL_DISALLOW_COPY_ASSIGN(SpheroidItem)

  public:
    SpheroidItem();
    ~SpheroidItem();

    char *spheroid_name;
    double equitorial_radius;  // TODO(schwehr): Spelling.
    double polar_radius;
    double inverse_flattening;

    void SetValuesByRadii(const char *spheroidname, double eq_radius,
                          double p_radius);
    void SetValuesByEqRadiusAndInvFlattening(const char *spheroidname,
                                             double eq_radius,
                                             double inverseflattening);
};

class SpheroidList
{
    CPL_DISALLOW_COPY_ASSIGN(SpheroidList)

  public:
    int num_spheroids;
    // Acceptable errors for radii, inverse flattening.
    double epsilonR;
    double epsilonI;

// Maximum number of expected spheroids.
#define MAX_RECOGNIZED_SPHEROIDS 256

    // TODO(schwehr): Make this a vector.
    SpheroidItem spheroids[MAX_RECOGNIZED_SPHEROIDS];

#undef MAX_RECOGNIZED_SPHEROIDS

    SpheroidList();
    ~SpheroidList();

    char *GetSpheroidNameByRadii(double eq_radius, double polar_radius);
    char *GetSpheroidNameByEqRadiusAndInvFlattening(double eq_radius,
                                                    double inverse_flatting);

    int SpheroidInList(const char *spheroid_name);
    double GetSpheroidEqRadius(const char *spheroid_name);
    double GetSpheroidPolarRadius(const char *spheroid_name);
    double GetSpheroidInverseFlattening(const char *spheroid_name);
};
