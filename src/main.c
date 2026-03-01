#include "buffer.h"
#include <stdio.h>

int main(void) {
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