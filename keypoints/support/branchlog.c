#include <stdio.h>
#include <stdlib.h>

void csc512project_log_branch(char* br_tag) {
    FILE *f = fopen("branch_trace.txt", "a");
    fprintf(f, "%s\n", br_tag);
}