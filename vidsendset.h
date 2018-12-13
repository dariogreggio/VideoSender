// vidsend2Set.h : interface of the CVidsendSet class
//
/////////////////////////////////////////////////////////////////////////////

#if !defined(AFX_VIDSEND2SET_H__242A4EB0_2072_11D4_94A8_00A0C9AFFE49__INCLUDED_)
#define AFX_VIDSEND2SET_H__242A4EB0_2072_11D4_94A8_00A0C9AFFE49__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

class CRecordsetEx : public CRecordset {
public:
	CRecordsetEx(CDatabase *pdb) { CRecordset::CRecordset(pdb);};
	BOOL goTo(DWORD );
	DWORD getCount();
	};


class CVidsendSet : public CRecordsetEx {
public:
	CVidsendSet(CDatabase* pDatabase = NULL);
	DECLARE_DYNAMIC(CVidsendSet)

// Field/Param Data
	//{{AFX_FIELD(CVidsendSet, CRecordset)
	long	m_ID;
	CString	m_INDIRIZZO;
	CTime	m_DATA;
	CString	m_DESCRIZIONE;
	BYTE m_LIVELLO;
	//}}AFX_FIELD

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CVidsendSet)
	public:
	virtual CString GetDefaultConnect();	// Default connection string
	virtual CString GetDefaultSQL(); 	// default SQL for Recordset
	virtual void DoFieldExchange(CFieldExchange* pFX);	// RFX support
	//}}AFX_VIRTUAL

// Implementation
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

};

/////////////////////////////////////////////////////////////////////////////
// CVidsendSet2 recordset

class CVidsendSet2 : public CRecordsetEx {
public:
	CVidsendSet2(CDatabase* pDatabase = NULL);
	DECLARE_DYNAMIC(CVidsendSet2)

// Field/Param Data
	//{{AFX_FIELD(CVidsendSet2, CRecordset)
	long	m_ID;
	CString	m_Nome;
	CString	m_Cognome;
	CTime	m_DataNascita;
	CString	m_Telefono;
	CString	m_Fax;
	BYTE	m_riservato;
	CString	m_eMail;
	CString	m_Username;
	CString	m_Password;
	BYTE	m_Caratteristiche;
	CString	m_UltimoIP;
	CString	m_WelcomeMsg;
	long	m_TimedConn;
	BYTE	m_bTimedConn;
	BYTE	m_Abilitato;
	long	m_Gruppo;
	BYTE	m_RemoteControl;
	BYTE	m_DontSave;
	BYTE	m_PayPerView;
	CTime	m_LoginTime;
	CTime	m_LogoutTime;
	BYTE	m_QVideo;
	float	m_Credito;
	BYTE	m_Esib;
	CString	m_Titolo;
	CString	m_Descrizione;
	long	m_IDCategoria;
	long	m_IDTipoSessione;
	long  m_IDGruppo;
	CString	m_Tariffa;
	CString	m_TariffaD;
	CString	m_PasswordSconto;
	float	m_Sconto;
	BYTE	m_ComePago;
	BYTE	m_IDTipoValuta;
	BYTE	m_IDLingua1;
	BYTE	m_IDLingua2;
	BYTE	m_IDLingua3;
	BYTE	m_IDLingua4;
	BYTE	m_IDLingua5;
	CString m_NomeProAttivo;
	CTime m_DataGuest;
	CTime m_DataMember;
	long  m_Refer;
	long  m_ReferSpecial;
	CTime m_FreeDataC;
	CTime m_FreeDataO;
	CString m_KeySoft;

	CString	m_Nome1;
	BYTE	m_Presente1;
	BYTE	m_Eta1;
	CString	m_Sesso1;
	CString	m_Preferenza1;
	CString	m_Altezza1;
	CString	m_Peso1;
	CString	m_ColoreCapelli1;
	CString	m_LunghezzaCapelli1;
	CString	m_ColoreOcchi1;
	CString	m_Corporatura1;
	CString	m_Etnia1;
	CString	m_Particolarita1;
	CString	m_Altro1;
	CString	m_Nome2;
	BYTE	m_Presente2;
	BYTE	m_Eta2;
	CString	m_Sesso2;
	CString	m_Preferenza2;
	CString	m_Altezza2;
	CString	m_Peso2;
	CString	m_ColoreCapelli2;
	CString	m_LunghezzaCapelli2;
	CString	m_ColoreOcchi2;
	CString	m_Corporatura2;
	CString	m_Etnia2;
	CString	m_Particolarita2;
	CString	m_Altro2;
	//}}AFX_FIELD


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CVidsendSet2)
	public:
	virtual CString GetDefaultConnect();    // Default connection string
	virtual CString GetDefaultSQL();    // Default SQL for Recordset
	virtual void DoFieldExchange(CFieldExchange* pFX);  // RFX support
	//}}AFX_VIRTUAL

