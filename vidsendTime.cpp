#include "stdafx.h"
#include <stdlib.h>
#include "vidsend.h"
//#include "mainfrm.h"
#include "vidsendLog.h"
#include "vidsendTime.h"
#include <math.h>



BOOL CSole::hLegale;
CTimeSpan CSole::t1SolstizioInverno(0,7,55,0);	// il 22 dic. il sole sorge alle 7:34 (http://www.eurometeo.com/italian/ephem) (era 8.08 con calendario cartaceo!)
CTimeSpan CSole::t2SolstizioInverno(0,16,52,0);	// il 22 dic. il sole tramonta alle 16.47
CTimeSpan CSole::t1SolstizioEstate(0,4,35,0);	// il 21 giu. il sole sorge alle 4.42
CTimeSpan CSole::t2SolstizioEstate(0,20,25,0);	// il 21 giu. il sole tramonta alle 20.20
	// da Conoscere: 10/8: 5:18-19.38; 1/12: 7.44-16.40
	// 21 marzo: sorge 6.31, tramonta 18.43
	// 21 settembre: sorge 6.14, tramonta 18.31
	// guardare anche: http://www.aopa.it/sole.asp?citta=Torino&tipo=cal 26/9/06

//al 16/2/2010, il "sorge" è sbagliato di 30 min: 7:27 vs. 6.54 ... (da time.is)
//4/2/2011: modificato con algoritmo "navale USA" da forum.microchip.com

CSole::CSole(double latitudine,double longitudine,double myUTC) {

	hLegale=0;					 // metterci magari il flag di Windows...
	latitude=latitudine;
	longitude=longitudine;
	utcOffset = myUTC;
	zenith = Official;

	daylightChanges.delta=1;
	{
		CTime myT(CTime::GetCurrentTime().GetYear(),3,getUltimoWeekend(3),0,0,0);			// senza anno rompe le palle!
		daylightChanges.start=myT;
	}
	{
		CTime myT(CTime::GetCurrentTime().GetYear(),10,getUltimoWeekend(10),0,0,0);
		daylightChanges.end=myT;
	}

	}

CSole::CSole(WORD lat_deg,WORD lat_min,WORD lat_sec,WORD long_deg,WORD long_min,WORD long_sec,double myUTC) {

	hLegale=0;					 // metterci magari il flag di Windows...
	latitude=DegreesToAngle(lat_deg,lat_min,lat_sec);		
	longitude=DegreesToAngle(long_deg,long_min,long_sec);
	utcOffset = 1.0;		// da qua unificare con altro costruttore!
	zenith = Official;

	daylightChanges.delta=1;
	{
		CTime myT(CTime::GetCurrentTime().GetYear(),3,getUltimoWeekend(3),0,0,0);			// senza anno rompe le palle!
		daylightChanges.start=myT;
	}
	{
		CTime myT(CTime::GetCurrentTime().GetYear(),10,getUltimoWeekend(10),0,0,0);
		daylightChanges.end=myT;
	}

	}

int CSole::getUltimoWeekend(int m) {
	int i,n;
	
	if(m<0)
		m=CTime::GetCurrentTime().GetMonth();

	for(i=1; i<=31; i++) {
		CTime t1(CTime::GetCurrentTime().GetYear(),m,i,0,0,0);

		if(t1.GetDayOfWeek() == 1)		// domenica!
			n=i; 
		}

	return n;
	}

