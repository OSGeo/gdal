
/* A Bison parser, made by GNU Bison 2.4.1.  */

/* Skeleton implementation for Bison's Yacc-like parsers in C
   
      Copyright (C) 1984, 1989, 1990, 2000, 2001, 2002, 2003, 2004, 2005, 2006
   Free Software Foundation, Inc.
   
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
#define YYBISON_VERSION "2.4.1"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 1

/* Push parsers.  */
#define YYPUSH 0

/* Pull parsers.  */
#define YYPULL 1

/* Using locations.  */
#define YYLSP_NEEDED 0

/* Substitute the variable and function names.  */
#define yyparse         swqparse
#define yylex           swqlex
#define yyerror         swqerror
#define yylval          swqlval
#define yychar          swqchar
#define yydebug         swqdebug
#define yynerrs         swqnerrs


/* Copy the first part of user declarations.  */

/* Line 189 of yacc.c  */
#line 1 "swq_parser.y"

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

static void swqerror( swq_parse_context *context, const char *msg )
{
    CPLError( CE_Failure, CPLE_AppDefined, 
              "SQL Expression Parsing Error: %s", msg );
}



/* Line 189 of yacc.c  */
#line 134 "swq_parser.cpp"

/* Enabling traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif

/* Enabling verbose error messages.  */
#ifdef YYERROR_VERBOSE
# undef YYERROR_VERBOSE
# define YYERROR_VERBOSE 1
#else
# define YYERROR_VERBOSE 0
#endif

/* Enabling the token table.  */
#ifndef YYTOKEN_TABLE
# define YYTOKEN_TABLE 0
#endif


/* Tokens.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
   /* Put the tokens into the symbol table, so that GDB and other debuggers
      know about them.  */
   enum yytokentype {
     SWQT_NUMBER = 258,
     SWQT_STRING = 259,
     SWQT_IDENTIFIER = 260,
     SWQT_IN = 261,
     SWQT_LIKE = 262,
     SWQT_BETWEEN = 263,
     SWQT_NULL = 264,
     SWQT_IS = 265,
     SWQT_SELECT = 266,
     SWQT_LEFT = 267,
     SWQT_JOIN = 268,
     SWQT_WHERE = 269,
     SWQT_ON = 270,
     SWQT_ORDER = 271,
     SWQT_BY = 272,
     SWQT_FROM = 273,
     SWQT_AS = 274,
     SWQT_ASC = 275,
     SWQT_DESC = 276,
     SWQT_DISTINCT = 277,
     SWQT_CAST = 278,
     SWQT_LOGICAL_START = 279,
     SWQT_VALUE_START = 280,
     SWQT_SELECT_START = 281,
     SWQT_NOT = 282,
     SWQT_OR = 283,
     SWQT_AND = 284,
     SWQT_UMINUS = 285
   };
#endif



#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef int YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
#endif


/* Copy the second part of user declarations.  */


/* Line 264 of yacc.c  */
#line 206 "swq_parser.cpp"

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
#elif (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
typedef signed char yytype_int8;
#else
typedef short int yytype_int8;
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
# elif ! defined YYSIZE_T && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# else
#  define YYSIZE_T unsigned int
# endif
#endif

#define YYSIZE_MAXIMUM ((YYSIZE_T) -1)

#ifndef YY_
# if YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> /* INFRINGES ON USER NAME SPACE */
#   define YY_(msgid) dgettext ("bison-runtime", msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(msgid) msgid
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YYUSE(e) ((void) (e))
#else
# define YYUSE(e) /* empty */
#endif

/* Identity function, used to suppress warnings about constant conditions.  */
#ifndef lint
# define YYID(n) (n)
#else
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static int
YYID (int yyi)
#else
static int
YYID (yyi)
    int yyi;
#endif
{
  return yyi;
}
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
#    if ! defined _ALLOCA_H && ! defined _STDLIB_H && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
#     include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#     ifndef _STDLIB_H
#      define _STDLIB_H 1
#     endif
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's `empty if-body' warning.  */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (YYID (0))
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
#  if (defined __cplusplus && ! defined _STDLIB_H \
       && ! ((defined YYMALLOC || defined malloc) \
	     && (defined YYFREE || defined free)))
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   ifndef _STDLIB_H
#    define _STDLIB_H 1
#   endif
#  endif
#  ifndef YYMALLOC
#   define YYMALLOC malloc
#   if ! defined malloc && ! defined _STDLIB_H && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
void *malloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if ! defined free && ! defined _STDLIB_H && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
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

/* Copy COUNT objects from FROM to TO.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined __GNUC__ && 1 < __GNUC__
#   define YYCOPY(To, From, Count) \
      __builtin_memcpy (To, From, (Count) * sizeof (*(From)))
#  else
#   define YYCOPY(To, From, Count)		\
      do					\
	{					\
	  YYSIZE_T yyi;				\
	  for (yyi = 0; yyi < (Count); yyi++)	\
	    (To)[yyi] = (From)[yyi];		\
	}					\
      while (YYID (0))
#  endif
# endif

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
# define YYSTACK_RELOCATE(Stack_alloc, Stack)				\
    do									\
      {									\
	YYSIZE_T yynewbytes;						\
	YYCOPY (&yyptr->Stack_alloc, Stack, yysize);			\
	Stack = &yyptr->Stack_alloc;					\
	yynewbytes = yystacksize * sizeof (*Stack) + YYSTACK_GAP_MAXIMUM; \
	yyptr += yynewbytes / sizeof (*yyptr);				\
      }									\
    while (YYID (0))

#endif

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  19
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   256

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  44
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  17
/* YYNRULES -- Number of rules.  */
#define YYNRULES  76
/* YYNRULES -- Number of states.  */
#define YYNSTATES  171

/* YYTRANSLATE(YYLEX) -- Bison symbol number corresponding to YYLEX.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   285

#define YYTRANSLATE(YYX)						\
  ((unsigned int) (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

/* YYTRANSLATE[YYLEX] -- Bison symbol number corresponding to YYLEX.  */
static const yytype_uint8 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,    41,     2,     2,     2,    34,     2,     2,
      36,    37,    32,    30,    42,    31,    43,    33,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
      39,    38,    40,     2,     2,     2,     2,     2,     2,     2,
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
      25,    26,    27,    28,    29,    35
};

#if YYDEBUG
/* YYPRHS[YYN] -- Index of the first RHS symbol of rule number YYN in
   YYRHS.  */
static const yytype_uint16 yyprhs[] =
{
       0,     0,     3,     6,     9,    12,    16,    20,    23,    27,
      31,    36,    41,    45,    49,    54,    59,    64,    69,    73,
      78,    84,    91,    97,   104,   108,   113,   117,   119,   121,
     125,   127,   129,   131,   135,   138,   142,   146,   150,   154,
     158,   163,   170,   172,   177,   184,   192,   194,   198,   201,
     204,   206,   211,   215,   217,   221,   226,   233,   239,   247,
     248,   251,   252,   260,   269,   270,   274,   278,   280,   282,
     285,   288,   290,   292,   294,   297,   301
};

/* YYRHS -- A `-1'-separated list of the rules' RHS.  */
static const yytype_int8 yyrhs[] =
{
      45,     0,    -1,    24,    46,    -1,    25,    49,    -1,    26,
      51,    -1,    46,    29,    46,    -1,    46,    28,    46,    -1,
      27,    46,    -1,    36,    46,    37,    -1,    49,    38,    49,
      -1,    49,    39,    40,    49,    -1,    49,    41,    38,    49,
      -1,    49,    39,    49,    -1,    49,    40,    49,    -1,    49,
      39,    38,    49,    -1,    49,    38,    39,    49,    -1,    49,
      38,    40,    49,    -1,    49,    40,    38,    49,    -1,    49,
       7,    49,    -1,    49,    27,     7,    49,    -1,    49,     6,
      36,    47,    37,    -1,    49,    27,     6,    36,    47,    37,
      -1,    49,     8,    49,    29,    49,    -1,    49,    27,     8,
      49,    29,    49,    -1,    49,    10,     9,    -1,    49,    10,
      27,     9,    -1,    49,    42,    47,    -1,    49,    -1,     5,
      -1,     5,    43,     5,    -1,     3,    -1,     4,    -1,    48,
      -1,    36,    49,    37,    -1,    31,    49,    -1,    49,    30,
      49,    -1,    49,    31,    49,    -1,    49,    32,    49,    -1,
      49,    33,    49,    -1,    49,    34,    49,    -1,     5,    36,
      47,    37,    -1,    23,    36,    49,    19,    50,    37,    -1,
       5,    -1,     5,    36,     3,    37,    -1,     5,    36,     3,
      42,     3,    37,    -1,    11,    52,    18,    60,    55,    54,
      56,    -1,    53,    -1,    53,    42,    52,    -1,    22,    48,
      -1,    22,     4,    -1,    49,    -1,    22,    48,    19,    59,
      -1,    49,    19,    59,    -1,    32,    -1,     5,    43,    32,
      -1,     5,    36,    32,    37,    -1,     5,    36,    32,    37,
      19,    59,    -1,     5,    36,    22,    48,    37,    -1,     5,
      36,    22,    48,    37,    19,    59,    -1,    -1,    14,    46,
      -1,    -1,    13,    60,    15,    48,    38,    48,    55,    -1,
      12,    13,    60,    15,    48,    38,    48,    55,    -1,    -1,
      16,    17,    57,    -1,    58,    42,    57,    -1,    58,    -1,
      48,    -1,    48,    20,    -1,    48,    21,    -1,     5,    -1,
       4,    -1,    59,    -1,    59,     5,    -1,     4,    43,    59,
      -1,     4,    43,    59,     5,    -1
};

