# MiniShell

一个使用 C++20 编写的 Unix/Linux 命令行 shell 实验项目。项目用于练习进程创建、程序替换、管道、文件描述符重定向、作业控制以及父子进程同步。

## 功能概览

当前版本支持以下功能：

- 执行外部程序，并通过 `execvp` 使用系统的 `PATH` 查找程序。
- 内置命令：`cd`、`pwd`、`echo`、`exit`、`export`、`unexport`、`env`。
- `cd` 不指定参数或使用 `~` 时切换到用户主目录。
- 环境变量：`export 变量名 值` 定义、`unexport 变量名` 删除、`env` 列出。
- 变量展开：`$变量名` 在解析阶段替换为对应的值，可用于命令名、参数或重定向文件名。
- 内置变量 `prompt` 控制提示符内容，默认值为 `$miniShell> `，删除后回退到该默认值。
- 输入重定向：`<`
- 输出重定向并覆盖文件：`>`
- 输出重定向并追加文件：`>>`
- 单管道和多管道：`command1 | command2 | command3`
- 命令连接：`;` 顺序执行，`&&`、`||` 依据左侧命令的退出码短路执行。
- 并行命令：`&` 分隔的各项先全部启动，最后统一 `waitpid` 回收，全部结束后才返回提示符或执行下一行。
- batch（批处理）模式：以单个文件作为参数启动时从该文件逐行读取命令，不打印提示符，文件读到 EOF 即结束。
- 通过 `fork`、`pipe`、`dup2` 和 `waitpid` 管理进程与文件描述符。
- 使用进程组（`setpgid`）与终端前台进程组（`tcsetpgrp`）进行作业控制。
- 处理 `SIGINT`（Ctrl-C）中断，屏蔽 shell 自身的 `SIGTTOU`、`SIGTTIN`、`SIGTSTP`。
- 输入支持单引号或双引号包裹的、带空格的参数。
- 管道和重定向运算符不必与其他内容用空格分开，例如使用 `cat<input.txt`。
- 支持输入指令历史保存功能，使用`!!`执行上次命令，`history`显示命令历史，`! [0-..]` 来执行历史中的第n个命令

## 构建与运行

项目不依赖第三方库，可直接使用支持 C++20 的编译器构建。

### 使用 CMake（推荐）

```bash
cmake -B build
cmake --build build
./build/MiniShell
```

不带参数进入交互模式；带一个文件名则进入 batch 模式，依次执行文件中的每一行：

```bash
./build/MiniShell batch.txt
```

参数多于一个、或 batch 文件无法打开时，会向标准错误输出提示并以非 0 状态码退出。

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

```bash
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
export greeting hello
echo $greeting
env
unexport greeting
export prompt ">>> "
export cmd pwd
$cmd
!!
history
! 3
sleep 1 & echo hello ; echo done
```

并行命令比 `;` 先绑定：`a ; b & c` 表示先执行 `a`，再并行执行 `b` 与 `c`；行尾多余的 `&` 会被忽略，`&` 单独一行时不执行任何命令。

`&&` 仅在左侧命令退出码为 `0` 时执行右侧命令，`||` 仅在左侧退出码非 `0` 时执行右侧命令，二者优先级相同且左结合，高于 `;`、低于
`|`。

环境变量以 `env` 列出，输出形如 `变量名:值`。`$变量名` 在构建 AST 之前被替换，未定义的变量保持原样输出；修改内置变量
`prompt` 可即时改变提示符，将其 `unexport` 后恢复默认提示符。变量状态保存在 shell 进程内，退出后不保留；参数不合法或删除
未定义的变量时，会向标准错误输出用法提示。

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
├── Shell.cpp      # 词法分析、语法解析、变量展开、执行与进程管理
├── buildin.h      # 内置命令声明
├── buildin.cpp    # 内置命令（pwd / cd / echo）实现
├── database.h     # 公共数据结构（Token、Command、ASTNode 等）
├── CMakeLists.txt # CMake 构建配置
└── README.md      # 项目说明
```

执行流程：`getLine` 读取输入 → `tokenize` 拆分为独立单元 → `parser` 展开 `$变量名` 后构建 AST → `executeAST`
分派到单命令、管道、`&&`/`||`、顺序执行或并行执行。解析器按优先级递归下降：`;`（最低）→ `&`（并行组）→ `&&`/`||`（左结合）→ `|`（最高）。
带 batch 文件启动时，`main` 先把标准输入切换到该文件并置位 `g_batch`，此后同一套循环不再打印提示符。
`export`、`unexport`、`env` 由 `Shell.cpp` 中的 `shellFork` 在父进程内直接处理，因此变量修改在 shell 生命周期内持续有效。

## 当前限制

- 并行项各自运行在子进程中，因此写在 `&` 列表里的 `cd`、`export`、`unexport` 不会改变 shell 自身的环境与工作目录。
- `&` 列表中的管道整体作为一个并行项，成员之间仍是串行；多个并行项同时访问终端时不做前台进程组交接。
- `export` 要求以空格分隔的两个参数，不支持 `变量名=值` 的写法。
- 变量展开仅识别"整词为 `$变量名`"，不支持 `${VAR}` 与字符串内嵌展开（如 `abc$VAR`）；尚未支持通配符展开。
- 环境变量由 shell 自身维护，不会通过 `environ` 传递给子进程。
- 错误信息由系统调用通过标准错误输出，具体文本可能因操作系统和运行环境而不同。

## 计划实现

| 计划实现                                        | 已实现 |
|-------------------------------------------------|--------|
| 整理代码至多个文件中                            | 已实现 |
| 增加 `cd`、`pwd`、`echo` 等内置命令。           | 已实现 |
| 完善单引号和双引号参数解析。                    | 已实现 |
| 实现 `&&`、`\|\|` 和 `;` 的执行顺序及短路逻辑。 | 已实现 |
| 增加环境变量与相关展开能力。                    | 已实现 |
| 记录历史命令并显示执行                          | 已实现 |
| 支持 batch 模式执行批处理文件                   | 已实现 |
| 支持 `&` 并行命令并统一回收                     | 已实现 |

## 许可证

本项目遵循仓库中的 [LICENSE](LICENSE) 文件。
