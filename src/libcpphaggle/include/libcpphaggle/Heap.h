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

#define HEAP_DEFAULT_SIZE 500
#define HEAP_DEFAULT_INCREASE_SIZE 100

/** */
class HeapItem
{
        friend class Heap;
public:
        HeapItem(double _metric) : metric(_metric), index(-1), active(0) {}
        void setIndex(int _index) {
                index = _index;
        }
        void activate() {
                active = 1;
        }
        void disable() {
                active = 0;
        }
        void setMetric(double _metric) {
                metric = _metric;
        }
        double getMetric() {
                return metric;
        }

private:
        double metric;
        int index;
        short active;
};

/** */
class Heap
{
public:
        Heap(unsigned int _size = HEAP_DEFAULT_SIZE) : max_size(_size), size(0), heap(new HeapItem*[_size]) {}
        ~Heap() {
                delete [] heap;
        }
        //add(
        void heapify(unsigned int i);
        bool isEmpty() {
                return (size == 0 ? true : false);
        }
        bool isFull() {
                return (size >= max_size ? true : false);
        }
        int insert(HeapItem *item);
        HeapItem *extractFirst();
        HeapItem *getFirst() {
                return heap[0];
        }
	unsigned int getSize() const { return size; }
private:
        int increaseSize(unsigned int increase_size = HEAP_DEFAULT_INCREASE_SIZE);
        unsigned int max_size;
        unsigned int size;
        HeapItem **heap;
};

}; // namespace haggle

#endif /* _HEAP_H */
