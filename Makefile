# Duke Nukem 3D (JFDuke3D, with JFBuild, JFMACT and JFAudioLib) on maxsdk: the PS2 port.
#
#   make submodules      the submodules, with ps2/patches applied (once)
#   make [OUT=<dir>] [FRAMES=n] [ARGS="-v1 -l1"]   $(OUT)/duke3d.elf
#   make run             duke3d.elf in PCSX2 with the game data in
#                        build/pcsx2-test/duke3d (DUKEDATA: a directory with
#                        DUKE3D.GRP, copied there once)
#
# Polymost, Build's GL renderer, draws through libs/libgl; ps2/ has the base
# layer (video, input, timer), the GL table and the sound driver. The upstream
# Makefile is Makefile.jf.

HERE     := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
PS2MAX   ?= $(abspath $(HERE)/../..)
MAXSDK   ?= $(PS2MAX)/libs/maxsdk
MAXSDK_OUT ?= $(PS2MAX)/build/libs/maxsdk
MXGL     ?= $(PS2MAX)/libs/libgl
LIBOUT   := $(PS2MAX)/build/apps/duke3d
OUT      ?= $(LIBOUT)
# all in physical memory: the game fits, and PCSX2's recompiler has no VM
MX_MEMORY ?= phys
# under mx-kernel, all in core (docs/kernel.md in maxsdk)
MX_KERNEL ?= 1
all: $(OUT)/duke3d.elf
include $(MXGL)/mk/mxgl.mk
include $(MAXSDK)/mk/maxsdk.mk
MX_CORE_OBJS = $(MXGL_OBJS)

PICOLIBC_SRC ?= $(PS2MAX)/libs/picolibc
# this port's picolibc (POSIX I/O), as apps/halflife's
PICOLIBC := $(LIBOUT)/picolibc/install
PICOLIBC_A := $(PICOLIBC)/lib/libc.a
LIBRT    = $(MAXSDK_OUT)/librt.a
LIBMXC_OBJS = $(OUT)/libc/glue.o $(MAXSDK_OUT)/libc/compat.c.o $(MAXSDK_OUT)/libc/atomic.c.o \
              $(MAXSDK_OUT)/libc/malloc.c.o
LIBC_LIBS = $(LIBMXC_OBJS) $(PICOLIBC_A) $(MX_LIB) $(LIBRT)

TARGET   := --target=mips64el -mcpu=r5900 -mabi=n32 -fno-pic -mno-abicalls -G0
OPT      ?= -O2
JB       := $(HERE)/jfbuild
JA       := $(HERE)/jfaudiolib
JM       := $(HERE)/jfmact
P        := $(HERE)/ps2
DCFLAGS  := $(TARGET) $(OPT) -std=gnu99 -w -MMD -fno-common -ffunction-sections -fdata-sections \
            -fno-strict-aliasing -fsigned-char -ffast-math -fapprox-func -mvu0 \
            -nostdlibinc -isystem $(P)/include -isystem $(PICOLIBC)/include -D__PS2__ -D_GNU_SOURCE -DNDEBUG \
            -DGL_GLEXT_PROTOTYPES -DUSE_POLYMOST=1 -DUSE_OPENGL=2 -DRENDERTYPESDL=1 -DHAVE_PS2 \
            -I$(JB)/include -I$(JB)/src -I$(JM) -I$(JA)/include -I$(JA)/src -I$(HERE)/src -I$(P) \
            -I$(OUT)/gen -I$(MXGL_INC) -I$(MAXSDK)/ee/include $(MXGL_DEFS) $(EXTRA_CFLAGS)
# the FPU has no doubles (ps2/ps2_float.h)
FLOATFLAGS := -cl-single-precision-constant -include $(P)/ps2_float.h

ENGINE   := a-c asmprot baselayer cache1d compat crc32 defs engine kplib mmulti_null osd pragmas \
            scriptfile textfont talltextfont smalltextfont polymost hightile mdsprite polymosttex \
            polymosttexcache
GAME     := game actors gamedef global menues player premap sector rts config osdfuncs osdcmds \
            grpscan sounds version
MACT     := util_lib file_lib control keyboard mouse mathutil scriplib animlib
AUDIO    := asssys cd drivers driver_nosound fx_man midi mix mixst multivoc music pitch
PS2SRC   := ps2_layer ps2_gl ps2_posix ps2_audio ps2_softfp

OBJS     := $(patsubst %,$(OUT)/jb/%.o,$(ENGINE)) $(patsubst %,$(OUT)/game/%.o,$(GAME)) \
            $(patsubst %,$(OUT)/mact/%.o,$(MACT)) $(patsubst %,$(OUT)/audio/%.o,$(AUDIO)) \
            $(patsubst %,$(OUT)/ps2/%.o,$(PS2SRC))

CDEPS    := $(PICOLIBC_A) $(OUT)/gen/runcfg.h
$(OUT)/jb/polymost.o $(OUT)/jb/mdsprite.o $(OUT)/jb/hightile.o $(OUT)/jb/defs.o $(OUT)/jb/scriptfile.o: DCFLAGS += $(FLOATFLAGS)
$(OUT)/jb/%.o: $(JB)/src/%.c $(CDEPS)
	@mkdir -p $(dir $@)
	$(CC) $(DCFLAGS) -c $< -o $@
