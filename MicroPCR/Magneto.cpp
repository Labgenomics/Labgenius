
#include "stdafx.h"
#include "Magneto.h"
#include "EziMOTIONPlusR\FAS_EziMOTIONPlusR.h"


CMagneto::CMagneto()
	: connected(false)
	, comPortNo(-1)
	, driverErrCnt(0)
	, currentAction(0)
	, currentSubAction(-1)
	, isStarted(false)
	, isWaitEnd(false)
	, isCompileEnd(false)
	, waitCounter(0)
	, currentTargetPos(0.0)
	, currnetPos(0.0)
{
	initPredefinedAction();
}

CMagneto::~CMagneto(){
	
}

void CMagneto::searchPort(vector<CString> &portList)
{
	for (int i = 0; i<30; i++)
	{
		CString portName;
		portName.Format(L"\\\\.\\COM%d", i + 1);

		HANDLE hComm = CreateFile(portName,
			GENERIC_READ | GENERIC_WRITE,
			0,
			NULL,
			OPEN_EXISTING,
			0,
			NULL);

		COMMPROP pProp;
		GetCommProperties(hComm, &pProp);

		if (hComm != INVALID_HANDLE_VALUE && pProp.dwProvSubType == PST_RS232 )
		{
			portName.Format(L"COM%d", i + 1);
			portList.push_back(portName);
		}

		CloseHandle(hComm);
	}
}

void CMagneto::setHwnd(HWND hwnd){
	this->hwnd = hwnd;
}

DriverStatus::Enum CMagneto::connect(int comPortNo){
	// Com Port 연결 체크
	if (!FAS_Connect(comPortNo, Magneto::BaudRate))
		return DriverStatus::NOT_CONNECTED;

	connected = true;
	this->comPortNo = comPortNo;
	
	// Magneto 에 맞춰 slave 의 개수를 체크한다.
	for (int i = 0; i < Magneto::MaxSlaves; ++i){
		if (!FAS_IsSlaveExist(comPortNo, i))
			return DriverStatus::TOO_FEW_SLAVES;
	}

	return DriverStatus::CONNECTED;
}

// 연결이 되어 있는 상태에서는 종료하는 명령어를 실행
// 연결이 되지 않은 상태에서는 아무 작업을 수행하지 않음
void CMagneto::disconnect(){
	if (connected && (comPortNo != -1)){
		FAS_Close(comPortNo);
		connected = false;
	}
}

bool CMagneto::isConnected(){
	return connected;
}

bool CMagneto::isCompileSuccess(CString res){
	return res.Compare(Magneto::CompileMessageOk) == 0;
}

CString CMagneto::loadProtocol(CString filePath){
	CStdioFile file;
	vector<CString> rawProtocol;
	
	try{
		file.Open(filePath, CStdioFile::modeRead);
		CString line;

		while (file.ReadString(line)){
			line.MakeLower();
			rawProtocol.push_back(line);
		}
	}
	catch (CFileException e){
		return L"Compile Error: 프로토콜 파일이 존재하지 않습니다.";
	}

	return protocolCompile(rawProtocol);
}

