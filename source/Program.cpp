#include "Program.h"

Program::Program()
{
	_setmode(_fileno(stdin), _O_U16TEXT);
	this->Vol = nullptr;
}

Program::~Program()
{
	if (this->Vol != nullptr) {
		delete this->Vol;
		this->Vol = nullptr;
	}
}

void Program::run()
{
	while (true) {
		system("cls");
		
		cout << "  FIT HCMUS - Operating System - Project 17" << "\n";
		cout << "  * Nguyen Hoang Nhan - 18127017" << "\n";
		cout << "  * Kieu Cong Hau     - 18127259" << "\n";
		cout << "  * Tran Thanh Tam    - 18127268" << "\n\n";

		cout << "  ===== ACCESS DATA OF A FILE =====" << "\n\n";
		cout << "  Program: * Input the path of a file you want to access." << "\n\n";
		cout << "           * Input <q> to exit this program." << "\n\n";
		cout << "  User: ";
		wstring str;
		getline(wcin, str);

		if (str == L"<q>") {
			break;
		}
		else {
			vector<uint32_t> clusterList;
			vector<uint64_t> startOffsetList;
			vector<uint64_t> endOffsetList;
			StreamState fileState = this->accessData(str, clusterList, startOffsetList, endOffsetList);

			if (fileState == StreamState::SUCCESS) {
				cout << "\n";
				cout << "  Program: SUCCESS." << "\n\n";
				cout << "           This file's data is stored at " << clusterList.size() << " clusters in this volume:" << "\n\n";
				
				size_t xCor[3] = { 15, 30, 50 };
				this->gotoXY(xCor[0], this->whereY());
				cout << "Cluster";
				this->gotoXY(xCor[1], this->whereY());
				cout << "Start Offset";
				this->gotoXY(xCor[2], this->whereY());
				cout << "End Offset" << "\n";

				for (size_t i = 0; i < clusterList.size(); ++i) {
					this->gotoXY(xCor[0], this->whereY());
					cout << clusterList[i];
					this->gotoXY(xCor[1], this->whereY());
					cout << startOffsetList[i];
					this->gotoXY(xCor[2], this->whereY());
					cout << endOffsetList[i] << "\n";
				}
				
				cout << "\n";
			}
			else if (fileState == StreamState::NOT_FILE) {
				cout << "\n";
				cout << "  Program: FAIL. This is not a file. Please check again!" << "\n\n";
			}
			else if (fileState == StreamState::BAD_PATH) {
				cout << "\n";
				cout << "  Program: FAIL. This path does not exist. Please check again!" << "\n\n";
			}
			else if (fileState == StreamState::NOT_FAT32) {
				cout << "\n";
				cout << "  Program: FAIL. This volume's format is not FAT32. Please check again!" << "\n\n";
			}
			else if (fileState == StreamState::NOT_VOLUME) {
				cout << "\n";
				cout << "  Program: FAIL. This volume does not exist or you need to run this program as admin to access this volume. Please check again!" << "\n\n";
			}
			else {
				throw "Enum Class Error";
			}

			cout << "  ";
			system("pause");
		}
	}
}

StreamState Program::accessData(wstring& filePath, vector<uint32_t>& clusterList, vector<uint64_t>& startOffsetList, vector<uint64_t>& endOffsetList)
{
	// Standardize filePath
	for (size_t i = 0; i < filePath.size(); ++i) {
		if (filePath[i] == '\\') {
			filePath[i] = Volume::DELIMITER;
		}
	}

	// Split filePath into a volume letter, many folder's names and a file's name
	// G:/Folder1/Folder2/FileA.txt
	// volLetter: G:
	// entryNameList: Folder1, Folder2, FileA.txt

	vector<wstring> entryNameList;
	size_t deliPos;

	wstring tempFilePath = filePath + Volume::DELIMITER;
	for (size_t i = 0; i < tempFilePath.length(); i = deliPos + 1) {
		deliPos = tempFilePath.find(Volume::DELIMITER, i);
		entryNameList.push_back(tempFilePath.substr(i, deliPos - i));
	}

	if (entryNameList.size() < 2) {
		return StreamState::BAD_PATH;
	}

	for (size_t i = 0; i < entryNameList.size(); ++i) {
		if (entryNameList[i].length() == 0) {
			return StreamState::BAD_PATH;
		}
	}

	wstring volLetter = entryNameList[0];
	entryNameList.erase(entryNameList.begin());

	// Open volume with the volume letter and access data in this volume with the entryNameList
	this->Vol = new Volume(volLetter);
	
	StreamState state = this->Vol->isOpen();
	if (state == StreamState::SUCCESS) {
		state = this->Vol->accessData(entryNameList, clusterList, startOffsetList, endOffsetList);
	}

	delete this->Vol;
	return state;
}

void Program::gotoXY(size_t const& xCor, size_t const& yCor) const
{
	COORD pos = { xCor, yCor };
	SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), pos);
}

size_t Program::whereX() const
{
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
	return csbi.dwCursorPosition.X;
}

size_t Program::whereY() const
{
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
	return csbi.dwCursorPosition.Y;
}

