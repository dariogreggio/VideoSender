// vidsend2Set.cpp : implementation of the CVidsendSet class
//

#include "stdafx.h"
#include "vidsend.h"
#include "vidsendLog.h"
#include "vidsendSet.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

BOOL CRecordsetEx::goTo(DWORD n) {
	int i;
	char p[1024];

	m_strFilter.Format("id=%u",n);
	TRY {
		i=Requery();
		}
	CATCH(CDBException,e) {
		strcpy(p,"errore nello scorrimento del DataSet: ");
		strcat(p,e->m_strStateNativeOrigin);
#ifdef _DEBUG
		AfxMessageBox(p);
#else
		if(theApp.FileSpool)
			*theApp.FileSpool << p;
#endif
		i=0;
		}
	END_CATCH
	m_strFilter.Empty();
	if(i)
		return !IsEOF();
	else
		return FALSE;
	}

DWORD CRecordsetEx::getCount() {
	DWORD n;

	m_strFilter.Empty();
	if(Requery()) {
		n=0;
		while(!IsEOF()) {
			MoveNext();
			n++;
			}
		return n;
		}
	else
		return -1;
	}

/////////////////////////////////////////////////////////////////////////////
// CVidsendSet implementation

IMPLEMENT_DYNAMIC(CVidsendSet, CRecordset)

CVidsendSet::CVidsendSet(CDatabase* pdb)
	: CRecordsetEx(pdb)
{
	//{{AFX_FIELD_INIT(CVidsendSet)
	m_ID = 0;
	m_INDIRIZZO = _T("");
	m_DATA = 0;
	m_DESCRIZIONE = _T("");
	m_LIVELLO = 0;
	m_nFields = 5;
	//}}AFX_FIELD_INIT
	m_nDefaultType = snapshot;
}

CString CVidsendSet::GetDefaultConnect() {
	
	return _T(theApp.theODBCConnectString);
	}

CString CVidsendSet::GetDefaultSQL() {
	
	return _T("[log]");
	}

void CVidsendSet::DoFieldExchange(CFieldExchange* pFX) {
	//{{AFX_FIELD_MAP(CVidsendSet)
	pFX->SetFieldType(CFieldExchange::outputColumn);
	RFX_Long(pFX, _T("[ID]"), m_ID);
	RFX_Text(pFX, _T("[indirizzo]"), m_INDIRIZZO);
	RFX_Date(pFX, _T("[data]"), m_DATA);
	RFX_Text(pFX, _T("[descrizione]"), m_DESCRIZIONE);
	RFX_Byte(pFX, _T("[LIVELLO]"), m_LIVELLO);
	//}}AFX_FIELD_MAP
}

/////////////////////////////////////////////////////////////////////////////
// CVidsendSet diagnostics

#ifdef _DEBUG
void CVidsendSet::AssertValid() const {
	CRecordset::AssertValid();
	}

void CVidsendSet::Dump(CDumpContext& dc) const {
	CRecordset::Dump(dc);
	}
#endif //_DEBUG


/////////////////////////////////////////////////////////////////////////////
// CVidsendSet2

IMPLEMENT_DYNAMIC(CVidsendSet2, CRecordset)

