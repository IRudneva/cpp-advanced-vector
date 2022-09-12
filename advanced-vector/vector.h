#pragma once
#include <cassert>
#include <cstdlib>
#include <new>
#include <memory>
#include <utility>


template <typename T>
class RawMemory {
public:
	RawMemory() = default;

	explicit RawMemory(size_t capacity)
		: buffer_(Allocate(capacity))
		, capacity_(capacity) {
	}

	RawMemory(const RawMemory&) = delete;
	RawMemory& operator=(const RawMemory& rhs) = delete;

	RawMemory(RawMemory&& other) noexcept
		: buffer_(std::move(other.buffer_))
		, capacity_(other.capacity_) {
		other.capacity_ = 0;
		other.buffer_ = nullptr;
	}

	RawMemory& operator=(RawMemory&& rhs) noexcept {
		Swap(rhs);
		rhs.Deallocate(rhs.buffer_);
		rhs.capacity_ = 0;
		rhs.buffer_ = nullptr;
		return *this;
	}

	~RawMemory() {
		Deallocate(buffer_);
	}

	T* operator+(size_t offset) noexcept {
		// Разрешается получать адрес ячейки памяти, следующей за последним элементом массива
		assert(offset <= capacity_);
		return buffer_ + offset;
	}

	const T* operator+(size_t offset) const noexcept {
		return const_cast<RawMemory&>(*this) + offset;
	}

	const T& operator[](size_t index) const noexcept {
		return const_cast<RawMemory&>(*this)[index];
	}

	T& operator[](size_t index) noexcept {
		assert(index < capacity_);
		return buffer_[index];
	}

	void Swap(RawMemory& other) noexcept {
		std::swap(buffer_, other.buffer_);
		std::swap(capacity_, other.capacity_);
	}

	const T* GetAddress() const noexcept {
		return buffer_;
	}

	T* GetAddress() noexcept {
		return buffer_;
	}

	size_t Capacity() const {
		return capacity_;
	}

private:
	// Выделяет сырую память под n элементов и возвращает указатель на неё
	static T* Allocate(size_t n) {
		return n != 0 ? static_cast<T*>(operator new(n * sizeof(T))) : nullptr;
	}

	// Освобождает сырую память, выделенную ранее по адресу buf при помощи Allocate
	static void Deallocate(T* buf) noexcept {
		operator delete(buf);
	}

	T* buffer_ = nullptr;
	size_t capacity_ = 0;
};

template <typename T>
class Vector {
public:
	explicit Vector() = default;

	explicit Vector(size_t size)
		: data_(size)
		, size_(size)
	{
		std::uninitialized_value_construct_n(data_.GetAddress(), size);
	}

	Vector(Vector&& other) noexcept
		:data_(std::move(other.data_)),
		size_(std::move(other.size_))
	{
		other.size_ = 0;
	}

	Vector(const Vector& other)
		: data_(other.size_)
		, size_(other.size_)
	{
		std::uninitialized_copy_n(other.data_.GetAddress(), other.size_, data_.GetAddress());
	}

	~Vector() {
		std::destroy_n(data_.GetAddress(), size_);
	}

	using iterator = T*;
	using const_iterator = const T*;

	iterator begin() noexcept {
		return data_.GetAddress();
	}
	iterator end() noexcept {
		return data_.GetAddress() + size_;
	}
	const_iterator begin() const noexcept {
		return data_.GetAddress();
	}
	const_iterator end() const noexcept {
		return data_.GetAddress() + size_;
	}
	const_iterator cbegin() const noexcept {
		return data_.GetAddress();
	}
	const_iterator cend() const noexcept {
		return data_.GetAddress() + size_;
	}

	Vector& operator=(const Vector& rhs) {
		if (this != &rhs) {
			if (rhs.size_ > data_.Capacity()) {
				Vector rhs_copy(rhs);
				Swap(rhs_copy);
			}
			else {
				if (rhs.size_ < size_) {
					for (size_t i = 0; i < rhs.size_; ++i) {
						data_[i] = rhs.data_[i];
					}
					std::destroy_n(data_.GetAddress() + rhs.size_, size_ - rhs.size_);
				}
				else {
					for (size_t i = 0; i < size_; ++i) {
						data_[i] = rhs.data_[i];
					}
					std::uninitialized_copy_n(rhs.data_.GetAddress() + size_, rhs.size_ - size_, data_.GetAddress() + size_);
				}
				size_ = rhs.size_;
			}
		}
		return *this;
	}

	Vector& operator=(Vector&& rhs) noexcept {
		if (this != &rhs) {
			data_ = std::move(rhs.data_);
			size_ = rhs.size_;
			rhs.size_ = 0;
		}
		return *this;
	}

