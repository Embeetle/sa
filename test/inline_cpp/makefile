
CFLAGS = -Wall -Werror
CPPFLAGS = $(patsubst %,-I%,$(HDIRS)) -U__clang__

%.o: %.c; arm-none-eabi-gcc -c $< -o $@ $(CFLAGS) $(CPPFLAGS)