CVidsendSet2::CVidsendSet2(CDatabase* pdb)
	: CRecordsetEx(pdb)
{
	//{{AFX_FIELD_INIT(CVidsendSet2)
	m_ID = 0;
	m_Nome = _T("");
	m_Cognome = _T("");
	m_Telefono = _T("");
	m_Fax = _T("");
	m_riservato = FALSE;
	m_eMail = _T("");
	m_Username = _T("");
	m_Password = _T("");
	m_Caratteristiche = 0;
	m_UltimoIP = _T("");
	m_WelcomeMsg = _T("");
	m_bTimedConn = FALSE;
	m_TimedConn=0;
	m_Abilitato = FALSE;
	m_Gruppo = 0;
	m_RemoteControl = 0;
	m_DontSave = 0;
	m_PayPerView = 0;
	m_QVideo=0;
	m_Credito = 0.0f;
	m_Esib = FALSE;
	m_Titolo = _T("");
	m_Descrizione = _T("");
	m_IDCategoria = 0;
	m_IDTipoSessione = 0;
	m_IDGruppo = 0;
	m_Tariffa = _T("");
	m_TariffaD = _T("");
	m_PasswordSconto = _T("");
	m_Sconto = 0.0f;
	m_ComePago = 0;
	m_IDTipoValuta=0;
	m_IDLingua1=0;
	m_IDLingua2=0;
	m_IDLingua3=0;
	m_IDLingua4=0;
	m_IDLingua5=0;
	m_NomeProAttivo = _T("");
	m_Refer=0;
	m_ReferSpecial=0;
	m_KeySoft = _T("");

	m_Nome1 = _T("");
	m_Presente1 = FALSE;
	m_Eta1 = 0;
	m_Sesso1 =  _T("");
	m_Preferenza1 = _T("");
	m_Altezza1 =  _T("");
	m_Peso1 =  _T("");
	m_ColoreCapelli1 = _T("");
	m_LunghezzaCapelli1 = _T("");
	m_ColoreOcchi1 = _T("");
	m_Corporatura1 = _T("");
	m_Etnia1 = _T("");
	m_Particolarita1 = _T("");
	m_Altro1 = _T("");
	m_Nome2 = _T("");
	m_Presente2 = FALSE;
	m_Eta2 = 0;
	m_Sesso2 =  _T("");
	m_Preferenza2 = _T("");
	m_Altezza2 =  _T("");
	m_Peso2 =  _T("");
	m_ColoreCapelli2 = _T("");
	m_LunghezzaCapelli2 = _T("");
	m_ColoreOcchi2 = _T("");
	m_Corporatura2 = _T("");
	m_Etnia2 = _T("");
	m_Particolarita2 = _T("");
	m_Altro2 = _T("");
	m_nFields = 77;
	//}}AFX_FIELD_INIT
	m_nDefaultType = snapshot;
}


CString CVidsendSet2::GetDefaultConnect()
{
	return _T(theApp.theODBCConnectString);
}

CString CVidsendSet2::GetDefaultSQL() {
	
	return _T("[utenti]");
	}

