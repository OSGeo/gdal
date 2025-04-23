// SPDX-License-Identifier: MIT
// Copyright 2024, Even Rouault <even.rouault at spatialys.com>

#include "embedded_resources.h"

static const char szSpecFile[] = {
#embed "data/nitf_spec.xml"
    , 0};

const char *NITFGetSpecFile()
{
    return szSpecFile;
}

static const char szGTDatum[] = {
#embed "data/gt_datum.csv"
    , 0};

const char *NITFGetGTDatum()
{
    return szGTDatum;
}

static const char szGTEllips[] = {
#embed "data/gt_ellips.csv"
    , 0};

const char *NITFGetGTEllips()
{
    return szGTEllips;
}
