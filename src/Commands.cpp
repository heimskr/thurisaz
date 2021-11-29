#include "Commands.h"
#include "Kernel.h"
#include "Print.h"
#include "util.h"
#include "fs/tfat/ThornFAT.h"
#include "fs/tfat/Util.h"
#include "storage/MBR.h"
#include "storage/WhyDevice.h"

namespace Thurisaz {
	bool runCommand(const std::map<std::string, Command> &commands, Context &context,
	                const std::vector<std::string> &pieces) {
		if (!pieces.empty()) {
			if (commands.count(pieces.front()) == 0)
				return false;
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
			else if (command.mbrNeeded && !context.mbr)
				strprint("MBR hasn't been read yet. Use \e[3mreadmbr\e[23m.\n");
			else
				command(context, pieces);
			return true;
		} else
			return false;
	}

	void addCommands(std::map<std::string, Command> &commands) {
		commands.try_emplace("drives", 0, 0, [](Context &, const std::vector<std::string> &) {
			printf("Number of drives: %lu\n", WhyDevice::count());
		});

		commands.emplace("count", commands.at("drives"));

		commands.try_emplace("drive", 0, 0, [](Context &context, const std::vector<std::string> &) {
			if (context.selectedDrive == -1)
				strprint("No drive selected.\n");
			else
				printf("Selected drive: %ld\n", context.selectedDrive);
		});

		commands.try_emplace("select", 1, 1, [](Context &context, const std::vector<std::string> &pieces) {
			if (!parseLong(pieces[1], context.selectedDrive) || context.selectedDrive < 0 ||
				context.driveCount <= context.selectedDrive) {
				strprint("Invalid drive ID.\n");
				context.selectedDrive = -1;
			} else {
				printf("Selected drive %ld.\n", context.selectedDrive);
			}
		}, "<drive>");

		commands.try_emplace("getsize", 1, 2, [](Context &context, const std::vector<std::string> &pieces) {
			long drive = context.selectedDrive;
			if (pieces.size() == 2 && (!parseLong(pieces[1], drive) || drive < 0 || context.driveCount <= drive)) {
				strprint("Invalid drive ID.\n");
				return;
			}
			long e0, r0;
			asm("%2 -> $a1    \n\
			     <io getsize> \n\
			     $e0 -> %0    \n\
			     $r0 -> %1    " : "=r"(e0), "=r"(r0) : "r"(drive));
			if (e0 != 0)
				printf("getsize failed: %ld\n", e0);
			else
				printf("Size of drive %ld: %lu byte%s\n", drive, r0, r0 == 1? "" : "s");
		}, "[drive]");

		commands.try_emplace("readmbr", 0, 1, [](Context &context, const std::vector<std::string> &pieces) {
			long drive = context.selectedDrive;
			if (pieces.size() == 2 && (!parseLong(pieces[1], drive) || drive < 0 || context.driveCount <= drive)) {
				strprint("Invalid drive ID.\n");
				return;
			}
			context.device = std::make_unique<WhyDevice>(drive);
			if (!context.mbr)
				context.mbr = std::make_unique<MBR>();
			ssize_t status = context.device->read(context.mbr.get(), sizeof(MBR), 0);
			if (status < 0) {
				printf("Couldn't read disk: %ld\n", -status);
				return;
			}
			printf("Disk ID: 0x%02x%02x%02x%02x\n", context.mbr->diskID[3], context.mbr->diskID[2],
				context.mbr->diskID[1], context.mbr->diskID[0]);
			for (int i = 0; i < 4; ++i) {
				const MBREntry &entry = (&context.mbr->firstEntry)[i];
				printf("%d: attributes(0x%02x), type(0x%02x), startLBA(%u), sectors(%u) @ 0x%lx\n",
					i, entry.attributes, entry.type, entry.startLBA, entry.sectors, &entry);
			}
			printf("Signature: 0x%02x%02x @ 0x%lx\n", context.mbr->signature[1],
				context.mbr->signature[0], &context.mbr->signature);
			printf("MBR @ 0x%lx\n", context.mbr.get());
		}, "[drive]");

		commands.emplace("make", Command(0, 0, [](Context &context, const std::vector<std::string> &pieces) {
			long e0, r0;
			asm("%2 -> $a1    \n\
					<io getsize> \n\
					$e0 -> %0    \n\
					$r0 -> %1    " : "=r"(e0), "=r"(r0) : "r"(context.selectedDrive));
			if (e0 != 0) {
				printf("getsize failed: %ld\n", e0);
				return;
			}

			context.mbr->firstEntry.debug();
			context.mbr->firstEntry = MBREntry(0, 0xfa, 1, uint32_t(r0 / 512 - 1));
			printf("r0: %ld -> %u\n", r0, uint32_t(r0 / 512 - 1));
			printf("Number of blocks: %u\n", context.mbr->firstEntry.sectors);
			context.mbr->firstEntry.debug();
			ssize_t result = context.device->write(context.mbr.get(), sizeof(MBR), 0);
			if (result < 0)
				Kernel::panicf("device.write failed: %ld\n", result);
			context.partition = std::make_unique<Partition>(*context.device, context.mbr->firstEntry);
			context.driver = std::make_unique<ThornFAT::ThornFATDriver>(context.partition.get());
			strprint("ThornFAT driver instantiated.\n");
			printf("ThornFAT creation %s.\n", context.driver->make(sizeof(ThornFAT::DirEntry) * 5)? "succeeded"
				: "failed");
		}, "").setDeviceNeeded().setMBRNeeded());

		commands.emplace("driver", Command(0, 0, [](Context &context, const std::vector<std::string> &pieces) {
			context.partition = std::make_unique<Partition>(*context.device, context.mbr->firstEntry);
			context.driver = std::make_unique<ThornFAT::ThornFATDriver>(context.partition.get());
			context.driver->getRoot(nullptr, true);
			strprint("ThornFAT driver instantiated.\n");
		}, "").setDeviceNeeded().setMBRNeeded());

		commands.emplace("ls", Command(0, 1, [](Context &context, const std::vector<std::string> &pieces) {
			const std::string path = 1 < pieces.size()? FS::simplifyPath(context.cwd, pieces[1]) : context.cwd;
			const int status = context.driver->readdir(path.c_str(), [](const char *item, off_t) {
				printf("%s\n", item);
			});
			if (status != 0)
				printf("Error: %ld\n", long(status));
		}, "[path]").setDriverNeeded());

		commands.emplace("create", Command(1, 1, [](Context &context, const std::vector<std::string> &pieces) {
			const int status = context.driver->create(FS::simplifyPath(context.cwd, pieces[1]).c_str(), 0666, 0, 0);
			if (status != 0)
				printf("create error: %ld\n", long(status));
		}, "<path>").setDriverNeeded());

		commands.emplace("write", Command(1, -1, [](Context &context, const std::vector<std::string> &pieces) {
			const std::string path = FS::simplifyPath(context.cwd, pieces[1]);
			std::string data = pieces.size() == 2? "" : pieces[2];
			for (size_t i = 3; i < pieces.size(); ++i)
				data += " " + pieces[i];

			ssize_t status = context.driver->truncate(path.c_str(), data.size());
			if (status != 0) {
				printf("truncate error: %ld\n", long(status));
				return;
			}

			status = context.driver->write(path.c_str(), data.c_str(), data.size(), 0);
			if (status < 0)
				printf("write error: %ld\n", long(status));
		}, "<path> [data...]").setDriverNeeded());

		commands.emplace("read", Command(1, 1, [](Context &context, const std::vector<std::string> &pieces) {
			const std::string path = pieces[1][0] == '/'? pieces[1] : FS::simplifyPath(context.cwd,
				pieces[1]);
			size_t size;
			ssize_t status = context.driver->getsize(path.c_str(), size);
			if (status != 0) {
				printf("getsize failed: %ld\n", -status);
				return;
			}

			std::string data;
			data.resize(size);
			status = context.driver->read(path.c_str(), &data[0], size, 0);
			if (status < 0)
				printf("read failed: %ld\n", -status);
			else
				printf("Read %lu byte%s:\n%s\n", status, status == 1? "" : "s", data.c_str());
		}, "<path>").setDriverNeeded());

		commands.emplace("cd", Command(0, 1, [](Context &context, const std::vector<std::string> &pieces) {
			const std::string path = FS::simplifyPath(context.cwd, pieces[1]);
			if (context.driver->isdir(path.c_str()))
				context.cwd = path;
			else
				printf("Cannot change directory to %s\n", path.c_str());
		}, "<path>").setDriverNeeded());

		commands.try_emplace("pwd", -1, -1, [](Context &context, const std::vector<std::string> &pieces) {
			printf("%s\n", context.cwd.c_str());
		}, "");

		commands.try_emplace("debug", 1, 1, [](Context &context, const std::vector<std::string> &pieces) {
			if (pieces[1] != "off" && pieces[1] != "on") {
				strprint("Usage: debug <on|off>\n");
			} else if (pieces[1] == "off") {
				debug_enable = 0;
				debug_disable = 1;
			} else {
				debug_enable = 1;
				debug_disable = 0;
			}
		}, "<on|off>");

		commands.emplace("rm", Command(1, 1, [](Context &context, const std::vector<std::string> &pieces) {
			const std::string path = FS::simplifyPath(context.cwd, pieces[1]);
			int status = context.driver->exists(path.c_str());
			if (status != 0) {
				strprint("Path not found.\n");
				return;
			}

			if (context.driver->isdir(path.c_str()))
				status = context.driver->rmdir(path.c_str(), true);
			else
				status = context.driver->unlink(path.c_str());

			if (status != 0)
				printf("Error: %d\n", -status);
		}, "<path>").setDriverNeeded());

		commands.emplace("mkdir", Command(1, 1, [](Context &context, const std::vector<std::string> &pieces) {
			const std::string path = FS::simplifyPath(context.cwd, pieces[1]);
			int status = context.driver->mkdir(path.c_str(), 0755, 0, 0);
			if (status != 0)
				printf("Error: %d\n", -status);
		}, "<path>").setDriverNeeded());
	}
}
