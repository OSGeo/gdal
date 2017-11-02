#include "cpl_string.h"
#include "cpl_minixml.h"
#include <vector>

CPLString URLEncode(CPLString str);

std::vector<CPLString> Split(const char *value,
                             const char *delim);

bool ParseList(CPLXMLNode *node,
               CPLString path,
               std::vector<CPLString> &list,
               bool swap = false);

bool ParseDoubleList(CPLXMLNode *node,
                     const char *path,
                     std::vector<double> &coords,
                     bool swap = false);

bool FileIsReadable(CPLString filename);

bool MakeDir(CPLString dirname);
    
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

bool CRS2Projection(CPLString crs, char
                    **projection);
    
bool ParseGridEnvelope(CPLXMLNode *node,
                       const char *path,
                       std::vector<int> &sizes);

bool ParseBoundingBox(CPLXMLNode *bbox,
                      CPLString &crs,
                      std::vector<double> &bounds);
