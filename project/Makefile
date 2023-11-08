#######################################################################################
.PHONY: help
help:
	@echo "Makefile Usage:"
	@echo ""
	@echo "  make all"
	@echo "      Command to build client, encoder and decoder."
	@echo ""
	@echo "  make clent"
	@echo "      Command to build client."
	@echo ""
	@echo "  make encoder "
	@echo "      Command to build encoder."
	@echo ""
	@echo "  make decoder "
	@echo "      Command to build decoder."
	@echo ""
	@echo "  make clean "
	@echo "      Command to remove the generated files."
	@echo ""
#######################################################################################

# compiler tools
# XILINX_VITIS ?= /media/lilbirb/research/Xilinx/Vitis/2020.1
# XILINX_VIVADO ?= /media/lilbirb/research/Xilinx/Vivado/2020.1
# XILINX_VIVADO_HLS ?= $(XILINX_VITIS)/Vivado_HLS

HOST_CXX ?= aarch64-linux-gnu-g++
VPP ?= ${XILINX_VITIS}/bin/v++
RM = rm -f
RMDIR = rm -rf

VITIS_PLATFORM = u96v2_sbc_base
VITIS_PLATFORM_DIR = ${PLATFORM_REPO_PATHS}
VITIS_PLATFORM_PATH = $(VITIS_PLATFORM_DIR)/u96v2_sbc_base.xpfm

# host compiler global settings
CXXFLAGS += -march=armv8-a+simd -mtune=cortex-a53 -std=c++11 -DVITIS_PLATFORM=$(VITIS_PLATFORM) -D__USE_XOPEN2K8 -I$(XILINX_VIVADO)/include/ -I$(VITIS_PLATFORM_DIR)/sw/u96v2_sbc_base/PetaLinux/sysroot/aarch64-xilinx-linux/usr/include/xrt/ -O3 -g -Wall -c -fmessage-length=0 --sysroot=$(VITIS_PLATFORM_DIR)/sw/u96v2_sbc_base/PetaLinux/sysroot/aarch64-xilinx-linux
LDFLAGS += -lxilinxopencl -lpthread -lrt -ldl -lcrypt -lstdc++ -L$(VITIS_PLATFORM_DIR)/sw/u96v2_sbc_base/PetaLinux/sysroot/aarch64-xilinx-linux/usr/lib/ --sysroot=$(VITIS_PLATFORM_DIR)/sw/u96v2_sbc_base/PetaLinux/sysroot/aarch64-xilinx-linux

# hardware compiler shared settings
VPP_OPTS = --target hw

#
# OpenCL kernel files
#
# XO := kernel.xo
# XCLBIN := kernel.xclbin
# ALL_MESSAGE_FILES = $(subst .xo,.mdb,$(XO)) $(subst .xclbin,.mdb,$(XCLBIN))

#
# host files
#
CLIENT_SOURCES = Client/client.cpp
CLIENT_EXE = client

SERVER_SOURCES = Server/encoder.cpp Server/server.cpp
SERVER_OBJECTS =$(SERVER_SOURCES:.cpp=.o)
SERVER_EXE = encoder

DECODER_SOURCES = Decoder/Decoder.cpp
DECODER_OBJECTS =$(DECODER_SOURCES:.cpp=.o)
DECODER_EXE = decoder

PROJECT_SOURCES = cdc.cpp sha.cpp dedup.cpp lzw.cpp main.cpp Server/server.cpp sha256/sha256.cpp
PROJECT_OBJECTS =$(PROJECT_SOURCES:.cpp=.o)
PROJECT_EXE = compress
PROJECT_EXE_CDC_DEBUG = encdc_debug
PROJECT_EXE_SHA_TEST = ensha_test
PROJECT_EXE_SHA_DEBUG = ensha_debug
PROJECT_EXE_MAIN_DEBUG = enmain_debug

# CPU_SOURCES = cpu/Host.cpp ../../common/EventTimer.cpp ../../common/Utilities.cpp
# CPU_OBJECTS=$(CPU_SOURCES:.cpp=.o)
# CPU_EXE = mmult_cpu

# .PHONY: cpu
# cpu: $(CPU_EXE)

