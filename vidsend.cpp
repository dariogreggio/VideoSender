// vidsend2.cpp : Defines the class behaviors for the application.
//

#include "stdafx.h"
#include "vidsend.h"

#include "MainFrm.h"
#include "ChildFrm.h"
#include "vidsendSet.h"
#include "vidsendDoc.h"
#include "vidsendView.h"
#include "vidsendDialog.h"
#include "vidsendlog.h"
#include "math.h"
#ifndef _NEWMEET_MODE
#include "ping.h"
#endif
#include <cjpeg2.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

//#define PI 3.1415926



/////////////////////////////////////////////////////////////////////////////
// CVidsendApp

BEGIN_MESSAGE_MAP(CVidsendApp, CWinApp)
	//{{AFX_MSG_MAP(CVidsendApp)
	ON_COMMAND(ID_APP_ABOUT, OnAppAbout)
	ON_COMMAND(ID_FILE_NEW, OnFileNew)
	ON_COMMAND(ID_FILE_NUOVO_BROWSER, OnFileNuovoBrowser)
	ON_UPDATE_COMMAND_UI(ID_FILE_NUOVO_BROWSER, OnUpdateFileNuovoBrowser)
	ON_COMMAND(ID_FILE_NUOVO_CONNESSIONI, OnFileNuovoConnessioni)
	ON_UPDATE_COMMAND_UI(ID_FILE_NUOVO_CONNESSIONI, OnUpdateFileNuovoConnessioni)
	ON_COMMAND(ID_FILE_NUOVO_LOG, OnFileNuovoLog)
	ON_UPDATE_COMMAND_UI(ID_FILE_NUOVO_LOG, OnUpdateFileNuovoLog)
	ON_COMMAND(ID_FILE_NUOVO_VIDEOSERVER, OnFileNuovoVideoserver)
	ON_UPDATE_COMMAND_UI(ID_FILE_NUOVO_VIDEOSERVER, OnUpdateFileNuovoVideoserver)
	ON_COMMAND(ID_FILE_NUOVO_VIDEOCLIENT, OnFileNuovoVideoclient)
	ON_UPDATE_COMMAND_UI(ID_FILE_NUOVO_VIDEOCLIENT, OnUpdateFileNuovoVideoclient)
	ON_COMMAND(ID_FILE_CONFIG, OnFileConfig)
	ON_COMMAND(ID_FILE_NUOVO_SERVERDISPONIBILI, OnFileNuovoServerdisponibili)
	ON_UPDATE_COMMAND_UI(ID_FILE_NUOVO_SERVERDISPONIBILI, OnUpdateFileNuovoServerdisponibili)
	ON_COMMAND(ID_FILE_NUOVO_CHAT, OnFileNuovoChat)
	ON_UPDATE_COMMAND_UI(ID_FILE_NUOVO_CHAT, OnUpdateFileNuovoChat)
#ifdef _NEWMEET_MODE
	ON_COMMAND(ID_FILE_IMPOSTAZIONIFORMATOVIDEO, OnFileImpostazioniformatovideo)
	ON_UPDATE_COMMAND_UI(ID_FILE_IMPOSTAZIONIFORMATOVIDEO, OnUpdateFileImpostazioniformatovideo)
	ON_COMMAND(ID_FILE_IMPOSTAZIONISORGENTEVIDEO, OnFileImpostazionisorgentevideo)
	ON_UPDATE_COMMAND_UI(ID_FILE_IMPOSTAZIONISORGENTEVIDEO, OnUpdateFileImpostazionisorgentevideo)
	ON_COMMAND(ID_FILE_IMPOSTAZIONISTREAMING, OnFileImpostazionistreaming)
	ON_UPDATE_COMMAND_UI(ID_FILE_IMPOSTAZIONISTREAMING, OnUpdateFileImpostazionistreaming)
	ON_COMMAND(ID_FILE_SAVE_VIDEO, OnFileSaveVideo)
	ON_UPDATE_COMMAND_UI(ID_FILE_SAVE_VIDEO, OnUpdateFileSaveVideo)
	ON_COMMAND(ID_FILE_SAVE_FOTOGRAMMA, OnFileSaveFotogramma)
	ON_UPDATE_COMMAND_UI(ID_FILE_SAVE_FOTOGRAMMA, OnUpdateFileSaveFotogramma)
	ON_COMMAND(ID_FILE_SAVE_FOTOGRAMMA2, OnFileSaveFotogramma2)
	ON_UPDATE_COMMAND_UI(ID_FILE_SAVE_FOTOGRAMMA2, OnUpdateFileSaveFotogramma2)
	ON_COMMAND(ID_FILE_ARCHIVIOIMMAGINI, OnFileArchivioimmagini)
	ON_UPDATE_COMMAND_UI(ID_FILE_ARCHIVIOIMMAGINI, OnUpdateFileArchivioimmagini)
	ON_COMMAND(ID_OPZIONI_CHAT, OnOpzioniChat)
	ON_UPDATE_COMMAND_UI(ID_OPZIONI_CHAT, OnUpdateOpzioniChat)
	ON_COMMAND(ID_OPZIONI_VARIE, OnOpzioniVarie)
	ON_UPDATE_COMMAND_UI(ID_OPZIONI_VARIE, OnUpdateOpzioniVarie)
	ON_COMMAND(ID_OPZIONI_VIDEO, OnOpzioniVideo)
	ON_UPDATE_COMMAND_UI(ID_OPZIONI_VIDEO, OnUpdateOpzioniVideo)
	ON_COMMAND(ID_VIDEO_TRASMISSIONE_DALVIVO, OnVideoTrasmissioneDalvivo)
	ON_UPDATE_COMMAND_UI(ID_VIDEO_TRASMISSIONE_DALVIVO, OnUpdateVideoTrasmissioneDalvivo)
	ON_COMMAND(ID_VIDEO_TRASMISSIONE_FILMATO, OnVideoTrasmissioneFilmato)
	ON_UPDATE_COMMAND_UI(ID_VIDEO_TRASMISSIONE_FILMATO, OnUpdateVideoTrasmissioneFilmato)
	ON_COMMAND(ID_INFO, OnInfo)
	ON_COMMAND(ID_HELPDESK, OnHelpDesk)
	ON_COMMAND(ID_DISCONNETTI, OnDisconnetti)
#endif
	//}}AFX_MSG_MAP
	// Standard file based document commands
	ON_COMMAND(ID_FILE_OPEN, CWinApp::OnFileOpen)
	// Standard print setup command
	ON_COMMAND(ID_FILE_PRINT_SETUP, CWinApp::OnFilePrintSetup)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CVidsendApp construction

CVidsendApp::CVidsendApp() {
	register int i;
	
	infoUtenteKey="InfoUtente";
	FileSpool=NULL;
	hRasConn=NULL;
	RASstate=RASiState=0;
	for(i=0; i<16; i++)
		aClient[i]=NULL;
	theServer=NULL;
	theLog=NULL;
	for(i=0; i<16; i++)
		aBrowser[i]=NULL;
	theConnections=NULL;
	authSocket=NULL;
	dirSocket=NULL;
	timeSocket=NULL;
	wwwSocket=NULL;
	serverWWWPort=80;
	bRemoteAccessEnabled=TRUE;

	szDevice=szDriver=szOutput=szTitle=NULL;
	hInitData=0;
	dInfo=NULL;
	devMode=NULL;

	}

/////////////////////////////////////////////////////////////////////////////
// The one and only CVidsendApp object

CVidsendApp theApp;

/////////////////////////////////////////////////////////////////////////////
// CVidsendApp initialization

