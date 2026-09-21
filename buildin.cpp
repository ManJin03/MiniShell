//
// Created by 33550 on 2026/9/20.
//

#include "buildin.h"
#include <iostream>
#include <pwd.h>
#include <unistd.h>

std::string miniShell::pwd(const bool print)
{
    const auto p{getcwd(nullptr , 0)};
    if (print) { std::cout << "pwd> " << p << '\n'; }
    std::string path{p};
    free(p);
    return path;
}

int miniShell::cd(const Command& command)
{
    if (command.argv.size() == 1 || command.argv[1] == "~") {
        const struct passwd* pw = getpwuid(getuid());
        if (pw == nullptr || pw->pw_dir == nullptr) {
            fprintf(stderr , "cannot get home dir\n");
            return 1;
        }
        if (chdir(pw->pw_dir) == -1) {
            perror("chdir");
            return 1;
        }
    }
    else {
        if (chdir(command.argv[1].data()) == -1) {
            perror("chdir error");
            return -1;
        }
    }
    return 0;
}

void miniShell::echo(const Command& command)
{
    auto& argv = command.argv;
    auto it{argv.begin()};
    cout << "echo> ";
    while (++it != argv.end()) {
        cout << *it;
        if (it != argv.end() - 1) cout << " ";
    }
    cout << '\n';
}
