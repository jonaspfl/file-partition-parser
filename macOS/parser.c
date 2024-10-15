#define __DARWIN_64_BIT_INO_T 1

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>

//  constant number of bytes which the length of byte-strings gets encoded
const uint64_t LEN_SIZE = 8;

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
int process_input_file(char *);
struct byte_string *encode_file(char *);
uint64_t fwrite64(const void *, uint64_t, FILE *);
void bytes_cpy(const char *, char *, uint64_t);

void print_help(char *app_name) {
    fprintf(stdout, "This application can be executed in 2 different modes (encode, decode).\n"
                    "Syntax:\n1) %s encode <max output filesize> <output filename> <input filename 1> ... <input filename n>\n"
                    "2) %s decode <input filename>\n"
                    "| <max output filesize>: 5K -> 5 KiB, 7M -> 7 MiB, 13G -> 13 GiB (0 -> unlimited)\n"
                    "|-> output will be split into multiple data-files if total data exceeds the max output filesize.\n"
                    "| Multiple input files can be added.\n"
                    "Examples:\n1) %s encode 32M out dir/file0 dir/file1\n"
                    "2) %s encode 10K out file\n"
                    "3) %s decode out\n", app_name, app_name, app_name, app_name, app_name);
    fflush(stdout);
}

int main(int argc, char **argv) {
    //  check if number of passed arguments is correct
    if (argc < 2) {
        print_help(argv[0]);
        return 1;
    }

    //  can be executed in two different modes (encode, decode)
    if (!strcmp(argv[1], "encode")) {
        if (argc < 5) {
            fprintf(stderr, "Wrong number of arguments for mode 'encode'. Expected at least 4.\n");
            fflush(stderr);
            print_help(argv[0]);
            return 1;
        }

        //  parse thw given max output-filesize in kb
        char *ptr = argv[2];
        if (*ptr == '-') {
            fprintf(stderr, "Error parsing the given max. output size: '%s'\n", argv[2]);
            fflush(stderr);
            return 1;
        }
        uint64_t max_fsize = strtol(argv[2], &ptr, 10);
        if (ptr == argv[2]) {
            fprintf(stderr, "Error parsing the given max. output size: '%s'\n", argv[2]);
            fflush(stderr);
            return 1;
        }
        if (max_fsize) {
            switch (*ptr) {
                case 'k':
                case 'K':
                    if (max_fsize *  1024 < max_fsize) {
                        fprintf(stderr, "Error parsing the given max. output size: '%s' (overflow)\n", argv[2]);
                        fflush(stderr);
                        return 1;
                    }
                    max_fsize *= 1024;
                    break;
                case 'm':
                case 'M':
                    if (max_fsize *  1024 * 1024 < max_fsize) {
                        fprintf(stderr, "Error parsing the given max. output size: '%s' (overflow)\n", argv[2]);
                        fflush(stderr);
                        return 1;
                    }
                    max_fsize *= 1024 * 1024;
                    break;
                case 'g':
                case 'G':
                    if (max_fsize *  1024 * 1024 * 1024 < max_fsize) {
                        fprintf(stderr, "Error parsing the given max. output size: '%s' (overflow)\n", argv[2]);
                        fflush(stderr);
                        return 1;
                    }
                    max_fsize *= 1024 * 1024 * 1024;
                    break;
                default:
                    fprintf(stderr, "Missing unit for the given max. output size: '%s' (valid are: K | M | G)\n", argv[2]);
                    fflush(stderr);
                    return 1;
            }
        }
        if (!max_fsize) {
            max_fsize--;
        }

        uint32_t f_name_len = strlen(argv[3]);
        char *f_name =  malloc(f_name_len + 32);
        if (!f_name) {
            fprintf(stderr, "Could not allocate memory'.\n");
            fflush(stderr);
            return 1;
        }

        //  setting up all the variables, that need to be persistent over potentially many iterations
        //  all are adjusted within the loop to reflect the current state of the data output process
        uint64_t written_total = 0;
        uint64_t bytes_carry = 0;
        uint64_t bytes_offset = 0;
        uint32_t f_idx = 0;
        struct byte_string *bytes = NULL;
        FILE *f_output = NULL;

        //  iterate through all input files
        //  one file can be written in multiple iterations depending on the maximum output file size
        //  iteration counter is adjusted accordingly
        for (uint32_t i = 4; i < (uint32_t) argc; i++) {
            //  when there is no carry, the current byte-string needs to be cleared and a new file needs to be read
            if (!bytes_carry) {
                if (bytes) {
                    free(bytes->data);
                    free(bytes);
                }
                //  read and encode the new file
                bytes = encode_file(argv[i]);
                if (!bytes) {
                    fprintf(stderr, "Could not encode file '%s'.\n", argv[i]);
                    fflush(stderr);
                    free(f_name);
                    return 1;
                }
                bytes_offset = 0;
            }

            //  the current byte-string must not be NULL at this point
            if (!bytes) {
                fprintf(stderr, "Error, memory is not allocated.");
                fflush(stderr);
                free(f_name);
                return 1;
            }

            //  calculating how many bytes are left to write in the same file
            uint64_t bytes_left = max_fsize - (written_total % max_fsize);
            if (bytes_left < bytes->len - bytes_offset) {
                bytes_carry = bytes->len - bytes_offset - bytes_left;
            } else {
                bytes_carry = 0;
            }

            //  when the current byte-string is shorter than the remaining filesize, only write as much as needed
            uint64_t write_n = bytes_left;
            if (bytes->len - bytes_offset < bytes_left) {
                write_n = bytes->len - bytes_offset;
            }

            //  check if new output file needs to be created
            if (bytes_left == max_fsize) {
                //  setting up the filename of the new data file
                memset(f_name, 0, f_name_len + 31);
                strncpy(f_name, argv[3], f_name_len);
                snprintf(f_name + f_name_len, f_name_len + 31, "_data%u", f_idx);

                //  close the currently open file
                if (f_output) {
                    fclose(f_output);
                }

                //  open the new file
                f_output = fopen(f_name, "wb+");
                if (!f_output) {
                    fprintf(stderr, "Could not open file '%s'.\n", f_name);
                    fflush(stderr);
                    free(f_name);
                    free(bytes->data);
                    free(bytes);
                    return 1;
                }
                f_idx++;
            }

            //  the output file must be open at this point
            if (!f_output) {
                fprintf(stderr, "Error, output file is not open.");
                fflush(stderr);
                free(bytes->data);
                free(bytes);
                free(f_name);
                return 1;
            }

            //  write the actual output data until everything is written, or until max file size is reached
            uint64_t written;
            fprintf(stdout, "Writing %llu MiB to file '%s'.\n", write_n / (1024 * 1024), f_name);
            fflush(stdout);
            written = fwrite64(bytes->data + bytes_offset, write_n, f_output);
            if (written < write_n) {
                fprintf(stderr, "Could not write to file '%s'.\n", f_name);
                fflush(stderr);
                free(bytes->data);
                free(bytes);
                free(f_name);
                return 1;
            }

            //  adjust how many bytes were written
            //  also don't increase file idx if the current file was not completely written
            written_total += written;
            bytes_offset += written;
            if (bytes_carry) {
                i--;
            }
        }

        //  free all remaining recourses
        if (bytes) {
            free(bytes->data);
            free(bytes);
        }
        if (f_output) {
            fclose(f_output);
        }
        free(f_name);

        //  open main output file, containing the information about the other files
        //  this includes file count and total size written to them
        f_output = fopen(argv[3], "wb+");
        if (!f_output) {
            fprintf(stderr, "Could not open file '%s'.\n", argv[3]);
            fflush(stderr);
            return 1;
        }
        char *f_count = to_bytes(f_idx, LEN_SIZE);
        uint32_t written = fwrite64(f_count, LEN_SIZE, f_output);
        if (written < LEN_SIZE) {
            fprintf(stderr, "Could not write to file '%s'.\n", argv[3]);
            fflush(stderr);
            free(f_count);
            return 1;
        }
        free(f_count);
        char *bytes_written = to_bytes(written_total, LEN_SIZE);
        written = fwrite64(bytes_written, LEN_SIZE, f_output);
        if (written < LEN_SIZE) {
            fprintf(stderr, "Could not write to file '%s'.\n", argv[3]);
            fflush(stderr);
            free(bytes_written);
            return 1;
        }
        free(bytes_written);
        fclose(f_output);

        fprintf(stdout, "Successfully wrote %llu bytes to %u files.\n", written_total, f_idx);
        fflush(stdout);

        return 0;
    } else if (!strcmp(argv[1], "decode")) {
        if (argc != 3) {
            fprintf(stderr, "Wrong number of arguments for mode 'decode'. Expected 2.\n");
            fflush(stderr);
            print_help(argv[0]);
            return 1;
        }

        //  extract the files from the input file and return the error code
        int err = process_input_file(argv[2]);
        if (!err) {
            fprintf(stdout, "Successfully extracted all files.\n");
            fflush(stdout);
        }
        return err;
    }

    print_help(argv[0]);
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
        free(bytes->data);
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
    char buf[1024];
    uint64_t bytes_read;
    uint64_t offset = 0;
    while ((bytes_read = fread(buf, 1, 1023, file))) {
        bytes_cpy(buf, f_data + offset, bytes_read);
        offset += bytes_read;
    }

    //  finally close the file
    fclose(file);
    return bytes;
}

