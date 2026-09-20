# MiniShell

一个使用 C++ 编写的 Unix/Linux 命令行 shell 实验项目。项目用于练习进程创建、程序替换、管道、文件描述符重定向以及父子进程同步。

## 功能概览

当前版本支持以下功能：

- 执行不带复杂参数的外部程序，并通过 `execvp` 使用系统的 `PATH` 查找程序。
- 使用 `exit` 退出 shell。
- 输入重定向：`<`
- 输出重定向并覆盖文件：`>`
- 输出重定向并追加文件：`>>`
- 单管道和多管道：`command1 | command2 | command3`
- 通过 `fork`、`pipe`、`dup2` 和 `waitpid` 管理进程与文件描述符。
- 支持 `cd`、`pwd`、后台运行、作业控制和信号处理。

## 构建与运行

项目不依赖第三方库，可直接使用支持 C++20 的编译器构建：

```bash
g++ -std=c++20 -Wall -Wextra -pedantic main.cpp -o minishell
./minishell
```

启动后会看到类似下面的提示符：

```text
./minishell$miniShell>
```

输入 `exit` 即可退出。

## 使用示例

以下示例中的运算符两侧需要保留空格：

```text
ls
cat input.txt
cat < input.txt
echo hello > output.txt
echo hello >> output.txt
cat input.txt | grep hello
cat input.txt | grep hello | wc -l
```

重定向可以出现在命令参数之后；同一命令多次使用同类重定向时，后一次重定向会覆盖前一次设置，例如：

```text
echo hello > first.txt > second.txt
```

## 当前限制

- 输入目前仅按空白字符分词，不支持单引号、双引号或带空格的参数。
- 管道和重定向运算符必须与其他内容用空格分开，例如使用 `cat < input.txt`，而不是 `cat<input.txt`。
- 当前只执行单命令和纯管道表达式。`&&`、`||`、`;` 已在语法结构中预留，但尚未实现对应的执行逻辑。
- 尚未支持环境变量展开、通配符展开。
- 错误信息由系统调用通过标准错误输出，具体文本可能因操作系统和运行环境而不同。

## 计划实现

| 计划实现                                        | 已实现 |
|-------------------------------------------------|--------|
| 整理代码至多个文件中                            |        |
| 增加 `cd`、`pwd`、`echo` 等内置命令。           | 已实现 |
| 完善单引号和双引号参数解析。                    |        |
| 实现 `&&`、`\|\|` 和 `;` 的执行顺序及短路逻辑。 |        |
| 增加环境变量与相关展开能力。                    |        |

## 项目结构

```text
.
├── main.cpp   # shell 的解析、执行和进程管理实现
└── README.md  # 项目说明
```

## 许可证

本项目遵循仓库中的 [LICENSE](LICENSE) 文件。