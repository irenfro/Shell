#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>

#include "readLine.h"

#define MAX_LEN 1024
#define STDIN 0
#define STDOUT 1
#define BACKSPACE 8

struct termios oldtermios; 
int len;
int pos;
char line[MAX_LEN];
char** history = NULL;
int histTotalEntries = 0;
int histCurrEntry = 0;
int histMaxEntries = 16;
int printPrompt;

char READ();
void WRITE(char*, int);
void DELETE(int);
void OVERWRITE(int);
void initHistory();
void addToHistory(char*);
void writeHistory();
void gotoEnd();

char* readLine() {
        if(history == NULL) {
            initHistory();
        }
        len = 0;
        pos = 0;

        memset(&line[0], '\0', sizeof(line));

        while(1) {
                char c = READ();

                if(c == 10) {
                        //add to history here
                        //ENTER was typed
                        if(len > 0) {
                                addToHistory(line);
                                histCurrEntry = histTotalEntries;   
                        } 
                        WRITE(&c, sizeof(char));
                        break;
                } else if(c == 1) {
                        DELETE(pos);
                        pos = 0;
                } else if(c == 4) {
                        if(pos == len) {
                                continue;
                        }
                        int savedPos = pos;
                        gotoEnd();
                        OVERWRITE(len);
                        memmove(line + savedPos, line + savedPos + 1, len - savedPos);
                        len--;
                        line[len] = '\0';
                        WRITE(line, len);
                        pos = savedPos;
                        DELETE(len - savedPos);
                } else if(c == 5) {
                        gotoEnd();
                }else if(c < 127 && c >= 32) {
                        //Readable Characters
                        //if we are at the end of the input so far
                        if(pos == len) {
                            WRITE(&c, sizeof(char));

                            //reached the end of allocated space
                            if(len == MAX_LEN - 1) {
                                    break;
                            }

                            line[pos] = c;
                            len++;
                            pos++;
                        //Somewhere in the middle of the line
                        } else {
                            //use memmove to move memory to the right
                            memmove(line + pos + 1, line + pos, len - pos);
                            line[pos++] = c;
                            len++;
                            int savedPos = pos;
                            gotoEnd();
                            OVERWRITE(len-1);
                            WRITE(line, len);
                            pos = savedPos;
                            DELETE(len - savedPos);
                        }
                } else if(c == 127 || c == 8) {
                        //DELETE Character
                        if(pos <= 0) {
                                pos = 0;
                                continue;
                        } else if(pos < len) {
                            int savedPos = pos;
                            gotoEnd();
                            OVERWRITE(len);
                            memmove(line + savedPos - 1, line + savedPos, len - savedPos);
                            len--;
                            line[len] = '\0';
                            WRITE(line, len);
                            pos = savedPos - 1;
                            DELETE(len - savedPos + 1);
                        } else {
                            OVERWRITE(1);
                            len--;
                            pos--;
                        }
                } else if(c == 27) {
                        //ESCAPE Sequences
                        //up arrow = \027[A
                        //down arrow = \027[B
                        //right arrow = \027[C
                        //left arrow = \027[D
                        
                        char c1 = READ();
                        char c2 = READ();

                        if(c1 == '[') {
                                switch(c2) {
                                        //These involve history
                                        case 'A':
                                                if(histCurrEntry == 0) {
                                                        break;
                                                }
                                                OVERWRITE(len);
                                                histCurrEntry--;
                                                writeHistory();
                                                break;
                                        case 'B':
                                                if(histCurrEntry == histTotalEntries) {
                                                        break;
                                                }
                                                OVERWRITE(len);
                                                histCurrEntry++;
                                                writeHistory();
                                                break;
                                        //End onvolving history
                                        case 'C':
                                                if(pos == len) {
                                                        break;
                                                }
                                                WRITE(&line[pos], sizeof(char));
                                                pos++;
                                                break;
                                        case 'D':
                                                if(pos <= 0) {
                                                        break;
                                                }
                                                pos--;
                                                DELETE(1);
                                                break;
                                }
                        }
                }
        }
        pos = len;
        line[pos++] = '\n';
        len++;
        line[pos] = '\0';
        return line;
}

char READ() {
        char temp;
        read(STDIN, &temp, sizeof(char));
        return temp;
}

void WRITE(char* str, int size) {
        write(STDOUT, str, size);
}

void DELETE(int n) {
        char c = BACKSPACE;
        for(int i = 0; i < n; i++) {
                WRITE(&c, sizeof(char));
        }
}

void OVERWRITE(int n) {
        DELETE(n);
        char c = ' ';
        for(int i = 0; i < n; i++) {
                WRITE(&c, sizeof(char));
        }
        DELETE(n);
}

void initHistory() {
        history = (char**)(calloc(sizeof(char*), histMaxEntries));
}

void addToHistory(char* inp) {
        if(inp == NULL) {
                return;
        }
        char* entry = strdup(inp);
        if(histTotalEntries > 0 && !strcmp(history[histTotalEntries-1], entry)) {
                free(entry);
                return;
        }
        if(histTotalEntries == histMaxEntries) {
                histMaxEntries *= 2;
                history = (char**)(realloc(history, histMaxEntries * sizeof(char*)));
        }
        history[histTotalEntries++] = entry;
}

void writeHistory() {
    memset(&line[0], '\0', sizeof(line));
    if(history[histCurrEntry] == NULL) {
            strcpy(line, "");
    } else {
            strcpy(line, history[histCurrEntry]);
    }
    len = strlen(line);
    pos = len;
    WRITE(line, len);
}

void gotoEnd() {
        while(pos < len) {
            WRITE(&line[pos], sizeof(char));
            pos++;
        }
}

void resetLine() {
        memset(&line[0], '\0', sizeof(line));
        len = 0;
        pos = 0;
}

int getPrompt() {
        return printPrompt;
}

void setPrompt(int n) {
        printPrompt = n;
}

char* getHistoryCmmd() {
        if(histTotalEntries > 1) {
            return strdup(history[histTotalEntries - 2]);
        }
        return NULL;
}

//Save and Restore the terminal state
void tty_raw_mode() {
    /* save defaults for later */
    tcgetattr(0,&oldtermios);        
    termios tty_attr = oldtermios;
    /* set raw mode */
    tty_attr.c_lflag &= (~(ICANON|ECHO));
    tty_attr.c_cc[VTIME] = 0;
    tty_attr.c_cc[VMIN] = 1;
        
    tcsetattr(0,TCSANOW,&tty_attr);
}
    
void unset_tty_raw_mode() {
    tcsetattr(0, TCSAFLUSH, &oldtermios);
}
