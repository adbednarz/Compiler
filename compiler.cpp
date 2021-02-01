#include "symbols.h"

vector<string> commands;
vector<string> front_commands;
vector<string> loops;

ofstream result_file;

bool bool_registers[] = {true, true, true, true, true};
variable *registers[5];
string name_registers[] = {"a", "b", "c", "d", "e"};
vector<string> priority_registers;
vector<vector<string>> rem_reg;
vector<int> last_deletions_while;

int last_deletions = 0;
long long int offset = 0;

bool flag_while = false;
bool flag_repeat = false;

void error(string message, int lineno)
{
    cerr << "\033[31m" << "Błąd: " << "\033[0m" << "linia " << lineno << ": " << message  << endl;
    return exit(-1);
}

void set_offset(int number)
{
    offset = number;
}

void end_command(vector<string> *cmds)
{
    cmds->push_back("HALT");
    pass_to_file(cmds);
}

vector<string> *pass_commands(vector<string> *cmds)
{
    return cmds;
}

vector<string> *pass_commands(vector<string> *cmds, vector<string> *cmd)
{ 
    cmds->insert(cmds->end(), cmd->begin(), cmd->end());
    cmd->clear();
    return cmds;
}

vector<string> *assign_command(variable *var, operation *expr, int lineno)
{
    vector<string> *cmds = new vector<string>();  
    if (var)
    {        
        if (get_symbol(var->name)->is_iter)
        {
            error("zmienna " + var->name + " jest iteratorem, a jego modyfikacja jest niedozwolona", lineno);
            return cmds;
        }
        if (var->type != VARIABLE && var->type != PTR)
        {
            error("nieprawidłowa zmienna", lineno);
            return cmds;
        }
        set_init(var->name);
        if (!expr->second)
        {
            string reg = get_var_register(var->name);
            string reg_expr = get_var_register(expr->first->name);
            priority_registers.push_back(reg);
            priority_registers.push_back(reg_expr);
            if (get_var_type(var->name) == VAR)
            {
                if (reg.compare("") == 0)
                {
                    check_free_registers(1);
                    reg = get_register(var);                   
                }
                if (expr->first->type == VALUE)
                    create_value_in_register(reg, expr->first->index);
                else if (reg_expr.compare("") != 0)
                {
                    commands.push_back("RESET " + reg);
                    commands.push_back("ADD " + reg + " " + reg_expr);
                }
                else
                    get_from_memory(expr->first, reg, true);
                set_register(var->name, reg);
            }
            else if (var->type == PTR && expr->first->type == PTR)
            {
                check_free_registers(2);
                vector<string> f = get_all_free_registers();
                get_from_memory(var, f.at(0), false);
                get_from_memory(expr->first, f.at(1), true);
                commands.push_back("STORE " + f.at(1) + " " + f.at(0));             
            }
            else if (var->type != PTR && reg_expr.compare("") != 0)
            {               
                get_from_memory(var, "f", false);
                commands.push_back("STORE " + reg_expr + " f");
            }
            else if (expr->first->type != PTR)
            {
                check_free_registers(1);
                vector<string> f = get_all_free_registers();
                get_from_memory(var, f.at(0), false);
                if (expr->first->type == VALUE)
                {
                    create_value_in_register("f", expr->first->index);
                    commands.push_back("STORE f " + f.at(0));                    
                }
                else if (reg_expr.compare("") != 0)
                {
                    commands.push_back("STORE " + reg_expr + " " + f.at(0)); 
                } 
                else
                {
                    get_from_memory(expr->first, "f", true);
                    commands.push_back("STORE f " + f.at(0));                                      
                }
             
            }
            erase_priority_registers(reg);
            erase_priority_registers(reg_expr);              
        }
        else if (expr->type == PLUS)             
            oper_plus(var, expr->first, expr->second);
        else if (expr->type == MINUS)
            oper_minus(var, expr->first, expr->second);
        else if (expr->type == MULT)
            oper_mult(var, expr->first, expr->second);
        else if (expr->type == DIV)
            oper_div_mod(var, expr->first, expr->second, true);
        else if (expr->type == MOD)
            oper_div_mod(var, expr->first, expr->second, false);
    }
    cmds->insert(cmds->end(), commands.begin(), commands.end());
    commands.clear();    
    return cmds;
}


vector<string> *if_command(condition *cond, vector<string> *cmds, int lineno)
{
    vector<string> *if_cmds = new vector<string>();
    if_cmds->insert(if_cmds->end(), cond->commands.begin(), cond->commands.end());
    set_previous_registers(rem_reg.back());
    rem_reg.pop_back(); 
    cmds->insert(cmds->end(), commands.begin(), commands.end());
    commands.clear();       
    long long int size = cmds->size() + 1;
    if (cond->type == JEQ || cond->type == JGEQ)
    {
        if_cmds->push_back("JZERO f 2");
        if_cmds->push_back("JUMP " + to_string(size));
    }
    else if (cond->type == JGE || cond->type == JNEQ)
    {
        if_cmds->push_back("JZERO f " + to_string(size));
    }
    else
    {
        error("nieprawidłowa konstrukcja warunku", lineno);
    }
    if_cmds->insert(if_cmds->end(), cmds->begin(), cmds->end());

    return if_cmds;
}

vector<string> *if_else_init()
{
    vector<string> *if_else_cmds = new vector<string>();
    vector<string> f = rem_reg.back();
    set_previous_registers(f);
    if_else_cmds->insert(if_else_cmds->end(), commands.begin(), commands.end());  
    commands.clear();   
    return if_else_cmds;
}

vector<string> *if_else_command(condition *cond, vector<string> *cmds, vector<string> *old_regs, vector<string> *else_cmds, int lineno)
{
    vector<string> *if_else_cmds = new vector<string>();
    if_else_cmds->insert(if_else_cmds->end(), cond->commands.begin(), cond->commands.end());
    cmds->insert(cmds->end(), old_regs->begin(), old_regs->end());
    long long int size = cmds->size() + 2;                                      // + 2 bo jeszcze jump z then za else
    if (cond->type == JEQ || cond->type == JGEQ)
    {
        if_else_cmds->push_back("JZERO f 2");
        if_else_cmds->push_back("JUMP " + to_string(size));
    }
    else if (cond->type == JGE || cond->type == JNEQ)
    {
        if_else_cmds->push_back("JZERO f " + to_string(size));
    }
    else
    {
        error("nieprawidłowa konstrukcja warunku", lineno);
    }
    if_else_cmds->insert(if_else_cmds->end(), cmds->begin(), cmds->end());
    set_previous_registers(rem_reg.back());
    rem_reg.pop_back(); 
    else_cmds->insert(else_cmds->end(), commands.begin(), commands.end());
    commands.clear();
    size = else_cmds->size() + 1;
    if_else_cmds->push_back("JUMP " + to_string(size));
    if_else_cmds->insert(if_else_cmds->end(), else_cmds->begin(), else_cmds->end());
    return if_else_cmds;
}

void while_init()
{
    flag_while = true;
}

vector<string> *while_command(condition *cond, vector<string> *cmds, int lineno)
{
    vector<string> *while_cmds = new vector<string>();
    while_cmds->insert(while_cmds->end(), cond->commands.begin(), cond->commands.end());   
    set_previous_registers(rem_reg.back()); 
    rem_reg.pop_back(); 
    cmds->insert(cmds->end(), commands.begin(), commands.end());
    last_deletions = last_deletions_while.back();
    last_deletions_while.pop_back();
    do_condition(cond->first, cond->second, cond->type);                        // program zawsze wykona warunek, a rejestry są sprzedy
    commands.clear();
    long long int size = cmds->size() + 2;                                      // + 2 bo jeszcze jump powrotny na końcu
    if (cond->type == JEQ || cond->type == JGEQ)
    {
        while_cmds->push_back("JZERO f 2");
        while_cmds->push_back("JUMP " + to_string(size));
    }
    else if (cond->type == JGE || cond->type == JNEQ)
    {
        while_cmds->push_back("JZERO f " + to_string(size));
    }
    else
    {
        error("nieprawidłowa konstrukcja warunku", lineno);
    }
    while_cmds->insert(while_cmds->end(), cmds->begin(), cmds->end());
    while_cmds->push_back("JUMP -" + to_string(while_cmds->size()));      
    return while_cmds;
}

void repeat_init() 
{
    rem_reg.push_back(remember_registers());
}

void until_init() 
{
    flag_repeat = true;    
}

vector<string> *repeat_until_command(vector<string> *cmds, condition *cond, int lineno)
{
    vector<string> *repeat_until_cmds = new vector<string>();
    repeat_until_cmds->insert(repeat_until_cmds->end(), cmds->begin(), cmds->end());
    repeat_until_cmds->insert(repeat_until_cmds->end(), cond->commands.begin(), cond->commands.end());  
    set_previous_registers(rem_reg.back());
    rem_reg.pop_back();
    repeat_until_cmds->insert(repeat_until_cmds->end(), commands.begin(), commands.end());
    commands.clear();     
    long long int size = repeat_until_cmds->size();
    if (cond->type == JEQ || cond->type == JGEQ)
    {
        repeat_until_cmds->push_back("JZERO f 2");
        repeat_until_cmds->push_back("JUMP -" + to_string(size + 1));          
    }
    else if (cond->type == JGE || cond->type == JNEQ)
    {
        repeat_until_cmds->push_back("JZERO f -" + to_string(size));      
    }
    else
    {
        error("nieprawidłowa konstrukcja warunku", lineno);
    }
    return repeat_until_cmds;
}

