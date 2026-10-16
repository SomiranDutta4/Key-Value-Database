#include <cassert>
#include <iostream>

#include "forgedb/parser.hpp"

int main() {
    {
        forgedb::Command command =
            forgedb::parse_command(
                "PUT name Mohit"
            );

        assert(
            command.type ==
            forgedb::CommandType::PUT
        );

        assert(command.key == "name");
        assert(command.value == "Mohit");
    }

    {
        forgedb::Command command =
            forgedb::parse_command(
                "PUT message Hello this is ForgeDB"
            );

        assert(
            command.type ==
            forgedb::CommandType::PUT
        );

        assert(command.key == "message");

        assert(
            command.value ==
            "Hello this is ForgeDB"
        );
    }

    {
        forgedb::Command command =
            forgedb::parse_command(
                "GET name"
            );

        assert(
            command.type ==
            forgedb::CommandType::GET
        );

        assert(command.key == "name");
    }

    {
        forgedb::Command command =
            forgedb::parse_command(
                "DEL name"
            );

        assert(
            command.type ==
            forgedb::CommandType::DELETE_KEY
        );

        assert(command.key == "name");
    }

    {
        forgedb::Command command =
            forgedb::parse_command(
                "EXIT"
            );
        assert(
            command.type ==
            forgedb::CommandType::EXIT
        );
    }

    {
        forgedb::Command command =
            forgedb::parse_command(
                "SOMETHING_INVALID"
            );

        assert(
            command.type ==
            forgedb::CommandType::INVALID
        );
    }

    {
        forgedb::Command command =
            forgedb::parse_command(
                "PUT only_key"
            );

        assert(
            command.type ==
            forgedb::CommandType::INVALID
        );
    }

    {
        forgedb::Command command =
            forgedb::parse_command(
                "GET"
            );

        assert(
            command.type ==
            forgedb::CommandType::INVALID
        );
    }

    std::cout
        << "test_parser: PASSED\n";

    return 0;
}
