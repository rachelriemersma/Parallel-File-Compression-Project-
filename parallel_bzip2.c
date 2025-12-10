#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <bzlib.h>
#include <omp.h>
#include <sys/stat.h>
#include <unistd.h>

typedef struct {
    unsigned char *data; // where data is stored
    unsigned int size; // bytes after compression
    unsigned int original_size; // bytes before compression 
} CompressedBlock;
// declarations
long get_file_size(const char *filename);
int compress_block(unsigned char *input, unsigned int input_size, 
                   CompressedBlock *output);
int write_bzip2_file(const char *output_filename, CompressedBlock *blocks, 
                     int num_blocks);
void cleanup_blocks(CompressedBlock *blocks, int num_blocks);
// main
int main(int argc, char *argv[]) {
    // start with default size - can be overridden 
    int block_size_kb = 900;  
    // parse command line args to look for custom block size 
    int opt;
    while ((opt = getopt(argc, argv, "b:")) != -1) {
        // ascii to into
        switch (opt) {
            case 'b':
                block_size_kb = atoi(optarg);
                // make sure block size is positive 
                if (block_size_kb <= 0) {
                    fprintf(stderr, "Invalid block size\n");
                    return 1;
                }
                break;
            default:
                fprintf(stderr, "Usage: %s [-b block_size_kb] <input_file> <output_file>\n", argv[0]);
                return 1;
        }
    }
    // first non flag arg 
    int arg_offset = optind;
    // check if we have input and output file 
    if (argc - arg_offset != 2) {
        fprintf(stderr, "Usage: %s [-b block_size_kb] <input_file> <output_file>\n", argv[0]);
        return 1;
    }
    // store file names 
    const char *input_filename = argv[arg_offset];
    const char *output_filename = argv[arg_offset + 1];
    // convert kb to bytes 
    int BLOCK_SIZE = block_size_kb * 1024;  
    // open the input file 
    FILE *input_file = fopen(input_filename, "rb");
    // check if opening failed 
    if (!input_file) {
        perror("Error opening input file");
        return 1;
    }
    // get file size 
    long file_size = get_file_size(input_filename);
    // calculate number of blocks needed - rounding up
    int num_blocks = (file_size + BLOCK_SIZE - 1) / BLOCK_SIZE;
    // output
    printf("File size: %ld bytes\n", file_size);
    printf("Number of blocks: %d\n", num_blocks);
    printf("Block size: %d bytes\n", BLOCK_SIZE);
    // allocate memory for meta data  
    CompressedBlock *compressed_blocks = calloc(num_blocks, sizeof(CompressedBlock));
    // check if if calloc failed
    if (!compressed_blocks) {
        fprintf(stderr, "Memory allocation failed\n");
        fclose(input_file);
        return 1;
    }
    // allocate as much memory as the size of the file in bytes 
    unsigned char *file_data = malloc(file_size);
    // check if malloc failed - if so free up
    if (!file_data) {
        fprintf(stderr, "Memory allocation failed for file data\n");
        free(compressed_blocks);
        fclose(input_file);
        return 1;
    }
    // read the entire file into memory 
    size_t bytes_read = fread(file_data, 1, file_size, input_file);
    // check if read failed - if so free up
    if (bytes_read != file_size) {
        fprintf(stderr, "Error reading file\n");
        free(file_data);
        free(compressed_blocks);
        fclose(input_file);
        return 1;
    }
    fclose(input_file);
    // get current time 
    double start_time = omp_get_wtime();
    // count for how many blocks failed to compress 
    int compression_errors = 0;
    // create threads - iterations distributed dynamicly 
    #pragma omp parallel for schedule(dynamic)
    for (int i = 0; i < num_blocks; i++) {
        // calculate where in the file the block starts and assume full size 
        unsigned int offset = i * BLOCK_SIZE;
        unsigned int block_size = BLOCK_SIZE;
        // handle the last block - most likely smaller 
        if (offset + block_size > file_size) {
            block_size = file_size - offset;
        }
        // compress block!
        int result = compress_block(file_data + offset, block_size, 
                                   &compressed_blocks[i]);
        // check if compression failed if so increase count 
        if (result != 0) {
            #pragma omp atomic
            compression_errors++;
        }
    }
    printf("\n");
    // get the current time and calculate how long compression took 
    double end_time = omp_get_wtime();
    double compression_time = end_time - start_time;
    // check if any blocks didnt compress if yes free up comp block memory and file data 
    if (compression_errors > 0) {
        fprintf(stderr, "Compression failed for %d blocks\n", compression_errors);
        cleanup_blocks(compressed_blocks, num_blocks);
        free(file_data);
        return 1;
    }
    // write all compressed blocks to output file - if it fails clean it up 
    if (write_bzip2_file(output_filename, compressed_blocks, num_blocks) != 0) {
        fprintf(stderr, "Failed to write output file\n");
        cleanup_blocks(compressed_blocks, num_blocks);
        free(file_data);
        return 1;
    }
    // add up all the compressed block sizes 
    long total_compressed = 0;
    for (int i = 0; i < num_blocks; i++) {
        total_compressed += compressed_blocks[i].size;
    }
    // print stats 
    printf("\nCompression Statistics:\n");
    printf("Original size: %ld bytes\n", file_size);
    printf("Compressed size: %ld bytes\n", total_compressed);
    printf("Compression ratio: %.2f%%\n", 
           (1.0 - (double)total_compressed / file_size) * 100);
    printf("Compression time: %.3f seconds\n", compression_time);
    printf("Throughput: %.2f MB/s\n", 
           (file_size / (1024.0 * 1024.0)) / compression_time);
    // free all allocated memory 
    cleanup_blocks(compressed_blocks, num_blocks);
    free(file_data);

    return 0;
}
// get the size of a file without opening it 
long get_file_size(const char *filename) {
    struct stat st;
    if (stat(filename, &st) == 0) {
        return st.st_size;
    }
    return -1;
}
// actual compression 
int compress_block(unsigned char *input, unsigned int input_size, 
                   CompressedBlock *output) {
    // allocate output buffer a little bigger in case of expansion              
    unsigned int output_buffer_size = input_size + (input_size / 100) + 600;
    // allocate the buffer 
    output->data = malloc(output_buffer_size);
    // check if we ran out of memory 
    if (!output->data) {
        fprintf(stderr, "Memory allocation failed in compress_block\n");
        return -1;
    }
    // fill in the struct fields 
    output->size = output_buffer_size;
    output->original_size = input_size;
    // call bzip2 library for compression
    int result = BZ2_bzBuffToBuffCompress(
        (char *)output->data,
        &output->size,
        (char *)input,
        input_size,
        9,  
        0,  
        30  
    );
    // check if compression failed, if so free buffer 
    if (result != BZ_OK) {
        fprintf(stderr, "BZ2_bzBuffToBuffCompress failed with error %d\n", result);
        free(output->data);
        output->data = NULL;
        return -1;
    }
    // shrink the buffer to actual size 
    output->data = realloc(output->data, output->size);

    return 0;
}
// write all compressed blocks to output file 
int write_bzip2_file(const char *output_filename, CompressedBlock *blocks, 
                     int num_blocks) {
    // open output file for writing 
    FILE *output = fopen(output_filename, "wb");
    // check if it failed 
    if (!output) {
        perror("Error opening output file");
        return -1;
    }
    // write each blocks 
    for (int i = 0; i < num_blocks; i++) {
        size_t written = fwrite(blocks[i].data, 1, blocks[i].size, output);
        // check if it failed 
        if (written != blocks[i].size) {
            fprintf(stderr, "Write error for block %d\n", i);
            fclose(output);
            return -1;
        }
    }

    fclose(output);
    return 0;
}
// free all the memory 
void cleanup_blocks(CompressedBlock *blocks, int num_blocks) {
    for (int i = 0; i < num_blocks; i++) {
        if (blocks[i].data) {
            free(blocks[i].data);
        }
    }
    free(blocks);
}