vector<string> *for_init(string iterator, variable *from_value, variable *to_value, int lineno, bool flag)
{
    vector<string> *for_cmds = new vector<string>();
    if (is_symbol(iterator))
        error("zmienna " + iterator + " jest już zadeklarowana i nie może być użyta w pętli for", lineno);
    else            // tworzenie iteratora i licznika pętli
    {
        put_symbol(iterator, 1 , 1, VAR);
        put_symbol(iterator + "counter", 1, 1, VAR);
        set_init(iterator);
        set_init(iterator + "counter");
        get_symbol(iterator)->is_iter = true;
        get_symbol(iterator + "counter")->is_iter = true;
        variable *tmp1 = new variable;
        variable *tmp2 = new variable;
        tmp1->name = iterator;
        tmp1->type = VARIABLE;
        tmp2->name = iterator + "counter";
        tmp2->type = VARIABLE;
        string from_value_reg = get_var_register(from_value->name);
        string to_value_reg = get_var_register(to_value->name);
        priority_registers.push_back(from_value_reg);
        priority_registers.push_back(to_value_reg);
        check_free_registers(2);
        string reg1 = get_register(tmp1);
        string reg2 = get_register(tmp2);
        set_register(tmp1->name, reg1);
        set_register(tmp2->name, reg2);        
        if (from_value && to_value)
        {
            if (from_value->type == VALUE && to_value->type == VALUE)
            {     
                create_value_in_register(reg1, from_value->index);
                if (flag)
                    create_value_in_register(reg2, to_value->index - from_value->index + 2);
                else
                    create_value_in_register(reg2, from_value->index - to_value->index + 2);
            }
            else if (from_value->type == VALUE)
            {
                if (flag)
                {
                    create_value_in_register(reg1, from_value->index);
                    if (to_value_reg.compare("") == 0)
                        get_from_memory(to_value, reg2, true);
                    else
                    {
                        commands.push_back("RESET " + reg2);
                        commands.push_back("ADD " + reg2 + " " + to_value_reg);
                    }
                    commands.push_back("INC " + reg2);
                    commands.push_back("INC " + reg2);                    
                    commands.push_back("SUB " + reg2 + " " + reg1);                                
                }
                else
                {
                    string tmp = "f";
                    if (to_value_reg.compare("") == 0)
                    {
                        if (to_value->type == PTR)
                        {
                            check_free_registers(1);
                            vector<string> f = get_all_free_registers();
                            tmp = f.at(0);
                        }
                        get_from_memory(to_value, tmp, true);                        
                    }
                    else
                        tmp = to_value_reg;
                    create_value_in_register(reg1, from_value->index);           
                    commands.push_back("RESET " + reg2);
                    commands.push_back("ADD " + reg2 + " " + reg1);
                    commands.push_back("INC " + reg2);
                    commands.push_back("INC " + reg2);                     
                    commands.push_back("SUB " + reg2 + " " + tmp);
                }

            }
            else if (to_value->type == VALUE)
            {
                if (flag)
                {
                    if (from_value_reg.compare("") == 0)
                        get_from_memory(from_value, reg1, true);
                    else
                    {
                        commands.push_back("RESET " + reg1);
                        commands.push_back("ADD " + reg1 + " " + from_value_reg);
                    }
                    create_value_in_register(reg2, to_value->index + 2);
                    commands.push_back("SUB " + reg2 + " " + reg1);                
                }
                else
                {
                    if (from_value_reg.compare("") == 0)
                        get_from_memory(from_value, reg1, true);
                    else
                    {
                        commands.push_back("RESET " + reg1);
                        commands.push_back("ADD " + reg1 + " " + from_value_reg);
                    }
                    commands.push_back("RESET " + reg2);
                    commands.push_back("ADD " + reg2 + " " + reg1);
                    if (to_value->index == 0)
                    {
                        commands.push_back("INC " + reg2);
                        commands.push_back("INC " + reg2);
                    }
                    else if (to_value->index == 1)
                    {
                        commands.push_back("INC " + reg2);
                    }
                    else if (to_value->index < 14)
                    {
                        to_value->index -= 2;
                        while ( to_value->index > 0)
                        {
                            commands.push_back("DEC " + reg2);
                            to_value->index -= 1;
                        }
                    }
                    else if (to_value->index > 13)
                    {
                        create_value_in_register("f", to_value->index - 2);
                        commands.push_back("SUB " + reg2 + " f"); 
                    }
                }
            }
            else
            {
                if (from_value_reg.compare("") == 0)
                    get_from_memory(from_value, reg1, true);
                else
                {
                    commands.push_back("RESET " + reg1);
                    commands.push_back("ADD " + reg1 + " " + from_value_reg);
                }                
                if (flag)
                {
                    if (to_value_reg.compare("") == 0)
                        get_from_memory(to_value, reg2, true);
                    else
                    {
                        commands.push_back("RESET " + reg2);
                        commands.push_back("ADD " + reg2 + " " + to_value_reg);
                    }
                    commands.push_back("INC " + reg2);
                    commands.push_back("INC " + reg2);                      
                    commands.push_back("SUB " + reg2 + " "  + reg1);
                }
                else
                {
                    string tmp = "f";
                    if (to_value_reg.compare("") == 0)
                    {
                        if (to_value->type == PTR)
                        {
                            check_free_registers(1);
                            vector<string> f = get_all_free_registers();
                            tmp = f.at(0);
                        }
                        get_from_memory(to_value, tmp, true);                        
                    }
                    else
                    {
                        commands.push_back("RESET f");
                        commands.push_back("ADD f" + to_value_reg);
                    } 
                    commands.push_back("RESET " + reg2);
                    commands.push_back("ADD " + reg2 + " " + reg1);
                    commands.push_back("INC " + reg2);
                    commands.push_back("INC " + reg2);                    
                    commands.push_back("SUB " + reg2 + " " + tmp);                                      
                }
            }
            commands.push_back("DEC " + reg2);            
        }
        erase_priority_registers(from_value_reg);
        erase_priority_registers(to_value_reg);
        rem_reg.push_back(remember_registers());
        if (flag)       
            loops.push_back("INC ");
        else
            loops.push_back("DEC ");
        loops.push_back(iterator);     
    }
    for_cmds->insert(for_cmds->end(), commands.begin(), commands.end());
    commands.clear();
    return for_cmds;
}

vector<string> *for_command(vector<string> *cmds)
{
    vector<string> *for_cmds = new vector<string>();
    string iterator = loops.back();
    loops.pop_back();
    string for_type = loops.back();
    loops.pop_back();    
    set_previous_registers(rem_reg.back());   
    rem_reg.pop_back();    
    commands.push_back(for_type + get_var_register(iterator));   
    cmds->insert(cmds->end(), commands.begin(), commands.end());
    commands.clear();            
    for_cmds->push_back("JZERO " + get_var_register(iterator + "counter") + " " + to_string(cmds->size() + 2));
    for_cmds->insert(for_cmds->end(), cmds->begin(), cmds->end());     
    for_cmds->push_back("JUMP -" + to_string(for_cmds->size() + 1));
    free_register(get_var_register(iterator));    
    delete_symbol(iterator);
    free_register(get_var_register(iterator + "counter"));
    delete_symbol(iterator+"counter");
    return for_cmds;
}

vector<string> *read_command(variable *var, int lineno)
{
    vector<string> *cmds = new vector<string>();
    if (var)
    {
        if (get_symbol(var->name)->is_iter)
        {
            error("zmienna " + var->name + " jest iteratorem, a jego modyfikacja jest niedozwolona", lineno);
            return cmds;
        }         
        set_init(var->name);
        if (get_var_index(var) == -1 && var->type == VARIABLE)
        {
            symbol *s = get_symbol(var->name);
            s->stored_at = offset;
            offset += 1;            
        }
        if (get_var_type(var->name) == VAR)
        {
            if (get_var_register(var->name).compare("") != 0)
            {
                get_from_memory(var, "f", false);
                commands.push_back("GET f");
                commands.push_back("LOAD " + get_var_register(var->name) + " f"); 
            }
            else
            {
                check_free_registers(1);
                string reg = get_register(var);
                get_from_memory(var, reg, false);
                commands.push_back("GET " + reg);
                commands.push_back("LOAD " + reg + " " + reg);
                set_register(var->name, reg);               
            }     
        }
        else if (var->type == PTR)
        {
            check_free_registers(1);
            vector<string> f = get_all_free_registers();
            get_from_memory(var, f.at(0), false);
            commands.push_back("GET " + f.at(0));           
        }
        else
        {
            get_from_memory(var, "f", false);
            commands.push_back("GET f");  
        }
    }
    cmds->insert(cmds->end(), commands.begin(), commands.end());
    commands.clear();
    return cmds;
}

vector<string> *write_command(variable *var, int lineno)
{
    vector<string> *cmds = new vector<string>();
    if (var)
    {
        if (var->type == VALUE)
        {
            check_free_registers(1);
            vector<string> f = get_all_free_registers();
            create_value_in_register(f.at(0), var->index);
            var->index = offset;
            create_value_in_register("f", var->index);
            commands.push_back("STORE " + f.at(0) + " f");
            commands.push_back("PUT f");
            offset += 1;                       
        }
        else if (get_var_register(var->name).compare("") != 0)
        {
            symbol *s = get_symbol(var->name);
            if (s->stored_at == -1)
            {
                s->stored_at = offset;
                offset += 1;
            }   
            get_from_memory(var, "f", false);
            commands.push_back("STORE " + get_var_register(var->name) + " f");
            commands.push_back("PUT f");        
        }
        else if (var->type == PTR)
        {
            check_free_registers(1);
            vector<string> f = get_all_free_registers();
            get_from_memory(var, f.at(0), false);
            commands.push_back("PUT " + f.at(0)); 
        }
        else
        {         
            get_from_memory(var, "f", false);
            commands.push_back("PUT f");           
        }      
    }
    cmds->insert(cmds->end(), commands.begin(), commands.end());
    commands.clear();   
    return cmds;
}

operation *expr_val(variable *var, int lineno)
{
    operation *oper = new operation;
    if (var)
    {
        if (var->type != VALUE && var->type != VARIABLE && var->type != PTR)      
            error("nieprawidłowe wyrażenie", lineno);
        oper->first = var;
        oper->second = nullptr;
    }
    return oper;
}

operation *expr(variable *var1, variable *var2, oper_type type)
{
    operation *oper = new operation;
    if (var1 && var2)
    {
        oper->first = var1;
        oper->second = var2; 
        oper->type = type; 
    }
    return oper;
}

