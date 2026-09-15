#ifndef COMMONS_H
#define COMMONS_H

#include <stdio.h>

int copy_stream(FILE *in, FILE *out);
int copy_file(const char *prog, const char *path);

#endif /* COMMONS_H */
