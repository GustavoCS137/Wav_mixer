CC = gcc
CFLAGS = -Wall -g `pkg-config --cflags gtk+-3.0` -lm
LIBS = `pkg-config --libs gtk+-3.0` -lm
SRC = main.c audio_editor.c wav_reader.c
OBJ = $(SRC:.c=.o)
TARGET = audio_editor

$(TARGET): $(OBJ)
	$(CC) -o $(TARGET) $(OBJ) $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)

run: $(TARGET)
	./$(TARGET)

.PHONY: clean run
