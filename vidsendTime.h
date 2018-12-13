
#ifndef _JOSHUA_TIMER_INCLUDED_
#define _JOSHUA_TIMER_INCLUDED_




class CLuna {
public:
private:
	static CTime timeBase;

public:
	CLuna();
	HICON getIcon() const;
	static int getFase(CTime tm=NULL);
	static double moon_position(double j, double ls);
	static double moon_phase(int year,int month,int day, double hour, int* ip);
	static double sun_position(double );
	static double Julian(int year,int month,double day);
	BOOL draw(CDC *,int,int);
	};

class CSole {
public:
	enum ZENITH_VALUE {
		/// Official zenith (90.5)
		Official = 90500,
		/// Civil zenith (96)
		Civil = 96000,
		/// Nautical zenith (102)
		Nautical = 102000,
		/// Astronomical zenith (108)
		Astronomical = 108000
    };

  enum DIRECTION {
    Sunrise,
    Sunset
    };
	struct DAYLIGHT_CHANGES {
		CTime start,end;
		WORD delta;		// ore
		};

public:
	CSole(double latitudine=0,double longitudine=0,double myUTC=1.0);
	CSole(WORD lat_deg,WORD lat_min,WORD lat_sec,WORD long_deg,WORD long_min,WORD long_sec,double myUTC=1.0);
	int isAlba();
	int isTramonto();
	static CTimeSpan getSunRiseAndSet(int );
	CTimeSpan getSunRiseAndSet(enum DIRECTION );
	CTimeSpan getAlba(), getTramonto();
	BYTE isGiorno(),isNotte();
	WORD lucePresunta(CTimeSpan t=NULL);
	static double Deg2Rad(double angle);
	static double Rad2Deg(double angle);
	static double FixValue(double value, double min, double max);
	static double DegreesToAngle(double degrees, double minutes, double seconds);

public:
	static BOOL hLegale;
	double longitude,latitude;
  double utcOffset;
	enum ZENITH_VALUE zenith;
	struct DAYLIGHT_CHANGES daylightChanges;

private:
	static CTimeSpan t1SolstizioInverno,t2SolstizioInverno,t1SolstizioEstate,t2SolstizioEstate;
	static int getUltimoWeekend(int mese=-1);

public:

	};


class CTimeSpanEx : public CTimeSpan {
public:
	static CTimeSpan GetCurrentTime();
	};


class CTimeEx : public CTime {
public:
	static CString Num2Mese(int);
	static CString Num2Giorno(int);
	static CString Num2Month3(int);
	static CString Num2Day3(int);
	static CString getNow(int ex=0);
	static CString getNowGMT(BOOL bAddCR=TRUE);
	static CString getNowGoogle(BOOL bAddCR=TRUE);
	static int getMonthFromGMTString(const CString);
	static CString getMese() { return Num2Giorno(GetCurrentTime().GetDay()); };
	static CString getGiorno() { return Num2Mese(GetCurrentTime().GetMonth()); };
	static CString getFasciaDellaGiornata();
	static CString getSaluto();
	static WORD GetDayOfYear();
	static CTime parseGMTTime(const CString);
	static CTime parseTime(const CString);
	static BOOL isWeekend();
	BOOL isWeekend(CTime);
	};

#endif
