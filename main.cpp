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
using std::endl;
using std::string;

static void call(const string &input) {
    if (const int rc = fork(); rc < 0) {
        perror("fork error");
        exit(-1);
    }
    else if (rc == 0) {
        printf("child process(pid:%d)\n", rc);
        std::stringstream ss(input);
        string line;
        std::vector<char *> tokens;
        while (ss >> line) {
            char *token = strdup(line.c_str());
            cout << token << endl;
            tokens.push_back(token);
        }
        tokens.push_back(nullptr);
        execvp(tokens[0], tokens.data());
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
    const string prompt = "input.txt";
    while (true) {
        cout << prompt;
        string input;
        getline(cin, input);
        if (input == "q") {
            break;
        }
        call(input);
    }
    return 0;
}
