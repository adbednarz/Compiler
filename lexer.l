%option noyywrap
%option yylineno
%option nounput
     
%{
#include <iostream>
#include <string>
#include <vector>
#include "parser.h"
%}

%%
"DECLARE"               { return DECLARE;  }
"BEGIN"                 { return BEGIN_;   }
"END"                   { return END;      }
"READ"                  { return READ;     }
"WRITE"                 { return WRITE;    }
"IF"                    { return IF;       }
"THEN"                  { return THEN;     }
"ELSE"                  { return ELSE;     }
"ENDIF"                 { return ENDIF;    }
"WHILE"                 { return WHILE;    }
"DO"                    { return DO;       }
"ENDWHILE"              { return ENDWHILE; }
"REPEAT"                { return REPEAT;   }
"UNTIL"                 { return UNTIL;    }
"FOR"                   { return FOR;      }
"FROM"                  { return FROM;     }
"TO"                    { return TO;       }  
"DOWNTO"                { return DOWNTO;   }
"ENDFOR"                { return ENDFOR;   }
"!="                    { return NEQ;      }
"<="                    { return LEQ;      }
">="                    { return GEQ;      }
":="                    { return ASSIGN;   }

[_a-z]+                 { yylval.pidentifier = new std::string(yytext); return pidentifier; }
[0-9]+                  { yylval.num = std::stoll(yytext); return num;                  }
[-+()*/%:;,=<>]         { return yytext[0];                                                 }

"["[^]]*"]"             ;
[ \t\r\n]+              ;
.                       { std::cout << yytext[0] << std::endl; return ERROR; }

%%