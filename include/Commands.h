#pragma once

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

class Kernel;
struct Partition;
struct WhyDevice;

namespace FS {
	class Driver;
}

namespace ThornFAT {
	class ThornFATDriver;
}

namespace Thurisaz {
	struct Context {
		Kernel &kernel;
		std::shared_ptr<WhyDevice> device;
		std::shared_ptr<Partition> partition;
		std::shared_ptr<ThornFAT::ThornFATDriver> driver;
		std::string cwd = "/";
		long driveCount = 0;
		long selectedDrive = -1;
		Context(Kernel &kernel_): kernel(kernel_) {}
	};

	struct Command {
		constexpr static long SUCCESS = 0;
		constexpr static long NOT_FOUND = -1;
		constexpr static long BAD_ARGUMENTS = -2;
		constexpr static long EXIT_SHELL = -3;

		int minArgs; // -1 = no minimum
		int maxArgs; // -1 = no maximum
		std::function<long(Context &, const std::vector<std::string> &)> action;
		const char *usage = nullptr;
		bool driverNeeded = false;
		bool deviceNeeded = false;

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
	};

	int runCommand(const std::map<std::string, Command> &, Context &, const std::vector<std::string> &);
	void addCommands(std::map<std::string, Command> &);
}
