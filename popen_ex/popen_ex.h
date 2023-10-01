#ifndef GOOFER_POPEN_EXTEND_H
#define GOOFER_POPEN_EXTEND_H


#include <cstddef>
#include <cstdio>

FILE * goofer_popen(const char * command, const char * mode, size_t & child);
bool goofer_palive(size_t child);
void goofer_pkill(size_t child);
int goofer_pclose(FILE * file, size_t child);


#endif // GOOFER_POPEN_EXTEND_H
