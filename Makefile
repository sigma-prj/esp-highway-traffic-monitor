TARGET = eagle
FLAVOR ?= release

ifndef PDIR # {
GEN_IMAGES= eagle.app.v6.out
GEN_BINS= eagle.app.v6.bin
SPECIAL_MKTARGETS=$(APP_MKTARGETS)
SUBDIRS=    \
    user    \
    utils
endif # } PDIR

APPDIR = .
LDDIR = ../ld

CCFLAGS += -Os

TARGET_LDFLAGS =  \
    -nostdlib     \
    -Wl,-EL       \
    --longcalls   \
    --text-section-literals

ifeq ($(FLAVOR),debug)
    TARGET_LDFLAGS += -g -O2
endif

ifeq ($(FLAVOR),release)
    TARGET_LDFLAGS += -g -O0
endif

COMPONENTS_eagle.app.v6 = \
    user/libuser.a        \
    utils/libutils.a

LINKFLAGS_eagle.app.v6 =      \
    -L../lib                  \
    -nostdlib                 \
    -T$(LD_FILE)              \
    -Wl,--no-check-sections   \
    -Wl,--gc-sections	      \
    -u call_user_start        \
    -Wl,-static               \
    -Wl,--start-group         \
    -lc                       \
    -lgcc                     \
    -lphy                     \
    -lpp                      \
    -lnet80211                \
    -llwip                    \
    -lwpa                     \
    -lcrypto                  \
    -lmain                    \
    -lmbedtls                 \
    -ljson                    \
    -ldriver	              \
    $(DEP_LIBS_eagle.app.v6)  \
    -Wl,--end-group

DEPENDS_eagle.app.v6 = \
                $(LD_FILE) \
                $(LDDIR)/eagle.rom.addr.v6.ld

CONFIGURATION_DEFINES = -DICACHE_FLASH

DEFINES +=              \
    $(UNIVERSAL_TARGET_DEFINES) \
    $(CONFIGURATION_DEFINES)

DDEFINES +=             \
    $(UNIVERSAL_TARGET_DEFINES) \
    $(CONFIGURATION_DEFINES)

INCLUDES := $(INCLUDES) -I $(PDIR)include
PDIR := ../$(PDIR)
sinclude $(PDIR)Makefile

.PHONY: FORCE
FORCE:

.PHONY: clean_image
clean_image:
	@echo "[INFO] Cleaning eagle image..."
	rm -f ../bin/eagle.*
	@echo "[INFO] Cleaning Completed"

.PHONY: esp_erase
esp_erase:
	@echo "[INFO] Erasing from ESP memory..."
	./erase_mem_non_ota.sh

.PHONY: esp_flash
esp_flash:
	@echo "[INFO] Flashing eagle image..."
	./flash_mem_non_ota.sh