CString CMagneto::protocolCompile(vector<CString> &protocol){
	CString compileMessage = L"=====Compile Error=====\n";
	// cmd list 를 만들어 command 와 mapping 시킨다.
	static const CString tempCmdList[9] = { L"goto", L"load", L"mixing", L"waiting", L"magnet up", L"magnet down", L"waste", L"home", L"rotate" };

	// 이전의 Protocol 을 비운다.
	protcolBinary.clear();
	isCompileEnd = false;

	// 비정상적인 파일임을 알림
	if (protocol.size() == 0)
		return L"Compile Error: 비정상인 파일입니다.";

	// 모든 Protocol line 을 읽는다.
	for (int i = 0; i < protocol.size(); ++i){
		int offset = 0;
		CString line = protocol[i].Trim();
		CString cmd = line.Tokenize(L" ", offset).Trim();
		
		// Command 가 없는 경우와 주석 문자가 처음 시작되는 경우 무시
		if (cmd.IsEmpty())
			continue;
		else if (cmd.GetAt(0) == '%')
			continue;
		
		// 값을 저장할 구조체 초기화
		ProtocolBinary bin = { -1, -1 };

		for (int j = 0; j < ProtocolCmd::MAX+1; ++j){
			// magnet 의 경우는 line 자체가 command 이므로, line 을 체크한다.
			if (j == ProtocolCmd::MAGNET_UP || j == ProtocolCmd::MAGNET_DOWN){
				// magnet 의 경우는 arg 가 없으니 체크할 필요가 없다.
				if (line.Compare(tempCmdList[j]) == 0)
					bin.cmd = j;
			}
			else if (cmd.Compare(tempCmdList[j]) == 0){
				bin.cmd = j;

				// LOAD, WASTE, HOME, TEST 명령은 arg 가 필요하지 않음
				// MAGNET 은 위의 if 문에서 체크 되어진다.
				if ( !((j == ProtocolCmd::LOAD) || (j == ProtocolCmd::MAGNET_UP) || 
					(j == ProtocolCmd::HOME) || (j == ProtocolCmd::WASTE) || (j == ProtocolCmd::ROTATE)) ){
					CString arg = line.Tokenize(L" ", offset);

					// arg 값이 있는지 체크
					if (arg.Compare(L"") != 0)
						bin.arg = _ttoi(arg);
					else	// 없는 경우 에러 메시지 추가
						compileMessage.Format(L"%s\nLine %d : Invalid argument value", compileMessage, i+1);
				}
				break;
			}
		}

		if (bin.cmd == -1)
			compileMessage.Format(L"%s\nLine %d : Invalid command value", compileMessage, i + 1);
		else
			protcolBinary.push_back(bin);
	}

	// Compile error message 가 변경되지 않은 경우 성공한 경우
	if (compileMessage.Compare(L"=====Compile Error=====\n") == 0){
		isCompileEnd = true;
		return Magneto::CompileMessageOk;
	}

	return compileMessage;
}

