// Player.cpp : Implementation of Player class
//

#include "stdafx.h"
#include "vidsend.h"
#include "vidsendView.h"
#include "vidsendDoc.h"
//#include "MATException.h"
#include "JoshuaMP3.h"
#include "Player.h"
#include "DirectSoundErr.h"
#include "Mutex.h"
#include "ScopeGuardMutex.h"
#include "DigitalText.h"
#include "DbgAssert.h"
#include "ToolMisc.h"
#include "math.h"
#include <vector>



Player::Player(HWND p_hWnd) : m_hWnd(p_hWnd) {

	// Reset the sound buffer
	m_lpDSBuffer = NULL;
	m_lpDS = NULL;

	m_nbReadEvent = 0;
	m_pReadEvent = NULL;
	m_lpdsNotify = NULL;
	m_pDSNotify = NULL;
	m_hNotifyEndThread = NULL;
	objectListener=NULL;

	m_mutex = new Mutex();
	}

Player::~Player() {

	if(m_hNotifyEndThread) {
		// set the first handle to terminate the waitNotif thread
		SetEvent(m_pReadEvent[m_nbReadEvent]);
		WaitForSingleObject(m_hNotifyEndThread, INFINITE);
		CloseHandle(m_hNotifyEndThread);
		
		// delete all remaining handles<
		for(int i=0; i <=m_nbReadEvent; ++i)
			CloseHandle(m_pReadEvent[i]);			

		delete []m_pReadEvent;
		}


	//Free up the buffer
	if(m_lpdsNotify)
		m_lpdsNotify->Release();
	
	if(m_lpDSBuffer)		
		m_lpDSBuffer->Release();
		
	if(m_lpDS)
		m_lpDS->Release();

	if(m_pDSNotify)
		delete[] m_pDSNotify;


	delete m_mutex;
	}


/*****
* Player::Init() : This function is the first one to be called after the Player instance.
*					It creates the DirectSound object which will be use after by the player.
*
*/
struct DIRECTSOUND_DESCR {
	int num;
	LPGUID guid;
	char descr[128];
	};
BOOL CALLBACK Player::DSEnumCallback(
         LPGUID lpGuid,
         LPCSTR lpcstrDescription,
         LPCSTR lpcstrModule,
         LPVOID lpContext) {

	struct DIRECTSOUND_DESCR *dsd=(struct DIRECTSOUND_DESCR *)lpContext;
	dsd->guid=lpGuid;
	_tcsncpy(dsd->descr,lpcstrDescription,127);
	dsd->descr[127]=0;

	return dsd->num-- ? 1 : 0;
	}

BYTE Player::Init(int q)	{
	struct DIRECTSOUND_DESCR dsd;

	dsd.num=q;
	dsd.guid=0;

	DirectSoundEnumerate(DSEnumCallback,&dsd);
  //			 per usare più periferiche (preascolto cuffia ecc)
		
	if(dsd.num<0) {
		Init(dsd.guid);
		return 1;
		}
	else
		return 0;
	}

void Player::Init(LPGUID q)	{
	char buffer[32];

	// Create DirectSound Object
	HRESULT hres = DirectSoundCreate(q, &m_lpDS, NULL);

	if(FAILED(hres))
		EXCEP(DirectSoundErr::GetErrDesc(hres), _T("Player::CreateDS DirectSoundCreate"));

	// Set Cooperative Level
	hres = m_lpDS->SetCooperativeLevel(m_hWnd, DSSCL_NORMAL /*DSSCL_EXCLUSIVE | */ /*DSSCL_PRIORITY*/);
	
	if(FAILED(hres))
		EXCEP(DirectSoundErr::GetErrDesc(hres), _T("Player::CreateDS SetCooperativeLevel"));
	}


/*****
* Player::SetHWnd :	This function must be called before calling Init() 
*					Use to set the Window handling used in the Init function.
*
*/
void Player::SetHWnd(HWND p_hWnd)	{
	m_hWnd = p_hWnd;
	}

/*****
*	Player::CreateSoundBuffer : This function creates the directSoundBuffer which will
*								eventually be read. We have to set the buffer desc 
*								(DSBUFFERDESC) and then, call CreateSoundBuffer.
*
*	@format :		The format of the data that will be read.
*	@bufferLength : The size of the buffer you will read.
*	@flags	:		specifying the capabilities to include when creating a 
*					new DirectSoundBuffer object
*
*/
bool Player::CreateSoundBuffer(SOUNDFORMAT format, long bufferLength, DWORD flags)	{
	WAVEFORMATEX wfx;
	DSBUFFERDESC dsbdesc;

	if(m_lpDSBuffer)		
		m_lpDSBuffer->Release();

	// Set up wave format structure.
	ZeroMemory(&wfx, sizeof(WAVEFORMATEX));
	format.toWFX(wfx);	

	// Set up DSBUFFERDESC structure.
	ZeroMemory(&dsbdesc, sizeof(DSBUFFERDESC));  // Zero it out. 
	dsbdesc.dwSize              = sizeof(DSBUFFERDESC);
	dsbdesc.dwFlags             = flags | DSBCAPS_CTRLPOSITIONNOTIFY | DSBCAPS_STICKYFOCUS | DSBCAPS_GLOBALFOCUS;
	dsbdesc.dwBufferBytes       = bufferLength; 
	dsbdesc.lpwfxFormat         = (LPWAVEFORMATEX)&wfx;
	dsbdesc.guid3DAlgorithm=DS3DALG_DEFAULT;

	// Once the buffer description complete, we have to create the buffer. 
	HRESULT hres = m_lpDS->CreateSoundBuffer(&dsbdesc, &m_lpDSBuffer, NULL);

	if(FAILED(hres))
		EXCEP(DirectSoundErr::GetErrDesc(hres), _T("Player::Create CreateSoundBuffer"));	

	return true;
	}


/*****
*	Player::Write : This function is used to fill in the LPDIRECTSOUND buffer with new data
*
*	@start	: Position where to start to put data in the DirectSoundBuffer
*	@data	: data itself.
*	@size	: length of the data buffer.
*
*/
void Player::Write(long start, const BYTE *data, long size) {
	// Lock data in buffer for writing
	LPVOID ptrData1;
	DWORD  ptrData1Size;
	LPVOID ptrData2;
	DWORD  ptrData2Size;
	HRESULT hres;
	
rifo:

	hres = m_lpDSBuffer->Lock(start, size, &ptrData1, &ptrData1Size, &ptrData2, &ptrData2Size, 
		0 /*0*/ /*DSBLOCK_FROMWRITECURSOR*/ /*DSBLOCK_ENTIREBUFFER*/);
	// dovrebbe essere DSBLOCK_FROMWRITECURSOR (=1) ma va peggio... boh.. 2023
	//   con 0 non ci sono mai ptrData2...
	if(FAILED(hres))
		EXCEP(DirectSoundErr::GetErrDesc(hres), _T("Player::Write Lock"));

	
	// https://www.yaldex.com/games-programming/0672323699_ch10lev1sec10.html
	// Fill in the LPDIRECTSOUND buffer with new data
//	CopyMemory(ptrData1, data, ptrData1Size);

//	CopyMemory(ptrData2, data+ptrData1Size, ptrData2Size /*size*/);		//2023

#if 0
	if(ptrData2) {		// NON deve succedere (v. DSBLOCK_FROMWRITECURSOR)! va comunque peggio...
//		hres = m_lpDSBuffer->Unlock(ptrData1, ptrData1Size, ptrData2, ptrData2Size);
//			hres = m_lpDSBuffer->Stop();		
//		hres = m_lpDSBuffer->SetCurrentPosition(start);
		theApp.m_Socket.bufferResync++;
//		goto rifo;
		CopyMemory(ptrData1,data+ptrData2Size /* se sono in ritardo, amen butto via il dipiù */, 
			ptrData1Size);
		}		// fa schifo sempre...
	else
		CopyMemory(ptrData1, data, ptrData1Size);
#endif


//	if(ptrData1Size != size)
//		theApp.bufferResync++;

	CopyMemory(ptrData1, data, ptrData1Size);
	if(ptrData2)
		CopyMemory(ptrData2, data+ptrData1Size, ptrData2Size);		//2023

	
	// Unlock data in buffer
	hres = m_lpDSBuffer->Unlock(ptrData1, ptrData1Size, ptrData2, ptrData2Size);
	if(FAILED(hres)) {
		EXCEP(DirectSoundErr::GetErrDesc(hres), _T("Player::Write Unlock"));
		}
	
	}


/*****
*	Player::Play : This function play the LPDIRECTSOUNDBUFFER. The buffer must be valid and
*					the buffer must have something in it to be played.
*
*	@flag	:	looping mode (0 = no, 1= yes)
*			
* https://www.yaldex.com/games-programming/0672323699_ch10lev1sec8.html
*
*/
void Player::Play(DWORD p_flag) {
	DWORD status;

	if(p_flag == 1)	{
		p_flag = DSBPLAY_LOOPING;
		}
	else {
		p_flag = 0;
		}

	HRESULT hres = m_lpDSBuffer->GetStatus(&status);
	if(FAILED(hres))
		EXCEP(DirectSoundErr::GetErrDesc(hres), _T("Player::Play GetStatus"));


//	if((status & DSBSTATUS_PLAYING) == DSBSTATUS_PLAYING)	 PROVARE?? in caso...
//		hres = m_lpDSBuffer->Stop();		



//	m_lpDSBuffer->SetCurrentPosition(0);		//2023



	if((status & DSBSTATUS_PLAYING) != DSBSTATUS_PLAYING)	{
		hres = m_lpDSBuffer->Play(0, 0, p_flag);
	
		if(FAILED(hres))		// Play the sound
			EXCEP(DirectSoundErr::GetErrDesc(hres), _T("Player::Play Play"));
		}
	}


/*****
*	Player::Stop : This function just stop playing the buffer.
*
*/
void Player::Stop()	{
	DWORD status;

	if(!m_lpDSBuffer)
		return;		// object not created

	HRESULT hres = m_lpDSBuffer->GetStatus(&status);
	if(FAILED(hres))
		EXCEP(DirectSoundErr::GetErrDesc(hres), _T("Player::Stop GetStatus"));

	if((status & DSBSTATUS_PLAYING) == DSBSTATUS_PLAYING)	{
		hres = m_lpDSBuffer->Stop();		

		if(FAILED(hres))		// stop the sound
			EXCEP(DirectSoundErr::GetErrDesc(hres), _T("Player::Stop Stop"));
		
		}
	}

BOOL Player::SetNotificationPositions(DWORD size,SIZE_T num) {
	DWORD *dwOffsets = new DWORD[num];		// per fine
	int i;

	for(i=0; i<num-1; i++)
		dwOffsets[i]=(size*(i+1))/(num+1);
		
	dwOffsets[i]=DSBPN_OFFSETSTOP;

	i=SetNotificationPositions(dwOffsets,num);
	delete dwOffsets;
	return i;
	}

BOOL Player::SetNotificationPositions(DWORD dwOffsets[], SIZE_T nSize) {
	std::vector<DWORD> events;

	for(int t=0; t<nSize; t++)
		events.push_back(dwOffsets[t]);

	return CreateEventReadNotification(events);

	}

void Player::SetSoundEventListener(/*IPlayer::SoundEventListener*/ CVidsendView *p_listener) {	// per client audio/video

	objectListener= p_listener;
	}
void Player::SetSoundEventListener(/*IPlayer::SoundEventListener*/ CVidsendView22 *p_listener) { // per server audio

	objectListener2= p_listener;
	}

	
void Player::RemoveSoundEventListener()	{
	ScopeGuardMutex g(m_mutex);

	objectListener= NULL;
	}

bool Player::CreateEventReadNotification(std::vector<DWORD>&vect_offset) {
	m_nbReadEvent = vect_offset.size();

	DBGASSERT( m_lpDSBuffer != NULL )
	DBGASSERT( m_pReadEvent == NULL )
	DBGASSERT( m_lpdsNotify == NULL )		// no event should have been created before
	DBGASSERT( m_pDSNotify == NULL )	
	
	DBGASSERT( m_nbReadEvent+2 <= MAXIMUM_WAIT_OBJECTS )	// must not create more than MAXIMUM_WAIT_OBJECTS events
	
	m_pReadEvent = new HANDLE[m_nbReadEvent + 1];
	m_pDSNotify = new DSBPOSITIONNOTIFY[m_nbReadEvent + 1];	
	
	for(int t=0; t < m_nbReadEvent; t++) {
		m_pReadEvent[t] = CreateEvent(NULL, 0, 0, NULL);

		m_pDSNotify[t].hEventNotify = m_pReadEvent[t];
		m_pDSNotify[t].dwOffset = vect_offset[t];
		}

	// end event
	m_pReadEvent[m_nbReadEvent] = CreateEvent(NULL, 0, 0, NULL);

	bool Success = true;

	HRESULT res = m_lpDSBuffer->QueryInterface(IID_IDirectSoundNotify,(void**)&m_lpdsNotify);
	if(FAILED(res))	{		
		EXCEP(DirectSoundErr::GetErrDesc(res), _T("Player::CreateWriteNotification QueryInterface"))
				
		Success = false;
		}

	if(Success == true)	{	
		res = m_lpdsNotify->SetNotificationPositions(m_nbReadEvent, m_pDSNotify);
		
		if( FAILED(res)) {
			EXCEP(DirectSoundErr::GetErrDesc(res), _T("Player::CreateWriteNotification SetNotificationPositions"))
		
			Success = false;
			}

		}
	
	if(Success == false)	
		return 0;
	
	
//	m_hNotifyEndThread = runThread((LPTHREAD_START_ROUTINE)WaitForNotify, this);		// (NO! uso polling in Play)
//	SetThreadPriority(m_hNotifyEndThread,/*THREAD_PRIORITY_ABOVE_NORMAL*/ THREAD_PRIORITY_HIGHEST);

	return true;
	}

