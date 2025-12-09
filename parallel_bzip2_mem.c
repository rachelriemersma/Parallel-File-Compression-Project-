// parallel_bzip2_memopt.c - Memory optimized version
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <bzlib.h>
#include <omp.h>
#include <sys/stat.h>
#include <unistd.h>

typedef struct {
    unsigned char *data;
    unsigned int size;
    unsigned int original_size;
} CompressedBlock;

long get_file_size(const char *filename);
int compress_block(unsigned char *input, unsigned int input_size, 
                   CompressedBlock *output);
int write_bzip2_file(const char *output_filename, CompressedBlock *blocks, 
                     int num_blocks);
void cleanup_blocks(CompressedBlock *blocks, int num_blocks);

int main(int argc, char *argv[]) {
    int block_size_kb = 900;
    
    int opt;
    while ((opt = getopt(argc, argv, "b:")) != -1) {
        switch (opt) {
            case 'b':
                block_size_kb = atoi(optarg);
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
    
    int arg_offset = optind;
    
    if (argc - arg_offset != 2) {
        fprintf(stderr, "Usage: %s [-b block_size_kb] <input_file> <output_file>\n", argv[0]);
        return 1;
    }

    const char *input_filename = argv[arg_offset];
    const char *output_filename = argv[arg_offset + 1];
    
    int BLOCK_SIZE = block_size_kb * 1024;
    
    long file_size = get_file_size(input_filename);
    int num_blocks = (file_size + BLOCK_SIZE - 1) / BLOCK_SIZE;
    
    printf("File size: %ld bytes\n", file_size);
    printf("Number of blocks: %d\n", num_blocks);
    printf("Block size: %d bytes\n", BLOCK_SIZE);
    printf("Memory usage (optimized): ~%d MB (vs %ld MB unoptimized)\n", 
           (BLOCK_SIZE * omp_get_max_threads()) / (1024 * 1024),
           file_size / (1024 * 1024));

    CompressedBlock *compressed_blocks = calloc(num_blocks, sizeof(CompressedBlock));
    if (!compressed_blocks) {
        fprintf(stderr, "Memory allocation failed\n");
        return 1;
    }

    double start_time = omp_get_wtime();
    int compression_errors = 0;
    
    // MEMORY OPTIMIZED: Each thread reads its own block from disk
    #pragma omp parallel for schedule(dynamic)
    for (int i = 0; i < num_blocks; i++) {
        // Calculate block boundaries
        unsigned int offset = (long)i * BLOCK_SIZE;
        unsigned int block_size = BLOCK_SIZE;
        
        if (offset + block_size > file_size) {
            block_size = file_size - offset;
        }

        // Each thread opens file and reads only its block
        FILE *fp = fopen(input_filename, "rb");
        if (!fp) {
            #pragma omp atomic
            compression_errors++;
            continue;
        }

        // Allocate memory for ONLY this block (not entire file!)
        unsigned char *block_data = malloc(block_size);
        if (!block_data) {
            fclose(fp);
            #pragma omp atomic
            compression_errors++;
            continue;
        }

        // Seek to block position and read
        fseek(fp, offset, SEEK_SET);
        size_t bytes_read = fread(block_data, 1, block_size, fp);
        fclose(fp);

        if (bytes_read != block_size) {
            free(block_data);
            #pragma omp atomic
            compression_errors++;
            continue;
        }

        // Compress this block
        int result = compress_block(block_data, block_size, 
                                   &compressed_blocks[i]);
        
        // Free block data immediately (not needed anymore!)
        free(block_data);
        
        if (result != 0) {
            #pragma omp atomic
            compression_errors++;
        }
        
        #pragma omp critical
        {
            static int completed = 0;
            completed++;
            if (completed % 10 == 0 || completed == num_blocks) {
                printf("\rCompressed %d/%d blocks", completed, num_blocks);
                fflush(stdout);
            }
        }
    }
    printf("\n");

    double end_time = omp_get_wtime();
    double compression_time = end_time - start_time;

    if (compression_errors > 0) {
        fprintf(stderr, "Compression failed for %d blocks\n", compression_errors);
        cleanup_blocks(compressed_blocks, num_blocks);
        return 1;
    }

    printf("Writing compressed file...\n");
    if (write_bzip2_file(output_filename, compressed_blocks, num_blocks) != 0) {
        fprintf(stderr, "Failed to write output file\n");
        cleanup_blocks(compressed_blocks, num_blocks);
        return 1;
    }

    long total_compressed = 0;
    for (int i = 0; i < num_blocks; i++) {
        total_compressed += compressed_blocks[i].size;
    }

    printf("\nCompression Statistics:\n");
    printf("Original size: %ld bytes\n", file_size);
    printf("Compressed size: %ld bytes\n", total_compressed);
    printf("Compression ratio: %.2f%%\n", 
           (1.0 - (double)total_compressed / file_size) * 100);
    printf("Compression time: %.3f seconds\n", compression_time);
    printf("Throughput: %.2f MB/s\n", 
           (file_size / (1024.0 * 1024.0)) / compression_time);

    cleanup_blocks(compressed_blocks, num_blocks);
    return 0;
}

long get_file_size(const char *filename) {
    struct stat st;
    if (stat(filename, &st) == 0) {
        return st.st_size;
    }
    return -1;
}

int compress_block(unsigned char *input, unsigned int input_size, 
                   CompressedBlock *output) {
    unsigned int output_buffer_size = input_size + (input_size / 100) + 600;
    output->data = malloc(output_buffer_size);
    
    if (!output->data) {
        return -1;
    }

    output->size = output_buffer_size;
    output->original_size = input_size;

    int result = BZ2_bzBuffToBuffCompress(
        (char *)output->data,
        &output->size,
        (char *)input,
        input_size,
        9,
        0,
        30
    );

    if (result != BZ_OK) {
        free(output->data);
        output->data = NULL;
        return -1;
    }

    output->data = realloc(output->data, output->size);
    return 0;
}

int write_bzip2_file(const char *output_filename, CompressedBlock *blocks, 
                     int num_blocks) {
    FILE *output = fopen(output_filename, "wb");
    if (!output) {
        return -1;
    }

    for (int i = 0; i < num_blocks; i++) {
        size_t written = fwrite(blocks[i].data, 1, blocks[i].size, output);
        if (written != blocks[i].size) {
            fclose(output);
            return -1;
        }
    }

    fclose(output);
    return 0;
}

void cleanup_blocks(CompressedBlock *blocks, int num_blocks) {
    for (int i = 0; i < num_blocks; i++) {
        if (blocks[i].data) {
            free(blocks[i].data);
        }
    }
    free(blocks);
}