/* YYRLINE[YYN] -- source line where rule number YYN was defined.  */
static const yytype_uint16 yyrline[] =
{
       0,   101,   101,   106,   111,   117,   125,   133,   140,   145,
     153,   161,   169,   177,   185,   193,   201,   209,   217,   225,
     238,   247,   262,   271,   285,   292,   306,   312,   319,   326,
     342,   347,   351,   356,   361,   377,   384,   391,   398,   405,
     412,   436,   444,   450,   457,   466,   472,   473,   476,   485,
     494,   503,   515,   526,   540,   562,   592,   626,   650,   678,
     679,   684,   685,   694,   704,   705,   708,   709,   712,   718,
     724,   732,   736,   742,   752,   763,   774
};
#endif

#if YYDEBUG || YYERROR_VERBOSE || YYTOKEN_TABLE
/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "$end", "error", "$undefined", "SWQT_NUMBER", "SWQT_STRING",
  "SWQT_IDENTIFIER", "SWQT_IN", "SWQT_LIKE", "SWQT_BETWEEN", "SWQT_NULL",
  "SWQT_IS", "SWQT_SELECT", "SWQT_LEFT", "SWQT_JOIN", "SWQT_WHERE",
  "SWQT_ON", "SWQT_ORDER", "SWQT_BY", "SWQT_FROM", "SWQT_AS", "SWQT_ASC",
  "SWQT_DESC", "SWQT_DISTINCT", "SWQT_CAST", "SWQT_LOGICAL_START",
  "SWQT_VALUE_START", "SWQT_SELECT_START", "SWQT_NOT", "SWQT_OR",
  "SWQT_AND", "'+'", "'-'", "'*'", "'/'", "'%'", "SWQT_UMINUS", "'('",
  "')'", "'='", "'<'", "'>'", "'!'", "','", "'.'", "$accept", "input",
  "logical_expr", "value_expr_list", "field_value", "value_expr",
  "type_def", "select_statement", "select_field_list", "column_spec",
  "opt_where", "opt_joins", "opt_order_by", "sort_spec_list", "sort_spec",
  "string_or_identifier", "table_def", 0
};
#endif

# ifdef YYPRINT
/* YYTOKNUM[YYLEX-NUM] -- Internal token number corresponding to
   token YYLEX-NUM.  */
static const yytype_uint16 yytoknum[] =
{
       0,   256,   257,   258,   259,   260,   261,   262,   263,   264,
     265,   266,   267,   268,   269,   270,   271,   272,   273,   274,
     275,   276,   277,   278,   279,   280,   281,   282,   283,   284,
      43,    45,    42,    47,    37,   285,    40,    41,    61,    60,
      62,    33,    44,    46
};
# endif

/* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_uint8 yyr1[] =
{
       0,    44,    45,    45,    45,    46,    46,    46,    46,    46,
      46,    46,    46,    46,    46,    46,    46,    46,    46,    46,
      46,    46,    46,    46,    46,    46,    47,    47,    48,    48,
      49,    49,    49,    49,    49,    49,    49,    49,    49,    49,
      49,    49,    50,    50,    50,    51,    52,    52,    53,    53,
      53,    53,    53,    53,    53,    53,    53,    53,    53,    54,
      54,    55,    55,    55,    56,    56,    57,    57,    58,    58,
      58,    59,    59,    60,    60,    60,    60
};

/* YYR2[YYN] -- Number of symbols composing right hand side of rule YYN.  */
static const yytype_uint8 yyr2[] =
{
       0,     2,     2,     2,     2,     3,     3,     2,     3,     3,
       4,     4,     3,     3,     4,     4,     4,     4,     3,     4,
       5,     6,     5,     6,     3,     4,     3,     1,     1,     3,
       1,     1,     1,     3,     2,     3,     3,     3,     3,     3,
       4,     6,     1,     4,     6,     7,     1,     3,     2,     2,
       1,     4,     3,     1,     3,     4,     6,     5,     7,     0,
       2,     0,     7,     8,     0,     3,     3,     1,     1,     2,
       2,     1,     1,     1,     2,     3,     4
};

/* YYDEFACT[STATE-NAME] -- Default rule to reduce with in state
   STATE-NUM when YYTABLE doesn't specify something else to do.  Zero
   means the default is an error.  */
static const yytype_uint8 yydefact[] =
{
       0,     0,     0,     0,     0,    30,    31,    28,     0,     0,
       0,     0,     2,    32,     0,     0,     3,     0,     4,     1,
       0,     0,     0,     7,    34,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    28,     0,    53,    50,     0,    46,
       0,    27,    29,     0,     8,    33,     6,     5,     0,    18,
       0,    24,     0,     0,     0,     0,    35,    36,    37,    38,
      39,     0,     0,     9,     0,     0,    12,     0,    13,     0,
       0,     0,    49,    28,    48,     0,     0,     0,    40,     0,
       0,     0,     0,    25,     0,    19,     0,    15,    16,    14,
      10,    17,    11,     0,     0,    54,     0,    72,    71,    52,
      72,    73,    61,    47,    26,    42,     0,    20,    22,     0,
       0,     0,    55,    51,     0,    74,     0,     0,    59,     0,
      41,    21,    23,    57,     0,    75,     0,     0,     0,    64,
       0,     0,    56,    76,     0,     0,    60,     0,    45,    43,
       0,    58,     0,     0,     0,     0,     0,     0,    68,    65,
      67,    44,     0,    61,    69,    70,     0,    61,    62,    66,
      63
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
      -1,     4,    12,    50,    13,    14,   116,    18,    48,    49,
     139,   128,   148,   159,   160,   111,   112
};

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
#define YYPACT_NINF -111
static const yytype_int16 yypact[] =
{
      90,   177,   188,    37,    24,  -111,  -111,    13,    14,   177,
     188,   177,     9,  -111,   135,   188,   -21,   167,  -111,  -111,
     188,    57,   188,     9,  -111,   -23,   120,   177,   177,    35,
     188,   188,    -6,   113,   188,   188,   188,   188,   188,    50,
     100,    29,    40,   206,    15,    70,  -111,   195,    65,    49,
      56,   153,  -111,   201,  -111,  -111,    69,  -111,   188,   -21,
     215,  -111,    93,    64,   188,   188,   101,   101,  -111,  -111,
    -111,   188,   188,   -21,   188,   188,   -21,   188,   -21,   188,
     174,    -1,  -111,    66,    92,    83,    91,   167,  -111,   188,
     107,    80,   188,  -111,   188,   -21,   221,   -21,   -21,   -21,
     -21,   -21,   -21,   119,    95,  -111,    83,  -111,  -111,  -111,
      96,   132,    94,  -111,  -111,   108,   109,  -111,   -21,   118,
     188,   126,   137,  -111,    83,  -111,   151,    91,   180,   185,
    -111,  -111,   -21,   182,    83,   197,    91,   192,   177,   193,
     -14,    83,  -111,  -111,   200,   119,     9,   199,  -111,  -111,
     209,  -111,   119,   179,   119,   181,   183,   119,   128,  -111,
     214,  -111,   119,    94,  -111,  -111,   119,    94,  -111,  -111,
    -111
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
    -111,  -111,    -9,   -28,   -44,     5,  -111,  -111,   136,  -111,
    -111,   -95,  -111,    75,  -111,   -77,  -110
};

/* YYTABLE[YYPACT[STATE-NUM]].  What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule which
   number is the opposite.  If zero, do what YYDEFACT says.
   If YYTABLE_NINF, syntax error.  */