CTimeSpan CSole::getSunRiseAndSet(int q) {  // q=0 alba, 1 tramonto
	int m,g,i,n;
	int h,min;
	CTimeSpan t2;

	hLegale=0;
	m=CTime::GetCurrentTime().GetMonth();
	g=CTime::GetCurrentTime().GetDay();
	switch(m) {
		case 12:
			if(g<21)
				goto caso7_11;
			else
				m=0;
		case 1:
		case 2:
		case 3:
		case 4:
		case 5:
caso1_5:
			if(m>=4 || (m==3 && g>getUltimoWeekend(3)))
				hLegale=1;
			n=(m-1)*30+g-1+(31-22);			// calcolo i giorni dal solstizio d'inverno
			if(!q) {
				t2=t1SolstizioInverno-t1SolstizioEstate;
				h=t2.GetHours();
				min=t2.GetMinutes();
				i=((h*60+min)*n)/180;

				return CTimeSpan::CTimeSpan(0,t1SolstizioInverno.GetHours()-i/60+hLegale,t1SolstizioInverno.GetMinutes()- (i % 60),0);
				}
			else {
				t2=t2SolstizioEstate-t2SolstizioInverno;
				h=t2.GetHours();
				min=t2.GetMinutes();
				i=((h*60+min)*n)/180;

				return CTimeSpan::CTimeSpan(0,t2SolstizioInverno.GetHours()+i/60+hLegale,t2SolstizioInverno.GetMinutes()+ (i % 60),0);
				}
			break;
		case 6:
			if(g<21)
				goto caso1_5;
		case 7:
		case 8:
		case 9:
			hLegale=1;
		case 10:
			if(g<=getUltimoWeekend(10))
				hLegale=1;
		case 11:
caso7_11:
			n=((m-1)*30+g-1)+10-180;			 // calcolo i giorni dal solstizio d'estate
			if(!q) {
				t2=t1SolstizioInverno-t1SolstizioEstate;
				h=t2.GetHours();
				min=t2.GetMinutes();
				i=((h*60+min)*n)/180;

				return CTimeSpan::CTimeSpan(0,t1SolstizioEstate.GetHours()+i/60+hLegale,t1SolstizioEstate.GetMinutes()+ (i % 60),0);
				}
			else {
				// C'è uno STRANO ERRORE +1 ora al tramonto a novembre... boh? verificare, forse c'era pure su alba in primavera? 2017
				t2=t2SolstizioEstate-t2SolstizioInverno;
				h=t2.GetHours();
				min=t2.GetMinutes();
				i=((h*60+min)*n)/180;

				return CTimeSpan::CTimeSpan(0,t2SolstizioEstate.GetHours()-i/60+hLegale,t2SolstizioEstate.GetMinutes()- (i % 60),0);
				}
			break;

		}
  }


    /// Implementation of algorithm found in Almanac for Computers, 1990
    /// published by Nautical Almanac Office
    /// 
    /// Implemented by Huysentruit Wouter, Fastload-Media.be

CTimeSpan CSole::getSunRiseAndSet(enum DIRECTION direction) {  

	/* doy (N) */
  int N = CTimeEx::GetDayOfYear();

  /* appr. time (t) */
  double lngHour = longitude / 15.0;

  double t;

  if(direction == Sunrise)
    t = N + ((6.0 - lngHour) / 24.0);
  else
    t = N + ((18.0 - lngHour) / 24.0);

  /* mean anomaly (M) */
  double M = (0.9856 * t) - 3.289;

	/* true longitude (L) */
	double L = M + (1.916 * sin(Deg2Rad(M))) + (0.020 * sin(Deg2Rad(2*M))) + 282.634;
	L = FixValue(L,0,360);

	/* right asc (RA) */
  double RA = Rad2Deg(atan(0.91764 * tan(Deg2Rad(L))));
  RA = FixValue(RA,0,360);

  /* adjust quadrant of RA */
  double Lquadrant = (floor(L / 90.0)) * 90.0;
  double RAquadrant = (floor(RA / 90.0)) * 90.0;
  RA += (Lquadrant - RAquadrant);

	RA /= 15.0;

  /* sin cos DEC (sinDec / cosDec) */
  double sinDec = 0.39782 * sin(Deg2Rad(L));
  double cosDec = cos(asin(sinDec));

  /* local hour angle (cosH) */
  double cosH = (cos(Deg2Rad((double)zenith / 1000.0f)) - (sinDec * sin(Deg2Rad(latitude)))) / (cosDec * cos(Deg2Rad(latitude)));

  /* local hour (H) */
  double H;

  if(direction == Sunrise)
    H = 360.0 - Rad2Deg(acos(cosH));
  else
		H = Rad2Deg(acos(cosH));

  H /= 15.0;

  /* time (T) */
  double T = H + RA - (0.06571 * t) - 6.622;

	/* universal time (T) */
	double UT = T - lngHour;

  UT += utcOffset;  // local UTC offset

  if(daylightChanges.delta) {
		CTimeSpan t1(0,CTime::GetCurrentTime().GetMonth(),CTime::GetCurrentTime().GetDay(),0);
		CTimeSpan t2(0,daylightChanges.start.GetMonth(),daylightChanges.start.GetDay(),0);
		CTimeSpan t3(0,daylightChanges.end.GetMonth(),daylightChanges.end.GetDay(),0);
		if(t1>=t2 && t1<t3)
			UT += daylightChanges.delta;
		}

  UT = FixValue(UT,0,24);

	{
		CTimeSpan myTS=(int)(UT * 3600);		// Convert to seconds
		return myTS;
	}

  }



