#include "test_harness.h"

#include "fdir.h"

TEST(test_add)
{
    ASSERT_EQ_INT(add(2, 3), 5);
    ASSERT_EQ_INT(add(-1, 1), 0);
}

int main(void)
{
    RUN(test_add);
    return test_harness_summary();
}
