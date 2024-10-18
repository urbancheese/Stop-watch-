#include <iostream>
#include <chrono>
#include <thread>
#include <iomanip>
#include <limits>
#include <atomic>
#include <mutex>
#include <vector>
#include <fstream>
#include <sstream>
#include <stdexcept>

class Stopwatch {
private:
    std::chrono::steady_clock::time_point start_time;
    std::chrono::duration<double> elapsed_time;
    std::atomic<bool> is_running;
    std::atomic<bool> is_paused;
    std::atomic<bool> display_running;
    std::chrono::duration<double> display_interval;
    std::mutex mtx;
    std::thread display_thread;
    std::vector<std::chrono::duration<double>> laps;

public:
    Stopwatch() : elapsed_time(0), is_running(false), is_paused(false), display_running(false), display_interval(std::chrono::seconds(1)) {
        try {
            loadConfig();
        } catch (const std::exception& e) {
            std::cerr << "Error loading config: " << e.what() << std::endl;
            std::cerr << "Using default display interval of 1 second." << std::endl;
        }
    }

    ~Stopwatch() {
        stopDisplayThread();
        try {
            saveConfig();
        } catch (const std::exception& e) {
            std::cerr << "Error saving config: " << e.what() << std::endl;
        }
    }

    void start() {
        std::lock_guard<std::mutex> lock(mtx);
        if (!is_running) {
            start_time = std::chrono::steady_clock::now();
            is_running = true;
            is_paused = false;
            std::cout << "Stopwatch started." << std::endl;
            startDisplayThread();
        } else if (is_paused) {
            start_time = std::chrono::steady_clock::now();
            is_paused = false;
            std::cout << "Stopwatch resumed." << std::endl;
        } else {
            std::cout << "Stopwatch is already running." << std::endl;
        }
    }

    void stop() {
        std::lock_guard<std::mutex> lock(mtx);
        if (is_running && !is_paused) {
            auto end_time = std::chrono::steady_clock::now();
            elapsed_time += std::chrono::duration_cast<std::chrono::duration<double>>(end_time - start_time);
            is_running = false;
            is_paused = false;
            stopDisplayThread();
            displayFormattedTime(elapsed_time.count());
            std::cout << " (Stopwatch stopped)" << std::endl;
        } else {
            std::cout << "Stopwatch is not running." << std::endl;
        }
    }

    void pause() {
        std::lock_guard<std::mutex> lock(mtx);
        if (is_running && !is_paused) {
            auto end_time = std::chrono::steady_clock::now();
            elapsed_time += std::chrono::duration_cast<std::chrono::duration<double>>(end_time - start_time);
            is_paused = true;
            stopDisplayThread();
            displayFormattedTime(elapsed_time.count());
            std::cout << " (Stopwatch paused)" << std::endl;
        } else if (is_paused) {
            std::cout << "Stopwatch is already paused." << std::endl;
        } else {
            std::cout << "Stopwatch is not running." << std::endl;
        }
    }

    void reset() {
        std::lock_guard<std::mutex> lock(mtx);
        std::cout << "Are you sure you want to reset the stopwatch? (y/n): ";
        char confirm;
        std::cin >> confirm;
        if (confirm == 'y' || confirm == 'Y') {
            elapsed_time = std::chrono::duration<double>::zero();
            is_running = false;
            is_paused = false;
            stopDisplayThread();
            laps.clear();
            std::cout << "Stopwatch reset." << std::endl;
        } else {
            std::cout << "Reset cancelled." << std::endl;
        }
    }

    void display() {
        std::lock_guard<std::mutex> lock(mtx);
        if (is_running && !is_paused) {
            auto current_time = std::chrono::steady_clock::now();
            auto current_elapsed = elapsed_time + std::chrono::duration_cast<std::chrono::duration<double>>(current_time - start_time);
            displayFormattedTime(current_elapsed.count());
            std::cout << " (Running)" << std::endl;
            displayProgressBar(current_elapsed.count());
        } else if (is_paused) {
            displayFormattedTime(elapsed_time.count());
            std::cout << " (Paused)" << std::endl;
            displayProgressBar(elapsed_time.count());
        } else {
            displayFormattedTime(elapsed_time.count());
            std::cout << " (Stopped)" << std::endl;
            displayProgressBar(elapsed_time.count());
        }
    }

    void setDisplayInterval(double seconds) {
        const double MIN_INTERVAL = 0.1;
        const double MAX_INTERVAL = 60.0;
        
        if (seconds >= MIN_INTERVAL && seconds <= MAX_INTERVAL) {
            display_interval = std::chrono::duration<double>(seconds);
            std::cout << "Display interval set to " << seconds << " seconds." << std::endl;
        } else {
            std::cout << "Invalid interval. Please enter a number between " 
                      << MIN_INTERVAL << " and " << MAX_INTERVAL << " seconds." << std::endl;
        }
    }

    void lap() {
        std::lock_guard<std::mutex> lock(mtx);
        if (is_running && !is_paused) {
            auto current_time = std::chrono::steady_clock::now();
            auto current_elapsed = elapsed_time + std::chrono::duration_cast<std::chrono::duration<double>>(current_time - start_time);
            laps.push_back(current_elapsed);
            std::cout << "Lap " << laps.size() << ": ";
            displayFormattedTime(current_elapsed.count());
            std::cout << std::endl;
        } else {
            std::cout << "Cannot record lap: Stopwatch is not running." << std::endl;
        }
    }

