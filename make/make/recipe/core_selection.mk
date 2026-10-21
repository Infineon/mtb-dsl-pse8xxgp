################################################################################
# \file core_selection.mk
#
# \brief
# Determine which MCU core is being targeted.
#
################################################################################
# \copyright
# Copyright (c) 2018-2026, Infineon Technologies AG, or an affiliate of
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

#
# Define the default core
#
ifeq ($(CORE),)
MTB_RECIPE__CORE?=CM33
else
MTB_RECIPE__CORE=$(CORE)
endif

ifeq ($(CORE_NAME),)
MTB_RECIPE__CORE_NAME?=$(MTB_RECIPE__CORE)_0
else
MTB_RECIPE__CORE_NAME=$(CORE_NAME)
endif

COMPONENTS+=$(MTB_RECIPE__CORE) $(MTB_RECIPE__CORE_NAME)

# MVE support — resolved here (before toolchain .mk files that consume it at parse time).
# If MVE is not available on device then MVE_SELECT=NO_MVE.
# If MVE is available on device and VFP_SELECT=softfloat, then MVE_SELECT=MVE-I,
# else MVE_SELECT=<empty> (MVE-F mode).
ifeq ($(filter $(MTB_RECIPE__CORE_NAME)_MVE_PRESENT,$(DEVICE_$(DEVICE)_FEATURES)),)
MVE_SELECT?=NO_MVE
else
ifeq ($(VFP_SELECT),softfloat)
MVE_SELECT?=MVE-I
else
MVE_SELECT?=
endif
endif