void CVidsendSet2::DoFieldExchange(CFieldExchange* pFX)
{
	//{{AFX_FIELD_MAP(CVidsendSet2)
	pFX->SetFieldType(CFieldExchange::outputColumn);
	RFX_Long(pFX, _T("[ID]"), m_ID);
	RFX_Text(pFX, _T("[Nome]"), m_Nome);
	RFX_Text(pFX, _T("[Cognome]"), m_Cognome);
	RFX_Date(pFX, _T("[DataNascita]"), m_DataNascita);
	RFX_Text(pFX, _T("[Telefono]"), m_Telefono);
	RFX_Text(pFX, _T("[Fax]"), m_Fax);
	RFX_Byte(pFX, _T("[riservato]"), m_riservato);
	RFX_Text(pFX, _T("[eMail]"), m_eMail);
	RFX_Text(pFX, _T("[Username]"), m_Username);
	RFX_Text(pFX, _T("[Password]"), m_Password);
	RFX_Byte(pFX, _T("[Caratteristiche]"), m_Caratteristiche);
	RFX_Text(pFX, _T("[UltimoIP]"), m_UltimoIP);
	RFX_Text(pFX, _T("[WelcomeMsg]"), m_WelcomeMsg);
	RFX_Long(pFX, _T("[TimedConn]"), m_TimedConn);
	RFX_Byte(pFX, _T("[bTimedConn]"), m_bTimedConn);
	RFX_Byte(pFX, _T("[Abilitato]"), m_Abilitato);
	RFX_Long(pFX, _T("[Gruppo]"), m_Gruppo);
	RFX_Byte(pFX, _T("[RemoteControl]"), m_RemoteControl);
	RFX_Byte(pFX, _T("[DontSave]"), m_DontSave);
	RFX_Byte(pFX, _T("[PayPerView]"), m_PayPerView);
	RFX_Date(pFX, _T("[LoginTime]"), m_LoginTime);
	RFX_Date(pFX, _T("[LogoutTime]"), m_LogoutTime);
	RFX_Byte(pFX, _T("[QVideo]"), m_QVideo);
	RFX_Single(pFX, _T("[Credito]"), m_Credito);
	RFX_Byte(pFX, _T("[Esib]"), m_Esib);
	RFX_Text(pFX, _T("[Titolo]"), m_Titolo);
	RFX_Text(pFX, _T("[Descrizione]"), m_Descrizione);
	RFX_Long(pFX, _T("[IDCategoria]"), m_IDCategoria);
	RFX_Long(pFX, _T("[IDTipoSessione]"), m_IDTipoSessione);
	RFX_Long(pFX, _T("[IDGruppo]"), m_IDGruppo);
	RFX_Text(pFX, _T("[Tariffa]"), m_Tariffa);
	RFX_Text(pFX, _T("[TariffaD]"), m_TariffaD);
	RFX_Text(pFX, _T("[PasswordSconto]"), m_PasswordSconto);
	RFX_Single(pFX, _T("[Sconto]"), m_Sconto);
	RFX_Byte(pFX, _T("[ComePago]"), m_ComePago);
	RFX_Byte(pFX, _T("[IDTipoValuta]"), m_IDTipoValuta);
	RFX_Byte(pFX, _T("[IDLingua1]"), m_IDLingua1);
	RFX_Byte(pFX, _T("[IDLingua2]"), m_IDLingua2);
	RFX_Byte(pFX, _T("[IDLingua3]"), m_IDLingua3);
	RFX_Byte(pFX, _T("[IDLingua4]"), m_IDLingua4);
	RFX_Byte(pFX, _T("[IDLingua5]"), m_IDLingua5);
	RFX_Text(pFX, _T("[NomeProAttivo]"), m_NomeProAttivo);
	RFX_Date(pFX, _T("[DataGuest]"), m_DataGuest);
	RFX_Date(pFX, _T("[DataMember]"), m_DataMember);
	RFX_Long(pFX, _T("[Refer]"), m_Refer);
	RFX_Long(pFX, _T("[ReferSpecial]"), m_ReferSpecial);
	RFX_Date(pFX, _T("[FreeDataC]"), m_FreeDataC);
	RFX_Date(pFX, _T("[FreeDataO]"), m_FreeDataO);
	RFX_Text(pFX, _T("[KeySoft]"), m_KeySoft);

	RFX_Text(pFX, _T("[Nome1]"), m_Nome1);
	RFX_Byte(pFX, _T("[Presente1]"), m_Presente1);
	RFX_Byte(pFX, _T("[Eta1]"), m_Eta1);
	RFX_Text(pFX, _T("[Sesso1]"), m_Sesso1);
	RFX_Text(pFX, _T("[Preferenza1]"), m_Preferenza1);
	RFX_Text(pFX, _T("[Altezza1]"), m_Altezza1);
	RFX_Text(pFX, _T("[Peso1]"), m_Peso1);
	RFX_Text(pFX, _T("[ColoreCapelli1]"), m_ColoreCapelli1);
	RFX_Text(pFX, _T("[LunghezzaCapelli1]"), m_LunghezzaCapelli1);
	RFX_Text(pFX, _T("[ColoreOcchi1]"), m_ColoreOcchi1);
	RFX_Text(pFX, _T("[Corporatura1]"), m_Corporatura1);
	RFX_Text(pFX, _T("[Etnia1]"), m_Etnia1);
	RFX_Text(pFX, _T("[Particolarita1]"), m_Particolarita1);
	RFX_Text(pFX, _T("[Altro1]"), m_Altro1);
	RFX_Text(pFX, _T("[Nome2]"), m_Nome2);
	RFX_Byte(pFX, _T("[Presente2]"), m_Presente2);
	RFX_Byte(pFX, _T("[Eta2]"), m_Eta2);
	RFX_Text(pFX, _T("[Sesso2]"), m_Sesso2);
	RFX_Text(pFX, _T("[Preferenza2]"), m_Preferenza2);
	RFX_Text(pFX, _T("[Altezza2]"), m_Altezza2);
	RFX_Text(pFX, _T("[Peso2]"), m_Peso2);
	RFX_Text(pFX, _T("[ColoreCapelli2]"), m_ColoreCapelli2);
	RFX_Text(pFX, _T("[LunghezzaCapelli2]"), m_LunghezzaCapelli2);
	RFX_Text(pFX, _T("[ColoreOcchi2]"), m_ColoreOcchi2);
	RFX_Text(pFX, _T("[Corporatura2]"), m_Corporatura2);
	RFX_Text(pFX, _T("[Etnia2]"), m_Etnia2);
	RFX_Text(pFX, _T("[Particolarita2]"), m_Particolarita2);
	RFX_Text(pFX, _T("[Altro2]"), m_Altro2);
	//}}AFX_FIELD_MAP
}

