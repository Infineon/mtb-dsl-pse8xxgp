/***************************************************************************//**
* \file cy_rram.c
* \version 1.10
*
* \brief
* Provides an API implementation of the RRAM driver.
*
********************************************************************************
* \copyright
* (c) 2022-2026, Infineon Technologies AG or an affiliate of
* Infineon Technologies AG.
* SPDX-License-Identifier: Apache-2.0
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*******************************************************************************/

#include "cy_rram.h"
#ifdef WA__PM_0159902_6
#include "cy_ipc_drv.h"
#endif
#define DEFAULT_MACRO 0
#if defined (CY_IP_MXS22RRAMC)
#include "cy_ms_ctl.h"

/*******************************************************************************
*       Internal Defines
*******************************************************************************/
/* Timeout in CPU cycles: worst-case 10 ms at maximum core frequency of 400 MHz.
 * 10 ms * 400 MHz = 4,000,000 cycles. Must be a multiple of CY_RRAM_STATUS_CHECK_DELAY.
 * The RRAM IP state machine is guaranteed to complete, so this timeout is a safety measure. */
#define CY_RRAM_OPERATION_TIMEOUT                  (4000000UL)
/* Delay in cpu cycles before repeating the status check */
#define CY_RRAM_STATUS_CHECK_DELAY                 (50UL)
/* Maximum number of write trials.
 * Up to 3 attempts are made to handle correctable ECC errors (1-bit and 2-bit).
 * After the final trial, a 2-bit error is accepted as correctable by hardware ECC. */
#define CY_RRAM_MAX_WRITE_TRIALS                   (3UL)

/* Mask for extracting the byte offset within a 16-byte RRAM block */
#define CY_RRAM_BLOCK_OFFSET_MASK                  (CY_RRAM_BLOCK_SIZE_BYTES - 1UL)
#if (CY_IP_MXS22RRAMC_VERSION == 2u)
/* Invalid macro Index */
#define CY_RRAM_MACRO_IDX_INVALID                  (0xFFUL)
#endif

/* RRAM read operation ECC fail status */
#define CY_RRAM_ECC_READ_NO_FAIL                   (0x00000000UL)                         /**< Read is success without any ECC fail */
#define CY_RRAM_ECC_READ_SINGLE_BIT_FAIL           RRAMC_RRAM_SFR_NVM_STATUS_ECC1READ_Msk /**< At least one ECC single-bit failure was detected and corrected during the last memory read operation */
#define CY_RRAM_ECC_READ_DOUBLE_BIT_FAIL           RRAMC_RRAM_SFR_NVM_STATUS_ECC2READ_Msk /**< At least one ECC double-bit failure was detected and corrected during the last memory read operation */
#define CY_RRAM_ECC_READ_TRIPLE_BIT_FAIL           RRAMC_RRAM_SFR_NVM_STATUS_ECC3READ_Msk /**< At least one ECC triple-bit failure was detected and corrected during the last memory read operation */
#define CY_RRAM_ECC_READ_MULTIPLE_BIT_FAIL         RRAMC_RRAM_SFR_NVM_STATUS_ECC4READ_Msk /**< At least one ECC-uncorrectable multi-bit failure was detected during the last memory read operation */
#define CY_RRAM_ECC_READ_ADDRESS_MISMATCH          RRAMC_RRAM_SFR_NVM_STATUS_ECCAREAD_Msk /**< At least one address mismatch ECC failure was detected during the last memory read operation */
#define CY_RRAM_ECC_READ_BLOCK_SINGLE_BIT_FAIL     RRAMC_RRAM_SFR_NVM_STATUS_ECC1MRD_Msk  /**< An ECC single-bit failure was detected and corrected during the most recent memory read operation to a new block */
#define CY_RRAM_ECC_READ_BLOCK_DOUBLE_BIT_FAIL     RRAMC_RRAM_SFR_NVM_STATUS_ECC2MRD_Msk  /**< An ECC double-bit failure was detected and corrected during the most recent memory read operation to a new block */
#define CY_RRAM_ECC_READ_BLOCK_TRIPLE_BIT_FAIL     RRAMC_RRAM_SFR_NVM_STATUS_ECC3MRD_Msk  /**< An ECC triple-bit failure was detected and corrected during the most recent memory read operation to a new block */
#define CY_RRAM_ECC_READ_BLOCK_MULTIPLE_BIT_FAIL   RRAMC_RRAM_SFR_NVM_STATUS_ECC4MRD_Msk  /**< An ECC-uncorrectable multi-bit failure was detected during the most recent memory read operation to a new block */
#define CY_RRAM_ECC_READ_BLOCK_ADDRESS_MISMATCH    RRAMC_RRAM_SFR_NVM_STATUS_ECCAMRD_Msk  /**< An address mismatch ECC failure was detected during the most recent memory read operation to a new block */

CY_MISRA_DEVIATE_BLOCK_START('MISRA C-2012 Rule 10.8', 2, \
'Value extracted from _VAL2FLD macro will not exceed enum range.')

/* MISRA Rule 14.3 deviation: The work and sflash subsection sizes are zero on current
 * devices (CY_RRAM_WORK_Z=0, CY_RRAM_FLASH_Y=0), making some conditions always false.
 * This is expected and correct - the conditions become true on future devices with
 * non-zero work/sflash regions. */
CY_MISRA_DEVIATE_BLOCK_START('MISRA C-2012 Rule 14.3', 12, \
'Because the size of work, flash subsection is zero but will be true if devices have valid size.')


/* RRAM config structure */
typedef struct
{
    RRAMC_Type * base;         /*!< The pointer to the RRAMC instance.*/
    uint32_t addr;             /*!< Address of the RRAM subregion.*/
    uint32_t subRegionBase;    /*!< Base address of subregion.*/
    uint32_t subRegionSize;    /*!< Size of subregion.*/
#if (CY_IP_MXS22RRAMC_VERSION == 2u)
    uint8_t  subRegionMacroIdx;/*!< Macro index of subregion. */
#endif
    bool blockingWrite;        /*!< To define blocking write for indirect write operation.*/
    bool tearingSafe;          /*!< Boolean to check if API is called from tearing safe library.*/
} cy_stc_rram_config_t;

typedef struct
{
    uint32_t base;        /*!< Base address of subregion.*/
    uint32_t size;        /*!< Size of subregion.*/
#if (CY_IP_MXS22RRAMC_VERSION == 2u)
    uint8_t  macro_idx;   /*!< Macro index.*/
#endif
}  cy_stc_rram_rgn_info_t;

typedef union
{
    uint32_t w[4];    /*!< Store word data.*/
    uint8_t b[16];    /*!< Store byte data.*/
} cy_rram_blk_buf_t;


/*******************************************************************************
* Function Name: Cy_RRAM_ReleasePCLock
****************************************************************************//**
*
* Release PC lock.
*
*******************************************************************************/
__STATIC_INLINE void Cy_RRAM_ReleasePCLock(RRAMC_Type * base)
{
    RRAM_PC_MASK(base) &= ~RRAMC_RRAMC_PC_MASK_PC_LOCK_Msk;
#ifdef WA__PM_0159902_6
    (void)Cy_IPC_Drv_LockRelease (Cy_IPC_Drv_GetIpcBaseAddress(CY_IPC_CHAN_RRAM_LOCK), CY_IPC_NO_NOTIFICATION);
#endif
}


/*******************************************************************************
* Function Name: Cy_RRAM_IsPCLockAcquired
****************************************************************************//**
*
* Indicates whether PC lock is acquired or not.
*
*******************************************************************************/
__STATIC_INLINE bool Cy_RRAM_IsPCLockAcquired(RRAMC_Type * base)
{
#ifdef WA__PM_0159902_6
    (void) base;
    return (Cy_IPC_Drv_IsLockAcquired (Cy_IPC_Drv_GetIpcBaseAddress(CY_IPC_CHAN_RRAM_LOCK)));
#else
    return (_FLD2VAL(RRAMC_RRAMC_PC_MASK_PC_LOCK, RRAM_PC_MASK(base)) == CY_RRAM_PC_LOCK_ACQUIRE);
#endif
}


/*******************************************************************************
* Function Name: Cy_RRAM_AcquirePCLock
****************************************************************************//**
*
* Acquire a PC lock for RRAM to perform indirect operations.
*
*******************************************************************************/
__STATIC_INLINE bool Cy_RRAM_AcquirePCLock(RRAMC_Type * base, bool secure)
{
    bool status = false;
    uint32_t activePC;
    uint32_t savedIntrStatus;

    /* Enter Critical section */
    savedIntrStatus = Cy_SysLib_EnterCriticalSection();

    /* Check if Lock is already acquired */
    if(Cy_RRAM_IsPCLockAcquired(base))
    {
        /* Lock acquired already, exit Critical section */
        Cy_SysLib_ExitCriticalSection(savedIntrStatus);
        return false;
    }

#ifdef WA__PM_0159902_6
    if (CY_IPC_DRV_SUCCESS != Cy_IPC_Drv_LockAcquire (Cy_IPC_Drv_GetIpcBaseAddress(CY_IPC_CHAN_RRAM_LOCK)))
    {
        /* Exit Critical section */
        Cy_SysLib_ExitCriticalSection(savedIntrStatus);
        return false;
    }
#endif

    /* Acquire the lock */
    RRAM_PC_MASK(base) |= _VAL2FLD(RRAMC_RRAMC_PC_MASK_PC_LOCK, CY_RRAM_PC_LOCK_ACQUIRE);

    /* Get the value of active PC */
    #if (CY_CPU_CORTEX_M0P)
    CY_UNUSED_PARAMETER(secure);
    activePC = ((uint32_t)_FLD2VAL(M0SECCPUSS_CM0_MS_PC_PC, M0SECCPUSS_CM0_MS_PC));
    #elif (CY_CPU_CORTEX_M33)
    #if ((CY_IP_MXS22RRAMC_VERSION == 2) && ((CY_IP_MXS22RRAMC_VERSION_MINOR == 0) || (CY_IP_MXS22RRAMC_VERSION_MINOR == 1)))
    if (secure)
    {
        activePC = Cy_Ms_Ctl_GetActivePC_S(CY_MS_CTL_ID_CM33_0);
    }
    else
    {
        /* For non-secure access, use the non-secure Alias */
        activePC = Cy_Ms_Ctl_GetActivePC(CY_MS_CTL_ID_CM33_0);
    }
    #else
    CY_UNUSED_PARAMETER(secure);
    activePC = Cy_Ms_Ctl_GetActivePC(CY_MS_CTL_ID_CM33_0);
    #endif // ((CY_IP_MXS22RRAMC_VERSION == 2) && ((CY_IP_MXS22RRAMC_VERSION_MINOR == 0) || (CY_IP_MXS22RRAMC_VERSION_MINOR == 1)))
    #else // CY_CPU_CORTEX_M55
    CY_UNUSED_PARAMETER(secure);
    activePC = Cy_Ms_Ctl_GetActivePCV1(CY_MS_CTL_ID_CM55_MS_0);
    #endif

    if((_FLD2VAL(RRAMC_RRAMC_PC_MASK_PC, RRAM_PC_MASK(base)) == activePC) && \
                   (_FLD2VAL(RRAMC_RRAMC_PC_MASK_PC_LOCK, RRAM_PC_MASK(base)) == CY_RRAM_PC_LOCK_ACQUIRE))
    {
        status = true;
    }

    /* Exit Critical section */
    Cy_SysLib_ExitCriticalSection(savedIntrStatus);

    return status;
}

/*******************************************************************************
* Function Name: Cy_RRAM_SetOperationMode
****************************************************************************//**
*
* Sets operation mode refer cy_en_rram_operation_t for supported operations.
*
*******************************************************************************/
#if (CY_IP_MXS22RRAMC_VERSION == 2u)
__STATIC_INLINE void Cy_RRAM_SetOperationMode(RRAMC_Type * base, uint8_t idx, cy_en_rram_operation_t operation)
{
    uint32_t temp = RRAM_NVM_PROG(base, idx) & ~ RRAMC_RRAM_SFR_NVM_PROG_ACTION_Msk;
    RRAM_NVM_PROG(base, idx) = temp | _VAL2FLD(RRAMC_RRAM_SFR_NVM_PROG_ACTION, operation);
}
#else
__STATIC_INLINE void Cy_RRAM_SetOperationMode(RRAMC_Type * base, cy_en_rram_operation_t operation)
{
    uint32_t temp = RRAM_NVM_PROG(base) & ~ RRAMC_RRAM_SFR_NVM_PROG_ACTION_Msk;
    RRAM_NVM_PROG(base) = temp | _VAL2FLD(RRAMC_RRAM_SFR_NVM_PROG_ACTION, operation);
}
#endif

/*******************************************************************************
* Function Name: Cy_RRAM_StartOperation
****************************************************************************//**
*
* Start the RRAM operation that is set by Cy_RRAM_SetOperationMode().
*
*******************************************************************************/
#if (CY_IP_MXS22RRAMC_VERSION == 2u)
__STATIC_INLINE void Cy_RRAM_StartOperation(RRAMC_Type * base, uint8_t idx)
{
    RRAM_NVM_PROG(base, idx) |= RRAMC_RRAM_SFR_NVM_PROG_STARTOP_Msk;
}
#else
__STATIC_INLINE void Cy_RRAM_StartOperation(RRAMC_Type * base)
{
    RRAM_NVM_PROG(base) |= RRAMC_RRAM_SFR_NVM_PROG_STARTOP_Msk;
}
#endif

#if (CY_CPU_CORTEX_M0P)
/*******************************************************************************
* Function Name: Cy_RRAM_StopOperation
****************************************************************************//**
*
* Stop the RRAM operation that is set by Cy_RRAM_SetOperationMode().
*
*******************************************************************************/
#if (CY_IP_MXS22RRAMC_VERSION == 2u)
__STATIC_INLINE void Cy_RRAM_StopOperation(RRAMC_Type * base, uint8_t idx)
{
    RRAM_NVM_PROG(base, idx) &= ~RRAMC_RRAM_SFR_NVM_PROG_STARTOP_Msk;
}
#else
__STATIC_INLINE void Cy_RRAM_StopOperation(RRAMC_Type * base)
{
    RRAM_NVM_PROG(base) &= ~RRAMC_RRAM_SFR_NVM_PROG_STARTOP_Msk;
}
#endif
#endif


/*******************************************************************************
* Function Name: CY_RRAM_GetFailBitInfo
****************************************************************************//**
*
* Information on number of fail bits for a block after verify in indirect
* write operation.
*
*******************************************************************************/
#if (CY_IP_MXS22RRAMC_VERSION == 2u)
__STATIC_INLINE cy_en_rram_verr_status_t CY_RRAM_GetFailBitInfo(RRAMC_Type * base, uint8_t idx)
{
    return (cy_en_rram_verr_status_t)_FLD2VAL(RRAMC_RRAM_SFR_NVM_STATUS_VERR, RRAM_NVM_STATUS(base, idx));
}
#else
__STATIC_INLINE cy_en_rram_verr_status_t CY_RRAM_GetFailBitInfo(RRAMC_Type * base)
{
    return (cy_en_rram_verr_status_t)_FLD2VAL(RRAMC_RRAM_SFR_NVM_STATUS_VERR, RRAM_NVM_STATUS(base));
}
#endif

