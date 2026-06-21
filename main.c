#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    size_t cursor_x;
    size_t cursor_y;
    uint8_t file_open_t; // Number of (t)times file is opened
    FILE *file_ptr;
    size_t file_size;
    size_t line_size; // This is a metadata to track buffer size when file is opened or closed
    size_t line_no;
    char *line_buffer; // This is the pointer to the buffer which contains actual file lines
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

/* Function to get metadata as number of lines in file and size of file */
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
    f_struct->cursor_x = 0;
    f_struct->cursor_y = 0;
    f_struct->file_open_t = 0;
    f_struct->file_ptr = file;
    f_struct->file_size = f_stats[0];
    f_struct->line_size = f_stats[0]+f_stats[1];
    f_struct->line_no = f_stats[1];
    f_struct->line_buffer = malloc(f_struct->line_size + f_struct->line_no);
    f_struct->line_offset = malloc(sizeof(size_t*) * f_struct->line_no);

    if (f_struct->line_buffer == NULL || f_struct->line_offset == NULL) {
        printf("Something went wrong when allocating memory for lines..");
        free(f_struct->line_buffer);
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

/* Safely map opened file to struct with additional metadata 
in order to manipulate file later */
FileStruct* file_to_struct(FILE *file) {
    
    FileStruct *f_struct = file_struct_init(file);
    size_t alloc_bytes = 0;
    size_t ret = 0;
    size_t offset = 0;
    size_t i = 0;
    char *l = NULL;

    while((ret = getline(&l, &alloc_bytes, file)) != -1) {
        memcpy(&f_struct->line_buffer[offset], l, ret+1);
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

// size_t* pos_calculator(FileStruct *f_struct, char *key) {
//     if (*key == '')
// }

void struct_print(FileStruct* f_struct) {
    size_t n_lines = f_struct->line_no;
    for (int i = 0; i < n_lines; ++i) {
        printf("%s", &f_struct->line_buffer[f_struct->line_offset[i]]);
    }
    printf("\n");
    printf("\033[%d;%dH", 1, 1);

    return;
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

/* Instanciate a buffer for editing content of file. */
// char* load_buf(FileStruct* f_struct) {
//     char *edit_buffer = malloc(f_struct->line_size);
//     if (edit_buffer == NULL) {
//         printf("Something went wrong when opening the file..");
//         exit(1);
//     }

//     memcpy(edit_buffer, f_struct->line_buffer, f_struct->line_size);
//     if (edit_buffer == NULL)
//         exit(1);

//     return edit_buffer;
// }

int main () {
    FileStruct *f = file_open("./File.txt", "r");

    // printf("Line %s", &f->line_buffer[f->line_offset[4]]);
    // fflush(stdout);
    struct_print(f);
    int y = 0;
    while((y = getchar()) > 0) {
        if (y == 10) continue;
        if (y == 48) y = 0;
        if (y == 49) y = 1;
        if (y == 50) y = 2;
        if (y == 51) y = 3;
        if (y == 52) y = 4;

        printf("\nPrinted line: %s", &f->line_buffer[f->line_offset[y]]);
    }
    free(f);

    return 0;
}