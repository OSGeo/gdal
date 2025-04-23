// SPDX-License-Identifier: MIT
// Copyright (c) 2019, Guilherme Agostinelli
// Ported from https://github.com/guilhermeagostinelli/levenshtein/blob/master/levenshtein.cpp

#ifndef CPL_LEVENSHTEIN_H
#define CPL_LEVENSHTEIN_H

#include "cpl_port.h"

size_t CPL_DLL CPLLevenshteinDistance(const char *word1, const char *word2,
                                      bool transpositionAllowed);

#endif
