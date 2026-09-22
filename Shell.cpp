//
// Created by 33550 on 2026/9/20.
//

#include "database.h"
#include "buildin.h"
#include "Shell.h"
#include <csignal>
#include <cstring>
#include <sys/wait.h>
#include <fcntl.h>

namespace miniShell
{
    //提示符默认值，变量被 unexport 后回退到该值
    const string defaultPrompt{"$miniShell> "};

    //读取提示符，变量未定义时返回默认值
    static const string& prompt();

    static volatile sig_atomic_t g_interrupted{0};

    static void sigintHandler(int) { g_interrupted = 1; }

    //根据 waitpid 返回的状态计算子进程退出码
    static int exitStatus(int status)
    {
        if (WIFEXITED(status)) { return WEXITSTATUS(status); }
        if (WIFSIGNALED(status)) { return 128 + WTERMSIG(status); }
        return status;
    }

    static Input getLine();

    //将输入拆成一个一个的独立单元，方便解析
    static Tokens tokenize(const Input& line);

    //解析输入单元，填充ASTNode
    static Node_ptr parser(Tokens& tokens);

    //解析单条命令（argv 与重定向），遇到连接运算符停止
    static Command parseCommand(const Tokens& tokens , size_t& pos);

    //解析管道：command ('|' command)*
    static Node_ptr parsePipeline(const Tokens& tokens , size_t& pos);

    //解析 && 与 || ，二者优先级相同且左结合
    static Node_ptr parseAndOr(const Tokens& tokens , size_t& pos);

    //解析顺序执行：expr (';' expr)*
    static Node_ptr parseSequence(const Tokens& tokens , size_t& pos);

    //执行命令
    static int executeAST(const ASTNode* node);

    //执行单条独立命令
    static int singleCommand(const Command& command);

    //执行管道命令
    static int pipeCommand(const ASTNode* node);

    //内建命令fork
    static int shellFork(const Command& command);

    //外部程序fork
    static int execFork(const Command& command);

    //内建命令执行
    static void shellCommand(const Command& command);

    //外部程序命令
    static void execCommand(const Command& command);

    //运行重定向指令
    static void redirectCommand(const Redirections& redirections);

    static int historyCommand(const Command& command);
} // miniShell

const miniShell::string& miniShell::prompt()
{
    if (n_env.contains("prompt")) { return n_env.at("prompt"); }
    return defaultPrompt;
}

miniShell::Shell::Shell()
{
    struct sigaction sa{};
    sa.sa_handler = sigintHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT , &sa , nullptr);
    signal(SIGTTOU , SIG_IGN);
    signal(SIGTTIN , SIG_IGN);
    signal(SIGTSTP , SIG_IGN);
    n_path = pwd(false);
    n_env["prompt"] = defaultPrompt;
}

void miniShell::Shell::run()
{
    while (true) {
        n_path = pwd(false);
        cout << n_path << prompt();
        Input line{getLine()};
        if (line.empty()) { continue; }
        Tokens tokens{tokenize(line)};
        if (tokens.empty()) { continue; }
        Node_ptr root{parser(tokens)};
        if (root) { executeAST(root.get()); }
    }
}

miniShell::Input miniShell::getLine()
{
    Input line;
    if (!getline(cin , line)) {
        if (g_interrupted) {
            g_interrupted = 0;
            cout << "\n";
            cin.clear();
            return {};
        }
        if (cin.eof()) {
            cout << "\n";
            exit(EXIT_SUCCESS);
        }
        return {};
    }
    if (line == "!!") {
        if (n_history.empty()) {
            cout << "bash: syntax error, no command before\n";
            return {};
        }
        auto n = n_history.size() - 1;
        while (n_history[n] == "!!") { --n; }
        line = n_history[n];
        cout << prompt() << line << "\n";
        n_history.emplace_back("!!");
    }
    else { n_history.push_back(line); }
    return line;
}

