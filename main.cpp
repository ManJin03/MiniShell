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

        void runCommands();

        void executeCommand();

        [[nodiscard]] bool checkCommand() const;

        void shellCommand();

        void execCommand() const;

        void endCommand();

        void setPrompt(string prompt = "$miniShell>");

        void setPath(string path);

        void printPrompt() const;

        static void occurError(const char *message);

        miniShell() = default;

        ~miniShell() = default;

        string m_path{"/"};
        string m_prompt{};
        string m_input{};
        COMMAND_T m_command{};
        vector<string> m_redirect{};
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
        runCommands();
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


void miniShell::runCommands()
{
    std::stringstream ss(m_input);
    while (true) {
        string token;
        if (ss >> token) {
            if (token == "&&") {
                endCommand();
                executeCommand();
            }
            else if (token == "<" || token == ">") {
                if (string file{}; ss >> file) {
                    m_redirect.push_back(token);
                    m_redirect.push_back(file);
                }
                else {
                    occurError("shell: syntax error near unexpected token `newline'");
                    break;
                }
            }
            else { m_command.push_back(strdup(token.c_str())); }
        }
        else {
            endCommand();
            executeCommand();
            break;
        }
    }
}

void miniShell::shellCommand()
{
    m_exit = true;
}

void miniShell::execCommand() const
{
    if (m_pipe) {}
    int rc = fork();
    if (rc < 0) {
        occurError("fork error");
        return;
    }
    if (rc == 0) {
        execvp(m_command[0], m_command.data());
        occurError("execvp error");
    }
    else {
        if (const int wc = wait(&rc); wc < 0) { occurError("wait error"); }
    }
}

void miniShell::executeCommand()
{
    if (m_command[0] == nullptr) {
        cout << "command miss" << '\n';
        return;
    }
    if (checkCommand()) {
        shellCommand();
    }
    else {
        execCommand();
    }
    m_command.clear();
}

void miniShell::endCommand() { m_command.push_back(nullptr); }

void miniShell::setPath(string path) { m_path = std::move(path); }

void miniShell::setPrompt(string prompt) { m_prompt = std::move(prompt); }

void miniShell::printPrompt() const { cout << m_path << m_prompt << ' '; }

void miniShell::occurError(const char *message) { perror(message); }

bool miniShell::checkCommand() const { return string{m_command[0]} == "exit"; }

int main(int argc, char *argv[])
{
    miniShell &shell = miniShell::init(argc, argv);
    shell.run();
    return 0;
}
