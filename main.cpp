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
        enum class Op
        {
            _non ,
            _and ,
            _or ,
            _pipe ,
        };

        Progress argv{};
        Redirections redirections{};
        Op op{};
    };

    using Commands = vector<Command>;

    class Shell
    {
    public:
        //初始化一个单例shell
        static Shell& init(string path);

        //循环运行
        void run() const;

        Shell(const Shell&) = delete;

        Shell& operator=(const Shell&) = delete;

    private:
        Shell() = default;

        ~Shell() = default;

        string m_prompt{"$miniShell> "};
        string m_path{};
    };

    //将输入拆成一个一个的独立单元，方便解析
    static Tokens tokenize(const Input& line);

    //解析输入单元，填充命令
    static Commands parser(const Tokens& tokens);

    //执行命令
    static void executeCommands(const Commands& commands);

    //执行单条独立命令
    static int singleCommand(const Command& command);

    //内建shell命令
    static int shellFork(const Command& command);

    //外部程序fork
    static int execFork(const Command& command);

    //外部程序命令
    static void execCommand(const Command& command);

    //运行重定向指令
    static void redirectCommand(const Redirections& redirections);
}

miniShell::Shell& miniShell::Shell::init(string path)
{
    static miniShell::Shell shell{};
    shell.m_path = std::move(path);
    return shell;
}

void miniShell::Shell::run() const
{
    while (true) {
        cout << m_path << m_prompt;
        Input line;
        getline(cin , line);
        Tokens tokens{tokenize(line)};
        Commands commands{parser(tokens)};
        executeCommands(commands);
    }
}

miniShell::Tokens miniShell::tokenize(const Input& line)
{
    Tokens tokens{};
    std::stringstream ss(line);
    Token token;
    while (ss >> token) {
        tokens.push_back(token);
    }
    return tokens;
}

miniShell::Commands miniShell::parser(const Tokens& tokens)
{
    Commands commands{};
    for (auto it = tokens.begin() ; it != tokens.end() ;) {
        Command command{};
        for (; it != tokens.end() ; ++it) {
            if (*it == ";" || *it == "&&" || *it == "||" || *it == "|") {
                if (*it == ";") {
                    command.op = Command::Op::_non;
                }
                else if (*it == "&&") {
                    command.op = Command::Op::_and;
                }
                else if (*it == "||") {
                    command.op = Command::Op::_or;
                }
                else if (*it == "|") {
                    command.op = Command::Op::_pipe;
                }
                ++it;
                break;
            }
            if (*it == "<" || *it == ">" || *it == ">>") {
                Redirect redirect{};
                if (*it == "<") {
                    redirect.mode = Redirect::Mode_t::input;
                    redirect.filename = *(++it);
                }
                else if (*it == ">") {
                    redirect.mode = Redirect::Mode_t::output;
                    redirect.filename = *(++it);
                }
                else if (*it == ">>") {
                    redirect.mode = Redirect::Mode_t::append;
                    redirect.filename = *(++it);
                }
                command.redirections.push_back(std::move(redirect));
                continue;
            }
            command.argv.push_back(*it);
        }
        commands.push_back(std::move(command));
    }
    return commands;
}

void miniShell::executeCommands(const Commands& commands)
{
    for (auto& it : commands) {
        if (it.op == Command::Op::_non) {
            singleCommand(it);
        }
    }
}

int miniShell::singleCommand(const Command& command)
{
    if (command.argv.empty()) {
        return 0;
    }
    if (command.argv[0] == "exit") {
        return shellFork(command);
    }
    return execFork(command);
}

int miniShell::shellFork(const Command& command)
{
    if (command.argv[0] == "exit") {
        exit(0);
    }
    return 0;
}

int miniShell::execFork(const Command& command)
{
    const pid_t rc = fork();
    if (rc < 0) {
        perror("fork error");
        return -1;
    }
    if (rc == 0) {
        execCommand(command);
        return -1;
    }
    else {
        int* status{};
        if (const pid_t wc = waitpid(rc , status , 0) ; wc < 0) {
            perror("wait error");
            return *status;
        }
    }
    return 0;
}

void miniShell::execCommand(const Command& command)
{
    redirectCommand(command.redirections);
    vector<char*> argv{};
    argv.reserve(command.argv.size());
    for (auto& it : command.argv) {
        argv.push_back(strdup(it.c_str()));
    }
    argv.push_back(nullptr);
    execvp(argv[0] , argv.data());
    perror("execvp error");
    for (const auto& it : argv) {
        free(it);
    }
    exit(-1);
}

void miniShell::redirectCommand(const Redirections& redirections)
{
    for (const auto& [mode, filename] : redirections) {
        int oflags{};
        int fd2{};
        if (mode == Redirect::Mode_t::input) {
            oflags = O_RDONLY;
            fd2 = STDIN_FILENO;
        }
        if (mode == Redirect::Mode_t::output) {
            oflags = O_WRONLY | O_CREAT | O_TRUNC;
            fd2 = STDOUT_FILENO;
        }
        if (mode == Redirect::Mode_t::append) {
            oflags = O_WRONLY | O_CREAT | O_APPEND;
            fd2 = STDOUT_FILENO;
        }

        if (const int fd = open(filename.data() , oflags , S_IRWXU) ; fd == -1) {
            perror("open error");
            _exit(EXIT_FAILURE);
        }
        else {
            if (dup2(fd , fd2) == -1) {
                perror("dup2 error");
                _exit(EXIT_FAILURE);
            }
            if (fd != fd2) {
                close(fd);
            }
        }
    }
}

int main(int argc , char* argv[])
{
    const miniShell::Shell& shell = miniShell::Shell::init(argv[0]);
    shell.run();
    return 0;
}
