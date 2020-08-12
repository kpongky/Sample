#include "StdAfx.h"
#include "Thread.h"
    
Thread::Thread()
	: m_ThreadHandle(nullptr)
    , m_ThreadStatus(TS_WaitInitialize)
	, m_Task(nullptr)
	, m_PauseCount(0)
	, m_IsStart(false)
	, m_IsStop(false)
{
}

void Thread::Initialize(std::function<void (Thread&)> task)
{
	if (m_ThreadStatus != TS_WaitInitialize)
	{
		assert(false);
		return;
	}
	m_Task = task;
	m_ThreadHandle = (HANDLE)::_beginthreadex(nullptr, 0, Thread::ThreadFunction, this, 0, nullptr);
}

void Thread::Finalize()
{
	if (TS_WaitFinalize == m_ThreadStatus)
	{
		::CloseHandle(m_ThreadHandle);	
		m_ThreadStatus = TS_Finalized;
	}
	else
	{
		assert(false);
	}
}

Thread::~Thread()
{
	while(m_ThreadStatus != TS_WaitInitialize && m_ThreadStatus < TS_WaitFinalize)
	{
		Sleep(0);
	}
	if (m_ThreadStatus == TS_WaitFinalize)
	{
		Finalize();
	}
}

void Thread::Start() 
{
	m_IsStart = true;
}
      
void Thread::Stop() 
{
	m_IsStop = true;
}

void Thread::Pause() 
{
    InterlockedIncrement(reinterpret_cast<volatile long*>(&m_PauseCount));
}

void Thread::Resume() 
{
    if(InterlockedDecrement(reinterpret_cast<volatile long*>(&m_PauseCount)) < 0) 
	{
		InterlockedIncrement(reinterpret_cast<volatile long*>(&m_PauseCount));
    }
}

const bool Thread::IsTerminated() const 
{
    return m_ThreadStatus == TS_WaitFinalize;
}
 
unsigned __stdcall Thread::ThreadFunction(void* parameter)
{
    Thread& thread = *static_cast<Thread*>(parameter);
	thread.m_ThreadStatus = TS_WaitStart;
	while(thread.m_IsStart == false)
	{
		Sleep(0);
	}

	while(false == thread.m_IsStop)
	{
		if (0 < thread.m_PauseCount)
		{
			thread.m_ThreadStatus = TS_Pause;
			Sleep(0);
			continue;
		}
		else
		{
			thread.m_ThreadStatus = TS_Working;
		}
		thread.m_Task(thread);
	}	
	thread.m_ThreadStatus = TS_WaitFinalize;
    return 0;
}