    void displayLaps() {
        std::lock_guard<std::mutex> lock(mtx);
        if (laps.empty()) {
            std::cout << "No laps recorded." << std::endl;
        } else {
            std::cout << "Recorded Laps:" << std::endl;
            for (size_t i = 0; i < laps.size(); ++i) {
                std::cout << "Lap " << i + 1 << ": ";
                displayFormattedTime(laps[i].count());
                std::cout << std::endl;
            }
        }
    }

private:
    void displayFormattedTime(double seconds) {
        int minutes = static_cast<int>(seconds) / 60;
        seconds = std::fmod(seconds, 60.0);
        std::cout << "Elapsed time: " << std::setfill('0') << std::setw(2) << minutes << ":" 
                  << std::setfill('0') << std::setw(5) << std::fixed << std::setprecision(2) << seconds;
    }

    void displayProgressBar(double seconds) {
        const int barWidth = 50;
        int progress = static_cast<int>((seconds / 60) * barWidth) % barWidth;
        std::cout << "\n[";
        for (int i = 0; i < barWidth; ++i) {
            if (i < progress) std::cout << "=";
            else if (i == progress) std::cout << ">";
            else std::cout << " ";
        }
        std::cout << "] " << static_cast<int>(seconds) % 60 << "s" << std::endl;
    }

    void startDisplayThread() {
        stopDisplayThread();
        display_running = true;
        display_thread = std::thread([this]() {
            while (display_running) {
                {
                    std::lock_guard<std::mutex> lock(mtx);
                    if (is_running && !is_paused) {
                        display();
                    }
                }
                std::this_thread::sleep_for(display_interval);
            }
        });
    }

    void stopDisplayThread() {
        display_running = false;
        if (display_thread.joinable()) {
            display_thread.join();
        }
    }

    void saveConfig() {
        std::ofstream config_file("stopwatch_config.txt");
        if (config_file.is_open()) {
            config_file << display_interval.count() << std::endl;
            config_file.close();
        } else {
            throw std::runtime_error("Unable to open config file for writing");
        }
    }

    void loadConfig() {
        std::ifstream config_file("stopwatch_config.txt");
        if (config_file.is_open()) {
            double interval;
            if (config_file >> interval) {
                setDisplayInterval(interval);
            } else {
                throw std::runtime_error("Invalid data in config file");
            }
            config_file.close();
        } else {
            throw std::runtime_error("Unable to open config file for reading");
        }
    }
};

void print_menu() {
    std::cout << "\nStopwatch Menu:" << std::endl;
    std::cout << "1. Start/Resume" << std::endl;
    std::cout << "2. Pause" << std::endl;
    std::cout << "3. Stop" << std::endl;
    std::cout << "4. Reset" << std::endl;
    std::cout << "5. Display Time" << std::endl;
    std::cout << "6. Set Display Interval" << std::endl;
    std::cout << "7. Record Lap" << std::endl;
    std::cout << "8. Display Laps" << std::endl;
    std::cout << "9. Help" << std::endl;
    std::cout << "10. Exit" << std::endl;
}

void display_help() {
    std::cout << "\nHelp: This stopwatch allows you to:" << std::endl;
    std::cout << "1. Start and stop timing." << std::endl;
    std::cout << "2. Pause and resume timing." << std::endl;
    std::cout << "3. Record lap times." << std::endl;
    std::cout << "4. View recorded lap times." << std::endl;
    std::cout << "5. Change the display update interval." << std::endl;
    std::cout << "6. Reset the stopwatch." << std::endl;
    std::cout << "Type the number corresponding to each option to use the stopwatch." << std::endl;
}

int get_menu_choice() {
    int choice;
    while (true) {
        std::cout << "Enter your choice (1-10): ";
        if (std::cin >> choice) {
            if (choice >= 1 && choice <= 10) {
                return choice;
            }
        }
        std::cout << "Invalid input. Please enter a number between 1 and 10." << std::endl;
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }
}

double get_valid_interval() {
    double interval;
    while (true) {
        std::cout << "Enter new display interval in seconds (0.1 to 60): ";
        if (std::cin >> interval) {
            if (interval >= 0.1 && interval <= 60) {
                return interval;
            }
        }
        std::cout << "Invalid input. Please enter a number between 0.1 and 60." << std::endl;
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }
}

int main() {
    Stopwatch stopwatch;
    int choice;

    std::cout << "Welcome to the Robust C++ Stopwatch!" << std::endl;
    std::cout << "Type 9 for help on how to use the stopwatch." << std::endl;

    while (true) {
        print_menu();
        choice = get_menu_choice();

        switch (choice) {
            case 1:
                stopwatch.start();
                break;
            case 2:
                stopwatch.pause();
                break;
            case 3:
                stopwatch.stop();
                break;
            case 4:
                stopwatch.reset();
                break;
            case 5:
                stopwatch.display();
                break;
            case 6:
                {
                    double interval = get_valid_interval();
                    stopwatch.setDisplayInterval(interval);
                }
                break;
            case 7:
                stopwatch.lap();
                break;
            case 8:
                stopwatch.displayLaps();
                break;
            case 9:
                display_help();
                break;
            case 10:
                return 0;
        }
    }

    return 0;
}
