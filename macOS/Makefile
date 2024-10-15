CC = gcc
CFLAGS = -Wall -Wextra -O2
SRC = parser.c
TARGET = parser
$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET)

.PHONY: clean
clean:
	rm -f $(TARGET)