//
// Created by noabengallim on 6/19/23.
//

#include "VirtualMemory.h"
#include "PhysicalMemory.h"


/**
 * Struct that keeps the arguments needed for the DFS search for frame
 */
struct SearchArguments {
    word_t currentFrame;  // the current frame that should not be evicted
    word_t maxFrame;  // the maximum frame index we have seen so far
    uint64_t pageNumber;  // the virtual page number we want to map to a physical address
    int maxCyclicFrame;  // the frame that has the maximal cyclic distance
    int maxCyclicDist;  // the current maximal cyclic distance
    uint64_t maxCyclicPage;  // the page that has the maximal cyclic distance
    uint64_t maxCyclicParent;  // the parent of the frame that has the maximal cyclic distance
    int emptyFrame;  // the frame with an empty table (if exists)
    int priority;  // the priority {1, 2, 3} of the chosen frame
};


/**
 * Divides the virtual address to an array of offsets.
 *
 * @param virtualAddress The virtual address
 * @param offsets Array of offsets
 */
void init_offsets(uint64_t virtualAddress, uint64_t* offsets) {
    for (int i = TABLES_DEPTH; i >= 0; i--) {
        offsets[i] = virtualAddress & (PAGE_SIZE - 1);
        virtualAddress = virtualAddress >> OFFSET_WIDTH;
    }
}


/**
 * Calculates the cyclic distance: min{NUM_PAGES - |page_swapped_in - p|, |page_swapped_in - p|}
 *
 * @param page_swapped_in The page we want to swap in
 * @param p The page that we consider to swap out
 * @return The cyclic distance between both pages
 */
int cyclic_distance(uint64_t page_swapped_in, uint64_t p) {
    uint64_t abs_distance;

    // calculate |page_swapped_in - p|
    if (page_swapped_in > p) {
        abs_distance = page_swapped_in - p;
    } else {
        abs_distance = p - page_swapped_in;
    }

    // return the minimum between the 2 options
    if (abs_distance < NUM_PAGES - abs_distance) {
        return (int) abs_distance;
    }
    return NUM_PAGES - abs_distance;
}


/**
 * Updates the maximal cyclic distance if needed
 *
 * @param args Arguments provided for the DFS
 * @param rootFrame The root frame of the current recursion level
 * @param currentVirtual The virtual address of the root frame
 * @param parent The parent of the current root frame (in the tree)
 * @param offset The last frame's offset
 */
void update_max_cyclic_distance(SearchArguments* args, word_t rootFrame, uint64_t currentVirtual,
                                uint64_t parent, uint64_t offset) {
    int cyclicDist = cyclic_distance(args->pageNumber, currentVirtual);

    // check if a larger distance was found and update accordingly
    if (cyclicDist >= args->maxCyclicDist) {
        args->maxCyclicFrame = rootFrame;
        args->maxCyclicDist = cyclicDist;
        args->maxCyclicPage = currentVirtual;
        args->maxCyclicParent = parent * PAGE_SIZE + offset;
    }
}


/**
 * Handles the case an empty frame was not founds and checks for the other priorities - an unused
 * frame or eviction of a frame
 * 
 * @param args Arguments provided for the DFS
 */
void empty_frame_not_found(SearchArguments* args) {
    // check if there is an unused frame
    if (args->maxFrame + 1 < NUM_FRAMES) {
        args->priority = 2;
        return;
    }

    // no available frames - need to evict
    PMwrite(args->maxCyclicParent, 0);
    PMevict(args->maxCyclicFrame, args->maxCyclicPage);
    args->priority = 3;
}


/**
 * Choose the frame by traversing the entire tree of tables in the physical memory while looking
 * for one of the following (prioritized):
 * (1) Frame with an empty table
 * (2) Unused frame
 * (3) Evict the frame that contains a page with the maximal cyclical distance
 *
 * @param args Arguments provided for the DFS
 * @param rootFrame The root frame of the current recursion level
 * @param currentVirtual The virtual address of the root frame
 * @param parent The parent of the current root frame (in the tree)
 * @param depth The current depth we have reached so far in the tree
 * @param offset The last frame's offset
 */