// Implementation
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif
};

/////////////////////////////////////////////////////////////////////////////
// CVidsendSet3 recordset

class CVidsendSet3 : public CRecordsetEx {
public:
	CVidsendSet3(CDatabase* pDatabase = NULL);
	DECLARE_DYNAMIC(CVidsendSet3)
	BOOL goToUtente(DWORD );

// Field/Param Data
	//{{AFX_FIELD(CVidsendSet3, CRecordset)
	long	m_ID;
	long	m_IDUtente;
	CString	m_IP;
	BYTE	m_Esib;
	long	m_LockCount;
	long	m_SessionID;
	BYTE  m_TipoGuest;
	//}}AFX_FIELD


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CVidsendSet3)
	public:
	virtual CString GetDefaultConnect();    // Default connection string
	virtual CString GetDefaultSQL();    // Default SQL for Recordset
	virtual void DoFieldExchange(CFieldExchange* pFX);  // RFX support
	//}}AFX_VIRTUAL

// Implementation
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif
};

/////////////////////////////////////////////////////////////////////////////
// CVidsendSet4 recordset
// LogChiamate

class CVidsendSet4 : public CRecordsetEx {
public:
	CVidsendSet4(CDatabase* pDatabase = NULL);
	DECLARE_DYNAMIC(CVidsendSet4)

// Field/Param Data
	//{{AFX_FIELD(CVidsendSet4, CRecordset)
	long	m_ID;
	long	m_IDOsserv;
	long	m_IDEspos;
	CTime	m_OraInizio;
	CTime	m_OraFine;
	CString	m_Tariffa;
	CTime	m_DataPagamento;
	BYTE	m_Girato;
	BYTE	m_ComePago;
	long	m_SessionID;
	CString	m_IPOsserv;
	CString	m_IPEspos;
	BYTE	m_Chiusa;
	//}}AFX_FIELD


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CVidsendSet4)
	public:
	virtual CString GetDefaultConnect();    // Default connection string
	virtual CString GetDefaultSQL();    // Default SQL for Recordset
	virtual void DoFieldExchange(CFieldExchange* pFX);  // RFX support
	//}}AFX_VIRTUAL

// Implementation
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif
};

/////////////////////////////////////////////////////////////////////////////
// CVidsendSet5 recordset
// LogConnessioni

class CVidsendSet5 : public CRecordsetEx {
public:
	CVidsendSet5(CDatabase* pDatabase = NULL);
	DECLARE_DYNAMIC(CVidsendSet5)

// Field/Param Data
	//{{AFX_FIELD(CVidsendSet5, CRecordset)
	long	m_ID;
	long	m_IDUtente;
	CTime	m_OraInizio;
	CTime	m_OraFine;
	CTime	m_OraCorrente;
	CString	m_IP;
	long	m_SessionID;
	long	m_Versione;
	long	m_VersioneW;
	CString	m_SerNum;
	//}}AFX_FIELD


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CVidsendSet5)
	public:
	virtual CString GetDefaultConnect();    // Default connection string
	virtual CString GetDefaultSQL();    // Default SQL for Recordset
	virtual void DoFieldExchange(CFieldExchange* pFX);  // RFX support
	//}}AFX_VIRTUAL

// Implementation
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif
};


/////////////////////////////////////////////////////////////////////////////
// CVidsendSet5Ex recordset
// LogConnessioniEx

class CVidsendSet5Ex : public CRecordsetEx
{
public:
	CVidsendSet5Ex(CDatabase* pDatabase = NULL);
	DECLARE_DYNAMIC(CVidsendSet5Ex)

// Field/Param Data
	//{{AFX_FIELD(CVidsendSet5Ex, CRecordset)
	long	m_ID;
	long	m_IDUtente;
	CTime	m_OraInizio;
	CTime	m_OraFine;
	CTime	m_OraCorrente;
	CString	m_IP;
	long	m_SessionID;
	long	m_Versione;
	long	m_VersioneW;
	CString	m_SerNum;
	//}}AFX_FIELD


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CVidsendSet5Ex)
	public:
	virtual CString GetDefaultConnect();    // Default connection string
	virtual CString GetDefaultSQL();    // Default SQL for Recordset
	virtual void DoFieldExchange(CFieldExchange* pFX);  // RFX support
	//}}AFX_VIRTUAL

