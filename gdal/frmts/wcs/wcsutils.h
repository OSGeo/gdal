#include "cpl_string.h"
#include "cpl_minixml.h"
#include <vector>

CPLString URLEncode(CPLString str);

std::vector<CPLString> Split(const char *value,
                             const char *delim,
                             bool swap_the_first_two = false);

std::vector<double> Flist(std::vector<CPLString> list,
                          unsigned int from = 0,
                          unsigned int count = 2);

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

bool CRS2Projection(CPLString crs,
                    OGRSpatialReference &oSRS,
                    char **projection);

bool CRSImpliesAxisOrderSwap(CPLString crs, bool &swap, char **projection = NULL);
    
std::vector<std::vector<int>> ParseGridEnvelope(CPLXMLNode *node,
                                                bool swap_the_first_two = false);

std::vector<CPLString> ParseBoundingBox(CPLXMLNode *node);
