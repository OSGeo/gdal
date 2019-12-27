/******************************************************************************
 * $Id$
 *
 * Project:  GDAL
 * Purpose:  Run autotest/pytest from C
 * Author:   Even Rouault, even.rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2019, Hiroshi Miura <miurahr@linux.com>
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

#include <string>
#include <Python.h>


int main(int argc, char **argv)
{
    std::string args;
    if ( argc > 1) {
        args.append("[");
        for (int i = 1; i < argc; i++) {
            if (i > 2)
                args.append(",");
            args.append("\"");
            args.append(argv[i]);
            args.append("\"");
        }
        args.append("]");
    }
    std::string pycode = "import pytest\npytest.main(" + args + ")\n";

    wchar_t * program_name = Py_DecodeLocale(argv[0], NULL);
    Py_SetProgramName(program_name);
    Py_Initialize();
    PyRun_SimpleString(&*pycode.begin());
    Py_Finalize();
    return 0;
}