/******************************************************

  WaitNotif

  Wait for a read notif.

******************************************************/
void Player::WaitNotif() {	
	DBGASSERT( m_pReadEvent != NULL );	
	
	DWORD WaitRet;
	bool Continue = true;	

	do {
		WaitRet = WaitForMultipleObjects(m_nbReadEvent+1,m_pReadEvent,0,INFINITE);		
		
		DWORD EventNumber = WaitRet - WAIT_OBJECT_0;
		if(WaitRet == WAIT_FAILED)
			Continue = false;
		else if(EventNumber < m_nbReadEvent)
			CallEvent(EventNumber);
		else if(EventNumber == m_nbReadEvent)
			Continue = false;

		} while(Continue);	
	
	}

void Player::WaitForNotify(Player* p_player) {

	p_player->WaitNotif();
	}

void Player::CallEvent(int eventNumber)	{
	ScopeGuardMutex g(m_mutex);

	if(objectListener)
		objectListener->OnSoundPlayerNotify(eventNumber);
	}


void Player::SetVolume(/*DWORD cursor,*/DWORD volume,DWORD volumeR,WORD dst) {
// NO!	MIXERLINE mxl;
//v. msdn: 0..-10000, solo attenuazione
//http://msdn.microsoft.com/library/default.asp?url=/library/en-us/directx9_c/directx/htm/idirectsoundbuffer8setvolume.asp

	if(!m_lpDSBuffer) {
		return;		// object not created
		}

	long n;
	if(volumeR==-1)
		n=volume;		// 			n=-(10001-2500.0*log10((volume)/7));
	else
		n=max(volume,volumeR);

	HRESULT hres = m_lpDSBuffer->SetVolume(n);

	if(FAILED(hres))	{			// 
		EXCEP(DirectSoundErr::GetErrDesc(hres), _T("Player::SetVolume"));
		}

	if(volumeR != -1) {
		n=(((int)volumeR)-((int)volume))/40;
		HRESULT hres = m_lpDSBuffer->SetPan(min(max(n,DSBPAN_LEFT /*-10000*/),DSBPAN_RIGHT /*10000*/));
		if(FAILED(hres))	{			// 
			EXCEP(DirectSoundErr::GetErrDesc(hres), _T("Player::SetPan"));
			}
		}

	}


void Player::SetPosition(long pos) {

	if(m_lpDSBuffer)
		m_lpDSBuffer->SetCurrentPosition(pos);
	}

DWORD Player::GetPosition() {
	DWORD dwCurrentPlayCursor,dwCurrentWriteCursor;

	HRESULT res = m_lpDSBuffer->GetCurrentPosition(&dwCurrentPlayCursor,&dwCurrentWriteCursor);
	if( FAILED(res)) {
		}
	return dwCurrentPlayCursor;
	}


// non si trova un modo semplice per windows 7 ecc...
// provare IDirectSoundCapture8 buffer come sopra
#if 0
#include <functiondiscoverykeys.h>
#include <EndpointVolume.h>


LPWSTR GetDeviceName(IMMDeviceCollection *DeviceCollection, UINT DeviceIndex)
{
    IMMDevice *device;
    LPWSTR deviceId;
    HRESULT hr;

    hr = DeviceCollection->Item(DeviceIndex, &device);
    if (FAILED(hr))
    {
        printf("Unable to get device %d: %x\n", DeviceIndex, hr);
        return NULL;
    }
    hr = device->GetId(&deviceId);
    if (FAILED(hr))
    {
        printf("Unable to get device %d id: %x\n", DeviceIndex, hr);
        return NULL;
    }

    IPropertyStore *propertyStore;
    hr = device->OpenPropertyStore(STGM_READ, &propertyStore);
    SafeRelease(&device);
    if (FAILED(hr))
    {
        printf("Unable to open device %d property store: %x\n", DeviceIndex, hr);
        return NULL;
    }

    PROPVARIANT friendlyName;
    PropVariantInit(&friendlyName);
    hr = propertyStore->GetValue(PKEY_Device_FriendlyName, &friendlyName);
    SafeRelease(&propertyStore);

    if (FAILED(hr))
    {
        printf("Unable to retrieve friendly name for device %d : %x\n", DeviceIndex, hr);
        return NULL;
    }

    wchar_t deviceName[128];
    hr = StringCbPrintf(deviceName, sizeof(deviceName), L"%s (%s)", friendlyName.vt != VT_LPWSTR ? L"Unknown" : friendlyName.pwszVal, deviceId);
    if (FAILED(hr))
    {
        printf("Unable to format friendly name for device %d : %x\n", DeviceIndex, hr);
        return NULL;
    }

    PropVariantClear(&friendlyName);
    CoTaskMemFree(deviceId);

    wchar_t *returnValue = _wcsdup(deviceName);
    if (returnValue == NULL)
    {
        printf("Unable to allocate buffer for return\n");
        return NULL;
    }
    return returnValue;
}
#endif


#if 0
#include <Audiopolicy.h>
#include <Mmdeviceapi.h>
//#include "AudioSessionVolume.h"

class CAudioSessionVolume : public IAudioSessionEvents
{
public:
    // Static method to create an instance of the object.
    static HRESULT CreateInstance( 
        UINT uNotificationMessage, 
        HWND hwndNotification, 
        CAudioSessionVolume **ppAudioSessionVolume 
    );

    // IUnknown methods.
    STDMETHODIMP QueryInterface(REFIID iid, void** ppv);
    STDMETHODIMP_(ULONG) AddRef();
    STDMETHODIMP_(ULONG) Release();

    // IAudioSessionEvents methods.

    STDMETHODIMP OnSimpleVolumeChanged( 
        float NewVolume, 
        BOOL NewMute, 
        LPCGUID EventContext 
        );

    STDMETHODIMP OnDisplayNameChanged( 
        LPCWSTR /*NewDisplayName*/,
        LPCGUID /*EventContext*/
        )
    {
        return S_OK;
    }
        
    STDMETHODIMP OnIconPathChanged(
        LPCWSTR /*NewIconPath*/, 
        LPCGUID /*EventContext*/
        )
    {
        return S_OK;
    }
        
    STDMETHODIMP OnChannelVolumeChanged( 
        DWORD /*ChannelCount*/,
        float /*NewChannelVolumeArray*/[],
        DWORD /*ChangedChannel*/,
        LPCGUID /*EventContext*/
        )
    {
        return S_OK;
    }
        
    STDMETHODIMP OnGroupingParamChanged( 
        LPCGUID /*NewGroupingParam*/,
        LPCGUID /*EventContext*/
        )
    {
        return S_OK;
    }
        
    STDMETHODIMP OnStateChanged(
        AudioSessionState /*NewState*/
        )
    {
        return S_OK;
    }
        
    STDMETHODIMP OnSessionDisconnected( 
        AudioSessionDisconnectReason /*DisconnectReason*/
        )
    {
        return S_OK;
    }

    // Other methods
    HRESULT EnableNotifications(BOOL bEnable );
    HRESULT GetVolume(float *pflVolume);
    HRESULT SetVolume(float flVolume);
    HRESULT GetMute(BOOL *pbMute);
    HRESULT SetMute(BOOL bMute);
    HRESULT SetDisplayName(const WCHAR *wszName);

protected:
    CAudioSessionVolume(UINT uNotificationMessage, HWND hwndNotification);
    ~CAudioSessionVolume();

    HRESULT Initialize();

protected:
    LONG m_cRef;                        // Reference count.
    UINT m_uNotificationMessage;        // Window message to send when an audio event occurs.
    HWND m_hwndNotification;            // Window to receives messages.
    BOOL m_bNotificationsEnabled;       // Are audio notifications enabled?

    IAudioSessionControl    *m_pAudioSession;
    ISimpleAudioVolume      *m_pSimpleAudioVolume;
};
#endif

CAudioMixer::CAudioMixer(const CWnd *w) {
	int i;
	WAVEFORMATEX wfx;
	DSCBUFFERDESC dsbdesc;

	theWnd=w;

	hMixer = NULL;
	m_lpDSCBuffer = NULL;
	m_lpDSC = NULL;

	// Create DirectSound Object
	// NON SERVE CMQ A UN CAZZO... NON C'è IL VOLUME; forse audioinputmixer
	HRESULT hres = DirectSoundCaptureCreate(0, &m_lpDSC, NULL);

	if(FAILED(hres))
		EXCEP(DirectSoundErr::GetErrDesc(hres), _T("Player::CreateDS DirectSoundCreate"));

// boh	hres = DirectSoundCaptureCreate(0, &m_AMMixer, NULL);

	// Set up wave format structure.
	ZeroMemory(&wfx, sizeof(WAVEFORMATEX));
		wfx.wFormatTag = WAVE_FORMAT_PCM;
		wfx.nChannels = 2;
		wfx.nSamplesPerSec = 44100;
		wfx.nBlockAlign = 4;
		wfx.wBitsPerSample = 16;
		wfx.nAvgBytesPerSec = wfx.nSamplesPerSec*wfx.nChannels*(wfx.wBitsPerSample/8);
		wfx.cbSize = 0;


	// Set up DSCBUFFERDESC structure.
	ZeroMemory(&dsbdesc, sizeof(DSCBUFFERDESC));  // Zero it out. 
	dsbdesc.dwSize              = sizeof(DSCBUFFERDESC);
	dsbdesc.dwFlags             = /*flags |*/ DSCBCAPS_WAVEMAPPED;
	dsbdesc.dwBufferBytes       = 44100L*2*2;
	dsbdesc.lpwfxFormat         = (LPWAVEFORMATEX)&wfx;
	dsbdesc.dwFXCount						= 0;
	dsbdesc.lpDSCFXDesc		 			= NULL;

	hres = m_lpDSC->CreateCaptureBuffer(&dsbdesc, &m_lpDSCBuffer, NULL);

	if(FAILED(hres))
		EXCEP(DirectSoundErr::GetErrDesc(hres), _T("Player::Create CreateCaptureSoundBuffer"));	

	m_TotMixers=mixerGetNumDevs();

//	m_volumeControlID = NULL;

	}

int CAudioMixer::SetVolume(DWORD cursor,DWORD level,DWORD level2,WORD dst) {
	int retVal=FALSE,error;
	DWORD j;

//	m_lpDSCBuffer->SetVolume();
	if(hMixer) {

   // Get the line info for the wave in destination line
		MIXERLINE mxl;
		mxl.cbStruct = sizeof(mxl);
    mxl.dwComponentType = dst /*MIXERLINE_COMPONENTTYPE_DST_WAVEIN*/;
    error=mixerGetLineInfo((HMIXEROBJ)hMixer, &mxl,	MIXER_GETLINEINFOF_COMPONENTTYPE);

		if(error != MMSYSERR_NOERROR)			// MIXERR_INVALLINE=1024 su Vista ecc... :(
			return retVal;

		// Now find the microphone source line connected to this wave in
		// destination
		DWORD cConnections = mxl.cConnections;

//CString S;
//S.Format("%u",cConnections);
//AfxMessageBox(S);

		for(j=0; j<cConnections; j++) {
			mxl.dwSource = j;
      mixerGetLineInfo((HMIXEROBJ)hMixer, &mxl, MIXER_GETLINEINFOF_SOURCE);
      if(cursor /*MIXERLINE_COMPONENTTYPE_SRC_MICROPHONE*/ == mxl.dwComponentType)
				break;
			}

		if(j == cConnections)
			return retVal;

   // Find a volume control, if any, of the microphone line
		LPMIXERCONTROL pmxctrl = (LPMIXERCONTROL)malloc(sizeof MIXERCONTROL);
		MIXERLINECONTROLS mxlctrl = {sizeof mxlctrl, mxl.dwLineID,
      MIXERCONTROL_CONTROLTYPE_VOLUME, 1, sizeof MIXERCONTROL, pmxctrl};
		if(!mixerGetLineControls((HMIXEROBJ) hMixer, &mxlctrl, MIXER_GETLINECONTROLSF_ONEBYTYPE)) {
      // Found!
      DWORD cChannels = mxl.cChannels;
//      if(MIXERCONTROL_CONTROLF_UNIFORM & pmxctrl->fdwControl)
//         cChannels = 1;

      LPMIXERCONTROLDETAILS_UNSIGNED pUnsigned =
				(LPMIXERCONTROLDETAILS_UNSIGNED)
        malloc(cChannels * sizeof MIXERCONTROLDETAILS_UNSIGNED);
			MIXERCONTROLDETAILS mxcd = {sizeof(mxcd), pmxctrl->dwControlID,
				cChannels, (HWND)0,
				sizeof MIXERCONTROLDETAILS_UNSIGNED, (LPVOID) pUnsigned};
			mixerGetControlDetails((HMIXEROBJ)hMixer, &mxcd, MIXER_SETCONTROLDETAILSF_VALUE);
      // Set the volume to the middle  (for both channels as needed)
//      pUnsigned[0].dwValue = pUnsigned[cChannels - 1].dwValue =
//       (pmxctrl->Bounds.dwMinimum+pmxctrl->Bounds.dwMaximum)/2;
      pUnsigned[0].dwValue = level;
			if(level2==0xffffffff)
				level2=level;
			if(cChannels>1)
				pUnsigned[1].dwValue = level2;
      mixerSetControlDetails((HMIXEROBJ)hMixer, &mxcd, MIXER_SETCONTROLDETAILSF_VALUE);

			free(pmxctrl);
			free(pUnsigned);
			}
		else
			free(pmxctrl);
		}
	return retVal;
	}

