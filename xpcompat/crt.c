// SPDX-License-Identifier: MIT
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#ifndef FLS_OUT_OF_INDEXES
#define FLS_OUT_OF_INDEXES ((DWORD)0xFFFFFFFF)
#endif

typedef VOID(WINAPI *FLS_CB)(PVOID);

typedef DWORD(WINAPI *FlsAlloc_t)(FLS_CB);
typedef BOOL(WINAPI *FlsFree_t)(DWORD);
typedef PVOID(WINAPI *FlsGetValue_t)(DWORD);
typedef BOOL(WINAPI *FlsSetValue_t)(DWORD, PVOID);
typedef BOOL(WINAPI* IsThreadAFiber_t)(void);
typedef BOOL(WINAPI *InitializeCriticalSectionEx_t)(LPCRITICAL_SECTION, DWORD, DWORD);
typedef VOID(WINAPI *AcqSRW_t)(PSRWLOCK);
typedef VOID(WINAPI *RelSRW_t)(PSRWLOCK);
typedef BOOL(WINAPI *SleepCV_t)(PCONDITION_VARIABLE, PSRWLOCK, DWORD, ULONG);
typedef VOID(WINAPI *WakeCV_t)(PCONDITION_VARIABLE);
typedef int(WINAPI *LCMapEx_t)(LPCWSTR, DWORD, LPCWSTR, int, LPWSTR, int, LPNLSVERSIONINFO, LPVOID, LPARAM);

static FlsAlloc_t r_alloc;
static FlsFree_t r_free;
static FlsGetValue_t r_get;
static FlsSetValue_t r_set;
static IsThreadAFiber_t r_istaf;
static InitializeCriticalSectionEx_t r_critex;
static AcqSRW_t r_acq;
static RelSRW_t r_rel;
static SleepCV_t r_sleepcv;
static WakeCV_t r_wakeall;
static WakeCV_t r_wakeone;
static LCMapEx_t r_lcmapex;
static int r_probed;

static void probe(void) {
	if (!r_probed) {
		HMODULE k = GetModuleHandleW(L"kernel32.dll");
		if (k) {
			r_alloc = (FlsAlloc_t)GetProcAddress(k, "FlsAlloc");
			r_free = (FlsFree_t)GetProcAddress(k, "FlsFree");
			r_get = (FlsGetValue_t)GetProcAddress(k, "FlsGetValue");
			r_set = (FlsSetValue_t)GetProcAddress(k, "FlsSetValue");
			r_istaf = (IsThreadAFiber_t)GetProcAddress(k, "IsThreadAFiber");
			r_critex = (InitializeCriticalSectionEx_t)GetProcAddress(k, "InitializeCriticalSectionEx");
			r_acq = (AcqSRW_t)GetProcAddress(k, "AcquireSRWLockExclusive");
			r_rel = (RelSRW_t)GetProcAddress(k, "ReleaseSRWLockExclusive");
			r_sleepcv = (SleepCV_t)GetProcAddress(k, "SleepConditionVariableSRW");
			r_wakeall = (WakeCV_t)GetProcAddress(k, "WakeAllConditionVariable");
			r_wakeone = (WakeCV_t)GetProcAddress(k, "WakeConditionVariable");
			r_lcmapex = (LCMapEx_t)GetProcAddress(k, "LCMapStringEx");
		}
		r_probed = 1;
	}
}

BOOL WINAPI MyInitializeCriticalSectionEx(LPCRITICAL_SECTION p, DWORD spin, DWORD flags) {
	probe();
	if (r_critex)
		return r_critex(p, spin, flags);
	(void)flags;
	return InitializeCriticalSectionAndSpinCount(p, spin);
}

#define FLS_SLOTS 128
typedef struct {
	FLS_CB cb;
	DWORD  tls;
	BOOL   used;
} fls_slot;

static fls_slot s_slots[FLS_SLOTS];

static volatile LONG s_lock;

static void fls_lock(void) {
	while (InterlockedCompareExchange(&s_lock, 1, 0) != 0)
		Sleep(0);
}

static void fls_unlock(void) {
	InterlockedExchange(&s_lock, 0);
}

