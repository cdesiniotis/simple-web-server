#ifndef STR_H
#define STR_H

#include <stdlib.h>

typedef struct {
    char *p;		/* Memory block pointer */
    size_t len;		/* Memory block length */	
} str;

void free_str(str *s);
char *skip(char *start, char *end, char *delims, str *s);

#endif /* STR_H */
