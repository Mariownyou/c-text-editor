#ifndef BUFFER_H_
#define BUFFER_H_

#include <stdlib.h>

typedef struct {
    size_t capacity;
    size_t size;
    char *chars;
} Line;

void line_insert_text_after(Line *line, const char *text, size_t col);
void line_backspace(Line *line, size_t col);

#endif
