/* A Bison parser, made by GNU Bison 3.0.  */

/* Bison implementation for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2013 Free Software Foundation, Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.

   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

/* C LALR(1) parser skeleton written by Richard Stallman, by
   simplifying the original so-called "semantic" parser.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Identify Bison output.  */
#define YYBISON 1

/* Bison version.  */
#define YYBISON_VERSION "3.0"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 1

/* Push parsers.  */
#define YYPUSH 0

/* Pull parsers.  */
#define YYPULL 1


/* Substitute the variable and function names.  */
#define yyparse         swqparse
#define yylex           swqlex
#define yyerror         swqerror
#define yydebug         swqdebug
#define yynerrs         swqnerrs


/* Copy the first part of user declarations.  */
#line 1 "swq_parser.y" /* yacc.c:339  */

/******************************************************************************
 *
 * Component: OGR SQL Engine
 * Purpose: expression and select parser grammar.
 *          Requires Bison 2.4.0 or newer to process.  Use "make parser" target.
 * Author: Frank Warmerdam <warmerdam@pobox.com>
 * 
 ******************************************************************************
 * Copyright (C) 2010 Frank Warmerdam <warmerdam@pobox.com>
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


#include "cpl_conv.h"
#include "cpl_string.h"
#include "swq.h"

#define YYSTYPE  swq_expr_node*

/* Defining YYSTYPE_IS_TRIVIAL is needed because the parser is generated as a C++ file. */ 
/* See http://www.gnu.org/s/bison/manual/html_node/Memory-Management.html that suggests */ 
/* increase YYINITDEPTH instead, but this will consume memory. */ 
/* Setting YYSTYPE_IS_TRIVIAL overcomes this limitation, but might be fragile because */ 
/* it appears to be a non documented feature of Bison */ 
#define YYSTYPE_IS_TRIVIAL 1


#line 118 "swq_parser.cpp" /* yacc.c:339  */

# ifndef YY_NULL
#  if defined __cplusplus && 201103L <= __cplusplus
#   define YY_NULL nullptr
#  else
#   define YY_NULL 0
#  endif
# endif

/* Enabling verbose error messages.  */
#ifdef YYERROR_VERBOSE
# undef YYERROR_VERBOSE
# define YYERROR_VERBOSE 1
#else
# define YYERROR_VERBOSE 1
#endif

/* In a future release of Bison, this section will be replaced
   by #include "swq_parser.hpp".  */
#ifndef YY_SWQ_SWQ_PARSER_HPP_INCLUDED
# define YY_SWQ_SWQ_PARSER_HPP_INCLUDED
/* Debug traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif
#if YYDEBUG
extern int swqdebug;
#endif

/* Token type.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
  enum yytokentype
  {
    END = 0,
    SWQT_NUMBER = 258,
    SWQT_STRING = 259,
    SWQT_IDENTIFIER = 260,
    SWQT_IN = 261,
    SWQT_LIKE = 262,
    SWQT_ESCAPE = 263,
    SWQT_BETWEEN = 264,
    SWQT_NULL = 265,
    SWQT_IS = 266,
    SWQT_SELECT = 267,
    SWQT_LEFT = 268,
    SWQT_JOIN = 269,
    SWQT_WHERE = 270,
    SWQT_ON = 271,
    SWQT_ORDER = 272,
    SWQT_BY = 273,
    SWQT_FROM = 274,
    SWQT_AS = 275,
    SWQT_ASC = 276,
    SWQT_DESC = 277,
    SWQT_DISTINCT = 278,
    SWQT_CAST = 279,
    SWQT_UNION = 280,
    SWQT_ALL = 281,
    SWQT_LOGICAL_START = 282,
    SWQT_VALUE_START = 283,
    SWQT_SELECT_START = 284,
    SWQT_NOT = 285,
    SWQT_OR = 286,
    SWQT_AND = 287,
    SWQT_UMINUS = 288,
    SWQT_RESERVED_KEYWORD = 289
  };
#endif

/* Value type.  */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef int YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif



int swqparse (swq_parse_context *context);

#endif /* !YY_SWQ_SWQ_PARSER_HPP_INCLUDED  */

/* Copy the second part of user declarations.  */

#line 204 "swq_parser.cpp" /* yacc.c:358  */

#ifdef short
# undef short
#endif

#ifdef YYTYPE_UINT8
typedef YYTYPE_UINT8 yytype_uint8;
#else
typedef unsigned char yytype_uint8;
#endif

#ifdef YYTYPE_INT8
typedef YYTYPE_INT8 yytype_int8;
#else
typedef signed char yytype_int8;
#endif

#ifdef YYTYPE_UINT16
typedef YYTYPE_UINT16 yytype_uint16;
#else
typedef unsigned short int yytype_uint16;
#endif

#ifdef YYTYPE_INT16
typedef YYTYPE_INT16 yytype_int16;
#else
typedef short int yytype_int16;
#endif

#ifndef YYSIZE_T
# ifdef __SIZE_TYPE__
#  define YYSIZE_T __SIZE_TYPE__
# elif defined size_t
#  define YYSIZE_T size_t
# elif ! defined YYSIZE_T
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# else
#  define YYSIZE_T unsigned int
# endif
#endif

#define YYSIZE_MAXIMUM ((YYSIZE_T) -1)

#ifndef YY_
# if defined YYENABLE_NLS && YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> /* INFRINGES ON USER NAME SPACE */
#   define YY_(Msgid) dgettext ("bison-runtime", Msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(Msgid) Msgid
# endif
#endif

#ifndef __attribute__
/* This feature is available in gcc versions 2.5 and later.  */
# if (! defined __GNUC__ || __GNUC__ < 2 \
      || (__GNUC__ == 2 && __GNUC_MINOR__ < 5))
#  define __attribute__(Spec) /* empty */
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YYUSE(E) ((void) (E))
#else
# define YYUSE(E) /* empty */
#endif

#if defined __GNUC__ && 407 <= __GNUC__ * 100 + __GNUC_MINOR__
/* Suppress an incorrect diagnostic about yylval being uninitialized.  */
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN \
    _Pragma ("GCC diagnostic push") \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")\
    _Pragma ("GCC diagnostic ignored \"-Wmaybe-uninitialized\"")
# define YY_IGNORE_MAYBE_UNINITIALIZED_END \
    _Pragma ("GCC diagnostic pop")
#else
# define YY_INITIAL_VALUE(Value) Value
#endif
#ifndef YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_END
#endif
#ifndef YY_INITIAL_VALUE
# define YY_INITIAL_VALUE(Value) /* Nothing. */
#endif


#if ! defined yyoverflow || YYERROR_VERBOSE

/* The parser invokes alloca or malloc; define the necessary symbols.  */

# ifdef YYSTACK_USE_ALLOCA
#  if YYSTACK_USE_ALLOCA
#   ifdef __GNUC__
#    define YYSTACK_ALLOC __builtin_alloca
#   elif defined __BUILTIN_VA_ARG_INCR
#    include <alloca.h> /* INFRINGES ON USER NAME SPACE */
#   elif defined _AIX
#    define YYSTACK_ALLOC __alloca
#   elif defined _MSC_VER
#    include <malloc.h> /* INFRINGES ON USER NAME SPACE */
#    define alloca _alloca
#   else
#    define YYSTACK_ALLOC alloca
#    if ! defined _ALLOCA_H && ! defined EXIT_SUCCESS
#     include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
      /* Use EXIT_SUCCESS as a witness for stdlib.h.  */
#     ifndef EXIT_SUCCESS
#      define EXIT_SUCCESS 0
#     endif
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's 'empty if-body' warning.  */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (0)
#  ifndef YYSTACK_ALLOC_MAXIMUM
    /* The OS might guarantee only one guard page at the bottom of the stack,
       and a page size can be as small as 4096 bytes.  So we cannot safely
       invoke alloca (N) if N exceeds 4096.  Use a slightly smaller number
       to allow for a few compiler-allocated temporary stack slots.  */
#   define YYSTACK_ALLOC_MAXIMUM 4032 /* reasonable circa 2006 */
#  endif
# else
#  define YYSTACK_ALLOC YYMALLOC
#  define YYSTACK_FREE YYFREE
#  ifndef YYSTACK_ALLOC_MAXIMUM
#   define YYSTACK_ALLOC_MAXIMUM YYSIZE_MAXIMUM
#  endif
#  if (defined __cplusplus && ! defined EXIT_SUCCESS \
       && ! ((defined YYMALLOC || defined malloc) \
             && (defined YYFREE || defined free)))
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   ifndef EXIT_SUCCESS
#    define EXIT_SUCCESS 0
#   endif
#  endif
#  ifndef YYMALLOC
#   define YYMALLOC malloc
#   if ! defined malloc && ! defined EXIT_SUCCESS
void *malloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if ! defined free && ! defined EXIT_SUCCESS
void free (void *); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
# endif
#endif /* ! defined yyoverflow || YYERROR_VERBOSE */


