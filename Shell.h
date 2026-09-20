//
// Created by 33550 on 2026/9/20.
//

#ifndef MINISHELL_SHELL_H
#define MINISHELL_SHELL_H
#include <string>

namespace miniShell
{
    class Shell
    {
    public:
        void run();

        Shell();

    private:
        std::string m_prompt{" $miniShell> "};
        std::string m_path{};
    };
} // miniShell

#endif //MINISHELL_SHELL_H
