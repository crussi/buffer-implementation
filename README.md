# buffer-implementation

This is just a simple implementation of this data-structure:

```c
typdef struct {
    int length;
    char* line;
} row;

typedef struct {
    row* rows;
    int numrows;
    int capacity;
} buffer;
```

This data-structure is designed to be ver simple, and fundamentally just an array of strings.<br>

There are very few methods:
```c
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
```

Feel free to contribute if you would like to!

> NOTE <br>
> The main function in this program just tests the methods, if you would like to use the buffer data-structure then you should remove the main function.
