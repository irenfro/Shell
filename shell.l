
/*
 *
 * CS-252 Fall 2013
 * shell.l: lexical analyzer for shell
 * You have to extend it.
 *
 */

%{

#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "readLine.h"
#include "y.tab.h"

char* subshellEnd = "\nexit\n\0";
char* pathToShell = "/homes/renfroi/cs252/lab3-src/shell";
char* string = NULL;

static  void yyunput (int c,char *buf_ptr  );

void myunputc(int c) {
    unput(c);
}

char mygetc(FILE* f) {
        char res;

        if(!isatty(0)) {
                return getc(f);
        } else if(string == NULL ||  *string == '\0') {
                //need to write a readline method
                tty_raw_mode();
                string = strdup(readLine());
                unset_tty_raw_mode();
        }

        res = *string;
        string++;
        return res;
        
}

#undef getc
#define getc(f) mygetc(f)
%}
%x src
%%

"source"        BEGIN(src);

<src>[ \t]*      /* eat the whitespace */
<src>[^ \t\n]+   { /* got the include file name */
    yyin = fopen( yytext, "r" );
    
    if ( ! yyin ) {
        perror("ERROR IN: source opening file");
    } 
    setPrompt(0); 	/* Disable printing prompts while reading contents from the file. */
    yypush_buffer_state(yy_create_buffer( yyin, YY_BUF_SIZE ));  /* Put current buffer on stack, and use the file */
    BEGIN(INITIAL);
}
     
<<EOF>> {
    yypop_buffer_state();
    setPrompt(1);    /* Enable printing prompts again.  This assumes that we only use import for one level. */
    if ( !YY_CURRENT_BUFFER ) {
        yyterminate();
    }
}  /* End of code supporting import. */

[`][^`\n]*[`] {
        char* temp = strndup(yytext + 1, strlen(yytext) - 2);
        char* inp = (char*)(calloc(sizeof(char*), strlen(temp) + strlen(subshellEnd) + 1));
        strcpy(inp, temp);
        strncat(inp, subshellEnd, strlen(subshellEnd));
 
        int totalEntries = 2048;
        char* buf = (char*)(calloc(sizeof(char), totalEntries));
        
        int defaultin = dup(0);
        int defaultout = dup(1);

        int pToC[2];
        int cToP[2];

        if(pipe(pToC) == -1 || pipe(cToP) == -1) {
            perror("ERROR IN: piping for subshell");
            exit(1);
        }

        int pid = fork();

        if(pid == 0) {
            dup2(pToC[0], 0);
            dup2(cToP[1], 1);
            
            close(pToC[1]);
            close(cToP[0]);
            
            close(defaultin);
            close(defaultout);
            char* argv[2];
            argv[0] = strdup("shell");
            argv[1] = NULL;
            execvp(pathToShell, argv);  
        } else {
            write(pToC[1], inp, strlen(inp));
            dup2(cToP[0], 0);

            close(pToC[0]);
            close(pToC[1]);
            close(cToP[1]);

            waitpid(pid, NULL, 0);
        }
        
        char* ptr = buf;
        char c;
        int t = 0;       
        while((c = getchar()) != EOF) {
                *ptr = c;
                ptr++;
                if(t == totalEntries - 1) {
                        totalEntries *= 2;
                        buf = (char*)(realloc(buf, totalEntries));
                }
                t++;
        }
        char* s = strstr(buf, "\nGood bye!!");
        *s = '\0';
        
        dup2(defaultout, 1);
        dup2(defaultin, 0);
        //printf("buf:%s:\n", buf);
        close(defaultin);
        close(defaultout);
        s--;
        while(s >= buf) {
                if(*s == '\n') {
                        myunputc(' ');
                } else {
                        myunputc(*s);
                }
                s--;
        }

}

\n 	{
		return NEWLINE;
	}

[;] {
        return NEWLINE;
}

[ \t] 	{
		/* Discard spaces and tabs */
     	}

[>]{2}[&]{1} {
        return APPEND_STDO_STDE;
}

[>]{1}[&]{1} {
        return GREAT_STDO_STDE;
}

[>]{2} {
        return APPEND;
}

[>]{1} 	{
		return GREAT;
	}

[<]{1} {
        return LESS;
}

[|]{1} {
        return PIP;
}

[&]{1} {
        return AMP;
}

["][^"]*["] {
        yylval.string_val = strndup(yytext + 1, strlen(yytext) - 2);
        return WORD;
}


[^ \t\n][^ \t\n]*  {
		/* Assume that file names have only alpha chars */
        char* inp = (char*)(calloc(sizeof(char), strlen(yytext) * 2));
        int rewind = 0;
        char* ptr = inp;
        char* p = yytext;
        while(*p != NULL) {
                if((*p == '>' || *p == '<' || *p == '&' || *p == '|') && *(p-1) != '\\') {
                        *ptr = ' ';
                        ptr++;
                        *ptr = *p;
                        ptr++;
                        *ptr = ' ';
                        ptr++;
                        rewind = 1;
                } else {
                        *ptr = *p;
                        ptr++;
                }
                p++;
        }
        *ptr = '\0';
        ptr--;
		if(rewind) {
            while(ptr >= inp) {
                myunputc(*ptr);
                ptr--;
            }
        } else {
            yylval.string_val = strdup(yytext);
		    return WORD;
        }
	}

