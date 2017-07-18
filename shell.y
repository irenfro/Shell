
/*
 * CS-252
 * shell.y: parser for shell
 *
 * This parser compiles the following grammar:
 *
 *	cmd [arg]* [> filename]
 *
 * you must extend it to understand the complete shell grammar
 *
 */

%token	<string_val> WORD

%token 	NOTOKEN APPEND_STDO_STDE GREAT_STDO_STDE APPEND GREAT LESS PIP AMP NEWLINE

%union	{
		char   *string_val;
	}

%{
//#define yylex yylex
#define MAXFILENAME 1024
#include <stdio.h>
#include <string.h>
#include "command.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <regex.h>
#include <dirent.h>
char* getNonEscaped(char*);
void expandWildcards(char*, char*);
/*void insertWildcards(char*, char*);*/
int currEntry = 0;
int maxEntries = 256;
void addArguments();
int shouldSortDirs = 0;
int wildcardExists = 0;
int checkWord(char*);
int is_regular_file(const char* path);
static int compare (const void* a, const void* b);
char** array;
void clear();
void yyerror(const char * s);
int yylex();

%}

%%

goal:	
	commands
	;

commands: 
	command
	| commands command 
	;

command: simple_command
       | command_and_args PIP command
        ;

simple_command:	
	command_and_args iomodifier_opt NEWLINE {
		/*printf("   Yacc: Execute command\n");*/
		Command::_currentCommand.execute();
	}
	| NEWLINE { Command::_currentCommand.prompt(); }
	| error NEWLINE { yyerrok; }
	;

command_and_args:
	command_word argument_list {
		Command::_currentCommand.
			insertSimpleCommand( Command::_currentSimpleCommand );
	}
	;

argument_list:
	argument_list argument
	| /* can be empty */
	;

argument:
	WORD {
            /*printf("   Yacc: insert argument \"%s\"\n", $1);*/
	       /*Command::_currentSimpleCommand->insertArgument( $1 );*/
           char* temp = getNonEscaped($1);
	       expandWildcards(NULL, temp);

           if(!wildcardExists) {
                array[0] = temp;
                currEntry++;
           }

           if(shouldSortDirs) {
                qsort(array, currEntry, sizeof(char*), compare);
           }

           addArguments();
           clear();
    }
	;

command_word:
	WORD {
               /*printf("   Yacc: insert command \"%s\"\n", $1);*/
           if(!strcmp($1, "exit")) {
                printf("Good bye!!\n");
                exit(0);
           }
	       Command::_currentSimpleCommand = new SimpleCommand();
	       Command::_currentSimpleCommand->insertArgument( $1 );
	}
	;


iomodifier_opt:
	iomodifier_opt GREAT WORD {
		/*printf("   Yacc: insert output \"%s\"\n", $3);*/
		/*Command::_currentCommand._outFile = $3;*/
        Command::_currentCommand.setOut($3);
	}
    | iomodifier_opt LESS WORD {
       /* printf("   Yacc: insert input \"%s\"\n", $3);*/
        /*Command::_currentCommand._inFile = $3;*/
        Command::_currentCommand.setIn($3);
    }
    | iomodifier_opt APPEND WORD {
       /* printf("   Yacc: insert output append \"%s\"\n", $3);*/
        /*Command::_currentCommand._outFile = $3;*/
        Command::_currentCommand.setOut($3);
        Command::_currentCommand._append = 1;
    }
    | iomodifier_opt APPEND_STDO_STDE WORD {
        /*printf("   Yacc: insert stdout stderr append \"%s\"\n", $3);*/
        /*Command::_currentCommand._outFile = $3;*/
		/*Command::_currentCommand._errFile = $3;*/
        Command::_currentCommand.setOut($3);
        Command::_currentCommand.setErr($3);
        Command::_currentCommand._append = 1;
    }
    | iomodifier_opt GREAT_STDO_STDE WORD {
		/*printf("   Yacc: insert overwrite file with stdout and stderr \"%s\"\n", $3);*/
        /*Command::_currentCommand._outFile = $3;*/
		/*Command::_currentCommand._errFile = $3;*/
        Command::_currentCommand.setOut($3);
        Command::_currentCommand.setErr($3);
    }
    | AMP {
        /*printf("   Yacc: make this background \"&\"\n");*/
		Command::_currentCommand._background = 1;
    }
	| /* can be empty */ 
	;

%%

int is_regular_file(const char *path) {
    struct stat path_stat;
    stat(path, &path_stat);
    return S_ISREG(path_stat.st_mode);
}

char* getNonEscaped(char * temp) {
    char* finalArgs;
    if(strstr(temp, "\\") == NULL) {
         finalArgs = strdup(temp);
    } else {
         finalArgs = (char*)(calloc(strlen(temp) + 1, sizeof(char)));
         char* i = temp;
         char* j = finalArgs;
         int flag = 0;
         while(*i) {
             if(flag && *i == '\\') {
                 i++;
             }
             if(*i != '\\') {
                 *j = *i;
                 i++;
                 j++;
                 flag = 0;
             }else if(*i == '\\' && *(i+1) == '\\'){
                 *j = '\\';
                 i++;
                 j++;
                 flag = 1;
             } else {
                 i++;
                 flag = 0;
             }
         }
         *j = '\0';
    }
    return finalArgs;
}

