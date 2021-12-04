#include "Commands.h"
#include "Kernel.h"
#include "Print.h"
#include "util.h"
#include "fs/tfat/ThornFAT.h"
#include "fs/tfat/Util.h"
#include "storage/WhyDevice.h"
#include "wasm/BinaryParser.h"

namespace Thurisaz {
	int runCommand(const std::map<std::string, Command> &commands, Context &context,
	               const std::vector<std::string> &pieces) {
		if (!pieces.empty()) {
			if (commands.count(pieces.front()) == 0)
				return Command::NOT_FOUND;
			const Command &command = commands.at(pieces.front());
			if (!command.valid(pieces.size() - 1)) {
				if (command.usage && command.usage[0] == '\0')
					printf("Usage: %s\n", pieces.front().c_str());
				else if (command.usage)
					printf("Usage: %s %s\n", pieces.front().c_str(), command.usage);
				else
					strprint("Invalid number of arguments.\n");
			} else if (command.driverNeeded && !context.driver)
				strprint("Driver not initialized. Use \e[3mdriver\e[23m.\n");
			else if (command.deviceNeeded && (!context.device || context.selectedDrive < 0))
				strprint("No device selected. Use \e[3mselect\e[23m.\n");
			else
				return command(context, pieces);
		}

		return Command::BAD_ARGUMENTS;
	}

