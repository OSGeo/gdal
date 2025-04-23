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

#include "atlsci_spheroid.h"
#include "cpl_string.h"

/**********************************************************************/
/* ================================================================== */
/*          Spheroid definitions                                      */
/* ================================================================== */
/**********************************************************************/

SpheroidItem::SpheroidItem()
    : spheroid_name(nullptr), equitorial_radius(-1.0), polar_radius(-1.0),
      inverse_flattening(-1.0)
{
}

SpheroidItem::~SpheroidItem()
{
    CPLFree(spheroid_name);
}

void SpheroidItem::SetValuesByRadii(const char *spheroidname, double eq_radius,
                                    double p_radius)
{
    spheroid_name = CPLStrdup(spheroidname);
    equitorial_radius = eq_radius;
    polar_radius = p_radius;
    inverse_flattening =
        eq_radius == polar_radius ? 0 : eq_radius / (eq_radius - polar_radius);
}

void SpheroidItem::SetValuesByEqRadiusAndInvFlattening(const char *spheroidname,
                                                       double eq_radius,
                                                       double inverseflattening)
{
    spheroid_name = CPLStrdup(spheroidname);
    equitorial_radius = eq_radius;
    inverse_flattening = inverseflattening;
    polar_radius = inverse_flattening == 0
                       ? eq_radius
                       : eq_radius * (1.0 - (1.0 / inverse_flattening));
}

SpheroidList::SpheroidList() : num_spheroids(0), epsilonR(0.0), epsilonI(0.0)
{
}

SpheroidList::~SpheroidList()
{
}

char *SpheroidList::GetSpheroidNameByRadii(double eq_radius,
                                           double polar_radius)
{
    for (int index = 0; index < num_spheroids; index++)
    {
        const double er = spheroids[index].equitorial_radius;
        const double pr = spheroids[index].polar_radius;
        if ((fabs(er - eq_radius) < epsilonR) &&
            (fabs(pr - polar_radius) < epsilonR))
            return CPLStrdup(spheroids[index].spheroid_name);
    }

    return nullptr;
}

char *SpheroidList::GetSpheroidNameByEqRadiusAndInvFlattening(
    double eq_radius, double inverse_flattening)
{
    for (int index = 0; index < num_spheroids; index++)
    {
        const double er = spheroids[index].equitorial_radius;
        const double invf = spheroids[index].inverse_flattening;
        if ((fabs(er - eq_radius) < epsilonR) &&
            (fabs(invf - inverse_flattening) < epsilonI))
            return CPLStrdup(spheroids[index].spheroid_name);
    }

    return nullptr;
}

double SpheroidList::GetSpheroidEqRadius(const char *spheroid_name)
{
    for (int index = 0; index < num_spheroids; index++)
    {
        if (EQUAL(spheroids[index].spheroid_name, spheroid_name))
            return spheroids[index].equitorial_radius;
    }

    return -1.0;
}

int SpheroidList::SpheroidInList(const char *spheroid_name)
{
    /* Return 1 if the spheroid name is recognized; 0 otherwise */
    for (int index = 0; index < num_spheroids; index++)
    {
        if (EQUAL(spheroids[index].spheroid_name, spheroid_name))
            return 1;
    }

    return 0;
}

double SpheroidList::GetSpheroidInverseFlattening(const char *spheroid_name)
{
    for (int index = 0; index < num_spheroids; index++)
    {
        if (EQUAL(spheroids[index].spheroid_name, spheroid_name))
            return spheroids[index].inverse_flattening;
    }

    return -1.0;
}

double SpheroidList::GetSpheroidPolarRadius(const char *spheroid_name)
{
    for (int index = 0; index < num_spheroids; index++)
    {
        if (strcmp(spheroids[index].spheroid_name, spheroid_name) == 0)
            return spheroids[index].polar_radius;
    }

    return -1.0;
}
