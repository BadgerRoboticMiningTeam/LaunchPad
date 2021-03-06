#include "circular_buffer.h"
#include "interrupt.h"

#include "launchPadUIO.h"

bool initCircularBuffer(circularBuffer_t *buff, unsigned int itemSize, unsigned int numItems, void *buffAddr) {
	if (buff == 0 || buffAddr == 0 || itemSize == 0 || numItems == 0) return false;
	
	buff->itemSize = itemSize;
	buff->numItems = numItems;
	buff->rdCnt = 0;
	buff->wrCnt = 0;
	buff->data = buffAddr;
	
	return true;
}

bool circularBufferFull(circularBuffer_t *buff) {
	if (buff->wrCnt - buff->rdCnt >= buff->numItems) {
		return true;
	} else {
		return false;
	}
}

//*****************************************************************************
//
//! Add a single item to the buffer without performing any parameter checks.
//! This function encapsulates the code that can be shared between
//! circularBufferAddItem() and circularBufferAddMultiple().
//!
//! \param buff is a pointer to a circularBuffer struct
//!
//! \param item is the pointer where the removed item should be copied to
//!
//! \return none
//
//*****************************************************************************
void addSingleItemUnsafe(circularBuffer_t *buff, void *item) {
	unsigned int index;
	int bytesLeft;
	uint8_t *dataPtr8;
	uint64_t *dataPtr64;
	
	// if the item size is a standard number of bytes, use a simpler copy
	index = buff->wrCnt % buff->numItems;
	switch (buff->itemSize) {
		case 1 :
			buff->data[index] = *((uint8_t *)item);
			break;
		case 2 :
			((uint16_t *)(buff->data))[index] = *((uint16_t *)item);
			break;
		case 4 :
			((uint32_t *)(buff->data))[index] = *((uint32_t *)item);
			break;
		case 8 :
			((uint64_t *)(buff->data))[index] = *((uint64_t *)item);
			break;
		default :
			// if the item size is a non-standard number of bytes, first copy
			// 64-bit chunks, then copy the remainder byte by byte
			bytesLeft = buff->itemSize;
			dataPtr64 = item;
			while (bytesLeft >= 8) {
				((uint64_t *)(buff->data))[index] = *dataPtr64;
				index += 8;
				dataPtr64++;
				bytesLeft -= 8;
			}
			dataPtr8 = (uint8_t *)dataPtr64;
			while (bytesLeft > 0) {
				buff->data[index] = *dataPtr8;
				index++;
				bytesLeft--;
			}
			break;
	}
	
	return;
}

bool circularBufferAddItem(circularBuffer_t *buff, void *item) {
	// if the buffer doesn't have any more space, return false
	if (buff == 0 || item == 0 || buff->wrCnt - buff->rdCnt >= buff->numItems) {
		return false;
	}
	
	IntMasterDisable();
	
	// use the unsafe internal function to add a single item
	addSingleItemUnsafe(buff, item);
	
	// increment the write counter
	buff->wrCnt++;
	
	IntMasterEnable();
	
	return true;
}

unsigned int circularBufferAddMultiple(circularBuffer_t *buff, void *item, unsigned int numItems) {
	unsigned int itemsRemaining;
	
	IntMasterDisable();
	
	if (buff == 0 || item == 0) {
		IntMasterEnable();
		return 0;
	}
	
	// if the buffer doesn't have enough room, only add enough elements to fill
	// the buffer
	if (buff->numItems - (buff->wrCnt - buff->rdCnt) < numItems) {
		numItems = buff->numItems - (buff->wrCnt - buff->rdCnt);
	}
	itemsRemaining = numItems;
	
	// increment the buffer's write count
	buff->wrCnt += itemsRemaining;
	
	// use the unsafe internal function to add single items until all items are added
	while (itemsRemaining > 0) {
		addSingleItemUnsafe(buff, item);
		
		// move the pointer to the next item to be added
		item = (uint8_t *)item + buff->itemSize;
		// update remaining items count
		itemsRemaining--;
	}
	
	IntMasterEnable();
	
	return numItems;
}