#if (! defined yyoverflow \
     && (! defined __cplusplus \
         || (defined YYSTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  yytype_int16 yyss_alloc;
  YYSTYPE yyvs_alloc;
};

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (sizeof (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (sizeof (yytype_int16) + sizeof (YYSTYPE)) \
      + YYSTACK_GAP_MAXIMUM)

# define YYCOPY_NEEDED 1

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
# define YYSTACK_RELOCATE(Stack_alloc, Stack)                           \
    do                                                                  \
      {                                                                 \
        YYSIZE_T yynewbytes;                                            \
        YYCOPY (&yyptr->Stack_alloc, Stack, yysize);                    \
        Stack = &yyptr->Stack_alloc;                                    \
        yynewbytes = yystacksize * sizeof (*Stack) + YYSTACK_GAP_MAXIMUM; \
        yyptr += yynewbytes / sizeof (*yyptr);                          \
      }                                                                 \
    while (0)

#endif

#if defined YYCOPY_NEEDED && YYCOPY_NEEDED
/* Copy COUNT objects from SRC to DST.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined __GNUC__ && 1 < __GNUC__
#   define YYCOPY(Dst, Src, Count) \
      __builtin_memcpy (Dst, Src, (Count) * sizeof (*(Src)))
#  else
#   define YYCOPY(Dst, Src, Count)              \
      do                                        \
        {                                       \
          YYSIZE_T yyi;                         \
          for (yyi = 0; yyi < (Count); yyi++)   \
            (Dst)[yyi] = (Src)[yyi];            \
        }                                       \
      while (0)
#  endif
# endif
#endif /* !YYCOPY_NEEDED */

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  22
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   298

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  48
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  21
/* YYNRULES -- Number of rules.  */
#define YYNRULES  86
/* YYNSTATES -- Number of states.  */
#define YYNSTATES  185

/* YYTRANSLATE[YYX] -- Symbol number corresponding to YYX as returned
   by yylex, with out-of-bounds checking.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   289

#define YYTRANSLATE(YYX)                                                \
  ((unsigned int) (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

/* YYTRANSLATE[TOKEN-NUM] -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex, without out-of-bounds checking.  */
static const yytype_uint8 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,    45,     2,     2,     2,    37,     2,     2,
      40,    41,    35,    33,    46,    34,    47,    36,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
      43,    42,    44,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     1,     2,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    38,    39
};

#if YYDEBUG
  /* YYRLINE[YYN] -- Source line where rule number YYN was defined.  */
static const yytype_uint16 yyrline[] =
{
       0,   109,   109,   114,   119,   125,   133,   141,   148,   153,
     161,   169,   177,   185,   193,   201,   209,   217,   225,   233,
     246,   255,   269,   278,   293,   302,   316,   323,   337,   343,
     350,   357,   373,   378,   382,   387,   392,   397,   413,   420,
     427,   434,   441,   448,   472,   480,   486,   493,   502,   503,
     506,   511,   512,   514,   522,   523,   526,   535,   544,   553,
     565,   576,   590,   612,   642,   676,   700,   729,   735,   738,
     739,   744,   745,   754,   764,   765,   768,   769,   772,   778,
     784,   792,   796,   802,   812,   823,   834
};
#endif

#if YYDEBUG || YYERROR_VERBOSE || 1
/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "\"end of string\"", "error", "$undefined", "\"number\"", "\"string\"",
  "\"identifier\"", "\"IN\"", "\"LIKE\"", "\"ESCAPE\"", "\"BETWEEN\"",
  "\"NULL\"", "\"IS\"", "\"SELECT\"", "\"LEFT\"", "\"JOIN\"", "\"WHERE\"",
  "\"ON\"", "\"ORDER\"", "\"BY\"", "\"FROM\"", "\"AS\"", "\"ASC\"",
  "\"DESC\"", "\"DISTINCT\"", "\"CAST\"", "\"UNION\"", "\"ALL\"",
  "SWQT_LOGICAL_START", "SWQT_VALUE_START", "SWQT_SELECT_START", "\"NOT\"",
  "\"OR\"", "\"AND\"", "'+'", "'-'", "'*'", "'/'", "'%'", "SWQT_UMINUS",
  "\"reserved keyword\"", "'('", "')'", "'='", "'<'", "'>'", "'!'", "','",
  "'.'", "$accept", "input", "logical_expr", "value_expr_list",
  "field_value", "value_expr", "type_def", "select_statement",
  "select_core", "opt_union_all", "union_all", "select_field_list",
  "column_spec", "as_clause", "opt_where", "opt_joins", "opt_order_by",
  "sort_spec_list", "sort_spec", "string_or_identifier", "table_def", YY_NULL
};
#endif

# ifdef YYPRINT
/* YYTOKNUM[NUM] -- (External) token number corresponding to the
   (internal) symbol number NUM (which must be that of a token).  */
static const yytype_uint16 yytoknum[] =
{
       0,   256,   257,   258,   259,   260,   261,   262,   263,   264,
     265,   266,   267,   268,   269,   270,   271,   272,   273,   274,
     275,   276,   277,   278,   279,   280,   281,   282,   283,   284,
     285,   286,   287,    43,    45,    42,    47,    37,   288,   289,
      40,    41,    61,    60,    62,    33,    44,    46
};
# endif

#define YYPACT_NINF -130

#define yypact_value_is_default(Yystate) \
  (!!((Yystate) == (-130)))

#define YYTABLE_NINF -1

#define yytable_value_is_error(Yytable_value) \
  0

  /* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
     STATE-NUM.  */
static const yytype_int16 yypact[] =
{
      65,   218,   221,     6,     8,  -130,  -130,   -28,  -130,   -24,
     218,   221,   218,    56,  -130,   132,   221,   261,   177,    13,
    -130,    20,  -130,   221,    29,   221,    56,  -130,    35,   112,
     218,   218,    18,   221,   221,    -3,    94,   221,   221,   221,
     221,   221,    25,    86,   168,    31,   240,    15,   154,  -130,
     229,    41,    33,    34,    58,  -130,     6,    54,   204,  -130,
      78,  -130,  -130,    50,  -130,   221,   162,   250,  -130,    89,
      64,   221,   221,   115,   115,  -130,  -130,  -130,   221,   221,
     261,   221,   221,   261,   221,   261,   221,   180,     9,  -130,
      60,    52,  -130,  -130,   156,  -130,  -130,   159,   177,    20,
    -130,  -130,  -130,   221,   104,    76,   221,   221,  -130,   221,
     235,   256,   261,   261,   261,   261,   261,   261,   120,    88,
    -130,  -130,  -130,    84,   128,   175,  -130,  -130,  -130,    97,
      99,  -130,   261,   261,   138,   221,   221,   145,    52,   156,
    -130,   191,   159,   176,   203,  -130,  -130,   261,   261,    52,
    -130,   202,   159,   193,   218,   196,   -15,  -130,  -130,   200,
     120,    56,   201,  -130,  -130,   215,   120,   185,   120,   188,
     190,   120,   172,  -130,   184,  -130,   120,   175,  -130,  -130,
     120,   175,  -130,  -130,  -130
};

  /* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
     Performed when YYTABLE does not specify something else to do.  Zero
     means the default is an error.  */
static const yytype_uint8 yydefact[] =
{
       0,     0,     0,     0,     0,    32,    33,    30,    36,     0,
       0,     0,     0,     2,    34,     0,     0,     3,     0,     0,
       4,    51,     1,     0,     0,     0,     7,    37,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    30,     0,    61,
      58,     0,    54,     0,     0,    48,     0,     0,    29,    31,
       0,     8,    35,     6,     5,     0,    18,     0,    26,     0,
       0,     0,     0,    38,    39,    40,    41,    42,     0,     0,
       9,     0,     0,    12,     0,    13,     0,     0,     0,    57,
      30,    56,    82,    81,     0,    60,    68,     0,     0,    51,
      53,    52,    43,     0,     0,     0,     0,     0,    27,     0,
      19,     0,    15,    16,    14,    10,    17,    11,     0,     0,
      62,    59,    67,    82,    83,    71,    55,    49,    28,    45,
       0,    22,    20,    24,     0,     0,     0,     0,    63,     0,
      84,     0,     0,    69,     0,    44,    23,    21,    25,    65,
      64,    85,     0,     0,     0,    74,     0,    66,    86,     0,
       0,    70,     0,    50,    46,     0,     0,     0,     0,     0,
       0,     0,    78,    75,    77,    47,     0,    71,    79,    80,
       0,    71,    72,    76,    73
};

  /* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
    -130,  -130,   -10,   -56,   -44,    -1,  -130,   179,   217,   147,
    -130,   146,  -130,   -86,  -130,  -127,  -130,    67,  -130,   -91,
    -129
};

  /* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
      -1,     4,    13,    57,    14,    15,   130,    20,    21,    55,
      56,    51,    52,    95,   155,   143,   163,   173,   174,    96,
     125
};

  /* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
     positive, shift that token.  If negative, reduce the rule whose
     number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_uint8 yytable[] =
{
      26,    17,    28,   122,    91,   121,   124,    68,    22,   105,
      27,    29,    23,   153,    59,    46,    25,    50,    18,    24,
      63,    64,    58,   159,    60,    18,   164,    69,     5,     6,
       7,   165,    66,    67,    59,     8,    73,    74,    75,    76,
      77,    80,    83,    85,   120,    54,    19,   128,   151,     9,
     182,   124,   150,   134,   184,    87,    92,    93,    65,    11,
      97,   124,    88,   157,    58,    16,    30,    31,    78,    79,
     110,   111,    94,    86,   137,    99,    61,   112,   113,    98,
     114,   115,    31,   116,   100,   117,    58,    30,    31,     5,
       6,     7,     1,     2,     3,   102,     8,    50,   104,   108,
      70,    71,    58,    72,   109,   132,   133,    24,    58,   129,
       9,    37,    38,    39,    40,    41,   167,   131,    32,    33,
      11,    34,   170,    35,   172,    90,    16,   177,    81,   138,
      82,   139,   181,   140,   147,   148,   172,   144,    32,    33,
     145,    34,    36,    35,   161,    37,    38,    39,    40,    41,
      39,    40,    41,    62,    42,    43,    44,    45,    89,    90,
      92,    93,    36,   123,    93,    37,    38,    39,    40,    41,
     106,     5,     6,     7,    42,    43,    44,    45,     8,   146,
       5,     6,    47,     5,     6,     7,   149,     8,   141,   142,
       8,   154,     9,   178,   179,    37,    38,    39,    40,    41,
      48,     9,    11,   118,     9,   152,   156,   158,    16,   160,
      84,    11,    49,   162,    11,   119,   166,    16,   169,   168,
      16,     5,     6,     7,     5,     6,     7,   171,     8,   175,
     180,     8,   176,    92,    93,   101,    53,    37,    38,    39,
      40,    41,     9,   135,   126,     9,   127,   183,    10,    94,
     103,     0,    11,     0,     0,    11,     0,     0,    12,     0,
       0,    16,    37,    38,    39,    40,    41,     0,    37,    38,
      39,    40,    41,    37,    38,    39,    40,    41,     0,     0,
       0,    62,   107,    37,    38,    39,    40,    41,   136,    37,
      38,    39,    40,    41,    37,    38,    39,    40,    41
};

static const yytype_int16 yycheck[] =
{
      10,     2,    12,    94,    48,    91,    97,    10,     0,    65,
      11,    12,    40,   142,     5,    16,    40,    18,    12,    47,
      30,    31,    23,   152,    25,    12,    41,    30,     3,     4,
       5,    46,    33,    34,     5,    10,    37,    38,    39,    40,
      41,    42,    43,    44,    35,    25,    40,   103,   139,    24,
     177,   142,   138,   109,   181,    40,     4,     5,    40,    34,
      19,   152,    47,   149,    65,    40,    31,    32,    43,    44,
      71,    72,    20,    42,   118,    41,    41,    78,    79,    46,
      81,    82,    32,    84,    26,    86,    87,    31,    32,     3,
       4,     5,    27,    28,    29,    41,    10,    98,    20,    10,
       6,     7,   103,     9,    40,   106,   107,    47,   109,     5,
      24,    33,    34,    35,    36,    37,   160,    41,     6,     7,
      34,     9,   166,    11,   168,     5,    40,   171,    42,    41,
      44,    47,   176,     5,   135,   136,   180,    40,     6,     7,
      41,     9,    30,    11,   154,    33,    34,    35,    36,    37,
      35,    36,    37,    41,    42,    43,    44,    45,     4,     5,
       4,     5,    30,     4,     5,    33,    34,    35,    36,    37,
       8,     3,     4,     5,    42,    43,    44,    45,    10,    41,
       3,     4,     5,     3,     4,     5,    41,    10,    13,    14,
      10,    15,    24,    21,    22,    33,    34,    35,    36,    37,
      23,    24,    34,    23,    24,    14,     3,     5,    40,    16,
      42,    34,    35,    17,    34,    35,    16,    40,     3,    18,
      40,     3,     4,     5,     3,     4,     5,    42,    10,    41,
      46,    10,    42,     4,     5,    56,    19,    33,    34,    35,
      36,    37,    24,     8,    98,    24,    99,   180,    30,    20,
      46,    -1,    34,    -1,    -1,    34,    -1,    -1,    40,    -1,
      -1,    40,    33,    34,    35,    36,    37,    -1,    33,    34,
      35,    36,    37,    33,    34,    35,    36,    37,    -1,    -1,
      -1,    41,    32,    33,    34,    35,    36,    37,    32,    33,
      34,    35,    36,    37,    33,    34,    35,    36,    37
};

  /* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
     symbol of state STATE-NUM.  */
