/******************************************************************************
 * $Id$
 *
 * Project:  GML Utils
 * Purpose:  GML reader
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 ******************************************************************************
 * Copyright (c) 2010, Even Rouault
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#include "gmlreaderp.h"

/************************************************************************/
/*                          BuildOGRGeometry()                          */
/************************************************************************/

OGRGeometry* BuildOGRGeometryFromList(char** papszGeometryList, int bTryToMakeMultipolygons)
{
    OGRGeometry* poGeom = NULL;
    if( papszGeometryList != NULL )
    {
        char** papszIter = papszGeometryList;
        OGRGeometryCollection* poCollection = NULL;
        while(*papszIter)
        {
            OGRGeometry* poSubGeom = OGRGeometryFactory::createFromGML( *papszIter );
            if (poSubGeom)
            {
                if (poGeom == NULL)
                    poGeom = poSubGeom;
                else
                {
                    if (poCollection == NULL)
                    {
                        if (bTryToMakeMultipolygons &&
                            wkbFlatten(poGeom->getGeometryType()) == wkbPolygon &&
                            wkbFlatten(poSubGeom->getGeometryType()) == wkbPolygon)
                        {
                            OGRGeometryCollection* poGeomColl = new OGRMultiPolygon();
                            poGeomColl->addGeometryDirectly(poGeom);
                            poGeomColl->addGeometryDirectly(poSubGeom);
                            poGeom = poGeomColl;
                        }
                        else if (bTryToMakeMultipolygons &&
                                  wkbFlatten(poGeom->getGeometryType()) == wkbMultiPolygon &&
                                 wkbFlatten(poSubGeom->getGeometryType()) == wkbPolygon)
                        {
                            OGRGeometryCollection* poGeomColl = (OGRGeometryCollection* )poGeom;
                            poGeomColl->addGeometryDirectly(poSubGeom);
                        }
                        else if (bTryToMakeMultipolygons &&
                                 wkbFlatten(poGeom->getGeometryType()) == wkbMultiPolygon &&
                                 wkbFlatten(poSubGeom->getGeometryType()) == wkbMultiPolygon)
                        {
                            OGRGeometryCollection* poGeomColl = (OGRGeometryCollection* )poGeom;
                            OGRGeometryCollection* poGeomColl2 = (OGRGeometryCollection* )poSubGeom;
                            int nCount = poGeomColl2->getNumGeometries();
                            int i;
                            for(i=0;i<nCount;i++)
                            {
                                poGeomColl->addGeometry(poGeomColl2->getGeometryRef(i));
                            }
                            delete poSubGeom;
                        }
                        else if (bTryToMakeMultipolygons &&
                                 wkbFlatten(poGeom->getGeometryType()) == wkbMultiPolygon)
                        {
                            delete poGeom;
                            delete poSubGeom;
                            return BuildOGRGeometryFromList(papszGeometryList, FALSE);
                        }
                        else
                        {
                            poCollection = new OGRGeometryCollection();
                            poCollection->addGeometryDirectly(poGeom);
                            poGeom = poCollection;
                        }
                    }
                    if (poCollection != NULL)
                    {
                        poCollection->addGeometryDirectly(poSubGeom);
                    }
                }
            }
            papszIter ++;
        }
    }
    return poGeom;
}
