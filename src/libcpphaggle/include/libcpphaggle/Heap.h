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
#ifndef _HEAP_H
#define _HEAP_H

namespace haggle {

class HeapItem;
class Heap;

#define HEAP_DEFAULT_SIZE 200
#define HEAP_DEFAULT_INCREASE_SIZE 100

/** 
 The HeapItem class should be inherited by any data item that should
 be placed in a heap.
 */
class HeapItem
{
        friend class Heap;
	static const unsigned long npos = -1;
public:
        HeapItem();
	virtual ~HeapItem();
        void activate();
        void disable();
	/**
		getKey() returns the key which decides where to place the item
		in the heap. Must be overridden by derived class.
	*/
        virtual double getKey() const = 0;
private:
        unsigned long index;
        bool active;
};

/** 
 The Heap class implements a min-heap data structure.
 */
class Heap
{
public:
        Heap(unsigned long max_size = HEAP_DEFAULT_SIZE);
        ~Heap();
        bool empty() const;
        bool full() const;
        bool insert(HeapItem *item);
	void push_back(HeapItem *item);
        HeapItem *extractFirst();
	void pop_front();
        HeapItem *front();
	unsigned long size() const;
private:
        void heapify(unsigned long i);
        bool increaseSize(unsigned long increase_size = HEAP_DEFAULT_INCREASE_SIZE);
        unsigned long _max_size;
        unsigned long _size;
        HeapItem **heap;
};

}; // namespace haggle

#endif /* _HEAP_H */