static const yytype_uint8 yystos[] =
{
       0,    27,    28,    29,    49,     3,     4,     5,    10,    24,
      30,    34,    40,    50,    52,    53,    40,    53,    12,    40,
      55,    56,     0,    40,    47,    40,    50,    53,    50,    53,
      31,    32,     6,     7,     9,    11,    30,    33,    34,    35,
      36,    37,    42,    43,    44,    45,    53,     5,    23,    35,
      53,    59,    60,    56,    25,    57,    58,    51,    53,     5,
      53,    41,    41,    50,    50,    40,    53,    53,    10,    30,
       6,     7,     9,    53,    53,    53,    53,    53,    43,    44,
      53,    42,    44,    53,    42,    53,    42,    40,    47,     4,
       5,    52,     4,     5,    20,    61,    67,    19,    46,    41,
      26,    55,    41,    46,    20,    51,     8,    32,    10,    40,
      53,    53,    53,    53,    53,    53,    53,    53,    23,    35,
      35,    61,    67,     4,    67,    68,    59,    57,    51,     5,
      54,    41,    53,    53,    51,     8,    32,    52,    41,    47,
       5,    13,    14,    63,    40,    41,    41,    53,    53,    41,
      61,    67,    14,    68,    15,    62,     3,    61,     5,    68,
      16,    50,    17,    64,    41,    46,    16,    52,    18,     3,
      52,    42,    52,    65,    66,    41,    42,    52,    21,    22,
      46,    52,    63,    65,    63
};

  /* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_uint8 yyr1[] =
{
       0,    48,    49,    49,    49,    50,    50,    50,    50,    50,
      50,    50,    50,    50,    50,    50,    50,    50,    50,    50,
      50,    50,    50,    50,    50,    50,    50,    50,    51,    51,
      52,    52,    53,    53,    53,    53,    53,    53,    53,    53,
      53,    53,    53,    53,    53,    54,    54,    54,    55,    55,
      56,    57,    57,    58,    59,    59,    60,    60,    60,    60,
      60,    60,    60,    60,    60,    60,    60,    61,    61,    62,
      62,    63,    63,    63,    64,    64,    65,    65,    66,    66,
      66,    67,    67,    68,    68,    68,    68
};

  /* YYR2[YYN] -- Number of symbols on the right hand side of rule YYN.  */
static const yytype_uint8 yyr2[] =
{
       0,     2,     2,     2,     2,     3,     3,     2,     3,     3,
       4,     4,     3,     3,     4,     4,     4,     4,     3,     4,
       5,     6,     5,     6,     5,     6,     3,     4,     3,     1,
       1,     3,     1,     1,     1,     3,     1,     2,     3,     3,
       3,     3,     3,     4,     6,     1,     4,     6,     2,     4,
       7,     0,     2,     2,     1,     3,     2,     2,     1,     3,
       2,     1,     3,     4,     5,     5,     6,     2,     1,     0,
       2,     0,     7,     8,     0,     3,     3,     1,     1,     2,
       2,     1,     1,     1,     2,     3,     4
};


#define yyerrok         (yyerrstatus = 0)
#define yyclearin       (yychar = YYEMPTY)
#define YYEMPTY         (-2)
#define YYEOF           0

#define YYACCEPT        goto yyacceptlab
#define YYABORT         goto yyabortlab
#define YYERROR         goto yyerrorlab


#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)                                  \
do                                                              \
  if (yychar == YYEMPTY)                                        \
    {                                                           \
      yychar = (Token);                                         \
      yylval = (Value);                                         \
      YYPOPSTACK (yylen);                                       \
      yystate = *yyssp;                                         \
      goto yybackup;                                            \
    }                                                           \
  else                                                          \
    {                                                           \
      yyerror (context, YY_("syntax error: cannot back up")); \
      YYERROR;                                                  \
    }                                                           \
while (0)

/* Error token number */
#define YYTERROR        1
#define YYERRCODE       256



/* Enable debugging if requested.  */
#if YYDEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)                        \
do {                                            \
  if (yydebug)                                  \
    YYFPRINTF Args;                             \
} while (0)

/* This macro is provided for backward compatibility. */
#ifndef YY_LOCATION_PRINT
# define YY_LOCATION_PRINT(File, Loc) ((void) 0)
#endif


# define YY_SYMBOL_PRINT(Title, Type, Value, Location)                    \
do {                                                                      \
  if (yydebug)                                                            \
    {                                                                     \
      YYFPRINTF (stderr, "%s ", Title);                                   \
      yy_symbol_print (stderr,                                            \
                  Type, Value, context); \
      YYFPRINTF (stderr, "\n");                                           \
    }                                                                     \
} while (0)


/*----------------------------------------.
| Print this symbol's value on YYOUTPUT.  |
`----------------------------------------*/

static void
yy_symbol_value_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep, swq_parse_context *context)
{
  FILE *yyo = yyoutput;
  YYUSE (yyo);
  YYUSE (context);
  if (!yyvaluep)
    return;
# ifdef YYPRINT
  if (yytype < YYNTOKENS)
    YYPRINT (yyoutput, yytoknum[yytype], *yyvaluep);
# endif
  YYUSE (yytype);
}


/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