DWORD CAudioMixer::GetVolume(DWORD cursor) {
	MIXERLINE mxl;
	MIXERCONTROLDETAILS_UNSIGNED mx1;
	MIXERCONTROL mxc;
	MIXERLINECONTROLS mxlc;
	MIXERCONTROLDETAILS_UNSIGNED mxcdVolume;
	MIXERCONTROLDETAILS mxcd;
	int retVal=-1;
//https://stackoverflow.com/questions/294292/changing-master-volume-level/294525#294525

//	ISimpleAudioVolume volume;
	// non c'è speranza

	if(hMixer) {
		ZeroMemory(&mxl,sizeof(mxl));
		ZeroMemory(&mxc,sizeof(mxc));
		ZeroMemory(&mxlc,sizeof(mxlc));
		ZeroMemory(&mxcd,sizeof(mxcd));
		// get dwLineID
		mxl.cbStruct = sizeof(MIXERLINE);
		mxl.dwComponentType = cursor;
		// MIXERR_INVALLINE=1024 su Vista ecc... :(
		if(mixerGetLineInfo((HMIXEROBJ)hMixer,&mxl,MIXER_OBJECTF_HMIXER | MIXER_GETLINEINFOF_COMPONENTTYPE) == MMSYSERR_NOERROR) {
			// get dwControlID
			mxlc.cbStruct = sizeof(MIXERLINECONTROLS);
			mxlc.dwLineID = mxl.dwLineID;
			mxlc.dwControlType = MIXERCONTROL_CONTROLTYPE_VOLUME;
			mxlc.cControls = 1;
			mxlc.cbmxctrl = sizeof(MIXERCONTROL);
			mxlc.pamxctrl = &mxc;
			if(mixerGetLineControls((HMIXEROBJ)hMixer,&mxlc,MIXER_OBJECTF_HMIXER | MIXER_GETLINECONTROLSF_ONEBYTYPE) == MMSYSERR_NOERROR) {
				// save record dwControlID
				mxcd.cbStruct = sizeof(MIXERCONTROLDETAILS);
				mxcd.dwControlID = mxc.dwControlID;
				mxcd.cChannels = /*2,*/ mxl.cChannels;
				mxcd.cMultipleItems = 0;
				mxcd.cbDetails = sizeof(MIXERCONTROLDETAILS_UNSIGNED);
				mxcd.paDetails = &mxcdVolume;
				if(mixerGetControlDetails((HMIXEROBJ)hMixer,&mxcd,MIXER_OBJECTF_HMIXER | MIXER_GETCONTROLDETAILSF_VALUE) == MMSYSERR_NOERROR) {
					retVal=mxcdVolume.dwValue;
					}
				}
			}
		}
	return retVal;
	}

int CAudioMixer::Switch(DWORD cursor,BOOL bEnable) {
	MIXERLINE mxl;
	MIXERCONTROLDETAILS_BOOLEAN mx1;
	MIXERCONTROL mxc;
	MIXERLINECONTROLS mxlc;
	MIXERCONTROLDETAILS_UNSIGNED mxcdVolume;
	MIXERCONTROLDETAILS mxcd;
	int retVal=FALSE;

	if(hMixer) {
		ZeroMemory(&mxl,sizeof(mxl));
		ZeroMemory(&mxc,sizeof(mxc));
		ZeroMemory(&mxlc,sizeof(mxlc));
		ZeroMemory(&mxcd,sizeof(mxcd));
		// get dwLineID
		mxl.cbStruct = sizeof(MIXERLINE);
		mxl.dwComponentType = cursor;
		if(mixerGetLineInfo((HMIXEROBJ)hMixer,&mxl,MIXER_OBJECTF_HMIXER | MIXER_GETLINEINFOF_COMPONENTTYPE) == MMSYSERR_NOERROR) {
			// get dwControlID
			mxlc.cbStruct = sizeof(MIXERLINECONTROLS);
			mxlc.dwLineID = mxl.dwLineID;
			mxlc.dwControlType = MIXERCONTROL_CONTROLTYPE_MUTE;
			mxlc.cControls = 1;
			mxlc.cbmxctrl = sizeof(MIXERCONTROL);
			mxlc.pamxctrl = &mxc;
			if(mixerGetLineControls((HMIXEROBJ)hMixer,&mxlc,MIXER_OBJECTF_HMIXER | MIXER_GETLINECONTROLSF_ONEBYTYPE) == MMSYSERR_NOERROR) {
				mxcd.cbStruct = sizeof(MIXERCONTROLDETAILS);
				mxcd.dwControlID = mxc.dwControlID;
				mxcd.cChannels = 1;
				mxcd.cMultipleItems = 0;
				mxcd.cbDetails = sizeof(MIXERCONTROLDETAILS_BOOLEAN);
				mx1.fValue=!bEnable;		// MUTE (v. sopra) e' al contrario...
				mxcd.paDetails=&mx1;
				retVal=mixerSetControlDetails((HMIXEROBJ)hMixer,&mxcd,MIXER_OBJECTF_HMIXER | MIXER_SETCONTROLDETAILSF_VALUE) == MMSYSERR_NOERROR;
				}
			}
		}
	return retVal;
	}

int CAudioMixer::Choose(DWORD cursor) {
	MIXERLINE mxl;
	MIXERCONTROL mxc;
	MIXERCONTROLDETAILS mxcd;
	MIXERLINECONTROLS mxlc;
	DWORD dwControlType;
	DWORD dwSelectControlID,dwMultipleItems,dwIndex;
	int retVal=FALSE;

	if(hMixer) {

		// get dwLineID
		mxl.cbStruct = sizeof(MIXERLINE);
		mxl.dwComponentType = MIXERLINE_COMPONENTTYPE_DST_WAVEIN;
		if(mixerGetLineInfo((HMIXEROBJ)hMixer,&mxl,
								 MIXER_OBJECTF_HMIXER | MIXER_GETLINEINFOF_COMPONENTTYPE)
									!= MMSYSERR_NOERROR) {
			goto fine;
			}

		// get dwControlID
		dwControlType = MIXERCONTROL_CONTROLTYPE_MIXER;
		mxlc.cbStruct = sizeof(MIXERLINECONTROLS);
		mxlc.dwLineID = mxl.dwLineID;
		mxlc.dwControlType = dwControlType;
		mxlc.cControls = 1;
		mxlc.cbmxctrl = sizeof(MIXERCONTROL);
		mxlc.pamxctrl = &mxc;
		if(mixerGetLineControls((HMIXEROBJ)hMixer,&mxlc,
							   MIXER_OBJECTF_HMIXER |
							   MIXER_GETLINECONTROLSF_ONEBYTYPE)
									!= MMSYSERR_NOERROR) {
			// no mixer, try MUX
			dwControlType = MIXERCONTROL_CONTROLTYPE_MUX;
			mxlc.cbStruct = sizeof(MIXERLINECONTROLS);
			mxlc.dwLineID = mxl.dwLineID;
			mxlc.dwControlType = dwControlType;
			mxlc.cControls = 1;
			mxlc.cbmxctrl = sizeof(MIXERCONTROL);
			mxlc.pamxctrl = &mxc;
			if(mixerGetLineControls((HMIXEROBJ)hMixer,&mxlc,
								   MIXER_OBJECTF_HMIXER | MIXER_GETLINECONTROLSF_ONEBYTYPE)
										!= MMSYSERR_NOERROR) {
				goto fine;
				}
			}

		dwSelectControlID = mxc.dwControlID;
		dwMultipleItems = mxc.cMultipleItems;

		if(dwMultipleItems == 0)
			goto fine;

		// get the index of the Microphone Select control
		MIXERCONTROLDETAILS_LISTTEXT *pmxcdSelectText =
			new MIXERCONTROLDETAILS_LISTTEXT[dwMultipleItems];

		if(pmxcdSelectText) {
			mxcd.cbStruct = sizeof(MIXERCONTROLDETAILS);
			mxcd.dwControlID = dwSelectControlID;
			mxcd.cChannels = 1;
			mxcd.cMultipleItems = dwMultipleItems;
			mxcd.cbDetails = sizeof(MIXERCONTROLDETAILS_LISTTEXT);
			mxcd.paDetails = pmxcdSelectText;
			if(mixerGetControlDetails((HMIXEROBJ)hMixer,&mxcd,
										 MIXER_OBJECTF_HMIXER | MIXER_GETCONTROLDETAILSF_LISTTEXT)
				== MMSYSERR_NOERROR) {
				// determine which controls the Microphone source line
				for(DWORD dwi = 0; dwi < dwMultipleItems; dwi++) {
					// get the line information
					MIXERLINE mxl2;
					mxl2.cbStruct = sizeof(MIXERLINE);
					mxl2.dwLineID = pmxcdSelectText[dwi].dwParam1;
					if(mixerGetLineInfo((HMIXEROBJ)hMixer,&mxl2,
											 MIXER_OBJECTF_HMIXER | MIXER_GETLINEINFOF_LINEID)
						== MMSYSERR_NOERROR &&
						mxl2.dwComponentType == cursor /*MIXERLINE_COMPONENTTYPE_SRC_MICROPHONE*/) {
						// found, dwi is the index.
						dwIndex = dwi;
	//					m_strMicName = pmxcdSelectText[dwi].szName;
						break;
						}
					}

				if(dwi >= dwMultipleItems) {
					// could not find it using line IDs, some mixer drivers have
					// different meaning for MIXERCONTROLDETAILS_LISTTEXT.dwParam1.
					// let's try comparing the item names.
					for(dwi = 0; dwi < dwMultipleItems; dwi++) {
						if(!strcmp(pmxcdSelectText[dwi].szName,_T("Microphone"))) {
							// found, dwi is the index.
							dwIndex = dwi;
	//						m_strMicName = pmxcdSelectText[dwi].szName;
							break;
							}
						}
					}
				}

			delete []pmxcdSelectText;
			}

		if(dwMultipleItems == 0 || dwIndex >= dwMultipleItems)
			goto fine;

		BOOL bRetVal = FALSE;

		// get all the values first
		MIXERCONTROLDETAILS_BOOLEAN *pmxcdSelectValue =
			new MIXERCONTROLDETAILS_BOOLEAN[dwMultipleItems];

		if(pmxcdSelectValue != NULL) {
			mxcd.cbStruct = sizeof(MIXERCONTROLDETAILS);
			mxcd.dwControlID = dwSelectControlID;
			mxcd.cChannels = 1;
			mxcd.cMultipleItems = dwMultipleItems;
			mxcd.cbDetails = sizeof(MIXERCONTROLDETAILS_BOOLEAN);
			mxcd.paDetails = pmxcdSelectValue;
			if(mixerGetControlDetails((HMIXEROBJ)hMixer,&mxcd,
										 MIXER_OBJECTF_HMIXER | MIXER_GETCONTROLDETAILSF_VALUE)
										== MMSYSERR_NOERROR) {
				ASSERT(dwControlType == MIXERCONTROL_CONTROLTYPE_MIXER ||
						 dwControlType == MIXERCONTROL_CONTROLTYPE_MUX);

				// MUX restricts the line selection to one source line at a time.
				DWORD lVal=1;			// attivo...
				if(lVal != 0 && dwControlType == MIXERCONTROL_CONTROLTYPE_MUX) {
					ZeroMemory(pmxcdSelectValue,dwMultipleItems * sizeof(MIXERCONTROLDETAILS_BOOLEAN));
					}

				// set the Microphone value
				pmxcdSelectValue[dwIndex].fValue = lVal;

				mxcd.cbStruct = sizeof(MIXERCONTROLDETAILS);
				mxcd.dwControlID = dwSelectControlID;
				mxcd.cChannels = 1;
				mxcd.cMultipleItems = dwMultipleItems;
				mxcd.cbDetails = sizeof(MIXERCONTROLDETAILS_BOOLEAN);
				mxcd.paDetails = pmxcdSelectValue;
				if(mixerSetControlDetails((HMIXEROBJ)hMixer,&mxcd,
											 MIXER_OBJECTF_HMIXER | MIXER_SETCONTROLDETAILSF_VALUE)
											== MMSYSERR_NOERROR) {
					retVal = TRUE;
					}
				}

			delete []pmxcdSelectValue;
		
			}
		}

fine:
	return retVal;
	}


CAudioMixer::~CAudioMixer() {

//	if(m_lpdsNotify)
//		m_lpdsNotify->Release();
	
	if(m_lpDSCBuffer)		
		m_lpDSCBuffer->Release();
		
	if(m_lpDSC)
		m_lpDSC->Release();

	if(hMixer)
		mixerClose(hMixer);
	}


/////////////////////////////////////////////////////////////////////////////////////////////////////////

// Copyright(C) 2004-2006, Arman Sahakyan


// This comment-directives perform linking with libraries.
// If You order linking through Project/Settings/Link tab,
// You may remove this comment-diractives.
//#pragma comment(lib, "Wavelib.lib")
//#pragma comment(lib, "dsound.lib")

//using namespace DirectSound;

//------------------------------------ CDSound
CDSound::CDSound() {

	m_pDirectSound = NULL;
	}

CDSound::~CDSound() {

	Destroy();
	}