void oper_plus(variable *var, variable *var1, variable *var2)
{
    if (var1 && var2)
    {
        string reg = get_var_register(var->name);
        string reg_var1 = get_var_register(var1->name); 
        string reg_var2 = get_var_register(var2->name);
        priority_registers.push_back(reg);
        priority_registers.push_back(reg_var1);
        priority_registers.push_back(reg_var2);               
        if (var1->type == VALUE && var2->type == VALUE)
        {
            if (reg.compare("") != 0)
                create_value_in_register(reg, var1->index + var2->index);
            else
            {
                check_free_registers(1);
                string reg = get_register(var);
                if (get_var_type(var->name) == VAR)
                {
                    create_value_in_register(reg, var1->index + var2->index);
                    set_register(var->name, reg);
                }
                else
                {
                    get_from_memory(var, reg, false);
                    free_register(reg);
                    create_value_in_register("f", var1->index + var2->index);
                    commands.push_back("STORE f " + reg);               
                }
            }
        }
        else if (var1->type == VALUE && var2->type != VALUE)
        {
            if (get_var_type(var->name) == VAR)
            {           
                if (reg.compare("") == 0)
                {
                    check_free_registers(1);
                    reg = get_register(var);
                    set_register(var->name, reg);
                }
                if (get_var_register(var2->name).compare("") != 0)
                {
                    if (var->name.compare(var2->name) == 0)
                    {
                        if (reg_var2.compare("") == 0)
                            get_from_memory(var2, reg, true);
                        if (var1->index < 12)
                        {
                            while (var1->index > 0)
                            {
                                commands.push_back("INC " + reg);
                                var1->index -= 1;
                            }
                        }
                        else
                        {
                            create_value_in_register("f", var1->index);
                            commands.push_back("ADD " + reg + " f");                                                 
                        }
                    }
                    else
                    {
                        create_value_in_register(reg, var1->index);
                        commands.push_back("ADD " + reg + " " + reg_var2);                        
                    }
                }
                else
                {
                    if (var->name.compare(var2->name) != 0)
                        get_from_memory(var2, reg, true);
                    if (var1->index < 12)
                    {
                        while (var1->index > 0)
                        {
                            commands.push_back("INC " + reg);
                            var1->index -= 1;
                        }
                    }
                    else
                    {
                        create_value_in_register("f", var1->index);
                        commands.push_back("ADD " + reg + " f");                                                 
                    }
                }
            }
            else
            {
                if (reg_var2.compare("") != 0)
                {
                    check_free_registers(1);
                    vector<string> f = get_all_free_registers();
                    get_from_memory(var, f.at(0), false);
                    create_value_in_register("f", var1->index);
                    commands.push_back("ADD f " + reg_var2);
                    commands.push_back("STORE f " + f.at(0));                  
                }
                else
                {
                    check_free_registers(2);
                    vector<string> f = get_all_free_registers();                     
                    get_from_memory(var, f.at(0), false);
                    get_from_memory(var2, "f", true);
                    create_value_in_register(f.at(1), var1->index);
                    commands.push_back("ADD " + f.at(1) + " f");
                    commands.push_back("STORE " + f.at(1) + " " + f.at(0));                     
                }
            }            
        } 
        else if (var1->type != VALUE && var2->type == VALUE)
        {
            if (get_var_type(var->name) == VAR)
            {
                if (reg.compare("") == 0)
                {
                    check_free_registers(1);
                    reg = get_register(var);
                    set_register(var->name, reg);                    
                } 
                if (get_var_register(var1->name).compare("") != 0)
                {
                    if (var->name.compare(var1->name) == 0)
                    {
                        if (reg_var1.compare("") == 0)
                            get_from_memory(var1, reg, true);
                        if (var2->index < 12)
                        {
                            while (var2->index > 0)
                            {
                                commands.push_back("INC " + reg);
                                var2->index -= 1;
                            }
                        }
                        else
                        {
                            create_value_in_register("f", var2->index);
                            commands.push_back("ADD " + reg + " f");                                                 
                        }
                    }
                    else
                    {
                        create_value_in_register(reg, var2->index);
                        commands.push_back("ADD " + reg + " " + reg_var1);                        
                    }
                }
                else
                {
                    get_from_memory(var1, reg, true);
                    if (var2->index < 12)
                    {
                        while (var2->index > 0)
                        {
                            commands.push_back("INC " + reg);
                            var2->index -= 1;
                        }
                    }
                    else
                    {
                        create_value_in_register("f", var2->index);
                        commands.push_back("ADD " + reg + " f");                                                 
                    }
                }
            }
            else
            {
                if (reg_var1.compare("") != 0)
                {
                    check_free_registers(1);
                    vector<string> f = get_all_free_registers();
                    get_from_memory(var, f.at(0), false);
                    create_value_in_register("f", var2->index);
                    commands.push_back("ADD f " + reg_var1);
                    commands.push_back("STORE f " + f.at(0));                  
                }
                else
                {
                    check_free_registers(2);
                    vector<string> f = get_all_free_registers();                    
                    get_from_memory(var, f.at(0), false);
                    get_from_memory(var1, f.at(1), true);
                    create_value_in_register("f", var2->index);
                    commands.push_back("ADD " + f.at(1) + " f");
                    commands.push_back("STORE " + f.at(1) + " " + f.at(0));                      
                }
            }            
        } 
        else if (var1->type != VALUE && var2->type != VALUE)
        {
            if (reg.compare("") == 0)
            {
                check_free_registers(1);
                reg = get_register(var);
                if (get_var_type(var->name) == VAR)
                    set_register(var->name, reg);
                priority_registers.push_back(reg);                
            }
            if (get_var_register(var1->name).compare("") != 0 && get_var_register(var2->name).compare("") != 0)
            {
                if (var->name.compare(var1->name) == 0 && var->name.compare(var2->name) == 0)
                {
                    if (reg_var1.compare("") == 0)
                        get_from_memory(var, reg, true);
                    commands.push_back("SHL " + reg);                    
                }
                else if (var->name.compare(var1->name) == 0) 
                {                 
                    if (reg_var1.compare("") == 0)
                        get_from_memory(var, reg, true); 
                    commands.push_back("ADD " + reg + " " + reg_var2);                     
                }
                else if (var->name.compare(var2->name) == 0) 
                {
                    if (reg_var2.compare("") == 0)
                        get_from_memory(var, reg, true);
                    commands.push_back("ADD " + reg + " " + reg_var1);                     
                }
                else if (var1->name.compare(var2->name) == 0)
                {
                    commands.push_back("RESET " + reg);
                    commands.push_back("ADD " + reg + " " + reg_var1);
                    commands.push_back("SHL " + reg);                  
                }
                else
                {
                    commands.push_back("RESET " + reg);
                    commands.push_back("ADD " + reg + " " + reg_var1);
                    commands.push_back("ADD " + reg + " " + reg_var2);                    
                }
            }
            else if (get_var_register(var1->name).compare("") != 0 && reg_var2.compare("") == 0)
            {
                if (var->name.compare(var1->name) == 0)
                {
                    if (reg_var1.compare("") == 0)
                        get_from_memory(var, reg, true);
                    string tmp = "f";
                    if (var2->type == PTR)
                    {
                        check_free_registers(1);
                        vector<string> f = get_all_free_registers();
                        tmp = f.at(0);
                    }
                    get_from_memory(var2, tmp, true);
                    commands.push_back("ADD " + reg + " " + tmp);
                }
                else
                {
                    get_from_memory(var2, reg, true);
                    commands.push_back("ADD " + reg + " " + reg_var1);                    
                }
            }
            else if (reg_var1.compare("") == 0 && get_var_register(var2->name).compare("") != 0)
            {
                if (var->name.compare(var2->name) == 0)
                {
                    if (reg_var2.compare("") == 0)
                        get_from_memory(var, reg, true);
                    string tmp = "f";
                    if (var1->type == PTR)
                    {
                        check_free_registers(1);
                        vector<string> f = get_all_free_registers();
                        tmp = f.at(0);
                    }
                    get_from_memory(var1, tmp, true);
                    commands.push_back("ADD " + reg + " " + tmp);
                }
                else
                {
                    get_from_memory(var1, reg, true);
                    commands.push_back("ADD " + reg + " " + reg_var2);                    
                }
            }
            else if (var2->type != PTR)
            {
                if (var1->name.compare(var2->name) == 0 && var1->index == var2->index)
                {
                    get_from_memory(var1, reg, true);
                    commands.push_back("SHL " + reg);
                }
                else
                {
                    get_from_memory(var1, reg, true);
                    get_from_memory(var2, "f", true);
                    commands.push_back("ADD " + reg + " f");                    
                }
            }
            else if (var1->type != PTR)
            {
                if (var1->name.compare(var2->name) == 0 && var1->index == var2->index)
                {
                    get_from_memory(var2, reg, true);
                    commands.push_back("SHL " + reg);
                }
                else
                {
                    get_from_memory(var2, reg, true);
                    get_from_memory(var1, "f", true);
                    commands.push_back("ADD " + reg + " f");                    
                }
            }
            else
            {
                if (var1->name.compare(var2->name) == 0 && var1->index_name == var2->index_name)
                {
                    get_from_memory(var1, reg, true);
                    commands.push_back("SHL " + reg);
                }
                else
                {
                    check_free_registers(1);
                    vector<string> f = get_all_free_registers();
                    get_from_memory(var1, reg, true);
                    get_from_memory(var2, f.at(0), true);
                    commands.push_back("ADD " + reg + " " + f.at(0));                    
                }
            }
            erase_priority_registers(reg_var1);
            erase_priority_registers(reg_var2);
            if (get_var_type(var->name) != VAR && var->type != PTR)
            {
                get_from_memory(var, "f", false);
                commands.push_back("STORE " + reg + " f");               
            }
            else if (get_var_type(var->name) != VAR && var->type == PTR)
            {
                check_free_registers(1);
                vector<string> f = get_all_free_registers();
                get_from_memory(var, f.at(0), false);               
                commands.push_back("STORE " + reg + " " + f.at(0));            
            }
            if (get_var_type(var->name) != VAR)
                free_register(reg);                             
        }
        erase_priority_registers(reg);
        erase_priority_registers(reg_var1);
        erase_priority_registers(reg_var2);        
    }
}

