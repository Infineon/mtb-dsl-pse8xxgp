################################################################################
# \file defines.mk
#
# \brief
# Defines, needed for the PSOC(TM) Edge build recipe.
#
################################################################################
# \copyright
# Copyright (c) 2021-2026, Infineon Technologies AG, or an affiliate of
# Infineon Technologies AG. All rights reserved.
# SPDX-License-Identifier: Apache-2.0
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
################################################################################

ifeq ($(WHICHFILE),true)
$(info Processing $(lastword $(MAKEFILE_LIST)))
endif

_MTB_RECIPE__NOT_SUPPORT_LEGACY_MEMCALC:=1

include $(MTB_TOOLS__RECIPE_DIR)/make/recipe/defines_common.mk

ifneq ($(OTA_SUPPORT),)
# OTA post-build script needs python.
CY_PYTHON_REQUIREMENT=true
endif

################################################################################
# General
################################################################################
_MTB_RECIPE__PROGRAM_INTERFACE_SUPPORTED:=KitProg3 JLink
#
# Compatibility interface for this recipe make
#
MTB_RECIPE__INTERFACE_VERSION:=2
MTB_RECIPE__EXPORT_INTERFACES:=4 5

MTB_RECIPE__NINJA_SUPPORT:=1 2

ifeq ($(MTB_TYPE),PROJECT)
_MTB_RECIPE__IS_MULTI_CORE_APPLICATION:=true
endif

_MTB_RECIPE__OPENOCD_CHIP_NAME:=cat1d

#
# List the supported toolchains
#
ifdef CY_SUPPORTED_TOOLCHAINS
MTB_SUPPORTED_TOOLCHAINS?=$(CY_SUPPORTED_TOOLCHAINS)
else
MTB_SUPPORTED_TOOLCHAINS?=GCC_ARM IAR ARM LLVM_ARM
endif

ifeq ($(TOOLCHAIN),ARM)
PC_SYMBOL=__main
SP_SYMBOL=Image$$$$ARM_LIB_STACK$$$$ZI$$$$Limit
else ifeq ($(TOOLCHAIN),IAR)
PC_SYMBOL=Reset_Handler
SP_SYMBOL=CSTACK$$$$Limit
else ifeq ($(TOOLCHAIN),GCC_ARM)
PC_SYMBOL=Reset_Handler
SP_SYMBOL=__StackTop
endif

#
# Define the default device mode
#
VCORE_ATTRS?=SECURE

# MVE support
# If MVE is not available on device then MVE_SELECT=NO_MVE.
# If MVE is available on device and VFP_SELECT=softfloat, then MVE_SELECT=MVE-I,
# else MVE_SELECT=<empty> (MVE-F mode).
ifeq ($(filter $(CORE_NAME)_MVE_PRESENT,$(DEVICE_$(DEVICE)_FEATURES)),)
MVE_SELECT?=NO_MVE
else
ifeq ($(VFP_SELECT),softfloat)
MVE_SELECT?=MVE-I
else
MVE_SELECT?=
endif
endif

ifeq ($(APPTYPE), ram)
_MTB_RECIPE__APPTYPE_DIR:=ram
else
_MTB_RECIPE__APPTYPE_DIR:=flash
endif

ifeq ($(MTB_RECIPE__CORE),CM33)
_MTB_RECIPE__OPENOCD_CORE_NAME:=cm33
else
_MTB_RECIPE__OPENOCD_CORE_NAME:=cm55
endif

#
# Memory erasure message customization
#
ifeq ($(MTB_ERASE_EXT_MEM),)
_MTB_RECIPE__ERASING_TARGET_MSG:="Erasing the target device's internal memory only..."
else
_MTB_RECIPE__ERASING_TARGET_MSG:="Erasing the target device's internal and external memories..."
endif

################################################################################
# Include device specific defines
################################################################################

include $(MTB_TOOLS__RECIPE_DIR)/make/recipe/defines_pse8xxgp.mk

# main launch configs defines

# helper defines

_MTB_RECIPE__ECLIPSE_QUOT=&quot;
_MTB_RECIPE__ECLIPSE_NEWLINE:=&\#13;&\#10;

_MTB_RECIPE__ECLIPSE_ADD_SYMBOL_CMD:=\#add-symbol-file &lt;other_proj_elf_file_path&gt;
_MTB_RECIPE__PRJ_DIR_NAME := $(notdir $(realpath $(MTB_TOOLS__PRJ_DIR)))
_MTB_RECIPE__ECLIPSE_ADD_SYMBOL_PLACEHOLDER = $(if $(MTB_COMBINE_SIGN_$(_MTB_RECIPE__PRJ_DIR_NAME)_HEX_FILES),,$(_MTB_RECIPE__ECLIPSE_ADD_SYMBOL_CMD)$(_MTB_RECIPE__ECLIPSE_NEWLINE))
_MTB_RECIPE__ECLIPSE_PROG_FILE = $(if $(MTB_COMBINE_SIGN_$(_MTB_RECIPE__PRJ_DIR_NAME)_HEX_FILES),$${cy_prj_path}/$(MTB_COMBINE_SIGN_$(_MTB_RECIPE__COMBINE_SIGN_IDX)_HEX_PATH),$(_MTB_RECIPE__ECLIPSE_PRJ_PROG_FILE))

