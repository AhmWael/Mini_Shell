
/*
 * 
 *
 * Template file.
 * You will need to add more code here to execute the command table.
 *
 * NOTE: You are responsible for fixing any bugs this code may have!
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <time.h>
#include <glob.h>
#include <wordexp.h>
#include <vector>
#include <string>
#include <cctype>
#include <iostream>
#include <sstream>
#include "command.h"
#include "tokenizer.h"

void sigint_handler(int signum) {
    printf("\nmyshell>");
    fflush(stdout);
    Command::_currentCommand.clear();
}

void sigchld_handler(int signum){
    pid_t pid;
    int status;

    // Wait for all child processes that have terminated
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        time_t now;
        time(&now);
        char *timestamp = ctime(&now);
        timestamp[strlen(timestamp) - 1] = '\0';

        // Create log message
        char log_message[256];
        snprintf(log_message, sizeof(log_message),
                 "[%s] Child PID: %d terminated with status: %s\n",
                 timestamp, pid, 
                 WIFEXITED(status) ? "normal" : (WIFSIGNALED(status) ? "signal" : "stopped"));

        int fd = open("log.txt", O_WRONLY | O_CREAT | O_APPEND, 0666);
        if (fd == -1) {
            perror("Error opening log file");
            return;
        }
        ssize_t bytes_written = write(fd, log_message, strlen(log_message));
        if (bytes_written == -1) {
            perror("Error writing to log file");
        }
        close(fd);
    }
}

void parse(std::vector<Token> &tokens)
{
    Command::_currentCommand.clear();
    SimpleCommand *currentSimpleCommand = new SimpleCommand();

    for(size_t i = 0; i < tokens.size(); i++){
        Token &t = tokens[i];

        if(t.type == TOKEN_COMMAND || t.type == TOKEN_ARGUMENT){
            // covert string to char*
            currentSimpleCommand->insertArgument(strdup(t.value.c_str()));
        }
        else if(t.type == TOKEN_REDIRECT){  // token ---- ">"
            if (i + 1 < tokens.size() && tokens[i + 1].type == TOKEN_ARGUMENT) {
                Command::_currentCommand._outFile = strdup(tokens[i+1].value.c_str());
                ++i;
            } else {
                perror("Error: Missing file after redirection '>'");
                printf("myshell>");
                return; 
            }
        }
        else if(t.type == TOKEN_APPEND){    // token ---- ">>"
            if (i + 1 < tokens.size() && tokens[i + 1].type == TOKEN_ARGUMENT) {
                Command::_currentCommand._outFile = strdup(tokens[i+1].value.c_str());
                Command::_currentCommand._append = 1;
                ++i;
            } else {
                perror("Error: Missing file after append '>>'");
                printf("myshell>");
                return; 
            }
        }
        else if(t.type == TOKEN_BACKGROUND){    // token ---- "&"
            Command::_currentCommand._background = 1;
        }
        else if(t.type == TOKEN_ERROR){    // token ---- "2>"
            if (i + 1 < tokens.size() && tokens[i + 1].type == TOKEN_ARGUMENT) {
                Command::_currentCommand._errFile = strdup(tokens[i+1].value.c_str());
                ++i;
            } else {
                perror("Error: Missing file after error redirection '2>'");
                printf("myshell>");
                return; 
            }
        }
        else if(t.type == TOKEN_REDIRECT_AND_ERROR){    // token ---- ">>&"
            if (i + 1 < tokens.size() && tokens[i + 1].type == TOKEN_ARGUMENT) {
                Command::_currentCommand._outFile = strdup(tokens[i+1].value.c_str());
                Command::_currentCommand._errFile = strdup(tokens[i+1].value.c_str());
                Command::_currentCommand._out_error = 1;
                ++i;
            } else {
                perror("Error: Missing file after redirect and error '>>&'");
                printf("myshell>");
                return; 
            }
        }
        else if(t.type == TOKEN_INPUT){  // token ---- "<"
            if (i + 1 < tokens.size() && tokens[i + 1].type == TOKEN_ARGUMENT) {
                Command::_currentCommand._inputFile = strdup(tokens[i+1].value.c_str());
                ++i;
            } else {
                perror("Error: Missing file after input '<'");
                printf("myshell>");
                return; 
            }
        }
        else if(t.type == TOKEN_PIPE){  // token ---- "|"
            if (i == 0 || i + 1 >= tokens.size() || tokens[i + 1].type != TOKEN_COMMAND) {
                perror("Error: Pipe usage error. A command is expected on both sides of the pipe");
                printf("myshell>");
                return;
            }
            // if detected pipes save last command as simple command and start a new one
            Command::_currentCommand.insertSimpleCommand(currentSimpleCommand);
            currentSimpleCommand = new SimpleCommand();
        }
        else if(t.type == TOKEN_EOF){
            break;
        }
        else {
            perror("Error in parsing");
            break;
        }
    }

    // Check if there are arguments available
    if(currentSimpleCommand->_numberOfArguments > 0){
        Command::_currentCommand.insertSimpleCommand(currentSimpleCommand);
    }

    Command::_currentCommand.execute();
}

SimpleCommand::SimpleCommand()
{
    _numberOfAvailableArguments = 5;
    _numberOfArguments = 0;
    _arguments = (char **)malloc(_numberOfAvailableArguments * sizeof(char *));
}

void SimpleCommand::insertArgument(char *argument)
{
    // handle wildcards
    if (strchr(argument, '*') || strchr(argument, '?')){
        glob_t globbuf;
        // get all matches
        int ret = glob(argument, 0, NULL, &globbuf);
        // if matches are found
        if(ret == 0){
            for(size_t i = 0; i < globbuf.gl_pathc; i++){
                if (_numberOfAvailableArguments == _numberOfArguments + 1)
                {
                    _numberOfAvailableArguments *= 2;
                    _arguments = (char **)realloc(_arguments,
                                                _numberOfAvailableArguments * sizeof(char *));
                }
                _arguments[_numberOfArguments] = strdup(globbuf.gl_pathv[i]);
                _arguments[_numberOfArguments + 1] = NULL;

                _numberOfArguments++;
            }
        }
        globfree(&globbuf);
    }
    else {
        if (_numberOfAvailableArguments == _numberOfArguments + 1)
        {
            _numberOfAvailableArguments *= 2;
            _arguments = (char **)realloc(_arguments,
                                        _numberOfAvailableArguments * sizeof(char *));
        }

        _arguments[_numberOfArguments] = argument;
        _arguments[_numberOfArguments + 1] = NULL;

        _numberOfArguments++;
    }
}

Command::Command()
{
    _numberOfAvailableSimpleCommands = 1;
    _simpleCommands = (SimpleCommand **)
        malloc(_numberOfAvailableSimpleCommands * sizeof(SimpleCommand *));

    _numberOfSimpleCommands = 0;
    _outFile = 0;
    _inputFile = 0;
    _errFile = 0;
    _background = 0;
}

void Command::insertSimpleCommand(SimpleCommand *simpleCommand)
{
    if (_numberOfAvailableSimpleCommands == _numberOfSimpleCommands)
    {
        _numberOfAvailableSimpleCommands *= 2;
        _simpleCommands = (SimpleCommand **)realloc(_simpleCommands,
                                                    _numberOfAvailableSimpleCommands * sizeof(SimpleCommand *));
    }

    _simpleCommands[_numberOfSimpleCommands] = simpleCommand;
    _numberOfSimpleCommands++;
}

void Command::clear()
{
    for (int i = 0; i < _numberOfSimpleCommands; i++)
    {
        for (int j = 0; j < _simpleCommands[i]->_numberOfArguments; j++)
        {
            free(_simpleCommands[i]->_arguments[j]);
        }

        free(_simpleCommands[i]->_arguments);
        free(_simpleCommands[i]);
    }

    if (_outFile)
    {
        free(_outFile);
    }

    if (_inputFile)
    {
        free(_inputFile);
    }

    if (_errFile)
    {
        free(_errFile);
    }

    _append = 0;
    _out_error = 0;
    _numberOfSimpleCommands = 0;
    _outFile = 0;
    _inputFile = 0;
    _errFile = 0;
    _background = 0;
}

void Command::print()
{
    printf("\n\n");
    printf("              COMMAND TABLE                \n");
    printf("\n");
    printf("  #   Simple Commands\n");
    printf("  --- ----------------------------------------------------------\n");

    for (int i = 0; i < _numberOfSimpleCommands; i++)
    {
        printf("  %-3d ", i);
        for (int j = 0; j < _simpleCommands[i]->_numberOfArguments; j++)
        {
            printf("\"%s\" \t", _simpleCommands[i]->_arguments[j]);
        }
    }

    printf("\n\n");
    printf("  Output       Input        Error        Err&Out       Background\n");
    printf("  ------------ ------------ ------------ ------------ ------------\n");
    printf("  %-12s %-12s %-12s %-12s %-12s\n", _outFile ? _outFile : "default",
           _inputFile ? _inputFile : "default", _errFile ? _errFile : "default", _out_error == 1 ? _errFile : "default"
           ,_background ? "YES" : "NO");
    printf("\n\n");
}

void Command::execute()
{
    print();

    pid_t pids[_numberOfSimpleCommands];

    // initialize pipes array with size 2*(n-1)
    int pipes[2 * (_numberOfSimpleCommands - 1)];

    // check if pipe is needed
    if(_numberOfSimpleCommands > 1){
        // create pipes and store them in array
        for(int i = 0; i < _numberOfSimpleCommands - 1; i++){
            pipe(pipes + i * 2);
        }
    }
    
    for(int i = 0; i < _numberOfSimpleCommands; i++){

        SimpleCommand *cmd = _simpleCommands[i];

        if (strcmp(cmd->_arguments[0], "exit") == 0){
            printf("Good bye!!\n");
            exit(0);
        }

        if (strcmp(cmd->_arguments[0], "cd") == 0){
            if(cmd->_arguments[1] == NULL){
                char *home_dir = getenv("HOME");
                if(chdir(home_dir) != 0){
                    perror("Error in changing directory");
                    clear();
                    prompt();
                    return;
                }
                printf("Changing to directory 'HOME'\n");
            }
            else{
                if(chdir(cmd->_arguments[1]) != 0){
                    perror("Error in changing directory");
                    clear();
                    prompt();
                    return;
                }
                printf("Changing to directory '%s'\n", cmd->_arguments[1]);
            }

            char cwd[1024];
            if (getcwd(cwd, sizeof(cwd)) != NULL) {
                printf("You are now in %s\n", cwd);
            }
            clear();
            prompt();
            return;
        }

        pid_t pid = fork();
        if(pid == 0){
            if(i == _numberOfSimpleCommands - 1){
                // handle redirection of output in child process
                if (_currentCommand._outFile){
                    int fd;
                    if(_currentCommand._append){
                        // Open file given as argument with append flag
                        fd = open(_currentCommand._outFile, O_WRONLY | O_CREAT | O_APPEND, 0666);
                        if (fd == -1) {
                            perror("Output file does not exist or cannot be opened");
                            exit(1);
                        }
                    }
                    else {
                        // Open file given as argument with truncate flag
                        fd = open(_currentCommand._outFile, O_WRONLY | O_CREAT | O_TRUNC, 0666);
                        if (fd == -1) {
                            perror("Output file does not exist or cannot be opened");
                            exit(1);
                        }
                    }

                    // Direct error to file
                    if(_currentCommand._out_error){
                        dup2(fd, STDERR_FILENO);
                    }

                    // Direct output to file
                    dup2(fd, STDOUT_FILENO);
                    close(fd);
                }
                else if (_currentCommand._errFile){
                    int fd = open(_currentCommand._errFile, O_WRONLY | O_CREAT | O_TRUNC, 0666);
                    if (fd == -1) {
                        perror("Error file does not exist or cannot be opened");
                        exit(1);
                    }
                    // Direct output to file
                    dup2(fd, STDERR_FILENO);
                    close(fd);
                }
            }
            else if(i < _numberOfSimpleCommands - 1){
                // not last command
                // direct output to upcoming command
                dup2(pipes[i * 2 + 1], STDOUT_FILENO);
            }

            if (_currentCommand._inputFile && i == 0){
                int fd = open(_currentCommand._inputFile, O_RDONLY);
                if (fd == -1) {
                    perror("Input file does not exist or cannot be opened");
                    exit(1);
                }
                // Direct input from file
                dup2(fd, STDIN_FILENO);
                close(fd);
            } 
            else if(i > 0){
                // not first command
                // direct input from last command
                dup2(pipes[((i - 1) * 2)], STDIN_FILENO);
            }

            // close all child pipe file descriptors
            for(int j = 0; j < 2 * (_numberOfSimpleCommands - 1); j++){
                close(pipes[j]);
            }

            // execute the command
            if (execvp(cmd->_arguments[0], cmd->_arguments) == -1) {
                perror("Command execution failed");
                exit(1);
            }
        }
        else if(pid < 0){
            perror("Error in forking");
        }
        pids[i] = pid;
    }

    if(_numberOfSimpleCommands > 1){
        // close all pipe file descriptors
        for(int j = 0; j < 2 * (_numberOfSimpleCommands - 1); j++){
            close(pipes[j]);
        }
    }
    
    if(!_background){
        int status;
        for(int i = 0; i < _numberOfSimpleCommands; i++){
            waitpid(pids[i], &status, 0);

            // Log termination for foreground children
            time_t now;
            time(&now);
            char *timestamp = ctime(&now);
            timestamp[strlen(timestamp) - 1] = '\0';

            char log_message[256];
            snprintf(log_message, sizeof(log_message),
                    "[%s] Child PID: %d terminated with status: %s\n",
                    timestamp, pids[i],
                    WIFEXITED(status) ? "normal" :
                    (WIFSIGNALED(status) ? "signal" : "stopped"));

            int fd = open("log.txt", O_WRONLY | O_CREAT | O_APPEND, 0666);
            if (fd != -1) {
                write(fd, log_message, strlen(log_message));
                close(fd);
            }
        }
    }

    clear();
    prompt();
}

void Command::prompt()
{
    printf("myshell>");
    fflush(stdout);
    std::string input;
    while (true)
    {
        std::getline(std::cin, input); 
        std::vector<Token> tokens = tokenize(input);
        parse(tokens);
    }
}

Command Command::_currentCommand;
SimpleCommand *Command::_currentSimpleCommand;

int main()
{
    // Set up the SIGINT handler
    signal(SIGINT, sigint_handler);

    // Set up the SIGCHLD handler for logging child process terminations
    signal(SIGCHLD, sigchld_handler);

    Command::_currentCommand.prompt();
    return 0;
}