/*******************************************************************************
* Function Name: CY_RRAM_ReadOperationECCStatus
****************************************************************************//**
*
* Indicates ECC-status. ECC single, double and triple bit failures in read operation
* are treated as only warnings.
*
*******************************************************************************/
#if (CY_IP_MXS22RRAMC_VERSION == 2u)
__STATIC_INLINE bool CY_RRAM_ReadOperationECCStatus(RRAMC_Type * base, uint8_t idx, uint32_t ecc_error)
{
    return (0U != (RRAM_NVM_STATUS(base, idx) & ecc_error));
}
#else
__STATIC_INLINE bool CY_RRAM_ReadOperationECCStatus(RRAMC_Type * base, uint32_t ecc_error)
{
    return (0U != (RRAM_NVM_STATUS(base) & ecc_error));
}
#endif

#if (CY_IP_MXS22RRAMC_VERSION == 2u)
/*******************************************************************************
* Function Name: CY_RRAM_GetMacroIdx
****************************************************************************//**
*
* Search for the Macro index in the given Memory region.
*
*******************************************************************************/
static uint8_t CY_RRAM_GetMacroIdx(uint32_t addr, uint32_t start_addr, uint32_t size)
{
    uint8_t macro_idx = CY_RRAM_MACRO_IDX_INVALID;
    for (uint8_t idx = 0; idx < CY_RRAMC_NUM_MACROS_MUL; idx++)
    {
        if (((start_addr + (size * idx)) <= addr) && (addr < (start_addr + (size * (idx + 1)))))
        {
            macro_idx = idx;
            break;
        }
    }

    return macro_idx;
}

#if (CY_RRAM_RECLAIM_SIZE > 0UL)
/* Reclaim occupies the first CY_RRAM_RECLAIM_SIZE bytes of macro0's PROTECTED
 * physical area, so callers see PROTECTED as (N*PROTECTED - RECLAIM) bytes.
 * All macro-index / phy-address math for PROTECTED must pre-shift the visible
 * offset by RECLAIM_SIZE. This maps macro0's usable slot to [RECLAIM, PROTECTED)
 * and macros 1..N-1 to [0, PROTECTED) uniformly for any macro count. */
static bool Cy_RRAM_IsProtectedSubregion(uint32_t subRegionBase)
{
    uint32_t baseNs = subRegionBase & ~CY_RRAM_S_OFFSET_MASK;
    bool isProt = (baseNs == CY_RRAM_PROTECTED_HOST_NS_START_ADDRESS);
#if (CY_CPU_CORTEX_M33)
    isProt = isProt || (baseNs == CY_RRAM_PROTECTED_HOST_CBUS_NS_START_ADDRESS);
#endif
    return isProt;
}
#endif

static uint32_t Cy_RRAM_ProtectedShift(uint32_t subRegionBase)
{
#if (CY_RRAM_RECLAIM_SIZE > 0UL)
    return Cy_RRAM_IsProtectedSubregion(subRegionBase) ? CY_RRAM_RECLAIM_SIZE : 0UL;
#else
    (void)subRegionBase;
    return 0UL;
#endif
}
#endif

/*******************************************************************************
* Function Name: Cy_RRAM_GetOTPSubRegionInfo
****************************************************************************//**
*
* Get base address, size of the OTP sub region for the provided address.
*
*******************************************************************************/
static cy_stc_rram_rgn_info_t Cy_RRAM_GetOTPSubRegionInfo(uint32_t addr)
{
    uint32_t secureBitState = addr & CY_RRAM_S_OFFSET_MASK;
    uint32_t addr_ns         = addr & ~CY_RRAM_S_OFFSET_MASK;

#if (CY_IP_MXS22RRAMC_VERSION == 2u)
    /* Default Macro index is 0 for single macro products */
    cy_stc_rram_rgn_info_t rgnInfo= {CY_RRAM_INVALID_ACCESS, 0UL, 0UL};
#else
    cy_stc_rram_rgn_info_t rgnInfo= {CY_RRAM_INVALID_ACCESS, 0UL};
#endif
#if (CY_IP_MXS22RRAMC_VERSION == 1u)
    if((addr_ns >= CY_RRAM_GENERAL_OTP_MMIO_NS_START_ADDRESS) && (addr_ns <= CY_RRAM_GENERAL_OTP_MMIO_NS_END_ADDRESS))
    {
        rgnInfo.base = CY_RRAM_GENERAL_OTP_MMIO_NS_START_ADDRESS;
        rgnInfo.size = CY_RRAM_GENERAL_OTP_SIZE_INTERNAL;
    }
    else if((addr_ns >= CY_RRAM_PROTECTED_OTP_PROTECTED_NS_START_ADDRESS) && (addr_ns <= CY_RRAM_PROTECTED_OTP_PROTECTED_NS_END_ADDRESS))
    {
        rgnInfo.base = CY_RRAM_PROTECTED_OTP_PROTECTED_NS_START_ADDRESS;
        rgnInfo.size = CY_RRAM_PROTECTED_OTP_SIZE_INTERNAL;
    }
#else
#if (RRAMC_PROTECTED_INTF_PRESENT == 1)
    if((addr_ns >= CY_RRAM_PROTECTED_OTP_PROTECTED_NS_START_ADDRESS) && (addr_ns <= CY_RRAM_PROTECTED_OTP_PROTECTED_NS_END_ADDRESS))
    {
        rgnInfo.base = CY_RRAM_PROTECTED_OTP_PROTECTED_NS_START_ADDRESS;
        rgnInfo.size = CY_RRAM_PROTECTED_OTP_SIZE_INTERNAL;
        rgnInfo.macro_idx = CY_RRAM_GetMacroIdx(addr_ns, CY_RRAM_PROTECTED_OTP_PROTECTED_NS_START_ADDRESS, CY_RRAM_PROTECTED_OTP_SIZE_INTERNAL);
    }
    else if((addr_ns >= CY_RRAM_GENERAL_OTP_NS_START_ADDRESS ) && (addr_ns <= CY_RRAM_GENERAL_OTP_NS_END_ADDRESS))
    {
        rgnInfo.base = CY_RRAM_GENERAL_OTP_NS_START_ADDRESS;
        rgnInfo.size = CY_RRAM_GENERAL_OTP_SIZE_INTERNAL;
        rgnInfo.macro_idx = CY_RRAM_GetMacroIdx(addr_ns, CY_RRAM_GENERAL_OTP_NS_START_ADDRESS, CY_RRAM_GENERAL_OTP_SIZE_INTERNAL);
      }
#else
    if((addr_ns >= GENERAL_OTP_1_NS_START_ADDRESS) && (addr_ns <= GENERAL_OTP_1_NS_END_ADDRESS))
    {
        rgnInfo.base = GENERAL_OTP_1_NS_START_ADDRESS;
        rgnInfo.size = CY_RRAM_OTP_1_5_SIZE;
    }
    else if((addr_ns >= GENERAL_OTP_2_NS_START_ADDRESS) && (addr_ns <= GENERAL_OTP_2_NS_END_ADDRESS))
    {
        rgnInfo.base = GENERAL_OTP_2_NS_START_ADDRESS;
        rgnInfo.size = CY_RRAM_OTP_1_5_SIZE;
    }
    else if((addr_ns >= GENERAL_OTP_3_NS_START_ADDRESS) && (addr_ns <= GENERAL_OTP_3_NS_END_ADDRESS))
    {
        rgnInfo.base = GENERAL_OTP_3_NS_START_ADDRESS;
        rgnInfo.size = CY_RRAM_OTP_1_5_SIZE;
    }
    else if((addr_ns >= GENERAL_OTP_4_NS_START_ADDRESS) && (addr_ns <= GENERAL_OTP_4_NS_END_ADDRESS))
    {
        rgnInfo.base = GENERAL_OTP_4_NS_START_ADDRESS;
        rgnInfo.size = CY_RRAM_OTP_1_5_SIZE;
    }
    else if((addr_ns >= GENERAL_OTP_5_NS_START_ADDRESS) && (addr_ns <= GENERAL_OTP_5_NS_END_ADDRESS))
    {
        rgnInfo.base = GENERAL_OTP_5_NS_START_ADDRESS;
        rgnInfo.size = CY_RRAM_OTP_1_5_SIZE;
    }
    else if((addr_ns >= GENERAL_OTP_6_NS_START_ADDRESS) && (addr_ns <= GENERAL_OTP_6_NS_END_ADDRESS))
    {
        rgnInfo.base = GENERAL_OTP_6_NS_START_ADDRESS;
        rgnInfo.size = CY_RRAM_OTP_6_SIZE;
    }
#endif // (RRAMC_PROTECTED_INTF_PRESENT == 1)
#endif
    else
    {
        secureBitState = 0UL;
    }

    rgnInfo.base |= secureBitState;

    return rgnInfo;
}


/*******************************************************************************
* Function Name: Cy_RRAM_GetNVMRegionInfo
****************************************************************************//**
*
* Get base address, size of the NVM sub region for the provided address.
*
*******************************************************************************/
static cy_stc_rram_rgn_info_t Cy_RRAM_GetNVMRegionInfo(uint32_t addr)
{
    uint32_t secureBitState = addr & CY_RRAM_S_OFFSET_MASK;
    uint32_t addr_ns         = addr & ~CY_RRAM_S_OFFSET_MASK;

    /* Default Macro index is 0 for single macro products */
#if (CY_IP_MXS22RRAMC_VERSION == 2u)
    cy_stc_rram_rgn_info_t rgnInfo= {CY_RRAM_INVALID_ACCESS, 0UL, 0UL};
#else
    cy_stc_rram_rgn_info_t rgnInfo= {CY_RRAM_INVALID_ACCESS, 0UL};
#endif

#if (CY_RRAM_RECLAIM_SIZE > 0UL)
    /* Check reclaim before MAIN because MAIN's visible end includes reclaim. */
    if((addr_ns >= CY_RRAM_MAIN_RECLAIM_NS_START_ADDRESS) && (addr_ns <= CY_RRAM_MAIN_RECLAIM_NS_END_ADDRESS))
    {
        rgnInfo.base = CY_RRAM_MAIN_RECLAIM_NS_START_ADDRESS;
        rgnInfo.size = CY_RRAM_RECLAIM_SIZE;
#if (CY_IP_MXS22RRAMC_VERSION == 2u)
        rgnInfo.macro_idx = CY_RRAM_GetMacroIdx(addr_ns, CY_RRAM_MAIN_RECLAIM_NS_START_ADDRESS,
                    CY_RRAM_RECLAIM_SIZE);
#endif
    }
    else
#endif
    if((addr_ns >= CY_RRAM_MAIN_HOST_NS_START_ADDRESS) && (addr_ns <= CY_RRAM_MAIN_HOST_NS_END_ADDRESS))
    {
        rgnInfo.base = CY_RRAM_MAIN_HOST_NS_START_ADDRESS;
        rgnInfo.size = CY_RRAM_MAIN_REGION_SIZE_INTERNAL;
#if (CY_IP_MXS22RRAMC_VERSION == 2u)
        rgnInfo.macro_idx = CY_RRAM_GetMacroIdx(addr_ns, CY_RRAM_MAIN_HOST_NS_START_ADDRESS, CY_RRAM_MAIN_REGION_SIZE_INTERNAL);
#endif
    }
    else if((addr_ns >= CY_RRAM_WORK_HOST_NS_START_ADDRESS) && (addr_ns <= CY_RRAM_WORK_HOST_NS_END_ADDRESS))
    {
        rgnInfo.base = CY_RRAM_WORK_HOST_NS_START_ADDRESS;
        rgnInfo.size = CY_RRAM_WORK_REGION_SIZE_INTERNAL;
#if (CY_IP_MXS22RRAMC_VERSION == 2u)
        rgnInfo.macro_idx = CY_RRAM_GetMacroIdx(addr_ns, CY_RRAM_WORK_HOST_NS_START_ADDRESS, CY_RRAM_WORK_REGION_SIZE_INTERNAL);
#endif
    }
    else if((addr_ns >= CY_RRAM_SFLASH_HOST_NS_START_ADDRESS) && (addr_ns <= CY_RRAM_SFLASH_HOST_NS_END_ADDRESS))
    {
        rgnInfo.base = CY_RRAM_SFLASH_HOST_NS_START_ADDRESS;
        rgnInfo.size = CY_RRAM_SFLASH_REGION_SIZE_INTERNAL;
#if (CY_IP_MXS22RRAMC_VERSION == 2u)
        rgnInfo.macro_idx = CY_RRAM_GetMacroIdx(addr_ns, CY_RRAM_SFLASH_HOST_NS_START_ADDRESS, CY_RRAM_SFLASH_REGION_SIZE_INTERNAL);
#endif
    }
    else if((addr_ns >= CY_RRAM_PROTECTED_HOST_NS_START_ADDRESS) && (addr_ns <= CY_RRAM_PROTECTED_HOST_NS_END_ADDRESS))
    {
        rgnInfo.base = CY_RRAM_PROTECTED_HOST_NS_START_ADDRESS;
        rgnInfo.size = CY_RRAM_PROTECTED_REGION_SIZE_INTERNAL;
#if (CY_IP_MXS22RRAMC_VERSION == 2u)
        rgnInfo.macro_idx = CY_RRAM_GetMacroIdx(addr_ns + CY_RRAM_RECLAIM_SIZE, CY_RRAM_PROTECTED_HOST_NS_START_ADDRESS, CY_RRAM_PROTECTED_REGION_SIZE_INTERNAL);
#endif
    }
#if (CY_CPU_CORTEX_M0P)
    else if((addr_ns >= CY_RRAM_PROTECTED_PROTECTED_NS_START_ADDRESS) && (addr_ns <= CY_RRAM_PROTECTED_PROTECTED_NS_END_ADDRESS))
    {
        rgnInfo.base = CY_RRAM_PROTECTED_PROTECTED_NS_START_ADDRESS;
        rgnInfo.size = CY_RRAM_PROTECTED_REGION_SIZE_INTERNAL;
    }
#endif
#if (CY_CPU_CORTEX_M33)
    else if((addr_ns >= CY_RRAM_MAIN_HOST_CBUS_NS_START_ADDRESS) && (addr_ns <= CY_RRAM_MAIN_HOST_CBUS_NS_END_ADDRESS))
    {
        rgnInfo.base = CY_RRAM_MAIN_HOST_CBUS_NS_START_ADDRESS;
        rgnInfo.size = CY_RRAM_MAIN_REGION_SIZE_INTERNAL;
#if (CY_IP_MXS22RRAMC_VERSION == 2u)
        rgnInfo.macro_idx = CY_RRAM_GetMacroIdx(addr_ns, CY_RRAM_MAIN_HOST_CBUS_NS_START_ADDRESS, CY_RRAM_MAIN_REGION_SIZE_INTERNAL);
#endif
    }
    else if((addr_ns >= CY_RRAM_PROTECTED_HOST_CBUS_NS_START_ADDRESS) && (addr_ns <= CY_RRAM_PROTECTED_HOST_CBUS_NS_END_ADDRESS))
    {
        rgnInfo.base = CY_RRAM_PROTECTED_HOST_CBUS_NS_START_ADDRESS;
        rgnInfo.size = CY_RRAM_PROTECTED_REGION_SIZE_INTERNAL;
#if (CY_IP_MXS22RRAMC_VERSION == 2u)
        rgnInfo.macro_idx = CY_RRAM_GetMacroIdx(addr_ns + CY_RRAM_RECLAIM_SIZE, CY_RRAM_PROTECTED_HOST_CBUS_NS_START_ADDRESS, CY_RRAM_PROTECTED_REGION_SIZE_INTERNAL);
#endif
    }
#if (CY_RRAM_RECLAIM_SIZE > 0UL)
    else if((addr_ns >= CY_RRAM_MAIN_RECLAIM_CBUS_NS_START_ADDRESS) && (addr_ns <= CY_RRAM_MAIN_RECLAIM_CBUS_NS_END_ADDRESS))
    {
        rgnInfo.base = CY_RRAM_MAIN_RECLAIM_CBUS_NS_START_ADDRESS;
        rgnInfo.size = CY_RRAM_RECLAIM_SIZE;
#if (CY_IP_MXS22RRAMC_VERSION == 2u)
        rgnInfo.macro_idx = CY_RRAM_GetMacroIdx(addr_ns, CY_RRAM_MAIN_RECLAIM_CBUS_NS_START_ADDRESS,
                    CY_RRAM_RECLAIM_SIZE);
#endif
    }
#endif
#endif
    else
    {
        secureBitState = 0UL;
    }
    rgnInfo.base |= secureBitState;

    return rgnInfo;
}


