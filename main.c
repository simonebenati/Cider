#include <errno.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>

static struct termios orig_termios;

void disableRawMode(void) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

void enableRawMode(void) {
    if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) {
        perror("tcgetattr");
        exit(1);
    }

    atexit(disableRawMode);

    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON);
    raw.c_iflag &= ~(IXON | ICRNL);
    raw.c_oflag &= ~(OPOST);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
        perror("tcsetattr");
        exit(1);
    }
}

typedef struct {
    size_t len;
    char *string;
} Line;

typedef struct {
    size_t lineCount;
    Line *lineArray;
} FileContent;

size_t getTermSize(const FileContent *file) {
    size_t fileSize = 1;
    for (size_t i = 0; i < file->lineCount; i++) {
        fileSize += file->lineArray[i].len+2;
    }
    return fileSize;
}

size_t getFileSize(const FileContent *file) {
    size_t size = 1;
    for (size_t i = 0; i < file->lineCount; i++) {
        size += file->lineArray[i].len;

        if (i + 1 != file->lineCount)
            size += 1;
    }
    return size;
}

FILE* openFile (const char* filename, char* modes) {
    FILE *fd = fopen(filename, modes);
    if (fd == NULL) {
        fprintf(stderr, "Error opening file: %s\n", strerror(errno));
        abort();
    }
    return fd;
}

FileContent* loadContent (FILE* fd) {
    // Push File lines into an array of buffers
    FileContent *fileContent = malloc(sizeof(FileContent));
    if (!fileContent) {
        fprintf(stderr, "Error allocating memory for buffer: %s\n", strerror(errno));
        abort();
    }

    fileContent->lineArray = NULL;
    fileContent->lineCount = 0;
    
    char *l = NULL;
    size_t cap = 0;
    ssize_t contentLen;

    while ((contentLen = getline(&l, &cap, fd)) != (ssize_t)-1) {
        if (contentLen > 0 && l[contentLen-1] == '\n') l[contentLen-1] = '\0';

        Line *tmp = realloc(fileContent->lineArray, sizeof(Line) * (fileContent->lineCount + 1u));
        if(!tmp) {
            fprintf(stderr, "Error allocating memory for buffer: %s\n", strerror(errno));
            abort();
        }

        fileContent->lineArray = tmp;

        fileContent->lineArray[fileContent->lineCount].len = strlen(l);
        fileContent->lineArray[fileContent->lineCount].string = strdup(l);
        fileContent->lineCount++;
    }
    
    free(l);
    return fileContent;
}

ssize_t safeAppend (char **buf, size_t *size, size_t *pos, const char *format, ...) {
    va_list args, cp;
    va_start(args, format);
    va_copy(cp, args);

    int n = 0;
    n = vsnprintf(*buf + *pos, *size - *pos, format, args);
    if (n < 0) {
        free(*buf);
        *buf = NULL;
        va_end(args);
        va_end(cp);
        return -1;
    }
    size_t need = (size_t)n;
    // Bytes to write bigger than actual buf
    if (need >= *size - *pos) {
        void *tmp = NULL;
        // To avoid theoretical overflow of size_t (wraparound)
        while (*size - *pos <= need) {
            *size *= 2;
            tmp = realloc(*buf, *size);
            if (tmp == NULL) {
                free(*buf);
                *buf = NULL;
                va_end(args);
                va_end(cp);
                return -1;
            };
            *buf = tmp;
        }
        n = vsnprintf(*buf + *pos, *size - *pos, format, cp);
        if (n < 0) {
            free(*buf);
            *buf = NULL;
            va_end(args);
            va_end(cp);
            return -1;
        }
        need = (size_t)n;
    }

    *pos += need;
    va_end(args);
    va_end(cp);
    return 1;
}

