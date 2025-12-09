CC = gcc
CFLAGS = -O3 -Wall -fopenmp
LDFLAGS = -lbz2 -fopenmp

TARGET = parallel_bzip2
SOURCES = parallel_bzip2.c
OBJECTS = $(SOURCES:.c=.o)

all: $(TARGET) parallel_bzip2_memopt

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(TARGET) $(LDFLAGS)

parallel_bzip2_memopt: parallel_bzip2_memopt.c
	$(CC) $(CFLAGS) parallel_bzip2_memopt.c -o parallel_bzip2_memopt $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET) parallel_bzip2_memopt

test: $(TARGET)
	./$(TARGET) test_input.txt test_output.bz2
	bzip2 -d -c test_output.bz2 > decompressed.txt
	diff test_input.txt decompressed.txt

.PHONY: all clean test