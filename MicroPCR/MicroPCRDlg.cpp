
// MicroPCRDlg.cpp : ���� ����
//

#include "stdafx.h"
#include "MicroPCR.h"
#include "MicroPCRDlg.h"
#include "ConvertTool.h"
#include "FileManager.h"
#include "PIDManagerDlg.h"

#include <locale.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// CMicroPCRDlg ��ȭ ����

CMicroPCRDlg::CMicroPCRDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CMicroPCRDlg::IDD, pParent)
	, device(NULL)
	, m_Timer(NULL)
	, magneto(NULL)
	, openConstants(true)	// true �� �� ��, �Լ��� ȣ���Ͽ� �ݵ��� �Ѵ�. 
	, openGraphView(false)
	, isConnected(false)
	, m_cMaxActions(1000)
	, m_cTimeOut(1000)
	, m_cArrivalDelta(0.5)
	, m_cTemperature(0)
	, m_sProtocolName(L"")
	, m_totalActionNumber(0)
	, m_currentActionNumber(-1)
	, actions(NULL)
	, m_nLeftSec(0)
	, m_blinkCounter(0)
	, m_timerCounter(0)
	, recordFlag(false)
	, blinkFlag(false)
	, isRecording(false)
	, isStarted(false)
	, isCompletePCR(false)
	, isTargetArrival(false)
	, isFirstDraw(false)
	, m_startTime(0)
	, m_nLeftTotalSec(0)
	, m_prevTargetTemp(25)
	, m_currentTargetTemp(25)
	, m_timeOut(0)
	, m_leftGotoCount(-1)
	, m_recordingCount(0)
	, m_recStartTime(0)
	, m_cGraphYMin(0)
	, m_cGraphYMax(4096)
	, ledControl_wg(1)
	, ledControl_r(1)
	, ledControl_g(1)
	, ledControl_b(1)
	, currentCmd(CMD_READY)
	, m_kp(0.0)
	, m_ki(0.0)
	, m_kd(0.0)
	, isFanOn(false)
	, isLedOn(false)
	, m_cIntegralMax(INTGRALMAX)
	, loadedPID(L"")
	, m_cycleCount2(0)
	, isTempGraphOn(false)
	, targetTempFlag(false)
	, freeRunning(false)
	, freeRunningCounter(0)
	, previousAction(0)
	, testStartTime(0.0)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

CMicroPCRDlg::~CMicroPCRDlg()
{
	// ������ ��ü�� �Ҹ��ڿ��� �������ش�.
	if( device != NULL )
		delete device;
	if( actions != NULL )
		delete []actions;
	if( m_Timer != NULL )
		delete m_Timer;
	if( magneto != NULL )
		delete magneto;
}

void CMicroPCRDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_PROGRESS_CHAMBER_TEMP, m_cProgressBar);
	DDX_Control(pDX, IDC_CUSTOM_PID_TABLE, m_cPidTable);
	DDX_Text(pDX, IDC_EDIT_MAX_ACTIONS, m_cMaxActions);
	DDV_MinMaxInt(pDX, m_cMaxActions, 1, 10000);
	DDX_Text(pDX, IDC_EDIT_TIME_OUT, m_cTimeOut);
	DDV_MinMaxInt(pDX, m_cTimeOut, 1, 10000);
	DDX_Text(pDX, IDC_EDIT_ARRIVAL_DELTA, m_cArrivalDelta);
	DDV_MinMaxFloat(pDX, m_cArrivalDelta, 0, 10.0);
	DDX_Text(pDX, IDC_EDIT_CHAMBER_TEMP, m_cTemperature);
	DDX_Control(pDX, IDC_LIST_PCR_PROTOCOL, m_cProtocolList);
	DDX_Text(pDX, IDC_EDIT_GRAPH_Y_MIN, m_cGraphYMin);
	DDV_MinMaxInt(pDX, m_cGraphYMin, 0, 4096);
	DDX_Text(pDX, IDC_EDIT_Y_MAX, m_cGraphYMax);
	DDV_MinMaxInt(pDX, m_cGraphYMax, 0, 4096);
	DDX_Text(pDX, IDC_EDIT_INTEGRAL_MAX, m_cIntegralMax);
	DDV_MinMaxFloat(pDX, m_cIntegralMax, 0.0, 10000.0);
	DDX_Control(pDX, IDC_TREE_PROTOCOL, actionTreeCtrl);
	DDX_Control(pDX, IDC_COMBO_COMPORT, comPortList);
	DDX_Control(pDX, IDC_PROGRESS_BAR, progressBar);
	DDX_Control(pDX, IDC_STATIC_X_AXIS, imgX);
	DDX_Control(pDX, IDC_STATIC_Y_AXIS, imgY);
	DDX_Control(pDX, IDC_STATIC_MAGNET, imgMagnet);
	DDX_Control(pDX, IDC_STATIC_LOAD, imgLoad);
	DDX_Control(pDX, IDC_STATIC_SYRINGE, imgSyringe);
	DDX_Control(pDX, IDC_STATIC_ROTATE, imgRotate);
	DDX_Control(pDX, IDC_STATIC_FILTER, imgFilter);
}

BEGIN_MESSAGE_MAP(CMicroPCRDlg, CDialog)
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_WM_DEVICECHANGE()
	ON_MESSAGE(WM_SET_SERIAL, SetSerial)
	ON_MESSAGE(WM_MMTIMER, OnmmTimer)

	// For Magneto message 150922 YJ
	ON_MESSAGE(WM_MOTOR_POS_CHANGED, OnMotorPosChanged)
	//}}AFX_MSG_MAP
	ON_BN_CLICKED(IDC_BUTTON_CONSTANTS_APPLY, &CMicroPCRDlg::OnBnClickedButtonConstantsApply)
	ON_BN_CLICKED(IDC_BUTTON_ENTER_PID_MANAGER, &CMicroPCRDlg::OnBnClickedButtonEnterPidManager)
	ON_WM_MOVING()
	ON_WM_SIZE()
	ON_BN_CLICKED(IDC_BUTTON_COM_CONNECT, &CMicroPCRDlg::OnBnClickedButtonComConnect)
	ON_BN_CLICKED(IDC_BUTTON_COM_CONNECT2, &CMicroPCRDlg::OnBnClickedButtonComConnect2)
	ON_BN_CLICKED(IDC_BUTTON_START_OPERATION, &CMicroPCRDlg::OnBnClickedButtonStartOperation)
	ON_WM_TIMER()
END_MESSAGE_MAP()


// CMicroPCRDlg �޽��� ó����

