#include "stdinc.h"

static volatile GDALWMSMiniDriverManager *g_mini_driver_manager = NULL;
static void *g_mini_driver_manager_mutex = NULL;

GDALWMSMiniDriver::GDALWMSMiniDriver() {
}

GDALWMSMiniDriver::~GDALWMSMiniDriver() {
}

CPLErr GDALWMSMiniDriver::Initialize(CPLXMLNode *config) {
	return CE_None;
}

void GDALWMSMiniDriver::GetCapabilities(GDALWMSMiniDriverCapabilities *caps) {
}

void GDALWMSMiniDriver::ImageRequest(CPLString *url, const GDALWMSImageRequestInfo &iri) {
}

void GDALWMSMiniDriver::TiledImageRequest(CPLString *url, const GDALWMSImageRequestInfo &iri, const GDALWMSTiledImageRequestInfo &tiri) {
}

GDALWMSMiniDriverFactory::GDALWMSMiniDriverFactory() {
}

GDALWMSMiniDriverFactory::~GDALWMSMiniDriverFactory() {
}

GDALWMSMiniDriverManager *GetGDALWMSMiniDriverManager() {
	if (g_mini_driver_manager == 0) {
		CPLMutexHolderD(&g_mini_driver_manager_mutex);
		if (g_mini_driver_manager == 0) {
			g_mini_driver_manager = new GDALWMSMiniDriverManager();
		}
		CPLAssert(g_mini_driver_manager != NULL);
	}
	return const_cast<GDALWMSMiniDriverManager *>(g_mini_driver_manager);
}

GDALWMSMiniDriverManager::GDALWMSMiniDriverManager() {
}

GDALWMSMiniDriverManager::~GDALWMSMiniDriverManager() {
}

void GDALWMSMiniDriverManager::Register(GDALWMSMiniDriverFactory *mdf) {
	CPLMutexHolderD(&g_mini_driver_manager_mutex);

	m_mdfs.push_back(mdf);
}

GDALWMSMiniDriverFactory *GDALWMSMiniDriverManager::Find(const CPLString &name) {
	CPLMutexHolderD(&g_mini_driver_manager_mutex);

	for (std::list<GDALWMSMiniDriverFactory *>::iterator it = m_mdfs.begin(); it != m_mdfs.end(); ++it) {
		GDALWMSMiniDriverFactory *const mdf = *it;
		if (strcasecmp(mdf->GetName().c_str(), name.c_str()) == 0) return mdf;
	}
	return NULL;
}
