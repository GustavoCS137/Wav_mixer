CC = gcc

SDL2_AVAILABLE := $(shell pkg-config --exists sdl2 && echo yes || echo no)

ifeq ($(SDL2_AVAILABLE),yes)
	CFLAGS = -Wall -g `pkg-config --cflags gtk+-3.0 sdl2` -lm -DUSE_SDL2
	LIBS = `pkg-config --libs gtk+-3.0 sdl2` -lm
else
	CFLAGS = -Wall -g `pkg-config --cflags gtk+-3.0` -lm
	LIBS = `pkg-config --libs gtk+-3.0` -lm
	$(warning SDL2 não encontrado. Reprodução de áudio será desabilitada.)
endif
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