void expandWildcards(char* prefix, char* suffix) {
    if(array == NULL) {
        array = (char**)(calloc(maxEntries, sizeof(char*)));
    }
    if (suffix[0] == 0) {
        wildcardExists = 1;
        shouldSortDirs = 1;
        if(currEntry == maxEntries - 1) {
            maxEntries *= 2;
            char** tempArr = (char**)(calloc(maxEntries, sizeof(char*)));
            for(int i = 0; i < currEntry; i++) {
                tempArr[i] = array[i];
            }
            array = tempArr;
        }
        array[currEntry] = strdup(prefix);
        currEntry++;
        return;
    }
    char* temp = strchr(suffix, '?');
    if(temp != NULL && *(temp-1) == '{' && *(temp+1) == '}') {
        return;
    }
    if (strchr(suffix, '*') || temp) {
        wildcardExists = 1;
        shouldSortDirs = 1;
    }
    
    char* s = strchr(suffix, '/');
    char component[MAXFILENAME];
    if (s != NULL) {
        if(s-suffix == 0) {
            component[0] = '\0';
        } else {
            strncpy(component, suffix, s-suffix);
            component[strlen(suffix) - strlen(s)] = '\0';
        }
        suffix = s + 1;
    } else {
        strcpy(component, suffix);
        suffix = suffix + strlen(suffix);
    }

    char newPrefix[MAXFILENAME];

    if(strchr(component, '*') == NULL && strchr(component, '?') == NULL){
        
        if(prefix == NULL && component[0] != '\0') {
            sprintf(newPrefix, "%s", component);
            if(component[0] != '\0') {
                expandWildcards(newPrefix, suffix);
            } else {
                expandWildcards("", suffix);
            }
            return;
        } else if(component[0] != '\0') {
            sprintf(newPrefix, "%s/%s", prefix, component);
            expandWildcards(newPrefix, suffix);
            return;
        } else {
            expandWildcards("", suffix);
        }
        return;
        
    }


    char* reg = (char*)calloc(2 * strlen(component) + 10, sizeof(char));
    char* a = component;
    char* r = reg;
    *r = '^';
    r++;
    while(*a) {
        if(*a == '*') {
            *r = '.';
            r++;
            *r = '*';
            r++;
        } else if (*a == '?') {
            *r = '.';
            r++;
        } else if (*a == '.') {
            *r = '\\';
            r++;
            *r = '.';
            r++;
        } else {
            *r = *a;
            r++;
        }
        a++;
    }
   // if(*(r-1) != '*'){
        *r = '$';
  //      r++;
   // }
    if(*(reg+2) == '*') {
        reg++;
    }
    *r = '\0';
    /*printf("reg: %s\n", reg);*/
    regex_t preg;
    int expbuf = regcomp(&preg, reg, REG_EXTENDED|REG_NOSUB);
    if(expbuf){
        perror("ERROR IN: yacc compile");
        exit(0);
    }
   
    //std::regex temp(reg);
    if(is_regular_file(prefix)) {
        return;
    }

    DIR * dir;
    if(prefix == NULL) {
        dir = opendir(".");
    } else if(!strcmp("", prefix)){
        dir = opendir("/");
    }else {
        dir = opendir(prefix);
    }
    if(dir == NULL) {
        perror("ERROR IN: opendir");
        exit(0);
    }

   struct dirent* ent;
   regmatch_t match[1];
   while((ent = readdir(dir)) != NULL) {
       //printf("%s: %d, %d\n", ent->d_name, std::regex_match(ent->d_name, temp), ent->d_name[0]);
       if(!regexec(&preg, ent->d_name, 1, match, 0)) {
       //if(std::regex_match(ent->d_name, temp)) {
            if(*ent->d_name != '.' || component[0] == '.') {
                if(prefix == NULL) {
                    sprintf(newPrefix, "%s", ent->d_name);
                } else {
                    sprintf(newPrefix, "%s/%s", prefix, ent->d_name);
                }
                expandWildcards(newPrefix, suffix);
            }
       }
   }
   closedir(dir);
   return;
}

void clear() {
    currEntry = 0;
    maxEntries = 2300;
    array = NULL;
    shouldSortDirs = 0;
    wildcardExists = 0;
}

static int compare(const void* a, const void* b) {
    return strcmp(*(const char**) a, *(const char**) b);
}

int checkWord(char* a) {
    for(int i = 0; i < strlen(a); i++) {
        if((int)*(a + i) < 33 || (int)*(a + i) > 126){
            return 0;
        }
    }
    return 1;
}

void addArguments() {
    for(int i = 0; i < currEntry; i++) {
        //if(checkWord(array[i]) {
            Command::_currentSimpleCommand->insertArgument(strdup(array[i]));
        //}
    }
    return;
}

void
yyerror(const char * s)
{
	fprintf(stderr,"%s", s);
}

#if 0
main()
{
	yyparse();
}
#endif
