#pragma once
#include "../udp/client.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <string>

namespace dr {
namespace yaskawa {

namespace detail {
	class RpcService {
	public:
		using OnExecute = std::function<void(std::function<void(DetailedError)> resolve)>;

		/// Atomic flag to remember if the service is currently busy.
		std::atomic_flag busy = ATOMIC_FLAG_INIT;

		/// Name of the service (for debugging purposes).
		std::string name;

		/// Functor to call when executing the service.
		OnExecute execute;

		/// Construct an RpcService.
		RpcService(std::string name, OnExecute execute) : name{std::move(name)}, execute{std::move(execute)} {}
	};
}

namespace service_status {
	constexpr std::uint8_t idle      = 0;
	constexpr std::uint8_t requested = 1;
	constexpr std::uint8_t error     = 2;
}

void disabledService(udp::Client &, std::function<void(DetailedError)> resolve);

class RpcServer {
	/// The client to use for reading/writing command status.
	udp::Client * client_;

	/// The base register to use when reading command status.
	std::uint8_t base_register_;

	/// Delay between reading commands.
	std::chrono::steady_clock::duration read_commands_delay_;

	/// Timer to wait between reading commands.
	asio::steady_timer read_commands_timer_;

	/// Vector of services.
	std::vector<std::unique_ptr<detail::RpcService>> services_;

	/// If true, we're started. If false, we should stop ASAP.
	std::atomic<bool> started_{false};

	/// A callback to invoke when an error occurs.
	std::function<void(DetailedError)> on_error_;

public:
	/// Construct a RPC server.
	RpcServer(
		udp::Client & client,                       ///< The client to use for reading/writing command status.
		std::uint8_t base_register,                 ///< The base register to use for reading/writing command status.
		std::chrono::steady_clock::duration delay,  ///< Delay between reading command registers.
		std::function<void(DetailedError)> on_error ///< The callback to invoke when an error occurs.
	);

	/// Register a new service without parameters.
	/**
	 * The service callback is invoked as:
	 *   callback(resolve)
	 * where `resolve` is a functor taking a DetailedError that the service should invoke
	 * to notify the RPC server that the service call is finished.
	 */
	template<typename Callback>
	void addService(std::string name, Callback && callback) {
		services_.push_back(std::make_unique<detail::RpcService>(std::move(name), callback));
	}

	/// Register a new service with parameters.
	/**
	 * When the service is invoked, all pre_commands are executed.
	 * If an error occurs for one of the commands, the RPC server error handler is called with the error.
	 * If all commands succeeded, the service callback is invoked as:
	 *   callback(result, resolve)
	 * where `result` is a tuple with the results of each pre_command and `resolve` is a functor taking a DetailedError
	 * that the service should invoke to notify the RPC server that the service call is finished.
	 */
	template<typename PreCommands, typename Callback>
	void addService(std::string name, PreCommands && pre_commands, std::chrono::steady_clock::duration timeout, Callback && callback) {
		auto service = std::make_unique<detail::RpcService>(std::move(name), [
			&client = *client_,
			pre_commands = std::forward<PreCommands>(pre_commands),
			timeout,
			callback = std::forward<Callback>(callback)
		] (std::function<void(DetailedError)> resolve) {
			auto on_response = [&client, resolve = std::move(resolve), callback = std::move(callback)] (udp::MultiCommandResult<PreCommands> && result) {
				if (!result) std::move(resolve)(std::move(result.error_unchecked()));
				else callback(std::move(*result), std::move(resolve));
			};
			client.sendCommands(pre_commands, timeout, std::move(on_response));
		});
		services_.push_back(std::move(service));
	}

	/// Start the RPC server.
	/**
	 * Does nothing if the RPC server is already started.
	 * \return False if the RPC server was already started, true otherwise.
	 */
	bool start();

	/// Stop the RPC server as soon as possible.
	/**
	 * Does nothing if the RPC server is already stopped.
	 * \return False if the RPC server was already stopped, true otherwise.
	 */
	bool stop();

protected:
	/// Start the timer for reading commands.
	void startReadCommandsTimer();

	/// Read command status.
	void readCommands();

	/// Execute a service and manage the busy flag and status variable.
	bool execute(std::size_t index);
};

}}