void oper_minus(variable *var, variable *var1, variable *var2)
{
    if (var1 && var2)
    {
        string reg = get_var_register(var->name);
        string reg_var1 = get_var_register(var1->name); 
        string reg_var2 = get_var_register(var2->name);
        priority_registers.push_back(reg);
        priority_registers.push_back(reg_var1);
        priority_registers.push_back(reg_var2); 
        if (var1->type == VALUE && var2->type == VALUE)
        {
            int total = 0;
            if (var2->index > var1->index)
                total = 0;
            else 
                total = var1->index - var2->index;
            if (get_var_type(var->name) == VAR)
            {
                if (reg.compare("") == 0)
                {
                    check_free_registers(1);
                    reg = get_register(var);
                    set_register(var->name, reg);                
                }
                create_value_in_register(reg, total);                                
            }
            else
            {
                check_free_registers(1);
                vector<string> f = get_all_free_registers();
                get_from_memory(var, f.at(0), false);
                create_value_in_register("f", total);
                commands.push_back("STORE f " + f.at(0));                             
            }
        }
        else if (var1->type != VALUE && var2->type == VALUE)
        {
            if (get_var_type(var->name) == VAR)
            {
                if (reg.compare("") == 0)
                {
                    check_free_registers(1);
                    reg = get_register(var);
                    set_register(var->name, reg);               
                }
                if (reg_var1.compare("") != 0)
                {
                    if (var->name.compare(var1->name) != 0)
                    {
                        commands.push_back("RESET " + reg);
                        commands.push_back("ADD " + reg + " " + reg_var1);                         
                    }            
                }
                else
                    get_from_memory(var1, reg, true);
                if (var2->index < 12)
                {
                    while (var2->index > 0)
                    {
                        commands.push_back("DEC " + reg);
                        var2->index -= 1;
                    }
                }
                else
                {
                    create_value_in_register("f", var2->index);
                    commands.push_back("SUB " + reg + " f");                      
                }                        
            }
            else
            {
                check_free_registers(2);
                vector<string> f = get_all_free_registers();
                get_from_memory(var, f.at(0), false);
                if (reg_var1.compare("") != 0)
                {
                    commands.push_back("RESET " + f.at(1));
                    commands.push_back("ADD " + f.at(1) + " " + reg_var1);
                }
                else
                    get_from_memory(var1, f.at(1), true);
                create_value_in_register("f", var2->index);
                commands.push_back("SUB " + f.at(1) + " f");
                commands.push_back("STORE " + f.at(1) + " " + f.at(0));
            }          
        }
        else if (var1->type == VALUE && var2->type != VALUE)
        {
            if (get_var_type(var->name) == VAR)
            {
                if (reg.compare("") == 0)
                {
                    check_free_registers(1);
                    reg = get_register(var);
                    set_register(var->name, reg);             
                }
                if (reg_var2.compare("") != 0)
                {
                    commands.push_back("RESET f");
                    commands.push_back("ADD f " + reg_var2);                   
                }
                else
                    get_from_memory(var2, "f", reg, true);
                create_value_in_register(reg, var1->index);
                commands.push_back("SUB " + reg + " f");                         
            }
            else
            {
                check_free_registers(2);
                vector<string> f = get_all_free_registers();
                get_from_memory(var, f.at(0), false);
                if (reg_var2.compare("") != 0)
                {
                    commands.push_back("RESET " + f.at(1));
                    commands.push_back("ADD " + f.at(1) + " " + reg_var2);
                }
                else
                    get_from_memory(var2, f.at(1), true);
                create_value_in_register("f", var1->index);
                commands.push_back("SUB f " + f.at(1));
                commands.push_back("STORE f " + f.at(0));
            }    
        }
        else if (var1->type != VALUE && var2->type != VALUE)
        {
            if (reg.compare("") == 0)
            {
                check_free_registers(1);
                reg = get_register(var);
                if (get_var_type(var->name) == VAR)
                    set_register(var->name, reg);
                priority_registers.push_back(reg);                
            }
            if (var1->name.compare(var2->name) == 0 && var1->index == var2->index && var1->index_name.compare(var2->index_name) == 0)
                commands.push_back("RESET " + reg);
            else if (var->name.compare(var1->name) == 0 && var->index == var1->index && var->index_name.compare(var1->index_name) == 0)
            {
                if (reg_var1.compare("") == 0)
                    get_from_memory(var, reg, true);
                if (reg_var2.compare("") != 0)
                    commands.push_back("SUB " + reg + " " + reg_var2);
                else if (var2->type != PTR)
                {
                    get_from_memory(var2, "f", true);
                    commands.push_back("SUB " + reg + " f");
                }
                else
                {
                    check_free_registers(1);
                    vector<string> f = get_all_free_registers();
                    get_from_memory(var2, f.at(0), true);
                    commands.push_back("SUB " + reg + " " + f.at(0));
                }                   
            }
            else if (var->name.compare(var2->name) == 0 && var->index == var2->index && var->index_name.compare(var2->index_name) == 0)
            {
                if (reg_var2.compare("") == 0)
                    get_from_memory(var, reg, true);
                if (reg_var1.compare("") != 0)
                    commands.push_back("SUB " + reg + " " + reg_var1);
                else if (var1->type != PTR)
                {
                    get_from_memory(var1, "f", true);
                    commands.push_back("SUB " + reg + " f");
                }
                else
                {
                    check_free_registers(1);
                    vector<string> f = get_all_free_registers();
                    get_from_memory(var1, f.at(0), true);
                    commands.push_back("SUB " + reg + " " + f.at(0));
                }                   
            }
            else if (get_var_register(var1->name).compare("") != 0 && get_var_register(var2->name).compare("") != 0)
            {
                commands.push_back("RESET " + reg);                
                if (reg_var1.compare("") == 0)
                    get_from_memory(var, reg, true);
                else
                    commands.push_back("ADD " + reg + " " + reg_var1);
                if (reg_var2.compare("") == 0)
                {
                    get_from_memory(var, "f", true);
                    commands.push_back("SUB " + reg + " f");                    
                }
                else
                    commands.push_back("SUB " + reg + " " + reg_var2);                    
            }
            else if (reg_var1.compare("") != 0 && reg_var2.compare("") == 0)
            {
                get_from_memory(var2, "f", reg, true);
                commands.push_back("RESET " + reg);
                commands.push_back("ADD " + reg + " " + reg_var1);
                commands.push_back("SUB " + reg + " f");
            }
            else if (reg_var1.compare("") == 0 && reg_var2.compare("") != 0)
            {
                get_from_memory(var1, reg, true);
                commands.push_back("SUB " + reg + " " + reg_var2);
            }
            else if (var2->type != PTR)
            {
                get_from_memory(var1, reg, true); 
                get_from_memory(var2, "f", true);
                commands.push_back("SUB " + reg + " f");  
            }
            else if (var1->type != PTR)
            {
                get_from_memory(var2, "f", reg, true);
                get_from_memory(var1, reg, true);
                commands.push_back("SUB " + reg + " f");             
            }
            else
            {
                check_free_registers(1);
                vector<string> f = get_all_free_registers();
                get_from_memory(var1, reg, true);
                get_from_memory(var2, f.at(0), true);
                commands.push_back("SUB " + reg + " " + f.at(0));                     
            }
            erase_priority_registers(reg_var1);
            erase_priority_registers(reg_var2);
            if (get_var_type(var->name) != VAR && var->type != PTR)
            {
                get_from_memory(var, "f", false);               
                commands.push_back("STORE " + reg + " f");               
            }
            else if (get_var_type(var->name) != VAR && var->type == PTR)
            {
                check_free_registers(1);
                vector<string> f = get_all_free_registers();
                get_from_memory(var, f.at(0), false);                
                commands.push_back("STORE " + reg + " " + f.at(0));            
            }
            if (get_var_type(var->name) != VAR)
                free_register(reg);                    
        }
        erase_priority_registers(reg);
        erase_priority_registers(reg_var1);
        erase_priority_registers(reg_var2);       
    }
}

