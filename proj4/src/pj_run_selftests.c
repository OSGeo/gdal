/******************************************************************************
 * Project:  PROJ.4
 * Purpose:  Generic regression test for PROJ.4 projection algorithms.
 * Author:   Thomas Knudsen
 *
 ******************************************************************************
 * Copyright (c) 2016, Thomas Knudsen
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
 *****************************************************************************/

#include <stdio.h>
#define PJ_LIB__
#include <projects.h>


static void run_one_test (const char *mnemonic, int (testfunc)(void), int verbosity, int *n_ok, int *n_ko, int *n_stubs) {
    int ret = testfunc ();
    switch (ret) {
        case 0:      (*n_ok)++;     break;
        case 10000:  (*n_stubs)++;  break;
        default:     (*n_ko)++;
    }

    if (verbosity) {
        if (ret==10000)
            printf ("Testing: %10s - [stub]\n", mnemonic);
        else
            printf ("Testing: %10s - return code: %d\n", mnemonic, ret);
    }
    return;
}


#ifndef PJ_SELFTEST
int pj_run_selftests (int verbosity) {
    printf ("This version of libproj is not configured for internal regression tests.\n");
    return 0;
}
#else


int pj_run_selftests (int verbosity) {
    int n_ok = 0, n_ko = 0, n_stubs = 0, i = 0;

    struct PJ_SELFTEST_LIST *tests = pj_get_selftest_list_ref ();

    if (0==tests)
        printf ("This version of libproj is not configured for internal regression tests.\n");

    if (verbosity)
       printf ("Running internal regression tests\n");

    for (i = 0; tests[i].testfunc != 0; i++)
        run_one_test (tests[i].id,     tests[i].testfunc,      verbosity, &n_ok, &n_ko, &n_stubs);

    if (0==verbosity)
       printf ("Internal regression tests done. ");
    printf ("[Stubs: %d]          Total: %d. Failure: %d. Success: %d\n", n_stubs, n_ok+n_ko, n_ko, n_ok);
    return n_ko;
}
#endif
