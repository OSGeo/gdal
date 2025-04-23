/******************************************************************************
 *
 * Project:  APP ENVISAT Support
 * Purpose:  Test mainline for dumping ENVISAT format files.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2001, Atlantis Scientific, Inc.
 *
 * SPDX-License-Identifier: MIT
 *****************************************************************************/

#include "cpl_conv.h"
#include "EnvisatFile.h"

int main(int argc, char **argv)

{
    EnvisatFile *es_file;
    int ds_index;
    int ds_offset, ds_size, num_dsr, dsr_size, i_record;

    if (argc != 2)
    {
        printf("Usage: dumpgeo filename\n");
        exit(1);
    }

    if (EnvisatFile_Open(&es_file, argv[1], "r") != 0)
    {
        printf("EnvisatFile_Open(%s) failed.\n", argv[1]);
        exit(2);
    }

    ds_index = EnvisatFile_GetDatasetIndex(es_file, "GEOLOCATION GRID ADS");
    if (ds_index == -1)
    {
        printf("Can't find geolocation grid ads.\n");
        exit(3);
    }

    EnvisatFile_GetDatasetInfo(es_file, ds_index, NULL, NULL, NULL, &ds_offset,
                               &ds_size, &num_dsr, &dsr_size);
    if (ds_offset == 0)
    {
        printf("No data for geolocation grid ads.\n");
        exit(4);
    }

    CPLAssert(dsr_size == 521);

    for (i_record = 0; i_record < num_dsr; i_record++)
    {
        GByte abyRecord[521];
        GUInt32 unValue;
        float fValue;
        int sample;

        EnvisatFile_ReadDatasetRecord(es_file, ds_index, i_record, abyRecord);

        printf("<====================== Record %d ==================>\n",
               i_record);

        /* field 1 */
        CPL_SWAP32PTR(abyRecord + 0);
        CPL_SWAP32PTR(abyRecord + 4);
        CPL_SWAP32PTR(abyRecord + 8);

        printf("start line: mjd_days = %d, sec = %d, msec = %d\n",
               ((int *)abyRecord)[0], ((unsigned int *)abyRecord)[1],
               ((unsigned int *)abyRecord)[2]);

        /* field 2 */
        printf("Attachment flag = %d\n", abyRecord[12]);

        /* field 3 */
        memcpy(&unValue, abyRecord + 13, 4);
        printf("range line (first in granule) = %d\n", CPL_SWAP32(unValue));

        /* field 4 */
        memcpy(&unValue, abyRecord + 17, 4);
        printf("lines in granule = %d\n", CPL_SWAP32(unValue));

        /* field 5 */
        memcpy(&fValue, abyRecord + 21, 4);
        CPL_SWAP32PTR(&fValue);
        printf("track heading (first line) = %f\n", fValue);

        /* field 6 */

        printf("first line of granule:\n");
        for (sample = 0; sample < 11; sample++)
        {
            memcpy(&unValue, abyRecord + 25 + sample * 4, 4);
            printf("  sample=%d ", CPL_SWAP32(unValue));

            memcpy(&fValue, abyRecord + 25 + 44 + sample * 4, 4);
            CPL_SWAP32PTR(&fValue);
            printf("time=%g ", fValue);

            memcpy(&fValue, abyRecord + 25 + 88 + sample * 4, 4);
            CPL_SWAP32PTR(&fValue);
            printf("angle=%g ", fValue);

            memcpy(&unValue, abyRecord + 25 + 132 + sample * 4, 4);
            printf("(%.9f,", ((int)CPL_SWAP32(unValue)) * 0.000001);

            memcpy(&unValue, abyRecord + 25 + 176 + sample * 4, 4);
            printf("%.9f)\n", ((int)CPL_SWAP32(unValue)) * 0.000001);
        }

        /* field 8 */
        CPL_SWAP32PTR(abyRecord + 267);
        CPL_SWAP32PTR(abyRecord + 271);
        CPL_SWAP32PTR(abyRecord + 275);

        printf("end line: mjd_days = %d, sec = %d, msec = %d\n",
               ((int *)(abyRecord + 267))[0],
               ((unsigned int *)(abyRecord + 267))[1],
               ((unsigned int *)(abyRecord + 267))[2]);

        /* field 9 */
        printf("final line of granule:\n");
        for (sample = 0; sample < 11; sample++)
        {
            memcpy(&unValue, abyRecord + 279 + sample * 4, 4);
            printf("  sample=%d ", CPL_SWAP32(unValue));

            memcpy(&fValue, abyRecord + 279 + 44 + sample * 4, 4);
            CPL_SWAP32PTR(&fValue);
            printf("time=%g ", fValue);

            memcpy(&fValue, abyRecord + 279 + 88 + sample * 4, 4);
            CPL_SWAP32PTR(&fValue);
            printf("angle=%g ", fValue);

            memcpy(&unValue, abyRecord + 279 + 132 + sample * 4, 4);
            printf("(%.9f,", ((int)CPL_SWAP32(unValue)) * 0.000001);

            memcpy(&unValue, abyRecord + 279 + 176 + sample * 4, 4);
            printf("%.9f)\n", ((int)CPL_SWAP32(unValue)) * 0.000001);
        }
    }

    EnvisatFile_Close(es_file);

    exit(0);
}
