
/*
 * CS252: Shell project
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
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <pwd.h>
#include <regex.h>

#include "readLine.h"
#include "command.h"

char* PATH;
int shellPid = 0;
int backPid = 0;
int returnCode = 0;

SimpleCommand::SimpleCommand()
{
	// Create available space for 5 arguments
	_numOfAvailableArguments = 5;
	_numOfArguments = 0;
	_arguments = (char **) malloc( _numOfAvailableArguments * sizeof( char * ) );
}

void
SimpleCommand::insertArgument( char * argument )
{
	if ( _numOfAvailableArguments == _numOfArguments  + 1 ) {
		// Double the available space
		_numOfAvailableArguments *= 2;
		_arguments = (char **) realloc( _arguments,
				  _numOfAvailableArguments * sizeof( char * ) );
	}
	
    if(!strncmp(argument, "~", 1)) {
            if(strlen(argument) == 1) {
                    argument = strdup(getenv("HOME"));
            } else {
                    char* pnt = strstr(argument, "/");
                    if(pnt == NULL) {
                        argument = strdup(getpwnam(argument + 1)->pw_dir);
                    } else {
                        int numBytes = pnt - (argument + 1);
                        char* temp = (char*)malloc(numBytes + 1);
                        strncpy(temp, argument + 1, numBytes);
                        strcat(temp, "\0");
                        char* temp2 = strdup(getpwnam(temp)->pw_dir);
                        char* finString = (char*)(calloc(sizeof(char), strlen(temp) + strlen(argument) - numBytes));
                        strncpy(finString, temp2, strlen(temp2));
                        strncat(finString, (argument + 1 + numBytes), strlen(argument) - 1 - numBytes);
                        strcat(finString, "\0");
                        argument = strdup(finString);
                        free(finString);
                        free(temp2);
                        free(temp);
                    }
            }
    }

    const char* patt = "\${.[^\{\}]*}";
    regex_t preg;
    regmatch_t match;
    if(regcomp(&preg, patt, 0)) {
            perror("ERROR IN: regex, unable to compile");
            exit(1);
    }

    if(!regexec(&preg, argument, 1, &match, 0)) {
        char* expansion = (char*)calloc(sizeof(char), 1024);  //assuming that the max length of an argument can be 1024
        char* i = argument;
        char* j = expansion;

        while(*i != 0 && strlen(expansion) < 1024) {
                if(*i != '$') { 
                    *j = *i;
                    j++;
                    *j = '\0';
                    i++;
                } else {
                        char* opCurly = strstr(i, "{");
                        char* closCurly = strstr(i, "}");
                        char* variable = (char*)calloc(1, (closCurly - opCurly) + 1);
                        strncpy(variable, opCurly + 1, closCurly - opCurly - 1);
                        strcat(variable, "\0");
                        char* value;
                        if(strlen(variable) == 1 && *variable == '_') {
                            value = getHistoryCmmd();    
                        } else if(strlen(variable) == 1 && *variable == '$') {
                            int temp = shellPid;
                            char t[50] = {'\0'};
                            sprintf(t, "%d", temp);
                            value = strdup(t);
                        } else if(strlen(variable) == 1 && *variable == '?') {
                            int temp = returnCode;
                            char t[2] = {'\0'};
                            sprintf(t, "%d", temp);
                            value = strdup(t);
                        } else if(strlen(variable) == 5 && !strcmp(variable, "SHELL")){
                            value = strdup(PATH);
                        } else if(strlen(variable) == 1 && *variable == '!') {
                            int temp = backPid;
                            char t[50] = {'\0'};
                            sprintf(t, "%d", temp);
                            value = strdup(t);
                        } else {
                            value = getenv(variable);
                        }
                        if(value != NULL) {
                                strncat(expansion, value, strlen(value));
                        }
                        i = closCurly;
                        i++;
                        j += strlen(value);
                        free(variable);
                }
        }
        argument = strdup(expansion);
    }

	_arguments[ _numOfArguments ] = argument;

	// Add NULL argument at the end
	_arguments[ _numOfArguments + 1] = NULL;
	
	_numOfArguments++;
}

Command::Command()
{
	// Create available space for one simple command
	_numOfAvailableSimpleCommands = 1;
	_simpleCommands = (SimpleCommand **)
		malloc( _numOfSimpleCommands * sizeof( SimpleCommand * ) );

	_numOfSimpleCommands = 0;
	_outFile = 0;
	_inFile = 0;
	_errFile = 0;
    _append = 0;
	_background = 0;
}

void
Command::insertSimpleCommand( SimpleCommand * simpleCommand )
{
	if ( _numOfAvailableSimpleCommands == _numOfSimpleCommands ) {
		_numOfAvailableSimpleCommands *= 2;
		_simpleCommands = (SimpleCommand **) realloc( _simpleCommands,
			 _numOfAvailableSimpleCommands * sizeof( SimpleCommand * ) );
	}
	
	_simpleCommands[ _numOfSimpleCommands ] = simpleCommand;
	_numOfSimpleCommands++;
}

void
Command:: clear()
{
	for ( int i = 0; i < _numOfSimpleCommands; i++ ) {
		for ( int j = 0; j < _simpleCommands[ i ]->_numOfArguments; j ++ ) {
			free ( _simpleCommands[ i ]->_arguments[ j ] );
		}
		
		free ( _simpleCommands[ i ]->_arguments );
		free ( _simpleCommands[ i ] );
	}

	if ( _outFile ) {
		free( _outFile );
	}

	if ( _inFile ) {
		free( _inFile );
	}

	if ( _errFile ) {
		free( _errFile );
	}

	_numOfSimpleCommands = 0;
	_outFile = 0;
	_inFile = 0;
	_errFile = 0;
	_background = 0;
    _append = 0;
}

void
Command::print()
{
	printf("\n\n");
	printf("              COMMAND TABLE                \n");
	printf("\n");
	printf("  #   Simple Commands\n");
	printf("  --- ----------------------------------------------------------\n");
	
	for ( int i = 0; i < _numOfSimpleCommands; i++ ) {
		printf("  %-3d ", i );
		for ( int j = 0; j < _simpleCommands[i]->_numOfArguments; j++ ) {
			printf("\"%s\" \t", _simpleCommands[i]->_arguments[ j ] );
		}
	}

	printf( "\n\n" );
	printf( "  Output       Input        Error        Background\n" );
	printf( "  ------------ ------------ ------------ ------------\n" );
	printf( "  %-12s %-12s %-12s %-12s\n", _outFile?_outFile:"default",
		_inFile?_inFile:"default", _errFile?_errFile:"default",
		_background?"YES":"NO");
	printf( "\n\n" );
	
}

void Command::setIn(char* fdin){
        if(_inFile != NULL) {
                fprintf(stderr, "Ambiguous output redirect");
                exit(1);
        }
        _inFile = fdin;
}


void Command::setOut(char* fdout){
        if(_outFile != NULL) {
                fprintf(stderr, "Ambiguous output redirect");
                exit(1);
        }
        _outFile = fdout;
}

void Command::setErr(char* fderr){
        if(_errFile != NULL) {

                fprintf(stderr, "Ambiguous output redirect");
                exit(1);
        }
        _errFile = fderr;
}

void
Command::execute()
{
	// Don't do anything if there are no simple commands
	if ( _numOfSimpleCommands == 0 ) {
		prompt();
		return;
	}

	// Print contents of Command data structure
	//print();

	// Add execution here
	// For every simple command fork a new process
	// Setup i/o redirection
	// and call exec
    
    
    /*----------------------Start Of Added Code--------------------------------- */ 
    
    // Save the stdin, stdout, stderr
    int defaultin = dup(0);
    int defaultout = dup(1);
    int defaulterr = dup(2);


    // Files for stdin, stdout, stderr
    int std_in_file = -1;
    int std_out_file = -1;
    int std_err_file = -1;



    // Redirect stdin if needed
    if(_inFile != NULL) {
        
        // Open the file
        std_in_file = open(_inFile, O_RDONLY);  
        
        // If we were unable to open the file then report an error
        if(std_in_file == -1) {
                perror("ERROR IN: stdin open file");
                exit(2);
        }
        // Redirect stdin to the file
        dup2(std_in_file, 0); 
        
    } else {
            std_in_file = dup(0);
    }

    // Redirect stdout if needed
    if(_outFile != NULL) {
     
        //  Stdout depends on whether or not we are appending
        if(!_append) {
        
            // Open the file
            std_out_file = open(_outFile, O_WRONLY | O_TRUNC | O_CREAT, 0666);
        } else {
        
            // Since we are appending then we can just open it
            std_out_file = open(_outFile, O_WRONLY | O_APPEND | O_CREAT, 0666);
        }

        // If there was an error opening the file then report it
        if(std_out_file == -1) {
                perror("ERROR IN: stdout open file");
                exit(2);
        }
        
    } else {
            std_out_file = dup(1);
    }

    // Redirect stderr if needed
    if(_errFile != NULL) {
     
        // If we have the command >& or >>& then we need to redirect stdout 
        // and stderr to the same place
        if(!strcmp(_errFile, _outFile)) {
        
            // In the grammer we set both the _outFile and the _errFile to 
            // the same file name and if we do not make one of them null then 
            // we will get a double free error
            _errFile = 0;
            
            // Redirect the stderr to stdout which gets redirected to the std_out_file
            dup2(std_out_file, 2);
        } else {
            
            // What we do with the file depends on if we are appending or not
            if(!_append) {
            
                // Open the file
                std_err_file = open(_errFile, O_CREAT | O_WRONLY | O_TRUNC, 0666);
            } else {
                
                // Since we are appending then we can just open the file 
                // and there is no need to clear it
                std_err_file = open(_errFile, O_CREAT | O_WRONLY | O_APPEND, 0666);
            }
     
            // If we encountered an error while opening the file then 
            // we need to report it
            if(std_err_file == -1) {
                    perror("ERROR IN: stderr open file");
                    exit(2);
            }
            // Redirect stderr to the file
            dup2(std_err_file, 2);
        }
    } else {
            std_err_file = dup(2);
    }

    int pid;
    int fdpipe[2];

    if(!strcmp(_simpleCommands[0]->_arguments[0], "setenv")) {          
        char* temp  = (char*)calloc(sizeof(char), strlen(_simpleCommands[0]->_arguments[1]) + strlen(_simpleCommands[0]->_arguments[2]) + 1);
        strncpy(temp, _simpleCommands[0]->_arguments[1], strlen(_simpleCommands[0]->_arguments[1]));
        strcat(temp, "=");
        strncat(temp, _simpleCommands[0]->_arguments[2], strlen(_simpleCommands[0]->_arguments[2]));
        strcat(temp, "\0");
        int ret = putenv(temp);
        if(ret != 0) {
                perror("ERROR IN: setenv");
                exit(1);
        }
        clear();
        prompt();
        return;
    } else if (!strcmp(_simpleCommands[0]->_arguments[0], "unsetenv")) {
        int temp = unsetenv(_simpleCommands[0]->_arguments[1]);
        if(temp != 0) {
                perror("ERROR IN: unsetenv");
                exit(1);
        }
        clear();
        prompt();
        return;
    }
    int tempBackPid;

    // Iterate over all of the simple commands
    for(int i = 0; i < _numOfSimpleCommands; i++) {

        if(!strcmp(_simpleCommands[i]->_arguments[0], "cd")) {
            int result;
            if(_simpleCommands[i]->_numOfArguments > 1) {
                result = chdir(_simpleCommands[i]->_arguments[1]);
            } else {
                result = chdir(getenv("HOME"));
            }
            if(result) {
                    perror("ERROR IN: cd");
                    exit(1);
            }
            continue;
        }
        if (pipe(fdpipe) ==  -1) {
            perror("ERROR IN: piping");
            exit(1);
        }

        // For each command fork to create children 
        // processes to run the commands
        pid = fork();


        // If the process ID is 0 then it is the child process
        if(pid == 0) {
            if(i == 0 && _numOfSimpleCommands > 1) {
                    dup2(fdpipe[1], 1);
                    close(fdpipe[1]);
            } else if (i > 0 && i < _numOfSimpleCommands - 1) {
                    dup2(fdpipe[1], 1);
                    close(fdpipe[1]);
            } 
            if(i == _numOfSimpleCommands - 1) {
                    /*if(i != 0) {
                            dup2(fdpipe[0], 0);
                            close(fdpipe[0]);
                    }*/
                    dup2(std_out_file, 1);
            }

            close(std_out_file);
            close(defaultin);
            close(defaultout);
            close(defaulterr);
            //
            // Execute the command
            execvp(_simpleCommands[i]->_arguments[0], _simpleCommands[i]->_arguments); 
            // If execvp returned then there was an error, report it
            perror("ERROR IN: execvp");
           _exit(1);
        
        // If it is less then 0 then an error occured in fork 
        // and we need to report it
        } else if (pid < 0) {
            perror("ERROR IN: fork");
            exit(2);
        } else {
                tempBackPid = pid;
                dup2(fdpipe[0], 0);
                close(fdpipe[0]);
                close(fdpipe[1]);
        }
    }

    // If these commands were supposed to run in the 
    // background then we need to wait for the other 
    // processes to finish
    if(!_background) {
        int status;
        waitpid(pid, &status, 0);
        if(WIFEXITED(status)) {
            returnCode = WEXITSTATUS(status);
        }
    } else {
        backPid = tempBackPid;
    }

    // Restore the stdin, stdout, and stderr
    dup2(defaultin, 0);
    dup2(defaultout, 1);
    dup2(defaulterr, 2);

    // Close the default file descriptors
    close(defaultin);
    close(defaultout);
    close(defaulterr);

    // Close the file descriptors that were used
    if(std_in_file != -1) {
        close(std_in_file);
    }

    if(std_out_file != -1) {
        close(std_out_file);
    }

    if(std_err_file != -1) {
        close(std_err_file);
    }
    
    /*----------------------End Of Added Code----------------------------------- */ 
	
    // Clear to prepare for next command
	clear();
	
	// Print new prompt
	prompt();
}

