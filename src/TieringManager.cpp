#include "TieringManager.h"
#include <sys/mman.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <linux/mempolicy.h>
#include <cstdio>
#include <cstdlib>

#define PAGE_SIZE 4096

TieringManager::TieringManager() {}

TieringManager::~TieringManager() {}

void* TieringManager::allocatePage() {
    void* page = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    if (page == MAP_FAILED) {
        perror("mmap failed");
        return nullptr;
    }
    return page;
}

int TieringManager::queryNUMANode(void* page) {
    int status;
    if (movePagesWrapper(0, 1, &page, nullptr, &status, 0) != 0) {
        perror("Querying NUMA node failed");
        return -1;
    }
    return status;
}

bool TieringManager::movePage(void* page, int targetNode) {
    int status;
    if (movePagesWrapper(0, 1, &page, &targetNode, &status, MPOL_MF_MOVE) != 0) {
        perror("Moving page failed");
        return false;
    }
    return true;
}

long TieringManager::movePagesWrapper(int pid, unsigned long count, void** pages, const int* nodes, int* status, int flags) {
    return syscall(SYS_move_pages, pid, count, pages, nodes, status, flags);
}