void MyFlsThreadCleanup(void) {
	for (int i = 0; i < FLS_SLOTS; ++i) {
		fls_lock();
		fls_slot s = s_slots[i];
		fls_unlock();
		if (s.used && s.cb) {
			PVOID v = TlsGetValue(s.tls);
			if (v) {
				s.cb(v);
				TlsSetValue(s.tls, NULL);
			}
		}
	}
}

DWORD WINAPI MyFlsAlloc(FLS_CB cb) {
	probe();
	if (r_alloc)
		return r_alloc(cb);
	fls_lock();
	for (int i = 0; i < FLS_SLOTS; ++i) {
		fls_slot* s = &s_slots[i];
		if (!s->used) {
			DWORD t = TlsAlloc();
			if (t == TLS_OUT_OF_INDEXES)
				break;
			s->used = TRUE;
			s->cb = cb;
			s->tls = t;
			fls_unlock();
			return (DWORD)i;
		}
	}
	fls_unlock();
	return FLS_OUT_OF_INDEXES;
}

BOOL WINAPI MyFlsSetValue(DWORD idx, PVOID val) {
	probe();
	if (r_set)
		return r_set(idx, val);
	if (idx >= FLS_SLOTS || !s_slots[idx].used) {
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}
	return TlsSetValue(s_slots[idx].tls, val);
}

PVOID WINAPI MyFlsGetValue(DWORD idx) {
	probe();
	if (r_get)
		return r_get(idx);
	if (idx >= FLS_SLOTS || !s_slots[idx].used) {
		SetLastError(ERROR_INVALID_PARAMETER);
		return NULL;
	}
	return TlsGetValue(s_slots[idx].tls);
}

BOOL WINAPI MyFlsFree(DWORD idx) {
	probe();
	if (r_free)
		return r_free(idx);
	if (idx >= FLS_SLOTS) {
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}
	fls_lock();
	fls_slot* s = &s_slots[idx];
	if (s->used) {
		FLS_CB cb = s->cb;
		DWORD  tls = s->tls;
		s->used = FALSE;
		s->cb = NULL;
		s->tls = 0;
		fls_unlock();
		if (cb) {
			PVOID v = TlsGetValue(tls);
			if (v)
				cb(v);
		}
		TlsFree(tls);
	} else {
		fls_unlock();
	}
	return TRUE;
}

BOOL WINAPI MyIsThreadAFiber() {
	probe();
	if (r_istaf) {
		return r_istaf();
	}
	return FALSE;
}

static CRITICAL_SECTION *srw_cs(PSRWLOCK l) {
	CRITICAL_SECTION *cs = (CRITICAL_SECTION *)*(void **)l;
	if (!cs) {
		CRITICAL_SECTION *n = (CRITICAL_SECTION *)HeapAlloc(GetProcessHeap(), 0, sizeof(*n));
		InitializeCriticalSection(n);
		void *prev = InterlockedCompareExchangePointer((void **)l, n, NULL);
		if (prev) { // lost the init race: keep the winner, drop ours
			DeleteCriticalSection(n);
			HeapFree(GetProcessHeap(), 0, n);
			cs = (CRITICAL_SECTION *)prev;
		} else {
			cs = n;
		}
	}
	return cs;
}

VOID WINAPI MyAcquireSRWLockExclusive(PSRWLOCK l) {
	probe();
	if (r_acq) {
		r_acq(l);
		return;
	}
	EnterCriticalSection(srw_cs(l));
}

VOID WINAPI MyReleaseSRWLockExclusive(PSRWLOCK l) {
	probe();
	if (r_rel) {
		r_rel(l);
		return;
	}
	LeaveCriticalSection(srw_cs(l));
}

typedef struct {
	HANDLE       sem;
	volatile LONG waiters;
} cv_impl;

static cv_impl *cv_get(PCONDITION_VARIABLE c) {
	cv_impl *p = (cv_impl *)*(void **)c;
	if (!p) {
		cv_impl *n = (cv_impl *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*n));
		n->sem = CreateSemaphoreW(NULL, 0, 0x7fffffff, NULL);
		void *prev = InterlockedCompareExchangePointer((void **)c, n, NULL);
		if (prev) {
			CloseHandle(n->sem);
			HeapFree(GetProcessHeap(), 0, n);
			p = (cv_impl *)prev;
		} else {
			p = n;
		}
	}
	return p;
}