/////////////////////////////////////////////////////////////////////////////
// CVidsendSet2 diagnostics

#ifdef _DEBUG
void CVidsendSet2::AssertValid() const
{
	CRecordset::AssertValid();
}

void CVidsendSet2::Dump(CDumpContext& dc) const
{
	CRecordset::Dump(dc);
}
#endif //_DEBUG



/////////////////////////////////////////////////////////////////////////////
// CVidsendSet3

IMPLEMENT_DYNAMIC(CVidsendSet3, CRecordset)

CVidsendSet3::CVidsendSet3(CDatabase* pdb)
	: CRecordsetEx(pdb)
{
	//{{AFX_FIELD_INIT(CVidsendSet3)
	m_ID = 0;
	m_IDUtente = 0;
	m_IP = _T("");
	m_Esib = FALSE;
	m_LockCount = 0;
	m_SessionID=0;
	m_TipoGuest=0;
	m_nFields = 7;
	//}}AFX_FIELD_INIT
	m_nDefaultType = snapshot;
}


CString CVidsendSet3::GetDefaultConnect()
{
	return _T(theApp.theODBCConnectString);
}

CString CVidsendSet3::GetDefaultSQL()
{
	return _T("[utentiOnline]");
}


BOOL CVidsendSet3::goToUtente(DWORD n) {
	char myBuf[128];

	wsprintf(myBuf,"idUtente=%u",n);
	m_strFilter=myBuf;
	if(Requery())
		return !IsEOF();
	else
		return FALSE;
	}

void CVidsendSet3::DoFieldExchange(CFieldExchange* pFX)
{
	//{{AFX_FIELD_MAP(CVidsendSet3)
	pFX->SetFieldType(CFieldExchange::outputColumn);
	RFX_Long(pFX, _T("[ID]"), m_ID);
	RFX_Long(pFX, _T("[IDUtente]"), m_IDUtente);
	RFX_Text(pFX, _T("[IP]"), m_IP);
	RFX_Byte(pFX, _T("[Esib]"), m_Esib);
	RFX_Long(pFX, _T("[LockCount]"), m_LockCount);
	RFX_Long(pFX, _T("[SessionID]"), m_SessionID);
	RFX_Byte(pFX, _T("[TipoGuest]"), m_TipoGuest);
	//}}AFX_FIELD_MAP
}

/////////////////////////////////////////////////////////////////////////////
// CVidsendSet3 diagnostics

#ifdef _DEBUG
void CVidsendSet3::AssertValid() const
{
	CRecordset::AssertValid();
}

void CVidsendSet3::Dump(CDumpContext& dc) const
{
	CRecordset::Dump(dc);
}
#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CVidsendSet4

IMPLEMENT_DYNAMIC(CVidsendSet4, CRecordset)

CVidsendSet4::CVidsendSet4(CDatabase* pdb)
	: CRecordsetEx(pdb)
{
	//{{AFX_FIELD_INIT(CVidsendSet4)
	m_ID = 0;
	m_IDOsserv = 0;
	m_IDEspos = 0;
	m_Tariffa = _T("");
	m_Girato = FALSE;
	m_ComePago = 0;
	m_SessionID=0;
	m_IPOsserv = _T("");
	m_IPEspos = _T("");
	m_Chiusa = 0;
	m_nFields = 13;
	//}}AFX_FIELD_INIT
	m_nDefaultType = snapshot;
}


CString CVidsendSet4::GetDefaultConnect()
{
	return _T(theApp.theODBCConnectString);
}

CString CVidsendSet4::GetDefaultSQL()
{
	return _T("[logChiamate]");
}

