#pragma once
#include<iostream>
#include<memory>
#include<vector>
#include<stack>
#include<functional>

template<typename T>
class memory_pool {
private:
	struct Chunk {
		char* data;
		size_t size;


		Chunk(size_t sz) :size(sz) {
			data = static_cast<char*>(::operator new(sizeof(T) * sz));
		}

		~Chunk(){
			::operator delete(data);
		}
	};

	std::vector<Chunk> chunks;
	std::stack<T*> freeList;
	size_t chunkSize;

	void allocateChunk() {
		chunks.emplace_back(chunkSize);
		char* block = chunks.back().data;

		for (size_t i = 0; i < chunkSize; i++) {
			T* obj = reinterpret_cast<T*>(block + i * sizeof(T));
			freeList.push(obj);
		}
	}

public:
	explicit memory_pool(size_t size = 64) :chunkSize(size) {
		allocateChunk();
	}

	memory_pool(const memory_pool&) = delete;
	memory_pool& operator=(const memory_pool&) = delete;

	memory_pool(memory_pool&&) = default;
	memory_pool& operator=(memory_pool&&) = default;

	template<typename...Args>
	std::unique_ptr<T, std::function<void(T*)>> acquire(Args&& ... args) {
		if (freeList.empty()) {
			allocateChunk();
		}

		T* ptr = freeList.top();
		freeList.pop();

		try
		{
			new(ptr) T(std::forward<Args>(args)...);
		}
		catch (...)
		{
			freeList.push(ptr);
			throw;
		}

		return std::unique_ptr<T, std::function<void(T*)>>(
			ptr, [this](T* p) {
				if (p) {
					p->~T();
					freeList.push(p);
				}
			}
		);
	}

	size_t available() const {
		return freeList.size();
	}

	size_t chunkcount() const {
		return chunks.size();
	}
};