miniShell::Tokens miniShell::tokenize(const Input& line)
{
    Tokens tokens{};
    Token token{};
    for (auto c{line.begin()} ; c != line.end() ; ++c) {
        if (*c == '|' ||
            *c == '&' ||
            *c == '\'' ||
            *c == '>' ||
            *c == '<' ||
            *c == ';' ||
            *c == '\"') {
            if (tokens.empty() && token.empty()) {
                cout << "bash: syntax error, no command before op\n";
                return {};
            }
            if (!token.empty()) {
                tokens.push_back(token);
                token.clear();
            }
            switch (*c) {
                case'<': {
                    while (++c != line.end() && isspace(*c)) {}
                    if (c == line.end()) {
                        cout << "bash: syntax error, no command after op <\n";
                        return {};
                    }
                    tokens.emplace_back("<");
                    break;
                }
                case';': {
                    while (++c != line.end() && isspace(*c)) {}
                    if (c == line.end()) {
                        cout << "bash: syntax error, no command after op <\n";
                        return {};
                    }
                    tokens.emplace_back(";");
                    break;
                }
                case'>': {
                    Token op{">"};
                    auto t{c};
                    if (++t == line.end()) {
                        cout << "bash: syntax error, no command after op >\n";
                        return {};
                    }
                    if (*t == '>') {
                        op.push_back(*t);
                        ++c;
                    }
                    while (++c != line.end() && isspace(*c)) {}
                    if (c == line.end()) {
                        cout << "bash: syntax error, no command after op >\n";
                        return {};
                    }
                    tokens.push_back(op);
                    break;
                }
                case'|': {
                    Token op{"|"};
                    auto t{c};
                    if (++t == line.end()) {
                        cout << "bash: syntax error, no command after op |\n";
                        return {};
                    }
                    if (*t == '|') {
                        op.push_back(*t);
                        ++c;
                    }
                    while (++c != line.end() && isspace(*c)) {}
                    if (c == line.end()) {
                        cout << "bash: syntax error, no command after op |\n";
                        return {};
                    }
                    tokens.push_back(op);
                    break;
                }
                case'&': {
                    Token op{"&"};
                    auto t{c};
                    if (++t == line.end()) {
                        cout << "bash: syntax error, no command after op &\n";
                        return {};
                    }
                    if (*t == *c) {
                        op.push_back(*t);
                        ++c;
                    }
                    while (++c != line.end() && isspace(*c)) {}
                    if (c == line.end()) {
                        cout << "bash: syntax error, no command after op &\n";
                        return {};
                    }
                    tokens.push_back(op);
                    break;
                }
                case'\'': {
                    while (++c != line.end() && *c != '\'') { token.push_back(*c); }
                    if (c == line.end()) {
                        cout << "bash: syntax error, no end op \'\n";
                        return {};
                    }
                    tokens.push_back(token);
                    token.clear();
                    continue;
                }
                case'\"': {
                    while (++c != line.end() && *c != '\"') { token.push_back(*c); }
                    if (c == line.end()) {
                        cout << "bash: syntax error, no end op \'\n";
                        return {};
                    }
                    tokens.push_back(token);
                    token.clear();
                    continue;
                }
                default:
                    return {};
            }
        }
        if (isspace(*c)) {
            if (!token.empty()) {
                tokens.push_back(token);
                token.clear();
            }
            continue;
        }
        token.push_back(*c);
    }
    if (!token.empty()) {
        tokens.push_back(token);
        token.clear();
    }
    return tokens;
}

miniShell::Command miniShell::parseCommand(const Tokens& tokens , size_t& pos)
{
    Command command{};
    while (pos < tokens.size()) {
        const Token& token{tokens[pos]};
        if (token == "|" || token == "&&" || token == "||" || token == ";" || token == "&") {
            break;
        }
        if (token == "<" || token == ">" || token == ">>") {
            Redirect redirect{};
            if (token == "<") {
                redirect.mode = Redirect::Mode_t::input;
            }
            else if (token == ">") {
                redirect.mode = Redirect::Mode_t::output;
            }
            else {
                redirect.mode = Redirect::Mode_t::append;
            }
            if (pos + 1 >= tokens.size()) { break; } //缺少重定向文件名
            redirect.filename = tokens[++pos];
            command.redirections.push_back(std::move(redirect));
            ++pos;
            continue;
        }
        command.argv.push_back(token);
        ++pos;
    }
    return command;
}

miniShell::Node_ptr miniShell::parsePipeline(const Tokens& tokens , size_t& pos)
{
    vector<Command> commands{};
    commands.push_back(parseCommand(tokens , pos));
    while (pos < tokens.size() && tokens[pos] == "|") {
        ++pos;
        commands.push_back(parseCommand(tokens , pos));
    }
    if (commands.size() == 1) {
        return std::make_unique<ASTNode>(ASTNode{
            .op = ASTNode::Op::Command , .command = std::move(commands[0])
        });
    } //单命令
    auto node{std::make_unique<ASTNode>(ASTNode{.op = ASTNode::Op::Pipe})};
    for (auto& it : commands) {
        node->children.push_back(std::make_unique<ASTNode>(ASTNode{
            .op = ASTNode::Op::Command , .command = std::move(it)
        }));
    } //管道命令
    return node;
}

