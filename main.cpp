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
    static void executeAST(const ASTNode* node);

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
    }
    Node_ptr root{};
    if (ops.empty()) {
        root = std::move(std::make_unique<ASTNode>(ASTNode{
            ASTNode::Op::Command , std::move(commands[0]) , {}
        }));
    }
    else if (ops[0] == ASTNode::Op::Pipe) {
        auto node1 = std::move(std::make_unique<ASTNode>(ASTNode{
            .op = ASTNode::Op::Command , .command = std::move(commands[0])
        }));
        auto node2 = std::move(std::make_unique<ASTNode>(ASTNode{
            .op = ASTNode::Op::Command , .command = std::move(commands[1])
        }));
        root = std::move(std::make_unique<ASTNode>(ASTNode{
            .op = ASTNode::Op::Pipe
        }));
        root->children.push_back(std::move(node1));
        root->children.push_back(std::move(node2));
    }
    return root;
}

void miniShell::executeAST(const ASTNode* node)
{
    if (node->op == ASTNode::Op::Command) {
        singleCommand(node->command);
    }
    else if (node->op == ASTNode::Op::Pipe) {
        pipeCommand(node);
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
    int pipefd[2];
    pipe(pipefd);
    const int rc1 = fork();
    if (rc1 == 0) {
        close(pipefd[0]); //关闭读端
        if (dup2(pipefd[1] , STDOUT_FILENO) == -1) {
            perror("rc1 dup2 error");
        }
        close(pipefd[1]);
        singleCommand(node->children[0]->command);
        exit(0);
    }
    const int rc2 = fork();
    if (rc2 == 0) {
        close(pipefd[1]); //关闭写端
        if (dup2(pipefd[0] , STDIN_FILENO) == -1) {
            perror("rc2 dup2 error");
        }
        close(pipefd[0]);
        singleCommand(node->children[1]->command);
        exit(0);
    }
    close(pipefd[0]);
    close(pipefd[1]);
    const int wc1 = waitpid(rc1 , nullptr , 0);
    const int wc2 = waitpid(rc2 , nullptr , 0);
    if (wc1 < 0 || wc2 < 0) {
        perror("pipe waitpid error");
    }
    return 0;
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
        int* status{};
        if (const pid_t wc = waitpid(rc , status , 0) ; wc < 0) {
            perror("exec wait error");
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
