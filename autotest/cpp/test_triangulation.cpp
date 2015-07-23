/******************************************************************************
 * $Id$
 *
 * Project:  GDAL algorithms
 * Purpose:  Test Delaunay triangulation
 * Author:   Even Rouault, even.rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2015, Even Rouault <even.rouault at spatialys.com>
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

#include <tut.h>
#include <gdal_alg.h>

namespace tut
{
    // Common fixture with test data
    struct test_triangulation_data
    {
        GDALTriangulation* psDT;
        
        test_triangulation_data() : psDT(NULL) {}
        ~test_triangulation_data() { GDALTriangulationFree(psDT); }
    };

    // Register test group
    typedef test_group<test_triangulation_data> group;
    typedef group::object object;
    group test_triangulation_group("Triangulation");

    template<>
    template<>
    void object::test<1>()
    {
        if( GDALHasTriangulation() )
        {
            double adfX[] = { 0, -5, -5, 5, 5 };
            double adfY[] = { 0, -5, 5, -5, 5 };
            CPLPushErrorHandler(CPLQuietErrorHandler);
            psDT = GDALTriangulationCreateDelaunay(2, adfX, adfY);
            CPLPopErrorHandler();
            ensure(psDT == NULL);
        }
    }
    
    template<>
    template<>
    void object::test<2>()
    {
        if( GDALHasTriangulation() )
        {
            double adfX[] = { 0, 1, 2, 3 };
            double adfY[] = { 0, 1, 2, 3 };
            CPLPushErrorHandler(CPLQuietErrorHandler);
            psDT = GDALTriangulationCreateDelaunay(4, adfX, adfY);
            CPLPopErrorHandler();
            ensure(psDT == NULL);
        }
    }
    
    template<>
    template<>
    void object::test<3>()
    {
        if( GDALHasTriangulation() )
        {
            double adfX[] = { 0, -5, -5, 5, 5 };
            double adfY[] = { 0, -5, 5, -5, 5 };
            int i, j;
            psDT = GDALTriangulationCreateDelaunay(5, adfX, adfY);
            ensure(psDT != NULL);
            ensure_equals(psDT->nFacets, 4);
            for(i=0;i<psDT->nFacets;i++)
            {
                for(j=0;j<3;j++)
                {
                    ensure(psDT->pasFacets[i].anVertexIdx[j] >= 0);
                    ensure(psDT->pasFacets[i].anVertexIdx[j] <= 4);
                    ensure(psDT->pasFacets[i].anNeighborIdx[j] >= -1);
                    ensure(psDT->pasFacets[i].anNeighborIdx[j] <= 4);
                }
            }
            int face;
            CPLPushErrorHandler(CPLQuietErrorHandler);
            ensure_equals(GDALTriangulationFindFacetDirected(psDT, 0, 0, 0, &face), FALSE);
            ensure_equals(GDALTriangulationFindFacetBruteForce(psDT, 0, 0, &face), FALSE);
            double l1, l2, l3;
            ensure_equals(GDALTriangulationComputeBarycentricCoordinates(psDT, 0, 0, 0, &l1, &l2, &l3), FALSE);
            CPLPopErrorHandler();
            ensure_equals(GDALTriangulationComputeBarycentricCoefficients(psDT, adfX, adfY) , TRUE);
            ensure_equals(GDALTriangulationComputeBarycentricCoefficients(psDT, adfX, adfY) , TRUE);

            // Points inside
            {
                double adfX[] = { 0, 1, 0, -1 };
                double adfY[] = { 1, 0, -1, 0 };
                for(int i=0;i<4;i++)
                {
                    double x = adfX[i];
                    double y = adfY[i];
                    int new_face;
                    ensure_equals(GDALTriangulationFindFacetDirected(psDT, 0, x, y, &face), TRUE);
                    ensure(face >= 0 && face < 4);
                    ensure_equals(GDALTriangulationFindFacetDirected(psDT, 1, x, y, &new_face), TRUE);
                    ensure_equals(face, new_face);
                    ensure_equals(GDALTriangulationFindFacetDirected(psDT, 2, x, y, &new_face), TRUE);
                    ensure_equals(face, new_face);
                    ensure_equals(GDALTriangulationFindFacetDirected(psDT, 3, x, y, &new_face), TRUE);
                    ensure_equals(face, new_face);
                    ensure_equals(GDALTriangulationFindFacetBruteForce(psDT, x, y, &new_face), TRUE);
                    ensure_equals(face, new_face);

                    double l1, l2, l3;
                    GDALTriangulationComputeBarycentricCoordinates(psDT, face, x, y, &l1, &l2, &l3);
                    ensure(l1 >= 0 && l1 <= 1);
                    ensure(l2 >= 0 && l2 <= 1);
                    ensure(l3 >= 0 && l3 <= 1);
                    ensure_equals(l3, 1 -l1 - l2);
                }
            }

            // Points outside
            {
                double adfX[] = { 0, 10, 0, -10 };
                double adfY[] = { 10, 0, -10, 0 };
                for(int i=0;i<4;i++)
                {
                    double x = adfX[i];
                    double y = adfY[i];
                    int new_face;
                    ensure_equals(GDALTriangulationFindFacetDirected(psDT, 0, x, y, &face), FALSE);
                    ensure(face >= 0 && face < 4);
                    ensure_equals(GDALTriangulationFindFacetDirected(psDT, 1, x, y, &new_face), FALSE);
                    ensure_equals(face, new_face);
                    ensure_equals(GDALTriangulationFindFacetDirected(psDT, 2, x, y, &new_face), FALSE);
                    ensure_equals(face, new_face);
                    ensure_equals(GDALTriangulationFindFacetDirected(psDT, 3, x, y, &new_face), FALSE);
                    ensure_equals(face, new_face);
                    ensure_equals(GDALTriangulationFindFacetBruteForce(psDT, x, y, &new_face), FALSE);
                    ensure_equals(face, new_face);

                    double l1, l2, l3;
                    GDALTriangulationComputeBarycentricCoordinates(psDT, face, x, y, &l1, &l2, &l3);
                    ensure("outside", !((l1 >= 0 && l1 <= 1) && (l2 >= 0 && l2 <= 1) && (l3 >= 0 && l3 <= 1)));
                    ensure_equals(l3, 1 -l1 - l2);
                }
            }
        }
    }
} // namespace tut
