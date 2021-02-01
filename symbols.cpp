#include "symbols.h"

symbol *symbol_table = (symbol *)0;
long long int array_offset = 0;

void add_variable(string name, int lineno, bool flag)
{
    symbol *s = get_symbol(name);
    if (s == 0)
        s = put_symbol(name, 1, 1, VAR);
    else 
        error("ponowna deklaracja zmiennej \033[36m" + name + "\033[0m", lineno);
    if (flag)
        set_offset(array_offset);
}

void add_array(string name, long long int start, long long int end, int lineno, bool flag)
{
    symbol *s = get_symbol(name);
    if (s != 0)
        error("ponowna deklaracja zmiennej \033[36m" + name + "\033[0m", lineno);
    else if (start < 0 && end < 0)
        error("tablica nie może być indeksowana ujemnymi liczbami", lineno);
    else if (start > end)
        error("nieprawidłowy zakres tablicy", lineno);
    else
        s = put_symbol(name, (end - (start - 1)), start, ARRAY);
    if (flag)
        set_offset(array_offset);
}

symbol *put_symbol(string name, long long int length, long long int start, symbol_type type)
{
    symbol *current_symbol = new symbol;
    current_symbol->name = name;
    current_symbol->lenght = length;
    current_symbol->start_index = start;
    current_symbol->is_init = false;
    current_symbol->stored_at = -1;
    if (type == ARRAY)
    {
        current_symbol->stored_at = array_offset;
        array_offset += length;        
    }
    current_symbol->type = type;
    current_symbol->next = (struct symbol *)symbol_table;
    symbol_table = current_symbol;
    return current_symbol;
}

symbol *get_symbol(string name)
{
    symbol *ptr = symbol_table;
    while (ptr != (symbol *)0)
    {
        if (ptr->name.compare(name) == 0 && name.compare("") != 0)
            return ptr;
        ptr = ptr->next;
    }
    return 0;
}

long long int get_var_index(variable *current)
{
    if(current)
    {
        symbol *s;
        if (current->type == PTR)
        {
            s = get_symbol(current->index_name);
            if (s != 0)
                return s->stored_at;            
        }
        else
        {
            s = get_symbol(current->name);
            if (s != 0)
            {
                // gdy jakas zmienna to p[x] + 1 - 1
                // gdy jakiś element i talicy tab(10:100) to p[x] + i - 10             
                return s->stored_at + current->index - s->start_index;
            }    
        }

    }
    return -1;
}

void delete_symbol(string name)
{
    symbol *s = get_symbol(name);
    s->name = "";
}

symbol_type get_var_type(string name)
{
    symbol *s = get_symbol(name);
    if (s != 0)
    {
        return s->type;
    }  
    return NOT_FOUND;
}

string get_var_register(string name)
{
    symbol *s = get_symbol(name);
    if (s != 0)
    {
        return s->regist;
    }  
    return "";
}

bool is_symbol(string name)
{
    symbol *s = get_symbol(name);
    if (s == 0)
        return false;
    else
        return true;
}

void set_init(string name)
{
    symbol *s = get_symbol(name);
    s->is_init = true;
}

void set_register(string name, string reg)
{
    symbol *s = get_symbol(name);
    s->regist = reg;
}