BOOL CVidsendApp::InitInstance() {
	char myBuf[128],*p,*p1;
	CSplashScreenEx *splashDlg=NULL;
	register i;
	WSADATA wsa;
	
	if(!AfxSocketInit(&wsa)) {
		AfxMessageBox(IDP_SOCKETS_INIT_FAILED);
		return FALSE;
		}

//	wsprintf(myBuf,"udp size=%u",wsa.iMaxUdpDg );
//	AfxMessageBox(myBuf);

	AfxEnableControlContainer();		// per HtmlView! no...
	
	// Standard initialization
	// If you are not using these features and wish to reduce the size
	//  of your final executable, you should remove from the following
	//  the specific initialization routines you do not need.

#ifdef _AFXDLL
	Enable3dControls();			// Call this when using MFC in a shared DLL
#else
	Enable3dControlsStatic();	// Call this when linking to MFC statically
#endif

	SetRegistryKey(_T("ADPM Synthesis"));

#ifdef _CAMPARTY_MODE
	m_pszProfileName=_tcsdup(_T("camparty.ini"));
#else
	m_pszProfileName=_tcsdup(_T("videosender.ini"));
#endif

#ifdef _CPRIVATEPROFILE_USEINI
	prStore=new CProfileStore(m_hInstance,m_pszProfileName,NULL /* uso INI!!*/);
#else
	prStore=new CProfileStore(m_hInstance,m_pszRegistryKey,m_pszAppName);
#endif
	
	LoadStdProfileSettings();  // Load standard INI file options (including MRU)

	// Register the application's document templates.  Document templates
	//  serve as the connection between documents, frame windows and views.

	pDocTemplate = new CMultiDocTemplate(
		IDR_VIDSENTYPE,
		RUNTIME_CLASS(CVidsendDoc),
		RUNTIME_CLASS(CChildFrame), // custom MDI child frame
		RUNTIME_CLASS(CVidsendView));
	AddDocTemplate(pDocTemplate);
	pDocTemplate2 = new CMultiDocTemplate(
#ifdef _NEWMEET_MODE
#ifdef _LINGUA_INGLESE
		IDR_VIDSENTYPE2_NM_ENG,
#else
#ifdef _CAMPARTY_MODE
		IDR_VIDSENTYPE2_CM,
#else
		IDR_VIDSENTYPE2_NM,
#endif
#endif
#else
		IDR_VIDSENTYPE2,
#endif
		RUNTIME_CLASS(CVidsendDoc2),
		RUNTIME_CLASS(CChildFrame2), // custom MDI child frame
		RUNTIME_CLASS(CVidsendView2));
	AddDocTemplate(pDocTemplate2);
	pDocTemplate3 = new CMultiDocTemplate(
		IDR_VIDSENTYPE3,
		RUNTIME_CLASS(CVidsendDoc3),
		RUNTIME_CLASS(CChildFrame3), // standard MDI child frame
		RUNTIME_CLASS(CVidsendView3));
	AddDocTemplate(pDocTemplate3);
	pDocTemplate4 = new CMultiDocTemplate(
#ifdef _NEWMEET_MODE
#ifdef _LINGUA_INGLESE
		IDR_VIDSENTYPE4_NM_ENG,
#else
#ifdef _CAMPARTY_MODE
		IDR_VIDSENTYPE4_CM,
#else
		IDR_VIDSENTYPE4_NM,
#endif
#endif
#else
		IDR_VIDSENTYPE4,
#endif
		RUNTIME_CLASS(CVidsendDoc4),
		RUNTIME_CLASS(CChildFrame4), // standard MDI child frame
		RUNTIME_CLASS(CVidsendView4));
	AddDocTemplate(pDocTemplate4);
	pDocTemplate5 = new CMultiDocTemplate(
		IDR_VIDSENTYPE5,
		RUNTIME_CLASS(CVidsendDoc5),
		RUNTIME_CLASS(CChildFrame5), // custom MDI child frame
		RUNTIME_CLASS(CVidsendView5));
	AddDocTemplate(pDocTemplate5);
	pDocTemplate6 = new CMultiDocTemplate(
#ifdef _NEWMEET_MODE
#ifdef _LINGUA_INGLESE
		IDR_VIDSENTYPE6_NM_ENG,
#else
		IDR_VIDSENTYPE6_NM,
#endif
#else
		IDR_VIDSENTYPE6,
#endif
		RUNTIME_CLASS(CVidsendDoc6),
		RUNTIME_CLASS(CChildFrame3), // custom MDI child frame
		RUNTIME_CLASS(CVidsendView6));
	AddDocTemplate(pDocTemplate6);
	pDocTemplate7 = new CMultiDocTemplate(
		IDR_VIDSENTYPE7,
		RUNTIME_CLASS(CVidsendDoc7),
		RUNTIME_CLASS(CChildFrame3), // custom MDI child frame
		RUNTIME_CLASS(CVidsendView7));
	AddDocTemplate(pDocTemplate7);

	// create main MDI Frame window
	CMainFrame* pMainFrame = new CMainFrame;
	if(!pMainFrame->LoadFrame(IDR_MAINFRAME))
		return FALSE;
	m_pMainWnd = pMainFrame;

	debugMode=0;

	// Parse command line for standard shell commands, DDE, file open
	CCommandLineInfoEx cmdInfo;
	ParseCommandLine(cmdInfo);
	if(cmdInfo.m_restoreOptions) {
		//FARE
		;
		}
	if(!cmdInfo.m_ODBCname.IsEmpty()) {
		_tcscpy(ODBCname,(LPCTSTR)cmdInfo.m_ODBCname);
		}
	else {
		prStore->GetProfileVariabileString(IDS_ODBCNAME,ODBCname,63,"vidsendMDB");
		}
#ifndef _DEBUG
	debugMode=cmdInfo.m_debugMode;
#else
	debugMode=2;
#endif

	timeBeginPeriod(1);			// per timer alta precisione!

#ifndef _DEBUG
	if(cmdInfo.m_bShowSplash) {
		splashDlg=new CSplashScreenEx;
		if(splashDlg) {
			splashDlg->Create(NULL,NULL,0,CSplashScreenEx::CSS_FADE | CSplashScreenEx::CSS_CENTERSCREEN | CSplashScreenEx::CSS_SHADOW);
			splashDlg->SetBitmap(IDB_BITMAP1,RGB(255,255,255));
			splashDlg->SetTextFont("Tahoma",100,CSplashScreenEx::CSS_TEXT_BOLD);
			splashDlg->SetTextRect(CRect(125,320,291,104));
			splashDlg->SetTextColor(RGB(255,255,255));
			splashDlg->SetTextFormat(DT_SINGLELINE | DT_CENTER | DT_VCENTER);
			splashDlg->Show();
	//		splashDlg->SetWindowPos(&CWnd::wndTopMost,0,0,0,0,SWP_NOREDRAW | SWP_NOMOVE | SWP_NOSIZE);


#ifdef _NEWMEET_MODE
			Sleep(2000);
			splashDlg->Hide();
			splashDlg=NULL;
#endif

			}
		}
#endif

//AfxMessageBox("eccomi");


#ifdef _DEBUG
	FileSpool=new CLogFile("c:\\vidsendSpool.txt",m_pMainWnd,CLogFile::dateTimeMillisec | CLogFile::flushImmediate);
#else
	FileSpool=new CLogFile("c:\\vidsendSpool.txt",m_pMainWnd);
#endif
	Opzioni=prStore->GetProfileVariabileInt(IDS_OPZIONI);
	if(!Opzioni)
		Opzioni = canSendVideo | canSendAudio | canRecvVideo | canRecvAudio | canRecvText | canSendText | saveLayout;
	OpzioniLog=prStore->GetProfileVariabileInt(IDS_OPZIONILOG);
	OpzioniDirSrv=prStore->GetProfileVariabileInt(IDS_OPZIONIDIRSRV);
	OpzioniOpenWin=prStore->GetProfileVariabileInt(IDS_OPZIONI_OPENWIN);
	maxHTMLconn=prStore->GetProfileVariabileInt(IDS_MAXHTMLCONN);
	serverWWWPort=prStore->GetProfileVariabileInt(IDS_SERVERWWW_PORT);
	maxDirSrvConn=maxAuthConn=100; /*GetProfileVariabileInt(IDS_MAXDIRSRVCONN); */
	prStore->GetProfileVariabileString(IDS_DIALUPNOME,myBuf,127);
	DialUpNome=myBuf;
	prStore->GetProfileVariabileString(IDS_IP_SCELTO,myBuf,127);
	if(!*myBuf)
		CSocketEx::getMyIPAddress(myBuf);
	IPScelto=myBuf;

#ifdef _NEWMEET_MODE
	Opzioni = canSendVideo | canSendAudio | canRecvVideo | canRecvAudio | canRecvText | canSendText | saveLayout | openServerVideo | openServerChat | openClientChat;
	OpzioniLog=0;
	OpzioniDirSrv=0;
#ifndef _CAMPARTY_MODE
	OpzioniOpenWin=32;
#else
	OpzioniOpenWin=0;
#endif
#endif


	prStore->GetProfileVariabileString(IDS_SUONO1,myBuf,127);
	suonoInizio=myBuf;
	prStore->GetProfileVariabileString(IDS_SUONO2,myBuf,127);
	suonoFine=myBuf;
	if(!suonoInizio.IsEmpty())
		sndPlaySound(suonoInizio,SND_ASYNC);
	ultimaOra=prStore->GetPrivateProfileTime(prStore->getVariabiliKey(myBuf),IDS_ULTIMA_ORA);

#ifndef _NEWMEET_MODE
	theDB=new CDatabase;
	if(theDB) {
		CString S;
		S.Format("DSN=%s",ODBCname);
		theApp.theODBCConnectString="ODBC;"+S;
	TRY {
		theDB->OpenEx(S,CDatabase::useCursorLib | CDatabase::noOdbcDialog);
		}
	CATCH(CDBException,e) {
		AfxMessageBox("Impossibile aprire il database!");
		goto no_db;
		}
	END_CATCH
		}

	mySetUtenti=new CVidsendSet2_(theDB);
	mySetSernum=new CVidsendSet8(theDB);
	mySetUtentiOnline=new CVidsendSet3(theDB);
	mySetLogConnessioni=new CVidsendSet5(theDB);
	mySetLogConnessioniEx=new CVidsendSet5Ex(theDB);
	mySetLogChiamate=new CVidsendSet4(theDB);
	mySetUt=new CVidsendSet2(theDB);
	if(mySetLogConnessioni) {
		if(!mySetLogConnessioni->Open(CRecordset::dynaset /*AFX_DB_USE_DEFAULT_TYPE*/,NULL,CRecordset::none)) {
#ifdef _DEBUG
			AfxMessageBox("impossibile aprire LogConnessioni");
#else
			if(FileSpool)
				*FileSpool << "impossibile aprire LogConnessioni";
#endif
			}
		}
	if(mySetLogConnessioniEx) {
		if(!mySetLogConnessioniEx->Open(CRecordset::dynaset /*AFX_DB_USE_DEFAULT_TYPE*/,NULL,CRecordset::none)) {
#ifdef _DEBUG
			AfxMessageBox("impossibile aprire LogConnessioniEx");
#else
			if(FileSpool)
				*FileSpool << "impossibile aprire LogConnessioniEx";
#endif
			}
		}
	if(mySetLogChiamate) {
		if(!mySetLogChiamate->Open(CRecordset::dynaset /*AFX_DB_USE_DEFAULT_TYPE*/,NULL,CRecordset::none)) {
#ifdef _DEBUG
			AfxMessageBox("impossibile aprire LogChiamate");
#else
			if(FileSpool)
				*FileSpool << "impossibile aprire LogChiamate";
#endif
			}
		}
	if(mySetUt) {
		if(!mySetUt->Open(CRecordset::forwardOnly,NULL,CRecordset::readOnly)) {
#ifdef _DEBUG
			AfxMessageBox("impossibile aprire Ut");
#else
			if(FileSpool)
				*FileSpool << "impossibile aprire Ut";
#endif
			}
		}
	if(mySetUtenti) {
		if(!mySetUtenti->Open(AFX_DB_USE_DEFAULT_TYPE,NULL,CRecordset::none /*readOnly*/)) {
#ifdef _DEBUG
			AfxMessageBox("impossibile aprire Utenti");
#else
			if(FileSpool)
				*FileSpool << "impossibile aprire Utenti";
#endif
			}
		}
	if(mySetUtentiOnline) {
		if(!mySetUtentiOnline->Open(AFX_DB_USE_DEFAULT_TYPE,NULL,CRecordset::none)) {
#ifdef _DEBUG
			AfxMessageBox("impossibile aprire UtentiOnline");
#else
			if(FileSpool)
				*FileSpool << "impossibile aprire UtentiOnline";
#endif
			}
		}
	if(mySetSernum) {
		if(!mySetSernum->Open(AFX_DB_USE_DEFAULT_TYPE,NULL,CRecordset::none)) {
#ifdef _DEBUG
			AfxMessageBox("impossibile aprire Sernum");
#else
			if(FileSpool)
				*FileSpool << "impossibile aprire Sernum";
#endif
			}
		}

#endif

no_db:


	myCRC=new CChecksum(8,0x52);		// a caso!

	p1=infoUtenteKey;
	prStore->GetPrivateProfileString(p1,"nome",infoUtente.nome,32);
	prStore->GetPrivateProfileString(p1,"cognome",infoUtente.cognome,32);
	prStore->GetPrivateProfileString(p1,"indirizzo",infoUtente.indirizzo,80);
	prStore->GetPrivateProfileString(p1,"citta",infoUtente.citta,32);
	prStore->GetPrivateProfileString(p1,"email",infoUtente.email,32);
	prStore->GetPrivateProfileString(p1,"note",infoUtente.note,256);
#ifndef _CAMPARTY_MODE
	prStore->GetPrivateProfileString(p1,"login",infoUtente.login,20);
#else
	prStore->GetPrivateProfileString(p1,"login",infoUtente.login,20,"camparty");
	//leggere la chiave messa in Utente/Installshield!
#endif
	prStore->GetPrivateProfileString(p1,"pasw",infoUtente.pasw,20);


	// Dispatch commands specified on the command line
	if(!ProcessShellCommand(cmdInfo))
		return FALSE;

	if(theApp.Opzioni & CVidsendApp::saveLayout) {
		RECT rc;

		prStore->GetProfileVariabileString(IDS_COORDINATE,myBuf,32);
#ifdef _NEWMEET_MODE
#ifndef _CAMPARTY_MODE
		_tcscpy(myBuf,"16,12,714,503");
#else
		_tcscpy(myBuf,"16,12,354,553");
#endif
#endif
		sscanf(myBuf,"%d,%d,%d,%d",&rc.left,&rc.top,&rc.right,&rc.bottom);
		if(!IsRectEmpty(&rc))
			m_pMainWnd->SetWindowPos(NULL,rc.left,rc.top,rc.right-rc.left,rc.bottom-rc.top,SWP_NOZORDER);
		}
#ifndef _NEWMEET_MODE
	m_pMainWnd->DragAcceptFiles(TRUE);
#endif
//	IDropTarget meDropTarget;		// per abilitare il drag/drop da Explorer/Netscape
//	RegisterDragDrop(m_pMainWnd->m_hWnd,&meDropTarget);

	if(splashDlg)
	  splashDlg->SetText("Inizializzazione video...");

// se i documenti (v. File->New) venissero DOPO i socket, allora funzionerebbe l'autenticazione anche sul medesimo server video...
	SOCKADDR_IN sock_in;
	if(CSocketEx::getMyIPAddress(&sock_in)) {
		sock_in.sin_family=AF_INET;
		sock_in.sin_port=0;
		if(Opzioni & webServer) {
			if(wwwSocket=new CWebSrvSocket(serverWWWPort,IPScelto,0,theDB)) {		// FINIRE IPScelto... e' una prova!!
				if(wwwSocket->Create()) {
	//						i=WSAGetLastError();
					if(wwwSocket->Listen()) {
						goto www_ok;
						}
					}
				}
			p="Impossibile installare server WWW";
			AfxMessageBox(p,MB_ICONHAND);
			if(FileSpool)
				*FileSpool << p;
			}
/*				CPing myPing;
			CPingReply pr;
			i=myPing.Ping("192.168.0.4",pr);*/
www_ok:
		if(Opzioni & timeServer) {
			if(timeSocket = new CTimeSocket) {
				timeSocket->Create();
				if(!(i=timeSocket->Listen())) {
					p="Impossibile installare server Time";
#ifdef _DEBUG
					AfxMessageBox(p,MB_ICONHAND);
#endif
					if(FileSpool)
						*FileSpool << p;
					}
				}
			}
#ifndef _CAMPARTY_MODE
		if(Opzioni & authServer) {
			if(authSocket = new CAuthSrvSocket) {
				authSocket->Create(AUTHENTICATION_SOCKET /*,SOCK_STREAM,(char *)&sock_in*/);
				if(!(i=authSocket->Listen())) {
					p="Impossibile installare server autenticazione";
#ifdef _DEBUG
					AfxMessageBox(p,MB_ICONHAND);
#endif
					if(FileSpool)
						*FileSpool << p;
					}
				}
			}
#endif
		if(Opzioni & dirServer) {
			if(dirSocket = new CDirectorySrvSocket) {
				dirSocket->Create(DIRECTORY_SOCKET /*,SOCK_STREAM,(char *)&sock_in*/);
				if(!(i=dirSocket->Listen())) {
					p="Impossibile installare Directory server";
#ifdef _DEBUG
					AfxMessageBox(p,MB_ICONHAND);
#endif
					if(FileSpool)
						*FileSpool << p;
					}
				}
			}
		}

	// The main window has been initialized, so show and update it.
	pMainFrame->ShowWindow(m_nCmdShow);
	pMainFrame->UpdateWindow();
	pMainFrame->showToolbar(Opzioni & hasToolbar);
	pMainFrame->showStatusbar(Opzioni & hasStatusbar);

	if(splashDlg)
		splashDlg->Hide();

	if(FileSpool)
		*FileSpool << "VideoSender attivo";


	return TRUE;
	}

int CVidsendApp::ExitInstance() {
	char *p1;
	char myBuf[128];

	timeEndPeriod(1);

	if(!suonoFine.IsEmpty())
		sndPlaySound(suonoFine,SND_SYNC);
	if(hRasConn) {
		RasHangUp(hRasConn);
		hRasConn=FALSE;
		}
	if(authSocket) {
		authSocket->Close();
		delete authSocket;
		}
	if(dirSocket) {
		dirSocket->Close();
		delete dirSocket;
		}
	if(timeSocket) {
		timeSocket->Close();
		delete timeSocket;
		}
	if(wwwSocket) {
		wwwSocket->Close();
		delete wwwSocket;
		}

#ifndef _NEWMEET_MODE
	if(mySetUtenti)
		mySetUtenti->Close();
	delete mySetUtenti;
	if(mySetSernum)
		mySetSernum->Close();
	delete mySetSernum;
	if(mySetUtentiOnline)
		mySetUtentiOnline->Close();
	delete mySetUtentiOnline;
	if(mySetLogConnessioni)
		mySetLogConnessioni->Close();
	delete mySetLogConnessioni;
	if(mySetLogConnessioniEx)
		mySetLogConnessioniEx->Close();
	delete mySetLogConnessioniEx;
	if(mySetLogChiamate)
		mySetLogChiamate->Close();
	delete mySetLogChiamate;
	if(mySetUt)
		mySetUt->Close();
	delete mySetUt;
	if(theDB)
		theDB->Close();
	delete theDB;
#endif

	delete myCRC;

	AfxSocketTerm();

	if(FileSpool)
		*FileSpool << "VideoSender terminato";

	prStore->WritePrivateProfileTime(prStore->getVariabiliKey(myBuf),IDS_ULTIMA_ORA,ultimaOra);
  prStore->WriteProfileVariabileInt(IDS_OPZIONI,Opzioni);
  prStore->WriteProfileVariabileInt(IDS_OPZIONI_OPENWIN,OpzioniOpenWin);
  prStore->WriteProfileVariabileInt(IDS_OPZIONILOG,OpzioniLog);
  prStore->WriteProfileVariabileInt(IDS_OPZIONIDIRSRV,OpzioniDirSrv);
  prStore->WriteProfileVariabileInt(IDS_MAXHTMLCONN,maxHTMLconn);
  prStore->WriteProfileVariabileInt(IDS_SERVERWWW_PORT,serverWWWPort);
  prStore->WriteProfileVariabileString(IDS_SUONO1,(LPCTSTR)suonoInizio);
  prStore->WriteProfileVariabileString(IDS_SUONO2,(LPCTSTR)suonoFine);
  prStore->WriteProfileVariabileString(IDS_DIALUPNOME,(LPCTSTR)DialUpNome);
  prStore->WriteProfileVariabileString(IDS_ODBCNAME,ODBCname);
  prStore->WriteProfileVariabileString(IDS_IP_SCELTO,(LPCTSTR)IPScelto);
	p1=infoUtenteKey;
	prStore->WritePrivateProfileString(p1,"nome",infoUtente.nome);
	prStore->WritePrivateProfileString(p1,"cognome",infoUtente.cognome);
	prStore->WritePrivateProfileString(p1,"indirizzo",infoUtente.indirizzo);
	prStore->WritePrivateProfileString(p1,"citta",infoUtente.citta);
	prStore->WritePrivateProfileString(p1,"email",infoUtente.email);
	prStore->WritePrivateProfileString(p1,"note",infoUtente.note);
	prStore->WritePrivateProfileString(p1,"login",infoUtente.login);
#ifdef _NEWMEET_MODE
	prStore->WritePrivateProfileString(p1,"pasw",infoUtente.pasw);
#endif

	delete FileSpool;

	delete prStore;

	return CWinApp::ExitInstance();
  }


/////////////////////////////////////////////////////////////////////////////
// CVidsendApp message handlers