bool circularBufferEmpty(circularBuffer_t *buff) {
	if (buff->wrCnt - buff->rdCnt == 0) {
		return true;
	} else {
		return false;
	}
}

//*****************************************************************************
//
//! Remove a single item from the buffer without performing any parameter
//! checks. This function encapsulates the code that can be shared between
//! circularBufferRemoveItem() and circularBufferRemoveMultiple().
//!
//! \param buff is a pointer to a circularBuffer struct
//!
//! \param item is the pointer where the removed item should be copied to
//!
//! \return none
//
//*****************************************************************************
void removeSingleItemUnsafe(circularBuffer_t *buff, void *data) {
	unsigned int index;
	unsigned int bytesLeft;
	uint64_t *dataPtr64;
	uint8_t *dataPtr8;
	
	// if the item size is a standard number of bytes, use a simpler copy
	index = buff->rdCnt % buff->numItems;
	switch (buff->itemSize) {
		case 1 :
			*((uint8_t *)data) = buff->data[index];
			break;
		case 2 :
			*((uint16_t *)data) = ((uint16_t *)(buff->data))[index];
			break;
		case 4 :
			*((uint32_t *)data) = ((uint32_t *)(buff->data))[index];
			break;
		case 8 :
			*((uint64_t *)data) = ((uint64_t *)(buff->data))[index];
			break;
		default :
			// if the item size is a non-standard number of bytes, first copy
			// 64-bit chunks, then copy the remainder byte by byte
			bytesLeft = buff->itemSize;
			dataPtr64 = data;
			while (bytesLeft >= 8) {
				*dataPtr64 = buff->data[index];
				dataPtr64++;
				index += 8;
				bytesLeft -= 8;
			}
			dataPtr8 = (uint8_t *)dataPtr64;
			while (bytesLeft > 0) {
				*dataPtr8 = buff->data[index];
				dataPtr8++;
				index++;
				bytesLeft--;
			}
			break;
	}
	
	return;
}

bool circularBufferRemoveItem(circularBuffer_t *buff, void *data) {
	IntMasterDisable();
	
	// if the buffer is empty, return false
	if (buff == 0 || data == 0 || buff->rdCnt >= buff->wrCnt) {
		IntMasterEnable();
		return false;
	}
	
	// use the unsafe internal function to remove a single item
	removeSingleItemUnsafe(buff, data);
	
	// increment the read counter
	buff->rdCnt++;
	
	// do a rollover check to avoid eventual overflows in the counters.
	// presumably the write count is always larger than the read count, so it
	// doesn't need to be checked.
	if (buff->rdCnt >= buff->numItems) {
		buff->rdCnt -= buff->numItems;
		buff->wrCnt -= buff->numItems;
	}
	
	IntMasterEnable();
	
	return true;
}

unsigned int circularBufferRemoveMultiple(circularBuffer_t *buff, void *data, unsigned int numItems) {
	unsigned int itemsRemaining;
	
	IntMasterDisable();
	
	if (buff == 0 || data == 0 || numItems == 0) {
		IntMasterEnable();
		return 0;
	}
	
	// check if there are actually numItems items in the buffer. If not, only
	// remove as many items as there are in the buffer.
	if (buff->wrCnt - buff->rdCnt < numItems) {
		numItems = buff->wrCnt - buff->rdCnt;
	}
	itemsRemaining = numItems;
	
	// increment the buffer's read count
	buff->rdCnt += itemsRemaining;
	
	// use the unsafe internal function to remove single items until all items
	// are removed
	while (itemsRemaining > 0) {
		removeSingleItemUnsafe(buff, data);
		
		// move the pointer to the next empty space in the data array
		data = (uint8_t *)data + buff->itemSize;
		// update remaining items count
		itemsRemaining--;
	}
	
	// do a rollover check to avoid eventual overflows in the counters.
	// presumably the write count is always larger than the read count, so it
	// doesn't need to be checked.
	if (buff->rdCnt >= buff->numItems) {
		buff->rdCnt -= buff->numItems;
		buff->wrCnt -= buff->numItems;
	}
	
	IntMasterEnable();
	
	return numItems;
}
