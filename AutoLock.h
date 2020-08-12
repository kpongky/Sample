#pragma once

class ILock
{
public:
	virtual void Enter() = 0;
	virtual void Leave() = 0;
	virtual const bool TryEnter() = 0;
};

class SpinLock : public ILock
{
public:
	SpinLock();
	void Enter() override;
	void Leave() override;
	const bool TryEnter() override;
private:
	uint32_t m_ThreadId;
};

class CsLock : public ILock
{
public:
	CsLock();
	~CsLock();
	void Enter() override;
	void Leave() override;
	const bool TryEnter() override;
private:
  CRITICAL_SECTION _criticalSection;
};

class AutoLock 
{
public:
  AutoLock(ILock& lock);
  ~AutoLock();
private:
  ILock& m_Lock;
};