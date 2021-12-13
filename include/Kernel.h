#pragma once

#include <map>
#include <memory>
#include <string>

#include "Commands.h"
#include "Paging.h"
#include "fs/FS.h"

struct ProcessData {
	long pid;
	/** Points to the address returned by new; the actual starting address of the tables will be upaligned to 2048. */
	Paging::Table *tableBase;
	size_t tableCount;
	Paging::Tables wrapper;
	void *physicalStart;
	size_t pagesNeeded;

	ProcessData(long pid_, Paging::Table *table_base, size_t table_count, const Paging::Tables &wrapper_,
	void *physical_start, size_t pages_needed):
		pid(pid_), tableBase(table_base), tableCount(table_count), wrapper(wrapper_), physicalStart(physical_start),
		pagesNeeded(pages_needed) {}

	ProcessData(long pid_, Paging::Table *table_base, size_t table_count, Paging::Tables &&wrapper_,
	void *physical_start, size_t pages_needed):
		pid(pid_), tableBase(table_base), tableCount(table_count), wrapper(std::move(wrapper_)),
		physicalStart(physical_start), pagesNeeded(pages_needed) {}
};

class Kernel;

extern Kernel *global_kernel;

extern "C" void terminate_process(long pid);
extern "C" void kernel_loop();
extern long keybrd_index;
extern unsigned long keybrd_queue[16];

class Kernel {
	private:
		std::string line;
		std::vector<std::string> pieces;

	public:
		static void __attribute__((noreturn)) panic(const std::string &);
		static void __attribute__((noreturn)) panic(const char *);
		static void __attribute__((noreturn)) panicf(const char *, ...);

		std::map<std::string, std::shared_ptr<FS::Driver>> mounts;
		std::map<long, ProcessData> processes;
		Paging::Tables &tables;
		Thurisaz::Context context = {*this};
		std::map<std::string, Thurisaz::Command> commands;

		Kernel() = delete;
		Kernel(const Kernel &) = delete;
		Kernel(Kernel &&) = delete;

		Kernel(Paging::Tables &tables_): tables(tables_) {
			global_kernel = this;
			Thurisaz::addCommands(commands);
			line.reserve(256);
		}

		Kernel & operator=(const Kernel &) = delete;
		Kernel & operator=(Kernel &&) = delete;

		bool getDriver(const std::string &path, std::string &relative_out, std::shared_ptr<FS::Driver> &driver_out);
		bool mount(const std::string &, std::shared_ptr<FS::Driver>);
		bool unmount(const std::string &);
		long getPID() const;
		void terminateProcess(long pid);
		void loop();

		int rename(const char *path, const char *newpath);
		int release(const char *path);
		int statfs(const char *path, FS::DriverStats &);
		int utimens(const char *path, const timespec &);
		int create(const char *path, mode_t, uid_t, gid_t);
		int write(const char *path, const char *buffer, size_t size, off_t offset);
		int mkdir(const char *path, mode_t, uid_t, gid_t);
		int truncate(const char *path, off_t size);
		int rmdir(const char *path, bool recursive = false);
		int unlink(const char *path);
		int open(const char *path);
		int read(const char *path, void *buffer, size_t size, off_t offset);
		int readdir(const char *path, FS::DirFiller filler);
		int getattr(const char *path, FS::FileStats &);
		int getsize(const char *path, size_t &out);
		/** Returns 1 if the path is a directory, 0 if it isn't or a negative error code if an error occurred. */
		int isdir(const char *path);
		/** Returns 1 if the path is a file, 0 if it isn't or a negative error code if an error occurred. */
		int isfile(const char *path);
		/** Returns 0 if the file exists or a negative error code otherwise. */
		int exists(const char *path);
};
