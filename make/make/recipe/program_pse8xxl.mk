################################################################################
# \file program_pse8xxl.mk
#
# \brief
# This make file is called recursively and is used to build the
# resoures file system. It is expected to be run from the example directory.
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

_MTB_RECIPE__OPENOCD_BOARD=set BOARD psvp;

ifeq ($(APPTYPE), ram)
MTB_RECIPE__OPENOCD_PRETARGET_COMMAND+=; set ENABLE_ACQUIRE 0
_MTB_RECIPE__OPENOCD_PROGRAM=init; reset init; load_image $(_MTB_RECIPE__OPENOCD_PROGRAM_IMG); mww 0x52261000 0x34000400; mww 0x52260004 0x05FA0000; exit
else
_MTB_RECIPE__OPENOCD_PROGRAM=program $(_MTB_RECIPE__OPENOCD_PROGRAM_IMG) verify reset exit;
endif