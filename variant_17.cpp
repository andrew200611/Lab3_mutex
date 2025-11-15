#include <iostream>
#include <string>
#include <mutex>
#include <thread>
#include <vector>
#include <fstream>
#include <chrono>
#include <random>
#include <sstream>

using namespace std;


class OptimizedStructure
{
public:
    OptimizedStructure() : field0(0), field1(0) {}
    OptimizedStructure(const OptimizedStructure&) = delete;
    OptimizedStructure& operator=(const OptimizedStructure&) = delete;
    ~OptimizedStructure() = default;

    void write(int fieldIndex, int value)
    {
        if (fieldIndex == 0) {
            lock_guard<mutex> lk(mutex_field0);
            field0 = value;
        }
        else if (fieldIndex == 1) {
            lock_guard<mutex> lk(mutex_field1);
            field1 = value;
        }
    }

    int read(int fieldIndex)
    {
        if (fieldIndex == 0) {
            lock_guard<mutex> lk(mutex_field0);
            return field0;
        }
        else if (fieldIndex == 1) {
            lock_guard<mutex> lk(mutex_field1);
            return field1;
        }
        return -1;
    }

    operator string()
    {
        int f0, f1;

        
        {
            lock_guard<mutex> lk0(mutex_field0);
            f0 = field0;
        }
        {
            lock_guard<mutex> lk1(mutex_field1);
            f1 = field1;
        }

        
        stringstream ss;
        ss << "[" << f0 << ", " << f1 << "]";
        return ss.str();
    }

private:
    int field0, field1;
    mutable mutex mutex_field0;
    mutable mutex mutex_field1;
};

void generate_command_file(const string& filename,
    const vector<double>& weights,
    long num_commands = 1000000)
{
    const vector<string> commands = {
        "read 0",    
        "write 0 1", 
        "read 1",    
        "write 1 1", 
        "string"     
    };

    ofstream file(filename);
    if (!file.is_open()) {
        cerr << "Помилка: неможливо відкрити файл " << filename << endl;
        return;
    }

    random_device rd;
    mt19937 gen(rd());
    discrete_distribution<> dist(weights.begin(), weights.end());

   

    for (long i = 0; i < num_commands; ++i) {
        int command_index = dist(gen);
        file << commands[command_index] << "\n";
    }

    file.close();
    
}

void setup_files()
{
    cout << "Генерація файлів" << endl;

    generate_command_file("var17.txt", { 5.0, 5.0, 30.0, 5.0, 55.0 });

    generate_command_file("equal.txt", { 20.0, 20.0, 20.0, 20.0, 20.0 });

    generate_command_file("custom.txt", { 5.0, 45.0, 5.0, 45.0, 0.0 });

    cout << "Генерацію файлів завершено" << endl;
}

void worker(OptimizedStructure& structure, const vector<string>& commands)
{
    volatile int read_result = 0;
    string string_result;

    for (const string& cmd_line : commands)
    {
        if (cmd_line == "read 0") {
            read_result = structure.read(0);
        }
        else if (cmd_line == "read 1") {
            read_result = structure.read(1);
        }
        else if (cmd_line == "write 0 1") {
            structure.write(0, 1);
        }
        else if (cmd_line == "write 1 1") {
            structure.write(1, 1);
        }
        else if (cmd_line == "string") {
            string_result = (string)structure;
        }
    }
    
    (void)read_result;
    (void)string_result;
}

bool load_commands(const string& filename, vector<string>& commands)
{
    ifstream file(filename);
    if (!file.is_open()) {
        cerr << "Помилка: неможливо відкрити файл " << filename << endl;
        return false;
    }

    string line;
    commands.clear();
    while (getline(file, line)) {
        if (!line.empty()) {
            commands.push_back(line);
        }
    }
    file.close();
    
    return true;
}

double run_test(const string& filename, int num_threads)
{
   
    vector<string> all_commands;

    if (!load_commands(filename, all_commands)) {
        return -1.0;
    }

    vector<vector<string>> thread_commands(num_threads);
    size_t block_size = all_commands.size() / num_threads;

    for (int i = 0; i < num_threads; ++i) {
        auto start_it = all_commands.begin() + i * block_size;
        auto end_it = (i == num_threads - 1) ?
            all_commands.end() : (start_it + block_size);
        thread_commands[i].assign(start_it, end_it);
    }

    
    const int RUNS = 3;
    double total_time = 0.0;

    for (int run = 0; run < RUNS; ++run) {
        OptimizedStructure structure;
        vector<thread> threads;
        auto start_time = chrono::high_resolution_clock::now();

        for (int i = 0; i < num_threads; ++i) {
            threads.emplace_back(worker, ref(structure),
                ref(thread_commands[i]));
        }

        for (auto& t : threads) {
            t.join();
        }

        auto end_time = chrono::high_resolution_clock::now();
        double run_time = chrono::duration<double, milli>(end_time - start_time).count();
        total_time += run_time;
    }

    return total_time / RUNS;
}

int main()
{
    system("chcp 65001");

    const long NUM_COMMANDS = 1000000;

    setup_files();

    cout << endl;
    cout << "Варіант 17 (m=2): поле 0: read 5%, write 5%; поле 1: read 30%, write 5%; string 55%" << endl;
    cout << "Схема захисту: 2x mutex" << endl;
    cout << "Команд на файл: " << NUM_COMMANDS << endl;

    vector<string> files = {
        "var17.txt",
        "equal.txt",
        "custom.txt"
    };

    vector<string> descriptions = {
        "Варіант 17 (55% string)",
        "Рівні частоти (20% string)",
        "Багато write (0% string)"
    };

    vector<int> thread_counts = { 1, 2, 3 };
    vector<vector<double>> results(files.size(),
        vector<double>(thread_counts.size()));

    cout.precision(1);
    cout << fixed;

 
    for (size_t i = 0; i < files.size(); ++i) {
        cout << endl << "Тестування " << descriptions[i] << ":" << endl;

        for (size_t j = 0; j < thread_counts.size(); ++j) {
            cout << "  " << thread_counts[j] << " потік: Завантажено " << NUM_COMMANDS << " команд з " << files[i] << endl;

            double time_ms = run_test(files[i], thread_counts[j]);
            results[i][j] = time_ms;

            cout << time_ms << " мс" << endl;
        }
    }

   
    cout << endl << "Результати:" << endl;
    for (size_t i = 0; i < files.size(); ++i) {
        cout << descriptions[i] << ":" << endl;
        cout << "  1 потік: " << results[i][0] << " мс" << endl;
        cout << "  2 потоки: " << results[i][1] << " мс" << endl;
        cout << "  3 потоки: " << results[i][2] << " мс" << endl;
    }

    

    return 0;
}