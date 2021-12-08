# Configuration

CC		= gcc
LD		= gcc
AR		= ar
CFLAGS		= -g -std=gnu99 -Wall -Iinclude -fPIC
LDFLAGS		= -Llib
LIBS		= -lm
ARFLAGS		= rcs

# Variables

SFS_LIB_HDRS	= $(wildcard include/sfs/*.h)
SFS_LIB_SRCS	= src/disk.c src/fs.c
SFS_LIB_OBJS	= $(SFS_LIB_SRCS:.c=.o)
SFS_LIBRARY	= lib/libsfs.a

SFS_SHL_SRCS	= src/sfssh.c
SFS_SHL_OBJS	= $(SFS_SHL_SRCS:.c=.o)
SFS_SHELL	= bin/sfssh

SFS_TEST_SRCS   = $(wildcard tests/*.c)
SFS_TEST_OBJS   = $(SFS_TEST_SRCS:.c=.o)
SFS_UNIT_TESTS	= $(patsubst tests/%,bin/%,$(patsubst %.c,%,$(wildcard tests/unit_*.c)))

# Rules

all:		$(SFS_LIBRARY) $(SFS_UNIT_TESTS) $(SFS_SHELL)

%.o:		%.c $(SFS_LIB_HDRS)
	@echo "Compiling $@"
	@$(CC) $(CFLAGS) -c -o $@ $<

$(SFS_LIBRARY):	$(SFS_LIB_OBJS)
	@echo "Linking   $@"
	@$(AR) $(ARFLAGS) $@ $^

$(SFS_SHELL):	$(SFS_SHL_OBJS) $(SFS_LIBRARY)
	@echo "Linking   $@"
	@$(LD) $(LDFLAGS) -o $@ $^ $(LIBS)

bin/unit_%:	tests/unit_%.o $(SFS_LIBRARY)
	@echo "Linking   $@"
	@$(LD) $(LDFLAGS) -o $@ $^

test-units:	$(SFS_UNIT_TESTS)
	@EXIT=0; for test in bin/run_*_unit.sh; do 	\
	    $$test;					\
	    EXIT=$$(($$EXIT + $$?));			\
	done; exit $$EXIT

test-shell:	$(SFS_SHELL)
	@EXIT=0; for test in bin/run_*_test.sh; do	\
	    $$test;					\
	    EXIT=$$(($$EXIT + $$?));			\
	done; exit $$EXIT

test-all:	test-units test-shell

test:
	@$(MAKE) -sk test-all

clean:
	@echo "Removing  objects"
	@rm -f $(SFS_LIB_OBJS) $(SFS_SHL_OBJS) $(SFS_TEST_OBJS)

	@echo "Removing  libraries"
	@rm -f $(SFS_LIBRARY)

	@echo "Removing  programs"
	@rm -f $(SFS_SHELL)

	@echo "Removing  tests"
	@rm -f $(SFS_UNIT_TESTS) test.log

.PRECIOUS: %.o