BOOL CMicroPCRDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	// �� ��ȭ ������ �������� �����մϴ�. ���� ���α׷��� �� â�� ��ȭ ���ڰ� �ƴ� ��쿡��
	//  �����ӿ�ũ�� �� �۾��� �ڵ����� �����մϴ�.
	SetIcon(m_hIcon, TRUE);			// ū �������� �����մϴ�.
	SetIcon(m_hIcon, FALSE);		// ���� �������� �����մϴ�.

	// 150922 YJ
	SetDlgItemText(IDC_EDIT_STATUS1, L"Disconnected");
	SetDlgItemText(IDC_EDIT_STATUS2, L"Disconnected");

	SetDlgItemText(IDC_EDIT_X_TARGET, L"0.000");
	SetDlgItemText(IDC_EDIT_X_CURRENT, L"0.000");
	SetDlgItemText(IDC_EDIT_Y_TARGET, L"0.000");
	SetDlgItemText(IDC_EDIT_Y_CURRENT, L"0.000");
	SetDlgItemText(IDC_EDIT_MAGNET_TARGET, L"0.000");
	SetDlgItemText(IDC_EDIT_MAGNET_CURRENT, L"0.000");
	SetDlgItemText(IDC_EDIT_LOAD_TARGET, L"0.000");
	SetDlgItemText(IDC_EDIT_LOAD_CURRENT, L"0.000");
	SetDlgItemText(IDC_EDIT_SYRINGE_TARGET, L"0.000");
	SetDlgItemText(IDC_EDIT_SYRINGE_CURRENT, L"0.000");
	SetDlgItemText(IDC_EDIT_ROTATE_TARGET, L"0.000");
	SetDlgItemText(IDC_EDIT_ROTATE_CURRENT, L"0.000");
	SetDlgItemText(IDC_EDIT_FILTER_TARGET, L"0.000");
	SetDlgItemText(IDC_EDIT_FILTER_CURRENT, L"0.000");

	onImg.LoadBitmapW(IDB_BITMAP_ON);
	offImg.LoadBitmapW(IDB_BITMAP_OFF);

	/* UI Settings */
	bmpProgress.LoadBitmapW(IDB_BITMAP_SCALE);
	bmpRecNotWork.LoadBitmapW(IDB_BITMAP_REC_NOT_WORKING); 
	bmpRecWork.LoadBitmapW(IDB_BITMAP_REC_WORKING);

	m_cProgressBar.SetRange32(-10, 110);
	m_cProgressBar.SendMessage(PBM_SETBARCOLOR,0,RGB(200, 0, 50));

	SetDlgItemText(IDC_EDIT_ELAPSED_TIME, L"0m 0s");

	CFont font;
	CRect rect;
	CString labels[3] = { L"No", L"Temp.", L"Time" };

	font.CreatePointFont(100, L"Arial", NULL);

	m_cProtocolList.SetFont(&font);
	m_cProtocolList.GetClientRect(&rect);

	for(int i=0; i<3; ++i)
		m_cProtocolList.InsertColumn(i, labels[i], LVCFMT_CENTER, (rect.Width()/3));

	actions = new Action[m_cMaxActions];

	OnBnClickedButtonConstants();
	OnBnClickedButtonGraphview();

	// 150725 PID Check
	// ������ �ҷ��� PID ���� �ִ��� Ȯ���Ѵ�.
	
	FileManager::loadPID( L"pid", pids );
	
	initPidTable();
	loadPidTable();

	loadConstants();

	// Chart Settings
	/*
	CAxis *axis;
	axis = m_Chart.AddAxis( kLocationBottom );
	axis->SetTitle(L"PCR Cycles");
	axis->SetRange(0, 40);

	axis = m_Chart.AddAxis( kLocationLeft );
	axis->SetTitle(L"Sensor Value");
	axis->SetRange(m_cGraphYMin, m_cGraphYMax);

	sensorValues.push_back( 1.0 );
	*/
	
	device = new CDeviceConnect( GetSafeHwnd() );
	m_Timer = new CMMTimers(1, GetSafeHwnd());

	// ���� �õ�
	
	BOOL status = device->CheckDevice();

	if( status )
	{
		SetDlgItemText(IDC_EDIT_STATUS2, L"Connected");
		isConnected = true;

		readProtocol(L"PCRProtocol.txt");

	//	m_Timer->startTimer(TIMER_DURATION, FALSE);
	}
	else
	{
		SetDlgItemText(IDC_EDIT_STATUS2, L"Disconnected");
		isConnected = false;
	}

	// enableWindows();

	// 150922 YJ
	// For magneto
	magneto = new CMagneto();
	
	OnBnClickedButtonComConnect2();

	// Load magneto protocol
	CString res = magneto->loadProtocol(L"MagnetoProtocol.txt");

	if (magneto->isCompileSuccess(res)){	// ������ ���, file ������ protocol �̸��� �����Ѵ�.

		// ������ ���, Action Protocol List �� ��� �ֱ� ���� actionList �� �����ϰ�
		// Tree �� ǥ���� String List �� �ҷ��´�.
		vector<ActionBeans> treeList;
		magneto->generateActionList(treeList);

		actionTreeCtrl.DeleteAllItems();
		
		for (int i = 0; i < treeList.size(); ++i){
			HTREEITEM root = actionTreeCtrl.InsertItem(treeList[i].parentAction, TVI_ROOT, TVI_LAST);

			for (int j = 0; j < treeList[i].childAction.size(); ++j)
				actionTreeCtrl.InsertItem(treeList[i].childAction[j], root, TVI_LAST);
			actionTreeCtrl.Expand(root, TVE_EXPAND);
		}

		// �� ���� �����ϵ��� 
		HTREEITEM item = actionTreeCtrl.GetRootItem();
		actionTreeCtrl.SelectItem(item);
	}
	else{
		AfxMessageBox(res);
	}

	progressBar.SetRange(0, 10);

	return TRUE;  // ��Ŀ���� ��Ʈ�ѿ� �������� ������ TRUE�� ��ȯ�մϴ�.
}

// ��ȭ ���ڿ� �ּ�ȭ ���߸� �߰��� ��� �������� �׸�����
//  �Ʒ� �ڵ尡 �ʿ��մϴ�. ����/�� ���� ����ϴ� MFC ���� ���α׷��� ��쿡��
//  �����ӿ�ũ���� �� �۾��� �ڵ����� �����մϴ�.

void CMicroPCRDlg::OnPaint()
{
	CPaintDC dc(this); // �׸��⸦ ���� ����̽� ���ؽ�Ʈ

	if (IsIconic())
	{
		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// Ŭ���̾�Ʈ �簢������ �������� ����� ����ϴ�.
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// �������� �׸��ϴ�.
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		/*
		CRect graphRect;

		int oldMode = dc.SetMapMode( MM_LOMETRIC );

		graphRect.SetRect( 15, 350, 570, 760 );
		
		dc.DPtoLP( (LPPOINT)&graphRect, 2 );

		CDC *dc2 = CDC::FromHandle(dc.m_hDC);
		m_Chart.OnDraw( dc2, graphRect, graphRect );

		dc.SetMapMode( oldMode );
		*/

		CDialog::OnPaint();
	}
}

// ����ڰ� �ּ�ȭ�� â�� ���� ���ȿ� Ŀ���� ǥ�õǵ��� �ý��ۿ���
//  �� �Լ��� ȣ���մϴ�.
HCURSOR CMicroPCRDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

LRESULT CMicroPCRDlg::SetSerial(WPARAM wParam, LPARAM lParam)
{
	char *Serial = (char *)lParam;
	return TRUE;
}

BOOL CMicroPCRDlg::OnDeviceChange(UINT nEventType, DWORD dwData)
{
	// ���� �õ�
	BOOL status = device->CheckDevice();

	if( status )
	{
		// �ߺ� connection ����
		if( isConnected ) 
			return TRUE;

		SetDlgItemText(IDC_EDIT_STATUS2, L"Connected");

		isConnected = true;

		CString recentPath;
		FileManager::loadRecentPath(FileManager::PROTOCOL_PATH, recentPath);

		if( !recentPath.IsEmpty() )
			readProtocol(recentPath);
		else
			AfxMessageBox(L"No Recent Protocol File! Please Read Protocol!");

		m_Timer->startTimer(TIMER_DURATION, FALSE);
	}
	else
	{
		SetDlgItemText(IDC_EDIT_STATUS2, L"Disconnected");
		isConnected = false;

		m_Timer->stopTimer();
	}

	//enableWindows();

	return TRUE;
}

