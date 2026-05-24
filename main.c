#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    uint8_t file_open_t; // Number of (t)times file is opened
    FILE *file_ptr;
    size_t file_size;
    size_t line_size;
    size_t line_no;
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
    if (f_struct-> file_open_t == 0) free(f_struct);
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

size_t* file_get_lines_and_size (FILE* file) {
    char *l = 0;
    size_t n = 0;
    size_t ln = 0;
    size_t ret = 0;
    // stats[0] holds size stats[1] holds lines
    size_t *stats = malloc(2*(sizeof(size_t*)));
    if (stats == NULL) {
        perror("Memory allocation failed during file_get_lines_and_size()");
        exit(1);
    }
    stats[0] = 0;
    stats[1] = 0;

    while((ret = getline(&l, &n, file)) != -1) {
        stats[0] += ret;
        stats[1]++;
    }
    rewind(file);

    return stats;
}

/* 
Safely initialize struct FileStruct 
First allocates memory to represent the struct.
Second sets values to non-garbage
Lastly allocates memory for directly accessible buffer for text and array of size_t 
*/
FileStruct* file_struct_init(FILE *file) {
    FileStruct *f_struct = malloc(sizeof(FileStruct));
    if (f_struct == NULL) {
        printf("Something went wrong when allocating memory for lines..");
        exit(1);
    }
    size_t *f_stats = file_get_lines_and_size(file);
    f_struct->file_open_t = 0;
    f_struct->file_ptr = file;
    f_struct->file_size = f_stats[0];
    f_struct->line_size = f_stats[0]+f_stats[1];
    f_struct->line_no = f_stats[1];
    f_struct->line = malloc(f_struct->line_size + f_struct->line_no);
    f_struct->line_offset = malloc(sizeof(size_t*) * f_struct->line_no);

    if (f_struct->line == NULL || f_struct->line_offset == NULL) {
        printf("Something went wrong when allocating memory for lines..");
        free(f_struct->line);
        free(f_struct->line_offset);
        free(f_struct);
        free(f_stats);
        exit(1);
    }

    *f_struct->line_offset = 0;
    file_track_open(f_struct);

    free(f_stats);

    return f_struct;
}

FileStruct* file_to_struct(FILE *file) {
    
    FileStruct *f_struct = file_struct_init(file);
    size_t alloc_bytes = 0;
    size_t ret = 0;
    size_t offset = 0;
    size_t i = 0;
    char *l = NULL;

    while((ret = getline(&l, &alloc_bytes, file)) != -1) {
        memcpy(&f_struct->line[offset], l, ret+1);
        f_struct->line_offset[i] = offset;
        offset += ret+1;
        ++i;
    }

    free(l);
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

    printf("Line %s", &f->line[f->line_offset[4]]);
    fflush(stdout);

    free(f);

    return 0;
}