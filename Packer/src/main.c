#define _GNU_SOURCE
#include "woody.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

int error(const char *message)
{
    fprintf(stderr, "woody: %s\n", message);
    return 1;
}

static int arguments(int argc, char **argv, const char **input,
                     const char **output)
{
    int options = 1;
    int i;

    *input = NULL;
    *output = NULL;
    for (i = 1; i < argc; ++i) {
        if (options && strcmp(argv[i], "--") == 0)
            options = 0;
        else if (options && strcmp(argv[i], "-o") == 0) {
            if (*output || i + 1 == argc || !argv[i + 1][0])
                break;
            *output = argv[++i];
        } else if ((options && argv[i][0] == '-') || *input || !argv[i][0])
            break;
        else
            *input = argv[i];
    }
    if (i != argc || !*input)
        return error("usage: ./woody <ELF64 program> [-o output_file]");
    return 0;
}

static int output_ok(const char *path)
{
    struct stat st;

    if (lstat(path, &st) == 0)
        return S_ISREG(st.st_mode) ? 0 : error("output must be a regular file");
    if (errno == ENOENT)
        return 0;
    perror(path);
    return 1;
}

static int save(const char *path, const unsigned char *data, size_t size,
                mode_t mode)
{
    char *temporary;
    size_t done = 0;
    ssize_t n;
    int fd;
    int failed = 0;

    if (output_ok(path))
        return 1;
    temporary = malloc(strlen(path) + sizeof(".woody-XXXXXX"));
    if (!temporary)
        return error("cannot allocate temporary path");
    sprintf(temporary, "%s.woody-XXXXXX", path);
    fd = mkstemp(temporary);
    if (fd < 0) {
        perror(path);
        free(temporary);
        return 1;
    }
    while (done < size) {
        n = write(fd, data + done, size - done);
        if (n < 0 && errno == EINTR)
            continue;
        if (n <= 0) {
            failed = 1;
            break;
        }
        done += (size_t)n;
    }
    if (!failed && (fchmod(fd, (mode & 0777) | S_IXUSR) < 0 || fsync(fd) < 0))
        failed = 1;
    if (failed)
        perror(path);
    if (close(fd) < 0) {
        perror(path);
        failed = 1;
    }
    if (!failed && output_ok(path))
        failed = 1;
    if (!failed && rename(temporary, path) < 0) {
        perror(path);
        failed = 1;
    }
    if (failed)
        unlink(temporary);
    free(temporary);
    return failed;
}

static int process(const unsigned char *data, size_t size, const char *output,
                   mode_t mode)
{
    unsigned char *stripped = NULL;
    unsigned char *packed = NULL;
    const unsigned char *chosen = data;
    const char *method = "original";
    size_t stripped_size = size;
    size_t chosen_size = size;
    size_t packed_size;
    t_report report = {0};
    int result;
    int version = already_packed(data);

    if (version == 2)
        puts("[2/4] Already packed and masked by woody; keeping the current image.");
    else if (version == 1)
        puts("[2/4] Already packed without masking; use the original ELF to add it.");
    else {
        stripped = trim_sections(data, size, &stripped_size);
        if (stripped) {
            chosen = stripped;
            chosen_size = stripped_size;
            method = "stripped";
        }
        printf("[2/4] Strip metadata: %zu -> %zu bytes\n", size, chosen_size);
        puts("[3/4] Compressing and masking executable code with LZSS...");
        fflush(stdout);
        if (needs_early_code(data, size))
            report.reason = "pre-entry code or unsupported dynamic metadata; strip only";
        else
            packed = pack_elf(chosen, chosen_size, &packed_size, &report);
        if (report.raw_size)
            printf("      Code stream: %zu -> %zu bytes\n", report.raw_size,
                   report.compressed_size);
        if (packed) {
            chosen = packed;
            chosen_size = packed_size;
            method = "stripped + LZSS + mask";
        } else
            printf("      Keeping smaller input/stripped ELF: %s\n", report.reason);
    }
    printf("[4/4] Writing %s...\n", output);
    result = save(output, chosen, chosen_size, mode);
    if (!result)
        printf("Done: %s\n%zu -> %zu bytes | %.2f%% smaller | %s\n", output,
               size, chosen_size, 100.0 * (1.0 - (double)chosen_size / size), method);
    free(packed);
    free(stripped);
    return result;
}

int main(int argc, char **argv)
{
    const char *input;
    const char *output;
    char *default_output = NULL;
    const unsigned char *data;
    struct stat st;
    int fd;
    int result = 1;

    if (arguments(argc, argv, &input, &output))
        return 1;
    if (!output) {
        default_output = malloc(strlen(input) + sizeof(".packed"));
        if (!default_output)
            return error("cannot allocate output path");
        sprintf(default_output, "%s.packed", input);
        output = default_output;
    }
    printf("[1/4] Reading %s...\n", input);
    fd = open(input, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) {
        perror(input);
        free(default_output);
        return 1;
    }
    if (fstat(fd, &st) < 0)
        perror(input);
    else if (!S_ISREG(st.st_mode) || st.st_size <= 0
        || (uintmax_t)st.st_size > SIZE_MAX)
        error("input must be a nonempty regular file");
    else if (!output_ok(output)) {
        data = mmap(NULL, (size_t)st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
        if (data == MAP_FAILED)
            perror("mmap");
        else {
            if (!validate_elf(data, (size_t)st.st_size))
                result = process(data, (size_t)st.st_size, output, st.st_mode);
            munmap((void *)data, (size_t)st.st_size);
        }
    }
    close(fd);
    free(default_output);
    return result;
}
