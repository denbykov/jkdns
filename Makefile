# === Project Settings ===
TARGET := jkdns
CC := gcc
CFLAGS := -Wall -Wextra -std=c11 -D_GNU_SOURCE -g
INCLUDES := -Isrc

# === Source and Object Files ===
SRCDIR := src
BUILDDIR := build
INSTALLDIR := debug
TARGET_PATH := $(BUILDDIR)/$(TARGET)

IGNORE_SRC := $(SRCDIR)/main_old.c

# Recursive source search
SRC := $(filter-out $(IGNORE_SRC), $(shell find $(SRCDIR) -name '*.c'))

# Map src/.../*.c -> build/.../*.o
OBJ := $(patsubst $(SRCDIR)/%.c,$(BUILDDIR)/%.o,$(SRC))

# === Default Target ===
all: $(TARGET_PATH) install

# === Link ===
$(TARGET_PATH): $(OBJ)
	$(CC) $(OBJ) -o $@

# === Compile ===
$(BUILDDIR)/%.o: $(SRCDIR)/%.c | $(BUILDDIR)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# === Install ===
$(INSTALLDIR)/$(TARGET): $(TARGET_PATH) | $(INSTALLDIR)
	cp $< $@

.PHONY: install
install: $(INSTALLDIR)/$(TARGET)

# === Create Build Directory ===
$(BUILDDIR):
	mkdir -p $(BUILDDIR)

# === Create Install Directory ===
$(INSTALLDIR):
	mkdir -p $(INSTALLDIR)

# === Clean ===
.PHONY: clean
clean:
	rm -rf $(BUILDDIR) $(TARGET_PATH)

# === Rebuild ===
.PHONY: rebuild
rebuild: clean all