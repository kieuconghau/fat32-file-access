#include "Volume.h"

wchar_t const Volume::DELIMITER = '/';

Volume::Volume(wstring const& letter)
{
	this->Path = L"\\\\.\\" + letter;
}

Volume::~Volume()
{
	if (this->FAT != nullptr) {
		delete[] this->FAT;
		this->FAT = nullptr;
	}
}

StreamState Volume::isOpen()
{
	StreamState volState = StreamState::SUCCESS;

	ifstream volStream(this->Path, ios_base::binary);

	if (!volStream.is_open()) {
		volState = StreamState::NOT_VOLUME;
	}
	else {
		// Read the first sector (boot sector) into buffer
		size_t bufferSize = 512;
		uint8_t* buffer;
		buffer = new uint8_t[bufferSize];

		volStream.seekg(0, ios_base::beg);
		volStream.read((char*)buffer, bufferSize);

		// Check if this volume is FAT32
		string signature;
		signature.resize(8);
		for (size_t i = 0; i < 8; ++i) {
			signature[i] = buffer[0x52 + i];
		}
		
		if (signature != "FAT32   ") {
			volState = StreamState::NOT_FAT32;
		}
		else {
			// Boot Sector: Get metadata from the buffer
			this->ByteSector = 0;
			this->ByteSector |= buffer[0xB];
			this->ByteSector |= buffer[0xC] << 8;

			this->SectorCluster = 0;
			this->SectorCluster |= buffer[0xD];

			this->SectorBootSector = 0;
			this->SectorBootSector |= buffer[0xE];
			this->SectorBootSector |= buffer[0xF] << 8;

			this->NumFAT = 0;
			this->NumFAT |= buffer[0x10];

			this->SizeFAT = 0;
			this->SizeFAT |= buffer[0x24];
			this->SizeFAT |= buffer[0x25] << 8;
			this->SizeFAT |= buffer[0x26] << 16;
			this->SizeFAT |= buffer[0x27] << 24;

			this->StartClusterRDET = 0;
			this->StartClusterRDET |= buffer[0x2C];
			this->StartClusterRDET |= buffer[0x2D] << 8;
			this->StartClusterRDET |= buffer[0x2E] << 16;
			this->StartClusterRDET |= buffer[0x2F] << 24;

			// FAT: Read FAT into RAM
			this->FAT = new uint32_t[this->SizeFAT];
			volStream.seekg(this->SectorBootSector * this->ByteSector, ios_base::beg);
			volStream.read((char*)this->FAT, this->SizeFAT);

			volStream.close();
		}

		delete[] buffer;
		buffer = nullptr;
	}
	
	return volState;
}

uint64_t Volume::getOffset(uint32_t const& cluster) const
{
	return ((uint64_t)this->SectorBootSector + (uint64_t)this->NumFAT * (uint64_t)this->SizeFAT + (uint64_t)(cluster - 2) * (uint64_t)this->SectorCluster) * (uint64_t)this->ByteSector;
}

vector<uint64_t> Volume::getOffsetList(uint32_t const& startCluster) const
{
	vector<uint32_t> clusterList = this->getClusterList(startCluster);

	vector<uint64_t> offsetList;
	offsetList.resize(clusterList.size());

	for (size_t i = 0; i < offsetList.size(); ++i) {
		offsetList[i] = this->getOffset(clusterList[i]);
	}

	return offsetList;
}

vector<uint32_t> Volume::getClusterList(uint32_t const& startCluster) const
{
	vector<uint32_t> clusterList;

	if (this->isUsedCluster(startCluster) || this->isEofCluster(startCluster)) {
		clusterList.push_back(startCluster);

		uint32_t preCluster = startCluster;
		while (!this->isEofCluster(preCluster)) {
			preCluster = this->FAT[preCluster];
			clusterList.push_back(preCluster);
		}
	}

	return clusterList;
}

bool Volume::isFreeCluster(uint32_t const& cluster) const
{
	return this->FAT[cluster] == 0;
}

bool Volume::isBadCluster(uint32_t const& cluster) const
{
	return this->FAT[cluster] == 0x0FFFFFF7;
}

