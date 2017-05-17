#include <cstdlib>
#include <ctime>
#include <vector>
#include <mutex>
#include <set>
#include <iostream>
#include <thread>

using std::vector;
using std::set;

typedef unsigned char byte;

const int MAX_MEM_BLOCK_POWER = 15;
const int MIN_BLOCK_LEN = (1 << MAX_MEM_BLOCK_POWER);

class MemBox{
public:
    std::mutex mutex;
    int id;
    vector<byte*> memBlocks[MAX_MEM_BLOCK_POWER + 1];
    set<void*> memBlocksBegins;
    MemBox(int id) : id(id) {}
    MemBox(MemBox&&) {
        std::cerr << "COPYING MEMBOX #" << id << "\n";
        throw 1;
    }
    ~MemBox() {
        for (auto mem : memBlocksBegins)
            free(mem);
    }
    void* getMem(size_t bytes) {
        int blockPow = 3;
        while ((1 << blockPow) < bytes + 2) blockPow++;

        std::unique_lock<std::mutex> lock(mutex);

        if (memBlocks[blockPow].size() == 0) {
            byte* mem = (byte*)malloc(MIN_BLOCK_LEN);
            if (mem == nullptr) return nullptr;
            for (size_t i = 0; i < (MIN_BLOCK_LEN >> blockPow); ++i) {
                memBlocks[blockPow].push_back(mem + (i << blockPow));
                memBlocksBegins.insert(mem);
            }
        }
        byte* mem = memBlocks[blockPow].back();
        memBlocks[blockPow].pop_back();

        lock.unlock();

        mem[0] = id;
        mem[1] = blockPow;

        return mem + 2;
    }
    void freeMem(byte* ptr) {
        std::unique_lock<std::mutex> lock(mutex);
        memBlocks[ptr[1]].push_back(ptr);
        if (memBlocks[ptr[1]].size() > (MIN_BLOCK_LEN >> ptr[1]) * 5) {
            // TODO
            // you can add erasion memory from local storages to allow
            // reallocation for different blocks sizes and memBoxes
            // be careful, it can be a little bit harder than simple tries of delete operation 
        }
    }
};

thread_local size_t boxId = std::hash<std::thread::id>()(std::this_thread::get_id());

class MemStorage{
public:
    vector<MemBox> boxes;
    MemStorage(int coresNum = std::thread::hardware_concurrency()) {
        boxes.reserve(coresNum * 4);
        for (int i = 0; i < coresNum * 4; ++i)
            boxes.emplace_back(i);
    }
    void* getMem(size_t bytes) {
        if (bytes + 2 > MIN_BLOCK_LEN) {
            void* mem_ = malloc(bytes + 1);
            if (mem_ == nullptr) return nullptr;
            ((byte*)mem_)[0] = 0xFF;
            return (byte*)mem_ + 1;
        }
        if (bytes == 0) {
            std::cerr << "allocate 0 bytes\n";
            throw 1;
        }
        boxId %= boxes.size();
        for (size_t i = 0; i < boxes.size(); ++i) {
            void* ans = boxes[(boxId + i) % boxes.size()].getMem(bytes);
            if (ans != nullptr) return ans;
        }
        return nullptr;
    }
    void freeMem(void* ptr) {
        if (ptr == nullptr) return;
        if (((byte*)ptr)[-1] == 0xFF) {
            free((byte*)ptr - 1);
            return;
        }
        byte* mptr = ((byte*)ptr) - 2;
        boxes[mptr[0]].freeMem(mptr);
    }
};

MemStorage storage;

extern 
void* mtalloc(size_t bytes) {
    return storage.getMem(bytes);
}

extern 
void mtfree(void* ptr) {
    storage.freeMem(ptr);
}

/*
int main() {

}
*/