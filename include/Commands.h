#pragma once

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

struct WhyDevice;
struct MBR;
struct Partition;

namespace FS {
	class Driver;
}

namespace ThornFAT {
	class ThornFATDriver;
}

namespace Thurisaz {
	struct Context {
		std::unique_ptr<WhyDevice> device;
		std::unique_ptr<MBR> mbr;
		std::unique_ptr<Partition> partition;
		std::unique_ptr<ThornFAT::ThornFATDriver> driver;
		std::string cwd = "/";
		long driveCount;
		long selectedDrive;
	};

	struct Command {
		constexpr static long SUCCESS = 0;
		constexpr static long NOT_FOUND = -1;
		constexpr static long BAD_ARGUMENTS = -2;

		int minArgs; // -1 = no minimum
		int maxArgs; // -1 = no maximum
		std::function<long(Context &, const std::vector<std::string> &)> action;
		const char *usage = nullptr;
		bool driverNeeded = false;
		bool deviceNeeded = false;
		bool mbrNeeded = false;

		Command(int min_args, int max_args, const decltype(action) &action_, const char *usage_ = nullptr):
			minArgs(min_args), maxArgs(max_args), action(action_), usage(usage_) {}
		
		bool valid(int arg_count) const {
			return (minArgs == -1 || minArgs <= arg_count) && (maxArgs == -1 || arg_count <= maxArgs);
		}

		long operator()(Context &context, const std::vector<std::string> &pieces) const {
			return action(context, pieces);
		}

		Command & setUsage(const char *usage_) {
			usage = usage_;
			return *this;
		}

		Command & setDriverNeeded(const bool needed = true) {
			driverNeeded = needed;
			return *this;
		}

		Command & setDeviceNeeded(const bool needed = true) {
			deviceNeeded = needed;
			return *this;
		}

		Command & setMBRNeeded(const bool needed = true) {
			mbrNeeded = needed;
			return *this;
		}
	};

	int runCommand(const std::map<std::string, Command> &, Context &, const std::vector<std::string> &);
	void addCommands(std::map<std::string, Command> &);
}
