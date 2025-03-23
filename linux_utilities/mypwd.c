#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <limits.h>
#include <stdlib.h>

#define MAX_PATH 500 /* max number of character for the path */

#ifndef PATH_MAX
    #define PATH_MAX 4096
#endif

int main()
{
    int max_path_sz = PATH_MAX;
    char *path = (char *)malloc(MAX_PATH);
    if (getcwd(path, MAX_PATH) == NULL)
    {
        if (errno == ERANGE)
        { // path exceed the allocated size. reallocate memory
            path = (char *)realloc(path, max_path_sz);
            if (getcwd(path, max_path_sz) == NULL)
            {
                perror("the path name is too long");
                return -1;
            }
        }

    }

    printf("%s\n", path);
    free(path);

    return 0;
}