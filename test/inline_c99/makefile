
CFLAGS = -Wall -Werror
CPPFLAGS = $(patsubst %,-I%,$(HDIRS)) -U__clang__

%.o: %.c; $(TOOLPREFIX)gcc -std=c99 -c $< -o $@ $(CFLAGS) $(CPPFLAGS)
