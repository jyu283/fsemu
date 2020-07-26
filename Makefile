# project name (generate executable with this name)
TARGET   = fsemu

CC       = gcc
# compiling flags here
CFLAGS   = -Wall -g -Og -I./include \
		   -Wno-unused-variable -Wno-unused-function

LINKER   = gcc
# linking flags here
LFLAGS   = -Wall -I./include -lm

# change these to proper directories where each file should be
SRCDIR   = src
OBJDIR   = obj

SOURCES  := $(wildcard $(SRCDIR)/*.c)
INCLUDES := $(wildcard $(SRCDIR)/*.h)
OBJECTS  := $(SOURCES:$(SRCDIR)/%.c=$(OBJDIR)/%.o)
rm       = rm -f
mkdir    = mkdir -p


$(TARGET): $(OBJECTS)
	$(rm) fs.img
	$(LINKER) $(OBJECTS) $(LFLAGS) -o $@

$(OBJECTS): $(OBJDIR)/%.o : $(SRCDIR)/%.c
	@$(mkdir) $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: clean
clean:
	$(rm) $(OBJECTS)
	$(rm) $(TARGET)

.PHONY: remove
remove: clean
	@$(rm) $(TARGET)
