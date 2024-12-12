#ifndef COMMON_H
#define COMMON_H

#include <string>
#include <sstream>

enum class OperationType {
    READ,
    WRITE,
};

struct Message {
    void* page_address;
    OperationType op_type;

    Message(void* addr, OperationType op) : page_address(addr), op_type(op) {}

    std::string toString() const {
        std::stringstream ss;
        ss << "Address: " << page_address
            << ", Operation: " << (op_type == OperationType::READ ? "READ" : "WRITE");
        return ss.str();
    }
};

#endif // COMMON_H