# Copyright 2024-2025 Cypress Semiconductor Corporation
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

# The script parses the parameters and generates two types of data. The first is a JSON
# result that can be passed back to ModusToolbox for connectivity DRC processing.
# The second is a set of PPC locations that are assigned to each domain.

set param_dict_string [lindex $argv 0]
set param_dict [dict create {*}$param_dict_string]

set regCnt [dict get $param_dict "regCnt"]
set locationCnt [dict get $param_dict "locationCnt"]

# Create JSON object with all the resources and their associated domains.
# This is used for DRC processing to make sure connected items are not being
# driven by less secure items.
set objs {}
for {set idx 0} {$idx < $locationCnt} {incr idx} {

    set valid [dict get $param_dict [format "valid_%d" $idx]]

    if {$valid} {
        set id "parent_$idx"
        set name [dict get $param_dict [format "location_%d" $idx]]
        set domain [dict get $param_dict $id]
        set associated [dict get $param_dict [format "associated_%d" $idx]]

        # Build JSON array
        set obj " {"
        append obj " \"paramId\": \"$id\","
        append obj " \"paramDisplay\": \"$name\","
        append obj " \"domain\": \"$domain\","
        append obj " \"associated\": \"$associated\""
        append obj " }"
        lappend objs $obj
    }
}

# Create mapping of each domain to all the PPC regions that need to be
# configured for it.
array set domainToRegions {}

# Define list of regions to filter out from domainToRegions
set filteredRegions [list "PROT_PERI1_PPC1_PPC_PPC_SECURE" "PROT_PERI1_PPC1_PPC_PPC_NONSECURE" "PROT_PERI0_GR0_GROUP" "PROT_PERI0_GR0_BOOT" "PROT_PERI0_GR1_BOOT" "PROT_PERI0_GR2_BOOT" "PROT_PERI0_GR3_BOOT" "PROT_PERI0_GR4_BOOT" "PROT_PERI0_GR5_BOOT" "PROT_PERI0_RRAMC0_RRAM_EXTRA_AREA_RRAMC_PROTECTED" "PROT_PERI0_RRAMC0_RRAM_EXTRA_AREA_RRAMC_REPAIR" "PROT_PERI0_RRAMC0_RRAM_EXTRA_AREA_RRAMC_EXTRA" "PROT_PERI0_RRAMC0_RRAMC0_RRAMC_M0SEC" "PROT_PERI0_RRAMC0_MPC0_PPC_MPC_MAIN" "PROT_PERI0_RRAMC0_MPC1_PPC_MPC_MAIN" "PROT_PERI0_RRAMC0_MPC0_PPC_MPC_PC" "PROT_PERI0_RRAMC0_MPC1_PPC_MPC_PC" "PROT_PERI0_RRAMC0_MPC0_PPC_MPC_ROT" "PROT_PERI0_RRAMC0_MPC1_PPC_MPC_ROT" "PROT_PERI0_RRAMC0_RRAM_SFR_RRAMC_SFR_FPGA" "PROT_PERI0_RRAMC0_RRAM_SFR_RRAMC_SFR_NONUSER" "PROT_PERI0_RAMC0_BOOT" "PROT_PERI0_RAMC1_BOOT" "PROT_PERI0_MXCM33_BOOT_PC0" "PROT_PERI0_MXCM33_BOOT_PC1" "PROT_PERI0_MXCM33_BOOT_PC3" "PROT_PERI0_MXCM33_BOOT" "PROT_PERI0_CPUSS_AP" "PROT_PERI0_MS0_MAIN" "PROT_PERI0_MS4_MAIN" "PROT_PERI0_MS5_MAIN" "PROT_PERI0_MS6_MAIN" "PROT_PERI0_MS7_MAIN" "PROT_PERI0_MS8_MAIN" "PROT_PERI0_MS9_MAIN" "PROT_PERI0_MS10_MAIN" "PROT_PERI0_MS11_MAIN" "PROT_PERI0_MS29_MAIN" "PROT_PERI0_MS31_MAIN" "PROT_PERI0_MS_PC31_PRIV" "PROT_PERI0_CPUSS_SL_CTL_GROUP" "PROT_PERI0_SRSS_SECURE2" "PROT_PERI0_M0SECCPUSS_STATUS_MAIN" "PROT_PERI0_M0SECCPUSS_STATUS_PC1" "PROT_PERI0_PPC0_PPC_PPC_SECURE" "PROT_PERI0_PPC0_PPC_PPC_NONSECURE" "PROT_PERI0_SRSS_SECURE" ]

for {set idx 0} {$idx < $regCnt} {incr idx} {

    set valid [dict get $param_dict [format "reg%d_valid" $idx]]

    if {$valid} {
        set id "reg$idx"
        set domain [dict get $param_dict $id]
        set region [dict get $param_dict [format "reg%d_enum_name" $idx]]

        # Create dictionary of unique domains with their associated regions
        # Filter out regions that are in the filteredRegions list
        set shouldFilter [expr {$region in $filteredRegions}]
        if {$domain ne "" && $region ne "" && !$shouldFilter} {
            if {[info exists domainToRegions($domain)]} {
                lappend domainToRegions($domain) $region
            } else {
                set domainToRegions($domain) [list $region]
            }
        }
    }
}

set json "{"
append json " \"parameters\": \["
append json [join $objs ", "]
append json " ]"
append json "}"

# Output the JSON
puts ModusToolbox param:json=$json

foreach {key value} [array get domainToRegions] {
    puts ModusToolbox param:$key=$value
}