void oper_mult(variable *var, variable *var1, variable *var2)
{
    if (var1 && var2)
    {
        string reg = get_var_register(var->name);
        string reg_var1 = get_var_register(var1->name); 
        string reg_var2 = get_var_register(var2->name);
        priority_registers.push_back(reg);
        priority_registers.push_back(reg_var1);
        priority_registers.push_back(reg_var2);
        if (var1->type == VALUE && var2->type == VALUE)
        {
            if (get_var_type(var->name) == VAR)
            {
                if (reg.compare("") == 0)
                {
                    check_free_registers(1);
                    reg = get_register(var);
                    set_register(var->name, reg);                
                }
                create_value_in_register(reg, var1->index * var2->index);                                
            }
            else
            {
                check_free_registers(1);
                vector<string> f = get_all_free_registers();
                get_from_memory(var, f.at(0), false);
                create_value_in_register("f", var1->index * var2->index);
                commands.push_back("STORE f " + f.at(0));                             
            }
        }
        else if (var1->type != VALUE && var2->type == VALUE)
        {
            string reg_arr;
            if (get_var_type(var->name) != VAR)
            {
                check_free_registers(1);
                reg_arr = get_register(var);
                get_from_memory(var, reg_arr, false);
                priority_registers.push_back(reg_arr);
            }
            if (reg.compare("") == 0)
            {
                check_free_registers(1);
                reg = get_register(var);
                priority_registers.push_back(reg);
                if (get_var_type(var->name) == VAR)
                    set_register(var->name, reg);               
            }
            if (var2->index == 0)
            {
                if (get_var_type(var->name) == VAR && var1->name.compare(var->name) == 0 && reg_var1.compare("") == 0)
                    get_from_memory(var, reg, true);
                commands.push_back("RESET " + reg);
            }
            else if (var2->index == 1)
            {
                if (var->name.compare(var1->name) != 0 || var->index != var1->index || var->index_name.compare(var1->index_name) != 0)
                    get_from_memory(var1, reg, true);                    
            }            
            else if (get_var_type(var->name) == VAR && var->name.compare(var1->name) == 0)
            {
                if (reg_var1.compare("") == 0)
                    get_from_memory(var1, reg, true);
            }         
            else if (reg_var1.compare("") != 0)
            {
                commands.push_back("RESET " + reg);
                commands.push_back("ADD " + reg + " " + reg_var1);                   
            }
            else
                get_from_memory(var1, reg, true);

            if ((var2->index & (var2->index - 1)) == 0 && var2->index != 1 && var2->index != 0)
            {
                while (var2->index != 1)
                {
                    commands.push_back("SHL " + reg);
                    var2->index /= 2;                        
                }
            }
            else if (var2->index != 1)
            {
                check_free_registers(1);
                string tmp = get_register(var);
                create_value_in_register("f", var2->index);
                multiplication(tmp, reg, "f");
                if (get_var_type(var->name) == VAR)
                    set_register(var->name, tmp); 
                free_register(reg);
                erase_priority_registers(reg);
                reg = tmp;                      
            }
            if (get_var_type(var->name) != VAR)
            {
                commands.push_back("STORE " + reg + " " + reg_arr);
                free_register(reg);
                free_register(reg_arr);
                erase_priority_registers(reg_arr);
            }              
        }
        else if (var1->type == VALUE && var2->type != VALUE)
        {
            string reg_arr;
            if (get_var_type(var->name) != VAR)
            {
                check_free_registers(1);
                reg_arr = get_register(var);
                get_from_memory(var, reg_arr, false);
                priority_registers.push_back(reg_arr);
            }
            if (reg.compare("") == 0)
            {
                check_free_registers(1);
                reg = get_register(var);
                priority_registers.push_back(reg);
                if (get_var_type(var->name) == VAR)
                    set_register(var->name, reg);               
            }
            if (var1->index == 0)
            {
                if (get_var_type(var->name) == VAR && var2->name.compare(var->name) == 0 && reg_var2.compare("") == 0)
                    get_from_memory(var, reg, true);
                commands.push_back("RESET " + reg);
            }
            else if (var1->index == 1)
            {
                if (var->name.compare(var1->name) != 0 || var->index != var1->index || var->index_name.compare(var1->index_name) != 0)
                    get_from_memory(var2, reg, true);
            } 
            else if (get_var_type(var->name) == VAR && var->name.compare(var2->name) == 0)
            {
                if (reg_var2.compare("") == 0)
                    get_from_memory(var2, reg, true);
            }           
            else if (reg_var2.compare("") != 0)
            {
                commands.push_back("RESET " + reg);
                commands.push_back("ADD " + reg + " " + reg_var2);                   
            }
            else
                get_from_memory(var2, reg, true);

            if ((var1->index & (var1->index - 1)) == 0&& var1->index != 1 && var1->index != 0)
            {
                while (var1->index != 1)
                {
                    commands.push_back("SHL " + reg);
                    var1->index /= 2;                        
                }
            }
            else if (var1->index != 1)
            {
                check_free_registers(1);
                string tmp = get_register(var);
                create_value_in_register("f", var1->index);
                multiplication(tmp, reg, "f");
                if (get_var_type(var->name) == VAR)
                    set_register(var->name, tmp);
                free_register(reg);
                erase_priority_registers(reg);
                reg = tmp;                      
            }
            if (get_var_type(var->name) != VAR)
            {
                commands.push_back("STORE " + reg + " " + reg_arr);
                free_register(reg);
                free_register(reg_arr);
                erase_priority_registers(reg_arr);
            }    
        }
        else if (var1->type != VALUE && var2->type != VALUE)
        {
            if (get_var_type(var->name) == VAR)
            {
                string tmp1;
                string tmp2;
                if (reg.compare("") == 0)
                {
                    check_free_registers(1);
                    reg = get_register(var);
                    priority_registers.push_back(reg);               
                }
                if (var->name.compare(var1->name) == 0 && var->name.compare(var2->name) == 0)
                {
                    if (get_var_register(var->name).compare("") == 0)
                        get_from_memory(var, reg, true);
                    check_free_registers(1);
                    vector<string> f = get_all_free_registers();
                    tmp1 = f.at(0);
                    tmp2 = "f";
                    commands.push_back("RESET " + tmp1);
                    commands.push_back("ADD " + tmp1 + " " + reg);
                    commands.push_back("RESET " + tmp2);
                    commands.push_back("ADD " + tmp2 + " " + reg);         
                }
                else if (var1->name.compare(var2->name) == 0 && var1->index == var2->index && var1->index_name.compare(var2->index_name) == 0)
                {
                    check_free_registers(1);
                    vector<string> f = get_all_free_registers();
                    tmp1 = f.at(0);
                    tmp2 = "f";
                    commands.push_back("RESET " + tmp1);
                    get_from_memory(var1, tmp1, true);
                    commands.push_back("RESET " + tmp2);
                    commands.push_back("ADD " + tmp2 + " " + tmp1);
                }
                else if (var1->type != PTR)
                {
                    check_free_registers(1);
                    vector<string> f = get_all_free_registers();
                    tmp1 = "f";
                    tmp2 = f.at(0);
                    if (reg_var2.compare("") != 0)
                    {
                        commands.push_back("RESET " + tmp2);
                        commands.push_back("ADD " + tmp2 + " " + reg_var2);                   
                    }
                    else
                        get_from_memory(var2, tmp2, true);                     
                    if (reg_var1.compare("") != 0)
                    {
                        commands.push_back("RESET " + tmp1);
                        commands.push_back("ADD " + tmp1 + " " + reg_var1);                   
                    }
                    else
                        get_from_memory(var1, tmp1, true);
                }
                else if (var2->type != PTR)
                {
                    check_free_registers(1);
                    vector<string> f = get_all_free_registers();
                    tmp1 = f.at(0);
                    tmp2 = "f";
                    if (reg_var1.compare("") != 0)
                    {
                        commands.push_back("RESET " + tmp1);
                        commands.push_back("ADD " + tmp1 + " " + reg_var1);                   
                    }
                    else
                        get_from_memory(var1, tmp1, true);
                    if (reg_var2.compare("") != 0)
                    {
                        commands.push_back("RESET " + tmp2);
                        commands.push_back("ADD " + tmp2 + " " + reg_var2);                   
                    }
                    else
                        get_from_memory(var2, tmp2, true);                      
                }
                else
                {
                    check_free_registers(2);
                    vector<string> f = get_all_free_registers();
                    tmp1 = f.at(0);
                    tmp2 = f.at(1);
                    get_from_memory(var1, tmp1, true);
                    get_from_memory(var2, tmp2, true);                    
                }
                set_register(var->name, reg);
                multiplication(reg, tmp1, tmp2);
            }
            else
            {
                check_free_registers(3);
                vector<string> f = get_all_free_registers();
                get_from_memory(var, f.at(0), false);
                if (var->name.compare(var1->name) == 0 && var->index == var1->index && var->index_name.compare(var1->index_name) == 0 &&
                        var->name.compare(var2->name) == 0 && var->index == var2->index && var->index_name.compare(var2->index_name) == 0)
                {       
                    commands.push_back("LOAD " + f.at(1) + " " + f.at(0));
                    commands.push_back("RESET " + f.at(2));
                    commands.push_back("ADD " + f.at(2) + " " + f.at(1)); 
                }
                else if (var1->name.compare(var2->name) == 0 && var1->index == var2->index && var1->index_name.compare(var2->index_name) == 0)
                {
                    commands.push_back("RESET " + f.at(2));                    
                    if (reg_var1.compare("") != 0)
                    {
                        commands.push_back("RESET " + f.at(1));
                        commands.push_back("ADD " + f.at(1) + " " + reg_var1);
                        commands.push_back("ADD " + f.at(2) + " " + reg_var1);
                    }
                    else
                    {
                        get_from_memory(var1, f.at(1), true);
                        commands.push_back("ADD " + f.at(2) + " " + f.at(1));
                    }  
                }
                else if (var->name.compare(var1->name) == 0 && var->index == var1->index && var->index_name.compare(var1->index_name) == 0)
                {
                   commands.push_back("LOAD " + f.at(1) + " " + f.at(0));
                    if (reg_var2.compare("") != 0)
                    {
                        commands.push_back("RESET " + f.at(2));
                        commands.push_back("ADD " + f.at(2) + " " + reg_var2);                   
                    }               
                    else
                        get_from_memory(var2, f.at(2), true);  
                }
                else if (var->name.compare(var2->name) == 0 && var->index == var2->index && var->index_name.compare(var2->index_name) == 0)
                {
                   commands.push_back("LOAD " + f.at(2) + " " + f.at(0));
                    if (reg_var1.compare("") != 0)
                    {
                        commands.push_back("RESET " + f.at(1));
                        commands.push_back("ADD " + f.at(1) + " " + reg_var1);                   
                    }               
                    else
                        get_from_memory(var1, f.at(1), true);  
                }
                else
                {
                    if (reg_var1.compare("") != 0)
                    {
                        commands.push_back("RESET " + f.at(1));
                        commands.push_back("ADD " + f.at(1) + " " + reg_var1);                  
                    }
                    else
                        get_from_memory(var1, f.at(1), true);
                    if (reg_var2.compare("") != 0)
                    {
                        commands.push_back("RESET " + f.at(2));
                        commands.push_back("ADD " + f.at(2) + " " + reg_var2);                   
                    }               
                    else
                        get_from_memory(var2, f.at(2), true);
                }
                multiplication("f", f.at(1), f.at(2));
                commands.push_back("STORE f " + f.at(0));
            } 
        }
        erase_priority_registers(reg);
        erase_priority_registers(reg_var1);
        erase_priority_registers(reg_var2); 
    }
}

