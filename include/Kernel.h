#pragma once

#include <map>
#include <memory>
#include <string>

#include "fs/FS.h"

namespace Paging {
	class Tables;
}

struct Kernel {
	static void __attribute__((noreturn)) panic(const std::string &);
	static void __attribute__((noreturn)) panic(const char *);
	static void __attribute__((noreturn)) panicf(const char *, ...);

	std::map<std::string, std::shared_ptr<FS::Driver>> mounts;
	Paging::Tables &tables;

	Kernel() = delete;
	Kernel(const Kernel &) = delete;
	Kernel(Kernel &&) = delete;

	Kernel(Paging::Tables &tables_): tables(tables_) {}

	Kernel & operator=(const Kernel &) = delete;
	Kernel & operator=(Kernel &&) = delete;

	bool getDriver(const std::string &path, std::string &relative_out, std::shared_ptr<FS::Driver> &driver_out);
	bool mount(const std::string &, std::shared_ptr<FS::Driver>);
	bool unmount(const std::string &);

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
