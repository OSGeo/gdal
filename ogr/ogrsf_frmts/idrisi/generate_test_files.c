/******************************************************************************
 * $Id$
 *
 * Project:  
 * Purpose:  Generate sample .VCT files
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 ******************************************************************************
 * Copyright (c) 2011, Even Rouault <even dot rouault at mines dash paris dot org>
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

#include <stdio.h>

int main(int argc, char* argv[])
{
    char c;
    int i;
    int nfeatures;
    int nodes;
    int nparts;
    FILE* f;
    double id, x, y;
    double minx, maxx, miny, maxy;

    f = fopen("points.vct", "wb");
    c = 0x01;
    fwrite(&c, 1, 1, f);
    nfeatures = 2;
    fwrite(&nfeatures, 1, sizeof(int), f);
    for(i=5;i<0x105;i++)
    {
        c = 0;
        fwrite(&c, 1, 1, f);
    }

    id = 1;
    x = 400000;
    y = 5000000;
    fwrite(&id, 1, sizeof(double), f);
    fwrite(&x, 1, sizeof(double), f);
    fwrite(&y, 1, sizeof(double), f);

    id = 2;
    x = 600000;
    y = 4000000;
    fwrite(&id, 1, sizeof(double), f);
    fwrite(&x, 1, sizeof(double), f);
    fwrite(&y, 1, sizeof(double), f);

    fclose(f);



    f = fopen("lines.vct", "wb");
    c = 0x02;
    fwrite(&c, 1, 1, f);
    nfeatures = 2;
    fwrite(&nfeatures, 1, sizeof(int), f);
    for(i=5;i<0x105;i++)
    {
        c = 0;
        fwrite(&c, 1, 1, f);
    }

    id = 10;
    minx = 400000;
    miny = 4500000;
    maxx = 600000;
    maxy = 5000000;
    fwrite(&id, 1, sizeof(double), f);
    fwrite(&minx, 1, sizeof(double), f);
    fwrite(&miny, 1, sizeof(double), f);
    fwrite(&maxx, 1, sizeof(double), f);
    fwrite(&maxy, 1, sizeof(double), f);
    nodes = 2;
    fwrite(&nodes, 1, sizeof(int), f);
    x = 400000;
    y = 5000000;
    fwrite(&x, 1, sizeof(double), f);
    fwrite(&y, 1, sizeof(double), f);
    x = 600000;
    y = 4500000;
    fwrite(&x, 1, sizeof(double), f);
    fwrite(&y, 1, sizeof(double), f);

    id = 20;
    minx = 400000;
    miny = 4000000;
    maxx = 600000;
    maxy = 4500000;
    fwrite(&id, 1, sizeof(double), f);
    fwrite(&minx, 1, sizeof(double), f);
    fwrite(&miny, 1, sizeof(double), f);
    fwrite(&maxx, 1, sizeof(double), f);
    fwrite(&maxy, 1, sizeof(double), f);
    nodes = 2;
    fwrite(&nodes, 1, sizeof(int), f);
    x = 450000;
    y = 4000000;
    fwrite(&x, 1, sizeof(double), f);
    fwrite(&y, 1, sizeof(double), f);
    x = 550000;
    y = 4500000;
    fwrite(&x, 1, sizeof(double), f);
    fwrite(&y, 1, sizeof(double), f);

    fclose(f);



    f = fopen("polygons.vct", "wb");
    c = 0x03;
    fwrite(&c, 1, 1, f);
    nfeatures = 1;
    fwrite(&nfeatures, 1, sizeof(int), f);
    for(i=5;i<0x105;i++)
    {
        c = 0;
        fwrite(&c, 1, 1, f);
    }

    id = 1;
    minx = 400000;
    miny = 4000000;
    maxx = 600000;
    maxy = 5000000;
    fwrite(&id, 1, sizeof(double), f);
    fwrite(&minx, 1, sizeof(double), f);
    fwrite(&miny, 1, sizeof(double), f);
    fwrite(&maxx, 1, sizeof(double), f);
    fwrite(&maxy, 1, sizeof(double), f);
    nparts = 2;
    fwrite(&nparts, 1, sizeof(int), f);
    nodes = 10;
    fwrite(&nodes, 1, sizeof(int), f);

    nodes = 5;
    fwrite(&nodes, 1, sizeof(int), f);
    x = 400000;
    y = 4000000;
    fwrite(&x, 1, sizeof(double), f);
    fwrite(&y, 1, sizeof(double), f);
    x = 400000;
    y = 5000000;
    fwrite(&x, 1, sizeof(double), f);
    fwrite(&y, 1, sizeof(double), f);
    x = 600000;
    y = 5000000;
    fwrite(&x, 1, sizeof(double), f);
    fwrite(&y, 1, sizeof(double), f);
    x = 600000;
    y = 4000000;
    fwrite(&x, 1, sizeof(double), f);
    fwrite(&y, 1, sizeof(double), f);
    x = 400000;
    y = 4000000;
    fwrite(&x, 1, sizeof(double), f);
    fwrite(&y, 1, sizeof(double), f);


    nodes = 5;
    fwrite(&nodes, 1, sizeof(int), f);
    x = 450000;
    y = 4250000;
    fwrite(&x, 1, sizeof(double), f);
    fwrite(&y, 1, sizeof(double), f);
    x = 450000;
    y = 4750000;
    fwrite(&x, 1, sizeof(double), f);
    fwrite(&y, 1, sizeof(double), f);
    x = 550000;
    y = 4750000;
    fwrite(&x, 1, sizeof(double), f);
    fwrite(&y, 1, sizeof(double), f);
    x = 550000;
    y = 4250000;
    fwrite(&x, 1, sizeof(double), f);
    fwrite(&y, 1, sizeof(double), f);
    x = 450000;
    y = 4250000;
    fwrite(&x, 1, sizeof(double), f);
    fwrite(&y, 1, sizeof(double), f);

    fclose(f);

    return 0;
}
