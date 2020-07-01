#pragma once

#include "Volume.h"

#include <fcntl.h>
#include <io.h>
#include <Windows.h>

class Program
{
public:
	Program();
	~Program();
	void run();

private:
	Volume* Vol;
private:
	StreamState accessData(wstring& filePath, vector<uint32_t>& clusterList, vector<uint64_t>& startOffsetList, vector<uint64_t>& endOffsetList);
	void gotoXY(size_t const& xCor, size_t const& yCor) const;
	size_t whereX() const;
	size_t whereY() const;
};

