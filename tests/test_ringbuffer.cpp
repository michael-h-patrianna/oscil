#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <vector>

#include "../src/dsp/RingBuffer.h"

TEST_CASE("RingBuffer basic push/peekLatest", "[ringbuffer]") {
    dsp::RingBuffer<float> rb(8);
    std::vector<float> out(4, 0.0f);

    // less than requested
    float a[] = {1, 2};
    rb.push(a, 2);
    rb.peekLatest(out.data(), out.size());
    REQUIRE(out[0] == Catch::Approx(0));
    REQUIRE(out[1] == Catch::Approx(0));
    REQUIRE(out[2] == Catch::Approx(1));
    REQUIRE(out[3] == Catch::Approx(2));

    // wrap and overwrite
    float b[] = {3, 4, 5, 6, 7, 8};
    rb.push(b, 6);
    rb.peekLatest(out.data(), out.size());
    REQUIRE(out[0] == Catch::Approx(5));
    REQUIRE(out[1] == Catch::Approx(6));
    REQUIRE(out[2] == Catch::Approx(7));
    REQUIRE(out[3] == Catch::Approx(8));
}
