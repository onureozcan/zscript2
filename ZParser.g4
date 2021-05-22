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
        | ifStatement
        | forLoop
        | emptyStatement
        | ret=RET expression?
        | BREAK
        | CONTINUE
        )
        ;

emptyStatement
        : SEMICOL
        ;

forLoop :
        FOR LPAREN (variableDeclaration? SEMICOL expression? SEMICOL expression?) RPAREN LCURLY
        program
        RCURLY
        ;

ifStatement
        : IF LPAREN (expression) RPAREN LCURLY
            program
          RCURLY
          (ELSE ifStatement)?
          (ELSE LCURLY
            program
          RCURLY
          )?
        ;

function
        : (FUN) typeParameterBlock? LPAREN ((typedIdent) (COMMA (typedIdent))*)? RPAREN (DOUBLE_DOT type=typeDescriptor)? LCURLY
        program
        RCURLY
        ;

typeParameterBlock
        : (LT (typedIdent) (COMMA (typedIdent))* GT)
        ;

typeDescriptor
        : typeName = (IDENT | FUN) (LT (typeDescriptor) (COMMA (typeDescriptor))* GT)?
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
       | expression bop=(CMP_EQ | CMP_NE) expression
       | expression bop=EQ expression
       | expression bop=AND expression
       | expression bop=OR expression
       ;

primaryExpresssion
      : LPAREN expression RPAREN
      | atom
      ;

atom: (STRING|INT|DECIMAL|TRUE_|FALSE_|NULL_|IDENT|function);
