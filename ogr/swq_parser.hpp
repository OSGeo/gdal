/* A Bison parser, made by GNU Bison 3.0.4.  */

/* Bison interface for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2015 Free Software Foundation, Inc.

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
    SWQT_INTEGER_NUMBER = 258,
    SWQT_FLOAT_NUMBER = 259,
    SWQT_STRING = 260,
    SWQT_IDENTIFIER = 261,
    SWQT_IN = 262,
    SWQT_LIKE = 263,
    SWQT_ILIKE = 264,
    SWQT_ESCAPE = 265,
    SWQT_BETWEEN = 266,
    SWQT_NULL = 267,
    SWQT_IS = 268,
    SWQT_SELECT = 269,
    SWQT_LEFT = 270,
    SWQT_JOIN = 271,
    SWQT_WHERE = 272,
    SWQT_ON = 273,
    SWQT_ORDER = 274,
    SWQT_BY = 275,
    SWQT_FROM = 276,
    SWQT_AS = 277,
    SWQT_ASC = 278,
    SWQT_DESC = 279,
    SWQT_DISTINCT = 280,
    SWQT_CAST = 281,
    SWQT_UNION = 282,
    SWQT_ALL = 283,
    SWQT_LIMIT = 284,
    SWQT_OFFSET = 285,
    SWQT_VALUE_START = 286,
    SWQT_SELECT_START = 287,
    SWQT_NOT = 288,
    SWQT_OR = 289,
    SWQT_AND = 290,
    SWQT_UMINUS = 291,
    SWQT_RESERVED_KEYWORD = 292
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