void CVidsendApp::OnFileNew() {
	
	if(OpzioniOpenWin & 64)
		OnFileNuovoServerdisponibili();
	if(OpzioniOpenWin & 32)
		OnFileNuovoConnessioni();
	if((OpzioniOpenWin & 2 || Opzioni & openServerVideo) && Opzioni & canSendVideo)
		OnFileNuovoVideoserver();
#ifdef _NEWMEET_MODE
	if(!theServer) {			// se l'autenticazione dell'exhibitor è fallita, esco dal programma!
		HideApplication();
		CloseAllDocuments(FALSE);
		m_pMainWnd->PostMessage(WM_CLOSE);		
		}
#endif
#ifndef _NEWMEET_MODE
	if((OpzioniOpenWin & 1 || Opzioni & openClientVideo) && Opzioni & canRecvVideo) {
		OnFileNuovoVideoclient();
		}
#endif
	if((OpzioniOpenWin & 8 || Opzioni & (openServerChat | openClientChat)) && Opzioni & canSendText) {
		OnFileNuovoChat();
		}
#ifndef _NEWMEET_MODE
	if(OpzioniOpenWin & 16)
		OnFileNuovoBrowser();
	if(OpzioniOpenWin & 4)
		OnFileNuovoLog();
#endif
	}

void CVidsendApp::OnFileNuovoVideoclient() {
	int i;

	for(i=0; i<16; i++) {
		if(!aClient[i]) {
			aClient[i]=(CVidsendDoc *)pDocTemplate->OpenDocumentFile(NULL);
			return;
			}
		}
	return;
	}

void CVidsendApp::OnUpdateFileNuovoVideoclient(CCmdUI* pCmdUI) {
	
	pCmdUI->Enable(1 /*aClient[0] == NULL*/);		// per ora sempre si'!
	}

void CVidsendApp::OnFileNuovoVideoserver() {
	
	theServer=(CVidsendDoc2 *)pDocTemplate2->OpenDocumentFile(NULL);
	}

void CVidsendApp::OnUpdateFileNuovoVideoserver(CCmdUI* pCmdUI) {

	pCmdUI->Enable(theServer == NULL);	
	}

void CVidsendApp::OnFileNuovoLog() {
	
	theLog=(CVidsendDoc3 *)pDocTemplate3->OpenDocumentFile(NULL);
	}

void CVidsendApp::OnUpdateFileNuovoLog(CCmdUI* pCmdUI) {
	
	pCmdUI->Enable(theLog == NULL);
	}

void CVidsendApp::OnFileConfig() {
	int i;
	CVidsendPropPage mySheet("Configurazione",m_pMainWnd);
	CVidsendPropPage0 myPage0;
	CVidsendPropPage1 myPage1;
	CVidsendPropPage2 myPage2;
	
	if(Opzioni & passwordProtect) {
		if(confPassword.IsEmpty()) {
			}
		else {
			if(0)
				goto fine;
			}
		}
	mySheet.AddPage(&myPage0);
	mySheet.AddPage(&myPage1);
	mySheet.AddPage(&myPage2);
	mySheet.m_psh.dwFlags |= PSH_NOAPPLYNOW;
	if(mySheet.DoModal() == IDOK) {
#ifndef _NEWMEET_MODE
		if(myPage0.isInitialized) {
			Opzioni &= 0xffff0000;
			Opzioni |= myPage0.m_ServerWWW ? webServer : 0;
			Opzioni |= myPage0.m_ServerOra ? timeServer : 0;
			Opzioni |= myPage0.m_ServerAuth ? authServer : 0;
			Opzioni |= myPage0.m_ServerDir ? dirServer : 0;
			Opzioni |= myPage0.m_DDEenable ? DDEenabled : 0;
			Opzioni |= myPage0.m_RiconnettiV ? openClientVideo : 0;
			Opzioni |= myPage0.m_RiconnettiC ? openClientChat : 0;
			Opzioni |= myPage0.m_RiapriV ? openServerVideo : 0;
			Opzioni |= myPage0.m_RiapriC ? openServerChat : 0;
			Opzioni |= myPage0.m_TCP_UDP ? TCP_UDP : 0;
			Opzioni |= myPage0.m_NamedPipes ? namedPipes : 0;
			serverWWWPort=myPage0.m_ServerWWWPort;
			IPScelto=myPage0.m_IPAddressText;
			if(myPage0.m_Spool) {
				if(!FileSpool)
#ifdef _DEBUG
					FileSpool=new CLogFile("c:\\vidsendSpool.txt",m_pMainWnd,CLogFile::dateTimeMillisec | CLogFile::flushImmediate);
#else
					FileSpool=new CLogFile("c:\\vidsendSpool.txt",m_pMainWnd);
#endif
				}
			else {
				delete FileSpool;
				FileSpool=NULL;
				}
			}
		
		if(myPage1.isInitialized) {
			_tcscpy(infoUtente.nome,(LPCTSTR)myPage1.m_Nome);
			_tcscpy(infoUtente.cognome,(LPCTSTR)myPage1.m_Cognome);
			_tcscpy(infoUtente.indirizzo,(LPCTSTR)myPage1.m_Indirizzo);
			_tcscpy(infoUtente.citta,(LPCTSTR)myPage1.m_Citta);
			_tcscpy(infoUtente.email,(LPCTSTR)myPage1.m_Email);
			_tcscpy(infoUtente.note,(LPCTSTR)myPage1.m_Note);
			Opzioni &= (0xff00ffff | (hasToolbar | hasStatusbar) | advancedConf /* togliere, poi, e usare solo Commmandline*/);
			Opzioni |= myPage1.m_PasswordProtect ? passwordProtect : 0;
			Opzioni |= myPage1.m_SaveLayout ? saveLayout : 0;
			suonoInizio=myPage1.m_SuonoInizio;
			suonoFine=myPage1.m_SuonoFine;
			}
		if(myPage2.isInitialized) {
			}
#endif
		}
fine:
		;
	}

void CVidsendApp::OnFileNuovoBrowser() {

	nuovoBrowser();
	}

int CVidsendApp::nuovoBrowser(char *s) {
	int i;

	for(i=0; i<16; i++) {
		if(!aBrowser[i]) {
			aBrowser[i]=(CVidsendDoc5 *)pDocTemplate5->OpenDocumentFile(NULL);
			if(s)
				aBrowser[i]->setURL(s);		// dovrebbe finire da qualche parte gia' con OpenDocumentFile... ma non lo fa!
			return TRUE;
			}
		}
	return FALSE;
	}

void CVidsendApp::OnUpdateFileNuovoBrowser(CCmdUI* pCmdUI) {
	
	pCmdUI->Enable(1 /*aBrowser[0] == NULL*/);
	}

void CVidsendApp::OnFileNuovoConnessioni() {

	theConnections=(CVidsendDoc6 *)pDocTemplate6->OpenDocumentFile(NULL);
	}

void CVidsendApp::OnUpdateFileNuovoConnessioni(CCmdUI* pCmdUI) {
	
	pCmdUI->Enable(theConnections == NULL);
	}


void CVidsendApp::OnFileNuovoChat() {
	
	theChat=(CVidsendDoc4 *)pDocTemplate4->OpenDocumentFile(NULL);
	}

void CVidsendApp::OnUpdateFileNuovoChat(CCmdUI* pCmdUI) {
	
	pCmdUI->Enable(theChat == NULL);
	}

void CVidsendApp::OnFileNuovoServerdisponibili() {
	
	theDirectoryServer=(CVidsendDoc7 *)pDocTemplate7->OpenDocumentFile(NULL);
	}

void CVidsendApp::OnUpdateFileNuovoServerdisponibili(CCmdUI* pCmdUI) {
	
	pCmdUI->Enable(theDirectoryServer == NULL);
	}


//----------------------------------------------------------------------------------
char *CProfileStore::emptyString="";

CProfileStore::CProfileStore(HINSTANCE h,const char *s,const char *s1) {

	m_hInstance=h;
	variabiliKey="variabili";
	regRoot=s;
#ifndef _CPRIVATEPROFILE_USEINI
	regRoot+="\\";
	regRoot+=s1;
	regRoot+="\\";
	regRoot+="Settings";
#endif
	}

char *CProfileStore::getProfileKey(char *d,const char *s) {

	_tcscpy(d,(LPCTSTR)regRoot);
	if(s) {
		_tcscat(d,"\\");
		_tcscat(d,s);
		}
	return d;
	}

char *CProfileStore::getProfileRoot(char *s) {

	_tcscpy(s,(LPCTSTR)regRoot);
	return s;
	}

char *CProfileStore::getVariabiliKey(char *s) {

#ifdef _CPRIVATEPROFILE_USEINI
	_tcscpy(s,variabiliKey);
#else
	_tcscpy(s,(LPCTSTR)regRoot);
	_tcscat(s,variabiliKey);
#endif
	return s;
	}

int CProfileStore::WritePrivateProfileString(const char *s,const char *v,const char *n) {

#ifdef _CPRIVATEPROFILE_USEINI

	return ::WritePrivateProfileString(s,v,n,regRoot);

#else

	char myBuf[256];
	HKEY pk,pksub,pksub2;
	int i,retVal=-1;

	getProfileKey(myBuf,s);
	if(!RegCreateKeyEx(HKEY_CURRENT_USER,"Software",0L,"",REG_OPTION_NON_VOLATILE,
		KEY_WRITE,NULL,&pk,NULL)) {
		if(!RegCreateKeyEx(pk,myBuf,0L,"",REG_OPTION_NON_VOLATILE,
			KEY_WRITE,NULL,&pksub,NULL)) {
			retVal=RegSetValueEx(pksub,v,0,REG_SZ,(const BYTE *)n,strlen(n));
			RegCloseKey(pksub);
			}
		RegCloseKey(pk);
		}

	return retVal;
#endif

  }

int CProfileStore::WritePrivateProfileInt(const char *s,const char *v,int n) {
	char myBuf[256];

#ifdef _CPRIVATEPROFILE_USEINI

	itoa(n,myBuf,10);
	return ::WritePrivateProfileString(s,v,myBuf,regRoot);

#else

	HKEY pk,pksub,pksub2;
	int i,retVal=-1;

	getProfileKey(myBuf,s);
	if(!RegCreateKeyEx(HKEY_CURRENT_USER,"Software",0L,"",REG_OPTION_NON_VOLATILE,
		KEY_WRITE,NULL,&pk,NULL)) {
		if(!RegCreateKeyEx(pk,myBuf,0L,"",REG_OPTION_NON_VOLATILE,
			KEY_WRITE,NULL,&pksub,NULL)) {
			retVal=RegSetValueEx(pksub,v,0,REG_DWORD,(const BYTE *)&n,4);
			RegCloseKey(pksub);
			}
		RegCloseKey(pk);
		}

	return retVal;

#endif

  }

int CProfileStore::GetPrivateProfileString(const char *s, const char *k, char *v, int len, const char *def) {

#ifdef _CPRIVATEPROFILE_USEINI

	return ::GetPrivateProfileString(s,k,def,v,len,regRoot);

#else

	char myBuf[256];
	HKEY pk,pksub;
	int i,retVal=-1;
	DWORD vType,vLen=len;

	getProfileKey(myBuf,s);
	if(def)
		_tcscpy(v,def);
	else
		*v=0;
	if(!RegOpenKeyEx(HKEY_CURRENT_USER,"Software",0L,KEY_READ,&pk)) {
		if(!RegOpenKeyEx(pk,myBuf,0L,KEY_READ,&pksub)) {
			retVal=RegQueryValueEx(pksub,k,0,NULL /*&vType*/,(BYTE *)v,&vLen);
			RegCloseKey(pksub);
			}
		RegCloseKey(pk);
		}
	return retVal;

#endif

	}

int CProfileStore::GetPrivateProfileInt(const char *s, const char *k, int def) {

#ifdef _CPRIVATEPROFILE_USEINI

	return ::GetPrivateProfileInt(s,k,def,regRoot);

#else

	char myBuf[256];
	HKEY pk,pksub;
	int i,v,retVal=def;
	DWORD vType,vLen=4;

	getProfileKey(myBuf,s);
	v=def;
	if(!RegOpenKeyEx(HKEY_CURRENT_USER,"Software",0L,KEY_READ,&pk)) {
		if(!RegOpenKeyEx(pk,myBuf,0L,KEY_READ,&pksub)) {
			i=RegQueryValueEx(pksub,k,0,NULL /*&vType*/,(BYTE *)&v,&vLen);
			if(!i) {
				retVal=v;
				}
			RegCloseKey(pksub);
			}
		RegCloseKey(pk);
		}
	return retVal;

#endif

	}

CTime CProfileStore::GetPrivateProfileTime(const char *s,const char *v) {
	char myBuf[64];
	int i,h,d,m,y;

	GetPrivateProfileString(s,v,myBuf,18 /*"01/01/1997 00:00"*/);
	d=*myBuf ? atoi(myBuf) : 1;
	m=*myBuf ? atoi(myBuf+3) : 1;
	y=*myBuf ? atoi(myBuf+6) : 1997;
	h=*myBuf ? atoi(myBuf+11) : 0;
	i=*myBuf ? atoi(myBuf+14) : 0;
	{
		CTime t(y,m,d,h,i,0);
		return t;
		}
	}

CTimeSpan CProfileStore::GetPrivateProfileTimeSpan(const char *s,char *v) {
	char myBuf[64];
	int i,h,m;

	GetPrivateProfileString(s,v,myBuf,8 /*"00:00"*/);
	h=*myBuf ? atoi(myBuf) : 1;
	m=*myBuf ? atoi(myBuf+3) : 1;
	{
		CTimeSpan ts(0,h,m,0);
		return ts;
		}
	}

int CProfileStore::WritePrivateProfileTime(const char *s,const char *v,CTime t) {
	CString c;

	c=t > 0 ? t.Format("%d/%m/%Y %H:%M") : "";
	return WritePrivateProfileString(s,v,(LPCTSTR)c);
	}

int CProfileStore::WritePrivateProfileTime(const char *s,const char *v,CTimeSpan t) {
	CString c;

	c=t.Format("%H:%M");
	return WritePrivateProfileString(s,v,(LPCTSTR)c);
	}


#ifdef _CPRIVATEPROFILE_USEINI
int CProfileStore::WritePrivateProfileInt(char *s,char *v,int n,const char *nf) {
	char myBuf[64];

	itoa(n,myBuf,10);
	return ::WritePrivateProfileString(s,v,myBuf,nf);
  }
#endif


int CProfileStore::FlushProfile() {

#ifdef _CPRIVATEPROFILE_USEINI
	return ::WritePrivateProfileString(NULL,NULL,NULL,regRoot);
#else
	HKEY pk,pksub;
	char myBuf[128];
	int i;

	_tcscpy(myBuf,regRoot);
	if(!RegOpenKeyEx(HKEY_CURRENT_USER,"Software",0L,KEY_WRITE,&pk)) {
		if(!RegOpenKeyEx(pk,myBuf,0L,KEY_READ,&pksub)) {
			i=RegFlushKey(pksub);
			if(theApp.debugMode>2) {
				if(theApp.FileSpool)
					theApp.mySkynet.FileSpool->print(CLogFile::flagInfo,"Flushkey: %x lstErr=%x",i,i);
				}
			RegCloseKey(pksub);
			}
		RegCloseKey(pk);
		}
	return i;

#endif
	}


