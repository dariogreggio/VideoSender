#ifndef _VIDSENDDSHOW_INCLUDED
#define _VIDSENDDSHOW_INCLUDED


#define WM_GRABBED_BUFFER WM_APP+16		// verifica se ci sono altri msg., p.es. in vidsend.h

#pragma once

#include <string>
#include <stdlib.h>

class VideoFormat
{
public:
	VideoFormat()
		: width(0)
		, height(0)
		, bitsPerPixel(0)
		, fourCC(0)
	{
	}

	VideoFormat (int width, int height, int bitsPerPixel, unsigned int fourCC)
	{
		this->width = width;
		this->height = height;
		this->bitsPerPixel = bitsPerPixel;
		this->fourCC = fourCC;
	}

	int width;
	int height;
	int bitsPerPixel;
	unsigned int fourCC;

	std::string ToString()
	{
		char temp[1024];
		char a, b, c, d;
		a = fourCC & 0xFF;
		b = (fourCC>>8) & 0xFF;
		c = (fourCC>>16) & 0xFF;
		d = (fourCC>>24) & 0xFF;
		sprintf (temp, "%dx%dx%dbpp %c%c%c%c", width, height, bitsPerPixel, a, b, c, d);
		return temp;
	}
};



#pragma once


class IVideoCapture
{
public:

	class Exception
	{
	public:
		Exception (const char* s) : message (s) {}
		std::string message;
	};


	IVideoCapture() {}
	virtual ~IVideoCapture() {}

	virtual void StartCapture() = 0;
	virtual void StopCapture() = 0;
	virtual bool IsCapturing() = 0;

	virtual void WaitForNextFrame() = 0;
	virtual unsigned char* GetCurrentFrame() = 0;
	virtual int GetCurrentFrameSize() = 0;

	virtual int GetWidth() = 0;
	virtual int GetHeight() = 0;
	virtual int GetBitsPerPixel() = 0;
	virtual unsigned int GetFourCC() = 0;
};





//
// direct show video capture
//
// all methods may throw an exception
//
// Written by Maxwell Sayles.  Copyright (C) 2003.
//

#pragma once

#include <dshow.h>
//#include <d3dx8.h>
#include <qedit.h>
#include <atlbase.h>
//#include <windows.h>
#include <vector>
//#include "VideoFormat.h"
//#include "IVideoCapture.h"

// sample callback routine
class SampleGrabberCallback : public ISampleGrabberCB
{
public:
	SampleGrabberCallback(HWND notifWnd=NULL);
	~SampleGrabberCallback();
  STDMETHODIMP_(ULONG) AddRef();
  STDMETHODIMP_(ULONG) Release();
  STDMETHODIMP QueryInterface(REFIID riid, void** ppvObject);
  STDMETHODIMP SampleCB(double Time, IMediaSample* sample);
  STDMETHODIMP BufferCB(double time, BYTE* buffer, long bufferSize);

	static double round(double,double);
	void Reset();
	void WaitForCallback(DWORD time=INFINITE);
	void SetWindow(HWND hWnd);
	void SetFramePerSecond(WORD );

public:
	BYTE *buffer;
	int bufferSize;

private:
	int refcount;
	HANDLE event;
	HWND m_hWnd;
	WORD m_Fps;
	double lastTimeCaptured;
};


class DShowVideoCapture : public IVideoCapture {

public:
	DShowVideoCapture(int deviceIndex=0,HWND hWnd=NULL);
	DShowVideoCapture(HWND);
	virtual ~DShowVideoCapture();
	
	virtual void StartCapture();
	virtual void StopCapture();
	virtual bool IsCapturing() { return isRunning; }

	virtual void WaitForNextFrame();
	virtual unsigned char* GetCurrentFrame();
	virtual int GetCurrentFrameSize();

	virtual int GetWidth() { return videoFormat.width; }
	virtual int GetHeight() { return videoFormat.height; }
	virtual int GetBitsPerPixel() { return videoFormat.bitsPerPixel; }
	virtual unsigned int GetFourCC() { return videoFormat.fourCC; }

	IBaseFilter *getCaptureFilter() { return captureFilter; }
	ICaptureGraphBuilder2 *getCaptureGraphBuilder() { return graphBuilder; }

	std::vector<VideoFormat> EnumerateNativeVideoFormats();
	void SetNativeFormat (int enumerationIndex); // index is an index from EnumerateNativeVideoFormats
	void SetFormat (int width, int height, int bitsPerPixel, WORD framePerSecond, WORD framePerSecond_hard=0, unsigned long fourCC=0); // default fourCC is for RGB
	VideoFormat GetFormat() { return videoFormat; }
	void SetWindow(HWND hWnd) { sampleGrabberCB->SetWindow(m_hWnd=hWnd); }
	HRESULT SetupVideoWindow();
	BOOL SetPreview(BOOL m) { doPreview=m; return m; };

	std::vector<int> EnumerateCrossbarInputs(); // values are enumerates from PhysicalConnectorType
	std::vector<int> EnumerateCrossbarOutputs();
	void RouteCrossbar(int input, int output);
	static const char* GetPhysicalConnectorName(int type); // can be used to get a human readable name for the enumerate
	static const char* GetVideoStandardName(int type); // idem
	CString GetDeviceName() { return captureDeviceName; }
	static BOOL GetDriverDescription(WORD , LPSTR , INT , LPSTR , INT );

	int SetGainParameters(double brt,double cont,double sat);
	static void GetDXVersion(DWORD* , DWORD* );

private:
	DShowVideoCapture(const DShowVideoCapture&);
	DShowVideoCapture& operator= (const DShowVideoCapture&);

	IBaseFilter* CreateCaptureDevice(int deviceIndex);
	void CreateFilters(int deviceIndex);
	void ReleaseFilters();

	void ConnectAllFilters();
	void DisconnectAllFilters();

	void CacheVideoFormat();

	int FindClosestNativeFormat(int width, int height, int bitsPerPixel, unsigned int fourCC);

  CComPtr<IBaseFilter> captureFilter;
	CComPtr<IBaseFilter> sampleGrabberFilter;
	CComPtr<IBaseFilter> nullRendererFilter;
	CComPtr<IBaseFilter> previewRendererFilter;
	
	CComPtr<ISampleGrabber> sampleGrabber;

  CComPtr<IGraphBuilder> filterGraph;
  CComPtr<ICaptureGraphBuilder2> graphBuilder;
	CComPtr<IMediaControl> control;
	CComPtr<IVideoWindow> window;
	IAMCrossbar* crossbar;
	IAMAnalogVideoDecoder* analogVideo;
	IAMVideoProcAmp* videoAmp;

	SampleGrabberCallback* sampleGrabberCB;

	VideoFormat videoFormat;
	bool isRunning,doPreview;
	CString captureDeviceName;
	HWND m_hWnd;

	friend LRESULT CALLBACK VideoCallback(HWND , UINT , WPARAM , LPARAM );
};


#endif