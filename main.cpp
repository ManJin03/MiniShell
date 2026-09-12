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
#include <fcntl.h>

using std::cout;
using std::cin;
using std::string;
using std::vector;
using TOKEN_t = string;
using TOKENS_T = vector<TOKEN_t>;
using INPUT_T = string;

namespace
{
    struct Command
    {
        TOKENS_T argv{};
    };

    class miniShell
    {
    public:
        //初始化一个单例shell
        static miniShell &init(string path);

        //循环运行
        void run();

        miniShell(const miniShell &) = delete;

        miniShell &operator=(const miniShell &) = delete;

    private:
        //将输入拆成一个一个的独立单元，方便解析
        static TOKENS_T tokenize(const INPUT_T &line);

        //解析输入单元，填充命令
        void parser(const TOKENS_T &tokens);

        //执行命令
        void executeCommands();

        //检查是否为内置shell命令
        static int runCommand(const Command &command);

        //内建shell命令
        static int shellCommand(const Command &command);

        //外部程序命令
        static int execCommand(const Command &command);

        miniShell() = default;

        ~miniShell() = default;

        string m_prompt{"$miniShell> "};
        string m_path{};
        vector<Command> m_commands{};
    };
}

miniShell &miniShell::init(string path)
{
    static miniShell shell{};
    shell.m_path = std::move(path);
    return shell;
}

void miniShell::run()
{
    while (true) {
        cout << m_path << m_prompt;
        INPUT_T line;
        getline(cin, line);
        parser(tokenize(line));
        executeCommands();
    }
}

TOKENS_T miniShell::tokenize(const INPUT_T &line)
{
    TOKENS_T tokens{};
    std::stringstream ss(line);
    TOKEN_t token;
    while (ss >> token) {
        tokens.push_back(token);
    }
    return tokens;
}

void miniShell::parser(const TOKENS_T &tokens)
{
    Command command{};
    command.argv = tokens;
    m_commands.push_back(std::move(command));
}

void miniShell::executeCommands()
{
    if (m_commands.empty()) {
        return;
    }
    for (auto &it: m_commands) {
        runCommand(it);
    }
    m_commands.clear();
}

int miniShell::runCommand(const Command &command)
{
    if (command.argv[0] == "exit") {
        return shellCommand(command);
    }
    else {
        return execCommand(command);
    }
}

int miniShell::shellCommand(const Command &command)
{
    return 0;
}

int miniShell::execCommand(const Command &command)
{
    const pid_t rc = fork();
    if (rc < 0) {
        perror("fork error");
        return -1;
    }
    if (rc == 0) {
        vector<char *> argv{};
        argv.reserve(command.argv.size());
        for (auto &it: command.argv) {
            argv.push_back(strdup(it.c_str()));
        }
        argv.push_back(nullptr);
        execvp(argv[0], argv.data());
        perror("execvp error");
        for (const auto &it: argv) {
            free(it);
        }
        exit(-1);
    }
    else {
        int *status{};
        if (const pid_t wc = waitpid(rc, status, 0); wc < 0) {
            perror("wait error");
            return *status;
        }
    }
    return 0;
}

int main(int argc, char *argv[])
{
    miniShell &shell = miniShell::init(argv[0]);
    shell.run();
    return 0;
}
