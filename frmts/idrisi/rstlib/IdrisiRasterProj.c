#include "IdrisiRasterDoc.h"
#include "idrisiRasterProj.h"
#include "idrisiRasterUtils.h"

rst_Ref * CreateImgRef()
{
	rst_Ref * imgRef;

	imgRef = (rst_Ref *) CALLOC((size_t) sizeof(rst_Ref), 1);

	imgRef->fileName = NULL;
	imgRef->RefName = (rst_RefName *) CALLOC((size_t) sizeof(rst_RefName), 1);
	imgRef->name = NULL;
	imgRef->Datum = (rst_Datum *) CALLOC((size_t) sizeof(rst_Datum), 1);
	imgRef->originLatitude = 0.0;
	imgRef->originLongitude = 0.0;
	imgRef->originX = 0.0;
	imgRef->originY = 0.0;
	imgRef->scaleFactor = 0.0;
	imgRef->Unit = (rst_Unit *) CALLOC((size_t) sizeof(rst_Unit), 1);

	return imgRef;
}

void FreeImgRef(rst_Ref *imgRef)
{
	if (imgRef->name != NULL)
		FREE(imgRef->name);

	if (imgRef->fileName != NULL)
		FREE(imgRef->fileName);

	if  ((imgRef->RefName != NULL) &&
		((imgRef->RefName < &rstRefNames[0]) || (imgRef->RefName > &rstRefNames[REFNAMESARRAYCOUNT])))
		FREE(imgRef->RefName);

	if  ((imgRef->Datum != NULL) &&
		((imgRef->Datum < &rstDatums[0]) || (imgRef->Datum > &rstDatums[DATUMSARRAYCOUNT])))
		FREE(imgRef->Datum);

	if  ((imgRef->Unit != NULL) &&
		((imgRef->Unit < &rstUnits[0]) || (imgRef->Unit > &rstUnits[UNITSARRAYCOUNT])))
		FREE(imgRef->Unit);

	FREE(imgRef);
}
rst_Ref * ReadImgRef(const char *fileName)
{
	unsigned long cat_count = 0;
	unsigned int i;
	unsigned int match = FALSE;
	rst_Ref *imgRef;
	char aux[MINSTRLEN];

	FILE *stream;

	imgRef = CreateImgRef();

	if((stream = fopen(fileName, "r")) != NULL )
	{
		strcpy(aux,		ReadValueAsString(stream,	LABEL_REF_REF_SYSTEM));
		imgRef->name =  allocTextString(aux);
		strcpy(aux,		ReadValueAsString(stream,	LABEL_REF_PROJECTION));
		imgRef->projection = allocTextString(aux); 
		match = FALSE;
		for (i = 0; i < REFNAMESARRAYCOUNT; i++)
		{
			if ((strcmp(aux, rstRefNames[i].name) == 0) || 
				(strcmp(aux, rstRefNames[i].esriName) == 0))
			{
				imgRef->RefName	= &rstRefNames[i];
				match = TRUE;
				break;
			}
		}
		if (match == FALSE)
		{
			imgRef->RefName->name				= allocTextString(aux);
			imgRef->RefName->esriName			= allocTextString(aux);
		}
		strcpy(aux,					ReadValueAsString(stream,		LABEL_REF_DATUM));
		for (i = 0; i < DATUMSARRAYCOUNT; i++)
		{
			if ((strcmp(aux, rstDatums[i].name) == 0) || 
				(strcmp(aux, rstDatums[i].esriName) == 0))
			{
				imgRef->Datum = &rstDatums[i];
				match = TRUE;
				break;
			}
		}
		if (match == FALSE)
		{
			imgRef->Datum->name					= allocTextString(aux);
			imgRef->Datum->esriName				= allocTextString(aux);
			strcpy(aux,							  ReadValueAsString(stream,		LABEL_REF_ELLIPSOID));
			imgRef->Datum->ellipsoidName		= allocTextString(aux);
		}
		imgRef->majorSemiAxis					= ReadValueAsFloat(stream,		LABEL_REF_MAJOR_S_AX);
		imgRef->minorSemiAxis					= ReadValueAsFloat(stream,		LABEL_REF_MINOR_S_AX);
		imgRef->originLongitude					= ReadValueAsFloat(stream,		LABEL_REF_ORIGIN_LONG);
		imgRef->originLatitude					= ReadValueAsFloat(stream,		LABEL_REF_ORIGIN_LAT);
		imgRef->originX							= ReadValueAsFloat(stream,		LABEL_REF_ORIGIN_X);
		imgRef->originY							= ReadValueAsFloat(stream,		LABEL_REF_ORIGIN_Y);
		imgRef->scaleFactor						= ReadValueAsFloat(stream,		LABEL_REF_SCALE_FAC);
		strcpy(aux,								  ReadValueAsString(stream,		LABEL_REF_UNITS));
		match = FALSE;
		for (i = 0; i < UNITSARRAYCOUNT; i++)
		{
			if (stricmp(aux, rstUnits[i].name) == 0)
			{
				imgRef->Unit = &rstUnits[i];
				match = TRUE;
				break;
			}
		}
		if (match == FALSE)
		{
			imgRef->Unit->name					= allocTextString(aux);
			imgRef->Unit->esriName				= allocTextString(aux);
			imgRef->Unit->meters				= 1.0;
		}
		fclose(stream);
	}
	return imgRef;
}
void WriteimgRef(rst_Ref *imgRef, char *fileName)
{
	FILE *stream;

	char ProjFile[MAXSTRLEN];

	strcpy(ProjFile, fileName);

	PathRenameExtension(ProjFile, ".ref");

	stream = fopen(ProjFile, "w");

	fprintf(stream, "%-12s: %s\n", 		LABEL_REF_REF_SYSTEM,	imgRef->name);
	fprintf(stream, "%-12s: %s\n", 		LABEL_REF_PROJECTION,  	imgRef->RefName->name);
	fprintf(stream, "%-12s: %s\n", 		LABEL_REF_DATUM,      	imgRef->Datum->name);
	fprintf(stream, "%-12s: %s\n", 		LABEL_REF_DELTA_WGS84,	"0 0 0");
	fprintf(stream, "%-12s: %s\n", 		LABEL_REF_ELLIPSOID,	imgRef->Datum->ellipsoidName);
	fprintf(stream, "%-12s: %.7f\n", 	LABEL_REF_MAJOR_S_AX,  	imgRef->majorSemiAxis);
	fprintf(stream, "%-12s: %.7f\n", 	LABEL_REF_MINOR_S_AX, 	imgRef->minorSemiAxis);
	fprintf(stream, "%-12s: %.7f\n", 	LABEL_REF_ORIGIN_LONG,	imgRef->originLongitude);
	fprintf(stream, "%-12s: %.7f\n", 	LABEL_REF_ORIGIN_LAT, 	imgRef->originLatitude);
	fprintf(stream, "%-12s: %.7f\n", 	LABEL_REF_ORIGIN_X,   	imgRef->originX);
	fprintf(stream, "%-12s: %.7f\n", 	LABEL_REF_ORIGIN_Y,   	imgRef->originY);
	fprintf(stream, "%-12s: %.7f\n", 	LABEL_REF_SCALE_FAC,  	imgRef->scaleFactor);
	fprintf(stream, "%-12s: %s\n",		LABEL_REF_UNITS,       	imgRef->Unit->name);
	fprintf(stream, "%-12s: %.7f\n",	LABEL_REF_PARAMETERS,  	0);

	fflush(stream);
	fclose(stream);
}
char * ReadProjSystem(char *fileName)
{
	static char geoegraphic_cs[MAXPESTRING];
	static char projected_cs[MAXPESTRING];

	char refName[MINSTRLEN];
	char refFile[MAXSTRLEN];
	char refPath[MINSTRLEN];

	rst_Doc *imgDoc;
	rst_Ref *imgRef;

	imgRef = NULL;

	imgDoc = ReadImgDoc(fileName);

	if (stricmp(imgDoc->ref_system, VALUE_DOC_LATLONG) == 0)
	{
		strcpy(projected_cs, "GEOGCS[\"GCS_WGS_1984\",DATUM[\"D_WGS_1984\",SPHEROID[\"WGS_1984\",6378137.0,298.257223563]],PRIMEM[\"Greenwich\",0.0],UNIT[\"Degree\",0.0174532925199433]]");
		FreeImgDoc(imgDoc);
		return projected_cs;
	}
	else
	if (stricmp(imgDoc->ref_system, VALUE_DOC_PLANE) == 0)
	{
		strcpy(projected_cs, "GEOGCS[\"unnamed\",DATUM[\"unknown\",SPHEROID[\"unretrievable - using WGS84\",6378137,298.257223563]],PRIMEM[\"Greenwich\",0],UNIT[\"unknown\",0.0174532925199433]]");
		FreeImgDoc(imgDoc);
		return projected_cs;
	}
	else
	{
		strcpy(refName, imgDoc->ref_system);
		strlwr(refName);
		strcpy(refFile, fileName);
		Backslash2Slash(refFile);
		PathRemoveFileSpec(refFile);
		sprintf(refFile, "%s\\%s.ref", refFile, refName);


		if (PathFileExists(refFile) == FALSE)
		{
			sprintf(refPath, "Georef//%s.ref", refName);
			FindInIdrisiInstallation(refPath, refFile);
		}

		if (PathFileExists(refFile))
		{
			imgRef = ReadImgRef(refFile);
		}

		if (imgRef != NULL)
		{
			imgRef->fileName = allocTextString(imgDoc->ref_system);
		}
	}

	FreeImgDoc(imgDoc);

	sprintf(geoegraphic_cs, "GEOGCS[\"%s\"," \
		"DATUM[\"%s\"," \
		"SPHEROID[\"%s\"," \
		"%.0f," \
		"%.9f]]," \
		"PRIMEM[\"Greenwich\",0],UNIT[\"Degree\",%.18f]]",
		imgRef->Datum->esriName,
		imgRef->Datum->esriName,
		imgRef->Datum->ellipsoidName,
		imgRef->majorSemiAxis, 
		INVERSEFLATTENING,
		DEGREE2METERS
		);

	if (stricmp(imgRef->projection, "none") == 0)
	{
		FreeImgRef(imgRef);
		return geoegraphic_cs;	
	}							
	else						
	{							
		sprintf(projected_cs, 	"PROJCS[\"%s\",%s," \
			"PROJECTION[\"%s\"]," \
			"PARAMETER[\"False_Easting\",%.4f]," \
			"PARAMETER[\"False_Northing\",%.4f]," \
			"PARAMETER[\"Central_Meridian\",%.4f]," \
			"PARAMETER[\"Scale_Factor\",%.4f]," \
			"PARAMETER[\"Latitude_of_Origin\",%.4f]," \
			"UNIT[\"%s\",%.4f]]",
			imgRef->fileName,
			geoegraphic_cs,
			imgRef->RefName->esriName,
			imgRef->originX / imgRef->Unit->meters, 
			imgRef->originY / imgRef->Unit->meters, 
			imgRef->originLongitude, 
			imgRef->scaleFactor, 
			imgRef->originLatitude, 
			imgRef->Unit->esriName,
			imgRef->Unit->meters
			);
		FreeImgRef(imgRef);
		return projected_cs;
	}
}