CTimeSpan CSole::getAlba() {
	CTimeSpan ts;

	ts=getSunRiseAndSet(Sunrise);
	ts -= ts.GetSeconds();
	return ts;
  }

CTimeSpan CSole::getTramonto() {
	CTimeSpan ts;

	ts=getSunRiseAndSet(Sunset);
	ts -= ts.GetSeconds();
	return ts;
  }

BYTE CSole::isGiorno() {		 // bit 0=1 se sì, bit 8=1 se ORA è l'alba, bit 1=1 se siamo entro 30 minuti dagli estremi
	int i;
	CTimeSpan t1,t2;
	CTimeSpan myT=CTimeSpanEx::GetCurrentTime();

	t1=getAlba();
	t2=getTramonto();
	if(myT>=t1 && myT<t2) {
		i=1;
		if(myT==t1)
			i |= 0x80;
		if(((myT>=t1) && ((CTimeSpan)(myT-t1)).GetTotalMinutes() < 30) || 
			((myT<t2) && ((CTimeSpan)(t2-myT)).GetTotalMinutes() < 30))
			i |= 0x2;
		}
	else
		i=0;

	return i;
	}

BYTE CSole::isNotte() {		 // bit 0=1 se sì, bit 8=1 se ORA è il tramonto, bit 1=1 se siamo entro 30 minuti dagli estremi
	int i;
	CTimeSpan t1,t2;
	CTimeSpan myT=CTimeSpanEx::GetCurrentTime();

	t1=getAlba();
	t2=getTramonto();
	if(myT<t1 || myT>=t2) {
		i=1;
		if(myT==t2)
			i |= 0x80;
		if(((myT<t1) && ((CTimeSpan)(t1-myT)).GetTotalMinutes() < 30) || 
			((myT>=t2) && ((CTimeSpan)(myT-t2)).GetTotalMinutes() < 30))
			i |= 0x2;
		}
	else
		i=0;

	return i;
	}

int CSole::isAlba() {
	int i;
	CTimeSpan t1;
	CTimeSpan myT=CTimeSpanEx::GetCurrentTime();

	t1=getAlba();
	if(myT>(t1-30*60) && myT<=(t1+120*60)) {
		i=1;
		}
	else
		i=0;

	return i;
	}

int CSole::isTramonto() {
	int i;
	CTimeSpan t1;
	CTimeSpan myT=CTimeSpanEx::GetCurrentTime();

	t1=getTramonto();
	if(myT>(t1-30*60) && myT<=(t1+120*60)) {
		i=1;
		}
	else
		i=0;

	return i;
	}

WORD CSole::lucePresunta(CTimeSpan t) {
	int i,v,m=120;				// minuti previsti per alba/tramonto
	CTimeSpan t1,ts,tsA,tsT;
	const int minLuce=5,maxLuce=90;

	if(t==0) {
		t=CTimeSpanEx::GetCurrentTime();
		}
	tsA=getAlba(); tsA-=1800;		// mezz'ora prima c'e' già luce...
	tsT=getTramonto();	tsT+=1800;		// mezz'ora dopo c'e' ancora luce...
	if(t<tsA) {
		v=minLuce;
		}
	else {
		if(t>tsT) {
			v=minLuce;
			}
		else {
			ts=t-tsA;
			if(ts.GetTotalMinutes() <= m) {		// da min a (max-25+min) all'alba...
				v=minLuce+((maxLuce-25)*ts.GetTotalMinutes())/m;
				}
			else {
				ts=tsT-t;
				if(ts.GetTotalMinutes() <= m) {	 // ...e al tramonto
					v=minLuce+((maxLuce-25)*ts.GetTotalMinutes())/m;
					}
				else {					 // un valore tra max-25 e max di giorno
					ts=tsT-tsA;
					v=maxLuce-25*abs(t.GetHours()-tsA.GetHours()-ts.GetHours()/2)/(ts.GetHours()/2);
					}
				}
			}
		}
	return v;
  }

