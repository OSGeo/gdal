/******************************************************************************
 *
 * Component: Test ODS formula Engine
 * Purpose:
 * Author: Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2012, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ods_formula.h"

class MyCellEvaluator : public IODSCellEvaluator
{
  public:
    explicit MyCellEvaluator() = default;

    int EvaluateRange(int, int, int, int, std::vector<ods_formula_node> &)
    {
        return false;
    }
};

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        printf("Usage: testparser \"expression\"\n");
        return 1;
    }

    ods_formula_node *expr_out = ods_formula_compile(argv[1]);
    if (expr_out)
    {
        printf("Raw expression dump :\n");
        expr_out->Dump(stderr, 0);
        MyCellEvaluator cellEvaluator;
        if (expr_out->Evaluate(&cellEvaluator))
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