#define YYTABLE_NINF -1
static const yytype_uint8 yytable[] =
{
      23,    84,    25,    61,    52,    27,    28,    16,   109,    34,
      35,    36,    37,    38,    54,    24,    26,   137,    56,    57,
      43,    62,    47,   149,    19,    51,   144,    53,   150,   123,
      91,   105,     5,     6,     7,    59,    60,    27,    28,    66,
      67,    68,    69,    70,    73,    76,    78,   135,    17,    20,
      22,    80,     8,     5,     6,     7,    21,   142,    81,   121,
      10,   114,    52,    51,   151,    15,   119,    77,   168,    95,
      96,    58,   170,     8,    82,    83,    97,    98,    79,    99,
     100,    10,   101,    86,   102,    51,    15,   107,   108,    71,
      72,    87,    47,    88,    51,   110,   108,   118,    28,    51,
      94,   153,    93,     5,     6,     7,   126,   127,   156,    21,
     158,   106,   115,   163,     1,     2,     3,   117,   167,    63,
      64,    65,   158,     8,    83,   132,    29,    30,    31,   146,
      32,    10,   122,    36,    37,    38,    15,   125,    74,   124,
      75,    29,    30,    31,   129,    32,   130,    33,   164,   165,
      34,    35,    36,    37,    38,   131,   134,    55,    39,    40,
      41,    42,    33,   133,   136,    34,    35,    36,    37,    38,
       5,     6,    44,    39,    40,    41,    42,     5,     6,     7,
       5,     6,     7,    34,    35,    36,    37,    38,   140,    45,
       8,     5,     6,     7,   138,    89,   103,     8,    10,    46,
       8,   141,   143,    15,     9,    10,   104,   145,    10,   147,
      15,     8,   155,    11,    85,   152,   154,   157,   161,    10,
      90,   162,     0,   113,    15,    34,    35,    36,    37,    38,
       0,    34,    35,    36,    37,    38,    34,    35,    36,    37,
      38,   169,     0,    55,    92,    34,    35,    36,    37,    38,
     120,    34,    35,    36,    37,    38,   166
};

static const yytype_int16 yycheck[] =
{
       9,    45,    11,     9,     5,    28,    29,     2,    85,    30,
      31,    32,    33,    34,    37,    10,    11,   127,    27,    28,
      15,    27,    17,    37,     0,    20,   136,    22,    42,   106,
      58,    32,     3,     4,     5,    30,    31,    28,    29,    34,
      35,    36,    37,    38,    39,    40,    41,   124,    11,    36,
      36,    36,    23,     3,     4,     5,    43,   134,    43,   103,
      31,    89,     5,    58,   141,    36,    94,    38,   163,    64,
      65,    36,   167,    23,     4,     5,    71,    72,    38,    74,
      75,    31,    77,    18,    79,    80,    36,     4,     5,    39,
      40,    42,    87,    37,    89,     4,     5,    92,    29,    94,
      36,   145,     9,     3,     4,     5,    12,    13,   152,    43,
     154,    19,     5,   157,    24,    25,    26,    37,   162,     6,
       7,     8,   166,    23,     5,   120,     6,     7,     8,   138,
      10,    31,    37,    32,    33,    34,    36,     5,    38,    43,
      40,     6,     7,     8,    36,    10,    37,    27,    20,    21,
      30,    31,    32,    33,    34,    37,    19,    37,    38,    39,
      40,    41,    27,    37,    13,    30,    31,    32,    33,    34,
       3,     4,     5,    38,    39,    40,    41,     3,     4,     5,
       3,     4,     5,    30,    31,    32,    33,    34,     3,    22,
      23,     3,     4,     5,    14,    42,    22,    23,    31,    32,
      23,    19,     5,    36,    27,    31,    32,    15,    31,    16,
      36,    23,     3,    36,    19,    15,    17,    38,    37,    31,
      19,    38,    -1,    87,    36,    30,    31,    32,    33,    34,
      -1,    30,    31,    32,    33,    34,    30,    31,    32,    33,
      34,   166,    -1,    37,    29,    30,    31,    32,    33,    34,
      29,    30,    31,    32,    33,    34,    42
};

/* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
   symbol of state STATE-NUM.  */
static const yytype_uint8 yystos[] =
{
       0,    24,    25,    26,    45,     3,     4,     5,    23,    27,
      31,    36,    46,    48,    49,    36,    49,    11,    51,     0,
      36,    43,    36,    46,    49,    46,    49,    28,    29,     6,
       7,     8,    10,    27,    30,    31,    32,    33,    34,    38,
      39,    40,    41,    49,     5,    22,    32,    49,    52,    53,
      47,    49,     5,    49,    37,    37,    46,    46,    36,    49,
      49,     9,    27,     6,     7,     8,    49,    49,    49,    49,
      49,    39,    40,    49,    38,    40,    49,    38,    49,    38,
      36,    43,     4,     5,    48,    19,    18,    42,    37,    42,
      19,    47,    29,     9,    36,    49,    49,    49,    49,    49,
      49,    49,    49,    22,    32,    32,    19,     4,     5,    59,
       4,    59,    60,    52,    47,     5,    50,    37,    49,    47,
      29,    48,    37,    59,    43,     5,    12,    13,    55,    36,
      37,    37,    49,    37,    19,    59,    13,    60,    14,    54,
       3,    19,    59,     5,    60,    15,    46,    16,    56,    37,
      42,    59,    15,    48,    17,     3,    48,    38,    48,    57,
      58,    37,    38,    48,    20,    21,    42,    48,    55,    57,
      55
};

#define yyerrok		(yyerrstatus = 0)
#define yyclearin	(yychar = YYEMPTY)
#define YYEMPTY		(-2)
#define YYEOF		0

#define YYACCEPT	goto yyacceptlab
#define YYABORT		goto yyabortlab
#define YYERROR		goto yyerrorlab


/* Like YYERROR except do call yyerror.  This remains here temporarily
   to ease the transition to the new meaning of YYERROR, for GCC.
   Once GCC version 2 has supplanted version 1, this can go.  */

#define YYFAIL		goto yyerrlab

#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)					\
do								\
  if (yychar == YYEMPTY && yylen == 1)				\
    {								\
      yychar = (Token);						\
      yylval = (Value);						\
      yytoken = YYTRANSLATE (yychar);				\
      YYPOPSTACK (1);						\
      goto yybackup;						\
    }								\
  else								\
    {								\
      yyerror (context, YY_("syntax error: cannot back up")); \
      YYERROR;							\
    }								\
while (YYID (0))


#define YYTERROR	1
#define YYERRCODE	256


/* YYLLOC_DEFAULT -- Set CURRENT to span from RHS[1] to RHS[N].
   If N is 0, then set CURRENT to the empty location which ends
   the previous symbol: RHS[0] (always defined).  */

#define YYRHSLOC(Rhs, K) ((Rhs)[K])
#ifndef YYLLOC_DEFAULT
# define YYLLOC_DEFAULT(Current, Rhs, N)				\
    do									\
      if (YYID (N))                                                    \
	{								\
	  (Current).first_line   = YYRHSLOC (Rhs, 1).first_line;	\
	  (Current).first_column = YYRHSLOC (Rhs, 1).first_column;	\
	  (Current).last_line    = YYRHSLOC (Rhs, N).last_line;		\
	  (Current).last_column  = YYRHSLOC (Rhs, N).last_column;	\
	}								\
      else								\
	{								\
	  (Current).first_line   = (Current).last_line   =		\
	    YYRHSLOC (Rhs, 0).last_line;				\
	  (Current).first_column = (Current).last_column =		\
	    YYRHSLOC (Rhs, 0).last_column;				\
	}								\
    while (YYID (0))
#endif


/* YY_LOCATION_PRINT -- Print the location on the stream.
   This macro was not mandated originally: define only if we know
   we won't break user code: when these are the locations we know.  */

#ifndef YY_LOCATION_PRINT
# if YYLTYPE_IS_TRIVIAL
#  define YY_LOCATION_PRINT(File, Loc)			\
     fprintf (File, "%d.%d-%d.%d",			\
	      (Loc).first_line, (Loc).first_column,	\
	      (Loc).last_line,  (Loc).last_column)
# else
#  define YY_LOCATION_PRINT(File, Loc) ((void) 0)
# endif
#endif


/* YYLEX -- calling `yylex' with the right arguments.  */

#ifdef YYLEX_PARAM
# define YYLEX yylex (&yylval, YYLEX_PARAM)
#else
# define YYLEX yylex (&yylval, context)
#endif

/* Enable debugging if requested.  */
#if YYDEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)			\
do {						\
  if (yydebug)					\
    YYFPRINTF Args;				\
} while (YYID (0))

# define YY_SYMBOL_PRINT(Title, Type, Value, Location)			  \
do {									  \
  if (yydebug)								  \
    {									  \
      YYFPRINTF (stderr, "%s ", Title);					  \
      yy_symbol_print (stderr,						  \
		  Type, Value, context); \
      YYFPRINTF (stderr, "\n");						  \
    }									  \
} while (YYID (0))


/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

