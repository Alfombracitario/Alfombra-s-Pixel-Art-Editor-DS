# -------------------------------------------------
# Ruta a BlocksDS
# -------------------------------------------------
BLOCKSDS ?= /opt/blocksds/core

#--------------------------------------------------
# Librerías
#--------------------------------------------------
LIBS		:= -lnds9 -lmm9
LIBDIRS		:= $(BLOCKSDS)/libs/maxmod

# -------------------------------------------------
# Información del ROM
# -------------------------------------------------
NAME            := A-Pix
GAME_TITLE      := A-Pix DS
GAME_SUBTITLE   := @Alfombracitario

# -------------------------------------------------
# Carpetas del proyecto
# -------------------------------------------------
SOURCEDIRS      := source
INCLUDEDIRS     := include
GFXDIRS         := graphics
AUDIODIRS       := audio

# -------------------------------------------------
# Características usadas
# -------------------------------------------------
USE_ARM9        := 1
USE_FAT         := 1
USE_MAXMOD      := 1

# -------------------------------------------------
# Opciones extra (opcional)
# -------------------------------------------------
CFLAGS          += -O3 -Wall
CXXFLAGS        += -O3 -Wall
DEFINES         += -DARM9

# -------------------------------------------------
# Makefile base BlocksDS
# -------------------------------------------------
include $(BLOCKSDS)/sys/default_makefiles/rom_arm9/Makefile
