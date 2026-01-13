#ifndef _SCOPEGUARDMUTEX_INCLUDED
#define _SCOPEGUARDMUTEX_INCLUDED

#include <windows.h>

/********************************************
  
  Release the mutex when the object is destroyed.  

  Guaranty that the mutex will be released...

********************************************/
class ScopeGuardMutex {
	class Mutex *m_pMutex;

public:
	ScopeGuardMutex(Mutex *pMutex);
	~ScopeGuardMutex();
};


//https://forums.codeguru.com/showthread.php?387675-CSingleLock-question
class lock_base {
public:
  lock_base() {}
  virtual ~lock_base() {}
  virtual void acquire(unsigned long timeout = 0xffffffff) = 0;
  virtual void release() = 0;
};

class critical_section_sync : public lock_base {
public:
  critical_section_sync() { ::InitializeCriticalSection(&critical_section_); }
  ~critical_section_sync() { ::DeleteCriticalSection(&critical_section_); }
  void acquire(unsigned long) { ::EnterCriticalSection(&critical_section_); }
  void release() { ::LeaveCriticalSection(&critical_section_); }

private:
  CRITICAL_SECTION critical_section_;
};

class lock_guard {
public:
  lock_guard(lock_base& sync) : sync_(sync) { lock(); }
  ~lock_guard() { unlock(); }

private:
  lock_base& sync_;

  lock_guard(const lock_guard&);
  lock_guard& operator=(const lock_guard&);
  void lock() { sync_.acquire(); }
  void unlock() { sync_.release(); }
};

//https://stackoverflow.com/questions/19408896/csinglelock-implementation-works-ok-on-windows-7-but-ends-in-a-deadlock-on-win-x
class CCriticalSection {
private:
    CRITICAL_SECTION m_cs; 
public:
    CCriticalSection()    {
        InitializeCriticalSection(&m_cs);
    }
    ~CCriticalSection()    {
        DeleteCriticalSection(&m_cs);
    }
    void Lock()    {   
        EnterCriticalSection(&m_cs);
    }
//    BOOL TryLock()    {
//  non c'è diofa       return TryEnterCriticalSection(&m_cs);
//    }
    void Unlock()    {
         if(m_cs.LockCount > -1)
              LeaveCriticalSection(&m_cs);
    }
};
// quella di Windows non la trova... chissà dove sono afxmt.h ecc...
class CSingleLock {
private:
    CCriticalSection *m_cs;
    bool m_bLock;

public:
    CSingleLock(CCriticalSection* cs = NULL, bool bLock = false)    {
        m_cs = cs;
        if(m_cs)        {
            if(bLock)
                m_cs->Lock();
            m_bLock = bLock;
        }
    }
    void Unlock()    {
        if(!m_cs || !m_bLock)
            return;
        m_cs->Unlock();
        m_bLock = false;
    }
    void Lock()    {
        if(!m_cs || m_bLock)
            return;
        m_cs->Lock();
        m_bLock = true;
    }
    ~CSingleLock()    {
        Unlock();
    }
};
#endif
