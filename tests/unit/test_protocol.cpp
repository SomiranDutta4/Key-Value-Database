#include <cassert>
#include <iostream>
#include <string>

#include "forgedb/protocol.hpp"

int main() {
    {
        std::string payload =
            "HELLO";

        std::string frame =
            forgedb::create_frame(
                payload
            );

        std::string buffer;
        std::string extracted;

        buffer.append(
            frame.data(),
            2
        );

        bool complete =
            forgedb::try_extract_frame(
                buffer,
                extracted
            );

        assert(!complete);

        buffer.append(
            frame.data() + 2,
            frame.size() - 2
        );

        complete =
            forgedb::try_extract_frame(
                buffer,
                extracted
            );

        assert(complete);
        assert(extracted == "HELLO");
        assert(buffer.empty());
    }

    {
        std::string frame1 =
            forgedb::create_frame(
                "FIRST"
            );

        std::string frame2 =
            forgedb::create_frame(
                "SECOND"
            );

        std::string buffer =
            frame1 + frame2;

        std::string payload;

        bool complete =
            forgedb::try_extract_frame(
                buffer,
                payload
            );

        assert(complete);
        assert(payload == "FIRST");

        complete =
            forgedb::try_extract_frame(
                buffer,
                payload
            );

        assert(complete);
        assert(payload == "SECOND");

        assert(buffer.empty());
    }

    {
        std::string payload =
            "Hello from ForgeDB";

        std::string frame =
            forgedb::create_frame(
                payload
            );

        std::string extracted;
        bool complete =
            forgedb::try_extract_frame(
                frame,
                extracted
            );

        assert(complete);
        assert(extracted == payload);
        assert(frame.empty());
    }

    std::cout
        << "test_protocol: PASSED\n";

    return 0;
}
