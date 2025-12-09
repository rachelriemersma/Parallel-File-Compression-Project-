#!/bin/bash

echo "=== Memory Optimization Benchmark ==="
echo ""

# Create a 200MB test file
echo "Creating 200MB test file..."
for i in {1..4000000}; do 
    echo "This is line $i with test data for memory optimization benchmarking."
done > large_test.txt

FILE_SIZE=$(stat -c%s large_test.txt)
FILE_SIZE_MB=$((FILE_SIZE / 1024 / 1024))
echo "Test file size: ${FILE_SIZE_MB} MB"
echo ""

export OMP_NUM_THREADS=16

echo "=== BEFORE Optimization (loads entire file) ==="
echo "Expected memory usage: ~${FILE_SIZE_MB} MB"
/usr/bin/time -v ./parallel_bzip2 -b 900 large_test.txt output_original.bz2 2>&1 | grep -E "(Maximum resident|Compression time|Throughput)"
echo ""

echo "=== AFTER Optimization (streams blocks) ==="
BLOCK_SIZE_KB=900
NUM_THREADS=16
EXPECTED_MEM=$(((BLOCK_SIZE_KB * NUM_THREADS) / 1024))
echo "Expected memory usage: ~${EXPECTED_MEM} MB"
/usr/bin/time -v ./parallel_bzip2_memopt -b 900 large_test.txt output_memopt.bz2 2>&1 | grep -E "(Maximum resident|Compression time|Throughput)"
echo ""

# Verify outputs are identical
echo "Verifying both outputs are identical..."
diff output_original.bz2 output_memopt.bz2
if [ $? -eq 0 ]; then
    echo "Outputs match - optimization is correct!"
else
    echo "Outputs differ - something is wrong!"
fi

# Cleanup
rm large_test.txt output_original.bz2 output_memopt.bz2

echo ""
echo "Memory Optimization Complete!"