//------------------------------------------------------------------



char *CVidsendApp::getNow(char *s) {
	int i;

	_strdate(s);
	i=s[0];
	s[0]=s[3];
	s[3]=i;
	i=s[1];
	s[1]=s[4];
	s[4]=i;
	_strtime(s+9);
	s[8]=' ';

	return s;
	}

DWORD CVidsendApp::getVersione(char *n,char *n1) {
	char szFullPath[256],szGetName[128];
	DWORD dwVerInfoSize,dwVerHnd;
	int wRootLen,bRetCode,i,j,k;
	LPSTR lpVersion,lpV2;
	DWORD	dwVersion=-1;
	DWORD	dwResult;
	UINT uVersionLen,uV2;

	GetModuleFileName(m_hInstance, szFullPath, sizeof(szFullPath));

	dwVerInfoSize = GetFileVersionInfoSize(szFullPath, &dwVerHnd);
	if(dwVerInfoSize) {
		LPSTR   lpstrVffInfo;

		lpstrVffInfo = (char *)GlobalAlloc(GPTR, dwVerInfoSize);
		GetFileVersionInfo(szFullPath, dwVerHnd, dwVerInfoSize, lpstrVffInfo);
		_tcscpy(szGetName, "\\StringFileInfo\\041004b0\\");	 
		wRootLen = strlen(szGetName);
			
		_tcscat(szGetName, "ProductName");
		uVersionLen=uV2=0;
		lpVersion=lpV2=NULL;
		bRetCode = VerQueryValue((LPVOID)lpstrVffInfo,(LPSTR)szGetName,
						(LPVOID *)&lpVersion,(UINT *)&uVersionLen);
		szGetName[wRootLen] = 0;
		_tcscat(szGetName, "FileVersion");
		bRetCode = VerQueryValue((LPVOID)lpstrVffInfo,(LPSTR)szGetName,
						(LPVOID *)&lpV2,(UINT *)&uV2);

		if(bRetCode && uVersionLen && lpVersion && uV2 && lpV2) {
			if(n) {
				_tcscpy(n, lpVersion);
				_tcscat(n, lpV2);
				}
			sscanf(lpV2,"%u,%u,%u",&i,&j,&k);
			dwVersion=MAKELONG(j*10+k,i);
		  }
		else {
			dwResult = GetLastError();
			wsprintf(n, "Error %lu", dwResult);
		  }

		if(n1) {
			szGetName[wRootLen] = (char)0;
			_tcscat (szGetName, "LegalCopyright");
			uVersionLen   = 0;
			lpVersion     = NULL;
			bRetCode      =  VerQueryValue((LPVOID)lpstrVffInfo,(LPSTR)szGetName,
							(LPVOID *)&lpVersion,(UINT *)&uVersionLen);

			if(bRetCode && uVersionLen && lpVersion) {
				_tcscpy(n1, lpVersion);
				}
			else {
				dwResult = GetLastError();
				wsprintf(n1, "Error %lu", dwResult);
				}
			}
		GlobalFree(lpstrVffInfo);
		}

	return dwVersion;
	}

char *CVidsendApp::getNowGMT(char *myBuf) {
	time_t aclock;
	struct tm *newtime;
	int i;

	time(&aclock);                 // Get time in seconds 
	newtime = localtime(&aclock);  // Convert time to struct tm form 
	_tcscpy(myBuf,asctime(newtime));
	i=-(_timezone/3600);
	if(!i)
		_tcscpy(myBuf+24," GMT\xd\xa");
	else
		wsprintf(myBuf+24," %c%02d00\xd\xa",i>=0 ? '+' : '-',abs(i));

	return myBuf;
	}


int CVidsendApp::mandaALog(const char *s1,const char *s2, int lev) {
	CVidsendSet *mySet;
	int i;

	if(OpzioniLog & CVidsendDoc3::logAttivo) {
		mySet=new CVidsendSet(theDB);
		mySet->Open(AFX_DB_USE_DEFAULT_TYPE,NULL,CVidsendSet::appendOnly);
		mySet->AddNew();
		mySet->m_DATA=CTime::GetCurrentTime();
		mySet->m_INDIRIZZO=s1;
		mySet->m_DESCRIZIONE=s2;
		mySet->m_LIVELLO=lev;
		i=mySet->Update();
		mySet->Close();
		delete mySet;
		return 1;
		}
	return 0;
	}

int CVidsendApp::renderBitmap(CDC *dc,int res,RECT *r) {
	CBitmap b;
	BITMAP bmp;
	BITMAPINFO bi;
	int i;

	i=b.LoadBitmap(res);
	i=b.GetBitmap(&bmp);
	i=bmp.bmWidth*bmp.bmHeight*bmp.bmBitsPixel/8;
	bmp.bmBits=GlobalAlloc(GPTR,i);
	b.GetBitmapBits(i,bmp.bmBits);
	ZeroMemory(&bi,sizeof(BITMAPINFOHEADER));
	bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
	bi.bmiHeader.biWidth=bmp.bmWidth;
	bi.bmiHeader.biHeight=bmp.bmHeight;
	bi.bmiHeader.biPlanes=bmp.bmPlanes;
	bi.bmiHeader.biBitCount=bmp.bmBitsPixel;
	bi.bmiHeader.biCompression=0;
	if(!r->bottom && !r->right) {		// se mancano entrambi, uso le dim. originali
		r->bottom=r->top+bmp.bmHeight;
		r->right=r->top+bmp.bmWidth;
		}
	else if(!r->right)		// se ne manca una, calcolo il ratio dall'altra
		r->right=(bmp.bmWidth*(r->bottom))/bmp.bmHeight;
	else if(!r->bottom)
		r->bottom=(bmp.bmHeight*(r->right))/bmp.bmWidth;
	i=StretchDIBits(dc->m_hDC,r->left,r->top+r->bottom,r->right,-r->bottom,0,0,
		bmp.bmWidth,bmp.bmHeight,bmp.bmBits,&bi,DIB_RGB_COLORS,SRCCOPY);
	GlobalFree(bmp.bmBits);
		//	DrawIconEx(pDC->m_hDC,0,0,(HICON)LoadImage(theApp.m_hInstance,MAKEINTRESOURCE(IDB_MONOSCOPIO),IMAGE_BITMAP,0,0,LR_DEFAULTCOLOR | LR_SHARED),r.right,r.bottom,0,0,DI_NORMAL);
	return i;
	}

int CVidsendApp::renderBitmap(CDC *dc,CBitmap *b,RECT *r) {
	BITMAP bmp;
	BITMAPINFO bi;
	int i;

	i=b->GetBitmap(&bmp);
	i=bmp.bmWidth*bmp.bmHeight*bmp.bmBitsPixel/8;
	bmp.bmBits=GlobalAlloc(GPTR,i);
	b->GetBitmapBits(i,bmp.bmBits);
	ZeroMemory(&bi,sizeof(BITMAPINFOHEADER));
	bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
	bi.bmiHeader.biWidth=bmp.bmWidth;
	bi.bmiHeader.biHeight=bmp.bmHeight;
	bi.bmiHeader.biPlanes=bmp.bmPlanes;
	bi.bmiHeader.biBitCount=bmp.bmBitsPixel;
	bi.bmiHeader.biCompression=0;
	if(!r->bottom && !r->right) {		// se mancano entrambi, uso le dim. originali
		r->bottom=r->top+bmp.bmHeight;
		r->right=r->top+bmp.bmWidth;
		}
	else if(!r->right)		// se ne manca una, calcolo il ratio dall'altra
		r->right=(bmp.bmWidth*(r->bottom))/bmp.bmHeight;
	else if(!r->bottom)
		r->bottom=(bmp.bmHeight*(r->right))/bmp.bmWidth;
	i=StretchDIBits(dc->m_hDC,r->left,r->bottom-r->top,r->right,r->top-r->bottom,0,0,
		bmp.bmWidth,bmp.bmHeight,bmp.bmBits,&bi,DIB_RGB_COLORS,SRCCOPY);
	GlobalFree(bmp.bmBits);
		//	DrawIconEx(pDC->m_hDC,0,0,(HICON)LoadImage(theApp.m_hInstance,MAKEINTRESOURCE(IDB_MONOSCOPIO),IMAGE_BITMAP,0,0,LR_DEFAULTCOLOR | LR_SHARED),r.right,r.bottom,0,0,DI_NORMAL);
	return i;
	}

int CVidsendApp::renderBitmap(CDC *dc,BITMAPINFO *bi,BYTE *p,RECT *r) {
	int i;

	i=StretchDIBits(dc->m_hDC,r->left,r->top,r->right,r->bottom,0,0,
		bi->bmiHeader.biWidth,bi->bmiHeader.biHeight,p,bi,DIB_RGB_COLORS,SRCCOPY);
	return i;
	}

int CVidsendApp::renderBitmap(CDC *dc,const char *aBitmapFile,RECT *r) {
	CFile hf1;
	BITMAPFILEHEADER bmD;
	BITMAPINFO *bmI;
	int i=0;

	if(hf1.Open(aBitmapFile,CFile::modeRead | CFile::typeBinary)) {
		hf1.Read((void *)&bmD,sizeof(BITMAPFILEHEADER));
		bmI=(LPBITMAPINFO)LocalAlloc(LPTR,1200);
		hf1.Read((void *)bmI,sizeof(BITMAPINFO)+256*4 /*ev.palette*/);
		if(bmI->bmiHeader.biBitCount>0 && bmI->bmiHeader.biBitCount <= 256)
			hf1.Read((void *)bmI->bmiColors,4*bmI->bmiHeader.biBitCount);
		_llseek(hf1,bmD.bfOffBits,FILE_BEGIN);
		BYTE *p=(BYTE *)GlobalAlloc(GPTR,bmI->bmiHeader.biSizeImage);
		if(p) {
			hf1.Read(p,bmI->bmiHeader.biSizeImage);
			i=StretchDIBits(dc->m_hDC,r->left,r->top,r->right,r->bottom,0,0,bmI->bmiHeader.biWidth,bmI->bmiHeader.biHeight,
				p,bmI,DIB_RGB_COLORS,SRCCOPY);
			}
		LocalFree(bmI);
		hf1.Close();
		}

	return i;
	}

BYTE *CVidsendApp::scaleBitmap(BITMAPINFO *sb,BITMAPINFO *db,BYTE *d) {
	register int x,y,x2,y2,xRatio,yRatio;
	register BYTE *p3,*s=((BYTE *)sb)+sizeof(BITMAPINFOHEADER),*p1;
		// ce ne sbattiamo della palette, casomai...
	int xCnt,yCnt;
	int i,j,j1;
	register DWORD n;
	DWORD l=db->bmiHeader.biWidth*db->bmiHeader.biHeight*db->bmiHeader.biBitCount/8;

	xRatio=0;
	yRatio=0;
	x=sb->bmiHeader.biWidth;
	y=sb->bmiHeader.biHeight;
	x2=db->bmiHeader.biWidth;
	y2=db->bmiHeader.biHeight;

	if(!d) {
		d=(BYTE *)GlobalAlloc(GMEM_FIXED,l+10000 /* PATCH! verificare dove sfora...*/ );
		if(!d)
			goto fine;
		}
	if(sb->bmiHeader.biWidth == db->bmiHeader.biWidth && sb->bmiHeader.biHeight==db->bmiHeader.biHeight && sb->bmiHeader.biBitCount==db->bmiHeader.biBitCount) {
		memcpy(d,s,l);
		goto fine;
		}

	for(j=0,j1=0; j<y; j++) {
		p1=s+j*(x*3);
		p3=d+j1*(x2*3);
//		p1=p2;
		xRatio=0;
		for(i=0; i<x; i++) {
			n=*(DWORD *)p1;
			// OCCHIO a accesso DWORD alla fine...
//			n=(*(WORD *)p1) | ((*(BYTE *)(p1+2)) << 16);		// cast a DWORD poteva fallire a fine buffer.. !

			if(x2 < x) {				// se la dest è minore della source...
				*(WORD *)p3=n;
				*(p3+2)=LOBYTE(HIWORD(n));
				do {
					xRatio+=x2;
					p1+=3;
					} while(xRatio<x);
				xRatio-=x;
				p3+=3;
			// forse andrebbe anche incrementato i... non si nota ma... v. o7ecosto
				i++;

				}
			else {
				while(xRatio<x2) {
					*(WORD *)p3=n;
					*(p3+2)=LOBYTE(HIWORD(n));
					p3+=3;
					xRatio+=x;
					}
				xRatio-=x2;
				p1+=3;
				} 

			}
		if(y2 < y) {				// se la dest è minore della source...
			while(yRatio<y) {
				yRatio+=y2;
				j++;
				}
			yRatio-=y;
			j--;
			j1++;
			}
		else {
			int j2=j1+1;
			int x3=x2*3;
			p1=d+j1*x3;
			while(yRatio < (y2-yRatio)) {
				p3=d+j2*x3;
				memcpy(p3,p1,x3);
				yRatio+=y;
				j2++;
				j1++;
				}
			yRatio-=y2;
			} 
		}


fine:
	return d;
	}

int CVidsendApp::adjustBitmap(BYTE *p,short int l,short int c,short int s) {
	int i;

	return i;
	}



typedef BYTE COLOR_BARS_PTR[3][8];		// comodo per farmi un puntatore, v. sotto

const static COLOR_BARS_PTR NTSCColorBars75Amp100SatRGB24 = {
//  Whi Yel Cya Grn Mag Red Blu Blk
    191,  0,191,  0,191,  0,191,  0,    // Blue
    191,191,191,191,  0,  0,  0,  0,    // Green
    191,191,  0,  0,191,191,  0,  0,    // Red
	};

// 100% Amplitude, 100% Saturation
const static COLOR_BARS_PTR NTSCColorBars100Amp100SatRGB24 = {
//  Whi Yel Cya Grn Mag Red Blu Blk
    255,  0,255,  0,255,  0,255,  0,    // Blue
    255,255,255,255,  0,  0,  0,  0,    // Green
    255,255,  0,  0,255,255,  0,  0,    // Red
	};

const static COLOR_BARS_PTR GreyBars100 = {
    255,217,181,145,109, 73, 36,  0,    // Blue
    255,217,181,145,109, 73, 36,  0,    // Green
    255,217,181,145,109, 73, 36,  0,    // Red
	};