void oper_div_mod(variable *var, variable *var1, variable *var2, bool flag)
{
    if (var1 && var2)
    {
        string reg = get_var_register(var->name);
        string reg_var1 = get_var_register(var1->name); 
        string reg_var2 = get_var_register(var2->name);
        priority_registers.push_back(reg);
        priority_registers.push_back(reg_var1);
        priority_registers.push_back(reg_var2);
        if (var1->type == VALUE && var2->type == VALUE)
        {
            long long int result;
            if (var2->index == 0)
                result = 0;
            else if (flag)
                result  = var1->index / var2->index;
            else
                result = var1->index % var2->index;
            if (get_var_type(var->name) == VAR)
            {
                if (reg.compare("") == 0)
                {
                    check_free_registers(1);
                    reg = get_register(var);
                    set_register(var->name, reg);
                }
                create_value_in_register(reg, result);
            }
            else
            {
                check_free_registers(1);
                vector<string> f = get_all_free_registers();
                get_from_memory(var, f.at(0), false);
                create_value_in_register("f", result);
                commands.push_back("STORE f " + f.at(0));
            }           
        }
        else if ((var1->type == VALUE && var1->index == 0) || (var2->type == VALUE && var2->index == 0) || (var2->type == VALUE && var2->index == 1 && !flag))
        {
            if (get_var_type(var->name) == VAR)
            {
                if (reg.compare("") == 0)
                {
                    check_free_registers(1);
                    reg = get_register(var);
                    set_register(var->name, reg);                    
                }
                commands.push_back("RESET " + reg);
            }
            else
            {
                check_free_registers(1);
                vector<string> f = get_all_free_registers();
                get_from_memory(var, f.at(0), false);
                commands.push_back("RESET f");
                commands.push_back("STORE f " + f.at(0));               
            }
        }
        else if (var2->type == VALUE && var2->index == 1)
        {
            if (var->name.compare(var1->name) == 0 && var->index_name.compare(var1->index_name) == 0 && var->index == var1->index)
            {
                if (get_var_type(var->name) == VAR && reg.compare("") == 0)
                {
                    check_free_registers(1);
                    reg = get_register(var);
                    set_register(var->name, reg);
                    get_from_memory(var, reg, true);                    
                }
            }
            else if (get_var_type(var->name) == VAR)
            {
                if (reg.compare("") == 0)
                {
                    check_free_registers(1);
                    reg = get_register(var);
                    set_register(var->name, reg);
                }
                if (reg_var1.compare("") != 0)
                {
                    commands.push_back("RESET " + reg);
                    commands.push_back("ADD " + reg + " " + reg_var1);
                }
                else
                    get_from_memory(var1, reg, true);
            }
            else
            {
                string tmp;
                string tmp2;
                if (var->type == PTR && var1->type == PTR)
                {
                    check_free_registers(2);
                    vector<string> f = get_all_free_registers();
                    tmp = f.at(0);
                    tmp2 = f.at(1);
                    get_from_memory(var, tmp, false);
                    get_from_memory(var1, tmp2, true);
                }
                else if (var->type == PTR)
                {
                    check_free_registers(1);
                    vector<string> f = get_all_free_registers();
                    tmp = f.at(0);
                    tmp2 = "f";
                    get_from_memory(var, tmp, false);
                    if (reg_var1.compare("") == 0)
                        get_from_memory(var1, tmp2, true);
                    else
                    {
                        commands.push_back("RESET " + tmp2);
                        commands.push_back("ADD " + tmp2 + " " + reg_var1);                        
                    }                   
                }
                else
                {
                    check_free_registers(1);
                    vector<string> f = get_all_free_registers();
                    tmp = f.at(0);
                    tmp2 = "f";
                    if (reg_var1.compare("") == 0)
                        get_from_memory(var1, tmp, true);
                    else
                    {
                        commands.push_back("RESET " + tmp);
                        commands.push_back("ADD " + tmp + " " + reg_var1);                        
                    }
                    get_from_memory(var, tmp2, false);
                    string swap = tmp;
                    tmp = tmp2;
                    tmp2 = swap;                  
                }
                commands.push_back("STORE " + tmp2 + " " + tmp);                    
            } 
        }
        else if (flag && var2->type == VALUE && (var2->index & (var2->index - 1)) == 0)
        {
            if (get_var_type(var->name) == VAR)
            {
                if (reg.compare("") == 0)
                {
                    check_free_registers(1);
                    reg = get_register(var);
                    set_register(var->name, reg);
                    if (reg_var1.compare("") == 0)
                        get_from_memory(var1, reg, true);
                    else
                    {
                        commands.push_back("RESET " + reg);
                        commands.push_back("ADD " + reg + " " + reg_var1);
                    }    
                }
                else
                {
                    if (reg_var1.compare("") == 0)
                        get_from_memory(var1, reg, true);
                    else if (var->name.compare(var1->name) != 0)
                    {
                        commands.push_back("RESET " + reg);
                        commands.push_back("ADD " + reg + " " + reg_var1);
                    }                     
                }
                while (var2->index != 1)
                {
                    commands.push_back("SHR " + reg);
                    var2->index /= 2;                        
                }
            }
            else
            {
                string tmp;
                string tmp2;
                if (var->type == PTR && var1->type == PTR)
                {
                    check_free_registers(2);
                    vector<string> f = get_all_free_registers();
                    tmp = f.at(0);
                    tmp2 = f.at(1);
                    get_from_memory(var, tmp, false);
                    get_from_memory(var1, tmp2, true);
                }
                else if (var->type == PTR)
                {
                    check_free_registers(1);
                    vector<string> f = get_all_free_registers();
                    tmp = f.at(0);
                    tmp2 = "f";
                    get_from_memory(var, tmp, false);
                    if (reg_var1.compare("") == 0)
                        get_from_memory(var1, tmp2, true);
                    else
                    {
                        commands.push_back("RESET " + tmp2);
                        commands.push_back("ADD " + tmp2 + " " + reg_var1);                        
                    }                   
                }
                else
                {
                    check_free_registers(1);
                    vector<string> f = get_all_free_registers();
                    tmp = f.at(0);
                    tmp2 = "f";
                    if (reg_var1.compare("") == 0)
                        get_from_memory(var1, tmp, true);
                    else
                    {
                        commands.push_back("RESET " + tmp);
                        commands.push_back("ADD " + tmp + " " + reg_var1);                        
                    }
                    get_from_memory(var, tmp2, false);
                    string swap = tmp;
                    tmp = tmp2;
                    tmp2 = swap;                  
                }
                while (var2->index != 1)
                {
                    commands.push_back("SHR " + tmp2);
                    var2->index /= 2;                        
                }
                commands.push_back("STORE " + tmp2 + " " + tmp);              
            }
        }
        else if (var1->name.compare(var2->name) == 0 && var1->index_name.compare(var2->index_name) == 0 && var1->index == var2->index)
        {
            if (get_var_type(var->name) != VAR)
            {
                check_free_registers(1);
                vector<string> f = get_all_free_registers();
                get_from_memory(var, f.at(0), false);
                if (flag)
                    create_value_in_register("f", 1);
                else
                    create_value_in_register("f", 0); 
                commands.push_back("STORE f " + f.at(0));
            }
            else
            {
                if (reg.compare("") == 0)
                {
                    check_free_registers(1);
                    reg = get_register(var);
                    set_register(var->name, reg);                    
                }
                if (flag)
                    create_value_in_register(reg, 1);
                else
                    create_value_in_register(reg, 0);
            }
        }
        else 
        {
            erase_priority_registers(reg);
            free_register(reg);
            erase_priority_registers(reg_var1);
            erase_priority_registers(reg_var2);
            check_free_registers(5);
            get_register(reg_var1, var);
            get_register(reg_var2, var);
            if (var1->type != VALUE)
            {
                if (reg_var1.compare("") == 0)
                {
                    reg_var1 = get_register(var);
                    get_from_memory(var1, reg_var1, true);                
                }                    
            }
            else
            {
                reg_var1 = get_register(var);
                create_value_in_register(reg_var1, var1->index);           
            }
            if (var2->type != VALUE)
            {
                if (reg_var2.compare("") == 0)
                {
                    reg_var2 = get_register(var);
                    get_from_memory(var2, reg_var2, true);  
                }               
            }
            else
            {
                reg_var2 = get_register(var);
                create_value_in_register(reg_var2, var2->index);                
            }
            string a = get_register(var);
            vector<string> free_reg = get_all_free_registers();
            dividing(a, reg_var1, reg_var2, free_reg.at(0), free_reg.at(1), "f");
            if (get_var_type(var->name) == VAR)
            {
                if (flag)
                {
                    free_register(reg_var1);
                    set_register(var->name, a);              
                }
                else
                {
                    free_register(a);
                    set_register(var->name, reg_var1);                    
                }
            }
            else
            {
                get_from_memory(var, reg_var2, false);
                if (flag)
                    commands.push_back("STORE " + a  + " " + reg_var2);
                else
                    commands.push_back("STORE " + reg_var1  + " " + reg_var2);
                free_register(a);
                free_register(reg_var1);
            }
            free_register(reg_var2);
        }
        erase_priority_registers(reg);
        erase_priority_registers(reg_var1);
        erase_priority_registers(reg_var2);
    }
}

