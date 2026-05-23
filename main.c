#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    uint8_t file_open_t; // Number of (t)times file is opened
    FILE *file_ptr;
    size_t line_counter;
    size_t line_size;
    char *line;
    size_t *line_offset; 
} FileStruct;

void file_track_open(FileStruct *f_struct) {
    if (f_struct->file_open_t >= 255) return;
    f_struct->file_open_t++;
    return;
}

void file_track_close(FileStruct *f_struct) {
    if (f_struct->file_open_t == 0) return;
    f_struct->file_open_t--;
    if (f_struct == 0) free(f_struct);
    return;
}

/* Takes a FILE* object and returns its size safely. */
size_t file_get_size (FILE* file) {
    size_t pos = fseek(file, 0, SEEK_END);
    if (pos != 0) {
        printf("An error occurred while getting file size");
        exit(1);
    }
    ssize_t size = ftell(file);
    if (size == -1) {
        printf("An error occurred while getting file size: %d", errno);
        exit(errno);
    }
    rewind(file);
    return size;
}

FileStruct* file_to_struct(FILE* file) {
    FileStruct *f_struct = malloc(sizeof(FileStruct));
    if (f_struct == NULL) {
        printf("Something went wrong when allocating memory for lines..");
        exit(1);
    }

    f_struct->file_open_t = 0;
    file_track_open(f_struct);
    f_struct->file_ptr = file;
    f_struct->line_counter = 0;
    size_t *l_ctr = &f_struct->line_counter;
    f_struct->line_size = 1024;
    f_struct->line = malloc(f_struct->line_size);
    f_struct->line_offset = malloc(sizeof(size_t));

    size_t alloc_bytes = 0;
    size_t ret = 0;
    size_t offset = 0;
    char *l = NULL;
    while((ret = getline(&l, &alloc_bytes, file)) != -1) {
        if (f_struct->line_size - offset < ret) {
            while(f_struct->line_size - offset < ret) {
                f_struct->line_size *= 2;
                f_struct->line = realloc(f_struct->line, f_struct->line_size);
            }
        }
        memcpy(&f_struct->line[offset], l, ret);
        f_struct->line_offset[*l_ctr] = offset;
        offset += ret+1;
        f_struct->line[offset-1] = '\0';
        (*l_ctr)++;
        f_struct->line_offset = realloc(f_struct->line_offset, sizeof(size_t) * (*l_ctr + 1));
    }

    return f_struct;
}



/* Takes path and permissions to safely open file. 
Exits in case of error.*/
FileStruct* file_open(const char *path, const char *perms) {
    FILE *open_file = fopen(path, perms);
    if (open_file == NULL) {
        printf("Something went wrong when opening the file..");
        exit(1);
    }
    FileStruct *f_struct = file_to_struct(open_file);

    return f_struct;
}

/* Takes a file into input and safely prints its content.
It checks for size first and measures if bytes printed match file dimension. */
void file_print(FILE* file) {
    char *l = NULL;
    size_t file_size = file_get_size(file);
    size_t bytes_read = 0;
    ssize_t ret = 0;
    size_t total_size = 0;
    while((ret = getline(&l, &bytes_read, file)) != -1) {
        printf("%s", l);
        total_size += ret;
    }
    if (total_size > file_size || total_size < file_size) {
        printf("An error occurred while reading file size...");
        exit(1);
    }

    printf("%c", '\n');

    return;
}

int main () {
    FileStruct *f = file_open("./File.txt", "r");
    
    //file_print(f->file_ptr);
    printf("%s", &f->line[f->line_offset[0]]);

    return 0;
}