
libfoo = $(abspath $(SOURCE_DIR)/libfoo)

HDIR_FLAGS = -I$(libfoo)

$(eval $(call compilation-rules,ext.foo/,$(libfoo)/))