CBitmap *CVidsendApp::createTestBitmap(RECT *r,LPBITMAPINFOHEADER bmi,int q) {
	CBitmap *b,b1;
	BITMAP bmp;
	LPBITMAPINFOHEADER bi;
	BYTE *d,*d1;
	int i,x,y;
	COLOR_BARS_PTR *pb;

	b=new CBitmap;
	switch(q) {
		case 0:
			pb=(COLOR_BARS_PTR *)&NTSCColorBars100Amp100SatRGB24;
			break;
		case 1:
			pb=(COLOR_BARS_PTR *)&NTSCColorBars75Amp100SatRGB24;
			break;
		case 2:
			pb=(COLOR_BARS_PTR *)&GreyBars100;
			break;
		case 3:
			q=IDB_MONOSCOPIO;
			goto altro;
			break;
		case 4:
			q=IDB_MONOSCOPIORAI;
			goto altro;
			break;
		case 5:
			q=IDB_TRUEIMG;
			goto altro;
			break;
		}

	i=r->right*r->bottom*bmi->biBitCount/8;
	d=d1=(BYTE *)GlobalAlloc(GMEM_FIXED,i);
	for(x=0; x<r->right; x++) {
		i=(x*8)/(r->right);
		*d1++=(*pb)[0][i];
		*d1++=(*pb)[1][i];
		*d1++=(*pb)[2][i];
		}
	for(y=1; y<r->bottom; y++) {
		memcpy(d1,d,r->right*3);
		d1+=r->right*3;
		}
	goto fine;

altro:
	b1.LoadBitmap(q);
	b1.GetBitmap(&bmp);
	i=bmp.bmWidth*bmp.bmHeight*bmp.bmBitsPixel/8;
	bmp.bmBits=GlobalAlloc(GMEM_FIXED,i);
	b1.GetBitmapBits(i,bmp.bmBits);

	bi=(LPBITMAPINFOHEADER)GlobalAlloc(GMEM_FIXED,i+sizeof(BITMAPINFO));
	ZeroMemory(bi,sizeof(BITMAPINFOHEADER));
	bi->biSize=sizeof(BITMAPINFOHEADER);
	bi->biWidth=bmp.bmWidth;
	bi->biHeight=bmp.bmHeight;
	bi->biPlanes=bmp.bmPlanes;
	bi->biBitCount=bmp.bmBitsPixel;
	bi->biCompression=0;
//	i=StretchDIBits(dc->m_hDC,0,r->bottom,r->right,-r->bottom,0,0,bmp.bmWidth,bmp.bmHeight,bmp.bmBits,&bi,DIB_RGB_COLORS,SRCCOPY);

// arrivano a 32bit... 2018.. non so bene ma faccio così
	//memcpy(((BYTE *)bi)+sizeof(BITMAPINFO),bmp.bmBits,i);
	DWORD step;
	if(bmp.bmBitsPixel==32)
		step=4;
	else
		step=3;
	d=((BYTE *)bi)+sizeof(BITMAPINFOHEADER);
	for(y=bmp.bmHeight; y>0; y--) {
		d1=((BYTE *)bmp.bmBits)+((y-1)*bmp.bmWidth*step);
		for(x=0; x<bmp.bmWidth; x++) {
			*(DWORD *)d=*(DWORD *)d1;
			d+=3;
			d1+=step;
			}
		}

	i=r->right*r->bottom*bmi->biBitCount/8;
	d=theApp.scaleBitmap((LPBITMAPINFO)bi,(LPBITMAPINFO)bmi,NULL);

	GlobalFree(bmp.bmBits);
	GlobalFree(bi);

fine:
	b->CreateBitmap(r->right,r->bottom,1,bmi->biBitCount,d);
	GlobalFree(d);
	return b;
	}


BOOL CVidsendApp::salvaBitmap(HBITMAP hbmpPicture,char *nomefile) {
	int i;
	BITMAPFILEHEADER bmD;
	BITMAPINFO *bmH;
	BITMAP bmp;
	DWORD l;
	BYTE *p;
	CFile f;
	CDC *dc;

	if(f.Open(nomefile,CFile::modeWrite | CFile::modeCreate)) {
//		hbmpPicture=(HBITMAP)::LoadImage(theApp.m_hInstance,MAKEINTRESOURCE(i+IDB_EMOT_001),
//			IMAGE_BITMAP,0,0,LR_CREATEDIBSECTION);
		(CBitmap::FromHandle(hbmpPicture))->GetBitmap(&bmp);
		l=bmp.bmWidth*bmp.bmHeight*4;		// perche "x 4" e non 3??
 		p=(BYTE *)GlobalAlloc(GMEM_FIXED,l+256*4);

		bmD.bfType='MB';
		bmD.bfSize=l+sizeof(BITMAPFILEHEADER)+sizeof(BITMAPINFOHEADER);
		bmD.bfReserved1=bmD.bfReserved2=0;
		bmD.bfOffBits=sizeof(BITMAPFILEHEADER)+sizeof(BITMAPINFOHEADER);
		f.Write((void *)&bmD,sizeof(BITMAPFILEHEADER));
		bmH=(BITMAPINFO *)GlobalAlloc(GPTR,sizeof(BITMAPINFOHEADER));		// palette
		bmH->bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
		bmH->bmiHeader.biWidth=bmp.bmWidth;
		bmH->bmiHeader.biHeight=bmp.bmHeight;
		bmH->bmiHeader.biBitCount=24 /*bmp.bmBitsPixel*/;
		bmH->bmiHeader.biPlanes=bmp.bmPlanes;
		bmH->bmiHeader.biClrImportant=bmH->bmiHeader.biClrUsed=0;
		bmH->bmiHeader.biXPelsPerMeter=bmH->bmiHeader.biYPelsPerMeter=0;
		bmH->bmiHeader.biCompression=0;
		bmH->bmiHeader.biSizeImage=l;

//						i=GetBitmapBits(hbmpPicture,l,p);
		dc=theApp.m_pMainWnd->GetDC();
		i=GetDIBits(dc->m_hDC,hbmpPicture,0,bmp.bmHeight,p,bmH,DIB_RGB_COLORS);
		theApp.m_pMainWnd->ReleaseDC(dc);
		f.Write((void *)bmH,sizeof(BITMAPINFOHEADER));
		f.Write(p,l);
		f.Close();
		GlobalFree(p);
		GlobalFree(bmH);
//		DeleteObject(hbmpPicture);
		return 0;
		}
	return 1;
	}


BYTE *CVidsendApp::createTestWave(WAVEFORMATEX *wf, DWORD *t, int freq, int sweep) {
	// sempre un secondo di audio, secondo le spec. in wf
	BYTE *d,*d1;
	int i,k,k2=0;
	DWORD x;

	d=d1=(BYTE *)GlobalAlloc(GPTR,wf->nAvgBytesPerSec);
	for(x=0; x<wf->nSamplesPerSec; x++) {
		k=(wf->nSamplesPerSec/freq)+k2;

//			e[i]=i & k ? 0xc0 : 0x40;		// quadra
		if(wf->wBitsPerSample==8) {
			*d1++=128+(80*sin((x*PI)/k));		// sinus
			if(wf->nChannels> 1)
				*d1++=128+(80*sin((x*PI)/(k/2)));		// canale 2 = 2*freq!
			}
		else {
			*((WORD *)d1)=32768+(80*256*sin((x*PI)/k));		// sinus
			d1+=2;
			if(wf->nChannels > 1) {
				*((WORD *)d1)=32768+(80*256*sin((x*PI)/(k/2)));		// canale 2 = 2*freq!
				d1+=2;
				}
			}

		if(sweep) {
			if(!(x % 400)) {
				k2++;
				if(k2==3)
					k2=-3;
				}
			}

		}
	*t=d1-d;
	return d;
	}

CString CVidsendApp::creaStringaDaGiorno() {
	CString S;
	S=CTime::GetCurrentTime().Format("%Y%m%d.avi");
	return S;
	}


char *CVidsendApp::Scramble(char *d, const char *s, int n, DWORD key) {
	register int i,ch,k;

	*d++=n;			// primo byte indica la lunghezza della pasw criptata

	k=LOBYTE(LOWORD(key))+LOBYTE(HIWORD(key));
	for(i=0; i<n; i++) {
		ch=*s++;
		*d++ = myCRC->getCrcTable((ch+k+i) & 255);
		}
	*d=0;				// resta x sicurezza, anche se i byte possono contenere "0"...
	return d;
	}

#ifndef _CAMPARTY_MODE

int CVidsendApp::UtenteCheck(CVidsendSet2_ *,const CString nome,const BYTE *pasw,DWORD *user,CTime *ct,char *tariffa,
														 double *sconto,char *passSconto,DWORD bExib, 
														 DWORD serNum,DWORD idserver,BOOL allowMultiAccess) {
	int i=0;
	char myBuf[64];
	long l;

	*user=0;
	if(mySetUtenti) {
		mySetUtenti->m_strFilter="username='";
		mySetUtenti->m_strFilter+=nome;
		mySetUtenti->m_strFilter+="'";
		mySetUtenti->Requery();
		if(!mySetUtenti->IsEOF()) {

/*				wsprintf(myBuf,"trovato utente, abilitato=%d",mySetUtenti->m_Abilitato);
			AfxMessageBox(myBuf);*/

			if(!bExib && !mySetUtenti->m_Abilitato)		// UTENTE guest dev'essere abilitato (23/1/03), exib non necessariamente
				i=-3;
			else {

				if(mySetSernum) {
					mySetSernum->m_strFilter.Format("idutente=%u and sernum='%08X'",mySetUtenti->m_ID,serNum);
					mySetSernum->Requery();
					if(!mySetSernum->IsEOF()) {
						if(mySetSernum->m_AccessoSis==1 /* || mySetSernum->m_AccessoChat==0*/) {
							i=-5;
							}
						mySetSernum->Edit();
						mySetSernum->m_DataAccesso=CTime::GetCurrentTime();
						mySetSernum->m_OraAccesso=CTime::GetCurrentTime();
						mySetSernum->Update();
						}
					else {
						mySetSernum->AddNew();
						mySetSernum->m_IDUtente=mySetUtenti->m_ID;
						mySetSernum->m_IDEsib=idserver;
//								mySetSernum->m_IP=0;		/*  */
						mySetSernum->m_SerNum.Format("%08X",serNum);
						mySetSernum->m_AccessoChat=0;
						mySetSernum->m_AccessoSis=0;
						mySetSernum->m_DataAccesso=CTime::GetCurrentTime();
						mySetSernum->m_OraAccesso=CTime::GetCurrentTime();
						mySetSernum->m_ContatoreAccessi=0;
						mySetSernum->Update();
						}
					}
				else {
					i=-5;
					}
				if(i==-5)
					goto fine;

				Scramble(myBuf,(LPCTSTR)mySetUtenti->m_Password,mySetUtenti->m_Password.GetLength(),serNum);
				*user=mySetUtenti->m_ID;
				strncpy(tariffa,(LPCTSTR)mySetUtenti->m_Tariffa,15);
				*sconto=mySetUtenti->m_Sconto;
				strncpy(passSconto,(LPCTSTR)mySetUtenti->m_PasswordSconto,15);

				//gestire int in tabella utenti vs. CTime "ct"
				l=mySetUtenti->m_TimedConn;		// puo' essere negativo...
				if(l<5)
					l=5;
				l=mySetUtenti->m_bTimedConn ? l : 0;
				*ct=l;

				char *p2=(char *)(LPCTSTR)pasw;
				if(!memcmp(myBuf+1,((LPCTSTR)pasw)+1,myBuf[0])) {
					if(!allowMultiAccess)
						i=-1;		// imposto per "non disponibile"
					else {
						i=1;
						goto fine;
						}
					if(mySetUtentiOnline) {
						mySetUtentiOnline->m_strFilter.Format("idutente=%u and esib=1",mySetUtenti->m_ID);
						mySetUtentiOnline->Requery();
						if(mySetUtentiOnline->IsEOF()) 		// non deve essere gia' on-line!
							i=1;
						else
							i=-4;
						}
					}
				else
					i=-2;
				}
			}
		else {
not_avail:
			i=-1;
fine:	;
			}
		}
	return i;
	}

DWORD CVidsendApp::AggUtenteOnline(CVidsendSet5 *,DWORD id,DWORD idUtente,LPCTSTR IP,
																	 DWORD versione,DWORD versioneW,DWORD serNum) {
	char myBuf[64];

	if(mySetLogConnessioni) {
//						mySetLogConnessioni->m_pDatabase->BeginTrans();

		if(!id)
			goto do_insert;
//			mySetLogConnessioni->m_strFilter.Format("id=%u",id);
//			mySetLogConnessioni->Requery();
//			if(mySetLogConnessioni->IsEOF()) {
		if(!mySetLogConnessioni->goTo(id)) {
do_insert:
			mySetLogConnessioni->AddNew();
			mySetLogConnessioni->m_IDUtente=idUtente;
			mySetLogConnessioni->m_IP=IP;
			mySetLogConnessioni->m_OraInizio=CTime::GetCurrentTime();
			mySetLogConnessioni->SetFieldNull(&mySetLogConnessioni->m_OraFine);
//			mySetLogConnessioni->m_OraFine=0;
			mySetLogConnessioni->m_SerNum.Format("%08X",serNum);;
			mySetLogConnessioni->m_Versione=versione;
			mySetLogConnessioni->m_VersioneW=versioneW;
			mySetLogConnessioni->Update();
			// ...sta merda da' errore sui campi data se il database e' vuoto! senza parole...
//						mySetLogConnessioni->m_pDatabase->CommitTrans();

			mySetLogConnessioni->m_strSort="id DESC";
			mySetLogConnessioni->m_strFilter.Empty();
			mySetLogConnessioni->Requery();
			
			id=mySetLogConnessioni->m_ID;
			}
		else {
			mySetLogConnessioni->Edit();
			mySetLogConnessioni->m_OraFine=CTime::GetCurrentTime();
			mySetLogConnessioni->Update();

			}
		}
	return id;
	}

DWORD CVidsendApp::AggUtenteOnline(CVidsendSet5Ex *,DWORD id,
																	 DWORD idUtente,LPCTSTR IP,DWORD versione,DWORD versioneW,DWORD serNum) {
	char *p,myBuf[64];

	if(mySetLogConnessioniEx) {
//						mySetLogConnessioniEx->m_pDatabase->BeginTrans();
		if(!id) {
			mySetLogConnessioniEx->AddNew();
			mySetLogConnessioniEx->m_IDUtente=idUtente;
			mySetLogConnessioniEx->m_IP=IP;
			mySetLogConnessioniEx->m_OraInizio=CTime::GetCurrentTime();
			mySetLogConnessioniEx->SetFieldNull(&mySetLogConnessioniEx->m_OraCorrente);
			mySetLogConnessioniEx->SetFieldNull(&mySetLogConnessioniEx->m_OraFine);
//			mySetLogConnessioniEx->m_OraFine=0;
		// ...sta merda da' errore sui campi data se il database e' vuoto! senza parole...
			mySetLogConnessioniEx->m_SessionID=rand();
			mySetLogConnessioniEx->m_Versione=versione;
			wsprintf(myBuf,"%08X",serNum);
			mySetLogConnessioniEx->m_SerNum=myBuf;
			mySetLogConnessioniEx->m_VersioneW=versioneW;
			mySetLogConnessioniEx->Update();


//						mySetLogConnessioniEx->m_pDatabase->CommitTrans();
			mySetLogConnessioniEx->m_strSort="id DESC";
			mySetLogConnessioniEx->m_strFilter.Empty();
			mySetLogConnessioniEx->Requery();
			id=mySetLogConnessioniEx->m_ID;
			goto do_update;
			}
		else {
			if(mySetLogConnessioniEx->goTo(id)) {
do_update:
				mySetLogConnessioniEx->Edit();
				mySetLogConnessioniEx->m_OraCorrente=CTime::GetCurrentTime();
//				mySetLogConnessioniEx->m_OraFine=CTime::GetCurrentTime();
				mySetLogConnessioniEx->Update();
				}
			else {
				p="tentativo di aggiornare un record LogConnessioniEx non più presente!";
#ifdef _DEBUG
				AfxMessageBox(p);
#else
				if(theApp.FileSpool)
					*theApp.FileSpool << p;
#endif
				}
			}
		}
	return id;
	}