void find_next_frame(SearchArguments* args, word_t rootFrame, uint64_t currentVirtual,
                    uint64_t parent, uint64_t depth, uint64_t offset) {

    // reached the leaves - need to calculate the cyclic distance and update
    if (depth == TABLES_DEPTH) {
        update_max_cyclic_distance(args, rootFrame, currentVirtual, parent, offset);
        return;
    }

    // check if the current root frame is empty (contains a non-zero page)
    bool isCurrentRootEmpty = true;
    for (int i = 0; i < PAGE_SIZE; i++) {
        word_t value;
        PMread(rootFrame * PAGE_SIZE + i, &value);

        // this frame contains a non-zero page
        if (value != 0) {
            isCurrentRootEmpty = false;
            break;
        }
    }

    // check the current root frame is empty & valid for being the next frame
    if (rootFrame != 0 && rootFrame != args->currentFrame && isCurrentRootEmpty) {
        args->emptyFrame = rootFrame;
        PMwrite(parent * PAGE_SIZE + offset, 0);
        args->priority = 1;
        return;
    }

    // search for empty/unused frames
    int nextFrame = 0;

    for (int i = 0; i < PAGE_SIZE; i++) {
        PMread(rootFrame * PAGE_SIZE + i, &nextFrame);

        // there is a next frame in the path (the current frame is not empty)
        if (nextFrame != 0) {
            if (nextFrame >= args->maxFrame) {
                args->maxFrame = nextFrame;
            }

            find_next_frame(args, nextFrame, (currentVirtual << OFFSET_WIDTH) + i, rootFrame,
                            depth + 1, i);

            // an empty frame was found during the DFS search
            if (args->priority == 1) {
                return;
            }
        }
    }

    // finished the recursive search for empty frame and reached the original root frame
    if (rootFrame == 0) {
        empty_frame_not_found(args);
        return;
    }
}




/**
 * Finds the physical address of a given virtual address
 *
 * @param virtualAddress The virtual address we want to translate
 * @param offsets Array of offsets
 * @return The physical address of the given virtual address
 */
uint64_t find_physical_address(uint64_t virtualAddress, uint64_t* offsets) {
    int nextFrame = 0;

    uint64_t pageNumber = virtualAddress >> OFFSET_WIDTH;
    SearchArguments args = {0, 0, pageNumber, 0, 0, 0, 0, 0, 0};

    for (int i = 0; i < TABLES_DEPTH; i++) {
        PMread(args.currentFrame * PAGE_SIZE + offsets[i], &nextFrame);

        // need to search for the next address
        if (nextFrame == 0) {
            find_next_frame(&args, 0, 0, 0, 0, 0);
            args.maxFrame++;

            // 1st priority - empty frame
            if (args.priority == 1) {
                nextFrame = args.emptyFrame;
            }

            // 2nd priority - unused frame
            else if (args.priority == 2) {
                nextFrame = args.maxFrame;
            }

            // 3rd priority - evicted the frame with the maximal cyclic distance
            else if (args.priority == 3) {
                nextFrame = args.maxCyclicFrame;
            }

            PMwrite(args.currentFrame * PAGE_SIZE + offsets[i], nextFrame);

            // found the physical address
            if (i == TABLES_DEPTH - 1) {
                PMrestore(nextFrame, args.pageNumber);
            }

            // unlink it from its parent
            else {
                for (int j = 0; j < PAGE_SIZE; j++) {
                    PMwrite(nextFrame * PAGE_SIZE + j, 0);
                }
            }
        }

        args = {nextFrame, 0, pageNumber, 0, 0, 0, 0, 0, 0};
    }

    return nextFrame;
}


/**
 * Initialize the virtual memory.
 */
void VMinitialize() {
    for (int i = 0; i < PAGE_SIZE; i++) {
        PMwrite(i, 0);
    }
}


/**
 * Reads a word from the given virtual address
 * and puts its content in *value.
 *
 * returns 1 on success.
 * returns 0 on failure (if the address cannot be mapped to a physical
 * address for any reason)
 */
int VMread(uint64_t virtualAddress, word_t* value) {
    if (virtualAddress >= VIRTUAL_MEMORY_SIZE) {
        return 0;
    }

    if ((virtualAddress >> OFFSET_WIDTH) >= NUM_PAGES) {
        return 0;
    }

    uint64_t offsets[TABLES_DEPTH + 1];
    init_offsets(virtualAddress, offsets);

    uint64_t physical_address = find_physical_address(virtualAddress, offsets);
    PMread(physical_address * PAGE_SIZE + offsets[TABLES_DEPTH], value);

    return 1;
}


/**
 * Writes a word to the given virtual address.
 *
 * returns 1 on success.
 * returns 0 on failure (if the address cannot be mapped to a physical
 * address for any reason)
 */
int VMwrite(uint64_t virtualAddress, word_t value) {
    if (virtualAddress >= VIRTUAL_MEMORY_SIZE) {
        return 0;
    }

    if ((virtualAddress >> OFFSET_WIDTH) >= NUM_PAGES) {
        return 0;
    }

    uint64_t offsets[TABLES_DEPTH + 1];
    init_offsets(virtualAddress, offsets);

    uint64_t physical_address = find_physical_address(virtualAddress, offsets);
    PMwrite(physical_address * PAGE_SIZE + offsets[TABLES_DEPTH], value);

    return 1;
}