static void
yy_symbol_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep, swq_parse_context *context)
{
  YYFPRINTF (yyoutput, "%s %s (",
             yytype < YYNTOKENS ? "token" : "nterm", yytname[yytype]);

  yy_symbol_value_print (yyoutput, yytype, yyvaluep, context);
  YYFPRINTF (yyoutput, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

static void
yy_stack_print (yytype_int16 *yybottom, yytype_int16 *yytop)
{
  YYFPRINTF (stderr, "Stack now");
  for (; yybottom <= yytop; yybottom++)
    {
      int yybot = *yybottom;
      YYFPRINTF (stderr, " %d", yybot);
    }
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)                            \
do {                                                            \
  if (yydebug)                                                  \
    yy_stack_print ((Bottom), (Top));                           \
} while (0)


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

static void
yy_reduce_print (yytype_int16 *yyssp, YYSTYPE *yyvsp, int yyrule, swq_parse_context *context)
{
  unsigned long int yylno = yyrline[yyrule];
  int yynrhs = yyr2[yyrule];
  int yyi;
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %lu):\n",
             yyrule - 1, yylno);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      YYFPRINTF (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr,
                       yystos[yyssp[yyi + 1 - yynrhs]],
                       &(yyvsp[(yyi + 1) - (yynrhs)])
                                              , context);
      YYFPRINTF (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)          \
do {                                    \
  if (yydebug)                          \
    yy_reduce_print (yyssp, yyvsp, Rule, context); \
} while (0)

/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !YYDEBUG */
# define YYDPRINTF(Args)
# define YY_SYMBOL_PRINT(Title, Type, Value, Location)
# define YY_STACK_PRINT(Bottom, Top)
# define YY_REDUCE_PRINT(Rule)
#endif /* !YYDEBUG */


/* YYINITDEPTH -- initial size of the parser's stacks.  */
#ifndef YYINITDEPTH
# define YYINITDEPTH 200
#endif

/* YYMAXDEPTH -- maximum size the stacks can grow to (effective only
   if the built-in stack extension method is used).

   Do not make this value too large; the results are undefined if
   YYSTACK_ALLOC_MAXIMUM < YYSTACK_BYTES (YYMAXDEPTH)
   evaluated with infinite-precision integer arithmetic.  */

#ifndef YYMAXDEPTH
# define YYMAXDEPTH 10000
#endif


#if YYERROR_VERBOSE

# ifndef yystrlen
#  if defined __GLIBC__ && defined _STRING_H
#   define yystrlen strlen
#  else
/* Return the length of YYSTR.  */
static YYSIZE_T
yystrlen (const char *yystr)
{
  YYSIZE_T yylen;
  for (yylen = 0; yystr[yylen]; yylen++)
    continue;
  return yylen;
}
#  endif
# endif

# ifndef yystpcpy
#  if defined __GLIBC__ && defined _STRING_H && defined _GNU_SOURCE
#   define yystpcpy stpcpy
#  else
/* Copy YYSRC to YYDEST, returning the address of the terminating '\0' in
   YYDEST.  */
static char *
yystpcpy (char *yydest, const char *yysrc)
{
  char *yyd = yydest;
  const char *yys = yysrc;

  while ((*yyd++ = *yys++) != '\0')
    continue;

  return yyd - 1;
}
#  endif
# endif

# ifndef yytnamerr
/* Copy to YYRES the contents of YYSTR after stripping away unnecessary
   quotes and backslashes, so that it's suitable for yyerror.  The
   heuristic is that double-quoting is unnecessary unless the string
   contains an apostrophe, a comma, or backslash (other than
   backslash-backslash).  YYSTR is taken from yytname.  If YYRES is
   null, do not copy; instead, return the length of what the result
   would have been.  */
static YYSIZE_T
yytnamerr (char *yyres, const char *yystr)
{
  if (*yystr == '"')
    {
      YYSIZE_T yyn = 0;
      char const *yyp = yystr;

      for (;;)
        switch (*++yyp)
          {
          case '\'':
          case ',':
            goto do_not_strip_quotes;

          case '\\':
            if (*++yyp != '\\')
              goto do_not_strip_quotes;
            /* Fall through.  */
          default:
            if (yyres)
              yyres[yyn] = *yyp;
            yyn++;
            break;

          case '"':
            if (yyres)
              yyres[yyn] = '\0';
            return yyn;
          }
    do_not_strip_quotes: ;
    }

  if (! yyres)
    return yystrlen (yystr);

  return yystpcpy (yyres, yystr) - yyres;
}
# endif

/* Copy into *YYMSG, which is of size *YYMSG_ALLOC, an error message
   about the unexpected token YYTOKEN for the state stack whose top is
   YYSSP.

   Return 0 if *YYMSG was successfully written.  Return 1 if *YYMSG is
   not large enough to hold the message.  In that case, also set
   *YYMSG_ALLOC to the required number of bytes.  Return 2 if the
   required number of bytes is too large to store.  */
static int
yysyntax_error (YYSIZE_T *yymsg_alloc, char **yymsg,
                yytype_int16 *yyssp, int yytoken)
{
  YYSIZE_T yysize0 = yytnamerr (YY_NULL, yytname[yytoken]);
  YYSIZE_T yysize = yysize0;
  enum { YYERROR_VERBOSE_ARGS_MAXIMUM = 5 };
  /* Internationalized format string. */
  const char *yyformat = YY_NULL;
  /* Arguments of yyformat. */
  char const *yyarg[YYERROR_VERBOSE_ARGS_MAXIMUM];
  /* Number of reported tokens (one for the "unexpected", one per
     "expected"). */
  int yycount = 0;

  /* There are many possibilities here to consider:
     - If this state is a consistent state with a default action, then
       the only way this function was invoked is if the default action
       is an error action.  In that case, don't check for expected
       tokens because there are none.
     - The only way there can be no lookahead present (in yychar) is if
       this state is a consistent state with a default action.  Thus,
       detecting the absence of a lookahead is sufficient to determine
       that there is no unexpected or expected token to report.  In that
       case, just report a simple "syntax error".
     - Don't assume there isn't a lookahead just because this state is a
       consistent state with a default action.  There might have been a
       previous inconsistent state, consistent state with a non-default
       action, or user semantic action that manipulated yychar.
     - Of course, the expected token list depends on states to have
       correct lookahead information, and it depends on the parser not
       to perform extra reductions after fetching a lookahead from the
       scanner and before detecting a syntax error.  Thus, state merging
       (from LALR or IELR) and default reductions corrupt the expected
       token list.  However, the list is correct for canonical LR with
       one exception: it will still contain any token that will not be
       accepted due to an error action in a later state.
  */
  if (yytoken != YYEMPTY)
    {
      int yyn = yypact[*yyssp];
      yyarg[yycount++] = yytname[yytoken];
      if (!yypact_value_is_default (yyn))
        {
          /* Start YYX at -YYN if negative to avoid negative indexes in
             YYCHECK.  In other words, skip the first -YYN actions for
             this state because they are default actions.  */
          int yyxbegin = yyn < 0 ? -yyn : 0;
          /* Stay within bounds of both yycheck and yytname.  */
          int yychecklim = YYLAST - yyn + 1;
          int yyxend = yychecklim < YYNTOKENS ? yychecklim : YYNTOKENS;
          int yyx;

          for (yyx = yyxbegin; yyx < yyxend; ++yyx)
            if (yycheck[yyx + yyn] == yyx && yyx != YYTERROR
                && !yytable_value_is_error (yytable[yyx + yyn]))
              {
                if (yycount == YYERROR_VERBOSE_ARGS_MAXIMUM)
                  {
                    yycount = 1;
                    yysize = yysize0;
                    break;
                  }
                yyarg[yycount++] = yytname[yyx];
                {
                  YYSIZE_T yysize1 = yysize + yytnamerr (YY_NULL, yytname[yyx]);
                  if (! (yysize <= yysize1
                         && yysize1 <= YYSTACK_ALLOC_MAXIMUM))
                    return 2;
                  yysize = yysize1;
                }
              }
        }
    }

  switch (yycount)
    {
# define YYCASE_(N, S)                      \
      case N:                               \
        yyformat = S;                       \
      break
      YYCASE_(0, YY_("syntax error"));
      YYCASE_(1, YY_("syntax error, unexpected %s"));
      YYCASE_(2, YY_("syntax error, unexpected %s, expecting %s"));
      YYCASE_(3, YY_("syntax error, unexpected %s, expecting %s or %s"));
      YYCASE_(4, YY_("syntax error, unexpected %s, expecting %s or %s or %s"));
      YYCASE_(5, YY_("syntax error, unexpected %s, expecting %s or %s or %s or %s"));
# undef YYCASE_
    }

  {
    YYSIZE_T yysize1 = yysize + yystrlen (yyformat);
    if (! (yysize <= yysize1 && yysize1 <= YYSTACK_ALLOC_MAXIMUM))
      return 2;
    yysize = yysize1;
  }

  if (*yymsg_alloc < yysize)
    {
      *yymsg_alloc = 2 * yysize;
      if (! (yysize <= *yymsg_alloc
             && *yymsg_alloc <= YYSTACK_ALLOC_MAXIMUM))
        *yymsg_alloc = YYSTACK_ALLOC_MAXIMUM;
      return 1;
    }

  /* Avoid sprintf, as that infringes on the user's name space.
     Don't have undefined behavior even if the translation
     produced a string with the wrong number of "%s"s.  */
  {
    char *yyp = *yymsg;
    int yyi = 0;
    while ((*yyp = *yyformat) != '\0')
      if (*yyp == '%' && yyformat[1] == 's' && yyi < yycount)
        {
          yyp += yytnamerr (yyp, yyarg[yyi++]);
          yyformat += 2;
        }
      else
        {
          yyp++;
          yyformat++;
        }
  }
  return 0;
}
#endif /* YYERROR_VERBOSE */

/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

static void
yydestruct (const char *yymsg, int yytype, YYSTYPE *yyvaluep, swq_parse_context *context)
{
  YYUSE (yyvaluep);
  YYUSE (context);
  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yytype, yyvaluep, yylocationp);

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  switch (yytype)
    {
          case 3: /* "number"  */
#line 103 "swq_parser.y" /* yacc.c:1257  */
      { delete ((*yyvaluep)); }
#line 1157 "swq_parser.cpp" /* yacc.c:1257  */
        break;

    case 4: /* "string"  */
#line 103 "swq_parser.y" /* yacc.c:1257  */
      { delete ((*yyvaluep)); }
#line 1163 "swq_parser.cpp" /* yacc.c:1257  */
        break;

    case 5: /* "identifier"  */
#line 103 "swq_parser.y" /* yacc.c:1257  */
      { delete ((*yyvaluep)); }
#line 1169 "swq_parser.cpp" /* yacc.c:1257  */
        break;

    case 50: /* logical_expr  */
#line 104 "swq_parser.y" /* yacc.c:1257  */
      { delete ((*yyvaluep)); }
#line 1175 "swq_parser.cpp" /* yacc.c:1257  */
        break;

    case 51: /* value_expr_list  */
#line 104 "swq_parser.y" /* yacc.c:1257  */
      { delete ((*yyvaluep)); }
#line 1181 "swq_parser.cpp" /* yacc.c:1257  */
        break;

    case 52: /* field_value  */
#line 104 "swq_parser.y" /* yacc.c:1257  */
      { delete ((*yyvaluep)); }
#line 1187 "swq_parser.cpp" /* yacc.c:1257  */
        break;

    case 53: /* value_expr  */
#line 104 "swq_parser.y" /* yacc.c:1257  */
      { delete ((*yyvaluep)); }
#line 1193 "swq_parser.cpp" /* yacc.c:1257  */
        break;

    case 54: /* type_def  */
#line 104 "swq_parser.y" /* yacc.c:1257  */
      { delete ((*yyvaluep)); }
#line 1199 "swq_parser.cpp" /* yacc.c:1257  */
        break;

    case 67: /* string_or_identifier  */
#line 104 "swq_parser.y" /* yacc.c:1257  */
      { delete ((*yyvaluep)); }
#line 1205 "swq_parser.cpp" /* yacc.c:1257  */
        break;

    case 68: /* table_def  */
#line 104 "swq_parser.y" /* yacc.c:1257  */
      { delete ((*yyvaluep)); }
#line 1211 "swq_parser.cpp" /* yacc.c:1257  */
        break;


      default:
        break;
    }
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}




/*----------.
| yyparse.  |
`----------*/

