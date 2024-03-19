/* A Bison parser, made by GNU Bison 3.8.2.  */

/* Bison interface for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2015, 2018-2021 Free Software Foundation,
   Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

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

/* DO NOT RELY ON FEATURES THAT ARE NOT DOCUMENTED in the manual,
   especially those whose name start with YY_ or yy_.  They are
   private implementation details that can be changed or removed.  */

#ifndef YY_SWQ_SWQ_PARSER_HPP_INCLUDED
#define YY_SWQ_SWQ_PARSER_HPP_INCLUDED
/* Debug traces.  */
#ifndef YYDEBUG
#define YYDEBUG 0
#endif
#if YYDEBUG
extern int swqdebug;
#endif

/* Token kinds.  */
#ifndef YYTOKENTYPE
#define YYTOKENTYPE

enum yytokentype
{
    YYEMPTY = -2,
    END = 0,                    /* "end of string"  */
    YYerror = 256,              /* error  */
    YYUNDEF = 257,              /* "invalid token"  */
    SWQT_INTEGER_NUMBER = 258,  /* "integer number"  */
    SWQT_FLOAT_NUMBER = 259,    /* "floating point number"  */
    SWQT_STRING = 260,          /* "string"  */
    SWQT_IDENTIFIER = 261,      /* "identifier"  */
    SWQT_IN = 262,              /* "IN"  */
    SWQT_LIKE = 263,            /* "LIKE"  */
    SWQT_ILIKE = 264,           /* "ILIKE"  */
    SWQT_ESCAPE = 265,          /* "ESCAPE"  */
    SWQT_BETWEEN = 266,         /* "BETWEEN"  */
    SWQT_NULL = 267,            /* "NULL"  */
    SWQT_IS = 268,              /* "IS"  */
    SWQT_SELECT = 269,          /* "SELECT"  */
    SWQT_LEFT = 270,            /* "LEFT"  */
    SWQT_JOIN = 271,            /* "JOIN"  */
    SWQT_WHERE = 272,           /* "WHERE"  */
    SWQT_ON = 273,              /* "ON"  */
    SWQT_ORDER = 274,           /* "ORDER"  */
    SWQT_BY = 275,              /* "BY"  */
    SWQT_FROM = 276,            /* "FROM"  */
    SWQT_AS = 277,              /* "AS"  */
    SWQT_ASC = 278,             /* "ASC"  */
    SWQT_DESC = 279,            /* "DESC"  */
    SWQT_DISTINCT = 280,        /* "DISTINCT"  */
    SWQT_CAST = 281,            /* "CAST"  */
    SWQT_UNION = 282,           /* "UNION"  */
    SWQT_ALL = 283,             /* "ALL"  */
    SWQT_LIMIT = 284,           /* "LIMIT"  */
    SWQT_OFFSET = 285,          /* "OFFSET"  */
    SWQT_EXCEPT = 286,          /* "EXCEPT"  */
    SWQT_EXCLUDE = 287,         /* "EXCLUDE"  */
    SWQT_VALUE_START = 288,     /* SWQT_VALUE_START  */
    SWQT_SELECT_START = 289,    /* SWQT_SELECT_START  */
    SWQT_NOT = 290,             /* "NOT"  */
    SWQT_OR = 291,              /* "OR"  */
    SWQT_AND = 292,             /* "AND"  */
    SWQT_UMINUS = 293,          /* SWQT_UMINUS  */
    SWQT_RESERVED_KEYWORD = 294 /* "reserved keyword"  */
};
typedef enum yytokentype yytoken_kind_t;
#endif

/* Value type.  */
#if !defined YYSTYPE && !defined YYSTYPE_IS_DECLARED
typedef int YYSTYPE;
#define YYSTYPE_IS_TRIVIAL 1
#define YYSTYPE_IS_DECLARED 1
#endif

int swqparse(swq_parse_context *context);

#endif /* !YY_SWQ_SWQ_PARSER_HPP_INCLUDED  */
