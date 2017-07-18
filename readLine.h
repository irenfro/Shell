#ifndef readLine_h
#define readLine_h
int getPrompt();
void setPrompt(int);
char* getHistoryCmmd();
char* readLine();
void resetLine();
void tty_raw_mode();
void unset_tty_raw_mode();
#endif
