#include "fs/FS.h"

namespace FS {
	bool Driver::operator==(const Driver &other) const {
		return this == &other || *partition == *other.partition;
	}
}