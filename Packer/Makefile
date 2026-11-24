NAME = woody
CC = cc
LD = ld
CFLAGS = -Wall -Wextra -Werror -std=c11 -O2 -Iinclude
STUB_FLAGS = -Wall -Wextra -Werror -std=c11 -Os -Iinclude -fPIC \
    -ffreestanding -fno-builtin -fno-stack-protector \
    -fno-asynchronous-unwind-tables -fno-unwind-tables \
    -mno-red-zone -mgeneral-regs-only -fvisibility=hidden
OBJ = build/main.o build/elf.o build/pack.o build/lz_encode.o \
    build/lz_decode.o build/embed.o

all: $(NAME)

$(NAME): $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) -o $@

build:
	mkdir -p build

build/%.o: src/%.c include/woody.h | build
	$(CC) $(CFLAGS) -c $< -o $@

build/entry.o: src/entry.S | build
	$(CC) -c $< -o $@

build/unpack.o: src/unpack.c include/woody.h | build
	$(CC) $(STUB_FLAGS) -c $< -o $@

build/stub_decode.o: src/lz_decode.c include/woody.h | build
	$(CC) $(STUB_FLAGS) -c $< -o $@

build/stub.elf: build/entry.o build/unpack.o build/stub_decode.o src/stub.ld
	$(LD) -s -z noexecstack -T src/stub.ld -o $@ build/entry.o build/unpack.o build/stub_decode.o

build/embed.o: src/embed.S build/stub.elf
	$(CC) -c $< -o $@

clean:
	rm -rf build

fclean: clean
	rm -f $(NAME)

re: fclean
	$(MAKE) all

.PHONY: all clean fclean re
