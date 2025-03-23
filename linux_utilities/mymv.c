#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/stat.h>
#include <libgen.h>
#include <errno.h>
#include <unistd.h>

#ifndef PATH_MAX
    #define PATH_MAX 4096
#endif


#define SRC_REG_FILE    0x01
#define DEST_REG_FILE   0x02
#define SRC_DIR_FILE    0x04
#define DEST_DIR_FILE   0x08
#define NA              0x00 /* path doesn't exist */

#define FILE_2_FILE     0x03
#define FILE_2_DIR      0x09
#define DIR_2_DIR       0x0C
#define DIR_2_FILE      0x06

#define ERR_UNSUPPORTED_FTYPE(_DIR)         #_DIR " file is not a regular file or directory\n"
#define ERR_PERMISSION_DENIED(_PER, _STR)   #_STR " does not have permission to " #_PER " source file\n"

const struct stat *get_stat(const char *path)
{
    struct stat* path_stat = NULL;
    path_stat = (struct  stat*)malloc(sizeof(struct stat));
    if (path_stat == NULL)
    {
        return NULL;
    }

    if (stat(path, path_stat) != 0)
    {
        return NULL;
    }

    return (const struct stat *)path_stat;
}

void mv_to_file(const char *source_file, const char *destination_file)
{
    // rename should return 0 on success
    if (rename(source_file, destination_file) != 0) 
    {
        perror("Failed to move");
        exit(-1);
    }
}

void mv_to_dir(char *source_file, const char *destination_file)
{
    char new_destination_file[PATH_MAX];
    char *basesrcfile = basename(source_file);
    snprintf(new_destination_file, sizeof(new_destination_file), "%s/%s", destination_file, basesrcfile);

    mv_to_file(source_file, new_destination_file);
}

int main(int argc, char** argv)
{
    if (argc != 3)
    {
        printf("Usage: %s <source> <destination>\n", argv[0]);
        exit(1);
    }

    const char* source_file 	 = argv[1];
    const char* destination_file = argv[2];
    uint32_t source_type = 0;
    uint32_t dest_type   = 0; 
    const struct stat* src_path_stat  = get_stat(source_file);
    if (src_path_stat == NULL)
    {   // source should be exist on the FS
        char err_msg[PATH_MAX];
        snprintf(err_msg, sizeof(err_msg), "cannot stat for '%s'", source_file);
        perror(err_msg);
        exit(-1);
    }

    /* check source file type */
    if (S_ISREG(src_path_stat->st_mode))
    {
        source_type = SRC_REG_FILE;
    }
    else if (S_ISDIR(src_path_stat->st_mode))
    {
        source_type = SRC_DIR_FILE;
    }
    else
    {
        write(STDERR_FILENO, ERR_UNSUPPORTED_FTYPE(SRC), strlen(ERR_UNSUPPORTED_FTYPE(SRC)));
        free((void*)src_path_stat);
        exit(-1);
    }

    const struct stat* dest_path_stat = get_stat(destination_file);
    if (dest_path_stat == NULL)
    {
        // its ok if get_stat returns NULL, it means destination file does not exist
        dest_type = NA;
    }
    else
    {
        /* check destination file type */
        if (S_ISREG(dest_path_stat->st_mode))
        {
            dest_type = DEST_REG_FILE;
        }
        else if (S_ISDIR(dest_path_stat->st_mode))
        {
            dest_type = DEST_DIR_FILE;
        }
        else
        {
            write(STDERR_FILENO, ERR_UNSUPPORTED_FTYPE(DEST), strlen(ERR_UNSUPPORTED_FTYPE(DEST)));
            free((void*)src_path_stat);
            free((void*)dest_path_stat);
            exit(-1);
        }
    }

    /* handle different cases for source and destination file types */
    // 1. source is a regular file and destination is a regular file (exist, overwrite)
    // 2. source is a regular file and destination is a regular file (not exist, create new file and move)
    // 3. source is a regular file and destination file is a directory (cut the file to the dir)
    // 4. source is a directory and destination file is a directory (exist, cut the src dir into the dest dir)
    // 5. source is a directory and destination file is a directory (not exist, create new dir and move)
    switch (source_type | dest_type)
    {
    case FILE_2_FILE:
        mv_to_file(source_file, destination_file);
        break;
    case FILE_2_DIR:
        mv_to_dir((char *)source_file, destination_file);
        break;
    case DIR_2_DIR:
        mv_to_dir((char *)source_file, destination_file);
        break;
    case SRC_REG_FILE:
        mv_to_file(source_file, destination_file);
        break;
    case SRC_DIR_FILE:
        mv_to_file(source_file, destination_file);
        break;
    case DIR_2_FILE:
        char err_msg[50];
        snprintf(err_msg, sizeof(err_msg), "Cannot copy a directory to a regular file\n");
        write(STDERR_FILENO, err_msg, strlen(err_msg));
        break;
    default:
        write(STDERR_FILENO, ERR_UNSUPPORTED_FTYPE(SRC), strlen(ERR_UNSUPPORTED_FTYPE(SRC)));
        break;
    }

    // free resources
    free((void*)src_path_stat);
    if (dest_path_stat != NULL)
        free((void*)dest_path_stat);

    return 0;
}