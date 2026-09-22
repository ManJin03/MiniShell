//
// Created by 33550 on 2026/9/20.
//

#ifndef MINISHELL_BUILDIN_H
#define MINISHELL_BUILDIN_H
#include "database.h"

namespace miniShell
{
    std::string pwd(bool print);

    int cd(const Command& command);

    void echo(const Command& command);

    void history();
} // miniShell

#endif //MINISHELL_BUILDIN_H
