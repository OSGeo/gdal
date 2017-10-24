/******************************************************************************
 *
 * Project:  DXF Translator
 * Purpose:  Provides additional functionality for DXF features
 * Author:   Alan Thomas, alant@outlook.com.au
 *
 ******************************************************************************
 * Copyright (c) 2017, Alan Thomas <alant@outlook.com.au>
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

#include "ogr_dxf.h"
#include "cpl_string.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                            OGRDXFFeature()                           */
/************************************************************************/

OGRDXFFeature::OGRDXFFeature( OGRFeatureDefn * poFeatureDefn ):
    OGRFeature( poFeatureDefn ),
    bIsBlockReference(false),
    dfBlockAngle(0.0)
{
    adfBlockScale[0] = adfBlockScale[1] = adfBlockScale[2] = 1.0;

    adfBlockOCS[0] = adfBlockOCS[1] = 0.0;
    adfBlockOCS[2] = 1.0;

    adfOriginalCoords[0] = adfOriginalCoords[1] = adfOriginalCoords[2] = 0.0;
}

/************************************************************************/
/*                          CloneDXFFeature()                           */
/*                                                                      */
/*      Replacement for OGRFeature::Clone() for DXF features.           */
/************************************************************************/

OGRDXFFeature *OGRDXFFeature::CloneDXFFeature()
{
    OGRDXFFeature *poNew = new OGRDXFFeature( GetDefnRef() );
    if( poNew == NULL )
        return NULL;

    if( !CopySelfTo( poNew ) )
    {
        delete poNew;
        return NULL;
    }

    poNew->bIsBlockReference = bIsBlockReference;
    poNew->osBlockName = osBlockName;
    poNew->dfBlockAngle = dfBlockAngle;
    poNew->adfBlockScale[0] = adfBlockScale[0];
    poNew->adfBlockScale[1] = adfBlockScale[1];
    poNew->adfBlockScale[2] = adfBlockScale[2];
    poNew->adfBlockOCS[0] = adfBlockOCS[0];
    poNew->adfBlockOCS[1] = adfBlockOCS[1];
    poNew->adfBlockOCS[2] = adfBlockOCS[2];
    poNew->adfOriginalCoords[0] = adfOriginalCoords[0];
    poNew->adfOriginalCoords[1] = adfOriginalCoords[1];
    poNew->adfOriginalCoords[2] = adfOriginalCoords[2];

    return poNew;
}
