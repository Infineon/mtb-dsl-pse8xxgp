################################################################################
# \file defines_pse8xxl.mk
#
# \brief
# Defines, needed for the PSOC(TM) Edge build recipe.
#
################################################################################
# \copyright
# (c) 2025, Cypress Semiconductor Corporation (an Infineon company) or
# an affiliate of Cypress Semiconductor Corporation. All rights reserved.
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

#
# Architecure specifics
#
_MTB_RECIPE__OPENOCD_DEVICE_CFG=infineon/pse86.cfg
_MTB_RECIPE__IDE_TEMPLATE_TARGET_DIR=pse8xxl
_MTB_RECIPE__BITFILE_LIFECYCLE_SUBDIR:=virgin
ifeq ($(APPTYPE), ram)
_MTB_RECIPE__PREBUILT_CM0_IMAGE=$(MTB_TOOLS__PRJ_DIR)/bin/cm0_boot_app_secure.elf
_MTB_RECIPE__PREBUILT_CM0_IMAGE_APPLICATION=$(patsubst $(call mtb__path_normalize,$(MTB_TOOLS__PRJ_DIR)/../)/%,%,$(call mtb__path_normalize,$(MTB_TOOLS__TARGET_DIR)/COMPONENT_CM33/TOOLCHAIN_$(TOOLCHAIN)/COMPONENT_PREBUILT_CM0P/cm0_boot_app_secure.elf))
endif
_MTB_RECIPE__OPENOCD_BOARD=set BOARD psvp
