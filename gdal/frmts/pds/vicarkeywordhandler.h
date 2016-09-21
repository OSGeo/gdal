/******************************************************************************
 *
 * Project:  VICAR Driver; JPL/MIPL VICAR Format
 * Purpose:  Implementation of VICARKeywordHandler - a class to read
 *           keyword data from VICAR data products.
 * Authors:  Sebastian Walter <sebastian dot walter at fu-berlin dot de>
 *
 * NOTE: This driver code is loosely based on the ISIS and PDS drivers.
 * It is not intended to diminish the contribution of the authors.
 ******************************************************************************
 * Copyright (c) 2014, Sebastian Walter <sebastian dot walter at fu-berlin dot de>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/
class VICARKeywordHandler
{
    char     **papszKeywordList;

    CPLString osHeaderText;
    const char *pszHeaderNext;

    int     LabelSize;

    void    SkipWhite();
    int     ReadWord( CPLString &osWord );
    int     ReadPair( CPLString &osName, CPLString &osValue );
    int     ReadGroup( const char *pszPathPrefix );

public:
    VICARKeywordHandler();
    ~VICARKeywordHandler();

    int     Ingest( VSILFILE *fp, GByte *pabyHeader );

    const char *GetKeyword( const char *pszPath, const char *pszDefault );
    char **GetKeywordList();
};
