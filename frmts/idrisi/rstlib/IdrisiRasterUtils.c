#include "IdrisiRasterUtils.h"
#include "IdrisiRasterDoc.h"

char * allocTextString(const char* text)
{
	char * newString;

	newString =	(char *) CALLOC((size_t) strlen(text) +	1, 1);
	strcpy(newString, text);

	return newString;
}
char * Backslash2Slash(char * str)
{
	unsigned int i, j;
	char in_str[MAXSTRLEN];
	strcpy(in_str, str);

	for (i = 0, j = 0; i < strlen(str); i++)
	{
		if (in_str[i] == '/')
		{
			str[j] = '\\';
			j++;
			str[j] = '\\';
		} else
			str[j] = in_str[i];
		j++;
	}
	str[j] = '\0';

	return str;
}
char * ReadValueAsString(FILE *stream, const char *Field)
{
	char line[MAXSTRLEN];
	char *out = "";

	while (fgets(line, MAXSTRLEN, stream) != NULL )
	{
		line[strlen(line) - 1] = '\0';
		line[strlen(Field)] = '\0';
		if (stricmp(line, Field) == 0)
		{
			out = allocTextString(&line[RDCSEPARATOR + 2]);
			break;
		}
	}

	return out;
}
int DataTypeAsInteger(const char * dataType)
{
	if (stricmp(dataType, VALUE_DOC_BYTE) == 0)
		return 0;
	else
		if (stricmp(dataType, VALUE_DOC_INTEGER) == 0)
			return 1;
		else
			if (stricmp(dataType, VALUE_DOC_RGB) == 0)
				return 2;
			else
				if (stricmp(dataType, VALUE_DOC_REAL) == 0)
					return 3;
				else
					return -1;
}
float ReadValueAsFloat(FILE *stream, const char *Field)
{
	char *out = ReadValueAsString(stream, Field);

	if (out == '\0')
		return (float) 0.0;
	else
		return (float) atof(out);

}
int ReadValueAsInteger(FILE *stream, const char *Field)
{
	char *out = ReadValueAsString(stream, Field);

	if (out == '\0')
		return (int) 0;
	else
		return (int) atoi(out);

}
void ReadValueAsArrayFloat(FILE *stream, const char *Field, double *Value)
{
	unsigned int r, g, b;
	char *out = ReadValueAsString(stream, Field);
	Value[0] = 0.0;
	Value[1] = 0.0;
	Value[2] = 0.0;

	if (out != '\0')
	{
		if (strchr(out, ' ') != NULL)
		{
			sscanf(out, "%d %d %d", &r, &g, &b);
			Value[0] = r;
			Value[1] = g;
			Value[2] = b;
		}
		else
		{
			Value[0] = atof(out);
		}
	}
}
char * ReadParameterString(const char *ParameterStr, const char *search_string)
{
	char *p1, *p2, *p3, *out;
	out = NULL;
	p1 = strstr(ParameterStr, search_string);
	if (p1 != NULL)
	{
		p1 += strlen(search_string) + 2;
		p2 = strchr(p1, '\"');
		p3 = strchr(p1, ']');
		if (p3 < p2)
			p2 = p3;
		out = (char *) CALLOC((size_t) (p2 - p1) + 1, 1);
		strncpy(out, p1, (p2 - p1));
		out[p2 - p1] = '\0';
	}
	return out;
}
float ReadParameterFloat(const char *ParameterStr, const char *search_string)
{
	char *out = ReadParameterString(ParameterStr, search_string);

	if (out == '\0')
		return (float) 0.0;
	else
		return (float) atof(out);
}

void ReadValueAsLegendTable(FILE *stream, unsigned long numCats, char *** categories, unsigned long ** codes)
{
	unsigned long i;
	char line[MAXSTRLEN];

	*categories = (char **) CALLOC((size_t) numCats,    sizeof(char *));
	*codes = (unsigned long *) CALLOC((size_t) numCats, sizeof(unsigned long));

	for (i = 0; i < numCats; i ++)
	{
		if (fgets(line, MAXSTRLEN, stream) != NULL )
		{
			line[strlen(line) - 1] = '\0';
			line[RDCSEPARATOR] = '\0';
			if (strstr(line, "code") != NULL)
			{
				sscanf(line, "code %d :", &codes[0][i]);
				(*categories)[i] = allocTextString(&line[RDCSEPARATOR + 2]);
			}
		}
	}
}
void ReadcommentsLines(FILE *stream, unsigned int *counter, char *** comments_lines, const char *search_string)
{
	char aux[MAXSTRLEN];

	unsigned int i;
	unsigned int count;

	count = 0;
	strcpy(aux, ReadValueAsString(stream, search_string));
	while (aux[0] != '\0')
	{
		strcpy(aux, ReadValueAsString(stream, search_string));
		count++;
	} 

	if (count == 0)
		return;

	fseek(stream, 0, 0);

	*comments_lines = (char **) CALLOC((size_t) count, sizeof(char *));

	for (i = 0; i < count; i++)
	{
		strcpy(aux, ReadValueAsString(stream, search_string));
		(*comments_lines)[i] = allocTextString(aux);
	}

	*counter = count;
}
void FindInIdrisiInstallation(char * pathName, char * fileName)
{
	unsigned long buffer_size;
	char buffer[MINSTRLEN];
	char *pos;
	char outFile[MAXSTRLEN];

	if (RegQueryValue(HKEY_CLASSES_ROOT, "Applications\\idrisi32.exe\\shell\\open\\command", buffer, &buffer_size) == ERROR_SUCCESS)
	{
		pos = strstr(buffer, "idrisi32.exe");
		if (pos != NULL)
		{
			*pos = '\0';
			sprintf(outFile, "%s%s", buffer, pathName);
			if (PathFileExists(outFile))
				strcpy(fileName, outFile);
		}
	}
}
char * GetFromUserPreference(char * key)
{
	static char result[MINSTRLEN];
	char outFile[MAXSTRLEN];
	char line[MINSTRLEN];
	char *pos;
	FILE *stream;

	result[0] = '\0';

	FindInIdrisiInstallation("Idrusers.ini", outFile);

	if (PathFileExists(outFile))
	{
		stream = fopen(outFile, "r");
		while (fgets(line, MAXSTRLEN, stream) != NULL )
		{
			if (strstr(line, key) != NULL)
			{
				pos = strchr(line, '=');
				strcpy(result, pos + 1);
				result[strlen(result) - 1] = '\0';
				break;
			}
		}
		fclose(stream);
	}

	return result;
}
int GetMaxLegendsCats()
{
	int result;

	if (max_LegendCats != -1)
		return max_LegendCats;

	result = MAXLEGENDDEFAULT;

	result = atoi(GetFromUserPreference("MaxVisibleLegendCats"));

	max_LegendCats = result;

	return result;
}