BOOL CDSound::Create(BYTE q) {
	struct DIRECTSOUND_DESCR dsd;

	dsd.num=q;
	dsd.guid=0;

	DirectSoundEnumerate(Player::DSEnumCallback,&dsd);
  //			 per usare più periferiche (preascolto cuffia ecc)
		
	if(dsd.num>=0)
		return 0;

	if(::DirectSoundCreate(dsd.guid, &m_pDirectSound, NULL) != DS_OK) {
//		theApp.FileSpool->print(CLogFile::flagInfo,"Cannot create DirectSound object!");
//		TRACE("Cannot create DirectSound object!", MB_OK, MB_ICONERROR);
		Destroy();
		return FALSE;
		}
	return TRUE;
	}

BOOL CDSound::SetCooperativeLevel(HWND hWnd, DWORD dwLevel /* = DSSCL_NORMAL*/) {

	if(m_pDirectSound->SetCooperativeLevel(hWnd, dwLevel) != DS_OK) {
//		theApp.FileSpool->print(CLogFile::flagInfo,"Cannot set cooperative level!");
//		TRACE("Cannot set cooperative level!", MB_OK, MB_ICONERROR);
		Destroy();
		return FALSE;
		}
	return TRUE;
	}


//---------------------------------------- CDSBuffer

CDSBuffer::CDSBuffer() {

	m_pDSBuffer = NULL;
	m_uIDRcsrWave = -1;
	m_dwFlags = 0;
	m_dwImageLen=0;
	}

CDSBuffer::~CDSBuffer() {
	}


BOOL CDSBuffer::Create(IDirectSound *pDirectSound, const CWave &wave, DWORD dwFlags) {

	DWORD dwDataLen = wave.GetDataLen();
	if(!dwDataLen)
		return 0;

	m_dwImageLen=dwDataLen;

	WAVEFORMATEX wfFormat;
	wave.GetFormat(wfFormat);
	DSBUFFERDESC dsbdDesc;
	ZeroMemory(&dsbdDesc, sizeof(DSBUFFERDESC));
	dsbdDesc.dwSize = sizeof(DSBUFFERDESC);
	dsbdDesc.dwFlags = dwFlags; 
		//DSBCAPS_CTRLDEFAULT | DSBCAPS_STATIC | DSBCAPS_LOCSOFTWARE
		//| DSBCAPS_CTRLPOSITIONNOTIFY ;
	dsbdDesc.dwBufferBytes = dwDataLen;
	dsbdDesc.lpwfxFormat = &wfFormat;
	dsbdDesc.guid3DAlgorithm=DS3DALG_DEFAULT;
	HRESULT hr;
	CComPtr<IDirectSoundBuffer> lpDSB;
	if((hr = pDirectSound->CreateSoundBuffer(&dsbdDesc, &lpDSB, NULL)) != DS_OK) {
		CString str(TEXT(""));
		if(hr == DSERR_BADFORMAT) 
			str = TEXT(" The specified wave format is not supported.");
//		theApp.FileSpool->print(CLogFile::flagInfo,"Cannot create SoundBuffer object %s! ", str);
//		TRACE("Cannot create SoundBuffer object! " +  str);
		return FALSE;
		}
	if(FAILED(hr = lpDSB->QueryInterface(IID_IDirectSoundBuffer, (LPVOID *)&m_pDSBuffer)))	{
//		theApp.FileSpool->print(CLogFile::flagInfo,"Cannot query SoundBuffer8 object! ");
//		TRACE("Cannot query SoundBuffer8 object! ");
		return hr;
		}
	PBYTE pDSBuffData;
	if(m_pDSBuffer->Lock(0, dwDataLen, (PVOID *)&pDSBuffData, &dwDataLen, NULL, 0, 0) != DS_OK) {
//		theApp.FileSpool->print(CLogFile::flagInfo,"Fail to lock SoundBuffer object!");
//		TRACE("Fail to lock SoundBuffer object!", MB_OK, MB_ICONERROR);
		return FALSE;
		}
	// putting WAV datas into sound buffer
	dwDataLen = wave.GetData(pDSBuffData, dwDataLen);
	if(m_pDSBuffer->Unlock(pDSBuffData, dwDataLen, NULL, 0) != DS_OK) {
//		theApp.FileSpool->print(CLogFile::flagInfo,"Fail to unlock SoundBuffer object!");
//		TRACE("Fail to unlock SoundBuffer object!", MB_OK, MB_ICONERROR);
		return FALSE;
		}
	m_pDirectSound = pDirectSound;

	if(dwFlags & DSBCAPS_CTRLPOSITIONNOTIFY) {
		hr = m_pDSBuffer->QueryInterface(IID_IDirectSoundNotify, (void **)&m_pDSNotify);
		if(FAILED(hr))	{
	//		theApp.FileSpool->print(CLogFile::flagInfo,"Fail to query IDirectSoundNotify object!");
	//		TRACE("Fail to query IDirectSoundNotify object!", MB_OK, MB_ICONERROR);
			return FALSE;
			}
		}

	return TRUE;
	}

BOOL CDSBuffer::Create(IDirectSound *pDirectSound, UINT uIDRcsrWave, DWORD dwFlags, HMODULE hMod /*= AfxGetInstanceHandle()*/) {
	CWave wave;

	wave.Create(uIDRcsrWave, hMod);
	return Create(pDirectSound, wave, dwFlags);
	}

BOOL CDSBuffer::Create(IDirectSound *pDirectSound, LPCTSTR lpszFileName, DWORD dwFlags) {
	CWave wave;

	wave.Create(lpszFileName);
	return Create(pDirectSound, wave, dwFlags);
	}

BOOL CDSBuffer::Create(IDirectSound *pDirectSound, LPCTSTR lpszFileName, CWave &wave, DWORD dwFlags) {
	
	wave.Create(lpszFileName);
	return Create(pDirectSound, wave, dwFlags);
	}

BOOL CDSBuffer::Restore() {
	
	if(m_pDSBuffer->Restore() != DS_OK)
		return FALSE;
	return Create(m_pDirectSound, m_uIDRcsrWave, m_dwFlags);
	}

BOOL CDSBuffer::Create(IDirectSound *pDirectSound, const BYTE *pBuf, DWORD dwDataLen, WAVEFORMATEX wfFormat, DWORD dwFlags) {

	if(!dwDataLen)
		return 0;

	m_dwImageLen=dwDataLen;

	DSBUFFERDESC dsbdDesc;
	ZeroMemory(&dsbdDesc, sizeof (DSBUFFERDESC));
	dsbdDesc.dwSize = sizeof (DSBUFFERDESC);
	dsbdDesc.dwFlags = dwFlags | DSBCAPS_STICKYFOCUS | DSBCAPS_GLOBALFOCUS;
		//DSBCAPS_CTRLDEFAULT | DSBCAPS_STATIC | DSBCAPS_LOCSOFTWARE
		//| DSBCAPS_CTRLPOSITIONNOTIFY ;
	dsbdDesc.dwBufferBytes = dwDataLen;
	dsbdDesc.lpwfxFormat = &wfFormat;
	HRESULT hr;
	CComPtr<IDirectSoundBuffer> lpDSB;
	if((hr = pDirectSound->CreateSoundBuffer(&dsbdDesc, &lpDSB, NULL)) != DS_OK) {
		CString str(TEXT(""));
		if(hr == DSERR_BADFORMAT) 
			str = TEXT(" The specified wave format is not supported.");
		str.Format("%x",DSERR_BUFFERTOOSMALL);
//		theApp.FileSpool->print(CLogFile::flagInfo,"Cannot create SoundBuffer object %s! ", str);
//		TRACE("Cannot create SoundBuffer object! " +  str);
		return FALSE;
		}
	if(FAILED( hr = lpDSB->QueryInterface(IID_IDirectSoundBuffer, (LPVOID *) &m_pDSBuffer)))	{
//		theApp.FileSpool->print(CLogFile::flagInfo,"Cannot query SoundBuffer8 object! ");
//		TRACE("Cannot query SoundBuffer8 object! ");
		return hr;
		}

	PBYTE pDSBuffData;
	if(m_pDSBuffer->Lock(0, dwDataLen, (PVOID *)&pDSBuffData, &dwDataLen, NULL, 0, 0) != DS_OK) {
//		theApp.FileSpool->print(CLogFile::flagInfo,"Fail to lock SoundBuffer object!");
//		TRACE("Fail to lock SoundBuffer object!", MB_OK, MB_ICONERROR);
		return FALSE;
		}
	// putting WAV datas into sound buffer
	memcpy(pDSBuffData,pBuf,dwDataLen);
	if(m_pDSBuffer->Unlock((BYTE *)pDSBuffData, dwDataLen, NULL, 0) != DS_OK) {
//		theApp.FileSpool->print(CLogFile::flagInfo,"Fail to unlock SoundBuffer object!");
//		TRACE("Fail to unlock SoundBuffer object!", MB_OK, MB_ICONERROR);
		return FALSE;
		}
	m_pDirectSound = pDirectSound;

	if(dwFlags & DSBCAPS_CTRLPOSITIONNOTIFY) {
	hr = m_pDSBuffer->QueryInterface(IID_IDirectSoundNotify, (void **)&m_pDSNotify);
	if(FAILED(hr))	{
//		theApp.FileSpool->print(CLogFile::flagInfo,"Fail to query IDirectSoundNotify object!");
//		TRACE("Fail to query IDirectSoundNotify object!", MB_OK, MB_ICONERROR);
		return FALSE;
		}
	}

	return TRUE;
	}

//////------------------ CDSoundPlay
CDSoundPlay::CDSoundPlay() {

	m_pDirectSound = NULL;
	m_pFunc = NULL;
	m_phEvents = NULL;
	m_pThread = NULL;
	m_pFuncParm=0;
	}

CDSoundPlay::~CDSoundPlay() {
	
	/*this->*/Destroy();
	}

void CDSoundPlay::Destroy() {
	
	if(m_pThread)	{
		::PulseEvent(m_hEventThreadStop);
		::WaitForSingleObject(m_pThread->m_hThread, 500/*INFINITE*/); // tolto infinite che a volte si blocca... 2023 boh?
		}
	m_pFunc = NULL;
	m_pFuncParm=0;
	m_phEvents = NULL;
	m_pThread = NULL;
	m_hEventThreadStop = NULL;
	m_DSBuffer.Destroy();
	}

BOOL CDSoundPlay::Create(CDSound *pDSound, UINT uIDRsrcWave, DWORD dwFlags, HMODULE hMod /*= AfxGetInstanceHandle()*/) {
	
	m_pDirectSound = pDSound;
	return m_DSBuffer.Create(*m_pDirectSound, uIDRsrcWave, dwFlags, hMod);
	}

BOOL CDSoundPlay::Create(CDSound *pDSound, LPCTSTR lpszFileName, DWORD dwFlags) {
	
	m_DSBuffer.Destroy();

	m_pDirectSound = pDSound;
	return m_DSBuffer.Create(*m_pDirectSound, lpszFileName, dwFlags);
	}

BOOL CDSoundPlay::Create(CDSound *pDSound, LPCTSTR lpszFileName, CWave &wave, DWORD dwFlags) {
	
	m_DSBuffer.Destroy();

	m_pDirectSound = pDSound;
	return m_DSBuffer.Create(*m_pDirectSound, lpszFileName, wave, dwFlags);
	}

BOOL CDSoundPlay::Create(CDSound *pDSound, const BYTE *pBuf, DWORD len, WAVEFORMATEX *wf, DWORD dwFlags) {
	WAVEFORMATEX defWAVE;

	m_pDirectSound = pDSound;
	if(wf) {
		defWAVE=*wf;
		}
	else {
		defWAVE.wFormatTag=WAVE_FORMAT_PCM;		// duplicato da sopra... x ora!
		defWAVE.nChannels=1;
		defWAVE.nSamplesPerSec=22050;
		defWAVE.nAvgBytesPerSec=22050;
		defWAVE.nBlockAlign=1;
		defWAVE.wBitsPerSample=8;
		defWAVE.cbSize=sizeof(WAVEFORMATEX);
		}

	return m_DSBuffer.Create(*m_pDirectSound, pBuf, len, defWAVE, dwFlags);
	}

DWORD CDSoundPlay::GetCurrentPosition(LPDWORD lpdwCurrentWriteCursor) const {
	DWORD dwCurrPlayPos;
	HRESULT hr;

	hr = m_DSBuffer->GetCurrentPosition(&dwCurrPlayPos, lpdwCurrentWriteCursor);
	if(FAILED(hr)) {
		TRACE("- fail: GetCurrentPosition: hresult = 0x%x\n", hr);
		return (DWORD)DSOUND_ERR;
		}
	return dwCurrPlayPos;
	}

DWORD CDSoundPlay::GetFrequency() const {
	DWORD dwFreq;
	HRESULT hr;

	hr = m_DSBuffer->GetFrequency(&dwFreq);
	if(FAILED(hr))	{
		TRACE("- fail: GetFrequency: hresult = 0x%x\n", hr);
		return (DWORD)DSOUND_ERR;
		}
	return dwFreq;
	}

LONG CDSoundPlay::GetPan() const {
	LONG lPan;
	HRESULT hr;

	hr = m_DSBuffer->GetPan(&lPan);
	if(FAILED(hr)) {
		TRACE("- fail: GetPan: hresult = 0x%x\n", hr);
		return DSOUND_ERR;
		}
	return lPan;
	}

LONG CDSoundPlay::GetVolume() const {
	LONG lVol;
	HRESULT hr;

	hr = m_DSBuffer->GetVolume(&lVol);
	if(FAILED(hr)) {
		TRACE("- fail: GetVolume: hresult = 0x%x\n", hr);
		return DSOUND_ERR;
		}
	return lVol;
	}

DWORD CDSoundPlay::GetStatus() const {
	DWORD dwStatus;
	HRESULT hr;

	hr = m_DSBuffer->GetStatus(&dwStatus);
	if(FAILED(hr)) {
		TRACE("- fail: GetStatus: hresult = 0x%x\n", hr);
		return (DWORD)DSOUND_ERR;
		}
	return dwStatus;
	}

