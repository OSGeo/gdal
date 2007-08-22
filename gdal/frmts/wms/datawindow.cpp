#include "stdinc.h"

CPLErr GDALWMSDataWindow::Initialize(CPLXMLNode *config) {
	const char *ulx = CPLGetXMLValue(config, "UpperLeftX", "");
	const char *uly = CPLGetXMLValue(config, "UpperLeftY", "");
	const char *lrx = CPLGetXMLValue(config, "LowerRightX", "");
	const char *lry = CPLGetXMLValue(config, "LowerRightY", "");
	const char *sx = CPLGetXMLValue(config, "SizeX", "");
	const char *sy = CPLGetXMLValue(config, "SizeY", "");

	if ((ulx[0] != '\0') && (uly[0] != '\0') && (lrx[0] != '\0') && (lry[0] != '\0') && (sx[0] != '\0') && (sy[0] != '\0')) {
		m_x0 = atof(ulx);
		m_y0 = atof(uly);
		m_x1 = atof(lrx);
		m_y1 = atof(lry);
		m_sx = atoi(sx);
		m_sy = atoi(sy);

//		printf("DataWindow: %f %f %f %f %d %d\n", m_x0, m_y0, m_x1, m_y1, m_sx, m_sy);

		return CE_None;
	}

	return CE_Failure;
}