double CSole::Deg2Rad(double angle) {

  return PI * angle / 180.0;
  }

double CSole::Rad2Deg(double angle) {

  return 180.0 * angle / PI;
  }

double CSole::FixValue(double value, double min, double max) {
  
	while(value < min)
    value += (max - min);

  while(value >= max)
		value -= (max - min);

	return value;
  }

double CSole::DegreesToAngle(double degrees, double minutes, double seconds) {

  if(degrees < 0)
    return degrees - (minutes / 60.0) - (seconds / 3600.0);
  else
    return degrees + (minutes / 60.0) + (seconds / 3600.0);
	}

// ---------------------------------------------------------------------------
CTimeSpan CTimeSpanEx::GetCurrentTime() {

	return CTimeSpan::CTimeSpan(0,CTime::GetCurrentTime().GetHour(),CTime::GetCurrentTime().GetMinute(),
		0 /*CTime::GetCurrentTime().GetSecond()*/);
	}


// ---------------------------------------------------------------------------
CString CTimeEx::getNow(int ex) {
	int i;
	CString S;

	switch(ex) {
		case 0:
			S=CTime::GetCurrentTime().Format("%d/%m/%Y %H:%M:%S");
			break;
		case 1:
		case 2:
			S.Format(ex == 1 ? _T("sono le ore %d e %d del %d %s %d") : _T("sono le ore %02d e %02d del %d %s %d"),
				GetCurrentTime().GetHour(),GetCurrentTime().GetMinute(),
				GetCurrentTime().GetDay(),(LPTSTR)(LPCTSTR)Num2Mese(GetCurrentTime().GetMonth()),
				GetCurrentTime().GetYear());
			break;
		}

	return S;
  }


CString CTimeEx::getNowGMT(BOOL bAddCR) {
	time_t aclock;
	struct tm *newtime;
	int i;
	CString S;

	S.Format(_T("%s %s %02u %02u:%02u:%02u %04u"),
		Num2Day3(CTime::GetCurrentTime().GetDayOfWeek()),
		Num2Month3(CTime::GetCurrentTime().GetMonth()),
		CTime::GetCurrentTime().GetDay(),
		CTime::GetCurrentTime().GetHour(),
		CTime::GetCurrentTime().GetMinute(),
		CTime::GetCurrentTime().GetSecond(),
		CTime::GetCurrentTime().GetYear());

#ifndef _WIN32_WCE
	i=-(_timezone/3600)+(_daylight ? 1 : 0);
#else
	i=0;
#endif
	if(!i)
		S+=_T(" UTC");				// anche "GMT"
	else {
		CString S2;
		S2.Format(_T(" %c%02d00"),i>=0 ? '+' : '-',abs(i));
		S+=S2;
		}
	if(bAddCR)
		S+="\r\n";

	return S;
	}

CString CTimeEx::getNowGoogle(BOOL bAddCR) {
	time_t aclock;
	struct tm *newtime;
	int i;
	CString S;

	S.Format(_T("%04u-%02u-%02uT%02u:%02u:%02uZ"),
		CTime::GetCurrentTime().GetYear(),
		CTime::GetCurrentTime().GetMonth(),
		CTime::GetCurrentTime().GetDay(),
		CTime::GetCurrentTime().GetHour(),
		CTime::GetCurrentTime().GetMinute(),
		CTime::GetCurrentTime().GetSecond());

#ifndef _WIN32_WCE
	i=-(_timezone/3600)+(_daylight ? 1 : 0);
#else
	i=0;
#endif
	if(!i)
		S+=_T(" UTC");				// anche "GMT"
	else {
		CString S2;
		S2.Format(_T(" %c%02d00"),i>=0 ? '+' : '-',abs(i));
		S+=S2;
		}
	if(bAddCR)
		S+="\r\n";

	return S;
	}

