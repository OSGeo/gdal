/******************************************************************************
 *
 * Project:  Elasticsearch Translator
 * Purpose:
 * Author:
 *
 ******************************************************************************
 * Copyright (c) 2011, Adam Estrada
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef OGRELASTICDRIVERCORE_H
#define OGRELASTICDRIVERCORE_H

#include "gdal_priv.h"

constexpr const char *DRIVER_NAME = "Elasticsearch";

#define OGRElasticsearchDriverIdentify                                         \
    PLUGIN_SYMBOL_NAME(OGRElasticsearchDriverIdentify)
#define OGRElasticsearchDriverSetCommonMetadata                                \
    PLUGIN_SYMBOL_NAME(OGRElasticsearchDriverSetCommonMetadata)

int OGRElasticsearchDriverIdentify(GDALOpenInfo *poOpenInfo);

void OGRElasticsearchDriverSetCommonMetadata(GDALDriver *poDriver);

#endif