/*******************************************************************************
* Function Name: Cy_RRAM_GetPhyBase
****************************************************************************//**
*
* Get the physical base address for the provided logical base address.
*
*******************************************************************************/
static uint32_t Cy_RRAM_GetPhyBase(uint32_t addr)
{
    uint32_t physicalAddr = 0;
    addr &= ~CY_RRAM_S_OFFSET_MASK;

    if(addr == CY_RRAM_MAIN_HOST_NS_START_ADDRESS)
    {
        physicalAddr = CY_RRAM_PHYSICAL_MAIN_BASE_ADDR;
    }
    else if(addr == CY_RRAM_WORK_HOST_NS_START_ADDRESS)
    {
        physicalAddr = CY_RRAM_PHYSICAL_WORK_BASE_ADDR;
    }
    else if(addr == CY_RRAM_SFLASH_HOST_NS_START_ADDRESS)
    {
        physicalAddr = CY_RRAM_PHYSICAL_SFLASH_BASE_ADDR;
    }
    else if(addr == CY_RRAM_PROTECTED_HOST_NS_START_ADDRESS)
    {
        physicalAddr = CY_RRAM_PHYSICAL_PROTECTED_BASE_ADDR;
    }
#if (CY_RRAM_RECLAIM_SIZE > 0UL)
    /* Reclaimed band lives at the base of macro0's PROTECTED physical area. */
    else if(addr == CY_RRAM_MAIN_RECLAIM_NS_START_ADDRESS)
    {
        physicalAddr = CY_RRAM_PHYSICAL_PROTECTED_BASE_ADDR;
    }
#if (CY_CPU_CORTEX_M33)
    else if(addr == CY_RRAM_MAIN_RECLAIM_CBUS_NS_START_ADDRESS)
    {
        physicalAddr = CY_RRAM_PHYSICAL_PROTECTED_BASE_ADDR;
    }
#endif
#endif
#if (CY_CPU_CORTEX_M0P)
    else if(addr == CY_RRAM_PROTECTED_PROTECTED_NS_START_ADDRESS)
    {
        physicalAddr = CY_RRAM_PHYSICAL_PROTECTED_BASE_ADDR;
    }
#endif
#if (CY_IP_MXS22RRAMC_VERSION == 1u)
    else if(addr == CY_RRAM_GENERAL_OTP_MMIO_NS_START_ADDRESS)
    {
        physicalAddr = CY_RRAM_PHYSICAL_GENERAL_OTP_BASE_ADDR;
    }
    else
    {
        physicalAddr = CY_RRAM_PHYSICAL_PROTECTED_OTP_BASE_ADDR;
    }
#else
#if (RRAMC_PROTECTED_INTF_PRESENT == 1)
    else if(addr == CY_RRAM_PROTECTED_HOST_CBUS_NS_START_ADDRESS)
    {
        physicalAddr = CY_RRAM_PHYSICAL_PROTECTED_BASE_ADDR;
    }
    else if(addr == CY_RRAM_PROTECTED_OTP_PROTECTED_NS_START_ADDRESS)
    {
        physicalAddr = CY_RRAM_PHYSICAL_PROTECTED_OTP_BASE_ADDR;
    }
    else if(addr == CY_RRAM_GENERAL_OTP_NS_START_ADDRESS)
    {
        physicalAddr = PHYSICAL_GENERAL_OTP_BASE_ADDRESS;
    }
    else
    {
        /* No action required */
    }
#else

    else if(addr == GENERAL_OTP_1_NS_START_ADDRESS)
    {
        physicalAddr = PHYSICAL_GENERAL_OTP_1_BASE_ADDRESS;
    }
    else if(addr == GENERAL_OTP_2_NS_START_ADDRESS)
    {
        physicalAddr = PHYSICAL_GENERAL_OTP_2_BASE_ADDRESS;
    }
    else if(addr == GENERAL_OTP_3_NS_START_ADDRESS)
    {
        physicalAddr = PHYSICAL_GENERAL_OTP_3_BASE_ADDRESS;
    }
    else if(addr == GENERAL_OTP_4_NS_START_ADDRESS)
    {
        physicalAddr = PHYSICAL_GENERAL_OTP_4_BASE_ADDRESS;
    }
    else if(addr == GENERAL_OTP_5_NS_START_ADDRESS)
    {
        physicalAddr = PHYSICAL_GENERAL_OTP_5_BASE_ADDRESS;
    }
    else
    {
        physicalAddr = PHYSICAL_GENERAL_OTP_6_BASE_ADDRESS;
    }
#endif
#endif
    return physicalAddr;
}


/*******************************************************************************
* Function Name: Cy_RRAM_ReadBlock
****************************************************************************//**
*
* Read one of block data from the specified address.
*
*******************************************************************************/
static cy_en_rram_status_t Cy_RRAM_ReadBlock(cy_stc_rram_config_t * config, uint8_t * data, uint32_t pos, uint32_t numBytes)
{
    cy_rram_blk_buf_t blk;
    volatile uint32_t *readAddr = (uint32_t *) config->addr;
    cy_en_rram_status_t status = CY_RRAM_ECC_FAIL;
    uint32_t timeout;

    if(config->tearingSafe)
    {
#if (CY_IP_MXS22RRAMC_VERSION == 2u)
        uint32_t effOffset = (config->addr - config->subRegionBase) + Cy_RRAM_ProtectedShift(config->subRegionBase);
        uint32_t phy_addr = Cy_RRAM_GetPhyBase(config->subRegionBase) +
                ((effOffset - (config->subRegionSize * config->subRegionMacroIdx)) / CY_RRAM_BLOCK_SIZE_BYTES);
#else
        uint32_t phy_addr = Cy_RRAM_GetPhyBase(config->subRegionBase) +
                ((config->addr - config->subRegionBase) / CY_RRAM_BLOCK_SIZE_BYTES);
#endif

#if (CY_IP_MXS22RRAMC_VERSION == 2u)
        /* Set mode to idle to meet initialization condition. */
        Cy_RRAM_SetOperationMode(config->base, config->subRegionMacroIdx, CY_RRAM_IDLE);

        /* Set operation mode to indirect write. */
        Cy_RRAM_SetOperationMode(config->base, config->subRegionMacroIdx, CY_RRAM_INDIRECT_WRITE);

        /* Set the module-internal target address (physical address) */
        RRAM_NVM_ADDR(config->base, config->subRegionMacroIdx) = phy_addr;
#else
        /* Set mode to idle to meet initialization condition. */
        Cy_RRAM_SetOperationMode(config->base, CY_RRAM_IDLE);

        /* Set operation mode to indirect write. */
        Cy_RRAM_SetOperationMode(config->base, CY_RRAM_INDIRECT_WRITE);

        /* Set the module-internal target address (physical address) */
        RRAM_NVM_ADDR(config->base) = phy_addr;
#endif


        timeout = CY_RRAM_OPERATION_TIMEOUT / CY_RRAM_STATUS_CHECK_DELAY;

        /* Wait until internal read operation is completed. As per RRAM IP team,
        * there is no need for timeout as the state machine will definitely
        * return to non-busy state. As a safety measure this timeout is added */
        while(Cy_RRAM_IsBusy(config->base) && (0UL != timeout))
        {
            timeout--;
            Cy_SysLib_DelayCycles(CY_RRAM_STATUS_CHECK_DELAY);
        }

        if(timeout == 0UL)
        {
            return CY_RRAM_OPERATION_TIME_OUT_ERROR;
        }

        /* Validate ECC status */
#if (CY_IP_MXS22RRAMC_VERSION == 2u)
        if(!CY_RRAM_ReadOperationECCStatus(config->base, config->subRegionMacroIdx, CY_RRAM_ECC_READ_BLOCK_MULTIPLE_BIT_FAIL))
#else
        if(!CY_RRAM_ReadOperationECCStatus(config->base, CY_RRAM_ECC_READ_BLOCK_MULTIPLE_BIT_FAIL))
#endif
        {
            /* Continue and read block, there is no uncorrectable ECC errors */
            status = CY_RRAM_SUCCESS;
        }

        /* Set mode to idle */
#if (CY_IP_MXS22RRAMC_VERSION == 2u)
        Cy_RRAM_SetOperationMode(config->base, config->subRegionMacroIdx, CY_RRAM_IDLE);
#else
        Cy_RRAM_SetOperationMode(config->base, CY_RRAM_IDLE);
#endif
    }
    else
    {
        /* Set status to success if read operation is not called from TS library. */
        status = CY_RRAM_SUCCESS;
    }

    if(status == CY_RRAM_SUCCESS)
    {
        for (uint32_t wordCount = 0UL; wordCount < CY_RRAM_BLOCK_SIZE_WORDS; wordCount++)
        {
            blk.w[wordCount] = *readAddr++;
        }

        for (; numBytes != 0UL; --numBytes)
        {
            *data++ = blk.b[pos++];
        }
    }

    return status;
}


/*******************************************************************************
* Function Name: Cy_RRAM_SubRegionReadData
****************************************************************************//**
*
* Reads a number of bytes to a array from the provided address.
*
*******************************************************************************/
static cy_en_rram_status_t Cy_RRAM_SubRegionReadData(cy_stc_rram_config_t * config, uint8_t * data, uint32_t numBytes)
{
    cy_en_rram_status_t status = CY_RRAM_BAD_PARAM;

    /* Validate the parameters */
#if (CY_IP_MXS22RRAMC_VERSION == 2u)
    /* Total accessible bytes for this subregion: MAIN adds reclaim, PROTECTED
     * loses reclaim (from macro0), reclaim itself is single-macro. */
    uint32_t regionTotalR = (CY_RRAMC_NUM_MACROS_MUL) * config->subRegionSize;
#if (CY_RRAM_RECLAIM_SIZE > 0UL)
    {
        uint32_t bnsR = config->subRegionBase & ~CY_RRAM_S_OFFSET_MASK;
        bool isReclaimR = (bnsR == CY_RRAM_MAIN_RECLAIM_NS_START_ADDRESS);
#if (CY_CPU_CORTEX_M33)
        isReclaimR = isReclaimR || (bnsR == CY_RRAM_MAIN_RECLAIM_CBUS_NS_START_ADDRESS);
#endif
        if (isReclaimR) { regionTotalR = config->subRegionSize; }
        else if (Cy_RRAM_IsProtectedSubregion(config->subRegionBase)) { regionTotalR -= CY_RRAM_RECLAIM_SIZE; }
        else { /* Do nothing, regionTotalR remains unchanged */ }
    }
#endif
    if((regionTotalR - (config->addr - config->subRegionBase) < numBytes)
        || (config->subRegionMacroIdx == CY_RRAM_MACRO_IDX_INVALID))
#else
    if((config->subRegionSize - (config->addr - config->subRegionBase)) < numBytes)
#endif
    {
        return status;
    }
    else
    {
        /* The status is being set to SUCCESS in order to make one of the
           entry check for read aligned blocks case as true.
           While reading from an non-aligned block address the status will be overwritten
           based on result from Cy_RRAM_ReadBlock.*/
        status = CY_RRAM_SUCCESS;
    }


#if (CY_IP_MXS22RRAMC_VERSION == 2u)
    /* Reset previous errors on all macros while they are still IDLE,
     * so that Cy_RRAM_GetStatus() does not return stale errors from other macros */
    for(uint8_t idx = 0; idx < CY_RRAMC_NUM_MACROS_MUL; idx++)
    {
        RRAM_NVM_PROG(config->base, idx) |= RRAMC_RRAM_SFR_NVM_PROG_RSTECC_Msk;
    }
#else
    RRAM_NVM_PROG(config->base) |= RRAMC_RRAM_SFR_NVM_PROG_RSTECC_Msk;
#endif
    /* Offset in bytes from block boundary */
    uint32_t offsetStart = config->addr & CY_RRAM_BLOCK_OFFSET_MASK;

    /* If the address is not aligned to the block boundary or the number of bytes to
    * read is less than one block
    */
    if (0UL != offsetStart)
    {
        uint32_t blkAlignedAddr = config->addr & ~CY_RRAM_BLOCK_OFFSET_MASK;
        uint32_t bytesInFirstBlk = CY_RRAM_BLOCK_SIZE_BYTES - offsetStart;

        /* If there is more room in this block than the number of bytes to read */
        if (bytesInFirstBlk > numBytes)
        {
            bytesInFirstBlk = numBytes;
        }
        config->addr = blkAlignedAddr;
        status = Cy_RRAM_ReadBlock(config, data, offsetStart, bytesInFirstBlk);

        numBytes  -= bytesInFirstBlk;
        data += bytesInFirstBlk;
        config->addr += CY_RRAM_BLOCK_SIZE_BYTES;
    }

    /* Read complete 16-byte aligned blocks */
    uint32_t blockCount = numBytes / CY_RRAM_BLOCK_SIZE_BYTES;

#if (CY_IP_MXS22RRAMC_VERSION == 2u)
    /* Calculate how many blocks can be written in the current Macro before switching to the next one */
    uint32_t effOffsetR = (config->addr - config->subRegionBase) + Cy_RRAM_ProtectedShift(config->subRegionBase);
    uint32_t currMacroBytesCount = config->subRegionSize - (effOffsetR - (config->subRegionSize * config->subRegionMacroIdx));
    uint32_t currMacroBlockCount = 0;
    if (currMacroBytesCount > 0)
    {
        currMacroBlockCount = currMacroBytesCount / CY_RRAM_BLOCK_SIZE_BYTES;
    }
#endif
    while (blockCount > 0UL)
    {
        if(CY_RRAM_SUCCESS != status)
        {
            break;
        }
#if (CY_IP_MXS22RRAMC_VERSION == 2u)
        if (currMacroBlockCount > 0)
        {
            status = Cy_RRAM_ReadBlock(config, data, 0UL, CY_RRAM_BLOCK_SIZE_BYTES);
            /* Update the current macro space in block count */
            currMacroBlockCount--;
            config->addr += CY_RRAM_BLOCK_SIZE_BYTES;
            numBytes -= CY_RRAM_BLOCK_SIZE_BYTES;
            data += CY_RRAM_BLOCK_SIZE_BYTES;
            blockCount --;
        }
        else
        {
            /* Switch to the next Macro */
            config->subRegionMacroIdx++;
            /* Set the number of blocks can be written in the next Macros */
            currMacroBlockCount = config->subRegionSize / CY_RRAM_BLOCK_SIZE_BYTES;
        }
#else
        status = Cy_RRAM_ReadBlock(config, data, 0UL, CY_RRAM_BLOCK_SIZE_BYTES);
        config->addr += CY_RRAM_BLOCK_SIZE_BYTES;
        numBytes    -= CY_RRAM_BLOCK_SIZE_BYTES;
        data += CY_RRAM_BLOCK_SIZE_BYTES;
        blockCount --;
#endif
    }

    /* Read remaining bytes from the beginning of the last block */
    if ((0UL != numBytes) && (CY_RRAM_SUCCESS == status))
    {
#if (CY_IP_MXS22RRAMC_VERSION == 2u)
        if (currMacroBlockCount == 0)
        {
            /* Switch to the next Macro */
            config->subRegionMacroIdx++;
        }
#endif
        status = Cy_RRAM_ReadBlock(config, data, 0UL, numBytes);
    }

    return status;
}

