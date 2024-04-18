#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

#include "os.h"

/*
The setting is identical to the x86 Multi-Level page table 
we saw in class except the fact we use 45 bits so divide that equally
by 9 and we get 5 levels instead of 4. all other metrics are identical.
*/
void page_table_update(uint64_t pt, uint64_t vpn, uint64_t ppn) {
    uint64_t* curr_pte = phys_to_virt(pt << 12); // Add 12 bits offset and point to the root
    int entry;

    for (int i = 4; i > 0; i--) {
        entry = (vpn >> (i * 9)) & 0x1ff; // Getting entry index using the 9-bit key of level i from MSB to LSB
        if ((curr_pte[entry] & 1) == 0) { // If needed allocate a page for the next level and mark pte valid
            if (ppn == NO_MAPPING) {
                return;
            }
            curr_pte[entry] = (alloc_page_frame() << 12) + 1; 
        }
        curr_pte = phys_to_virt(curr_pte[entry] - 1); // Update pointer to the next level page
    }

    entry = vpn & 0x1ff; // Last level (level 4) entry index from LSB
    if (ppn == NO_MAPPING) {
        curr_pte[entry] = 0; // Destroy VPN mapping
        return;
    }
    curr_pte[entry] = (ppn << 12) + 1; // Update last level pte to ppn and mark valid
    return;
}

uint64_t page_table_query(uint64_t pt, uint64_t vpn) {
	uint64_t* curr_pte = phys_to_virt(pt<<12); /*adding offset 0 and getting a pointer to table root (level 0)*/
	int entry;

	for (int i = 0; i < 5; i++) {
		entry = (vpn >> (36 - i * 9)) & 0x1ff; /*getting entry index using the 9-bit key of level i*/
		if ((curr_pte[entry] & 1) == 0) { 
			return NO_MAPPING;
		}
		if (i < 4) { /*update pointer to the next level page unless it's the last level (level 4)*/
			curr_pte = phys_to_virt(curr_pte[entry] - 1);
		}
	}

	return curr_pte[entry]>>12; /*return ppn stored in last level pte*/
}