CString CTimeEx::Num2Mese(int i) {

  switch(i) {
		case 1:
      return _T("Gennaio");
			break;
		case 2:
      return _T("Febbraio");
			break;
		case 3:
      return _T("Marzo");
			break;
		case 4:
      return _T("Aprile");
			break;
		case 5:
      return _T("Maggio");
			break;
		case 6:
      return _T("Giugno");
			break;
		case 7:
      return _T("Luglio");
			break;
		case 8:
      return _T("Agosto");
			break;
		case 9:
	    return _T("Settembre");
			break;
		case 10:
      return _T("Ottobre");
			break;
		case 11:
      return _T("Novembre");
			break;
		case 12:
      return _T("Dicembre");
			break;
	  }
  
  }

CString CTimeEx::Num2Giorno(int i) {

  switch(i) {
		case 1:
      return _T("Domenica");
			break;
		case 2:
      return _T("Lunedì");
			break;
		case 3:
      return _T("Martedì");
			break;
		case 4:
      return _T("Mercoledì");
			break;
		case 5:
      return _T("Giovedì");
			break;
		case 6:
      return _T("Venerdì");
			break;
		case 7:
      return _T("Sabato");
			break;
	  }
  }

CString CTimeEx::Num2Month3(int i) {

  switch(i) {
		case 1:
      return _T("Jan");
			break;
		case 2:
      return _T("Feb");
			break;
		case 3:
      return _T("Mar");
			break;
		case 4:
      return _T("Apr");
			break;
		case 5:
      return _T("May");
			break;
		case 6:
      return _T("Jun");
			break;
		case 7:
      return _T("Jul");
			break;
		case 8:
      return _T("Aug");
			break;
		case 9:
	    return _T("Sep");
			break;
		case 10:
      return _T("Oct");
			break;
		case 11:
      return _T("Nov");
			break;
		case 12:
      return _T("Dec");
			break;
	  }
  
  }

CString CTimeEx::Num2Day3(int i) {

  switch(i) {
		case 1:
      return _T("Sun");
			break;
		case 2:
      return _T("Mon");
			break;
		case 3:
      return _T("Tue");
			break;
		case 4:
      return _T("Wed");
			break;
		case 5:
      return _T("Thu");
			break;
		case 6:
      return _T("Fri");
			break;
		case 7:
      return _T("Sat");
			break;
	  }
  }

int CTimeEx::getMonthFromGMTString(const CString S) {
	CString S2=S.Left(3);

	if(!S2.CompareNoCase(_T("JAN")))
		return 1;
	else if(!S2.CompareNoCase(_T("FEB")))
		return 2;
	else if(!S2.CompareNoCase(_T("MAR")))
		return 3;
	else if(!S2.CompareNoCase(_T("APR")))
		return 4;
	else if(!S2.CompareNoCase(_T("MAY")))
		return 5;
	else if(!S2.CompareNoCase(_T("JUN")))
		return 6;
	else if(!S2.CompareNoCase(_T("JUL")))
		return 7;
	else if(!S2.CompareNoCase(_T("AUG")))
		return 8;
	else if(!S2.CompareNoCase(_T("SEP")))
		return 9;
	else if(!S2.CompareNoCase(_T("OCT")))
		return 10;
	else if(!S2.CompareNoCase(_T("NOV")))
		return 11;
	else if(!S2.CompareNoCase(_T("DEC")))
		return 12;

	return -1;
	}


CString CTimeEx::getFasciaDellaGiornata() {

	if(GetCurrentTime().GetHour() >=7 && GetCurrentTime().GetHour()<13)
		return _T("stamattina");
	else if(GetCurrentTime().GetHour() >=13 && GetCurrentTime().GetHour()<20)
		return _T("oggi");
	else if(GetCurrentTime().GetHour() >=20 && GetCurrentTime().GetHour()<24)
		return _T("stasera");
	else if(GetCurrentTime().GetHour() >=0 && GetCurrentTime().GetHour()<7)
		return _T("stanotte");

	}