void CVidsendSet4::DoFieldExchange(CFieldExchange* pFX)
{
	//{{AFX_FIELD_MAP(CVidsendSet4)
	pFX->SetFieldType(CFieldExchange::outputColumn);
	RFX_Long(pFX, _T("[ID]"), m_ID);
	RFX_Long(pFX, _T("[IDOsserv]"), m_IDOsserv);
	RFX_Long(pFX, _T("[IDEspos]"), m_IDEspos);
	RFX_Date(pFX, _T("[OraInizio]"), m_OraInizio);
	RFX_Date(pFX, _T("[OraFine]"), m_OraFine);
	RFX_Text(pFX, _T("[Tariffa]"), m_Tariffa);
	RFX_Date(pFX, _T("[DataPagamento]"), m_DataPagamento);
	RFX_Byte(pFX, _T("[Girato]"), m_Girato);
	RFX_Byte(pFX, _T("[Comepago]"), m_ComePago);
	RFX_Long(pFX, _T("[SessionID]"), m_SessionID);
	RFX_Text(pFX, _T("[IPOsserv]"), m_IPOsserv);
	RFX_Text(pFX, _T("[IPEspos]"), m_IPEspos);
	RFX_Byte(pFX, _T("[Chiusa]"), m_Chiusa);
	//}}AFX_FIELD_MAP
}

/////////////////////////////////////////////////////////////////////////////
// CVidsendSet4 diagnostics

#ifdef _DEBUG
void CVidsendSet4::AssertValid() const
{
	CRecordset::AssertValid();
}

void CVidsendSet4::Dump(CDumpContext& dc) const
{
	CRecordset::Dump(dc);
}
#endif //_DEBUG


/////////////////////////////////////////////////////////////////////////////
// CVidsendSet5

IMPLEMENT_DYNAMIC(CVidsendSet5, CRecordset)

CVidsendSet5::CVidsendSet5(CDatabase* pdb)
	: CRecordsetEx(pdb)
{
	//{{AFX_FIELD_INIT(CVidsendSet5)
	m_ID = 0;
	m_IDUtente = 0;
	m_IP = _T("");
	m_SessionID = 0;
	m_Versione = 0;
	m_VersioneW = 0;
	m_SerNum= _T("");
	m_nFields = 10;
	//}}AFX_FIELD_INIT
	m_nDefaultType = snapshot;
}


CString CVidsendSet5::GetDefaultConnect()
{
	return _T(theApp.theODBCConnectString);
}

CString CVidsendSet5::GetDefaultSQL()
{
	return _T("[logConnessioni]");
}

void CVidsendSet5::DoFieldExchange(CFieldExchange* pFX)
{
	//{{AFX_FIELD_MAP(CVidsendSet5)
	pFX->SetFieldType(CFieldExchange::outputColumn);
	RFX_Long(pFX, _T("[ID]"), m_ID);
	RFX_Long(pFX, _T("[IDUtente]"), m_IDUtente);
	RFX_Date(pFX, _T("[OraInizio]"), m_OraInizio);
	RFX_Date(pFX, _T("[OraFine]"), m_OraFine);
	RFX_Date(pFX, _T("[OraCorrente]"), m_OraCorrente);
	RFX_Text(pFX, _T("[IP]"), m_IP);
	RFX_Long(pFX, _T("[SessionID]"), m_SessionID);
	RFX_Long(pFX, _T("[versione]"), m_Versione);
	RFX_Long(pFX, _T("[versioneW]"), m_VersioneW);
	RFX_Text(pFX, _T("[SerNum]"), m_SerNum);
	//}}AFX_FIELD_MAP
}

/////////////////////////////////////////////////////////////////////////////
// CVidsendSet5 diagnostics

#ifdef _DEBUG
void CVidsendSet5::AssertValid() const
{
	CRecordset::AssertValid();
}

void CVidsendSet5::Dump(CDumpContext& dc) const
{
	CRecordset::Dump(dc);
}
#endif //_DEBUG



/////////////////////////////////////////////////////////////////////////////
// CVidsendSet5Ex

IMPLEMENT_DYNAMIC(CVidsendSet5Ex, CRecordset)