/*ARGSUSED*/
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_symbol_value_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep, swq_parse_context *context)
#else
static void
yy_symbol_value_print (yyoutput, yytype, yyvaluep, context)
    FILE *yyoutput;
    int yytype;
    YYSTYPE const * const yyvaluep;
    swq_parse_context *context;
#endif
{
  if (!yyvaluep)
    return;
  YYUSE (context);
# ifdef YYPRINT
  if (yytype < YYNTOKENS)
    YYPRINT (yyoutput, yytoknum[yytype], *yyvaluep);
# else
  YYUSE (yyoutput);
# endif
  switch (yytype)
    {
      default:
	break;
    }
}


/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_symbol_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep, swq_parse_context *context)
#else
static void
yy_symbol_print (yyoutput, yytype, yyvaluep, context)
    FILE *yyoutput;
    int yytype;
    YYSTYPE const * const yyvaluep;
    swq_parse_context *context;
#endif
{
  if (yytype < YYNTOKENS)
    YYFPRINTF (yyoutput, "token %s (", yytname[yytype]);
  else
    YYFPRINTF (yyoutput, "nterm %s (", yytname[yytype]);

  yy_symbol_value_print (yyoutput, yytype, yyvaluep, context);
  YYFPRINTF (yyoutput, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_stack_print (yytype_int16 *yybottom, yytype_int16 *yytop)
#else
static void
yy_stack_print (yybottom, yytop)
    yytype_int16 *yybottom;
    yytype_int16 *yytop;
#endif
{
  YYFPRINTF (stderr, "Stack now");
  for (; yybottom <= yytop; yybottom++)
    {
      int yybot = *yybottom;
      YYFPRINTF (stderr, " %d", yybot);
    }
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)				\
do {								\
  if (yydebug)							\
    yy_stack_print ((Bottom), (Top));				\
} while (YYID (0))


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_reduce_print (YYSTYPE *yyvsp, int yyrule, swq_parse_context *context)
#else
static void
yy_reduce_print (yyvsp, yyrule, context)
    YYSTYPE *yyvsp;
    int yyrule;
    swq_parse_context *context;
#endif
{
  int yynrhs = yyr2[yyrule];
  int yyi;
  unsigned long int yylno = yyrline[yyrule];
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %lu):\n",
	     yyrule - 1, yylno);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      YYFPRINTF (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr, yyrhs[yyprhs[yyrule] + yyi],
		       &(yyvsp[(yyi + 1) - (yynrhs)])
		       		       , context);
      YYFPRINTF (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)		\
do {					\
  if (yydebug)				\
    yy_reduce_print (yyvsp, Rule, context); \
} while (YYID (0))

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
#ifndef	YYINITDEPTH
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
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static YYSIZE_T
yystrlen (const char *yystr)
#else
static YYSIZE_T
yystrlen (yystr)
    const char *yystr;
#endif
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
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static char *
yystpcpy (char *yydest, const char *yysrc)
#else
static char *
yystpcpy (yydest, yysrc)
    char *yydest;
    const char *yysrc;
#endif
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

/* Copy into YYRESULT an error message about the unexpected token
   YYCHAR while in state YYSTATE.  Return the number of bytes copied,
   including the terminating null byte.  If YYRESULT is null, do not
   copy anything; just return the number of bytes that would be
   copied.  As a special case, return 0 if an ordinary "syntax error"
   message will do.  Return YYSIZE_MAXIMUM if overflow occurs during
   size calculation.  */
static YYSIZE_T
yysyntax_error (char *yyresult, int yystate, int yychar)
{
  int yyn = yypact[yystate];

  if (! (YYPACT_NINF < yyn && yyn <= YYLAST))
    return 0;
  else
    {
      int yytype = YYTRANSLATE (yychar);
      YYSIZE_T yysize0 = yytnamerr (0, yytname[yytype]);
      YYSIZE_T yysize = yysize0;
      YYSIZE_T yysize1;
      int yysize_overflow = 0;
      enum { YYERROR_VERBOSE_ARGS_MAXIMUM = 5 };
      char const *yyarg[YYERROR_VERBOSE_ARGS_MAXIMUM];
      int yyx;

# if 0
      /* This is so xgettext sees the translatable formats that are
	 constructed on the fly.  */
      YY_("syntax error, unexpected %s");
      YY_("syntax error, unexpected %s, expecting %s");
      YY_("syntax error, unexpected %s, expecting %s or %s");
      YY_("syntax error, unexpected %s, expecting %s or %s or %s");
      YY_("syntax error, unexpected %s, expecting %s or %s or %s or %s");
# endif
      char *yyfmt;
      char const *yyf;
      static char const yyunexpected[] = "syntax error, unexpected %s";
      static char const yyexpecting[] = ", expecting %s";
      static char const yyor[] = " or %s";
      char yyformat[sizeof yyunexpected
		    + sizeof yyexpecting - 1
		    + ((YYERROR_VERBOSE_ARGS_MAXIMUM - 2)
		       * (sizeof yyor - 1))];
      char const *yyprefix = yyexpecting;

      /* Start YYX at -YYN if negative to avoid negative indexes in
	 YYCHECK.  */
      int yyxbegin = yyn < 0 ? -yyn : 0;

      /* Stay within bounds of both yycheck and yytname.  */
      int yychecklim = YYLAST - yyn + 1;
      int yyxend = yychecklim < YYNTOKENS ? yychecklim : YYNTOKENS;
      int yycount = 1;

      yyarg[0] = yytname[yytype];
      yyfmt = yystpcpy (yyformat, yyunexpected);

      for (yyx = yyxbegin; yyx < yyxend; ++yyx)
	if (yycheck[yyx + yyn] == yyx && yyx != YYTERROR)
	  {
	    if (yycount == YYERROR_VERBOSE_ARGS_MAXIMUM)
	      {
		yycount = 1;
		yysize = yysize0;
		yyformat[sizeof yyunexpected - 1] = '\0';
		break;
	      }
	    yyarg[yycount++] = yytname[yyx];
	    yysize1 = yysize + yytnamerr (0, yytname[yyx]);
	    yysize_overflow |= (yysize1 < yysize);
	    yysize = yysize1;
	    yyfmt = yystpcpy (yyfmt, yyprefix);
	    yyprefix = yyor;
	  }

      yyf = YY_(yyformat);
      yysize1 = yysize + yystrlen (yyf);
      yysize_overflow |= (yysize1 < yysize);
      yysize = yysize1;

      if (yysize_overflow)
	return YYSIZE_MAXIMUM;

      if (yyresult)
	{
	  /* Avoid sprintf, as that infringes on the user's name space.
	     Don't have undefined behavior even if the translation
	     produced a string with the wrong number of "%s"s.  */
	  char *yyp = yyresult;
	  int yyi = 0;
	  while ((*yyp = *yyf) != '\0')
	    {
	      if (*yyp == '%' && yyf[1] == 's' && yyi < yycount)
		{
		  yyp += yytnamerr (yyp, yyarg[yyi++]);
		  yyf += 2;
		}
	      else
		{
		  yyp++;
		  yyf++;
		}
	    }
	}
      return yysize;
    }
}
#endif /* YYERROR_VERBOSE */


/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

/*ARGSUSED*/
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yydestruct (const char *yymsg, int yytype, YYSTYPE *yyvaluep, swq_parse_context *context)
#else
static void
yydestruct (yymsg, yytype, yyvaluep, context)
    const char *yymsg;
    int yytype;
    YYSTYPE *yyvaluep;
    swq_parse_context *context;
#endif
{
  YYUSE (yyvaluep);
  YYUSE (context);

  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yytype, yyvaluep, yylocationp);

  switch (yytype)
    {
      case 3: /* "SWQT_NUMBER" */

/* Line 1000 of yacc.c  */
#line 95 "swq_parser.y"
	{ delete (*yyvaluep); };

/* Line 1000 of yacc.c  */
#line 1271 "swq_parser.cpp"
	break;
      case 4: /* "SWQT_STRING" */

/* Line 1000 of yacc.c  */
#line 95 "swq_parser.y"
	{ delete (*yyvaluep); };

/* Line 1000 of yacc.c  */
#line 1280 "swq_parser.cpp"
	break;
      case 5: /* "SWQT_IDENTIFIER" */

/* Line 1000 of yacc.c  */
#line 95 "swq_parser.y"
	{ delete (*yyvaluep); };

/* Line 1000 of yacc.c  */
#line 1289 "swq_parser.cpp"
	break;
      case 46: /* "logical_expr" */

/* Line 1000 of yacc.c  */
#line 96 "swq_parser.y"
	{ delete (*yyvaluep); };

/* Line 1000 of yacc.c  */
#line 1298 "swq_parser.cpp"
	break;
      case 47: /* "value_expr_list" */

/* Line 1000 of yacc.c  */
#line 96 "swq_parser.y"
	{ delete (*yyvaluep); };

/* Line 1000 of yacc.c  */
#line 1307 "swq_parser.cpp"
	break;
      case 48: /* "field_value" */

/* Line 1000 of yacc.c  */
#line 96 "swq_parser.y"
	{ delete (*yyvaluep); };

/* Line 1000 of yacc.c  */
#line 1316 "swq_parser.cpp"
	break;
      case 49: /* "value_expr" */

/* Line 1000 of yacc.c  */
#line 96 "swq_parser.y"
	{ delete (*yyvaluep); };

/* Line 1000 of yacc.c  */
#line 1325 "swq_parser.cpp"
	break;
      case 50: /* "type_def" */

/* Line 1000 of yacc.c  */
#line 96 "swq_parser.y"
	{ delete (*yyvaluep); };

/* Line 1000 of yacc.c  */
#line 1334 "swq_parser.cpp"
	break;
      case 59: /* "string_or_identifier" */

/* Line 1000 of yacc.c  */
#line 96 "swq_parser.y"
	{ delete (*yyvaluep); };

/* Line 1000 of yacc.c  */
#line 1343 "swq_parser.cpp"
	break;
      case 60: /* "table_def" */

/* Line 1000 of yacc.c  */
#line 96 "swq_parser.y"
	{ delete (*yyvaluep); };

/* Line 1000 of yacc.c  */
#line 1352 "swq_parser.cpp"
	break;

      default:
	break;
    }
}

