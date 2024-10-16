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
#include "cadclasses.h"
#include "opencad.h"

#include <iostream>

using namespace std;

//------------------------------------------------------------------------------
// CADClass
//------------------------------------------------------------------------------

CADClass::CADClass() : sCppClassName(""),
                sApplicationName(""),
                sDXFRecordName(""),
                dProxyCapFlag(0),
                dInstanceCount(0),
                bWasZombie(false),
                bIsEntity(false),
                dClassNum(0),
                dClassVersion(0)
{
}

//------------------------------------------------------------------------------
// CADClasses
//------------------------------------------------------------------------------

CADClasses::CADClasses()
{
}

void CADClasses::addClass( CADClass stClass )
{
    classes.push_back( stClass );

    DebugMsg( "CLASS INFO\n"
                      "  Class Number: %d\n"
                      "  Proxy capabilities flag or Version: %d\n"
                      "  App name: %s\n"
                      "  C++ Class Name: %s\n"
                      "  DXF Class name: %s\n"
                      "  Was a zombie? %x\n"
                      "  Is-an-entity flag: %x\n\n", stClass.dClassNum, stClass.dProxyCapFlag,
              stClass.sApplicationName.c_str(), stClass.sCppClassName.c_str(), stClass.sDXFRecordName.c_str(),
              stClass.bWasZombie, stClass.bIsEntity );
}

CADClass CADClasses::getClassByNum( short num ) const
{
    for( const CADClass &cadClass : classes )
    {
        if( cadClass.dClassNum == num )
            return cadClass;
    }
    return CADClass();
}

void CADClasses::print() const
{
    cout << "============ CLASSES Section ============\n";

    for( CADClass stClass : classes )
    {
        cout << "Class:" <<
        "\n  Class Number: " << stClass.dClassNum <<
        "\n  Proxy capabilities flag or Version: " << stClass.dProxyCapFlag <<
        "\n  App name: " << stClass.sApplicationName <<
        "\n  C++ Class Name: " << stClass.sCppClassName <<
        "\n  DXF Class name: " << stClass.sDXFRecordName <<
        "\n  Was a zombie: " << stClass.bWasZombie <<
        "\n  Is-an-entity flag: " << stClass.bIsEntity << "\n\n";
    }
}