BOOL CMicroPCRDlg::PreTranslateMessage(MSG* pMsg)
{
	if( pMsg -> message == WM_KEYDOWN )
	{
		if( pMsg -> wParam == VK_RETURN )
			return TRUE;

		if( pMsg -> wParam == VK_ESCAPE )
			return TRUE;
	}

	return CDialog::PreTranslateMessage(pMsg);
}

void CMicroPCRDlg::Serialize(CArchive& ar)
{
	// Constants ���� ������ �� �����.
	if (ar.IsStoring())
	{
		ar << m_cMaxActions << m_cTimeOut << m_cArrivalDelta << m_cGraphYMin << m_cGraphYMax << m_cIntegralMax;
	}
	else	// Constants ���� ���Ϸκ��� �ҷ��� �� ����Ѵ�.
	{
		ar >> m_cMaxActions >> m_cTimeOut >> m_cArrivalDelta >> m_cGraphYMin >> m_cGraphYMax >> m_cIntegralMax;

		UpdateData(FALSE);
	}
}

static const int PID_TABLE_COLUMN_WIDTHS[6] = { 88, 120, 120, 75, 75, 75 };

// pid table �� grid control �� �׸��� ���� ����.
void CMicroPCRDlg::initPidTable()
{
	// grid control �� ���� ������ �����Ѵ�.
	m_cPidTable.SetListMode(true);

	m_cPidTable.DeleteAllItems();

	m_cPidTable.SetSingleRowSelection();
	m_cPidTable.SetSingleColSelection();
	m_cPidTable.SetRowCount(1);
	m_cPidTable.SetColumnCount(PID_CONSTANTS_MAX+1);
    m_cPidTable.SetFixedRowCount(1);
    m_cPidTable.SetFixedColumnCount(1);
	m_cPidTable.SetEditable(true);

	// �ʱ� gridControl �� table ������ �������ش�.
	DWORD dwTextStyle = DT_RIGHT|DT_VCENTER|DT_SINGLELINE;

    for (int col = 0; col < m_cPidTable.GetColumnCount(); col++) { 
		GV_ITEM Item;
        Item.mask = GVIF_TEXT|GVIF_FORMAT;
        Item.row = 0;
        Item.col = col;

        if (col > 0) {
                Item.nFormat = DT_LEFT|DT_WORDBREAK;
                Item.strText = PID_TABLE_COLUMNS[col-1];
        }

        m_cPidTable.SetItem(&Item);
		m_cPidTable.SetColumnWidth(col, PID_TABLE_COLUMN_WIDTHS[col]);
    }
}

// ���Ϸκ��� pid ���� �ҷ��ͼ� table �� �׷��ش�.
void CMicroPCRDlg::loadPidTable()
{
	m_cPidTable.SetRowCount(pids.size()+1);

	for(int i=0; i<pids.size(); ++i){
		float *temp[5] = { &(pids[i].startTemp), &(pids[i].targetTemp), 
			&(pids[i].kp), &(pids[i].kd), &(pids[i].ki)};
		for(int j=0; j<PID_CONSTANTS_MAX+1; ++j){
			GV_ITEM item;
			item.mask = GVIF_TEXT|GVIF_FORMAT;
			item.row = i+1;
			item.col = j;
			item.nFormat = DT_LEFT|DT_WORDBREAK;

			// ù��° column �� PID 1 ���� ǥ��
			if( j == 0 )
				item.strText.Format(L"PID #%d", i+1);
			else
				item.strText.Format(L"%.4f", *temp[j-1]);
			
			m_cPidTable.SetItem(&item);
		}
	}
}

void CMicroPCRDlg::loadConstants()
{
	CFile file;

	if( file.Open(CONSTANTS_PATH, CFile::modeRead) ){
		CArchive ar(&file, CArchive::load);
		Serialize(ar);
		ar.Close();
		file.Close();
	}
	else{
		AfxMessageBox(L"Constants ������ ���� ���� �ʱ�ȭ�Ǿ����ϴ�.\n�ٽ� ���� �������ּ���.");
		saveConstants();
		OnBnClickedButtonConstants();
	}
}

void CMicroPCRDlg::saveConstants()
{
	CFile file;

	file.Open(CONSTANTS_PATH, CFile::modeCreate|CFile::modeWrite);
	CArchive ar(&file, CArchive::store);
	Serialize(ar);
	ar.Close();
	file.Close();
}

void CMicroPCRDlg::enableWindows()
{
	GetDlgItem(IDC_BUTTON_PCR_START)->EnableWindow(isConnected&&!loadedPID.IsEmpty());
	GetDlgItem(IDC_BUTTON_PCR_OPEN)->EnableWindow(isConnected);
	GetDlgItem(IDC_BUTTON_PCR_RECORD)->EnableWindow(isConnected);
}

void CMicroPCRDlg::readProtocol(CString path)
{
	CFile file;
	if( path.IsEmpty() || !file.Open(path, CFile::modeRead) )
	{
		AfxMessageBox(L"No Recent Protocol File! Please Read Protocol!");
		return;
	}

	int fileSize = (int)file.GetLength() + 1;
	char *inTemp = new char[fileSize * sizeof(char)];
	file.Read(inTemp, fileSize);

	CString inString = AfxCharToString(inTemp);
	
	m_sProtocolName = getProtocolName(path);

	if( m_sProtocolName.GetLength() > MAX_PROTOCOL_LENGTH )
	{
		AfxMessageBox(L"Protocol Name is Too Long !");
		return;
	}

	if( inTemp ) delete[] inTemp;

	int markPos = inString.Find(L"%PCR%");
	if( markPos < 0 )
	{
		AfxMessageBox(L"This is not PCR File !!");
		return;
	}
	markPos = inString.Find(L"%END");
	if( markPos < 0 )
	{
		AfxMessageBox(L"This is not PCR File !!");
		return;
	}

	int line = 0;
	for(int i=0; i<markPos; i++)
	{
		if( inString.GetAt(i) == '\n' )
			line++;
	}

	m_totalActionNumber = 0;
	for(int i=0; i<line; i++)
	{
		int linePos = inString.FindOneOf(L"\n\r");
		CString oneLine = (CString)inString.Left(linePos);
		inString.Delete(0, linePos + 2);
		if( i > 0 )
		{
			int spPos = oneLine.FindOneOf(L" \t");
			// Label Extraction
			actions[m_totalActionNumber].Label = oneLine.Left(spPos);
			oneLine.Delete(0, spPos);
			oneLine.TrimLeft();
			// Temperature Extraction
			spPos = oneLine.FindOneOf(L" \t");
			CString tmpStr = oneLine.Left(spPos);
			oneLine.Delete(0, spPos);
			oneLine.TrimLeft();

			actions[m_totalActionNumber].Temp = (double)_wtof(tmpStr);

			bool timeflag = false;
			wchar_t tempChar = NULL;

			for(int j=0; j<oneLine.GetLength(); j++)
			{
				tempChar = oneLine.GetAt(j);
				if( tempChar == (wchar_t)'m' )
					timeflag = true;
				else if( tempChar == (wchar_t)'M' )
					timeflag = true;
				else
					timeflag = false;
			}

			// Duration Extraction
			double time = (double)_wtof(oneLine);

			if(timeflag)
				actions[m_totalActionNumber].Time = time * 60;
			else
				actions[m_totalActionNumber].Time = time;

			timeflag = false;
			
			if( actions[m_totalActionNumber].Label != "GOTO" )
			{
				m_nLeftTotalSec += (int)(actions[m_totalActionNumber].Time);
			}

			int label = 0;
			CString temp;
			if( actions[m_totalActionNumber].Label == "GOTO" )
			{
				while(true && actions[m_totalActionNumber].Temp != 0 && actions[m_totalActionNumber].Temp < 101 )
				{
					temp.Format(L"%.0f", actions[m_totalActionNumber].Temp);
					if( actions[label++].Label == temp) break;
				}

				for(int j=0; j<actions[m_totalActionNumber].Time; j++)
				{
					for(int k=label-1; k<m_totalActionNumber; k++)
						m_nLeftTotalSec += (int)(actions[k].Time);
				}
			}
			m_totalActionNumber++;
		}
	}

	int labelNo = 1;

	for(int i=0; i<m_totalActionNumber; i++)
	{
		if( actions[i].Label != "GOTO" )
		{
			if( _ttoi(actions[i].Label) != labelNo )
			{
				AfxMessageBox(L"Label numbering error");
				return;
			}
			else
				labelNo++;

			if( (actions[i].Temp > 100) || (actions[i].Temp < 0) )
			{
				AfxMessageBox(L"Target Temperature error!!");
				return;
			}

			if( (actions[i].Time > 3600) || (actions[i].Time < 0) )
			{
				AfxMessageBox(L"Target Duration error!!");
				return;
			}
		}
		else
		{
			if( (actions[i].Temp > (double)_wtof(actions[i-1].Label) ) ||
				(actions[i].Temp < 1) )
			{
				AfxMessageBox(L"GOTO Label error !!");
				return;
			}

			if( (actions[i].Time > 100) || (actions[i].Time < 1) )
			{
				AfxMessageBox(L"GOTO repeat count error !!");
				return;
			}
		}
	}

	m_nLeftTotalSecBackup = m_nLeftTotalSec;

	// ����Ʈ�� ǥ�����ش�.
	displayList();
}

