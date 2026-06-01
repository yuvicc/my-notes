#include <chrono>
#include <iostream>
#include <thread>
#include <mutex>

int counter = 0;
std::mutex mtx1, mtx2;

void increamentVal() {
    for (int i = 0; i < 10000; i++) {
        std::lock_guard<std::mutex> lock(mtx1);
        ++counter;
    }
}

void task1() {
    std::lock_guard<std::mutex> lock1(mtx1);
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    std::lock_guard<std::mutex> lock2(mtx2);
    std::cout << "Task 1 completed!!" << std::endl;
}

void task2() {
    std::lock_guard<std::mutex> lock2(mtx2);
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    std::lock_guard<std::mutex> lock1(mtx1);
    std::cout << "Task 2 completed!!";
}

int main() {

    std::thread t1(task1);
    std::thread t2(task2);

    t1.join();
    t2.join();

    std::cout << "Final val of counter: " << counter;

    return 0;
}
