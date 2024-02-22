#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>

//  constant number of bytes which the length of byte-strings gets encoded
uint64_t LEN_SIZE = 8;

//  struct for storing byte-strings and their length
struct byte_string {
    uint64_t len;
    char *data;
};

//  function declarations
struct byte_string *read_bytes(char *);
uint64_t f_size(char *);
uint64_t from_bytes(uint64_t, uint64_t, const char *);
char *to_bytes(uint64_t, uint64_t);
int extract_files(char *);
struct byte_string *encode_file(char *);

int main(int argc, char **argv) {
    //  check if number of passed arguments is correct
    if (argc < 2) {
        return 1;
    }

    //  can be executed in two different modes (encode, decode)
    if (!strcmp(argv[1], "encode")) {
        if (argc < 4) {
            fprintf(stderr, "Wrong number of arguments for mode 'encode'. Expected at least 4.\n");
            return 1;
        }

        //  open and clear the file where everything will be stored
        FILE *f_output = fopen(argv[2], "wb+");
        if (!f_output) {
            fprintf(stderr, "Could not open file '%s'.\n", argv[2]);
            return 1;
        }

        //  iterate through all passed files, encode them and write them to the output file
        uint64_t written;
        for (int i = 3; i < argc; i++) {
            struct byte_string *bytes = encode_file(argv[i]);
            if (!bytes) {
                fprintf(stderr, "Could not encode file '%s'.\n", argv[i]);
                return 1;
            }
            written = fwrite(bytes->data, 1, bytes->len, f_output);
            if (written < bytes->len) {
                fprintf(stderr, "Could not write to file '%s'.\n", argv[2]);
                free(bytes->data);
                free(bytes);
                return 1;
            }
            free(bytes->data);
            free(bytes);
        }

        fclose(f_output);
        return 0;
    } else if (!strcmp(argv[1], "decode")) {
        if (argc != 3) {
            fprintf(stderr, "Wrong number of arguments for mode 'decode'. Expected 3.\n");
            return 1;
        }

        //  extract the files from the input file and return the error code
        return extract_files(argv[2]);
    }
    return 1;
}

//  read the complete content of a specific file as a byte-string
struct byte_string *read_bytes(char *filepath) {
    uint64_t len = f_size(filepath);

    //  allocate the struct for storing the bytes
    struct byte_string *bytes = malloc(sizeof (struct byte_string));
    if (!bytes) {
        return NULL;
    }
    bytes->len = len;

    //  open the specified file
    FILE *file = fopen(filepath, "rb");
    if (!file) {
        free(bytes);
        return NULL;
    }

    //  allocate the byte-string
    char *f_data = malloc(len);
    if (!f_data) {
        free(bytes);
        fclose(file);
        return NULL;
    }
    bytes->data = f_data;

    //  read the file
    char buf[128];
    uint64_t bytes_read;
    uint64_t offset = 0;
    while ((bytes_read = fread(buf, 1, 127, file))) {
        memcpy(f_data + offset, buf, bytes_read);
        offset += bytes_read;
    }

    //  finally close the file
    fclose(file);
    return bytes;
}

//  calculate the size in bytes of a specific file
uint64_t f_size(char *filepath) {
    struct stat f_info;
    lstat(filepath, &f_info);
    return f_info.st_size;
}

//  encode unsigned long to byte-string of a specific length
char *to_bytes(uint64_t value, uint64_t length) {
    //  allocate byte-string
    char *bytes = malloc(length);
    if (!bytes) {
        return NULL;
    }

    //  convert value byte-wise
    for (uint64_t i = 0; i < length; i++) {
        *(bytes + i) = (char) (value % 256);
        value/= 256;
    }
    return bytes;
}

//  decode byte-string to unsigned long
uint64_t from_bytes(uint64_t start, uint64_t length, const char *bytes) {
    if (!bytes) {
        return 0;
    }

    //  convert the byte-string byte-wise
    uint64_t res = 0;
    uint64_t multiplier = 1;
    for (uint64_t i = start; i < start + length; i++) {
        int val = (int) *(bytes + i);
        if (val < 0) {
            val += 256;
        }
        res += val * multiplier;
        multiplier *= 256;
    }
    return res;
}

//  return the starting index of the filename included in the specified path
uint64_t extract_filename(const char *filepath, uint64_t len) {
    for (uint64_t i = len; i > 0; i--) {
        if (*(filepath + i - 1) == '/') {
            return i;
        }
    }
    return 0;
}

