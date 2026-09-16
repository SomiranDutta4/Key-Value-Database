#include "forgedb/parser.hpp"

#include <cctype>
#include <sstream>
#include <iostream>

namespace forgedb {

namespace {

std::string to_upper(
    std::string value
) {
    for (char& ch : value) {
        ch = static_cast<char>(
            std::toupper(
                static_cast<unsigned char>(ch)
            )
        );
    }

    return value;
}

} // namespace

Command parse_command(
    const std::string& input
) {
    Command command;

    std::istringstream stream(input);

    std::string operation;

    if (!(stream >> operation)) {
        return command;
    }

    operation = to_upper(operation);

    if (operation == "PUT") {
        if (!(stream >> command.key)) {
            return Command{};
        }

        std::getline(
            stream,
            command.value
        );

        if (!command.value.empty() &&
            command.value.front() == ' ') {
            command.value.erase(
                command.value.begin()
            );
        }

        if (command.value.empty()) {
            return Command{};
        }

        command.type = CommandType::PUT;

        return command;
    }

    if (operation == "GET") {
        if (!(stream >> command.key)) {
            return Command{};
        }

        std::string extra;

        if (stream >> extra) {
            return Command{};
        }

        command.type = CommandType::GET;

        return command;
    }

    if (operation == "DELETE" ||
        operation == "DEL") {
        if (!(stream >> command.key)) {
            return Command{};
        }

        std::string extra;

        if (stream >> extra) {
            return Command{};
        }

        command.type =
            CommandType::DELETE_KEY;

        return command;
    }

    if (operation == "EXIT" ||
        operation == "QUIT") {


        command.type =
            CommandType::EXIT;

        return command;
    }

    return Command{};
}

} // namespace forgedb