ifneq ($(PROG_FILE),)
ifeq (,$(_MTB_RECIPE__IS_MULTI_CORE_APPLICATION))
_MTB_RECIPE__VSCODE_HEX_FILE:=$(PROG_FILE)
_MTB_RECIPE__ECLIPSE_PROG_FILE:=$(PROG_FILE)
endif
endif

# Debug

_MTB_RECIPE__ECLIPSE_OPENOCD_DEBUG_DO_CONTINUE:=true
_MTB_RECIPE__ECLIPSE_OPENOCD_ATTACH_DO_CONTINUE:=false
_MTB_RECIPE__ECLIPSE_OPENOCD_ATTACH_GDB_CLIENT_OTHER_COMMANDS:=set mem inaccessible-by-default off$(_MTB_RECIPE__ECLIPSE_NEWLINE)set remotetimeout 60
_MTB_RECIPE__ECLIPSE_OPENOCD_ATTACH_SET_STOP_AT:=true
_MTB_RECIPE__ECLIPSE_OPENOCD_ATTACH_STOP_AT:=main
_MTB_RECIPE__ECLIPSE_OPENOCD_ATTACH_ATTR_BUILD_BEFORE_LAUNCH:=0
_MTB_RECIPE__ECLIPSE_OPENOCD_DEBUG_MULTICORE_SECOND_CONFIG_NAME:=Add CM55 to CM33

_MTB_RECIPE__ECLIPSE_OPENOCD_DEBUG_GDB_CLIENT_OTHER_COMMANDS:=set mem inaccessible-by-default off$(_MTB_RECIPE__ECLIPSE_NEWLINE)set remotetimeout 500

_MTB_RECIPE__ECLIPSE_OPENOCD_DEBUG_LOAD_IMAGE=false

_MTB_RECIPE__ECLIPSE_OPENOCD_ATTACH_POST_TARGET_COMMANDS=-c $(_MTB_RECIPE__ECLIPSE_QUOT)$(_MTB_RECIPE__OPENOCD_CHIP_NAME).$(_MTB_RECIPE__OPENOCD_CORE_NAME) configure -rtos auto -rtos-wipe-on-reset-halt 1$(_MTB_RECIPE__ECLIPSE_QUOT)$(_MTB_RECIPE__ECLIPSE_NEWLINE)-c $(_MTB_RECIPE__ECLIPSE_QUOT)gdb_breakpoint_override hard$(_MTB_RECIPE__ECLIPSE_QUOT)


## JLink
_MTB_RECIPE__ECLIPSE_JLINK_DEBUG_FIRST_RESET_SPEED=500
_MTB_RECIPE__ECLIPSE_JLINK_DEBUG_GDB_CLIENT_OTHER_COMMANDS=set mem inaccessible-by-default off$(_MTB_RECIPE__ECLIPSE_NEWLINE)set remotetimeout 60
_MTB_RECIPE__ECLIPSE_JLINK_DEBUG_GDB_SERVER_DEVICE_NAME=$(_MTB_RECIPE__JLINK_DEVICE_CFG)
_MTB_RECIPE__ECLIPSE_JLINK_ATTACH_GDB_SERVER_DEVICE_NAME=$(_MTB_RECIPE__JLINK_DEVICE_CFG)
_MTB_RECIPE__ECLIPSE_JLINK_ATTACH_GDB_CLIENT_OTHER_COMMANDS=set mem inaccessible-by-default off$(_MTB_RECIPE__ECLIPSE_NEWLINE)set remotetimeout 60
ifeq ($(MTB_RECIPE__CORE),CM33)
_MTB_RECIPE__ECLIPSE_JLINK_ATTACH_GDB_PORT_NUMBER=2331
_MTB_RECIPE__ECLIPSE_JLINK_ATTACH_SWO_PORT_NUMBER=2332
_MTB_RECIPE__ECLIPSE_JLINK_ATTACH_TELNET_PORT_NUMBER=2333
_MTB_RECIPE__ECLIPSE_JLINK_ATTACH_OTHER_RUN_COMMANDS=$(_MTB_RECIPE__ECLIPSE_ADD_SYMBOL_PLACEHOLDER)
_MTB_RECIPE__ECLIPSE_JLINK_DEBUG_GDB_PORT_NUMBER=2331
_MTB_RECIPE__ECLIPSE_JLINK_DEBUG_SWO_PORT_NUMBER=2332
_MTB_RECIPE__ECLIPSE_JLINK_DEBUG_TELNET_PORT_NUMBER=2333
else
_MTB_RECIPE__ECLIPSE_JLINK_ATTACH_GDB_PORT_NUMBER=2334
_MTB_RECIPE__ECLIPSE_JLINK_ATTACH_SWO_PORT_NUMBER=2335
_MTB_RECIPE__ECLIPSE_JLINK_ATTACH_TELNET_PORT_NUMBER=2336
_MTB_RECIPE__ECLIPSE_JLINK_DEBUG_GDB_PORT_NUMBER=2334
_MTB_RECIPE__ECLIPSE_JLINK_DEBUG_SWO_PORT_NUMBER=2335
_MTB_RECIPE__ECLIPSE_JLINK_DEBUG_TELNET_PORT_NUMBER=2336
endif

_MTB_RECIPE__ECLIPSE_JLINK_LAUNCH_GROUP_0_ACTION=DELAY
_MTB_RECIPE__ECLIPSE_JLINK_DEBUG_MULTICORE_GDB_CLIENT_OTHER_COMMANDS=set mem inaccessible-by-default off$(_MTB_RECIPE__ECLIPSE_NEWLINE)set remotetimeout 90
