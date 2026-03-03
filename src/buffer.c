#include "buffer.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

buffer* newBuf(void) {
    buffer* buf = malloc(sizeof(buffer));
    if (!buf) return NULL;
    buf->capacity = 1;
    buf->rows = malloc(sizeof(row));
    if (!buf->rows) { free(buf); return NULL; }
    buf->rows[0].length = 0;
    buf->rows[0].line = malloc(1);
    if (!buf->rows[0].line) { free(buf->rows); free(buf); return NULL; }
    buf->rows[0].line[0] = '\0';
    buf->numrows = 1;
    return buf;
}

buffer* fileToBuf(FILE* f) {
    if (!f) return NULL;
    buffer* buf = malloc(sizeof(buffer));
    if (!buf) return NULL;
    buf->rows = NULL; buf->numrows = 0; buf->capacity = 0;
    char* line = NULL; size_t len = 0; long int nread;
    while ((nread = fileGetline(&line, &len, f)) != -1) {
        if (nread > 0 && line[nread-1] == '\n') { line[nread-1] = '\0'; nread--; }
        if (buf->numrows == buf->capacity) {
            int newCap = buf->capacity ? buf->capacity * 2 : 16;
            row* tmp = realloc(buf->rows, newCap * sizeof(row));
            if (!tmp) { free(line); freeBuf(buf); return NULL; }
            buf->rows = tmp; buf->capacity = newCap;
        }
        buf->rows[buf->numrows].line = malloc(nread + 1);
        if (!buf->rows[buf->numrows].line) { free(line); freeBuf(buf); return NULL; }
        memcpy(buf->rows[buf->numrows].line, line, nread + 1);
        buf->rows[buf->numrows].length = nread;
        buf->numrows++;
    }
    free(line);
    if (buf->numrows == 0) {
        buf->capacity = 1;
        buf->rows = malloc(sizeof(row));
        if (!buf->rows) { free(buf); return NULL; }
        buf->rows[0].length = 0;
        buf->rows[0].line = malloc(1);
        if (!buf->rows[0].line) { free(buf->rows); free(buf); return NULL; }
        buf->rows[0].line[0] = '\0';
        buf->numrows = 1;
    }
    return buf;
}

FILE* bufToFile(buffer* buf) {
    FILE* f = tmpfile();
    if (!buf || !f) return NULL;
    for (int i = 0; i < buf->numrows; i++) {
        fwrite(buf->rows[i].line, 1, buf->rows[i].length, f);
        fputc('\n', f);
    }
    rewind(f);
    return f;
}

void insertChar(row* r, int at, char c) {
    if (!r) return;
    if (at < 0) at = 0;
    if (at > r->length) at = r->length;
    char* temp = realloc(r->line, r->length + 2);
    if (!temp) return;
    r->line = temp;
    memmove(&r->line[at+1], &r->line[at], r->length - at + 1);
    r->line[at] = c;
    r->length++;
}

void deleteChar(buffer* buf, int rowIndex, int at) {
    row* r = &buf->rows[rowIndex];
    if (!r) return;
    if (at < 0) { deleteCR(buf, rowIndex); return; }
    if (at >= r->length) return;
    memmove(&r->line[at], &r->line[at+1], r->length - at);
    r->length--;
    char* tmp = realloc(r->line, r->length + 1);
    if (tmp) r->line = tmp;
}

void insertCR(buffer* buf, int rowIndex, int at) {
    if (!buf || rowIndex < 0 || rowIndex >= buf->numrows) return;
    if (at < 0) at = 0;
    row* r = &buf->rows[rowIndex];
    if (at > r->length) at = r->length;
    int rightLen = r->length - at;
    row newRow;
    newRow.length = rightLen;
    newRow.line = malloc(newRow.length + 1);
    if (!newRow.line) return;
    memcpy(newRow.line, r->line + at, newRow.length);
    newRow.line[newRow.length] = '\0';
    r->length = at; r->line[at] = '\0';
    char *trimmed = realloc(r->line, at + 1);
    if (trimmed) r->line = trimmed;
    if (buf->numrows == buf->capacity) {
        int newCapacity = buf->capacity ? buf->capacity * 2 : 16;
        row* temp = realloc(buf->rows, newCapacity * sizeof(row));
        if (!temp) { free(newRow.line); return; }
        buf->rows = temp; buf->capacity = newCapacity;
    }
    memmove(&buf->rows[rowIndex+2], &buf->rows[rowIndex+1], (buf->numrows - rowIndex - 1) * sizeof(row));
    buf->rows[rowIndex+1] = newRow;
    buf->numrows++;
}

void deleteCR(buffer* buf, int rowIndex) {
    if (!buf || rowIndex <= 0 || rowIndex >= buf->numrows) return;
    row* prev = &buf->rows[rowIndex-1];
    row* curr = &buf->rows[rowIndex];
    int oldLen = prev->length;
    char* temp = realloc(prev->line, prev->length + curr->length + 1);
    if (!temp) return;
    prev->line = temp;
    memcpy(prev->line + oldLen, curr->line, curr->length + 1);
    prev->length += curr->length;
    free(curr->line);
    memmove(&buf->rows[rowIndex], &buf->rows[rowIndex+1], (buf->numrows - rowIndex - 1) * sizeof(row));
    buf->numrows--;
}

Position insertText(buffer *buf, int rowIndex, int col, const char *text) {
    Position pos;
    pos.row = rowIndex;
    pos.col = col;
    if (!buf || !text) return pos;
    for (const char *p = text; *p != '\0'; p++) {
        if (*p == '\n') {
            insertCR(buf, pos.row, pos.col);
            pos.row++;
            pos.col = 0;
        } else {
            if (pos.row < buf->numrows) {
                insertChar(&buf->rows[pos.row], pos.col, *p);
                pos.col++;
            }
        }
    }
    return pos;
}

void deleteTextRange(buffer *buf, Position start, int len) {
    if (!buf || len <= 0) return;
    int r = start.row;
    int c = start.col;
    for (int i = 0; i < len; i++) {
        if (r >= buf->numrows) break;
        if (c >= buf->rows[r].length) {
            if (r + 1 < buf->numrows) {
                deleteCR(buf, r + 1);
            } else {
                break;
            }
        } else {
            deleteChar(buf, r, c);
        }
    }
}

void freeBuf(buffer* buf) {
    if (!buf) return;
    for (int i = 0; i < buf->numrows; i++) free(buf->rows[i].line);
    free(buf->rows);
    free(buf);
}

long int fileGetline(char **lineptr, size_t *n, FILE *stream) {
    if (!lineptr || !n || !stream) return -1;
    if (*lineptr == NULL || *n == 0) {
        *n = 128;
        *lineptr = malloc(*n);
        if (!*lineptr) return -1;
    }
    size_t pos = 0;
    while (1) {
        int c = fgetc(stream);
        if (c == EOF) { if (pos == 0) return -1; break; }
        if (pos + 1 >= *n) {
            size_t newSize = *n * 2;
            char *temp = realloc(*lineptr, newSize);
            if (!temp) return -1;
            *lineptr = temp; *n = newSize;
        }
        (*lineptr)[pos++] = (char)c;
        if (c == '\n') break;
    }
    (*lineptr)[pos] = '\0';
    return (long int)pos;
}

void printBuf(buffer* buf) {
    for (int i = 0; i < buf->numrows; i++) printf("%s\n", buf->rows[i].line);
}