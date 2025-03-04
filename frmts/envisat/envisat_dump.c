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
    int i;
    const char *key;

    if (argc != 2)
    {
        printf("Usage: envisatdump filename\n");
        exit(1);
    }

    if (EnvisatFile_Open(&es_file, argv[1], "r") != 0)
    {
        printf("EnvisatFile_Open(%s) failed.\n", argv[1]);
        exit(2);
    }

    printf("MPH\n");
    printf("===\n");

    for (i = 0; (key = EnvisatFile_GetKeyByIndex(es_file, MPH, i)) != NULL; i++)
    {
        const char *value =
            EnvisatFile_GetKeyValueAsString(es_file, MPH, key, "");

        printf("%s = [%s]\n", key, value);
    }

    printf("\n");
    printf("SPH\n");
    printf("===\n");

    for (i = 0; (key = EnvisatFile_GetKeyByIndex(es_file, SPH, i)) != NULL; i++)
    {
        const char *value =
            EnvisatFile_GetKeyValueAsString(es_file, SPH, key, "");

        printf("%s = [%s]\n", key, value);
    }

    printf("\n");
    printf("Datasets\n");
    printf("========\n");

    for (i = 0; TRUE; i++)
    {
        char *ds_name, *ds_type, *filename;
        int ds_offset, ds_size, num_dsr, dsr_size;

        if (EnvisatFile_GetDatasetInfo(es_file, i, &ds_name, &ds_type,
                                       &filename, &ds_offset, &ds_size,
                                       &num_dsr, &dsr_size) == 1)
            break;

        printf("\nDataset %d\n", i);

        printf("ds_name = %s\n", ds_name);
        printf("ds_type = %s\n", ds_type);
        printf("filename = %s\n", filename);
        printf("ds_offset = %d\n", ds_offset);
        printf("ds_size = %d\n", ds_size);
        printf("num_dsr = %d\n", num_dsr);
        printf("dsr_size = %d\n", dsr_size);
    }

    EnvisatFile_Close(es_file);

    exit(0);
}