bool Volume::isEofCluster(uint32_t const& cluster) const
{
	return this->FAT[cluster] == 0x0FFFFFFF;
}

bool Volume::isUsedCluster(uint32_t const& cluster) const
{
	return this->FAT[cluster] >= 2 && this->FAT[cluster] <= 0x0FFFFFEF;
}

EntryState Volume::findEntry(ifstream& volStream, wstring const& entryName, uint32_t& startCluster) const
{
	if (entryName.length() == 0) {
		return EntryState::NOT_FOUND;
	}
	
	// Read RDET/SDET into buffer
	vector<uint64_t> offsetList = this->getOffsetList(startCluster);

	uint32_t const CLUSTER_SIZE = this->SectorCluster * this->ByteSector;
	uint32_t const BUFFER_SIZE = CLUSTER_SIZE * offsetList.size();
	
	uint8_t* buffer = new uint8_t[BUFFER_SIZE];

	for (size_t i = 0; i < offsetList.size(); ++i) {
		volStream.seekg(offsetList[i], ios_base::beg);
		volStream.read((char*)(buffer + i * CLUSTER_SIZE), CLUSTER_SIZE);
	}

	// Find the entry whose name is entryName on this buffer
	EntryState entryState = EntryState::NOT_FOUND;

	uint8_t numSecondaryEntry = (entryName.length() + 12) / 13;

	for (uint64_t pos = 0x20; pos < BUFFER_SIZE; pos += 0x20) {
		if (buffer[pos] == 0xE5) {
			continue;
		}
		
		if (buffer[pos] == 0x00) {
			uint8_t check = 0;
			for (size_t i = 0; i < 0x20; ++i) {
				check |= buffer[pos + i];
			}

			if (check == 0) {
				break;
			}
		}
		
		if (buffer[pos + 0xB] == 0x0F) {
			if (buffer[pos] == (0x40 | numSecondaryEntry)) {
				if (entryName == this->getEntryLongName(buffer, pos, numSecondaryEntry)) {
					if (this->isFolder(buffer, pos + numSecondaryEntry * 0x20)) {
						entryState = EntryState::IS_FOLDER;
					}
					else {
						entryState = EntryState::IS_FILE;
					}

					startCluster = this->getStartCluster(buffer, pos + numSecondaryEntry * 0x20);
					break;
				}
			}

			// 0x45 & 0x3F = 0x05
			// 0100 0101 & 0011 1111
			pos += (buffer[pos] & 0x3F) * 0x20;
		}
		else if (entryName.length() <= 8) {
			string shortEntryName = this->convertToShortName(entryName);

			if (shortEntryName == "") {
				break;
			}
			
			if (shortEntryName == this->getEntryShortName(buffer, pos)) {
				if (this->isFolder(buffer, pos)) {
					entryState = EntryState::IS_FOLDER;
				}
				else {
					entryState = EntryState::IS_FILE;
				}

				startCluster = this->getStartCluster(buffer, pos);
				break;
			}
		}
	}

	delete[] buffer;
	return entryState;
}

wstring Volume::getEntryLongName(uint8_t* buffer, uint64_t const& offsetSecondaryEntry, uint8_t const& numSecondaryEntry) const
{
	wstring longName(numSecondaryEntry * 13, 0);
	uint64_t pos = offsetSecondaryEntry + (numSecondaryEntry - 1) * 0x20;
	size_t nameIdx = 0;

	for (uint8_t i = 0; i < numSecondaryEntry; ++i) {
		for (size_t j = 0; j < 5; ++j) {
			longName[nameIdx] |= buffer[pos + 0x1 + j * 2];
			longName[nameIdx] |= buffer[pos + 0x1 + j * 2 + 1] << 8;

			if (longName[nameIdx] == 0x0000) {
				longName.resize(nameIdx);
				longName.shrink_to_fit();
				return longName;
			}
			++nameIdx;
		}

		for (size_t j = 0; j < 6; ++j) {
			longName[nameIdx] |= buffer[pos + 0xE + j * 2];
			longName[nameIdx] |= buffer[pos + 0xE + j * 2 + 1] << 8;

			if (longName[nameIdx] == 0x0000) {
				longName.resize(nameIdx);
				longName.shrink_to_fit();
				return longName;
			}
			++nameIdx;
		}

		for (size_t j = 0; j < 2; ++j) {
			longName[nameIdx] |= buffer[pos + 0x1C + j * 2];
			longName[nameIdx] |= buffer[pos + 0x1C + j * 2 + 1] << 8;

			if (longName[nameIdx] == 0x0000) {
				longName.resize(nameIdx);
				longName.shrink_to_fit();
				return longName;
			}
			++nameIdx;
		}

		pos -= 0x20;
	}

	longName.resize(nameIdx);
	longName.shrink_to_fit();

	return longName;
}

