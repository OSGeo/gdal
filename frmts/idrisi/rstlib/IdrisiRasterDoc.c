#include "IdrisiRasterDoc.h"
#include "IdrisiRasterUtils.h"

rst_Doc * CreateImgDoc()
{
	rst_Doc *imgDoc;

	imgDoc = (rst_Doc *) CALLOC((size_t) 1, sizeof(rst_Doc));

	strcpy(imgDoc->file_format, VALUE_DOC_FILE_FORMAT);
	strcpy(imgDoc->file_title, VALUE_DOC_TITLE);
	imgDoc->data_type = 0;
	strcpy(imgDoc->file_type, VALUE_DOC_BYNARY);
	imgDoc->columns = 0;
	imgDoc->rows = 0;
    strcpy(imgDoc->ref_system, VALUE_DOC_LATLONG);
    strcpy(imgDoc->ref_units, VALUE_DOC_DEGREE);
	imgDoc->unit_dist = 1.0;
	imgDoc->min_X = 0.0;
	imgDoc->max_X = 0.0;
	imgDoc->min_Y = 0.0;
	imgDoc->max_Y = 0.0;
	strcpy(imgDoc->posn_error, VALUE_DOC_UNKNOW);
	imgDoc->resolution = 1.0;
	imgDoc->min_value[0] = 0.0;
	imgDoc->min_value[1] = 0.0;
	imgDoc->min_value[2] = 0.0;
	imgDoc->max_value[0] = 0.0;
	imgDoc->max_value[1] = 0.0;
	imgDoc->max_value[2] = 0.0;
	imgDoc->display_min[0] = 0.0;
	imgDoc->display_min[1] = 0.0;
	imgDoc->display_min[2] = 0.0;
	imgDoc->display_max[0] = 0.0;
	imgDoc->display_max[1] = 0.0;
	imgDoc->display_max[2] = 0.0;
	strcpy(imgDoc->value_units, VALUE_DOC_UNSPECIFIED);
	strcpy(imgDoc->value_error, VALUE_DOC_UNKNOW);
	imgDoc->flag_value = 0.0;
	strcpy(imgDoc->flag_defn, VALUE_DOC_NONE);
	imgDoc->legend_cats = 0;
	imgDoc->categories = NULL;
	imgDoc->lineages = NULL;
	imgDoc->comments = NULL;
	imgDoc->codes = NULL;
	imgDoc->is_thematic = FALSE;

	return imgDoc;
}

void FreeImgDoc(rst_Doc *imgDoc)
{
	unsigned int i;

	if (imgDoc->legend_cats > 0) 
	{
		for (i = 0; i < imgDoc->legend_cats; i++)
			FREE(imgDoc->categories[i]);
		FREE(imgDoc->categories);
		FREE(imgDoc->codes);
	}

	if (imgDoc->lineages_count > 0) 
	{
		for (i = 0; i < imgDoc->lineages_count; i++)
			FREE(imgDoc->lineages[i]);
		FREE(imgDoc->lineages);
	}

	if (imgDoc->comments_count > 0) 
	{
		for (i = 0; i < imgDoc->comments_count; i++)
			FREE(imgDoc->comments[i]);
		FREE(imgDoc->comments);
	}

	FREE(imgDoc);
}

