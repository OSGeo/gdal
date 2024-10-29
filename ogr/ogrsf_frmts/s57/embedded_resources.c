// SPDX-License-Identifier: MIT
// Copyright 2024, Even Rouault <even.rouault at spatialys.com>

#include "embedded_resources.h"

static const char s57agencies_csv[] = {
#embed "data/s57agencies.csv"
    , 0};

static const char s57attributes_csv[] = {
#embed "data/s57attributes.csv"
    , 0};

static const char s57expectedinput_csv[] = {
#embed "data/s57expectedinput.csv"
    , 0};

static const char s57objectclasses_csv[] = {
#embed "data/s57objectclasses.csv"
    , 0};

const char *S57GetEmbeddedCSV(const char *pszFilename)
{
    if (EQUAL(pszFilename, "s57agencies.csv"))
        return s57agencies_csv;
    if (EQUAL(pszFilename, "s57attributes.csv"))
        return s57attributes_csv;
    if (EQUAL(pszFilename, "s57expectedinput.csv"))
        return s57expectedinput_csv;
    if (EQUAL(pszFilename, "s57objectclasses.csv"))
        return s57objectclasses_csv;
    return NULL;
}
