#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
	int length;
	char* line;
} row;

typedef struct {
	row* rows;
	int numrows;
	int capacity;
} buffer;

buffer* newBuf(void);
buffer* fileToBuf(FILE*);
FILE* bufToFile(buffer*);
void insertChar(row*, int, char);
void deleteChar(buffer*, int, int);
void insertCR(buffer*, int, int);
void deleteCR(buffer*, int);
void freeBuf(buffer* buf);
long int fileGetline(char**, size_t*, FILE*);
void printBuf(buffer*);

int main(void)
{
	buffer* buf1 = newBuf();

	for (int i = 0; i < 26; i++)
		insertChar(&buf1->rows[0], i, 'A' + i);
	printBuf(buf1);
	for (int i = 0; i < 26; i++)
		insertChar(&buf1->rows[0], i, 'a' + i);
	printBuf(buf1);
	for (int i = 0; i < 26; i++)
		deleteChar(buf1, 0, buf1->rows[0].length-1);
	printBuf(buf1);
	for (int i = 0; i < 25; i++)
		insertCR(buf1, i, 1);
	printBuf(buf1);
	for (int i = 0; i < 26; i++)
		deleteCR(buf1, 26-i);
	printBuf(buf1);

	freeBuf(buf1);
	return 0;
}

buffer* newBuf(void)
{
	buffer* buf = malloc(sizeof(buffer));

	buf->capacity = 1;
	buf->rows = malloc(sizeof(row));
	buf->rows[0].length = 0;
	buf->rows[0].line = malloc(1);
	buf->rows[0].line[0] = '\0';
	buf->numrows = 1;

	return buf;
}

buffer* fileToBuf(FILE* f)
{
	if (!f) return NULL;
	buffer* buf = malloc(sizeof(buffer));
	if (!buf) return NULL;

	buf->rows = NULL;
	buf->numrows = 0;
	buf->capacity = 0;

	char* line = NULL;
	size_t len = 0;
	long int nread;

	while ((nread = fileGetline(&line, &len, f)) != -1) {
		if (nread > 0 && line[nread - 1] == '\n') {
			line[nread - 1] = '\0';
			nread--;
		}

		if (buf->numrows == buf->capacity) {
			int newCap = buf->capacity ? buf->capacity * 2 : 16;
			row* tmp = realloc(buf->rows, newCap * sizeof(row));
			if (!tmp) {
				free(line);
				free(buf);
				return NULL;
			}
			buf->rows = tmp;
			buf->capacity = newCap;
		}

		buf->rows[buf->numrows].line = malloc(nread + 1);
		if (!buf->rows[buf->numrows].line) {
			free(line);
			free(buf);
			return NULL;
		}

		memcpy(buf->rows[buf->numrows].line, line, nread + 1);
		buf->rows[buf->numrows].length = nread;
		buf->numrows++;
	}

	free(line);
	if (buf->numrows == 0) {
		buf->capacity = 1;
		buf->rows = malloc(sizeof(row));
		buf->rows[0].length = 0;
		buf->rows[0].line = malloc(1);
		buf->rows[0].line[0] = '\0';
		buf->numrows = 1;
	}
	return buf;
}

FILE* bufToFile(buffer* buf)
{
	FILE* f = tmpfile();
	if (!buf || !f) return NULL;

	for (int i = 0; i < buf->numrows; i++) {
		fwrite(buf->rows[i].line, 1, buf->rows[i].length, f);
		fputc('\n', f);
	}

	rewind(f);
	return f;
}

void insertChar(row* r, int at, char c)
{
	if (!r) return;
	if (at <= 0) at = 0;
	if (at > r->length) at = r->length;

	char* temp = realloc(r->line, r->length + 2);
	if (!temp) return;
	r->line = temp;
	memmove(&r->line[at + 1],
			&r->line[at],
			r->length - at + 1);

	r->line[at] = c;
	r->length++;
}

void deleteChar(buffer* buf, int rowIndex, int at)
{
	row* r = &buf->rows[rowIndex];
	if (!r) return;
	if (at <= 0) {
		deleteCR(buf, rowIndex);
		return;
	}
	if (at >= r->length) return;

	memmove(&r->line[at],
			&r->line[at + 1],
			r->length - at);
	r->length--;

	char* tmp = realloc(r->line, r->length + 1);
	if (tmp) r->line = tmp;
}

void insertCR(buffer* buf, int rowIndex, int at)
{
	if (!buf || rowIndex < 0 || rowIndex >= buf->numrows) return;
	if (at < 0) at = 0;

	row* r = &buf->rows[rowIndex];
	if (at > r->length) at = r->length;
	int rightLen = r->length - at;

	row newRow;
	newRow.length = rightLen;
	newRow.line = malloc(newRow.length + 1);
	if (!newRow.line) return;
	memcpy(newRow.line,
			r->line + at,
			newRow.length);
	newRow.line[newRow.length] = '\0';

	r->length = at;
	r->line[at] = '\0';
	if (buf->numrows == buf->capacity) {
		int newCapacity = buf->capacity ? buf->capacity * 2 : 16;
		row* temp = realloc(buf->rows, newCapacity * sizeof(row));

		if (!temp) {
			free(newRow.line);
			return;
		}

		buf->rows = temp;
		buf->capacity = newCapacity;
	}

	memmove(&buf->rows[rowIndex + 2],
			&buf->rows[rowIndex + 1],
			(buf->numrows - rowIndex - 1) * sizeof(row));

	buf->rows[rowIndex + 1] = newRow;
	buf->numrows++;
}

void deleteCR(buffer* buf, int rowIndex)
{
	if (!buf || rowIndex <= 0 || rowIndex >= buf->numrows) return;

	row* prev = &buf->rows[rowIndex - 1];
	row* curr = &buf->rows[rowIndex];

	int oldLen = prev->length;
	char* temp = realloc(prev->line, prev->length + curr->length + 1);
	if (!temp) return;
	prev->line = temp;

	memcpy(prev->line + oldLen,
			curr->line,
			curr->length + 1);

	prev->length += curr->length;
	free(curr->line);

	memmove(&buf->rows[rowIndex],
			&buf->rows[rowIndex + 1],
			(buf->numrows - rowIndex - 1) * sizeof(row));

	buf->numrows--;
}

void freeBuf(buffer* buf)
{
    if (!buf) return;

    for (int i = 0; i < buf->numrows; i++)
        free(buf->rows[i].line);

    free(buf->rows);
    free(buf);
}

long int fileGetline(char **lineptr, size_t *n, FILE *stream)
{
	if (!lineptr || !n || !stream) return -1;
	if (*lineptr == NULL || *n == 0) {
		*n = 128;
		*lineptr = malloc(*n);
		if (!*lineptr) return -1;
	}
	size_t pos = 0;

	while (1) {
		int c = fgetc(stream);
		if (c == EOF) {
			if (pos == 0) return -1;
			break;
		}

		if (pos + 1 >= *n) {
			size_t newSize = *n * 2;
			char *temp = realloc(*lineptr, newSize);
			if (!temp) return -1;
			*lineptr = temp;
			*n = newSize;
		}

		(*lineptr)[pos++] = (char)c;
		if (c == '\n') break;
	}

	(*lineptr)[pos] = '\0';
	return (long int)pos;
}

void printBuf(buffer* buf)
{
	for (int i = 0; i < buf->numrows; i++) 
		printf("%s\n", buf->rows[i].line);
}
