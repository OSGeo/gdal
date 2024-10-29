/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  List of reserver keywords for File Geodatabase
 * Author:   Even Rouault, <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2024, Even Rouault, <even.rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef FILEGDB_RESERVED_KEYWORDS_H
#define FILEGDB_RESERVED_KEYWORDS_H

// from https://support.esri.com/en-us/knowledge-base/what-are-the-reserved-words-for-esri-s-file-geodatabase-000010906
static constexpr const char *const apszRESERVED_WORDS[] = {
    "ADD",    "ALTER",  "AND",    "BETWEEN", "BY",     "COLUMN", "CREATE",
    "DELETE", "DROP",   "EXISTS", "FOR",     "FROM",   "GROUP",  "IN",
    "INSERT", "INTO",   "IS",     "LIKE",    "NOT",    "NULL",   "OR",
    "ORDER",  "SELECT", "SET",    "TABLE",   "UPDATE", "VALUES", "WHERE",
};

#endif  // FILEGDB_RESERVED_KEYWORDS_H