void do_condition(variable *a, variable *b, cond_type type)
{
    if (a && b)
    {
        string reg_a = get_var_register(a->name);
        string reg_b = get_var_register(b->name);
        priority_registers.push_back(reg_a);
        priority_registers.push_back(reg_b);
        if (type == JEQ || type == JNEQ)
        { 
            if (a->type == VALUE && b->type == VALUE)
            {
                long long int res1 = a->index - b->index;
                if (res1 < 0)
                    res1 = 0;
                long long int res2 = b->index - a->index;
                if (res2 < 0)
                    res2 = 0;             
                create_value_in_register("f", res1 + res2);
                if (flag_repeat)
                {
                    check_free_registers(1);
                    flag_repeat = false;                 
                }
                return;
            }
            else if (a->type != VALUE && b->type != VALUE)
            {
                if (reg_a.compare("") == 0 && reg_b.compare("") == 0)
                {

                    check_free_registers(2);
                    vector<string> f = get_all_free_registers();
                    reg_a = f.at(0);
                    reg_b = f.at(1);
                    get_from_memory(a, reg_a, true);
                    get_from_memory(b, reg_b, true);
                    commands.push_back("RESET f");
                    commands.push_back("ADD f " + reg_a);
                    commands.push_back("SUB f " + reg_b);
                    commands.push_back("SUB " + reg_b + " " + reg_a);
                    commands.push_back("ADD f " + reg_b);                     
                }
                else if (reg_a.compare("") == 0)
                {
                    check_free_registers(1);
                    vector<string> f = get_all_free_registers();
                    reg_a = f.at(0);
                    get_from_memory(a, reg_a, true);
                    commands.push_back("RESET f");
                    commands.push_back("ADD f " + reg_b);
                    commands.push_back("SUB f " + reg_a);
                    commands.push_back("SUB " + reg_a + " " + reg_b);
                    commands.push_back("ADD f " + reg_a);
                }
                else if (reg_b.compare("") == 0)
                {
                    check_free_registers(1);
                    vector<string> f = get_all_free_registers();
                    reg_b = f.at(0);
                    get_from_memory(b, reg_b, true);
                    commands.push_back("RESET f");
                    commands.push_back("ADD f " + reg_a);
                    commands.push_back("SUB f " + reg_b);
                    commands.push_back("SUB " + reg_b + " " + reg_a);
                    commands.push_back("ADD f " + reg_b);                                         
                }
                else 
                {
                    check_free_registers(1);
                    vector<string> f = get_all_free_registers();
                    commands.push_back("RESET f");
                    commands.push_back("ADD f " + reg_a);
                    commands.push_back("SUB f " + reg_b);
                    commands.push_back("RESET " + f.at(0));
                    commands.push_back("ADD " + f.at(0) + " " + reg_b);
                    commands.push_back("SUB " + f.at(0) + " " + reg_a);
                    commands.push_back("ADD f " + f.at(0));
                }
            }
            else if (a->type == VALUE && b->type != VALUE)
            {
                if (reg_b.compare("") == 0)
                {
                    check_free_registers(2);
                    vector<string> f = get_all_free_registers();
                    reg_a = f.at(0);
                    reg_b = f.at(1);                    
                    create_value_in_register(reg_a, a->index);                    
                    get_from_memory(b, reg_b, true);
                }
                else
                {
                    check_free_registers(1);
                    vector<string> f = get_all_free_registers();
                    reg_a = f.at(0);                    
                    create_value_in_register(reg_a, a->index);                   
                }
                commands.push_back("RESET f");                    
                commands.push_back("ADD f " + reg_b);
                commands.push_back("SUB f " + reg_a);
                commands.push_back("SUB " + reg_a + " " + reg_b);                    
                commands.push_back("ADD f " + reg_a);                                        
            }
            else if (a->type != VALUE && b->type == VALUE)
            {
                if (reg_a.compare("") == 0)
                {
                    check_free_registers(2);
                    vector<string> f = get_all_free_registers();
                    reg_a = f.at(0);
                    reg_b = f.at(1);
                    get_from_memory(a, reg_a, true);                                        
                    create_value_in_register(reg_b, b->index);
                }
                else
                {
                    check_free_registers(1);
                    vector<string> f = get_all_free_registers();
                    reg_b = f.at(0);                    
                    create_value_in_register(reg_b, b->index);                   
                }
                commands.push_back("RESET f");
                commands.push_back("ADD f " + reg_a);
                commands.push_back("SUB f " + reg_b);
                commands.push_back("SUB " + reg_b + " " + reg_a);
                commands.push_back("ADD f " + reg_b);                                       
            }
        }
        else if (type == JGE || type == JGEQ)
        {
            if (a->type == VALUE && b->type == VALUE)
            {             
                long long int res1 = a->index - b->index;
                if (res1 < 0)
                    res1 = 0;           
                create_value_in_register("f", res1);
                if (flag_repeat)
                {
                    check_free_registers(1);
                    flag_repeat = false;                 
                }
            }
            else if (a->type == VALUE && b->type != VALUE)
            {
                if (reg_b.compare("") == 0)
                {
                    check_free_registers(1);
                    vector<string> f = get_all_free_registers();
                    reg_b = f.at(0);
                    get_from_memory(b, reg_b, true);                    
                }
                create_value_in_register("f", a->index); 
                commands.push_back("SUB f " + reg_b);
            }
            else if (a->type != VALUE && b->type == VALUE)
            {
                if (reg_a.compare("") == 0)
                {
                    check_free_registers(2);
                    vector<string> f = get_all_free_registers();
                    reg_a = f.at(0);
                    reg_b = f.at(1);
                    get_from_memory(a, reg_a, true);                                       
                }
                else
                {
                    check_free_registers(1);
                    vector<string> f = get_all_free_registers();
                    reg_b = f.at(0);                     
                }
                create_value_in_register(reg_b, b->index);
                commands.push_back("RESET f");
                commands.push_back("ADD f " + reg_a);                
                commands.push_back("SUB f " + reg_b);
            }
            else
            {
                if (reg_a.compare("") == 0 && reg_b.compare("") == 0)
                {
                    check_free_registers(2);
                    vector<string> f = get_all_free_registers();
                    reg_a = f.at(0);
                    reg_b = f.at(1);
                    get_from_memory(a, reg_a, true);
                    get_from_memory(b, reg_b, true);                     
                }
                else if (reg_a.compare("") == 0)
                {
                    check_free_registers(1);
                    vector<string> f = get_all_free_registers();
                    reg_a = f.at(0);
                    get_from_memory(a, reg_a, true); 
                }
                else if (reg_b.compare("") == 0)
                {
                    check_free_registers(1);
                    vector<string> f = get_all_free_registers();
                    reg_b = f.at(0);
                    get_from_memory(b, reg_b, true);                     
                }
                commands.push_back("RESET f");
                commands.push_back("ADD f " + reg_a); 
                commands.push_back("SUB f " + reg_b);
            }    
        }
        erase_priority_registers(reg_a);
        erase_priority_registers(reg_b);       
    }
}

condition *set_condition(variable *a, variable *b, cond_type type)
{
    condition *cond = new condition;
    cond->type = type;
    cond->first = a;
    cond->second = b;
    if (flag_while)
    {
        last_deletions_while.push_back(last_deletions);
        rem_reg.push_back(remember_registers());         
    }
    do_condition(a, b, type);      
    if (!flag_while && !flag_repeat)
        rem_reg.push_back(remember_registers());
    else
    {
        flag_while = false;
        flag_repeat = false;      
    }
    cond->commands.insert(cond->commands.end(), commands.begin(), commands.end());
    commands.clear();
    return cond;
}

variable *num_command(long long int value, int lineno)
{
    variable *current = new variable;
    current->index = value;
    current->type = VALUE;
    return current;
}

variable *id_command(variable *var, int lineno, bool flag)
{
    if (var)
    {
        if (var->type == VARIABLE && !get_symbol(var->name)->is_init && flag)
        {
            error("zmienna " + var->name + " nie została zainicjalizowana", lineno);
            return NULL;
        }
    }
    return var;
}

variable *pid_command(string name, int lineno)
{
    symbol *s = get_symbol(name);
    if (s == 0)
    {
        error("zmienna \033[36m" + name + "\033[0m nie zostala zadeklarowana", lineno);
        return nullptr;
    }
    else if (s->type == ARRAY)
    {
        error("zmienna \033[36m" + name + "\033[0m jest zmienną tablicową", lineno);
        return nullptr;
    }
    else
    {
        variable *current = new variable;
        current->name = name;
        current->index = 1;
        current->type = VARIABLE;
        return current;
    }
}

variable *pid_command(string name, long long int index, int lineno)
{
    symbol *s = get_symbol(name);
    if (s == 0)
    {
        error("zmienna \033[36m" + name + "\033[0m nie zostala zadeklarowana", lineno);
        return nullptr;
    }
    else if (s->type != ARRAY)
    {
        error("zmienna \033[36m" + name + "\033[0m nie jest zmienną tablicową", lineno);
        return nullptr;
    }
    else if (s->type == ARRAY && (index < s->start_index || index >= (s->start_index + s->lenght)))
    {
        error("indeks \033[36m" + to_string(index) + "\033[0m jest poza zakresem tablicy \033[36m" + name + "\033[0m", lineno);
        return nullptr;
    }
    else
    {
        variable *current = new variable;
        current->name = name;
        current->index = index;
        current->type = VARIABLE;
        return current;
    }
}

variable *pid_arr_command(string name, string index_name, int lineno)
{
    symbol *s = get_symbol(name);
    if (s == 0)
    {
        error("zmienna \033[36m" + name + "\033[0m nie zostala zadeklarowana", lineno);
        return nullptr;
    }
    else
    {
        if (!is_symbol(index_name))
        {
            error("zmienna \033[36m" + index_name + "\033[0m nie zostala zadeklarowana", lineno); 
            return nullptr;
        }
        else if (!get_symbol(index_name)->is_init)
        {
            error("zmienna \033[36m" + index_name + "\033[0m nie zostala zainicjalizowana", lineno); 
            return nullptr;                
        }  
        variable *current = new variable;
        current->name = name;
        // gdy jakiś element i talicy tab(10:100) to p[x] + i - 10 
        current->index = s->stored_at;
        current->index_name = index_name;
        current->type = PTR;
        return current;
    }
}

void get_from_memory(variable *var, string regist, bool flag)
{
    if (var) 
    {
        symbol *s = get_symbol(var->name);
        if (var->type == VARIABLE)
        {
            long long int var_index = get_var_index(var);
            create_value_in_register(regist, var_index);
            if (flag)
                commands.push_back("LOAD " + regist + " " + regist);                  
        }
        else if (var->type == PTR)
        {
            create_value_in_register("f", var->index);
            if (get_var_register(var->index_name).compare("") == 0)
            {
                long long int index_ptr = get_var_index(var);
                create_value_in_register(regist, index_ptr);
                commands.push_back("LOAD " + regist + " " + regist);                
            }
            else
            {
                commands.push_back("RESET " + regist);
                commands.push_back("ADD " + regist + " " + get_var_register(var->index_name));
            } 
            commands.push_back("ADD " + regist + " f");
            if (s->start_index != 0)
            {
                create_value_in_register("f", s->start_index);  
                commands.push_back("SUB " + regist + " f");                
            }
            if (flag)
                commands.push_back("LOAD " + regist + " " + regist);
        }
    }
}

void get_from_memory(variable *var, string regist, string regist2, bool flag)
{
    if (var)
    {
        symbol *s = get_symbol(var->name);
        if (var->type == VARIABLE)
        {
            long long int var_index = get_var_index(var);
            create_value_in_register(regist, var_index);
            if (flag)
                commands.push_back("LOAD " + regist + " " + regist);                  
        }
        else if (var->type == PTR)
        {
            create_value_in_register(regist2, var->index);
            if (get_var_register(var->index_name).compare("") == 0)
            {
                long long int index_ptr = get_var_index(var);
                create_value_in_register(regist, index_ptr);
                commands.push_back("LOAD " + regist + " " + regist);                
            }
            else
            {
                commands.push_back("RESET " + regist);
                commands.push_back("ADD " + regist + " " + get_var_register(var->index_name));
            } 
            commands.push_back("ADD " + regist + " " + regist2);
            create_value_in_register(regist2, s->start_index);       
            commands.push_back("SUB " + regist + " " + regist2);
            if (flag)
                commands.push_back("LOAD " + regist + " " + regist);
        }
    }
} 