size_t printContent (FileContent* file, size_t *xCord, size_t *yCord) {
    size_t pos = 0;
    size_t size = 50;

    char *buf = malloc(size);
    if (buf == NULL) return 1;

    ssize_t ret = safeAppend(&buf, &size, &pos, "\x1b[2J\x1b[H");
    if (ret == -1) {
        return 1;
    }

    // Go over each file line and add them to buffer
    for (size_t i = 1; i <= file->lineCount; i++) {
        ret = safeAppend(&buf, &size, &pos, "%s\r\n", file->lineArray[i-1].string);
        if (ret == -1) {
            return 1;
        }
    }

    // Escape sequence to place cursor at the beginning
    ret = safeAppend(&buf, &size, &pos, "\x1b[H");
    if (ret == -1) {
        return 1;
    }

    // If x or y are different move appropriately the cursor
    if (*xCord != 0u || *yCord != 0) {

        ret = safeAppend(&buf, &size, &pos, "\x1b[%d;%dH", (int)((*yCord)+(size_t)1), (int)((*xCord)+(size_t)1));
        if (ret == -1) {
            return 1;
        }
    }

    write(STDOUT_FILENO, buf, pos);
    free(buf);

    return size;
}

size_t handleWrite(FileContent *file, FILE *fd, char *key, size_t *x, size_t *y) {
    if (*key != '\x1b') {
        fseek(fd, 0, SEEK_SET);
        file->lineArray[*y].string = realloc(file->lineArray[*y].string, file->lineArray[*y].len+2);
        memmove(&file->lineArray[*y].string[(*x)+1], &file->lineArray[*y].string[(*x)], file->lineArray[*y].len - *x + 1);
          
        file->lineArray[*y].string[(*x)] = *key;
        file->lineArray[*y].len++;

        size_t len = 0;
        size_t size = getTermSize(file);
        char *fileBuf = malloc(size);
        if (fileBuf == NULL) return 1;
        //Write to file
         for (size_t i = 0; i < file->lineCount; i++) {
            if (i+1 == file->lineCount)
                safeAppend(&fileBuf, &size, &len, "%s", file->lineArray[i].string);
            else
                safeAppend(&fileBuf, &size, &len, "%s\n", file->lineArray[i].string);
        }
        
        (*x)++;
        fwrite(fileBuf, 1, strlen(fileBuf), fd);
        fflush(fd);
    }
    return printContent(file, x, y);
}

size_t handleMove (FileContent* file, char *key, size_t *x, size_t *y, char *buf) {
    if (*key == '\x1b') {
            char seq[2];
            
            // Did input succeed? if the first or second read didn't return 1 we exit - Safety check
            if (read(STDIN_FILENO, &seq[0], 1) != 1) return 1;
            if (read(STDIN_FILENO, &seq[1], 1) != 1) return 1;
            
            if (seq[0] == '[') {
                switch (seq[1]) {
                    case 'A': 
                        if ((*y) > 0) {
                            (*y)--; 
                            *x = file->lineArray[(*y)+1].len > file->lineArray[(*y)].len && *x == file->lineArray[(*y)+1].len ? file->lineArray[*y].len : *x;
                        }
                        break;
                    case 'B': 
                        if ((*y) < file->lineCount - 1) {
                            (*y)++;
                            *x = file->lineArray[(*y)-1].len > file->lineArray[(*y)].len && *x == file->lineArray[(*y)-1].len ? file->lineArray[(*y)].len : file->lineArray[(*y)-1].len > file->lineArray[(*y)].len ? file->lineArray[(*y)].len : *x;
                        }
                        break;
                    case 'C': 
                        if ((*x) < file->lineArray[*y].len) {
                            (*x)++; 
                        }
                        break;
                    case 'D': 
                        if (*x > 0) {
                            (*x)--; 
                        }
                        break;
                    default:
                        printf("Unrecognised sequence! Abort...");
                        abort();
                        break;
                }
                
                
            }
        }
        

    return printContent(file, x, y);;
}

int main () {

    // Enable terminal raw mode
    enableRawMode();

    // Open a file
    FILE *fd = openFile("./File.txt", "r+");

    // Load content of file in memory ready to be parsed
    FileContent *content = loadContent(fd);

    char screen[4096];
    // char *fileBuf = NULL;
    size_t x = 0;
    size_t y = 0;

    size_t bufSize =  printContent(content, &x, &y);

    while (1) {
        char c;
        if (read(STDIN_FILENO, &c, 1) != 1) {
            fprintf(stderr, "Error reading input: %s\n", strerror(errno));
            abort();
        }

        // \x1b is ESC
        if (c == '\x1b') 
            handleMove(content, &c, &x, &y, screen);
        else
            handleWrite(content, fd, &c, &x, &y);
            
        
    }

    // Close the file
    free(content);
    fclose(fd);

    return 0;
}