void CMicroPCRDlg::displayList()
{
	int j=0;

	m_cProtocolList.DeleteAllItems();

	for(int i=0; i<m_totalActionNumber; i++)
	{
		m_cProtocolList.InsertItem(i, actions[i].Label);		// �� ���� ����Ʈ�� ǥ���Ѵ�.
		CString tempString;

		if( actions[i].Label == "GOTO" )		// GOTO �� ���
		{
			tempString.Format(L"%d", (int)actions[i].Temp);
			m_cProtocolList.SetItemText(i, 1, tempString);

			tempString.Format(L"%d",(int)actions[i].Time);
			m_cProtocolList.SetItemText(i, 2, tempString);
		}
		else					// ���� �� ���
		{
			tempString.Format(L"%d", (int)actions[i].Temp);
			m_cProtocolList.SetItemText(i, 1, tempString);			// �µ��� ����Ʈ�� ǥ���Ѵ�.

			// �ð� �ޱ�(�д���)
			int durs = (int)actions[i].Time;
			// �Ҽ��� ���� �ʷ� ȯ��
			int durm = durs/60;
			durs = durs%60;

			if( durs == 0 )
			{
				if( durm == 0 ) tempString.Format(L"��");
				else tempString.Format(L"%dm", durm);
			}
			else
			{
				if( durm == 0 ) tempString.Format(L"%ds", durs);
				else tempString.Format(L"%dm %ds", durm, durs);
			}

			m_cProtocolList.SetItemText(i, 2, tempString);		// �ð��� ����Ʈ�� ǥ���Ѵ�.
		}
	}

	// �������� �̸�
	// InvalidateRect(&CRect(10, 5, 10 * m_sProtocolName.GetLength() + MAX_PROTOCOL_LENGTH, 34));		// Protocol Name ����

	// ��ü �ð��� ��, ��, �� ������ ���
	int second = m_nLeftTotalSec % 60;
	int minute = m_nLeftTotalSec / 60;
	int hour   = minute / 60;
	minute = minute - hour * 60;

	// ��ü �ð� ǥ��
	CString leftTime;
	leftTime.Format(L"%02d:%02d:%02d", hour, minute, second);
	SetDlgItemText(IDC_EDIT_LEFT_PROTOCOL_TIME, leftTime);
}

CString CMicroPCRDlg::getProtocolName(CString path)
{
	int pos = 0;
	CString protocol = path;

	for(int i=0; i<protocol.GetLength(); i++)
	{
		if( protocol.GetAt(i) == '\\' )
			pos = i;
	}

	return protocol.Mid(pos + 1, protocol.GetLength());
}

void CMicroPCRDlg::OnBnClickedButtonConstants()
{
	int w = GetSystemMetrics(SM_CXSCREEN);
	int h = GetSystemMetrics(SM_CYSCREEN);

	if( openConstants )
	{
		SetWindowPos(NULL, w/3, h/3, 630, 375, SWP_NOZORDER);
	}
	else
	{
		if( openGraphView )
			OnBnClickedButtonGraphview();
		
		// openConstants
		if( isTempGraphOn )
			SetWindowPos(NULL, 100, h/3, 1175, 375, SWP_NOZORDER);
		else
			SetWindowPos(NULL, w/5, h/3, 1175, 375, SWP_NOZORDER);
	}

	m_cPidTable.SetEditable(!openConstants);
	openConstants = !openConstants;
}

void CMicroPCRDlg::OnBnClickedButtonGraphview()
{
	int w = GetSystemMetrics(SM_CXSCREEN);
	int h = GetSystemMetrics(SM_CYSCREEN);

	if( openGraphView )
	{
		SetWindowPos(NULL, w/3, h/3, 630, 375, SWP_NOZORDER);
	}
	else
	{
		if( openConstants )
			OnBnClickedButtonConstants();
		SetWindowPos(NULL, w/3, h/7, 630, 760, SWP_NOZORDER);
	}

	openGraphView = !openGraphView;
}

void CMicroPCRDlg::OnBnClickedButtonConstantsApply()
{
	if( !UpdateData() )
		return;

	if( !pids.empty() ){
		pids.clear();
	
		for (int row = 1; row < m_cPidTable.GetRowCount(); row++) {
			CString startTemp = m_cPidTable.GetItemText(row, 1);
			CString targetTemp = m_cPidTable.GetItemText(row, 2);
			CString kp = m_cPidTable.GetItemText(row, 3);
			CString kd = m_cPidTable.GetItemText(row, 4);
			CString ki = m_cPidTable.GetItemText(row, 5);
	
			pids.push_back( PID( _wtof(startTemp), _wtof(targetTemp), _wtof(kp), _wtof(ki), _wtof(kd) ) );
		}

		// ����� ���� ���� �������ش�.
		FileManager::savePID( loadedPID, pids );
	}

	// PID ���� ������ ������ Constants ���� �����Ѵ�.
	saveConstants();

	CAxis *axis = m_Chart.GetAxisByLocation( kLocationLeft );
	axis->SetRange(m_cGraphYMin, m_cGraphYMax);

	// ���� ���� ���, ���� ����� ������ pid �� �����Ѵ�.
	if( isStarted )
		findPID();
}

