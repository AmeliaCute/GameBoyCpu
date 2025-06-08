#include "../includes/log.h"

#define LOG_BUFFER_SIZE 1024
#define LOG_MAX_LEN 512

#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct 
{
    char msg[LOG_MAX_LEN];
} logItem;

static logItem queue[LOG_BUFFER_SIZE];
static int head = 0, tail = 0;
static int running = 0;

static pthread_mutex_t  logMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t   queueCond = PTHREAD_COND_INITIALIZER;
static pthread_t logThread;

static void *logThreadFunc(void *arg)
{
    (void)arg; 
    while(1)
    {
        pthread_mutex_lock(&logMutex);
        while(head == tail && running)
            pthread_cond_wait(&queueCond, &logMutex);

        if(head == tail && !running)
        {
            pthread_mutex_unlock(&logMutex);
            break; // Exit thread if not running
        }

        logItem item = queue[tail];
        tail = (tail + 1) % LOG_BUFFER_SIZE;
        pthread_mutex_unlock(&logMutex);

        time_t now = time(NULL);
        struct tm *tm_info = localtime(&now);
        char timeStr[26];
        strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", tm_info);

        printf("[%s] %s\n", timeStr, item.msg);
    }

    return NULL;
}

int logInit(void)
{
    running = 1;
    if(pthread_create(&logThread, NULL, logThreadFunc, NULL) != 0)
    {
        perror("Failed to create log thread");
        running = 0;
        return 1;
    }

    return 0;
}

void logFree(void)
{
    pthread_mutex_lock(&logMutex);
    running = 0;

    pthread_cond_signal(&queueCond);
    pthread_mutex_unlock(&logMutex);
    pthread_join(logThread, NULL);
}

void logMessage(const char *message, ...)
{
    char buffer[LOG_MAX_LEN];
    va_list args;
    va_start(args, message);
    vsnprintf(buffer, LOG_MAX_LEN, message, args);
    va_end(args);

    pthread_mutex_lock(&logMutex);
    int next = (head + 1) % LOG_BUFFER_SIZE;
    if(next != tail)
    {
        memcpy(queue[head].msg, buffer, LOG_MAX_LEN);
        queue[head].msg[LOG_MAX_LEN - 1] = '\0';
        head = next;
        pthread_cond_signal(&queueCond);
    }

    pthread_mutex_unlock(&logMutex);
}