void CMagneto::initPredefinedAction(){
	// Protocol Command 와 vector 와 mapping 이 되므로 
	// ProtocolCmd namespace 의 enum 순서대로 생성하면 된다.

	// 기존의 preDefinedAction 이 있는 경우, vector 초기화
	if (preDefinedAction.size() != 0)
		preDefinedAction.clear();
	
	// Command 의 길이만큼 vector 에 새로운 ActionBinary 들을 미리 넣어둔다.
	// 변수 값으로 가지지 않고, 바로 생성하여 넣음으로써 memory leak 을 방지.
	for (int i = 0; i < ProtocolCmd::MAX + 1; ++i)
		preDefinedAction.push_back(vector<ActionBinary>());
	// GO Command argument 값을 넣어준다.
	preDefinedAction[ProtocolCmd::GO].push_back(ActionBinary(ActionCmd::MOVE_ABS, 3, MotorType::X_AXIS, Magneto::DefaultPos, M_X_AXIS_SPEED));
	preDefinedAction[ProtocolCmd::GO].push_back(ActionBinary(ActionCmd::MOVE_ABS, 3, MotorType::Y_AXIS, Magneto::DefaultPos, M_X_AXIS_SPEED));

	// LOAD Command argument 값을 넣어준다.
	preDefinedAction[ProtocolCmd::LOAD].push_back(ActionBinary(ActionCmd::MOVE_ABS, 3, MotorType::LOAD, M_LOAD_POS_LOAD, M_LOAD_SPEED));
	preDefinedAction[ProtocolCmd::LOAD].push_back(ActionBinary(ActionCmd::MOVE_ABS, 3, MotorType::LOAD, M_LOAD_POS_ORIGIN, M_LOAD_SPEED));

	// MIX Command argument 값을 넣어준다.
	preDefinedAction[ProtocolCmd::MIX].push_back(ActionBinary(ActionCmd::MOVE_ABS, 3, MotorType::X_AXIS, CHAMBER_6_POS_X, M_X_AXIS_SPEED));
	preDefinedAction[ProtocolCmd::MIX].push_back(ActionBinary(ActionCmd::MOVE_ABS, 3, MotorType::Y_AXIS, CHAMBER_6_POS_Y, M_Y_AXIS_SPEED));
	preDefinedAction[ProtocolCmd::MIX].push_back(ActionBinary(ActionCmd::MOVE_ABS, 3, MotorType::LOAD, M_LOAD_POS_MIXING_BOTTOM, M_LOAD_MIXING_SPEED));
	preDefinedAction[ProtocolCmd::MIX].push_back(ActionBinary(ActionCmd::MOVE_ABS, 3, MotorType::LOAD, M_LOAD_POS_ORIGIN, M_LOAD_MIXING_SPEED));
	
	// WAIT Command argument 값을 넣어준다.
	preDefinedAction[ProtocolCmd::WAIT].push_back(ActionBinary(ActionCmd::WAIT, 1, Magneto::DefaultPos));
	
	// MAGNET UP Command argument 값을 넣어준다.
	preDefinedAction[ProtocolCmd::MAGNET_UP].push_back(ActionBinary(ActionCmd::MOVE_ABS, 3, MotorType::MAGNET, M_MAGNET_POS_ORIGIN, M_MAGNET_SPEED));
	
	// MAGNET DOWN Command argument 값을 넣어준다.
	preDefinedAction[ProtocolCmd::MAGNET_DOWN].push_back(ActionBinary(ActionCmd::MOVE_ABS, 3, MotorType::X_AXIS, M_X_AXIS_MAGNET_POSITION, M_X_AXIS_SPEED));
	preDefinedAction[ProtocolCmd::MAGNET_DOWN].push_back(ActionBinary(ActionCmd::MOVE_ABS, 3, MotorType::MAGNET, M_MAGNET_POS_DOWN, M_MAGNET_SPEED));
	
	//  waste 와 pcr 에서 syringe와 체결에 관한 서술
	// 1. waste 와 PCR 과정은 모두 음압을 필요로 하며 syringe 가 DOWN의 위치에서 ORIGIN 위치로 이동해야 한다.
	// 2. waste 와 PCR 과정에서 magnet 카트리지의 회전에 의해 waste 채널과 PCR 채널을 선택할 수 있으며,
	//    모든 채널이 잠긴 blocked 모드도 가능하다.
	// 3. syringe의 DOWN은 카트리지 깔대기 부분에 액체가 없을 때, magnet 카트리지가 waste 채널일 때 가능하다.
	// 4. PCR 과정에서 음압이 작용할 때 PCR 칩의 한 쪽에서 음압이 작용하므로 시료가 PCR 칩으로 전달된다.
	
	// waste 에 관한 서술
	// 1. waste 를 하기 전, syringe 는 반드시 DOWN 상태여야 한다.
	// 2. magnet 을 체결 준비 위치까지 다운시킨다.
	// 3. rotate 축을 회전시켜 waste 채널(waste 를 위한 배출구) 을 개방한다.
	// 4. magnet 을 체결 시킨다(체결 후 회전을 시키지 않는 이유는 마찰로 인한 마모를 최소화 하기 위함).
	// 5. syringe 를 ORIGIN 위치로 올리면 음압에 의해 깔대기 안의 액체가 바깥으로 쏟아진다(배출=waste).
	// 6. 다시 syringe 를 DOWN 시켜 다음 waste 준비를 한다(이 때, waste 채널을 통해 공기가 바깥으로 배출된다).
	// 7. magnet 을 살짝 들어올린 후 rotate 축을 회전시켜 blocked 채널을 활성화 한 후 magnet을 완전히 UP 시킨다.

	// WASTE Command argument 값을 넣어준다.
	preDefinedAction[ProtocolCmd::WASTE].push_back(ActionBinary(ActionCmd::HOME, 1, MotorType::MAGNET));
	preDefinedAction[ProtocolCmd::WASTE].push_back(ActionBinary(ActionCmd::HOME, 1, MotorType::LOAD));
	preDefinedAction[ProtocolCmd::WASTE].push_back(ActionBinary(ActionCmd::MOVE_ABS, 3, MotorType::X_AXIS, M_X_AXIS_MAGNET_POSITION, M_X_AXIS_SPEED));
	preDefinedAction[ProtocolCmd::WASTE].push_back(ActionBinary(ActionCmd::MOVE_ABS, 3, MotorType::SYRINGE, M_SYRINGE_POS_DOWN, M_SYRINGE_SPEED));
	preDefinedAction[ProtocolCmd::WASTE].push_back(ActionBinary(ActionCmd::MOVE_ABS, 3, MotorType::MAGNET, M_MAGNET_POS_DOWN, M_MAGNET_SPEED));
	preDefinedAction[ProtocolCmd::WASTE].push_back(ActionBinary(ActionCmd::MOVE_ABS, 3, MotorType::MAGNET, M_MAGNET_POS_LOCK, M_MAGNET_SPEED));
	preDefinedAction[ProtocolCmd::WASTE].push_back(ActionBinary(ActionCmd::MOVE_ABS, 3, MotorType::SYRINGE, M_SYRINGE_POS_ORIGIN, M_SYRINGE_SPEED));
	preDefinedAction[ProtocolCmd::WASTE].push_back(ActionBinary(ActionCmd::MOVE_ABS, 3, MotorType::SYRINGE, M_SYRINGE_POS_DOWN, M_SYRINGE_SPEED));
	preDefinedAction[ProtocolCmd::WASTE].push_back(ActionBinary(ActionCmd::MOVE_ABS, 3, MotorType::MAGNET, M_MAGNET_POS_DOWN, M_MAGNET_SPEED));
	preDefinedAction[ProtocolCmd::WASTE].push_back(ActionBinary(ActionCmd::MOVE_ABS, 3, MotorType::MAGNET, M_MAGNET_POS_ORIGIN, M_MAGNET_SPEED));

	// HOME Command argument 값을 넣어준다.
	preDefinedAction[ProtocolCmd::HOME].push_back(ActionBinary(ActionCmd::HOME, 1, MotorType::MAGNET));
	preDefinedAction[ProtocolCmd::HOME].push_back(ActionBinary(ActionCmd::HOME, 1, MotorType::LOAD));
	preDefinedAction[ProtocolCmd::HOME].push_back(ActionBinary(ActionCmd::HOME, 1, MotorType::SYRINGE));
	preDefinedAction[ProtocolCmd::HOME].push_back(ActionBinary(ActionCmd::HOME, 1, MotorType::X_AXIS));
	preDefinedAction[ProtocolCmd::HOME].push_back(ActionBinary(ActionCmd::MOVE_ABS, 3, MotorType::X_AXIS, CHAMBER_1_POS_X, M_X_AXIS_SPEED));
	preDefinedAction[ProtocolCmd::HOME].push_back(ActionBinary(ActionCmd::HOME, 1, MotorType::Y_AXIS));
	preDefinedAction[ProtocolCmd::HOME].push_back(ActionBinary(ActionCmd::MOVE_ABS, 3, MotorType::SYRINGE, M_SYRINGE_POS_DOWN, M_SYRINGE_SPEED));
	preDefinedAction[ProtocolCmd::HOME].push_back(ActionBinary(ActionCmd::MOVE_ABS, 3, MotorType::ROTATE, M_ROTATE_POS_ORIGIN, M_ROTATE_SPEED));
	preDefinedAction[ProtocolCmd::HOME].push_back(ActionBinary(ActionCmd::HOME, 1, MotorType::FILTER));	// 150922 YJ filter home
	
	// ROTATE command argument 값을 넣어줌 150922 YJ
	preDefinedAction[ProtocolCmd::ROTATE].push_back(ActionBinary(ActionCmd::MOVE_ABS, 3, MotorType::ROTATE, M_ROTATE_POS_WASTE, M_ROTATE_SPEED));
	preDefinedAction[ProtocolCmd::ROTATE].push_back(ActionBinary(ActionCmd::MOVE_ABS, 3, MotorType::FILTER, M_FILTER_180, M_FILTER_SPEED));
}

