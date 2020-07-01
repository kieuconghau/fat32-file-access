#pragma once

#include <iostream>
#include <vector>
#include <string>
#include <fstream>
using namespace std;

enum class StreamState {
	SUCCESS,
	NOT_FILE,
	NOT_FAT32,
	NOT_VOLUME,
	BAD_PATH
};

enum class EntryState {
	IS_FILE,
	IS_FOLDER,
	NOT_FOUND
};

class Volume
{
public:
	static wchar_t const DELIMITER;
public:
	Volume(wstring const& letter);	// 'letter' is "C:", "D:", "G:",...
	~Volume();
	StreamState isOpen();
	StreamState accessData(vector<wstring> entryNameList, vector<uint32_t>& clusterList, vector<uint64_t>& startOffsetList, vector<uint64_t>& endOffsetList) const;

private:
	wstring   Path;					// \\\\.\\X:
	uint16_t  ByteSector;
	uint8_t   SectorCluster;
	uint16_t  SectorBootSector;
	uint8_t   NumFAT;
	uint32_t  SizeFAT;
	uint32_t  StartClusterRDET;
	uint32_t* FAT;
private:
	uint64_t getOffset(uint32_t const& cluster) const;
	vector<uint64_t> getOffsetList(uint32_t const& startCluster) const;
	vector<uint32_t> getClusterList(uint32_t const& startCluster) const;

	bool isFreeCluster(uint32_t const& cluster) const;
	bool isBadCluster(uint32_t const& cluster) const;
	bool isEofCluster(uint32_t const& cluster) const;
	bool isUsedCluster(uint32_t const& cluster) const;

	EntryState findEntry(ifstream& volStream, wstring const& fileName, uint32_t& startCluster) const;
	wstring getEntryLongName(uint8_t* buffer, uint64_t const& offsetSecondaryEntry, uint8_t const& numSecondaryEntry) const;
	string getEntryShortName(uint8_t* buffer, uint64_t const& offsetPrimaryEntry) const;
	string convertToShortName(wstring const& longName) const;
	uint32_t getStartCluster(uint8_t* buffer, uint64_t const& offsetPrimaryEntry) const;
	bool isFolder(uint8_t* buffer, uint64_t const& offsetPrimaryEntry) const;
};