// Shell implementation

void
Command::prompt()
{
    if(isatty(0) && getPrompt()) {
        printf("myshell>");
        fflush(stdout);
    }
}

Command Command::_currentCommand;
SimpleCommand * Command::_currentSimpleCommand;

int yyparse(void);

extern "C" void ctrlC( int sig ){
	//fprintf( stderr, "\n      sigaction\n");
    if (isatty(0)) {
        printf("\n");
        resetLine();
    }
    Command::_currentCommand.prompt();
}

extern "C" void child( int sig ) {
    int status;
    pid_t pid;
    while((pid = wait3(&status, WNOHANG, NULL)) > 0) {
        if(status != 0) {
            if(isatty(0)) {
                printf("        %d exited\n", pid);
            }
        }
    }
    Command::_currentCommand.prompt();
}

main(int argc, char** argv)
{
    PATH = strdup(argv[0]);
    struct sigaction c;
    c.sa_handler = ctrlC;
    sigemptyset(&c.sa_mask);
    c.sa_flags = SA_RESTART;
    
    struct sigaction zomb;
    zomb.sa_handler = child;
    sigemptyset(&zomb.sa_mask);
    zomb.sa_flags = SA_RESTART;
    

    if(sigaction(SIGINT, &c, NULL)){
        perror("ERROR IN: sigaction sigint");
        exit(2);
    }
    
    if(sigaction(SIGCHLD, &zomb, NULL)) {
        perror("ERROR IN: sigaction zombie");
        exit(2);
    }
    
    shellPid = getpid();

    setPrompt(1);

	Command::_currentCommand.prompt();
	yyparse();
}
