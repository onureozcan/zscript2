lexer grammar ZLexer;

BlockComment
    :   '/*' .*? '*/'
        -> skip
    ;

LineComment
    :   '//' ~[\r\n]*
        -> skip
    ;

NEWLINE            : '\r' '\n' | '\n' | '\r';
WS                 : [\t ]+ -> skip ;

SEMICOL: ';';
LPAREN: '(';
RPAREN: ')';
LCURLY: '{';
RCURLY: '}';
COMMA: ',';
EQ: '=';
DOT: '.';
MINUS: '-';
TILDE: '~';
STAR: '*';
DIV: '/';
MOD: '%';
PLUS: '+';
LT: '<';
GT: '>';
LTE: '<=';
GTE: '>=';
AND: '&&';
OR: '||';
CMP_EQ: '==';
CMP_NE: '!=';
EX: '!';

VAR: 'var';
FUN: 'fun';
CLASS: 'class';

INT             : '0'|[1-9][0-9]* ;
DECIMAL         : [0-9][0-9]* '.' [0-9]+ ;
STRING
    : '"' SCharSequence? '"'
    ;

fragment
SCharSequence
    :   SChar+
    ;

fragment
SChar
    :   ~["\\\r\n]
    |   EscapeSequence
    |   '\n'   // Added line
    |   '\r\n' // Added line
;
fragment
EscapeSequence
    :   '\\' ['"?abfnrtv\\]
;

IDENT                 : [_]*[A-Za-z0-9_]+ ;
