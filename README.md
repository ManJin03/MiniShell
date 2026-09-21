# MiniShell

一个使用 C++20 编写的 Unix/Linux 命令行 shell 实验项目。项目用于练习进程创建、程序替换、管道、文件描述符重定向、作业控制以及父子进程同步。

## 功能概览

当前版本支持以下功能：

- 执行外部程序，并通过 `execvp` 使用系统的 `PATH` 查找程序。
- 内置命令：`cd`、`pwd`、`echo`、`exit`。
- `cd` 不指定参数或使用 `~` 时切换到用户主目录。
- 输入重定向：`<`
- 输出重定向并覆盖文件：`>`
- 输出重定向并追加文件：`>>`
- 单管道和多管道：`command1 | command2 | command3`
- 命令连接：`;` 顺序执行，`&&`、`||` 依据左侧命令的退出码短路执行。
- 通过 `fork`、`pipe`、`dup2` 和 `waitpid` 管理进程与文件描述符。
- 使用进程组（`setpgid`）与终端前台进程组（`tcsetpgrp`）进行作业控制。
- 处理 `SIGINT`（Ctrl-C）中断，屏蔽 shell 自身的 `SIGTTOU`、`SIGTTIN`、`SIGTSTP`。
- 输入支持单引号包裹的、带空格的参数。
- 管道和重定向运算符不必与其他内容用空格分开，例如使用 `cat<input.txt`。

## 构建与运行

项目不依赖第三方库，可直接使用支持 C++20 的编译器构建。

### 使用 CMake（推荐）

```bash
cmake -B build
cmake --build build
./build/MiniShell
```

### 直接使用 g++

```bash
g++ -std=c++20 -Wall -Wextra -pedantic main.cpp Shell.cpp buildin.cpp -o minishell
./minishell
```

启动后会看到包含当前工作目录的提示符：

```text
/home/user/projects $miniShell>
```

输入 `exit` 或按下 Ctrl-D 即可退出。

## 使用示例

```text
ls
cat input.txt
cat < input.txt
echo hello > output.txt
echo hello >> output.txt
cat input.txt | grep hello
cat input.txt | grep hello | wc -l
pwd ; cd /tmp ; pwd
cd /tmp && pwd
cd /not/exist || echo "cd failed"
make && ./a.out
```

`&&` 仅在左侧命令退出码为 `0` 时执行右侧命令，`||` 仅在左侧退出码非 `0` 时执行右侧命令，二者优先级相同且左结合，高于 `;`、低于 `|`。

重定向可以出现在命令参数之后；同一命令多次使用同类重定向时，后一次重定向会覆盖前一次设置，例如：

```text
echo hello > first.txt > second.txt
```

> `pwd` 会以 `pwd> <路径>` 的形式输出，`echo` 会以 `echo> ` 作为前缀输出内容。

## 项目结构

```text
.
├── main.cpp       # 程序入口，创建并启动 Shell
├── Shell.h        # Shell 类声明
├── Shell.cpp      # 词法分析、语法解析、执行与进程管理
├── buildin.h      # 内置命令声明
├── buildin.cpp    # 内置命令（pwd / cd / echo）实现
├── database.h     # 公共数据结构（Token、Command、ASTNode 等）
├── CMakeLists.txt # CMake 构建配置
└── README.md      # 项目说明
```

执行流程：`getLine` 读取输入 → `tokenize` 拆分为独立单元 → `parser` 构建 AST → `executeAST` 分派到单命令、管道、`&&`/`||` 或顺序执行。解析器按优先级递归下降：`;`（最低）→ `&&`/`||`（左结合）→ `|`（最高）。

## 当前限制

- 后台运行符 `&` 已在词法分析中识别，但尚未实现对应的后台执行逻辑，目前按顺序执行处理。
- 尚未支持环境变量展开、通配符展开。
- 错误信息由系统调用通过标准错误输出，具体文本可能因操作系统和运行环境而不同。

## 计划实现

| 计划实现                                        | 已实现   |
|-------------------------------------------------|----------|
| 整理代码至多个文件中                            | 已实现   |
| 增加 `cd`、`pwd`、`echo` 等内置命令。           | 已实现   |
| 完善单引号和双引号参数解析。                    | 已实现   |
| 实现 `&&`、`\|\|` 和 `;` 的执行顺序及短路逻辑。 | 已实现   |
| 增加环境变量与相关展开能力。                    |          |

## 许可证

本项目遵循仓库中的 [LICENSE](LICENSE) 文件。
