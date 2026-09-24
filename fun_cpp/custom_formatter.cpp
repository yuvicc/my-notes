// C++ 20 introduced the custom formatter using std::format so that I can create a formatter for my own type.
// I can create my custom formatter using std::formatter<T>, T is the type that we would want to create for.
// So, basically what happens is std::formatter<T> calls two member functions which is `parse(ctx)` and `format()`.
// `parse(ctx) reads the formar spec, which is everything after the ':' inside {}`
// `format(value, ctx) writes the output through ctx.out() usually with std::format_to`

// Basic formatting


#include <format>
#include <cmath>
#include <iostream>

struct Point { double x, y; };

template <>
struct std::formatter<Point> {
    char mode = 'c';

    constexpr auto parse(std::format_parse_context& ctx) {
        auto it = ctx.begin();
        if (it != ctx.end() && (*it == 'c' || *it == 'p')) mode = *it++;
        if (it != ctx.end() && *it != '}')
            throw std::format_error("invalid Point spec");
        return it;
    }

    auto format(const Point& p, std::format_context& ctx) const {
        if (mode == 'p')
            return std::format_to(ctx.out(), "(r={:.3f}, 0={:.3f})", std::hypot(p.x, p.y), std::atan2(p.y, p.x));
        return std::format_to(ctx.out(), "({}, {})", p.x, p.y);
    }
};


int main() {

    Point p {1, 2};
    std::cout << std::format("{}", Point{3, 4}); // c for cartesian and p is for polar
    std::cout << std::endl;
    std::cout << std::format("{:p}", Point{3, 4}); // polar coordinates

}
