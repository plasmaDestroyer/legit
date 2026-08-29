#include <ctype.h>
#include <errno.h>

#include <limits.h>
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <zlib.h>

#define DIR_MODE 0755
#define SHA_HEX_LEN 40

static bool streq(const char *a, const char *b) {
    if (!a || !b) return false;
    return strcmp(a, b) == 0;
}

static bool is_hex_sha(const char *s) {
    if (!s || strlen(s) != SHA_HEX_LEN) return false;
    for (size_t i = 0; i < SHA_HEX_LEN; i++) if (!isxdigit((unsigned char)s[i])) return false;
    return true;
}

static int make_dir(const char *path) {
    if (mkdir(path, DIR_MODE) == 0) return 0;
    if (errno == EEXIST) {
        struct stat st;
        if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) return 0;
    }
    fprintf(stderr, "Failed to create %s: %s\n", path, strerror(errno));
    return 1;
}

static int command_init(void) {
    fprintf(stderr, "Logs from your program will appear here!\n");

    if (make_dir(".git") != 0) return 1;
    if (make_dir(".git/objects") != 0) return 1;
    if (make_dir(".git/refs") != 0) return 1;
    if (make_dir(".git/refs/heads") != 0) return 1;


    FILE *head_file = fopen(".git/HEAD", "w");
    if (!head_file) {
        fprintf(stderr, "Failed to create .git/HEAD: %s\n", strerror(errno));
        return 1;
    }

    if (fprintf(head_file, "ref: refs/heads/main\n") < 0) {
        fclose(head_file);
        return 1;
    }
    if (fclose(head_file) != 0) {
        fprintf(stderr, "Error closing .git/HEAD: %s\n", strerror(errno));
        return 1;
    }

    printf("Initialized git directory\n");
    return 0;
}


static int command_cat_file(const char *flag, const char *blob_sha) {
    if (!streq(flag, "-p")) {
        fprintf(stderr, "Usage: ./your_program.sh cat-file -p <blob_sha>\n");
        return 1;
    }

    if (!is_hex_sha(blob_sha)) {
        fprintf(stderr, "Invalid sha: %s\n", blob_sha ? blob_sha : "(null)");
        return 1;
    }

    char file_path[PATH_MAX];

    int n = snprintf(file_path, sizeof(file_path), ".git/objects/%.2s/%s", blob_sha, (blob_sha + 2));
    if (n < 0 || (size_t)n >= sizeof(file_path)) {
        fprintf(stderr, "Path too long\n");
        return 1;
    }

    FILE *file = fopen(file_path, "rb");
    if (!file) {
        fprintf(stderr, "Error opening %s: %s\n", file_path, strerror(errno));
        return 1;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fprintf(stderr, "fseek failed: %s\n", strerror(errno));
        fclose(file);
        return 1;
    }
    long sz = ftell(file);
    if (sz < 0) {
        fprintf(stderr, "ftell failed: %s\n", strerror(errno));
        fclose(file);
        return 1;
    }
    if (sz == 0) {
        fprintf(stderr, "Empty object file\n");
        fclose(file);
        return 1;
    }
    size_t file_size = (size_t)sz;
    if (fseek(file, 0, SEEK_SET) != 0) {
        fprintf(stderr, "fseek failed: %s\n", strerror(errno));
        fclose(file);
        return 1;
    }

    unsigned char *buf = (unsigned char *)malloc(file_size);
    if (!buf) {
        fprintf(stderr, "Failed to allocate memory\n");
        fclose(file);
        return 1;
    }

    size_t bytes_read = fread(buf, 1, file_size, file);
    if (bytes_read != file_size) {
        fprintf(stderr, "Error reading file: %s\n", ferror(file) ? strerror(errno) : "short read");
        free(buf);
        fclose(file);
        return 1;
    }

    fclose(file);

    size_t decompressed_cap = file_size * 4 + 64;
    if (decompressed_cap < 1024) decompressed_cap = 1024;

    unsigned char *decompressed_data = NULL;
    int zret = Z_BUF_ERROR;
    while (zret == Z_BUF_ERROR) {
        unsigned char *tmp = realloc(decompressed_data, decompressed_cap);
        if (!tmp) {
            free(buf);
            free(decompressed_data);
            return 1;
        }
        decompressed_data = tmp;
        unsigned long decompressed_size = decompressed_cap;

        zret = uncompress(decompressed_data, &decompressed_size, buf, file_size);
        if (zret == Z_BUF_ERROR) {
            decompressed_cap *= 2;
            if (decompressed_cap > 100*1024*1024) {
                free(buf);
                free(decompressed_data);
                return 1;
            }
            continue;
        }
        if (zret != Z_OK) {
            fprintf(stderr, "uncompress failed: %s\n", zError(zret));
            free(buf);
            free(decompressed_data);
            return 1;
        }

        unsigned char *header_end = memchr(decompressed_data, '\0', decompressed_size);
        if (!header_end) {
            free(buf);
            free(decompressed_data);
            return 1;
        }

        if (strncmp((char *)decompressed_data, "blob ", 5) != 0) {
            free(buf);
            free(decompressed_data);
            return 1;
        }

        unsigned char *content = header_end + 1;

        size_t content_len = (size_t)decompressed_size - (header_end + 1 - decompressed_data);
        if (fwrite(content, 1, content_len, stdout) != content_len) {
            perror("fwrite");
            free(buf);
            free(decompressed_data);
            return 1;
        }
        break;
    }

    free(buf);
    free(decompressed_data);
    return 0;
}

int main(int argc, char *argv[]) {
    // Disable output buffering
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    if (argc < 2) {
        fprintf(stderr, "Usage: ./your_program.sh <command> [<args>]\n");
        return EXIT_FAILURE;
    }

    const char *command = argv[1];

    if (streq(command, "init")) {
        if (argc != 2) return EXIT_FAILURE;
        return command_init() ? EXIT_FAILURE : EXIT_SUCCESS;

    } else if (streq(command, "cat-file")) {
        if (argc != 4) return EXIT_FAILURE;
        return command_cat_file(argv[2], argv[3]) ? EXIT_FAILURE : EXIT_SUCCESS;

    } else {
        fprintf(stderr, "Unknown command %s\n", command);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
