#ifndef FILEDATASOURCE_H
#define FILEDATASOURCE_H

#include "datasource.h"
#include "cpl_vsi.h"

class FileDataSource : public DataSource
{
public:
	FileDataSource(const char * fileName);
	FileDataSource(VSILFILE* fp);
	virtual ~FileDataSource();
	virtual size_t DataSourceFread(void* lpBuf, size_t size, size_t count);
	virtual int DataSourceFgetc();
	virtual int DataSourceUngetc(int c);
	virtual int DataSourceFseek(long offset, int origin);
	virtual int DataSourceFeof();
	virtual long DataSourceFtell();
private:
	VSILFILE * fp;
	bool closeFile;
};

#endif /* FILEDATASOURCE_H */
