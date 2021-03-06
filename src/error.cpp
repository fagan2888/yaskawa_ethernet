/* Copyright 2016-2019 Fizyr B.V. - https://fizyr.com
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 *    may be used to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "error.hpp"
#include "types.hpp"

#include <string>

namespace dr {
namespace yaskawa {

namespace {
	class : public std::error_category {
		/// Get the name of the error category
		char const * name() const noexcept override {
			return "yaskawa";
		}

		/// Get a descriptive error message for an error code.
		std::string message(int error) const noexcept override {
			switch (errc::errc_t(error)) {
				case errc::malformed_response:    return "malformed message";
				case errc::command_failed:        return "command failed";
				case errc::unknown_request:       return "unknown request";
			}
			return "unkown error: " + std::to_string(error);
		}
	} yaskawa_category_;
}

/// The error category for protocol errors.
std::error_category const & yaskawa_category() {
	return yaskawa_category_;
}

namespace {
	char hexNibble(int value) {
		if (value < 10) return '0' + value;
		return 'A' + value - 10;
	}

	template<typename T>
	std::string toHex(T val) {
		static_assert(std::is_integral<T>::value, "");
		std::string result(sizeof(T) * 2, '0');
		for (unsigned int i = 0; i < sizeof(T); i++) {
			result[(sizeof(T) - i) * 2 - 1] = hexNibble(val >> (8 * i)     & 0x0f);
			result[(sizeof(T) - i) * 2 - 2] = hexNibble(val >> (8 * i + 4) & 0x0f);
		}
		return result;
	}
}

Error malformedResponse(std::string message) {
	return {errc::malformed_response, std::move(message)};
}

Error commandFailed(std::uint16_t status, std::uint16_t extra_status) {
	return {errc::command_failed,
		"command failed with status 0x" + toHex(status)
		+ " and additional status 0x" + toHex(extra_status)
	};
}

Error expectValue(std::string name, int value, int expected) {
	if (value == expected) return {};
	return malformedResponse(
		"unexpected " + std::move(name) + ", "
		"expected exactly " + std::to_string(expected) + ", "
		"got " + std::to_string(value)
	);
}

Error expectValueMin(std::string name, int value, int min) {
	if (value >= min) return {};
	return malformedResponse(
		"unexpected " + std::move(name) + ", "
		"expected at least " + std::to_string(min) + ", "
		"got " + std::to_string(value)
	);
}

Error expectValueMax(std::string name, int value, int max) {
	if (value <= max) return {};
	return malformedResponse(
		"unexpected " + std::move(name) + ", "
		"expected at most " + std::to_string(max) + ", "
		"got " + std::to_string(value)
	);
}

Error expectValueMinMax(std::string name, int value, int min, int max) {
	if (value >= min && value <= max) return {};
	return malformedResponse(
		"unexpected " + std::move(name) + ", "
		"expected a value in the range [" + std::to_string(min) + ", " + std::to_string(max) + "] (inclusive), "
		"got " + std::to_string(value)
	);
}

Error expectSize(std::string description, std::size_t actual_size, std::size_t expected_size) {
	if (actual_size == expected_size) return {};
	return {errc::malformed_response,
		"unexpected " + description + " size, "
		"expected exactly " + std::to_string(expected_size) + " bytes, "
		"got " + std::to_string(actual_size)
	};
}

Error expectSizeMin(std::string description, std::size_t actual_size, std::size_t minimum_size) {
	if (actual_size >= minimum_size) return {};
	return {errc::malformed_response,
		"unexpected " + description + " size, "
		"expected at least " + std::to_string(minimum_size) + " bytes, "
		"got " + std::to_string(actual_size)
	};
}

Error expectSizeMax(std::string description, std::size_t actual_size, std::size_t maximum_size) {
	if (actual_size <= maximum_size) return {};
	return {errc::malformed_response,
		"unexpected " + description + " size, "
		"expected at most " + std::to_string(maximum_size) + " bytes, "
		"got " + std::to_string(actual_size)
	};
}

Error expectSizeMinMax(std::string description, std::size_t actual_size, std::size_t min, std::size_t max) {
	if (actual_size >= min &&  actual_size <= max) return {};
	return {errc::malformed_response,
		"unexpected " + description + " size, "
		"expected a size in the range of [" + std::to_string(min) + ", " + std::to_string(max) + "] bytes (inclusive), "
		"got " + std::to_string(actual_size)
	};
}

}}