void CMagneto::initDriverParameter(){
	if (FAS_SetParameter(comPortNo, 0, 17, 20000) != FMM_OK)
		driverErrCnt++;// magnet home speed: 20000 pps
	if (FAS_SetParameter(comPortNo, 1, 17, 20000) != FMM_OK)
		driverErrCnt++;// load home speed: 20000 pps
	if (FAS_SetParameter(comPortNo, 2, 17, 60000) != FMM_OK)
		driverErrCnt++;// Y axis home speed: 60000 pps
	if (FAS_SetParameter(comPortNo, 3, 17, 20000) != FMM_OK)
		driverErrCnt++;// rotate home speed: 미정
	if (FAS_SetParameter(comPortNo, 4, 17, 20000) != FMM_OK)
		driverErrCnt++;// X axis home speed: 20000 pps
	if (FAS_SetParameter(comPortNo, 5, 17, 60000) != FMM_OK)
		driverErrCnt++;// Syringe home speed: 미정
	if (FAS_SetParameter(comPortNo, 4, 21, 1) != FMM_OK)
		driverErrCnt++;		// X axis 의 홈 방향을 ccw 로 설정

	// For filter
	if (FAS_SetParameter(comPortNo, 6, 17, 20000) != FMM_OK)
		driverErrCnt++;// Syringe home speed: 미정
}

