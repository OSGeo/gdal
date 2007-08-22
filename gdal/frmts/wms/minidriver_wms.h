H_GDALWMSMiniDriverFactory(WMS)

class GDALWMSMiniDriver_WMS : public GDALWMSMiniDriver {
public:
	GDALWMSMiniDriver_WMS();
	virtual ~GDALWMSMiniDriver_WMS();

public:
	virtual CPLErr Initialize(CPLXMLNode *config);
	virtual void GetCapabilities(GDALWMSMiniDriverCapabilities *caps);
	virtual void ImageRequest(CPLString *url, const GDALWMSImageRequestInfo &iri);
	virtual void TiledImageRequest(CPLString *url, const GDALWMSImageRequestInfo &iri, const GDALWMSTiledImageRequestInfo &tiri);

protected:
	CPLString m_base_url;
	CPLString m_version;
	CPLString m_layers;
	CPLString m_styles;
	CPLString m_srs;
	CPLString m_crs;
	CPLString m_image_format;
};
