#include "ogr_cad.h"
#include "cpl_conv.h"

OGRCADLayer::OGRCADLayer( const char * pszFilename )
{
	nNextFID = 0;

	poFeatureDefn = new OGRFeatureDefn( CPLGetBasename( pszFilename ) );
	SetDescription( poFeatureDefn->GetName() );
	poFeatureDefn->Reference();

	OGRFieldDefn oFieldTemplate( "Name", OFTString );

	poFeatureDefn->AddFieldDefn( &oFieldTemplate );
}

OGRCADLayer::~OGRCADLayer()
{
	poFeatureDefn->Release();
}

void OGRCADLayer::ResetReading()
{
	nNextFID = 0;
}

OGRFeature *OGRCADLayer::GetNextFeature()
{
	return NULL;
}