void CMagneto::generateActionList(vector<ActionBeans> &returnValue){
	actionList.clear();
	returnValue.clear();

	for (int i = 0; i < protcolBinary.size(); ++i){
		ProtocolBinary pb = protcolBinary[i];

		// 저장할 action 의 부모 command 값을 설정
		ActionData actionExe(pb.cmd);
		CString protocolBean = ProtocolCmd::toString[pb.cmd];
		if( pb.arg != -1 )
			protocolBean.Format(L"%s %d", protocolBean, pb.arg);
		ActionBeans actionBean(protocolBean);

		// cmd 값을 predefined action 과 mapping 시킨다.
		vector<ActionBinary> ab = preDefinedAction[pb.cmd];
		
		// LOAD, WASTE, HOME 명령은 arg 가 필요하지 않음
		// MAGNET 은 위의 if 문에서 체크 되어진다.

		for (int j = 0; j < ab.size(); ++j){
			ActionBinary tempAb = ab[j];
			CString command = ActionCmd::toString[tempAb.cmd];
			CString motor = MotorType::toString[tempAb.args[0]];
			CString child;

			// GOTO 의 경우 arg 값을 chamber 에 따른 x, y 에 따라 다른 값을 설정해줘야 한다.
			// arg[1] 값은 position 값인데, default 로 되어 있을 경우, 정해져 있는 position 값으로 설정하도록 한다.
			if (pb.cmd == ProtocolCmd::GO){
				int chamber = pb.arg - 1;
				int x_pos = Magneto::gAxisPosition[chamber][0];
				int y_pos = Magneto::gAxisPosition[chamber][1];

				if (tempAb.args[1] == Magneto::DefaultPos){
					// args[0] 값은 어떤 모터를 설정할지 설정하는 값으로, x, y axis 에 따라 값을 설정한다.
					if (tempAb.args[0] == MotorType::X_AXIS)
						tempAb.args[1] = x_pos;
					else if (tempAb.args[0] == MotorType::Y_AXIS)
						tempAb.args[1] = y_pos;
				}
			}
			else{
				// 전체 argument 목록 중에 DEFAULT_POS 값으로 설정된 것이 있으면 argument 값으로 설정해준다.
			
				for (int k = 0; k < tempAb.args.size(); ++k){
					if (tempAb.args[k] == Magneto::DefaultPos)
						tempAb.args[k] = pb.arg;
				}
			}

			if (tempAb.cmd == ActionCmd::WAIT && tempAb.args.size() == 1)
				child.Format(L"%s, Wait(min): %d", command, tempAb.args[0]);
			else if (tempAb.args.size() == 1)
				child.Format(L"%s, %s", command, motor);
			else if (tempAb.args.size() == 3){
				child.Format(L"%s, %s, %0.3f, %d", 
					command, motor, pulse2mili(((MotorType::Enum)tempAb.args[0]), tempAb.args[1]), tempAb.args[2]);
			}
			else
				child.Format(L"%s, %s", command, motor);
			
			// actionList 에 저장하기 위해, childAction 에 현재 값을 저장함
			actionExe.actions.push_back(tempAb);
			actionBean.childAction.push_back(child);
		}

		// mix 값의 경우는 arg 에 mix 횟수 값이 있으므로, 해당 mixing 값만큼 정해진 mixing routing 을 반복한다.
		if (pb.cmd == ProtocolCmd::MIX){
			CString child;
			for (int j = 0; j < pb.arg; ++j){
				// bottom, top 순서로 집어넣는다.
				actionExe.actions.push_back(ActionBinary(ActionCmd::MOVE_ABS, 3, MotorType::LOAD, M_LOAD_POS_MIXING_BOTTOM, M_LOAD_MIXING_SPEED));
				actionExe.actions.push_back(ActionBinary(ActionCmd::MOVE_ABS, 3, MotorType::LOAD, M_LOAD_POS_MIXING_TOP, M_LOAD_MIXING_SPEED));

				// for return value
				child.Format(L"%s, %s, %0.3f, %d", 
					ActionCmd::toString[ActionCmd::MOVE_ABS], MotorType::toString[MotorType::LOAD], M_LOAD_PUSLE2MILI(M_LOAD_POS_MIXING_BOTTOM), M_LOAD_MIXING_SPEED);
				actionBean.childAction.push_back(child);
				child.Format(L"%s, %s, %0.3f, %d",
					ActionCmd::toString[ActionCmd::MOVE_ABS], MotorType::toString[MotorType::LOAD], M_LOAD_PUSLE2MILI(M_LOAD_POS_MIXING_TOP), M_LOAD_MIXING_SPEED);
				actionBean.childAction.push_back(child);
			}

			// origin 으로 이동하는 routine 을 삽입
			actionExe.actions.push_back(ActionBinary(ActionCmd::MOVE_ABS, 3, MotorType::LOAD, M_LOAD_POS_ORIGIN, M_LOAD_MIXING_SPEED));
			child.Format(L"%s, %s, %0.3f, %d",
				ActionCmd::toString[ActionCmd::MOVE_ABS], MotorType::toString[MotorType::LOAD], M_LOAD_PUSLE2MILI(M_LOAD_POS_ORIGIN), M_LOAD_MIXING_SPEED);
			actionBean.childAction.push_back(child);
		}

		returnValue.push_back(actionBean);
		actionList.push_back(actionExe);
	}
}

