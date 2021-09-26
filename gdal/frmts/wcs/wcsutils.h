/******************************************************************************
 *
 * Project:  WCS Client Driver
 * Purpose:  Implementation of utilities.
 * Author:   Ari Jolma <ari dot jolma at gmail dot com>
 *
 ******************************************************************************
 * Copyright (c) 2006, Frank Warmerdam
 * Copyright (c) 2008-2013, Even Rouault <even dot rouault at spatialys.com>
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

namespace WCSUtils {

void Swap(double &a, double &b);

CPLString URLEncode(const CPLString &str);

CPLString URLRemoveKey(const char *url, const CPLString &key);

std::vector<CPLString> &SwapFirstTwo(std::vector<CPLString> &array);

std::vector<CPLString> Split(const char *value,
                             const char *delim,
                             bool swap_the_first_two = false);

CPLString Join(const std::vector<CPLString> &array,
               const char *delim,
               bool swap_the_first_two = false);

std::vector<int> Ilist(const std::vector<CPLString> &array,
                       unsigned int from = 0,
                       size_t count = std::string::npos);

std::vector<double> Flist(const std::vector<CPLString> &array,
                          unsigned int from = 0,
                          size_t count = std::string::npos);

// index of string or integer in an array
// indexes of strings in an array
// index of key in a key value pairs    
int IndexOf(const CPLString &str, const std::vector<CPLString> &array);
int IndexOf(int i, const std::vector<int> &array);
std::vector<int> IndexOf(const std::vector<CPLString> &strs,
                         const std::vector<CPLString> &array);
int IndexOf(const CPLString &key, const std::vector<std::vector<CPLString> > &kvps);
    
bool Contains(const std::vector<int> &array, int value);

CPLString FromParenthesis(const CPLString &s);

std::vector<CPLString> ParseSubset(const std::vector<CPLString> &subset_array,
                                   const CPLString &dim);

bool FileIsReadable(const CPLString &filename);

CPLString RemoveExt(const CPLString &filename);

bool MakeDir(const CPLString &dirname);

CPLXMLNode *SearchChildWithValue(CPLXMLNode *node, const char *path, const char *value);

bool CPLGetXMLBoolean(CPLXMLNode *poRoot, const char *pszPath);

bool CPLUpdateXML(CPLXMLNode *poRoot, const char *pszPath, const char *new_value);

void XMLCopyMetadata(CPLXMLNode *node, CPLXMLNode *metadata, CPLString key);
    
bool SetupCache(CPLString &cache,
                bool clear);

std::vector<CPLString> ReadCache(const CPLString &cache);

bool DeleteEntryFromCache(const CPLString &cache,
                          const CPLString &key,
                          const CPLString &value);

CPLErr SearchCache(const CPLString &cache,
                   const CPLString &url,
                   CPLString &filename,
                   const CPLString &ext,
                   bool &found);

CPLErr AddEntryToCache(const CPLString &cache,
                       const CPLString &url,
                       CPLString &filename,
                       const CPLString &ext);

CPLXMLNode *AddSimpleMetaData(char ***metadata,
                              CPLXMLNode *node,
                              CPLString &path,
                              const CPLString &from,
                              const std::vector<CPLString> &keys);

CPLString GetKeywords(CPLXMLNode *root,
                      const CPLString &path,
                      const CPLString &kw);

CPLString ParseCRS(CPLXMLNode *node);

bool CRS2Projection(const CPLString &crs,
                    OGRSpatialReference *sr,
                    char **projection);

bool CRSImpliesAxisOrderSwap(const CPLString &crs, bool &swap, char **projection = nullptr);
    
std::vector<std::vector<int> > ParseGridEnvelope(CPLXMLNode *node,
                                                bool swap_the_first_two = false);

std::vector<CPLString> ParseBoundingBox(CPLXMLNode *node);

}