int
yyparse (swq_parse_context *context)
{
/* The lookahead symbol.  */
int yychar;


/* The semantic value of the lookahead symbol.  */
/* Default value used for initialization, for pacifying older GCCs
   or non-GCC compilers.  */
YY_INITIAL_VALUE (static YYSTYPE yyval_default;)
YYSTYPE yylval YY_INITIAL_VALUE (= yyval_default);

    /* Number of syntax errors so far.  */
    int yynerrs;

    int yystate;
    /* Number of tokens to shift before error messages enabled.  */
    int yyerrstatus;

    /* The stacks and their tools:
       'yyss': related to states.
       'yyvs': related to semantic values.

       Refer to the stacks through separate pointers, to allow yyoverflow
       to reallocate them elsewhere.  */

    /* The state stack.  */
    yytype_int16 yyssa[YYINITDEPTH]; /* workaround bug with gcc 4.1 -O2 */ memset(yyssa, 0, sizeof(yyssa));
    yytype_int16 *yyss;
    yytype_int16 *yyssp;

    /* The semantic value stack.  */
    YYSTYPE yyvsa[YYINITDEPTH];
    YYSTYPE *yyvs;
    YYSTYPE *yyvsp;

    YYSIZE_T yystacksize;

  int yyn;
  int yyresult;
  /* Lookahead token as an internal (translated) token number.  */
  int yytoken = 0;
  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;

#if YYERROR_VERBOSE
  /* Buffer for error messages, and its allocated size.  */
  char yymsgbuf[128];
  char *yymsg = yymsgbuf;
  YYSIZE_T yymsg_alloc = sizeof yymsgbuf;
#endif

#define YYPOPSTACK(N)   (yyvsp -= (N), yyssp -= (N))

  /* The number of symbols on the RHS of the reduced rule.
     Keep to zero when no symbol should be popped.  */
  int yylen = 0;

  yyssp = yyss = yyssa;
  yyvsp = yyvs = yyvsa;
  yystacksize = YYINITDEPTH;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yystate = 0;
  yyerrstatus = 0;
  yynerrs = 0;
  yychar = YYEMPTY; /* Cause a token to be read.  */
  goto yysetstate;

/*------------------------------------------------------------.
| yynewstate -- Push a new state, which is found in yystate.  |
`------------------------------------------------------------*/
 yynewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed.  So pushing a state here evens the stacks.  */
  yyssp++;

 yysetstate:
  *yyssp = yystate;

  if (yyss + yystacksize - 1 <= yyssp)
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYSIZE_T yysize = yyssp - yyss + 1;

#ifdef yyoverflow
      {
        /* Give user a chance to reallocate the stack.  Use copies of
           these so that the &'s don't force the real ones into
           memory.  */
        YYSTYPE *yyvs1 = yyvs;
        yytype_int16 *yyss1 = yyss;

        /* Each stack pointer address is followed by the size of the
           data in use in that stack, in bytes.  This used to be a
           conditional around just the two extra args, but that might
           be undefined if yyoverflow is a macro.  */
        yyoverflow (YY_("memory exhausted"),
                    &yyss1, yysize * sizeof (*yyssp),
                    &yyvs1, yysize * sizeof (*yyvsp),
                    &yystacksize);

        yyss = yyss1;
        yyvs = yyvs1;
      }
#else /* no yyoverflow */
# ifndef YYSTACK_RELOCATE
      goto yyexhaustedlab;
# else
      /* Extend the stack our own way.  */
      if (YYMAXDEPTH <= yystacksize)
        goto yyexhaustedlab;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
        yystacksize = YYMAXDEPTH;

      {
        yytype_int16 *yyss1 = yyss;
        union yyalloc *yyptr =
          (union yyalloc *) YYSTACK_ALLOC (YYSTACK_BYTES (yystacksize));
        if (! yyptr)
          goto yyexhaustedlab;
        YYSTACK_RELOCATE (yyss_alloc, yyss);
        YYSTACK_RELOCATE (yyvs_alloc, yyvs);
#  undef YYSTACK_RELOCATE
        if (yyss1 != yyssa)
          YYSTACK_FREE (yyss1);
      }
# endif
#endif /* no yyoverflow */

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;

      YYDPRINTF ((stderr, "Stack size increased to %lu\n",
                  (unsigned long int) yystacksize));

      if (yyss + yystacksize - 1 <= yyssp)
        YYABORT;
    }

  YYDPRINTF ((stderr, "Entering state %d\n", yystate));

  if (yystate == YYFINAL)
    YYACCEPT;

  goto yybackup;

/*-----------.
| yybackup.  |
`-----------*/
yybackup:

  /* Do appropriate processing given the current state.  Read a
     lookahead token if we need one and don't already have one.  */

  /* First try to decide what to do without reference to lookahead token.  */
  yyn = yypact[yystate];
  if (yypact_value_is_default (yyn))
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* YYCHAR is either YYEMPTY or YYEOF or a valid lookahead symbol.  */
  if (yychar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token: "));
      yychar = yylex (&yylval, context);
    }

  if (yychar <= YYEOF)
    {
      yychar = yytoken = YYEOF;
      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else
    {
      yytoken = YYTRANSLATE (yychar);
      YY_SYMBOL_PRINT ("Next token is", yytoken, &yylval, &yylloc);
    }

  /* If the proper action on seeing token YYTOKEN is to reduce or to
     detect an error, take that action.  */
  yyn += yytoken;
  if (yyn < 0 || YYLAST < yyn || yycheck[yyn] != yytoken)
    goto yydefault;
  yyn = yytable[yyn];
  if (yyn <= 0)
    {
      if (yytable_value_is_error (yyn))
        goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }

  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (yyerrstatus)
    yyerrstatus--;

  /* Shift the lookahead token.  */
  YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc);

  /* Discard the shifted token.  */
  yychar = YYEMPTY;

  yystate = yyn;
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END

  goto yynewstate;


/*-----------------------------------------------------------.
| yydefault -- do the default action for the current state.  |
`-----------------------------------------------------------*/
yydefault:
  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;
  goto yyreduce;


/*-----------------------------.
| yyreduce -- Do a reduction.  |
`-----------------------------*/
yyreduce:
  /* yyn is the number of a rule to reduce with.  */
  yylen = yyr2[yyn];

  /* If YYLEN is nonzero, implement the default value of the action:
     '$$ = $1'.

     Otherwise, the following line sets YYVAL to garbage.
     This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  yyval = yyvsp[1-yylen];


  YY_REDUCE_PRINT (yyn);
  switch (yyn)
    {
        case 2:
#line 110 "swq_parser.y" /* yacc.c:1646  */
    {
            context->poRoot = (yyvsp[0]);
        }
#line 1481 "swq_parser.cpp" /* yacc.c:1646  */
    break;

  case 3:
#line 115 "swq_parser.y" /* yacc.c:1646  */
    {
            context->poRoot = (yyvsp[0]);
        }
#line 1489 "swq_parser.cpp" /* yacc.c:1646  */
    break;

  case 4:
#line 120 "swq_parser.y" /* yacc.c:1646  */
    {
            context->poRoot = (yyvsp[0]);
        }
#line 1497 "swq_parser.cpp" /* yacc.c:1646  */
    break;

  case 5:
#line 126 "swq_parser.y" /* yacc.c:1646  */
    {
            (yyval) = new swq_expr_node( SWQ_AND );
            (yyval)->field_type = SWQ_BOOLEAN;
            (yyval)->PushSubExpression( (yyvsp[-2]) );
            (yyval)->PushSubExpression( (yyvsp[0]) );
        }
#line 1508 "swq_parser.cpp" /* yacc.c:1646  */
    break;

  case 6:
#line 134 "swq_parser.y" /* yacc.c:1646  */
    {
            (yyval) = new swq_expr_node( SWQ_OR );
            (yyval)->field_type = SWQ_BOOLEAN;
            (yyval)->PushSubExpression( (yyvsp[-2]) );
            (yyval)->PushSubExpression( (yyvsp[0]) );
        }
#line 1519 "swq_parser.cpp" /* yacc.c:1646  */
    break;

  case 7:
#line 142 "swq_parser.y" /* yacc.c:1646  */
    {
            (yyval) = new swq_expr_node( SWQ_NOT );
            (yyval)->field_type = SWQ_BOOLEAN;
            (yyval)->PushSubExpression( (yyvsp[0]) );
        }
#line 1529 "swq_parser.cpp" /* yacc.c:1646  */
    break;

  case 8:
#line 149 "swq_parser.y" /* yacc.c:1646  */
    {
            (yyval) = (yyvsp[-1]);
        }
#line 1537 "swq_parser.cpp" /* yacc.c:1646  */
    break;

  case 9:
#line 154 "swq_parser.y" /* yacc.c:1646  */
    {
            (yyval) = new swq_expr_node( SWQ_EQ );
            (yyval)->field_type = SWQ_BOOLEAN;
            (yyval)->PushSubExpression( (yyvsp[-2]) );
            (yyval)->PushSubExpression( (yyvsp[0]) );
        }
#line 1548 "swq_parser.cpp" /* yacc.c:1646  */
    break;

  case 10:
#line 162 "swq_parser.y" /* yacc.c:1646  */
    {
            (yyval) = new swq_expr_node( SWQ_NE );
            (yyval)->field_type = SWQ_BOOLEAN;
            (yyval)->PushSubExpression( (yyvsp[-3]) );
            (yyval)->PushSubExpression( (yyvsp[0]) );
        }
#line 1559 "swq_parser.cpp" /* yacc.c:1646  */
    break;

  case 11:
#line 170 "swq_parser.y" /* yacc.c:1646  */
    {
            (yyval) = new swq_expr_node( SWQ_NE );
            (yyval)->field_type = SWQ_BOOLEAN;
            (yyval)->PushSubExpression( (yyvsp[-3]) );
            (yyval)->PushSubExpression( (yyvsp[0]) );
        }
#line 1570 "swq_parser.cpp" /* yacc.c:1646  */
    break;

  case 12:
#line 178 "swq_parser.y" /* yacc.c:1646  */
    {
            (yyval) = new swq_expr_node( SWQ_LT );
            (yyval)->field_type = SWQ_BOOLEAN;
            (yyval)->PushSubExpression( (yyvsp[-2]) );
            (yyval)->PushSubExpression( (yyvsp[0]) );
        }
#line 1581 "swq_parser.cpp" /* yacc.c:1646  */
    break;

  case 13:
#line 186 "swq_parser.y" /* yacc.c:1646  */
    {
            (yyval) = new swq_expr_node( SWQ_GT );
            (yyval)->field_type = SWQ_BOOLEAN;
            (yyval)->PushSubExpression( (yyvsp[-2]) );
            (yyval)->PushSubExpression( (yyvsp[0]) );
        }
#line 1592 "swq_parser.cpp" /* yacc.c:1646  */
    break;

  case 14:
#line 194 "swq_parser.y" /* yacc.c:1646  */
    {
            (yyval) = new swq_expr_node( SWQ_LE );
            (yyval)->field_type = SWQ_BOOLEAN;
            (yyval)->PushSubExpression( (yyvsp[-3]) );
            (yyval)->PushSubExpression( (yyvsp[0]) );
        }
#line 1603 "swq_parser.cpp" /* yacc.c:1646  */
    break;

  case 15:
#line 202 "swq_parser.y" /* yacc.c:1646  */
    {
            (yyval) = new swq_expr_node( SWQ_LE );
            (yyval)->field_type = SWQ_BOOLEAN;
            (yyval)->PushSubExpression( (yyvsp[-3]) );
            (yyval)->PushSubExpression( (yyvsp[0]) );
        }
#line 1614 "swq_parser.cpp" /* yacc.c:1646  */
    break;

  case 16:
#line 210 "swq_parser.y" /* yacc.c:1646  */
    {
            (yyval) = new swq_expr_node( SWQ_LE );
            (yyval)->field_type = SWQ_BOOLEAN;
            (yyval)->PushSubExpression( (yyvsp[-3]) );
            (yyval)->PushSubExpression( (yyvsp[0]) );
        }
#line 1625 "swq_parser.cpp" /* yacc.c:1646  */
    break;

  case 17:
#line 218 "swq_parser.y" /* yacc.c:1646  */
    {
            (yyval) = new swq_expr_node( SWQ_GE );
            (yyval)->field_type = SWQ_BOOLEAN;
            (yyval)->PushSubExpression( (yyvsp[-3]) );
            (yyval)->PushSubExpression( (yyvsp[0]) );
        }
#line 1636 "swq_parser.cpp" /* yacc.c:1646  */
    break;

  case 18:
#line 226 "swq_parser.y" /* yacc.c:1646  */
    {
            (yyval) = new swq_expr_node( SWQ_LIKE );
            (yyval)->field_type = SWQ_BOOLEAN;
            (yyval)->PushSubExpression( (yyvsp[-2]) );
            (yyval)->PushSubExpression( (yyvsp[0]) );
        }
#line 1647 "swq_parser.cpp" /* yacc.c:1646  */
    break;

  case 19:
#line 234 "swq_parser.y" /* yacc.c:1646  */
    {
            swq_expr_node *like;
            like = new swq_expr_node( SWQ_LIKE );
            like->field_type = SWQ_BOOLEAN;
            like->PushSubExpression( (yyvsp[-3]) );
            like->PushSubExpression( (yyvsp[0]) );

            (yyval) = new swq_expr_node( SWQ_NOT );
            (yyval)->field_type = SWQ_BOOLEAN;
            (yyval)->PushSubExpression( like );
        }
#line 1663 "swq_parser.cpp" /* yacc.c:1646  */
    break;

  case 20:
#line 247 "swq_parser.y" /* yacc.c:1646  */
    {
            (yyval) = new swq_expr_node( SWQ_LIKE );
            (yyval)->field_type = SWQ_BOOLEAN;
            (yyval)->PushSubExpression( (yyvsp[-4]) );
            (yyval)->PushSubExpression( (yyvsp[-2]) );
            (yyval)->PushSubExpression( (yyvsp[0]) );
        }
#line 1675 "swq_parser.cpp" /* yacc.c:1646  */
    break;

  case 21:
#line 256 "swq_parser.y" /* yacc.c:1646  */
    {
            swq_expr_node *like;
            like = new swq_expr_node( SWQ_LIKE );
            like->field_type = SWQ_BOOLEAN;
            like->PushSubExpression( (yyvsp[-5]) );
            like->PushSubExpression( (yyvsp[-2]) );
            like->PushSubExpression( (yyvsp[0]) );

            (yyval) = new swq_expr_node( SWQ_NOT );
            (yyval)->field_type = SWQ_BOOLEAN;
            (yyval)->PushSubExpression( like );
        }
#line 1692 "swq_parser.cpp" /* yacc.c:1646  */
    break;

  case 22:
#line 270 "swq_parser.y" /* yacc.c:1646  */
    {
            (yyval) = (yyvsp[-1]);
            (yyval)->field_type = SWQ_BOOLEAN;
            (yyval)->nOperation = SWQ_IN;
            (yyval)->PushSubExpression( (yyvsp[-4]) );
            (yyval)->ReverseSubExpressions();
        }
#line 1704 "swq_parser.cpp" /* yacc.c:1646  */
    break;

  case 23:
#line 279 "swq_parser.y" /* yacc.c:1646  */
    {
            swq_expr_node *in;

            in = (yyvsp[-1]);
            in->field_type = SWQ_BOOLEAN;
            in->nOperation = SWQ_IN;
            in->PushSubExpression( (yyvsp[-5]) );
            in->ReverseSubExpressions();
            
            (yyval) = new swq_expr_node( SWQ_NOT );
            (yyval)->field_type = SWQ_BOOLEAN;
            (yyval)->PushSubExpression( in );
        }
#line 1722 "swq_parser.cpp" /* yacc.c:1646  */
    break;

  case 24:
#line 294 "swq_parser.y" /* yacc.c:1646  */
    {
            (yyval) = new swq_expr_node( SWQ_BETWEEN );
            (yyval)->field_type = SWQ_BOOLEAN;
            (yyval)->PushSubExpression( (yyvsp[-4]) );
            (yyval)->PushSubExpression( (yyvsp[-2]) );
            (yyval)->PushSubExpression( (yyvsp[0]) );
        }
#line 1734 "swq_parser.cpp" /* yacc.c:1646  */
    break;

  case 25:
#line 303 "swq_parser.y" /* yacc.c:1646  */
    {
            swq_expr_node *between;
            between = new swq_expr_node( SWQ_BETWEEN );
            between->field_type = SWQ_BOOLEAN;
            between->PushSubExpression( (yyvsp[-5]) );
            between->PushSubExpression( (yyvsp[-2]) );
            between->PushSubExpression( (yyvsp[0]) );

            (yyval) = new swq_expr_node( SWQ_NOT );
            (yyval)->field_type = SWQ_BOOLEAN;
            (yyval)->PushSubExpression( between );
        }
#line 1751 "swq_parser.cpp" /* yacc.c:1646  */
    break;

  case 26:
#line 317 "swq_parser.y" /* yacc.c:1646  */
    {
            (yyval) = new swq_expr_node( SWQ_ISNULL );
            (yyval)->field_type = SWQ_BOOLEAN;
            (yyval)->PushSubExpression( (yyvsp[-2]) );
        }
#line 1761 "swq_parser.cpp" /* yacc.c:1646  */
    break;

  case 27:
#line 324 "swq_parser.y" /* yacc.c:1646  */
    {
        swq_expr_node *isnull;

            isnull = new swq_expr_node( SWQ_ISNULL );
            isnull->field_type = SWQ_BOOLEAN;
            isnull->PushSubExpression( (yyvsp[-3]) );

            (yyval) = new swq_expr_node( SWQ_NOT );
            (yyval)->field_type = SWQ_BOOLEAN;
            (yyval)->PushSubExpression( isnull );
        }
#line 1777 "swq_parser.cpp" /* yacc.c:1646  */
    break;

  case 28:
#line 338 "swq_parser.y" /* yacc.c:1646  */
    {
            (yyval) = (yyvsp[0]);
            (yyvsp[0])->PushSubExpression( (yyvsp[-2]) );
        }
#line 1786 "swq_parser.cpp" /* yacc.c:1646  */
    break;

  case 29:
#line 344 "swq_parser.y" /* yacc.c:1646  */
    {
            (yyval) = new swq_expr_node( SWQ_UNKNOWN ); /* list */
            (yyval)->PushSubExpression( (yyvsp[0]) );
        }
#line 1795 "swq_parser.cpp" /* yacc.c:1646  */
    break;

  case 30:
#line 351 "swq_parser.y" /* yacc.c:1646  */
    {
            (yyval) = (yyvsp[0]);  // validation deferred.
            (yyval)->eNodeType = SNT_COLUMN;
            (yyval)->field_index = (yyval)->table_index = -1;
        }
#line 1805 "swq_parser.cpp" /* yacc.c:1646  */
    break;

  case 31:
#line 358 "swq_parser.y" /* yacc.c:1646  */
    {
            (yyval) = (yyvsp[-2]);  // validation deferred.
            (yyval)->eNodeType = SNT_COLUMN;
            (yyval)->field_index = (yyval)->table_index = -1;
            (yyval)->string_value = (char *) 
                            CPLRealloc( (yyval)->string_value, 
                                        strlen((yyval)->string_value) 
                                        + strlen((yyvsp[0])->string_value) + 2 );
            strcat( (yyval)->string_value, "." );
            strcat( (yyval)->string_value, (yyvsp[0])->string_value );
            delete (yyvsp[0]);
            (yyvsp[0]) = NULL;
        }
#line 1823 "swq_parser.cpp" /* yacc.c:1646  */
    break;

  case 32:
#line 374 "swq_parser.y" /* yacc.c:1646  */
    {
            (yyval) = (yyvsp[0]);
        }
#line 1831 "swq_parser.cpp" /* yacc.c:1646  */
    break;

  case 33:
#line 379 "swq_parser.y" /* yacc.c:1646  */
    {
            (yyval) = (yyvsp[0]);
        }
#line 1839 "swq_parser.cpp" /* yacc.c:1646  */
    break;

  case 34:
#line 383 "swq_parser.y" /* yacc.c:1646  */
    {
            (yyval) = (yyvsp[0]);
        }
#line 1847 "swq_parser.cpp" /* yacc.c:1646  */
    break;

  case 35:
#line 388 "swq_parser.y" /* yacc.c:1646  */
    {
            (yyval) = (yyvsp[-1]);
        }
#line 1855 "swq_parser.cpp" /* yacc.c:1646  */
    break;

  case 36:
#line 393 "swq_parser.y" /* yacc.c:1646  */
    {
            (yyval) = new swq_expr_node((const char*)NULL);
        }
#line 1863 "swq_parser.cpp" /* yacc.c:1646  */
    break;

  case 37:
#line 398 "swq_parser.y" /* yacc.c:1646  */
    {
            if ((yyvsp[0])->eNodeType == SNT_CONSTANT)
            {
                (yyval) = (yyvsp[0]);
                (yyval)->int_value *= -1;
                (yyval)->float_value *= -1;
            }
            else
            {
                (yyval) = new swq_expr_node( SWQ_MULTIPLY );
                (yyval)->PushSubExpression( new swq_expr_node(-1) );
                (yyval)->PushSubExpression( (yyvsp[0]) );
            }
        }
#line 1882 "swq_parser.cpp" /* yacc.c:1646  */
    break;

  case 38:
#line 414 "swq_parser.y" /* yacc.c:1646  */
    {
            (yyval) = new swq_expr_node( SWQ_ADD );
            (yyval)->PushSubExpression( (yyvsp[-2]) );
            (yyval)->PushSubExpression( (yyvsp[0]) );
        }
#line 1892 "swq_parser.cpp" /* yacc.c:1646  */
    break;

  case 39:
#line 421 "swq_parser.y" /* yacc.c:1646  */
    {
            (yyval) = new swq_expr_node( SWQ_SUBTRACT );
            (yyval)->PushSubExpression( (yyvsp[-2]) );
            (yyval)->PushSubExpression( (yyvsp[0]) );
        }
#line 1902 "swq_parser.cpp" /* yacc.c:1646  */
    break;

  case 40:
#line 428 "swq_parser.y" /* yacc.c:1646  */
    {
            (yyval) = new swq_expr_node( SWQ_MULTIPLY );
            (yyval)->PushSubExpression( (yyvsp[-2]) );
            (yyval)->PushSubExpression( (yyvsp[0]) );
        }
#line 1912 "swq_parser.cpp" /* yacc.c:1646  */
    break;

  case 41:
#line 435 "swq_parser.y" /* yacc.c:1646  */
    {
            (yyval) = new swq_expr_node( SWQ_DIVIDE );
            (yyval)->PushSubExpression( (yyvsp[-2]) );
            (yyval)->PushSubExpression( (yyvsp[0]) );
        }
#line 1922 "swq_parser.cpp" /* yacc.c:1646  */
    break;

  case 42:
