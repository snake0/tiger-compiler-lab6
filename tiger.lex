%{
/* Lab2 Attention: You are only allowed to add code in this file and start at Line 26.*/
#include <string.h>
#include "util.h"
#include "symbol.h"
#include "errormsg.h"
#include "absyn.h"
#include "y.tab.h"

int charPos=1;

int yywrap(void)
{
   charPos=1;
   return 1;
}

void adjust(void)
{
   EM_tokPos=charPos;
   charPos+=yyleng;
}

/*
* Please don't modify the lines above.
* You can add C declarations of your own below.
*/

unsigned clayer = 0, strleng = 0;
char str[4096];

%}
/* You can add lex definitions here. */
digit [0-9]
letter [a-zA-Z]
id {letter}({letter}|{digit}|_)*

%start COMMENT CSTRING ERROR
%%
<INITIAL>"," {adjust(); return COMMA;}
<INITIAL>":" {adjust(); return COLON;}
<INITIAL>";" {adjust(); return SEMICOLON;}
<INITIAL>"(" {adjust(); return LPAREN;}
<INITIAL>")" {adjust(); return RPAREN;}
<INITIAL>"[" {adjust(); return LBRACK;}
<INITIAL>"]" {adjust(); return RBRACK;}
<INITIAL>"{" {adjust(); return LBRACE;}
<INITIAL>"}" {adjust(); return RBRACE;}
<INITIAL>"." {adjust(); return DOT;}
<INITIAL>"+" {adjust(); return PLUS;}
<INITIAL>"-" {adjust(); return MINUS;}
<INITIAL>"*" {adjust(); return TIMES;}
<INITIAL>"/" {adjust(); return DIVIDE;}
<INITIAL>"=" {adjust(); return EQ;}
<INITIAL>"<>" {adjust(); return NEQ;}
<INITIAL>"<" {adjust(); return LT;}
<INITIAL>"<=" {adjust(); return LE;}
<INITIAL>">" {adjust(); return GT;}
<INITIAL>">=" {adjust(); return GE;}
<INITIAL>"&" {adjust(); return AND;}
<INITIAL>"|" {adjust(); return OR;}
<INITIAL>":=" {adjust(); return ASSIGN;}
<INITIAL>"array" {adjust(); return ARRAY;}
<INITIAL>"if" {adjust(); return IF;}
<INITIAL>"then" {adjust(); return THEN;}
<INITIAL>"else" {adjust(); return ELSE;}
<INITIAL>"while" {adjust(); return WHILE;}
<INITIAL>"for" {adjust(); return FOR;}
<INITIAL>"to" {adjust(); return TO;}
<INITIAL>"do" {adjust(); return DO;}
<INITIAL>"let" {adjust(); return LET;}
<INITIAL>"in" {adjust(); return IN;}
<INITIAL>"end" {adjust(); return END;}
<INITIAL>"of" {adjust(); return OF;}
<INITIAL>"break" {adjust(); return BREAK;}
<INITIAL>"nil" {adjust(); return NIL;}
<INITIAL>"function" {adjust(); return FUNCTION;}
<INITIAL>"var" {adjust(); return VAR;}
<INITIAL>"type" {adjust(); return TYPE;}

<INITIAL>"/*" {adjust(); if (!clayer) { BEGIN COMMENT;}++clayer;}
<COMMENT>"*/" {adjust();--clayer;if (!clayer) BEGIN INITIAL;}
<COMMENT>"/*" {adjust();++clayer;}
<COMMENT>"\n" {adjust(); EM_newline(); continue;}
<COMMENT>. {adjust();}
<COMMENT><<EOF>> {EM_error(EM_tokPos, "comment error"); BEGIN ERROR;}

<INITIAL>\" {adjust(); strleng = 0; BEGIN CSTRING;}
<CSTRING>\" {charPos += yyleng; BEGIN INITIAL; str[strleng] = '\0'; yylval.sval = strleng ? String(str) : String(""); return STRING;}
<CSTRING>\\n {charPos += yyleng; str[strleng] = '\n'; ++strleng;}
<CSTRING>\\t {charPos += yyleng; str[strleng] = '\t'; ++strleng;}
<CSTRING>\\\\ {charPos += yyleng; str[strleng] = '\\'; ++strleng;}
<CSTRING>\\[ \t\n\f]+\\ {charPos += yyleng;}
<CSTRING>\\{digit}{digit}{digit} { charPos += yyleng; str[strleng] = atoi(yytext+1); ++strleng;}
<CSTRING>\\\^{letter} { charPos += yyleng; str[strleng] = 1 + yytext[2] - 'A'; ++strleng;}
<CSTRING>\\\" {charPos += yyleng; str[strleng] = '"'; ++strleng;}
<CSTRING>. {charPos += yyleng; strcpy(str + strleng, yytext); strleng += yyleng;}

<INITIAL>{digit}+ {adjust(); yylval.ival = atoi(yytext); return INT;}
<INITIAL>{id} {adjust(); yylval.sval = String(yytext); return ID;}
<INITIAL>" "|"\t" { adjust(); continue; }
<INITIAL>"\n" { adjust(); EM_newline(); continue; }
<INITIAL>. {adjust(); EM_error(EM_tokPos, "no symbol matched"); BEGIN ERROR;}

<ERROR>.|"\n" {BEGIN INITIAL; yyless(0);}
