#include "fs/tfat/ThornFAT.h"
#include "printf.h"
#include "util.h"

namespace Armaz::ThornFAT {
	DirEntry::DirEntry(const Times &times_, size_t length_, FileType type_):
		times(times_), length(length_), type(type_) {}

	DirEntry::DirEntry(const DirEntry &other) {
		memcpy(name.str, other.name.str, sizeof(name.str));
		times = other.times;
		length = other.length;
		startBlock = other.startBlock;
		type = other.type;
		modes = other.modes;
		uid = other.uid;
		gid = other.gid;
	}

	DirEntry & DirEntry::operator=(const DirEntry &other) {
		memcpy(name.str, other.name.str, sizeof(name.str));
		times = other.times;
		length = other.length;
		startBlock = other.startBlock;
		type = other.type;
		modes = other.modes;
		uid = other.uid;
		gid = other.gid;
		return *this;
	}

	void DirEntry::reset() {
		memset(name.str, 0, sizeof(name));
		times = {0, 0, 0};
		length = 0;
		startBlock = UNUSABLE;
		type = FileType::File;
		modes = 0;
		uid = 0;
		gid = 0;
	}

	DirEntry::operator std::string() const {
		return "DirEntry[name=" + std::string(name.str) + ", length=" + std::to_string(length) + ", startBlock=" +
			std::to_string(startBlock) + ", type=" + std::to_string((int) type) + ", modes=" + std::to_string(modes) +
			", uid=" + std::to_string(uid) + ", gid=" + std::to_string(gid) + "]";
	}

	void DirEntry::print() const {
		printf("%s\n", std::string(*this).c_str());
	}
}