void create_value_in_register(string regist, long long int value)
{
    commands.push_back("RESET " + regist);
    if (value != 0)
    {
        commands.push_back("INC " + regist);
        if (value != 1)
        {
            string bin = unique_factorization(value);
            for (unsigned long long int i = 1; i < bin.size(); i++)
            {
                if (bin.at(i) == '1')
                {
                    commands.push_back("SHL " + regist);
                    commands.push_back("INC " + regist);
                }
                else
                {
                    commands.push_back("SHL " + regist);
                }
            } 
        }
    }
}

vector<string> get_all_free_registers()
{
    vector<string> free_reg;
    for (int i = 0; i < 5; i++)
        if (bool_registers[i])
            free_reg.push_back(name_registers[i]);
    return free_reg;
}

string get_register(variable *var)
{    
    for(int i = 0; i < 5; i++)
    {
        if (bool_registers[i])
        {
            bool_registers[i] = false;
            registers[i] = var;
            return name_registers[i];                
        }
    }
    return "";
}

void get_register(string name, variable *var)
{
    for(int i = 0; i < 5; i++)
    {
        if (name_registers[i].compare(name) == 0)
        {
            bool_registers[i] = false;
            registers[i] = var;
        }
    }
}

void free_register(string name)
{
    for(int i = 0; i < 5; i++)
        if (name_registers[i].compare(name) == 0)
            bool_registers[i] = true;
}

void erase_priority_registers(string name)
{
    for (unsigned int i = 0; i < priority_registers.size(); i++)
        if (priority_registers.at(i).compare(name) == 0)
            priority_registers.erase(priority_registers.begin() + i);
}

void check_free_registers(int number)
{
    int counter = 0;
    for(int i = 0; i < 5; i++)
        if(bool_registers[i])
            counter++;
    if (number > counter)
    {
        int i = last_deletions;        
        if (i > 4)
            i = 0;
        while (number > counter)
        {
            if(!bool_registers[i])
            {
                bool priority = false;
                for (unsigned int j = 0; j < priority_registers.size(); j++)
                    if (priority_registers.at(j).compare(name_registers[i]) == 0)
                        priority = true;
                if (!priority)
                {
                    symbol *s = get_symbol(registers[i]->name);
                    if (s->stored_at == -1)
                    {
                        s->stored_at = offset;
                        offset += 1;
                    }
                    create_value_in_register("f", s->stored_at);
                    commands.push_back("STORE " + s->regist + " f");
                    s->regist = "";
                    bool_registers[i] = true;
                    number--;
                    if (number == counter)
                        last_deletions = i + 1;                     
                } 
            }
            i++;
            if (i > 4)
                i = 0; 
        }
    }
}

vector<string> remember_registers()
{
    vector<string> remember_regist = {"", "", "", "", ""};
    for (int i = 0; i < 5; i++)
        if (!bool_registers[i])
            remember_regist[i] = registers[i]->name;
    return remember_regist;
}

void set_previous_registers(vector<string> remember_regist)
{
    // najpierw wywalamy te, który nie były wcześniej w rejestrze
    for (int i = 0; i < 5; i++)
    {
        if (!bool_registers[i])
        {
            bool flag = false;
            for (int j = 0; j < 5; j++)
                if (registers[i]->name.compare(remember_regist[j]) == 0)
                    flag = true;
            if (!flag)
            {
                string tmp = "f";
                vector<string> f = get_all_free_registers();
                if (f.size() > 0)
                {
                    tmp = f.at(0);
                }
                symbol *s = get_symbol(registers[i]->name);
                if (s->stored_at == -1)
                {
                    s->stored_at = offset;
                    offset += 1;
                }
                create_value_in_register(tmp, get_var_index(registers[i]));
                string reg = get_var_register(registers[i]->name);
                commands.push_back("STORE " + reg + " " + tmp);
                free_register(reg);
                set_register(registers[i]->name, "");             
            }            
        }
    }
    // teraz przestawiamy jeśli zostały zmienione
    bool flag = true;
    variable *tmp = nullptr;
    while(flag)
    {
        bool changed = false;
        for (int i = 0; i < 5; i++)
        {
            if (!bool_registers[i])
            {
                if (registers[i]->name.compare(remember_regist[i]) != 0)
                {
                    for (int j = 0; j < 5; j++)
                    {
                        if (registers[i]->name.compare(remember_regist[j]) == 0)
                        {
                            if (bool_registers[j])
                            {
                                commands.push_back("RESET " + name_registers[j]);
                                commands.push_back("ADD " + name_registers[j] + " " + get_var_register(registers[i]->name));
                                bool_registers[j] = false;
                                set_register(registers[i]->name, name_registers[j]); 
                                registers[j] = registers[i];
                                changed = true;
                                bool_registers[i] = true; 
                            }
                            else
                            {
                                string reg = get_register(registers[j]);
                                if (reg.compare("") != 0)
                                {
                                    set_register(registers[j]->name, reg);                                 
                                    commands.push_back("RESET " + reg);
                                    commands.push_back("ADD " + reg + " " + name_registers[j]);
                                    commands.push_back("RESET " + name_registers[j]);
                                    commands.push_back("ADD " + name_registers[j] + " " + get_var_register(registers[i]->name));
                                    set_register(registers[i]->name, name_registers[j]);
                                    registers[j] = registers[i];
                                    changed = true;
                                    bool_registers[i] = true; 
                                }                               
                            }                           
                        }
                    }                       
                }
            }
        }
        flag = false;
        for (int i = 0; i < 5; i++)
            if(!bool_registers[i])
                if (registers[i]->name.compare(remember_regist[i]) != 0)
                    flag = true;
        if (flag && !changed)
        {
            for (int i = 0; i < 5; i++)
            {
                if(!bool_registers[i])
                {
                    if (registers[i]->name.compare(remember_regist[i]) != 0)
                    {
                        set_register(registers[i]->name, "f");                                 
                        commands.push_back("RESET f");
                        commands.push_back("ADD f " + name_registers[i]);
                        bool_registers[i] = true;
                        tmp = registers[i];                    
                    }
                }    
            }
        }
        if (tmp)
        {
            for (int i = 0; i < 5; i++)
            {
                if (bool_registers[i] && tmp->name.compare(remember_regist[i]) == 0)
                {
                    set_register(tmp->name, name_registers[i]);                                 
                    commands.push_back("RESET " + name_registers[i]);
                    commands.push_back("ADD " + name_registers[i] + " f");
                    bool_registers[i] = false;
                    registers[i] = tmp;
                    tmp = nullptr;                     
                }
            }
        }
    }
    // teraz dokładamy te zepchnięte do pamięci
    for (int i = 0; i < 5; i++)
    {
        if (bool_registers[i])
        {
            if (remember_regist[i].compare("") != 0)
            {
                symbol *s = get_symbol(remember_regist[i]);
                bool_registers[i] = false;
                create_value_in_register(name_registers[i], s->stored_at);
                commands.push_back("LOAD " + name_registers[i] + " " + name_registers[i]);
                s->regist = name_registers[i];
                variable *var = new variable;
                var->name = remember_regist[i];
                var->index = 1;
                var->type = VARIABLE;
                registers[i] =  var;               
            }       
        }
    }
}

void open_file(char *file_name)
{
    result_file.open(file_name);
}

void pass_to_file(vector<string> *cmds)
{
    //check_jumps(cmds);
    for (unsigned int cmd = 0; cmd < cmds->size(); cmd++)
    {
        result_file << cmds->at(cmd) << endl;
    }
    cmds->clear();
}

void close_file()
{
    result_file.close();
}

// ********************* ARYTMETYKA *********************

string unique_factorization(long long int num)
{
    string bin;
    while (num != 0)
    {
        bin = (num % 2 == 0 ? "0" : "1") + bin;
        num /= 2;
    }
    return bin;
}

void multiplication(string result, string x, string y)
{
    // sprawdzamy, która liczba jest większa
    commands.push_back("RESET " + result);
    commands.push_back("ADD " + result + " " + x);
    commands.push_back("SUB " + result + " " + y);
    commands.push_back("JZERO " + result + " " + to_string(9));
    // x < y
    commands.push_back("RESET " + result);
    commands.push_back("JODD " + y + " " + to_string(2));
    commands.push_back("JUMP " + to_string(2));
    commands.push_back("ADD " + result + " " + x);
    commands.push_back("JZERO " + y + " " + to_string(12));
    commands.push_back("SHL " + x);
    commands.push_back("SHR " + y);
    commands.push_back("JUMP " + to_string(-6));
    // x >= y
    commands.push_back("RESET " + result);
    commands.push_back("JODD " + x + " " + to_string(2));
    commands.push_back("JUMP " + to_string(2));
    commands.push_back("ADD " + result + " " + y);
    commands.push_back("JZERO " + x + " " + to_string(4));
    commands.push_back("SHL " + y);
    commands.push_back("SHR " + x);
    commands.push_back("JUMP " + to_string(-6));
}

// dividend dzielna, reszta
void dividing(string qoutient, string dividend, string divisor, string d, string e, string f)
{
    commands.push_back("RESET " + qoutient);
    commands.push_back("JZERO " + divisor + " " + to_string(28));
    commands.push_back("RESET " + d);
    commands.push_back("ADD " + d + " " + divisor);
    commands.push_back("RESET " + e);
    commands.push_back("INC " + e);
    commands.push_back("RESET " + f);
    commands.push_back("ADD " + f + " " + d);
    commands.push_back("SUB " + f + " " + dividend);
    commands.push_back("JZERO " + f + " " + to_string(2));
    commands.push_back("JUMP 4");
    commands.push_back("SHL " + d);
    commands.push_back("SHL " + e);
    commands.push_back("JUMP -7");
    commands.push_back("RESET " + f);
    commands.push_back("ADD " + f + " " + divisor);
    commands.push_back("SUB " + f + " " + d);
    commands.push_back("JZERO " + f + " " + to_string(2));
    commands.push_back("JUMP 12");
    commands.push_back("RESET " + f);
    commands.push_back("ADD " + f + " " + d);
    commands.push_back("SUB " + f + " " + dividend);
    commands.push_back("JZERO " + f + " " + to_string(2));
    commands.push_back("JUMP 3");
    commands.push_back("ADD " + qoutient + " " + e);
    commands.push_back("SUB " + dividend + " " + d);
    commands.push_back("SHR " + d);
    commands.push_back("SHR " + e);
    commands.push_back("JUMP -14");
    commands.push_back("RESET " + dividend);
}