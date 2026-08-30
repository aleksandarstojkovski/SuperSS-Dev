# Practice 3-hole autonomous Multi Client bot (Linux loopback).
.DEFAULT_GOAL := all

CXX := g++
CXXFLAGS := -Og -D _DEBUG -ggdb -fpermissive -fsigned-char -std=c++20 `pkg-config --cflags glib-2.0`
CXXLINK := -pthread `pkg-config --libs glib-2.0`

PROGRAM := practice_bot
DIRSAVEPRROGRAM := ./Multi\ Client
BUILD_FOLDER := output-mc-debug

CFILES := \
	practice_bot.cpp \
	../Multi|Client/PRACTICE/practice_fsm.cpp \
	../Multi|Client/PRACTICE/versus_fsm.cpp \
	../Multi|Client/PRACTICE/practice_loopback.cpp \
	../Projeto|IOCP/UTIL/exception.cpp \
	../Projeto|IOCP/UTIL/message_pool.cpp \
	../Projeto|IOCP/UTIL/message.cpp \
	../Projeto|IOCP/UTIL/WinPort.cpp \
	../Projeto|IOCP/UTIL/hex_util.cpp \
	../Projeto|IOCP/UTIL/util_time.cpp \
	../Projeto|IOCP/UTIL/reader_ini.cpp \
	../Projeto|IOCP/PACKET/packet.cpp \
	../Projeto|IOCP/COMPRESS/compress.cpp \
	../Projeto|IOCP/COMPRESS/minilzo.c \
	../Projeto|IOCP/CRYPT/crypt.cpp

target = $(BUILD_FOLDER)/$(subst |,\ ,$(filter %.o,$(patsubst %.cpp,%.o,${1}) $(patsubst %.c,%.o,${1})))
target_dep = $(BUILD_FOLDER)/$(subst |,\ ,$(filter %.o.d,$(patsubst %.cpp,%.o.d,${1}) $(patsubst %.c,%.o.d,${1})))
obj.cpp :=
obj.c :=
define obj
 $(call target,$(notdir ${1})): $(subst |,\ ,${1}) | ${BUILD_FOLDER}
 obj$(suffix ${1}) += $(call target,$(notdir ${1}))
endef

define SOURCES
 $(foreach src,${1},$(eval $(call obj,${src})))
endef

$(eval $(call SOURCES,${CFILES}))

define obj_link
	$(foreach src,${CFILES},$(call target,$(notdir ${src})))
endef

define dep_link
	$(foreach src,${CFILES},$(call target_dep,$(notdir ${src})))
endef

space := $(subst ,, )
define scape_WS
 $(subst $(space),\ ,${1})
endef

.PHONY: all
all: $(BUILD_FOLDER)/$(PROGRAM)

OBJ_FILES := $(call obj_link)
DEP_FILES := $(call dep_link)
DEPFLAGS = -MMD -MP -MF $(call scape_WS,$@).d -MT '$(strip $(call scape_WS,$@)) $(strip $(call scape_WS,$@)).d'
-include $(DEP_FILES)

CFLAGS += $(CXXFLAGS)
LDLIBS += $(CXXLINK)

${obj.cpp} ${obj.c}: %.o :
	@echo Compiling $(CXX) $(CFLAGS) -c $(call scape_WS,$<) -o $(call scape_WS,$@)
	@$(CXX) $(CFLAGS) $(DEPFLAGS) -c $(call scape_WS,$<) -o $(call scape_WS,$@)

OUT_OBJECTS = $(subst .a\,.a, $(subst .o\,.o,$(addsuffix \,$^)))

$(BUILD_FOLDER)/$(PROGRAM): $(OBJ_FILES)
	@echo Linking $(notdir $@)
	@mkdir -p $(DIRSAVEPRROGRAM)
	$(CXX) $(CFLAGS) $(LDFLAGS) $(OUT_OBJECTS) $(LDLIBS) -o $@
	cp $(BUILD_FOLDER)/$(PROGRAM) $(DIRSAVEPRROGRAM)/$(PROGRAM)

$(BUILD_FOLDER):
	@mkdir -p $@

.PHONY: clean
clean:
	- find $(BUILD_FOLDER)/ -name "*.[od]" -exec rm -rf {} \;
	- rm -f $(DIRSAVEPRROGRAM)/$(PROGRAM)
