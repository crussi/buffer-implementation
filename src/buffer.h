#ifndef BUFFER_H
#define BUFFER_H

#include <stdio.h>

// --- Row type ---
typedef struct {
    int length;
    char *line;
} row;

// --- Buffer type ---
typedef struct {
    row *rows;
    int numrows;
    int capacity;
} buffer;

// --- Buffer API ---
buffer* newBuf(void);
buffer* fileToBuf(FILE *f);
FILE* bufToFile(buffer *buf);

void insertChar(row *r, int at, char c);
void deleteChar(buffer *buf, int rowIndex, int at);
void insertCR(buffer *buf, int rowIndex, int at);
void deleteCR(buffer *buf, int rowIndex);

void freeBuf(buffer *buf);
long int fileGetline(char **lineptr, size_t *n, FILE *stream);
void printBuf(buffer *buf);

#endif // BUFFER_H