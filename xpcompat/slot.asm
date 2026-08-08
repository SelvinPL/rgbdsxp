; SPDX-License-Identifier: MIT

.686
.model flat

EXTERN _MyInitializeCriticalSectionEx@12	: PROC
EXTERN _MyFlsAlloc@4						: PROC
EXTERN _MyFlsFree@4							: PROC
EXTERN _MyFlsGetValue@4						: PROC
EXTERN _MyFlsSetValue@8						: PROC
EXTERN _MyIsThreadAFiber@0					: PROC
EXTERN _MyAcquireSRWLockExclusive@4			: PROC
EXTERN _MyReleaseSRWLockExclusive@4			: PROC
EXTERN _MySleepConditionVariableSRW@16		: PROC
EXTERN _MyWakeAllConditionVariable@4		: PROC
EXTERN _MyWakeConditionVariable@4			: PROC
EXTERN _MyLCMapStringEx@36					: PROC

.data

PUBLIC __imp__InitializeCriticalSectionEx@12
PUBLIC __imp__FlsAlloc@4
PUBLIC __imp__FlsFree@4
PUBLIC __imp__FlsGetValue@4
PUBLIC __imp__FlsSetValue@8
PUBLIC __imp__IsThreadAFiber@0
PUBLIC __imp__AcquireSRWLockExclusive@4
PUBLIC __imp__ReleaseSRWLockExclusive@4
PUBLIC __imp__SleepConditionVariableSRW@16
PUBLIC __imp__WakeAllConditionVariable@4
PUBLIC __imp__WakeConditionVariable@4
PUBLIC __imp__LCMapStringEx@36

__imp__InitializeCriticalSectionEx@12	dd _MyInitializeCriticalSectionEx@12
__imp__FlsAlloc@4						dd _MyFlsAlloc@4
__imp__FlsFree@4						dd _MyFlsFree@4
__imp__FlsGetValue@4					dd _MyFlsGetValue@4
__imp__FlsSetValue@8					dd _MyFlsSetValue@8
__imp__IsThreadAFiber@0					dd _MyIsThreadAFiber@0
__imp__AcquireSRWLockExclusive@4		dd _MyAcquireSRWLockExclusive@4
__imp__ReleaseSRWLockExclusive@4		dd _MyReleaseSRWLockExclusive@4
__imp__SleepConditionVariableSRW@16		dd _MySleepConditionVariableSRW@16
__imp__WakeAllConditionVariable@4		dd _MyWakeAllConditionVariable@4
__imp__WakeConditionVariable@4			dd _MyWakeConditionVariable@4
__imp__LCMapStringEx@36					dd _MyLCMapStringEx@36

END