CString CTimeEx::getSaluto() {

	if(GetCurrentTime().GetHour() >=7 && GetCurrentTime().GetHour()<13)
		return _T("Buongiorno");
	else if(GetCurrentTime().GetHour() >=13 && GetCurrentTime().GetHour()<20)
		return _T("Buon pomeriggio");
	else if(GetCurrentTime().GetHour() >=20 && GetCurrentTime().GetHour()<24)
		return _T("Buonasera");
	else if(GetCurrentTime().GetHour() >=0 && GetCurrentTime().GetHour()<7)
		return _T("Buonanotte");

	}

CTime CTimeEx::parseGMTTime(const CString S) {
	char *p;
	int i,j,tzFound=0,reverseUTC=0;
	struct tm t;
	CString s=S;
	
//	_tzset();			// questo imposterebbe la timezone, che altrimenti potrebbe defaultare a -8h
	// v. Joshua.cpp::InitInstance

	while(_istspace(s.GetAt(0)))
		s=s.Mid(1);
	if(_istalpha(s.GetAt(0))) {
		s.MakeUpper();
		if(!s.Left(2).CompareNoCase(_T("SU")))
			i=0;
		else if(!s.Left(2).CompareNoCase(_T("MO")))
			i=1;
		else if(!s.Left(2).CompareNoCase(_T("TU")))
			i=2;
		else if(!s.Left(2).CompareNoCase(_T("WE")))
			i=3;
		else if(!s.Left(2).CompareNoCase(_T("TH")))
			i=4;
		else if(!s.Left(2).CompareNoCase(_T("FR")))
			i=5;
		else if(!s.Left(2).CompareNoCase(_T("SA")))
			i=6;
		else				// NON deve capitare... patch per evitare il peggio!
			i=0;
		t.tm_wday=i;
		s=s.Mid(3);
		while(!isspace(s.GetAt(0)))
			s=s.Mid(1);
		if(s.GetAt(0) ==',')
			s=s.Mid(1);
		s=s.Mid(1);
no_day:
		while(_istspace(s.GetAt(0)))
			s=s.Mid(1);
		if(_istdigit(s.GetAt(0))) {
			t.tm_mday=_ttoi((LPTSTR)(LPCTSTR)s);
			while(iswdigit(s.GetAt(0)))
				s=s.Mid(1);
			s=s.Mid(1);
			t.tm_mon=getMonthFromGMTString(s);
			s=s.Mid(4);
			t.tm_year=_ttoi((LPTSTR)(LPCTSTR)s);
			if(t.tm_year<80)
				t.tm_year+=100;
			if(t.tm_year>=200)
				t.tm_year-=1900;
			s=s.Mid(5);
			t.tm_hour=_ttoi(s);
			s=s.Mid(3);
			t.tm_min=_ttoi(s);
			s=s.Mid(3);
			t.tm_sec=_ttoi(s);
			if(s.GetAt(1) == '-')
				reverseUTC=1;
			s=s.Mid(2,2);
			i=_ttoi(s);
			if(reverseUTC)
				i=-i;
			}
		else {
			t.tm_mon=getMonthFromGMTString(s);
			s=s.Mid(4);
			t.tm_mday=_ttoi(s);
			s=s.Mid(3);
			t.tm_hour=_ttoi(s);
			s=s.Mid(3);
			t.tm_min=_ttoi(s);
			s=s.Mid(3);
			t.tm_sec=_ttoi(s);
			s=s.Mid(3);
			if(s.GetAt(0) == 'U') {		// variante con "UTC 1998
				t.tm_year=_ttoi(s.Mid(4))-1900;
				i=0;								// greenwich
				tzFound=1;
				}
			else {							  // variante con 1998 +0100
				t.tm_year=_ttoi(s)-1900;
				s=s.Mid(4);
				while(s.GetLength()>0 && _istspace(s.GetAt(0)))
					s=s.Mid(1);
				if(s.GetLength()>0) {		// se la timezone segue l'anno...
					if(_istdigit(s.GetAt(0)) || s.GetAt(0)=='+' || s.GetAt(0)=='-') {
						tzFound=1;
						if(s.GetAt(0) == '-')
							reverseUTC=1;
						if(s.GetAt(0)=='+' || s.GetAt(0)=='-')
							s=s.Mid(1);
						s=s.Mid(0,2);		// stronco i minuti!
						i=_ttoi(s);									// ...la leggo
						if(reverseUTC)
							i=-i;
						}
					else if(S.GetAt(0)=='U' || S.GetAt(0)=='G')	{		// per ora solo UTC o GMT (sono lo stesso?)!
						i=0;								// greenwich
						tzFound=1;
						}
					}
				}
			}
		}
	else {
		if(_istalpha(s.GetAt(4))) {		// caso in cui manca il giorno della settimana, poi idem come sopra...
			s.MakeUpper();
			t.tm_wday=0;
			goto no_day;
			}
		else {
			s=s.Mid(2);		// salto \xd\xa
			s=s.Mid(6);
			t.tm_year=_ttoi((LPTSTR)(LPCTSTR)s);
			s=s.Mid(3);
			t.tm_mon=_ttoi((LPTSTR)(LPCTSTR)s);
			s=s.Mid(3);
			t.tm_mday=_ttoi((LPTSTR)(LPCTSTR)s);
			s=s.Mid(3);
			t.tm_hour=_ttoi((LPTSTR)(LPCTSTR)s);
			s=s.Mid(3);
			t.tm_min=_ttoi((LPTSTR)(LPCTSTR)s);
			s=s.Mid(3);
			t.tm_sec=_ttoi((LPTSTR)(LPCTSTR)s);
			s=s.Mid(3);
			i=_ttoi((LPTSTR)(LPCTSTR)s);
			}
		}
	if(tzFound)
#ifndef _WIN32_WCE
		t.tm_hour=t.tm_hour-i-(_timezone/3600)+(_daylight ? 1 : 0);
#else
		t.tm_hour=t.tm_hour-i-(0/3600)+0;
#endif

	return CTime::CTime(t.tm_year+1900,t.tm_mon,t.tm_mday,t.tm_hour,t.tm_min,t.tm_sec);
	}

