#include <iostream>
#include <string>
#include <vector>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <fstream>
#include <sstream>
#include <algorithm>

// 解析命令行输入
std::vector<std::string> parseCommand(const std::string& input) {
    std::istringstream iss(input);
    std::vector<std::string> args;
    std::string arg;
    while (iss >> arg) {
        args.push_back(arg);
    }
    return args;
}

// 执行命令，包含 I/O 重定向和管道功能
void executeCommandWithRedirectionAndPipes(const std::vector<std::string>& args) {
    if (args.empty()) return;

    int in_fd = 0, out_fd = 1;
    std::vector<char*> c_args;
    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == ">") {
            out_fd = open(args[i+1].c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
            break;
        } else if (args[i] == "<") {
            in_fd = open(args[i+1].c_str(), O_RDONLY);
            break;
        } else if (args[i] == "|") {
            int pipefd[2];
            pipe(pipefd);
            pid_t pid = fork();
            if (pid == 0) {
                // 左侧子进程
                dup2(pipefd[1], STDOUT_FILENO);
                close(pipefd[0]);
                close(pipefd[1]);
                c_args.push_back(nullptr);
                execvp(c_args[0], c_args.data());
                perror("execvp failed");
                exit(EXIT_FAILURE);
            } else if (pid > 0) {
                // 右侧父进程
                waitpid(pid, nullptr, 0);
                dup2(pipefd[0], STDIN_FILENO);
                close(pipefd[1]);
                close(pipefd[0]);
                executeCommandWithRedirectionAndPipes(std::vector<std::string>(args.begin() + i + 1, args.end()));
                return;
            } else {
                perror("fork failed");
                return;
            }
        } else {
            c_args.push_back(const_cast<char*>(args[i].c_str()));
        }
    }

    c_args.push_back(nullptr);
    pid_t pid = fork();
    if (pid == 0) {
        // 子进程
        if (in_fd != 0) {
            dup2(in_fd, STDIN_FILENO);
            close(in_fd);
        }
        if (out_fd != 1) {
            dup2(out_fd, STDOUT_FILENO);
            close(out_fd);
        }
        execvp(c_args[0], c_args.data());
        perror("execvp failed");
        exit(EXIT_FAILURE);
    } else if (pid > 0) {
        // 父进程
        waitpid(pid, nullptr, 0);
    } else {
        perror("fork failed");
    }
}

// 执行脚本文件
void executeScript(const std::string& filename) {
    std::ifstream scriptFile(filename);
    if (!scriptFile.is_open()) {
        std::cerr << "Error: Could not open script file " << filename << std::endl;
        return;
    }

    std::string line;
    while (std::getline(scriptFile, line)) {
        auto args = parseCommand(line);
        executeCommandWithRedirectionAndPipes(args);
    }
}

// Shell 主循环
void shell() {
    std::string input;
    while (true) {
        std::cout << "myshell> ";
        std::getline(std::cin, input);
        if (input == "exit") break;
        auto args = parseCommand(input);
        if (!args.empty() && args[0] == "source") {
            if (args.size() < 2) {
                std::cerr << "Error: No script file specified" << std::endl;
            } else {
                executeScript(args[1]);
            }
        } else {
            executeCommandWithRedirectionAndPipes(args);
        }
    }
}

int main() {
    shell();
    return 0;
}