void CMicroPCRDlg::OnBnClickedButtonPcrStart()
{
	if( m_totalActionNumber < 1 )
	{
		AfxMessageBox(L"There is no action list. Please load task file!!");
		return;
	}

	if( !isStarted )
	{
		initValues();

		isStarted = true;

		currentCmd = CMD_PCR_RUN;

		m_prevTargetTemp = 25;
		m_currentTargetTemp = (BYTE)actions[0].Temp;
		findPID();

		m_startTime = timeGetTime();

		isFirstDraw = false;
		clearSensorValue();
	}
	else
	{
		PCREndTask();
	}
}

void CMicroPCRDlg::OnBnClickedButtonPcrOpen()
{
	if( !isConnected )
		return;

	m_nLeftTotalSec = 0;

	CFileDialog fdlg(TRUE, NULL, NULL, NULL, L"*.txt |*.txt|");
	if( fdlg.DoModal() == IDOK )
	{
		FileManager::saveRecentPath(FileManager::PROTOCOL_PATH, fdlg.GetPathName());

		m_sProtocolName = getProtocolName(fdlg.GetPathName());

		readProtocol(fdlg.GetPathName());
	}
}

void CMicroPCRDlg::OnBnClickedButtonPcrRecord()
{
	if( !isRecording )
	{
		CreateDirectory(L"./Record/", NULL);

		CString fileName, fileName2, fileName3;
		CTime time = CTime::GetCurrentTime();
		fileName = time.Format(L"./Record/%Y%m%d-%H%M-%S.txt");
		fileName2 = time.Format(L"./Record/pd%Y%m%d-%H%M-%S.txt");
		fileName3 = time.Format(L"./Record/pdRaw%Y%m%d-%H%M-%S.txt");
		
		m_recFile.Open(fileName, CStdioFile::modeCreate|CStdioFile::modeWrite);
		m_recFile.WriteString(L"Number	Time	Temperature\n");

		m_recPDFile.Open(fileName2, CStdioFile::modeCreate|CStdioFile::modeWrite);
		m_recPDFile.WriteString(L"Cycle	Time	Value\n");

		m_recPDFile2.Open(fileName3, CStdioFile::modeCreate|CStdioFile::modeWrite);
		m_recPDFile2.WriteString(L"Cycle	Time	Value\n");

		m_recordingCount = 0;
		m_cycleCount = 0;
		m_cycleCount2 = 0;
		m_recStartTime = timeGetTime();
	}
	else
	{
		m_recFile.Close();
		m_recPDFile.Close();
		m_recPDFile2.Close();
	}

	isRecording = !isRecording;
}

void CMicroPCRDlg::OnBnClickedButtonFanControl()
{
	isFanOn = !isFanOn;

	if( isFanOn )
	{
		currentCmd = CMD_FAN_ON;
		SetDlgItemText(IDC_BUTTON_FAN_CONTROL, L"FAN OFF");
		GetDlgItem(IDC_BUTTON_PCR_START)->EnableWindow(FALSE);
	}
	else
	{
		currentCmd = CMD_FAN_OFF;
		SetDlgItemText(IDC_BUTTON_FAN_CONTROL, L"FAN ON");
		GetDlgItem(IDC_BUTTON_PCR_START)->EnableWindow(TRUE);
	}
}

// PID �� ������ �� �ִ� Manager Dialog �� �����Ѵ�.
void CMicroPCRDlg::OnBnClickedButtonEnterPidManager()
{
	static CPIDManagerDlg dlg;

	if( dlg.DoModal() == IDOK )
	{
		loadedPID = dlg.selectedPID;
		SetDlgItemText(IDC_EDIT_LOADED_PID, loadedPID);
		FileManager::saveRecentPath(FileManager::PID_PATH, loadedPID);
		if( !FileManager::loadPID(loadedPID, pids) ){
			AfxMessageBox(L"PID �� �����ϴµ� ������ �߻��Ͽ����ϴ�.\n�����ڿ��� �����ϼ���.");
		}
	}

	if( loadedPID.IsEmpty() )
		AfxMessageBox(L"PID �� ���õ��� ������ PCR �� ������ �� �����ϴ�.");

	if( !dlg.isHasPidList() ){
		loadedPID.Empty();
		SetDlgItemText(IDC_EDIT_LOADED_PID, loadedPID);
		pids.clear();
	}

	// ���������� ��������� Start ��ư ��Ȱ��ȭ
	GetDlgItem(IDC_BUTTON_PCR_START)->EnableWindow(!loadedPID.IsEmpty());

	initPidTable();
	loadPidTable();
}

void CMicroPCRDlg::OnBnClickedButtonLedControl()
{
	isLedOn = !isLedOn;

	if( isLedOn )
	{
		ledControl_b = 0;
		SetDlgItemText(IDC_BUTTON_LED_CONTROL, L"LED OFF");
	}
	else
	{
		ledControl_b = 1;
		SetDlgItemText(IDC_BUTTON_LED_CONTROL, L"LED ON");
	}
}



void CMicroPCRDlg::blinkTask()
{
	m_blinkCounter++;
	if( m_blinkCounter > (500/TIMER_DURATION) )
	{
		m_blinkCounter = 0;

		if( isStarted )
		{
			blinkFlag = !blinkFlag;
			if( blinkFlag )
				m_cProtocolList.SetTextBkColor(RGB(240,200,250));
			else
				m_cProtocolList.SetTextBkColor(RGB(255,255,255));

			m_cProtocolList.RedrawItems(m_currentActionNumber, m_currentActionNumber);
		}
		else
		{
			m_cProtocolList.SetTextBkColor(RGB(255,255,255));
			m_cProtocolList.RedrawItems(0, m_totalActionNumber);
		}
	}
}

CString dslr_title;
BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam)
{
	char title[128];
	GetWindowTextA(hwnd,title,sizeof(title));

	if( strstr(title, "DSLR Remote Pro for Windows - Connected") != NULL )
		dslr_title = title;

	return TRUE;
}

void CMicroPCRDlg::findPID()
{
	if ( fabs(m_prevTargetTemp - m_currentTargetTemp) < .5 ) 
		return; // if target temp is not change then do nothing 1->.5 correct

	// enable the constant changing.
	GetDlgItem(IDC_BUTTON_CONSTANTS_APPLY)->EnableWindow(FALSE);
	
	double dist	 = 10000;
	int paramIdx = 0;
	
	for ( int i = 0; i < pids.size(); i++ )
	{
		double tmp = fabs(m_prevTargetTemp - pids[i].startTemp) + fabs(m_currentTargetTemp - pids[i].targetTemp);

		if ( tmp < dist )
		{
			dist = tmp;
			paramIdx = i;
		}
	}

	m_kp = pids[paramIdx].kp;
	m_ki = pids[paramIdx].ki;
	m_kd = pids[paramIdx].kd;

	GetDlgItem(IDC_BUTTON_CONSTANTS_APPLY)->EnableWindow(TRUE);
}