bool CMagneto::isLimitSwitchPushed(){
	// slave 들에 대해 limit switch 들을 체크해본다.
	EZISTEP_MINI_AXISSTATUS axisStatus;
	for (int i = 0; i < Magneto::MaxSlaves; ++i){
		if (FAS_GetAxisStatus(comPortNo, i, &axisStatus.dwValue) != FMM_OK){
			driverErrCnt++;
			return true;
		}
		if (axisStatus.FFLAG_HWPOSILMT || axisStatus.FFLAG_HWNEGALMT)
			return true;
	}

	return false;
}

bool CMagneto::isActionFinished(){
	ActionData action = actionList[currentAction];
	ActionBinary ab = action.actions[currentSubAction];

	// axis 상태를 받아온다.
	EZISTEP_MINI_AXISSTATUS axisStatus;
	if (FAS_GetAxisStatus(comPortNo, ab.args[0], &axisStatus.dwValue) != FMM_OK){
		driverErrCnt++;
		return true;
	}

	switch (ab.cmd){
		case ActionCmd::MOVE_ABS:
		case ActionCmd::MOVE_INC:
		case ActionCmd::MOVE_DEC:
		case ActionCmd::HOME:
			return !axisStatus.FFLAG_MOTIONING;
		case ActionCmd::WAIT:
			return isWaitEnd;
	}

	return false;
}

/******For Waiting Command Counter**********/
UINT waitThread(LPVOID pParam){
	CMagneto *magneto = (CMagneto *)pParam;
	HWND hwnd = magneto->getSafeHwnd();
	long startTime = timeGetTime();
	int waitTime = magneto->getWaitingTime();
	
	while (true){
		Sleep(1000);
		double elapsedTime = ((double)(timeGetTime()-startTime)/1000.);

		::SendMessage(hwnd, WM_WAIT_TIME_CHANGED, waitTime, elapsedTime);

		if (magneto->isIdle())
			break;

		if (elapsedTime >= (waitTime*60))
			break;
	}

	magneto->setWaitEnded();

	return 0;
}

/******For Waiting Command Counter**********/