/*
 * Weak function controlling ECC validation during read operations.
 * By default ECC check is enabled (returns true). This function is overridden
 * only during very early pre-silicon bring-up when tag bits are being reset.
 * Application code should NOT override this in production.
 *
 * \return true if ECC errors should be checked, false to bypass ECC validation.
 */

__WEAK bool cy_rram_should_check_ecc(void)
{
    return true;
}

/*******************************************************************************
* Function Name: Cy_RRAM_UpdateBlock
****************************************************************************//**
*
* Update one 16-byte RRAM block.
*
*******************************************************************************/
static cy_en_rram_status_t Cy_RRAM_UpdateBlock(cy_stc_rram_config_t * config, const uint8_t * data, uint32_t pos, uint32_t numBytes)
{
    cy_en_rram_status_t status = CY_RRAM_ECC_FAIL;
    cy_rram_blk_buf_t blk;
    uint32_t timeout;
    volatile uint32_t *nvm_addr = (uint32_t *) config->addr;
#if (CY_IP_MXS22RRAMC_VERSION == 2u)
    uint32_t effOffset = (config->addr - config->subRegionBase) + Cy_RRAM_ProtectedShift(config->subRegionBase);
    uint32_t phy_addr = Cy_RRAM_GetPhyBase(config->subRegionBase) + ((effOffset - (config->subRegionSize * config->subRegionMacroIdx)) / CY_RRAM_BLOCK_SIZE_BYTES);
#else
    uint32_t phy_addr = Cy_RRAM_GetPhyBase(config->subRegionBase) + ((config->addr - config->subRegionBase) / CY_RRAM_BLOCK_SIZE_BYTES);
#endif
#if (CY_IP_MXS22RRAMC_VERSION == 1u)
    bool is_otp = (((config->addr >= CY_RRAM_GENERAL_OTP_MMIO_NS_START_ADDRESS) && (config->addr <= CY_RRAM_GENERAL_OTP_MMIO_NS_END_ADDRESS)) ||
                   ((config->addr >= CY_RRAM_GENERAL_OTP_MMIO_S_START_ADDRESS) && (config->addr <= CY_RRAM_GENERAL_OTP_MMIO_S_END_ADDRESS)) ||
                   ((config->addr >= CY_RRAM_PROTECTED_OTP_PROTECTED_S_START_ADDRESS) && (config->addr <= CY_RRAM_PROTECTED_OTP_PROTECTED_S_END_ADDRESS)) ||
                   ((config->addr >= CY_RRAM_PROTECTED_OTP_PROTECTED_NS_START_ADDRESS) && (config->addr <= CY_RRAM_PROTECTED_OTP_PROTECTED_NS_END_ADDRESS)));
#else
#if (RRAMC_PROTECTED_INTF_PRESENT == 1)
    bool is_otp = (((config->addr >= CY_RRAM_GENERAL_OTP_NS_START_ADDRESS) && (config->addr <= CY_RRAM_GENERAL_OTP_NS_END_ADDRESS)) ||
                   ((config->addr >= CY_RRAM_PROTECTED_OTP_PROTECTED_NS_START_ADDRESS) && (config->addr <= CY_RRAM_PROTECTED_OTP_PROTECTED_NS_END_ADDRESS)));
#else
    bool is_otp = (((config->addr >= GENERAL_OTP_1_NS_START_ADDRESS) && (config->addr <= GENERAL_OTP_1_NS_END_ADDRESS)) ||
                   ((config->addr >= GENERAL_OTP_2_NS_START_ADDRESS) && (config->addr <= GENERAL_OTP_2_NS_END_ADDRESS)) ||
                   ((config->addr >= GENERAL_OTP_3_NS_START_ADDRESS) && (config->addr <= GENERAL_OTP_3_NS_END_ADDRESS)) ||
                   ((config->addr >= GENERAL_OTP_4_NS_START_ADDRESS) && (config->addr <= GENERAL_OTP_4_NS_END_ADDRESS)) ||
                   ((config->addr >= GENERAL_OTP_5_NS_START_ADDRESS) && (config->addr <= GENERAL_OTP_5_NS_END_ADDRESS)) ||
                   ((config->addr >= GENERAL_OTP_6_NS_START_ADDRESS) && (config->addr <= GENERAL_OTP_6_NS_END_ADDRESS)) ||
                   ((config->addr >= GENERAL_OTP_1_S_START_ADDRESS) && (config->addr <= GENERAL_OTP_1_S_END_ADDRESS)) ||
                   ((config->addr >= GENERAL_OTP_2_S_START_ADDRESS) && (config->addr <= GENERAL_OTP_2_S_END_ADDRESS)) ||
                   ((config->addr >= GENERAL_OTP_3_S_START_ADDRESS) && (config->addr <= GENERAL_OTP_3_S_END_ADDRESS)) ||
                   ((config->addr >= GENERAL_OTP_4_S_START_ADDRESS) && (config->addr <= GENERAL_OTP_4_S_END_ADDRESS)) ||
                   ((config->addr >= GENERAL_OTP_5_S_START_ADDRESS) && (config->addr <= GENERAL_OTP_5_S_END_ADDRESS)) ||
                   ((config->addr >= GENERAL_OTP_6_S_START_ADDRESS) && (config->addr <= GENERAL_OTP_6_S_END_ADDRESS)));
#endif // (RRAMC_PROTECTED_INTF_PRESENT == 1)
#endif

    /* Set the module-internal target address (physical address) */
#if (CY_IP_MXS22RRAMC_VERSION == 2u)
    RRAM_NVM_ADDR(config->base, config->subRegionMacroIdx) = phy_addr;
#else
    RRAM_NVM_ADDR(config->base) = phy_addr;
#endif
    timeout = CY_RRAM_OPERATION_TIMEOUT / CY_RRAM_STATUS_CHECK_DELAY;

    /* Wait until internal read operation is completed. As per RRAM IP team,
    * there is no need for timeout as the state machine will definitely
    * return to non-busy state. As a safety measure this timeout is added */
    while(Cy_RRAM_IsBusy(config->base) && (0UL != timeout))
    {
        timeout--;
        Cy_SysLib_DelayCycles(CY_RRAM_STATUS_CHECK_DELAY);
    }

    if(timeout == 0UL)
    {
        return CY_RRAM_OPERATION_TIME_OUT_ERROR;
    }

    /* Validate ECC status */
#if (CY_IP_MXS22RRAMC_VERSION == 2u)
    if (CY_RRAM_ReadOperationECCStatus(config->base, config->subRegionMacroIdx, CY_RRAM_ECC_READ_BLOCK_MULTIPLE_BIT_FAIL))
#else
    if (CY_RRAM_ReadOperationECCStatus(config->base, CY_RRAM_ECC_READ_BLOCK_MULTIPLE_BIT_FAIL))
#endif
    {
        if ((!config->tearingSafe) && cy_rram_should_check_ecc())
        {
            return CY_RRAM_ECC_FAIL;
        }

        /* If the block is partially modified, fill block with zero. The block
        * content can't be read due to ECC error.
        */
        for (uint32_t wordCount = 0UL; wordCount < CY_RRAM_BLOCK_SIZE_WORDS; wordCount++)
        {
            blk.w[wordCount] = 0UL;
        }
    }
    else
    {
        /* If the block is partially modified or is located in OTP */
        if ((0UL != pos) || (numBytes < CY_RRAM_BLOCK_SIZE_BYTES) || is_otp)
        {
            for (uint32_t wordCount = 0UL; wordCount < CY_RRAM_BLOCK_SIZE_WORDS; wordCount++)
            {
                blk.w[wordCount] = nvm_addr[wordCount];
            }
        }
    }

    /* Update the block buffer with the data to be written */
    if (is_otp)
    {
        for (; numBytes != 0UL; --numBytes)
        {
            blk.b[pos++] |= *data++;
        }
    }
    else
    {
        for (; numBytes != 0UL; --numBytes)
        {
            blk.b[pos++] = *data++;
        }
    }

    /* Compare the NVM_ADDR register against the correct address before starting the write operation */
#if (CY_IP_MXS22RRAMC_VERSION == 2u)
    if (RRAM_NVM_ADDR(config->base, config->subRegionMacroIdx) != phy_addr)
#else
    if (RRAM_NVM_ADDR(config->base) != phy_addr)
#endif
    {
        return CY_RRAM_WRITE_OPERATION_ERROR;
    }

    if(config->blockingWrite)
    {
        for (uint32_t trials = 0; trials < CY_RRAM_MAX_WRITE_TRIALS; trials++)
        {
            /* Initiate AHB write transactions */
            for (uint32_t wordCount = 0UL; wordCount < CY_RRAM_BLOCK_SIZE_WORDS; wordCount++)
            {
                nvm_addr[wordCount] = blk.w[wordCount];
            }

            /* Flush D-Cache so the writes above reach the RRAMC write buffer before
             * the program command is issued. Without this, dirty cache lines may be
             * evicted after the programming window closes, causing an AXI slave error
             * (imprecise BusFault) on CM55. */
            #if defined(__DCACHE_PRESENT) && (__DCACHE_PRESENT == 1U)
            SCB_CleanDCache_by_Addr((uint32_t *)nvm_addr, (int32_t)CY_RRAM_BLOCK_SIZE_BYTES);
            #endif

#if (CY_IP_MXS22RRAMC_VERSION == 2u)
            Cy_RRAM_StartOperation(config->base, config->subRegionMacroIdx);
#else
            Cy_RRAM_StartOperation(config->base);
#endif
            timeout = CY_RRAM_OPERATION_TIMEOUT / CY_RRAM_STATUS_CHECK_DELAY;

            /* Check for the actual write completion. As per RRAM IP team,
            * there is no need for timeout as the state machine will definitely
            * return to non-busy state. As a safety measure this timeout is added */
            while(Cy_RRAM_IsBusy(config->base) && (0UL != timeout))
            {
                timeout--;
                Cy_SysLib_DelayCycles(CY_RRAM_STATUS_CHECK_DELAY);
            }

            if(timeout == 0UL)
            {
                status = CY_RRAM_OPERATION_TIME_OUT_ERROR;
                break;
            }
#if (CY_IP_MXS22RRAMC_VERSION == 2u)
            cy_en_rram_verr_status_t verrStatus= CY_RRAM_GetFailBitInfo(config->base, config->subRegionMacroIdx);
#else
            cy_en_rram_verr_status_t verrStatus= CY_RRAM_GetFailBitInfo(config->base);
#endif

            /* if no error or single bit error then return success. After final trial if there is a two bit error then also return success */
            if((verrStatus == CY_RRAM_NO_ERROR) ||
               (verrStatus == CY_RRAM_SINGLE_BIT_ERROR) ||
               ((verrStatus == CY_RRAM_DOUBLE_BIT_ERROR) && (trials == (CY_RRAM_MAX_WRITE_TRIALS - 1UL))))
            {
                status = CY_RRAM_SUCCESS;
                break;
            }
            /* For any error other than two or three bit and ECC uncorrectable bit error, return failure */
            else if((verrStatus == CY_RRAM_ECC_ADDR_MISMATCH_ERROR) ||
                    (verrStatus == CY_RRAM_CONFIG_ERROR) ||
                    (verrStatus == CY_RRAM_ALGORITHM_ERROR))
            {
                status = CY_RRAM_VERR_BLOCK_FAIL_BITS;
                break;
            }
            /* Control will reach here when there is two bit or three bit or ECC Uncorrectable bit error. But it will reach here in the
               final trial only when there is three bit or ECC uncorrectable bit error */
            else
            {
                status = CY_RRAM_VERR_BLOCK_FAIL_BITS;
            }
        }
    }
    else
    {
        /* Initiate AHB write transactions */
        for (uint32_t wordCount = 0UL; wordCount < CY_RRAM_BLOCK_SIZE_WORDS; wordCount++)
        {
            nvm_addr[wordCount] = blk.w[wordCount];
        }

        /* Flush D-Cache so the writes above reach the RRAMC write buffer before
         * the program command is issued. Without this, dirty cache lines may be
         * evicted after the programming window closes, causing an AXI slave error
         * (imprecise BusFault) on CM55. */
        #if defined(__DCACHE_PRESENT) && (__DCACHE_PRESENT == 1U)
        SCB_CleanDCache_by_Addr((uint32_t *)nvm_addr, (int32_t)CY_RRAM_BLOCK_SIZE_BYTES);
        #endif

#if (CY_IP_MXS22RRAMC_VERSION == 2u)
        Cy_RRAM_StartOperation(config->base, config->subRegionMacroIdx);
#else
        Cy_RRAM_StartOperation(config->base);
#endif
        status = CY_RRAM_SUCCESS;
    }

    return status;
}


