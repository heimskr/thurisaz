#include <cerrno>
#include <cstdarg>

#include "Kernel.h"
#include "Print.h"
#include "util.h"

Kernel *global_kernel = nullptr;
long keybrd_index = 0;
unsigned long keybrd_queue[16] = {0};

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
	size_t largest_length = 0;
	const std::shared_ptr<FS::Driver> *largest_driver = nullptr;
	for (const auto &[mountpoint, driver]: mounts) {
		const size_t msize = mountpoint.size();
		if (msize <= path.size() && path.substr(0, msize) == mountpoint)
			if (largest_length < msize) {
				largest_length = msize;
				largest_driver = &driver;
			}
	}

	if (largest_driver) {
		// When largest_length is 1, the path is "/".
		relative_out = largest_length == 1? path : path.substr(largest_length);
		driver_out = *largest_driver;
		return true;
	}

	return false;
}

bool Kernel::mount(const std::string &path, std::shared_ptr<FS::Driver> driver) {
	const std::string simplified = FS::simplifyPath(path);
	if (mounts.count(simplified) != 0)
		return false;
	for (const auto &[mountpoint, existing_driver]: mounts)
		if (*driver == *existing_driver)
			return false;
	mounts.emplace(simplified, driver);
	return true;
}

bool Kernel::unmount(const std::string &path) {
	const std::string simplified = FS::simplifyPath(path);
	if (mounts.count(simplified) != 0) {
		mounts.erase(simplified);
		return true;
	}

	const size_t size = simplified.size();
	for (const auto &[mountpoint, driver]: mounts) {
		const size_t msize = mountpoint.size();
		if (msize < size && simplified.substr(0, msize) == mountpoint) {
			mounts.erase(mountpoint);
			return true;
		}
	}

	return false;
}

long Kernel::getPID() const {
	long out;
	for (out = 0; processes.count(out) != 0 && 0 <= out; ++out);
	return out;
}

void Kernel::terminateProcess(long pid) {
	if (processes.count(pid) == 0)
		panicf("Can't terminate %ld: process not found", pid);
	processes.erase(pid);
	printf("Terminated %ld.\n", pid);
}

void Kernel::loop() {
	strprint("\e[32m$\e[39;1m ");

	for (;;) {
		asm("<rest>");
		for (long i = 0; i <= keybrd_index; ++i) {
			const long combined = keybrd_queue[i];
			const char key = combined & 0xff;
			long mask = 1l;
			mask <<= 32;
			// const bool shift = (combined & mask) != 0;
			// const bool alt   = (combined & (mask << 1)) != 0;
			const bool ctrl  = (combined & (mask << 2)) != 0;

			if (key == 'u' && ctrl) {
				line.clear();
				strprint("\e[2K\e[G\e[0;32m$\e[39;1m ");
			} else if (key == 0x7f) {
				if (!line.empty()) {
					strprint("\e[D \e[D");
					line.pop_back();
				}
			} else if (key == 0x0a) {
				if (!line.empty()) {
					strprint("\e[0m\n");
					pieces = split<std::vector>(line, " ", true);
					line.clear();
					const long status = Thurisaz::runCommand(commands, context, pieces);
					if (status == Thurisaz::Command::NOT_FOUND)
						strprint("Unknown command.\n");
					else if (status == Thurisaz::Command::EXIT_SHELL)
						return;
					if (status == 0)
						strprint("\e[32m$\e[39;1m ");
					else
						strprint("\e[31m$\e[39;1m ");
				}
			} else if (0x7f < key) {
				asm("<prx %0>" :: "r"(key));
			} else {
				line.push_back(key & 0xff);
				asm("<prc %0>" :: "r"(key & 0xff));
			}
		}

		keybrd_index = -1;
	}
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

extern "C" void terminate_process(long pid) {
	if (!global_kernel)
		Kernel::panic("Can't terminate process: no global kernel");
	global_kernel->terminateProcess(pid);
}

extern "C" void kernel_loop() {
	if (!global_kernel)
		Kernel::panic("Can't loop: no global kernel");
	global_kernel->loop();
}
