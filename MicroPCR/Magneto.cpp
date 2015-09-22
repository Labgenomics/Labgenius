
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
	// Com Port ���� üũ
	if (!FAS_Connect(comPortNo, Magneto::BaudRate))
		return DriverStatus::NOT_CONNECTED;

	connected = true;
	this->comPortNo = comPortNo;
	
	// Magneto �� ���� slave �� ������ üũ�Ѵ�.
	for (int i = 0; i < Magneto::MaxSlaves; ++i){
		if (!FAS_IsSlaveExist(comPortNo, i))
			return DriverStatus::TOO_FEW_SLAVES;
	}

	return DriverStatus::CONNECTED;
}

// ������ �Ǿ� �ִ� ���¿����� �����ϴ� ��ɾ ����
// ������ ���� ���� ���¿����� �ƹ� �۾��� �������� ����
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
		return L"Compile Error: �������� ������ �������� �ʽ��ϴ�.";
	}

	return protocolCompile(rawProtocol);
}

CString CMagneto::protocolCompile(vector<CString> &protocol){
	CString compileMessage = L"=====Compile Error=====\n";
	// cmd list �� ����� command �� mapping ��Ų��.
	static const CString tempCmdList[9] = { L"goto", L"load", L"mixing", L"waiting", L"magnet up", L"magnet down", L"waste", L"home", L"rotate" };

	// ������ Protocol �� ����.
	protcolBinary.clear();
	isCompileEnd = false;

	// ���������� �������� �˸�
	if (protocol.size() == 0)
		return L"Compile Error: �������� �����Դϴ�.";

	// ��� Protocol line �� �д´�.
	for (int i = 0; i < protocol.size(); ++i){
		int offset = 0;
		CString line = protocol[i].Trim();
		CString cmd = line.Tokenize(L" ", offset).Trim();
		
		// Command �� ���� ���� �ּ� ���ڰ� ó�� ���۵Ǵ� ��� ����
		if (cmd.IsEmpty())
			continue;
		else if (cmd.GetAt(0) == '%')
			continue;
		
		// ���� ������ ����ü �ʱ�ȭ
		ProtocolBinary bin = { -1, -1 };

		for (int j = 0; j < ProtocolCmd::MAX+1; ++j){
			// magnet �� ���� line ��ü�� command �̹Ƿ�, line �� üũ�Ѵ�.
			if (j == ProtocolCmd::MAGNET_UP || j == ProtocolCmd::MAGNET_DOWN){
				// magnet �� ���� arg �� ������ üũ�� �ʿ䰡 ����.
				if (line.Compare(tempCmdList[j]) == 0)
					bin.cmd = j;
			}
			else if (cmd.Compare(tempCmdList[j]) == 0){
				bin.cmd = j;

				// LOAD, WASTE, HOME, TEST ����� arg �� �ʿ����� ����
				// MAGNET �� ���� if ������ üũ �Ǿ�����.
				if ( !((j == ProtocolCmd::LOAD) || (j == ProtocolCmd::MAGNET_UP) || 
					(j == ProtocolCmd::HOME) || (j == ProtocolCmd::WASTE) || (j == ProtocolCmd::ROTATE)) ){
					CString arg = line.Tokenize(L" ", offset);

					// arg ���� �ִ��� üũ
					if (arg.Compare(L"") != 0)
						bin.arg = _ttoi(arg);
					else	// ���� ��� ���� �޽��� �߰�
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

	// Compile error message �� ������� ���� ��� ������ ���
	if (compileMessage.Compare(L"=====Compile Error=====\n") == 0){
		isCompileEnd = true;
		return Magneto::CompileMessageOk;
	}

	return compileMessage;
}

void CMagneto::initPredefinedAction(){
	// Protocol Command �� vector �� mapping �� �ǹǷ� 
	// ProtocolCmd namespace �� enum ������� �����ϸ� �ȴ�.

	// ������ preDefinedAction �� �ִ� ���, vector �ʱ�ȭ
	if (preDefinedAction.size() != 0)
		preDefinedAction.clear();
	
	// Command �� ���̸�ŭ vector �� ���ο� ActionBinary ���� �̸� �־�д�.
	// ���� ������ ������ �ʰ�, �ٷ� �����Ͽ� �������ν� memory leak �� ����.
	for (int i = 0; i < ProtocolCmd::MAX + 1; ++i)
		preDefinedAction.push_back(vector<ActionBinary>());
	// GO Command argument ���� �־��ش�.
	preDefinedAction[ProtocolCmd::GO].push_back(ActionBinary(ActionCmd::MOVE_ABS, 3, MotorType::X_AXIS, Magneto::DefaultPos, M_X_AXIS_SPEED));
	preDefinedAction[ProtocolCmd::GO].push_back(ActionBinary(ActionCmd::MOVE_ABS, 3, MotorType::Y_AXIS, Magneto::DefaultPos, M_X_AXIS_SPEED));

	// LOAD Command argument ���� �־��ش�.
	preDefinedAction[ProtocolCmd::LOAD].push_back(ActionBinary(ActionCmd::MOVE_ABS, 3, MotorType::LOAD, M_LOAD_POS_LOAD, M_LOAD_SPEED));
	preDefinedAction[ProtocolCmd::LOAD].push_back(ActionBinary(ActionCmd::MOVE_ABS, 3, MotorType::LOAD, M_LOAD_POS_ORIGIN, M_LOAD_SPEED));

	// MIX Command argument ���� �־��ش�.
	preDefinedAction[ProtocolCmd::MIX].push_back(ActionBinary(ActionCmd::MOVE_ABS, 3, MotorType::X_AXIS, CHAMBER_6_POS_X, M_X_AXIS_SPEED));
	preDefinedAction[ProtocolCmd::MIX].push_back(ActionBinary(ActionCmd::MOVE_ABS, 3, MotorType::Y_AXIS, CHAMBER_6_POS_Y, M_Y_AXIS_SPEED));
	preDefinedAction[ProtocolCmd::MIX].push_back(ActionBinary(ActionCmd::MOVE_ABS, 3, MotorType::LOAD, M_LOAD_POS_MIXING_BOTTOM, M_LOAD_MIXING_SPEED));
	preDefinedAction[ProtocolCmd::MIX].push_back(ActionBinary(ActionCmd::MOVE_ABS, 3, MotorType::LOAD, M_LOAD_POS_ORIGIN, M_LOAD_MIXING_SPEED));
	
	// WAIT Command argument ���� �־��ش�.
	preDefinedAction[ProtocolCmd::WAIT].push_back(ActionBinary(ActionCmd::WAIT, 1, Magneto::DefaultPos));
	
	// MAGNET UP Command argument ���� �־��ش�.
	preDefinedAction[ProtocolCmd::MAGNET_UP].push_back(ActionBinary(ActionCmd::MOVE_ABS, 3, MotorType::MAGNET, M_MAGNET_POS_ORIGIN, M_MAGNET_SPEED));
	
	// MAGNET DOWN Command argument ���� �־��ش�.
	preDefinedAction[ProtocolCmd::MAGNET_DOWN].push_back(ActionBinary(ActionCmd::MOVE_ABS, 3, MotorType::X_AXIS, M_X_AXIS_MAGNET_POSITION, M_X_AXIS_SPEED));
	preDefinedAction[ProtocolCmd::MAGNET_DOWN].push_back(ActionBinary(ActionCmd::MOVE_ABS, 3, MotorType::MAGNET, M_MAGNET_POS_DOWN, M_MAGNET_SPEED));
	
	//  waste �� pcr ���� syringe�� ü�ῡ ���� ����
	// 1. waste �� PCR ������ ��� ������ �ʿ�� �ϸ� syringe �� DOWN�� ��ġ���� ORIGIN ��ġ�� �̵��ؾ� �Ѵ�.
	// 2. waste �� PCR �������� magnet īƮ������ ȸ���� ���� waste ä�ΰ� PCR ä���� ������ �� ������,
	//    ��� ä���� ��� blocked ��嵵 �����ϴ�.
	// 3. syringe�� DOWN�� īƮ���� ���� �κп� ��ü�� ���� ��, magnet īƮ������ waste ä���� �� �����ϴ�.
	// 4. PCR �������� ������ �ۿ��� �� PCR Ĩ�� �� �ʿ��� ������ �ۿ��ϹǷ� �÷ᰡ PCR Ĩ���� ���޵ȴ�.
	
	// waste �� ���� ����
	// 1. waste �� �ϱ� ��, syringe �� �ݵ�� DOWN ���¿��� �Ѵ�.
	// 2. magnet �� ü�� �غ� ��ġ���� �ٿ��Ų��.
	// 3. rotate ���� ȸ������ waste ä��(waste �� ���� ���ⱸ) �� �����Ѵ�.
	// 4. magnet �� ü�� ��Ų��(ü�� �� ȸ���� ��Ű�� �ʴ� ������ ������ ���� ���� �ּ�ȭ �ϱ� ����).
	// 5. syringe �� ORIGIN ��ġ�� �ø��� ���п� ���� ���� ���� ��ü�� �ٱ����� �������(����=waste).
	// 6. �ٽ� syringe �� DOWN ���� ���� waste �غ� �Ѵ�(�� ��, waste ä���� ���� ���Ⱑ �ٱ����� ����ȴ�).
	// 7. magnet �� ��¦ ���ø� �� rotate ���� ȸ������ blocked ä���� Ȱ��ȭ �� �� magnet�� ������ UP ��Ų��.

	// WASTE Command argument ���� �־��ش�.
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

	// HOME Command argument ���� �־��ش�.
	preDefinedAction[ProtocolCmd::HOME].push_back(ActionBinary(ActionCmd::HOME, 1, MotorType::MAGNET));
	preDefinedAction[ProtocolCmd::HOME].push_back(ActionBinary(ActionCmd::HOME, 1, MotorType::LOAD));
	preDefinedAction[ProtocolCmd::HOME].push_back(ActionBinary(ActionCmd::HOME, 1, MotorType::SYRINGE));
	preDefinedAction[ProtocolCmd::HOME].push_back(ActionBinary(ActionCmd::HOME, 1, MotorType::X_AXIS));
	preDefinedAction[ProtocolCmd::HOME].push_back(ActionBinary(ActionCmd::MOVE_ABS, 3, MotorType::X_AXIS, CHAMBER_1_POS_X, M_X_AXIS_SPEED));
	preDefinedAction[ProtocolCmd::HOME].push_back(ActionBinary(ActionCmd::HOME, 1, MotorType::Y_AXIS));
	preDefinedAction[ProtocolCmd::HOME].push_back(ActionBinary(ActionCmd::MOVE_ABS, 3, MotorType::SYRINGE, M_SYRINGE_POS_DOWN, M_SYRINGE_SPEED));
	preDefinedAction[ProtocolCmd::HOME].push_back(ActionBinary(ActionCmd::MOVE_ABS, 3, MotorType::ROTATE, M_ROTATE_POS_ORIGIN, M_ROTATE_SPEED));
	preDefinedAction[ProtocolCmd::HOME].push_back(ActionBinary(ActionCmd::HOME, 1, MotorType::FILTER));	// 150922 YJ filter home
	
	// ROTATE command argument ���� �־��� 150922 YJ
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
		driverErrCnt++;// rotate home speed: ����
	if (FAS_SetParameter(comPortNo, 4, 17, 20000) != FMM_OK)
		driverErrCnt++;// X axis home speed: 20000 pps
	if (FAS_SetParameter(comPortNo, 5, 17, 60000) != FMM_OK)
		driverErrCnt++;// Syringe home speed: ����
	if (FAS_SetParameter(comPortNo, 4, 21, 1) != FMM_OK)
		driverErrCnt++;		// X axis �� Ȩ ������ ccw �� ����

	// For filter
	if (FAS_SetParameter(comPortNo, 6, 17, 20000) != FMM_OK)
		driverErrCnt++;// Syringe home speed: ����
}

void CMagneto::generateActionList(vector<ActionBeans> &returnValue){
	actionList.clear();
	returnValue.clear();

	for (int i = 0; i < protcolBinary.size(); ++i){
		ProtocolBinary pb = protcolBinary[i];

		// ������ action �� �θ� command ���� ����
		ActionData actionExe(pb.cmd);
		CString protocolBean = ProtocolCmd::toString[pb.cmd];
		if( pb.arg != -1 )
			protocolBean.Format(L"%s %d", protocolBean, pb.arg);
		ActionBeans actionBean(protocolBean);

		// cmd ���� predefined action �� mapping ��Ų��.
		vector<ActionBinary> ab = preDefinedAction[pb.cmd];
		
		// LOAD, WASTE, HOME ����� arg �� �ʿ����� ����
		// MAGNET �� ���� if ������ üũ �Ǿ�����.

		for (int j = 0; j < ab.size(); ++j){
			ActionBinary tempAb = ab[j];
			CString command = ActionCmd::toString[tempAb.cmd];
			CString motor = MotorType::toString[tempAb.args[0]];
			CString child;

			// GOTO �� ��� arg ���� chamber �� ���� x, y �� ���� �ٸ� ���� ��������� �Ѵ�.
			// arg[1] ���� position ���ε�, default �� �Ǿ� ���� ���, ������ �ִ� position ������ �����ϵ��� �Ѵ�.
			if (pb.cmd == ProtocolCmd::GO){
				int chamber = pb.arg - 1;
				int x_pos = Magneto::gAxisPosition[chamber][0];
				int y_pos = Magneto::gAxisPosition[chamber][1];

				if (tempAb.args[1] == Magneto::DefaultPos){
					// args[0] ���� � ���͸� �������� �����ϴ� ������, x, y axis �� ���� ���� �����Ѵ�.
					if (tempAb.args[0] == MotorType::X_AXIS)
						tempAb.args[1] = x_pos;
					else if (tempAb.args[0] == MotorType::Y_AXIS)
						tempAb.args[1] = y_pos;
				}
			}
			else{
				// ��ü argument ��� �߿� DEFAULT_POS ������ ������ ���� ������ argument ������ �������ش�.
			
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
			
			// actionList �� �����ϱ� ����, childAction �� ���� ���� ������
			actionExe.actions.push_back(tempAb);
			actionBean.childAction.push_back(child);
		}

		// mix ���� ���� arg �� mix Ƚ�� ���� �����Ƿ�, �ش� mixing ����ŭ ������ mixing routing �� �ݺ��Ѵ�.
		if (pb.cmd == ProtocolCmd::MIX){
			CString child;
			for (int j = 0; j < pb.arg; ++j){
				// bottom, top ������ ����ִ´�.
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

			// origin ���� �̵��ϴ� routine �� ����
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
	// slave �鿡 ���� limit switch ���� üũ�غ���.
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

	// axis ���¸� �޾ƿ´�.
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

	// ������ action ���� üũ 
	if (currentSubAction == actionList[currentAction].actions.size()){
		currentSubAction = 0;
		currentAction++;

		if (currentAction == actionList.size()){
			// start flag �� �����ν�, emergency stop �� ȣ����� �ʵ��� ����
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

	// 3���� command �� �߰����� arg �� �����Ѵ�.
	// args �� size �� üũ�� �ص� ��(3���� ���)
	if (cmd == ActionCmd::MOVE_ABS || cmd == ActionCmd::MOVE_DEC || cmd == ActionCmd::MOVE_INC){
		cmdPos = ab.args[1];
		velocity = ab.args[2];
	}

	// Command �� ���� motor driver �� ����� �Ѵ�.
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

	// Motor position �� ����
	long tempPos = 0;
	double cmdPos = 0.0, targetPos = 0.0;

	if (FAS_GetCommandPos(comPortNo, ab.args[0], &tempPos) != FMM_OK)
		driverErrCnt++;

	cmdPos = pulse2mili((MotorType::Enum)ab.args[0], tempPos);
	if (ab.args.size() >= 2)
		targetPos = pulse2mili((MotorType::Enum)ab.args[0], ab.args[1]);

	MotorPos motorPos = { targetPos, cmdPos };

	::SendMessage(hwnd, WM_MOTOR_POS_CHANGED, reinterpret_cast<WPARAM>(&motorPos), getCurrentAction());

	// Action �� ���� ���¸� üũ
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