#line 442 "swq_parser.y" /* yacc.c:1646  */
    {
            (yyval) = new swq_expr_node( SWQ_MODULUS );
            (yyval)->PushSubExpression( (yyvsp[-2]) );
            (yyval)->PushSubExpression( (yyvsp[0]) );
        }
#line 1932 "swq_parser.cpp" /* yacc.c:1646  */
    break;

  case 43:
#line 449 "swq_parser.y" /* yacc.c:1646  */
    {
            const swq_operation *poOp = 
                    swq_op_registrar::GetOperator( (yyvsp[-3])->string_value );

            if( poOp == NULL )
            {
                CPLError( CE_Failure, CPLE_AppDefined, 
                                "Undefined function '%s' used.",
                                (yyvsp[-3])->string_value );
                delete (yyvsp[-3]);
                delete (yyvsp[-1]);
                YYERROR;
            }
            else
            {
                (yyval) = (yyvsp[-1]);
                            (yyval)->eNodeType = SNT_OPERATION;
                            (yyval)->nOperation = poOp->eOperation;
                (yyval)->ReverseSubExpressions();
                delete (yyvsp[-3]);
            }
        }
#line 1959 "swq_parser.cpp" /* yacc.c:1646  */
    break;

  case 44:
#line 473 "swq_parser.y" /* yacc.c:1646  */
    {
            (yyval) = (yyvsp[-1]);
            (yyval)->PushSubExpression( (yyvsp[-3]) );
            (yyval)->ReverseSubExpressions();
        }
#line 1969 "swq_parser.cpp" /* yacc.c:1646  */
    break;

  case 45:
#line 481 "swq_parser.y" /* yacc.c:1646  */
    {
        (yyval) = new swq_expr_node( SWQ_CAST );
        (yyval)->PushSubExpression( (yyvsp[0]) );
    }
#line 1978 "swq_parser.cpp" /* yacc.c:1646  */
    break;

  case 46:
#line 487 "swq_parser.y" /* yacc.c:1646  */
    {
        (yyval) = new swq_expr_node( SWQ_CAST );
        (yyval)->PushSubExpression( (yyvsp[-1]) );
        (yyval)->PushSubExpression( (yyvsp[-3]) );
    }
#line 1988 "swq_parser.cpp" /* yacc.c:1646  */
    break;

  case 47:
#line 494 "swq_parser.y" /* yacc.c:1646  */
    {
        (yyval) = new swq_expr_node( SWQ_CAST );
        (yyval)->PushSubExpression( (yyvsp[-1]) );
        (yyval)->PushSubExpression( (yyvsp[-3]) );
        (yyval)->PushSubExpression( (yyvsp[-5]) );
    }
#line 1999 "swq_parser.cpp" /* yacc.c:1646  */
    break;

  case 50:
#line 507 "swq_parser.y" /* yacc.c:1646  */
    {
        delete (yyvsp[-3]);
    }
#line 2007 "swq_parser.cpp" /* yacc.c:1646  */
    break;

  case 53:
#line 515 "swq_parser.y" /* yacc.c:1646  */
    {
        swq_select* poNewSelect = new swq_select();
        context->poCurSelect->PushUnionAll(poNewSelect);
        context->poCurSelect = poNewSelect;
    }
#line 2017 "swq_parser.cpp" /* yacc.c:1646  */
    break;

  case 56:
#line 527 "swq_parser.y" /* yacc.c:1646  */
    {
            if( !context->poCurSelect->PushField( (yyvsp[0]), NULL, TRUE ) )
            {
                delete (yyvsp[0]);
                YYERROR;
            }
        }
#line 2029 "swq_parser.cpp" /* yacc.c:1646  */
    break;

  case 57:
#line 536 "swq_parser.y" /* yacc.c:1646  */
    {
            if( !context->poCurSelect->PushField( (yyvsp[0]), NULL, TRUE ) )
            {
                delete (yyvsp[0]);
                YYERROR;
            }
        }
#line 2041 "swq_parser.cpp" /* yacc.c:1646  */
    break;

  case 58:
#line 545 "swq_parser.y" /* yacc.c:1646  */
    {
            if( !context->poCurSelect->PushField( (yyvsp[0]) ) )
            {
                delete (yyvsp[0]);
                YYERROR;
            }
        }
#line 2053 "swq_parser.cpp" /* yacc.c:1646  */
    break;

  case 59:
#line 554 "swq_parser.y" /* yacc.c:1646  */
    {
            if( !context->poCurSelect->PushField( (yyvsp[-1]), (yyvsp[0])->string_value, TRUE ))
            {
                delete (yyvsp[-1]);
                delete (yyvsp[0]);
                YYERROR;
            }

            delete (yyvsp[0]);
        }
#line 2068 "swq_parser.cpp" /* yacc.c:1646  */
    break;

  case 60:
#line 566 "swq_parser.y" /* yacc.c:1646  */
    {
            if( !context->poCurSelect->PushField( (yyvsp[-1]), (yyvsp[0])->string_value ) )
            {
                delete (yyvsp[-1]);
                delete (yyvsp[0]);
                YYERROR;
            }
            delete (yyvsp[0]);
        }
#line 2082 "swq_parser.cpp" /* yacc.c:1646  */
    break;

  case 61:
#line 577 "swq_parser.y" /* yacc.c:1646  */
    {
            swq_expr_node *poNode = new swq_expr_node();
            poNode->eNodeType = SNT_COLUMN;
            poNode->string_value = CPLStrdup( "*" );
            poNode->table_index = poNode->field_index = -1;

            if( !context->poCurSelect->PushField( poNode ) )
            {
                delete poNode;
                YYERROR;
            }
        }
#line 2099 "swq_parser.cpp" /* yacc.c:1646  */
    break;

  case 62:
#line 591 "swq_parser.y" /* yacc.c:1646  */
    {
            CPLString osQualifiedField;

            osQualifiedField = (yyvsp[-2])->string_value;
            osQualifiedField += ".*";

            delete (yyvsp[-2]);
            (yyvsp[-2]) = NULL;

            swq_expr_node *poNode = new swq_expr_node();
            poNode->eNodeType = SNT_COLUMN;
            poNode->string_value = CPLStrdup( osQualifiedField );
            poNode->table_index = poNode->field_index = -1;

            if( !context->poCurSelect->PushField( poNode ) )
            {
                delete poNode;
                YYERROR;
            }
        }
#line 2124 "swq_parser.cpp" /* yacc.c:1646  */
    break;

  case 63:
#line 613 "swq_parser.y" /* yacc.c:1646  */
    {
                // special case for COUNT(*), confirm it.
            if( !EQUAL((yyvsp[-3])->string_value,"COUNT") )
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                        "Syntax Error with %s(*).", 
                    (yyvsp[-3])->string_value );
                delete (yyvsp[-3]);
                    YYERROR;
            }

            delete (yyvsp[-3]);
            (yyvsp[-3]) = NULL;
                    
            swq_expr_node *poNode = new swq_expr_node();
            poNode->eNodeType = SNT_COLUMN;
            poNode->string_value = CPLStrdup( "*" );
            poNode->table_index = poNode->field_index = -1;

            swq_expr_node *count = new swq_expr_node( (swq_op)SWQ_COUNT );
            count->PushSubExpression( poNode );

            if( !context->poCurSelect->PushField( count ) )
            {
                delete count;
                YYERROR;
            }
        }
#line 2157 "swq_parser.cpp" /* yacc.c:1646  */
    break;

  case 64:
#line 643 "swq_parser.y" /* yacc.c:1646  */
    {
                // special case for COUNT(*), confirm it.
            if( !EQUAL((yyvsp[-4])->string_value,"COUNT") )
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                        "Syntax Error with %s(*).", 
                        (yyvsp[-4])->string_value );
                delete (yyvsp[-4]);
                delete (yyvsp[0]);
                YYERROR;
            }

            delete (yyvsp[-4]);
            (yyvsp[-4]) = NULL;

            swq_expr_node *poNode = new swq_expr_node();
            poNode->eNodeType = SNT_COLUMN;
            poNode->string_value = CPLStrdup( "*" );
            poNode->table_index = poNode->field_index = -1;

            swq_expr_node *count = new swq_expr_node( (swq_op)SWQ_COUNT );
            count->PushSubExpression( poNode );

            if( !context->poCurSelect->PushField( count, (yyvsp[0])->string_value ) )
            {
                delete count;
                delete (yyvsp[0]);
                YYERROR;
            }

            delete (yyvsp[0]);
        }
#line 2194 "swq_parser.cpp" /* yacc.c:1646  */
    break;

  case 65:
#line 677 "swq_parser.y" /* yacc.c:1646  */
    {
                // special case for COUNT(DISTINCT x), confirm it.
            if( !EQUAL((yyvsp[-4])->string_value,"COUNT") )
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                        "DISTINCT keyword can only be used in COUNT() operator." );
                delete (yyvsp[-4]);
                delete (yyvsp[-1]);
                    YYERROR;
            }

            delete (yyvsp[-4]);
            
            swq_expr_node *count = new swq_expr_node( SWQ_COUNT );
            count->PushSubExpression( (yyvsp[-1]) );
                
            if( !context->poCurSelect->PushField( count, NULL, TRUE ) )
            {
                delete count;
                YYERROR;
            }
        }
#line 2221 "swq_parser.cpp" /* yacc.c:1646  */
    break;

  case 66:
#line 701 "swq_parser.y" /* yacc.c:1646  */
    {
            // special case for COUNT(DISTINCT x), confirm it.
            if( !EQUAL((yyvsp[-5])->string_value,"COUNT") )
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                        "DISTINCT keyword can only be used in COUNT() operator." );
                delete (yyvsp[-5]);
                delete (yyvsp[-2]);
                delete (yyvsp[0]);
                YYERROR;
            }

            swq_expr_node *count = new swq_expr_node( SWQ_COUNT );
            count->PushSubExpression( (yyvsp[-2]) );

            if( !context->poCurSelect->PushField( count, (yyvsp[0])->string_value, TRUE ) )
            {
                delete (yyvsp[-5]);
                delete count;
                delete (yyvsp[0]);
                YYERROR;
            }

            delete (yyvsp[-5]);
            delete (yyvsp[0]);
        }
#line 2252 "swq_parser.cpp" /* yacc.c:1646  */
    break;

  case 67:
#line 730 "swq_parser.y" /* yacc.c:1646  */
    {
            delete (yyvsp[-1]);
            (yyval) = (yyvsp[0]);
        }
#line 2261 "swq_parser.cpp" /* yacc.c:1646  */
    break;

  case 70:
#line 740 "swq_parser.y" /* yacc.c:1646  */
    {
            context->poCurSelect->where_expr = (yyvsp[0]);
        }