rst_Doc * ReadImgDoc(const char *fileName)
{
	rst_Doc *imgDoc;

	char aux[MINSTRLEN];
	char docFile[MAXSTRLEN];

	FILE *stream;

	imgDoc = CreateImgDoc();

	strcpy(docFile, fileName);
	PathRenameExtension(docFile, ".rdc");

	if((stream = fopen(docFile, "r")) != NULL )
	{
		strcpy(imgDoc->file_format,		ReadValueAsString(stream,		LABEL_DOC_FILE_FORMAT));
		strcpy(imgDoc->file_title,		ReadValueAsString(stream,		LABEL_DOC_FILE_TITLE));
		strcpy(aux,						ReadValueAsString(stream,		LABEL_DOC_DATA_TYPE));
		imgDoc->data_type =				DataTypeAsInteger(strlwr(aux));
		strcpy(imgDoc->file_type,		ReadValueAsString(stream,		LABEL_DOC_FILE_TYPE));
		imgDoc->columns =				ReadValueAsInteger(stream,		LABEL_DOC_COLUMNS);
		imgDoc->rows =					ReadValueAsInteger(stream,		LABEL_DOC_ROWS);
		strcpy(imgDoc->ref_system,		ReadValueAsString(stream,		LABEL_DOC_REF_SYSTEM));
		strcpy(imgDoc->ref_units,		strlwr(ReadValueAsString(stream,LABEL_DOC_REF_UNITS)));
		imgDoc->unit_dist =				ReadValueAsInteger(stream,		LABEL_DOC_UNIT_DIST);
		imgDoc->min_X =					ReadValueAsFloat(stream,		LABEL_DOC_MIN_X);
		imgDoc->max_X =					ReadValueAsFloat(stream,		LABEL_DOC_MAX_X);
		imgDoc->min_Y =					ReadValueAsFloat(stream,		LABEL_DOC_MIN_Y);	
		imgDoc->max_Y =					ReadValueAsFloat(stream,		LABEL_DOC_MAX_Y);
		strcpy(imgDoc->posn_error,		ReadValueAsString(stream,		LABEL_DOC_POSN_ERROR));
		strcpy(aux,						ReadValueAsString(stream,		LABEL_DOC_RESOLUTION));
		if (strcmp(aux, VALUE_DOC_UNKNOW) != 0) 
			imgDoc->resolution = atof(aux);			
		ReadValueAsArrayFloat(stream,	LABEL_DOC_MIN_VALUE,			imgDoc->min_value);
		ReadValueAsArrayFloat(stream,	LABEL_DOC_MAX_VALUE,			imgDoc->max_value);
		ReadValueAsArrayFloat(stream,	LABEL_DOC_DISPLAY_MIN,			imgDoc->display_min);
		ReadValueAsArrayFloat(stream,	LABEL_DOC_DISPLAY_MAX,			imgDoc->display_max);
		strcpy(imgDoc->value_units,		ReadValueAsString(stream,		LABEL_DOC_VALUE_UNITS));
		strcpy(imgDoc->value_error,		ReadValueAsString(stream,		LABEL_DOC_VALUE_ERROR));
		imgDoc->flag_value =			ReadValueAsFloat(stream,		LABEL_DOC_FLAG_VALUE);
		strcpy(imgDoc->flag_defn,		ReadValueAsString(stream,		LABEL_DOC_FLAG_DEFN));
		imgDoc->legend_cats =			ReadValueAsInteger(stream,		LABEL_DOC_LEGEND_CATS);
		if (imgDoc->legend_cats > 0)
			ReadValueAsLegendTable(stream, imgDoc->legend_cats, &imgDoc->categories, &imgDoc->codes);
		if ((imgDoc->legend_cats > 0) || 
			((imgDoc->max_value[0] - imgDoc->min_value[0]) <= GetMaxLegendsCats()))
		{
			imgDoc->is_thematic = TRUE;
		}
		ReadcommentsLines(stream, &imgDoc->lineages_count, &imgDoc->lineages, LABEL_DOC_lineages);
		ReadcommentsLines(stream, &imgDoc->comments_count, &imgDoc->comments, LABEL_DOC_comments);
		fclose(stream);
		return imgDoc;
	}
	else
		return NULL;
}
void WriteImgDoc(rst_Doc *imgDoc, char *fileName)
{
	FILE *stream;
	char *numeric_format;

	char docFile[MAXSTRLEN];
	unsigned long i;

	strcpy(docFile, fileName);

	PathRenameExtension(docFile, ".rdc");

	stream = fopen(docFile, "w");

	fprintf(stream, "%-12s: %s\n", 		LABEL_DOC_FILE_FORMAT, 	VALUE_DOC_FILE_FORMAT);
	fprintf(stream, "%-12s: %s\n", 		LABEL_DOC_FILE_TITLE, 	imgDoc->file_title);

	switch (imgDoc->data_type)
	{
		case 0 :
			fprintf(stream, "%-12s: %s\n", 	LABEL_DOC_DATA_TYPE, 	VALUE_DOC_BYTE);
			numeric_format = NUMERIC_FORMAT_INT;
			break;
		case 1 :
			fprintf(stream, "%-12s: %s\n", 	LABEL_DOC_DATA_TYPE, 	VALUE_DOC_INTEGER);
			numeric_format = NUMERIC_FORMAT_INT;
			break;
		case 2 :
			fprintf(stream, "%-12s: %s\n", 	LABEL_DOC_DATA_TYPE, 	VALUE_DOC_RGB);
			numeric_format = NUMERIC_FORMAT_INT;
			break;
		case 3 :
			fprintf(stream, "%-12s: %s\n", 	LABEL_DOC_DATA_TYPE, 	VALUE_DOC_REAL);
			numeric_format = NUMERIC_FORMAT_FLOAT;
	}
	fprintf(stream, "%-12s: %s\n", 		LABEL_DOC_FILE_TYPE, 	imgDoc->file_type);
	fprintf(stream, "%-12s: %d\n", 		LABEL_DOC_COLUMNS, 		imgDoc->columns);
	fprintf(stream, "%-12s: %d\n", 		LABEL_DOC_ROWS, 		imgDoc->rows);
	fprintf(stream, "%-12s: %s\n", 		LABEL_DOC_REF_SYSTEM, 	imgDoc->ref_system);
	fprintf(stream, "%-12s: %s\n", 		LABEL_DOC_REF_UNITS, 	imgDoc->ref_units);
	fprintf(stream, "%-12s: %.7f\n", 	LABEL_DOC_UNIT_DIST, 	imgDoc->unit_dist);
	fprintf(stream, "%-12s: %.7f\n", 	LABEL_DOC_MIN_X, 		imgDoc->min_X);
	fprintf(stream, "%-12s: %.7f\n", 	LABEL_DOC_MAX_X, 		imgDoc->max_X);
	fprintf(stream, "%-12s: %.7f\n", 	LABEL_DOC_MIN_Y, 		imgDoc->min_Y);
	fprintf(stream, "%-12s: %.7f\n", 	LABEL_DOC_MAX_Y, 		imgDoc->max_Y);
	fprintf(stream, "%-12s: %s\n",		LABEL_DOC_POSN_ERROR, 	imgDoc->posn_error);
	fprintf(stream, "%-12s: %.7f\n",	LABEL_DOC_RESOLUTION, 	imgDoc->resolution);

	if (imgDoc->data_type == 2)
	{
		fprintf(stream, "%-12s: %.0f %.0f %.0f\n", LABEL_DOC_MIN_VALUE, 	imgDoc->min_value[0],	imgDoc->min_value[1],	imgDoc->min_value[2]);
		fprintf(stream, "%-12s: %.0f %.0f %.0f\n", LABEL_DOC_MAX_VALUE, 	imgDoc->max_value[0],	imgDoc->max_value[1],	imgDoc->max_value[2]);
		fprintf(stream, "%-12s: %.0f %.0f %.0f\n", LABEL_DOC_DISPLAY_MIN, 	imgDoc->display_min[0], imgDoc->display_min[1], imgDoc->display_min[2]);
		fprintf(stream, "%-12s: %.0f %.0f %.0f\n", LABEL_DOC_DISPLAY_MAX, 	imgDoc->display_max[0], imgDoc->display_max[1], imgDoc->display_max[2]);
	}
	else
		{
			fprintf(stream, numeric_format, 	LABEL_DOC_MIN_VALUE, 	imgDoc->min_value[0]);
			fprintf(stream, numeric_format, 	LABEL_DOC_MAX_VALUE, 	imgDoc->max_value[0]);
			fprintf(stream, numeric_format, 	LABEL_DOC_DISPLAY_MIN, 	imgDoc->display_min[0]);
			fprintf(stream, numeric_format, 	LABEL_DOC_DISPLAY_MAX, 	imgDoc->display_max[0]);
		}
	fprintf(stream, "%-12s: %s\n", 				LABEL_DOC_VALUE_UNITS, 	imgDoc->value_units);
	fprintf(stream, "%-12s: %s\n",				LABEL_DOC_VALUE_ERROR, 	imgDoc->value_error);
	fprintf(stream, numeric_format,				LABEL_DOC_FLAG_VALUE, 	imgDoc->flag_value);
	fprintf(stream, "%-12s: %s\n", 				LABEL_DOC_FLAG_DEFN, 	imgDoc->flag_defn);
	fprintf(stream, "%-12s: %d\n", 				LABEL_DOC_LEGEND_CATS, 	imgDoc->legend_cats);

	for (i = 0; i < imgDoc->legend_cats; i++)
		fprintf(stream, "code %6d : %s\n", 		imgDoc->codes[i],		imgDoc->categories[i]);

	for (i = 0; i < imgDoc->lineages_count; i++)
		fprintf(stream, "%-12s: %s\n", 			LABEL_DOC_lineages,		imgDoc->lineages[i]);

	for (i = 0; i < imgDoc->comments_count; i++)
		fprintf(stream, "%-12s: %s\n", 			LABEL_DOC_comments,		imgDoc->comments[i]);

	fflush(stream);
	fclose(stream);
}
long ReadPalette(const char* fileName,
				 int rgb_index,
				 double *colorTable,
				 int rowCount, 
				 int thematic) 
{
	FILE *stream;
	unsigned char rgb_row[3];
	int i;
	char smpFile[MAXSTRLEN];

	strcpy(smpFile, fileName);

	PathRenameExtension(smpFile, ".smp");

	if (PathFileExists(smpFile) == FALSE)
	{
		if (thematic == TRUE)
			strcpy(smpFile, GetFromUserPreference("DefaultQualPal"));
		else
			strcpy(smpFile, GetFromUserPreference("DefaultQuantPal"));
	}

	CPLDebug("RST", "Pallet FileName = %s", smpFile);

	if (PathFileExists(smpFile) == TRUE)
	{
		if ((stream = fopen(smpFile, "r")) != NULL )
		{
			fseek(stream, PALHEADERSZ, SEEK_SET);
			i = 0;
			while ((fread(&rgb_row, sizeof(rgb_row), 1, stream)) && (i < rowCount))
			{
				colorTable[i] = (double) rgb_row[rgb_index] / 255;
				i++;
			}
			fclose(stream);
		}
	}
	else 
	{
		for (i = 0; i < rowCount; i++)
		{
			colorTable[i] = (float) i / 255;
		}
	}

	return SUCCESS;
}
void WritePalette(const char* fileName,
				 int rgb_index,
				 double *colorTable,
				 int rowCount) 
{
	FILE *stream;
	unsigned char rgb_row[3];
	int i;
	char docFile[MAXSTRLEN];

	strcpy(docFile, fileName);

	PathRenameExtension(docFile, ".smp");

	if ((stream = fopen(docFile, "a+")) != NULL )
	{
		fseek(stream, PALHEADERSZ, SEEK_SET);
		for (i = 0; i < 255; i++)
		{
			if (i < rowCount)
				rgb_row[rgb_index] = (unsigned char) colorTable[i] * 255;
			else
				rgb_row[rgb_index] = 0;
			fwrite(&rgb_row, sizeof(rgb_row), 1, stream); //?? There is an not obious error here!
		}
		fclose(stream);
	}
}
