#include "stdinc.h"

CPP_GDALWMSMiniDriverFactory(WMS)

GDALWMSMiniDriver_WMS::GDALWMSMiniDriver_WMS() {
	//    printf("GDALWMSMiniDriver_WMS::GDALWMSMiniDriver_WMS()\n");
}

GDALWMSMiniDriver_WMS::~GDALWMSMiniDriver_WMS() {
}

CPLErr GDALWMSMiniDriver_WMS::Initialize(CPLXMLNode *config) {
	m_version = CPLGetXMLValue(config, "Version", "1.1.0");
	m_base_url = CPLGetXMLValue(config, "ServerUrl", "");
	m_crs = CPLGetXMLValue(config, "CRS", "CRS:83");
	m_srs = CPLGetXMLValue(config, "SRS", "EPSG:4326");
	m_image_format = CPLGetXMLValue(config, "ImageFormat", "image/jpeg");
	m_layers = CPLGetXMLValue(config, "Layers", "");
	m_styles = CPLGetXMLValue(config, "Styles", "");

	return CE_None;
}

void GDALWMSMiniDriver_WMS::GetCapabilities(GDALWMSMiniDriverCapabilities *caps) {
	caps->m_has_arb_overviews = 1;
	caps->m_has_image_request = 1;
	caps->m_has_tiled_image_requeset = 1;
	caps->m_max_overview_count = 32;
}

void GDALWMSMiniDriver_WMS::ImageRequest(CPLString *url, const GDALWMSImageRequestInfo &iri) {
	// http://onearth.jpl.nasa.gov/wms.cgi?request=GetMap&width=1000&height=500&layers=modis,global_mosaic&styles=&srs=EPSG:4326&format=image/jpeg&bbox=-180.000000,-90.000000,180.000000,090.000000    
	*url = m_base_url;
	URLAppend(url, "&request=GetMap");
	URLAppendF(url, "&version=%s", m_version.c_str());
	URLAppendF(url, "&layers=%s", m_layers.c_str());
	URLAppendF(url, "&styles=%s", m_styles.c_str());
	URLAppendF(url, "&srs=%s", m_srs.c_str());
	URLAppendF(url, "&format=%s", m_image_format.c_str());
	URLAppendF(url, "&width=%d", iri.m_sx);
	URLAppendF(url, "&height=%d", iri.m_sy);
	URLAppendF(url, "&bbox=%f,%f,%f,%f", iri.m_x0, std::min(iri.m_y0, iri.m_y1), iri.m_x1, std::max(iri.m_y0, iri.m_y1));
}

void GDALWMSMiniDriver_WMS::TiledImageRequest(CPLString *url, const GDALWMSImageRequestInfo &iri, const GDALWMSTiledImageRequestInfo &tiri) {
	ImageRequest(url, iri);
}
