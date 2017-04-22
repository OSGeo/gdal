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
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in all
 *  copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *  SOFTWARE.
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
