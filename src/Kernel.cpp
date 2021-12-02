#include <cerrno>
#include <cstdarg>

#include "Kernel.h"
#include "Print.h"

void __attribute__((noreturn)) Kernel::panic(const std::string &message) {
	panic(message.c_str());
}

void __attribute__((noreturn)) Kernel::panic(const char *message) {
	strprint(message);
	for (;;)
		asm("<halt>");
}

void __attribute__((noreturn)) Kernel::panicf(const char *format, ...) {
	strprint("\e[2m[\e[22;31mPANIC\e[39;2m]\e[22m ");
	va_list var;
	va_start(var, format);
	vprintf(format, var);
	va_end(var);
	prc('\n');
	for (;;)
		asm("<halt>");
}

bool Kernel::getDriver(const std::string &path, std::string &relative_out, std::shared_ptr<FS::Driver> &driver_out) {
	for (const auto &[mountpoint, driver]: mounts)
		if (mountpoint.size() <= path.size() && path.substr(0, mountpoint.size()) == mountpoint) {
			relative_out = path.substr(mountpoint.size());
			driver_out = driver;
			return true;
		}
	return false;
}

bool Kernel::mount(const std::string &path, std::shared_ptr<FS::Driver> driver) {
	const std::string simplified = FS::simplifyPath(path);
	const size_t size = simplified.size();
	for (const auto &[mountpoint, driver]: mounts) {
		const size_t msize = mountpoint.size();
		if ((msize < size && simplified.substr(0, msize) == mountpoint) || (msize == size && simplified == mountpoint))
			return false;
	}
	mounts.emplace(simplified, driver);
	return true;
}

int Kernel::rename(const char *path, const char *newpath) {
	std::string relative, path_str(path);
	std::shared_ptr<FS::Driver> driver;
	if (getDriver(path_str, relative, driver))
		return driver->rename(relative.c_str(), std::string(newpath).substr(path_str.size() - relative.size()).c_str());
	return -ENODEV;
}

int Kernel::release(const char *path) {
	std::string relative, path_str(path);
	std::shared_ptr<FS::Driver> driver;
	if (getDriver(path_str, relative, driver))
		return driver->release(relative.c_str());
	return -ENODEV;
}

int Kernel::statfs(const char *path, FS::DriverStats &stats) {
	std::string relative, path_str(path);
	std::shared_ptr<FS::Driver> driver;
	if (getDriver(path_str, relative, driver))
		return driver->statfs(relative.c_str(), stats);
	return -ENODEV;

}

int Kernel::utimens(const char *path, const timespec &ts) {
	std::string relative, path_str(path);
	std::shared_ptr<FS::Driver> driver;
	if (getDriver(path_str, relative, driver))
		return driver->utimens(relative.c_str(), ts);
	return -ENODEV;
}

int Kernel::create(const char *path, mode_t mode, uid_t uid, gid_t gid) {
	std::string relative, path_str(path);
	std::shared_ptr<FS::Driver> driver;
	if (getDriver(path_str, relative, driver))
		return driver->create(relative.c_str(), mode, uid, gid);
	return -ENODEV;
}

int Kernel::write(const char *path, const char *buffer, size_t size, off_t offset) {
	std::string relative, path_str(path);
	std::shared_ptr<FS::Driver> driver;
	if (getDriver(path_str, relative, driver))
		return driver->write(relative.c_str(), buffer, size, offset);
	return -ENODEV;
}

int Kernel::mkdir(const char *path, mode_t mode, uid_t uid, gid_t gid) {
	std::string relative, path_str(path);
	std::shared_ptr<FS::Driver> driver;
	if (getDriver(path_str, relative, driver))
		return driver->mkdir(relative.c_str(), mode, uid, gid);
	return -ENODEV;
}

int Kernel::truncate(const char *path, off_t size) {
	std::string relative, path_str(path);
	std::shared_ptr<FS::Driver> driver;
	if (getDriver(path_str, relative, driver))
		return driver->truncate(relative.c_str(), size);
	return -ENODEV;
}

int Kernel::rmdir(const char *path, bool recursive) {
	std::string relative, path_str(path);
	std::shared_ptr<FS::Driver> driver;
	if (getDriver(path_str, relative, driver))
		return driver->rmdir(relative.c_str(), recursive);
	return -ENODEV;
}

int Kernel::unlink(const char *path) {
	std::string relative, path_str(path);
	std::shared_ptr<FS::Driver> driver;
	if (getDriver(path_str, relative, driver))
		return driver->unlink(relative.c_str());
	return -ENODEV;
}

int Kernel::open(const char *path) {
	std::string relative, path_str(path);
	std::shared_ptr<FS::Driver> driver;
	if (getDriver(path_str, relative, driver))
		return driver->open(relative.c_str());
	return -ENODEV;
}

int Kernel::read(const char *path, void *buffer, size_t size, off_t offset) {
	std::string relative, path_str(path);
	std::shared_ptr<FS::Driver> driver;
	if (getDriver(path_str, relative, driver))
		return driver->read(relative.c_str(), buffer, size, offset);
	return -ENODEV;
}

int Kernel::readdir(const char *path, FS::DirFiller filler) {
	std::string relative, path_str(path);
	std::shared_ptr<FS::Driver> driver;
	if (getDriver(path_str, relative, driver))
		return driver->readdir(relative.c_str(), filler);
	return -ENODEV;
}

int Kernel::getattr(const char *path, FS::FileStats &stats) {
	std::string relative, path_str(path);
	std::shared_ptr<FS::Driver> driver;
	if (getDriver(path_str, relative, driver))
		return driver->getattr(relative.c_str(), stats);
	return -ENODEV;
}

int Kernel::getsize(const char *path, size_t &out) {
	std::string relative, path_str(path);
	std::shared_ptr<FS::Driver> driver;
	if (getDriver(path_str, relative, driver))
		return driver->getsize(relative.c_str(), out);
	return -ENODEV;
}

int Kernel::isdir(const char *path) {
	std::string relative, path_str(path);
	std::shared_ptr<FS::Driver> driver;
	if (getDriver(path_str, relative, driver))
		return driver->isdir(relative.c_str())? 1 : 0;
	return -ENODEV;
}

int Kernel::isfile(const char *path) {
	std::string relative, path_str(path);
	std::shared_ptr<FS::Driver> driver;
	if (getDriver(path_str, relative, driver))
		return driver->isfile(relative.c_str())? 1 : 0;
	return -ENODEV;
}

/** Returns 0 if the file exists or a negative error code otherwise. */
int Kernel::exists(const char *path) {
	std::string relative, path_str(path);
	std::shared_ptr<FS::Driver> driver;
	if (getDriver(path_str, relative, driver))
		return driver->exists(relative.c_str());
	return -ENODEV;
}
