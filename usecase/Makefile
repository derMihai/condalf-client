# Path to the RIOT root directory. 
RIOTBASE ?= $(CURDIR)/../RIOT/
# Usecase environment variables. 
export ESP32_SDK_DIR = $(CURDIR)/../esp-idf/
export XTENSA_ESP32_ELF = $(CURDIR)/../xtensa-esp32-elf/
export PATH := $(PATH):$(XTENSA_ESP32_ELF)/bin/
export OBJCOPY = $(XTENSA_ESP32_ELF)/bin/xtensa-esp32-elf-objcopy

# ConDalF is not part of RIOT, so we have to tell the build system where to find it.
EXTERNAL_MODULE_DIRS += $(CURDIR)/../condalf

# Now we can include ConDaLF like any other RIOT module.
USEMODULE += condalf

# the usecase can work in different modi:
# CONDALF_USE_PUBLISHER == 1, CONDALF_USE_LTB == 1: data is buffered, then sent
# CONDALF_USE_PUBLISHER == 0, CONDALF_USE_LTB == 1: data is stored only locally
# CONDALF_USE_PUBLISHER == 1, CONDALF_USE_LTB == 0: data is directly sent
# CONDALF_USE_PUBLISHER == 0, CONDALF_USE_LTB == 0: not supported
CONDALF_USE_PUBLISHER   = 1
CONDALF_USE_LTB         = 1

# set to 1 to send/store diagnostics messages
CONDALF_USE_RDLOG       = 1

# name of the RIOT application
APPLICATION = condalf-usecase

# Board used for the usecase
BOARD = esp32-wroom-32

# Comment this out to disable code in RIOT that does safety checking
# which is not needed in a production environment but helps in the
# development process:
DEVELHELP = 1

# Change this to 0 show compiler invocation lines by default:
QUIET = 1

# light and temperature sensors
USEMODULE += periph_adc

# SNTP for time updates
USEMODULE += gnrc_udp
USEMODULE += sntp
USEMODULE += posix_inet
USEMODULE += auto_init_gnrc_netif
# in case we disable the publisher, we still want these modules for SNTP
ifneq ($(CONDALF_USE_PUBLISHER), 1)
USEMODULE += gnrc_ipv6_default
endif

# esp32 board wifi
USEMODULE += esp_wifi

# Set CoAP Payload to 256 with: 2^8 (used in nanocoap.h)
# Set CoAP PDU Buffer to 512 (used in gcoap.h)
CFLAGS += -DCONFIG_NANOCOAP_BLOCK_SIZE_EXP_MAX=8
CFLAGS += -DCONFIG_GCOAP_PDU_BUF_SIZE=512

#enable for detailed assertion failures
CFLAGS += -DDEBUG_ASSERT_VERBOSE

ifeq ($(CONDALF_USE_LTB), 1)
# we mount a file system on the ESP32 internal SPI flash
USEMODULE += mtd
USEMODULE += littlefs2
endif

# create this file in the same folder as this makefile and append the following
# lines, uncommented:
# CFLAGS += -DESP_WIFI_SSID=\"<your_WiFi_SSID>\" -DESP_WIFI_PASS=\"<your_WiFi_password>\"
# CFLAGS += -DUSECASE_BACKEND_ADDR=\"<IPv6 of the backend>\"
# CFLAGS += -DUSECASE_BACKEND_PORT=<CoAP server port number>
# CFLAGS += -DUSECASE_BACKEND_RESSOURCE=\"<CoAP ressource path>\"
# CFLAGS += -DUSECASE_INSTANCE=\"<instance name, e.g. ConDaLF1>\"
# CFLAGS += -DUSECASE_INFLUXDB=\"<InfluxDB database name, e.g. swp>\"
include usecase_private.include

# don't fail on unused static function definitions from headers
CFLAGS += -Wno-unused-function

include $(RIOTBASE)/Makefile.include