	void Swap(Vector& other) noexcept {
		data_.Swap(other.data_);
		std::swap(size_, other.size_);
	}

	size_t Size() const noexcept {
		return size_;
	}

	size_t Capacity() const noexcept {
		return data_.Capacity();
	}

	const T& operator[](size_t index) const noexcept {
		return const_cast<Vector&>(*this)[index];
	}

	T& operator[](size_t index) noexcept {
		assert(index < size_);
		return data_[index];
	}

	void Reserve(size_t new_capacity) {
		if (new_capacity <= data_.Capacity()) {
			return;
		}
		RawMemory<T> new_data(new_capacity);
		if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
			std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
		}
		else {
			std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
		}
		std::destroy_n(data_.GetAddress(), size_);
		data_.Swap(new_data);
	}

	void Resize(size_t new_size) {
		if (size_ == new_size)
		{
			return;
		}

		if (size_ > new_size)
		{
			std::destroy_n(data_.GetAddress() + new_size, size_ - new_size);
			size_ = new_size;
		}
		else
		{
			Reserve(new_size);
			std::uninitialized_value_construct_n(data_.GetAddress() + size_, new_size - size_);
			size_ = new_size;
		}
	}

	void PushBack(const T& value) {
		EmplaceBack(value);
	}

	void PushBack(T&& value) {
		EmplaceBack(std::move(value));
	}

	template <typename... Args>
	T& EmplaceBack(Args&&... args) {
		if (size_ == Capacity()) {
			RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
			new (new_data + size_) T(std::forward<Args>(args)...);
			if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
				std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
			}
			else {
				std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
			}
			std::destroy_n(data_.GetAddress(), size_);
			data_.Swap(new_data);
		}
		else {
			new (data_ + size_) T(std::forward<Args>(args)...);
		}
		++size_;
		return data_[size_ - 1];
	}

	void PopBack() /* noexcept */ {
		if (size_ == 0)
		{
			return;
		}
		std::destroy_n(data_.GetAddress() + size_ - 1, 1);
		--size_;
	}

	iterator Insert(const_iterator pos, const T& value) {
		return Emplace(pos, value);
	}

	iterator Insert(const_iterator pos, T&& value) {
		return Emplace(pos, std::move(value));
	}

	template <typename... Args>
	iterator Emplace(const_iterator pos, Args&&... args) {
		int pos_index = pos - cbegin();
		if (size_ == Capacity()) {
			RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
			new (new_data.GetAddress() + pos_index) T(std::forward<Args>(args)...);
			try {
				if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
					std::uninitialized_move_n(data_.GetAddress(), pos_index, new_data.GetAddress());
				}
				else {
					std::uninitialized_copy_n(data_.GetAddress(), pos_index, new_data.GetAddress());
				}
			}
			catch (...) {
				std::destroy_n(new_data.GetAddress() + pos_index, 1);
				throw;
			}

			try {
				if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
					std::uninitialized_move_n(data_.GetAddress() + pos_index, size_ - pos_index,
						new_data.GetAddress() + pos_index + 1);
				}
				else {
					std::uninitialized_copy_n(data_.GetAddress() + pos_index, size_ - pos_index,
						new_data.GetAddress() + pos_index + 1);
				}
			}
			catch (...) {
				std::destroy_n(new_data.GetAddress(), pos_index + 1);
				throw;
			}
			std::destroy_n(data_.GetAddress(), size_);
			data_.Swap(new_data);
		}
		else {
			if (pos != cend()) {
				new (end()) T(std::forward<T>(*(end() - 1)));
				std::move_backward(begin() + pos_index, end() - 1, end());
				data_[pos_index] = T(std::forward<Args>(args)...);
			}
			else {
				new (data_.GetAddress() + pos_index) T(std::forward<Args>(args)...);
			}
		}
		++size_;
		return begin() + pos_index;
	}

	iterator Erase(const_iterator er_pos) {
		int pos_index = er_pos - cbegin();
		auto pos = begin() + pos_index;
		if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
			std::move(pos + 1, end(), pos);
		}
		else {
			std::copy(pos + 1, end(), pos);
		}
		PopBack();
		return pos;
	}

private:
	RawMemory<T> data_;
	size_t size_ = 0;

	// Вызывает деструкторы n объектов массива по адресу buf
	static void DestroyN(T* buf, size_t n) noexcept {
		for (size_t i = 0; i != n; ++i) {
			Destroy(buf + i);
		}
	}

	// Вызывает деструктор объекта по адресу buf
	static void Destroy(T* buf) noexcept {
		buf->~T();
	}
};