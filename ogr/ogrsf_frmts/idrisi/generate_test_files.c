/******************************************************************************
 *
 * Project:
 * Purpose:  Generate sample .VCT files
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2011, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include <stdio.h>

int main(int argc, char *argv[])
{
    char c;
    int i;
    int nfeatures;
    int nodes;
    int nparts;
    FILE *f;
    double id, x, y;
    double minx, maxx, miny, maxy;

    f = fopen("points.vct", "wb");
    c = 0x01;
    fwrite(&c, 1, 1, f);
    nfeatures = 2;
    fwrite(&nfeatures, 1, sizeof(int), f);
    for (i = 5; i < 0x105; i++)
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
    for (i = 5; i < 0x105; i++)
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
    nfeatures = 2;
    fwrite(&nfeatures, 1, sizeof(int), f);
    for (i = 5; i < 0x105; i++)
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

    id = 2;
    minx = 400000;
    miny = 4000000;
    maxx = 600000;
    maxy = 5000000;
    fwrite(&id, 1, sizeof(double), f);
    fwrite(&minx, 1, sizeof(double), f);
    fwrite(&miny, 1, sizeof(double), f);
    fwrite(&maxx, 1, sizeof(double), f);
    fwrite(&maxy, 1, sizeof(double), f);
    nparts = 1;
    fwrite(&nparts, 1, sizeof(int), f);
    nodes = 5;
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

    fclose(f);

    return 0;
}
