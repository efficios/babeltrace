#include <iostream>
#include <utility>

#include "cpp-common/bt2/value.hpp"

void f(bt2::BoolValue::Shared lel)
{
}

int main()
{
    f(bt2::NullValue {}.shared());
}