#ifdef DARIO
DWORD 
CVidsendApp::AggUtenteOnline(CVidsendSet4 *,DWORD id,DWORD idUtenteOsserv,DWORD idUtenteExib,
														 LPCTSTR tariffa,BYTE comepago,LPCTSTR ipUtenteOsserv,LPCTSTR ipUtenteExib,DWORD versione /*per ora qui non lo uso*/) {
// questo per i client (v. vidsendSockets)
	CString myTariffa;
	BYTE myComePago;		// per ora ignoro quelli passati.. forse serviranno in futuro
	char *p;

	if(mySetLogChiamate) {
//						mySetLogConnessioni->m_pDatabase->BeginTrans();
			if(!id) {
				CString S;
				S.Format("l'osservatore %u sta entrando a vedere l'exhibitor %u: controllo se c'e' già...",idUtenteOsserv,idUtenteExib);
#ifdef _DEBUG
				AfxMessageBox(S);
#else
				if(theApp.FileSpool)
					*theApp.FileSpool << (char *)(LPCTSTR)S;
#endif
				mySetLogChiamate->m_strFilter.Format("idOsserv=%u and idEspos=%u",idUtenteOsserv,idUtenteExib);
				// in questo modo se un guest crashato va a rientrare, non creo un altro record ma riciclo il precedente
				TRY {
					mySetLogChiamate->Requery();
					}
				CATCH(CDBException,e) {
					char p1[1024];
					_tcscpy(p1,"errore nella ricerca di un osservatore in LogChiamate: ");
					_tcscat(p1,e->m_strStateNativeOrigin);
#ifdef _DEBUG
					AfxMessageBox(p1);
#else
					if(theApp.FileSpool)
						*theApp.FileSpool << p1;
#endif
					}
				END_CATCH
				if(mySetLogChiamate->IsEOF()) {
					CString S;
					S="non trovato!";
#ifdef _DEBUG
					AfxMessageBox(S);
#else
					if(theApp.FileSpool)
					*theApp.FileSpool << (char *)(LPCTSTR)S;
#endif

					myTariffa.Empty();
					if(mySetUt) {
						if(mySetUt->goTo(idUtenteExib))
							myTariffa=mySetUt->m_Tariffa;
						else {
							p="errore in ricerca dati exhibitor";
#ifdef _DEBUG
							AfxMessageBox(p);
#else
							if(FileSpool)
								*FileSpool << p;
#endif
							}
						}
					myComePago=0;
					if(mySetUt) {
						if(mySetUt->goTo(idUtenteOsserv))
							myComePago=mySetUt->m_ComePago;
						else {
							p="errore in ricerca dati guest";
#ifdef _DEBUG
							AfxMessageBox(p);
#else
							if(FileSpool)
								*FileSpool << p;
#endif
									}
						}
					mySetLogChiamate->AddNew();
					mySetLogChiamate->m_IDEspos=idUtenteExib;
					mySetLogChiamate->m_IDOsserv=idUtenteOsserv;
					mySetLogChiamate->m_OraInizio=CTime::GetCurrentTime();
					mySetLogChiamate->SetFieldNull(&mySetLogChiamate->m_OraFine);
//					mySetLogChiamate->m_OraFine=0;
					mySetLogChiamate->m_Tariffa=myTariffa;
					mySetLogChiamate->m_ComePago=myComePago;
					mySetLogChiamate->m_IPEspos=ipUtenteExib;
					mySetLogChiamate->m_IPOsserv=ipUtenteOsserv;
					mySetLogChiamate->Update();
				// ...sta merda da' errore sui campi data se il database e' vuoto! senza parole...
	//						mySetLogChiamate->m_pDatabase->CommitTrans();
					mySetLogChiamate->m_strSort="id DESC";
					mySetLogChiamate->m_strFilter.Empty();
					mySetLogChiamate->Requery();
					id=mySetLogChiamate->m_ID;
					}
				else {
					id=mySetLogChiamate->m_ID;
					CString S;
					S.Format("trovato: %u!",id);
#ifdef _DEBUG
					AfxMessageBox(S);
#else
					if(theApp.FileSpool)
					*theApp.FileSpool << (char *)(LPCTSTR)S;
#endif
					}
				goto do_update;
				}


			if(mySetLogChiamate->goTo(id)) {
do_update:
				mySetLogChiamate->Edit();
				mySetLogChiamate->m_OraFine=CTime::GetCurrentTime();
				mySetLogChiamate->Update();
				}
			else {
				p="tentativo di aggiornare un record LogChiamate non più presente!";
#ifdef _DEBUG
				AfxMessageBox(p);
#else
				if(theApp.FileSpool)
					*theApp.FileSpool << p;
#endif
				}
		}
	return id;
	}
#endif			// non la uso: doveva servire a controllare se guest crashati rientravano, ma senza un flag di "chiusa sessione correttamente" non va, ossia trova la sessione precedente e la allunga...


DWORD 
CVidsendApp::AggUtenteOnline(CVidsendSet4 *,DWORD id,DWORD idUtenteOsserv,DWORD idUtenteExib,
														 LPCTSTR tariffa,BYTE comepago,LPCTSTR ipUtenteOsserv,LPCTSTR ipUtenteExib,DWORD versione /*per ora qui non lo uso*/) {
// questo per i client (v. vidsendSockets)
	CString myTariffa,myTariffaD;
	BYTE myComePago;		// per ora ignoro quelli passati.. forse serviranno in futuro
	char *p;

	if(mySetLogChiamate) {
//						mySetLogConnessioni->m_pDatabase->BeginTrans();
		if(!id) {
			CString S;
			S.Format("l'osservatore %u sta entrando a vedere l'exhibitor %u",idUtenteOsserv,idUtenteExib);
#ifdef _DEBUG
			AfxMessageBox(S);
#else
			if(theApp.FileSpool)
				*theApp.FileSpool << (char *)(LPCTSTR)S;
#endif

			myTariffa.Empty();
			if(mySetUt) {
				if(mySetUt->goTo(idUtenteExib)) {


//							char myBuf[256];
//							wsprintf(myBuf,"ID=%d,ComePago=%d,Abilitato=%d,Caratteristiche=%d,Gruppo=%d",mySetUt->m_ID,mySetUt->m_ComePago,mySetUt->m_Abilitato,mySetUt->m_Caratteristiche,mySetUt->m_Gruppo);
//							AfxMessageBox(myBuf);


					myTariffaD=mySetUt->m_TariffaD;
					myTariffa=mySetUt->m_Tariffa;
					}
				else {
					p="errore in ricerca dati exhibitor";
#ifdef _DEBUG
					AfxMessageBox(p);
#else
					if(FileSpool)
						*FileSpool << p;
#endif
					}
				}
			myComePago=0;
			if(mySetUt) {
				if(mySetUt->goTo(idUtenteOsserv))
					myComePago=mySetUt->m_ComePago;
				else {
					p="errore in ricerca dati guest";
#ifdef _DEBUG
					AfxMessageBox(p);
#else
					if(FileSpool)
						*FileSpool << p;
#endif
					}
				}

			mySetLogChiamate->AddNew();
			mySetLogChiamate->m_IDEspos=idUtenteExib;
			mySetLogChiamate->m_IDOsserv=idUtenteOsserv;
			mySetLogChiamate->m_OraInizio=CTime::GetCurrentTime();
//			mySetLogChiamate->m_OraFine=0;
			mySetLogChiamate->SetFieldNull(&mySetLogChiamate->m_OraFine);
			switch(mySetUt->m_ComePago) {
				case 1:
					mySetLogChiamate->m_Tariffa=myTariffaD;
					break;
				case 2:
				default:
					mySetLogChiamate->m_Tariffa=myTariffa;
					break;
				}
			mySetLogChiamate->m_ComePago=myComePago;
			mySetLogChiamate->m_IPEspos=ipUtenteExib;
			mySetLogChiamate->m_IPOsserv=ipUtenteOsserv;
			mySetLogChiamate->Update();
		// ...sta merda da' errore sui campi data se il database e' vuoto! senza parole...
//						mySetLogChiamate->m_pDatabase->CommitTrans();

			mySetLogChiamate->m_strSort="id DESC";
			mySetLogChiamate->m_strFilter.Empty();
			mySetLogChiamate->Requery();
			id=mySetLogChiamate->m_ID;

			goto do_update;				// un po' stupido, ma per ora lascio cosi'...
			}

		if(mySetLogChiamate->goTo(id)) {
do_update:
			mySetLogChiamate->Edit();
			mySetLogChiamate->m_OraFine=CTime::GetCurrentTime();
			mySetLogChiamate->Update();

			}
		else {
			p="tentativo di aggiornare un record LogChiamate non più presente!";
#ifdef _DEBUG
			AfxMessageBox(p);
#else
			if(theApp.FileSpool)
				*theApp.FileSpool << p;
#endif
			}
		}
	return id;
	}


int CVidsendApp::UtenteOnline(CVidsendSet2_ *,CString s,CString IP,int mode,DWORD IDServer,
															DWORD versione,DWORD versioneW,DWORD serNum) {
	int i=0,j=0;
	char *p;

	if(mySetUtenti) {
		mySetUtenti->m_strFilter="username='";
		mySetUtenti->m_strFilter+=s;
		mySetUtenti->m_strFilter+="'";

		TRY {
			mySetUtenti->Requery();
			}
		CATCH(CDBException,e) {
			char p1[1024];
			_tcscpy(p1,"errore nella ricerca di un utente in UtentiOnLine: ");
			_tcscat(p1,e->m_strStateNativeOrigin);
#ifdef _DEBUG
			AfxMessageBox(p1);
#else
			if(theApp.FileSpool)
				*theApp.FileSpool << p1;
#endif
			}
		END_CATCH
		
		if(!mySetUtenti->IsEOF()) {

			TRY {
				mySetUtenti->Edit();
				mySetUtenti->m_LoginTime=CTime::GetCurrentTime();
				mySetUtenti->Update();
				}
			CATCH(CDBException,e) {
				char p1[1024];
				_tcscpy(p1,"errore nell'aggiornamento di un utente in UtentiOnLine: ");
				_tcscat(p1,e->m_strStateNativeOrigin);
#ifdef _DEBUG
				AfxMessageBox(p1);
#else
				if(theApp.FileSpool)
					*theApp.FileSpool << p1;
#endif
				}
			END_CATCH
			
//				if(mode) {			// solo gli exhibitor in tabella on-line... NO!
			if(mySetUtentiOnline) {
//						mySetUtentiOnline->m_pDatabase->BeginTrans();
				mySetUtentiOnline->m_strFilter.Format("idutente=%u",mySetUtenti->m_ID);

					
				TRY {
					mySetUtentiOnline->Requery();
					}
				CATCH(CDBException,e) {
					char p1[1024];
					_tcscpy(p1,"errore nella ricerca di idUtente in UtentiOnLine: ");
					_tcscat(p1,e->m_strStateNativeOrigin);
#ifdef _DEBUG
					AfxMessageBox(p1);
#else
					if(theApp.FileSpool)
						*theApp.FileSpool << p1;
#endif
					}
				END_CATCH

				if(mySetUtentiOnline->IsEOF()) {
//							if(mode) {			// solo gli exhibitor in tabella on-line... NO!

							
				TRY {
					mySetUtentiOnline->AddNew();
					mySetUtentiOnline->m_IDUtente=mySetUtenti->m_ID;
					mySetUtentiOnline->m_IP=IP;
					mySetUtentiOnline->m_Esib=mode;
					mySetUtentiOnline->m_TipoGuest=mode ? 0 : 2;		// questo non avrebbe + molto senso...
					mySetUtentiOnline->m_LockCount=1;
					i=mySetUtentiOnline->Update();
					}
				CATCH(CDBException,e) {
					char p1[1024];
					_tcscpy(p1,"errore nell'inserimento di un Utente in UtentiOnLine: ");
					_tcscat(p1,e->m_strStateNativeOrigin);
#ifdef _DEBUG
					AfxMessageBox(p1);
#else
					if(theApp.FileSpool)
						*theApp.FileSpool << p1;
#endif
					}
				END_CATCH

							
							//exception...
/*								}
						else {
							p="Tentativo di mettere OnLine un guest";
#ifdef _DEBUG
							AfxMessageBox(p);
#else
							if(FileSpool)
								*FileSpool << p;
#endif
							i=1;		// per forza, se lascio in on-line i guest...
							}*/
					}
				else {

					
					TRY {
						mySetUtentiOnline->Edit();
						mySetUtentiOnline->m_LockCount++;
						if(mode)
							mySetUtentiOnline->m_TipoGuest=0;
						else
							mySetUtentiOnline->m_TipoGuest |= 2;
						i=mySetUtentiOnline->Update();
						}
					CATCH(CDBException,e) {
						char p1[1024];
						_tcscpy(p1,"errore nell'aggiornamento di un Utente (lockCount) in UtentiOnLine: ");
						_tcscat(p1,e->m_strStateNativeOrigin);
#ifdef _DEBUG
						AfxMessageBox(p1);
#else
						if(theApp.FileSpool)
							*theApp.FileSpool << p1;
#endif
						}
					END_CATCH
						
					}
//						mySetUtentiOnline->m_pDatabase->CommitTrans();
				}
//					}
//				i=AggUtenteOnline(NULL,0,mySetUtenti->m_ID,(LPCTSTR)IP);	// questa per ora non la usiamo, ossia non uso Logconnessioni ma...
			if(i) {
				if(!mode)	{			// ...e solo i client in tabella chiamate...
					i=AggUtenteOnline((CVidsendSet4 *)NULL,0,mySetUtenti->m_ID,IDServer,
						mySetUtenti->m_Tariffa,mySetUtenti->m_ComePago /*,NULL,NULL,versione*/);		// ...LogChiamate (e SOLO per gli osservatori!)
				//mettiamo IP anche in tabella chiamate?
				//direi che lo mettiamo qua in LogConnessioni
					j=AggUtenteOnline((CVidsendSet5 *)NULL,0,mySetUtenti->m_ID,IP,
						versione,versioneW,serNum);		// ...LogConnessioni (per i clienti!)
					}
				else {
					i=AggUtenteOnline((CVidsendSet5Ex *)NULL,0,mySetUtenti->m_ID,IP,
						versione,versioneW,serNum);		// ...LogConnessioniEx (SOLO per gli exhibitor!)
					}
				}
			}
		else {
			p="Utente sconosciuto!";
#ifdef _DEBUG
			AfxMessageBox(p);
#else
			if(FileSpool)
				*FileSpool << p;
#endif
			}
		}
	return i;
	}