BOOL CDSoundPlay::Play(BOOL bLooping /* = FALSE */) {
	HRESULT hr;

	hr = m_DSBuffer->Play(0, 0, bLooping ? DSBPLAY_LOOPING : 0);
	if(FAILED(hr)) {
		TRACE("- fail: Play: hresult = 0x%x\n", hr);
		return FALSE;
		}

	return TRUE;
	}

BOOL CDSoundPlay::SetCurrentPosition(DWORD dwNewPos) {
	HRESULT hr;

	hr = m_DSBuffer->SetCurrentPosition(dwNewPos);
	if(FAILED(hr))	{
		TRACE("- fail: SetCurrentPosition: hresult = 0x%x\n", hr);
		return FALSE;
		}

	return TRUE;
	}

BOOL CDSoundPlay::SetFrequency(DWORD dwFreq) {
	HRESULT hr;

	hr = m_DSBuffer->SetFrequency(dwFreq);
	if(FAILED(hr))	{
		TRACE("- fail: SetFrequency: hresult = 0x%x\n", hr);
		return FALSE;
		}

	return TRUE;
	}

BOOL CDSoundPlay::SetPan(LONG lPan) {
	HRESULT hr;

	hr = m_DSBuffer->SetPan(lPan);
	if(FAILED(hr)) {
		TRACE("- fail: SetPan: hresult = 0x%x\n", hr);
		return FALSE;
		}

	return TRUE;
	}

BOOL CDSoundPlay::SetVolume(LONG lVolume) {
	HRESULT hr;

	if(m_DSBuffer.GetBuffer()) {
		hr = m_DSBuffer->SetVolume(lVolume);
		if(FAILED(hr)) {
			TRACE("- fail: SetVolume: hresult = 0x%x\n", hr);
			return FALSE;
			}
		}

	return TRUE;
	}

BOOL CDSoundPlay::Stop() {
	HRESULT hr;

	hr = m_DSBuffer->Stop();
	if(FAILED(hr)) {
		TRACE("- fail: Stop: hresult = 0x%x\n", hr);
		return FALSE;
		}

	return TRUE;
	}

BOOL CDSoundPlay::SetAutoNotificationPositions(SIZE_T num) {
	DWORD *dwOffsets = new DWORD[num];		// per fine
	int i;
//	unsigned long n;
//	WAVEFORMATEX *wfex;

//	m_DSBuffer->GetFormat(NULL,0,&n);
//	wfex=(WAVEFORMATEX *)GlobalAlloc(GPTR,n);
//	m_DSBuffer->GetFormat(wfex,n,NULL);
//		GlobalFree(wfex);

	for(i=0; i<num-1; i++)
		dwOffsets[i]=(m_DSBuffer.m_dwImageLen*(i+1))/(num+1);
		
	dwOffsets[i]=DSBPN_OFFSETSTOP;

	i=SetNotificationPositions(dwOffsets,num);
	delete dwOffsets;
	return i;
	}

BOOL CDSoundPlay::SetNotificationPositions(DWORD dwOffsets[], SIZE_T nSize) {
	
	if(!dwOffsets && m_pThread) { // Cancel
		::PulseEvent(m_hEventThreadStop);
		::WaitForSingleObject(m_pThread->m_hThread, INFINITE); // wait until the thread ends
		m_pThread = NULL;
		m_hEventThreadStop = NULL;
		return TRUE;
		}

	if(!m_pFunc)
		return FALSE;

	delete []m_phEvents;
	m_phEvents = new HANDLE[nSize+1];

	LPDSBPOSITIONNOTIFY pdsb = new DSBPOSITIONNOTIFY[nSize];

	for(SIZE_T j=0; j < nSize; j ++)	{
		pdsb[j].dwOffset = dwOffsets[j];
		pdsb[j].hEventNotify = m_phEvents[j] = ::CreateEvent(NULL, FALSE, FALSE, NULL);
		}

	HRESULT hr = m_DSBuffer.GetDSNotify()->SetNotificationPositions(nSize, pdsb);
	delete []pdsb;

	if(FAILED(hr)) {
		TRACE("- fail: SetNotificationPositions: hresult = 0x%x\n", hr);
		delete []m_phEvents;
		return FALSE;
		}

	PINFO pInfo = new _INFO;
	pInfo->dwOffsets = new DWORD[nSize];
	for(SIZE_T t=0; t < nSize; t++)
		pInfo->dwOffsets[t] = dwOffsets[t];
	pInfo->pFunc = m_pFunc;
	pInfo->pFuncParm=m_pFuncParm;
	pInfo->hEvents = m_phEvents;
	m_phEvents = NULL; // the memory will be deleted in the thread via pInfo->hEvents
	pInfo->hEvents[nSize] = m_hEventThreadStop = ::CreateEvent(NULL, FALSE, FALSE, NULL);
	pInfo->nSize = nSize;

	{
		unsigned long n;
		WAVEFORMATEX *wfex;
		m_DSBuffer->GetFormat(NULL,0,&n);
		wfex=(WAVEFORMATEX *)GlobalAlloc(GPTR,n);
		m_DSBuffer->GetFormat(wfex,n,NULL);
		pInfo->pFunc((void*)pInfo->pFuncParm,-3,CWave::GetSeconds(m_DSBuffer.m_dwImageLen,wfex));
		GlobalFree(wfex);
	}
	

	if(m_pThread) { // If running we should stop it and then start again
		::PulseEvent(m_hEventThreadStop);
		::WaitForSingleObject(m_pThread->m_hThread, INFINITE);
		}
	m_pThread = AfxBeginThread(_ThreadProc, pInfo);
	SetThreadPriority(m_pThread,THREAD_PRIORITY_ABOVE_NORMAL /*TREAD_PRIORITY_HIGHEST*/);

	return TRUE;
	}

UINT CDSoundPlay::_ThreadProc(PVOID pParam) {

	TRACE("- CDSoundPlay::_ThreadProc: start...\n");

	PINFO pInfo = reinterpret_cast<PINFO> (pParam);

	while(TRUE) {
		DWORD dwReason = ::WaitForMultipleObjects(pInfo->nSize + 1, pInfo->hEvents, FALSE, INFINITE);
		if(WAIT_FAILED == dwReason)	{ // Some error (you may do something reasonable in here)
			break;
			}
		if(WAIT_OBJECT_0 + pInfo->nSize == dwReason) { // Stop
			pInfo->pFunc((void*)pInfo->pFuncParm,-2,0);
			break;
			}
		if(dwReason==WAIT_OBJECT_0 -1 + pInfo->nSize) {
			pInfo->pFunc((void*)pInfo->pFuncParm, DSBPN_OFFSETSTOP,0);
			break;
			}
		else {
			for(SIZE_T j=0; j < pInfo->nSize; j++)	{
				if(WAIT_OBJECT_0 + j == dwReason) {
					ASSERT(pInfo->pFunc);
					pInfo->pFunc((void*)pInfo->pFuncParm, /*pInfo->dwOffsets[*/ j+1,0);
					break;
					}
				}
			}
		}

	for(SIZE_T j=0; j < pInfo->nSize + 1; j++)
		::CloseHandle(pInfo->hEvents[j]);

	delete []pInfo->dwOffsets;
	delete []pInfo->hEvents;
	delete pInfo;
	TRACE("- CDSoundPlay::_ThreadProc: exit.\n");
	return 0;
	}




//-----------------------------------------------------------------
// CMMChunk Class - Multimedia RIFF Chunk Object
//-----------------------------------------------------------------
class CMMChunk : public MMCKINFO {
protected:
	CMMChunk() { };
};

//-----------------------------------------------------------------
// CMMIdChunk Class - Multimedia RIFF Id Chunk Object
//-----------------------------------------------------------------
class CMMIdChunk : public CMMChunk {
public:
	CMMIdChunk(char c0, char c1, char c2, char c3);
	CMMIdChunk(LPCSTR psz, UINT uiFlags = 0u);
};

//-----------------------------------------------------------------
// CMMTypeChunk Class - Multimedia RIFF Type Chunk Object
//-----------------------------------------------------------------
class CMMTypeChunk : public CMMChunk {
public:
	CMMTypeChunk(char c0, char c1, char c2, char c3);
	CMMTypeChunk(LPCSTR psz, UINT uiFlags = 0u);
};

//-----------------------------------------------------------------
// CMMIOInfo Class - Multimedia RIFF I/O Info Object
//-----------------------------------------------------------------
class CMMIOInfo : public MMIOINFO {
public:
	CMMIOInfo();
};

//-----------------------------------------------------------------
// CMMMemoryIOInfo Class - Multimedia RIFF Memory I/O Info Object
//-----------------------------------------------------------------
class CMMMemoryIOInfo : public CMMIOInfo {
public:
	CMMMemoryIOInfo(LONG lBuffer, DWORD dwMinExpansion = 0);
	CMMMemoryIOInfo(HPSTR pBuffer, LONG lBuffer, DWORD dwMinExpansion = 0);
};

//-----------------------------------------------------------------
// CMMIO Class - Multimedia RIFF I/O Object
//-----------------------------------------------------------------
class CMMIO : public CObject {
public:
	CMMIO();
	CMMIO(HMMIO hmmio);
	CMMIO(const char* pszFileName, DWORD dwOpenFlag = MMIO_READ);
	CMMIO(CMMMemoryIOInfo& mmioinfo);

  // Public Methods
public:
	void      Open(const char* pszFileName, DWORD dwOpenFlags =
			  MMIO_READ);
	void      Open(CMMMemoryIOInfo &mmioinfo);
	MMRESULT  Close(UINT uiFlags = 0u);

	MMRESULT  Ascend(CMMChunk &mmckInfo, UINT uiFlags = 0u);
	MMRESULT  Descend(CMMChunk &mmckInfo, UINT uiFlags = 0u);
	MMRESULT  Descend(CMMChunk &mmckInfo, CMMChunk &mmckParent, UINT uiFlags = 0u);

	LONG      Read(HPSTR pData, LONG lLen);
	LONG      Write(const char* pData, LONG lLen);
	LONG      Seek(LONG lOffset, int iOrigin);

	LRESULT   SendMessage(UINT uiMsg, LPARAM lParam1, LPARAM lParam2);
	MMRESULT  SetBuffer(LPSTR pBuffer, LONG lBuffer, UINT uiFlags = 0u);

	MMRESULT  GetInfo(CMMIOInfo &, UINT uiFlags = 0);
	MMRESULT  SetInfo(CMMIOInfo &, UINT uiFlags = 0);
	MMRESULT  Advance(CMMIOInfo &, UINT uiFlags);

	// Public Data
public:
	HMMIO m_hmmio;
};


//-----------------------------------------------------------------
// CMMIdChunk Inline Public Constructor(s)/Destructor
//-----------------------------------------------------------------
inline CMMIdChunk::CMMIdChunk(char c0, char c1, char c2, char c3) {
	ckid = mmioFOURCC(c0, c1, c2, c3);
}

inline CMMIdChunk::CMMIdChunk(LPCSTR psz, UINT uiFlags) {
	ckid = ::mmioStringToFOURCC(psz, uiFlags);
}

//-----------------------------------------------------------------
// CMMTypeChunk Inline Public Constructor(s)/Destructor
//-----------------------------------------------------------------
inline CMMTypeChunk::CMMTypeChunk(char c0, char c1, char c2, char c3) {
	fccType = mmioFOURCC(c0, c1, c2, c3);
}

inline CMMTypeChunk::CMMTypeChunk(LPCSTR psz, UINT uiFlags) {
	fccType = ::mmioStringToFOURCC(psz, uiFlags);
}

//-----------------------------------------------------------------
// CMMIOInfo Inline Public Constructor(s)/Destructor
//-----------------------------------------------------------------
inline CMMIOInfo::CMMIOInfo() {
	::ZeroMemory(this, sizeof(MMIOINFO));
	}

//-----------------------------------------------------------------
// CMMMemoryIOInfo Inline Public Constructor(s)/Destructor
//-----------------------------------------------------------------
inline CMMMemoryIOInfo::CMMMemoryIOInfo(LONG lBuffer, DWORD dwMinExpansion) {

	pIOProc = NULL;
	fccIOProc = FOURCC_MEM;
	pchBuffer = NULL;
	cchBuffer = lBuffer;
	adwInfo[0] = dwMinExpansion;
	}

inline CMMMemoryIOInfo::CMMMemoryIOInfo(HPSTR pBuffer, LONG cchBuf, DWORD dwMinExpansion) {
	pIOProc = NULL;
	fccIOProc = FOURCC_MEM;
	pchBuffer = pBuffer;
	cchBuffer = cchBuf;
	adwInfo[0] = dwMinExpansion;
	}

//-----------------------------------------------------------------
// CMMIO Inline Public Constructor(s)/Destructor
//-----------------------------------------------------------------
inline CMMIO::CMMIO() : m_hmmio(NULL) {
	}

inline CMMIO::CMMIO(HMMIO hmmio) : m_hmmio(hmmio) {
	}

inline CMMIO::CMMIO(const char* pszFileName, DWORD dwOpenFlag) {
	Open(pszFileName, dwOpenFlag);
	}

inline CMMIO::CMMIO(CMMMemoryIOInfo &mmioinfo) {
	Open(mmioinfo);
	}

//-----------------------------------------------------------------
// CMMIO Inline Public Methods
//-----------------------------------------------------------------
inline MMRESULT CMMIO::Close(UINT uiFlags) {
	ASSERT(m_hmmio != NULL);
	MMRESULT mmr = ::mmioClose(m_hmmio, uiFlags);  
	m_hmmio = NULL;
	return mmr;
	}

