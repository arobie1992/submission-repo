#include <stdio.h>
#include <stdlib.h>

void csc512project_log_branch(char* br_tag) {
    FILE *f = fopen("branch_trace.txt", "a");
    fprintf(f, "%s\n", br_tag);
    fclose(f);
}

void csc512project_log_fp(void *fp) {
    FILE *f = fopen("branch_trace.txt", "a");
    // cast to get rid of warnings and use long since just int might lose precision
    fprintf(f, "func_0x%lx\n", (long int)fp);
    fclose(f);
}