//
// Created by 33550 on 2026/8/31.
//
#include "Shell.h"
#include "database.h"
#include <cstdlib>
#include <fstream>
#include <iostream>

int main(int argc , char* argv[])
{
    if (argc > 2) {
        std::cerr << "MiniShell: usage: MiniShell [batch-file]\n";
        return EXIT_FAILURE;
    } //最多只能带一个 batch 文件
    std::ifstream batch{};
    if (argc == 2) {
        batch.open(argv[1]);
        if (!batch.is_open()) {
            std::cerr << "MiniShell: " << argv[1] << ": cannot open batch file\n";
            return EXIT_FAILURE;
        }
        std::cin.rdbuf(batch.rdbuf()); //batch 模式从文件读取命令
        miniShell::g_batch = true;     //batch 模式不打印提示符
    }
    miniShell::Shell shell{};
    shell.run();
    return 0;
}
