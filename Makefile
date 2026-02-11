CC = gcc

CFLAGS = -Wall -Wextra -g

LIBS = -lcurl -lcjson -lpthread

TARGET = discordbotC

OBJ = main.o config.o

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $(TARGET) $(LIBS)
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)

.PHONY: all clean
