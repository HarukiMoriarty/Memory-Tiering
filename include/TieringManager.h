#ifndef TIERING_MANAGER_H
#define TIERING_MANAGER_H

#include <vector>
#include <string>

class TieringManager {
public:
    // Constructor and Destructor
    TieringManager();
    ~TieringManager();

    // Allocate a page of memory
    void* allocatePage();

    // Query the NUMA node of a page
    int queryNUMANode(void* page);

    // Move a page to a specified NUMA node
    bool movePage(void* page, int targetNode);

private:
    // Helper for syscall wrapper
    long movePagesWrapper(int pid, unsigned long count, void** pages, const int* nodes, int* status, int flags);
};

#endif // TIERING_MANAGER_H
