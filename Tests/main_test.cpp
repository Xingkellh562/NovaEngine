// main_test.cpp
#include <gtest/gtest.h>   // 引入 GoogleTest 头文件
#include <iostream>

int main(int argc, char** argv) {
    // Step 0: 确认程序启动
    std::cout << "[Step 0] main() entered." << std::endl;

    // Step 1&2: 初始化 GoogleTest
    std::cout << "[Step 1] Calling InitGoogleTest..." << std::endl;
    ::testing::InitGoogleTest(&argc, argv);
    std::cout << "[Step 2] InitGoogleTest completed successfully." << std::endl;

    // Step 3&4: 运行所有测试用例
    std::cout << "[Step 3] Running tests via RUN_ALL_TESTS()..." << std::endl << std::endl;
    int result = RUN_ALL_TESTS();
    std::cout << std::endl << "[Step 4] RUN_ALL_TESTS finished. Result = " << result << std::endl;

    // 暂停窗口，方便查看输出
    std::cout << "Press Enter to exit..." << std::endl;
    std::cin.get();

    return result;
}