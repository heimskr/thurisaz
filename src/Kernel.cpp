#include <cerrno>
#include <cstdarg>

#include "Kernel.h"
#include "Print.h"
#include "util.h"
#include "wasm/BinaryParser.h"

Kernel *global_kernel = nullptr;
long keybrd_index = 0;
unsigned long keybrd_queue[16] = {0};
bool timer_expired = false;

void __attribute__((noreturn)) Kernel::panic(const std::string &message) {
	panic(message.c_str());
}

void __attribute__((noreturn)) Kernel::panic(const char *message) {
	strprint("\e[2m[\e[22;31mPANIC\e[39;2m]\e[22m ");
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
		if (msize <= path.size() && path.substr(0, msize) == mountpoint && largest_length < msize) {
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

void Kernel::startProcess(const std::string *text) {
	strprint("Instantiating parser.\n");
	std::unique_ptr<Wasmc::BinaryParser> parser = std::make_unique<Wasmc::BinaryParser>(*text);
	delete text;
	strprint("Parsing.\n");
	parser->parse();
	strprint("Calculating.\n");
	const size_t code_offset = Paging::PageSize;
	const size_t code_length = parser->getCodeLength();
	const size_t data_length = parser->getDataLength();
	const size_t data_offset = code_offset + upalign(parser->getDataOffset(), Paging::PageSize);
	const size_t pages_needed = updiv(data_offset + data_length, Paging::PageSize)
		+ Kernel::PROCESS_STACK_PAGES + Kernel::PROCESS_DATA_PAGES;
	// We start here instead of 0 so that segfaults are more easily catchable.
	const ptrdiff_t virtual_start = 16 * Paging::PageSize;
	strprint("Allocating free physical addresses.\n");
	void *start = tables.allocateFreePhysicalAddress(pages_needed);
	if (!start)
		Kernel::panicf("Couldn't allocate %lu pages", pages_needed);

	parser->applyRelocation(virtual_start + code_offset, virtual_start + data_offset);

	strprint("Copying code.\n");
	for (size_t i = 0, max = parser->rawCode.size(); i < max; ++i)
		*((uint64_t *) ((char *) start + tables.pmmStart + code_offset) + i) = parser->rawCode[i];

	strprint("Copying data.\n");
	for (size_t i = 0, max = parser->rawData.size(); i < max; ++i)
		*((uint64_t *) ((char *) start + tables.pmmStart + data_offset) + i) = parser->rawData[i];

	strprint("Resetting parser.\n");
	parser.reset();

	Paging::Table *table_array = nullptr;
	size_t table_count = Paging::getTableCount(pages_needed);
	const size_t table_bytes = sizeof(Paging::Table) * table_count;
	strprint("Allocating tables.\n");
	Paging::Table *table_base = new Paging::Table[table_count + 1];
	if (!table_base)
		Kernel::panicf("Couldn't allocate %lu bytes", table_bytes + Paging::TableSize);
	table_array = (Paging::Table *) upalign(uintptr_t(table_base), Paging::TableSize);
	asm("memset %0 x $0 -> %1" :: "r"(table_bytes), "r"(table_array));
	Paging::Entry *p0 = table_array[0];

	strprint("Translating.\n");
	void *translated;
	asm("translate %1 -> %0" : "=r"(translated) : "r"(table_array));

	const uintptr_t code_end = upalign(code_offset + code_length, Paging::PageSize);
	const uintptr_t data_end = upalign(data_offset + data_length, Paging::PageSize);

	strprint("Creating wrapper.\n");
	Paging::Tables wrapper((Paging::Table *) translated, tables.bitmap, tables.pageCount);
	wrapper.setStarts((void *) code_offset, (void *) data_offset).setPMM(tables.pmmStart);

	uintptr_t physical;

	strprint("Assigning code.\n");
	for (physical = code_offset; physical < code_end; physical += Paging::PageSize)
		wrapper.assign((void *) (virtual_start + physical), (char *) start + physical, Paging::UserPage);

	strprint("Assigning data.\n");
	for (physical = data_offset; physical < data_end; physical += Paging::PageSize)
		wrapper.assign((void *) (virtual_start + physical), (char *) start + physical, Paging::UserPage);

	uintptr_t high;
	asm("$0 - %1 -> %0" : "=r"(high) : "r"(Paging::PageSize));
	uintptr_t global_start = virtual_start + data_end;

	strprint("Assigning stack.\n");
	for (size_t i = 0; i < Kernel::PROCESS_STACK_PAGES; ++i, physical += Paging::PageSize)
		wrapper.assign((void *) (high - i * Paging::PageSize), (char *) start + physical, Paging::UserPage);

	strprint("Assigning free area.\n");
	for (size_t i = 0; i < Kernel::PROCESS_DATA_PAGES; ++i, physical += Paging::PageSize)
		wrapper.assign((void *) (global_start + i * Paging::PageSize), (char *) start + physical, Paging::UserPage);

	long pid = getPID();
	if (pid < 0)
		Kernel::panicf("Invalid pid: %ld", pid);

	strprint("Creating process.\n");
	processes.try_emplace(pid, pid, table_base, table_count, std::move(wrapper), start, pages_needed);
	asm("translate %1 -> %0" : "=r"(translated) : "r"(p0));

	strprint("Preparing to jump.\n");
	asm("%0 -> $k0" :: "r"(pid));
	asm("%0 -> $ke" :: "r"(virtual_start + code_offset));
	asm("%0 -> $k4" :: "r"(tables.p0.entries));
	asm("$sp -> $k1");
	asm("%0 -> $g" :: "r"(global_start));
	asm("$ke -> $rt");
	asm("0xfffffff8 -> $sp");
	asm("lui: 0xffffffff -> $sp");
	asm("<p \"Jumping.\\n\">");
	asm(": %%setpt %0 $rt" :: "r"(translated));
	Kernel::panic("Start process failed: jump didn't do anything");
}

void Kernel::terminateProcess(long pid) {
	if (processes.count(pid) == 0)
		panicf("Can't terminate %ld: process not found", pid);
	ProcessData &process = processes.at(pid);

	const size_t index = uintptr_t(process.physicalStart) / Paging::PageSize;
	for (size_t i = 0; i < process.pagesNeeded; ++i)
		tables.mark(index + i, false);

	delete[] process.tableBase;

	printf("Terminated %ld.\n", pid);
	processes.erase(pid);
}

void Kernel::loop() {
	strprint("\e[32m$\e[39;1m ");

	for (;;) {
		if (timer_expired) {
			timer_expired = false;
			timer.onExpire();
		}

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
		asm("<rest>");
	}
}

void Kernel::timerCallback() {

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

extern "C" void timer_callback() {
	if (!global_kernel)
		Kernel::panic("Can't handle timer: no global kernel");
	global_kernel->timerCallback();
}
