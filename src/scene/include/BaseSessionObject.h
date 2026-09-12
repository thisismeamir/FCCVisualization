#pragma once

#include <algorithm>
#include <string>

namespace fccvis::scene {

// A base for all the object in a scene to identify them using the name.
class BaseSessionObject
{
public:
    explicit BaseSessionObject(std::string objectName) : name(std::move(objectName)) {}
    virtual ~BaseSessionObject() = default;
    std::string name;
};
}
