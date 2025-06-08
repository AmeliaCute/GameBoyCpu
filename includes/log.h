#ifndef LOG_H
    #define LOG_H

    #define LOG logMessage
    
int  logInit(void);
void logFree(void);
void logMessage(const char *message, ...);

#endif // !LOG_H