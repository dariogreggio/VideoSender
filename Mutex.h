#ifndef _MUTEX_INCLUDED
#define _MUTEX_INCLUDED

#include <windows.h>


class Mutex {
public:
	HANDLE m_hMutex;

public:
	Mutex();
	~Mutex();

	void Get() const;
	void Release() const;

	};

#endif