CVidsendSet5Ex::CVidsendSet5Ex(CDatabase* pdb)
	: CRecordsetEx(pdb)
{
	//{{AFX_FIELD_INIT(CVidsendSet5Ex)
	m_ID = 0;
	m_IDUtente = 0;
	m_IP = _T("");
	m_SessionID = 0;
	m_Versione = 0;
	m_VersioneW = 0;
	m_SerNum= _T("");
	m_nFields = 10;
	//}}AFX_FIELD_INIT
	m_nDefaultType = snapshot;
}


CString CVidsendSet5Ex::GetDefaultConnect()
{
	return _T(theApp.theODBCConnectString);
}

CString CVidsendSet5Ex::GetDefaultSQL()
{
	return _T("[LogConnessioniEx]");
}

void CVidsendSet5Ex::DoFieldExchange(CFieldExchange* pFX)
{
	//{{AFX_FIELD_MAP(CVidsendSet5Ex)
	pFX->SetFieldType(CFieldExchange::outputColumn);
	RFX_Long(pFX, _T("[ID]"), m_ID);
	RFX_Long(pFX, _T("[IDUtente]"), m_IDUtente);
	RFX_Date(pFX, _T("[OraInizio]"), m_OraInizio);
	RFX_Date(pFX, _T("[OraFine]"), m_OraFine);
	RFX_Date(pFX, _T("[OraCorrente]"), m_OraCorrente);
	RFX_Text(pFX, _T("[IP]"), m_IP);
	RFX_Long(pFX, _T("[SessionID]"), m_SessionID);
	RFX_Long(pFX, _T("[versione]"), m_Versione);
	RFX_Long(pFX, _T("[versioneW]"), m_VersioneW);
	RFX_Text(pFX, _T("[SerNum]"), m_SerNum);
	//}}AFX_FIELD_MAP
}

/////////////////////////////////////////////////////////////////////////////
// CVidsendSet5Ex diagnostics

#ifdef _DEBUG
void CVidsendSet5Ex::AssertValid() const
{
	CRecordset::AssertValid();
}

void CVidsendSet5Ex::Dump(CDumpContext& dc) const
{
	CRecordset::Dump(dc);
}
#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CVidsendSet2_

IMPLEMENT_DYNAMIC(CVidsendSet2_, CRecordset)

CVidsendSet2_::CVidsendSet2_(CDatabase* pdb)
	: CRecordsetEx(pdb)
{
	//{{AFX_FIELD_INIT(CVidsendSet2_)
	m_ID = 0;
	m_Nome = _T("");
	m_Cognome = _T("");
	m_Telefono = _T("");
	m_Fax = _T("");
	m_riservato = FALSE;
	m_eMail = _T("");
	m_Username = _T("");
	m_Password = _T("");
	m_Caratteristiche = 0;
	m_UltimoIP = _T("");
	m_WelcomeMsg = _T("");
	m_bTimedConn = FALSE;
	m_TimedConn=0;
	m_Abilitato = FALSE;
	m_Gruppo = 0;
	m_RemoteControl = 0;
	m_DontSave = 0;
	m_PayPerView = 0;
	m_Credito = 0.0f;
	m_Esib = FALSE;
	m_Tariffa = _T("");
	m_TariffaD = _T("");
	m_PasswordSconto = _T("");
	m_Sconto = 0.0f;
	m_ComePago = 0;
	m_nFields = 30;
	//}}AFX_FIELD_INIT
	m_nDefaultType = snapshot;
}


CString CVidsendSet2_::GetDefaultConnect()
{
	return _T(theApp.theODBCConnectString);
}

CString CVidsendSet2_::GetDefaultSQL()
{
	return _T("select ID,Nome,Cognome,DataNascita,Telefono,Fax,riservato,eMail,Username,Password,Caratteristiche,UltimoIP,WelcomeMsg,TimedConn,bTimedConn,Abilitato,Gruppo,RemoteControl,DontSave,PayPerView,LoginTime,LogoutTime,Credito,Esib,QVideo,Tariffa,TariffaD,PasswordSconto,Sconto,ComePago from utenti");
}

