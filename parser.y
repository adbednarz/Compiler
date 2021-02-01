
%{
#include "symbols.h"

int yylex();
int yyerror(const string str);
extern int yylineno;
extern FILE *yyin;

%}

%union {
    std::string *pidentifier;
    long long int num;
    struct variable *var;
    struct condition *cond;
    struct operation *oper;
    std::vector<std::string> *command;
}

%token DECLARE BEGIN_ END
%token IF WHILE DO REPEAT
%token <command> ELSE
%token FOR 
%token THEN ENDIF FROM TO DOWNTO ENDFOR ENDWHILE UNTIL
%token READ WRITE
%token LEQ GEQ NEQ
%token ASSIGN   
%token ERROR

%token <pidentifier> pidentifier
%token <num> num

%left '+' "-"
%left '*' '/' '%'

%type <command> commands
%type <command> command
%type <oper> expression
%type <cond> condition
%type <var> value
%type <var> r_value
%type <var> identifier

%%
program:

    DECLARE declarations BEGIN_ commands END                         { end_command($4); }
    | BEGIN_ commands END                                            { end_command($2); }
;

declarations:
    declarations',' pidentifier                                     { add_variable(*$3, yylineno, true);          }
    | declarations',' pidentifier'('num':'num')'                    { add_array(*$3, $5, $7, yylineno, true);     }
    | pidentifier                                                   { add_variable(*$1, yylineno, false);         }
    | pidentifier'('num':'num')'                                    { add_array(*$1, $3, $5, yylineno, false);    }
;

commands:

    commands command                                                   { $$ = pass_commands($1, $2);              }
    | command                                                          { $$ = pass_commands($1);                  }
;

command:

    identifier ASSIGN expression';'                                   { $$ = assign_command($1, $3, yylineno);                 }
    | IF condition THEN commands ELSE                                 { $5 = if_else_init();                                   }
      commands ENDIF                                                  { $$ = if_else_command($2, $4, $5, $7, yylineno);        }
    | IF condition THEN commands ENDIF                                { $$ = if_command($2, $4, yylineno);                     }
    | WHILE                                                           { while_init();                                          }
      condition DO commands ENDWHILE                                  { $$ = while_command($3, $5, yylineno);                  }
    | REPEAT                                                          { repeat_init();                                         }
      commands UNTIL                                                  { until_init();                                          }
      condition';'                                                    { $$ = repeat_until_command($3, $6, yylineno);           }
    | FOR pidentifier FROM value TO value                             { $$ = for_init(*$2, $4, $6, yylineno, true);            }
    | FOR pidentifier FROM value DOWNTO value                         { $$ = for_init(*$2, $4, $6, yylineno, false);           }
    | DO commands ENDFOR                                              { $$ = for_command($2);                                  }
    | READ r_value';'                                                 { $$ = read_command($2, yylineno);                       }
    | WRITE value';'                                                  { $$ = write_command($2, yylineno);                      }
;

expression:

    value                       { $$ = expr_val($1, yylineno);       }
    | value '+' value           { $$ = expr($1, $3, PLUS);           }
    | value '-' value           { $$ = expr($1, $3, MINUS);          }
    | value '*' value           { $$ = expr($1, $3, MULT);           }
    | value '/' value           { $$ = expr($1, $3, DIV);            }
    | value '%' value           { $$ = expr($1, $3, MOD);            }
;

condition:

    value '=' value              { $$ = set_condition($1, $3, JEQ);  }
    | value NEQ value            { $$ = set_condition($1, $3, JNEQ); }
    | value '<' value            { $$ = set_condition($3, $1, JGE);  }
    | value '>' value            { $$ = set_condition($1, $3, JGE);  }
    | value LEQ value            { $$ = set_condition($1, $3, JGEQ); }
    | value GEQ value            { $$ = set_condition($3, $1, JGEQ); }
;

value:

    num                         { $$ = num_command($1, yylineno); }
    | identifier                { $$ = id_command($1, yylineno, true);  }
;

r_value:

    identifier                  { $$ = id_command($1, yylineno, false);  }
;

identifier:

    pidentifier                                 { $$ = pid_command(*$1, yylineno);          }
    | pidentifier'('pidentifier')'              { $$ = pid_arr_command(*$1, *$3, yylineno); }
    | pidentifier'('num')'                      { $$ = pid_command(*$1, $3, yylineno);      }
;

%%

int yyerror(string err) 
{
    if (err == "syntax error")
        err = "niepoprawna składnia";
    error(err, yylineno);
    return 1;
}

int main(int argv, char* argc[]) 
{
    if( argv != 3 ) 
    {
        cerr << "Wywołanie programu za pomocą: ./kompilator <plik_wejsciowy> <plik_wyjsciowy>" << endl;
        return 1;
    }

    yyin = fopen(argc[1], "r");
    if (yyin == NULL)
    {
        cout << "Plik nie istnieje" << endl;
        return 1;
    }
    open_file(argc[2]);

	yyparse();

    close_file();

    cout << "\033[32m" << "Kompilacja zakonczona pomyslnie" << "\033[0m" << endl;

    return 0;
}