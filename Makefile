# project name (generate executable with this name)
TARGET   = fsemu

CC       = gcc
# compiling flags here
CFLAGS   = -Wall -Werror -g -O0 -I./include \
		   -Wno-unused-variable -Wno-unused-function

LINKER   = gcc
# linking flags here
LFLAGS   = -Wall -I./include -lm

# change these to proper directories where each file should be
SRCDIR   = src
OBJDIR   = obj
BINDIR   = bin

SOURCES  := $(wildcard $(SRCDIR)/*.c)
INCLUDES := $(wildcard $(SRCDIR)/*.h)
OBJECTS  := $(SOURCES:$(SRCDIR)/%.c=$(OBJDIR)/%.o)
rm       = rm -f
mkdir    = mkdir -p


$(BINDIR)/$(TARGET): $(OBJECTS)
	@$(mkdir) $(BINDIR)
	$(LINKER) $(OBJECTS) $(LFLAGS) -o $@
	@$(rm) $(TARGET)
	@ln $(BINDIR)/$(TARGET)

$(OBJECTS): $(OBJDIR)/%.o : $(SRCDIR)/%.c
	@$(mkdir) $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: clean
clean:
	$(rm) $(OBJECTS)
	$(rm) $(BINDIR)/$(TARGET)
	$(rm) $(TARGET)

.PHONY: remove
remove: clean
	@$(rm) $(BINDIR)/$(TARGET)
