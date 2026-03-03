#ifndef BUFFER_H
#define BUFFER_H

#include <stdio.h>
#include "action_stack.h"   // provides Position

typedef struct {
    int length;
    char *line;
} row;

typedef struct {
    row *rows;
    int numrows;
    int capacity;
} buffer;

buffer*  newBuf(void);
buffer*  fileToBuf(FILE *f);
FILE*    bufToFile(buffer *buf);

void     insertChar(row *r, int at, char c);
void     deleteChar(buffer *buf, int rowIndex, int at);
void     insertCR(buffer *buf, int rowIndex, int at);
void     deleteCR(buffer *buf, int rowIndex);

// Bulk insert: returns position after last inserted character.
Position insertText(buffer *buf, int rowIndex, int col, const char *text);
// Bulk delete: removes `len` logical chars (newlines count as 1) from start.
void     deleteTextRange(buffer *buf, Position start, int len);

void     freeBuf(buffer *buf);
long int fileGetline(char **lineptr, size_t *n, FILE *stream);
void     printBuf(buffer *buf);

#endif // BUFFER_H