void CMicroPCRDlg::timeTask()
{
	// elapse time 
	int elapsed_time = (int)((double)(timeGetTime()-m_startTime)/1000.);
	int min = elapsed_time/60;
	int sec = elapsed_time%60;
	CString temp;
	temp.Format(L"%dm %ds", min, sec);
	SetDlgItemText(IDC_EDIT_ELAPSED_TIME, temp);

	// timer counter increased
	m_timerCounter++;

	// 1s ���� ����ǵ��� ����
	if( m_timerCounter == (1000/TIMER_DURATION) )
	{
		m_timerCounter = 0;

		if( m_nLeftSec == 0 )
		{
			m_currentActionNumber++;

			// clear previous blink
			m_cProtocolList.SetTextBkColor(RGB(255,255,255));
			m_cProtocolList.RedrawItems(0, m_totalActionNumber);

			if( (m_currentActionNumber) >= m_totalActionNumber )
			{
				isCompletePCR = true;
				PCREndTask();
				return;
			}

			if( actions[m_currentActionNumber].Label.Compare(L"GOTO") != 0 )
			{
				m_prevTargetTemp = m_currentTargetTemp;
				m_currentTargetTemp = (int)actions[m_currentActionNumber].Temp;

				// 150828 YJ added for target temperature comparison by case
				targetTempFlag = m_prevTargetTemp > m_currentTargetTemp;

				isTargetArrival = false;
				m_nLeftSec = (int)(actions[m_currentActionNumber].Time);
				m_timeOut = m_cTimeOut*10;

				int min = m_nLeftSec/60;
				int sec = m_nLeftSec%60;

				// current left protocol time
				CString leftTime;
				if( min == 0 )
					leftTime.Format(L"%ds", sec);
				else
					leftTime.Format(L"%dm %ds", min, sec);
				m_cProtocolList.SetItemText(m_currentActionNumber, 2, leftTime);

				// find the proper pid values.
				findPID();
			}
			else	// is GOTO
			{
				if( m_leftGotoCount < 0 )
					m_leftGotoCount = (int)actions[m_currentActionNumber].Time;

				if( m_leftGotoCount == 0 )
					m_leftGotoCount = -1;
				else
				{
					m_leftGotoCount--;
					CString leftGoto;
					leftGoto.Format(L"%d", m_leftGotoCount);

					m_cProtocolList.SetItemText(m_currentActionNumber, 2, leftGoto);
					
					// GOTO ���� target label ���� �־���
					CString tmpStr;
					tmpStr.Format(L"%d",(int) actions[m_currentActionNumber].Temp);

					int pos = 0;
					for (pos = 0; pos < m_totalActionNumber; ++pos)
						if( tmpStr.Compare(actions[pos].Label) == 0 )
							break;

					m_currentActionNumber = pos-1;
				}
			}
		}
		else	// action �� �������� ���
		{
			if( !isTargetArrival )
			{
				m_timeOut--;

				if( m_timeOut == 0 )
				{
					AfxMessageBox(L"The target temperature cannot be reached!!");
					PCREndTask();
				}
			}
			else
			{
				m_nLeftSec--;
				m_nLeftTotalSec--;

				int min = m_nLeftSec/60;
				int sec = m_nLeftSec%60;

				// current left protocol time
				CString leftTime;
				if( min == 0 )
					leftTime.Format(L"%ds", sec);
				else
					leftTime.Format(L"%dm %ds", min, sec);
				m_cProtocolList.SetItemText(m_currentActionNumber, 2, leftTime);
				
				// total left protocol time
				min = m_nLeftTotalSec/60;
				sec = m_nLeftTotalSec%60;

				if( min == 0 )
					leftTime.Format(L"%ds", sec);
				else
					leftTime.Format(L"%dm %ds", min, sec);
				SetDlgItemText(IDC_EDIT_LEFT_PROTOCOL_TIME, leftTime);
			}
		}

		// 150108 YJ for camera shoot
		if( m_currentActionNumber != 0 && 
			( m_nLeftSec == 10 &&( m_currentActionNumber == 3 || m_currentActionNumber == 6)) ||
			  m_nLeftSec == 1 &&(m_currentActionNumber == 3))
		{
			dslr_title = "";
			EnumWindows(EnumWindowsProc, NULL);

			if( dslr_title != "" )
			{
				HWND checker = ::FindWindow(NULL, dslr_title);

				if( checker )
					::PostMessage(checker, WM_COMMAND, MAKELONG(1003, BN_CLICKED), (LPARAM)GetSafeHwnd());
			}
		}

		if( m_nLeftSec == 11 && 
			( ((int)(actions[m_currentActionNumber].Temp) == 72) || ((int)(actions[m_currentActionNumber].Temp) == 50) ))
		{
			ledControl_b = 0;
		}
		else if( m_nLeftSec == 0 && 
			( ((int)(actions[m_currentActionNumber].Temp) == 72) || ((int)(actions[m_currentActionNumber].Temp) == 50) ))
		{
			ledControl_b = 1;
		}

		/*
		// for graph drawing
		if( ((int)(actions[m_currentActionNumber].Temp) == 72) && 
			m_nLeftSec == 1 )
		{
			double lights = (double)(photodiode_h & 0x0f)*255. + (double)(photodiode_l);
			addSensorValue( lights );
		}
		*/
	}
}

void CMicroPCRDlg::PCREndTask()
{
	int elapsed_time = (int)((double)(timeGetTime()-testStartTime)/1000.);
	int min = elapsed_time/60;
	int sec = elapsed_time%60;
	CString temp;
	temp.Format(L"Test Complete(elapsed time: %dm %ds)", min, sec);

	// isStarted �� initValues �� ��ġ�� ����� �������� �� ��.
	initValues();

	isStarted = false;

	//SetDlgItemText(IDC_BUTTON_PCR_START, L"PCR Start");

	/*
	if( isCompletePCR )
		AfxMessageBox(L"PCR ended!!");
	else
		AfxMessageBox(L"PCR incomplete!!");
	*/

	//GetDlgItem(IDC_BUTTON_PCR_OPEN)->EnableWindow(TRUE);
	//GetDlgItem(IDC_BUTTON_FAN_CONTROL)->EnableWindow(TRUE);

	isCompletePCR = false;

	previousAction++;
	progressBar.SetPos(previousAction);
	SetDlgItemText(IDC_STATIC_PROGRESSBAR, L"100%");

	m_Timer->stopTimer();

	SetDlgItemText(IDC_EDIT_CHAMBER_TEMP, L"0.0");
	m_cProgressBar.SetPos(0);

	GetDlgItem(IDC_BUTTON_START_OPERATION)->EnableWindow();

	AfxMessageBox(temp);

	SetDlgItemText(IDC_EDIT_CHAMBER_TEMP, L"0.0");
	m_cProgressBar.SetPos(0);
}

void CMicroPCRDlg::initValues()
{
	// 150914 YJ changed for sending stop message to device.
	if( isStarted )
		currentCmd = CMD_PCR_STOP;

	m_currentActionNumber = -1;
	m_nLeftSec = 0;
	m_nLeftTotalSec = 0;
	m_timerCounter = 0;
	m_startTime = 0;
	recordFlag = false;

	m_nLeftTotalSec = m_nLeftTotalSecBackup;
	m_leftGotoCount = -1;
	m_recordingCount = 0;
	m_recStartTime = 0;
	blinkFlag = false;
	m_timeOut = 0;
	m_blinkCounter = 0;
	ledControl_wg = 1;
	ledControl_r = 1;
	ledControl_g = 1;
	ledControl_b = 1;

	m_kp = 0;
	m_ki = 0;
	m_kd = 0;

	isTargetArrival = false;

	m_prevTargetTemp = m_currentTargetTemp = 25;
}