// Implementation
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif
};

/////////////////////////////////////////////////////////////////////////////
// CVidsendSet6 recordset

class CVidsendSet6 : public CRecordsetEx
{
public:
	CVidsendSet6(CDatabase* pDatabase = NULL);
	DECLARE_DYNAMIC(CVidsendSet6)

// Field/Param Data
	//{{AFX_FIELD(CVidsendSet6, CRecordset)
	CString	m_FTPServer;
	CString	m_FTPLogin;
	CString	m_FTPPassword;
	//}}AFX_FIELD


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CVidsendSet6)
	public:
	virtual CString GetDefaultConnect();    // Default connection string
	virtual CString GetDefaultSQL();    // Default SQL for Recordset
	virtual void DoFieldExchange(CFieldExchange* pFX);  // RFX support
	//}}AFX_VIRTUAL

// Implementation
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif
};
/////////////////////////////////////////////////////////////////////////////
// CVidsendSet8 recordset

class CVidsendSet8 : public CRecordsetEx
{
public:
	CVidsendSet8(CDatabase* pDatabase = NULL);
	DECLARE_DYNAMIC(CVidsendSet8)

// Field/Param Data
	//{{AFX_FIELD(CVidsendSet8, CRecordset)
	long	m_ID;
	long	m_IDUtente;
	long	m_IDEsib;
	CString	m_IP;
	CString	m_SerNum;
	BYTE	m_AccessoChat;
	BYTE	m_AccessoSis;
	CTime	m_DataAccesso;
	CTime	m_OraAccesso;
	long	m_ContatoreAccessi;
	//}}AFX_FIELD


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CVidsendSet8)
	public:
	virtual CString GetDefaultConnect();    // Default connection string
	virtual CString GetDefaultSQL();    // Default SQL for Recordset
	virtual void DoFieldExchange(CFieldExchange* pFX);  // RFX support
	//}}AFX_VIRTUAL

// Implementation
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif
};
//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.


/////////////////////////////////////////////////////////////////////////////
// CVidsendSet2_ recordset

class CVidsendSet2_ : public CRecordsetEx {
public:
	CVidsendSet2_(CDatabase* pDatabase = NULL);
	DECLARE_DYNAMIC(CVidsendSet2_)

// Field/Param Data
	//{{AFX_FIELD(CVidsendSet2_, CRecordset)
	long	m_ID;
	CString	m_Nome;
	CString	m_Cognome;
	CTime	m_DataNascita;
	CString	m_Telefono;
	CString	m_Fax;
	BYTE	m_riservato;
	CString	m_eMail;
	CString	m_Username;
	CString	m_Password;
	BYTE	m_Caratteristiche;
	CString	m_UltimoIP;
	CString	m_WelcomeMsg;
	long	m_TimedConn;
	BYTE	m_bTimedConn;
	BYTE	m_Abilitato;
	long	m_Gruppo;
	BYTE	m_RemoteControl;
	BYTE	m_DontSave;
	BYTE	m_PayPerView;
	CTime	m_LoginTime;
	CTime	m_LogoutTime;
	float	m_Credito;
	BYTE	m_Esib;
	BYTE	m_QVideo;
	CString	m_Tariffa;
	CString	m_TariffaD;
	CString	m_PasswordSconto;
	float	m_Sconto;
	BYTE	m_ComePago;
	//}}AFX_FIELD


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CVidsendSet2_)
	public:
	virtual CString GetDefaultConnect();    // Default connection string
	virtual CString GetDefaultSQL();    // Default SQL for Recordset
	virtual void DoFieldExchange(CFieldExchange* pFX);  // RFX support
	//}}AFX_VIRTUAL

// Implementation
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif
};


/////////////////////////////////////////////////////////////////////////////
// CGenericSet recordset

class CGenericSet : public CRecordset {
public:
	CGenericSet(char *table,CDatabase* pDatabase = NULL);
	DECLARE_DYNAMIC(CGenericSet)

// Field/Param Data
	//{{AFX_FIELD(CGenericSet, CRecordset)
	CString	m_C;
	//}}AFX_FIELD

	CString Table;

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CGenericSet)
	public:
	virtual CString GetDefaultConnect();    // Default connection string
	virtual CString GetDefaultSQL();    // Default SQL for Recordset
	virtual void DoFieldExchange(CFieldExchange* pFX);  // RFX support
	//}}AFX_VIRTUAL

// Implementation
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif
};



//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_VIDSEND2SET_H__242A4EB0_2072_11D4_94A8_00A0C9AFFE49__INCLUDED_)