/* Prevent warnings from -Wmissing-prototypes.  */
#ifdef YYPARSE_PARAM
#if defined __STDC__ || defined __cplusplus
int yyparse (void *YYPARSE_PARAM);
#else
int yyparse ();
#endif
#else /* ! YYPARSE_PARAM */
#if defined __STDC__ || defined __cplusplus
int yyparse (swq_parse_context *context);
#else
int yyparse ();
#endif
#endif /* ! YYPARSE_PARAM */





/*-------------------------.
| yyparse or yypush_parse.  |
`-------------------------*/

#ifdef YYPARSE_PARAM
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
int
yyparse (void *YYPARSE_PARAM)
#else
int
yyparse (YYPARSE_PARAM)
    void *YYPARSE_PARAM;
#endif
#else /* ! YYPARSE_PARAM */
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
int
yyparse (swq_parse_context *context)
#else
int
yyparse (context)
    swq_parse_context *context;
#endif
#endif
{
/* The lookahead symbol.  */
int yychar;

/* The semantic value of the lookahead symbol.  */
YYSTYPE yylval;

    /* Number of syntax errors so far.  */
    int yynerrs;

    int yystate;
    /* Number of tokens to shift before error messages enabled.  */
    int yyerrstatus;

    /* The stacks and their tools:
       `yyss': related to states.
       `yyvs': related to semantic values.

       Refer to the stacks thru separate pointers, to allow yyoverflow
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
  int yytoken;
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

  yytoken = 0;
  yyss = yyssa;
  yyvs = yyvsa;
  yystacksize = YYINITDEPTH;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yystate = 0;
  yyerrstatus = 0;
  yynerrs = 0;
  yychar = YYEMPTY; /* Cause a token to be read.  */

  /* Initialize stack pointers.
     Waste one element of value and location stack
     so that they stay on the same level as the state stack.
     The wasted elements are never initialized.  */
  yyssp = yyss;
  yyvsp = yyvs;

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
  if (yyn == YYPACT_NINF)
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* YYCHAR is either YYEMPTY or YYEOF or a valid lookahead symbol.  */
  if (yychar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token: "));
      yychar = YYLEX;
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
      if (yyn == 0 || yyn == YYTABLE_NINF)
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
  *++yyvsp = yylval;

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
     `$$ = $1'.

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

/* Line 1455 of yacc.c  */
#line 102 "swq_parser.y"
    {
			context->poRoot = (yyvsp[(2) - (2)]);
		;}
    break;

  case 3:

/* Line 1455 of yacc.c  */
#line 107 "swq_parser.y"
    {
			context->poRoot = (yyvsp[(2) - (2)]);
		;}
    break;

  case 4:

/* Line 1455 of yacc.c  */
#line 112 "swq_parser.y"
    {
			context->poRoot = (yyvsp[(2) - (2)]);
		;}
    break;

  case 5:

/* Line 1455 of yacc.c  */
#line 118 "swq_parser.y"
    {
			(yyval) = new swq_expr_node( SWQ_AND );
			(yyval)->field_type = SWQ_BOOLEAN;
			(yyval)->PushSubExpression( (yyvsp[(1) - (3)]) );
			(yyval)->PushSubExpression( (yyvsp[(3) - (3)]) );
		     ;}
    break;

  case 6:

/* Line 1455 of yacc.c  */
#line 126 "swq_parser.y"
    {
			(yyval) = new swq_expr_node( SWQ_OR );
			(yyval)->field_type = SWQ_BOOLEAN;
			(yyval)->PushSubExpression( (yyvsp[(1) - (3)]) );
			(yyval)->PushSubExpression( (yyvsp[(3) - (3)]) );
		     ;}
    break;

  case 7:

/* Line 1455 of yacc.c  */
#line 134 "swq_parser.y"
    {
			(yyval) = new swq_expr_node( SWQ_NOT );
			(yyval)->field_type = SWQ_BOOLEAN;
			(yyval)->PushSubExpression( (yyvsp[(2) - (2)]) );
		     ;}
    break;

  case 8:

/* Line 1455 of yacc.c  */
#line 141 "swq_parser.y"
    {
			(yyval) = (yyvsp[(2) - (3)]);
		     ;}
    break;

  case 9:

/* Line 1455 of yacc.c  */
#line 146 "swq_parser.y"
    {
			(yyval) = new swq_expr_node( SWQ_EQ );
			(yyval)->field_type = SWQ_BOOLEAN;
			(yyval)->PushSubExpression( (yyvsp[(1) - (3)]) );
			(yyval)->PushSubExpression( (yyvsp[(3) - (3)]) );
		     ;}
    break;

  case 10:

/* Line 1455 of yacc.c  */
#line 154 "swq_parser.y"
    {
			(yyval) = new swq_expr_node( SWQ_NE );
			(yyval)->field_type = SWQ_BOOLEAN;
			(yyval)->PushSubExpression( (yyvsp[(1) - (4)]) );
			(yyval)->PushSubExpression( (yyvsp[(4) - (4)]) );
		     ;}
    break;

  case 11:

/* Line 1455 of yacc.c  */
#line 162 "swq_parser.y"
    {
			(yyval) = new swq_expr_node( SWQ_NE );
			(yyval)->field_type = SWQ_BOOLEAN;
			(yyval)->PushSubExpression( (yyvsp[(1) - (4)]) );
			(yyval)->PushSubExpression( (yyvsp[(4) - (4)]) );
		     ;}
    break;

  case 12:

/* Line 1455 of yacc.c  */
#line 170 "swq_parser.y"
    {
			(yyval) = new swq_expr_node( SWQ_LT );
			(yyval)->field_type = SWQ_BOOLEAN;
			(yyval)->PushSubExpression( (yyvsp[(1) - (3)]) );
			(yyval)->PushSubExpression( (yyvsp[(3) - (3)]) );
		     ;}
    break;

  case 13:

/* Line 1455 of yacc.c  */
#line 178 "swq_parser.y"
    {
			(yyval) = new swq_expr_node( SWQ_GT );
			(yyval)->field_type = SWQ_BOOLEAN;
			(yyval)->PushSubExpression( (yyvsp[(1) - (3)]) );
			(yyval)->PushSubExpression( (yyvsp[(3) - (3)]) );
		     ;}
    break;

  case 14:

/* Line 1455 of yacc.c  */
#line 186 "swq_parser.y"
    {
			(yyval) = new swq_expr_node( SWQ_LE );
			(yyval)->field_type = SWQ_BOOLEAN;
			(yyval)->PushSubExpression( (yyvsp[(1) - (4)]) );
			(yyval)->PushSubExpression( (yyvsp[(4) - (4)]) );
		     ;}
    break;

  case 15:

/* Line 1455 of yacc.c  */
#line 194 "swq_parser.y"
    {
			(yyval) = new swq_expr_node( SWQ_LE );
			(yyval)->field_type = SWQ_BOOLEAN;
			(yyval)->PushSubExpression( (yyvsp[(1) - (4)]) );
			(yyval)->PushSubExpression( (yyvsp[(4) - (4)]) );
		     ;}
    break;

  case 16:

/* Line 1455 of yacc.c  */
#line 202 "swq_parser.y"
    {
			(yyval) = new swq_expr_node( SWQ_LE );
			(yyval)->field_type = SWQ_BOOLEAN;
			(yyval)->PushSubExpression( (yyvsp[(1) - (4)]) );
			(yyval)->PushSubExpression( (yyvsp[(4) - (4)]) );
		     ;}
    break;

  case 17:

/* Line 1455 of yacc.c  */
#line 210 "swq_parser.y"
    {
			(yyval) = new swq_expr_node( SWQ_GE );
			(yyval)->field_type = SWQ_BOOLEAN;
			(yyval)->PushSubExpression( (yyvsp[(1) - (4)]) );
			(yyval)->PushSubExpression( (yyvsp[(4) - (4)]) );
		     ;}
    break;

  case 18:

/* Line 1455 of yacc.c  */
#line 218 "swq_parser.y"
    {
			(yyval) = new swq_expr_node( SWQ_LIKE );
			(yyval)->field_type = SWQ_BOOLEAN;
			(yyval)->PushSubExpression( (yyvsp[(1) - (3)]) );
			(yyval)->PushSubExpression( (yyvsp[(3) - (3)]) );
		     ;}
    break;

  case 19:

/* Line 1455 of yacc.c  */
#line 226 "swq_parser.y"
    {
		        swq_expr_node *like;
			like = new swq_expr_node( SWQ_LIKE );
			like->field_type = SWQ_BOOLEAN;
			like->PushSubExpression( (yyvsp[(1) - (4)]) );
			like->PushSubExpression( (yyvsp[(4) - (4)]) );

			(yyval) = new swq_expr_node( SWQ_NOT );
			(yyval)->field_type = SWQ_BOOLEAN;
			(yyval)->PushSubExpression( like );
		     ;}
    break;

  case 20:

/* Line 1455 of yacc.c  */
#line 239 "swq_parser.y"
    {
			(yyval) = (yyvsp[(4) - (5)]);
			(yyval)->field_type = SWQ_BOOLEAN;
			(yyval)->nOperation = SWQ_IN;
			(yyval)->PushSubExpression( (yyvsp[(1) - (5)]) );
			(yyval)->ReverseSubExpressions();
		     ;}
    break;

  case 21:

/* Line 1455 of yacc.c  */
#line 248 "swq_parser.y"
    {
		        swq_expr_node *in;

			in = (yyvsp[(5) - (6)]);
			in->field_type = SWQ_BOOLEAN;
			in->nOperation = SWQ_IN;
			in->PushSubExpression( (yyvsp[(1) - (6)]) );
			in->ReverseSubExpressions();
			
			(yyval) = new swq_expr_node( SWQ_NOT );
			(yyval)->field_type = SWQ_BOOLEAN;
			(yyval)->PushSubExpression( in );
		     ;}
    break;

  case 22:

/* Line 1455 of yacc.c  */
#line 263 "swq_parser.y"
    {
            (yyval) = new swq_expr_node( SWQ_BETWEEN );
            (yyval)->field_type = SWQ_BOOLEAN;
            (yyval)->PushSubExpression( (yyvsp[(1) - (5)]) );
            (yyval)->PushSubExpression( (yyvsp[(3) - (5)]) );
            (yyval)->PushSubExpression( (yyvsp[(5) - (5)]) );
             ;}
    break;

  case 23:

/* Line 1455 of yacc.c  */
#line 272 "swq_parser.y"
    {
            swq_expr_node *between;
            between = new swq_expr_node( SWQ_BETWEEN );
            between->field_type = SWQ_BOOLEAN;
            between->PushSubExpression( (yyvsp[(1) - (6)]) );
            between->PushSubExpression( (yyvsp[(4) - (6)]) );
            between->PushSubExpression( (yyvsp[(6) - (6)]) );

            (yyval) = new swq_expr_node( SWQ_NOT );
            (yyval)->field_type = SWQ_BOOLEAN;
            (yyval)->PushSubExpression( between );
             ;}
    break;

  case 24:

/* Line 1455 of yacc.c  */
#line 286 "swq_parser.y"
    {
			(yyval) = new swq_expr_node( SWQ_ISNULL );
			(yyval)->field_type = SWQ_BOOLEAN;
			(yyval)->PushSubExpression( (yyvsp[(1) - (3)]) );
		     ;}
    break;

  case 25:

/* Line 1455 of yacc.c  */
#line 293 "swq_parser.y"
    {
		        swq_expr_node *isnull;

			isnull = new swq_expr_node( SWQ_ISNULL );
			isnull->field_type = SWQ_BOOLEAN;
			isnull->PushSubExpression( (yyvsp[(1) - (4)]) );

			(yyval) = new swq_expr_node( SWQ_NOT );
			(yyval)->field_type = SWQ_BOOLEAN;
			(yyval)->PushSubExpression( isnull );
		     ;}
    break;

  case 26:

/* Line 1455 of yacc.c  */
#line 307 "swq_parser.y"
    {
			(yyval) = (yyvsp[(3) - (3)]);
			(yyvsp[(3) - (3)])->PushSubExpression( (yyvsp[(1) - (3)]) );
		;}
    break;

  case 27:

/* Line 1455 of yacc.c  */
#line 313 "swq_parser.y"
    {
			(yyval) = new swq_expr_node( SWQ_UNKNOWN ); /* list */
			(yyval)->PushSubExpression( (yyvsp[(1) - (1)]) );
		;}
    break;

  case 28:

/* Line 1455 of yacc.c  */
#line 320 "swq_parser.y"
    {
		        (yyval) = (yyvsp[(1) - (1)]);  // validation deferred.
			(yyval)->eNodeType = SNT_COLUMN;
			(yyval)->field_index = -1; 
		;}
    break;

  case 29:

/* Line 1455 of yacc.c  */
#line 327 "swq_parser.y"
    {
		        (yyval) = (yyvsp[(1) - (3)]);  // validation deferred.
			(yyval)->eNodeType = SNT_COLUMN;
			(yyval)->field_index = -1; 
			(yyval)->string_value = (char *) 
                            CPLRealloc( (yyval)->string_value, 
                                        strlen((yyval)->string_value) 
                                        + strlen((yyvsp[(3) - (3)])->string_value) + 2 );
			strcat( (yyval)->string_value, "." );
			strcat( (yyval)->string_value, (yyvsp[(3) - (3)])->string_value );
			delete (yyvsp[(3) - (3)]);
			(yyvsp[(3) - (3)]) = NULL;
		;}
    break;

  case 30:

/* Line 1455 of yacc.c  */
#line 343 "swq_parser.y"
    {
			(yyval) = (yyvsp[(1) - (1)]);
		;}
    break;

  case 31:

/* Line 1455 of yacc.c  */
#line 348 "swq_parser.y"
    {
			(yyval) = (yyvsp[(1) - (1)]);
		;}
    break;

  case 32:

/* Line 1455 of yacc.c  */
#line 352 "swq_parser.y"
    {
			(yyval) = (yyvsp[(1) - (1)]);
		;}
    break;

  case 33:

/* Line 1455 of yacc.c  */
#line 357 "swq_parser.y"
    {
			(yyval) = (yyvsp[(2) - (3)]);
		;}
    break;

  case 34:

/* Line 1455 of yacc.c  */
#line 362 "swq_parser.y"
    {
            if ((yyvsp[(2) - (2)])->eNodeType == SNT_CONSTANT)
            {
                (yyval) = (yyvsp[(2) - (2)]);
                (yyval)->int_value *= -1;
                (yyval)->float_value *= -1;
            }
            else
            {
                (yyval) = new swq_expr_node( SWQ_MULTIPLY );
                (yyval)->PushSubExpression( new swq_expr_node(-1) );
                (yyval)->PushSubExpression( (yyvsp[(2) - (2)]) );
            }
        ;}
    break;

  case 35:

/* Line 1455 of yacc.c  */
#line 378 "swq_parser.y"
    {
			(yyval) = new swq_expr_node( SWQ_ADD );
			(yyval)->PushSubExpression( (yyvsp[(1) - (3)]) );
			(yyval)->PushSubExpression( (yyvsp[(3) - (3)]) );
		;}
    break;

  case 36:

/* Line 1455 of yacc.c  */
#line 385 "swq_parser.y"
    {
			(yyval) = new swq_expr_node( SWQ_SUBTRACT );
			(yyval)->PushSubExpression( (yyvsp[(1) - (3)]) );
			(yyval)->PushSubExpression( (yyvsp[(3) - (3)]) );
		;}
    break;

  case 37:

/* Line 1455 of yacc.c  */
#line 392 "swq_parser.y"
    {
			(yyval) = new swq_expr_node( SWQ_MULTIPLY );
			(yyval)->PushSubExpression( (yyvsp[(1) - (3)]) );
			(yyval)->PushSubExpression( (yyvsp[(3) - (3)]) );
		;}
    break;

  case 38:

/* Line 1455 of yacc.c  */
#line 399 "swq_parser.y"
    {
			(yyval) = new swq_expr_node( SWQ_DIVIDE );
			(yyval)->PushSubExpression( (yyvsp[(1) - (3)]) );
			(yyval)->PushSubExpression( (yyvsp[(3) - (3)]) );
		;}
    break;

  case 39:

/* Line 1455 of yacc.c  */
#line 406 "swq_parser.y"
    {
			(yyval) = new swq_expr_node( SWQ_MODULUS );
			(yyval)->PushSubExpression( (yyvsp[(1) - (3)]) );
			(yyval)->PushSubExpression( (yyvsp[(3) - (3)]) );
		;}
    break;

  case 40:

/* Line 1455 of yacc.c  */
#line 413 "swq_parser.y"
    {
		    const swq_operation *poOp = 
                      swq_op_registrar::GetOperator( (yyvsp[(1) - (4)])->string_value );

		    if( poOp == NULL )
		    {
		        CPLError( CE_Failure, CPLE_AppDefined, 
                                  "Undefined function '%s' used.",
                                  (yyvsp[(1) - (4)])->string_value );
                delete (yyvsp[(1) - (4)]);
                delete (yyvsp[(3) - (4)]);
		        YYERROR;
		    }
		    else
		    {
			(yyval) = (yyvsp[(3) - (4)]);
                        (yyval)->eNodeType = SNT_OPERATION;
                        (yyval)->nOperation = poOp->eOperation;
			(yyval)->ReverseSubExpressions();
			delete (yyvsp[(1) - (4)]);
		    }
		;}
    break;

  case 41:

/* Line 1455 of yacc.c  */
#line 437 "swq_parser.y"
    {
		    (yyval) = (yyvsp[(5) - (6)]);
		    (yyval)->PushSubExpression( (yyvsp[(3) - (6)]) );
		    (yyval)->ReverseSubExpressions();
		;}
    break;

  case 42:

/* Line 1455 of yacc.c  */
#line 445 "swq_parser.y"
    {
	    (yyval) = new swq_expr_node( SWQ_CAST );
	    (yyval)->PushSubExpression( (yyvsp[(1) - (1)]) );
	;}
    break;

  case 43:

/* Line 1455 of yacc.c  */
#line 451 "swq_parser.y"
    {
	    (yyval) = new swq_expr_node( SWQ_CAST );
	    (yyval)->PushSubExpression( (yyvsp[(3) - (4)]) );
	    (yyval)->PushSubExpression( (yyvsp[(1) - (4)]) );
	;}
    break;

  case 44:

/* Line 1455 of yacc.c  */
#line 458 "swq_parser.y"
    {
	    (yyval) = new swq_expr_node( SWQ_CAST );
	    (yyval)->PushSubExpression( (yyvsp[(5) - (6)]) );
	    (yyval)->PushSubExpression( (yyvsp[(3) - (6)]) );
	    (yyval)->PushSubExpression( (yyvsp[(1) - (6)]) );
	;}
    break;

  case 45:

/* Line 1455 of yacc.c  */
#line 467 "swq_parser.y"
    {
	    delete (yyvsp[(4) - (7)]);
	;}
    break;

  case 48:

/* Line 1455 of yacc.c  */
#line 477 "swq_parser.y"
    {
		if( !context->poSelect->PushField( (yyvsp[(2) - (2)]), NULL, TRUE ) )
        {
            delete (yyvsp[(2) - (2)]);
		    YYERROR;
        }
	    ;}
    break;

  case 49:

/* Line 1455 of yacc.c  */
#line 486 "swq_parser.y"
    {
        if( !context->poSelect->PushField( (yyvsp[(2) - (2)]), NULL, TRUE ) )
        {
            delete (yyvsp[(2) - (2)]);
            YYERROR;
        }
        ;}
    break;

  case 50:

/* Line 1455 of yacc.c  */
#line 495 "swq_parser.y"
    {
		if( !context->poSelect->PushField( (yyvsp[(1) - (1)]) ) )
        {
            delete (yyvsp[(1) - (1)]);
		    YYERROR;
        }
	    ;}
    break;

  case 51:

/* Line 1455 of yacc.c  */
#line 504 "swq_parser.y"
    {
		if( !context->poSelect->PushField( (yyvsp[(2) - (4)]), (yyvsp[(4) - (4)])->string_value, TRUE ))
        {
            delete (yyvsp[(2) - (4)]);
            delete (yyvsp[(4) - (4)]);
		    YYERROR;
        }

		delete (yyvsp[(4) - (4)]);
	    ;}
    break;

  case 52:

/* Line 1455 of yacc.c  */
#line 516 "swq_parser.y"
    {
		if( !context->poSelect->PushField( (yyvsp[(1) - (3)]), (yyvsp[(3) - (3)])->string_value ) )
        {
            delete (yyvsp[(1) - (3)]);
            delete (yyvsp[(3) - (3)]);
		    YYERROR;
        }
		delete (yyvsp[(3) - (3)]);
	    ;}
    break;

  case 53:

/* Line 1455 of yacc.c  */
#line 527 "swq_parser.y"
    {
	        swq_expr_node *poNode = new swq_expr_node();
		poNode->eNodeType = SNT_COLUMN;
		poNode->string_value = CPLStrdup( "*" );
		poNode->table_index = poNode->field_index = -1;

		if( !context->poSelect->PushField( poNode ) )
        {
            delete poNode;
		    YYERROR;
        }
	    ;}
    break;

  case 54:

/* Line 1455 of yacc.c  */
#line 541 "swq_parser.y"
    {
                CPLString osQualifiedField;

                osQualifiedField = (yyvsp[(1) - (3)])->string_value;
                osQualifiedField += ".*";

                delete (yyvsp[(1) - (3)]);
                (yyvsp[(1) - (3)]) = NULL;

	        swq_expr_node *poNode = new swq_expr_node();
		poNode->eNodeType = SNT_COLUMN;
		poNode->string_value = CPLStrdup( osQualifiedField );
		poNode->table_index = poNode->field_index = -1;

		if( !context->poSelect->PushField( poNode ) )
        {
            delete poNode;
		    YYERROR;
        }
	    ;}
    break;

  case 55:

/* Line 1455 of yacc.c  */
#line 563 "swq_parser.y"
    {
	        // special case for COUNT(*), confirm it.
		if( !EQUAL((yyvsp[(1) - (4)])->string_value,"COUNT") )
		{
		    CPLError( CE_Failure, CPLE_AppDefined,
		    	      "Syntax Error with %s(*).", 
			      (yyvsp[(1) - (4)])->string_value );
            delete (yyvsp[(1) - (4)]);
	            YYERROR;
		}

        delete (yyvsp[(1) - (4)]);
        (yyvsp[(1) - (4)]) = NULL;
                
		swq_expr_node *poNode = new swq_expr_node();
		poNode->eNodeType = SNT_COLUMN;
		poNode->string_value = CPLStrdup( "*" );
		poNode->table_index = poNode->field_index = -1;

		swq_expr_node *count = new swq_expr_node( (swq_op)SWQ_COUNT );
		count->PushSubExpression( poNode );

		if( !context->poSelect->PushField( count ) )
        {
            delete count;
		    YYERROR;
        }
	    ;}
    break;

  case 56:

/* Line 1455 of yacc.c  */
#line 593 "swq_parser.y"
    {
	        // special case for COUNT(*), confirm it.
		if( !EQUAL((yyvsp[(1) - (6)])->string_value,"COUNT") )
		{
		    CPLError( CE_Failure, CPLE_AppDefined,
		    	      "Syntax Error with %s(*).", 
			      (yyvsp[(1) - (6)])->string_value );
            delete (yyvsp[(1) - (6)]);
            delete (yyvsp[(6) - (6)]);
	            YYERROR;
		}

        delete (yyvsp[(1) - (6)]);
        (yyvsp[(1) - (6)]) = NULL;

		swq_expr_node *poNode = new swq_expr_node();
		poNode->eNodeType = SNT_COLUMN;
		poNode->string_value = CPLStrdup( "*" );
		poNode->table_index = poNode->field_index = -1;

		swq_expr_node *count = new swq_expr_node( (swq_op)SWQ_COUNT );
		count->PushSubExpression( poNode );

		if( !context->poSelect->PushField( count, (yyvsp[(6) - (6)])->string_value ) )
        {
            delete count;
            delete (yyvsp[(6) - (6)]);
		    YYERROR;
        }

                delete (yyvsp[(6) - (6)]);
	    ;}
    break;

  case 57:

/* Line 1455 of yacc.c  */
#line 627 "swq_parser.y"
    {
	        // special case for COUNT(DISTINCT x), confirm it.
		if( !EQUAL((yyvsp[(1) - (5)])->string_value,"COUNT") )
		{
		    CPLError( CE_Failure, CPLE_AppDefined,
		    	      "DISTINCT keyword can only be used in COUNT() operator." );
            delete (yyvsp[(1) - (5)]);
            delete (yyvsp[(4) - (5)]);
	            YYERROR;
		}

                delete (yyvsp[(1) - (5)]);
                
                swq_expr_node *count = new swq_expr_node( SWQ_COUNT );
                count->PushSubExpression( (yyvsp[(4) - (5)]) );
                
		if( !context->poSelect->PushField( count, NULL, TRUE ) )
        {
            delete count;
		    YYERROR;
        }
	    ;}
    break;

  case 58:

/* Line 1455 of yacc.c  */
#line 651 "swq_parser.y"
    {
	        // special case for COUNT(DISTINCT x), confirm it.
		if( !EQUAL((yyvsp[(1) - (7)])->string_value,"COUNT") )
		{
		    CPLError( CE_Failure, CPLE_AppDefined,
		    	      "DISTINCT keyword can only be used in COUNT() operator." );
            delete (yyvsp[(1) - (7)]);
            delete (yyvsp[(4) - (7)]);
            delete (yyvsp[(7) - (7)]);
	            YYERROR;
		}

                swq_expr_node *count = new swq_expr_node( SWQ_COUNT );
                count->PushSubExpression( (yyvsp[(4) - (7)]) );
                
		if( !context->poSelect->PushField( count, (yyvsp[(7) - (7)])->string_value, TRUE ) )
        {
            delete (yyvsp[(1) - (7)]);
            delete count;
            delete (yyvsp[(7) - (7)]);
		    YYERROR;
        }

                delete (yyvsp[(1) - (7)]);
                delete (yyvsp[(7) - (7)]);
	    ;}
    break;

  case 60:

/* Line 1455 of yacc.c  */
#line 680 "swq_parser.y"
    {	     
	    	 context->poSelect->where_expr = (yyvsp[(2) - (2)]);
	    ;}
    break;

  case 62:

/* Line 1455 of yacc.c  */
#line 686 "swq_parser.y"
    {
	        context->poSelect->PushJoin( (yyvsp[(2) - (7)])->int_value, 
					     (yyvsp[(4) - (7)])->string_value, 
					     (yyvsp[(6) - (7)])->string_value );
                delete (yyvsp[(2) - (7)]);
	        delete (yyvsp[(4) - (7)]);
	        delete (yyvsp[(6) - (7)]);
	    ;}
    break;

  case 63:

/* Line 1455 of yacc.c  */
#line 695 "swq_parser.y"
    {
	        context->poSelect->PushJoin( (yyvsp[(3) - (8)])->int_value, 
					     (yyvsp[(5) - (8)])->string_value, 
					     (yyvsp[(7) - (8)])->string_value );
                delete (yyvsp[(3) - (8)]);
	        delete (yyvsp[(5) - (8)]);
	        delete (yyvsp[(7) - (8)]);
	    ;}
    break;

  case 68:

/* Line 1455 of yacc.c  */
#line 713 "swq_parser.y"
    {
                context->poSelect->PushOrderBy( (yyvsp[(1) - (1)])->string_value, TRUE );
                delete (yyvsp[(1) - (1)]);
                (yyvsp[(1) - (1)]) = NULL;
            ;}
    break;

  case 69:

/* Line 1455 of yacc.c  */
#line 719 "swq_parser.y"
    {
                context->poSelect->PushOrderBy( (yyvsp[(1) - (2)])->string_value, TRUE );
                delete (yyvsp[(1) - (2)]);
                (yyvsp[(1) - (2)]) = NULL;
            ;}
    break;

  case 70:

/* Line 1455 of yacc.c  */
#line 725 "swq_parser.y"
    {
                context->poSelect->PushOrderBy( (yyvsp[(1) - (2)])->string_value, FALSE );
                delete (yyvsp[(1) - (2)]);
                (yyvsp[(1) - (2)]) = NULL;
            ;}
    break;

  case 71:

/* Line 1455 of yacc.c  */
#line 733 "swq_parser.y"
    {
            (yyval) = (yyvsp[(1) - (1)]);
        ;}
    break;

  case 72:

/* Line 1455 of yacc.c  */
#line 737 "swq_parser.y"
    {
            (yyval) = (yyvsp[(1) - (1)]);
        ;}
    break;

  case 73:

/* Line 1455 of yacc.c  */
#line 743 "swq_parser.y"
    {
	    int iTable;
	    iTable =context->poSelect->PushTableDef( NULL, (yyvsp[(1) - (1)])->string_value, 
	    	   				     	   NULL );
	    delete (yyvsp[(1) - (1)]);

	    (yyval) = new swq_expr_node( iTable );
	;}
    break;

  case 74:

/* Line 1455 of yacc.c  */
#line 753 "swq_parser.y"
    {
	    int iTable;
	    iTable = context->poSelect->PushTableDef( NULL, (yyvsp[(1) - (2)])->string_value, 
	    					      (yyvsp[(2) - (2)])->string_value );
	    delete (yyvsp[(1) - (2)]);
	    delete (yyvsp[(2) - (2)]);

	    (yyval) = new swq_expr_node( iTable );
	;}
    break;

  case 75:

/* Line 1455 of yacc.c  */
#line 764 "swq_parser.y"
    {
	    int iTable;
	    iTable = context->poSelect->PushTableDef( (yyvsp[(1) - (3)])->string_value, 
	    					      (yyvsp[(3) - (3)])->string_value, NULL );
	    delete (yyvsp[(1) - (3)]);
	    delete (yyvsp[(3) - (3)]);

	    (yyval) = new swq_expr_node( iTable );
	;}
    break;

  case 76:

/* Line 1455 of yacc.c  */
#line 775 "swq_parser.y"
    {
	    int iTable;
	    iTable = context->poSelect->PushTableDef( (yyvsp[(1) - (4)])->string_value, 
	    				     	      (yyvsp[(3) - (4)])->string_value, 
					     	      (yyvsp[(4) - (4)])->string_value );
	    delete (yyvsp[(1) - (4)]);
	    delete (yyvsp[(3) - (4)]);
	    delete (yyvsp[(4) - (4)]);

	    (yyval) = new swq_expr_node( iTable );
	;}
    break;



/* Line 1455 of yacc.c  */
#line 2592 "swq_parser.cpp"
      default: break;
    }
  YY_SYMBOL_PRINT ("-> $$ =", yyr1[yyn], &yyval, &yyloc);

  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);

  *++yyvsp = yyval;

  /* Now `shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */

  yyn = yyr1[yyn];

  yystate = yypgoto[yyn - YYNTOKENS] + *yyssp;
  if (0 <= yystate && yystate <= YYLAST && yycheck[yystate] == *yyssp)
    yystate = yytable[yystate];
  else
    yystate = yydefgoto[yyn - YYNTOKENS];

  goto yynewstate;


/*------------------------------------.
| yyerrlab -- here on detecting error |
`------------------------------------*/
yyerrlab:
  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
#if ! YYERROR_VERBOSE
      yyerror (context, YY_("syntax error"));
#else
      {
	YYSIZE_T yysize = yysyntax_error (0, yystate, yychar);
	if (yymsg_alloc < yysize && yymsg_alloc < YYSTACK_ALLOC_MAXIMUM)
	  {
	    YYSIZE_T yyalloc = 2 * yysize;
	    if (! (yysize <= yyalloc && yyalloc <= YYSTACK_ALLOC_MAXIMUM))
	      yyalloc = YYSTACK_ALLOC_MAXIMUM;
	    if (yymsg != yymsgbuf)
	      YYSTACK_FREE (yymsg);
	    yymsg = (char *) YYSTACK_ALLOC (yyalloc);
	    if (yymsg)
	      yymsg_alloc = yyalloc;
	    else
	      {
		yymsg = yymsgbuf;
		yymsg_alloc = sizeof yymsgbuf;
	      }
	  }

	if (0 < yysize && yysize <= yymsg_alloc)
	  {
	    (void) yysyntax_error (yymsg, yystate, yychar);
	    yyerror (context, yymsg);
	  }
	else
	  {
	    yyerror (context, YY_("syntax error"));
	    if (yysize != 0)
	      goto yyexhaustedlab;
	  }
      }
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

  /* Do not reclaim the symbols of the rule which action triggered
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
  yyerrstatus = 3;	/* Each real token shifted decrements this.  */

  for (;;)
    {
      yyn = yypact[yystate];
      if (yyn != YYPACT_NINF)
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

  *++yyvsp = yylval;


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

#if !defined(yyoverflow) || YYERROR_VERBOSE
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
     yydestruct ("Cleanup: discarding lookahead",
		 yytoken, &yylval, context);
  /* Do not reclaim the symbols of the rule which action triggered
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
  /* Make sure YYID is used.  */
  return YYID (yyresult);
}



