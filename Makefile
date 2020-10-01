CFLAGS  = -Wall -g -std=c11 -pthread -lrt -rdynamic #-DDEBUG
SRC_DIR = ./src
OBJ_DIR = ./build
INCLUDE = -I ./src
SOURCES = $(shell ls $(SRC_DIR)/*.c)
OBJECTS = $(subst $(SRC_DIR), $(OBJ_DIR), $(SOURCES:.c=.o))
TARGET  = mes

$(TARGET): Makefile $(OBJECTS)
	gcc $(CFLAGS) -o $(TARGET) $(OBJECTS)
	# objdump -S -d mes > mes.disasm

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	gcc $(CFLAGS) $(DEFINES) $(INCLUDE) -o $@ -c $<

objclean:
	$(RM) $(OBJECTS)

clean:
	$(RM) $(OBJECTS) $(TARGET)
