/*******************************************************************************
 *  Project: libopencad
 *  Purpose: OpenSource CAD formats support library
 *  Author: Alexandr Borzykh, mush3d at gmail.com
 *  Author: Dmitry Baryshnikov, bishop.dev@gmail.com
 *  Language: C++
 *******************************************************************************
 *  The MIT License (MIT)
 *
 *  Copyright (c) 2016 Alexandr Borzykh
 *  Copyright (c) 2016 NextGIS, <info@nextgis.com>
 *
  * SPDX-License-Identifier: MIT
 *******************************************************************************/
#ifndef CADCOLORS_H
#define CADCOLORS_H

typedef struct
{
    unsigned char R;
    unsigned char G;
    unsigned char B;
} RGBColor;

/**
 * @brief Lookup table to translate ACI to RGB color.
 */
 const RGBColor getCADACIColor(short index);

#endif // CADCOLORS_H