inline LONG CMMIO::Read(HPSTR pData, LONG lLen) {
	ASSERT(m_hmmio != NULL);
	return ::mmioRead(m_hmmio, pData, lLen);
	}

inline MMRESULT CMMIO::Ascend(CMMChunk &mmckInfo, UINT uiFlags) {
	ASSERT(m_hmmio != NULL);
	return ::mmioAscend(m_hmmio, &mmckInfo, uiFlags);
	}

inline MMRESULT CMMIO::Descend(CMMChunk &mmckInfo, UINT uiFlags) {
	ASSERT(m_hmmio != NULL);
	return ::mmioDescend(m_hmmio, &mmckInfo, 0, uiFlags);
	}

inline MMRESULT CMMIO::Descend(CMMChunk &mmckInfo, CMMChunk &mmckParent, UINT uiFlags) {
	ASSERT(m_hmmio != NULL);
	return ::mmioDescend(m_hmmio, &mmckInfo, &mmckParent, uiFlags);
	}

inline LONG CMMIO::Seek(LONG lOffset, int iOrigin) {
	ASSERT(m_hmmio != NULL);
	return ::mmioSeek(m_hmmio, lOffset, iOrigin);
	}

inline LRESULT CMMIO::SendMessage(UINT uiMsg, LPARAM lParam1, LPARAM lParam2) {
	ASSERT(m_hmmio != NULL);
	return ::mmioSendMessage(m_hmmio, uiMsg, lParam1, lParam2);
	}

inline MMRESULT CMMIO::SetBuffer(LPSTR pBuffer, LONG lBuffer, UINT uiFlags) {
	ASSERT(m_hmmio != NULL);
	return ::mmioSetBuffer(m_hmmio, pBuffer, lBuffer, uiFlags);  
	}

inline LONG CMMIO::Write(const char* pData, LONG lLen) {
	ASSERT(m_hmmio != NULL);
	return ::mmioWrite(m_hmmio, pData, lLen);
	}

inline MMRESULT CMMIO::GetInfo(CMMIOInfo &Info, UINT uiFlags) {
	ASSERT(m_hmmio != NULL);
	return ::mmioGetInfo(m_hmmio, &Info, uiFlags);
	}

inline MMRESULT CMMIO::SetInfo(CMMIOInfo &Info, UINT uiFlags) {
	ASSERT(m_hmmio != NULL);
	return ::mmioSetInfo(m_hmmio, &Info, uiFlags);
	}

inline MMRESULT CMMIO::Advance(CMMIOInfo &Info, UINT uiFlags) {
	ASSERT(m_hmmio != NULL);
	return ::mmioAdvance(m_hmmio, &Info, uiFlags);
	}

void CMMIO::Open(const char* pszFileName, DWORD dwOpenFlags) {

	ASSERT(AfxIsValidString(pszFileName));
	m_hmmio = ::mmioOpen((char*)pszFileName, NULL, dwOpenFlags);
	}

void CMMIO::Open(CMMMemoryIOInfo &mmioinfo) {

	m_hmmio = ::mmioOpen(NULL, &mmioinfo, MMIO_READWRITE);
	}



CWave::CWave() : m_dwImageLen(0),m_bResource(FALSE),m_pImageData(NULL) {

	}

CWave::CWave(LPCTSTR lpszFileName) : m_dwImageLen(0),m_bResource(FALSE),m_pImageData(NULL) {

	Create(lpszFileName);
	}

CWave::CWave(UINT uiResID, HMODULE hmod) : m_dwImageLen(0),m_bResource(TRUE),m_pImageData(NULL) {

	Create(uiResID, hmod);
	}

CWave::~CWave() {

	Free();
	}

BOOL CWave::Create(LPCTSTR lpszFileName) {

	// Free any previous wave image data
	Free();

	// Flag as regular memory
	m_bResource = FALSE;

	// Open the wave file
	CFile fileWave;
	if(!fileWave.Open(lpszFileName, CFile::modeRead | CFile::shareDenyNone))
		return FALSE;

	// Get the file length
	m_dwImageLen = fileWave.GetLength();

	// Allocate and lock memory for the image data
	m_pImageData = (BYTE*)::GlobalLock(::GlobalAlloc(GMEM_MOVEABLE |
		GMEM_SHARE, m_dwImageLen));
	if(!m_pImageData)
		return FALSE;

	// Read the image data from the file
	fileWave.Read(m_pImageData, m_dwImageLen);

	return TRUE;
	}

BOOL CWave::Create(UINT uiResID, HMODULE hmod) {

	// Free any previous wave image data
	Free();

	// Flag as resource memory
	m_bResource = TRUE;

	// Find the wave resource
	HRSRC hresInfo;
	hresInfo = ::FindResource(hmod, MAKEINTRESOURCE(uiResID), "WAVE");
	if(!hresInfo)
		return FALSE;

	// Load the wave resource
	HGLOBAL hgmemWave = ::LoadResource(hmod, hresInfo);

	if(hgmemWave) {
		// Get pointer to and length of the wave image data
		m_pImageData= (BYTE*)::LockResource(hgmemWave);
		m_dwImageLen = ::SizeofResource(hmod, hresInfo);
		}

	return (m_pImageData ? TRUE : FALSE);
	}

BOOL CWave::Play(BOOL bAsync, BOOL bLooped) const {
	// Check validity
	if(!IsValid())
		return FALSE;

	// Play the wave
	return ::PlaySound((LPCSTR)m_pImageData, NULL, SND_MEMORY |	SND_NODEFAULT |
		(bAsync ? SND_ASYNC : SND_SYNC) | (bLooped ? (SND_LOOP | SND_ASYNC) : 0));
	}

BOOL CWave::GetFormat(WAVEFORMATEX& wfFormat) const {

	// Check validity
	if(!IsValid())
		return FALSE;

	// Setup and open the MMINFO structure
	CMMMemoryIOInfo mmioInfo((HPSTR)m_pImageData, m_dwImageLen);
	CMMIO mmio(mmioInfo);

	// Find the WAVE chunk
	CMMTypeChunk mmckParent('W','A','V','E');
	mmio.Descend(mmckParent, MMIO_FINDRIFF);

	// Find and read the format subchunk
	CMMIdChunk mmckSubchunk('f','m','t',' ');
	mmio.Descend(mmckSubchunk, mmckParent, MMIO_FINDCHUNK);
	mmio.Read((HPSTR)&wfFormat, sizeof(WAVEFORMATEX));
	wfFormat.cbSize=sizeof(wfFormat);
	mmio.Ascend(mmckSubchunk);

	return TRUE;
	}

DWORD CWave::GetDataLen() const {

	// Check validity
	if(!IsValid())
		return 0;

	// Setup and open the MMINFO structure
	CMMMemoryIOInfo mmioInfo((HPSTR)m_pImageData, m_dwImageLen);
	CMMIO mmio(mmioInfo);

	// Find the WAVE chunk
	CMMTypeChunk mmckParent('W','A','V','E');
	mmio.Descend(mmckParent, MMIO_FINDRIFF);

	// Find and get the size of the data subchunk
	CMMIdChunk mmckSubchunk('d','a','t','a');
	mmio.Descend(mmckSubchunk, mmckParent, MMIO_FINDCHUNK);
	return mmckSubchunk.cksize;
	}

DWORD CWave::GetSeconds() const {
	DWORD n;
	WAVEFORMATEX wf;

	if(n=GetDataLen()) {
		GetFormat(wf);
		return GetSeconds(n,&wf);
//		return wf.nAvgBytesPerSec ? n/wf.nAvgBytesPerSec : n/(wf.nSamplesPerSec*wf.nChannels*wf.wBitsPerSample/8);
		;
		}
	else 
		return 0;
	}

DWORD CWave::GetSeconds(DWORD n,WAVEFORMATEX *wf) {
	DWORD n2;

	if(wf->nAvgBytesPerSec)
		// safety :)
		n2=((double)n/wf->nAvgBytesPerSec)+0.9;		// diciamo
	else
		n2=((double)n/(wf->nSamplesPerSec*wf->nChannels*wf->wBitsPerSample/8))+0.9;		// diciamo
	return n2;
	}

DWORD CWave::GetData(BYTE*& pWaveData, DWORD dwMaxLen) const {

	// Check validity
	if(!IsValid())
		return 0;

	// Setup and open the MMINFO structure
	CMMMemoryIOInfo mmioInfo((HPSTR)m_pImageData, m_dwImageLen);
	CMMIO mmio(mmioInfo);

	// Find the WAVE chunk
	CMMTypeChunk mmckParent('W','A','V','E');
	mmio.Descend(mmckParent, MMIO_FINDRIFF);

	// Find and get the size of the data subchunk
	CMMIdChunk mmckSubchunk('d','a','t','a');
	mmio.Descend(mmckSubchunk, mmckParent, MMIO_FINDCHUNK);
	DWORD dwLenToCopy = mmckSubchunk.cksize;

	// Allocate memory if the passed in pWaveData was NULL
	if(!pWaveData)
		pWaveData = (BYTE*)::GlobalLock(::GlobalAlloc(GMEM_MOVEABLE, dwLenToCopy));
	else
		// If we didn't allocate our own memory, honor dwMaxLen
		if(dwMaxLen < dwLenToCopy)
			dwLenToCopy = dwMaxLen;
	if(pWaveData)
		// Read waveform data into the buffer
		mmio.Read((HPSTR)pWaveData, dwLenToCopy);

	return dwLenToCopy;
	}

// Protected
BOOL CWave::Free() {

	// Free any previous wave data
	if(m_pImageData)	{
//#ifndef WIN32 perch???
		HGLOBAL  hgmemWave = ::GlobalHandle(m_pImageData);

		if(hgmemWave) {
			if(m_bResource)
				// Free resource (Win95 does NOT automatically do this)
				::FreeResource(hgmemWave);
			else {
				// Unlock and free memory
				::GlobalUnlock(hgmemWave);
				::GlobalFree(hgmemWave);
				}
			m_pImageData = NULL;
			m_dwImageLen = 0;
			return TRUE;
			}
//#endif // WIN32
		m_pImageData = NULL;
		m_dwImageLen = 0;
		}

	return FALSE;
	}



//https://github.com/xiezhaoHi/test_chat/blob/master/WaveIn.cpp
/////////////////////////////////////////////////////////////////////////////
// CWaveIn per usare Thread come callback

IMPLEMENT_DYNCREATE(CWaveIn, CWinThread)

CWaveIn::CWaveIn(CWnd *w) {
	m_oneExe = TRUE;
	m_hwi=(HWAVEIN)-1;
	}

CWaveIn::CWaveIn(CWnd *w,WAVEFORMATEX *wf) {

	m_wfx=*wf;
	m_oneExe = TRUE;
	m_hwi=(HWAVEIN)-1;
	}

CWaveIn::~CWaveIn() {

	if(m_buf1) {
		delete [] m_buf1;
		m_buf1 = NULL;
		}
	if(m_buf2)	{
		delete [] m_buf2;
		m_buf2 = NULL;
		}
	}

BOOL CWaveIn::InitInstance() {

	m_dwThread = ::GetCurrentThreadId();
	SetThreadPriority(THREAD_PRIORITY_ABOVE_NORMAL /*THREAD_PRIORITY_HIGHEST*/);

	StartRecord();
	return TRUE;
	}

int CWaveIn::ExitInstance() {
	// TODO:  perform any per-thread cleanup here
	return CWinThread::ExitInstance();
	}

BEGIN_MESSAGE_MAP(CWaveIn, CWinThread)
	//{{AFX_MSG_MAP(CWaveIn)
		// NOTE - the ClassWizard will add and remove mapping macros here.
	ON_THREAD_MESSAGE(MM_WIM_OPEN, On_WIM_OPEN)
	ON_THREAD_MESSAGE(MM_WIM_DATA, On_WIM_DATA)
	ON_THREAD_MESSAGE(MM_WIM_CLOSE, On_WIM_CLOSE)
	ON_THREAD_MESSAGE(WM_WAVEINEND, OnWaveInEnd)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CWaveIn message handlers

void CWaveIn::On_WIM_OPEN(UINT wParam, LONG lParam) {
	m_bRecording = TRUE;

	//******************************************************
	//******************************************************
	}

