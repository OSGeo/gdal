#ifndef FILEDATASOURCE_H
#define FILEDATASOURCE_H

#include "datasource.h"
#include "cpl_vsi.h"

class FileDataSource : public DataSource
{
public:
        explicit FileDataSource(const char * fileName);
        explicit FileDataSource(VSILFILE* fp);
        virtual ~FileDataSource();
        virtual size_t DataSourceFread(void* lpBuf, size_t size, size_t count) override;
        virtual int DataSourceFgetc() override;
        virtual int DataSourceUngetc(int c) override;
        virtual int DataSourceFseek(long offset, int origin) override;
        virtual int DataSourceFeof() override;
        virtual long DataSourceFtell() override;
private:
        VSILFILE * fp;
        bool closeFile;
};

#endif /* FILEDATASOURCE_H */
