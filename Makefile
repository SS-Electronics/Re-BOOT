# File:        Makefile
# Author:      Subhajit Roy  
#              subhajitroy005@gmail.com 

# Moudle:      Build  
# Info:        Build executables of Re-Boot application              
# Dependency:  Configuration

# This file is part of Re-BOOT Project.

# Re-BOOT is free software: you can redistribute it and/or 
# modify it under the terms of the GNU General Public License 
# as published by the Free Software Foundation, either version 
# 3 of the License, or (at your option) any later version.

# Re-BOOT is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of 
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the 
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License 
# along with Re-BOOT. If not, see <https://www.gnu.org/licenses/>.



##############################################################
# Target OS (default Linux)

OS ?= Linux
##############################################################


##############################################################
# Directory setup
BUILD   := build

# Subdirectories
SUBDIRS := driver thread comm utility init

INCLUDES := -Iinclude -Iconfig

LINKER_SCRIPT :=

SYMBOL_DEF :=

TARGET_SYSMBOL_DEF +=

OPENOCD_INTERFACE :=

OPENOCD_TARGET:= 
##############################################################


##############################################################
# OS specific configuration

ifeq ($(OS),Linux)
CC := gcc
CPP := g++
CC_EXTRA_FLAGS += -D__linux__ -pthread
endif


ifeq ($(OS),Win)
CC := x86_64-w64-mingw32-gcc
CPP := x86_64-w64-mingw32-g++
CC_EXTRA_FLAGS += -D_WIN64 -lws2_32
endif

##############################################################




##############################################################
# Output configuration

ifeq ($(OS),Linux)
TARGET_NAME := re-boot
TARGET_EXT  :=
endif

ifeq ($(OS),Win)
TARGET_NAME := re-boot.exe
TARGET_EXT  := .exe
endif

TARGET := $(TARGET_NAME)

##############################################################




##############################################################
# Object list (collected from subdir Makefiles)
include $(patsubst %, %/Makefile, $(SUBDIRS))

# Prepend build/ to all objects
OBJS := $(addprefix $(BUILD)/, $(obj-y))

export INCLUDES

export LINKER_SCRIPT

export SYMBOL_DEF


##############################################################





























##############################################################
# build stages 
all: $(TARGET)

# Link final kernel
$(TARGET): $(OBJS) | $(BUILD)
	@echo '**********************************************'
	@echo 'Linking executable for $(OS)...'
	@echo '**********************************************'

	@$(CPP) $(TARGET_SYSMBOL_DEF) $(SYMBOL_DEF) $(CC_LINKER_FLAGS) -o $@ $(OBJS)

	@echo '##############################################'
	@echo ' '
	@echo 'Build completed!:   $@'
	@echo ' '
ifeq ($(OS),Linux)
	@size $@
endif
	@echo '##############################################'

# Rule for compiling into build dir
$(BUILD)/%.o: %.c | $(BUILD)
	@echo 'Building C Source $< ...'
	@echo '----------------------------------------------'
	@mkdir -p $(dir $@)
	@$(CC) $(TARGET_SYSMBOL_DEF) $(SYMBOL_DEF) $(CC_OPTIMIZATION) $(CC_EXTRA_FLAGS) $(CC_INPUT_STD) $(CC_WARNINGS) $(CC_TARGET_PROP) $(INCLUDES) -c $< -o $@

# Create build directory
$(BUILD):
	@mkdir -p $(BUILD)
	@echo '##############################################'
	@echo 'Bulding sources...'
	@echo '##############################################'



clean:
	@rm -rf $(BUILD)
	@echo '##############################################'
	@echo ' '
	@echo 'Clean completed!'
	@echo ' '
	@echo '##############################################'

##############################################################




##############################################################

#               Doc Generator 

# Path to Doxygen executable
DOXYGEN ?= doxygen

# Doxygen configuration file
DOXYFILE ?= Doxyfile

# Documentation output directory (must match Doxyfile OUTPUT_DIRECTORY + HTML_OUTPUT)
DOC_DIR ?= docs

.PHONY: docs clean-docs



# Generate documentation
docs:
	@echo "Generating documentation..."
	$(DOXYGEN) $(DOXYFILE)
	@echo "Documentation generated in $(DOC_DIR)/html"

# Clean generated documentation (keep docs/ folder, remove only generated files)
clean-docs:
	@echo "Cleaning documentation..."
	@find $(DOC_DIR) -maxdepth 1 -name "*.html" -o -name "*.js" -o -name "*.css" \
	    -o -name "*.png" -o -name "search" | xargs rm -rf
	@echo "Documentation cleaned."


##############################################################