int CVidsendApp::UtenteOffline(CVidsendSet2_ *,CString s,DWORD nLogConn,int mode,int bForced) {
	int i=0;
	char *p;

	if(mySetUtenti) {
		mySetUtenti->m_strFilter="username='";
		mySetUtenti->m_strFilter+=s;
		mySetUtenti->m_strFilter+="'";

			
		TRY {
			mySetUtenti->Requery();
			}
		CATCH(CDBException,e) {
			char p1[1024];
			_tcscpy(p1,"errore nella ricerca in Utenti: ");
			_tcscat(p1,e->m_strStateNativeOrigin);
#ifdef _DEBUG
			AfxMessageBox(p1);
#else
			if(theApp.FileSpool)
				*theApp.FileSpool << p1;
#endif
			}
		END_CATCH
			
		if(!mySetUtenti->IsEOF()) {

			TRY {
				mySetUtenti->Edit();
				if(!bForced)
					mySetUtenti->m_LogoutTime=CTime::GetCurrentTime();
				else
				mySetUtenti->SetFieldNull(&mySetUtenti->m_LogoutTime);
//					mySetUtenti->m_LogoutTime=0;
				mySetUtenti->Update();
				}
			CATCH(CDBException,e) {
				char p1[1024];
				_tcscpy(p1,"errore nell'aggiornamento di Utenti: ");
				_tcscat(p1,e->m_strStateNativeOrigin);
#ifdef _DEBUG
				AfxMessageBox(p1);
#else
				if(theApp.FileSpool)
					*theApp.FileSpool << p1;
#endif
				}
			END_CATCH

			if(mySetUtentiOnline) {
					if(mode) {		// se exhib ... 
						if(mySetUtentiOnline->goToUtente(mySetUtenti->m_ID)) {
//						mySetLogConnessioni->m_pDatabase->BeginTrans();

							
							TRY {
								mySetUtentiOnline->Delete();
								}
							CATCH(CDBException,e) {
								char p1[1024];
								_tcscpy(p1,"errore nella cancellazione di un Utente: ");
								_tcscat(p1,e->m_strStateNativeOrigin);
#ifdef _DEBUG
								AfxMessageBox(p1);
#else
								if(theApp.FileSpool)
									*theApp.FileSpool << p1;
#endif
								}
							END_CATCH

							// ripetere per tutti i record che combaciano, per cancellare ev. casini?
//						mySetLogConnessioni->m_pDatabase->CommitTrans();
							}
						else
							*theApp.FileSpool << "Utente (exhibitor) on line non trovato!";
						}
					else {
						if(mySetUtentiOnline->goToUtente(mySetUtenti->m_ID)) {
//						mySetLogConnessioni->m_pDatabase->BeginTrans();

						TRY {
							mySetUtentiOnline->Edit();
							mySetUtentiOnline->m_TipoGuest &= ~2;
							mySetUtentiOnline->Update();
							}
						CATCH(CDBException,e) {
							char p1[1024];
							_tcscpy(p1,"errore nell'aggiornamento di un UtenteOnLine (TipoGuest): ");
							_tcscat(p1,e->m_strStateNativeOrigin);
#ifdef _DEBUG
							AfxMessageBox(p1);
#else
							if(theApp.FileSpool)
								*theApp.FileSpool << p1;
#endif
							}
						END_CATCH

							// ripetere per tutti i record che combaciano, per cancellare ev. casini?
//						mySetLogConnessioni->m_pDatabase->CommitTrans();
							}
						else
							*theApp.FileSpool << "Utente (guest) on line non trovato!";
						}
				}
			/*mySetLogConnessioni=new CVidsendSet5(theDB);
			if(mySetLogConnessioni) {
				if(mySetLogConnessioni->Open(AFX_DB_USE_DEFAULT_TYPE,NULL,CRecordset::none)) {
					if(mySetLogConnessioni->goTo(nLogConn)) {
//						mySetLogConnessioni->m_pDatabase->BeginTrans();
						mySetLogConnessioni->Edit();
						mySetLogConnessioni->m_OraFine=CTime::GetCurrentTime();
						mySetLogConnessioni->Update();
//						mySetLogConnessioni->m_pDatabase->CommitTrans();
						}
					else {
						p="Record log connessioni non trovato!";
#ifdef _DEBUG
						AfxMessageBox(p);
#else
						if(FileSpool)
							*FileSpool << p;
#endif
						}
					mySetLogConnessioni->Close();
					}
				delete mySetLogConnessioni;
				}*/


				if(mySetLogConnessioniEx) {
						mySetLogConnessioniEx->m_strFilter.Format("idutente=%u",mySetUtenti->m_ID);


						TRY {
							mySetLogConnessioniEx->Requery();
							}
						CATCH(CDBException,e) {
							char p1[1024];
							_tcscpy(p1,"errore nella ricerca di ExibOnLine: ");
							_tcscat(p1,e->m_strStateNativeOrigin);
#ifdef _DEBUG
							AfxMessageBox(p1);
#else
							if(theApp.FileSpool)
								*theApp.FileSpool << p1;
#endif
							}
						END_CATCH

						if(!mySetLogConnessioniEx->IsEOF()) {
							TRY {
//								mySetLogConnessioniEx->Delete();		// era cosi'... facciamo EDIT 22/9/03
								mySetLogConnessioniEx->Edit();
								mySetLogConnessioniEx->SetFieldNull(&mySetLogConnessioniEx->m_OraCorrente);
								mySetLogConnessioniEx->m_OraFine=CTime::GetCurrentTime();
								mySetLogConnessioniEx->Update();
								}
							CATCH(CDBException,e) {
								char p1[1024];
								_tcscpy(p1,"errore nella cancellazione di un ExibOnLine: ");
								_tcscat(p1,e->m_strStateNativeOrigin);
#ifdef _DEBUG
								AfxMessageBox(p1);
#else
								if(theApp.FileSpool)
									*theApp.FileSpool << p1;
#endif
								}
							END_CATCH

							}
						else {
							p="Exhibitor on-line non trovato!";
#ifdef _DEBUG
							AfxMessageBox(p);
#else
							if(FileSpool)
								*FileSpool << p;
#endif
							}
					}
							


			if(nLogConn) {
				if(!mode) {		// se client...
					if(mySetLogChiamate) {
							if(mySetLogChiamate->goTo(nLogConn)) {
	//						mySetLogChiamate->m_pDatabase->BeginTrans();
								TRY {
									mySetLogChiamate->Edit();
									mySetLogChiamate->m_OraFine=CTime::GetCurrentTime();
									if(!bForced)
										mySetLogChiamate->m_Chiusa=1;
									else
										mySetLogChiamate->m_Chiusa=2;
									mySetLogChiamate->Update();
									}
								CATCH(CDBException,e) {
									char p1[1024];
									_tcscpy(p1,"errore nell'aggiornamento di LogChiamate (OraFine): ");
									_tcscat(p1,e->m_strStateNativeOrigin);
#ifdef _DEBUG
									AfxMessageBox(p1);
#else
									if(theApp.FileSpool)
										*theApp.FileSpool << p1;
#endif
									}
								END_CATCH
	//						mySetLogChiamate->m_pDatabase->CommitTrans();
								}
							else {
								p="Record log chiamate guest non trovato!";
#ifdef _DEBUG
								AfxMessageBox(p);
#else
								if(FileSpool)
									*FileSpool << p;
#endif
								}
						}
					}
				}
			}
		else {
			p="Utente sconosciuto!";
#ifdef _DEBUG
			AfxMessageBox(p);
#else
			if(FileSpool)
				*FileSpool << p;
#endif
			}
		}

	return i;
	}

int CVidsendApp::UtenteOffline(CAuthSrvSocket2 *ss,int bForced) {
	
	return UtenteOffline(NULL,ss->nome,ss->numLogConn,ss->proprieta & 1,bForced);
	}

#endif

/////////////////////////////////////////////////////////////////////////////
CCommandLineInfoEx::CCommandLineInfoEx() {

	m_restoreOptions=FALSE;
	m_openVideoServer=m_openChatServer=FALSE;
	m_debugMode=FALSE;
	m_ODBCname.Empty();
	}

void CCommandLineInfoEx::ParseParam(LPCTSTR lpszParam, BOOL bFlag, BOOL bLast ) {

	CCommandLineInfo::ParseParam(lpszParam,bFlag,bLast);
	if(bFlag) {
		if(!_tcsicmp(lpszParam,"nobanner"))
			m_bShowSplash=FALSE;
		if(!_tcsicmp(lpszParam,"gd"))
			m_restoreOptions=TRUE;
		if(!_tcsnicmp(lpszParam,"debug=",6))
			m_debugMode=atoi(lpszParam+6);
		else if(!_tcsicmp(lpszParam,"debug"))
			m_debugMode=1;
		if(!_tcsicmp(lpszParam,"vs"))
			m_openVideoServer=TRUE;
		if(!_tcsicmp(lpszParam,"cs"))
			m_openChatServer=TRUE;
		if(!_tcsnicmp(lpszParam,"odbc",4))
			m_ODBCname=lpszParam+5;
		}

	}


// App command to run the dialog
void CVidsendApp::OnAppAbout() {
	CAboutDlg aboutDlg;

	aboutDlg.DoModal();
	}


// -----------------------------------------------------------------------
CString CFileSizeString::InsertSeparator(DWORD dwNumber) {

  Format("%u", dwNumber);
  
  for(int i = GetLength()-3; i > 0; i -= 3) {
    Insert(i, ",");
    }

  return *this;
  }

CString CFileSizeString::FormatSize(DWORD dwFileSize) {
  static const DWORD dwKB = 1024;          // Kilobyte
  static const DWORD dwMB = 1024 * dwKB;   // Megabyte
  static const DWORD dwGB = 1024 * dwMB;   // Gigabyte

  DWORD dwNumber, dwRemainder;

  if(dwFileSize < dwKB) {
//    InsertSeparator(dwFileSize) + " B";		// non funziona (usare  *this o Format) e poi non mi piace!
    InsertSeparator(dwFileSize);
		} 
  else {
    if(dwFileSize < dwMB) {
      dwNumber = dwFileSize / dwKB;
      dwRemainder = (dwFileSize * 100 / dwKB) % 100;

      Format("%s.%02d KB", (LPCSTR)InsertSeparator(dwNumber), dwRemainder);
			}
    else {
      if(dwFileSize < dwGB) {
        dwNumber = dwFileSize / dwMB;
        dwRemainder = (dwFileSize * 100 / dwMB) % 100;
        Format("%s.%02d MB", InsertSeparator(dwNumber), dwRemainder);
				}
      else {
        if(dwFileSize >= dwGB) {
          dwNumber = dwFileSize / dwGB;
          dwRemainder = (dwFileSize * 100 / dwGB) % 100;
          Format("%s.%02d GB", InsertSeparator(dwNumber), dwRemainder);
					}
				}
			}
		}

  // Display decimal points only if needed
  // another alternative to this approach is to check before calling str.Format, and 
  // have separate cases depending on whether dwRemainder == 0 or not.
  Replace(".00", "");

	return *this;
	}


//------------------------------------------------------------------------------------
CChecksum::CChecksum(WORD w,DWORD t,DWORD iv,BOOL rIn,BOOL rOut) {
  register DWORD remainder;
	int	dividend;
	register BYTE bit;

	width=w;
	polynomial=t;
	topbit=(1 << (width - 1));
	widmask=(((1L << (width-1)) - 1L) << 1) | 1L;
	initValue=iv;
	reflectInput=rIn;
	reflectOutput=rOut;
	xorOutput=0;

	TRY {
		crcTable=new DWORD[256];
		}
	CATCH(CMemoryException,e) {
		}
	END_CATCH

  for(dividend = 0; dividend < 256; dividend++) {
    if(reflectInput)
			remainder = reflect(dividend,8) << (width - 8);
		else
			remainder = dividend << (width - 8);

    for(bit = /*width*/ 8; bit > 0; --bit) {
      if(remainder & topbit)
        remainder = (remainder << 1) ^ polynomial;
      else
        remainder <<= 1;
			}

    if(reflectInput)
	    crcTable[dividend] = reflect(remainder,width) & widmask;
		else
	    crcTable[dividend] = remainder & widmask;
		}
	}

CChecksum::~CChecksum() {
	delete crcTable;
	}

DWORD __fastcall CChecksum::GetChecksum(BYTE const message[], DWORD nBytes) {
  register DWORD remainder = initValue;
  register DWORD data;
	int  byte;

  for(byte=0; byte < nBytes; ++byte) {
    data = message[byte] ^ remainder;
  	remainder = crcTable[data];
		}

  if(reflectOutput)
		return (reflect(remainder,width) ^ xorOutput) & widmask;
	else
		return (remainder ^ xorOutput) & widmask;
	}


#define BITMASK(X) (1L << (X))

DWORD CChecksum::reflect(DWORD v,int b) {
// Returns the value v with the bottom b [0,32] bits reflected.
// Example: reflect(0x3e23L,3) == 0x3e26                       
	int   i;
//	DWORD t = v;

  // e' un po' diversa, ma ok!
	_asm {
		mov eax,dword ptr v
		xor edx,edx
		mov ecx,dword ptr b
myLoop:
		rcr eax,1
    rcl edx,1
		loop myLoop

    mov dword ptr i,edx
    }
	return i;

	}

//-----------------------------------------------------------------------

VOID RasDialFunc(UINT unMsg,	// type of event that has occurred
  RASCONNSTATE rasconnstate,	// connection state about to be entered
  DWORD dwError) {			// error that may have occurred
	char myBuf[128];

	theApp.RASiState=rasconnstate;
	wsprintf(myBuf,"RAScb: ");
	wsprintf(myBuf+7,"stato %d, errore %x",rasconnstate,dwError);
	theApp.FileSpool->print(CLogFile::flagInfo,myBuf);
	}

