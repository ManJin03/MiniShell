//
// Created by 33550 on 2026/8/31.
//
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <unistd.h>
#include <vector>
#include <sys/wait.h>
using std::cout;
using std::cin;
using std::string;

static void call(const string &input) {
    if (const int rc = fork(); rc < 0) {
        perror("fork error");
        exit(-1);
    }
    else if (rc == 0) {
        execvp(input.c_str(), nullptr);
        perror("execvp error");
    }
    else {
        printf("parent process(pid:%d)\n", rc);
        if (const int wc = wait(nullptr); wc < 0) {
            perror("wait error");
            exit(-1);
        }
        else {
            printf("child finished(wc:%d)\n", wc);
        }
    }
}

int main() {
    while (true) {
        cout << "minishell$";
        string input;
        cin >> input;
        if (input == "q") {
            break;
        }
        call(input);
    }
    return 0;
}
