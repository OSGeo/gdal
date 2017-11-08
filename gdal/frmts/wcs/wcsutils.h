/******************************************************************************
 *
 * Project:  WCS Client Driver
 * Purpose:  Implementation of utilities.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2006, Frank Warmerdam
 * Copyright (c) 2008-2013, Even Rouault <even dot rouault at mines-paris dot org>
 * Copyright (c) 2017, Ari Jolma
 * Copyright (c) 2017, Finnish Environment Institute
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#include "cpl_string.h"
#include "cpl_minixml.h"
#include <vector>

CPLString String(const char *str);

CPLString URLEncode(CPLString str);

CPLString URLRemoveKey(const char *url, CPLString key);

std::vector<CPLString> Split(const char *value,
                             const char *delim,
                             bool swap_the_first_two = false);

CPLString Join(std::vector<CPLString> array,
               const char *delim,
               bool swap_the_first_two = false);

std::vector<int> Ilist(std::vector<CPLString> array,
                       unsigned int from,
                       unsigned int count);

std::vector<double> Flist(std::vector<CPLString> array,
                          unsigned int from,
                          unsigned int count);

int IndexOf(CPLString str, std::vector<CPLString> array); // index of str in array
int IndexOf(int i, std::vector<int> array);

std::vector<int> IndexOf(std::vector<CPLString> strs, std::vector<CPLString> array); // index of strs in array

bool Contains(std::vector<int> array, int value);

CPLString FromParenthesis(CPLString s);

std::vector<CPLString> ParseSubset(std::vector<CPLString>
                                   subset_array,
                                   CPLString dim);

bool FileIsReadable(CPLString filename);

bool MakeDir(CPLString dirname);

CPLXMLNode *SearchChildWithValue(CPLXMLNode *node, const char *path, const char *value);
    
bool SetupCache(CPLString &cache_dir,
                bool clear);

bool DeleteEntryFromCache(CPLString cache_dir,
                          CPLString  key,
                          CPLString value);

int FromCache(CPLString cache_dir,
              CPLString &filename,
              CPLString url);

CPLXMLNode *AddSimpleMetaData(char ***metadata,
                              CPLXMLNode *node,
                              CPLString &path,
                              CPLString from,
                              std::vector<CPLString> keys);

CPLString GetKeywords(CPLXMLNode *root,
                      CPLString path,
                      CPLString kw);

CPLString ParseCRS(CPLXMLNode *node);

bool CRS2Projection(CPLString crs,
                    OGRSpatialReference *sr,
                    char **projection);

bool CRSImpliesAxisOrderSwap(CPLString crs, bool &swap, char **projection = NULL);
    
std::vector<std::vector<int>> ParseGridEnvelope(CPLXMLNode *node,
                                                bool swap_the_first_two = false);

std::vector<CPLString> ParseBoundingBox(CPLXMLNode *node);
