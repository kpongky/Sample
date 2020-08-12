#pragma once
#include "AutoLock.h"

template< typename T >
struct SingletonCreationPolicy 
{
	static T* Create() 
	{
		return new T();
	}
 };
template< typename T >
struct SingletonDestroyPolicy 
{
	static void Destroy(T* p) 
	{
		delete p;
	}
 };
template<typename T, template < typename > class CreationPolicy = SingletonCreationPolicy
					,  template < typename > class DestroyPolicy = SingletonDestroyPolicy>
class Singleton
{
protected:
	Singleton() {}
public:
	static T& GetSingleton() 
	{
		if (nullptr == m_Instance) 
		{
			AutoLock lock(m_Lock);
			if (nullptr == m_Instance) 
			{
				m_Instance = CreationPolicy<T>::Create();				
				atexit(Release);
			}
		}
		return *m_Instance;
	}

	static void Release() 
	{
		if (m_Instance != nullptr) 
		{
			AutoLock lock(m_Lock);
			if (m_Instance != nullptr) 
			{
				DestroyPolicy<T>::Destroy(m_Instance);
				m_Instance = nullptr;
			}
		}
	}
private:
  static T* m_Instance;
  static CsLock m_Lock;
};

template<typename T, template<typename> class CreationPolicy, template<typename> class DestroyPolicy>
T* Singleton<T, CreationPolicy, DestroyPolicy>::m_Instance = nullptr;

template<typename T, template<typename> class CreationPolicy, template<typename> class DestroyPolicy>
CsLock Singleton<T, CreationPolicy, DestroyPolicy>::m_Lock;