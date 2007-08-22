#include "stdinc.h"

GDALWMSCache::GDALWMSCache() {
	m_cache_path = "./gdalwmscache";
	m_postfix = "";
	m_cache_depth = 2;
}

GDALWMSCache::~GDALWMSCache() {
}

CPLErr GDALWMSCache::Initialize(CPLXMLNode *config) {
	const char *cache_path = CPLGetXMLValue(config, "Path", "./gdalwmscache");
	m_cache_path = cache_path;

	const char *cache_depth = CPLGetXMLValue(config, "Depth", "2");
	m_cache_depth = atoi(cache_depth);

	const char *cache_extension = CPLGetXMLValue(config, "Extension", "");
	m_postfix = cache_extension;

	return CE_None;
}

CPLErr GDALWMSCache::Write(const char *key, const CPLString &file_name) {
	CPLString cache_file(KeyToCacheFile(key));
	//	printf("GDALWMSCache::Write(%s, %s) -> %s\n", key, file_name.c_str());
	if (CPLCopyFile(cache_file.c_str(), file_name.c_str()) != CE_None) {
		MakeDirs(cache_file.c_str());
		CPLCopyFile(cache_file.c_str(), file_name.c_str());
	}

	return CE_None;
}

CPLErr GDALWMSCache::Read(const char *key, CPLString *file_name) {
	CPLErr ret = CE_Failure;
	CPLString cache_file(KeyToCacheFile(key));
	FILE *f = VSIFOpen(cache_file.c_str(), "rb");
	if (f != NULL) {
		VSIFClose(f);
		*file_name = cache_file;
		ret = CE_None;
	}
	//    printf("GDALWMSCache::Read(...) -> %s\n", cache_file.c_str());

	return ret;
}

CPLString GDALWMSCache::KeyToCacheFile(const char *key) {
	CPLString hash(MD5String(key));
	CPLString cache_file(m_cache_path);

	if (cache_file.size() && (cache_file[cache_file.size() - 1] != '/')) cache_file.append(1, '/');
	for (int i = 0; i < m_cache_depth; ++i) {
		cache_file.append(1, hash[i]);
		cache_file.append(1, '/');
	}
	cache_file.append(hash);
	cache_file.append(m_postfix);
	return cache_file;
}
