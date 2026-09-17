//
// Created by 33550 on 2026/8/31.
//
#include <iostream>
#include <memory>
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

    //解析输入单元，填充ASTNode
    static Node_ptr parser(const Tokens& tokens);

    //执行命令
    static int executeAST(const ASTNode* node);

    //执行单条独立命令
    static int singleCommand(const Command& command);

    //执行管道命令
    static int pipeCommand(const ASTNode* node);

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
        if (tokens.empty()) { continue; }
        Node_ptr root{parser(tokens)};
        if (root) { executeAST(root.get()); }
    }
}

miniShell::Tokens miniShell::tokenize(const Input& line)
{
    //TODO:解析输入字符串，解析出带空格的单引号双引号参数，运算符前后必须有命令，重定向后必须有文件名
    Tokens tokens{};
    std::stringstream ss(line);
    Token token;
    while (ss >> token) {
        tokens.push_back(token);
    }
    return tokens;
}

miniShell::Node_ptr miniShell::parser(const Tokens& tokens)
{
    vector<Command> commands{};
    vector<ASTNode::Op> ops{};
    for (auto it = tokens.begin() ; it != tokens.end() ;) {
        Command command{};
        for (; it != tokens.end() ; ++it) {
            if (*it == "|" || *it == "&&" || *it == "||" || *it == ";") {
                if (*it == "|") {
                    ops.push_back(ASTNode::Op::Pipe);
                }
                else if (*it == "&&") {
                    ops.push_back(ASTNode::Op::And);
                }
                else if (*it == "||") {
                    ops.push_back(ASTNode::Op::Or);
                }
                else if (*it == ";") {
                    ops.push_back(ASTNode::Op::Sequence);
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
        commands.emplace_back(std::move(command));
    } //解析命令与运算符
    Node_ptr root{};
    if (commands.empty()) {
        return root;
    }
    if (ops.empty()) {
        root = std::move(std::make_unique<ASTNode>(ASTNode{
            ASTNode::Op::Command , std::move(commands[0]) , {}
        }));
    } //单命令
    else if (ops[0] == ASTNode::Op::Pipe) {
        root = std::move(std::make_unique<ASTNode>(ASTNode{
            .op = ASTNode::Op::Pipe
        }));
        for (auto& it : commands) {
            auto node = std::move(std::make_unique<ASTNode>(ASTNode{
                .op = ASTNode::Op::Command , .command = std::move(it)
            }));
            root->children.push_back(std::move(node));
        }
    } //管道命令
    //TODO：解析其他命令运算符
    return root;
}

int miniShell::executeAST(const ASTNode* node)
{
    //TODO：递归调用以实现命令运算操作
    switch (node->op) {
        case ASTNode::Op::Command:
            return singleCommand(node->command);
        case ASTNode::Op::Pipe:
            return pipeCommand(node);
        default:
            return 0;
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

int miniShell::pipeCommand(const ASTNode* node)
{
    const int n = static_cast<int>(node->children.size());
    int pre_read{-1}; //-1表示没有读端
    vector<pid_t> pids{};
    for (int i = 0 ; i < n ; ++i) {
        int pipefd[2]{-1 , -1};
        if (i < n - 1) {
            if (pipe(pipefd) == -1) {
                perror("pipe error");
                break;
            }
        } //i<n-1才会创建pipe
        const int rc = fork();
        if (rc < 0) {
            perror("fork error");
            if (pipefd[0] != -1) { close(pipefd[0]); }
            if (pipefd[1] != -1) { close(pipefd[1]); }
            break;
        }
        if (rc == 0) {
            if (pre_read != -1) {
                if (dup2(pre_read , STDIN_FILENO) == -1) {
                    perror("rc stdin dup2 error");
                    _exit(EXIT_FAILURE);
                }
                close(pre_read);
            }
            if (i < n - 1) {
                close(pipefd[0]);
                if (dup2(pipefd[1] , STDOUT_FILENO) == -1) {
                    perror("rc stdout dup2 error");
                    _exit(EXIT_FAILURE);
                }
                close(pipefd[1]); //新管道写入}
            }
            executeAST(node->children[i].get());
            _exit(EXIT_SUCCESS);
        }
        if (pre_read != -1) {
            close(pre_read);
            pre_read = -1;
        } //关闭原来的读端
        if (i < n - 1) {
            close(pipefd[1]);
            pre_read = pipefd[0]; //保存新的读端
        }
        pids.emplace_back(rc);
    }
    if (pre_read != -1) {
        close(pre_read);
        pre_read = -1;
    }
    for (auto pid : pids) {
        int status{};
        while (waitpid(pid , &status , 0) == -1) {
            if (errno != EINTR) {
                perror("waitpid error");
                break;
            }
        }
    }
    return pids.size() == n ? 0 : -1;
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
        perror("exec fork error");
        return -1;
    }
    if (rc == 0) {
        execCommand(command);
        return -1;
    }
    else {
        int status{};
        while (waitpid(rc , &status , 0) == -1) {
            if (errno != EINTR) {
                perror("waitpid error");
                break;
            }
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
        switch (mode) {
            case Redirect::Mode_t::input:
                oflags = O_RDONLY;
                fd2 = STDIN_FILENO;
                break;
            case Redirect::Mode_t::output:
                oflags = O_WRONLY | O_CREAT | O_TRUNC;
                fd2 = STDOUT_FILENO;
                break;
            case Redirect::Mode_t::append:
                oflags = O_WRONLY | O_CREAT | O_APPEND;
                fd2 = STDOUT_FILENO;
                break;
            default:
                return;
        }
        if (const int fd = open(filename.data() , oflags , S_IRWXU) ; fd == -1) {
            perror("direct open error");
            _exit(EXIT_FAILURE);
        }
        else {
            if (dup2(fd , fd2) == -1) {
                perror("direct dup2 error");
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