void WriteProjSystem(char *peString, char *fileName)
{
	rst_Doc	*imgDoc;
	rst_Ref *imgRef;
	char refFile[MAXSTRLEN];
	char *projName;
	char *unitName;
	char *refName;
	char aux[MINSTRLEN];
	unsigned int i;
	unsigned int match = FALSE;

	if (strncmp(peString, "PROJCS", 6) == 0)
	{
		imgRef = CreateImgRef();

		refName = ReadParameterString(peString, "PROJCS");

		strcpy(refFile, fileName);
		Backslash2Slash(refFile);
		PathRemoveFileSpec(refFile);

		sprintf(refFile, "%s\\%s.ref", refFile, refName);

		if (PathFileExists(refFile))
		{
			match = TRUE;
		}
		if (match == FALSE)
		{
			imgRef->fileName		= ReadParameterString(peString, "PROJCS");
			projName				= ReadParameterString(peString, "PROJECTION");
			imgRef->originX			= ReadParameterFloat(peString,  "False_Easting");
			imgRef->originY			= ReadParameterFloat(peString,  "False_Northing");
			imgRef->originLongitude	= ReadParameterFloat(peString,  "Central_Meridian");
			imgRef->scaleFactor		= ReadParameterFloat(peString,  "Scale_Factor");
			imgRef->originLatitude	= ReadParameterFloat(peString,  "Latitude_of_Origin");
			unitName				= ReadParameterString(peString, "UNIT");
			for (i = 0; i < REFNAMESARRAYCOUNT; i++)
			{
				if (strcmp(projName, rstRefNames[i].esriName) == 0)
				{
					imgRef->RefName->name = allocTextString(rstRefNames[i].name);
					imgRef->RefName->esriName = allocTextString(rstRefNames[i].esriName);
					break;
				}
			}
			for (i = 0; i < UNITSARRAYCOUNT; i++)
			{
				if (stricmp(unitName, rstUnits[i].name) == 0)
				{
					imgRef->Unit->name = allocTextString(rstUnits[i].name);
					imgRef->Unit->esriName = allocTextString(rstUnits[i].esriName);
					imgRef->Unit->meters = rstUnits[i].meters;
					break;
				}
			}
			imgRef->Datum = (rst_Datum * ) CALLOC((size_t) sizeof(rst_Datum), 1);
			imgRef->Datum->name = ReadParameterString(peString, "SPHEROID");;
			imgRef->Datum->esriName = ReadParameterString(peString, "DATUM");
			imgRef->Datum->ellipsoidName = ReadParameterString(peString, "SPHEROID");
			sprintf(aux, "SPHEROID[\"%s", imgRef->Datum->ellipsoidName);
			imgRef->majorSemiAxis = ReadParameterFloat(peString, aux);
			imgRef->minorSemiAxis = -1 * ((imgRef->majorSemiAxis / INVERSEFLATTENING) - imgRef->majorSemiAxis);

			WriteimgRef(imgRef, refFile);
		}
		imgDoc = ReadImgDoc(fileName);
		strcpy(imgDoc->ref_system, imgRef->fileName);
		strcpy(imgDoc->ref_units, imgRef->Unit->name);
		WriteImgDoc(imgDoc, fileName);
		FreeImgDoc(imgDoc);

		FreeImgRef(imgRef);
	}

}
