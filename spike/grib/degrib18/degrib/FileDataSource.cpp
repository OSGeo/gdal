#include "FileDataSource.h"

FileDataSource::FileDataSource(const char * fileName)
: closeFile(true)
{
	fp = fopen(fileName, "rb");
}

FileDataSource::FileDataSource(FILE * fp)
: closeFile(false)
{
	this->fp = fp;
}

FileDataSource::~FileDataSource()
{
	if (closeFile)
		fclose(fp);
}

size_t FileDataSource::DataSourceFread(void* lpBuf, size_t size, size_t count)
{
	return fread(lpBuf, size, count, fp);
}

int FileDataSource::DataSourceFgetc()
{
	return fgetc(fp);
}

int FileDataSource::DataSourceUngetc(int c)
{
	return ungetc(c, fp);
}

int FileDataSource::DataSourceFseek(long offset, int origin)
{
	return fseek(fp, offset, origin);
}

int FileDataSource::DataSourceFeof()
{
	return feof(fp);	
}

long FileDataSource::DataSourceFtell()
{
	return ftell(fp);
}