int CVidsendApp::callRAS(const char *strPhoneNumber,const char *strUserName,const char *strPassword,BOOL sync) {
  RASDIALPARAMS rdParams;
  char szBuf[256];
	DWORD dwRet;
	int q=0,i;
	DWORD ti;

	if(hRasConn)
		return 2;
  rdParams.dwSize = sizeof(RASDIALPARAMS);
#ifdef _CAMPARTY_MODE
	_tcscpy(rdParams.szEntryName,"camparty");				// provo con una CONNESSIONE di ACCESSO REMOTO...
#else
	_tcscpy(rdParams.szEntryName,strPhoneNumber);				// provo con una CONNESSIONE di ACCESSO REMOTO...
#endif
	rdParams.szPhoneNumber[0]=0;
	rdParams.szCallbackNumber[0]='\0';
	if(strUserName)
		_tcscpy(rdParams.szUserName, strUserName);
	else {
		RASDIALPARAMS rdp;
		rdp.dwSize=sizeof(RASDIALPARAMS);
		_tcscpy(rdp.szEntryName,rdParams.szEntryName);
		if(!RasGetEntryDialParams(NULL,&rdp,&i))
			_tcscpy(rdParams.szUserName, rdp.szUserName);
		else
			*rdParams.szUserName=0;
		}
	if(strPassword)
		_tcscpy(rdParams.szPassword, strPassword);
	else {
		RASDIALPARAMS rdp;
		rdp.dwSize=sizeof(RASDIALPARAMS);
		_tcscpy(rdp.szEntryName,rdParams.szEntryName);
		if(!RasGetEntryDialParams(NULL,&rdp,&i))
			_tcscpy(rdParams.szPassword, rdp.szPassword);
		else
			*rdParams.szPassword=0;
		}	
	rdParams.szDomain[0] = '\0';
	RASiState=0;	// 0 e' giusto??
	dwRet=RasDial(NULL,NULL,&rdParams,0L,RasDialFunc,&hRasConn);
handleConn:
	if(!sync) {			// asincrona... esco subito dopo aver provato max 2 volte...
		if(!dwRet)
			return 1;
		if(q)				// al primo giro provo con il II modo...
			goto errore;
		}
	else {					// sincrona
		if(!dwRet) {
			ti=timeGetTime()+60000;		// max 1 minuto
			while(timeGetTime() < ti) {
//				if(RASiState >= RASCS_DONE)
//					break;
				if(RASiState == RASCS_Connected /*RASCS_Authenticated*/ || RASiState == RASCS_Disconnected )
					break;
				}
			if(RASiState == RASCS_Connected /*RASCS_Authenticated*/)
				return 1;
			else {
				RASCONNSTATUS rcs;
				RasGetConnectStatus(hRasConn,&rcs);
				dwRet=rcs.dwError;
				}
			}
		if(q)				// al primo giro provo con il II modo...
			goto errore;
		}

  q++;
	if(hRasConn)
		RasHangUp(hRasConn);
	hRasConn=NULL;
	rdParams.dwSize = sizeof(RASDIALPARAMS);
	rdParams.szEntryName[0] = '\0';				// altrimenti provo con il numero!
	_tcscpy(rdParams.szPhoneNumber, strPhoneNumber);
	rdParams.szCallbackNumber[0] = '\0';
	if(strUserName)
		_tcscpy(rdParams.szUserName, strUserName);
	else
		*rdParams.szUserName=0;
	if(strPassword)
		_tcscpy(rdParams.szPassword, strPassword);
	else
		*rdParams.szPassword=0;
	rdParams.szDomain[0] = '\0';
	dwRet=RasDial(NULL,NULL,&rdParams,0L,RasDialFunc,&hRasConn);
	goto handleConn;

errore:
	wsprintf(szBuf,"RAS: ");
	if(RasGetErrorString((UINT)dwRet,(LPSTR)szBuf+5,250) != 0)
		wsprintf( (LPSTR)szBuf+5,"Undefined Dial Error (%ld).",dwRet);
	if(hRasConn)
		RasHangUp(hRasConn);
	hRasConn=NULL;
	theApp.FileSpool->print(CLogFile::flagInfo,szBuf);
	return 0;
	}

BOOL CVidsendApp::hangUpRAS() {
//	RASCONN ras[20];
//	DWORD dSize,dNumber;
	char szBuf[256];
	BOOL bOK = TRUE;
	
/*	ras[0].dwSize = sizeof(RASCONN);
	dSize = sizeof(ras);   // Get active RAS - Connection
	DWORD dwRet = RasEnumConnections(ras,&dSize,&dNumber);
	if(dwRet != 0)	{
		if(RasGetErrorString( (UINT)dwRet, (LPSTR)szBuf, 256 ) != 0 )
			wsprintf((LPSTR)szBuf,"Undefined RAS Enum Connections error (%ld).", dwRet );
		AfxMessageBox((LPSTR)szBuf,MB_OK | MB_ICONSTOP );
		return FALSE;
		}
	for(DWORD dCount = 0; dCount < dNumber;  dCount++ )	{    // Hang up that connection
		HRASCONN hRasConn = ras[dCount].hrasconn;*/

	theApp.FileSpool->print(CLogFile::flagInfo,"RASHangUp: %x",hRasConn);
	if(hRasConn) {
		DWORD dwRet = RasHangUp(hRasConn);
		if(dwRet) {
			if(RasGetErrorString( (UINT)dwRet, (LPSTR)szBuf, 256 ) != 0)
				wsprintf((LPSTR)szBuf,"Undefined RAS HangUp Error (%ld).",dwRet);
			theApp.FileSpool->print(CLogFile::flagInfo,szBuf);
			bOK = FALSE;
			}
		Sleep(3000);
	/*do { i=RasGetConnectStatus(hrasconn);		// si consiglia di aspettare un po'... dopo RasHangUp...
		Sleep(0);
		}	loop until(i==ERROR_INVALID_HANDLE); */
	  hRasConn=NULL;
		}
//		}
	return bOK;
	}


//http://ip-api.com/docs/api:csv
// per IP location http://ip-api.com/csv/2.234.115.43
char *CVidsendApp::subGetIPLocation(const char *IP, char *response) {
	CWebCliSocket wc("Joshua 2.6");
	char myBuf[1000],*p;
	int i=0;


//int getPage(const char *url,char *buf,DWORD len,BOOL doRedirect);

	CString srv=_T("ip-api.com"),s,s2;		// ip-api
	if(wc.Connect(srv)) {
		s=_T("/csv/");
		s+=IP;
		s2=wc.buildQuery(srv,s);

		s2=wc.m_HTTPHeader->GetBuffer();	// togliere, xche' addHeader va gestita...
		if(theApp.debugMode)
			if(theApp.FileSpool)
				theApp.FileSpool->print(CLogFile::flagInfo,"  GET iplocator: %s",s2);

		i=wc.sendQuery((LPCTSTR)s2);
		if(theApp.debugMode)
			if(theApp.FileSpool)
				theApp.FileSpool->print(CLogFile::flagInfo,"  GET/HTTP iplocator send: %d",i);

		wc.readHeader();		// leggo e salto
		i=wc.readPage(myBuf,160,2);		// HTTP/1.1 200 OK<CR><LF> success,Italy,IT,45,Emilia-Romagna,Bologna,40139,44.4938,11.3387,Europe/Rome,Fastweb,Fastweb,"AS12874 Fastweb",2.234.115.43
		myBuf[i]=0;
		if(theApp.debugMode)
			if(theApp.FileSpool)
				theApp.FileSpool->print(CLogFile::flagInfo,"  GET ip locator receive: %s",myBuf);

		wc.Disconnect();

//		p=strstr(myBuf,"OK");
//		if(p) {
//			p+=4;
			p=myBuf;
			if(response)
			_tcscpy(response,p);
//			}
//		else {
				if(theApp.debugMode) 
					if(theApp.FileSpool)
						theApp.FileSpool->print(CLogFile::flagError,"  GET IP locator failed: %s",myBuf);
//			if(response)
//			*response=0;
//			}
		
		}
	else {
		if(response)
			*response=0;

		}

	return response;
	}



#ifdef _NEWMEET_MODE

void CVidsendApp::OnFileImpostazionisorgentevideo() {
	int i;

	if(theServer->theTV) {
		if(i=theServer->theTV->inCapture)
			theServer->theTV->Capture(FALSE); 
		capDlgVideoSource(theServer->theTV->GetHwnd());
		theServer->theTV->Capture(i); 
		}
	}

void CVidsendApp::OnUpdateFileImpostazionisorgentevideo(CCmdUI* pCmdUI) {
	
	pCmdUI->Enable(theServer != NULL && theServer->theTV != NULL);
	}

void CVidsendApp::OnFileImpostazioniformatovideo() {
	int i;

	if(theServer->theTV) {
		AfxMessageBox("Tenere presente che, in molti casi, è necessario impostare la webcam agli stessi valori indicati in Impostazioni Streaming",MB_ICONEXCLAMATION);
		if(i=theServer->theTV->inCapture)
			theServer->theTV->Capture(FALSE); 
		capDlgVideoFormat(theServer->theTV->GetHwnd());
		theServer->theTV->Capture(i); 
		}
	}

void CVidsendApp::OnUpdateFileImpostazioniformatovideo(CCmdUI* pCmdUI) {

	pCmdUI->Enable(theServer != NULL && theServer->theTV != NULL);
	}


void CVidsendApp::OnFileImpostazionistreaming() {
	
	theServer->OnFileProprieta();
	}

void CVidsendApp::OnUpdateFileImpostazionistreaming(CCmdUI* pCmdUI) {
	
	pCmdUI->Enable(theApp.theServer != NULL);
	}

void CVidsendApp::OnFileSaveVideo() {

	theServer->OnFileSaveVideo();
	}

void CVidsendApp::OnUpdateFileSaveVideo(CCmdUI* pCmdUI) {

	pCmdUI->Enable(theApp.theServer != NULL);
	}

void CVidsendApp::OnFileSaveFotogramma() {

	theServer->OnFileSaveFotogramma();
	}

void CVidsendApp::OnUpdateFileSaveFotogramma(CCmdUI* pCmdUI) {

	pCmdUI->Enable(theApp.theServer != NULL);
	}

void CVidsendApp::OnFileSaveFotogramma2() {

	theServer->OnFileSaveFotogramma2();
	}

void CVidsendApp::OnUpdateFileSaveFotogramma2(CCmdUI* pCmdUI) {

	pCmdUI->Enable(theApp.theServer != NULL);
	}


void CVidsendApp::OnFileArchivioimmagini() {

	theServer->OnFileArchivioimmagini();
	}

void CVidsendApp::OnUpdateFileArchivioimmagini(CCmdUI* pCmdUI) {

	pCmdUI->Enable(theApp.theServer != NULL);
	}

void CVidsendApp::OnOpzioniChat() {

	theChat->OnFileProprieta();
	}

void CVidsendApp::OnUpdateOpzioniChat(CCmdUI* pCmdUI) {
	
	pCmdUI->Enable(theApp.theChat != NULL);
	}

void CVidsendApp::OnOpzioniVarie() {
	// TODO: Add your command handler code here
	
	}

void CVidsendApp::OnUpdateOpzioniVarie(CCmdUI* pCmdUI) {
	// TODO: Add your command update UI handler code here
	
	}

void CVidsendApp::OnOpzioniVideo() {
	CVidsendDoc2PropPage2_NM myDlg(theServer);

	if(myDlg.DoModal()==IDOK) {
		theServer->maxConn=myDlg.m_MaxConn;
		if(myDlg.m_ActivateIf)
			theServer->Opzioni |= CVidsendDoc2::openVideoOnConnect;
		else
			theServer->Opzioni &= ~CVidsendDoc2::openVideoOnConnect;
		if(myDlg.m_ActivateWaitConfirm)
			theServer->Opzioni |= CVidsendDoc2::askOnConnect;
		else
			theServer->Opzioni &= ~CVidsendDoc2::askOnConnect;
		if(myDlg.m_DialUp)
			theServer->Opzioni |= CVidsendDoc2::doDialUp;
		else
			theServer->Opzioni &= ~CVidsendDoc2::doDialUp;
		theServer->suonoIn=myDlg.m_SuonoIn;
		theServer->suonoOut=myDlg.m_SuonoOut;
		theApp.DialUpNome=myDlg.m_DialUpNome;
		}

	
	}

void CVidsendApp::OnUpdateOpzioniVideo(CCmdUI* pCmdUI) {

	pCmdUI->Enable(theApp.theServer != NULL);
	}

void CVidsendApp::OnVideoTrasmissioneDalvivo() {

	}

void CVidsendApp::OnUpdateVideoTrasmissioneDalvivo(CCmdUI* pCmdUI) {

	pCmdUI->SetCheck(theServer->trasmMode == 0);
	}

void CVidsendApp::OnVideoTrasmissioneFilmato() {
	CApriVideoDlg myDlg(theServer);

	if(myDlg.DoModal() == IDOK) {
		theServer->nomeAVI_PB=myDlg.m_NomeFile;
		theServer->OpzioniSorgenteVideo &= 0xff00ffff;
		theServer->OpzioniSorgenteVideo |= myDlg.m_Loop ? CVidsendDoc2::aviLoop : 0;
		theServer->OpzioniSorgenteVideo |= myDlg.m_TipoVideo ? CVidsendDoc2::aviMode : 0;

		theServer->setTXMode(1);
		}
	}

void CVidsendApp::OnUpdateVideoTrasmissioneFilmato(CCmdUI* pCmdUI) {

	pCmdUI->SetCheck(theServer->trasmMode == 1);
	}

void CVidsendApp::OnInfo() {

	if(theServer) {
		theServer->OnVideoInformazioni();
		}
	}

void CVidsendApp::OnHelpDesk() {
	CString S;
	int i;
	CVidsendView2 *w;
	RECT rc;

	if(theServer) {
		w=(CVidsendView2 *)theServer->getView();

		S.Format("http://newmeet.com/Espos/Helpdesk.asp?ID=%u",theServer->myID);
	//	i=(int)ShellExecute(theApp.m_pMainWnd->m_hWnd,NULL,S,"","C:\\",SW_SHOW);
		if(w->myBrowserDlg)
			delete w->myBrowserDlg;
		w->myBrowserDlg=new CBrowserDlg(S);
		rc.top=70;
		rc.left=400;
		rc.right=770;
		rc.bottom=530;
		w->myBrowserDlg->Create("Help",&rc,WS_MINIMIZEBOX | WS_SYSMENU);
		}

	}

void CVidsendApp::OnDisconnetti() {
	CMenu *pMenu;

	if(theServer) {
		if(pMenu=m_pMainWnd->GetMenu()) {
			if(theServer->authSocket) {
				((CVidsendView2 *)theServer->getView())->endConnect();
				pMenu->ModifyMenu(ID_DISCONNETTI,MF_BYCOMMAND | MF_STRING,ID_DISCONNETTI,"&Connetti");
				}
			else {
				if(((CVidsendView2 *)theServer->getView())->doConnect())
					pMenu->ModifyMenu(ID_DISCONNETTI,MF_BYCOMMAND | MF_STRING,ID_DISCONNETTI,"&Disconnetti");
				else
					pMenu->ModifyMenu(ID_DISCONNETTI,MF_BYCOMMAND | MF_STRING,ID_DISCONNETTI,"&Connetti");
				}
			CVidsendView2 *v=(CVidsendView2 *)theApp.theServer->getView();
			((CChildFrame2 *)v->GetParent())->SendMessage(WM_NCPAINT);
			m_pMainWnd->DrawMenuBar();
			}
		}
	}


#endif


