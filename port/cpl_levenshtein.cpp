// SPDX-License-Identifier: MIT
// Copyright (c) 2019, Guilherme Agostinelli
// Ported from https://github.com/guilhermeagostinelli/levenshtein/blob/master/levenshtein.cpp

#include "cpl_levenshtein.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <vector>

/** Computes the Levenshtein distance between 2 words.
 *
 * If transpositionAllowed = true, then it is the Damerau-Levenshtein distance
 *
 * Return SIZE_T_MAX in case of error.
 */
size_t CPLLevenshteinDistance(const char *word1, const char *word2,
                              bool transpositionAllowed)
{
    const size_t size1 = strlen(word1);
    const size_t size2 = strlen(word2);

    // If one of the words has zero length, the distance is equal to the size of the other word.
    if (size1 == 0)
        return size2;
    if (size2 == 0)
        return size1;

    // Would cause enormous amount of memory to be allocated
    if (size1 >= 32768 || size2 >= 32768)
    {
        return strcmp(word1, word2) == 0 ? 0
                                         : std::numeric_limits<size_t>::max();
    }

    // Verification matrix i.e. 2D array which will store the calculated distance.
    const size_t dimFastSize = size2 + 1;
    std::vector<unsigned short> verif;
    try
    {
        verif.resize((size1 + 1) * dimFastSize);
    }
    catch (const std::exception &)
    {
        return std::numeric_limits<size_t>::max();
    }

    const auto verifVal = [&verif, dimFastSize](size_t i,
                                                size_t j) -> unsigned short &
    { return verif[i * dimFastSize + j]; };

    // Sets the first row and the first column of the verification matrix with the numerical order from 0 to the length of each word.
    for (size_t i = 0; i <= size1; i++)
        verifVal(i, 0) = static_cast<unsigned short>(i);
    for (size_t j = 0; j <= size2; j++)
        verifVal(0, j) = static_cast<unsigned short>(j);

    // Verification step / matrix filling.
    for (size_t i = 1; i <= size1; i++)
    {
        for (size_t j = 1; j <= size2; j++)
        {
            // Sets the modification cost.
            // 0 means no modification (i.e. equal letters) and 1 means that a modification is needed (i.e. unequal letters).
            const int cost = (word2[j - 1] == word1[i - 1]) ? 0 : 1;

            // Sets the current position of the matrix as the minimum value between a (deletion), b (insertion) and c (substitution).
            // a = the upper adjacent value plus 1: verif[i - 1][j] + 1
            // b = the left adjacent value plus 1: verif[i][j - 1] + 1
            // c = the upper left adjacent value plus the modification cost: verif[i - 1][j - 1] + cost
            verifVal(i, j) = std::min(
                std::min(static_cast<unsigned short>(verifVal(i - 1, j) + 1),
                         static_cast<unsigned short>(verifVal(i, j - 1) + 1)),
                static_cast<unsigned short>(verifVal(i - 1, j - 1) + cost));

            // Cf https://en.wikipedia.org/wiki/Damerau%E2%80%93Levenshtein_distance#Optimal_string_alignment_distance
            // (note that in the Wikipedia page, a[] and b[] are indexed from 1...
            if (transpositionAllowed && i > 1 && j > 1 &&
                word1[i - 1] == word2[j - 2] && word1[i - 2] == word2[j - 1])
            {
                verifVal(i, j) = std::min(
                    verifVal(i, j),
                    static_cast<unsigned short>(verifVal(i - 2, j - 2) + 1));
            }
        }
    }

    // The last position of the matrix will contain the Levenshtein distance.
    return verifVal(size1, size2);
}
