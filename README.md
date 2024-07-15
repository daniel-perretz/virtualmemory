### Virtual Memory Implementation with Hierarchical Page Tables

#### Overview

This project implements a virtual memory system using hierarchical page tables. Virtual memory is a memory management model that allows processes to use more memory than physically available on the host machine.

#### Key Concepts

1. **Virtual Memory**:
   - Maps virtual (logical) addresses to physical addresses in RAM.
   - Allows use of more memory than physically available.

2. **Paging**:
   - Divides virtual address space into fixed-size pages.
   - Physical memory is divided into frames of the same size.

3. **Page Tables**:
   - Map pages to frames.
   - Uses hierarchical (multi-level) page tables for efficiency.

4. **Swapping**:
   - Pages can be stored on disk when not in RAM.
   - Swapped in when needed and swapped out when RAM is full.

#### Implementation Details

##### Core Components

1. **Address Translation**:
   - Converts virtual addresses to physical addresses using multi-level page tables.
   - Ensures correct mapping of virtual pages to physical frames.

2. **Page Table Management**:
   - Organizes page tables in a tree-like hierarchical structure.
   - Each page table is stored in a frame in physical memory.
   - The root page table is always stored in frame 0.

3. **Page Replacement Algorithm**:
   - Prioritizes which frame to evict based on specific criteria:
     - Frames with empty tables.
     - Unused frames.
     - Frames with pages having the maximum cyclic distance (pages least likely to be used soon).
