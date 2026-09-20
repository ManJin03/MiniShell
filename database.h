//
// Created by 33550 on 2026/9/20.
//

#ifndef MINISHELL_DATABASE_H
#define MINISHELL_DATABASE_H
#include <iostream>
#include <memory>
#include <vector>

namespace miniShell
{
    using std::cout;
    using std::cin;
    using std::string;
    using std::vector;
    using Token = string;
    using Tokens = vector<Token>;
    using Progress = vector<Token>;
    using Input = string;
    using Filename = string;

    struct Redirect
    {
        enum class Mode_t
        {
            input ,
            output ,
            append ,
        };

        Mode_t mode{};
        Filename filename{};
    };

    using Redirections = vector<Redirect>;

    struct Command
    {
        Progress argv{};
        Redirections redirections{};
    };

    struct ASTNode
    {
        enum class Op
        {
            Command ,
            Pipe ,
            And ,
            Or ,
            Sequence ,
        };

        Op op{};
        Command command{};
        vector<std::unique_ptr<ASTNode>> children{};
    };

    using Node_ptr = std::unique_ptr<ASTNode>;
}
#endif //MINISHELL_DATABASE_H
