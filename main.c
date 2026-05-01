#include <errno.h>
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

FILE* openFile (char* filename, char* modes) {
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
    size_t contentLen;

    while ((contentLen = getline(&l, &cap, fd)) != (size_t)-1) {
        if (contentLen > 0 && l[contentLen-1] == '\n') l[contentLen-1] = '\0';

        Line *tmp = realloc(fileContent->lineArray, sizeof(Line) * (fileContent->lineCount + 1u));
        if(!tmp) {
            fprintf(stderr, "Error allocating memory for buffer: %s\n", strerror(errno));
            abort();
        }

        fileContent->lineArray = tmp;

        fileContent->lineArray[fileContent->lineCount].len = contentLen - 1u;
        fileContent->lineArray[fileContent->lineCount].string = strdup(l);
        fileContent->lineCount++;
    }
    
    free(l);
    return fileContent;
}

void printContent (FileContent* file, char *buf, size_t bufSize) {

    size_t len = 0;

    for (size_t i = 1; i <= file->lineCount; i++) {
        len += (size_t)snprintf(buf+len, bufSize - len, "%s\r\n", file->lineArray[i-1].string);
    }
    len += (size_t)snprintf(buf + len, bufSize - len, "\x1b[H");
    write(STDOUT_FILENO, buf, len);

    return;
}

void handleMove (FileContent* file, char *key, size_t *x, size_t *y, char *buf, size_t bufSize) {
    if (*key == '\x1b') {
            char seq[2];
            size_t len = 0;
            


            // If the first byte or second is not 1 (zero or corrupted) we return - Safety check
            if (read(STDIN_FILENO, &seq[0], 1) != 1) return;
            if (read(STDIN_FILENO, &seq[1], 1) != 1) return;
            
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
                            *x = file->lineArray[(*y)-1].len > file->lineArray[(*y)].len && *x == file->lineArray[(*y)-1].len ? file->lineArray[(*y)].len : *x;
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
                
                // Clear screen
                len += (size_t)snprintf(buf + len, bufSize - len, "\x1b[2J\x1b[H");

                // Print file content
                for (size_t i = 1; i <= file->lineCount; i++) {
                    len += (size_t)snprintf(buf+len, bufSize - len, "%s\r\n", file->lineArray[i-1].string);
                }

                // Move cursor after input
                len += (size_t)snprintf(buf + len, bufSize - len, "\x1b[%d;%dH", (int)((*y)+(size_t)1), (int)((*x)+(size_t)1));
                // len += snprintf(screen + len, sizeof(screen) - len, "\x1b[%d;%dH\n\n\ny value: %d", y + 1, x + 1, y);

                // Now re-render
                write(STDOUT_FILENO, buf, len);
            }
        }   
}

int main () {

    // Enable terminal raw mode
    enableRawMode();

    // Open a file
    FILE *fd = openFile("./File.txt", "r");

    // Load content of file in memory ready to be parsed
    FileContent *content = loadContent(fd);

    char screen[4096];
    printContent(content, screen, sizeof(screen));

    size_t x = 0;
    size_t y = 0;
    

    while (1) {
        char c;
        if (read(STDIN_FILENO, &c, 1) != 1) {
            fprintf(stderr, "Error reading input: %s\n", strerror(errno));
            abort();
        }
        handleMove(content, &c, &x, &y, screen, sizeof(screen));
        // \x1b is ESC
           
    }

    // Close the file
    free(content);
    fclose(fd);

    return 0;
}