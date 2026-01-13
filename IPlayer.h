#ifndef _IPLAYER_INCLUDED
#define _IPLAYER_INCLUDED

#include <windows.h>
#include <vector>
#include "soundDefs.h"

class CVidsendView;
class CVidsendView22;

// Abstract audio player
class IPlayer {

public:

	class SoundEventListener {
	public:
		
		virtual ~SoundEventListener() {};

		virtual void OnSoundPlayerNotify(int eventNumber)=0;	
		};
	
public:
	
	IPlayer() {}; 
	virtual ~IPlayer() {}; 
	
	virtual void Init(LPGUID q=NULL)=0;
	virtual void Play(DWORD p_flag)=0;
	virtual void Stop()=0;
	virtual bool CreateSoundBuffer(SOUNDFORMAT format, long bufferLength, DWORD flag)=0;
	virtual void Write(long start, const BYTE *data, long size)=0; 
	virtual void SetPosition(long pos)=0;

	virtual bool CreateEventReadNotification(std::vector<DWORD>&p_event)=0;	
	virtual void SetSoundEventListener(/*IPlayer::SoundEventListener*/ CVidsendView *p_listener)=0;	
	virtual void SetSoundEventListener(/*IPlayer::SoundEventListener*/ CVidsendView22 *p_listener)=0;	
	virtual void RemoveSoundEventListener()=0;	
    
	};


#endif