//static int t = 0;
void CWaveIn::On_WIM_DATA(UINT wParam, LONG lParam) {
	LPWAVEHDR lpWHdr = (LPWAVEHDR)lParam;
	CVidsendView22 *v=(CVidsendView22*)tag;
	CVidsendDoc22 *doc=v->GetDocument();
	static DWORD ptr1=0,ptr2=0;
	BYTE *soundSampleEnd=NULL;
	static BYTE which=0;
	char *p;
	BYTE *pSBuf,*pSBuf2;
	struct AV_PACKET_HDR *avh=NULL;
//  struct AV_PACKET_HDR avh;
	ACMSTREAMHEADER hhacm;
	int i,vol=0;
	BYTE preascoltoBuffer[44100L*2*2/AUDIO_BUFFER_DIVIDER];
	BYTE soundSampleUsed=0;


//	char buff[BUF_LEN] = { 0 };
//	memset(buff, 0, ccpwh->dwBytesRecorded);


	//******************************************************
	if (m_bRecording)	{
		ResetEvent(v->busyAudio3);

		waveInUnprepareHeader(m_hwi,lpWHdr,sizeof(WAVEHDR));

		if(theApp.theServer2) {

//			v=theApp.theServer2->getView();
//			if(theApp.theServer2->Opzioni & CVidsendApp::canSendAudio /*theApp.theServer2->getAudio()*/ && !theApp.theServer2->bPaused) {
#pragma warning FINIRE canSendAudio

			pSBuf=(BYTE *)HeapAlloc(GetProcessHeap(),HEAP_GENERATE_EXCEPTIONS,v->maxWaveoutSize+AV_PACKET_HDR_SIZE  +100);
			pSBuf2=pSBuf+AV_PACKET_HDR_SIZE;
			avh=(struct AV_PACKET_HDR *)pSBuf;

#ifdef _STANDALONE_MODE
			avh->tag=MAKEFOURCC('D','G','2','0');
#else
			avh->tag=MAKEFOURCC('G','D','2','0');
#endif
			avh->type=AV_PACKET_TYPE_AUDIO;
//				avh.psec=1000;
			avh->timestamp=*((DWORD *)&CTime::GetCurrentTime());
			avh->info=0;


			if(doc->Opzioni & (CVidsendDoc22::maySendAudio | CVidsendDoc22::sendAudioMP3)) {
			switch(doc->trasmMode) {
				case 0:
					if(doc->alternaSource) {

						}
	// dovrebbe esserci un modo di leggere il peak da MP3... replaygain o boh...?
	//gfc->ov_rpg.RadioGain;

					if(v->GetDocument()->Opzioni2 & CVidsendDoc22::preascoltoMono) {
						if(v->buttonPre4->GetState() & BST_CHECKED) {
							memcpy(preascoltoBuffer,lpWHdr->lpData,lpWHdr->dwBytesRecorded /*44100L*2*2/AUDIO_BUFFER_DIVIDER*/);
							}
						else {
							ZeroMemory(preascoltoBuffer,lpWHdr->dwBytesRecorded /*44100L*2*2/AUDIO_BUFFER_DIVIDER*/);
							}

						if(v->m_MP3player[0] && (v->buttonPre1->GetState() & BST_CHECKED)) {
							if(v->m_MP3player[0]->getAudioIF()->channels==1)		// 
								theApp.mixWaves1Mono((const short int *)(v->m_MP3player[0]->getBuffer()+ptr1),
									0,lpWHdr->dwBytesRecorded/2,(short int *)preascoltoBuffer,lpWHdr->dwBytesRecorded/2);
							else
								theApp.mixWaves((const short int *)(v->m_MP3player[0]->getBuffer()+ptr1),
									0,lpWHdr->dwBytesRecorded/2,(short int *)preascoltoBuffer,lpWHdr->dwBytesRecorded/2);
							}
						if(v->m_MP3player[1] && (v->buttonPre2->GetState() & BST_CHECKED)) {
							if(v->m_MP3player[1]->getAudioIF()->channels==1)		// 
								theApp.mixWaves1Mono((const short int *)(v->m_MP3player[1]->getBuffer()+ptr2),
									0,lpWHdr->dwBytesRecorded/2,(short int *)preascoltoBuffer,lpWHdr->dwBytesRecorded/2);
							else
								theApp.mixWaves((const short int *)(v->m_MP3player[1]->getBuffer()+ptr2),
									0,lpWHdr->dwBytesRecorded/2,(short int *)preascoltoBuffer,lpWHdr->dwBytesRecorded/2);
							}
						if(v->buttonPre3->GetState() & BST_CHECKED) {
							if(v->soundSample) {
								if(!v->soundSampleStart) {
									v->soundSampleStart=soundSampleEnd=v->soundSample;
									which=0;
									}
								else
									soundSampleEnd=v->soundSampleStart;
								theApp.mixWaves((signed char const * *)&soundSampleEnd,&v->soundWf,
									0,lpWHdr->dwBytesRecorded,(short int *)preascoltoBuffer,
									/*v->soundWf.wf.nAvgBytesPerSec/AUDIO_BUFFER_DIVIDER*/
									lpWHdr->dwBytesRecorded/2);
								}
							}
						}
					else {
						}

					if(v->buttonM3->GetState() & BST_CHECKED) {
						ZeroMemory(lpWHdr->lpData,lpWHdr->dwBytesRecorded);
						i=0;
						}
					// [manca volume input.. fare da HW/directx?? NON SI RIESCE windows 7+ v. mixer]
					else
						i=theApp.volWaves((const short int*)lpWHdr->lpData,NULL,lpWHdr->dwBytesRecorded/2,v->volume3->GetPos());
					//i=v->measureAudio((short int *)lpWHdr->lpData,lpWHdr->dwBytesRecorded/2);
					v->VUMeter3->SetWindowText(sqrt(i)/2);

					if(v->m_MP3player[0] && !(v->buttonM1->GetState() & BST_CHECKED)) {
						// usare getBufferSafePos(...

						i=v->m_MP3player[0]->getBufferPos();
						if(v->m_MP3player[0]->getBuffer()) {		// semaforo...non dovrebbe + servire
							if(v->m_MP3player[0]->getAudioIF()->channels==1)		// naturalmente andrebbe gestito anche sample rate...
								vol=theApp.mixWaves1Mono((const short int *)(v->m_MP3player[0]->getBuffer()+ptr1),
									0,lpWHdr->dwBytesRecorded/2/2,(short int *)lpWHdr->lpData,lpWHdr->dwBytesRecorded/2,
									0,v->getVolumeFader(0)*100);   // 
							else
								vol=theApp.mixWaves((const short int *)(v->m_MP3player[0]->getBuffer()+ptr1),
									0,lpWHdr->dwBytesRecorded/2,(short int *)lpWHdr->lpData,lpWHdr->dwBytesRecorded/2,
									0,v->getVolumeFader(0)*100);   // 
							}
						}
					if(v->m_MP3player[1] && !(v->buttonM2->GetState() & BST_CHECKED)) {
						i=v->m_MP3player[1]->getBufferPos();
						if(v->m_MP3player[1]->getBuffer()) {		// semaforo... non dovrebbe + servire
						// usare getBufferSafePos(...

							if(v->m_MP3player[1]->getAudioIF()->channels==1)		// naturalmente andrebbe gestito anche sample rate...
								vol=theApp.mixWaves1Mono((const short int *)(v->m_MP3player[1]->getBuffer()+ptr2),
									0,lpWHdr->dwBytesRecorded/2,(short int *)lpWHdr->lpData,lpWHdr->dwBytesRecorded/2,
									0,v->getVolumeFader(1)*100);   // 
							else
								vol=theApp.mixWaves((const short int *)(v->m_MP3player[1]->getBuffer()+ptr2),
									0,lpWHdr->dwBytesRecorded/2,(short int *)lpWHdr->lpData,lpWHdr->dwBytesRecorded/2,
									0,v->getVolumeFader(1)*100);   // 
	//									theApp.mixWaves((const short int *)(i<44100L*2*2/2 ? v->m_MP3player[1]->getBuffer() : v->m_MP3player[1]->getBuffer()+44100L*2*2/2),
	//										0,lpWHdr->dwBytesRecorded/2,(short int *)lpWHdr->lpData,lpWHdr->dwBytesRecorded/2,
	//										0,v->getVolumeFader(1)*100);   // 
							}
						}
					if(v->m_MP3player[0]) {
						if(v->m_MP3player[0]->getAudioIF()->channels==1) {
							ptr1+=44100L*1*2/AUDIO_BUFFER_DIVIDER;
							ptr1 %= 44100L*2*2;
							}
						else {
							ptr1+=44100L*2*2/AUDIO_BUFFER_DIVIDER;
							ptr1 %= 44100L*2*2;
							}
						}
					if(v->m_MP3player[1]) {
						if(v->m_MP3player[1]->getAudioIF()->channels==1) {
							ptr2+=44100L*1*2/AUDIO_BUFFER_DIVIDER;
							ptr2 %= 44100L*2*2;
							}
						else {
							ptr2+=44100L*2*2/AUDIO_BUFFER_DIVIDER;
							ptr2 %= 44100L*2*2;
							}
						}

					if(v->soundSample) {
						if(!v->soundSampleStart) {
							v->soundSampleStart=soundSampleEnd=v->soundSample;
							which=0;
							}
						else {
							if(!soundSampleEnd)
								soundSampleEnd=v->soundSampleStart;
							}

						if(!(v->buttonM4->GetState() & BST_CHECKED)) {
							theApp.mixWaves((signed char const * *)&soundSampleEnd,&v->soundWf,
								0,lpWHdr->dwBytesRecorded,(short int *)lpWHdr->lpData,
								lpWHdr->dwBytesRecorded/2
								/*v->soundWf.wf.nAvgBytesPerSec/AUDIO_BUFFER_DIVIDER*/,0,v->volume4->GetPos());
							}
						v->soundSampleStart=soundSampleEnd;
						if(soundSampleEnd-v->soundSample >= v->soundLength   -  v->soundWf.wf.nAvgBytesPerSec/AUDIO_BUFFER_DIVIDER  ) {
							v->soundLength=0;
							v->soundSampleStart=soundSampleEnd=NULL;
							v->playerPreascolto->Stop();
							delete v->soundSample;
							v->soundSample=NULL;
							}
						}

					if(v->GetDocument()->Opzioni & CVidsendDoc22::sstereo) {
						vol=theApp.makeSStereo((short int *)lpWHdr->lpData,lpWHdr->dwBytesRecorded/2);   // 
						}
					if(v->GetDocument()->Opzioni & CVidsendDoc22::mono) {
						vol=theApp.makeMono((short int *)lpWHdr->lpData,lpWHdr->dwBytesRecorded/2);   // 
						}
					// [quando si fa il mix in tempo reale, nel mentre si possono calcolare tutti i vumeter qua...]

					if(v->GetDocument()->Opzioni2 & CVidsendDoc22::preascoltoMono) {

						theApp.make2MonoFrom2Stereo((short int *)preascoltoBuffer,
							(const short int *)lpWHdr->lpData,lpWHdr->dwBytesRecorded/2);   // 

						if(v->playerPreascolto) {
							if(which >= AUDIO_BUFFER_DIVIDER) {
								if(WaitForSingleObject(v->playerPreascolto->m_pReadEvent[which % AUDIO_BUFFER_DIVIDER],/*500*/ INFINITE) == WAIT_TIMEOUT)
									;
								}

							v->playerPreascolto->Write(((which) % AUDIO_BUFFER_DIVIDER)*44100L*2*2/AUDIO_BUFFER_DIVIDER,
								(BYTE *)preascoltoBuffer, 44100L*2*2/AUDIO_BUFFER_DIVIDER);

							if(!which) {
								v->playerPreascolto->SetPosition(0);
								v->playerPreascolto->Play(1); // (loop) playing
								}

							which++;
							}
						}

					if(!vol)
						vol=v->measureAudio((short int *)lpWHdr->lpData,lpWHdr->dwBytesRecorded/2);
					else
						vol=sqrt(vol);		// così non la faccio sopra n volte :)
					v->VUMeter5->SetWindowText(vol/2);

					break;

				case 2:
					{
						static BYTE divider2=0;
						BYTE myWAVbuf[44100L*2*2/AUDIO_BUFFER_DIVIDER];

						divider2++;
						if(!(doc->pagProva.audioOpzioni & 1) || (divider2 & 1)) {
							switch(doc->pagProva.tipoAudio) {
								case 0:
									i=100;
									break;
								case 1:
									i=440;
									break;
								case 2:
									i=1000;
									break;
								case 3:
									i=5000;
									break;
								case 4:
									i=10000;
									break;
								}
							if(doc->Opzioni & CVidsendDoc22::sstereo) {
								}
							if(doc->Opzioni & CVidsendDoc22::mono) {
								}

							DWORD l;
							theApp.createTestWave(myWAVbuf,&v->wfex,&l,i,doc->pagProva.audioOpzioni & 2,
								20,!(doc->Opzioni & CVidsendDoc22::mono));
				//			l+=sizeof(WAVEFORMATEX);

							// [FINIRE la gestione di  / AUDIO_BUFFER_DIVIDER, servono 2 timer o boh]
							i=v->measureAudio((short int *)myWAVbuf,l/2);
							v->VUMeter5->SetWindowText(i/2);

							memcpy(lpWHdr->lpData,myWAVbuf,lpWHdr->dwBytesRecorded /*44100L*2*2/AUDIO_BUFFER_DIVIDER*/);

							}
						}
					break;

				case 1:
					if(doc->psAudio) {
						DWORD l;
						static BYTE isMono=0;
						BYTE myWAVbuf[44100L*2*2/AUDIO_BUFFER_DIVIDER];
						if(doc->nomeMP3_PB.FindNoCase(".mp3") != -1) {
							l=44100L*2*2/AUDIO_BUFFER_DIVIDER;
							if(!v->PBMP3bufferSize) {
								CJoshuaMP3 myMP3((DWORD)0);
								if(myMP3.Suona(doc->psAudio, v->PBMP3buffer, v->PBMP3from, ++v->PBMP3to,0,0,0.7)>0) {
									v->PBMP3from=v->PBMP3to;
									isMono=myMP3.getParameters().forceMono /*myMP3.getAudioIF()->channels==1*/;
									if(isMono)
										l /= 2;
									v->PBMP3bufferPointer=0 /*myMP3.getBuffer()*/;
									v->PBMP3bufferSize=myMP3.getBufferPos();

									if(v->PBMP3bufferSize<(isMono ? /*l/2*/ l : l)) {		// non è perfetto ma non importa!
										ASSERT(0);
										l=0;		// beh facciam così
										}
									memcpy(myWAVbuf,v->PBMP3buffer+v->PBMP3bufferPointer,l);
									v->PBMP3bufferPointer+=l;
									}
								else
									l=0;			// fine brano..
								}
							else if(v->PBMP3bufferPointer+l >= v->PBMP3bufferSize) {
								CJoshuaMP3 myMP3((DWORD)0);
								memcpy(myWAVbuf,v->PBMP3buffer+v->PBMP3bufferPointer,
									v->PBMP3bufferSize-v->PBMP3bufferPointer);
								v->PBMP3bufferPointer += l;
								v->PBMP3bufferPointer %= v->PBMP3bufferSize;

								if(myMP3.Suona(doc->psAudio, v->PBMP3buffer, v->PBMP3from, ++v->PBMP3to,0,0,0.7)>0) {
									v->PBMP3from=v->PBMP3to;
									isMono=myMP3.getParameters().forceMono /*myMP3.getAudioIF()->channels==1*/;
									if(isMono)
										l /= 2;
									memcpy(myWAVbuf+l-v->PBMP3bufferPointer,v->PBMP3buffer,v->PBMP3bufferPointer);
									v->PBMP3bufferSize=myMP3.getBufferPos();
									if(v->PBMP3bufferSize<(isMono ? /*l/2*/ l : l)) {		// non è perfetto ma non importa!
										ASSERT(0);
										l=0;		// beh facciam così
										}
									}
								else
									l=0;			// fine brano..

								}
							else {
								if(isMono)
									l /= 2;
								memcpy(myWAVbuf,v->PBMP3buffer+v->PBMP3bufferPointer,l);
								v->PBMP3bufferPointer+=l;
								}
							}
						else {
							l=0; // fare volendo...  myMP3.suonaWAV()
							}

						if(l) {
							if(isMono) {
								int k;
								short int *p=(short int *)lpWHdr->lpData,*p2=(short int *)myWAVbuf;
								for(k=0; k<l/2; k++) {
									*p++=*p2;
									*p++=*p2++;
									}
								}
							else
								memcpy(lpWHdr->lpData,myWAVbuf,l);
							i=v->measureAudio((short int *)myWAVbuf,l/2);
							v->VUMeter5->SetWindowText(i/2);

							v->gdaFrameNum++;
							}
						else {
							v->gdaFrameNum=0;
							if(doc->OpzioniSorgenteVideo & CVidsendDoc22::mp3Loop) {
								doc->psAudio->Seek(0,CFile::begin);
								v->PBMP3from=v->PBMP3to=0;
								v->PBMP3bufferSize=v->PBMP3bufferPointer=0;
								}
							else
								doc->trasmMode=2;
							}
						}		// psAudio
					else {
						ZeroMemory(lpWHdr->lpData,lpWHdr->dwBytesRecorded);		// o mettere un rumore bianco :)
						i=v->measureAudio((short int *)lpWHdr->lpData,lpWHdr->dwBytesRecorded/2);
						v->VUMeter5->SetWindowText(i/2);
						}
					break;

				}		// switch trasmMode
				}	// maySendAudio ecc.


			if(!theApp.theServer2->bPaused) {
				v->SendMessage(WM_RAWAUDIOFRAME_READY,(WPARAM)lpWHdr->lpData,(LPARAM)lpWHdr->dwBytesRecorded);
// OCCHIO! serve Send o il buffer viene cimito 
				// [bPaused gestito in socket qua, per avere VUmeter e registrazione locale!] no!

				if(v->m_hAcm) {
					hhacm.cbStruct=sizeof(ACMSTREAMHEADER);
					hhacm.fdwStatus=0;
					hhacm.dwUser=(DWORD)0 /*this*/;
					hhacm.pbSrc=(BYTE *)lpWHdr->lpData;
					hhacm.cbSrcLength=lpWHdr->dwBytesRecorded;
			//			hhacm.cbSrcLengthUsed=0;
					hhacm.dwSrcUser=0;
					hhacm.pbDst=pSBuf2;
					hhacm.cbDstLength=v->maxWaveoutSize;
			//			hhacm.cbDstLengthUsed=0;
					hhacm.dwDstUser=0;
					if(!acmStreamPrepareHeader(v->m_hAcm,&hhacm,0)) {
						i=acmStreamConvert(v->m_hAcm,&hhacm,ACM_STREAMCONVERTF_BLOCKALIGN);
					//	wsprintf(myBuf,"convertito: %d, %d bytes",i,l);
					//	AfxMessageBox(myBuf);
						acmStreamUnprepareHeader(v->m_hAcm,&hhacm,0);

						avh->len=hhacm.cbDstLengthUsed;


		/*				p=(char *)GlobalAlloc(GPTR,1024);
						wsprintf(p,"AFrameT# %ld: lungo %ld (%ld) %ld",v->gdaFrameNum,hhacm.cbDstLengthUsed,lpWHdr->dwBytesRecorded,lpWHdr->dwBufferLength); 
						theApp.m_pMainWnd->PostMessage(WM_UPDATE_PANE,0,(DWORD)p);*/
							avh->reserved1=avh->reserved2=0;
	//						theApp.OnAudioFrameReady((WPARAM)pSBuf,(LPARAM)avh->len+AV_PACKET_HDR_SIZE);
							v->PostMessage(WM_AUDIOFRAME_READY,(WPARAM)pSBuf,(LPARAM)avh->len+AV_PACKET_HDR_SIZE);
	//					if(theApp.theServer2->streamSocketA->Manda(&avh,pSBuf,hhacm.cbDstLengthUsed))
	//						v->aFrameNum;
						}
					}
				else {
					HeapFree(GetProcessHeap(),0,pSBuf);
					}
				}
			else {
				HeapFree(GetProcessHeap(),0,pSBuf);
				}

			}			// theServer2


fine_a:
					;

		lpWHdr->lpData= (char *)(lpWHdr == &m_wh1 ? m_buf1 : m_buf2);
		lpWHdr->dwBufferLength=v->wfex.nAvgBytesPerSec /*176400*/ /*8000*/ / AUDIO_BUFFER_DIVIDER;
#pragma warning AUDIO dwBufferLength2
		lpWHdr->dwBytesRecorded=0;
		lpWHdr->dwUser=(DWORD)v;
		lpWHdr->dwFlags=0;
		lpWHdr->dwLoops=0;
		lpWHdr->lpNext=NULL;
		lpWHdr->reserved=0;
		::waveInPrepareHeader(m_hwi,lpWHdr,sizeof(WAVEHDR));
		//https://www.codeproject.com/Messages/820760/waveInPrepareHeader-increases-handle
		// CONTROLLARE 2023
		DWORD ti=timeGetTime()+500;
		while(!(lpWHdr->dwFlags & WHDR_PREPARED) && timeGetTime()<ti);
		::waveInAddBuffer(m_hwi,lpWHdr,sizeof(WAVEHDR));

		SetEvent(v->busyAudio3);
		}
	}