/*******************************************************************************
* Function Name: Cy_RRAM_SubRegionWriteData
****************************************************************************//**
*
* Writes a number of bytes starting from the provided target address.
*
*******************************************************************************/
static cy_en_rram_status_t Cy_RRAM_SubRegionWriteData(cy_stc_rram_config_t * config, const uint8_t * data, uint32_t numBytes)
{
    cy_en_rram_status_t status = CY_RRAM_BAD_PARAM;

    /* CY_RRAMC_NUM_MACROS_MUL by design is >= subRegionMacroIdx */
#if (CY_IP_MXS22RRAMC_VERSION == 2u)
    /* Total accessible bytes for this subregion: MAIN adds reclaim, PROTECTED
     * loses reclaim (from macro0), reclaim itself is single-macro. */
    uint32_t regionTotalW = (CY_RRAMC_NUM_MACROS_MUL) * config->subRegionSize;
#if (CY_RRAM_RECLAIM_SIZE > 0UL)
    {
        uint32_t bnsW = config->subRegionBase & ~CY_RRAM_S_OFFSET_MASK;
        bool isReclaimW = (bnsW == CY_RRAM_MAIN_RECLAIM_NS_START_ADDRESS);
#if (CY_CPU_CORTEX_M33)
        isReclaimW = isReclaimW || (bnsW == CY_RRAM_MAIN_RECLAIM_CBUS_NS_START_ADDRESS);
#endif
        if (isReclaimW) { regionTotalW = config->subRegionSize; }
        else if (Cy_RRAM_IsProtectedSubregion(config->subRegionBase)) { regionTotalW -= CY_RRAM_RECLAIM_SIZE; }
        else { /* Do nothing, regionTotalW remains unchanged */ }
    }
#endif
    if((regionTotalW - (config->addr - config->subRegionBase) < numBytes)
        || (config->subRegionMacroIdx == CY_RRAM_MACRO_IDX_INVALID))
#else
    if((( config->subRegionSize) - (config->addr - config->subRegionBase)) < numBytes)
#endif
    {
        return status;
    }
    else
    {
        /* The status is being set to SUCCESS in order to make one of the
           entry check for only Write complete 16-byte aligned blocks case as true.
           While writing to a non-aligned block address the status will be overwritten
           based on result from Cy_RRAM_UpdateBlock.*/
        status = CY_RRAM_SUCCESS;
    }
#if (CY_IP_MXS22RRAMC_VERSION == 2u)

    /* Reset previous errors on all macros while they are still IDLE,
     * so that Cy_RRAM_GetStatus() does not return stale errors from other macros */
    for(uint8_t idx = 0; idx < CY_RRAMC_NUM_MACROS_MUL; idx++)
    {
        RRAM_NVM_PROG(config->base, idx) |= (RRAMC_RRAM_SFR_NVM_PROG_RSTVERR_Msk | RRAMC_RRAM_SFR_NVM_PROG_RSTECC_Msk);
    }

    /* Set mode to idle to meet initialization condition */
    Cy_RRAM_SetOperationMode(config->base, config->subRegionMacroIdx, CY_RRAM_IDLE);

    /* Set operation mode.to indirect write.*/
    Cy_RRAM_SetOperationMode(config->base, config->subRegionMacroIdx, CY_RRAM_INDIRECT_WRITE);
#else
    /* Reset previous errors while they are still IDLE,
     * so that Cy_RRAM_GetStatus() does not return stale errors from other macros */
    RRAM_NVM_PROG(config->base) |= RRAMC_RRAM_SFR_NVM_PROG_RSTVERR_Msk | RRAMC_RRAM_SFR_NVM_PROG_RSTECC_Msk;
    /* Set mode to idle to meet initialization condition */
    Cy_RRAM_SetOperationMode(config->base, CY_RRAM_IDLE);

    /* Set operation mode.to indirect write.*/
    Cy_RRAM_SetOperationMode(config->base, CY_RRAM_INDIRECT_WRITE);
#endif

    /* Offset in bytes from block boundary */
    uint32_t offsetStart = config->addr & CY_RRAM_BLOCK_OFFSET_MASK;

    /* If the address is not aligned to the block boundary or the number of bytes to
    * write is less than one block
    */
    if (0UL != offsetStart)
    {
        uint32_t blkAlignedAddr = config->addr & ~CY_RRAM_BLOCK_OFFSET_MASK;
        uint32_t bytesInFirstBlk = CY_RRAM_BLOCK_SIZE_BYTES - offsetStart;

        /* If there is more room in this block than the number of bytes to write */
        if (bytesInFirstBlk > numBytes)
        {
            bytesInFirstBlk = numBytes;
        }

        config->addr = blkAlignedAddr;
        status = Cy_RRAM_UpdateBlock(config, data, offsetStart, bytesInFirstBlk);

        numBytes -= bytesInFirstBlk;
        data += bytesInFirstBlk;
        config->addr += CY_RRAM_BLOCK_SIZE_BYTES;
    }

    /* Write complete 16-byte aligned blocks */
    uint32_t blockCount = numBytes / CY_RRAM_BLOCK_SIZE_BYTES;

#if (CY_IP_MXS22RRAMC_VERSION == 2u)
    /* Calculate how many blocks can be written in the current Macro before switching to the next one */
    uint32_t effOffsetW = (config->addr - config->subRegionBase) + Cy_RRAM_ProtectedShift(config->subRegionBase);
    uint32_t currMacroBytesCount = config->subRegionSize - (effOffsetW - (config->subRegionSize * config->subRegionMacroIdx));
    uint32_t currMacroBlockCount = 0;
    if (currMacroBytesCount > 0)
    {
        currMacroBlockCount = currMacroBytesCount / CY_RRAM_BLOCK_SIZE_BYTES;
    }
#endif
    while (blockCount > 0UL)
    {
        if(CY_RRAM_SUCCESS != status)
        {
            break;
        }
#if (CY_IP_MXS22RRAMC_VERSION == 2u)
        if (currMacroBlockCount > 0)
        {
            status = Cy_RRAM_UpdateBlock(config, data, 0UL, CY_RRAM_BLOCK_SIZE_BYTES);
            /* Update the current macro space in block count */
            currMacroBlockCount--;
            config->addr += CY_RRAM_BLOCK_SIZE_BYTES;
            numBytes -= CY_RRAM_BLOCK_SIZE_BYTES;
            data += CY_RRAM_BLOCK_SIZE_BYTES;
            blockCount--;
        }
        else
        {
            /* Set mode to idle */
            Cy_RRAM_SetOperationMode(config->base, config->subRegionMacroIdx, CY_RRAM_IDLE);

            /* Switch to the next Macro */
            config->subRegionMacroIdx++;
            /* Set the number of blocks can be written in the next Macros */
            currMacroBlockCount = config->subRegionSize / CY_RRAM_BLOCK_SIZE_BYTES;

            /* Set mode to idle to meet initialization condition */
            Cy_RRAM_SetOperationMode(config->base, config->subRegionMacroIdx, CY_RRAM_IDLE);

            /* Set operation mode.to indirect write.*/
            Cy_RRAM_SetOperationMode(config->base, config->subRegionMacroIdx, CY_RRAM_INDIRECT_WRITE);
        }
#else
        status = Cy_RRAM_UpdateBlock(config, data, 0UL, CY_RRAM_BLOCK_SIZE_BYTES);
        config->addr += CY_RRAM_BLOCK_SIZE_BYTES;
        numBytes    -= CY_RRAM_BLOCK_SIZE_BYTES;
        data += CY_RRAM_BLOCK_SIZE_BYTES;
        blockCount--;
#endif
    }

    /* Write remaining bytes at the beginning of the last block */
    if ((0UL != numBytes) && (CY_RRAM_SUCCESS == status))
    {
#if (CY_IP_MXS22RRAMC_VERSION == 2u)
        if (currMacroBlockCount == 0)
        {
            /* Set mode to idle */
            Cy_RRAM_SetOperationMode(config->base, config->subRegionMacroIdx, CY_RRAM_IDLE);

            /* Switch to the next Macro */
            config->subRegionMacroIdx++;

            /* Set mode to idle to meet initialization condition */
            Cy_RRAM_SetOperationMode(config->base, config->subRegionMacroIdx, CY_RRAM_IDLE);

            /* Set operation mode.to indirect write.*/
            Cy_RRAM_SetOperationMode(config->base, config->subRegionMacroIdx, CY_RRAM_INDIRECT_WRITE);
        }
#endif
        status = Cy_RRAM_UpdateBlock(config, data, 0UL, numBytes);
    }

    /* Set mode to idle */
#if (CY_IP_MXS22RRAMC_VERSION == 2u)
    Cy_RRAM_SetOperationMode(config->base, config->subRegionMacroIdx, CY_RRAM_IDLE);
#else
    Cy_RRAM_SetOperationMode(config->base, CY_RRAM_IDLE);
#endif

    return status;
}

/*******************************************************************************
* Function Name: Cy_RRAM_IsParamValid
****************************************************************************//**
*
* Validate the Parameters.
* Return true if the parameters are valid else return false
*
*******************************************************************************/
static bool Cy_RRAM_IsParamValid(RRAMC_Type *base, uint32_t addr, cy_stc_rram_rgn_info_t rgnInfo, uint32_t numBytes)
{
    uint32_t maxBytes;
    uint32_t offset;

    if((rgnInfo.base == CY_RRAM_INVALID_ACCESS) || (rgnInfo.size == CY_RRAM_INVALID_ACCESS))
    {
        return false;
    }
    offset = addr - rgnInfo.base;

#if (CY_IP_MXS22RRAMC_VERSION == 2u)
#if (CY_RRAM_RECLAIM_SIZE > 0UL)
    uint32_t baseNs = rgnInfo.base & ~CY_RRAM_S_OFFSET_MASK;
    bool isReclaim = (baseNs == CY_RRAM_MAIN_RECLAIM_NS_START_ADDRESS)
#if (CY_CPU_CORTEX_M33)
                  || (baseNs == CY_RRAM_MAIN_RECLAIM_CBUS_NS_START_ADDRESS)
#endif
                  ;
    bool isMain = (baseNs == CY_RRAM_MAIN_HOST_NS_START_ADDRESS)
#if (CY_CPU_CORTEX_M33)
               || (baseNs == CY_RRAM_MAIN_HOST_CBUS_NS_START_ADDRESS)
#endif
               ;
    /* Reclaim is single-macro; MAIN's visible size includes reclaim;
     * PROTECTED's visible size loses reclaim (macro0 gives up its first slice). */
    if (isReclaim)
    {
        maxBytes = rgnInfo.size;
    }
    else if (isMain)
    {
        maxBytes = (rgnInfo.size * CY_RRAMC_NUM_MACROS_MUL) + CY_RRAM_RECLAIM_SIZE;
    }
    else if (Cy_RRAM_IsProtectedSubregion(rgnInfo.base))
    {
        maxBytes = (rgnInfo.size * CY_RRAMC_NUM_MACROS_MUL) - CY_RRAM_RECLAIM_SIZE;
    }
    else
    {
        maxBytes = rgnInfo.size * CY_RRAMC_NUM_MACROS_MUL;
    }
#else
    maxBytes = rgnInfo.size * CY_RRAMC_NUM_MACROS_MUL;
#endif
#else
    maxBytes = rgnInfo.size;
#endif

    if((NULL != base) &&
       (numBytes > 0UL) &&
       (offset < maxBytes) &&
       (numBytes <= (maxBytes - offset)))
    {
        return true;
    }
    else
    {
        return false;
    }
}

#if (CY_IP_MXS22RRAMC_VERSION == 2u) && (CY_RRAM_RECLAIM_SIZE > 0UL)
/* Split MAIN transfer that overflows into the reclaim band.
 * Returns bytes consumable from the MAIN region for the current start address;
 * caller must issue the remainder against the reclaim subregion.
 * Returns 0 if this is not a MAIN transfer or fits entirely within MAIN. */
static uint32_t Cy_RRAM_MainReclaimSplit(const cy_stc_rram_config_t * config, uint32_t numBytes)
{
    uint32_t baseNs = config->subRegionBase & ~CY_RRAM_S_OFFSET_MASK;
    bool isMain = (baseNs == CY_RRAM_MAIN_HOST_NS_START_ADDRESS)
#if (CY_CPU_CORTEX_M33)
               || (baseNs == CY_RRAM_MAIN_HOST_CBUS_NS_START_ADDRESS)
#endif
               ;
    if(!isMain)
    {
        return 0UL;
    }
    uint32_t mainTotal    = config->subRegionSize * CY_RRAMC_NUM_MACROS_MUL;
    uint32_t offset       = config->addr - config->subRegionBase;
    uint32_t mainAvailable = (mainTotal > offset) ? (mainTotal - offset) : 0UL;
    return (numBytes > mainAvailable) ? mainAvailable : 0UL;
}

/* Rewrite config to point at the reclaim subregion after a MAIN portion finished. */
static void Cy_RRAM_SetupReclaimContinuation(cy_stc_rram_config_t * config)
{
    uint32_t secureBit = config->subRegionBase & CY_RRAM_S_OFFSET_MASK;
    uint32_t baseNs    = config->subRegionBase & ~CY_RRAM_S_OFFSET_MASK;
    uint32_t reclaimBaseNs;
#if (CY_CPU_CORTEX_M33)
    reclaimBaseNs = (baseNs == CY_RRAM_MAIN_HOST_CBUS_NS_START_ADDRESS)
                        ? CY_RRAM_MAIN_RECLAIM_CBUS_NS_START_ADDRESS
                        : CY_RRAM_MAIN_RECLAIM_NS_START_ADDRESS;
#else
    (void)baseNs;
    reclaimBaseNs = CY_RRAM_MAIN_RECLAIM_NS_START_ADDRESS;
#endif
    config->subRegionBase     = reclaimBaseNs | secureBit;
    config->subRegionSize     = CY_RRAM_RECLAIM_SIZE;
    config->subRegionMacroIdx = 0U;
    config->addr              = config->subRegionBase;
}
#endif

/*******************************************************************************
* Function Name: Cy_RRAM_WriteData
****************************************************************************//**
*
* Makes sure that the lock is acquired and performs the write Data operation.
* Return true if the parameters are valid else return false
*
*******************************************************************************/
static cy_en_rram_status_t Cy_RRAM_WriteData(RRAMC_Type *base, cy_stc_rram_config_t * config, const uint8_t * data, uint32_t numBytes, bool secure)
{
    cy_en_rram_status_t status = CY_RRAM_BAD_PARAM;

    /* Acquire a PC to confirm the transaction is effective. */
    if(Cy_RRAM_AcquirePCLock(base, secure))
    {
#if (CY_IP_MXS22RRAMC_VERSION == 2u) && (CY_RRAM_RECLAIM_SIZE > 0UL)
        uint32_t mainBytes = Cy_RRAM_MainReclaimSplit(config, numBytes);
        if(mainBytes != 0UL)
        {
            status = Cy_RRAM_SubRegionWriteData(config, data, mainBytes);
            if(CY_RRAM_SUCCESS == status)
            {
                Cy_RRAM_SetupReclaimContinuation(config);
                status = Cy_RRAM_SubRegionWriteData(config, data + mainBytes, numBytes - mainBytes);
            }
        }
        else
        {
            status = Cy_RRAM_SubRegionWriteData(config, data, numBytes);
        }
#else
        status = Cy_RRAM_SubRegionWriteData(config, data, numBytes);
#endif
        Cy_RRAM_ReleasePCLock(base);
    }
    else
    {
        status = CY_RRAM_ACQUIRE_PC_LOCK_FAIL;
    }

    return status;
}

