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
#ifndef CADCLASSES_H
#define CADCLASSES_H

#include "opencad.h"

// std headers
#include <string>
#include <vector>

class OCAD_EXTERN  CADClass
{
public:
    CADClass();

public:
    std::string          sCppClassName;       /**< TV, C++ class name */
    std::string          sApplicationName;    /**< TV, Application name */
    std::string          sDXFRecordName;      /**< TV, Class DXF record name */
    int             dProxyCapFlag;       /**< BITSHORT, Proxy capabilities flag, 90 */
    unsigned short  dInstanceCount;      /**< BITSHORT, Instance count for a custom class, 91 */
    bool            bWasZombie;          /**< BIT, Was-a-proxy flag, 280*/
    bool            bIsEntity;           /**< BITSHORT, Is-an-entity flag, 281 */
    short           dClassNum;           /**< BITSHORT, Class number */
    short           dClassVersion;       /**< BITSHORT, Class version */
};

class OCAD_EXTERN CADClasses
{
public:
    CADClasses();

public:
    void                addClass(CADClass stClass);
    CADClass            getClassByNum(short num) const;
    void                print() const;

protected:
    std::vector<CADClass>    classes;
};

#endif // CADCLASSES_H
