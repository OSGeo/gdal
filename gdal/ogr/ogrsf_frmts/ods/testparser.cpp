/******************************************************************************
 * $Id$
 *
 * Component: Test ODS formula Engine
 * Purpose:
 * Author: Even Rouault <even dot rouault at mines dash paris dot org>
 *
 ******************************************************************************
 * Copyright (c) 2012, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "ods_formula.h"

int main(int argc, char* argv[])
{
    if (argc != 2)
    {
        printf("Usage: testparser \"expression\"\n");
        return 1;
    }

    ods_formula_node* expr_out = ods_formula_compile( argv[1] );
    if (expr_out)
    {
        printf("Raw expression dump :\n");
        expr_out->Dump(stderr, 0);
        if (expr_out->Evaluate(NULL))
        {
            printf("After evaluation :\n");
            expr_out->Dump(stderr, 0);
        }
        else
        {
            printf("Error during evaluation\n");
        }
    }
    else
        printf("Invalid expression\n");
    delete expr_out;
    return 0;
}