/*******************************************************************************
* Function Name: Cy_RRAM_ReadData
****************************************************************************//**
*
* Makes sure that the lock is acquired and performs the write Data operation.
* Return true if the parameters are valid else return false
*
*******************************************************************************/
static cy_en_rram_status_t Cy_RRAM_ReadData(RRAMC_Type *base, cy_stc_rram_config_t * config, uint8_t * data, uint32_t numBytes, bool secure)
{
    cy_en_rram_status_t status = CY_RRAM_BAD_PARAM;

    /* Acquire a PC to confirm the transaction is effective. */
    if(Cy_RRAM_AcquirePCLock(base, secure))
    {
#if (CY_IP_MXS22RRAMC_VERSION == 2u) && (CY_RRAM_RECLAIM_SIZE > 0UL)
        uint32_t mainBytes = Cy_RRAM_MainReclaimSplit(config, numBytes);
        if(mainBytes != 0UL)
        {
            status = Cy_RRAM_SubRegionReadData(config, data, mainBytes);
            if(CY_RRAM_SUCCESS == status)
            {
                Cy_RRAM_SetupReclaimContinuation(config);
                status = Cy_RRAM_SubRegionReadData(config, data + mainBytes, numBytes - mainBytes);
            }
        }
        else
        {
            status = Cy_RRAM_SubRegionReadData(config, data, numBytes);
        }
#else
        status = Cy_RRAM_SubRegionReadData(config, data, numBytes);
#endif
        Cy_RRAM_ReleasePCLock(base);
    }
    else
    {
        status = CY_RRAM_ACQUIRE_PC_LOCK_FAIL;
    }

    return status;
}

/*******************************************************************************
* Function Name: Cy_RRAM_ReadByteArray_internal
****************************************************************************//**
*
* Reads array of byte data to the specified address.
*
*******************************************************************************/
static cy_en_rram_status_t Cy_RRAM_ReadByteArray_internal(cy_stc_rram_config_t * config, cy_stc_rram_rgn_info_t rgnInfo, uint8_t * data, uint32_t numBytes, bool secure)
{
    cy_en_rram_status_t status = CY_RRAM_BAD_PARAM;

    if(Cy_RRAM_IsParamValid(config->base, config->addr, rgnInfo, numBytes) && NULL != data)
    {
        status = Cy_RRAM_ReadData(config->base, config, data, numBytes, secure);
    }

    return status;
}

/*******************************************************************************
* Function Name: Cy_RRAM_WriteByteArray_internal
****************************************************************************//**
*
* Writes array of byte data to the specified address.
*
*******************************************************************************/
static cy_en_rram_status_t Cy_RRAM_WriteByteArray_internal(cy_stc_rram_config_t * config, cy_stc_rram_rgn_info_t rgnInfo, const uint8_t * data, uint32_t numBytes, bool secure)
{
    cy_en_rram_status_t status = CY_RRAM_BAD_PARAM;

    if(Cy_RRAM_IsParamValid(config->base, config->addr, rgnInfo, numBytes) && NULL != data)
    {
        status = Cy_RRAM_WriteData(config->base, config, data, numBytes, secure);
    }

    return status;
}


/*******************************************************************************
* Function Name: Cy_RRAM_OtpWriteByteArray_internal
****************************************************************************//**
*
* Writes array of byte data to the specified address.
* The address should fall in General OTP or Protected OTP subsection.
*
*******************************************************************************/
static cy_en_rram_status_t Cy_RRAM_OtpWriteByteArray_internal(RRAMC_Type * base, uint32_t addr, const uint8_t * data, uint32_t numBytes, bool secure)
{
    cy_en_rram_status_t status = CY_RRAM_BAD_PARAM;
    cy_stc_rram_rgn_info_t rgnInfo = Cy_RRAM_GetOTPSubRegionInfo(addr);

#if (CY_IP_MXS22RRAMC_VERSION == 2u)
    cy_stc_rram_config_t config = {base, addr, rgnInfo.base, rgnInfo.size, rgnInfo.macro_idx, true, false};
#else
    cy_stc_rram_config_t config = {base, addr, rgnInfo.base, rgnInfo.size, true, false};
#endif

    status = Cy_RRAM_WriteByteArray_internal(&config, rgnInfo, data, numBytes, secure);

    return status;
}

/*******************************************************************************
* Function Name: Cy_RRAM_NvmWriteByteArray_internal
****************************************************************************//**
*
* Writes array of byte data to the specified address in blocking mode.
* The address should fall in Main or Work or Sflash or Protected NVM subsection.
*
*******************************************************************************/
static cy_en_rram_status_t Cy_RRAM_NvmWriteByteArray_internal(RRAMC_Type * base, uint32_t addr, const uint8_t * data, uint32_t numBytes, bool secure)
{
    cy_en_rram_status_t status = CY_RRAM_BAD_PARAM;
    cy_stc_rram_rgn_info_t rgnInfo = Cy_RRAM_GetNVMRegionInfo(addr);

#if (CY_IP_MXS22RRAMC_VERSION == 2u)
    cy_stc_rram_config_t config = {base, addr, rgnInfo.base, rgnInfo.size, rgnInfo.macro_idx, true, false};
#else
    cy_stc_rram_config_t config = {base, addr, rgnInfo.base, rgnInfo.size, true, false};
#endif

    status = Cy_RRAM_WriteByteArray_internal(&config, rgnInfo, data, numBytes, secure);

    return status;
}


/*******************************************************************************
* Function Name: Cy_RRAM_TSWriteByteArray_internal
****************************************************************************//**
*
* Writes an array of byte data to the specified address in blocking mode without considering
* uncorrectable ECC fail error.
* This API should be used only for tearing safe use cases.
*
*******************************************************************************/
static cy_en_rram_status_t Cy_RRAM_TSWriteByteArray_internal(RRAMC_Type * base, uint32_t addr, const uint8_t * data, uint32_t numBytes, bool secure)
{
    cy_en_rram_status_t status = CY_RRAM_BAD_PARAM;
    cy_stc_rram_rgn_info_t rgnInfo;

    if(Cy_RRAM_GetNVMRegionInfo(addr).size != 0UL)
    {
        rgnInfo = Cy_RRAM_GetNVMRegionInfo(addr);

    }
    else if(Cy_RRAM_GetOTPSubRegionInfo(addr).size != 0UL)
    {
        rgnInfo = Cy_RRAM_GetOTPSubRegionInfo(addr);
    }
    else
    {
        return CY_RRAM_BAD_PARAM;
    }

#if (CY_IP_MXS22RRAMC_VERSION == 2u)
    cy_stc_rram_config_t config = {base, addr, rgnInfo.base, rgnInfo.size, rgnInfo.macro_idx, true, true};
#else
    cy_stc_rram_config_t config = {base, addr, rgnInfo.base, rgnInfo.size, true, true};
#endif

    status = Cy_RRAM_WriteByteArray_internal(&config, rgnInfo, data, numBytes, secure);

    return status;
}


/*******************************************************************************
* Function Name: Cy_RRAM_TSReadByteArray_internal
****************************************************************************//**
*
* Reads an array of byte data from the specified address after validating the
* uncorrectable ECC fail bits.
* Returns ECC fail error if uncorrectable ECC fail error exist.
* This API should be used only for tearing safe use cases.
*
*******************************************************************************/
static cy_en_rram_status_t Cy_RRAM_TSReadByteArray_internal(RRAMC_Type * base, uint32_t addr, uint8_t * data, uint32_t numBytes, bool secure)
{
    cy_en_rram_status_t status = CY_RRAM_BAD_PARAM;
    cy_stc_rram_rgn_info_t rgnInfo;

    if(Cy_RRAM_GetNVMRegionInfo(addr).size != 0UL)
    {
        rgnInfo = Cy_RRAM_GetNVMRegionInfo(addr);
    }
    else if(Cy_RRAM_GetOTPSubRegionInfo(addr).size != 0UL)
    {
        rgnInfo = Cy_RRAM_GetOTPSubRegionInfo(addr);
    }
    else
    {
        return CY_RRAM_BAD_PARAM;
    }

#if (CY_IP_MXS22RRAMC_VERSION == 2u)
    cy_stc_rram_config_t config = {base, addr, rgnInfo.base, rgnInfo.size, rgnInfo.macro_idx, false, true};
#else
    cy_stc_rram_config_t config = {base, addr, rgnInfo.base, rgnInfo.size, false, true};
#endif

    status = Cy_RRAM_ReadByteArray_internal(&config, rgnInfo, data, numBytes, secure);

    return status;
}

/*******************************************************************************
* Function Name: Cy_RRAM_OtpWriteByteArray_S
****************************************************************************//**
*
* Writes array of byte data to the specified address with Secure alias.
* The address should fall in General OTP or Protected OTP subsection.
*
*******************************************************************************/
cy_en_rram_status_t Cy_RRAM_OtpWriteByteArray_S(RRAMC_Type * base, uint32_t addr, const uint8_t * data, uint32_t numBytes)
{
    return Cy_RRAM_OtpWriteByteArray_internal(base, addr, data, numBytes, true);
}

/*******************************************************************************
* Function Name: Cy_RRAM_OtpWriteByteArray
****************************************************************************//**
*
* Writes array of byte data to the specified address.
* The address should fall in General OTP or Protected OTP subsection.
*
*******************************************************************************/
cy_en_rram_status_t Cy_RRAM_OtpWriteByteArray(RRAMC_Type * base, uint32_t addr, const uint8_t * data, uint32_t numBytes)
{
    return Cy_RRAM_OtpWriteByteArray_internal(base, addr, data, numBytes, false);
}


/*******************************************************************************
* Function Name: Cy_RRAM_OtpReadByteArray
****************************************************************************//**
*
* Reads array of byte data from the specified address.
* The address should fall in General OTP or Protected OTP subsection.
*
*******************************************************************************/
cy_en_rram_status_t Cy_RRAM_OtpReadByteArray(RRAMC_Type * base, uint32_t addr, uint8_t * data, uint32_t numBytes)
{
    cy_en_rram_status_t status = CY_RRAM_BAD_PARAM;
    cy_stc_rram_rgn_info_t rgnInfo = Cy_RRAM_GetOTPSubRegionInfo(addr);

    if(Cy_RRAM_IsParamValid(base, addr, rgnInfo, numBytes) && NULL != data)
    {
#if (CY_IP_MXS22RRAMC_VERSION == 2u)
        cy_stc_rram_config_t config = {base, addr, rgnInfo.base, rgnInfo.size, rgnInfo.macro_idx, false, false};
#else
        cy_stc_rram_config_t config = {base, addr, rgnInfo.base, rgnInfo.size, false, false};
#endif
        status = Cy_RRAM_SubRegionReadData(&config, data, numBytes);
    }

    return status;
}

/*******************************************************************************
* Function Name: Cy_RRAM_OtpWriteWord_S
****************************************************************************//**
*
* Writes a 32-bit word data to the specified address with Secure alias.
* The address should fall in General OTP or Protected OTP subsection.
*
*******************************************************************************/
cy_en_rram_status_t Cy_RRAM_OtpWriteWord_S(RRAMC_Type * base, uint32_t addr, uint32_t data)
{
    return Cy_RRAM_OtpWriteByteArray_S(base, addr, (const uint8_t *)(&data), sizeof(uint32_t));
}


/*******************************************************************************
* Function Name: Cy_RRAM_OtpWriteWord
****************************************************************************//**
*
* Writes a 32-bit word data to the specified address.
* The address should fall in General OTP or Protected OTP subsection.
*
*******************************************************************************/
cy_en_rram_status_t Cy_RRAM_OtpWriteWord(RRAMC_Type * base, uint32_t addr, uint32_t data)
{
    return Cy_RRAM_OtpWriteByteArray(base, addr, (const uint8_t *)(&data), sizeof(uint32_t));
}


/*******************************************************************************
* Function Name: Cy_RRAM_OtpReadWord
****************************************************************************//**
*
* Reads a 32bit word data from the specified address.
* The address should fall in General OTP or Protected OTP subsection.
*
*******************************************************************************/
cy_en_rram_status_t Cy_RRAM_OtpReadWord(RRAMC_Type * base, uint32_t addr, uint32_t * data)
{
    return Cy_RRAM_OtpReadByteArray(base, addr, (uint8_t *)(data), sizeof(uint32_t));
}


/*******************************************************************************
* Function Name: Cy_RRAM_OtpWriteBlock
****************************************************************************//**
*
* Writes a block(16B) data to the specified address.
* The address should be block aligned.
* The address should fall in General OTP or Protected OTP subsection.
*
*******************************************************************************/
cy_en_rram_status_t Cy_RRAM_OtpWriteBlock(RRAMC_Type * base, uint32_t addr, const uint8_t *data)
{
    cy_en_rram_status_t status = CY_RRAM_BAD_PARAM;

    if((addr % CY_RRAM_BLOCK_SIZE_BYTES == 0UL))
    {
         status =  Cy_RRAM_OtpWriteByteArray(base, addr, data, CY_RRAM_BLOCK_SIZE_BYTES);
    }

    return status;
}


/*******************************************************************************
* Function Name: Cy_RRAM_NvmWriteByteArray_S
****************************************************************************//**
*
* Writes array of byte data to the specified address in blocking mode.
* The address should fall in Main or Work or Sflash or Protected NVM subsection.
*
*******************************************************************************/
cy_en_rram_status_t Cy_RRAM_NvmWriteByteArray_S(RRAMC_Type * base, uint32_t addr, const uint8_t * data, uint32_t numBytes)
{
    return Cy_RRAM_NvmWriteByteArray_internal(base, addr, data, numBytes, true);
}


/*******************************************************************************
* Function Name: Cy_RRAM_NvmWriteByteArray
****************************************************************************//**
*
* Writes array of byte data to the specified address in blocking mode.
* The address should fall in Main or Work or Sflash or Protected NVM subsection.
*
*******************************************************************************/
cy_en_rram_status_t Cy_RRAM_NvmWriteByteArray(RRAMC_Type * base, uint32_t addr, const uint8_t * data, uint32_t numBytes)
{
    return Cy_RRAM_NvmWriteByteArray_internal(base, addr, data, numBytes, false);
}


/*******************************************************************************
* Function Name: Cy_RRAM_NvmReadByteArray
****************************************************************************//**
*
* Reads array of byte data from the specified address.
* The address should fall in Main, Work, Sflash, Protected NVM subsection.
*
*******************************************************************************/
cy_en_rram_status_t Cy_RRAM_NvmReadByteArray(RRAMC_Type * base, uint32_t addr, uint8_t * data, uint32_t numBytes)
{
    cy_en_rram_status_t status = CY_RRAM_BAD_PARAM;
    cy_stc_rram_rgn_info_t rgnInfo = Cy_RRAM_GetNVMRegionInfo(addr);

    if(Cy_RRAM_IsParamValid(base, addr, rgnInfo, numBytes) && NULL != data)
    {
#if (CY_IP_MXS22RRAMC_VERSION == 2u)
        cy_stc_rram_config_t config = {base, addr, rgnInfo.base, rgnInfo.size, rgnInfo.macro_idx, false, false};
#else
        cy_stc_rram_config_t config = {base, addr, rgnInfo.base, rgnInfo.size, false, false};
#endif
#if (CY_IP_MXS22RRAMC_VERSION == 2u) && (CY_RRAM_RECLAIM_SIZE > 0UL)
        uint32_t mainBytes = Cy_RRAM_MainReclaimSplit(&config, numBytes);
        if(mainBytes != 0UL)
        {
            status = Cy_RRAM_SubRegionReadData(&config, data, mainBytes);
            if(CY_RRAM_SUCCESS == status)
            {
                Cy_RRAM_SetupReclaimContinuation(&config);
                status = Cy_RRAM_SubRegionReadData(&config, data + mainBytes, numBytes - mainBytes);
            }
        }
        else
        {
            status = Cy_RRAM_SubRegionReadData(&config, data, numBytes);
        }
#else
        status = Cy_RRAM_SubRegionReadData(&config, data, numBytes);
#endif
    }

    return status;
}