miniShell::Node_ptr miniShell::parseAndOr(const Tokens& tokens , size_t& pos)
{
    Node_ptr left{parsePipeline(tokens , pos)};
    while (pos < tokens.size() && (tokens[pos] == "&&" || tokens[pos] == "||")) {
        const ASTNode::Op op{tokens[pos] == "&&" ? ASTNode::Op::And : ASTNode::Op::Or};
        ++pos;
        Node_ptr right{parsePipeline(tokens , pos)};
        auto node{std::make_unique<ASTNode>(ASTNode{.op = op})};
        node->children.push_back(std::move(left));
        node->children.push_back(std::move(right));
        left = std::move(node);
    } //左结合，形成二叉树
    return left;
}

miniShell::Node_ptr miniShell::parseSequence(const Tokens& tokens , size_t& pos)
{
    Node_ptr first{parseAndOr(tokens , pos)};
    while (pos < tokens.size() && (tokens[pos] == ";" || tokens[pos] == "&")) {
        ++pos;
        Node_ptr next{parseAndOr(tokens , pos)};
        if (first->op != ASTNode::Op::Sequence) {
            auto sequence{std::make_unique<ASTNode>(ASTNode{.op = ASTNode::Op::Sequence})};
            sequence->children.push_back(std::move(first));
            first = std::move(sequence);
        }
        first->children.push_back(std::move(next));
    } //展开为序列节点
    return first;
}

miniShell::Node_ptr miniShell::parser(Tokens& tokens)
{
    for (auto& token : tokens) {
        if (!token.empty() && token[0] == '$') {
            if (string key{token.substr(1)} ; n_env.contains(key)) {
                token = n_env.at(key);
            }
        }
    }
    size_t pos{0};
    return parseSequence(tokens , pos);
}

int miniShell::executeAST(const ASTNode* node)
{
    switch (node->op) {
        case ASTNode::Op::Command:
            return singleCommand(node->command);
        case ASTNode::Op::Pipe:
            return pipeCommand(node);
        case ASTNode::Op::And: {
            const int status{executeAST(node->children[0].get())};
            if (status == 0) {
                return executeAST(node->children[1].get());
            } //左侧成功才执行右侧
            return status;
        }
        case ASTNode::Op::Or: {
            const int status{executeAST(node->children[0].get())};
            if (status != 0) {
                return executeAST(node->children[1].get());
            } //左侧失败才执行右侧
            return status;
        }
        case ASTNode::Op::Sequence: {
            int status{0};
            for (auto& it : node->children) {
                status = executeAST(it.get());
            }
            return status;
        }
        default:
            return 0;
    }
}

int miniShell::singleCommand(const Command& command)
{
    if (command.argv.empty()) {
        return 0;
    }
    if (command.argv[0] == "exit" ||
        command.argv[0] == "pwd" ||
        command.argv[0] == "cd" ||
        command.argv[0] == "echo" ||
        command.argv[0] == "export" ||
        command.argv[0] == "unexport" ||
        command.argv[0] == "env" ||
        command.argv[0] == "history" ||
        command.argv[0] == "!") {
        return shellFork(command);
    }
    return execFork(command);
}

int miniShell::pipeCommand(const ASTNode* node)
{
    const int n = static_cast<int>(node->children.size());
    int pre_read{-1}; //-1表示没有读端
    vector<pid_t> pids{};
    pid_t pgid = -1;
    for (int i = 0 ; i < n ; ++i) {
        int pipefd[2]{-1 , -1};
        if (i < n - 1) {
            if (pipe(pipefd) == -1) {
                perror("pipe error");
                break;
            }
        }
        const pid_t pid = fork();
        if (pid < 0) {
            perror("fork error");
            if (pipefd[0] != -1) { close(pipefd[0]); }
            if (pipefd[1] != -1) { close(pipefd[1]); }
            break;
        }
        if (pid == 0) {
            if (pgid == -1) { pgid = getpid(); }
            setpgid(0 , pgid);
            signal(SIGINT , SIG_DFL);
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
            execCommand(node->children[i].get()->command);
            _exit(127);
        }
        if (pgid == -1) { pgid = pid; }
        setpgid(pid , pgid);
        if (pre_read != -1) {
            close(pre_read);
            pre_read = -1;
        } //关闭原来的读端
        if (i < n - 1) {
            close(pipefd[1]);
            pre_read = pipefd[0]; //保存新的读端
        }
        pids.emplace_back(pid);
    }
    if (pre_read != -1) {
        close(pre_read);
        pre_read = -1;
    }
    if (isatty(STDIN_FILENO)) {
        if (tcsetpgrp(STDIN_FILENO , pgid) == -1) {
            perror("tcsetpgrp");
        }
    } //设置整个管道为前台进程组
    int status{};
    for (auto pid : pids) {
        while (waitpid(pid , &status , 0) == -1) {
            if (errno != EINTR) {
                perror("waitpid error");
                break;
            }
        }
    }
    if (isatty(STDIN_FILENO)) {
        tcsetpgrp(STDIN_FILENO , getpgrp());
    }
    if (WIFSIGNALED(status) && WTERMSIG(status) == SIGINT) {
        write(STDOUT_FILENO , "\n" , 1);
    }
    if (pids.size() != static_cast<size_t>(n)) {
        return -1;
    }
    return exitStatus(status); //管道以最后一个命令的退出码为准
}

