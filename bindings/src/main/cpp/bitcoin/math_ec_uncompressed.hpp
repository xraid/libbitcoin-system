#ifndef LIBBITCOIN__MATH_EC_UNCOMPRESSED_HPP
#define LIBBITCOIN__MATH_EC_UNCOMPRESSED_HPP

#include <bitcoin/bitcoin/math/elliptic_curve.hpp>

namespace libbitcoin {
namespace api {

class math_ec_uncompressed {
public:
	ec_uncompressed getValue() {
		return value;
	}

	void setValue(ec_uncompressed value) {
		this->value = value;
	}
private:
	ec_uncompressed value;
};

} // namespace api
} // namespace libbitcoin
#endif
