/******************************************************************************
 *
 * Project:  SAP HANA Spatial Driver
 * Purpose:  OGRHanaDriver functions implementation
 * Author:   Maxim Rylov
 *
 ******************************************************************************
 * Copyright (c) 2020, SAP SE
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef OGRHANADRIVERCORE_H
#define OGRHANADRIVERCORE_H

#include "gdal_priv.h"

constexpr const char *DRIVER_NAME = "HANA";

constexpr const char *HANA_PREFIX = "HANA:";

#define OGRHanaLayerCreationOptionsConstants                                   \
    PLUGIN_SYMBOL_NAME(OGRHanaLayerCreationOptionsConstants)
#define OGRHanaOpenOptionsConstants                                            \
    PLUGIN_SYMBOL_NAME(OGRHanaOpenOptionsConstants)
#define OGRHanaDriverIdentify PLUGIN_SYMBOL_NAME(OGRHanaDriverIdentify)
#define OGRHANADriverSetCommonMetadata                                         \
    PLUGIN_SYMBOL_NAME(OGRHANADriverSetCommonMetadata)

class OGRHanaLayerCreationOptionsConstants
{
  public:
    OGRHanaLayerCreationOptionsConstants() = delete;

  public:
    static constexpr const char *OVERWRITE = "OVERWRITE";
    static constexpr const char *LAUNDER = "LAUNDER";
    static constexpr const char *PRECISION = "PRECISION";
    static constexpr const char *DEFAULT_STRING_SIZE = "DEFAULT_STRING_SIZE";
    static constexpr const char *GEOMETRY_NAME = "GEOMETRY_NAME";
    static constexpr const char *GEOMETRY_NULLABLE = "GEOMETRY_NULLABLE";
    static constexpr const char *GEOMETRY_INDEX = "GEOMETRY_INDEX";
    static constexpr const char *SRID = "SRID";
    static constexpr const char *FID = "FID";
    static constexpr const char *FID64 = "FID64";
    static constexpr const char *COLUMN_TYPES = "COLUMN_TYPES";
    static constexpr const char *BATCH_SIZE = "BATCH_SIZE";

    static const char *GetList();
};

class OGRHanaOpenOptionsConstants
{
  public:
    OGRHanaOpenOptionsConstants() = delete;

  public:
    static constexpr const char *DSN = "DSN";
    static constexpr const char *DRIVER = "DRIVER";
    static constexpr const char *HOST = "HOST";
    static constexpr const char *PORT = "PORT";
    static constexpr const char *DATABASE = "DATABASE";
    static constexpr const char *USER = "USER";
    static constexpr const char *PASSWORD = "PASSWORD";
    static constexpr const char *USER_STORE_KEY = "USER_STORE_KEY";
    static constexpr const char *SCHEMA = "SCHEMA";
    static constexpr const char *TABLES = "TABLES";

    static constexpr const char *ENCRYPT = "ENCRYPT";
    static constexpr const char *SSL_CRYPTO_PROVIDER = "SSL_CRYPTO_PROVIDER";
    static constexpr const char *SSL_KEY_STORE = "SSL_KEY_STORE";
    static constexpr const char *SSL_TRUST_STORE = "SSL_TRUST_STORE";
    static constexpr const char *SSL_VALIDATE_CERTIFICATE =
        "SSL_VALIDATE_CERTIFICATE";
    static constexpr const char *SSL_HOST_NAME_CERTIFICATE =
        "SSL_HOST_NAME_CERTIFICATE";

    static constexpr const char *CONNECTION_TIMEOUT = "CONNECTION_TIMEOUT";
    static constexpr const char *PACKET_SIZE = "PACKET_SIZE";
    static constexpr const char *SPLIT_BATCH_COMMANDS = "SPLIT_BATCH_COMMANDS";

    static constexpr const char *DETECT_GEOMETRY_TYPE = "DETECT_GEOMETRY_TYPE";

    static const char *GetList();
};

int OGRHanaDriverIdentify(GDALOpenInfo *poOpenInfo);

void OGRHANADriverSetCommonMetadata(GDALDriver *poDriver);

#endif