LRESULT CMicroPCRDlg::OnmmTimer(WPARAM wParam, LPARAM lParam)
{
	blinkTask();

	if( isStarted )
	{
		int errorPrevent = 0;
		timeTask();
	}

	RxBuffer rx;
	TxBuffer tx;
	float currentTemp = 0.0;

	memset(&rx, 0, sizeof(RxBuffer));
	memset(&tx, 0, sizeof(TxBuffer));

	tx.cmd = currentCmd;
	tx.currentTargetTemp = (BYTE)m_currentTargetTemp;
	tx.ledControl = 1;
	tx.led_wg = ledControl_wg;
	tx.led_r = ledControl_r;
	tx.led_g = ledControl_g;
	tx.led_b = ledControl_b;

	// pid ���� buffer �� �����Ѵ�.
	
	memcpy(&(tx.pid_p1), &(m_kp), sizeof(float));
	memcpy(&(tx.pid_i1), &(m_ki), sizeof(float));
	memcpy(&(tx.pid_d1), &(m_kd), sizeof(float));

	// integral max ���� buffer �� �����Ѵ�.
	memcpy(&(tx.integralMax_1), &(m_cIntegralMax), sizeof(float));

	BYTE senddata[65] = { 0, };
	BYTE readdata[65] = { 0, };
	memcpy(senddata, &tx, sizeof(TxBuffer));

	device->Write(senddata);

	if( device->Read(&rx) == 0 )
		return FALSE;

	memcpy(readdata, &rx, sizeof(RxBuffer));

	// Change the currentCmd to Ready after sending once except READY, RUN.
	if( currentCmd == CMD_FAN_OFF )
		currentCmd = CMD_READY;
	else if( currentCmd == CMD_PCR_STOP )
		currentCmd = CMD_READY;

	// ���κ��� ���� �µ� ���� �޾ƿͼ� ������.
	// convert BYTE pointer to float type for reading temperature value.
	memcpy(&currentTemp, &(rx.chamber_temp_1), sizeof(float));

	// ���κ��� ���� Photodiode ���� �޾ƿͼ� ������.
	photodiode_h = rx.photodiode_h;
	photodiode_l = rx.photodiode_l;

	if( currentTemp < 0.0 )
		return FALSE;

	// 150828 YJ added
	// 150904 YJ changed, for waiting device flag
	// 150918 YJ deleted, bug by usb communication
	/*
	if( targetTempFlag && rx.targetArrival )
		targetTempFlag = false;
	*/

	// 150918 YJ added, For falling stage routine
	if( targetTempFlag && !freeRunning ){
		// target temp ���ϰ� �Ǵ� �������� freeRunning ���� 
		if( currentTemp <= m_currentTargetTemp ){
			freeRunning = true;
			freeRunningCounter = 0;
		}
	}

	if( freeRunning ){
		freeRunningCounter++;
		// ���� �ٸ��� 3�� �ĺ��� arrival �� �ν��ϵ��� ����
		if( freeRunningCounter >= (3000/TIMER_DURATION) ){
			targetTempFlag = false;
			freeRunning = false;
			freeRunningCounter = 0;
			isTargetArrival = true;
		}
	}

	if( fabs(currentTemp-m_currentTargetTemp) < m_cArrivalDelta && !targetTempFlag )
		isTargetArrival = true;

	CString tempStr;
	tempStr.Format(L"%3.1f", currentTemp);
	SetDlgItemText(IDC_EDIT_CHAMBER_TEMP, tempStr);
	m_cProgressBar.SetPos((int)currentTemp);

	// photodiode
	double lights = (double)(photodiode_h & 0x0f)*255. + (double)(photodiode_l);
	tempStr.Format(L"%3.1f", lights);
	SetDlgItemText(IDC_EDIT_PHOTODIODE, tempStr);

	if( isTempGraphOn && isStarted )
		tempGraphDlg.addData(currentTemp);

	// Check the error from device
	static bool onceShow = true;
	if( rx.currentError == ERROR_ASSERT && onceShow ){
		onceShow = false;
		AfxMessageBox(L"Software error occured!\nPlease contact to developer");
	}

	return FALSE;
}

void CMicroPCRDlg::addSensorValue(double val)
{
	// ������ ����� ��Ʈ�� ���� ��, 
	// ���� ������ double �� vector �� �����Ͽ�
	// �� ���� ������� �ٽ� �׸�.
	sensorValues.push_back(val);
	m_Chart.DeleteAllData();

	int size = sensorValues.size();

	double *data = new double[size*2];
	int	nDims = 2, dims[2] = { 2, size };

	for(int i=0; i<size; ++i)
	{
		data[i] = i;
		data[i+size] = sensorValues[i];
	}

	m_Chart.AddData( data, nDims, dims );

	InvalidateRect(&CRect(15, 350, 1155, 760));
}

void CMicroPCRDlg::clearSensorValue()
{
	sensorValues.clear();
	sensorValues.push_back( 1.0 );

	m_Chart.DeleteAllData();
	InvalidateRect(&CRect(15, 350, 1155, 760));
}

void CMicroPCRDlg::OnBnClickedCheckTempGraph()
{
	CButton* check = (CButton*)GetDlgItem(IDC_CHECK_TEMP_GRAPH);
	isTempGraphOn = check->GetCheck();

	if( isTempGraphOn ){
		if( openConstants )
			SetWindowPos(NULL, 100, GetSystemMetrics(SM_CYSCREEN)/3, 1175, 375, SWP_NOZORDER);

		tempGraphDlg.Create(IDD_DIALOG_TEMP_GRAPH, this);
		
		CRect parent_rect, rect;
		tempGraphDlg.GetClientRect(&rect);
		GetWindowRect(&parent_rect);
		
		tempGraphDlg.SetWindowPos(this, parent_rect.right+10, parent_rect.top, 
			rect.Width(), rect.Height(),  SWP_SHOWWINDOW|SWP_NOSIZE|SWP_NOZORDER|SWP_FRAMECHANGED);
	}
	else
		tempGraphDlg.DestroyWindow();
}

void CMicroPCRDlg::OnMoving(UINT fwSide, LPRECT pRect)
{
	CDialog::OnMoving(fwSide, pRect);

	/*
	if( isTempGraphOn ){
		CRect parent_rect, rect;
		tempGraphDlg.GetClientRect(&rect);
		GetWindowRect(&parent_rect);
		
		tempGraphDlg.SetWindowPos(this, parent_rect.right+10, parent_rect.top, 
			rect.Width(), rect.Height(),  SWP_SHOWWINDOW|SWP_NOSIZE|SWP_NOZORDER|SWP_FRAMECHANGED);
	}
	*/
}

void CMicroPCRDlg::OnSize(UINT nType, int cx, int cy)
{
	CDialog::OnSize(nType, cx, cy);

	/*
	if( isTempGraphOn ){
		CRect parent_rect, rect;
		tempGraphDlg.GetClientRect(&rect);
		GetWindowRect(&parent_rect);
		
		tempGraphDlg.SetWindowPos(this, parent_rect.right+10, parent_rect.top, 
			rect.Width(), rect.Height(),  SWP_SHOWWINDOW|SWP_NOSIZE|SWP_NOZORDER|SWP_FRAMECHANGED);
	}
	*/
}




// For labgenius


