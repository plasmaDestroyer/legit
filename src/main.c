#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include <zlib.h>

#define MIN(a, b) (((a) < (b)) ? (a) : (b))

bool equal(const char *s1, const char *s2) { return strncmp(s1, s2, MIN(strlen(s1), strlen(s2))) == 0; }

void command_init() {
    fprintf(stderr, "Logs from your program will appear here!\n");

    if (mkdir(".git", 0755) == -1 || 
        mkdir(".git/objects", 0755) == -1 || 
        mkdir(".git/refs", 0755) == -1) {
        fprintf(stderr, "Failed to create directories: %s\n", strerror(errno));
        exit(1);
    }

    FILE *headFile = fopen(".git/HEAD", "w");
    if (headFile == NULL) {
        fprintf(stderr, "Failed to create .git/HEAD file: %s\n", strerror(errno));
        exit(1);
    }
    fprintf(headFile, "ref: refs/heads/main\n");
    fclose(headFile);

    printf("Initialized git directory\n");
}


void command_cat_file(const char *flag, const char *blob_sha) {
    if (!equal(flag, "-p")) {
        fprintf(stderr, "Usage: ./your_program.sh cat-file -p <blob-sha>\n");
        exit(1);
    }

    char file_path[256];
    snprintf(file_path, sizeof(file_path), ".git/objects/%.2s/%s", blob_sha, (blob_sha + 2));

    FILE *file = fopen(file_path, "rb");
    if (!file) {
        fprintf(stderr, "Error opening file: %s\n", strerror(errno));
        exit(1);
    }

    fseek(file, 0, SEEK_END);
    unsigned long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    unsigned char *buf = (unsigned char *)malloc(file_size);
    if (buf == NULL) {
        fprintf(stderr, "Failed to allocate memory\n");
        exit(1);
    }

    long bytes_read = fread(buf, 1, file_size, file);
    if (bytes_read < file_size) {
        fprintf(stderr, "Error reading file\n");
        exit(1);
    }

    fclose(file);

    unsigned long decompressed_size = 1024 * 1024;
    unsigned char *decompressed_data = (unsigned char *)malloc(decompressed_size);
    if (decompressed_data == NULL) {
        fprintf(stderr, "Failed to allocate memory\n");
        exit(1);
    }

    int result = uncompress(decompressed_data, &decompressed_size, buf, file_size);
    if (result != Z_OK) {
        fprintf(stderr, "Error during decompression\n");
        exit(1);
    }

    char *header_end = memchr(decompressed_data, '\0', decompressed_size);
    if (header_end == NULL) {
        fprintf(stderr, "Error finding Null byte\n");
        exit(1);
    }

    char *content = header_end + 1;

    size_t header_len = (size_t)(content - (char *)decompressed_data);
    size_t content_len = decompressed_size - header_len;

    fwrite(content, 1, content_len, stdout);

    free(buf);
    free(decompressed_data);
}

int main(int argc, char *argv[]) {
    // Disable output buffering
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);

    if (argc < 2) {
        fprintf(stderr, "Usage: ./your_program.sh <command> [<args>]\n");
        return 1;
    }

    const char *command = argv[1];

    if (equal(command, "init")) {
        command_init();

    } else if (equal(command, "cat-file")) {
        command_cat_file(argv[2], argv[3]);

    } else {
        fprintf(stderr, "Unknown command %s\n", command);
        return 1;
    }

    return 0;
}