/*******************************************************************************
* Function Name: Cy_RRAM_NonBlockingNvmWriteByteArray
****************************************************************************//**
*
* Writes array of byte data to the specified address in Non Blocking mode.
* The address should fall in Main or Work or Sflash or Protected NVM subsection.
*
*******************************************************************************/
cy_en_rram_status_t Cy_RRAM_NonBlockingNvmWriteByteArray(RRAMC_Type * base, uint32_t addr, const uint8_t * data, uint32_t numBytes)
{
    cy_en_rram_status_t status = CY_RRAM_BAD_PARAM;
    cy_stc_rram_rgn_info_t rgnInfo = Cy_RRAM_GetNVMRegionInfo(addr);

#if (CY_IP_MXS22RRAMC_VERSION == 2u)
    cy_stc_rram_config_t config = {base, addr, rgnInfo.base, rgnInfo.size, rgnInfo.macro_idx, false, false};
#else
    cy_stc_rram_config_t config = {base, addr, rgnInfo.base, rgnInfo.size, false, false};
#endif

    status = Cy_RRAM_WriteByteArray_internal(&config, rgnInfo, data, numBytes, false);


    return status;
}


/*******************************************************************************
* Function Name: Cy_RRAM_NvmWriteBlock
****************************************************************************//**
*
* Writes a block(16B) data to the specified address in Blocking mode.
* The address should fall in Main or Work or Sflash or Protected NVM subsection.
*
*******************************************************************************/
cy_en_rram_status_t Cy_RRAM_NvmWriteBlock(RRAMC_Type * base, uint32_t addr, const uint8_t * data)
{
    cy_en_rram_status_t status = CY_RRAM_BAD_PARAM;

    if((addr % CY_RRAM_BLOCK_SIZE_BYTES == 0UL))
    {
         status =  Cy_RRAM_NvmWriteByteArray(base, addr, data, CY_RRAM_BLOCK_SIZE_BYTES);
    }

    return status;
}


/*******************************************************************************
* Function Name: Cy_RRAM_WriteByteArray
****************************************************************************//**
*
* Writes array of byte data to the provided RRAM address in blocking mode.
* API will wait for the write operation to be completed. It returns error or success status.
*
*******************************************************************************/
cy_en_rram_status_t Cy_RRAM_WriteByteArray(RRAMC_Type * base, uint32_t addr, const uint8_t * data, uint32_t numBytes)
{
    cy_en_rram_status_t status = CY_RRAM_BAD_PARAM;

    if(Cy_RRAM_GetNVMRegionInfo(addr).size != 0UL)
    {
        status = Cy_RRAM_NvmWriteByteArray(base, addr, data, numBytes);
    }
    else if(Cy_RRAM_GetOTPSubRegionInfo(addr).size != 0UL)
    {
        status = Cy_RRAM_OtpWriteByteArray(base, addr, data, numBytes);
    }
    else
    {
        status = CY_RRAM_BAD_PARAM;
    }

    return status;
}

/*******************************************************************************
* Function Name: Cy_RRAM_TSWriteByteArray_S
****************************************************************************//**
*
* Writes an array of byte data to the specified address in blocking mode without considering
* uncorrectable ECC fail error.
* This API should be used only for tearing safe use cases.
*
*******************************************************************************/
cy_en_rram_status_t Cy_RRAM_TSWriteByteArray_S(RRAMC_Type * base, uint32_t addr, const uint8_t * data, uint32_t numBytes)
{
    return Cy_RRAM_TSWriteByteArray_internal(base, addr, data, numBytes, true);
}


/*******************************************************************************
* Function Name: Cy_RRAM_TSWriteByteArray
****************************************************************************//**
*
* Writes an array of byte data to the specified address in blocking mode without considering
* uncorrectable ECC fail error.
* This API should be used only for tearing safe use cases.
*
*******************************************************************************/
cy_en_rram_status_t Cy_RRAM_TSWriteByteArray(RRAMC_Type * base, uint32_t addr, const uint8_t * data, uint32_t numBytes)
{
    return Cy_RRAM_TSWriteByteArray_internal(base, addr, data, numBytes, false);
}


/*******************************************************************************
* Function Name: Cy_RRAM_ReadByteArray
****************************************************************************//**
*
* Reads a array of byte data from the provided RRAM address.
*
*******************************************************************************/
cy_en_rram_status_t Cy_RRAM_ReadByteArray(RRAMC_Type * base, uint32_t addr, uint8_t * data, uint32_t numBytes)
{
    cy_en_rram_status_t status = CY_RRAM_BAD_PARAM;

    if(Cy_RRAM_GetNVMRegionInfo(addr).size != 0UL)
    {
        status = Cy_RRAM_NvmReadByteArray(base, addr, data, numBytes);
    }
    else if(Cy_RRAM_GetOTPSubRegionInfo(addr).size != 0UL)
    {
        status = Cy_RRAM_OtpReadByteArray(base, addr, data, numBytes);
    }
    else
    {
        status = CY_RRAM_BAD_PARAM;
    }

    return status;
}

/*******************************************************************************
* Function Name: Cy_RRAM_TSReadByteArray_S
****************************************************************************//**
*
* Reads an array of byte data from the specified address after validating the
* uncorrectable ECC fail bits.
* Returns ECC fail error if uncorrectable ECC fail error exist.
* This API should be used only for tearing safe use cases.
*
*******************************************************************************/
cy_en_rram_status_t Cy_RRAM_TSReadByteArray_S(RRAMC_Type * base, uint32_t addr, uint8_t * data, uint32_t numBytes)
{
    return Cy_RRAM_TSReadByteArray_internal(base, addr, data, numBytes, true);
}

/*******************************************************************************
* Function Name: Cy_RRAM_TSReadByteArray
****************************************************************************//**
*
* Reads an array of byte data from the specified address after validating the
* uncorrectable ECC fail bits.
* Returns ECC fail error if uncorrectable ECC fail error exist.
* This API should be used only for tearing safe use cases.
*
*******************************************************************************/
cy_en_rram_status_t Cy_RRAM_TSReadByteArray(RRAMC_Type * base, uint32_t addr, uint8_t * data, uint32_t numBytes)
{
    return Cy_RRAM_TSReadByteArray_internal(base, addr, data, numBytes, false);
}

#if (CY_CPU_CORTEX_M0P)
/*******************************************************************************
* Function Name : Cy_RRAM_Init
****************************************************************************//**
*
* Retrieves configuration settings from configuration space in Extra area.
*
*******************************************************************************/
#if (CY_IP_MXS22RRAMC_VERSION == 2u)
cy_en_rram_status_t Cy_RRAM_Init(RRAMC_Type * base, cy_en_rram_vmode_t vmode, uint8_t idx, bool retrvAlgoInfo)
{
    cy_en_rram_status_t status = CY_RRAM_SUCCESS;
    uint32_t timeout;

    if(NULL == base)
    {
        return CY_RRAM_BAD_PARAM;
    }

    /* Mask for Uncorrectable failure or no configuration retrieved errors. */
    uint32_t errorMask = _VAL2FLD(RRAMC_RRAM_SFR_NVM_STATUS_VERR, 0x04UL);

    /* Sets voltage/frequency mode */
    Cy_RRAM_SetVoltageMode(base, vmode);

    /* Starts "Retrieve configuration settings" sequence and waits until the sequence finishes. */
    Cy_RRAM_SetOperationMode(base, idx, CY_RRAM_IDLE);
    Cy_RRAM_SetOperationMode(base, idx, CY_RRAM_RETRIEVE_CONFIG);

    /* Retrieve the write algorithm information */
    RRAM_NVM_PROG(base, idx) |= _VAL2FLD(RRAMC_RRAM_SFR_NVM_PROG_SELRCS, retrvAlgoInfo);

    /* Start operation with unchanged NVM_PROG.ACTION */
    Cy_RRAM_StartOperation(base, idx);

    timeout = CY_RRAM_OPERATION_TIMEOUT / CY_RRAM_STATUS_CHECK_DELAY;

    /* Wait for the retrieve operation to finish. As per RRAM IP team,
    * there is no need for timeout as the state machine will definitely
    * return to non-busy state. As a safety measure this timeout is added */
    while(Cy_RRAM_IsBusy(base) && (0UL != timeout))
    {
        timeout--;
        Cy_SysLib_DelayCycles(CY_RRAM_STATUS_CHECK_DELAY);
    }

    if(timeout == 0UL)
    {
        return CY_RRAM_OPERATION_TIME_OUT_ERROR;
    }
    Cy_RRAM_StopOperation(base, idx);

    /* Up to triple-bit fails are corrected by HW
       below check is for uncorrectable failure or no configuration retrieved errors*/
    if((((uint32_t)CY_RRAM_GetFailBitInfo(base, idx)) & errorMask) != ((uint32_t)CY_RRAM_NO_ERROR))
    {
        status =  CY_RRAM_INIT_FAIL;
    }

    return status;
}
#else
cy_en_rram_status_t Cy_RRAM_Init(RRAMC_Type * base, cy_en_rram_vmode_t vmode, bool retrvAlgoInfo)
{
    cy_en_rram_status_t status = CY_RRAM_SUCCESS;
    uint32_t timeout;

    if(NULL == base)
    {
        return CY_RRAM_BAD_PARAM;
    }

    /* Mask for Uncorrectable failure or no configuration retrieved errors. */
    uint32_t errorMask = _VAL2FLD(RRAMC_RRAM_SFR_NVM_STATUS_VERR, 0x04UL);

    /* Sets voltage/frequency mode */
    Cy_RRAM_SetVoltageMode(base, vmode);

    /* Starts "Retrieve configuration settings" sequence and waits until the sequence finishes. */
    Cy_RRAM_SetOperationMode(base, CY_RRAM_IDLE);
    Cy_RRAM_SetOperationMode(base, CY_RRAM_RETRIEVE_CONFIG);

    /* Retrieve the write algorithm information */
    RRAM_NVM_PROG(base) |= _VAL2FLD(RRAMC_RRAM_SFR_NVM_PROG_SELRCS, retrvAlgoInfo);

    /* Start operation with unchanged NVM_PROG.ACTION */
    Cy_RRAM_StartOperation(base);

    timeout = CY_RRAM_OPERATION_TIMEOUT / CY_RRAM_STATUS_CHECK_DELAY;

    /* Wait for the retrieve operation to finish. As per RRAM IP team,
    * there is no need for timeout as the state machine will definitely
    * return to non-busy state. As a safety measure this timeout is added */
    while(Cy_RRAM_IsBusy(base) && (0UL != timeout))
    {
        timeout--;
        Cy_SysLib_DelayCycles(CY_RRAM_STATUS_CHECK_DELAY);
    }

    if(timeout == 0UL)
    {
        return CY_RRAM_OPERATION_TIME_OUT_ERROR;
    }
    Cy_RRAM_StopOperation(base);

    /* Up to triple-bit fails are corrected by HW
       below check is for uncorrectable failure or no configuration retrieved errors*/
    if((((uint32_t)CY_RRAM_GetFailBitInfo(base)) & errorMask) != ((uint32_t)CY_RRAM_NO_ERROR))
    {
        status =  CY_RRAM_INIT_FAIL;
    }

    return status;
}
#endif
#endif /* (CY_CPU_CORTEX_M0P) */