void CMicroPCRDlg::OnBnClickedButtonComConnect()
{
	int idx = comPortList.GetCurSel();

	if (!magneto->isConnected()){
		if (idx != -1){
			CString port;
			comPortList.GetLBText(idx, port);
			port.Replace(L"COM", L"");

			
			DriverStatus::Enum res = magneto->connect(_ttoi(port));
			
			if (res == DriverStatus::CONNECTED){
				SetDlgItemText(IDC_EDIT_STATUS1, L"Connected");
				SetDlgItemText(IDC_BUTTON_COM_CONNECT, L"Disconnect");
				GetDlgItem(IDC_BUTTON_COM_CONNECT2)->EnableWindow(FALSE);
				GetDlgItem(IDC_BUTTON_START_OPERATION)->EnableWindow();

				AfxMessageBox(L"Magneto �� ����Ǿ����ϴ�.\nStart Operation �� ���� Test �� �����ϼ���.");
				magneto->setHwnd(GetSafeHwnd());
			}
			else if (res == DriverStatus::NOT_CONNECTED)
				SetDlgItemText(IDC_EDIT_STATUS1, L"Disconnected");
			else{
				SetDlgItemText(IDC_EDIT_STATUS1, L"Too few slaves");
				AfxMessageBox(L"All motors are not connected! Please check your device.");
			}
			
		}
		else
			AfxMessageBox(L"Com port �� �����ϼ���.");
	}
	else{
		magneto->disconnect();
		SetDlgItemText(IDC_EDIT_STATUS1, L"Disconnect");
		SetDlgItemText(IDC_BUTTON_COM_CONNECT, L"Connect");
		GetDlgItem(IDC_BUTTON_COM_CONNECT2)->EnableWindow();
		GetDlgItem(IDC_BUTTON_START_OPERATION)->EnableWindow(FALSE);
	}
	
}

void CMicroPCRDlg::OnBnClickedButtonComConnect2()
{
	vector<CString> list;
	magneto->searchPort(list);

	comPortList.ResetContent();

	for (int i = 0; i < list.size(); ++i)
		comPortList.AddString(list[i]);

	if (comPortList.GetCount() != 0)
		comPortList.SetCurSel(0);
}

void CMicroPCRDlg::OnBnClickedButtonStartOperation()
{
	testStartTime = timeGetTime();

	GetDlgItem(IDC_BUTTON_START_OPERATION)->EnableWindow(FALSE);
	GetDlgItem(IDC_BUTTON_COM_CONNECT2)->EnableWindow(FALSE);
	GetDlgItem(IDC_BUTTON_COM_CONNECT)->EnableWindow(FALSE);

	previousAction = 0;

	magneto->start();
	SetTimer(Magneto::TimerRuntaskID, Magneto::TimerRuntaskDuration, NULL);
}

HRESULT CMicroPCRDlg::OnMotorPosChanged(WPARAM wParam, LPARAM lParam){
	CString target = L"", cmd = L"";

	MotorPos *motorPos = reinterpret_cast<MotorPos *>(wParam);
	int currentAction = LOWORD(lParam);
	int currentSubAction = HIWORD(lParam);
	double cmdPos = motorPos->cmdPos;
	double targetPos = motorPos->targetPos;
	
	target.Format(L"%0.3f", targetPos);
	cmd.Format(L"%0.3f", cmdPos);

	static CStatic* imgIds[7] = {&imgX, &imgY, &imgMagnet, 
		&imgLoad, &imgSyringe, &imgRotate, &imgFilter };

	for(int i=0; i<7; ++i)
		imgIds[i]->SetBitmap(offImg);

	// �Ŀ� �迭�� �̿��� ���� �ʿ�
	if (magneto->getCurrentMotor() == MotorType::X_AXIS){
		SetDlgItemText(IDC_EDIT_X_TARGET, target);
		SetDlgItemText(IDC_EDIT_X_CURRENT, cmd);
		imgX.SetBitmap(onImg);
	}
	else if (magneto->getCurrentMotor() == MotorType::Y_AXIS){
		SetDlgItemText(IDC_EDIT_Y_TARGET, target);
		SetDlgItemText(IDC_EDIT_Y_CURRENT, cmd);
		imgY.SetBitmap(onImg);
	}
	else if (magneto->getCurrentMotor() == MotorType::MAGNET){
		SetDlgItemText(IDC_EDIT_MAGNET_TARGET, target);
		SetDlgItemText(IDC_EDIT_MAGNET_CURRENT, cmd);
		imgMagnet.SetBitmap(onImg);
	}
	else if (magneto->getCurrentMotor() == MotorType::LOAD){
		SetDlgItemText(IDC_EDIT_LOAD_TARGET, target);
		SetDlgItemText(IDC_EDIT_LOAD_CURRENT, cmd);
		imgLoad.SetBitmap(onImg);
	}
	else if (magneto->getCurrentMotor() == MotorType::SYRINGE){
		SetDlgItemText(IDC_EDIT_SYRINGE_TARGET, target);
		SetDlgItemText(IDC_EDIT_SYRINGE_CURRENT, cmd);
		imgSyringe.SetBitmap(onImg);
	}
	else if (magneto->getCurrentMotor() == MotorType::ROTATE){
		SetDlgItemText(IDC_EDIT_ROTATE_TARGET, target);
		SetDlgItemText(IDC_EDIT_ROTATE_CURRENT, cmd);
		imgRotate.SetBitmap(onImg);
	}
	else if (magneto->getCurrentMotor() == MotorType::FILTER){
		SetDlgItemText(IDC_EDIT_FILTER_TARGET, target);
		SetDlgItemText(IDC_EDIT_FILTER_CURRENT, cmd);
		imgFilter.SetBitmap(onImg);
	}

	selectTreeItem(currentAction, currentSubAction);

	if( currentAction == previousAction ){
		previousAction++;
		progressBar.SetPos(previousAction);

		CString percent;
		percent.Format(L"%d%%", (int)((previousAction/10.)*100.));
		SetDlgItemText(IDC_STATIC_PROGRESSBAR, percent);
	}

	return 0;
}

HRESULT CMicroPCRDlg::OnWaitTimeChanged(WPARAM wParam, LPARAM lParam){
	return 0;
}

void CMicroPCRDlg::selectTreeItem(int rootIndex, int childIndex){
	int curRootIndex = 0, curChildIndex = 0;
	HTREEITEM rootItem = actionTreeCtrl.GetRootItem();

	// ���� rootIndex �� ���� ���� root �� �����Ѵ�.
	for (int i = 0; i < rootIndex; ++i)
		rootItem = actionTreeCtrl.GetNextSiblingItem(rootItem);

	// childIndex �� ���� child �� �� �ϳ��� �����Ѵ�.
	HTREEITEM childItem = actionTreeCtrl.GetChildItem(rootItem);
	for (int i = 0; i < childIndex; ++i)
		childItem = actionTreeCtrl.GetNextItem(childItem, TVGN_NEXT);

	actionTreeCtrl.SelectItem(childItem);
	actionTreeCtrl.SetFocus();
}

void CMicroPCRDlg::OnTimer(UINT_PTR nIDEvent)
{
	if (nIDEvent == Magneto::TimerRuntaskID){
		if (!magneto->runTask()){
			KillTimer(Magneto::TimerRuntaskID);
			AfxMessageBox(L"Limit ����ġ�� �����Ǿ� task �� ����Ǿ����ϴ�.\n��⸦ Ȯ���ϼ���.");
			return;
		}

		// ��� Protocol �� �����
		if (magneto->isIdle()){
			KillTimer(Magneto::TimerRuntaskID);
			m_Timer->startTimer(TIMER_DURATION, FALSE);

			static CStatic* imgIds[7] = {&imgX, &imgY, &imgMagnet, 
				&imgLoad, &imgSyringe, &imgRotate, &imgFilter };

			for(int i=0; i<7; ++i)
				imgIds[i]->SetBitmap(offImg);

			OnBnClickedButtonPcrStart();
			return;
		}
	}

	CDialog::OnTimer(nIDEvent);
}
