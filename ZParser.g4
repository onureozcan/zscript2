parser grammar ZParser;

options {
	tokenVocab = ZLexer;
}

root
        : program
        ;

program
        : statement*
        ;

statement
        : (expression
        | variableDeclaration
        | ret=RET expression?
        )
        ;

function
        : (FUN) LPAREN ((typedIdent) (COMMA (typedIdent))*)? RPAREN (DOUBLE_DOT type=typeDescriptor)? LCURLY
        program
        RCURLY
        ;

typeDescriptor
        : IDENT | (FUN LT (typeDescriptor) (COMMA (typeDescriptor))* GT)
        ;

typedIdent
        : ident=IDENT (DOUBLE_DOT type=typeDescriptor)?
        ;

variableDeclaration
        : VAR typedIdent (EQ expression)?
        ;

expression
        :primaryExpresssion
       | expression bop=DOT expression
       | expression methodCall=LPAREN (expression (COMMA expression)*)? RPAREN
       | prefix=MINUS expression
       | prefix=(TILDE|EX) expression
       | expression bop=(STAR|DIV|MOD) expression
       | expression bop=(PLUS|MINUS) expression
       | expression bop=(LTE | GTE | GT | LT) expression
       | expression bop=AND expression
       | expression bop=OR expression
       | expression bop=(CMP_EQ | CMP_NE) expression
       | expression bop=EQ expression
       ;

primaryExpresssion
      : LPAREN expression RPAREN
      | atom
      ;

atom: (STRING|INT|DECIMAL|TRUE_|FALSE_|NULL_|IDENT|function);
