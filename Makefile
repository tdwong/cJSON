# Makefile for cJSONext
#

##CFLAGS += -DcJSON_CASE_INSENSITIVE
CFLAGS += -g -I. -fPIC
LDLIBS += -L$(CURDIR) -lcJSON -lm	## -lpthread -ldl

LIBS     =libcJSON.so libcJSON.a
LIB_SRCS = cJSON.c cJSONext.c
LIB_OBJS =${LIB_SRCS:.c=.o}
SHARED_LIB_FLAGS = --shared -fpic

.PHONY: all libs progs

all: libs progs

##	cJSON libraries
#
libs: $(LIBS)

libcJSON.so: $(LIB_OBJS)
	$(CC) -o $@ $(SHARED_LIB_FLAGS) $(LIB_OBJS)
# nm -D --defined-only libcJSON.$(SHARED_LIB_EXT)

libcJSON.a: $(LIB_OBJS)
	$(AR) -rcsv $@ $(LIB_OBJS)
# ar tv libcJSON.a
# nm --defined-only libcJSON.a

##	cJSON [test] programs
#
PROGRAMS=jsonCLI		# add other programs as needed

progs: $(PROGRAMS)

jsonCLI: jsonCLI.o libcJSON.so
	$(CC) -o $@ $< $(LDLIBS)
# $@	: The file name of the target of the rule.
# $<	: The name of the first prerequisite.
# $^	: The names of all the prerequisites.

.PHONY: clean libclean reallyclean distclean gitignore
clean:
	$(RM) -fv *.o

libclean:
	$(RM) -rfv $(LIBS)

reallyclean distclean: clean libclean
	$(RM) -rfv *.dSYM $(PROGRAMS) $(LIBS) $(TESTPROGS)
	$(RM) -fv .lib

.PHONY: echo vars
echo:
	@echo "export LD_LIBRARY_PATH=\$$LD_LIBRARY_PATH::"
vars:
	@echo PWD=$(PWD)
	@echo CURDIR=$(CURDIR)

