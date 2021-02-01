#pragma once

#include <iostream>
#include <cstdlib>
#include <string>
#include <cstdio>
#include "compiler.h"

enum symbol_type
{
    VAR,
    ARRAY,
    ITER,
    NOT_FOUND
};

typedef struct symbol
{
    string name;
    long long int lenght;
    long long int start_index;
    bool is_init;
    long long int stored_at;
    symbol_type type;
    string regist;
    bool is_iter = false;
    struct symbol *next;
} symbol;

symbol *put_symbol(string, long long int, long long int, symbol_type);
symbol *get_symbol(string);
void delete_symbol(string);

void add_variable(string, int, bool);
void add_array(string, long long int, long long int, int, bool);

long long int get_var_index(variable *);
symbol_type get_var_type(string);
string get_var_register(string);
bool is_symbol(string);
void set_init(string);
void set_register(string, string);