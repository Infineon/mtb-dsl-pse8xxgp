################################################################################
# \file recipe_ide.mk
#
# \brief
# This make file defines the IDE export variables and target.
#
################################################################################
# \copyright
# Copyright (c) 2022-2026, Infineon Technologies AG, or an affiliate of
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

MTB_RECIPE__IDE_SUPPORTED:=eclipse vscode uvision5 ewarm8

# This is used for combiner-signer single-core projects in order to add proper Erase tasks to VSCode.
_MTB_RECIPE__VSCODE_TASKS_ERASE_MEMORIES_SUPPORTED:=internal external

_MTB_RECIPE__IDE_EXPORT_INTERFACE_VERSION=interface_version_4
_MTB_RECIPE__IDE_RECIPE_DIR:=$(MTB_TOOLS__RECIPE_DIR)/make/recipe/$(_MTB_RECIPE__IDE_EXPORT_INTERFACE_VERSION)
ifeq ($(_MTB_RECIPE__PROGRAM_INTERFACE_SUBDIR),KitProg3)
_MTB_RECIPE__HIDE_ADVANCED_PROGRAM:=false
endif
include $(_MTB_RECIPE__IDE_RECIPE_DIR)/recipe_ide_common.mk

# Path to debug certificate
ifneq ($(CY_DBG_CERTIFICATE_PATH),)
CY_DBG_CERTIFICATE_PATH_APPLICATION:=$(CY_DBG_CERTIFICATE_PATH)
_USER_DEFINED_CERT=true
else
CY_DBG_CERTIFICATE_PATH:=./packets/debug_token.bin
CY_DBG_CERTIFICATE_PATH_APPLICATION:=./packets/debug_token.bin
ifneq (,$(_MTB_RECIPE__IS_MULTI_CORE_APPLICATION))
CY_DBG_CERTIFICATE_PATH:=../packets/debug_token.bin
endif
endif

##############################################
# Eclipse VSCode
##############################################
_MTB_RECIPE__IDE_TEXT_DATA_FILE=$(MTB_TOOLS__OUTPUT_CONFIG_DIR)/recipe_ide_text_data.txt
_MTB_RECIPE__IDE_TEMPLATE_META_DATA_FILE:=$(MTB_TOOLS__OUTPUT_CONFIG_DIR)/recipe_ide_template_meta_data.txt
_MTB_RECIPE__ECLIPSE_TEMPLATE_REGEX_DATA_FILE:=$(MTB_TOOLS__OUTPUT_CONFIG_DIR)/recipe_eclipse_template_regex_data.txt
_MTB_RECIPE__VSCODE_TEMPLATE_REGEX_DATA_FILE:=$(MTB_TOOLS__OUTPUT_CONFIG_DIR)/recipe_vscode_template_regex_data.txt


_MTB_RECIPE__IDE_TEMPLATE_SUBDIR:=$(_MTB_RECIPE__APPTYPE_DIR)/$(_MTB_RECIPE__PROGRAM_INTERFACE_SUBDIR)

CY_QSPI_FLM_DIR_OUTPUT?=$(CY_QSPI_FLM_DIR)
ifeq ($(CY_QSPI_FLM_DIR_OUTPUT),)
_MTB_RECIPE__OPENOCD_QSPI_FLASHLOADER=
_MTB_RECIPE__OPENOCD_QSPI_FLASHLOADER_WITH_FLAG=
else
_MTB_RECIPE__OPENOCD_QSPI_FLASHLOADER=set QSPI_FLASHLOADER $(patsubst %/,%,$(CY_QSPI_FLM_DIR_OUTPUT))/PSE84_SMIF.FLM
_MTB_RECIPE__OPENOCD_QSPI_FLASHLOADER_APPLICATION=set QSPI_FLASHLOADER $(patsubst ../%/,%,$(CY_QSPI_FLM_DIR_OUTPUT))/PSE84_SMIF.FLM
_MTB_RECIPE__OPENOCD_QSPI_FLASHLOADER_WITH_FLAG=-c &quot;$(_MTB_RECIPE__OPENOCD_QSPI_FLASHLOADER)&quot;&\#13;&\#10;
_MTB_RECIPE__OPENOCD_QSPI_FLASHLOADER_APPLICATION_WITH_FLAG=-c &quot;$(_MTB_RECIPE__OPENOCD_QSPI_FLASHLOADER_APPLICATION)&quot;&\#13;&\#10;
endif

##############################################
# Common Eclipse/VSCode targets
##############################################

