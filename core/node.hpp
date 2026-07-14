#pragma once
#include <vector>
#include "types.hpp"

typedef struct Node
{
    unsigned long id;
    char *Name;
    Point GlobalPosition;
    std::vector<Node> Children;
};