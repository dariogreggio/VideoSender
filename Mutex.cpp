#include "stdafx.h"
#include "Mutex.h"


Mutex::Mutex() {

	m_hMutex = CreateMutex(NULL, 0, NULL);
	}

Mutex::~Mutex() {	

	CloseHandle(m_hMutex);
	m_hMutex = NULL;
	}

void Mutex::Get() const {

	WaitForSingleObject(m_hMutex, INFINITE);
	}

void Mutex::Release() const {

	ReleaseMutex(m_hMutex);	
	}