void CMagneto::runNextAction(){
	currentSubAction++;

	// 마지막 action 인지 체크 
	if (currentSubAction == actionList[currentAction].actions.size()){
		currentSubAction = 0;
		currentAction++;

		if (currentAction == actionList.size()){
			// start flag 를 끔으로써, emergency stop 이 호출되지 않도록 설정
			isStarted = false;

			stop();
			return;
		}
	}

	ActionData action = actionList[currentAction];
	ActionBinary ab = action.actions[currentSubAction];

	int cmd = ab.cmd;
	int slaveNo = ab.args[0];
	int cmdPos = 0;
	int velocity = 0;

	// 3개의 command 만 추가적인 arg 가 존재한다.
	// args 의 size 로 체크를 해도 됨(3개인 경우)
	if (cmd == ActionCmd::MOVE_ABS || cmd == ActionCmd::MOVE_DEC || cmd == ActionCmd::MOVE_INC){
		cmdPos = ab.args[1];
		velocity = ab.args[2];
	}

	// Command 에 따라 motor driver 에 명령을 한다.
	switch (cmd){
		case ActionCmd::MOVE_ABS:
			if (FAS_MoveSingleAxisAbsPos(comPortNo, slaveNo, cmdPos, velocity) != FMM_OK)
				driverErrCnt++;
			break;
		case ActionCmd::MOVE_DEC:
			if (FAS_MoveSingleAxisIncPos(comPortNo, slaveNo, cmdPos * -1, velocity) != FMM_OK)
				driverErrCnt++;
			break;
		case ActionCmd::MOVE_INC:
			if (FAS_MoveSingleAxisIncPos(comPortNo, slaveNo, cmdPos, velocity) != FMM_OK)
				driverErrCnt++;
			break;
		case ActionCmd::HOME:
			if (FAS_MoveOriginSingleAxis(comPortNo, slaveNo) != FMM_OK)
				driverErrCnt++;
			break;
		case ActionCmd::WAIT:
			waitCounter = ab.args[0];
			isWaitEnd = false;

			CWinThread *thread = ::AfxBeginThread(waitThread, this, THREAD_PRIORITY_NORMAL, 0, CREATE_SUSPENDED);
			thread->m_bAutoDelete = TRUE;
			thread->ResumeThread();
			break;
	}
}

void CMagneto::resetAction(){
	currentAction = 0;
	currentSubAction = -1;
	isWaitEnd = false;
	driverErrCnt = 0;
	waitCounter = 0;
}

HWND CMagneto::getSafeHwnd(){
	return hwnd;
}

int CMagneto::getWaitingTime(){
	return waitCounter;
}

void CMagneto::setWaitEnded(){
	isWaitEnd = true;
}

bool CMagneto::isWaitEnded(){
	return isWaitEnd;
}

bool CMagneto::isIdle(){
	return !isStarted;
}

bool CMagneto::isCompileEnded(){
	return isCompileEnd;
}

LPARAM CMagneto::getCurrentAction(){
	return MAKELPARAM(currentAction, currentSubAction);
}

MotorType::Enum CMagneto::getCurrentMotor(){
	return (MotorType::Enum)actionList[currentAction].actions[currentSubAction].args[0];
}

void CMagneto::start(){
	if (!isStarted){
		initDriverParameter();
		resetAction();
		runNextAction();
		isStarted = true;
	}
}

bool CMagneto::runTask(){
	if (isLimitSwitchPushed()){
		stop();
		return false;
	}
	
	ActionData action = actionList[currentAction];
	ActionBinary ab = action.actions[currentSubAction];

	// Motor position 값 통지
	long tempPos = 0;
	double cmdPos = 0.0, targetPos = 0.0;

	if (FAS_GetCommandPos(comPortNo, ab.args[0], &tempPos) != FMM_OK)
		driverErrCnt++;

	cmdPos = pulse2mili((MotorType::Enum)ab.args[0], tempPos);
	if (ab.args.size() >= 2)
		targetPos = pulse2mili((MotorType::Enum)ab.args[0], ab.args[1]);

	MotorPos motorPos = { targetPos, cmdPos };

	::SendMessage(hwnd, WM_MOTOR_POS_CHANGED, reinterpret_cast<WPARAM>(&motorPos), getCurrentAction());

	// Action 의 종료 상태를 체크
	if (!isActionFinished())
		return true;

	runNextAction();

	return true;
}

void CMagneto::stop(){
	if (isStarted)
		FAS_AllEmergencyStop(comPortNo);

	resetAction();
	isStarted = false;

}