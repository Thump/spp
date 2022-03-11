############################################################################
#
# spp makefile
# Denis McLaughlin
# March 10, 2022
#
###########################################################################

# runtime default configuration values

# the base directory for all config files
BUILDDIR=build

# Fully Qualified Board Name
FQBN=esp8266:esp8266:generic

# port the Arduino is on
PORT=/dev/ttyUSB0

# version number
VERSION="1.0.0"

# app name: arduino-cli has weird requirements that the directory name has
# to match the source file and output file name, so be cautious changing this
NAME=spp

# arduino-cli program
ARDUINOCLI=arduino-cli


###########################################################################

SOURCE=spp.ino  jsbutton.h

BIN=$(BUILDDIR)/$(NAME).ino.bin

CLEAN=$(OBJ) $(AUX)

REALCLEAN=$(CLEAN) $(BIN)

#*******************************#

all : compile upload

compile : $(BIN)

$(BIN): $(SOURCE)
	$(ARDUINOCLI) compile --build-path $(BUILDDIR) --fqbn $(FQBN) $(NAME)

upload: $(BIN)
	$(ARDUINOCLI) upload --input-dir $(BUILDDIR) --port $(PORT) --fqbn $(FQBN) $(NAME)

clean:
	rm -f $(BUILDDIR)