int miniShell::shellFork(const Command& command)
{
    if (command.argv[0] == "exit") {
        exit(0);
    }
    if (command.argv[0] == "cd") {
        const int st{cd(command)};
        n_path = pwd(false);
        return st; //cd 需在父进程中执行才能改变 shell 的工作目录
    }
    if (command.argv[0] == "export") {
        if (command.argv.size() < 3) {
            std::cerr << "export: usage: export <name> <value>\n";
            return -1;
        }
        n_env[command.argv[1]] = command.argv[2];
        return 0;
    }
    if (command.argv[0] == "unexport") {
        if (command.argv.size() < 2) {
            std::cerr << "unexport: usage: unexport <name>\n";
            return -1;
        }
        if (n_env.contains(command.argv[1])) {
            n_env.erase(n_env.find(command.argv[1]));
            return 0;
        }
        std::cerr << "unexport: " << command.argv[1] << ": not defined\n";
        return -1;
    }
    if (command.argv[0] == "env") {
        for (const auto& i : n_env) {
            cout << i.first << ":" << i.second << '\n';
        }
        return 0;
    }
    const pid_t pid = fork();
    if (pid < 0) {
        perror("fork error");
        return -1;
    }
    if (pid == 0) {
        setpgid(0 , getpid());
        signal(SIGINT , SIG_DFL);
        shellCommand(command);
        cout.flush(); //_exit 不会刷新 stdio 缓冲区，需手动刷新
        _exit(EXIT_SUCCESS); //内建命令执行完即正常退出
    }
    setpgid(pid , pid);
    if (isatty(STDIN_FILENO)) {
        if (tcsetpgrp(STDIN_FILENO , pid) == -1) {
            perror("tcsetpgrp");
        }
    }
    int status{};
    while (waitpid(pid , &status , 0) == -1) {
        if (errno != EINTR) {
            perror("waitpid error");
            break;
        }
    }
    if (isatty(STDIN_FILENO)) {
        tcsetpgrp(STDIN_FILENO , getpgrp());
    }
    if (WIFSIGNALED(status) && WTERMSIG(status) == SIGINT) {
        write(STDOUT_FILENO , "\n" , 1);
    }
    return exitStatus(status);
}

int miniShell::execFork(const Command& command)
{
    const pid_t pid = fork();
    if (pid < 0) {
        perror("fork error");
        return -1;
    }
    if (pid == 0) {
        setpgid(0 , getpid());
        signal(SIGINT , SIG_DFL);
        execCommand(command);
        _exit(127);
    }
    setpgid(pid , pid);
    if (isatty(STDIN_FILENO)) {
        if (tcsetpgrp(STDIN_FILENO , pid) == -1) {
            perror("tcsetpgrp");
        }
    }
    int status{};
    while (waitpid(pid , &status , 0) == -1) {
        if (errno != EINTR) {
            perror("waitpid error");
            break;
        }
    }
    if (isatty(STDIN_FILENO)) {
        tcsetpgrp(STDIN_FILENO , getpgrp());
    }
    if (WIFSIGNALED(status) && WTERMSIG(status) == SIGINT) {
        write(STDOUT_FILENO , "\n" , 1);
    }
    return exitStatus(status);
}

void miniShell::shellCommand(const Command& command)
{
    redirectCommand(command.redirections);
    if (command.argv[0] == "pwd") {
        pwd(true);
    }
    else if (command.argv[0] == "echo") {
        echo(command);
    }
    else if (command.argv[0] == "history") {
        history();
    }
    else if (command.argv[0] == "!") {
        if (command.argv.size() < 2) {
            std::cerr << "echo: usage: ! <name>\n";
        }
        else {
            historyCommand(command);
        }
    }
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

int miniShell::historyCommand(const Command& command)
{
    const unsigned int n = std::stoi(command.argv[1]);
    if (n > n_history.size()) {
        std::cerr << "history size error\n";
        return -1;
    }
    const Input& line = n_history[n];
    Tokens tokens{tokenize(line)};
    if (tokens.empty()) { return 0; }
    const Node_ptr root{parser(tokens)};
    if (root) { executeAST(root.get()); }
    return 0;
}
