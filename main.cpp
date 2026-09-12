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

        void analysisToken(COMMAND_T &command, const string &token);

        void endCommand(COMMAND_T &command);

        void setCommand();

        void executeCommands();

        static bool checkCommand(const COMMAND_T &command);

        void shellCommand(const COMMAND_T &command);

        void execCommand(const COMMAND_T &command) const;

        void executeCommand(const COMMAND_T &command);

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
        analysisToken(command, token);
    }
    endCommand(command);
}

void miniShell::executeCommands()
{
    for (auto &command: m_commands) {
        executeCommand(command);
        if (m_exit) { return; }
    }
}


void miniShell::shellCommand(const COMMAND_T &command)
{
    m_exit = true;
}

void miniShell::execCommand(const COMMAND_T &command) const
{
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

void miniShell::executeCommand(const COMMAND_T &command)
{
    if (command[0] == nullptr) {
        cout << "command miss" << '\n';
        return;
    }
    if (checkCommand(command)) {
        shellCommand(command);
    }
    else {
        execCommand(command);
    }
}

void miniShell::setPath(string path) { m_path = std::move(path); }

void miniShell::setPrompt(string prompt) { m_prompt = std::move(prompt); }

void miniShell::printPrompt() const { cout << m_path << m_prompt << ' '; }

void miniShell::occurError(const char *message) { perror(message); }

void miniShell::analysisToken(COMMAND_T &command, const string &token)
{
    if (token == "&&") {
        endCommand(command);
    }
    else { command.push_back(strdup(token.c_str())); }
}

void miniShell::endCommand(COMMAND_T &command)
{
    command.push_back(nullptr);
    m_commands.push_back(command);
    command.clear();
}

bool miniShell::checkCommand(const COMMAND_T &command) { return string{command[0]} == "exit"; }

int main(int argc, char *argv[])
{
    miniShell &shell = miniShell::init(argc, argv);
    shell.run();
    return 0;
}
