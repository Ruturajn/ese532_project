#######################################################################################
.PHONY: help
help:
	@echo "Makefile Usage:"
	@echo ""
	@echo "  make all"
	@echo "      Command to build client, encoder and decoder."
	@echo ""
	@echo "  make fpga"
	@echo "      Command to build the sd card package."
	@echo ""
	@echo "  make client"
	@echo "      Command to build client."
	@echo ""
	@echo "  make encoder"
	@echo "      Command to build encoder."
	@echo ""
	@echo "  make decoder"
	@echo "      Command to build decoder."
	@echo ""
	@echo "  make clean"
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
# CXXFLAGS += -march=armv8-a+simd -mtune=cortex-a53 -std=c++11 -DVITIS_PLATFORM=$(VITIS_PLATFORM) -D__USE_XOPEN2K8 -I$(XILINX_VIVADO)/include/ -I$(VITIS_PLATFORM_DIR)/sw/u96v2_sbc_base/PetaLinux/sysroot/aarch64-xilinx-linux/usr/include/xrt/ -O3 -g -Wall -c -fmessage-length=0 --sysroot=$(VITIS_PLATFORM_DIR)/sw/u96v2_sbc_base/PetaLinux/sysroot/aarch64-xilinx-linux
CXXFLAGS += -march=armv8-a+crc+crypto+simd -mtune=cortex-a53 -std=c++11 -DVITIS_PLATFORM=$(VITIS_PLATFORM) -D__USE_XOPEN2K8 -I$(XILINX_VIVADO)/include/ -I$(VITIS_PLATFORM_DIR)/sw/u96v2_sbc_base/PetaLinux/sysroot/aarch64-xilinx-linux/usr/include/xrt/ -O3 -g -Wall -c -fmessage-length=0 --sysroot=$(VITIS_PLATFORM_DIR)/sw/u96v2_sbc_base/PetaLinux/sysroot/aarch64-xilinx-linux
LDFLAGS += -lxilinxopencl -lpthread -lrt -ldl -lcrypt -lstdc++ -L$(VITIS_PLATFORM_DIR)/sw/u96v2_sbc_base/PetaLinux/sysroot/aarch64-xilinx-linux/usr/lib/ --sysroot=$(VITIS_PLATFORM_DIR)/sw/u96v2_sbc_base/PetaLinux/sysroot/aarch64-xilinx-linux

# hardware compiler shared settings
VPP_OPTS = --target hw

#
# OpenCL kernel files
#
XO := encoder.xo
XCLBIN := encoder.xclbin
ALL_MESSAGE_FILES = $(subst .xo,.mdb,$(XO)) $(subst .xclbin,.mdb,$(XCLBIN))

#
# Client files
#
CLIENT_SOURCES = Client/client.cpp
CLIENT_EXE = client

#
# host files
#
HOST_SOURCES = Encoder/cdc.cpp Encoder/sha.cpp Encoder/dedup.cpp Encoder/lzw.cpp Encoder/host.cpp Server/server.cpp Encoder/sha256/sha256.cpp Encoder/Utilities.cpp Encoder/fast_cdc.cpp
HOST_OBJECTS=$(HOST_SOURCES:.cpp=.o)
HOST_EXE = encoder

$(HOST_EXE): $(HOST_OBJECTS)
	$(HOST_CXX) -I./Encoder/ -pthread -o "$@" $(+) $(LDFLAGS)

THREAD_HOST_SOURCES = Encoder/cdc.cpp Encoder/sha.cpp Encoder/dedup.cpp Encoder/lzw.cpp Encoder/thread_host.cpp Server/server.cpp Encoder/sha256/sha256.cpp Encoder/Utilities.cpp Encoder/fast_cdc.cpp
THREAD_HOST_OBJECTS=$(THREAD_HOST_SOURCES:.cpp=.o)
THREAD_HOST_EXE = thread_encoder

$(THREAD_HOST_EXE): $(THREAD_HOST_OBJECTS)
	$(HOST_CXX) -I./Encoder/ -pthread -o "$@" $(+) $(LDFLAGS)

SERVER_SOURCES = Server/encoder.cpp Server/server.cpp
SERVER_OBJECTS =$(SERVER_SOURCES:.cpp=.o)
SERVER_EXE = server

DECODER_SOURCES = Decoder/Decoder.cpp
DECODER_OBJECTS =$(DECODER_SOURCES:.cpp=.o)
DECODER_EXE = decoder

