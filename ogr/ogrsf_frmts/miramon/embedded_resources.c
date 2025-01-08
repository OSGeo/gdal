// SPDX-License-Identifier: MIT
// Copyright 2024, Even Rouault <even.rouault at spatialys.com>

#include "embedded_resources.h"

static const char MM_m_idofic_csv[] = {
#embed "data/MM_m_idofic.csv"
    , 0};

const char *MiraMonGetMM_m_idofic_csv(void)
{
    return MM_m_idofic_csv;
}