recipe_text_replacement_data_file:
	$(call mtb__file_write,$(_MTB_RECIPE__IDE_TEXT_DATA_FILE),&&_MTB_RECIPE__JLINK_CFG&&=$(_MTB_RECIPE__JLINK_DEVICE_CFG))
	$(call mtb__file_append,$(_MTB_RECIPE__IDE_TEXT_DATA_FILE),&&_MTB_RECIPE__JLINK_CM0_CFG&&=$(_MTB_RECIPE__JLINK_DEVICE_CM0_CFG))
	$(call mtb__file_append,$(_MTB_RECIPE__IDE_TEXT_DATA_FILE),&&PC_SYMBOL&&=$(PC_SYMBOL))
	$(call mtb__file_append,$(_MTB_RECIPE__IDE_TEXT_DATA_FILE),&&SP_SYMBOL&&=$(SP_SYMBOL))
	$(call mtb__file_append,$(_MTB_RECIPE__IDE_TEXT_DATA_FILE),&&_MTB_RECIPE__FIRST_APP_NAME&&=$(firstword $(MTB_APPLICATION_SUBPROJECTS)))
	$(call mtb__file_append,$(_MTB_RECIPE__IDE_TEXT_DATA_FILE),&&_MTB_RECIPE__SECOND_APP_NAME&&=$(lastword $(MTB_APPLICATION_SUBPROJECTS)))
	$(call mtb__file_append,$(_MTB_RECIPE__IDE_TEXT_DATA_FILE),&&_MTB_RECIPE__TARGET_PROCESSOR_NAME&&=$(MTB_RECIPE__CORE))
	$(call mtb__file_append,$(_MTB_RECIPE__IDE_TEXT_DATA_FILE),&&_MTB_RECIPE__DEVICE_DEBUG&&=$(_MTB_RECIPE__JLINK_DEVICE_CFG))
	$(call mtb__file_append,$(_MTB_RECIPE__IDE_TEXT_DATA_FILE),&&_MTB_RECIPE__OPENOCD_CM0_CFG&&=$(_MTB_RECIPE__OPENOCD_DEVICE_CM0_CFG))
	$(call mtb__file_append,$(_MTB_RECIPE__IDE_TEXT_DATA_FILE),&&_MTB_RECIPE__PROCESSOR_COUNT&&=2)
	$(call mtb__file_append,$(_MTB_RECIPE__IDE_TEXT_DATA_FILE),&&_MTB_RECIPE__HEX_FILE&&=$(_MTB_RECIPE__VSCODE_HEX_FILE))
	$(call mtb__file_append,$(_MTB_RECIPE__IDE_TEXT_DATA_FILE),&&_MTB_RECIPE__DBG_CERTIFICATE_PATH&&=$(CY_DBG_CERTIFICATE_PATH))
	$(call mtb__file_append,$(_MTB_RECIPE__IDE_TEXT_DATA_FILE),&&_MTB_RECIPE__DBG_CERTIFICATE_APPLICATION_PATH&&=$(CY_DBG_CERTIFICATE_PATH_APPLICATION))
	$(call mtb__file_append,$(_MTB_RECIPE__IDE_TEXT_DATA_FILE),&&_MTB_RECIPE__OPENOCD_RESET_TARGET&&=$(_MTB_RECIPE__OPENOCD_RESET_TARGET))
ifeq ($(MTB_RECIPE__CORE),CM33)
	$(call mtb__file_append,$(_MTB_RECIPE__IDE_TEXT_DATA_FILE),&&_MTB_RECIPE__TARGET_PROCESSOR_NUMBER&&=0)
	$(call mtb__file_append,$(_MTB_RECIPE__IDE_TEXT_DATA_FILE),&&_MTB_RECIPE__TARGET_PROCESSOR_NAME_LOWERCASE&&=cm33)
else
	$(call mtb__file_append,$(_MTB_RECIPE__IDE_TEXT_DATA_FILE),&&_MTB_RECIPE__TARGET_PROCESSOR_NUMBER&&=1)
	$(call mtb__file_append,$(_MTB_RECIPE__IDE_TEXT_DATA_FILE),&&_MTB_RECIPE__TARGET_PROCESSOR_NAME_LOWERCASE&&=cm55)
	$(call mtb__file_append,$(_MTB_RECIPE__IDE_TEXT_DATA_FILE),&&_MTB_RECIPE__CM55_PRJ_DIR&&=$(_MTB_RECIPE__IDE_PRJ_DIR_NAME))
endif

##############################################
# Eclipse
##############################################

eclipse_generate: recipe_text_replacement_data_file recipe_eclipse_text_replacement_data_file recipe_eclipse_meta_replacement_data_file recipe_eclipse_regex_replacement_data_file
eclipse_generate: MTB_CORE__EXPORT_CMDLINE += -textdata $(_MTB_RECIPE__IDE_TEXT_DATA_FILE) -metadata $(_MTB_RECIPE__IDE_TEMPLATE_META_DATA_FILE) -textregexdata $(_MTB_RECIPE__ECLIPSE_TEMPLATE_REGEX_DATA_FILE)

