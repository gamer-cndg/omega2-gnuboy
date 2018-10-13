# C compiler
CC:= mipsel-openwrt-linux-gcc
# path where the toolchain is
TOOLCHAIN_ROOT_DIR:= /home/max/source/staging_dir/target-mipsel_24kc_musl-1.1.16
#path where the omega_includes and omega_libs folder are
OMEGA_DIR:= /home/max/omega

# additional includes from toolchain
INCLUDE_DIRS:=$(TOOLCHAIN_ROOT_DIR)/usr/include
LIB_DIRS:=$(TOOLCHAIN_ROOT_DIR)/usr/lib

#libraries to link against.
#for testing purposes, link against everything
LDFLAGS_PROGRAM = -loniondebug -lonioni2c -lpthread -lasound -lwiiclassic -lusb -lusb-1.0 -ldlo
# -lili9225
# -lonionspi
# -lugpio
CFLAGS:= -O3 -mtune=24kec -mips32r2 -mdsp -ggdb -g -Wall -Wextra
IFLAGS:= -I $(INCLUDE_DIRS) -I $(OMEGA_DIR)/omega_includes -I /home/max/direct_displaylink_try/libdlo/install_dir/include

#space seperated list of all C files
PROGRAM_SOURCES = cpu.c     exports.c  keytable.c  main.c      path.c    rcvars.c   sound.c \
				  debug.c   fastmem.c  lcd.c       mem.c       rccmds.c  refresh.c  split.c \
				  emu.c     hw.c       lcdc.c      rcfile.c  rtc.c \
				  events.c  inflate.c  loader.c    palette.c   rckeys.c  save.c \
				  sys_linux.c sys_joy.c sys_video.c sys_sound.c

EXECUTABLE:= gnuboy

export STAGING_DIR="$TOOLCHAIN_ROOT_DIR/staging_dir/"

.PHONY : program all clean all

program:
	$(CC) -o $(EXECUTABLE) $(CFLAGS) $(IFLAGS) -L $(LIB_DIRS) -L $(OMEGA_DIR)/omega_libs -L. -L/home/max/direct_displaylink_try/libdlo/install_dir/lib  $(PROGRAM_SOURCES) $(LDFLAGS_PROGRAM)

all: | program

upload: | all
	sshpass -p "onioneer" scp $(EXECUTABLE) root@192.168.1.150:/root/.

clean:
	rm -rf $(EXECUTABLE)
