//
// Created by 33550 on 2026/8/31.
//
#include <iostream>
#include <sstream>
#include <string>
#include <unistd.h>
#include <vector>
#include <sys/wait.h>
#include <cstring>

using std::cout;
using std::cin;
using std::string;
using std::vector;
using COMMAND_T = vector<char *>;

namespace
{
    class miniShell
    {
    public:
        static miniShell &init(int argc, char *argv[]);

        void run();

        miniShell(const miniShell &) = delete;

        miniShell &operator=(const miniShell &) = delete;

    private:
        void getInput();

        void setCommand();

        void executeCommands() const;

        void executeCommand(const COMMAND_T &command) const;

        void setPrompt(string prompt = "$miniShell>");

        void setPath(string path);

        void printPrompt() const;

        static void occurError(const char *message);

        miniShell() = default;

        ~miniShell() = default;

        string m_path{"/"};
        string m_prompt{};
        string m_input{};
        vector<COMMAND_T> m_commands{};
        bool m_redirect{false};
        bool m_pipe{false};
        bool m_exit{false};
    };
}

miniShell &miniShell::init(int argc, char *argv[])
{
    static miniShell shell{};
    shell.setPrompt();
    shell.setPath(argv[0]);
    return shell;
}

void miniShell::run()
{
    while (true) {
        getInput();
        setCommand();
        executeCommands();
        if (m_exit) {
            break;
        }
    }
}

void miniShell::getInput()
{
    printPrompt();
    getline(cin, m_input);
}

void miniShell::setCommand()
{
    m_commands.clear();
    std::stringstream ss(m_input);
    COMMAND_T command;
    string token;
    while (ss >> token) {
        command.push_back(strdup(token.c_str()));
    }
    command.push_back(nullptr);
    m_commands.push_back(command);
}

void miniShell::executeCommands() const
{
    for (auto &command: m_commands) {
        if (m_exit) { return; }
        executeCommand(command);
    }
}

void miniShell::executeCommand(const COMMAND_T &command) const
{
    if (m_redirect) {}
    if (m_pipe) {}
    int rc = fork();
    if (rc < 0) { occurError("fork error"); }
    if (rc == 0) {
        execvp(command[0], command.data());
        occurError("execvp error");
    }
    else {
        if (const int wc = wait(&rc); wc < 0) { occurError("wait error"); }
    }
}

void miniShell::setPath(string path) { m_path = std::move(path); }

void miniShell::setPrompt(string prompt) { m_prompt = std::move(prompt); }

void miniShell::printPrompt() const { cout << m_path << m_prompt << ' '; }

void miniShell::occurError(const char *message)
{
    perror(message);
    exit(-1);
}

int main(int argc, char *argv[])
{
    miniShell &shell = miniShell::init(argc, argv);
    shell.run();
    return 0;
}
