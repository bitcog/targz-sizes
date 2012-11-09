/*
 ============================================================================
 Name        : targz_sizes.c
 Author      : Tomasz Melcer
 Version     :
 Copyright   : GPL v3
 Description : Compute compressed sizes of individual files in a .tar.gz file
 ============================================================================
 */

#include <stdint.h>
#include <stdio.h>

#include <zlib.h>

#define COMPRESSED_BUFFER 102400
#define TAR_BLOCK_SIZE 512

struct tarheader {
    char filename[100];
    char somejunk[24];
    char size[12];
    char otherjunk[512-100-24-12];
};

// technically 36 bits should be enough, but there is no uint_least36_t type
typedef uint_least64_t tarfilesize_t;

tarfilesize_t decode_octal(char* size) {
    tarfilesize_t value = 0;
    for (int i=0; i<11; i++) {
        value = value*8 + (size[i]&7);
    }
    return value;
}

int main(int argc, char** argv) {
    // argument parsing
    if (argc != 2) {
        fprintf(stderr, "Usage:\n   %s file.tar.gz\n", argv[0]);
        return 1;
    }

    char* filename = argv[1];

    // opening files and setting up counters
    FILE* file = fopen(filename, "rb");  // TODO: error checking

    unsigned char compressed[COMPRESSED_BUFFER];
    unsigned char decompressed[TAR_BLOCK_SIZE];
    struct tarheader header;
    gz_header gzip_header = {0};
    z_stream gzip_stream = {0};
    long long int start_of_last_header = 0;

    gzip_stream.next_in = compressed;
    gzip_stream.avail_in = 0;
    gzip_stream.zalloc = Z_NULL;
    gzip_stream.zfree = Z_NULL;
    gzip_stream.next_out = (unsigned char*) &header;
    gzip_stream.avail_out = sizeof(header);
    inflateInit2(&gzip_stream, 15+32); // TODO: error checking
    inflateGetHeader(&gzip_stream, &gzip_header);

    // state machine
    int skip_blocks = 0;
    while(1) {
        // if stream's avail_in is empty, we need more data.
        if (gzip_stream.avail_in == 0) {
            // TODO: error checking
            // TODO: end of file condition
            size_t bytes = fread(&compressed, 1, COMPRESSED_BUFFER, file);
            gzip_stream.next_in = compressed;
            gzip_stream.avail_in = bytes;
        }
        // post-condition: gzip_stream.avail_in > 0

        // if output buffer is not full, try to decompress more data
        if (gzip_stream.avail_out > 0) {
            // TODO: error checking
            int cond = inflate(&gzip_stream, Z_NO_FLUSH);
            if (cond == 1)
                return 0;
        }

        // if output buffer is full, perform an action.
        if (gzip_stream.avail_out == 0) {
            if (skip_blocks > 0) {
                // we're inside a file. just ignore the block
                skip_blocks--;
            } else {
                // we've got a header
                tarfilesize_t size = decode_octal(header.size);
                skip_blocks = ((size + 511) / 512);
            }

            if (skip_blocks > 0) {
                gzip_stream.next_out = (unsigned char*) &decompressed;
                gzip_stream.avail_out = sizeof(decompressed);
            } else {
                // TODO: handle long file names.
                printf("%lld %s\n",
                    gzip_stream.total_in - start_of_last_header,
                    header.filename);

                start_of_last_header = gzip_stream.total_in;

                gzip_stream.next_out = (unsigned char*) &header;
                gzip_stream.avail_out = sizeof(header);
            }
        }
    }
}