recipe_eclipse_regex_replacement_data_file:
	$(call mtb__file_write,$(_MTB_RECIPE__ECLIPSE_TEMPLATE_REGEX_DATA_FILE),^.*//triple-core only//.*$$=)
	$(call mtb__file_append,$(_MTB_RECIPE__ECLIPSE_TEMPLATE_REGEX_DATA_FILE),^.*//quad-core only//.*$$=)
	$(call mtb__file_append,$(_MTB_RECIPE__ECLIPSE_TEMPLATE_REGEX_DATA_FILE),^.*//penta-core only//.*$$=)

recipe_eclipse_text_replacement_data_file: recipe_text_replacement_data_file
	$(call mtb__file_append,$(_MTB_RECIPE__IDE_TEXT_DATA_FILE),&&_MTB_RECIPE__QSPI_CFG_PATH&&=$(_MTB_RECIPE__OPENOCD_QSPI_CFG_PATH_WITH_FLAG))
	$(call mtb__file_append,$(_MTB_RECIPE__IDE_TEXT_DATA_FILE),&&_MTB_RECIPE__QSPI_CFG_PATH_APPLICATION&&=$(_MTB_RECIPE__OPENOCD_QSPI_CFG_PATH_APPLICATION_WITH_FLAG))
	$(call mtb__file_append,$(_MTB_RECIPE__IDE_TEXT_DATA_FILE),&&_MTB_RECIPE__QSPI_FLASHLOADER&&=$(_MTB_RECIPE__OPENOCD_QSPI_FLASHLOADER_WITH_FLAG))
	$(call mtb__file_append,$(_MTB_RECIPE__IDE_TEXT_DATA_FILE),&&_MTB_RECIPE__QSPI_FLASHLOADER_APPLICATION&&=$(_MTB_RECIPE__OPENOCD_QSPI_FLASHLOADER_APPLICATION_WITH_FLAG))
	$(call mtb__file_append,$(_MTB_RECIPE__IDE_TEXT_DATA_FILE),&&_APP_NAME&&=$(_MTB_RECIPE__ECLIPSE_APPLICATION_NAME))
	$(call mtb__file_append,$(_MTB_RECIPE__IDE_TEXT_DATA_FILE),&&_PRJ_NAME&&=$(_MTB_RECIPE__ECLIPSE_PROJECT_NAME))
	$(call mtb__file_append,$(_MTB_RECIPE__IDE_TEXT_DATA_FILE),&&_MTB_RECIPE__OPENOCD_BOARD&&=$(if $(_MTB_RECIPE__OPENOCD_BOARD),-c &quot;$(_MTB_RECIPE__OPENOCD_BOARD)&quot;&#13;&#10;,))
ifneq ($(MTB_COMBINE_SIGN_$(_MTB_RECIPE__IDE_PRJ_DIR_NAME)_HEX_FILES),)
	$(call mtb__file_append,$(_MTB_RECIPE__IDE_TEXT_DATA_FILE),&&_PROGRAM_IMG_FOR_PGMGUI&&=$(call mtb__path_normalize,$(MTB_TOOLS__PRJ_DIR)/$(MTB_COMBINE_SIGN_$(_MTB_RECIPE__COMBINE_SIGN_IDX)_HEX_PATH)))
else
	$(call mtb__file_append,$(_MTB_RECIPE__IDE_TEXT_DATA_FILE),&&_PROGRAM_IMG_FOR_PGMGUI&&=$(_MTB_RECIPE__ECLIPSE_PRJ_PROG_FILE))
endif
	$(call mtb__file_append,$(_MTB_RECIPE__IDE_TEXT_DATA_FILE),&&_MTB_RECIPE__QSPI_FLM_FOR_PGMGUI&&=$(call mtb__path_normalize,$(MTB_TOOLS__PRJ_DIR)/$(patsubst %/,%,$(CY_QSPI_FLM_DIR_OUTPUT))/PSE84_SMIF.FLM))
ifeq ($(_USER_DEFINED_CERT),true)
	$(call mtb__file_append,$(_MTB_RECIPE__IDE_TEXT_DATA_FILE),&&_MTB_RECIPE__DBG_CERT_PATH_ABS&&=$(CY_DBG_CERTIFICATE_PATH))
else
	$(call mtb__file_append,$(_MTB_RECIPE__IDE_TEXT_DATA_FILE),&&_MTB_RECIPE__DBG_CERT_PATH_ABS&&=$(call mtb__path_normalize,$(MTB_TOOLS__PRJ_DIR)/$(CY_DBG_CERTIFICATE_PATH)))
endif

recipe_eclipse_meta_replacement_data_file:
	$(call mtb__file_write,$(_MTB_RECIPE__IDE_TEMPLATE_META_DATA_FILE),UUID=&&PROJECT_UUID&&)
ifneq (,$(_MTB_RECIPE__IS_MULTI_CORE_APPLICATION))
	$(call mtb__file_append,$(_MTB_RECIPE__IDE_TEMPLATE_META_DATA_FILE),APPLICATION_UUID=&&APPLICATION_UUID&&)
ifneq (,$(_MTB_RECIPE__IS_FIRST_PRJ))
	$(call mtb__file_append,$(_MTB_RECIPE__IDE_TEMPLATE_META_DATA_FILE),TEMPLATE_REPLACE=$(_MTB_RECIPE__IDE_RECIPE_DIR)/App/$(_MTB_RECIPE__IDE_TEMPLATE_SUBDIR)/multicore=../.mtbLaunchConfigs)
	$(call mtb__file_append,$(_MTB_RECIPE__IDE_TEMPLATE_META_DATA_FILE),TEMPLATE_REPLACE=$(_MTB_RECIPE__IDE_TEMPLATE_DIR)/eclipse/$(_MTB_RECIPE__IDE_TEMPLATE_TARGET_DIR)/App/$(_MTB_RECIPE__IDE_TEMPLATE_SUBDIR)=../.mtbLaunchConfigs)
	$(call mtb__file_append,$(_MTB_RECIPE__IDE_TEMPLATE_META_DATA_FILE),TEMPLATE_REPLACE=$(_MTB_RECIPE__IDE_RECIPE_DIR)/App/internal=../.mtbLaunchConfigs)
	$(call mtb__file_append,$(_MTB_RECIPE__IDE_TEMPLATE_META_DATA_FILE),TEMPLATE_REPLACE=$(_MTB_RECIPE__IDE_RECIPE_DIR)/App/external=../.mtbLaunchConfigs)
ifeq ($(_MTB_RECIPE__PROGRAM_INTERFACE_SUBDIR),KitProg3)
	$(call mtb__file_append,$(_MTB_RECIPE__IDE_TEMPLATE_META_DATA_FILE),TEMPLATE_REPLACE=$(_MTB_RECIPE__IDE_RECIPE_DIR)/App/$(_MTB_RECIPE__ERASE_TEMPLATE_FOLDER_NAME)=../.mtbLaunchConfigs)
endif
	$(call mtb__file_append,$(_MTB_RECIPE__IDE_TEMPLATE_META_DATA_FILE),UPDATE_APPLICATION_PREF_FILE=1)
else
	$(call mtb__file_append,$(_MTB_RECIPE__IDE_TEMPLATE_META_DATA_FILE),TEMPLATE_REPLACE=../.mtbLaunchConfigs=../.mtbLaunchConfigs)
endif #(,$(_MTB_RECIPE__IS_FIRST_PRJ))
else #(,$(_MTB_RECIPE__IS_MULTI_CORE_APPLICATION))
	$(call mtb__file_append,$(_MTB_RECIPE__IDE_TEMPLATE_META_DATA_FILE),TEMPLATE_REPLACE=$(_MTB_RECIPE__IDE_RECIPE_DIR)/Proj/single/$(_MTB_RECIPE__ERASE_TEMPLATE_FOLDER_NAME)=.mtbLaunchConfigs)
	$(call mtb__file_append,$(_MTB_RECIPE__IDE_TEMPLATE_META_DATA_FILE),TEMPLATE_REPLACE=$(_MTB_RECIPE__IDE_RECIPE_DIR)/Proj/single/internal=.mtbLaunchConfigs)
endif #(,$(_MTB_RECIPE__IS_MULTI_CORE_APPLICATION))
ifeq ($(MTB_COMBINE_SIGN_$(_MTB_RECIPE__IDE_PRJ_DIR_NAME)_HEX_FILES),)
ifneq (,$(_MTB_RECIPE__IS_MULTI_CORE_APPLICATION))
ifeq ($(MTB_RECIPE__CORE),CM55)
ifeq ($(_MTB_RECIPE__PROGRAM_INTERFACE_SUBDIR),KitProg3)
	$(call mtb__file_append,$(_MTB_RECIPE__IDE_TEMPLATE_META_DATA_FILE),TEMPLATE_REPLACE=$(_MTB_RECIPE__IDE_RECIPE_DIR)/Proj/$(_MTB_RECIPE__IDE_TEMPLATE_SUBDIR)/multicore/Add $(_MTB_RECIPE__PROGRAM_INTERFACE_LAUNCH_SUFFIX).launch=.mtbLaunchConfigs/$(_MTB_RECIPE__ECLIPSE_PROJECT_NAME) $(_MTB_RECIPE__ECLIPSE_OPENOCD_DEBUG_MULTICORE_SECOND_CONFIG_NAME) $(_MTB_RECIPE__PROGRAM_INTERFACE_LAUNCH_SUFFIX).launch)
endif
endif
endif
	$(call mtb__file_append,$(_MTB_RECIPE__IDE_TEMPLATE_META_DATA_FILE),TEMPLATE_REPLACE=$(_MTB_RECIPE__IDE_RECIPE_DIR)/Proj/$(_MTB_RECIPE__IDE_TEMPLATE_SUBDIR)/any=.mtbLaunchConfigs)
	$(call mtb__file_append,$(_MTB_RECIPE__IDE_TEMPLATE_META_DATA_FILE),TEMPLATE_REPLACE=$(_MTB_RECIPE__IDE_RECIPE_DIR)/Proj/any=.mtbLaunchConfigs)
endif
ifeq ($(_MTB_RECIPE__PROGRAM_INTERFACE_SUBDIR),KitProg3)
	$(call mtb__file_append,$(_MTB_RECIPE__IDE_TEMPLATE_META_DATA_FILE),TEMPLATE_REPLACE=$(_MTB_RECIPE__IDE_TEMPLATE_DIR)/eclipse/$(_MTB_RECIPE__IDE_TEMPLATE_TARGET_DIR)/$(MTB_RECIPE__CORE)/$(_MTB_RECIPE__IDE_TEMPLATE_SUBDIR)/advanced=.mtbLaunchConfigs)
endif

##############################################
# VSCode
##############################################
_MTB_RECIPE__VSCODE_MULTI_CORE_PRJ_TASKS_JSON:=$(_MTB_RECIPE__IDE_CORE_SCRIPT_DIR)/vscode/dependencies_tasks.json
_MTB_RECIPE__VSCODE_MULTI_CORE_APP_TASKS_JSON:=$(_MTB_RECIPE__IDE_CORE_SCRIPT_DIR)/vscode/$(_MTB_RECIPE__VSCODE_MULTI_CORE_TASKS_JSON_NAME)
_MTB_RECIPE__VSCODE_SINGLE_CORE_APP_TASKS_JSON:=$(_MTB_RECIPE__IDE_CORE_SCRIPT_DIR)/vscode/$(_MTB_RECIPE__VSCODE_MULTI_CORE_TASKS_JSON_NAME)

_MTB_RECIPE__VSCODE_MULTI_CORE_PRJ_LAUNCH_JSON:=$(_MTB_RECIPE__IDE_TEMPLATE_DIR)/vscode/$(_MTB_RECIPE__IDE_TEMPLATE_TARGET_DIR)/CMx/$(_MTB_RECIPE__IDE_TEMPLATE_SUBDIR)/launch_multicore.json
_MTB_RECIPE__VSCODE_MULTI_CORE_APP_LAUNCH_JSON:=$(_MTB_RECIPE__IDE_TEMPLATE_DIR)/vscode/$(_MTB_RECIPE__IDE_TEMPLATE_TARGET_DIR)/App/$(_MTB_RECIPE__IDE_TEMPLATE_SUBDIR)/launch.json
_MTB_RECIPE__VSCODE_SINGLE_CORE_APP_LAUNCH_JSON:=$(_MTB_RECIPE__IDE_TEMPLATE_DIR)/vscode/$(_MTB_RECIPE__IDE_TEMPLATE_TARGET_DIR)/CMx/$(_MTB_RECIPE__IDE_TEMPLATE_SUBDIR)/launch.json

vscode_generate: recipe_text_replacement_data_file recipe_vscode_text_replacement_data_file recipe_vscode_meta_replacement_data_file recipe_vscode_regex_replacement_data_file
vscode_generate: MTB_CORE__EXPORT_CMDLINE += -textdata $(_MTB_RECIPE__IDE_TEXT_DATA_FILE) -metadata $(_MTB_RECIPE__IDE_TEMPLATE_META_DATA_FILE) -textregexdata $(_MTB_RECIPE__VSCODE_TEMPLATE_REGEX_DATA_FILE)

recipe_vscode_text_replacement_data_file: recipe_text_replacement_data_file
	$(call mtb__file_append,$(_MTB_RECIPE__IDE_TEXT_DATA_FILE),&&_MTB_RECIPE__QSPI_CFG_PATH&&=$(_MTB_RECIPE__OPENOCD_QSPI_CFG_PATH))
	$(call mtb__file_append,$(_MTB_RECIPE__IDE_TEXT_DATA_FILE),&&_MTB_RECIPE__QSPI_CFG_PATH_APPLICATION&&=$(_MTB_RECIPE__OPENOCD_QSPI_CFG_PATH_APPLICATION))
	$(call mtb__file_append,$(_MTB_RECIPE__IDE_TEXT_DATA_FILE),&&_MTB_RECIPE__QSPI_FLASHLOADER&&=$(_MTB_RECIPE__OPENOCD_QSPI_FLASHLOADER))
	$(call mtb__file_append,$(_MTB_RECIPE__IDE_TEXT_DATA_FILE),&&_MTB_RECIPE__QSPI_FLASHLOADER_APPLICATION&&=$(_MTB_RECIPE__OPENOCD_QSPI_FLASHLOADER_APPLICATION))
	$(call mtb__file_append,$(_MTB_RECIPE__IDE_TEXT_DATA_FILE),&&_PRJ_NAME&&=$(APPNAME))
	$(call mtb__file_append,$(_MTB_RECIPE__IDE_TEXT_DATA_FILE),&&_MTB_RECIPE__OPENOCD_BOARD&&=$(_MTB_RECIPE__OPENOCD_BOARD))
	$(call mtb__file_append,$(_MTB_RECIPE__IDE_TEXT_DATA_FILE),&&_MTB_RECIPE__VSCODE_OPENOCD_PROBE_FREQUENCY&&=$(_MTB_RECIPE__OPENOCD_PROBE_FREQUENCY))
ifeq ($(MTB_RECIPE__CORE),CM33)
	$(call mtb__file_append,$(_MTB_RECIPE__IDE_TEXT_DATA_FILE),&&_MTB_RECIPE__PROG_FILE_CM33&&=$(subst $(MTB_RECIPE__SUFFIX_TARGET),hex,$(_MTB_RECIPE__VSCODE_ELF_FILE)))
	$(call mtb__file_append,$(_MTB_RECIPE__IDE_TEXT_DATA_FILE),&&_MTB_RECIPE__PROG_FILE_CM33_APPLICATION&&=$(subst $(MTB_RECIPE__SUFFIX_TARGET),hex,$(_MTB_RECIPE__VSCODE_ELF_FILE_APPLICATION)))
else
	$(call mtb__file_append,$(_MTB_RECIPE__IDE_TEXT_DATA_FILE),&&_MTB_RECIPE__PROG_FILE_CM55&&=$(subst $(MTB_RECIPE__SUFFIX_TARGET),hex,$(_MTB_RECIPE__VSCODE_ELF_FILE)))
	$(call mtb__file_append,$(_MTB_RECIPE__IDE_TEXT_DATA_FILE),&&_MTB_RECIPE__PROG_FILE_CM55_APPLICATION&&=$(subst $(MTB_RECIPE__SUFFIX_TARGET),hex,$(_MTB_RECIPE__VSCODE_ELF_FILE_APPLICATION)))
endif
ifneq ($(filter NON_SECURE,$(VCORE_ATTRS)),)
	$(call mtb__file_append,$(_MTB_RECIPE__IDE_TEXT_DATA_FILE),&&_MTB_RECIPE__ADD_SYMBOL_FILE_CMD&&=add-symbol-file $(_MTB_RECIPE__VSCODE_ELF_FILE_APPLICATION))
endif

recipe_vscode_regex_replacement_data_file:
ifeq ($(MTB_RECIPE__CORE),CM33)
	$(call mtb__file_write,$(_MTB_RECIPE__VSCODE_TEMPLATE_REGEX_DATA_FILE),^(.*)//CM33 Only//(.*)$$=\1\2)
	$(call mtb__file_append,$(_MTB_RECIPE__VSCODE_TEMPLATE_REGEX_DATA_FILE),^.*//CM55 Only//.*$$=)
else
	$(call mtb__file_write,$(_MTB_RECIPE__VSCODE_TEMPLATE_REGEX_DATA_FILE),^(.*)//CM55 Only//(.*)$$=\1\2)
	$(call mtb__file_append,$(_MTB_RECIPE__VSCODE_TEMPLATE_REGEX_DATA_FILE),^.*//CM33 Only//.*$$=)
endif

recipe_vscode_meta_replacement_data_file:
	$(call mtb__file_write,$(_MTB_RECIPE__IDE_TEMPLATE_META_DATA_FILE),)
ifeq ($(_MTB_RECIPE__PROGRAM_INTERFACE_SUBDIR),KitProg3)
	$(call mtb__file_append,$(_MTB_RECIPE__IDE_TEMPLATE_META_DATA_FILE),TEMPLATE_REPLACE=$(_MTB_RECIPE__IDE_TEMPLATE_DIR)/vscode/$(_MTB_RECIPE__IDE_TEMPLATE_TARGET_DIR)/CMx/openocd.tcl=openocd.tcl)
ifneq (,$(_MTB_RECIPE__IS_FIRST_PRJ))
	$(call mtb__file_append,$(_MTB_RECIPE__IDE_TEMPLATE_META_DATA_FILE),TEMPLATE_REPLACE=$(_MTB_RECIPE__IDE_TEMPLATE_DIR)/vscode/$(_MTB_RECIPE__IDE_TEMPLATE_TARGET_DIR)/App/openocd.tcl=../openocd.tcl)
endif
endif

.PHONY: recipe_text_replacement_data_file recipe_vscode_text_replacement_data_file recipe_vscode_meta_replacement_data_file recipe_vscode_regex_replacement_data_file
.PHONY: recipe_eclipse_text_replacement_data_file recipe_eclipse_meta_replacement_data_file recipe_eclipse_regex_replacement_data_file

##############################################
# UV
##############################################
_MTB_RECIPE__CMSIS_ARCH_NAME:=PSE8xxx_DFP
_MTB_RECIPE__CMSIS_VENDOR_NAME:=Infineon
_MTB_RECIPE__CMSIS_VENDOR_ID:=7

ifeq ($(MTB_RECIPE__CORE),CM55)
_MTB_RECIPE__CMSIS_PNAME:=Cortex-M55
else ifeq ($(MTB_RECIPE__CORE),CM33)
_MTB_RECIPE__CMSIS_PNAME:=Cortex-M33
endif

# Debug and program ini files
_MTB_RECIPE__PROGRAM_INI_FILE:=$(MTB_TOOLS__PRJ_DIR)/program.ini
_MTB_RECIPE__DEBUG_INI_FILE:=$(MTB_TOOLS__PRJ_DIR)/debug.ini

uvision5: recipe_uvision_debug_ini_file
recipe_uvision_debug_ini_file:
	$(call mtb__file_write,$(_MTB_RECIPE__DEBUG_INI_FILE),LOAD $$L%L NOCODE CLEAR INCREMENTAL)
	$(call mtb__file_append,$(_MTB_RECIPE__DEBUG_INI_FILE),g$(MTB__COMMA) main)

ifeq ($(MTB_RECIPE__CORE),CM33)
uvision5: recipe_uvision_program_ini_file
recipe_uvision_program_ini_file:
	$(call mtb__file_write,$(_MTB_RECIPE__PROGRAM_INI_FILE),LOAD ..\build\app_combined.hex)
endif

# uVision build data file
_MTB_RECIPE__UVISION_BUILD_DATA_FILE:=$(MTB_TOOLS__OUTPUT_CONFIG_DIR)/recipe_ide_build_data.txt

uvision5: MTB_CORE__EXPORT_CMDLINE += -build_data $(_MTB_RECIPE__UVISION_BUILD_DATA_FILE)
uvision5: recipe_uvision_build_data_file

recipe_uvision_build_data_file:
	$(call mtb__file_write,$(_MTB_RECIPE__UVISION_BUILD_DATA_FILE),LINKER_SCRIPT=$(MTB_RECIPE__LINKER_SCRIPT))
	$(call mtb__file_append,$(_MTB_RECIPE__UVISION_BUILD_DATA_FILE),FPU=$(_MTB_RECIPE_CMSIS__DFPU))
	$(call mtb__file_append,$(_MTB_RECIPE__UVISION_BUILD_DATA_FILE),LCS=$(_MTB_RECIPE_CMSIS__DSECURE))
	$(call mtb__file_append,$(_MTB_RECIPE__UVISION_BUILD_DATA_FILE),MVE=$(_MTB_RECIPE_CMSIS__DMVE))
.PHONY: recipe_uvision_build_data_file

# uVision DFP data file
_MTB_RECIPE__UVISION_DFP_DATA_FILE:=$(MTB_TOOLS__OUTPUT_CONFIG_DIR)/recipe_ide_dfp_data.txt

uvision5: recipe_uvision_dfp_data_file
uvision5: MTB_CORE__EXPORT_CMDLINE += -dfp_data $(_MTB_RECIPE__UVISION_DFP_DATA_FILE)

recipe_uvision_dfp_data_file:
	$(call mtb__file_write,$(_MTB_RECIPE__UVISION_DFP_DATA_FILE),DEVICE=$(DEVICE))
	$(call mtb__file_append,$(_MTB_RECIPE__UVISION_DFP_DATA_FILE),DFP_NAME=$(_MTB_RECIPE__CMSIS_ARCH_NAME))
	$(call mtb__file_append,$(_MTB_RECIPE__UVISION_DFP_DATA_FILE),VENDOR_NAME=$(_MTB_RECIPE__CMSIS_VENDOR_NAME))
	$(call mtb__file_append,$(_MTB_RECIPE__UVISION_DFP_DATA_FILE),VENDOR_ID=$(_MTB_RECIPE__CMSIS_VENDOR_ID))
	$(call mtb__file_append,$(_MTB_RECIPE__UVISION_DFP_DATA_FILE),PNAME=$(_MTB_RECIPE__CMSIS_PNAME))
	$(call mtb__file_append,$(_MTB_RECIPE__UVISION_DFP_DATA_FILE),DEBUG_INI_FILE=.\debug.ini)
.PHONY: recipe_uvision_debug_ini_file recipe_uvision_program_ini_file recipe_uvision_dfp_data_file

##############################################
# EW
##############################################
ifeq ($(MTB_RECIPE__CORE),CM55)
_MTB_RECIPE__IAR_CORE_SUFFIX:=M55
endif
ifeq ($(MTB_RECIPE__CORE),CM33)
_MTB_RECIPE__IAR_CORE_SUFFIX:=M33
endif

_MTB_RECIPE__EWARM_BUILD_DATA_FILE:=$(MTB_TOOLS__OUTPUT_CONFIG_DIR)/recipe_ide_build_data.txt

ewarm8: MTB_CORE__EXPORT_CMDLINE += -build_data $(_MTB_RECIPE__EWARM_BUILD_DATA_FILE)
ewarm8: recipe_ewarm_build_data_file

recipe_ewarm_build_data_file:
	$(call mtb__file_write,$(_MTB_RECIPE__EWARM_BUILD_DATA_FILE),LINKER_SCRIPT=$(MTB_RECIPE__LINKER_SCRIPT))
.PHONY: recipe_ewarm_build_data_file

##############################################
# Combine Sign
##############################################
ifneq ($(MTB_COMBINE_SIGN_$(_MTB_RECIPE__IDE_PRJ_DIR_NAME)_HEX_FILES),)

_MTB_RECIPE__VSCODE_CS_TASKS_JSON:=$(_MTB_RECIPE__IDE_CORE_SCRIPT_DIR)/vscode/tasks_program_sign_combine.json
_MTB_RECIPE__VSCODE_CS_LAUNCH_JSON:=$(_MTB_RECIPE__IDE_TEMPLATE_DIR)/vscode/$(_MTB_RECIPE__IDE_TEMPLATE_TARGET_DIR)/CMx/$(_MTB_RECIPE__IDE_TEMPLATE_SUBDIR)/launch_combine_sign.json

_MTB_RECIPE__ECLIPSE_COMBINE_SIGN_META_DATA_FILE=$(MTB_TOOLS__OUTPUT_CONFIG_DIR)/eclipse_combine_sign_meta_data.txt
_MTB_RECIPE__ECLIPSE_CS_DST_BASE_NAME:=.mtbLaunchConfigs/$(_MTB_RECIPE__ECLIPSE_APPLICATION_NAME).&&MTB_COMBINE_SIGN_&&IDX&&_CONFIG_NAME&&

eclipse_generate: MTB_CORE__EXPORT_CMDLINE += -metadata $(_MTB_RECIPE__ECLIPSE_COMBINE_SIGN_META_DATA_FILE)
eclipse_generate: recipe_eclipse_combine_sign_meta

recipe_eclipse_combine_sign_meta:
	$(call mtb__file_write,$(_MTB_RECIPE__ECLIPSE_COMBINE_SIGN_META_DATA_FILE))
	$(call mtb__file_append,$(_MTB_RECIPE__ECLIPSE_COMBINE_SIGN_META_DATA_FILE),TEMPLATE_REPLACE=$(_MTB_RECIPE__IDE_RECIPE_DIR)/Proj/$(_MTB_RECIPE__IDE_TEMPLATE_SUBDIR)/combine_sign/Debug.launch=$(_MTB_RECIPE__ECLIPSE_CS_DST_BASE_NAME) Debug $(_MTB_RECIPE__PROGRAM_INTERFACE_LAUNCH_SUFFIX).launch)
	$(call mtb__file_append,$(_MTB_RECIPE__ECLIPSE_COMBINE_SIGN_META_DATA_FILE),TEMPLATE_REPLACE=$(_MTB_RECIPE__IDE_RECIPE_DIR)/Proj/$(_MTB_RECIPE__IDE_TEMPLATE_SUBDIR)/combine_sign/Attach.launch=$(_MTB_RECIPE__ECLIPSE_CS_DST_BASE_NAME) Attach $(_MTB_RECIPE__PROGRAM_INTERFACE_LAUNCH_SUFFIX).launch)
	$(call mtb__file_append,$(_MTB_RECIPE__ECLIPSE_COMBINE_SIGN_META_DATA_FILE),TEMPLATE_REPLACE=$(_MTB_RECIPE__IDE_RECIPE_DIR)/Proj/combine_sign/Program.launch=$(_MTB_RECIPE__ECLIPSE_CS_DST_BASE_NAME) Program.launch)
ifeq ($(_MTB_RECIPE__PROGRAM_INTERFACE_SUBDIR),KitProg3)
ifeq ($(MTB_RECIPE__CORE),CM55)
	$(call mtb__file_append,$(_MTB_RECIPE__ECLIPSE_COMBINE_SIGN_META_DATA_FILE),TEMPLATE_REPLACE=$(_MTB_RECIPE__IDE_RECIPE_DIR)/Proj/$(_MTB_RECIPE__IDE_TEMPLATE_SUBDIR)/multicore/Add $(_MTB_RECIPE__PROGRAM_INTERFACE_LAUNCH_SUFFIX).launch=$(_MTB_RECIPE__ECLIPSE_CS_DST_BASE_NAME) $(_MTB_RECIPE__ECLIPSE_OPENOCD_DEBUG_MULTICORE_SECOND_CONFIG_NAME) $(_MTB_RECIPE__PROGRAM_INTERFACE_LAUNCH_SUFFIX).launch)
	$(call mtb__file_append,$(_MTB_RECIPE__ECLIPSE_COMBINE_SIGN_META_DATA_FILE),TEMPLATE_REPEAT=$(_MTB_RECIPE__ECLIPSE_CS_DST_BASE_NAME) Add CM55 to CM33 $(_MTB_RECIPE__PROGRAM_INTERFACE_LAUNCH_SUFFIX).launch=$(MTB_COMBINE_SIGN_$(_MTB_RECIPE__IDE_PRJ_DIR_NAME)_HEX_FILES))
endif
endif
	$(call mtb__file_append,$(_MTB_RECIPE__ECLIPSE_COMBINE_SIGN_META_DATA_FILE),TEMPLATE_REPEAT=$(_MTB_RECIPE__ECLIPSE_CS_DST_BASE_NAME) Debug $(_MTB_RECIPE__PROGRAM_INTERFACE_LAUNCH_SUFFIX).launch=$(MTB_COMBINE_SIGN_$(_MTB_RECIPE__IDE_PRJ_DIR_NAME)_HEX_FILES))
	$(call mtb__file_append,$(_MTB_RECIPE__ECLIPSE_COMBINE_SIGN_META_DATA_FILE),TEMPLATE_REPEAT=$(_MTB_RECIPE__ECLIPSE_CS_DST_BASE_NAME) Attach $(_MTB_RECIPE__PROGRAM_INTERFACE_LAUNCH_SUFFIX).launch=$(MTB_COMBINE_SIGN_$(_MTB_RECIPE__IDE_PRJ_DIR_NAME)_HEX_FILES))
	$(call mtb__file_append,$(_MTB_RECIPE__ECLIPSE_COMBINE_SIGN_META_DATA_FILE),TEMPLATE_REPEAT=$(_MTB_RECIPE__ECLIPSE_CS_DST_BASE_NAME) Program.launch=$(MTB_COMBINE_SIGN_$(_MTB_RECIPE__IDE_PRJ_DIR_NAME)_HEX_FILES))
.PHONY: recipe_eclipse_combine_sign_meta
endif
