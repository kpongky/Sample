#pragma once
class Thread
{
public:
	Thread();
	~Thread();
	void Initialize(std::function<void (Thread&)> task);
	void Finalize();
	void Start();
    void Stop();
    void Pause();
    void Resume();
	const bool IsTerminated() const;
private:
    static uint32_t __stdcall ThreadFunction(void* parameter);

	enum ThreadStatus
	{
		TS_WaitInitialize,
		TS_WaitStart,
		TS_Working,
		TS_Pause,
		TS_WaitFinalize,
		TS_Finalized,
	} m_ThreadStatus;
	bool m_IsStart;
	bool m_IsStop;
    int32_t m_PauseCount;
    HANDLE m_ThreadHandle;
	std::function<void (Thread&)> m_Task;
};