/*******************************************************************************
* Function Name: Cy_RRAM_EnableWP
****************************************************************************//**
*
* Enables write protection for number of sectors starting with sector 0 in RRAM NVM region.
* Before calling this API, user need to call Cy_RRAM_GetWPLockState() API and
* make sure the write protection lock is not locked. Otherwise this API will return
* CY_RRAM_WPLOCK_ENABLED error.
*
*******************************************************************************/
cy_en_rram_status_t Cy_RRAM_EnableWP(RRAMC_Type * base, uint32_t numSectors)
{
    cy_en_rram_status_t status = CY_RRAM_BAD_PARAM;

#if (CY_IP_MXS22RRAMC_VERSION == 2u)
    /* Calculate total sectors across all macros and regions */
    CY_MISRA_DEVIATE_LINE('MISRA C-2012 Rule 2.1', 'Intentional version-specific code block; reachable when CY_IP_MXS22RRAMC_VERSION == 2u.');
    uint32_t mainSectorsPerMacro = (CY_RRAM_MAIN_REGION_SIZE_INTERNAL) / (CY_RRAM_SECTOR_SIZE_KB * 1024UL);
    uint32_t workSectorsPerMacro = (CY_RRAM_WORK_REGION_SIZE_INTERNAL) / (CY_RRAM_SECTOR_SIZE_KB * 1024UL);
    uint32_t sflashSectorsPerMacro = (CY_RRAM_SFLASH_REGION_SIZE_INTERNAL) / (CY_RRAM_SECTOR_SIZE_KB * 1024UL);
    uint32_t protectedSectorsPerMacro = (CY_RRAM_PROTECTED_REGION_SIZE_INTERNAL) / (CY_RRAM_SECTOR_SIZE_KB * 1024UL);
    uint32_t totalMainSectors = mainSectorsPerMacro * CY_RRAMC_NUM_MACROS_MUL;
    uint32_t totalWorkSectors = workSectorsPerMacro * CY_RRAMC_NUM_MACROS_MUL;
    uint32_t totalSflashSectors = sflashSectorsPerMacro * CY_RRAMC_NUM_MACROS_MUL;
    uint32_t totalProtectedSectors = protectedSectorsPerMacro * CY_RRAMC_NUM_MACROS_MUL;
    uint32_t totalSectors = totalMainSectors + totalWorkSectors + totalSflashSectors + totalProtectedSectors;

    CY_ASSERT_L2(numSectors <= totalSectors);
#else
    CY_ASSERT_L2(CY_RRAM_SECTOR_COUNT_VALID(numSectors));
#endif

#if (CY_IP_MXS22RRAMC_VERSION == 2u)
    if(NULL != base && numSectors > 0UL && numSectors <= totalSectors)
#else
    if(NULL != base && numSectors > 0UL && numSectors <= (CY_RRAM_NVM_SIZE_KB / CY_RRAM_SECTOR_SIZE_KB))
#endif
    {
        /* Reset previous write protection errors by setting RSTECC bit in NVM_PROG register */
#if (CY_IP_MXS22RRAMC_VERSION == 2u)
        for(uint8_t idx = 0; idx < CY_RRAMC_NUM_MACROS_MUL; idx++)
        {
            RRAM_NVM_PROG(base, idx) |= RRAMC_RRAM_SFR_NVM_PROG_RSTECC_Msk;
        }
#else
        RRAM_NVM_PROG(base) |= RRAMC_RRAM_SFR_NVM_PROG_RSTECC_Msk;
#endif

        /* Check the status of write protection lock */
        CY_MISRA_DEVIATE_LINE('MISRA C-2012 Rule 14.3', 'WP lock state is a runtime-determined value; condition is not invariant across all configurations.');
        if(Cy_RRAM_GetWPLockState(base) == CY_RRAM_WP_UNLOCK)
        {
            uint32_t temp = 0;
#if (CY_IP_MXS22RRAMC_VERSION == 2u)
            uint32_t interruptState = Cy_SysLib_EnterCriticalSection();
            uint32_t remainingSectors = numSectors;

            /* Handle MAIN_NVM sectors sequentially across all macros */
            uint8_t idx = 0;
            while((idx < CY_RRAMC_NUM_MACROS_MUL) && (remainingSectors > 0UL))
            {
                uint32_t sectorsForThisMacro = 0;

                if(remainingSectors >= mainSectorsPerMacro)
                {
                    /* Protect all sectors in this MAIN macro */
                    sectorsForThisMacro = mainSectorsPerMacro;
                    remainingSectors -= mainSectorsPerMacro;
                }
                else
                {
                    /* Protect partial sectors in this MAIN macro */
                    sectorsForThisMacro = remainingSectors;
                    remainingSectors = 0UL;
                }

                temp = RRAM_NVM_CONF1(base, idx) & ~RRAMC_RRAM_SFR_NVM_CONF1_SECPROT_Msk;
                CY_MISRA_DEVIATE_LINE('MISRA C-2012 Rule 14.3', 'Loop body is reachable when CY_RRAMC_NUM_MACROS_MUL > 0; not dead code on multi-macro targets.');
                RRAM_NVM_CONF1(base, idx) = temp | _VAL2FLD(RRAMC_RRAM_SFR_NVM_CONF1_SECPROT, sectorsForThisMacro);
                idx++;
            }

            /* Handle WORK_NVM sectors sequentially across all macros if any sectors remain */
            idx = 0;
            while((idx < CY_RRAMC_NUM_MACROS_MUL) && (remainingSectors > 0UL))
            {
                uint32_t sectorsForThisMacro = 0;

                if(remainingSectors >= workSectorsPerMacro)
                {
                    /* Protect all sectors in this WORK macro */
                    sectorsForThisMacro = workSectorsPerMacro;
                    remainingSectors -= workSectorsPerMacro;
                }
                else
                {
                    /* Protect partial sectors in this WORK macro */
                    sectorsForThisMacro = remainingSectors;
                    remainingSectors = 0UL;
                }

                /* Add WORK sectors to existing protection in the same register */
                temp = RRAM_NVM_CONF1(base, idx) & ~RRAMC_RRAM_SFR_NVM_CONF1_SECPROT_Msk;
                uint32_t existingProtection = _FLD2VAL(RRAMC_RRAM_SFR_NVM_CONF1_SECPROT, RRAM_NVM_CONF1(base, idx));
                RRAM_NVM_CONF1(base, idx) = temp | _VAL2FLD(RRAMC_RRAM_SFR_NVM_CONF1_SECPROT, (existingProtection + sectorsForThisMacro));
                idx++;
            }

            /* Handle SFLASH_NVM sectors sequentially across all macros if any sectors remain */
            idx = 0;
            while((idx < CY_RRAMC_NUM_MACROS_MUL) && (remainingSectors > 0UL))
            {
                uint32_t sectorsForThisMacro = 0;

                if(remainingSectors >= sflashSectorsPerMacro)
                {
                    /* Protect all sectors in this SFLASH macro */
                    sectorsForThisMacro = sflashSectorsPerMacro;
                    remainingSectors -= sflashSectorsPerMacro;
                }
                else
                {
                    /* Protect partial sectors in this SFLASH macro */
                    sectorsForThisMacro = remainingSectors;
                    remainingSectors = 0UL;
                }

                /* Add SFLASH sectors to existing protection in the same register */
                temp = RRAM_NVM_CONF1(base, idx) & ~RRAMC_RRAM_SFR_NVM_CONF1_SECPROT_Msk;
                uint32_t existingProtection = _FLD2VAL(RRAMC_RRAM_SFR_NVM_CONF1_SECPROT, RRAM_NVM_CONF1(base, idx));
                RRAM_NVM_CONF1(base, idx) = temp | _VAL2FLD(RRAMC_RRAM_SFR_NVM_CONF1_SECPROT, (existingProtection + sectorsForThisMacro));
                idx++;
            }

            /* Handle PROTECTED_NVM sectors sequentially across all macros if any sectors remain */
            idx = 0;
            while((idx < CY_RRAMC_NUM_MACROS_MUL) && (remainingSectors > 0UL))
            {
                uint32_t sectorsForThisMacro = 0;

                if(remainingSectors >= protectedSectorsPerMacro)
                {
                    /* Protect all sectors in this PROTECTED macro */
                    sectorsForThisMacro = protectedSectorsPerMacro;
                    remainingSectors -= protectedSectorsPerMacro;
                }
                else
                {
                    /* Protect partial sectors in this PROTECTED macro */
                    sectorsForThisMacro = remainingSectors;
                    remainingSectors = 0UL;
                }

                /* Add PROTECTED sectors to existing protection in the same register */
                temp = RRAM_NVM_CONF1(base, idx) & ~RRAMC_RRAM_SFR_NVM_CONF1_SECPROT_Msk;
                uint32_t existingProtection = _FLD2VAL(RRAMC_RRAM_SFR_NVM_CONF1_SECPROT, RRAM_NVM_CONF1(base, idx));
                RRAM_NVM_CONF1(base, idx) = temp | _VAL2FLD(RRAMC_RRAM_SFR_NVM_CONF1_SECPROT, (existingProtection + sectorsForThisMacro));
                idx++;
            }

            Cy_SysLib_ExitCriticalSection(interruptState);
#else
            temp = RRAM_NVM_CONF1(base) & ~RRAMC_RRAM_SFR_NVM_CONF1_SECPROT_Msk;
            /* Erase and Update the number of sectors to be write protected */
            RRAM_NVM_CONF1(base) = temp | _VAL2FLD(RRAMC_RRAM_SFR_NVM_CONF1_SECPROT, numSectors);
#endif
            status = CY_RRAM_SUCCESS;
        }
        else
        {
            status = CY_RRAM_WPLOCK_ENABLED;
        }
    }

    return status;
}


/*******************************************************************************
* Function Name: Cy_RRAM_DisableWP
****************************************************************************//**
*
* Disables write protection for all sectors in RRAM NVM region.
* Before calling this API, user need to call Cy_RRAM_GetWPLockState() API and
* make sure the write protection lock is not locked. Otherwise this API will return
* CY_RRAM_WPLOCK_ENABLED error.
*
*******************************************************************************/
cy_en_rram_status_t Cy_RRAM_DisableWP(RRAMC_Type * base)
{
    cy_en_rram_status_t status = CY_RRAM_WPLOCK_ENABLED;

    if(NULL == base)
    {
        return CY_RRAM_BAD_PARAM;
    }

    /* Check the status of write protection lock */
    if(Cy_RRAM_GetWPLockState(base) == CY_RRAM_WP_UNLOCK)
    {

#if (CY_IP_MXS22RRAMC_VERSION == 2u)
        uint32_t interruptState = Cy_SysLib_EnterCriticalSection();
        for(uint8_t idx = 0; idx < CY_RRAMC_NUM_MACROS_MUL; idx++)
        {
            /* Disable write protection on all sectors by writing SECPROT to 0 */
            RRAM_NVM_CONF1(base, idx) &= ~RRAMC_RRAM_SFR_NVM_CONF1_SECPROT_Msk;
        }
        Cy_SysLib_ExitCriticalSection(interruptState);
#else
        /* Disable write protection on all sectors by writing SECPROT to 0 */
        RRAM_NVM_CONF1(base) &= ~RRAMC_RRAM_SFR_NVM_CONF1_SECPROT_Msk;
#endif


        status = CY_RRAM_SUCCESS;
    }

    return status;
}


/*******************************************************************************
* Function Name: Cy_RRAM_SetProtLockableRegion
****************************************************************************//**
*
* Sets the size of lockable region in PROTECTED_NVM region. If size is larger than
* PROTECTED_NVM, the entire PROTECTED_NVM is lockable. The lockable region can be configured
* only if the protected nvm lock state is unlocked. The configured region is protected only when the
* protected nvm lock state is locked
*
*******************************************************************************/
cy_en_rram_status_t Cy_RRAM_SetProtLockableRegion(RRAMC_Type * base, uint32_t regionSize)
{
    cy_en_rram_status_t status = CY_RRAM_BAD_PARAM;

    if(NULL != base && regionSize > 0UL && regionSize <= CY_RRAM_PROTECTED_LOCK_REGION_LIMIT)
    {
        if(Cy_RRAM_GetProtLockState(base) == CY_RRAM_PROTECTED_UNLOCK)
        {
            /* Align regionSize to update valid data to register */
            RRAM_PROTECTED_LOCKABLE_LOCK(base) |=
              (((regionSize - 1UL) << RRAMC_RRAMC_PROTECTED_LOCKABLE_LOCK_PARTITION_SIZE_1_Pos) | (regionSize - 1UL));
            status = CY_RRAM_SUCCESS;
        }
        else
        {
            status = CY_RRAM_UPDATE_LOCKABLE_REGION_FAILED;
        }
    }

    return status;
}

/*******************************************************************************
* Function Name: Cy_RRAM_GetNVMSubRegionInfo
****************************************************************************//**
*
* Get base address, size of the NVM sub region for the provided address.
*
*******************************************************************************/
cy_stc_rram_nvm_rgn_info_t Cy_RRAM_GetNVMSubRegionInfo(uint32_t addr)
{
    uint32_t secureBitState = addr & CY_RRAM_S_OFFSET_MASK;
    uint32_t addr_ns         = addr & ~CY_RRAM_S_OFFSET_MASK;
    cy_stc_rram_nvm_rgn_info_t rgnInfo= {CY_RRAM_INVALID_ACCESS, 0UL};

#if (CY_RRAM_RECLAIM_SIZE > 0UL)
    /* Check reclaim before MAIN because MAIN's visible end includes reclaim. */
    if((addr_ns >= CY_RRAM_MAIN_RECLAIM_NS_START_ADDRESS) && (addr_ns <= CY_RRAM_MAIN_RECLAIM_NS_END_ADDRESS))
    {
        rgnInfo.base = CY_RRAM_MAIN_RECLAIM_NS_START_ADDRESS;
        rgnInfo.size = CY_RRAM_RECLAIM_SIZE;
    }
    else
#endif
    if((addr_ns >= CY_RRAM_MAIN_HOST_NS_START_ADDRESS) && (addr_ns <= CY_RRAM_MAIN_HOST_NS_END_ADDRESS))
    {
        rgnInfo.base = CY_RRAM_MAIN_HOST_NS_START_ADDRESS;
        rgnInfo.size = CY_RRAM_MAIN_REGION_SIZE_INTERNAL;
    }
    else if((addr_ns >= CY_RRAM_WORK_HOST_NS_START_ADDRESS) && (addr_ns <= CY_RRAM_WORK_HOST_NS_END_ADDRESS))
    {
        rgnInfo.base = CY_RRAM_WORK_HOST_NS_START_ADDRESS;
        rgnInfo.size = CY_RRAM_WORK_REGION_SIZE_INTERNAL;
    }
    else if((addr_ns >= CY_RRAM_SFLASH_HOST_NS_START_ADDRESS) && (addr_ns <= CY_RRAM_SFLASH_HOST_NS_END_ADDRESS))
    {
        rgnInfo.base = CY_RRAM_SFLASH_HOST_NS_START_ADDRESS;
        rgnInfo.size = CY_RRAM_SFLASH_REGION_SIZE_INTERNAL;
    }
    else if((addr_ns >= CY_RRAM_PROTECTED_HOST_NS_START_ADDRESS) && (addr_ns <= CY_RRAM_PROTECTED_HOST_NS_END_ADDRESS))
    {
        rgnInfo.base = CY_RRAM_PROTECTED_HOST_NS_START_ADDRESS;
        rgnInfo.size = CY_RRAM_PROTECTED_REGION_SIZE_INTERNAL;
    }
#if (CY_CPU_CORTEX_M0P)
    else if((addr_ns >= CY_RRAM_PROTECTED_PROTECTED_NS_START_ADDRESS) && (addr_ns <= CY_RRAM_PROTECTED_PROTECTED_NS_END_ADDRESS))
    {
        rgnInfo.base = CY_RRAM_PROTECTED_PROTECTED_NS_START_ADDRESS;
        rgnInfo.size = CY_RRAM_PROTECTED_REGION_SIZE_INTERNAL;
    }
#endif
#if (CY_CPU_CORTEX_M33)
    else if((addr_ns >= CY_RRAM_MAIN_HOST_CBUS_NS_START_ADDRESS) && (addr_ns <= CY_RRAM_MAIN_HOST_CBUS_NS_END_ADDRESS))
    {
        rgnInfo.base = CY_RRAM_MAIN_HOST_CBUS_NS_START_ADDRESS;
        rgnInfo.size = CY_RRAM_MAIN_REGION_SIZE_INTERNAL;
    }
    else if((addr_ns >= CY_RRAM_PROTECTED_HOST_CBUS_NS_START_ADDRESS) && (addr_ns <= CY_RRAM_PROTECTED_HOST_CBUS_NS_END_ADDRESS))
    {
        rgnInfo.base = CY_RRAM_PROTECTED_HOST_CBUS_NS_START_ADDRESS;
        rgnInfo.size = CY_RRAM_PROTECTED_REGION_SIZE_INTERNAL;
    }
#endif
    else
    {
        secureBitState = 0UL;
    }
    rgnInfo.base |= secureBitState;
#if (CY_RRAM_RECLAIM_SIZE > 0UL)
    /* CY_RRAM_RECLAIM_SIZE is already the total across macros. */
    if ((rgnInfo.base & ~CY_RRAM_S_OFFSET_MASK) != CY_RRAM_MAIN_RECLAIM_NS_START_ADDRESS)
#endif
    {
        rgnInfo.size *= CY_RRAMC_NUM_MACROS_MUL;
    }
#if (CY_RRAM_RECLAIM_SIZE > 0UL)
    /* Reclaimed band is exposed as a tail extension of MAIN, so grow the
     * reported size accordingly when the caller asked about MAIN. Conversely,
     * PROTECTED loses its first RECLAIM_SIZE bytes to reclaim, so report the
     * visible (shortened) size. */
    {
        uint32_t baseNs = rgnInfo.base & ~CY_RRAM_S_OFFSET_MASK;
        bool isMainRgn = (baseNs == CY_RRAM_MAIN_HOST_NS_START_ADDRESS);
        bool isProtRgn = (baseNs == CY_RRAM_PROTECTED_HOST_NS_START_ADDRESS);
#if (CY_CPU_CORTEX_M33)
        isMainRgn = isMainRgn || (baseNs == CY_RRAM_MAIN_HOST_CBUS_NS_START_ADDRESS);
        isProtRgn = isProtRgn || (baseNs == CY_RRAM_PROTECTED_HOST_CBUS_NS_START_ADDRESS);
#endif
        if (isMainRgn)
        {
            rgnInfo.size += CY_RRAM_RECLAIM_SIZE;
        }
        else if (isProtRgn)
        {
            rgnInfo.size -= CY_RRAM_RECLAIM_SIZE;
        }
        else
        {
            /* other subregions unaffected */
        }
    }
#endif

    return rgnInfo;
}

CY_MISRA_BLOCK_END('MISRA C-2012 Rule 14.3')

CY_MISRA_BLOCK_END('MISRA C-2012 Rule 10.8')

#endif /* CY_IP_MXS22RRAMC */

/* [] END OF FILE */