BOOL WINAPI MySleepConditionVariableSRW(PCONDITION_VARIABLE c, PSRWLOCK l, DWORD ms, ULONG flags) {
	probe();
	if (r_sleepcv)
		return r_sleepcv(c, l, ms, flags);
	(void)flags;
	cv_impl *cv = cv_get(c);
	InterlockedIncrement(&cv->waiters);
	MyReleaseSRWLockExclusive(l);
	DWORD w = WaitForSingleObject(cv->sem, ms);
	MyAcquireSRWLockExclusive(l);
	if (w == WAIT_TIMEOUT) {
		InterlockedDecrement(&cv->waiters);
		SetLastError(ERROR_TIMEOUT);
		return FALSE;
	}
	return TRUE;
}

VOID WINAPI MyWakeAllConditionVariable(PCONDITION_VARIABLE c) {
	probe();
	if (r_wakeall) {
		r_wakeall(c);
		return;
	}
	cv_impl *cv = cv_get(c);
	LONG n = InterlockedExchange(&cv->waiters, 0);
	if (n > 0)
		ReleaseSemaphore(cv->sem, n, NULL);
}

VOID WINAPI MyWakeConditionVariable(PCONDITION_VARIABLE c) {
	probe();
	if (r_wakeone) {
		r_wakeone(c);
		return;
	}
	cv_impl *cv = cv_get(c);
	LONG cur = cv->waiters;
	while (cur > 0) {
		LONG prev = InterlockedCompareExchange(&cv->waiters, cur - 1, cur);
		if (prev == cur) {
			ReleaseSemaphore(cv->sem, 1, NULL);
			break;
		}
		cur = prev;
	}
}

int WINAPI MyLCMapStringEx(LPCWSTR locale, DWORD flags, LPCWSTR src, int cchSrc,
		LPWSTR dst, int cchDst, LPNLSVERSIONINFO ver,
		LPVOID reserved, LPARAM sortHandle) {
	probe();
	if (r_lcmapex)
		return r_lcmapex(locale, flags, src, cchSrc, dst, cchDst, ver, reserved, sortHandle);
	(void)ver;
	(void)reserved;
	(void)sortHandle;
	LCID lcid = (!locale || !locale[0]) ? LOCALE_INVARIANT : LOCALE_USER_DEFAULT;
	return LCMapStringW(lcid, flags, src, cchSrc, dst, cchDst);
}

static void NTAPI fls_tls_cb(PVOID h, DWORD reason, PVOID r) {
	(void)h;
	(void)r;
	switch (reason) {
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		MyFlsThreadCleanup();
		break;
	}
}

#ifdef _WIN64

#pragma comment(linker, "/INCLUDE:_tls_used")
#pragma comment(linker, "/INCLUDE:p_fls_tls_cb")
#pragma const_seg(".CRT$XLB")
EXTERN_C const PIMAGE_TLS_CALLBACK p_fls_tls_cb = fls_tls_cb;
#pragma const_seg()

void *__imp_InitializeCriticalSectionEx = (void *)&MyInitializeCriticalSectionEx;
void *__imp_FlsAlloc = (void *)&MyFlsAlloc;
void *__imp_FlsFree = (void *)&MyFlsFree;
void *__imp_FlsGetValue = (void *)&MyFlsGetValue;
void *__imp_FlsSetValue = (void *)&MyFlsSetValue;
void* __imp_IsThreadAFiber = (void*)MyIsThreadAFiber;
void *__imp_AcquireSRWLockExclusive = (void *)&MyAcquireSRWLockExclusive;
void *__imp_ReleaseSRWLockExclusive = (void *)&MyReleaseSRWLockExclusive;
void *__imp_SleepConditionVariableSRW = (void *)&MySleepConditionVariableSRW;
void *__imp_WakeAllConditionVariable = (void *)&MyWakeAllConditionVariable;
void *__imp_WakeConditionVariable = (void *)&MyWakeConditionVariable;
void *__imp_LCMapStringEx = (void *)&MyLCMapStringEx;

#else

#pragma comment(linker, "/INCLUDE:__tls_used")
#pragma comment(linker, "/INCLUDE:_p_fls_tls_cb")
#pragma data_seg(".CRT$XLB")
EXTERN_C PIMAGE_TLS_CALLBACK p_fls_tls_cb = fls_tls_cb;
#pragma data_seg()

#endif