void CVidsendSet2_::DoFieldExchange(CFieldExchange* pFX)
{
	//{{AFX_FIELD_MAP(CVidsendSet2_)
	pFX->SetFieldType(CFieldExchange::outputColumn);
	RFX_Long(pFX, _T("[ID]"), m_ID);
	RFX_Text(pFX, _T("[Nome]"), m_Nome);
	RFX_Text(pFX, _T("[Cognome]"), m_Cognome);
	RFX_Date(pFX, _T("[DataNascita]"), m_DataNascita);
	RFX_Text(pFX, _T("[Telefono]"), m_Telefono);
	RFX_Text(pFX, _T("[Fax]"), m_Fax);
	RFX_Byte(pFX, _T("[riservato]"), m_riservato);
	RFX_Text(pFX, _T("[eMail]"), m_eMail);
	RFX_Text(pFX, _T("[Username]"), m_Username);
	RFX_Text(pFX, _T("[Password]"), m_Password);
	RFX_Byte(pFX, _T("[Caratteristiche]"), m_Caratteristiche);
	RFX_Text(pFX, _T("[UltimoIP]"), m_UltimoIP);
	RFX_Text(pFX, _T("[WelcomeMsg]"), m_WelcomeMsg);
	RFX_Long(pFX, _T("[TimedConn]"), m_TimedConn);
	RFX_Byte(pFX, _T("[bTimedConn]"), m_bTimedConn);
	RFX_Byte(pFX, _T("[Abilitato]"), m_Abilitato);
	RFX_Long(pFX, _T("[Gruppo]"), m_Gruppo);
	RFX_Byte(pFX, _T("[RemoteControl]"), m_RemoteControl);
	RFX_Byte(pFX, _T("[DontSave]"), m_DontSave);
	RFX_Byte(pFX, _T("[PayPerView]"), m_PayPerView);
	RFX_Date(pFX, _T("[LoginTime]"), m_LoginTime);
	RFX_Date(pFX, _T("[LogoutTime]"), m_LogoutTime);
	RFX_Single(pFX, _T("[Credito]"), m_Credito);
	RFX_Byte(pFX, _T("[Esib]"), m_Esib);
	RFX_Byte(pFX, _T("[QVideo]"), m_QVideo);
	RFX_Text(pFX, _T("[Tariffa]"), m_Tariffa);
	RFX_Text(pFX, _T("[TariffaD]"), m_TariffaD);
	RFX_Text(pFX, _T("[PasswordSconto]"), m_PasswordSconto);
	RFX_Single(pFX, _T("[Sconto]"), m_Sconto);
	RFX_Byte(pFX, _T("[ComePago]"), m_ComePago);
	//}}AFX_FIELD_MAP
}

/////////////////////////////////////////////////////////////////////////////
// CVidsendSet2_ diagnostics

#ifdef _DEBUG
void CVidsendSet2_::AssertValid() const
{
	CRecordset::AssertValid();
}

void CVidsendSet2_::Dump(CDumpContext& dc) const
{
	CRecordset::Dump(dc);
}
#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CGenericSet

IMPLEMENT_DYNAMIC(CGenericSet, CRecordset)

CGenericSet::CGenericSet(char *table,CDatabase* pdb)
	: CRecordset(pdb)
{
	//{{AFX_FIELD_INIT(CGenericSet)
	m_C = _T("");
	m_nFields = 1;
	//}}AFX_FIELD_INIT
	m_nDefaultType = snapshot;
	if(table)
		Table=table;
	}


CString CGenericSet::GetDefaultConnect() {
	return _T(theApp.theODBCConnectString);
	}

CString CGenericSet::GetDefaultSQL() {
	CString S="select count(*) from ";
	return S+Table;
	}

void CGenericSet::DoFieldExchange(CFieldExchange* pFX) {
	//{{AFX_FIELD_MAP(CGenericSet)
	pFX->SetFieldType(CFieldExchange::outputColumn);
	RFX_Text(pFX, _T("[c]"), m_C);
	//}}AFX_FIELD_MAP
	}

/////////////////////////////////////////////////////////////////////////////
// CGenericSet diagnostics

#ifdef _DEBUG
void CGenericSet::AssertValid() const
{
	CRecordset::AssertValid();
}