//  calculate the size in bytes of a specific file
uint64_t f_size(char *filepath) {
    struct stat f_info;
    stat(filepath, &f_info);
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
        if (*(filepath + i - 1) == '/' || *(filepath + i - 1) == '\\') {
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

uint64_t fwrite64(const void *str, uint64_t len_bytes, FILE *file) {
    if (!str || !file) {
        return 0;
    }

    //  write the file in chunks of 128 MiB each
    const uint64_t CHUNK_SIZE = 128 * 1024 * 1024;
    uint64_t written_complete = 0;
    uint64_t to_write = 0;
    uint64_t offset = 0;
    while (offset < len_bytes) {
        if (len_bytes - offset < CHUNK_SIZE) {
            to_write = len_bytes - offset;
        } else {
            to_write = CHUNK_SIZE;
        }

        uint64_t bytes_written = fwrite(str + offset, 1, to_write, file);
        written_complete += bytes_written;
        if (bytes_written != to_write) {
            return written_complete;
        }

        offset += to_write;
    }

    return written_complete;
}

//  encode the file containing filename and content
struct byte_string *encode_file(char *filepath) {
    //  read all bytes of the specified file
    fprintf(stdout, "Encoding file '%s'...\n", filepath + extract_filename(filepath, strlen(filepath)));
    fflush(stdout);
    struct byte_string *bytes_f = read_bytes(filepath);
    if (!bytes_f) {
        fprintf(stderr, "Could not read file '%s'.\n", filepath);
        fflush(stderr);
        return NULL;
    }

    //  encode the length of the file-content
    char *bytes_len_f = to_bytes(bytes_f->len, LEN_SIZE);
    if (!bytes_len_f) {
        fprintf(stderr, "Error processing data.\n");
        fflush(stderr);
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
        fflush(stderr);
        free(bytes_f->data);
        free(bytes_f);
        free(bytes_len_f);
        return NULL;
    }

    //  allocate byte-string for storing the result
    struct byte_string *bytes = malloc(sizeof (struct byte_string));
    if (!bytes) {
        fprintf(stderr, "Memory allocation error.\n");
        fflush(stderr);
        free(bytes_f->data);
        free(bytes_f);
        free(bytes_len_f);
        return NULL;
    }

    //  allocate memory for storing the data
    uint64_t buf_size = LEN_SIZE * 2 + bytes_f->len + name_len;
    char *buf = malloc(buf_size);
    if (!buf) {
        fprintf(stderr, "Memory allocation error.\n");
        fflush(stderr);
        free(bytes);
        free(bytes_f->data);
        free(bytes_f);
        free(bytes_len_f);
        free(bytes_len_name);
        return NULL;
    }
    bytes->data = buf;
    bytes->len = buf_size;

    fprintf(stdout, "Extracting %llu MiB of data from file '%s'.\n", bytes->len / (1024 * 1024), filepath);
    fflush(stdout);
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

//  extract all files that are encoded in the given byte-string
int extract_files(struct byte_string *bytes_f) {
    //  setup log file
    FILE *log = fopen("parser.log", "wb+");
    if (!log) {
        fprintf(stderr, "Could not open file 'parser.log'.\n");
        fflush(stderr);
        return 1;
    }
    char *separator = malloc(1);
    if (!separator) {
        fprintf(stderr, "Memory allocation error.\n");
        fflush(stderr);
        return 1;
    }
    *separator = '\n';

    //  iterate through the byte-string and extract the encoded file data
    uint64_t pos = 0;
    while (pos < bytes_f->len) {
        //  decode length of filename
        uint64_t name_len = from_bytes(pos, LEN_SIZE, bytes_f->data);
        //  allocate memory for storing the filename
        char *f_name = malloc(name_len + 1);
        if (!f_name) {
            fprintf(stderr, "Memory allocation error.\n");
            fflush(stderr);
            free(bytes_f->data);
            free(bytes_f);
            fclose(log);
            free(separator);
            return 1;
        }
        //  copy the filename into the buffer
        *(f_name + name_len) = 0;
        pos += LEN_SIZE;
        bytes_cpy(bytes_f->data + pos, f_name, name_len);
        pos += name_len;

        //  create corresponding output file
        fprintf(stdout, "Writing file '%s'\n", f_name + extract_filename(f_name, name_len));
        fflush(stdout);
        FILE *out = fopen(f_name, "wb+");
        if (!out) {
            fprintf(stderr, "Could not create file '%s'.\n", f_name);
            fflush(stderr);
            free(bytes_f->data);
            free(bytes_f);
            free(f_name);
            fclose(log);
            free(separator);
            return 1;
        }

        //  decode length of the file-content
        uint64_t f_len = from_bytes(pos, LEN_SIZE, bytes_f->data);
        pos += LEN_SIZE;
        //  write file content to output file
        uint64_t written = fwrite64(bytes_f->data + pos, f_len, out);
        if (written < f_len) {
            fprintf(stderr, "Could not write file '%s'.\n", f_name);
            fflush(stderr);
            free(bytes_f->data);
            free(bytes_f);
            fclose(out);
            free(f_name);
            fclose(log);
            free(separator);
            return 1;
        }

        //  write parser log
        uint64_t written_log = fwrite64(f_name, name_len, log);
        uint64_t written_log_separator = fwrite64(separator, 1, log);
        if (written_log < name_len || written_log_separator < 1) {
            fprintf(stderr, "Could not write file 'parser.log'.\n");
            fflush(stderr);
            free(bytes_f->data);
            free(bytes_f);
            fclose(out);
            free(f_name);
            fclose(log);
            free(separator);
            return 1;
        }

        pos += f_len;
        free(f_name);
        fclose(out);
    }

    //  clean-up
    free(bytes_f->data);
    free(bytes_f);
    fclose(log);
    free(separator);
    return 0;
}

//  process the given input file
int process_input_file(char *filepath) {
    //  read data from the given file
    struct byte_string *bytes_f = read_bytes(filepath);
    if (!bytes_f) {
        fprintf(stderr, "Could not read file '%s'.\n", filepath);
        fflush(stderr);
        return 1;
    }

    //  read the information about the data that needs to be reads from the files
    uint64_t f_count = from_bytes(0, LEN_SIZE, bytes_f->data);
    uint64_t size_total = from_bytes(LEN_SIZE, LEN_SIZE, bytes_f->data);
    free(bytes_f->data);
    free(bytes_f);

    //  store all file names of the data files in an array, so they can be accessed easily
    uint32_t f_name_len = strlen(filepath) + 32;
    char *f_names = calloc(f_count, f_name_len);
    if (!f_names) {
        fprintf(stderr, "Could not allocate memory.\n");
        fflush(stderr);
        return 1;
    }
    for (uint32_t i = 0; i < f_count; i++) {
        snprintf(f_names + (i * f_name_len), f_name_len, "%s_data%u", filepath, i);
    }

    //  check if all needed data-files exist and are accessible
    for (uint32_t i = 0; i < f_count; i++) {
        if (access(f_names + (i * f_name_len), R_OK) == -1) {
            fprintf(stderr, "Error, can not access file '%s'.\n", f_names + (i * f_name_len));
            fflush(stderr);
            free(f_names);
            return 1;
        }
    }

    //  allocate enough memory to read in all data-files
    struct byte_string *bytes_complete = malloc(sizeof (struct byte_string));
    if (!bytes_complete) {
        fprintf(stderr, "Could not allocate memory.\n");
        fflush(stderr);
        free(f_names);
        return 1;
    }
    //  the actual bytes are stored here
    char *data = malloc(size_total);
    if (!data) {
        fprintf(stderr, "Could not allocate memory.\n");
        fflush(stderr);
        free(f_names);
        free(bytes_complete);
        return 1;
    }
    bytes_complete->data = data;
    bytes_complete->len = size_total;

    //  read all data from all data files
    uint64_t cpy_offset = 0;
    for (uint32_t i = 0; i < f_count; i++) {
        fprintf(stdout, "Reading file '%s'\n", f_names + (i * f_name_len + extract_filename( f_names + (i * f_name_len), f_name_len)));
        fflush(stdout);
        struct byte_string *bytes = read_bytes(f_names + (i * f_name_len));
        if (!bytes) {
            fprintf(stderr, "Error, could not read file '%s'.\n", f_names + (i * f_name_len));
            fflush(stderr);
            free(f_names);
            free(bytes_complete);
            free(data);
            return 1;
        }

        //  prevent buffer-overflow on corrupt input file
        if (cpy_offset + bytes->len > size_total) {
            fprintf(stderr, "Error, aborting to prevent buffer-overflow. Input file might be corrupted.\n");
            fflush(stderr);
            free(f_names);
            free(bytes->data);
            free(bytes);
            free(bytes_complete);
            free(data);
            return 1;
        }
        //  the data of all data-files will be stored as one large byte-string
        bytes_cpy(bytes->data, bytes_complete->data + cpy_offset, bytes->len);
        cpy_offset += bytes->len;

        free(bytes->data);
        free(bytes);
    }
    free(f_names);

    //  extract the files from the given bytes
    return extract_files(bytes_complete);
}
