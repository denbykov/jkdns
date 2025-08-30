# === Project Settings ===
TARGET := jkdns
CC := gcc
CFLAGS := -Wall -Wextra -std=c11 -O2 -D_GNU_SOURCE
INCLUDES := -Iinclude

# === Source and Object Files ===
SRCDIR := src
BUILDDIR := build
TARGET_PATH := $(BUILDDIR)/$(TARGET)
INSTALLDIR := debug
SRC := $(wildcard $(SRCDIR)/*.c)
OBJ := $(patsubst $(SRCDIR)/%.c,$(BUILDDIR)/%.o,$(SRC))

# === Default Target ===
all: $(TARGET_PATH) install

# === Link ===
$(TARGET_PATH): $(OBJ)
	$(CC) $(OBJ) -o $@

# === Compile ===
$(BUILDDIR)/%.o: $(SRCDIR)/%.c | $(BUILDDIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# === Install ===
.PHONY: install
install: | $(INSTALLDIR)
	cp $(TARGET_PATH) $(INSTALLDIR)

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