# $(CPU_EXE): $(CPU_OBJECTS)
# 	$(HOST_CXX) -I./fpga/hls/ -o "$@" $(+) $(LDFLAGS)
all: $(CLIENT_EXE) $(SERVER_EXE) $(DECODER_EXE) $(PROJECT_EXE)
project: $(PROJECT_EXE)
proj_main_debug: $(PROJECT_EXE_MAIN_DEBUG)
proj_sha_test: $(PROJECT_EXE_SHA_TEST)
proj_sha_debug: $(PROJECT_EXE_SHA_DEBUG)
proj_cdc_debug: $(PROJECT_EXE_CDC_DEBUG)

$(CLIENT_EXE):
	g++ -O3 $(CLIENT_SOURCES) -o "$@"

$(PROJECT_EXE):
	g++ -g $(PROJECT_SOURCES) -I./sha256/ -o "$@"

$(PROJECT_EXE_CDC_DEBUG):
	g++ -g $(PROJECT_SOURCES) -DCDC_DEBUG -I./sha256/ -o "$@"

$(PROJECT_EXE_SHA_TEST):
	g++ -g $(PROJECT_SOURCES) -DSHA_TEST -I./sha256/ -o "$@"

$(PROJECT_EXE_SHA_DEBUG):
	g++ -g $(PROJECT_SOURCES) -DSHA_DEBUG -I./sha256/ -o "$@"

$(PROJECT_EXE_MAIN_DEBUG):
	g++ -g $(PROJECT_SOURCES) -DMAIN_DEBUG -I./sha256/ -o "$@"

$(SERVER_EXE): $(SERVER_OBJECTS)
	$(HOST_CXX) -o "$@" $(+) $(LDFLAGS)
	# -@echo $(VPP) --package --config fpga/package.cfg --package.kernel_image $(PLATFORM_REPO_PATHS)/sw/ese532_hw6_pfm/linux_domain/image/image.ub --package.rootfs $(PLATFORM_REPO_PATHS)/sw/ese532_hw6_pfm/linux_domain/rootfs/rootfs.ext4 $(XCLBIN)
	# -@$(VPP) --package --config fpga/package.cfg --package.sd_file "$@" --package.kernel_image $(PLATFORM_REPO_PATHS)/sw/ese532_hw6_pfm/linux_domain/image/image.ub --package.rootfs $(PLATFORM_REPO_PATHS)/sw/ese532_hw6_pfm/linux_domain/rootfs/rootfs.ext4 $(XCLBIN)

$(DECODER_EXE): $(DECODER_OBJECTS)
	$(HOST_CXX) -o "$@" $(+) $(LDFLAGS)

.cpp.o:
	$(HOST_CXX) $(CXXFLAGS) -I./server -o "$@" "$<"

#
# primary build targets
#

# .PHONY: fpga clean
# fpga: $(XCLBIN)

.NOTPARALLEL: clean

clean:
	-$(RM) $(SERVER_EXE) $(SERVER_OBJECTS) $(DECODER_EXE) $(DECODER_OBJECTS) $(CLIENT_EXE) $(PROJECT_OBJECTS) $(PROJECT_EXE) $(PROJECT_EXE_CDC_DEBUG) $(PROJECT_EXE_MAIN_DEBUG) $(PROJECT_EXE_SHA_TEST) $(PROJECT_EXE_SHA_DEBUG)

# clean-cpu:
# 	-$(RM) $(CPU_EXE) $(CPU_OBJECTS) 

# clean-host:
# 	-$(RM) $(HOST_EXE) $(HOST_OBJECTS) 

# clean-accelerators:
# 	-$(RM) $(XCLBIN) $(XO) $(ALL_MESSAGE_FILES)
# 	-$(RM) *.xclbin.sh *.xclbin.info *.xclbin.link_summary* *.compile_summary
# 	-$(RMDIR) .Xil fpga/hls/proj_mmult

# clean-package:
# 	-${RMDIR} package
# 	-${RMDIR} package.build

# clean: clean-cpu clean-host clean-accelerators clean-package
# 	-$(RM) *.log *.package_summary
# 	-${RMDIR} _x .ipcache

#
# binary container: kernel.xclbin
#

# $(XO): fpga/hls/MMult.cpp
# 	-@$(RM) $@
# 	$(VPP) $(VPP_OPTS) -k mmult_fpga --compile -I"$(<D)" --config fpga/design.cfg -o"$@" "$<"
# $(XCLBIN): $(XO)
# 	$(VPP) $(VPP_OPTS) --link --config fpga/design.cfg -o"$@" $(+)