PROJECT_SOURCES = Encoder/cdc.cpp Encoder/sha.cpp Encoder/dedup.cpp Encoder/lzw.cpp Encoder/main.cpp Server/server.cpp Encoder/sha256/sha256.cpp
PROJECT_OBJECTS =$(PROJECT_SOURCES:.cpp=.o)
PROJECT_EXE = encoder_cpu
PROJECT_EXE_CDC_DEBUG = encdc_debug
PROJECT_EXE_SHA_TEST = ensha_test
PROJECT_EXE_SHA_DEBUG = ensha_debug
PROJECT_EXE_MAIN_DEBUG = enmain_debug

.PHONY: cpu

cpu: $(PROJECT_EXE)

all: $(CLIENT_EXE) $(SERVER_EXE) $(DECODER_EXE) $(PROJECT_EXE)
proj_main_debug: $(PROJECT_EXE_MAIN_DEBUG)
proj_sha_test: $(PROJECT_EXE_SHA_TEST)
proj_sha_debug: $(PROJECT_EXE_SHA_DEBUG)
proj_cdc_debug: $(PROJECT_EXE_CDC_DEBUG)

$(CLIENT_EXE):
	g++ -O3 $(CLIENT_SOURCES) -o "$@"

$(PROJECT_EXE):
	g++ $(PROJECT_SOURCES) -I./Encoder/ -o "$@"

$(PROJECT_EXE_CDC_DEBUG):
	$(HOST_CXX) $(PROJECT_SOURCES) -DCDC_DEBUG -I./Encoder/ -o "$@"

$(PROJECT_EXE_SHA_TEST):
	$(HOST_CXX) $(PROJECT_SOURCES) -DSHA_TEST -I./Encoder/ -o" $@" 

$(PROJECT_EXE_SHA_DEBUG):
	$(HOST_CXX) $(PROJECT_SOURCES) -DSHA_DEBUG -I./Encoder/ -o "$@"

$(PROJECT_EXE_MAIN_DEBUG):
	$(HOST_CXX) $(PROJECT_SOURCES) -DMAIN_DEBUG -I./Encoder/ -o "$@"

$(SERVER_EXE): $(SERVER_OBJECTS)
	$(HOST_CXX) -o "$@" $(+) $(LDFLAGS)

$(DECODER_EXE): $(DECODER_OBJECTS)
	$(HOST_CXX) -g -o "$@" $(+) $(LDFLAGS)

.cpp.o:
	$(HOST_CXX) $(CXXFLAGS) -I./server -o "$@" "$<"

#
# primary build targets
#
.PHONY: fpga clean
fpga: package/sd_card.img

.NOTPARALLEL: clean

clean-cpu:
	-$(RM) $(SERVER_EXE) $(SERVER_OBJECTS) $(DECODER_EXE) \
		$(DECODER_OBJECTS) $(CLIENT_EXE) $(PROJECT_OBJECTS) \
		$(PROJECT_EXE) $(PROJECT_EXE_CDC_DEBUG) \
		$(PROJECT_EXE_MAIN_DEBUG) $(PROJECT_EXE_SHA_TEST) \
		$(PROJECT_EXE_SHA_DEBUG)

clean-host:
	-$(RM) $(HOST_EXE) $(HOST_OBJECTS) 

clean-accelerators:
	-$(RM) $(XCLBIN) $(XO) $(ALL_MESSAGE_FILES)
	-$(RM) *.xclbin.sh *.xclbin.info *.xclbin.link_summary* *.compile_summary
	-$(RMDIR) .Xil fpga/hls/proj_mmult

clean-package:
	-${RMDIR} package
	-${RMDIR} package.build

clean: clean-cpu clean-host clean-accelerators clean-package
	-$(RM) *.log *.package_summary
	-${RMDIR} _x .ipcache

#
# binary container: encoder.xclbin
#
$(XO): Encoder/lzw_vitis.cpp
	-@$(RM) $@
	$(VPP) $(VPP_OPTS) -k encoder --compile -I"$(<D)" --config ./design.cfg -o"$@" "$<"

$(XCLBIN): $(XO)
	$(VPP) $(VPP_OPTS) --link --config ./design.cfg -o"$@" $(+)

package/sd_card.img: $(HOST_EXE) $(XCLBIN) ./xrt.ini
	$(VPP) --package $(VPP_OPTS) --config ./package.cfg $(XCLBIN) \
		--package.out_dir package \
		--package.sd_file $(HOST_EXE) \
		--package.kernel_image $(PLATFORM_REPO_PATHS)/sw/$(VITIS_PLATFORM)/PetaLinux/image/image.ub \
		--package.rootfs $(PLATFORM_REPO_PATHS)/sw/$(VITIS_PLATFORM)/PetaLinux/rootfs/rootfs.ext4 \
		--package.sd_file $(XCLBIN) \
		--package.sd_file ./xrt.ini
