#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------

.SECONDARY:

ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM in your environment. export DEVKITARM=<path to>devkitARM")
endif

include $(DEVKITARM)/ds_rules

#---------------------------------------------------------------------------------
# SETTINGS
#---------------------------------------------------------------------------------
TARGET		:=	$(shell basename $(CURDIR))
BUILD		:=	build
SOURCES		:=	source
DATA		:=
INCLUDES	:=	include
GRAPHICS	:=	data

#Directorio de música
MUSIC       :=  audio

#---------------------------------------------------------------------------------
# COMPILER FLAGS
#---------------------------------------------------------------------------------
ARCH	:=	-march=armv5te -mtune=arm946e-s -mthumb

CFLAGS	:=	-g -Wall -O2 -ffunction-sections -fdata-sections\
		$(ARCH)

CFLAGS	+=	$(INCLUDE) -DARM9

CXXFLAGS	:= $(CFLAGS) -fno-rtti -fno-exceptions

ASFLAGS	:=	-g $(ARCH)
LDFLAGS	=	-specs=ds_arm9.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map)

#---------------------------------------------------------------------------------
# LIBRARIES
#---------------------------------------------------------------------------------
#Agregamos -lmm9 para MaxMod
LIBS := -lfat -lnds9 -lmm9

LIBDIRS	:=	$(LIBNDS)

#---------------------------------------------------------------------------------
ifneq ($(BUILD),$(notdir $(CURDIR)))
#---------------------------------------------------------------------------------

export OUTPUT	:=	$(CURDIR)/$(TARGET)

export VPATH	:=	$(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
					$(foreach dir,$(DATA),$(CURDIR)/$(dir)) \
					$(foreach dir,$(GRAPHICS),$(CURDIR)/$(dir))

export DEPSDIR	:=	$(CURDIR)/$(BUILD)

CFILES		:= $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES	:= $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
SFILES		:= $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
BINFILES	:= $(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.*)))
PNGFILES	:= $(foreach dir,$(GRAPHICS),$(notdir $(wildcard $(dir)/*.png)))

#  Archivos de audio
export AUDIOFILES := $(foreach dir,$(notdir $(wildcard $(MUSIC)/*.*)),$(CURDIR)/$(MUSIC)/$(dir))

# selector linker
ifeq ($(strip $(CPPFILES)),)
	export LD	:=	$(CC)
else
	export LD	:=	$(CXX)
endif

#  Agregar soundbank.bin a binarios
BINFILES += soundbank.bin

export OFILES	:=	$(addsuffix .o,$(BINFILES)) \
					$(PNGFILES:.png=.o) \
					$(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)

export INCLUDE	:=	$(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
					$(foreach dir,$(LIBDIRS),-I$(dir)/include) \
					-I$(CURDIR)/$(BUILD)

export LIBPATHS	:= $(foreach dir,$(LIBDIRS),-L$(dir)/lib)

.PHONY: $(BUILD) clean

$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

clean:
	@echo clean ...
	@rm -fr $(BUILD) $(TARGET).elf $(TARGET).nds $(TARGET).ds.gba

#---------------------------------------------------------------------------------
else
#---------------------------------------------------------------------------------

DEPENDS	:=	$(OFILES:.o=.d)

#---------------------------------------------------------------------------------
# BUILD TARGETS
#---------------------------------------------------------------------------------
$(OUTPUT).nds	: 	$(OUTPUT).elf
$(OUTPUT).elf	:	$(OFILES)

#---------------------------------------------------------------------------------
#  Convertir audio -> soundbank
#---------------------------------------------------------------------------------
soundbank.bin soundbank.h : $(AUDIOFILES)
	@echo ">>> Generando soundbank..."
	@mmutil $^ -d -osoundbank.bin -hsoundbank.h

#---------------------------------------------------------------------------------
%.bin.o	:	%.bin
	@echo $(notdir $<)
	@$(bin2o)

%.s %.h	: %.png %.grit
	grit $< -fts -o$*

-include $(DEPENDS)

endif
#---------------------------------------------------------------------------------