#line 2269 "swq_parser.cpp" /* yacc.c:1646  */
    break;

  case 72:
#line 746 "swq_parser.y" /* yacc.c:1646  */
    {
            context->poCurSelect->PushJoin( (yyvsp[-5])->int_value,
                                            (yyvsp[-3])->string_value, 
                                            (yyvsp[-1])->string_value );
            delete (yyvsp[-5]);
            delete (yyvsp[-3]);
            delete (yyvsp[-1]);
        }
#line 2282 "swq_parser.cpp" /* yacc.c:1646  */
    break;

  case 73:
#line 755 "swq_parser.y" /* yacc.c:1646  */
    {
            context->poCurSelect->PushJoin( (yyvsp[-5])->int_value,
                                            (yyvsp[-3])->string_value, 
                                            (yyvsp[-1])->string_value );
            delete (yyvsp[-5]);
            delete (yyvsp[-3]);
            delete (yyvsp[-1]);
	    }
#line 2295 "swq_parser.cpp" /* yacc.c:1646  */
    break;

  case 78:
#line 773 "swq_parser.y" /* yacc.c:1646  */
    {
            context->poCurSelect->PushOrderBy( (yyvsp[0])->string_value, TRUE );
            delete (yyvsp[0]);
            (yyvsp[0]) = NULL;
        }
#line 2305 "swq_parser.cpp" /* yacc.c:1646  */
    break;

  case 79:
#line 779 "swq_parser.y" /* yacc.c:1646  */
    {
            context->poCurSelect->PushOrderBy( (yyvsp[-1])->string_value, TRUE );
            delete (yyvsp[-1]);
            (yyvsp[-1]) = NULL;
        }
#line 2315 "swq_parser.cpp" /* yacc.c:1646  */
    break;

  case 80:
#line 785 "swq_parser.y" /* yacc.c:1646  */
    {
            context->poCurSelect->PushOrderBy( (yyvsp[-1])->string_value, FALSE );
            delete (yyvsp[-1]);
            (yyvsp[-1]) = NULL;
        }
#line 2325 "swq_parser.cpp" /* yacc.c:1646  */
    break;

  case 81:
#line 793 "swq_parser.y" /* yacc.c:1646  */
    {
            (yyval) = (yyvsp[0]);
        }
#line 2333 "swq_parser.cpp" /* yacc.c:1646  */
    break;

  case 82:
#line 797 "swq_parser.y" /* yacc.c:1646  */
    {
            (yyval) = (yyvsp[0]);
        }
#line 2341 "swq_parser.cpp" /* yacc.c:1646  */
    break;

  case 83:
#line 803 "swq_parser.y" /* yacc.c:1646  */
    {
        int iTable;
        iTable =context->poCurSelect->PushTableDef( NULL, (yyvsp[0])->string_value,
                                                    NULL );
        delete (yyvsp[0]);

        (yyval) = new swq_expr_node( iTable );
    }
#line 2354 "swq_parser.cpp" /* yacc.c:1646  */
    break;

  case 84:
#line 813 "swq_parser.y" /* yacc.c:1646  */
    {
        int iTable;
        iTable = context->poCurSelect->PushTableDef( NULL, (yyvsp[-1])->string_value,
                                                     (yyvsp[0])->string_value );
        delete (yyvsp[-1]);
        delete (yyvsp[0]);

        (yyval) = new swq_expr_node( iTable );
    }
#line 2368 "swq_parser.cpp" /* yacc.c:1646  */
    break;

  case 85:
#line 824 "swq_parser.y" /* yacc.c:1646  */
    {
        int iTable;
        iTable = context->poCurSelect->PushTableDef( (yyvsp[-2])->string_value,
                                                     (yyvsp[0])->string_value, NULL );
        delete (yyvsp[-2]);
        delete (yyvsp[0]);

        (yyval) = new swq_expr_node( iTable );
    }
#line 2382 "swq_parser.cpp" /* yacc.c:1646  */
    break;

  case 86:
#line 835 "swq_parser.y" /* yacc.c:1646  */
    {
        int iTable;
        iTable = context->poCurSelect->PushTableDef( (yyvsp[-3])->string_value,
                                                     (yyvsp[-1])->string_value, 
                                                     (yyvsp[0])->string_value );
        delete (yyvsp[-3]);
        delete (yyvsp[-1]);
        delete (yyvsp[0]);

        (yyval) = new swq_expr_node( iTable );
    }
#line 2398 "swq_parser.cpp" /* yacc.c:1646  */
    break;


#line 2402 "swq_parser.cpp" /* yacc.c:1646  */
      default: break;
    }
  /* User semantic actions sometimes alter yychar, and that requires
     that yytoken be updated with the new translation.  We take the
     approach of translating immediately before every use of yytoken.
     One alternative is translating here after every semantic action,
     but that translation would be missed if the semantic action invokes
     YYABORT, YYACCEPT, or YYERROR immediately after altering yychar or
     if it invokes YYBACKUP.  In the case of YYABORT or YYACCEPT, an
     incorrect destructor might then be invoked immediately.  In the
     case of YYERROR or YYBACKUP, subsequent parser actions might lead
     to an incorrect destructor call or verbose syntax error message
     before the lookahead is translated.  */
  YY_SYMBOL_PRINT ("-> $$ =", yyr1[yyn], &yyval, &yyloc);

  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);

  *++yyvsp = yyval;

  /* Now 'shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */

  yyn = yyr1[yyn];

  yystate = yypgoto[yyn - YYNTOKENS] + *yyssp;
  if (0 <= yystate && yystate <= YYLAST && yycheck[yystate] == *yyssp)
    yystate = yytable[yystate];
  else
    yystate = yydefgoto[yyn - YYNTOKENS];

  goto yynewstate;


/*--------------------------------------.
| yyerrlab -- here on detecting error.  |
`--------------------------------------*/
yyerrlab:
  /* Make sure we have latest lookahead translation.  See comments at
     user semantic actions for why this is necessary.  */
  yytoken = yychar == YYEMPTY ? YYEMPTY : YYTRANSLATE (yychar);

  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
#if ! YYERROR_VERBOSE
      yyerror (context, YY_("syntax error"));
#else
# define YYSYNTAX_ERROR yysyntax_error (&yymsg_alloc, &yymsg, \
                                        yyssp, yytoken)
      {
        char const *yymsgp = YY_("syntax error");
        int yysyntax_error_status;
        yysyntax_error_status = YYSYNTAX_ERROR;
        if (yysyntax_error_status == 0)
          yymsgp = yymsg;
        else if (yysyntax_error_status == 1)
          {
            if (yymsg != yymsgbuf)
              YYSTACK_FREE (yymsg);
            yymsg = (char *) YYSTACK_ALLOC (yymsg_alloc);
            if (!yymsg)
              {
                yymsg = yymsgbuf;
                yymsg_alloc = sizeof yymsgbuf;
                yysyntax_error_status = 2;
              }
            else
              {
                yysyntax_error_status = YYSYNTAX_ERROR;
                yymsgp = yymsg;
              }
          }
        yyerror (context, yymsgp);
        if (yysyntax_error_status == 2)
          goto yyexhaustedlab;
      }
# undef YYSYNTAX_ERROR
#endif
    }



  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse lookahead token after an
         error, discard it.  */

      if (yychar <= YYEOF)
        {
          /* Return failure if at end of input.  */
          if (yychar == YYEOF)
            YYABORT;
        }
      else
        {
          yydestruct ("Error: discarding",
                      yytoken, &yylval, context);
          yychar = YYEMPTY;
        }
    }

  /* Else will try to reuse lookahead token after shifting the error
     token.  */
  goto yyerrlab1;


/*---------------------------------------------------.
| yyerrorlab -- error raised explicitly by YYERROR.  |
`---------------------------------------------------*/
yyerrorlab:

  /* Pacify compilers like GCC when the user code never invokes
     YYERROR and the label yyerrorlab therefore never appears in user
     code.  */
  if (/*CONSTCOND*/ 0)
     goto yyerrorlab;

  /* Do not reclaim the symbols of the rule whose action triggered
     this YYERROR.  */
  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);
  yystate = *yyssp;
  goto yyerrlab1;


/*-------------------------------------------------------------.
| yyerrlab1 -- common code for both syntax error and YYERROR.  |
`-------------------------------------------------------------*/
yyerrlab1:
  yyerrstatus = 3;      /* Each real token shifted decrements this.  */

  for (;;)
    {
      yyn = yypact[yystate];
      if (!yypact_value_is_default (yyn))
        {
          yyn += YYTERROR;
          if (0 <= yyn && yyn <= YYLAST && yycheck[yyn] == YYTERROR)
            {
              yyn = yytable[yyn];
              if (0 < yyn)
                break;
            }
        }

      /* Pop the current state because it cannot handle the error token.  */
      if (yyssp == yyss)
        YYABORT;


      yydestruct ("Error: popping",
                  yystos[yystate], yyvsp, context);
      YYPOPSTACK (1);
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END


  /* Shift the error token.  */
  YY_SYMBOL_PRINT ("Shifting", yystos[yyn], yyvsp, yylsp);

  yystate = yyn;
  goto yynewstate;


/*-------------------------------------.
| yyacceptlab -- YYACCEPT comes here.  |
`-------------------------------------*/
yyacceptlab:
  yyresult = 0;
  goto yyreturn;

/*-----------------------------------.
| yyabortlab -- YYABORT comes here.  |
`-----------------------------------*/
yyabortlab:
  yyresult = 1;
  goto yyreturn;

#if !defined yyoverflow || YYERROR_VERBOSE
/*-------------------------------------------------.
| yyexhaustedlab -- memory exhaustion comes here.  |
`-------------------------------------------------*/
yyexhaustedlab:
  yyerror (context, YY_("memory exhausted"));
  yyresult = 2;
  /* Fall through.  */
#endif

yyreturn:
  if (yychar != YYEMPTY)
    {
      /* Make sure we have latest lookahead translation.  See comments at
         user semantic actions for why this is necessary.  */
      yytoken = YYTRANSLATE (yychar);
      yydestruct ("Cleanup: discarding lookahead",
                  yytoken, &yylval, context);
    }
  /* Do not reclaim the symbols of the rule whose action triggered
     this YYABORT or YYACCEPT.  */
  YYPOPSTACK (yylen);
  YY_STACK_PRINT (yyss, yyssp);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
                  yystos[*yyssp], yyvsp, context);
      YYPOPSTACK (1);
    }
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif
#if YYERROR_VERBOSE
  if (yymsg != yymsgbuf)
    YYSTACK_FREE (yymsg);
#endif
  return yyresult;
}