//  copy all bytes from src to dest with the specified length
void bytes_cpy(const char *src, char *dest, uint64_t len) {
    for (uint64_t i = 0; i < len; i++) {
        *(dest + i) = *(src + i);
    }
}

//  encode the file containing filename and content
struct byte_string *encode_file(char *filepath) {
    //  read all bytes of the specified file
    struct byte_string *bytes_f = read_bytes(filepath);
    if (!bytes_f) {
        fprintf(stderr, "Could not read file '%s'.\n", filepath);
        return NULL;
    }

    //  encode the length of the file-content
    char *bytes_len_f = to_bytes(bytes_f->len, LEN_SIZE);
    if (!bytes_len_f) {
        fprintf(stderr, "Error processing data.\n");
        free(bytes_f->data);
        free(bytes_f);
        return NULL;
    }

    //  calculate and encode the length of the filename
    uint64_t path_len = strlen(filepath);
    uint64_t filename_offset = extract_filename(filepath, path_len);
    uint64_t name_len = path_len - filename_offset;
    char *bytes_len_name = to_bytes(name_len, LEN_SIZE);
    if (!bytes_len_name) {
        fprintf(stderr, "Error processing data.\n");
        free(bytes_f->data);
        free(bytes_f);
        free(bytes_len_f);
        return NULL;
    }

    //  allocate byte-string for storing the result
    struct byte_string *bytes = malloc(sizeof (struct byte_string));
    if (!bytes) {
        fprintf(stderr, "Memory allocation error.\n");
        return NULL;
    }

    //  allocate memory for storing the data
    uint64_t buf_size = LEN_SIZE * 2 + bytes_f->len + name_len;
    char *buf = malloc(buf_size);
    if (!buf) {
        fprintf(stderr, "Memory allocation error.\n");
        free(bytes);
        free(bytes_f->data);
        free(bytes_f);
        free(bytes_len_f);
        free(bytes_len_name);
        return NULL;
    }
    bytes->data = buf;
    bytes->len = buf_size;

    //  encode the filename and file-content and corresponding lengths
    uint64_t idx = 0;
    //  length of filename
    bytes_cpy(bytes_len_name, buf + idx, LEN_SIZE);
    idx += LEN_SIZE;
    //  filename
    bytes_cpy(filepath + filename_offset, buf + idx, name_len);
    idx += name_len;
    //  length of file content
    bytes_cpy(bytes_len_f, buf + idx, LEN_SIZE);
    idx += LEN_SIZE;
    //  file content
    bytes_cpy(bytes_f->data, buf + idx, bytes_f->len);

    //  clean-up
    free(bytes_f->data);
    free(bytes_f);
    free(bytes_len_f);
    free(bytes_len_name);
    return bytes;
}

//  extract all files encoded into the input file
int extract_files(char *filepath) {
    //  read data from file
    struct byte_string *bytes_f = read_bytes(filepath);
    if (!bytes_f) {
        fprintf(stderr, "Could not read file '%s'.\n", filepath);
        return 1;
    }

    //  iterate through the byte-string and extract the encoded file data
    uint64_t pos = 0;
    while (pos < bytes_f->len) {
        //  decode length of filename
        uint64_t name_len = from_bytes(pos, LEN_SIZE, bytes_f->data);
        //  allocate memory for storing the filename
        char *f_name = malloc(name_len + 1);
        if (!f_name) {
            fprintf(stderr, "Memory allocation error.\n");
            free(bytes_f->data);
            free(bytes_f);
            return 1;
        }
        //  copy the filename into the buffer
        *(f_name + name_len) = 0;
        pos += LEN_SIZE;
        bytes_cpy(bytes_f->data + pos, f_name, name_len);
        pos += name_len;

        //  create corresponding output file
        FILE *out = fopen(f_name, "wb+");
        if (!out) {
            fprintf(stderr, "Could not create file '%s'.\n", f_name);
            free(bytes_f->data);
            free(bytes_f);
            free(f_name);
            return 1;
        }
        free(f_name);

        //  decode length of the file-content
        uint64_t f_len = from_bytes(pos, LEN_SIZE, bytes_f->data);
        pos += LEN_SIZE;
        //  write file content to output file
        uint64_t written = fwrite(bytes_f->data + pos, 1, f_len, out);
        if (written < f_len) {
            fprintf(stderr, "Could not write file '%s'.\n", f_name);
            free(bytes_f->data);
            free(bytes_f);
            fclose(out);
            return 1;
        }
        pos += f_len;

        fclose(out);
    }

    //  clean-up
    free(bytes_f->data);
    free(bytes_f);
    return 0;
}
