/* Copyright 2008-2009 Uppsala University
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <string.h>
#include <stdio.h>

#include <libcpphaggle/Heap.h>

namespace haggle {

void Heap::heapify(unsigned long i)
{
	unsigned long l, r, smallest;
	HeapItem *tmp;

	l = (2 * i) + 1;	/* left child */
	r = l + 1;		/* right child */

	if ((l < _size) && (heap[l]->getKey() < heap[i]->getKey()))
		smallest = l;
	else
		smallest = i;

	if ((r < _size) && (heap[r]->getKey() < heap[smallest]->getKey()))
		smallest = r;

	if (smallest == i)
		return;

	/* exchange to maintain heap property */
	tmp = heap[smallest];
	heap[smallest] = heap[i];
	heap[smallest]->index = smallest;
	heap[i] = tmp;
	heap[i]->index = i;
	heapify(smallest);
}

bool Heap::increaseSize(unsigned long increase_size)
{
	HeapItem **new_heap;

	new_heap = new HeapItem *[_max_size + increase_size];

	if (!new_heap) {
		return false;
	}

	memcpy(new_heap, heap, _size * sizeof(HeapItem *));

	delete[] heap;

	heap = new_heap;

	_max_size += increase_size;

	return true;
}

bool Heap::insert(HeapItem *item)
{
	unsigned long i, parent;

	if (full()) {
		if (!increaseSize()) {
			fprintf(stderr, "Heap is full and could not increase heap _size, _size=%d\n", _size);
			return false;
		}
	}

	i = _size;
	parent = (i - 1) / 2;

	/* find the correct place to insert */
	while ((i > 0) && (heap[parent]->getKey() > item->getKey())) {
		heap[i] = heap[parent];
		heap[i]->index = i;
		i = parent;
		parent = (i - 1) / 2;
	}
	heap[i] = item;
	item->index = i;
	_size++;

	return true;
}

HeapItem *Heap::extractFirst(void)
{
	HeapItem *max;

	if (empty())
		return NULL;

	max = heap[0];
	_size--;
	heap[0] = heap[_size];
	heap[0]->index = 0;
	heapify(0);

	return max;
}

void Heap::pop_front()
{
	extractFirst();
}

}; // namespace haggle