CTime CTimeEx::parseTime(const CString S) {		// semplice, tipo "05/10/10 12:00"
	char *p;
	int i,j,tzFound=0;
	WORD nDay,nMonth,nYear,nHour,nMinute,nSecond;
	CString s=S;
	
//	_tzset();			// questo imposterebbe la timezone, che altrimenti potrebbe defaultare a -8h
	// v. Joshua.cpp::InitInstance

	p=(char *)(LPCTSTR)S;
	while(isspace(*p) && *p)
		p++;
	nDay=atoi(p);		// 2 digit
	while(isdigit(*p))
		p++;
	p++;
	while(isspace(*p) && *p)
		p++;
	nMonth=atoi(p);			// 2 digit
	while(isdigit(*p))
		p++;
	p++;
	while(isspace(*p) && *p)
		p++;
	nYear=atoi(p);		// 2-4 digit (v.sotto)
	while(isdigit(*p))
		p++;

	nHour=0;
	nMinute=0;
	nSecond=0;
	if(*p) {
		p++;
		while(isspace(*p) && *p)
			p++;
		nHour=atoi(p);		// 2 digit
		while(isdigit(*p))
			p++;
		p++;
		while(isspace(*p) && *p)
			p++;
		nMinute=atoi(p);	// 2 digit

		while(isdigit(*p))
			p++;
		if(*p) {
			p++;
			while(isspace(*p) && *p)
				p++;
			nSecond=atoi(p); // 2 digit
			}
		}

	if(nYear < 100) {
		if(nYear < 80)
			nYear+=2000;
		else
			nYear+=1900;
		}


	i=0;
	if(tzFound)
#ifndef _WIN32_WCE
		nHour=nHour-i-(_timezone/3600)+(_daylight ? 1 : 0);
#else
		nHour=nHour-i-(0/3600)+0;
#endif

	return CTime::CTime(nYear,nMonth,nDay,nHour,nMinute,nSecond);
	}

BOOL CTimeEx::isWeekend() {

	return GetCurrentTime().GetDayOfWeek() == 1 || GetCurrentTime().GetDayOfWeek() == 7;
	}

BOOL CTimeEx::isWeekend(CTime t) {

	return t.GetDayOfWeek() == 1 || t.GetDayOfWeek() == 7;
	}

WORD CTimeEx::GetDayOfYear() {
	struct tm *myTm;

	myTm=CTime::GetCurrentTime().GetLocalTm();
	return myTm->tm_yday;
	}

