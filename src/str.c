#include "str.h"
#include <stdlib.h>
#include <string.h>

// Free a dynamically allocated variable of type str
void free_str(str *s)
{
    char *sp = (char *) s->p;
    s->p = NULL;
    s->len = 0;
    if (sp != NULL) free(sp);
}

// Gets first substring (with delimiters) from the string
// start, end and puts the substring into s
char *skip(char *start, char *end, char *delims, str *s)
{
    s->p = start;
    // Find first instance of delimiter
    while ( start < end && strchr(delims, *start) == NULL) start++;
    // Update length of s
    s->len = start - s->p;
    // Remove any successive delimiter characters
    while ( start < end && strchr(delims, *start) != NULL) start++;
    return start;
}
