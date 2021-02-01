#pragma once

#include <string.h>
#include <iostream>
#include <unistd.h>
#include <fstream>
#include <sstream>
#include <vector>

using namespace std;

enum var_type
{
    VALUE,
    PTR,
    VARIABLE
};

enum cond_type
{
    JEQ,
    JNEQ,
    JGE,
    JGEQ
};

enum oper_type
{
    PLUS,
    MINUS,
    MULT,
    DIV,
    MOD
};

typedef struct variable
{
    string name;
    long long int index;        // w przypadku typu VALUE index to jest wartość
    string index_name;
    var_type type;
} variable;

typedef struct condition
{
    variable *first;
    variable *second;
    cond_type type;
    vector<string> commands;
} condition;

typedef struct operation
{
    variable *first;
    variable *second;
    oper_type type;
} operation;

void error(string, int);
void set_offset(int);

void end_command(vector<string> *);
void check();

vector<string> *pass_commands(vector<string> *, vector<string> *);
vector<string> *pass_commands(vector<string> *);

vector<string> *assign_command(variable *, operation *, int);

vector<string> *if_command(condition *, vector<string> *, int);
vector<string> *if_else_init();
vector<string> *if_else_command(condition *, vector<string> *, vector<string> *, vector<string> *, int);
void while_init();
vector<string> *while_command(condition *, vector<string> *, int);
void repeat_init();
void until_init();
vector<string> *repeat_until_command(vector<string> *, condition *, int);
vector<string> *for_command(vector<string> *);
vector<string> *for_init(string, variable *, variable *, int, bool);
vector<string> *read_command(variable *, int);
vector<string> *write_command(variable *, int);

operation *expr_val(variable *, int);
operation *expr(variable *, variable *, oper_type);

void oper_val(variable *, variable *, int);
void oper_plus(variable *, variable *, variable *);
void oper_minus(variable *, variable *, variable *);
void oper_mult(variable *, variable *, variable *);
void oper_div_mod(variable *, variable *, variable *, bool);

condition *set_condition(variable *, variable *, cond_type);
void do_condition(variable *, variable *, cond_type);

variable *num_command(long long int, int);
variable *id_command(variable *, int, bool);

variable *pid_command(string, int);
variable *pid_command(string, long long int, int);
variable *pid_arr_command(string, string, int);

void create_value_in_register(string, long long int);
void get_from_memory(variable *, string, bool);
void get_from_memory(variable *, string, string, bool);
vector<string> get_all_free_registers();
string get_register(variable *);
void get_register(string, variable *);
void free_register(string);
void erase_priority_registers(string);
void check_free_registers(int);
void set_previous_registers(vector<string>);
vector<string> remember_registers();

string unique_factorization(long long int);
long long get_var_index(variable *);

void open_file(char *);
void pass_to_file(vector<string> *);
void close_file();

void dividing(string, string, string, string, string, string);
void multiplication(string, string, string);