	void addCommands(std::map<std::string, Command> &commands) {
		commands.try_emplace("drives", 0, 0, [](Context &, const std::vector<std::string> &) -> long {
			printf("Number of drives: %lu\n", WhyDevice::count());
			return 0;
		});

		commands.emplace("count", commands.at("drives"));

		commands.try_emplace("drive", 0, 0, [](Context &context, const std::vector<std::string> &) -> long {
			if (context.selectedDrive == -1) {
				strprint("No drive selected.\n");
				return 1;
			}

			printf("Selected drive: %ld\n", context.selectedDrive);
			return 0;
		});

		commands.try_emplace("select", 1, 1, [](Context &context, const std::vector<std::string> &pieces) -> long {
			if (!parseLong(pieces[1], context.selectedDrive) || context.selectedDrive < 0 ||
				context.driveCount <= context.selectedDrive) {
				strprint("Invalid drive ID.\n");
				context.selectedDrive = -1;
				return 1;
			}

			context.device = std::make_shared<WhyDevice>(context.selectedDrive);
			printf("Selected drive %ld.\n", context.selectedDrive);
			return 0;
		}, "<drive>");

		commands.try_emplace("getsize", 1, 2, [](Context &context, const std::vector<std::string> &pieces) -> long {
			long drive = context.selectedDrive;
			if (pieces.size() == 2 && (!parseLong(pieces[1], drive) || drive < 0 || context.driveCount <= drive)) {
				strprint("Invalid drive ID.\n");
				return 1;
			}
			long e0, r0;
			asm("%2 -> $a1    \n\
			     <io getsize> \n\
			     $e0 -> %0    \n\
			     $r0 -> %1    " : "=r"(e0), "=r"(r0) : "r"(drive));
			if (e0 != 0) {
				printf("getsize failed: %ld\n", e0);
				return e0;
			}

			printf("Size of drive %ld: %lu byte%s\n", drive, r0, r0 == 1? "" : "s");
			return 0;
		}, "[drive]");

		commands.emplace("make", Command(0, 0, [](Context &context, const std::vector<std::string> &pieces) -> long {
			context.partition = std::make_shared<Partition>(context.device, 0, context.device->size());
			context.driver = std::make_shared<ThornFAT::ThornFATDriver>(context.partition);
			strprint("ThornFAT driver instantiated.\n");
			const bool success = context.driver->make(sizeof(ThornFAT::DirEntry) * 5);
			printf("ThornFAT creation %s.\n", success? "succeeded" : "failed");
			return success? 0 : 1;
		}, "").setDeviceNeeded());

		commands.emplace("driver", Command(0, 0, [](Context &context, const std::vector<std::string> &pieces) -> long {
			context.partition = std::make_shared<Partition>(context.device, 0, context.device->size());
			context.driver = std::make_shared<ThornFAT::ThornFATDriver>(context.partition);
			context.driver->getRoot(nullptr, true);
			strprint("ThornFAT driver instantiated.\n");
			return 0;
		}, "").setDeviceNeeded());

		commands.try_emplace("ls", 0, 1, [](Context &context, const std::vector<std::string> &pieces) -> long {
			const std::string path = 1 < pieces.size()? FS::simplifyPath(context.cwd, pieces[1]) : context.cwd;
			const int status = context.kernel.readdir(path.c_str(), [](const char *item, off_t) {
				printf("%s\n", item);
			});
			if (status != 0)
				printf("Error: %ld\n", -long(status));
			return -status;
		}, "[path]");

		commands.try_emplace("create", 1, 1, [](Context &context, const std::vector<std::string> &pieces) -> long {
			const int status = context.kernel.create(FS::simplifyPath(context.cwd, pieces[1]).c_str(), 0666, 0, 0);
			if (status != 0)
				printf("create error: %ld\n", -long(status));
			return -status;
		}, "<path>");

		commands.try_emplace("write", 1, -1, [](Context &context, const std::vector<std::string> &pieces) -> long {
			const std::string path = FS::simplifyPath(context.cwd, pieces[1]);
			std::string data = pieces.size() == 2? "" : pieces[2];
			for (size_t i = 3; i < pieces.size(); ++i)
				data += " " + pieces[i];

			ssize_t status = context.kernel.truncate(path.c_str(), data.size());
			if (status != 0) {
				printf("truncate error: %ld\n", -long(status));
			} else {
				status = context.kernel.write(path.c_str(), data.c_str(), data.size(), 0);
				if (status < 0) {
					printf("write error: %ld\n", -long(status));
					return -status;
				}
			}

			return 0;
		}, "<path> [data...]");

		commands.try_emplace("read", 1, 1, [](Context &context, const std::vector<std::string> &pieces) -> long {
			const std::string path = pieces[1][0] == '/'? pieces[1] : FS::simplifyPath(context.cwd,
				pieces[1]);
			size_t size;
			ssize_t status = context.kernel.getsize(path.c_str(), size);
			if (status != 0) {
				printf("getsize failed: %ld\n", -status);
				return -status;
			}

			std::string data;
			data.resize(size);
			status = context.kernel.read(path.c_str(), &data[0], size, 0);
			if (status < 0) {
				printf("read failed: %ld\n", -status);
				return -status;
			}

			if (status == 0)
				printf("File %s is empty.\n", path.c_str());
			else
				printf("Read %lu byte%s:\n%s\n", status, status == 1? "" : "s", data.c_str());

			return 0;
		}, "<path>");

		commands.try_emplace("run", 1, 1, [](Context &context, const std::vector<std::string> &pieces) -> long {
			const std::string path = FS::simplifyPath(context.cwd, pieces[1]);
			size_t size;
			ssize_t status = context.kernel.getsize(path.c_str(), size);
			if (status != 0) {
				printf("getsize failed: %ld\n", -status);
				return -status;
			}

			std::string data;
			data.resize(size);
			status = context.kernel.read(path.c_str(), &data[0], size, 0);
			if (status < 0) {
				printf("read failed: %ld\n", -status);
				return -status;
			}

			Wasmc::BinaryParser parser(data);
			return 0;
		});

		commands.try_emplace("cd", 0, 1, [](Context &context, const std::vector<std::string> &pieces) -> long {
			const std::string path = FS::simplifyPath(context.cwd, pieces[1]);
			if (context.kernel.isdir(path.c_str())) {
				context.cwd = path;
				return 0;
			}

			printf("Cannot change directory to %s\n", path.c_str());
			return 1;
		}, "<path>");

		commands.try_emplace("pwd", -1, -1, [](Context &context, const std::vector<std::string> &pieces) -> long {
			printf("%s\n", context.cwd.c_str());
			return 0;
		}, "");

		commands.try_emplace("debug", 1, 1, [](Context &context, const std::vector<std::string> &pieces) -> long {
			if (pieces[1] != "off" && pieces[1] != "on") {
				strprint("Usage: debug <on|off>\n");
				return Command::BAD_ARGUMENTS;
			}

			if (pieces[1] == "off") {
				debug_enable = 0;
				debug_disable = 1;
			} else {
				debug_enable = 1;
				debug_disable = 0;
			}

			return 0;
		}, "<on|off>");

		commands.try_emplace("rm", 1, 1, [](Context &context, const std::vector<std::string> &pieces) -> long {
			const std::string path = FS::simplifyPath(context.cwd, pieces[1]);
			int status = context.kernel.exists(path.c_str());
			if (status != 0) {
				strprint("Path not found.\n");
				return 1;
			}

			if (context.kernel.isdir(path.c_str()))
				status = context.kernel.rmdir(path.c_str(), true);
			else
				status = context.kernel.unlink(path.c_str());

			if (status != 0)
				printf("Error: %d\n", -status);

			return -status;
		}, "<path>");

		commands.try_emplace("mkdir", 1, 1, [](Context &context, const std::vector<std::string> &pieces) -> long {
			const std::string path = FS::simplifyPath(context.cwd, pieces[1]);
			const int status = context.kernel.mkdir(path.c_str(), 0755, 0, 0);
			if (status != 0)
				printf("Error: %d\n", -status);
			return -status;
		}, "<path>");

		commands.try_emplace("mv", 2, 2, [](Context &context, const std::vector<std::string> &pieces) -> long {
			const std::string source = FS::simplifyPath(context.cwd, pieces[1]);
			const std::string destination = FS::simplifyPath(context.cwd, pieces[2]);
			const int status = context.kernel.rename(source.c_str(), destination.c_str());
			if (status != 0)
				printf("Error: %d\n", -status);
			return -status;
		}, "<source> <destination>");

		commands.try_emplace("sleep", 1, 1, [](Context &context, const std::vector<std::string> &pieces) -> long {
			uint64_t micros;
			if (!parseUlong(pieces[1], micros)) {
				strprint("Invalid number.\n");
				return 1;
			}

			asm("<sleep %0>" :: "r"(micros));
			return 0;
		}, "<microseconds>");

		commands.try_emplace("init", 0, 0, [&](Context &context, const std::vector<std::string> &pieces) -> long {
			return runCommand(commands, context, {"mount", "0", "/"});
		});

		commands.try_emplace("clear", 0, 0, [](Context &context, const std::vector<std::string> &pieces) -> long {
			strprint("\e[2J\e[3J\e[H");
			return 0;
		});

		commands.try_emplace("mount", 2, 2, [](Context &context, const std::vector<std::string> &pieces) -> long {
			unsigned long index;
			if (!parseUlong(pieces[1], index) || context.driveCount <= index) {
				strprint("Invalid device index.\n");
				return 1;
			}

			const std::string mountpoint = FS::simplifyPath(context.cwd, pieces[2]);
			auto device = std::make_shared<WhyDevice>(index);
			auto partition = std::make_shared<Partition>(device, 0, device->size());
			auto driver = std::make_shared<ThornFAT::ThornFATDriver>(partition);
			if (!context.kernel.mount(mountpoint, driver)) {
				printf("Mounting at %s failed.\n", mountpoint.c_str());
				return 1;
			}

			printf("Mounted device %lu at %s.\n", index, mountpoint.c_str());
			return 0;
		});

		commands.try_emplace("mounts", 0, 0, [](Context &context, const std::vector<std::string> &pieces) -> long {
			if (context.kernel.mounts.empty())
				strprint("Nothing is mounted.\n");
			else
				for (const auto &[mountpoint, driver]: context.kernel.mounts) {
					strprint(mountpoint.c_str());
					asm("<prc %0>" :: "r"('\n'));
				}
			return 0;
		});

		commands.try_emplace("unmount", 1, 1, [](Context &context, const std::vector<std::string> &pieces) -> long {
			if (context.kernel.unmount(pieces[1])) {
				printf("Unmounted %s.\n", pieces[1].c_str());
				return 0;
			}

			printf("Couldn't unmount %s.\n", pieces[1].c_str());
			return 1;
		});

		commands.try_emplace("halt", 0, 0, [](Context &context, const std::vector<std::string> &pieces) -> long {
			strprint("Halted.\n");
			asm("<halt>");
			return 0;
		});
	}
}