void CGenericSet::Dump(CDumpContext& dc) const
{
	CRecordset::Dump(dc);
}
#endif //_DEBUG
/////////////////////////////////////////////////////////////////////////////
// CVidsendSet6

IMPLEMENT_DYNAMIC(CVidsendSet6, CRecordset)

CVidsendSet6::CVidsendSet6(CDatabase* pdb)
	: CRecordsetEx(pdb)
{
	//{{AFX_FIELD_INIT(CVidsendSet6)
	m_FTPServer = _T("");
	m_FTPLogin = _T("");
	m_FTPPassword = _T("");
	m_nFields = 3;
	//}}AFX_FIELD_INIT
	m_nDefaultType = snapshot;
}


CString CVidsendSet6::GetDefaultConnect()
{
	return _T(theApp.theODBCConnectString);
}

CString CVidsendSet6::GetDefaultSQL()
{
	return _T("[Server]");
}

void CVidsendSet6::DoFieldExchange(CFieldExchange* pFX)
{
	//{{AFX_FIELD_MAP(CVidsendSet6)
	pFX->SetFieldType(CFieldExchange::outputColumn);
	RFX_Text(pFX, _T("[FTPServer]"), m_FTPServer);
	RFX_Text(pFX, _T("[FTPLogin]"), m_FTPLogin);
	RFX_Text(pFX, _T("[FTPPassword]"), m_FTPPassword);
	//}}AFX_FIELD_MAP
}

/////////////////////////////////////////////////////////////////////////////
// CVidsendSet6 diagnostics

#ifdef _DEBUG
void CVidsendSet6::AssertValid() const
{
	CRecordset::AssertValid();
}

void CVidsendSet6::Dump(CDumpContext& dc) const
{
	CRecordset::Dump(dc);
}
#endif //_DEBUG
/////////////////////////////////////////////////////////////////////////////
// CVidsendSet8

IMPLEMENT_DYNAMIC(CVidsendSet8, CRecordset)

CVidsendSet8::CVidsendSet8(CDatabase* pdb)
	: CRecordsetEx(pdb)
{
	//{{AFX_FIELD_INIT(CVidsendSet8)
	m_ID = 0;
	m_IDUtente = 0;
	m_IDEsib = 0;
	m_IP = _T("");
	m_SerNum = _T("");
	m_AccessoChat = 0;
	m_AccessoSis = 0;
	m_ContatoreAccessi = 0;
	m_nFields = 10;
	//}}AFX_FIELD_INIT
	m_nDefaultType = snapshot;
}


CString CVidsendSet8::GetDefaultConnect()
{
	return _T(theApp.theODBCConnectString);
}

CString CVidsendSet8::GetDefaultSQL()
{
	return _T("[SerialNum]");
}

void CVidsendSet8::DoFieldExchange(CFieldExchange* pFX)
{
	//{{AFX_FIELD_MAP(CVidsendSet8)
	pFX->SetFieldType(CFieldExchange::outputColumn);
	RFX_Long(pFX, _T("[ID]"), m_ID);
	RFX_Long(pFX, _T("[IDUtente]"), m_IDUtente);
	RFX_Long(pFX, _T("[IDEsib]"), m_IDEsib);
	RFX_Text(pFX, _T("[IP]"), m_IP);
	RFX_Text(pFX, _T("[SerNum]"), m_SerNum);
	RFX_Byte(pFX, _T("[AccessoChat]"), m_AccessoChat);
	RFX_Byte(pFX, _T("[AccessoSis]"), m_AccessoSis);
	RFX_Date(pFX, _T("[DataAccesso]"), m_DataAccesso);
	RFX_Date(pFX, _T("[OraAccesso]"), m_OraAccesso);
	RFX_Long(pFX, _T("[ContatoreAccessi]"), m_ContatoreAccessi);
	//}}AFX_FIELD_MAP
}

/////////////////////////////////////////////////////////////////////////////
// CVidsendSet8 diagnostics

#ifdef _DEBUG
void CVidsendSet8::AssertValid() const
{
	CRecordset::AssertValid();
}

void CVidsendSet8::Dump(CDumpContext& dc) const
{
	CRecordset::Dump(dc);
}
#endif //_DEBUG