string Volume::getEntryShortName(uint8_t* buffer, uint64_t const& offsetPrimaryEntry) const
{
	string shortName;

	for (size_t i = 0; i < 8; ++i) {
		if (buffer[offsetPrimaryEntry + i] == 0x20) {
			break;
		}
		shortName.push_back(buffer[offsetPrimaryEntry + i]);
	}

	if (buffer[offsetPrimaryEntry + 8] != 0x20) {
		shortName += '.';
	}

	for (size_t i = 8; i < 11; ++i) {
		if (buffer[offsetPrimaryEntry + i] == 0x20) {
			break;
		}
		shortName.push_back(buffer[offsetPrimaryEntry + i]);
	}

	return shortName;
}

string Volume::convertToShortName(wstring const& longName) const
{
	string shortName = "";

	for (auto wc : longName) {
		if (wc > 0x0020 && wc <= 0x007E) {
			shortName.push_back((char)wc);
			if (shortName.back() >= 'a' && shortName.back() <= 'z') {
				shortName.back() -= 'a' - 'A';
			}
		}
		else {		// Sure that the entry whose has this longName has secondary entry(s)
			return "";
		}
	}

	return shortName;
}

uint32_t Volume::getStartCluster(uint8_t* buffer, uint64_t const& offsetPrimaryEntry) const
{
	uint32_t high = 0;
	uint32_t low = 0;

	high |= buffer[offsetPrimaryEntry + 0x14];
	high |= buffer[offsetPrimaryEntry + 0x15] << 8;

	low |= buffer[offsetPrimaryEntry + 0x1A];
	low |= buffer[offsetPrimaryEntry + 0x1B] << 8;

	return (high << 16) | low;
}

bool Volume::isFolder(uint8_t* buffer, uint64_t const& offsetPrimaryEntry) const
{
	return (buffer[offsetPrimaryEntry + 0xB] & 0x10) == 0x10;
}

StreamState Volume::accessData(vector<wstring> entryNameList, vector<uint32_t>& clusterListFile, vector<uint64_t>& startOffsetList, vector<uint64_t>& endOffsetList) const
{
	if (entryNameList.empty()) {
		return StreamState::BAD_PATH;
	}
	
	ifstream volStream(this->Path, ios_base::binary);

	if (volStream.is_open()) {
		wstring fileName = entryNameList.back();
		entryNameList.pop_back();

		// Find all entry's names in RDET/SDETs. Make sure that all of these entries are folder
		uint32_t startCluster = this->StartClusterRDET;

		for (auto entryName : entryNameList) {
			if (this->findEntry(volStream, entryName, startCluster) != EntryState::IS_FOLDER) {
				volStream.close();
				return StreamState::BAD_PATH;
			}
		}

		// Find fileName
		EntryState entryState = this->findEntry(volStream, fileName, startCluster);
		
		if (entryState == EntryState::IS_FOLDER) {
			volStream.close();
			return StreamState::NOT_FILE;
		}
		
		if (entryState == EntryState::NOT_FOUND) {
			volStream.close();
			return StreamState::BAD_PATH;
		}

		// If entryState is IS_FILE ...
		clusterListFile = this->getClusterList(startCluster);
		startOffsetList = this->getOffsetList(startCluster);
		
		vector<uint64_t> tempList;
		for (size_t i = 0; i < startOffsetList.size(); ++i) {
			tempList.push_back(startOffsetList[i] + this->SectorCluster * this->ByteSector);
		}
		endOffsetList = tempList;

		volStream.close();
	}

	return StreamState::SUCCESS;
}

