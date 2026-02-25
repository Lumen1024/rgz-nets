CC      = gcc
CFLAGS  = -Wall -Wextra -O2 -pthread
IFLAGS  = -I../common -I.

BINDIR  = ../bin
OBJDIR  = $(BINDIR)/client

SRCS    = main.c scanner.c session.c
OBJS    = $(SRCS:%.c=$(OBJDIR)/%.o) $(OBJDIR)/utils.o
TARGET  = $(BINDIR)/chat_client

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

$(OBJDIR)/%.o: %.c | $(OBJDIR)
	$(CC) $(CFLAGS) $(IFLAGS) -c $< -o $@

$(OBJDIR)/utils.o: ../common/utils.c | $(OBJDIR)
	$(CC) $(CFLAGS) $(IFLAGS) -c $< -o $@

$(OBJDIR):
	mkdir -p $@

clean:
	rm -rf $(OBJDIR) $(TARGET)