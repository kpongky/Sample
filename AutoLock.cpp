#include "StdAfx.h"
#include "AutoLock.h"

SpinLock::SpinLock() : m_ThreadId(0)
{
}

void SpinLock::Enter()
{
	while(!TryEnter())
	{		
	}
}

void SpinLock::Leave()
{
	m_ThreadId = 0;
}

const bool SpinLock::TryEnter()
{
	const uint32_t oldThreadId = m_ThreadId;
	const uint32_t currentThreadId = GetCurrentThreadId();
	if (0 == oldThreadId)
	{
		if (oldThreadId == ::InterlockedCompareExchange(&m_ThreadId, currentThreadId, oldThreadId))
		{
			return true;
		}
	}
	return false;
};



CsLock::CsLock() 
{
	InitializeCriticalSection(&_criticalSection);
}

CsLock::~CsLock() 
{
	DeleteCriticalSection(&_criticalSection);
}

void CsLock::Enter() 
{
	 EnterCriticalSection(&_criticalSection);
}

void CsLock::Leave() 
{
	LeaveCriticalSection(&_criticalSection);
}

 const bool CsLock::TryEnter() 
{
	return TryEnterCriticalSection(&_criticalSection) == TRUE;
}

 AutoLock::AutoLock(ILock& lock): m_Lock(lock)
 {
	 m_Lock.Enter();
 }
 AutoLock::~AutoLock()
 {
	 m_Lock.Leave();
 }