void CWaveIn::On_WIM_CLOSE(UINT wParam, LONG lParam) {
	//******************************************************

	//******************************************************

	if(m_buf1)	{
		delete[] m_buf1; m_buf1 = NULL;
		}

	if(m_buf2)	{
		delete[] m_buf2; m_buf2 = NULL;
		}
	}

BOOL CWaveIn::StartRecord() {

	// TODO:  perform and per-thread initialization here
	// ?? CD ???????
	CString err = "error";
	m_wfx.wFormatTag = WAVE_FORMAT_PCM;
	m_wfx.nChannels = 2;
	m_wfx.nSamplesPerSec = 44.1*1000;
	m_wfx.wBitsPerSample = 16;
	m_wfx.nBlockAlign = m_wfx.nChannels * (m_wfx.wBitsPerSample/8);
	m_wfx.nAvgBytesPerSec = m_wfx.nSamplesPerSec * m_wfx.nBlockAlign;
	m_wfx.cbSize = 0;

	m_ret = ::waveInOpen(&m_hwi, WAVE_MAPPER, &m_wfx, m_dwThread, tag, CALLBACK_THREAD);
	if (MMSYSERR_NOERROR != m_ret)	{
		err += "::waveInOpen";
		AfxMessageBox(err);
		return FALSE;
		}

	m_buf1 = new BYTE[BUF_LEN];
	m_buf2 = new BYTE[BUF_LEN];

	ZeroMemory(&m_wh1, sizeof(WAVEHDR));
	ZeroMemory(&m_wh2, sizeof(WAVEHDR));

	m_wh1.lpData = (char*)m_buf1;
	m_wh1.dwBufferLength = BUF_LEN;
	m_wh1.dwUser=tag;
	m_wh1.dwLoops = 0 /*1*/;

	m_ret = ::waveInPrepareHeader(m_hwi, &m_wh1, sizeof(WAVEHDR));
	if (MMSYSERR_NOERROR != m_ret)	{
		err += "::waveInPrepareHeader";
		AfxMessageBox(err);
		return FALSE;
		}

	m_wh2.lpData = (char*)m_buf2;
	m_wh2.dwBufferLength = BUF_LEN;
	m_wh2.dwUser=tag;
	m_wh2.dwLoops = 0 /*1*/;
	m_ret = ::waveInPrepareHeader(m_hwi, &m_wh2, sizeof(WAVEHDR));
	if (MMSYSERR_NOERROR != m_ret) {
		err += "::waveInPrepareHeader";
		AfxMessageBox(err);
		return FALSE;
		}

	m_ret = ::waveInAddBuffer(m_hwi, &m_wh1, sizeof(WAVEHDR));
	if (MMSYSERR_NOERROR != m_ret)	{
		err += "waveInAddBuffer";
		AfxMessageBox(err);
		return FALSE;
	}

	m_ret = ::waveInAddBuffer(m_hwi, &m_wh2, sizeof(WAVEHDR));
	if (MMSYSERR_NOERROR != m_ret)	{
		err += "waveInAddBuffer";
		AfxMessageBox(err);
		return FALSE;
		}

	waveInStart(m_hwi);

	return TRUE;
	}

BOOL CWaveIn::StopRecord() {

	m_bRecording = FALSE;

	if(m_hwi != (HWAVEIN)-1) {
		m_ret = ::waveInStop(m_hwi);
		if (MMSYSERR_NOERROR != m_ret)	{
			CString err = "waveInStop ";
			switch (m_ret)		{
				case MMSYSERR_INVALHANDLE:
					err += "Specified device handle is invalid.";
					break;
				case MMSYSERR_NODRIVER:
					err += "No device driver is present.";
					break;
				case MMSYSERR_NOMEM:
					err += "Unable to allocate or lock memory.";
					break;
				}
#ifdef _DEBUG
			AfxMessageBox(err);
#endif
			}

		m_ret = ::waveInReset(m_hwi);
		if (MMSYSERR_NOERROR != m_ret)	{
			CString err = "CWaveIn::StopRecord waveInReset ";
			switch (m_ret)		{
				case MMSYSERR_INVALHANDLE:
					err += "Specified device handle is invalid.";
					break;
				case MMSYSERR_NODRIVER:
					err += "No device driver is present.";
					break;
				case MMSYSERR_NOMEM:
					err += "Unable to allocate or lock memory.";
					break;
				}
#ifdef _DEBUG
			AfxMessageBox(err);
#endif
//			return FALSE;
			}
		m_ret = ::waveInUnprepareHeader(m_hwi, &m_wh1, sizeof(WAVEHDR));
		if (MMSYSERR_NOERROR != m_ret)	{
			CString err = "CWaveIn::On_WIM_DATA waveInUnprepareHeader ";
			switch (m_ret)		{
				case MMSYSERR_INVALHANDLE:
					err += "Specified device handle is invalid.";
					break;
				case MMSYSERR_NODRIVER:
					err += "No device driver is present.";
					break;
				case MMSYSERR_NOMEM:
					err += "Unable to allocate or lock memory.";
					break;
				case WAVERR_STILLPLAYING:
					err += "The buffer pointed to by the pwh parameter is still in the queue.";
					break;
				}
#ifdef _DEBUG
			AfxMessageBox(err);
#endif
//			return FALSE;
			}

		m_ret = ::waveInUnprepareHeader(m_hwi, &m_wh2, sizeof(WAVEHDR));
		if (MMSYSERR_NOERROR != m_ret) 	{
			CString err = "CWaveIn::On_WIM_DATA waveInUnprepareHeader ";;
			switch (m_ret)		{
				case MMSYSERR_INVALHANDLE:
					err += "Specified device handle is invalid.";
					break;
				case MMSYSERR_NODRIVER:
					err += "No device driver is present.";
					break;
				case MMSYSERR_NOMEM:
					err += "Unable to allocate or lock memory.";
					break;
				case WAVERR_STILLPLAYING:
					err += "The buffer pointed to by the pwh parameter is still in the queue.";
					break;
				}
#ifdef _DEBUG
			AfxMessageBox(err);
#endif
//			return FALSE;
			}
		

		m_ret = ::waveInClose(m_hwi);
		if (MMSYSERR_NOERROR != m_ret)	{
			CString err = "CWaveIn::On_WIM_DATA waveInClose ";
			switch (m_ret)		{
				case MMSYSERR_INVALHANDLE:
					err += "Specified device handle is invalid.";
					break;
				case MMSYSERR_NODRIVER:
					err += "No device driver is present.";
					break;
				case MMSYSERR_NOMEM:
					err += "Unable to allocate or lock memory.";
					break;
				case WAVERR_STILLPLAYING:
					err += "There are still buffers in the queue.";
					break;
				}
#ifdef _DEBUG
			AfxMessageBox(err);
#endif
//			return FALSE;
			}
		}

	m_hwi=(HWAVEIN)-1;

	return TRUE;
	}

void CWaveIn::OnWaveInEnd(WPARAM wParam, LPARAM lParam) {

	StopRecord();
	AfxEndThread(0);
	}

