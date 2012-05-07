
SHELL = /bin/sh
CC    = gcc


TARGET  = dp_lcd_backpack_utils
CFLAGS  = 
SOURCES = $(shell echo *.c)
LIBS    = -lftdi

$(TARGET) : $(SOURCES)
	$(CC) $(CFLAGS) $(LIBS) -o $(TARGET) $(SOURCES)