$(OUT)/game/%.o: $(HERE)/src/%.c $(CDEPS)
	@mkdir -p $(dir $@)
	$(CC) $(DCFLAGS) -c $< -o $@
$(OUT)/mact/%.o: $(JM)/%.c $(CDEPS)
	@mkdir -p $(dir $@)
	$(CC) $(DCFLAGS) -c $< -o $@
$(OUT)/audio/%.o: $(JA)/src/%.c $(CDEPS)
	@mkdir -p $(dir $@)
	$(CC) $(DCFLAGS) -c $< -o $@
$(OUT)/ps2/%.o: $(P)/%.c $(CDEPS)
	@mkdir -p $(dir $@)
	$(CC) $(DCFLAGS) -c $< -o $@

# maxsdk's libc glue with its read, write and _exit renamed: ps2/ps2_posix.c
# has the POSIX ones
$(OUT)/libc/glue.o: $(MAXSDK)/ee/libc/glue.c $(PICOLIBC_A) $(HERE)/Makefile
	@mkdir -p $(dir $@)
	$(CC) $(filter-out -ffreestanding,$(CFLAGS)) -nostdlibinc -isystem $(PICOLIBC)/include \
	  -Dread=mx_glue_read -Dwrite=mx_glue_write -D_exit=mx_glue_exit -c $< -o $@

# FRAMES: end after that many frames with a picture (host:duke3d.tga);
# PLAY: walk by scripted pad input; STATS: libgl's counters on (or R3);
# PROF: profile the run's second half, or from frame n; ARGS: more of the command line;
# MODE: libgl's mode whatever the settings (1 NTSC, 2 PAL, 3 1080i, +256 beam racing, +512 24-bit)
# KEYS: pad keys by script from frame 300, each 30 frames: s, x, o, u, d, l, r, . none
RUNCFG := $(OUT)/gen/runcfg.h
$(shell mkdir -p $(OUT)/gen; printf '#define PS2_FRAMES %s\n#define PS2_ARGS "%s"\n#define PS2_PLAY %s\n#define PS2_PROF %s\n#define PS2_STATS %s\n#define PS2_MODE (%s)\n#define PS2_KEYS "%s"\n' \
  '$(or $(FRAMES),0)' '$(ARGS)' '$(or $(PLAY),0)' '$(or $(PROF),0)' '$(or $(STATS),0)' '$(or $(MODE),-1)' '$(KEYS)' > $(RUNCFG).new; \
  cmp -s $(RUNCFG).new $(RUNCFG) || mv $(RUNCFG).new $(RUNCFG); rm -f $(RUNCFG).new)

# -z muldefs: ps2/ps2_softfp.c's double arithmetic before librt's
$(MXGL_OBJS): $(PICOLIBC_A)
$(OUT)/duke3d.elf: $(OBJS) $(MXGL_OBJS) $(HLSL_OBJS) $(MX_LIB) $(MX_CRT0) $(MX_LD) $(LIBC_LIBS)
	$(call link,$@,-z muldefs --defsym=__stack_size=0x100000 $(MX_CRT0) $(OBJS) $(MXGL_OBJS) $(HLSL_OBJS) $(LIBC_LIBS))

$(PICOLIBC_A):
	@mkdir -p $(LIBOUT)/picolibc
	sed "s|@LLVM@|$(LLVM)|g" $(MAXSDK)/ee/libc/cross-r5900.txt.in > $(LIBOUT)/picolibc/cross.txt
	meson setup --wipe $(LIBOUT)/picolibc/build-meson $(PICOLIBC_SRC) \
	  --cross-file $(LIBOUT)/picolibc/cross.txt --prefix=$(PICOLIBC) -Dmultilib=false \
	  -Dpicocrt=false -Dsemihost=false -Dtests=false -Dthread-local-storage=false \
	  -Dposix-io=true -Dspecsdir=none -Dio-long-long=true -Dbuildtype=release
	ninja -C $(LIBOUT)/picolibc/build-meson install

submodules:
	git submodule update --init jfbuild jfmact jfaudiolib
	ps2/patches.sh apply

# ---- running --------------------------------------------------------------------
DUKEDATA ?= .
RUNDIR   := $(PS2MAX)/build/pcsx2-test
run: $(OUT)/duke3d.elf
	@mkdir -p $(RUNDIR)/duke3d
	@[ -f $(RUNDIR)/duke3d/duke3d.grp ] || cp -v "$(DUKEDATA)/DUKE3D.GRP" $(RUNDIR)/duke3d/duke3d.grp
	$(PCSX2_RUN) $(OUT)/duke3d.elf $(or $(TIMEOUT),300)

clean:
	rm -rf $(filter-out $(LIBOUT)/picolibc,$(wildcard $(OUT)/*))

.PHONY: all submodules run clean

-include $(shell find $(OUT) -name '*.d' -not -path '*/picolibc/*' 2>/dev/null)
