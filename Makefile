# Variables
CC = gcc
CFLAGS = -Wall -Wextra -Iinclude -w  # Include the directory where exercise.h is located
TARGET = kilo
SRC_FILES = hash_table.c editor_config.c ini_parser.c kilo.c editor_commands.c utils.c text_highlighting.c row.c screen.c

# Build target
all: $(TARGET)

# Compile the source files (main.c, exercise.c, munit.c) into the TARGET executable
$(TARGET): $(SRC_FILES)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC_FILES)

# Clean target (removes the executable)
clean:
	rm -f $(TARGET)

# Run the tests
run-tests: $(TARGET)
	./$(TARGET)

# PHONY ensures that these targets will run even if a file with the same name exists
.PHONY: all clean run-tests
